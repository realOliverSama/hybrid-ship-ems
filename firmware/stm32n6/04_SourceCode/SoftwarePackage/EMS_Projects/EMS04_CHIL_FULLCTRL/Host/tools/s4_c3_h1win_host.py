#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_c3_h1win_host.py — EMS04 S4-C3：H1 特征窗 host 版模块（板侧语义的 Python 逐位复刻）。

语义权威 = 板侧 ems04_h1win.c（EMS04-S2 B1，只读参照）：
  EMS04_CHIL_FULLCTRL/Appli/App/ems04_h1win.c（本模块禁止重新设计语义）
用途 = S4-C3 闭环 runner 中为 B 链（部署同形输入）构造 300×11 hist。

浮点纪律（与 C 冻结一致，FPC-1.0 §1）：f64 构造域 + 出口统一 cast f32
（IEEE round-to-nearest-even）；中点严格 (v0 + v1) * 0.5；water_depth 严格
(w + w) * 0.5；禁止代数重排 / fast-math 等价物。

fail-closed：任何校验失败抛 H1WinError（code 对应板侧 EMS04_FP_ERR_* 枚举语义），
绝不推进状态、绝不产出部分结果。

环深 301 × 8 通道 f64；slot = tick_idx % 301；construct 仅当 t0 == last_tick 且
last_tick >= 300 时合法（窗内 301 样本齐备）。输出 (300,11) float32 C-order
t-major（.tobytes() 视角 = 13200 B LE）。

TICK2 载荷字段序（契约 §2.1）：v[0..10] =
  [0]t_s [1]P_load [2]SOC [3]v_ref [4]v_ship [5]accel
  [6]x_ship [7]n_prop [8]P_hotel [9]v_current [10]P_prop_demand

运行自检：.venv_tf/bin/python Host/tools/s4_c3_h1win_host.py --selftest
依赖：仅 numpy。
"""
import math
import struct
import sys

import numpy as np

# ---- 尺寸常量（ems04_h1win.h 冻结值） ----
N_TICK_F64 = 11        # EMS04_FP_N_TICK_F64
H1_N_CH = 8            # EMS04_H1_N_CH
H1_RING = 301          # EMS04_H1_RING
H1_ROWS = 300          # EMS04_H1_ROWS
H1_COLS = 11           # EMS04_H1_COLS
H1_BYTES = H1_ROWS * H1_COLS * 4  # 13200

# H1 输出列序（冻结，ems04_h1_col_names）
COL_NAMES = (
    "P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
    "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
    "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct",
)

# TICK2 载荷索引 → 环通道（内部通道序冻结，禁重排）：
#   环通道 = [P_LOAD, V_SHIP, ACCEL, N_PROP, P_HOTEL, V_CURRENT, V_REF, P_PROP_DEMAND]
RING_CHANNELS = (1, 4, 5, 7, 8, 9, 3, 10)

# H1 输出列 → 环通道（None = 常量列，见 construct 分支）
_H1COL_TO_RING = (0, 1, 2, 3, 4, 5, None, 7, None, 6, None)

# 错误码字符串（对应 EMS04_FP_ERR_* 枚举语义）
ERR_STATE = "ERR_STATE"          # 0x40 未 begin / 会话态缺失
ERR_POLICY = "ERR_POLICY"        # 0x41 manual_rule != 0 或 ctrl_mode != 0.0
ERR_ORDER = "ERR_ORDER"          # 0x42 tick 不连续 / 越出 uint32 域
ERR_TICK_T = "ERR_TICK_T"        # 0x43 v[0] != float(tick_idx)（NaN 亦落此）
ERR_NONFINITE = "ERR_NONFINITE"  # 0x44 非有限输入 / 构造中间值 / f32 溢出
ERR_NOT_READY = "ERR_NOT_READY"  # 0x45 未收满 301 拍或 t0 != last_tick
ERR_LEN = "ERR_LEN"              # 0x03 v 长度 != 11（对应 EMS04_FP_ERR_LEN）


class H1WinError(Exception):
    """fail-closed 错误；code 为板侧 EMS04_FP_ERR_* 同名语义字符串。"""

    def __init__(self, code, msg):
        self.code = code
        super().__init__("%s: %s" % (code, msg))


class H1WindowHost:
    """H1 特征窗 host 版（ems04_h1win.c 逐位复刻）。

    状态 = 8 通道 × 301 深 f64 环 + 航次常量 + 会话态。
    construct() 返回 numpy (300,11) float32（内部 f64 逐步计算、逐元素 RNE cast）。
    """

    def __init__(self):
        self._clear()

    def _clear(self):
        self.ring = [[0.0] * H1_N_CH for _ in range(H1_RING)]
        self.water_depth_m = 0.0
        self.ctrl_mode = 0.0
        self.manual_rule = 0
        self.tick_first = 0
        self.last_tick = 0
        self.has_tick = False
        self.has_voyage = False
        self.ticks_ok = 0
        self.constructs_ok = 0

    # ---- 航次初始化（VOYAGE_INIT）：清空环 + 落常量 ----
    def begin(self, tick_first, water_depth_m, ctrl_mode=0.0, manual_rule=0):
        if manual_rule != 0:  # 仅 ZERO_FILL 合法
            raise H1WinError(ERR_POLICY, "manual_rule != ZERO_FILL(0)")
        water_depth_m = float(water_depth_m)
        ctrl_mode = float(ctrl_mode)
        if not (math.isfinite(water_depth_m) and math.isfinite(ctrl_mode)):
            raise H1WinError(ERR_NONFINITE, "non-finite water_depth/ctrl_mode")
        self._clear()
        self.water_depth_m = water_depth_m
        self.ctrl_mode = ctrl_mode
        self.manual_rule = int(manual_rule)
        self.tick_first = int(tick_first)
        self.has_voyage = True
        return 0

    # ---- CMD_TICK2 校验 + 写入环；失败不推进状态 ----
    def tick(self, tick_idx, v):
        if not self.has_voyage:
            raise H1WinError(ERR_STATE, "tick before begin()")
        tick_idx = int(tick_idx)
        if not (0 <= tick_idx <= 0xFFFFFFFF):
            raise H1WinError(ERR_ORDER, "tick_idx out of uint32 domain")
        if len(v) != N_TICK_F64:
            raise H1WinError(ERR_LEN, "v length %d != %d" % (len(v), N_TICK_F64))
        # 连续性：首拍 == tick_first；其后逐拍 +1，其余 fail-closed
        if not self.has_tick:
            if tick_idx != self.tick_first:
                raise H1WinError(ERR_ORDER, "first tick != tick_first")
        elif tick_idx != self.last_tick + 1:
            raise H1WinError(ERR_ORDER, "duplicate/skip/out-of-order tick")
        # t_s 与 tick_idx 精确相等（f64 位级；NaN 亦落入此分支）
        if not (float(v[0]) == float(tick_idx)):
            raise H1WinError(ERR_TICK_T, "v[0] != tick_idx")
        vals = [float(x) for x in v]
        # 全部 11 字段 isfinite（含 SOC / x_ship：随帧携带、不消费但必须有限）
        for x in vals:
            if not math.isfinite(x):
                raise H1WinError(ERR_NONFINITE, "non-finite tick field")
        slot = tick_idx % H1_RING
        row = self.ring[slot]
        for i in range(H1_N_CH):
            row[i] = vals[RING_CHANNELS[i]]
        self.last_tick = tick_idx
        self.has_tick = True
        self.ticks_ok += 1
        return 0

    # ---- 锚点 t0 构造 hist：f64 构造域，出口逐元素 f32 RNE ----
    def construct(self, t0):
        if not self.has_tick:
            raise H1WinError(ERR_STATE, "construct before any tick")
        t0 = int(t0)
        if self.last_tick < H1_ROWS:
            raise H1WinError(ERR_NOT_READY, "fewer than %d ticks" % H1_ROWS)
        if t0 != self.last_tick:
            # 环深 301 = 窗长 301：仅 t0 == last_tick 时窗内 301 样本齐备
            raise H1WinError(ERR_NOT_READY, "t0 != last_tick")
        # zero-fill fail-closed（DGN-06 板侧分支：仅 AUTO 合法）
        if not (self.ctrl_mode == 0.0):
            raise H1WinError(ERR_POLICY, "ctrl_mode != 0.0 (AUTO)")

        f64 = np.empty((H1_ROWS, H1_COLS), dtype=np.float64)
        w = self.water_depth_m
        for j in range(H1_ROWS):
            ia = (t0 - H1_ROWS + j) % H1_RING   # v(t0-300+j)
            ib = (t0 - (H1_ROWS - 1) + j) % H1_RING  # v(t0-299+j)
            ra = self.ring[ia]
            rb = self.ring[ib]
            for c in range(H1_COLS):
                if c == 6:
                    # 常量列 water_depth_m：同一中点路径（(w+w)*0.5 = w 精确）
                    bin_ = (w + w) * 0.5
                elif c == 8:
                    # last_mode 于常量 = policy 值（合法 = 0.0 AUTO）
                    bin_ = self.ctrl_mode
                elif c == 9:
                    # interval_endpoint：bin = 右端点样本值
                    bin_ = rb[6]
                elif c == 10:
                    # DGN-06 zero-fill（AUTO 分支）：manual_power_cmd_pct := 0.0
                    bin_ = 0.0
                else:
                    # time_mean 退化形式：(v0 + v1) * 0.5（op 序冻结）
                    bin_ = (ra[_H1COL_TO_RING[c]] + rb[_H1COL_TO_RING[c]]) * 0.5
                if not math.isfinite(bin_):
                    raise H1WinError(ERR_NONFINITE, "non-finite bin (f64 gate)")
                f64[j, c] = bin_
        out = f64.astype(np.float32)  # 逐元素 IEEE RNE cast
        if not np.isfinite(out).all():
            raise H1WinError(ERR_NONFINITE, "f32 overflow (exit-domain gate)")
        self.constructs_ok += 1
        return out


# ============================================================================
# 自检（--selftest）：确定性夹具 + 独立第二实现对拍 + fail-closed 全覆盖
# ============================================================================

def _fixture_v(t):
    """确定性 TICK2 载荷，v[0..10] = C 注释序：
    [t_s, P_load, SOC, v_ref, v_ship, accel, x_ship, n_prop, P_hotel,
     v_current, P_prop_demand]。取 0.1 类系数使中点含非平凡 f32 RNE。"""
    return [
        float(t),              # [0] t_s
        100.0 + 0.1 * t,       # [1] P_load_total_kW
        0.6 - 1e-6 * t,        # [2] SOC（随帧携带）
        7.0 + 0.001 * t,       # [3] v_ref_mps
        1.5 + 1e-4 * t,        # [4] v_ship_mps
        1e-5 * t,              # [5] acceleration_mps2
        0.0,                   # [6] x_ship（随帧携带）
        55.0 + 0.008 * t,      # [7] n_prop_rpm
        20.0 + 0.002 * t,      # [8] P_hotel_kW
        0.3 + 1e-6 * t,        # [9] v_current_mps
        100.0 + 0.05 * t,      # [10] P_prop_demand_kW
    ]


def _f32(x):
    """f32 RNE cast，走 struct 往返（与 numpy astype 不同的独立代码路径）。"""
    return struct.unpack("<f", struct.pack("<f", x))[0]


def _reference_hist(samples, t0, water_depth_m):
    """独立第二实现：绝对拍键值 dict（无 %301 环）+ 纯 Python float +
    struct f32 RNE，逐列显式写公式。返回 13200 B LE。"""
    rows = []
    for j in range(H1_ROWS):
        va = samples[t0 - H1_ROWS + j]   # 绝对拍索引，无环
        vb = samples[t0 - (H1_ROWS - 1) + j]
        rows.append([
            _f32((va[1] + vb[1]) * 0.5),          # 0 P_load
            _f32((va[4] + vb[4]) * 0.5),          # 1 v_ship
            _f32((va[5] + vb[5]) * 0.5),          # 2 accel
            _f32((va[7] + vb[7]) * 0.5),          # 3 n_prop
            _f32((va[8] + vb[8]) * 0.5),          # 4 P_hotel
            _f32((va[9] + vb[9]) * 0.5),          # 5 v_current
            _f32((water_depth_m + water_depth_m) * 0.5),  # 6 water_depth
            _f32((va[10] + vb[10]) * 0.5),        # 7 P_prop_demand
            _f32(0.0),                            # 8 ctrl_mode
            _f32(vb[3]),                          # 9 v_ref（右端点）
            _f32(0.0),                            # 10 manual zero-fill
        ])
    flat = [x for row in rows for x in row]
    return struct.pack("<%df" % (H1_ROWS * H1_COLS), *flat)


def _bits_f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def _selftest():
    n_pass = 0
    failures = []

    def check(name, cond):
        nonlocal n_pass
        if cond:
            n_pass += 1
            print("PASS: %s" % name)
        else:
            failures.append(name)
            print("FAIL: %s" % name)

    def expect_err(name, code, fn, *args):
        try:
            fn(*args)
        except H1WinError as e:
            check(name, e.code == code)
        except Exception as e:  # noqa: BLE001
            check("%s [wrong exception %r]" % (name, e), False)
        else:
            check("%s [no exception raised]" % name, False)

    # ---- (a) 确定性夹具：302 拍 + construct(301) + 独立实现对拍 ----
    # 夹具值与十进制字面量逐位相等的先决条件（位级抽查依赖此等价）
    check("fixture P_load(1) == 100.1 literal", _fixture_v(1)[1] == 100.1)
    check("fixture P_load(2) == 100.2 literal", _fixture_v(2)[1] == 100.2)
    check("fixture v_ref(2) == 7.002 literal", _fixture_v(2)[3] == 7.002)

    h = H1WindowHost()
    h.begin(0, 20.0)
    samples = {}
    for t in range(302):
        v = _fixture_v(t)
        h.tick(t, v)
        samples[t] = [float(x) for x in v]

    out1 = h.construct(301)
    check("construct shape (300,11)", out1.shape == (300, 11))
    check("construct dtype float32", out1.dtype == np.float32)
    check("construct C-contiguous", bool(out1.flags["C_CONTIGUOUS"]))
    check("construct all finite", bool(np.isfinite(out1).all()))
    check("tobytes length 13200", len(out1.tobytes()) == H1_BYTES)

    ref_bytes = _reference_hist(samples, 301, 20.0)
    check("full bitwise vs independent impl (t0=301)",
          out1.tobytes() == ref_bytes)
    ref_arr = np.frombuffer(ref_bytes, dtype="<f4").reshape(H1_ROWS, H1_COLS)
    check("np.array_equal vs independent impl (f32 domain)",
          np.array_equal(out1, ref_arr))

    # 位级抽查：3 个已知元素的 f32 位型（含手推 0x41A00000）
    b = out1.tobytes()
    bits_load = struct.unpack("<I", b[0:4])[0]           # [0,0] P_load
    exp_load = _bits_f32((100.1 + 100.2) * 0.5)
    check("bit [0,0] P_load midpoint == RNE((100.1+100.2)*0.5) "
          "(got 0x%08X exp 0x%08X)" % (bits_load, exp_load),
          bits_load == exp_load)
    bits_wd = struct.unpack("<I", b[(0 * H1_COLS + 6) * 4:(0 * H1_COLS + 7) * 4])[0]
    check("bit [0,6] water_depth == 0x41A00000 (f32 20.0, got 0x%08X)" % bits_wd,
          bits_wd == 0x41A00000)
    bits_vref = struct.unpack("<I", b[(0 * H1_COLS + 9) * 4:(0 * H1_COLS + 10) * 4])[0]
    exp_vref = _bits_f32(7.002)
    check("bit [0,9] v_ref right endpoint == RNE(7.002) "
          "(got 0x%08X exp 0x%08X)" % (bits_vref, exp_vref),
          bits_vref == exp_vref)

    # 常量列
    check("col6 water_depth all 20.0", bool(np.all(out1[:, 6] == np.float32(20.0))))
    check("col8 ctrl_mode all 0.0", bool(np.all(out1[:, 8] == np.float32(0.0))))
    check("col10 manual_power_cmd all 0.0",
          bool(np.all(out1[:, 10] == np.float32(0.0))))

    # ---- (b) fail-closed ----
    fresh = H1WindowHost()
    expect_err("tick before begin -> ERR_STATE", ERR_STATE,
               fresh.tick, 0, _fixture_v(0))

    h2 = H1WindowHost()
    h2.begin(0, 20.0)
    h2.tick(0, _fixture_v(0))
    expect_err("skip tick (0 -> 2) -> ERR_ORDER", ERR_ORDER,
               h2.tick, 2, _fixture_v(2))
    check("state intact after failed tick (last_tick==0)", h2.last_tick == 0)
    h2.tick(1, _fixture_v(1))  # 跳拍失败后合法拍仍可继续
    for t in range(2, 302):
        h2.tick(t, _fixture_v(t))
    check("legal tick after skip-failure continues to 301", h2.last_tick == 301)
    out_b = h2.construct(301)
    check("construct succeeds after recovered session", out_b.shape == (300, 11))
    expect_err("duplicate tick -> ERR_ORDER", ERR_ORDER,
               h2.tick, 301, _fixture_v(301))

    h3 = H1WindowHost()
    h3.begin(0, 20.0)
    h3.tick(0, _fixture_v(0))
    bad_t = _fixture_v(1)
    bad_t[0] = 999.0
    expect_err("v[0] != tick_idx -> ERR_TICK_T", ERR_TICK_T, h3.tick, 1, bad_t)
    check("state intact after ERR_TICK_T", h3.last_tick == 0)

    nan_v = _fixture_v(1)
    nan_v[7] = float("nan")
    expect_err("NaN field -> ERR_NONFINITE", ERR_NONFINITE, h3.tick, 1, nan_v)
    nan_t = _fixture_v(1)
    nan_t[0] = float("nan")
    # C 顺序：NaN v[0] 先落 ERR_TICK_T（相等比较失败），非 ERR_NONFINITE
    expect_err("NaN v[0] -> ERR_TICK_T (C order precedence)", ERR_TICK_T,
               h3.tick, 1, nan_t)

    h4 = H1WindowHost()
    h4.begin(0, 20.0)
    for t in range(300):  # 仅 300 拍，last_tick=299 < 300
        h4.tick(t, _fixture_v(t))
    expect_err("construct with last_tick<300 -> ERR_NOT_READY", ERR_NOT_READY,
               h4.construct, 299)

    h5 = H1WindowHost()
    h5.begin(0, 20.0, ctrl_mode=1.0)
    for t in range(302):
        h5.tick(t, _fixture_v(t))
    expect_err("construct with ctrl_mode!=0 -> ERR_POLICY", ERR_POLICY,
               h5.construct, 301)
    expect_err("construct t0 != last_tick -> ERR_NOT_READY", ERR_NOT_READY,
               h5.construct, 300)

    expect_err("begin manual_rule=1 -> ERR_POLICY", ERR_POLICY,
               H1WindowHost().begin, 0, 20.0, 0.0, 1)
    expect_err("begin water_depth=inf -> ERR_NONFINITE", ERR_NONFINITE,
               H1WindowHost().begin, 0, float("inf"))
    expect_err("begin ctrl_mode=nan -> ERR_NONFINITE", ERR_NONFINITE,
               H1WindowHost().begin, 0, 20.0, float("nan"))

    # ---- (c) 冻结列名 ----
    check("COL_NAMES == frozen 11-column order",
          list(COL_NAMES) == [
              "P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
              "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
              "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct"])
    check("RING_CHANNELS == TICK2_TO_RING (1,4,5,7,8,9,3,10)",
          tuple(RING_CHANNELS) == (1, 4, 5, 7, 8, 9, 3, 10))

    # ---- (d) 窗口滚动：construct(302) 的 row[0:299] == construct(301) 的 row[1:300] ----
    h.tick(302, _fixture_v(302))
    samples[302] = [float(x) for x in _fixture_v(302)]
    out2 = h.construct(302)
    check("sliding window: out2[0:299] == out1[1:300]",
          np.array_equal(out2[0:299], out1[1:300]))
    check("sliding window: col6 still all 20.0",
          bool(np.all(out2[:, 6] == np.float32(20.0))))
    check("full bitwise vs independent impl (t0=302)",
          out2.tobytes() == _reference_hist(samples, 302, 20.0))

    print("")
    if failures:
        print("SELFTEST: FAIL (%d/%d checks failed)" % (len(failures), n_pass + len(failures)))
        return False
    print("SELFTEST: PASS (%d checks)" % n_pass)
    return True


def main(argv):
    if "--selftest" in argv:
        return 0 if _selftest() else 1
    print("usage: .venv_tf/bin/python Host/tools/s4_c3_h1win_host.py --selftest")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

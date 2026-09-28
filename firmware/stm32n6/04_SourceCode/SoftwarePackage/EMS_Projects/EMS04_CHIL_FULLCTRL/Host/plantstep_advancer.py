#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""plantstep_advancer.py — T-604 交付物：被控对象推进器（plant advancer）统一封装
（spec: ems01-chil-v3-closedloop，FR-602 / NFR-601 / AC-602；design §4.1/§6.5）。

统一 API（两后端一致，闭环 harness ems_closedloop.py 与 G-P2 门禁均经本模块驱动）:
    make_advancer(backend='stub'|'ctypes', scenario='S20'|'voyage002')
    adv.init(scenario)     # 加载对应 dylib（ctypes）+ model_initialize 复位 IC；
                           #   可重复调用 = 重置到模型初始条件（SOC=0.60 等）
    d = adv.step(p_gen_kw) # 写入 P_gen（本拍 ZOH）→ 推进 100×0.01 s 子步 →
                           #   返回 11 键信号 dict（键 = T-601 audit root_outports
                           #   1..11 名；值为该拍结束时读数）
    adv.state()            # 前 7 键（= CMD_TICK 载荷 [t, P_load, SOC, v_cmd,
                           #   v_ship, accel, x_ship]，design §3.3 接口零新增）
    adv.fuel()             # fuel_total_kg 累计值（增量由 harness 自行差分）
    adv.voyage_stop()      # Outport 11 (bool)
    adv.close()            # 释放 dylib（stub 为空操作）

ctypes 后端（P3 主案）:
    依赖 scripts/23_codegen_plantstep.m 生成的 C 代码（Host/plantstep/<scenario>/
    HybridShip_ModelD_PlantStep_ert_rtw/）经 tools/build_plantstep.sh 编译出的
    libplantstep_<scenario>.dylib（tools/plantstep_shim.c 封装 ERT 接口）。
    argtypes/restypes 显式声明；数值健康检查：任一返回信号非有限即抛
    PlantStepError（闭环事件环 PLANT_STEP_ABNORMAL 的来源，design §6.3）。

stub 后端（纯 Python，design §6.5 selftest 专用）:
    物理粗糙但接口/列与 ctypes 后端完全一致：P_load≡hotel=100 kW、v_cmd≡0、
    无推进动力学（v/x/accel 恒 0）、fuel 按 0.24 kg/kWh 常数 BSFC 积分、
    SOC 按 P_bat=100−P_gen 以 BatCap=1071.36 kWh 积分（放电为正）；
    voyage_stop 恒 False。仅用于 harness 机制 selftest（帧配对/列完整性/
    终止条件），不得用于任何指标结论。

时间语义（主 Agent codegen 后重点核对项，见 tools/plantstep_shim.c 尾注）:
    - step(p) = set_pgen(p) + 100 次 plantstep_step（每步 0.01 s）+ 读 Y；
      返回 dict 含 t_s（Outport 1）。ERT 根 Outport 写出时机（子步起点
      pre-integration / 终点 post-integration）决定 t_s 读数为 k+1 或 k+0.99：
      __main__ 冒烟按 tick 逐个打印 t_s 供核对；ctypes 冒烟发现 t_s 与整秒
      偏差 >1e-9 时打 WARN（不抛异常——采样对齐口径由 G-P2 门禁逐拍定案）。
    - init() 之后、第一次 step() 之前，ctypes 后端 state() 读数不可信
      （ERT model_initialize 不计算根 Outport，Y 为清零内存）；闭环 harness
      的 k=0 拍应按场景数据构造初始行（与 ref_G2 row0 一致：P_load=hotel_kW、
      SOC=Bat_SOC_init、v_cmd=VOYAGE_V_VALUE_MPS[0]、v_ship=x=accel=fuel=
      P_bat=P_prop=0、t=0）。

路径纪律（AGENTS §20.2）: 全部由本文件位置相对推导，禁绝对路径。
Python 环境: 仓库 model/simulink/.venv_tf（纯 stdlib，无第三方依赖）。
运行: python3 plantstep_advancer.py --backend stub   （默认 stub，无 dylib 依赖）
      python3 plantstep_advancer.py --backend ctypes --scenario S20
"""
import argparse
import ctypes
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))          # .../EMS01_CHIL_ECMS/Host
PLANTSTEP_DIR = os.path.join(HERE, "plantstep")
PLANTSTEP_DIR_E = os.path.join(HERE, "plantstep_e")
SUBSTEPS_PER_TICK = 100                                    # 1 s 控制拍 = 100×0.01 s（FD5）
DT_SUBSTEP = 0.01

# 11 键顺序 = 根 Outport 1..11（T-601 audit root_outports，禁重排）
OUT_KEYS = (
    "t_s", "P_load_total_kW", "SOC", "v_cmd_mps", "v_ship_mps",
    "acceleration_mps2", "x_ship_m", "fuel_total_kg", "P_bat_kW",
    "P_prop_kW", "voyage_stop",
)
# 前 7 键 = CMD_TICK 载荷（design §3.3）
STATE_KEYS = OUT_KEYS[:7]
SCENARIOS = ("S20", "voyage002")

# ---- S4-G2（backend='modelE'）：PlantStepE 场景注册与 16 键输出 ----
# C4 电池（2026-09-29）：新增 S02/S08/S03 三航次烘焙 dylib（23E + build_plantstep_e.sh）
SCENARIOS_E = ("S20_composite_voyage", "S02_downstream", "S08_resistance_bands",
               "S03_upstream")
OUT_KEYS_E = (
    "t_s", "P_load_total_kW", "SOC", "v_cmd_mps", "v_ship_mps",
    "acceleration_mps2", "x_ship_m", "fuel_total_kg", "P_bat_kW",
    "P_prop_kW", "state_DG1", "state_DG2", "P_DG1_kW", "P_DG2_kW",
    "V_dc_V", "voyage_stop",
)
STATE_KEYS_E = OUT_KEYS_E[:7]     # 同 CMD_TICK 语义（按名映射，20E SA-2）
SUBSTEPS_PER_TICK_E = 1000        # 10 s 控制拍 = 1000 × 0.01 s（Model E 拍周期）
DT_SUBSTEP_E = 0.01

# stub 后端常数（粗糙物理，selftest 专用；数值来源同 Model_Parameters 概念口径）
STUB_HOTEL_KW = 100.0
STUB_BSFC_KG_PER_KWH = 0.24
STUB_BAT_CAP_KWH = 1071.36
STUB_SOC_INIT = 0.60


class PlantStepError(Exception):
    """推进器接口/数值健康异常（harness 映射为 PLANT_STEP_ABNORMAL 事件）。"""


def _check_scenario(scenario):
    if scenario not in SCENARIOS:
        raise PlantStepError(
            "scenario 必须为 %s 之一，实际 %r" % (SCENARIOS, scenario))


def _as_out_dict(vals):
    d = {k: float(v) for k, v in zip(OUT_KEYS, vals)}
    bad = [k for k, v in d.items() if not math.isfinite(v)]
    if bad:
        raise PlantStepError("PLANT_STEP_ABNORMAL: 非有限数值 %s" % bad)
    return d


class StubBackend(object):
    """纯 Python 简单记账推进器（design §6.5 selftest 专用，物理粗糙）。"""

    def __init__(self):
        self._scenario = None
        self._initialized = False
        self._tick = 0
        self._t = 0.0
        self._soc = STUB_SOC_INIT
        self._fuel = 0.0

    def init(self, scenario):
        _check_scenario(scenario)
        self._scenario = scenario
        self._tick = 0
        self._t = 0.0
        self._soc = STUB_SOC_INIT     # 初始 SOC=0.60（design §3.4，同模型 IC）
        self._fuel = 0.0
        self._initialized = True

    def step(self, p_gen_kw):
        if not self._initialized:
            raise PlantStepError("stub: 先 init(scenario)")
        p = float(p_gen_kw)
        if not math.isfinite(p):
            raise PlantStepError("stub: P_gen 非有限")
        p_bat = STUB_HOTEL_KW - p     # 粗记账: P_bat = P_load - P_gen（放电为正）
        for _ in range(SUBSTEPS_PER_TICK):
            self._fuel += p * STUB_BSFC_KG_PER_KWH / 3600.0 * DT_SUBSTEP
            self._soc -= p_bat * DT_SUBSTEP / (STUB_BAT_CAP_KWH * 3600.0)
            self._t += DT_SUBSTEP
        self._tick += 1
        return _as_out_dict((
            self._t, STUB_HOTEL_KW, self._soc, 0.0, 0.0, 0.0, 0.0,
            self._fuel, p_bat, 0.0, 0,
        ))

    def state(self):
        if not self._initialized:
            raise PlantStepError("stub: 先 init(scenario)")
        return {k: v for k, v in self._snapshot().items() if k in STATE_KEYS}

    def _snapshot(self):
        return {
            "t_s": self._t, "P_load_total_kW": STUB_HOTEL_KW, "SOC": self._soc,
            "v_cmd_mps": 0.0, "v_ship_mps": 0.0, "acceleration_mps2": 0.0,
            "x_ship_m": 0.0, "fuel_total_kg": self._fuel,
            "P_bat_kW": 0.0, "P_prop_kW": 0.0, "voyage_stop": 0.0,
        }

    def fuel(self):
        return float(self._fuel)

    def voyage_stop(self):
        return False

    def close(self):
        pass


class CtypesBackend(object):
    """ctypes 加载 libplantstep_<scenario>.dylib 的推进器（P3 主案后端）。"""

    def __init__(self):
        self._lib = None
        self._lib_path = None
        self._scenario = None
        self._initialized = False
        self._tick = 0

    # ---- 库加载与符号绑定（argtypes/restypes 显式，FD5） ----
    def init(self, scenario):
        _check_scenario(scenario)
        lib_path = os.path.join(
            PLANTSTEP_DIR, scenario, "libplantstep_%s.dylib" % scenario)
        if not os.path.isfile(lib_path):
            raise PlantStepError(
                "dylib 不存在: %s（先执行模型工作区 scripts/23_codegen_plantstep.m，"
                "再执行 Host/tools/build_plantstep.sh %s）" % (lib_path, scenario))
        try:
            lib = ctypes.CDLL(lib_path)
        except OSError as exc:
            raise PlantStepError("dylib 加载失败 %s: %s" % (lib_path, exc))
        lib.plantstep_init.argtypes = [ctypes.c_char_p]
        lib.plantstep_init.restype = ctypes.c_int
        lib.plantstep_set_pgen.argtypes = [ctypes.c_double]
        lib.plantstep_set_pgen.restype = None
        lib.plantstep_step.argtypes = []
        lib.plantstep_step.restype = None
        lib.plantstep_get_outputs.argtypes = [ctypes.POINTER(ctypes.c_double)]
        lib.plantstep_get_outputs.restype = ctypes.c_int
        lib.plantstep_get_fuel.argtypes = []
        lib.plantstep_get_fuel.restype = ctypes.c_double
        lib.plantstep_get_voyage_stop.argtypes = []
        lib.plantstep_get_voyage_stop.restype = ctypes.c_int
        lib.plantstep_get_states.argtypes = [ctypes.POINTER(ctypes.c_double)]
        lib.plantstep_get_states.restype = ctypes.c_int
        rc = lib.plantstep_init(scenario.encode("utf-8"))
        if rc != 0:
            raise PlantStepError(
                "plantstep_init('%s') 失败 rc=%d（-2 = 与 dylib 烘焙场景不符）"
                % (scenario, rc))
        self._lib = lib
        self._lib_path = lib_path
        self._scenario = scenario
        self._tick = 0
        self._initialized = True

    def step(self, p_gen_kw):
        self._require_init()
        p = float(p_gen_kw)
        if not math.isfinite(p):
            raise PlantStepError("ctypes: P_gen 非有限")
        self._lib.plantstep_set_pgen(p)
        for _ in range(SUBSTEPS_PER_TICK):
            self._lib.plantstep_step()
        self._tick += 1
        return self._read_outputs()

    def state(self):
        self._require_init()
        outs = self._read_outputs()
        return {k: outs[k] for k in STATE_KEYS}

    def states(self):
        """连续状态直读（拍边界真值，修正 ERT 末级阶段写出语义的读点偏移）:
        keys = x_ship_m, v_ship_mps, omega_prop_radps, drive_lag_s, SOC,
               V_dc_V, p_gen_inertia_kW, ki_integrator, fuel_total_kg"""
        self._require_init()
        buf = (ctypes.c_double * 9)()
        rc = self._lib.plantstep_get_states(buf)
        if rc != 0:
            raise PlantStepError("plantstep_get_states 失败 rc=%d" % rc)
        vals = [float(x) for x in buf]
        for x in vals:
            if not math.isfinite(x):
                raise PlantStepError("PLANT_STEP_ABNORMAL: 连续状态非有限")
        keys = ("x_ship_m", "v_ship_mps", "omega_prop_radps", "drive_lag_s",
                "SOC", "V_dc_V", "p_gen_inertia_kW", "ki_integrator",
                "fuel_total_kg")
        return dict(zip(keys, vals))

    def fuel(self):
        self._require_init()
        v = float(self._lib.plantstep_get_fuel())
        if not math.isfinite(v):
            raise PlantStepError("PLANT_STEP_ABNORMAL: fuel 非有限")
        return v

    def voyage_stop(self):
        self._require_init()
        return bool(self._lib.plantstep_get_voyage_stop())

    def close(self):
        if self._lib is not None:
            try:
                ctypes.CDLL(None).dlclose(self._lib._handle)  # POSIX dlclose
            except Exception:
                pass
            self._lib = None
            self._lib_path = None
            self._scenario = None
            self._initialized = False
            self._tick = 0

    # ---- 内部 ----
    def _require_init(self):
        if not self._initialized:
            raise PlantStepError("ctypes: 先 init(scenario)")

    def _read_outputs(self):
        buf = (ctypes.c_double * len(OUT_KEYS))()
        rc = self._lib.plantstep_get_outputs(buf)
        if rc != 0:
            raise PlantStepError("plantstep_get_outputs 失败 rc=%d" % rc)
        return _as_out_dict(tuple(buf))


class ModelEBackend(object):
    """S4-G2：ctypes 加载 libplantstepE_<scenario>.dylib 的 Model E 推进器
    （10 s 控制拍 = 1000 × 0.01 s 子步；GRT 产物，Model D 先例同路径）。
    接口与 CtypesBackend 一致：init/step/state/states/fuel/voyage_stop/close。
    states() 键序 = shim plantstep_get_states（V_dc/SOC/DriveLag/Ki/v/x/omega/
    int_3/int_1/int_2，与生成头 X 定义序一致）。"""

    def __init__(self):
        self._lib = None
        self._lib_path = None
        self._scenario = None
        self._initialized = False
        self._tick = 0

    def init(self, scenario):
        if scenario not in SCENARIOS_E:
            raise PlantStepError(
                "modelE: scenario 必须为 %s 之一，实际 %r" % (SCENARIOS_E, scenario))
        lib_path = os.path.join(
            PLANTSTEP_DIR_E, scenario, "libplantstepE_%s.dylib" % scenario)
        if not os.path.isfile(lib_path):
            raise PlantStepError(
                "dylib 不存在: %s（先跑模型工作区 scripts/23E_codegen_plantstepE.m，"
                "再跑 Host/tools/build_plantstep_e.sh %s）" % (lib_path, scenario))
        try:
            lib = ctypes.CDLL(lib_path)
        except OSError as exc:
            raise PlantStepError("dylib 加载失败 %s: %s" % (lib_path, exc))
        lib.plantstep_init.argtypes = [ctypes.c_char_p]
        lib.plantstep_init.restype = ctypes.c_int
        lib.plantstep_set_pgen.argtypes = [ctypes.c_double]
        lib.plantstep_set_pgen.restype = None
        lib.plantstep_step.argtypes = []
        lib.plantstep_step.restype = None
        lib.plantstep_get_outputs.argtypes = [ctypes.POINTER(ctypes.c_double)]
        lib.plantstep_get_outputs.restype = ctypes.c_int
        lib.plantstep_get_fuel.argtypes = []
        lib.plantstep_get_fuel.restype = ctypes.c_double
        lib.plantstep_get_voyage_stop.argtypes = []
        lib.plantstep_get_voyage_stop.restype = ctypes.c_int
        lib.plantstep_get_states.argtypes = [ctypes.POINTER(ctypes.c_double)]
        lib.plantstep_get_states.restype = ctypes.c_int
        rc = lib.plantstep_init(scenario.encode("utf-8"))
        if rc != 0:
            raise PlantStepError(
                "plantstep_init('%s') 失败 rc=%d（-2 = 与 dylib 烘焙场景不符）"
                % (scenario, rc))
        self._lib = lib
        self._lib_path = lib_path
        self._scenario = scenario
        self._tick = 0
        self._initialized = True

    def step(self, p_gen_kw):
        self._require_init()
        p = float(p_gen_kw)
        if not math.isfinite(p):
            raise PlantStepError("modelE: P_gen 非有限")
        self._lib.plantstep_set_pgen(p)
        for _ in range(SUBSTEPS_PER_TICK_E):
            self._lib.plantstep_step()
        self._tick += 1
        return self._read_outputs()

    def state(self):
        self._require_init()
        outs = self._read_outputs()
        return {k: outs[k] for k in STATE_KEYS_E}

    def states(self):
        """连续状态直读（拍边界真值；keys 与 shim X 顺序一致）。"""
        self._require_init()
        buf = (ctypes.c_double * 10)()
        rc = self._lib.plantstep_get_states(buf)
        if rc != 0:
            raise PlantStepError("plantstep_get_states 失败 rc=%d" % rc)
        vals = [float(x) for x in buf]
        for x in vals:
            if not math.isfinite(x):
                raise PlantStepError("PLANT_STEP_ABNORMAL: 连续状态非有限")
        keys = ("V_dc_V", "SOC", "drive_lag_s", "ki_integrator",
                "v_ship_mps", "x_ship_m", "omega_prop_radps",
                "fuel_int_3", "fuel_int_1", "fuel_int_2")
        return dict(zip(keys, vals))

    def fuel(self):
        self._require_init()
        v = float(self._lib.plantstep_get_fuel())
        if not math.isfinite(v):
            raise PlantStepError("PLANT_STEP_ABNORMAL: fuel 非有限")
        return v

    def voyage_stop(self):
        self._require_init()
        return bool(self._lib.plantstep_get_voyage_stop())

    def close(self):
        if self._lib is not None:
            try:
                ctypes.CDLL(None).dlclose(self._lib._handle)
            except Exception:
                pass
            self._lib = None
            self._lib_path = None
            self._scenario = None
            self._initialized = False
            self._tick = 0

    def _require_init(self):
        if not self._initialized:
            raise PlantStepError("modelE: 先 init(scenario)")

    def _read_outputs(self):
        buf = (ctypes.c_double * len(OUT_KEYS_E))()
        rc = self._lib.plantstep_get_outputs(buf)
        if rc != 0:
            raise PlantStepError("plantstep_get_outputs 失败 rc=%d" % rc)
        d = {k: float(v) for k, v in zip(OUT_KEYS_E, buf)}
        bad = [k for k, v in d.items() if not math.isfinite(v)]
        if bad:
            raise PlantStepError("PLANT_STEP_ABNORMAL: 非有限数值 %s" % bad)
        return d


def make_advancer(backend="stub", scenario="S20"):
    """工厂函数（design §4.1）：backend='ctypes'|'stub'；返回已 init 的推进器。
    init(scenario) 可再次调用以复位 IC（= model_initialize / stub 重置）。"""
    if backend == "ctypes":
        adv = CtypesBackend()
    elif backend == "stub":
        adv = StubBackend()
    elif backend == "modelE":
        adv = ModelEBackend()
    else:
        raise PlantStepError("backend 必须为 'ctypes'|'stub'|'modelE'，实际 %r" % backend)
    adv.init(scenario)
    return adv


def _smoke(backend, scenario, ticks, p_gen):
    """__main__ 冒烟：IC 复位 + 逐拍推进数值合理性（AC-602 wrapper 冒烟）。"""
    adv = make_advancer(backend=backend, scenario=scenario)
    print("[adv] backend=%s scenario=%s substeps/tick=%d" %
          (backend, scenario, SUBSTEPS_PER_TICK))
    if backend == "stub":
        s0 = adv.state()
        print("[adv] t=0 初始状态（stub 构造 IC）: %s" %
              ", ".join("%s=%.9g" % (k, s0[k]) for k in STATE_KEYS))
    else:
        print("[adv] NOTE: ctypes 后端 init 后 Y 为清零内存（ERT initialize 不算"
              "根 Outport），k=0 拍由 harness 按场景数据构造（见模块头注释）")
    for k in range(ticks):
        d = adv.step(p_gen)
        ts = d["t_s"]
        line = ", ".join("%s=%.9g" % (key, d[key]) for key in OUT_KEYS)
        print("[adv] tick %d: %s" % (k, line))
        if abs(ts - round(ts)) > 1e-9:
            print("[adv] WARN: t_s=%.4f 非整秒——ERT Outport 写出时机为子步起点"
                  "(pre-integration)，采样对齐口径以 G-P2 门禁逐拍对照定案"
                  % ts)
    if backend == "stub":
        assert abs(adv.state()["SOC"] - (STUB_SOC_INIT - ticks *
                   (STUB_HOTEL_KW - p_gen) / (STUB_BAT_CAP_KWH * 3600.0))) < 1e-12
        assert abs(adv.fuel() - ticks * p_gen * STUB_BSFC_KG_PER_KWH / 3600.0) < 1e-12
        assert adv.voyage_stop() is False
    adv.close()
    print("[adv] 冒烟 OK（%d 拍，P_gen=%.6g kW）" % (ticks, p_gen))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="plantstep advancer 冒烟（T-604）")
    ap.add_argument("--backend", choices=("stub", "ctypes", "modelE"), default="stub")
    ap.add_argument("--scenario", choices=SCENARIOS + SCENARIOS_E, default="S20")
    ap.add_argument("--ticks", type=int, default=3)
    ap.add_argument("--pgen", type=float, default=20.0)
    args = ap.parse_args(argv)
    try:
        return _smoke(args.backend, args.scenario, args.ticks, args.pgen)
    except PlantStepError as exc:
        print("FAIL: %s" % exc, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
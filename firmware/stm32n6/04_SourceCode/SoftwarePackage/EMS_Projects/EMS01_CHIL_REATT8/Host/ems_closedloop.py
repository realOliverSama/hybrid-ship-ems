#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ems_closedloop.py — T-607：soft-HIL 闭环 lock-step harness 主循环 CLI
（spec: ems01-chil-v3-closedloop，FR-606/FR-607/NFR-602/AC-603；
 design §4.4a 读点语义 / §6.1-6.5 harness 设计 / §7.1 臂入口）。

lock-step 主循环（design §6.2，每拍）:
    1. 读被控对象状态: tick 0 = 构造态（t=0、P_load=hotel、SOC=Bat_SOC_init、
       v_cmd=VOYAGE 表首值、v_ship=accel=x_ship=0、fuel=0——实现上取自
       ref_G2 row0，与 gp1 replay row0 同口径）；tick k≥1 = 上拍 step() 后的
       Y 11 键读数（ERT 末级阶段时刻 t=k−0.0025，design §4.4a）。
    2. 7 状态 f32 量化（struct '<f' 往返，即线上实际值）→ enc_cmd_tick →
       send(CMD_TICK) → wait_reply(RSP_TICK, --rsp-timeout-ms) → dec_rsp_tick。
    3. advancer.step(P_gen_ref)（100×0.01 s 子步，ZOH）→ Y 11 项 → 记录行。
    4. 事件环（主机侧）: VOYAGE_STOP / PLANT_STEP_ABNORMAL / TIMEOUT /
       IDX_MISMATCH——写 meta 事件清单；固件侧事件环语义不动（T-609 另扩）。
    5. 终止: 固定拍数 N（S20=5521、voyage002=4212，含 tick0）为主；
       voyage_stop 提前 → 记 VOYAGE_STOP 事件并终止（兜底断言）。

Y 读点语义（design §4.4a，FD2）: CMD_TICK(k) 的状态 = 上拍 100 子步后的 Y
（t=k−0.0025，2.5 ms 早于拍边界，控制影响可忽略）；accel 列 = v_ship 相邻
tick 差（tick0=0，按记录值差分）；t 字段发送 Y.t_s（0.9975+k 型，Δ=1.0 满足
固件 EML tick 判定）。

两种模式:
    board（--port <tty> --image npu|ref）: 串口事务（PING 预同步 + CMD_RESET
        + 每拍 CMD_TICK/RSP_TICK）；RSP 字段为板上解析值。
    replay（缺省，无板）: 虚拟板 = 直接以 ref_G2 的 P_gen_ref_kW[tick] 驱动
        advancer（跳过串口事务，其余流程/记录/终止完全一致）；RSP 遥测字段
        （e_hat/K/cls_eff/flags/infer_us/ctrl_us）记 NaN，meta mode=replay。
        ——AC-603 冒烟基础 + T-608 selftest 载体。

整跑重做（OQ-4）: mid-run 失败（串口超时/idx 错位/推进器异常）→ 本次运行
作废：轨迹 CSV 不落盘（写 <csv>.aborted 存已收集行供排查），meta JSON 标
status=aborted + 错误信息，退出码 1；成功退出 0。不做断点续跑。

纪律: 协议常量/帧编解码只 import 自 Host/tests/test_proto_loopback.py（冻结
单一来源）；SerialSession 非阻塞收向原样复用（import 自 ems_inject）；纯
stdlib + pyserial（仅串口模式）+ 本目录 import；find_repo_root 上溯；本文件
位置相对推导路径，禁绝对路径（AGENTS §20.2）。Python 环境:
REPO/model/simulink/.venv_tf（pyserial 在内）。

运行:
    python3 ems_closedloop.py --scenario S20                 # replay 无板冒烟
    python3 ems_closedloop.py --scenario S20 --port /dev/ttyUSB0 --image ref
    python3 ems_closedloop.py --selftest                     # replay+stub 30 拍自检
产物（--out，缺省 Host/results/closedloop/<scenario>/）:
    closedloop_<scenario>_<image>.csv        逐拍 19 列（%.17g）
    closedloop_<scenario>_<image>_meta.json  meta（FD3 字段齐）
"""
import argparse
import csv
import hashlib
import json
import math
import os
import struct
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))              # .../EMS01/Host
TESTS_DIR = os.path.join(HERE, "tests")

# 协议冻结单一来源：帧构造/编解码只从 test_proto_loopback import（勿双实现）
sys.path.insert(0, TESTS_DIR)
from test_proto_loopback import (  # noqa: E402
    CMD_RESET, CMD_TICK, RSP_TICK, CMD_PING, ACK,
    enc_cmd_tick, dec_rsp_tick,
)
# SerialSession（非阻塞收向/事务超时语义）与 7 状态序常量复用 ems_inject
sys.path.insert(0, HERE)
from ems_inject import SerialSession, TRACE_COLS  # noqa: E402
from plantstep_advancer import make_advancer, PlantStepError  # noqa: E402


def find_repo_root(start_dir):
    """向上找同时含 AGENTS.md + model/ + firmware/ 的目录（仓库根）。
    惯例来源: Host/tests/test_l0_trace_diff.py / tools/run_gp2_gate.py。"""
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise RuntimeError("无法定位仓库根（AGENTS.md + model/ + firmware/）")
        d = parent


REPO_ROOT = find_repo_root(HERE)
MODEL_DIR_NAME = "model/simulink_modelD_(EMS_NN_AECMS)"
FW_REF_DIR = os.path.join(REPO_ROOT, MODEL_DIR_NAME,
                          "results", "ecms_phase25", "fw_reference")

# 场景表（design §7.1/§6.2: 拍数含 tick0；ref = 回放 P_gen 源，v1.1 冻结基线）
SCENARIOS = {
    "S20": {"ref": "ref_G2_S20_composite_voyage_matrix.csv", "ticks": 5521},
    "voyage002": {"ref": "ref_G2_test_voyage_002_v2.csv", "ticks": 4212},
}

# 逐拍记录 19 列（design §6.3；%.17g）。7 状态列记录 f32 量化后（线上实际值）；
# P_gen/e_hat/K/cls_eff/flags/infer_us/ctrl_us 为 RSP 解析值（replay 模式遥测
# 列为 NaN）；fuel/P_bat/P_prop/voyage_stop 为 Y 值。
RECORD_COLS = (
    "tick", "t_s", "P_load_kW", "SOC", "v_cmd_mps", "v_ship_mps",
    "accel_mps2", "x_ship_m", "P_gen_kW", "e_hat", "K", "cls_eff", "flags",
    "infer_us", "ctrl_us", "fuel_total_kg", "P_bat_kW", "P_prop_kW",
    "voyage_stop",
)
RSP_FIELDS = ("e_hat", "K", "cls_eff", "flags", "infer_us", "ctrl_us")

META_REQUIRED_FIELDS = (
    "task", "spec", "generated", "scenario", "image", "mode", "backend",
    "plantstep", "ref_csv", "out_csv", "start_ic", "tick_time",
    "termination_reason", "events", "status", "rows", "n_ticks_planned",
    "rsp_timeout_ms", "error",
)


def _f32(v):
    """f32 量化往返（线上实际值；enc_cmd_tick 的 '<f' 语义一致）。"""
    return struct.unpack("<f", struct.pack("<f", float(v)))[0]


def load_ref_pgen(path):
    """ref_G2 CSV → P_gen_ref_kW 列（按 tick 序，float list）。"""
    out = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            out.append(float(r["P_gen_ref_kW"]))
    return out


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _pct(seq, p):
    s = sorted(seq)
    if not s:
        return float("nan")
    i = min(len(s) - 1, max(0, int(round(p / 100.0 * (len(s) - 1)))))
    return s[i]


def run_closedloop(args):
    """主流程。返回 dict(status, out_csv, meta, error)。mid-run 失败按 FD5
    整跑作废（<csv>.aborted 落已收集行；meta status=aborted）。"""
    if args.scenario not in SCENARIOS:
        raise ValueError("非法 scenario: %r" % (args.scenario,))
    scn = SCENARIOS[args.scenario]
    mode = "replay" if (args.port is None or args.image == "replay") else "board"
    n_ticks = int(args.max_ticks) if args.max_ticks else scn["ticks"]
    ems01 = os.path.dirname(HERE)
    out_dir = args.out or os.path.join(ems01, "Host", "results", "closedloop",
                                       args.scenario)
    os.makedirs(out_dir, exist_ok=True)
    base = "closedloop_%s_%s" % (args.scenario, args.image)
    out_csv = os.path.join(out_dir, base + ".csv")
    out_meta = os.path.join(out_dir, base + "_meta.json")

    ref_path = os.path.join(args.runs_dir or FW_REF_DIR, scn["ref"])
    ref_pgen = load_ref_pgen(ref_path)
    with open(ref_path, newline="") as f:
        row0 = next(csv.DictReader(f))
    start_ic = {
        "t": 0.0,
        "P_load_kW": float(row0["P_load_kW"]),        # hotel（S20/v002 均 100）
        "SOC": float(row0["SOC"]),                    # Bat_SOC_init=0.60
        "v_cmd_mps": float(row0["v_cmd_mps"]),        # VOYAGE 表首值
        "v_ship_mps": float(row0["v_ship_mps"]),      # 0
        "accel_mps2": 0.0,
        "x_ship_m": float(row0["x_ship_m"]),          # 0
        "fuel_total_kg": 0.0,
    }
    state7 = [start_ic["t"], start_ic["P_load_kW"], start_ic["SOC"],
              start_ic["v_cmd_mps"], start_ic["v_ship_mps"],
              start_ic["accel_mps2"], start_ic["x_ship_m"]]

    plantstep_meta = {"backend": args.advancer}
    if args.advancer == "ctypes":
        dylib = os.path.join(ems01, "Host", "plantstep", args.scenario,
                             "libplantstep_%s.dylib" % args.scenario)
        plantstep_meta["dylib"] = dylib
        plantstep_meta["dylib_sha256"] = (sha256_file(dylib)
                                          if os.path.isfile(dylib) else None)
        plantstep_meta["build_manifest"] = os.path.join(
            ems01, "Host", "plantstep", args.scenario, "build_manifest.json")
    else:
        plantstep_meta["dylib"] = None
        plantstep_meta["dylib_sha256"] = None
        plantstep_meta["build_manifest"] = None

    adv = make_advancer(backend=args.advancer, scenario=args.scenario)
    events = []
    rows = []
    tick_dt = []
    termination = "tick_count_reached"
    error_msg = ""
    sess = None
    status = "completed"
    prev_v = None

    def _abort(reason, detail):
        """FD5: 整跑作废（记录事件；调用方统一落 aborted 产物）。"""
        events.append({"type": reason, "tick": len(rows), "detail": detail})
        return reason

    try:
        if mode == "board":
            sess = SerialSession(dev=args.port)
            ok = False
            for _ in range(5):                       # PING 预同步 ≤5 次
                sess.send(CMD_PING)
                if sess.wait_reply(ACK, timeout_ms=400) is not None:
                    ok = True
                    break
            if not ok:
                _abort("PING_TIMEOUT", "PING 预同步 5 次无 ACK")
                raise RuntimeError("board 打开失败: PING 预同步无 ACK")
            sess.send(CMD_RESET)
            if sess.wait_reply(ACK, timeout_ms=500) is None:
                _abort("RESET_TIMEOUT", "CMD_RESET 无 ACK")
                raise RuntimeError("board 打开失败: CMD_RESET 无 ACK")

        for k in range(n_ticks):
            t_tick = time.perf_counter()
            # ---- 1. 本拍 7 状态（f32 量化 = 线上实际值） ----
            if k == 0:
                accel = 0.0
                state7[5] = 0.0
            else:
                accel = state7[4] - prev_v
                state7[5] = accel
            rec7 = [_f32(v) for v in state7]         # [t,P_load,SOC,v_cmd,v_ship,accel,x]
            p_gen_ref = None
            e_hat = K = cls_eff = flags = infer_us = ctrl_us = float("nan")

            # ---- 2. 决策（board=串口事务 / replay=参考 P_gen） ----
            if mode == "board":
                sess.send(CMD_TICK, enc_cmd_tick(k, rec7))
                payload = sess.wait_reply(RSP_TICK,
                                          timeout_ms=args.rsp_timeout_ms)
                if payload is None:
                    _abort("TIMEOUT", "RSP_TICK 超时（%d ms）"
                           % args.rsp_timeout_ms)
                    raise RuntimeError("tick %d: RSP_TICK 超时" % k)
                rsp = dec_rsp_tick(payload)
                if rsp.get("idx") != k:
                    _abort("IDX_MISMATCH",
                           "RSP idx=%r 期望 %d" % (rsp.get("idx"), k))
                    raise RuntimeError("tick %d: RSP idx 错位" % k)
                p_gen_ref = float(rsp["P_gen_ref"])
                e_hat = float(rsp["e_hat"])
                K = float(rsp["K"])
                cls_eff = float(rsp["cls"])
                flags = float(rsp["flags"])
                infer_us = float(rsp["us_infer"])
                ctrl_us = float(rsp["us_ctrl"])
            else:
                if k >= len(ref_pgen):
                    _abort("REF_EXHAUSTED", "回放 P_gen 序列耗尽")
                    raise RuntimeError("tick %d: 回放序列耗尽" % k)
                p_gen_ref = ref_pgen[k]

            # ---- 3. 被控对象推进（100×0.01 s，ZOH）→ Y 11 项 ----
            y = adv.step(p_gen_ref)

            # ---- 4/5. 记录行 + 终止判定 ----
            row = [k, rec7[0], rec7[1], rec7[2], rec7[3], rec7[4], rec7[5],
                   rec7[6], p_gen_ref, e_hat, K, cls_eff, flags, infer_us,
                   ctrl_us, y["fuel_total_kg"], y["P_bat_kW"],
                   y["P_prop_kW"], float(y["voyage_stop"])]
            rows.append(row)
            prev_v = rec7[4]
            tick_dt.append(time.perf_counter() - t_tick)
            if y["voyage_stop"]:
                events.append({"type": "VOYAGE_STOP", "tick": k,
                               "detail": "Outport 11 提前置位（x_ship=%.6g m）"
                                         % y["x_ship_m"]})
                termination = "voyage_stop"
                break
            if k + 1 < n_ticks:
                state7 = [y["t_s"], y["P_load_total_kW"], y["SOC"],
                          y["v_cmd_mps"], y["v_ship_mps"], 0.0, y["x_ship_m"]]
    except PlantStepError as exc:
        status = "aborted"
        termination = "error"
        _abort("PLANT_STEP_ABNORMAL", str(exc))
        error_msg = "PLANT_STEP_ABNORMAL: %s" % exc
    except RuntimeError as exc:
        status = "aborted"
        termination = "error"
        error_msg = str(exc)
    finally:
        if sess is not None:
            sess.close()
        try:
            adv.close()
        except Exception:
            pass

    # ---- 产物落盘（FD5: 成功写 CSV+meta；作废写 .aborted+meta） ----
    if status == "completed":
        with open(out_csv, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(RECORD_COLS)
            for row in rows:
                w.writerow([row[0]] + ["%.17g" % v for v in row[1:]])
    else:
        with open(out_csv + ".aborted", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(RECORD_COLS)
            for row in rows:
                w.writerow([row[0]] + ["%.17g" % v for v in row[1:]])

    tick_time = {
        "mean": (sum(tick_dt) / len(tick_dt)) if tick_dt else None,
        "p50": _pct(tick_dt, 50),
        "p95": _pct(tick_dt, 95),
        "max": max(tick_dt) if tick_dt else None,
        "unit": "s",
        "n": len(tick_dt),
    }
    meta = {
        "task": "T-607",
        "spec": "ems01-chil-v3-closedloop (FR-606/FR-607/NFR-602/AC-603)",
        "generated": time.strftime("%Y-%m-%d %H:%M:%S"),
        "scenario": args.scenario,
        "image": args.image,
        "mode": mode,
        "backend": args.advancer,
        "plantstep": plantstep_meta,
        "ref_csv": ref_path,
        "out_csv": out_csv if status == "completed" else out_csv + ".aborted",
        "start_ic": start_ic,
        "tick_time": tick_time,
        "termination_reason": termination,
        "events": events,
        "status": status,
        "rows": len(rows),
        "n_ticks_planned": n_ticks,
        "rsp_timeout_ms": args.rsp_timeout_ms,
        "error": error_msg,
    }
    with open(out_meta, "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=1)
    return {"status": status, "out_csv": meta["out_csv"], "meta": meta,
            "error": error_msg}


def selftest_main():
    """--selftest: replay + stub + 30 拍（AC-603 无板自检；机制断言，无板无 dylib）。"""
    out_dir = tempfile.mkdtemp(prefix="ems_closedloop_selftest_")
    class _Args(object):
        pass
    args = _Args()
    args.scenario = "S20"
    args.port = None
    args.image = "replay"
    args.advancer = "stub"
    args.out = out_dir
    args.runs_dir = None
    args.rsp_timeout_ms = 500
    args.max_ticks = 30
    checks = []
    try:
        r = run_closedloop(args)
        meta = r["meta"]
        checks.append(("status=completed", r["status"] == "completed"))
        checks.append(("termination=tick_count_reached",
                       meta["termination_reason"] == "tick_count_reached"))
        checks.append(("rows=30", meta["rows"] == 30))
        csv_path = r["out_csv"]
        with open(csv_path, newline="") as f:
            rr = list(csv.reader(f))
        checks.append(("列完整性（19 列表头逐字符一致）",
                       tuple(rr[0]) == tuple(RECORD_COLS)))
        checks.append(("数据行数=30", len(rr) - 1 == 30))
        t_col = [float(x[1]) for x in rr[1:]]
        checks.append(("t_s 单调 +1（stub 整秒语义）",
                       all(abs(t_col[i] - i) < 1e-9 for i in range(len(t_col)))))
        missing = [k for k in META_REQUIRED_FIELDS if k not in meta]
        checks.append(("meta 必备字段齐", not missing))
        checks.append(("mode=replay", meta["mode"] == "replay"))
        checks.append(("tick_time 分布齐",
                       all(meta["tick_time"].get(k) is not None
                           for k in ("mean", "p50", "p95", "max"))))
        checks.append(("无错误事件", not meta["events"]))
    except Exception as exc:  # noqa: BLE001
        checks.append(("异常: %s" % exc, False))
    ok = all(okk for _, okk in checks)
    for name, okk in checks:
        print("  [%s] %s" % ("OK" if okk else "FAIL", name))
    print("EMS_CLOSEDLOOP SELFTEST %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="T-607 soft-HIL 闭环 lock-step harness（FR-606/607/NFR-602）")
    ap.add_argument("--scenario", default=None, choices=tuple(SCENARIOS),
                    help="回放场景（S20 | voyage002；--selftest 时可省）")
    ap.add_argument("--port", default=None,
                    help="串口设备（缺省 = replay 无板模式）")
    ap.add_argument("--image", default="replay", choices=("npu", "ref", "replay"),
                    help="镜像标识（board 模式必填 npu|ref；meta 原样记录）")
    ap.add_argument("--advancer", default="ctypes", choices=("ctypes", "stub"),
                    help="推进器后端（默认 ctypes=codegen C；stub 仅 selftest）")
    ap.add_argument("--out", default=None,
                    help="产物目录（缺省 Host/results/closedloop/<scenario>/）")
    ap.add_argument("--runs-dir", default=None,
                    help="fw_reference 目录（缺省 repo/model/.../fw_reference）")
    ap.add_argument("--rsp-timeout-ms", type=int, default=500,
                    help="RSP_TICK 事务超时（ms，design §6.2 沿袭 500）")
    ap.add_argument("--max-ticks", type=int, default=None,
                    help="最大拍数（缺省 = 场景拍数：S20=5521 / voyage002=4212）")
    ap.add_argument("--selftest", action="store_true",
                    help="无板自检（replay+stub，30 拍，机制断言）")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest_main()
    if not args.scenario:
        ap.error("--scenario 必填（S20 | voyage002；--selftest 时可省）")
    if args.port is None and args.image != "replay":
        print("ERROR: --image %s 需要串口（--port）；无板运行请用 --image replay"
              % args.image, file=sys.stderr)
        return 2
    if args.port is not None and args.image == "replay":
        print("ERROR: --port 与 --image replay 冲突（串口模式请 --image npu|ref）",
              file=sys.stderr)
        return 2
    r = run_closedloop(args)
    if r["status"] != "completed":
        print("FAIL: 闭环运行作废（%s）——meta: %s"
              % (r["error"], r["meta"]["out_csv"]), file=sys.stderr)
        return 1
    print("[closedloop] OK scenario=%s image=%s mode=%s rows=%d 终止=%s "
          "拍耗时 mean=%.3gms p95=%.3gms max=%.3gms"
          % (args.scenario, args.image, r["meta"]["mode"], r["meta"]["rows"],
             r["meta"]["termination_reason"],
             r["meta"]["tick_time"]["mean"] * 1000.0,
             r["meta"]["tick_time"]["p95"] * 1000.0,
             r["meta"]["tick_time"]["max"] * 1000.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())

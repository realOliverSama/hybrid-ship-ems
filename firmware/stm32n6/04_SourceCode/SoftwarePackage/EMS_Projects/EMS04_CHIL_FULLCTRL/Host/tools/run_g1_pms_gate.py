#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_g1_pms_gate.py — S4-G1 门：PMS/Safety C 移植 vs Simulink 逐拍对拍。
参考 = reference/s4_g1_pms_plant_inputs.csv（PlantStepE 解释执行 60s，
P_gen_ref 阶跃 300→800@30s；行 m = 第 m 步后，tick m 的植物输入喂 C，对比
C 输出（第 m+1 步前 = 拍内计算值）与参考运行同拍 07/16 输出——注意 07/16
内部输出不在根 Outport，本门禁通过【输出面端口】暴露：simref CSV 由
MATLAB 侧第二通道导出（G1_outputs.csv），此处消费两个 CSV。"""
import csv, ctypes, json, os, sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)

lib = ctypes.CDLL(os.environ.get("G1_LIB", "/tmp/s4g1/libems04_pms_safety.dylib"))
lib.ems04_pms_init.argtypes = [ctypes.c_void_p]
lib.ems04_pms_tick.argtypes = [ctypes.c_void_p, ctypes.c_void_p] + [ctypes.c_double]*13 + [ctypes.c_void_p]
lib.ems04_pms_tick.restype = None


class Params(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in (
        "PMS_LoadLPF_tau_s","PMS_SOC_lo","PMS_SOC_recovered","PMS_P_chg_recovery_kW",
        "PMS_start_threshold_kW","PMS_stop_threshold_kW","T_start_hold_ticks",
        "T_stop_hold_ticks","PMS_min_up_ticks","PMS_S2_hold_ticks","PMS_S4_hold_ticks",
        "PMS_start_fail_ticks","PMS_firm_margin_kW","ASSUMED_eta_rect",
        "DG_P_min_kW","DG_P_rated_kW","PMS_SOURCE_MODE","ADAPTER_FORCE_MODE",
        "ADAPTER_FORCE_P1_KW","ADAPTER_FORCE_P2_KW")]


class Arr2(ctypes.Structure):
    _fields_ = [("v", ctypes.c_double * 2)]


class Arr2(ctypes.Structure):
    _fields_ = [("v", ctypes.c_double * 2)]


class State(ctypes.Structure):
    # 布局必须与 C 一致：u_rec/policy_init_done/chart_entered/safety_init_done 是 int(4B)
    _fields_ = [
        ("P_load_filt", ctypes.c_double), ("u_rec", ctypes.c_int32),
        ("policy_init_done", ctypes.c_int32),
        ("commitment_state", ctypes.c_double), ("reason_code", ctypes.c_double),
        ("start_hold_timer", ctypes.c_double), ("stop_hold_timer", ctypes.c_double),
        ("online_timer", ctypes.c_double), ("pending_reason", ctypes.c_double),
        ("tr_reason", ctypes.c_double), ("chart_entered", ctypes.c_int32),
        ("fl_latch", Arr2), ("prev_state", Arr2), ("prev_fstp", Arr2),
        ("s2_cnt", ctypes.c_double), ("s4_cnt", ctypes.c_double),
        ("s1b_ticks", Arr2),
        ("fback_active", ctypes.c_double), ("fback_reason", ctypes.c_double),
        ("min_srp", ctypes.c_double), ("safety_init_done", ctypes.c_int32),
        ("UD_state_DG1", ctypes.c_double), ("UD_state_DG2", ctypes.c_double),
        ("UD_N_dispatch_fb", ctypes.c_double),
        ("UD_fault_latch", Arr2), ("UD_force_start_req", Arr2),
        ("UD_force_start_reason", ctypes.c_double),
        ("UD_force_stop_req", Arr2), ("UD_force_stop_reason", ctypes.c_double),
        ("UD_ems_health_ok", ctypes.c_double),
        ("prev_cmd1", ctypes.c_double), ("prev_cmd2", ctypes.c_double),
        ("Vdc_V_in", ctypes.c_double),
    ]


class Out(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in (
        "P_gen_req_baseline_kW","P_commit_kW","u_recovery","P_recovery_actual_kW",
        "cmd_onoff_DG1","cmd_onoff_DG2","P_DG1_ref_kW","P_DG2_ref_kW","N_dispatch",
        "P_gen_unserved_kW","P_gen_surplus_kW","dispatch_min_clamp_active",
        "dispatch_n_zero_flag","commitment_state","reason_code",
        "fs0","fs1","force_start_reason","fstp0","fstp1","force_stop_reason",
        "fault0","fault1","ems_health_ok","baseline_fallback_active",
        "baseline_fallback_reason")]


GATE_KEYS = [n for n, _ in Out._fields_]


def main():
    ref_in = os.path.join(HOST, "reference", "s4_g1_pms_plant_inputs.csv")
    ref_out = os.path.join(HOST, "reference", "s4_g1_pms_outputs.csv")
    rows_in = list(csv.DictReader(open(ref_in)))
    rows_out = list(csv.DictReader(open(ref_out)))
    if len(rows_in) != len(rows_out):
        print("FATAL: input/output row count mismatch", file=sys.stderr)
        return 2
    P = Params(
        PMS_LoadLPF_tau_s=60.0, PMS_SOC_lo=0.55, PMS_SOC_recovered=0.60,
        PMS_P_chg_recovery_kW=100.0, PMS_start_threshold_kW=425.0,
        PMS_stop_threshold_kW=350.0, T_start_hold_ticks=3000.0,
        T_stop_hold_ticks=12000.0, PMS_min_up_ticks=30000.0,
        PMS_S2_hold_ticks=200.0, PMS_S4_hold_ticks=200.0,
        PMS_start_fail_ticks=4000.0, PMS_firm_margin_kW=50.0,
        ASSUMED_eta_rect=0.97, DG_P_min_kW=125.0, DG_P_rated_kW=500.0,
        PMS_SOURCE_MODE=1.0, ADAPTER_FORCE_MODE=0.0,
        ADAPTER_FORCE_P1_KW=0.0, ADAPTER_FORCE_P2_KW=0.0)
    S = State()
    lib.ems04_pms_init(ctypes.byref(S))
    O = Out()
    TH = 1e-9
    n_pass = n_fail = 0
    fails = []
    snap_keys = ["fs0","fs1","force_start_reason","fstp0","fstp1",
                 "force_stop_reason","ems_health_ok"]
    for ri, ro in zip(rows_in, rows_out):
        # UD 探针（MATLAB index k）= 上一拍喂入值；C 侧同语义 = tick 前快照
        snap = {"fs0": S.UD_force_start_req.v[0], "fs1": S.UD_force_start_req.v[1],
                "force_start_reason": S.UD_force_start_reason,
                "fstp0": S.UD_force_stop_req.v[0], "fstp1": S.UD_force_stop_req.v[1],
                "force_stop_reason": S.UD_force_stop_reason,
                "ems_health_ok": S.UD_ems_health_ok}
        lib.ems04_pms_tick(ctypes.byref(S), ctypes.byref(P),
            float(ri["P_load_total_kW"]), float(ri["SOC"]),
            float(ri["state_DG1"]), float(ri["state_DG2"]),
            float(ri["P_DG1_kW"]), float(ri["P_DG2_kW"]),
            float(ri["P_bat_dis_avail_kW"]), float(ri["P_bat_chg_avail_kW"]),
            float(ri["P_prop_demand_kW"]), float(ri["P_hotel_kW"]),
            float(ri["P_gen_ref_kW"]), 1.0, float(ri["voyage_active"]),
            ctypes.byref(O))
        diffs = {}
        ok = True
        # UD 探针列（force_start/stop/health 的 CSV 列是【上一拍】值）↔ C 侧 S.UD_*
        ud_cols = {"fs0": "UD_force_start_req.v0", "fs1": "UD_force_start_req.v1",
                   "force_start_reason": "UD_force_start_reason",
                   "fstp0": "UD_force_stop_req.v0", "fstp1": "UD_force_stop_req.v1",
                   "force_stop_reason": "UD_force_stop_reason",
                   "ems_health_ok": "UD_ems_health_ok"}
        def cval(key):
            if key in snap: return snap[key]
            return getattr(O, key)
        for k in GATE_KEYS:
            exp = float(ro[k]) if k in ro else float("nan")
            got = cval(k)
            dv = abs(got - exp)
            # NaN 处理：两侧都 NaN 视为相等
            both_nan = (math.isnan(getattr(O, k)) and math.isnan(exp))
            if not both_nan and dv > TH:
                ok = False
                diffs[k] = (got, exp)
        n_pass += 1 if ok else 0
        n_fail += 0 if ok else 1
        if not ok and len(fails) < 10:
            fails.append({"tick": ri["tick"], "diffs": {k: str(v) for k, v in diffs.items()}})
    gate = {
        "schema": "s4_g1_pms_gate_v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "n_ticks": len(rows_in), "n_pass": n_pass, "n_fail": n_fail,
        "threshold": TH, "gate_keys": GATE_KEYS,
        "first_fails": fails,
        "overall": n_fail == 0,
    }
    out = os.path.join(HOST, "reference", "s4_g1_pms_gate.json")
    json.dump(gate, open(out, "w"), indent=1)
    print(f"G1 PMS gate: {n_pass}/{len(rows_in)} ticks exact")
    if fails:
        for f in fails:
            print("  FAIL tick", f["tick"], f["diffs"])
    print("GATE:", "PASS" if gate["overall"] else "FAIL", "->", out)
    return 0 if gate["overall"] else 1


import math
if __name__ == "__main__":
    sys.exit(main())

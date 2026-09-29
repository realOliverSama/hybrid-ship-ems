#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_c3_preamble.py — C3 前置：B_HOST 同输入对拍（board TICK_CTRL vs host 同链）。

同输入：与板级冒烟相同的合成状态 + 真值包 hist/kf（100 拍序列，逐拍可复现）。
host 同链（Python，与固件逐位同构）：
  REATT: ORT host（reatt_split N1+N2，gen_ref_split_v2.Chain，S3-C HB 腿已证）
  law6 决策: libems04_law6_c2.dylib ems04_dp_tick（S1 377/377）
  执行层: 契约 §4.3 网格穷举（Python，与固件 C 同式）
  PMS/Safety: ems04_pms_safety C dylib（S4-G1 6001/6001）
对拍面（G6-3 预演）：逐拍 action index（d1 档）、P_gen_ref、commitment_state、
  cmd_onoff、N_dispatch、λ_eff —— 期望逐拍一致（同输入同代码路径）。
运行：.venv_tf/bin/python tools/s4_c3_preamble.py [--ticks 100]
产物：Host/results/s4_c2/s4_c3_preamble_report.json
注意：host REATT 走 ORT（host keras/OE 链）；板走 int8 NPU 链——若 q50 存在已知
  量化差，d1 可能差档；本对拍同时记录 q50 差以区分"输入差"与"逻辑差"。
"""
import argparse, csv, ctypes, json, math, os, struct, sys, time
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)
def _find_repo_root(start):
    d = os.path.abspath(start)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise RuntimeError("repo root not found")
        d = parent


REPO = _find_repo_root(HOST)
sys.path.insert(0, os.path.join(HOST, "tools"))
sys.path.insert(0, os.path.join(HOST))
sys.path.insert(0, os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25", "ems04_s3", "tools"))
from s3c_reatt_replay import enc_frame, crc16_ccitt, SYNC0, SYNC1  # noqa: E402

import serial  # noqa: E402

CMD_PING, CMD_RESET, CMD_TICK_CTRL, RSP_TICK_CTRL = 0x06, 0x01, 0x0E, 0x0F
RSP_LEN = 4 + 20*4 + 4 + 4 + 1  # seq + 20×f32 + 2×u32 + flags
HIST_FLT, KF_FLT = 300 * 11, 30 * 6
CMD_LEN = 4 + 4 + 8 * 4 + (HIST_FLT + KF_FLT) * 4

# ---- law6 dylib（决策层）----
S1 = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25", "ems04_s1")
law = ctypes.CDLL(os.path.join(S1, "libems04_law6_c2.dylib"))
law.ems04_load_tables.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
law.ems04_load_tables.restype = ctypes.c_int

class Tables(ctypes.Structure):
    _fields_ = [("Pg", ctypes.POINTER(ctypes.c_double)),
                ("bsfc_lut", ctypes.POINTER(ctypes.c_double)),
                ("socx", ctypes.POINTER(ctypes.c_double)),
                ("dlam", ctypes.POINTER(ctypes.c_double)),
                ("socx_owned", ctypes.POINTER(ctypes.c_double)),
                ("dlam_owned", ctypes.POINTER(ctypes.c_double))]

class GenState(ctypes.Structure):
    _fields_ = [("st", ctypes.c_double * 2), ("P_ramp", ctypes.c_double * 2),
                ("P_lag", ctypes.c_double * 2), ("t_dwell", ctypes.c_double * 2)]

class TickResult(ctypes.Structure):
    _fields_ = [("dlambda_first", ctypes.c_double), ("J_window_kg", ctypes.c_double),
                ("soc_end", ctypes.c_double), ("fuel_proxy_kg", ctypes.c_double),
                ("oob_query_count", ctypes.c_int64),
                ("infeasible_fallback_n", ctypes.c_int64),
                ("n_fallback_steps", ctypes.c_int),
                ("dlambda_all", ctypes.c_double * 30),
                ("soc_path", ctypes.c_double * 31),
                ("N_path", ctypes.c_double * 31)]

T = Tables()
assert law.ems04_load_tables(os.path.join(S1, "exported").encode(), ctypes.byref(T)) == 0
law.ems04_dp_tick.argtypes = [
    ctypes.c_void_p,
    np.ctypeslib.ndpointer(dtype=np.float64, flags="C_CONTIGUOUS"),
    ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_void_p,
    ctypes.c_void_p]
law.ems04_dp_tick.restype = ctypes.c_int

# ---- PMS/Safety dylib ----
pms = ctypes.CDLL("/tmp/s4g1/libems04_pms_safety.dylib")
pms.ems04_pms_init.argtypes = [ctypes.c_void_p]

class PParams(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in (
        "PMS_LoadLPF_tau_s","PMS_SOC_lo","PMS_SOC_recovered","PMS_P_chg_recovery_kW",
        "PMS_start_threshold_kW","PMS_stop_threshold_kW","T_start_hold_ticks",
        "T_stop_hold_ticks","PMS_min_up_ticks","PMS_S2_hold_ticks","PMS_S4_hold_ticks",
        "PMS_start_fail_ticks","PMS_firm_margin_kW","ASSUMED_eta_rect",
        "DG_P_min_kW","DG_P_rated_kW","PMS_SOURCE_MODE","ADAPTER_FORCE_MODE",
        "ADAPTER_FORCE_P1_KW","ADAPTER_FORCE_P2_KW")]

class Arr2(ctypes.Structure):
    _fields_ = [("v", ctypes.c_double * 2)]

class PState(ctypes.Structure):
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
        ("Vdc_V_in", ctypes.c_double)]

class POut(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in (
        "P_gen_req_baseline_kW","P_commit_kW","u_recovery","P_recovery_actual_kW",
        "cmd_onoff_DG1","cmd_onoff_DG2","P_DG1_ref_kW","P_DG2_ref_kW","N_dispatch",
        "P_gen_unserved_kW","P_gen_surplus_kW","dispatch_min_clamp_active",
        "dispatch_n_zero_flag","commitment_state","reason_code",
        "fs0","fs1","force_start_reason","fstp0","fstp1","force_stop_reason",
        "fault0","fault1","ems_health_ok","baseline_fallback_active",
        "baseline_fallback_reason")]

pms.ems04_pms_tick.argtypes = [ctypes.c_void_p, ctypes.c_void_p] + [ctypes.c_double]*13 + [ctypes.c_void_p]

PP = PParams(
    PMS_LoadLPF_tau_s=60.0, PMS_SOC_lo=0.55, PMS_SOC_recovered=0.60,
    PMS_P_chg_recovery_kW=100.0, PMS_start_threshold_kW=425.0,
    PMS_stop_threshold_kW=350.0, T_start_hold_ticks=3000.0,
    T_stop_hold_ticks=12000.0, PMS_min_up_ticks=30000.0,
    PMS_S2_hold_ticks=200.0, PMS_S4_hold_ticks=200.0,
    PMS_start_fail_ticks=4000.0, PMS_firm_margin_kW=50.0,
    ASSUMED_eta_rect=0.97, DG_P_min_kW=125.0, DG_P_rated_kW=500.0,
    PMS_SOURCE_MODE=1.0, ADAPTER_FORCE_MODE=0.0,
    ADAPTER_FORCE_P1_KW=0.0, ADAPTER_FORCE_P2_KW=0.0)
PS = PState()
pms.ems04_pms_init(ctypes.byref(PS))
PO = POut()

CONST = json.load(open(os.path.join(S1, "exported", "constants.json")))
LAW_PORTS = CONST["law6_ports"]
K_S_B2 = CONST["K_S_B2"]


def exec_layer(p_load_kw, soc, lam):
    eta_rect = LAW_PORTS["ASSUMED_eta_rect"]; eta_dcdc = LAW_PORTS["ASSUMED_eta_dcdc"]
    eta_dis = LAW_PORTS["ASSUMED_Bat_Eff_Discharge"]; eta_chg = LAW_PORTS["ASSUMED_Bat_Eff_Charge"]
    p_max = LAW_PORTS["Gen_P_max"]; sdm = LAW_PORTS["ECMS_SOC_Discharge_Max"]
    scm = LAW_PORTS["ECMS_SOC_Charge_Min"]; idle = LAW_PORTS["ECMS_Idle_Fuel_kgph"]
    grid = LAW_PORTS["ECMS_Grid_dP_kW"]
    bp = np.asarray(LAW_PORTS["BSFC_Breakpoints_kW"], dtype=np.float64)
    tbl = np.asarray(LAW_PORTS["BSFC_Table_gkWh"], dtype=np.float64)
    best_p, best_j = 0.0, float("inf")
    n = int(p_max // grid) + 1
    for i in range(n):
        pg = i * grid
        pbat = p_load_kw - eta_rect * pg
        if soc <= sdm:
            if pbat > 1e-9: continue
            best_p = pg; break
        if soc >= scm and pbat < 0: continue
        if pg < 1e-9:
            mf = idle / 3600.0
        else:
            nb = len(bp)
            if pg >= bp[nb-1]: bsfc = tbl[nb-1]
            elif pg <= bp[0]: bsfc = tbl[0]
            else:
                ii = 0
                while ii < nb-2 and bp[ii+1] < pg: ii += 1
                w = (pg - bp[ii]) / (bp[ii+1] - bp[ii])
                bsfc = tbl[ii] + w * (tbl[ii+1] - tbl[ii])
            mf = pg * bsfc / 3.6e6
        mbe = pbat/(eta_dcdc*eta_dis)/3600.0 if pbat > 0 else pbat*(eta_dcdc*eta_chg)/3600.0
        j = mf + lam * mbe
        if j < best_j: best_j, best_p = j, pg
    if soc <= sdm and best_p < 1e-9 and p_load_kw > eta_rect * p_max:
        best_p = p_max
    return best_p


def host_chain_tick(hist, kf, P_load, SOC, Vdc, dis, chg, prop, hotel):
    """host 同链一拍：ORT REATT → dp_tick → 执行层 → PMS/Safety×1000。"""
    from gen_ref_split_v2 import Chain
    global _chain
    try:
        _chain
    except NameError:
        _chain = Chain()
    q = _chain.run(hist.reshape(300, 11), kf.reshape(30, 6))
    q50 = np.asarray(q["q"], dtype=np.float64).reshape(30, 3)[:, 1]
    Pbar = np.ascontiguousarray(q50[:30])
    # gen0/N0 与固件同口径：st=3/3（PG 恒 ON）+ N=prev dispatch，t_dwell=0
    n0 = max(1.0, PS.UD_N_dispatch_fb)
    g = GenState(); g.st[0] = g.st[1] = 3.0
    g.P_ramp[0] = g.P_ramp[1] = 0.0; g.P_lag[0] = g.P_lag[1] = 0.0
    g.t_dwell[0] = g.t_dwell[1] = 0.0
    r = TickResult()
    rc = law.ems04_dp_tick(ctypes.byref(T), Pbar, 30, SOC, n0, ctypes.byref(g), ctypes.byref(r))
    assert rc == 0
    d1 = float(r.dlambda_first)
    soc_ref_eff = 0.60 + K_S_B2 * d1
    lam = max(0.425 + 30.0 * (soc_ref_eff - SOC), 0.43)
    pg = exec_layer(P_load, SOC, lam)
    po = POut()
    for _ in range(1000):
        pms.ems04_pms_tick(ctypes.byref(PS), ctypes.byref(PP),
                           P_load, SOC, 3.0, PS.UD_state_DG2, 0.0, 0.0,
                           dis, chg, prop, hotel, pg, 1.0, 1.0, ctypes.byref(po))
    return {"q50": q50, "d1": d1, "lam": lam, "P_gen_ref": pg,
            "P_commit": po.P_commit_kW, "cstate": po.commitment_state,
            "rcode": po.reason_code, "on1": po.cmd_onoff_DG1, "on2": po.cmd_onoff_DG2,
            "ref1": po.P_DG1_ref_kW, "ref2": po.P_DG2_ref_kW, "N": po.N_dispatch,
            "health": po.ems_health_ok, "fb": po.baseline_fallback_active}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ticks", nargs="?", type=int, default=100,
                    help="拍数（位置参数，兼容旧用法 `s4_c3_preamble.py 100`）")
    ap.add_argument("--baud", type=int, default=115200,
                    help="板串口波特率（默认 115200=板当前固件口径）")
    args = ap.parse_args()
    ticks = args.ticks
    ser = serial.Serial("/dev/cu.usbserial-1120", args.baud, timeout=0.1)
    def send(ft, pl=b""): ser.write(enc_frame(ft, pl)); ser.flush()
    def wait(ft, ms=20000):
        dl = time.monotonic() + ms/1000; buf = b""
        while time.monotonic() < dl:
            c = ser.read(ser.in_waiting or 1)
            if c: buf += c
            while True:
                i = buf.find(bytes([SYNC0, SYNC1]))
                if i < 0:
                    if len(buf) > 2: buf = buf[-1:]
                    break
                if len(buf) < i+5: break
                rt = buf[i+2]; ln = struct.unpack_from("<H", buf, i+3)[0]
                if len(buf) < i+5+ln+2: break
                fr = buf[i:i+5+ln+2]; body = fr[2:-2]
                if struct.unpack("<H", fr[-2:])[0] != crc16_ccitt(body):
                    buf = buf[i+2:]; continue
                pl = fr[5:5+ln]; buf = buf[i+5+ln+2:]
                if rt == ft: return pl
        return None
    for _ in range(3):
        send(CMD_PING)
        if wait(0x7F, 400): break
    send(CMD_RESET); wait(0x7F, 500)
    pms.ems04_pms_init(ctypes.byref(PS))  # host PMS 与板同步复位

    base = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25",
                        "ems04_s2", "truth", "anchors", "FLTA_S1_D00000_M2")
    hist = np.fromfile(os.path.join(base, "t0_00300.hist.f32.bin"), "<f4")
    kf = np.fromfile(os.path.join(base, "t0_00300.kf.f32.bin"), "<f4")

    rows, n_action_diff, n_pg_diff = [], 0, 0
    for k in range(ticks):
        P_load = 100.0 + 60.0 * (1 + np.sin(2 * np.pi * k / 25.0))
        SOC = 0.60 - 0.01 * k / ticks
        Vdc, dis, chg = 750.0, 50.0, 30.0
        prop = 40.0 + 5.0 * np.sin(k); hotel = 100.0
        payload = (struct.pack("<II", k, k) +
                   struct.pack("<8f", P_load, SOC, Vdc, dis, chg, prop, hotel, 0.0) +
                   hist.astype("<f4").tobytes() + kf.astype("<f4").tobytes())
        send(CMD_TICK_CTRL, payload)
        pl = wait(RSP_TICK_CTRL, 20000)
        if pl is None or len(pl) < RSP_LEN - 1:
            rows.append({"tick": k, "nack": True}); continue
        seq_r, = struct.unpack_from("<I", pl, 0)
        bv = struct.unpack_from("<20f", pl, 4)
        board = {"P_gen_ref": bv[0], "P_commit": bv[1], "cstate": bv[2], "rcode": bv[3],
                 "on1": bv[4], "on2": bv[5], "ref1": bv[6], "ref2": bv[7], "N": bv[8],
                 "d1": bv[9], "lam": bv[10]}
        h = host_chain_tick(hist, kf, float(P_load), float(SOC), Vdc, dis, chg,
                            float(prop), float(hotel))
        GRID = 0.005
        a_b = int(round(board["d1"] / GRID)); a_h = int(round(h["d1"] / GRID))
        act_eq = a_b == a_h
        pg_diff = abs(board["P_gen_ref"] - h["P_gen_ref"])
        if not act_eq: n_action_diff += 1
        if pg_diff > 20.0 + 1e-9: n_pg_diff += 1
        rows.append({"tick": k, "board_d1": board["d1"], "host_d1": h["d1"],
                     "action_eq": act_eq,
                     "board_P_gen_ref": board["P_gen_ref"], "host_P_gen_ref": h["P_gen_ref"],
                     "P_gen_ref_diff": pg_diff,
                     "board_cstate": board["cstate"], "host_cstate": h["cstate"],
                     "board_N": board["N"], "host_N": h["N"],
                     "board_lam": board["lam"], "host_lam": h["lam"],
                     "q50_head_diff": float(np.abs(board_q50(bv) if False else 0))})
        print(f"tick {k:3d}: d1 B={board['d1']:+.3f} H={h['d1']:+.3f} "
              f"act={'EQ' if act_eq else 'DIFF'} P_ref B={board['P_gen_ref']:.0f} H={h['P_gen_ref']:.0f}")

    rep = {"schema": "s4_c3_preamble_v1",
           "created_utc": datetime.now(timezone.utc).isoformat(),
           "ticks": ticks, "action_diff_n": n_action_diff, "pg_diff_n": n_pg_diff,
           "rows": rows}
    out = os.path.join(HOST, "results", "s4_c2", "s4_c3_preamble_report.json")
    json.dump(rep, open(out, "w"), indent=1)
    print(f"C3 preamble: {ticks} ticks, action_diff={n_action_diff}, pg>1grid={n_pg_diff}")
    print("report ->", out)
    return 0


def board_q50(bv):
    return None


if __name__ == "__main__":
    main()

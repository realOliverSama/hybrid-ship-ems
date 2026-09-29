#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_gp2e_stream_gate.py — S4-P3 B2 步骤⑤：G-P2E 回放保真门（验收核心）。

对冻结 4 航次逐一加载 libplantstepE_stream.dylib（ctypes，同 advancer 装配），
P_gen_ref=恒 300 kW 占位（P_gen 实际不影响需求注入——需求侧为回放处方量），
子步级（0.01 s）全程推进并核验：

  G1 回放合成恒等（主门，1e-9）：P_load_total_out ≡ P_prop_realized + P_hotel
     （模型内 04 = 双输入 Sum 组合逻辑；期望逐位一致）
  G2 v_ship 回放直通（1e-9）：v_ship_out ≡ bin 行 v_ship（R6 直连组合逻辑）
  G3 P_prop 遥测直通（信息面）：P_prop_kW_out ≡ bin 行 P_prop_realized（R5 直连）
  G4 x_ship 积分一致性（<1e-6 m）：x_ship_out vs bin v_ship 的 Ts=0.01
     Forward Euler 累积参考（两种写出相位假设 A/B 归档，取小者判定）
  G5 能量域健康（有界、无 NaN）：SOC/V_dc 全程有限；SOC∈[0,1]；V_dc∈[0,1500]
  G6 终止语义：全程 voyage_stop=0（行数=duration+1，完整跑内不触发行尽）

结果: Host/results/s4_p3/gp2e_stream_gate.json（4/4 合取 overall）。
运行: PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python tools/run_gp2e_stream_gate.py
      [--voyage-id <vid>]（缺省 = 冻结件全部 4 航次）
"""
from __future__ import annotations

import argparse
import ctypes
import datetime
import hashlib
import json
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)


def find_repo_root(start):
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


REPO = find_repo_root(HOST)
FREEZE_REL = os.path.join("firmware", "stm32n6", "04_SourceCode",
                          "SoftwarePackage", "EMS_Projects",
                          "EMS04_CHIL_FULLCTRL", "Host", "results", "s4_c4",
                          "S4_P3_FORMAL_ARM_FREEZE.json")
STREAM_ROOT = os.path.join(HOST, "plantstep_e_stream")
RESULTS = os.path.join(HOST, "results", "s4_p3")
GATE_SCHEMA = "s4_p3_gp2e_stream_gate_v1"
P_GEN_CONST_KW = 300.0
SUB_PER_ROW = 100
DT = 0.01
TOL = 1e-9
X_TOL_M = 1e-6
OUT16 = 16


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_bin_rows(path):
    with open(path, "rb") as fh:
        blob = fh.read()
    if len(blob) < 20 or blob[:8] != b"EMS04STM":
        raise RuntimeError("stream bin 格式不符: %s" % path)
    ver, n, _res = struct.unpack_from("<III", blob, 8)
    if ver != 1:
        raise RuntimeError("stream bin 版本 %d != 1" % ver)
    rows = struct.unpack_from("<%dd" % (4 * n), blob, 20)
    return n, rows


def load_frozen_voyages():
    fp = os.path.join(REPO, FREEZE_REL)
    freeze = json.load(open(fp))
    if freeze.get("schema") != "s4_p3_formal_arm_freeze_v1" \
            or freeze.get("status") != "FROZEN":
        raise RuntimeError("冻结件 schema/status 异常")
    return fp, [e["voyage_id"] for e in freeze["battery"]]


def run_voyage(vid, rows, n_rows):
    lib_path = os.path.join(STREAM_ROOT, vid, "libplantstepE_stream.dylib")
    lib = ctypes.CDLL(lib_path)
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
    rc = lib.plantstep_init(vid.encode("ascii"))
    if rc != 0:
        raise RuntimeError("plantstep_init(%s) rc=%d" % (vid, rc))

    buf = (ctypes.c_double * OUT16)()
    n_sub = n_rows * SUB_PER_ROW
    lib.plantstep_set_pgen(P_GEN_CONST_KW)

    # 聚合面（子步全程）
    g1_bit = 0
    g1_max = 0.0
    g2_bit = 0
    g2_max = 0.0
    g3_bit = 0
    g3_max = 0.0
    xa_max = 0.0      # 假设 A：x 含当前步（xref 先加后比）
    xb_max = 0.0      # 假设 B：x 不含当前步（xref 后加）
    xa_end = xb_end = 0.0
    soc_min = soc_max = None
    vdc_min = vdc_max = None
    nan_n = 0
    stop_n = 0
    vstop_final = None
    t_end = None
    x_end = None

    xref = 0.0
    v_prev_rows = None
    step = lib.plantstep_step
    geto = lib.plantstep_get_outputs
    for k in range(n_sub):
        ri = (k // SUB_PER_ROW) * 4
        prop = rows[ri + 1]
        hotel = rows[ri + 2]
        vrow = rows[ri + 3]
        step()
        if geto(buf) != 0:
            raise RuntimeError("get_outputs 失败 @%d" % k)
        pload = buf[1]
        vs = buf[4]
        xs = buf[6]
        # G1: P_load ≡ prop + hotel（f64 加法，期望逐位）
        d1 = pload - (prop + hotel)
        if d1 != 0.0:
            g1_bit += 1
        ad1 = abs(d1)
        if ad1 > g1_max:
            g1_max = ad1
        # G2: v_ship 直通
        d2 = vs - vrow
        if d2 != 0.0:
            g2_bit += 1
        ad2 = abs(d2)
        if ad2 > g2_max:
            g2_max = ad2
        # G3: P_prop 遥测直通
        d3 = buf[9] - prop
        if d3 != 0.0:
            g3_bit += 1
        ad3 = abs(d3)
        if ad3 > g3_max:
            g3_max = ad3
        # G4: x_ship 积分参考（两种写出相位）
        xref += DT * vrow          # 假设 A：x_k+1 = x_k + Ts*u_k（当前步已含）
        da = abs(xs - xref)
        if da > xa_max:
            xa_max = da
        da_b = abs(xs - (xref - DT * vrow))
        if da_b > xb_max:
            xb_max = da_b
        # 采样健康面（每 100 子步 + 末步）
        if k % 100 == 0 or k == n_sub - 1:
            soc = buf[2]
            vdc = buf[14]
            if not (math.isfinite(soc) and math.isfinite(vdc)
                    and math.isfinite(xs) and math.isfinite(pload)):
                nan_n += 1
            if soc_min is None or soc < soc_min:
                soc_min = soc
            if soc_max is None or soc > soc_max:
                soc_max = soc
            if vdc_min is None or vdc < vdc_min:
                vdc_min = vdc
            if vdc_max is None or vdc > vdc_max:
                vdc_max = vdc
            if int(buf[15]) != 0:
                stop_n += 1
            v_prev_rows = (vrow,)
        if k == n_sub - 1:
            xa_end = abs(xs - xref)
            xb_end = abs(xs - (xref - DT * vrow))
            x_end = xs
            t_end = buf[0]
            vstop_final = int(buf[15])

    fuel = lib.plantstep_get_fuel()
    lib.plantstep_get_voyage_stop.restype = ctypes.c_int
    vstop_api = lib.plantstep_get_voyage_stop()

    checks = {
        "G1_pload_identity_1e-9": bool(g1_max <= TOL),
        "G2_vship_passthrough_1e-9": bool(g2_max <= TOL),
        "G4_xship_end_1e-6m": bool(min(xa_end, xb_end) <= X_TOL_M),
        "G5_healthy_finite_bounded": bool(
            nan_n == 0 and 0.0 <= soc_min and soc_max <= 1.0
            and 0.0 <= vdc_min and vdc_max <= 1500.0),
        "G6_no_early_stop": bool(stop_n == 0 and vstop_final == 0
                                 and vstop_api == 0),
    }
    return {
        "voyage_id": vid,
        "dylib_sha256": sha256_file(lib_path),
        "n_rows": n_rows,
        "n_substeps": n_sub,
        "p_gen_const_kw": P_GEN_CONST_KW,
        "G1_pload_identity": {
            "bitwise_mismatch_n": g1_bit, "max_abs_diff_kw": g1_max,
            "tol": TOL, "pass": checks["G1_pload_identity_1e-9"]},
        "G2_vship_passthrough": {
            "bitwise_mismatch_n": g2_bit, "max_abs_diff_mps": g2_max,
            "tol": TOL, "pass": checks["G2_vship_passthrough_1e-9"]},
        "G3_pprop_passthrough_info": {
            "bitwise_mismatch_n": g3_bit, "max_abs_diff_kw": g3_max},
        "G4_xship_integration": {
            "ref": "ForwardEuler Ts=0.01 on bin v_ship (ZOH)",
            "phase_A_incl_current_step": {
                "max_abs_diff_m": xa_max, "end_abs_diff_m": xa_end},
            "phase_B_excl_current_step": {
                "max_abs_diff_m": xb_max, "end_abs_diff_m": xb_end},
            "x_end_m": x_end,
            "x_ref_end_A_m": xref,
            "pass": checks["G4_xship_end_1e-6m"]},
        "G5_health": {
            "nan_n": nan_n, "soc_min": soc_min, "soc_max": soc_max,
            "vdc_min": vdc_min, "vdc_max": vdc_max,
            "pass": checks["G5_healthy_finite_bounded"]},
        "G6_stop": {
            "midrun_stop_samples_n": stop_n,
            "voyage_stop_final_out": vstop_final,
            "voyage_stop_api_final": vstop_api,
            "pass": checks["G6_no_early_stop"]},
        "t_end_s": t_end,
        "fuel_end_kg": fuel,
        "overall": bool(all(checks.values())),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--voyage-id", default=None)
    args = ap.parse_args()
    freeze_path, vids = load_frozen_voyages()
    if args.voyage_id:
        if args.voyage_id not in vids:
            ap.error("--voyage-id 不在冻结集合: %s" % vids)
        vids = [args.voyage_id]
    os.makedirs(RESULTS, exist_ok=True)

    per = []
    for vid in vids:
        bin_path = os.path.join(STREAM_ROOT, vid, "%s.stream.f64.bin" % vid)
        n_rows, rows = load_bin_rows(bin_path)
        print("[gp2e-stream] %s: %d rows x %d substeps …"
              % (vid, n_rows, n_rows * SUB_PER_ROW), flush=True)
        r = run_voyage(vid, rows, n_rows)
        per.append(r)
        print("[gp2e-stream] %s overall=%s G1bit=%d G1max=%.3g G2bit=%d "
              "G4end(A)=%.3g(B)=%.3g soc=[%.4f,%.4f] vdc=[%.1f,%.1f]"
              % (vid, r["overall"], r["G1_pload_identity"]["bitwise_mismatch_n"],
                 r["G1_pload_identity"]["max_abs_diff_kw"],
                 r["G2_vship_passthrough"]["bitwise_mismatch_n"],
                 r["G4_xship_integration"]["phase_A_incl_current_step"]["end_abs_diff_m"],
                 r["G4_xship_integration"]["phase_B_excl_current_step"]["end_abs_diff_m"],
                 r["G5_health"]["soc_min"], r["G5_health"]["soc_max"],
                 r["G5_health"]["vdc_min"], r["G5_health"]["vdc_max"]),
              flush=True)

    gate = {
        "schema": GATE_SCHEMA,
        "created_utc": datetime.datetime.now(
            datetime.timezone.utc).isoformat(),
        "freeze": {"path": os.path.relpath(freeze_path, REPO),
                   "sha256": sha256_file(freeze_path)},
        "model_sha256_note": "stream 模型 443e226d…（20S 审计）；dylib sha 见各航次",
        "tolerances": {"identity": TOL, "xship_end_m": X_TOL_M},
        "voyages": per,
        "overall": bool(all(r["overall"] for r in per)),
        "overall_note": "G-P2E 回放保真门 = 4/4 合取（G1 1e-9 主门 + G2 直通 + "
                        "G4 x_ship 1e-6 m + G5 健康 + G6 终止语义）",
    }
    out = os.path.join(RESULTS, "gp2e_stream_gate.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(gate, fh, indent=1, ensure_ascii=False)
    print("[gp2e-stream] overall=%s -> %s" % (gate["overall"],
                                              os.path.relpath(out, REPO)))
    return 0 if gate["overall"] else 1


if __name__ == "__main__":
    sys.exit(main())

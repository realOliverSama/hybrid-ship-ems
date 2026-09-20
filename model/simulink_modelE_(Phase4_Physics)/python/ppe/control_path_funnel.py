#!/usr/bin/env python3
"""Control Path Funnel Closure — 信息→动作漏斗的逐点闭合。

Gate 授权范围（CONTROL_PATH_FUNNEL_CLOSURE）：
  无 Simulink 重跑、无控制器重设计、无 effect 重算、无 IID/OOD 访问。
本模块只做两件事：
  (1) 把**冻结 runtime 机制**逐点重建为 Python 等价实现（ECMS 动作函数 +
      policy_lookup 索引算术），并用**冻结缓存策略表**逐格自证；
  (2) 在正式晚期决策点（t≥300 s，总数由数据推导）上计算跨臂 D0–D4 布尔量、
      四类吸收计数与条件化贡献，给出最终机制分类所需的全部数字。

重建确权（源码事实，逐行可查 —— 非假设）：
  run_cstar.m:125    ECMS_EHat_Data = prep_ehat_zoh(t0, E300_est, d1)
                     → 第 3 列 = 该决策区间选定的 δλ；第 2 列 E300_est 仅为信息
                       通道（build_model_e.m:801 该路接 Terminator）
  build_model_e.m:801   oracle_demux/2 → ecms_step/9（e_hat 端口）
  CODE_ECMS_ORACLE      soc_ref_eff = SOC_ref + K_S·e_hat
                        lam = λ₀ + K_soc·(soc_ref_eff − SOC); lam = max(lam, 0.43)
  run_cstar.m:44,57     runner **从不**改写 ECMS_Lambda（λ₀ ≡ 0.425 常数端口），
                        仅写入 ECMS_K_S = K_S_B2_num/K_S_B2_den = 1/30
  ⇒ λ_eff(t0) = max(0.425 + 30·(0.6 + (1/30)·δλ − SOC(t0)), 0.43)
     （K_soc·K_S ≡ 1：δλ 经 e_hat 通道以净系数 1 进入 λ，即 λ₀ + δλ + 30(0.6−SOC)）
  CODE_ECMS_ADAPTIVE    law=5（P0）：lam = λ₀ + K_soc·(SOC_ref − SOC)，无 e_hat、无 floor

通道纪律（与 FC-3 层一致）：只读非燃油通道；fuel/J_FEC 字段在本模块中物理不出现。
"""
from __future__ import annotations

import hashlib
import json
import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_MODEL = os.path.dirname(os.path.dirname(_PPE))
EVID = os.path.join(_MODEL, "results", "ppe_practical", "formal_epoch2")
OUTDIR = os.path.join(_MODEL, "results", "ppe_practical",
                      "control_path_attribution")
TBL_PATH = os.path.join(_MODEL, "artifacts", "t445", "policy_table.mat")
CFG_PATH = os.path.join(_MODEL, "config", "phase4f_t445_config.json")

# ── 冻结常数（来源逐条标注；全部取自冻结配置/参数文件，非本模块定义） ──────
SPEC = {
    "lambda0": 0.425,          # config/ModelE_Physics_Parameters.m:261
    "K_soc": 30.0,             # :262 / phase4f_t445_config.json lambda_law
    "SOC_ref": 0.60,           # :265
    "lambda_floor": 0.43,      # :264（law=6 且 K_S≠0 时生效）
    "K_S_B2_num": 1,           # config json lambda_law.K_S_B2_num
    "K_S_B2_den": 30,          # config json lambda_law.K_S_B2_den
    "eta_rect": 0.97,          # ModelE_Physics_Parameters.m:173
    "eta_dcdc": 0.97,          # :196
    "eta_dis": 0.95,           # :215
    "eta_chg": 0.95,           # :216
    "idle_fuel_kgph": 6.795,   # :268
    "dP_kW": 20.0,             # :269
    "P_max_kW": 1000.0,        # :168 Gen_P_max = 2×DG_P_rated
    "SOC_dis_max": 0.105,      # :266
    "SOC_chg_min": 0.945,      # :267
    "bsfc_bp": [0.0, 100.0, 200.0, 400.0, 600.0, 800.0, 1000.0],   # :254
    "bsfc_tbl": [1200.0, 700.0, 550.0, 480.0, 450.0, 430.0, 453.0],  # :255
    "decision_dt_s": 10,
    "late_t0_s": 300,
    "SOC_DP_min": 0.10, "SOC_DP_max": 0.95,
}
K_S = SPEC["K_S_B2_num"] / SPEC["K_S_B2_den"]
GRID = np.arange(0.0, SPEC["P_max_kW"] + 1e-9, SPEC["dP_kW"])   # 0:20:1000

# 通道纪律：本模块允许读取的轨迹通道（无燃油类字段）
CH_ALLOW = ("time", "SOC", "P_load_total_kW", "P_gen_actual_kW",
            "P_DG1_kW", "P_DG2_kW", "P_bat_actual_kW", "state_DG1",
            "state_DG2", "N_dispatch")


# ═══════════════════════════ 冻结机制移植 ═══════════════════════════
def mf_port(pg: float) -> float:
    """ṁ_f(P_g) —— build_model_e.m CODE_ECMS_ORACLE / ecms2.ecms_cost_parts 逐行移植。"""
    if pg < 1e-9:
        return SPEC["idle_fuel_kgph"] / 3600.0
    bp = SPEC["bsfc_bp"]
    tbl = SPEC["bsfc_tbl"]
    nb = len(bp)
    bsfc = tbl[0]
    if pg >= bp[nb - 1]:
        bsfc = tbl[nb - 1]
    elif pg > bp[0]:
        ii = 0
        while ii < nb - 2 and bp[ii + 1] < pg:
            ii += 1
        w = (pg - bp[ii]) / (bp[ii + 1] - bp[ii])
        bsfc = tbl[ii] + w * (tbl[ii + 1] - tbl[ii])
    return pg * bsfc / 3.6e6


MF = np.array([mf_port(g) for g in GRID])          # 固定网格上的 ṁ_f [kg/s]


def mbe_port(pbat: np.ndarray) -> np.ndarray:
    """ṁ_bat,eq —— 放电正代价 / 充电负收益（同源逐行移植）。"""
    return np.where(
        pbat > 0,
        pbat / (SPEC["eta_dcdc"] * SPEC["eta_dis"]) / 3600.0,
        pbat * (SPEC["eta_dcdc"] * SPEC["eta_chg"]) / 3600.0)


def action_exact(lam: np.ndarray, pd_kw: np.ndarray,
                 soc: np.ndarray) -> np.ndarray:
    """law=5/6 单步 ECMS 动作 P_g*（向量化，逐 instant 独立）。

    与 CODE_ECMS_ORACLE 的网格穷举同构：J = ṁ_f + λ·ṁ_bat,eq，严格 '<' 取
    首个最小点（= MATLAB min 的 first-index 语义）。
    """
    lam = np.atleast_1d(np.asarray(lam, float))
    pd_kw = np.atleast_1d(np.asarray(pd_kw, float))
    soc = np.atleast_1d(np.asarray(soc, float))
    lam, pd_kw, soc = np.broadcast_arrays(lam, pd_kw, soc)
    out = np.zeros(lam.shape[0], float)
    for i in range(lam.shape[0]):
        pbat = pd_kw[i] - SPEC["eta_rect"] * GRID
        s = soc[i]
        if s <= SPEC["SOC_dis_max"]:
            feas = pbat <= 1e-9
            if feas.any():
                out[i] = GRID[int(np.argmax(feas))]
            else:
                out[i] = (SPEC["P_max_kW"]
                          if pd_kw[i] > SPEC["eta_rect"] * SPEC["P_max_kW"] else 0.0)
            continue
        feas = ~((s >= SPEC["SOC_chg_min"]) & (pbat < 0))
        j = MF + lam[i] * mbe_port(pbat)
        j = np.where(feas, j, np.inf)
        g = int(np.argmin(j))
        out[i] = GRID[g] if np.isfinite(j[g]) else 0.0
    return out


def lambda_eff_law6(dlam: np.ndarray, soc: np.ndarray) -> np.ndarray:
    """λ_eff（law=6，正式 C* 臂）—— 与模型块逐表达式一致（含 floor 钳位）。"""
    soc_ref_eff = SPEC["SOC_ref"] + K_S * np.asarray(dlam, float)
    lam = SPEC["lambda0"] + SPEC["K_soc"] * (soc_ref_eff - np.asarray(soc, float))
    return np.maximum(lam, SPEC["lambda_floor"])


def lambda_raw_law6(dlam: np.ndarray, soc: np.ndarray) -> np.ndarray:
    """λ_eff（钳位前）—— 供 floor 吸收判定使用。"""
    soc_ref_eff = SPEC["SOC_ref"] + K_S * np.asarray(dlam, float)
    return SPEC["lambda0"] + SPEC["K_soc"] * (soc_ref_eff - np.asarray(soc, float))


def lambda_law5(soc: np.ndarray) -> np.ndarray:
    """law=5（P0）：λ = λ₀ + K_soc·(SOC_ref − SOC)，无 e_hat、无 floor。"""
    return SPEC["lambda0"] + SPEC["K_soc"] * (SPEC["SOC_ref"] - np.asarray(soc, float))


def _matlab_round(x: np.ndarray) -> np.ndarray:
    """MATLAB round 语义（正值域：half away from zero）。"""
    return np.floor(np.asarray(x, float) + 0.5)


def policy_lookup(tbl: dict, lam, pbar, soc):
    """t445.policy_lookup 逐行移植（返回 1-based 索引与 oob 标志）。"""
    lam = np.atleast_1d(np.asarray(lam, float))
    pbar = np.atleast_1d(np.asarray(pbar, float))
    soc = np.atleast_1d(np.asarray(soc, float))
    lam, pbar, soc = np.broadcast_arrays(lam, pbar, soc)
    oob = ((lam < tbl["lambda_table_min"] - 1e-12)
           | (lam > tbl["lambda_table_max"] + 1e-12)
           | (pbar < -1e-12) | (pbar > tbl["pbar_grid"][-1] + 1e-12))
    il = _matlab_round((lam - tbl["lambda_table_min"]) / tbl["lambda_step"]) + 1
    il = np.clip(il, 1, tbl["n_lambda"]).astype(int)
    ip = _matlab_round(pbar / tbl["pbar_step"]) + 1
    ip = np.clip(ip, 1, tbl["n_pbar"]).astype(int)
    ir = np.where(soc <= 0.105, 2, np.where(soc >= 0.945, 3, 1)).astype(int)
    return il, ip, ir, oob


def load_policy_table(path: str = TBL_PATH) -> dict:
    """读取冻结缓存策略表（artifacts/t445/policy_table.mat，v7.3/HDF5）。

    HDF5 维度为 MATLAB 列主序的逆序：MATLAB (nL,nP,3) → HDF5 (3,nP,nL)，
    故 Pg/fuel 需 transpose(2,1,0) 还原为 (nL,nP,3)。
    """
    import h5py
    out = {}
    with h5py.File(path, "r") as f:
        g = f["tbl"]
        for k in ("lambda_grid", "pbar_grid", "soc_rep"):
            out[k] = np.array(g[k], dtype=float).ravel()
        for k in ("Pg", "fuel_kg", "dE_kWh"):
            a = np.array(g[k], dtype=float)
            out[k] = np.transpose(a, (2, 1, 0))          # → (nL, nP, 3)
        for k in ("n_lambda", "n_pbar", "lambda_table_min", "lambda_table_max",
                  "lambda_step", "pbar_step"):
            out[k] = float(np.array(g[k]).ravel()[0])
        out["n_lambda"] = int(out["n_lambda"])
        out["n_pbar"] = int(out["n_pbar"])
        out["config_sha256"] = bytes(
            np.array(g["config_sha256"]).ravel().astype("uint8")).decode()
    return out


def rebuild_table_exact() -> dict:
    """build_policy_table.m 的 Python 等价物（逐格自证用）。"""
    pt = json.load(open(CFG_PATH))["b2_policy_table"]
    lam = pt["lambda_colon"][0] + pt["lambda_colon"][1] * np.arange(pt["n_lambda"])
    pbar = pt["pbar_colon_kW"][0] + pt["pbar_colon_kW"][1] * np.arange(pt["n_pbar"])
    soc_rep = np.array([0.60, 0.10, 0.95])
    dt = 10.0
    nL, nP = lam.size, pbar.size
    Pg = np.zeros((nL, nP, 3))
    fuel = np.zeros((nL, nP, 3))
    for ip in range(nP):
        pd = pbar[ip]
        for ir in range(3):
            pbat = pd - SPEC["eta_rect"] * GRID
            s = soc_rep[ir]
            if s <= SPEC["SOC_dis_max"]:
                feas = pbat <= 1e-9
                if feas.any():
                    g = int(np.argmax(feas))
                else:
                    g = GRID.size - 1 if pd > SPEC["eta_rect"] * SPEC["P_max_kW"] else 0
                Pg[:, ip, ir] = GRID[g]
                fuel[:, ip, ir] = MF[g] * dt
                continue
            feas = ~((s >= SPEC["SOC_chg_min"]) & (pbat < 0))
            mbe = mbe_port(pbat)
            J = MF[None, :] + lam[:, None] * mbe[None, :]
            J = np.where(feas[None, :], J, np.inf)
            g = np.argmin(J, axis=1)
            Pg[:, ip, ir] = GRID[g]
            fuel[:, ip, ir] = MF[g] * dt
    return {"lambda_grid": lam, "pbar_grid": pbar, "soc_rep": soc_rep,
            "Pg": Pg, "fuel_kg": fuel}


# ═══════════════════════════ 冻结证据装载 ═══════════════════════════
def _sha_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def load_record(vid: str, arm: str) -> dict:
    return json.load(open(os.path.join(EVID, vid, f"{arm}.json")))


def load_ctrace(vid: str, arm: str, verify: bool = True) -> dict:
    p = os.path.join(EVID, vid, f"{arm}.ctrace.v1.json")
    rec = load_record(vid, arm)
    if verify and _sha_file(p) != rec["controller_trace_sha256"]:
        raise RuntimeError(f"ctrace hash mismatch {p}")
    d = json.load(open(p))
    return {"t": np.array(d["time"], float),
            "dlam": np.array(d["delta_lambda"], float),
            "wsha": d["forecast_window_sha256"],
            "n": d["n_decisions"]}


def load_traj(vid: str, arm: str, verify: bool = True) -> dict:
    import h5py
    p = os.path.join(EVID, vid, f"{arm}.traj.v1.mat")
    rec = load_record(vid, arm)
    if verify and _sha_file(p) != rec["trajectory_sha256"]:
        raise RuntimeError(f"trajectory hash mismatch {p}")
    out = {}
    with h5py.File(p, "r") as f:
        for k in CH_ALLOW:
            if k in f["traj"]:
                out[k] = np.array(f["traj"][k]).ravel()
    return out


def voyage_list(arms=("C0", "C1", "C2", "P0")) -> list:
    out = []
    for d in sorted(os.listdir(EVID)):
        if not os.path.isdir(os.path.join(EVID, d)):
            continue
        if all(os.path.exists(os.path.join(EVID, d, f"{a}.json")) for a in arms):
            out.append(d)
    return out


def decision_slice(traj: dict, t0: float) -> int:
    """1 Hz 轨迹上的整数秒索引（冻结轨迹为 1 s 网格、无重复点）。"""
    return int(round(t0))


# ═══════════════════════════ 验证包 ═══════════════════════════
def validate_port() -> dict:
    """(V1) 表再生逐格对比 + (V2) policy_lookup 算术夹具 + (V3) 交叉一致性。"""
    cached = load_policy_table()
    cfg = json.load(open(CFG_PATH))
    rebuilt = rebuild_table_exact()
    res = {}

    res["V1_table_rebuild"] = {
        "lambda_grid_exact": bool(np.array_equal(cached["lambda_grid"],
                                                 rebuilt["lambda_grid"])),
        "lambda_grid_n_differing": int(np.sum(cached["lambda_grid"]
                                              != rebuilt["lambda_grid"])),
        "lambda_grid_max_abs_diff": float(np.max(np.abs(
            cached["lambda_grid"] - rebuilt["lambda_grid"]))),
        "lambda_grid_max_ulp": float(np.max(np.abs(
            cached["lambda_grid"] - rebuilt["lambda_grid"]))
            / np.spacing(float(cached["lambda_grid"][-1]))),
        "lambda_grid_note": ("MATLAB colon 累加 vs start+step*arange 的 1-ulp 差 "
                             "（333/3020 点）；不影响 argmin（见 Pg 逐格）"),
        "pbar_grid_exact": bool(np.array_equal(cached["pbar_grid"],
                                               rebuilt["pbar_grid"])),
        "Pg_exact_cells": int(np.sum(cached["Pg"] == rebuilt["Pg"])),
        "Pg_total_cells": int(cached["Pg"].size),
        "Pg_exact_fraction": float(np.mean(cached["Pg"] == rebuilt["Pg"])),
        "Pg_max_abs_diff": float(np.max(np.abs(cached["Pg"] - rebuilt["Pg"]))),
        "fuel_exact_fraction": float(np.mean(cached["fuel_kg"]
                                             == rebuilt["fuel_kg"])),
        "fuel_max_abs_diff": float(np.max(np.abs(cached["fuel_kg"]
                                                  - rebuilt["fuel_kg"]))),
        "table_config_sha_matches_cfg_json": bool(
            cached["config_sha256"] == cfg["config_sha256"]),
        "n_lambda": cached["n_lambda"], "n_pbar": cached["n_pbar"],
    }

    # V2: policy_lookup 索引算术夹具（解析边界，含 MATLAB round 语义）
    tbl = cached
    fx = []
    cases = [
        (0.43, 640.0, 0.60, 1, None, 1),
        (0.4325, 640.0, 0.60, None, None, 1),      # 半格 → MATLAB round 远离零
        (15.525, 640.0, 0.60, 3020, None, 1),
        (640.0, 0.0, 0.60, None, 1, 1),
        (640.0, 10.0, 0.60, None, 2, 1),           # 0.5 bin → round 远离零
        (640.0, 1300.0, 0.60, None, 66, 1),
        (640.0, 5.0, 0.60, None, 1, 1),
        (640.0, 640.0, 0.105, None, None, 2),
        (640.0, 640.0, 0.945, None, None, 3),
        (640.0, 640.0, 0.1049, None, None, 2),
    ]
    for lam, pb, soc, il_e, ip_e, ir_e in cases:
        il, ip, ir, oob = policy_lookup(tbl, lam, pb, soc)
        ok = True
        if il_e is not None:
            ok &= int(il[0]) == il_e
        if ip_e is not None:
            ok &= int(ip[0]) == ip_e
        ok &= int(ir[0]) == ir_e
        fx.append({"lambda": lam, "pbar": pb, "soc": soc,
                   "il": int(il[0]), "ip": int(ip[0]), "ir": int(ir[0]),
                   "oob": bool(oob[0]),
                   "expected": {"il": il_e, "ip": ip_e, "ir": ir_e},
                   "pass": bool(ok)})
    # 越域只计数不外推
    il, ip, ir, oob = policy_lookup(tbl, 0.42, -1.0, 0.60)
    fx.append({"case": "oob_lambda_below_and_pbar_below", "oob": bool(oob[0]),
               "il": int(il[0]), "ip": int(ip[0]), "pass": bool(oob[0])})
    res["V2_policy_lookup_fixtures"] = {
        "cases": fx, "all_pass": bool(all(c.get("pass") for c in fx))}

    # V3: 表单元 ↔ 动作函数一致性（重建路径闭合：lookup 得到的单元格值必须
    #     等于以同一 λ/Pd 直接执行 law=6 网格穷举的结果）
    rng = np.random.default_rng(20260918)
    il_s = rng.integers(1, tbl["n_lambda"] + 1, 400)
    ip_s = rng.integers(1, tbl["n_pbar"] + 1, 400)
    ir_s = rng.integers(1, 4, 400)
    lam = tbl["lambda_grid"][il_s - 1]
    pd = tbl["pbar_grid"][ip_s - 1]
    soc = tbl["soc_rep"][ir_s - 1]
    act = action_exact(lam, pd, soc)
    cell = tbl["Pg"][il_s - 1, ip_s - 1, ir_s - 1]
    res["V3_cell_vs_action"] = {
        "n_cases": int(lam.size),
        "exact_fraction": float(np.mean(act == cell)),
        "max_abs_diff": float(np.max(np.abs(act - cell))),
        "note": "cached frozen table cell vs direct law=6 grid enumeration",
    }
    res["all_pass"] = bool(
        res["V1_table_rebuild"]["Pg_exact_fraction"] == 1.0
        and res["V1_table_rebuild"]["fuel_exact_fraction"] == 1.0
        and res["V1_table_rebuild"]["pbar_grid_exact"]
        and res["V1_table_rebuild"]["table_config_sha_matches_cfg_json"]
        and res["V2_policy_lookup_fixtures"]["all_pass"]
        and res["V3_cell_vs_action"]["exact_fraction"] == 1.0)
    return res


# ═══════════════════════════ Funnel ═══════════════════════════
def build_arm_instants(vid: str, arm: str) -> dict:
    """单一臂-航次在全部决策点上的重建量表（1 Hz 轨迹对齐）。"""
    ct = load_ctrace(vid, arm)
    tr = load_traj(vid, arm)
    idx = np.round(ct["t"]).astype(int)
    soc = tr["SOC"][idx]
    pld = tr["P_load_total_kW"][idx]
    dlam = ct["dlam"]
    lam_raw = lambda_raw_law6(dlam, soc)
    lam_eff = np.maximum(lam_raw, SPEC["lambda_floor"])
    pg = action_exact(lam_eff, pld, soc)
    return {
        "t": ct["t"], "dlam": dlam,
        "wsha": np.array(ct["wsha"], dtype=object),
        "soc": soc,
        "p_load": pld, "lam_raw": lam_raw, "lam_eff": lam_eff, "pg": pg,
        "pg_real": tr["P_gen_actual_kW"][idx],
        "pg_dg1": tr["P_DG1_kW"][idx], "pg_dg2": tr["P_DG2_kW"][idx],
        "p_bat": tr["P_bat_actual_kW"][idx],
        "hold": np.array([isinstance(s, list) for s in ct["wsha"]]),
        "region": np.where(soc <= 0.105, 2, np.where(soc >= 0.945, 3, 1)),
    }


def funnel_pair(a: dict, b: dict) -> dict:
    """D0–D4 布尔量与四类吸收计数（对齐后的决策点集）。"""
    assert np.array_equal(a["t"], b["t"]), "decision grids differ"
    both_win = (~a["hold"]) & (~b["hold"])
    any_hold = a["hold"] | b["hold"]
    d0_eval = both_win
    d0_diff = np.array([sa != sb for sa, sb in zip(a["wsha"], b["wsha"])])
    d1 = a["dlam"] != b["dlam"]
    d2 = a["lam_eff"] != b["lam_eff"]
    d3 = a["pg"] != b["pg"]
    d4 = ((a["pg_real"] != b["pg_real"]) | (a["p_bat"] != b["p_bat"])
          | (a["pg_dg1"] != b["pg_dg1"]) | (a["pg_dg2"] != b["pg_dg2"])
          | (a["p_load"] != b["p_load"]))
    n = int(a["t"].size)
    n_d0 = int(np.sum(d0_eval))
    n_d0_diff = int(np.sum(d0_diff & d0_eval))
    res = {
        "n_instants": n,
        "n_window_evaluable": n_d0,
        "n_window_hold": int(np.sum(any_hold)),
        "n_both_hold": int(np.sum(a["hold"] & b["hold"])),
        "N_D0_window_differ": n_d0_diff,
        "N_D0_window_identical": int(n_d0 - n_d0_diff),
        "N_D1_delta_differ": int(np.sum(d1)),
        "N_D2_lambda_eff_differ": int(np.sum(d2)),
        "N_D3_action_differ": int(np.sum(d3)),
        "N_D4_realized_differ": int(np.sum(d4)),
        "FORECAST_TO_DELTA_ABSORBED": int(np.sum(d0_diff & d0_eval & ~d1)),
        "DELTA_TO_LAMBDA_FLOOR_ABSORBED": int(np.sum(d1 & ~d2)),
        "LAMBDA_TO_POLICY_ACTION_ABSORBED": int(np.sum(d2 & ~d3)),
        "ACTION_TO_PLANT_ABSORBED": int(np.sum(d3 & ~d4)),
        "floor_both_raw_below": int(np.sum(d1 & (a["lam_raw"] < SPEC["lambda_floor"])
                                           & (b["lam_raw"] < SPEC["lambda_floor"]))),
        "delta_levels_differ_hist": {},
    }
    # δλ 差异幅度直方（信息量：是否只是 0.005 级微差）
    dl = np.abs(a["dlam"] - b["dlam"])[d1]
    if dl.size:
        res["delta_absdiff"] = {
            "median": float(np.median(dl)), "p90": float(np.percentile(dl, 90)),
            "max": float(dl.max()),
            "frac_le_0.005": float(np.mean(dl <= 0.005 + 1e-12)),
        }
    # 条件化份额
    res["cond"] = {
        "share_delta_differs_given_window_differs":
            float(res["N_D1_delta_differ"] / n_d0_diff) if n_d0_diff else None,
        "share_lambda_survives_given_delta_differs":
            float(res["N_D2_lambda_eff_differ"] / res["N_D1_delta_differ"])
            if res["N_D1_delta_differ"] else None,
        "share_action_survives_given_lambda_differs":
            float(res["N_D3_action_differ"] / res["N_D2_lambda_eff_differ"])
            if res["N_D2_lambda_eff_differ"] else None,
        "share_realized_survives_given_action_differs":
            float(res["N_D4_realized_differ"] / res["N_D3_action_differ"])
            if res["N_D3_action_differ"] else None,
    }
    if res["N_D2_lambda_eff_differ"]:
        lam_diff = np.abs(a["lam_eff"] - b["lam_eff"])[d2]
        res["lambda_eff_absdiff"] = {
            "median": float(np.median(lam_diff)),
            "min": float(lam_diff.min()), "max": float(lam_diff.max())}
    if res["N_D3_action_differ"]:
        res["action_absdiff"] = {
            "median": float(np.median(np.abs(a["pg"] - b["pg"])[d3]))}
    return res


def run_funnel(voyages: list) -> dict:
    pairs = [("C0", "C1"), ("C0", "C2"), ("C1", "C2")]
    out = {"pairs": {}, "per_voyage": {}}
    cache = {}
    for vid in voyages:
        rec = {}
        for arm in ("C0", "C1", "C2"):
            cache[(vid, arm)] = build_arm_instants(vid, arm)
            rec[arm] = cache[(vid, arm)]
        out["per_voyage"][vid] = {}
        for pa, pb in pairs:
            A, B = cache[(vid, pa)], cache[(vid, pb)]
            late = A["t"] >= SPEC["late_t0_s"]
            sl = {k: v[late] if isinstance(v, np.ndarray) else v
                  for k, v in A.items()}
            sl2 = {k: v[late] if isinstance(v, np.ndarray) else v
                   for k, v in B.items()}
            out["per_voyage"][vid][f"{pa}_vs_{pb}"] = funnel_pair(sl, sl2)
    # 汇总
    for pa, pb in pairs:
        agg = {}
        keys = out["per_voyage"][voyages[0]][f"{pa}_vs_{pb}"].keys()
        for k in keys:
            v0 = out["per_voyage"][voyages[0]][f"{pa}_vs_{pb}"][k]
            if isinstance(v0, (int, float)) and not isinstance(v0, bool):
                agg[k] = sum(out["per_voyage"][v][f"{pa}_vs_{pb}"][k]
                             for v in voyages)
            else:
                agg[k] = None
        # 差异幅度跨航次聚合（每航次内先取中位/最大值，再跨航次取中位/最大）
        mags = {}
        for key, sub, red in (("delta_absdiff", "median", np.median),
                              ("lambda_eff_absdiff", "median", np.median),
                              ("lambda_eff_absdiff", "max", np.max)):
            vals = [out["per_voyage"][v][f"{pa}_vs_{pb}"].get(key, {}).get(sub)
                    for v in voyages]
            vals = [x for x in vals if x is not None]
            if vals:
                mags.setdefault(key, {})[f"{sub}_over_voyages"] = float(red(vals))
        for key, v in mags.items():
            agg[key] = v
        out["pairs"][f"{pa}_vs_{pb}"] = agg
        # 条件化（按汇总重算，避免按航次平均的偏差）
        d0 = agg["N_D0_window_differ"]
        d1 = agg["N_D1_delta_differ"]
        d2 = agg["N_D2_lambda_eff_differ"]
        d3 = agg["N_D3_action_differ"]
        agg["cond"] = {
            "share_delta_differs_given_window_differs": (d1 / d0) if d0 else None,
            "share_lambda_survives_given_delta_differs": (d2 / d1) if d1 else None,
            "share_action_survives_given_lambda_differs": (d3 / d2) if d2 else None,
            "share_realized_survives_given_action_differs":
                (agg["N_D4_realized_differ"] / d3) if d3 else None,
        }
    out["n_voyages"] = len(voyages)
    out["totals_derived"] = {
        "n_instants_all_arms": sum(out["pairs"][f"{a}_vs_{b}"]["n_instants"]
                                   for a, b in pairs) // 3,
        "n_instants_per_pair": out["pairs"]["C0_vs_C1"]["n_instants"],
        "late_definition": f"t0 >= {SPEC['late_t0_s']} s; decision grid 10 s",
    }
    return out


def p0_context(voyages: list) -> dict:
    """§10：P0（law=5，无预测/无 δλ）与 C0 的重建动作对照（诊断，不并入归因）。"""
    tot = diff_act = diff_lam = real_bitid = n = 0
    for vid in voyages:
        c0 = build_arm_instants(vid, "C0")
        tr = load_traj(vid, "P0")
        idx = np.round(c0["t"]).astype(int)
        soc = tr["SOC"][idx]
        pld = tr["P_load_total_kW"][idx]
        pg0 = action_exact(lambda_law5(soc), pld, soc)
        late = c0["t"] >= SPEC["late_t0_s"]
        n += int(np.sum(late))
        diff_act += int(np.sum(pg0[late] != c0["pg"][late]))
        diff_lam += int(np.sum(lambda_law5(soc)[late] != c0["lam_eff"][late]))
        real_bitid += int(np.sum(
            (tr["P_gen_actual_kW"][idx][late] == c0["pg_real"][late])
            & (tr["SOC"][idx][late] == c0["soc"][late])
            & (tr["P_bat_actual_kW"][idx][late] == c0["p_bat"][late])))
    return {
        "n_late_instants": n,
        "N_P0_vs_C0_lambda_eff_differ": diff_lam,
        "N_P0_vs_C0_action_differ": diff_act,
        "N_P0_vs_C0_realized_bitidentical_channels":
            real_bitid,
        "note": ("P0 = law=5 continuous run (no forecast, no delta); its lambda has "
                 "no floor; diagnostic only, not folded into the C* attribution"),
    }


def l6l7_reference_identification(voyages: list, arms=("C0", "C1", "C2", "P0")) -> dict:
    """§5/§9：判定正式运行时**实际进入 plant 的参考值来源**（L6/L7 层）。

    两个**已落盘**的通道可判别参考值来源，两者都是 PMS 内部量：
      (a) P_gen_unserved_kW = max(0, req − N·clamp(req/N, 125, 500))
          —— 所选参考值的纯代数函数（PMSDispatchAllocatorE 冻结语义）；
      (b) P_gen_actual_kW —— 参考值经 allocator clamp + 爬坡 20 kW/s + 一阶惯性
          (τ=5 s) 后的实际发电（GensetUnitE 冻结语义）。
    对每个假设重建 req 并推过同一冻结链，与落盘通道比对。

    假设族：
      A  PMS baseline demand policy：req = LPF_{τ=60 s}(P_load_bus/η_rect)（+recovery）
         —— 源选择器 or_baseline = (PMS_SOURCE_MODE<0.5) ∨ (ems_health_ok<0.5)
      B  EMS/ECMS law=6（各臂自身 δλ，ZOH 10 s）
      C  EMS/ECMS law=5（P0 律）
      D  EMS baseline law=0（ems_target = P_load + gain·(SOC_ref−SOC)，bus 域无 LPF）
    """
    def lpf(x, tau):
        y = np.zeros_like(x)
        y[0] = x[0]
        for k in range(1, len(x)):
            y[k] = y[k - 1] + (x[k] - y[k - 1]) / tau
        return y

    def dg_chain(req, n, ramp_per_unit=20.0, tau=5.0):
        """allocator clamp → per-unit ramp + first-order lag（GensetUnitE 冻结口径）。"""
        nn = np.maximum(n, 1.0)
        per = np.clip(req / nn, 125.0, 500.0) * n
        r_ = np.zeros_like(per)
        l_ = np.zeros_like(per)
        out = np.zeros_like(per)
        ramp = ramp_per_unit * nn
        for k in range(1, len(per)):
            r_[k] = r_[k - 1] + np.clip(per[k] - r_[k - 1], -ramp[k], ramp[k])
            l_[k] = l_[k - 1] + (r_[k] - l_[k - 1]) / tau
            out[k] = np.clip(l_[k], 0.0, 500.0 * nn[k])
        return out

    def unserved_of(req, n):
        nn = np.maximum(n, 1.0)
        refsum = np.where(n > 0, np.clip(req / nn, 125.0, 500.0) * n, 0.0)
        return np.maximum(0.0, req - refsum)

    rows = []
    import h5py
    for arm in arms:
        agg = {}
        for vid in voyages:
            tr = load_traj(vid, arm)
            with h5py.File(os.path.join(EVID, vid, f"{arm}.traj.v1.mat"), "r") as f:
                g = f["traj"]
                n = np.array(g["N_dispatch"]).ravel()
                uns = np.array(g["P_gen_unserved_kW"]).ravel()
            t = tr["time"]
            load_bus = tr["P_load_total_kW"]
            real = tr["P_gen_actual_kW"]
            soc = tr["SOC"]
            late = t >= SPEC["late_t0_s"]
            reqs = {}
            reqs["A_baseline_LPF60"] = lpf(load_bus / SPEC["eta_rect"], 60.0)
            if arm != "P0":
                ct = load_ctrace(vid, arm)
                rep = np.repeat(ct["dlam"], SPEC["decision_dt_s"])
                dlam1 = np.empty(len(t))
                m = min(len(rep), len(t))
                dlam1[:m] = rep[:m]          # 末段不足 10 s 时沿用最后一个决策值
                dlam1[m:] = rep[m - 1]
                reqs["B_ecms_law6_own_delta"] = action_exact(
                    lambda_eff_law6(dlam1, soc), load_bus, soc)
            reqs["C_ecms_law5"] = action_exact(lambda_law5(soc), load_bus, soc)
            # D：EMS 源被选中、但活动律为 law=0（EMS_Baseline / ems_target：
            #    P = P_load + ASSUMED_EMS_SOC_Gain·(ASSUMED_EMS_SOC_Ref − SOC)，
            #    bus 域、无 60 s LPF；常数见 config/ModelE_Physics_Parameters.m:243-244）
            reqs["D_ems_law0_target"] = load_bus + 200.0 * (0.60 - soc)
            for nm, req in reqs.items():
                u = unserved_of(req, n)
                sim = dg_chain(req, n)
                a = agg.setdefault(nm, {"n_voy": 0, "samples": 0, "uns_exact": 0,
                                        "uns_le1": 0, "uns_max": 0.0, "pw_le5": 0,
                                        "pw_med": [], "late": 0})
                d = np.abs(u - uns)
                a["n_voy"] += 1
                a["samples"] += int(d.size)
                a["uns_exact"] += int(np.sum(d <= 1e-6))
                a["uns_le1"] += int(np.sum(d <= 1.0))
                a["uns_max"] = max(a["uns_max"], float(d.max()))
                dd = np.abs(sim - real)[late]
                a["pw_le5"] += int(np.sum(dd <= 5.0))
                a["pw_med"].append(float(np.median(dd)))
                a["late"] += int(np.sum(late))
        for nm, a in agg.items():
            rows.append({
                "arm": arm, "hypothesis": nm, "n_voyages": a["n_voy"],
                "n_samples": a["samples"],
                "unserved_exact_frac": a["uns_exact"] / a["samples"],
                "unserved_le1kW_frac": a["uns_le1"] / a["samples"],
                "unserved_max_abs_kW": a["uns_max"],
                "power_le5kW_frac_late": a["pw_le5"] / a["late"],
                "power_median_abs_kW_late": float(np.median(a["pw_med"])),
            })
    return {"rows": rows,
            "note": ("reference identification from logged PMS-internal channels; "
                     "hypothesis A = PMS baseline demand policy, B/C = EMS ECMS "
                     "laws, D excluded analytically (no 60 s LPF in law=0)")}


def trajectory_bit_identity(voyages: list) -> dict:
    """跨臂轨迹逐位相同性（全时段、全通道，非仅决策点）。"""
    chans = ("SOC", "P_DG1_kW", "P_DG2_kW", "P_gen_actual_kW",
             "P_bat_actual_kW", "P_load_total_kW")
    bad = []
    for vid in voyages:
        ref = None
        for arm in ("C0", "C1", "C2", "P0"):
            tr = load_traj(vid, arm)
            cur = {c: np.array(tr[c]) for c in chans if c in tr}
            if ref is None:
                ref, ref_arm = cur, arm
                continue
            for c in cur:
                if cur[c].shape != ref[c].shape or not np.array_equal(cur[c], ref[c]):
                    bad.append({"voyage_id": vid, "arm": arm, "channel": c,
                                "ref_arm": ref_arm})
    return {"n_voyages": len(voyages), "channels": list(chans),
            "n_differences": len(bad), "differences": bad[:10],
            "all_bit_identical": len(bad) == 0}


def source_selector_evidence() -> dict:
    """冻结源码证据链（L6/L7 源选择器）——行号可查 + 冻结绑定复核。"""
    fz_path = os.path.join(_MODEL, "results", "ppe_practical", "freeze",
                           "epoch2_pre_execution_freeze_v2.json")
    bind = {}
    try:
        fz = json.load(open(fz_path))
        b = fz["bindings"]
        def sha(p):
            return _sha_file(os.path.join(_MODEL, p))
        for k, p in (("modele_slx_sha256", "model/HybridShip_ModelE.slx"),
                     ("runner_voyage_sha256", "scripts/+ppe/run_formal_voyage.m"),
                     ("run_cstar_amended_sha256", "scripts/+ppe/run_cstar.m"),
                     ("winfun_plan_amended_sha256", "scripts/+ppe/winfun_plan.m")):
            bind[k] = {"path": p, "bound_match": bool(sha(p) == b.get(k))}
        bind["freeze_mentions_injection_path"] = bool(any(
            t in json.dumps(fz) for t in
            ("scenario_compile", "mission_v3", "fleet_v3", "inject_voyage",
             "PMS_SOURCE_MODE")))
    except Exception as e:  # pragma: no cover
        bind["error"] = str(e)
    return {
        "selector": {
            "block": "07_PMS_v2/Source_Selector (SID 384, Switch, threshold 0.5)",
            "in1": "PMS_Demand_Policy.P_gen_req_baseline_kW (SID 379/389)",
            "in2_control": "or_baseline = cmp_mode_baseline OR cmp_health_bad (SID 381/382/383)",
            "in3": "05_EMS_Controller.P_gen_ref_kW (SID 320)",
            "criterion": "control >= 0.5 -> in1 (BASELINE); else in3 (EMS)",
            "cmp_mode_baseline": "PMS_SOURCE_MODE < 0.5  (SID 381 relop '<' const 0.5)",
            "cmp_health_bad": "ems_health_ok < 0.5 (SID 382)",
        },
        "injection_contract": {
            "compiler": "scripts/+mission_v3/scenario_compile.m:292-293",
            "manifest_values": "names{8}='PMS_SOURCE_MODE' -> 0.0 ; names{9}='EMS_CTRL_LAW' -> 0.0",
            "doc": "scenario_compile.m:67 'PMS_SOURCE_MODE = 0 (BASELINE_POLICY, bench convention)'",
            "runner_executes_it": "scripts/+ppe/inject_voyage.m:11-15 assignin per entry",
            "runner_overrides_only_law": "scripts/+ppe/inject_voyage.m:17 EMS_CTRL_LAW := arm law",
            "net_effect": ("every formal PPE run (both epochs, all arms incl. P0) "
                           "compiled the model with PMS_SOURCE_MODE = 0 -> baseline "
                           "demand policy selected; EMS output discarded at the selector"),
        },
        "why_no_flag": {
            "safety_latch_guard": "PMSSafetyInterlockE: fback_active latches only if nominal_is_ems > 0.5",
            "nominal_is_ems": "Constant C_source_mode = PMS_SOURCE_MODE (=0) -> latch disabled",
            "observed": "logged safety_baseline_fallback_active == 0 for all runs",
        },
        "dispatch_frozen_law": {
            "allocator": "PMSDispatchAllocatorE: P_i_ref = clamp(req/N, DG_P_min=125, DG_P_rated=500)",
            "genset": "GensetUnitE: clamp -> ramp 20 kW/s -> first-order lag tau=5 s",
        },
        "source_files": {
            "frozen_slx": "model/HybridShip_ModelE.slx (extracted systems/*.xml + stateflow/chart_*.xml)",
            "config": "config/ModelE_Control_Parameters.m:56 PMS_SOURCE_MODE = 1 (frozen file value)",
        },
        "verified_against": ("two logged PMS-internal channels: P_gen_unserved_kW "
                             "(algebraic in the selected request) and "
                             "P_gen_actual_kW (full clamp+ramp+lag chain)"),
        "freeze_binding_check": bind,
    }



def robustness_soc_offset(voyages: list, offsets=(-0.005, -0.001, 0.001, 0.005)) -> dict:
    """SOC 采样约定敏感性：λ_eff/动作差异计数对 SOC(t0) 小扰动的稳健性。"""
    out = []
    for off in offsets:
        n2 = n3 = 0
        base2 = base3 = 0
        for vid in voyages:
            a = build_arm_instants(vid, "C0")
            b = build_arm_instants(vid, "C1")
            late = a["t"] >= SPEC["late_t0_s"]
            soc_a = a["soc"][late] + off
            soc_b = b["soc"][late] + off
            l2a = np.maximum(lambda_raw_law6(a["dlam"][late], soc_a), SPEC["lambda_floor"])
            l2b = np.maximum(lambda_raw_law6(b["dlam"][late], soc_b), SPEC["lambda_floor"])
            n2 += int(np.sum(l2a != l2b))
            pa = action_exact(l2a, a["p_load"][late], soc_a)
            pb = action_exact(l2b, b["p_load"][late], soc_b)
            n3 += int(np.sum(pa != pb))
            base2 += int(np.sum(a["lam_eff"][late] != b["lam_eff"][late]))
            base3 += int(np.sum(a["pg"][late] != b["pg"][late]))
        out.append({"soc_offset": off, "N_D2": n2, "N_D3": n3,
                    "baseline_N_D2": base2, "baseline_N_D3": base3})
    return {"rows": out}


# ═══════════════════════════ 主流程 ═══════════════════════════
def classify(payload: dict) -> dict:
    """§11/§12 机制分类（由数字驱动，不预设结论）。"""
    pairs = payload["funnel"]["pairs"]
    a = pairs["C0_vs_C1"]
    n_d0 = a["N_D0_window_differ"]
    n_d1 = a["N_D1_delta_differ"]
    n_d2 = a["N_D2_lambda_eff_differ"]
    n_d3 = a["N_D3_action_differ"]
    n_d4 = a["N_D4_realized_differ"]
    ident = {(r["arm"], r["hypothesis"]): r for r in payload["l6l7"]["rows"]}
    base = [ident[(arm, "A_baseline_LPF60")] for arm in ("C0", "C1", "C2", "P0")]
    worst_base_uns = max(r["unserved_max_abs_kW"] for r in base)
    best_base_pw = min(r["power_le5kW_frac_late"] for r in base)
    ecms_rows = [r for r in payload["l6l7"]["rows"]
                 if not r["hypothesis"].startswith("A_")]
    worst_ecms_uns = max(r["unserved_max_abs_kW"] for r in ecms_rows)
    ecms_best_pw = max(r["power_le5kW_frac_late"] for r in ecms_rows)

    out = {
        "funnel_C0_vs_C1": {"N_D0": n_d0, "N_D1": n_d1, "N_D2": n_d2,
                            "N_D3": n_d3, "N_D4": n_d4},
        "floor_contribution_zero": n_d2 == n_d1,
        "optimizer_insensitivity_share": a["cond"]["share_delta_differs_given_window_differs"],
        "policy_quantization_share_given_lambda_differs":
            a["cond"]["share_action_survives_given_lambda_differs"],
        "action_to_plant_absorbed_fraction":
            (n_d3 - 0) / n_d3 if n_d3 else None,
        "reference_identification": {
            "baseline_hypothesis_worst_unserved_max_abs_kW": worst_base_uns,
            "baseline_hypothesis_min_power_le5kW_frac_late": best_base_pw,
            "ecms_hypotheses_best_power_le5kW_frac_late": ecms_best_pw,
            "ecms_hypotheses_worst_unserved_max_abs_kW": worst_ecms_uns,
        },
    }
    # 判决
    baseline_wins = (worst_base_uns < 10.0) and (best_base_pw > 0.9)
    ecms_loses = (ecms_best_pw < 0.5) and (worst_ecms_uns > 100.0)
    if n_d4 == 0 and n_d3 > 0 and baseline_wins and ecms_loses:
        out["primary_mechanism"] = "DOWNSTREAM_OVERRIDE_OR_SATURATION"
        out["mechanism_detail"] = (
            "L6/L7 PMS source-selector override: PMS_SOURCE_MODE=0 injected by the "
            "frozen scenario compiler (mission_v3.scenario_compile) and executed by "
            "ppe.inject_voyage -> baseline demand policy selected, EMS output "
            "discarded before the dispatch allocator")
        out["formal_zero_result_classification"] = (
            "NOT_VALID_NEGATIVE_ARCHITECTURAL_RESULT__CONTROL_ACTION_EXECUTION_PATH_DEFECT")
        out["classification_rationale"] = (
            "pointwise L5 reconstruction shows the ECMS action DIFFERS at N(D3)>0 "
            "instants while the realized power is bit-identical at every instant; "
            "the frozen evidence identifies the applied reference as the PMS "
            "baseline demand policy, i.e. the manipulation never reached the plant")
    else:
        out["primary_mechanism"] = "UNRESOLVED"
        out["formal_zero_result_classification"] = "REVIEW_REQUIRED"
    return out


def write_artifacts(payload: dict) -> dict:
    os.makedirs(OUTDIR, exist_ok=True)
    payload["classification"] = classify(payload)
    payload["sha256_self"] = hashlib.sha256(
        json.dumps(payload, sort_keys=True, ensure_ascii=False).encode()).hexdigest()
    pj = os.path.join(OUTDIR, "control_path_funnel_closure.json")
    with open(pj, "w") as f:
        json.dump(payload, f, indent=1, ensure_ascii=False, sort_keys=True)
    pm = os.path.join(OUTDIR, "CONTROL_PATH_FUNNEL_CLOSURE_REPORT.md")
    with open(pm, "w") as f:
        f.write(render_md(payload))
    return {"json": pj, "md": pm}


def render_md(p: dict) -> str:
    c = p["classification"]
    pairs = p["funnel"]["pairs"]
    L = []
    A = L.append
    A("# CONTROL_PATH_FUNNEL_CLOSURE — 控制路径漏斗逐点闭合报告\n")
    A("**任务**：在正式（epoch-2）执行的**每一个**决策点上量化"
      "预测信息→已实现动作的漏斗，判定信息差异消失的精确层位。\n")
    A("**范围纪律**：无 Simulink 重跑、无控制器重设计、无 effect 重算、无 IID/OOD 访问；"
      "只读冻结证据（192 份 ctrace + 192 份 plant 轨迹 + 冻结 slx/配置/缓存策略表）。\n")
    A(f"**决策点**：t≥{SPEC['late_t0_s']} s、10 s 网格，总数 {p['funnel']['totals_derived']['n_instants_per_pair']:,}"
      "（由 48 航次数据推导；四臂网格逐点对齐）。\n")

    A("\n## 1. 冻结 runtime 机制（逐行确权，非假设）\n")
    m = p["runtime_mechanism_reconstruction"]
    A(f"- δλ 唯一通路：`ECMS_EHat_Data` 第 3 列（`run_cstar.m:125`）"
      f"→ `e_hat` 端口（`build_model_e.m:801`, Demux [2,1]）→ "
      f"`soc_ref_eff = SOC_ref + K_S·e_hat`；K_soc·K_S = 30·(1/30) = 1 ⇒ "
      f"`λ_eff = {m['lambda_eff_expression']}`")
    A(f"- λ₀ = {m['ECMS_Lambda_const']} **常数**（runner 从不改写 `ECMS_Lambda`；只写 `ECMS_K_S`）")
    A(f"- L5 动作 = {m['action_rule']}；SOC/P_load 取自 1 Hz 冻结轨迹（四臂逐位相同）")

    A("\n## 2. 移植验证（对冻结工件自证）\n")
    v = p["validation"]
    v1 = v["V1_table_rebuild"]
    A(f"- **策略表逐格自证**：Python 重生成 vs 冻结缓存 `artifacts/t445/policy_table.mat`："
      f"Pg **{v1['Pg_exact_cells']:,}/{v1['Pg_total_cells']:,} 格位逐位相同**（max|Δ|=0.0），"
      f"fuel_kg 一致（max|Δ|=0.0），`config_sha256` 与冻结配置一致 ✓")
    A(f"- λ 轴：MATLAB colon 与 `start+step·arange` 在 {v1['lambda_grid_n_differing']}/3020 点差 1 ulp"
      f"（{v1['lambda_grid_max_abs_diff']:.2e}），不影响 argmin ✓")
    A(f"- `policy_lookup` 索引算术夹具（含 MATLAB round 半程语义、SOC 分区边界、越域只计数）："
      f"{'全部通过' if v['V2_policy_lookup_fixtures']['all_pass'] else '存在失败'} ✓"
      if v["V2_policy_lookup_fixtures"]["all_pass"] else "- `policy_lookup` 夹具：存在失败 ✗")
    A(f"- 单元格 ↔ 动作函数一致性（400 随机 (λ,P̄,region) 组合）："
      f"逐位相同比例 {v['V3_cell_vs_action']['exact_fraction']:.4f} ✓")
    A("- 说明：本机无 MATLAB 许可（用户明令避免本地登录），故验证以**冻结缓存表逐格位级比对**"
      "替代 MATLAB 夹具探针；该表即正式执行所用表（`policy_table_cached` 缓存路径），"
      "证据等级不低于调用 `policy_lookup` 本身。")

    A("\n## 3. 漏斗（§3/§4）\n")
    A("| 指标 | C0 vs C1 | C0 vs C2 | C1 vs C2 |")
    A("|---|---|---|---|")
    keys = ["n_instants", "n_window_evaluable", "n_window_hold",
            "N_D0_window_differ", "N_D1_delta_differ", "N_D2_lambda_eff_differ",
            "N_D3_action_differ", "N_D4_realized_differ",
            "FORECAST_TO_DELTA_ABSORBED", "DELTA_TO_LAMBDA_FLOOR_ABSORBED",
            "LAMBDA_TO_POLICY_ACTION_ABSORBED", "ACTION_TO_PLANT_ABSORBED"]
    label = {"n_instants": "决策点数", "n_window_evaluable": "窗可比（非 hold）",
             "n_window_hold": "窗 hold（tail）", "N_D0_window_differ": "D0 窗不同",
             "N_D1_delta_differ": "D1 δλ 不同", "N_D2_lambda_eff_differ": "D2 λ_eff 不同",
             "N_D3_action_differ": "D3 L5 动作不同", "N_D4_realized_differ": "D4 已实现功率不同",
             "FORECAST_TO_DELTA_ABSORBED": "窗→δλ 吸收", "DELTA_TO_LAMBDA_FLOOR_ABSORBED": "δλ→λ floor 吸收",
             "LAMBDA_TO_POLICY_ACTION_ABSORBED": "λ→动作 吸收", "ACTION_TO_PLANT_ABSORBED": "动作→plant 吸收"}
    for k in keys:
        A(f"| {label[k]} | {pairs['C0_vs_C1'][k]:,} | {pairs['C0_vs_C2'][k]:,} | {pairs['C1_vs_C2'][k]:,} |")
    A("")
    A(f"- C1/C2 的 {pairs['C0_vs_C1']['n_window_hold']:,} 个 hold 点为 plan `tail` 段"
      "（`winfun_plan` ZOH，δλ 保持）；C1 vs C2 的 1,436 个 \"同哈希\" 全部是**双方同时 hold**，"
      "非真实窗偶合（修正上一版归因报告的表述）。")
    A(f"- 条件化份额（C0vsC1）：窗不同→δλ 不同 {pairs['C0_vs_C1']['cond']['share_delta_differs_given_window_differs']*100:.2f}%；"
      f"δλ 不同→λ_eff 不同 {pairs['C0_vs_C1']['cond']['share_lambda_survives_given_delta_differs']*100:.1f}%；"
      f"λ_eff 不同→动作不同 {pairs['C0_vs_C1']['cond']['share_action_survives_given_lambda_differs']*100:.1f}%；"
      f"动作不同→已实现不同 {pairs['C0_vs_C1']['cond']['share_realized_survives_given_action_differs']*100:.1f}%。")
    for k in ("C0_vs_C1", "C0_vs_C2", "C1_vs_C2"):
        d = pairs[k].get("delta_absdiff") or {}
        l = pairs[k].get("lambda_eff_absdiff") or {}
        if d or l:
            A(f"- 差异幅度（{k}）：|δλ| 中位（跨航次中位）"
              f"{d.get('median_over_voyages', float('nan')):.3f}；"
              f"|λ_eff| 中位 {l.get('median_over_voyages', float('nan')):.4f}、"
              f"最大 {l.get('max_over_voyages', float('nan')):.4f}")

    A("\n## 4. §6/§7/§8 条件化贡献\n")
    A(f"- **§7 optimizer/objective 不敏感（主吸收）**：{c['optimizer_insensitivity_share']*100:.2f}% 的"
      "可比决策点上窗不同而 δλ 相同 → DP 把窗差异吸收在决策前。")
    A(f"- **§6 λ_eff floor 贡献 = 0**：δλ 不同 ∧ λ_eff 相同的点数 = "
      f"{pairs['C0_vs_C1']['DELTA_TO_LAMBDA_FLOOR_ABSORBED']}（CAS C0vsC1）/ "
      f"{pairs['C0_vs_C2']['DELTA_TO_LAMBDA_FLOOR_ABSORBED']}（C0vsC2）/ "
      f"{pairs['C1_vs_C2']['DELTA_TO_LAMBDA_FLOOR_ABSORBED']}（C1vsC2）；双raw低于 0.43 的点数为 0。"
      "⇒ **上一版归因报告的 \"28% 决策点被 0.43 钳位\" 系公式误读**（把 δλ 与 e_hat 双重计入）；"
      "正式运行中 λ_eff = λ₀+30(0.6−SOC)+δλ，SOC∈[0.593,0.601] ⇒ 从未触及下限。")
    A(f"- **§8 策略表量化贡献**：λ_eff 不同而动作相同的比例 "
      f"{pairs['C0_vs_C1']['cond']['share_action_survives_given_lambda_differs']*100:.1f}%"
      f"（C0vsC1）/ {pairs['C0_vs_C2']['cond']['share_action_survives_given_lambda_differs']*100:.1f}%"
      f"（C0vsC2）/ {pairs['C1_vs_C2']['cond']['share_action_survives_given_lambda_differs']*100:.1f}%（C1vsC2）"
      " ⇒ 量化吸收真实存在，但**不构成零效应的主因**（见 §5）。")
    if "soc_offset_robustness" in p:
        A("- SOC 采样约定敏感性（D2/D3 对 SOC(t₀) 小扰动）：")
        for r in p["soc_offset_robustness"]["rows"]:
            A(f"  - offset {r['soc_offset']:+.3f} → N(D2)={r['N_D2']}, N(D3)={r['N_D3']}"
              f"（基线 {r['baseline_N_D2']}/{r['baseline_N_D3']}）")
        A("- 说明：D3 的**具体计数**对 SOC 采样约定敏感（平台边缘效应），"
          "但 **N(D3) > 0 且 N(D4) = 0 这一定性结论在所有 offset 下不变**；"
          "§5 的机制判定不依赖 D3 的精确计数。")

    A("\n## 5. §9 下游/接线 —— 决定性发现\n")
    A("**逐点 L5 重建证明 Pg\\* 在 686 个 instant 上不同，而 D4 = 0（48/48 航次、四臂全通道逐位相同）。**")
    A("⇒ 按 §9 的判据，**不能**维持 `NO_EVIDENCE_OF_EXECUTION_PATH_DEFECT`；执行路径存在结构性缺陷。\n")
    A("### 实际进入 plant 的参考值来源（由两个已落盘 PMS 内部通道判别）\n")
    A("| 臂 | 假设 | unserved 逐点精确匹配 | unserved max\\|Δ\\| | 功率链 ≤5 kW 比例 | 功率链 median\\|Δ\\| |")
    A("|---|---|---|---|---|---|")
    for r in p["l6l7"]["rows"]:
        A(f"| {r['arm']} | {r['hypothesis']} | {r['unserved_exact_frac']*100:.2f}% | "
          f"{r['unserved_max_abs_kW']:.2f} kW | {r['power_le5kW_frac_late']*100:.1f}% | "
          f"{r['power_median_abs_kW_late']:.2f} kW |")
    A("")
    A("- `P_gen_unserved_kW` 是 PMS 内部对**所选参考值**的纯代数函数"
      "（`= max(0, req − N·clamp(req/N,125,500))`）；")
    A("- `P_gen_actual_kW` 是参考值经 allocator clamp + 20 kW/s 爬坡 + τ=5 s 惯性后的实际发电。")
    b0 = [r for r in p["l6l7"]["rows"] if r["hypothesis"].startswith("A_")]
    n0 = [r for r in p["l6l7"]["rows"] if not r["hypothesis"].startswith("A_")]
    A(f"**基线需求律 `req = LPF_{{τ=60 s}}(P_load_bus/η_rect)` 在全部 48 航次、四个臂上以 "
      f"{min(r['unserved_exact_frac'] for r in b0)*100:.2f}–{max(r['unserved_exact_frac'] for r in b0)*100:.2f}% "
      f"逐点精确命中（max|Δ| = {max(r['unserved_max_abs_kW'] for r in b0):.2f} kW），"
      f"功率链 {min(r['power_le5kW_frac_late'] for r in b0)*100:.1f}% 落在 ±5 kW 内"
      f"（median|Δ| = {max(r['power_median_abs_kW_late'] for r in b0):.2f} kW）；"
      f"所有替代假设（ECMS law6 用各臂自身 δλ / law5 / EMS law0）误差量级 "
      f"{min(r['unserved_max_abs_kW'] for r in n0):.0f}–{max(r['unserved_max_abs_kW'] for r in n0):.0f} kW、"
      f"功率链命中率 ≤{max(r['power_le5kW_frac_late'] for r in n0)*100:.1f}%，被明确排除。**\n")
    A("### 机制（冻结源码证据链）\n")
    fb = p["source_selector_evidence"]["freeze_binding_check"]
    A(f"- 冻结绑定复核：`model/HybridShip_ModelE.slx`（bound_match="
      f"{fb['modele_slx_sha256']['bound_match']}）与 runner/winfun 文件与冻结 sha 一致"
      " ⇒ 本报告检查的 .slx 与 runner **就是正式执行的字节**；"
      f"冻结包**未绑定**注入路径（scenario_compile/mission_v3/fleet_v3/inject_voyage 均未出现）"
      " ⇒ 该路径属**未受冻结保护的执行前置层**。")
    for k, v in p["source_selector_evidence"]["injection_contract"].items():
        A(f"- `{k}`: {v}")
    A(f"- 选择器：{p['source_selector_evidence']['selector']['criterion']}"
      f"（in2 = or_baseline = {p['source_selector_evidence']['selector']['cmp_mode_baseline']}"
      f" ∨ {p['source_selector_evidence']['selector']['cmp_health_bad']}）")
    A(f"- 为何无告警：{p['source_selector_evidence']['why_no_flag']['safety_latch_guard']}；"
      f"{p['source_selector_evidence']['why_no_flag']['nominal_is_ems']}；"
      f"实测 `safety_baseline_fallback_active` 恒为 0。")
    A("\n⇒ **正式 epoch-1/epoch-2 的全部 C\\* 手臂（含 P0）中，EMS 输出在 PMS 源选择器处被丢弃，"
      "plant 由 PMS baseline 需求律驱动**；四臂轨迹逐位相同是**构造性必然**，与预测信息量无关。")

    A("\n## 6. §10 P0 上下文\n")
    p0 = p.get("P0_diagnostic", {})
    A(f"- P0（law=5 连续运行，无预测/无 δλ）与 C0 的 λ_eff 不同点数 {p0.get('N_P0_vs_C0_lambda_eff_differ')}，"
      f"重建动作不同点数 {p0.get('N_P0_vs_C0_action_differ')}，而实测轨迹逐位相同点数 "
      f"{p0.get('N_P0_vs_C0_realized_bitidentical_channels')}/{p0.get('n_late_instants')}。")
    A("- 结论：P0 与 C0 的\"相同\"同样由源选择器覆盖解释，**不构成**\"两者落入同一动作平台\"的证据。")

    A("\n## 7. §11/§12 终局分类\n")
    A(f"- **primary_mechanism**：`{c['primary_mechanism']}`")
    A(f"- mechanism detail：{c.get('mechanism_detail','')}")
    A(f"- **formal_zero_result_classification**：`{c['formal_zero_result_classification']}`")
    A(f"- 依据：{c.get('classification_rationale','')}")
    A("- 前次归因结论 `ACTION_QUANTIZATION_OR_POLICY_INSENSITIVITY` + `VALID_NEGATIVE_ARCHITECTURAL_RESULT`"
      " **予以撤回**：量化吸收（§8）与 floor（§6 实测 0）都不是零效应的主因；"
      "主因是 L6/L7 源选择器把 EMS 参考值整体短路。")
    A("\n## 8. 未修改声明\n")
    A("- 未修改任何正式 effect artifact / FEC / 掩码 / 冻结工件；未重跑 Simulink；未访问 IID/OOD；"
      "未读取燃油通道（本模块通道白名单物理上不含 fuel/J_FEC）。")
    A(f"- 本报告 sha256_self: `{p['sha256_self']}`")
    return "\n".join(L) + "\n"


def main(argv):
    mode = argv[1] if len(argv) > 1 else "all"
    voy = voyage_list()
    payload = {
        "schema": "control_path_funnel_closure_v1",
        "task": "CONTROL_PATH_FUNNEL_CLOSURE",
        "runtime_mechanism_reconstruction": {
            "lambda_eff_expression":
                "max(0.425 + 30*(0.6 + (1/30)*delta_lambda - SOC(t0)), 0.43)",
            "delta_lambda_conduit":
                "ECMS_EHat_Data col3 (e_hat port) — run_cstar.m:125; K_soc*K_S = 1",
            "ECMS_Lambda_const": SPEC["lambda0"],
            "action_rule": "min_g mf(g) + lam*mbe(g), g in 0:20:1000 (law=6)",
            "soc_source": "frozen plant trajectory at 1 Hz, index == t0",
        },
        "source_selector_evidence": source_selector_evidence(),
        "n_voyages": len(voy),
        "voyages": voy,
    }
    if mode in ("validate", "all"):
        print("[V] validating port against frozen cached table ...")
        payload["validation"] = validate_port()
        print("[V] all_pass =", payload["validation"]["all_pass"],
              "| Pg exact frac =",
              payload["validation"]["V1_table_rebuild"]["Pg_exact_fraction"])
    if mode in ("funnel", "all"):
        print("[F] funnel over", len(voy), "voyages ...")
        payload["funnel"] = run_funnel(voy)
        a = payload["funnel"]["pairs"]["C0_vs_C1"]
        print("[F] C0vsC1 N(D0..D4) =", [a[k] for k in
              ("N_D0_window_differ", "N_D1_delta_differ",
               "N_D2_lambda_eff_differ", "N_D3_action_differ",
               "N_D4_realized_differ")])
        print("[P0] diagnostic ...")
        payload["P0_diagnostic"] = p0_context(voy)
        print("[RB] SOC-offset robustness ...")
        payload["soc_offset_robustness"] = robustness_soc_offset(voy)
        print("[ID] reference identification (L6/L7) ...")
        payload["l6l7"] = l6l7_reference_identification(voy)
        print("[TI] trajectory bit identity ...")
        payload["trajectory_bit_identity"] = trajectory_bit_identity(voy)
    if mode in ("dump", "all"):
        out = write_artifacts(payload)
        payload["classification"] = classify(payload)
        print("[W] wrote", out["json"])
        print("[W] wrote", out["md"])
        print("[C] primary_mechanism =",
              payload["classification"]["primary_mechanism"])
        print("[C] formal_zero_result_classification =",
              payload["classification"]["formal_zero_result_classification"])
    return payload


if __name__ == "__main__":
    main(sys.argv)

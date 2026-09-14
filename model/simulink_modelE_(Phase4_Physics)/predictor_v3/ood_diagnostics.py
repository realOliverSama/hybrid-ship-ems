# -*- coding: utf-8 -*-
"""ood_diagnostics.py — T-443 三轴 OOD 诊断（DGN-E-14；FR-600~603）。

- 轴①输入漂移 / 轴②target 漂移：数据集级证据（每冻结 OOD 定义算一次，不按模型重算）；
- 轴③预测退化：模型级（每候选/臂 × 每条件）R_deg + 正交字段；
- W1 = scipy.stats.wasserstein_distance（精确经验分布）；归一化分母 = train 参照 std
 （diagnostics_config 冻结）；分母为零 ⇒ UNDEFINED_CONSTANT_SUPPORT；
- 子采样：N > 200000 用确定性顺序无关 hash bottom-k（禁等距 stride）；
- 全部 OOD 条件自动附 SMALL_N_CAUTION（split_group < 20）。
"""
from __future__ import annotations

import hashlib
import json
import os

import numpy as np
from scipy.stats import wasserstein_distance

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)

from predictor_v3.inference_stats import (degradation_fields,  # noqa: E402
                                          two_sample_bootstrap_diff,
                                          two_sample_bootstrap_ratio)

SUBSAMPLE_CAP = 200000


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def _subsample_values(values: np.ndarray, key_prefix: str) -> np.ndarray:
    """确定性顺序无关 hash bottom-k（DGN-E-14；禁等距 stride）。"""
    v = np.asarray(values, dtype=np.float64).ravel()
    if v.size <= SUBSAMPLE_CAP:
        return v
    keys = [f"{key_prefix}|{i}".encode() for i in range(v.size)]
    order = np.argsort([hashlib.sha256(k).digest()[:8] for k in keys],
                       kind="stable")[:SUBSAMPLE_CAP]
    return v[np.sort(order)]


def normalized_w1(sample: np.ndarray, ref: np.ndarray, ref_std: float,
                  key_prefix: str) -> dict:
    """归一化 W1（义务 8）：分母 = train 参照 std；零 ⇒ UNDEFINED_CONSTANT_SUPPORT。"""
    ref_std = float(ref_std)
    if ref_std < 1e-12:
        return {"w1_normalized": "UNDEFINED_CONSTANT_SUPPORT", "ref_std": ref_std}
    s = _subsample_values(sample, key_prefix + "|s")
    r = _subsample_values(ref, key_prefix + "|r")
    w = float(wasserstein_distance(s, r))
    return {"w1_normalized": w / ref_std, "w1_raw": w, "ref_std": ref_std}


def percentile_out_of_support(sample: np.ndarray, p01: float, p99: float) -> dict:
    s = np.asarray(sample, dtype=np.float64).ravel()
    return {"frac_below_p01": float((s < p01).mean()),
            "frac_above_p99": float((s > p99).mean()),
            "p95_abs_z": None, "p99_abs_z": None, "max_abs_z": None}


def z_stats(sample: np.ndarray, mean: float, std: float) -> dict:
    std = float(std)
    if std < 1e-12:
        return {"p95_abs_z": None, "p99_abs_z": None, "max_abs_z": None,
                "note": "UNDEFINED_CONSTANT_SUPPORT"}
    z = np.abs((np.asarray(sample, dtype=np.float64) - mean) / std)
    return {"p95_abs_z": float(np.percentile(z, 95)),
            "p99_abs_z": float(np.percentile(z, 99)),
            "max_abs_z": float(z.max())}


# ------------------------------------------------------------ 轴① 输入漂移

def axis1_input_drift(*, ood_view, iid_view, train_ref: dict,
                      h1_channels: list, kf_channels: list,
                      models_consumer_channels: dict | None = None) -> dict:
    """轴①：每条件 × 每通道（H1 池化 + per-bin；KF per-lead + 池化）。

    models_consumer_channels: {model_id: [channels]}（解释过滤——H1-only 模型
    不挂 F 通道归因，FR-600）。
    """
    iid_raw = iid_view.raw_arrays()
    ood_raw = ood_view.raw_arrays()
    per_cond = {}
    conds = ood_view.ood_conditions
    for cond in sorted(set(conds.tolist())):
        sel = conds == cond
        ch_res = {}
        for i, ch in enumerate(h1_channels):
            o = ood_raw["hist_raw"][sel, :, i]
            r = iid_raw["hist_raw"][:, :, i]
            ref = train_ref["h1"][ch]
            res = {"pooled": percentile_out_of_support(o, *ref["pooled_p01_p99"]),
                   "w1": normalized_w1(o, r, ref["pooled_std"],
                                       f"ood/{cond}/h1/{ch}"),
                   "z": z_stats(o, float(np.mean(r)), float(np.std(r)))}
            ch_res[ch] = res
        for i, ch in enumerate(kf_channels):
            o = ood_raw["kf_raw"][sel, :, i]
            r = iid_raw["kf_raw"][:, :, i]
            ref = train_ref["kf"][ch]
            res = {"per_lead": percentile_out_of_support(o, *ref["per_lead_p01_p99"]),
                   "w1": normalized_w1(o, r, ref["pooled_std"],
                                       f"ood/{cond}/kf/{ch}"),
                   "z": z_stats(o, float(np.mean(r)), float(np.std(r)))}
            ch_res[ch] = res
        # top-shifting 通道排名（按归一化 W1 降序，跳过常值/UNDEFINED）
        ranking = []
        for ch, res in ch_res.items():
            w = res["w1"].get("w1_normalized")
            if isinstance(w, (int, float)) and np.isfinite(w):
                ranking.append((ch, w))
        ranking.sort(key=lambda x: -x[1])
        per_cond[cond] = {"channels": ch_res,
                          "top_shifting_5": [c for c, _ in ranking[:5]]}
    return {"per_condition": per_cond,
            "compare": "ood_condition vs iid_test（主）+ ood vs train（参照，"
                       "train_ref 见 diagnostics_config）",
            "constant_fields_note": "ctrl_mode/manual_power_cmd_pct 常值通道单列，"
                                    "禁标准化距离比（FR-581）"}


# ------------------------------------------------------------ 轴② target 漂移

def axis2_target_drift(*, ood_view, iid_view, train_ref: dict) -> dict:
    """轴②：E300 主标量（显式注明 E300=ȳ/12 等价）+ per-lead + 分段。"""
    iid_e = iid_view.raw_arrays()["e300_raw"]
    ood_raw = ood_view.raw_arrays()
    conds = ood_view.ood_conditions
    per_cond = {}
    ref = train_ref["e300"]
    for cond in sorted(set(conds.tolist())):
        sel = conds == cond
        e = ood_raw["e300_raw"][sel]
        y_ood = ood_raw["y_raw"]
        y_iid = iid_view.raw_arrays()["y_raw"]
        per_cond[cond] = {
            "e300": {
                "mean": float(e.mean()), "std": float(e.std()),
                "percentiles": {p: float(np.percentile(e, q)) for p, q in
                                [("p01", 1), ("p05", 5), ("p50", 50),
                                 ("p95", 95), ("p99", 99)]},
                "frac_below_train_p01": float((e < ref["p01_p99"][0]).mean()),
                "frac_above_train_p99": float((e > ref["p01_p99"][1]).mean()),
                "w1_vs_iid": normalized_w1(e, iid_e, ref["pooled_std"],
                                           f"ood/{cond}/e300"),
            },
            "per_lead_mean": [float(y_ood[sel][:, k].mean()) for k in range(30)],
        }
    return {"per_condition": per_cond,
            "identity_note": "E300 = ȳ/12（30×10s 几何下线性等价，不得计为两条"
                             "独立证据，FR-601）",
            "interpretation_invariant": "target shift ≠ predictor failure"}


# ------------------------------------------------------------ 轴③ 预测退化

def axis3_degradation(*, per_group_ood: dict, per_group_iid: dict,
                      metric: str = "pinball", b: int = 10000,
                      rng: np.random.Generator | None = None) -> dict:
    """轴③ per 模型×条件：two-sample cluster bootstrap → R_deg + 正交字段。

    per_group_ood/iid: {condition: [G 组值]}（同 DGN-E-12 冻结指标口径）。
    """
    rng = rng or np.random.default_rng()
    out = {}
    for cond, vals_o in per_group_ood.items():
        vals_i = per_group_iid[cond]
        r = two_sample_bootstrap_ratio(np.asarray(vals_o), np.asarray(vals_i),
                                       b=b, rng=rng)
        fields = degradation_fields(r)
        out[cond] = {
            "metric": metric,
            "R_deg": {"point": r.get("point"), "ci_lower": r.get("ci_lower"),
                      "ci_upper": r.get("ci_upper"), "status": r["status"]},
            **fields,
            "small_n_caution": len(vals_o) < 20,
            "n_groups_ood": len(vals_o), "n_groups_iid": len(vals_i),
        }
    return out

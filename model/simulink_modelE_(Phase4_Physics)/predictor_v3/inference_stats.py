# -*- coding: utf-8 -*-
"""inference_stats.py — bootstrap 统计推断 + 等价带（T-005，DGN-E-12；FR-599/602/604）。

冻结算法：
- paired split_group bootstrap（val 同 split 候选对比）：B=10000；每次有放回重采样
  全部 val split_group；组统计 = 组内 voyage 均值再均值（上游 metrics 已完成——
  本模块只消费 per-group 值）；CI = percentile 2.5/97.5%；
  相对差 replicate 定义 δ_rel* = (M_c* − M_b*)/M_b*——分母用重采样后的参照指标本身；
  分母数值非正/未定义 ⇒ 该比较 fail-safe 记 UNDEFINED（保守不判等价）；
- two-sample cluster bootstrap（OOD vs IID-test，轴③）：双侧独立重采样各自
  split_group → 各算冻结指标 → 预注册 ratio/difference；ratio 仅正误差指标
 （pinball/MAE/RMSE）；差值用于 ΔCE80/ΔPICP/ΔR²；
- 多种子聚合（obligation 9）：先 m̃(c,g)=median over seeds{0,1,2} of m(c,s,g)
 （per-split_group seed median），再跨组聚合/配对 bootstrap；种子不是独立重复单元；
  禁 best-seed；全候选同规则；
- 等价带（obligation 2）：相对差 CI ⊆ [−1%,+1%] ⇒ 实际等价。
"""
from __future__ import annotations

import numpy as np

B_BOOTSTRAP = 10000
EQUIVALENCE_BAND = (-0.01, 0.01)
CI_PERCENTILES = (2.5, 97.5)


def _check_groups(vals) -> np.ndarray:
    v = np.asarray(vals, dtype=np.float64)
    if v.ndim != 1 or v.size < 2:
        raise ValueError(f"组值须为一维且 ≥2 组，实际 shape={v.shape}")
    return v


def paired_bootstrap_rel_diff(vals_c, vals_b, b: int = B_BOOTSTRAP,
                              rng: np.random.Generator | None = None) -> dict:
    """候选 c vs 参照 b 的相对差 paired bootstrap（DGN-E-12 δ_rel 定义）。

    返回 {point, delta_rel, ci_lower, ci_upper, status}；
    status ∈ {"OK","UNDEFINED"}——分母 M_b* 非正/非有限的 replicate 占比 >0 时
    该 replicate 丢弃；有效 replicate < B 的一半或点估计分母非正 ⇒ UNDEFINED
    （保守不判等价）。
    """
    rng = rng or np.random.default_rng()
    c = _check_groups(vals_c)
    b_ = _check_groups(vals_b)
    if c.shape != b_.shape:
        raise ValueError("paired 两组值须同形状（同一 split_group 集合）")
    g = c.shape[0]
    point = None
    mb = float(b_.mean())
    if np.isfinite(mb) and mb > 0:
        point = float((c.mean() - mb) / mb)
    idx = rng.integers(0, g, size=(b, g))
    mc_star = c[idx].mean(axis=1)
    mb_star = b_[idx].mean(axis=1)
    ok = np.isfinite(mb_star) & (mb_star > 0)
    if ok.sum() < b // 2 or point is None:
        return {"point": point, "delta_rel": None, "ci_lower": None,
                "ci_upper": None, "status": "UNDEFINED",
                "valid_replicates": int(ok.sum())}
    delta = (mc_star[ok] - mb_star[ok]) / mb_star[ok]
    lo, hi = np.percentile(delta, CI_PERCENTILES)
    return {"point": point, "delta_rel": delta,
            "ci_lower": float(lo), "ci_upper": float(hi), "status": "OK",
            "valid_replicates": int(ok.sum())}


def paired_bootstrap_diff(vals_c, vals_b, b: int = B_BOOTSTRAP,
                          rng: np.random.Generator | None = None) -> dict:
    """有符号差 paired bootstrap（Δ_CE80 语义：禁止先取 abs 再算 CI，R6）。"""
    rng = rng or np.random.default_rng()
    c = _check_groups(vals_c)
    b_ = _check_groups(vals_b)
    g = c.shape[0]
    point = float(c.mean() - b_.mean())
    idx = rng.integers(0, g, size=(b, g))
    delta = c[idx].mean(axis=1) - b_[idx].mean(axis=1)
    ok = np.isfinite(delta)
    if ok.sum() < b // 2:
        return {"point": point, "ci_lower": None, "ci_upper": None,
                "status": "UNDEFINED", "valid_replicates": int(ok.sum())}
    lo, hi = np.percentile(delta[ok], CI_PERCENTILES)
    return {"point": point, "ci_lower": float(lo), "ci_upper": float(hi),
            "status": "OK", "valid_replicates": int(ok.sum())}


def two_sample_bootstrap_ratio(vals_ood, vals_iid, b: int = B_BOOTSTRAP,
                               rng: np.random.Generator | None = None) -> dict:
    """two-sample cluster bootstrap ratio（R_deg；仅正误差/proper-loss 指标）。

    双侧独立重采样各自 split_group（OOD 条件 n_g 组 / IID-test 18 组）→
    各算冻结指标 → ratio；IID 分母非正/非有限 replicate 丢弃；
    有效 replicate < B/2 ⇒ UNDEFINED。
    """
    rng = rng or np.random.default_rng()
    o = _check_groups(vals_ood)
    i = _check_groups(vals_iid)
    point = None
    mi = float(i.mean())
    if np.isfinite(mi) and mi > 0:
        point = float(o.mean() / mi)
    idx_o = rng.integers(0, o.shape[0], size=(b, o.shape[0]))
    idx_i = rng.integers(0, i.shape[0], size=(b, i.shape[0]))
    mo = o[idx_o].mean(axis=1)
    mi_star = i[idx_i].mean(axis=1)
    ok = np.isfinite(mi_star) & (mi_star > 0)
    if ok.sum() < b // 2 or point is None:
        return {"point": point, "ci_lower": None, "ci_upper": None,
                "status": "UNDEFINED", "valid_replicates": int(ok.sum())}
    ratio = mo[ok] / mi_star[ok]
    lo, hi = np.percentile(ratio, CI_PERCENTILES)
    return {"point": point, "ci_lower": float(lo), "ci_upper": float(hi),
            "status": "OK", "valid_replicates": int(ok.sum())}


def two_sample_bootstrap_diff(vals_ood, vals_iid, b: int = B_BOOTSTRAP,
                              rng: np.random.Generator | None = None) -> dict:
    """two-sample cluster bootstrap 差值（ΔCE80/ΔPICP/ΔR² 语义）。"""
    rng = rng or np.random.default_rng()
    o = _check_groups(vals_ood)
    i = _check_groups(vals_iid)
    point = float(o.mean() - i.mean())
    idx_o = rng.integers(0, o.shape[0], size=(b, o.shape[0]))
    idx_i = rng.integers(0, i.shape[0], size=(b, i.shape[0]))
    delta = o[idx_o].mean(axis=1) - i[idx_i].mean(axis=1)
    ok = np.isfinite(delta)
    if ok.sum() < b // 2:
        return {"point": point, "ci_lower": None, "ci_upper": None,
                "status": "UNDEFINED", "valid_replicates": int(ok.sum())}
    lo, hi = np.percentile(delta[ok], CI_PERCENTILES)
    return {"point": point, "ci_lower": float(lo), "ci_upper": float(hi),
            "status": "OK", "valid_replicates": int(ok.sum())}


# ------------------------------------------------------------ 多种子聚合

def median_over_seeds(vals: np.ndarray) -> np.ndarray:
    """per-split_group seed median：m̃(c,g)=median over seeds of m(c,s,g)。

    vals [S,G]（seed × split_group）；返回 [G]。种子永非独立重复单元。
    """
    v = np.asarray(vals, dtype=np.float64)
    if v.ndim != 2:
        raise ValueError(f"须为 [seeds, groups] 二维，实际 {v.shape}")
    return np.median(v, axis=0)


def in_equivalence_band(ci_lower: float, ci_upper: float,
                        band: tuple = EQUIVALENCE_BAND) -> bool:
    """等价带判定（obligation 2）：CI ⊆ [−1%,+1%] ⇒ 实际等价。"""
    lo, hi = band
    return bool(np.isfinite(ci_lower) and np.isfinite(ci_upper)
                and ci_lower >= lo and ci_upper <= hi)


# ------------------------------------------------------------ 轴③ 正交字段（FR-602）

def degradation_fields(ratio: dict) -> dict:
    """轴③正交字段（R2 修正语义；ratio = two_sample_bootstrap_ratio 产物）。

    - effect_band ∈ {SMALL (R≤1.2), MODERATE (1.2<R≤1.5), LARGE (R>1.5)}
     （实际效应带，非显著性定义）；
    - ci_lower_exceeds_1 = CI_lower(R) > 1.0；
    - ci_upper_within_20pct = CI_upper(R) ≤ 1.2；
    - substantial_degradation = (R_point > 1.5 AND CI_lower > 1.2)；
    - 派生汇总标签 precedence：MAINTAINED_WITHIN_20PCT（ci_upper_within_20pct）
      → SUPPORTED_DEGRADATION（ci_lower_exceeds_1）→ UNCERTAIN；
      正交字段为 canonical。
    """
    r = ratio.get("point")
    lo, hi = ratio.get("ci_lower"), ratio.get("ci_upper")
    if ratio.get("status") != "OK" or r is None or lo is None or hi is None:
        return {"effect_band": None, "ci_lower_exceeds_1": None,
                "ci_upper_within_20pct": None, "substantial_degradation": None,
                "summary_label": "UNCERTAIN", "status": "UNDEFINED"}
    band = "SMALL" if r <= 1.2 else ("MODERATE" if r <= 1.5 else "LARGE")
    lo_ex = bool(lo > 1.0)
    hi_w20 = bool(hi <= 1.2)
    substantial = bool(r > 1.5 and lo > 1.2)
    if hi_w20:
        label = "MAINTAINED_WITHIN_20PCT"
    elif lo_ex:
        label = "SUPPORTED_DEGRADATION"
    else:
        label = "UNCERTAIN"
    return {"effect_band": band, "ci_lower_exceeds_1": lo_ex,
            "ci_upper_within_20pct": hi_w20,
            "substantial_degradation": substantial,
            "summary_label": label, "status": "OK"}

# -*- coding: utf-8 -*-
"""metrics.py — Q2 指标栈 + 三视图层级聚合（T-005，DGN-E-11；FR-598/599）。

冻结语义：
- 主点预测 = q50（条件中位数，如实表述）；主指标 MAE(q50)、次 RMSE(q50)；
- 概率证据：mean pinball、per-quantile pinball、PICP80、coverage_error_80=|PICP80−0.80|、
  MPIW、crossing rate；分段 short 10–60s（bins 1–6）/ mid 70–180s（7–18）/
  long 190–300s（19–30）；
- 统计层级 window → voyage → split_group（R7）：voyage 层先聚合**原语**
 （pinball=原子均值；MAE=窗×lead 绝对误差均值；RMSE=sqrt(窗×lead 平方误差均值)；
  PICP=命中数/合格总数；MPIW=区间宽均值；E300-MAE=窗级 |ΔE300| 均值；
  E300-RMSE=sqrt(窗级 E300 误差平方均值)），再 voyage → split_group 均值；
  禁 mean-of-RMSE（窗口级）、禁平均 R²；
- 三视图：split_group_macro（主推断）/ voyage_macro / window_pooled（禁用于窄 CI）；
- E300 = Σ q50_k·(10/3600)；R² = population 统计（全部合格对一次计算；
  目标方差 < 1e-12 → R2_UNDEFINED）；
- 归一化指标分母：per-lead = frozen formal_norm_v3 y_std[k]；global = train 全局
  y std（eval_config 记录）；OOD 误差禁 OOD 分母（调用方契约）。
"""
from __future__ import annotations

import numpy as np

from .quantile import TAUS, crossing_rate, pinball_numpy

# 分段 bins（非重叠；FR-574/E-Q2-①）
SEGMENTS = {"short": (1, 6), "mid": (7, 18), "long": (19, 30)}
R2_UNDEF_MIN_VAR = 1e-12
PICP_LEVEL = 0.80


# ------------------------------------------------------------ 原子/原语载荷

def _seg_slice(seg: str) -> slice:
    lo, hi = SEGMENTS[seg]
    return slice(lo - 1, hi)          # bins 1-based → 0-based


def window_atoms(y_true: np.ndarray, y_pred: np.ndarray,
                 e300_true: np.ndarray | None = None) -> dict:
    """逐窗原子载荷（sum/count 形式，支持严格层级重聚合）。

    y_true [N,30] kW；y_pred [N,30,3] = [q10,q50,q90]。
    返回 dict of [N] 数组（每窗）+ 全局窗口级 crossing：
    - pinball_sum, pinball_n（每窗 pinball 为 30×3 均值 → sum=mean*90）
    - abs_err_sum, abs_err_n（MAE 原子）
    - sq_err_sum, sq_err_n（RMSE 原子）
    - picp_hits, picp_n、mpiw_sum（MPIW 原子）
    - per-quantile pinball sum/n（3 个）
    """
    y = np.asarray(y_true, dtype=np.float64)
    q = np.asarray(y_pred, dtype=np.float64)
    if y.ndim != 2 or q.shape != y.shape + (3,):
        raise ValueError(f"形状不符: y {y.shape}, q {q.shape}")
    err = q[..., 1] - y                                # q50 误差
    q10, q90 = q[..., 0], q[..., 2]
    hits = ((y >= q10) & (y <= q90)).astype(np.float64)
    pb = pinball_numpy(y, q)                           # [N] 30×3 均值
    pb_per_tau = []
    for ti, tau in enumerate(TAUS):
        u = y - q[..., ti]
        rho = np.maximum(tau * u, (tau - 1.0) * u)
        pb_per_tau.append(rho.sum(axis=1))
    atoms = {
        "pinball_sum": pb * (y.shape[1] * len(TAUS)),
        "pinball_n": np.full(y.shape[0], y.shape[1] * len(TAUS), dtype=np.float64),
        "abs_err_sum": np.abs(err).sum(axis=1),
        "abs_err_n": np.full(y.shape[0], y.shape[1], dtype=np.float64),
        "sq_err_sum": (err ** 2).sum(axis=1),
        "sq_err_n": np.full(y.shape[0], y.shape[1], dtype=np.float64),
        "picp_hits": hits.sum(axis=1),
        "picp_n": np.full(y.shape[0], y.shape[1], dtype=np.float64),
        "mpiw_sum": (q90 - q10).sum(axis=1),
        "mpiw_n": np.full(y.shape[0], y.shape[1], dtype=np.float64),
        "pinball_q10_sum": pb_per_tau[0], "pinball_q50_sum": pb_per_tau[1],
        "pinball_q90_sum": pb_per_tau[2],
        "pinball_q_n": np.full(y.shape[0], y.shape[1], dtype=np.float64),
    }
    if e300_true is not None:
        e300_hat = q[..., 1].sum(axis=1) * (10.0 / 3600.0)
        e_err = e300_hat - np.asarray(e300_true, dtype=np.float64)
        atoms["e300_abs_sum"] = np.abs(e_err)
        atoms["e300_abs_n"] = np.ones(y.shape[0])
        atoms["e300_sq_sum"] = e_err ** 2
        atoms["e300_sq_n"] = np.ones(y.shape[0])
    return atoms


def _primitive(atoms: dict, keys: tuple, seg: slice | None = None) -> float:
    """voyage/池化层原语：sum(原子)/sum(计数) → 指标原语值。"""
    (skey, nkey, kind) = keys
    s = atoms[skey] if seg is None else atoms[skey][seg]
    n = atoms[nkey] if seg is None else atoms[nkey][seg]
    tot_s, tot_n = float(np.sum(s)), float(np.sum(n))
    if tot_n <= 0:
        return float("nan")
    if kind == "mean":
        return tot_s / tot_n
    if kind == "rmse":
        return float(np.sqrt(tot_s / tot_n))
    raise ValueError(kind)


_ATOM_KEYS = {
    "pinball": ("pinball_sum", "pinball_n", "mean"),
    "mae_q50": ("abs_err_sum", "abs_err_n", "mean"),
    "rmse_q50": ("sq_err_sum", "sq_err_n", "rmse"),
    "picp80": ("picp_hits", "picp_n", "mean"),
    "mpiw": ("mpiw_sum", "mpiw_n", "mean"),
    "pinball_q10": ("pinball_q10_sum", "pinball_q_n", "mean"),
    "pinball_q50": ("pinball_q50_sum", "pinball_q_n", "mean"),
    "pinball_q90": ("pinball_q90_sum", "pinball_q_n", "mean"),
}
if True:  # e300 键仅当原子含 e300 时启用
    _E300_KEYS = {"e300_mae": ("e300_abs_sum", "e300_abs_n", "mean"),
                  "e300_rmse": ("e300_sq_sum", "e300_sq_n", "rmse")}


def population_r2(e300_true: np.ndarray, e300_hat: np.ndarray) -> dict:
    """E300 population R²（全部合格对一次计算；禁平均 per-window/voyage R²）。"""
    t = np.asarray(e300_true, dtype=np.float64)
    p = np.asarray(e300_hat, dtype=np.float64)
    ok = np.isfinite(t) & np.isfinite(p)
    t, p = t[ok], p[ok]
    var = float(np.var(t))
    if var < R2_UNDEF_MIN_VAR:
        return {"status": "R2_UNDEFINED", "r2": None, "n": int(t.size)}
    ss_res = float(np.sum((t - p) ** 2))
    ss_tot = float(np.sum((t - t.mean()) ** 2))
    return {"status": "OK", "r2": 1.0 - ss_res / ss_tot, "n": int(t.size)}


# ------------------------------------------------------------ 层级聚合

def aggregate_views(atoms: dict, voyage_ids: np.ndarray,
                    split_group_of: dict) -> dict:
    """三视图聚合（R7 层级原语）。

    voyage_ids : [N] 每窗航次 id
    split_group_of : {voyage_id: split_group_id}
    返回 {"split_group_macro": {...}, "voyage_macro": {...}, "window_pooled": {...},
          "per_split_group": {g: {...}}, "crossing_rate": float}
    per_split_group 保留组级值（供 bootstrap 重采样单元）。
    """
    keys = dict(_ATOM_KEYS)
    if "e300_abs_sum" in atoms:
        keys.update(_E300_KEYS)

    # voyage 层原语
    uniq_v = list(dict.fromkeys(voyage_ids.tolist()))
    v_idx = {v: np.flatnonzero(voyage_ids == v) for v in uniq_v}
    voyage_prims = {k: {} for k in keys}
    for v, idx in v_idx.items():
        for k, spec in keys.items():
            sel = np.asarray(idx)
            s = atoms[spec[0]][sel]
            n = atoms[spec[1]][sel]
            tot_s, tot_n = float(s.sum()), float(n.sum())
            val = tot_s / tot_n if tot_n > 0 else float("nan")
            if spec[2] == "rmse":
                val = float(np.sqrt(tot_s / tot_n)) if tot_n > 0 else float("nan")
            voyage_prims[k][v] = val

    # split_group 层（组内 voyage 均值）
    groups: dict = {}
    for v in uniq_v:
        groups.setdefault(split_group_of[str(v)], []).append(v)
    group_vals = {k: {} for k in keys}
    for g, vs in groups.items():
        for k in keys:
            group_vals[k][g] = float(np.mean([voyage_prims[k][v] for v in vs]))

    sg_macro = {k: float(np.mean(list(group_vals[k].values())))
                for k in keys}
    voyage_macro = {k: float(np.mean([voyage_prims[k][v] for v in uniq_v]))
                    for k in keys}
    window_pooled = {k: _primitive(atoms, spec) for k, spec in keys.items()}

    return {"split_group_macro": sg_macro,
            "voyage_macro": voyage_macro,
            "window_pooled": window_pooled,
            "per_split_group": group_vals,
            "n_voyages": len(uniq_v),
            "n_split_groups": len(groups)}


def segment_atoms(y_true: np.ndarray, y_pred: np.ndarray,
                  e300_true: np.ndarray | None = None) -> dict:
    """分段原子：{seg: window_atoms(段内 bins)}（E300 不分段）。"""
    out = {}
    for seg, (lo, hi) in SEGMENTS.items():
        s = slice(lo - 1, hi)
        out[seg] = window_atoms(y_true[:, s], y_pred[:, s])
    return out


def full_report(y_true: np.ndarray, y_pred: np.ndarray,
                voyage_ids: np.ndarray, split_group_of: dict,
                e300_true: np.ndarray | None = None,
                y_std_per_lead: np.ndarray | None = None,
                y_std_global: float | None = None) -> dict:
    """一次产出 DGN-E-16(8) metrics_result 所需的全部指标载荷。"""
    atoms = window_atoms(y_true, y_pred, e300_true)
    views = aggregate_views(atoms, voyage_ids, split_group_of)
    seg_atoms = segment_atoms(y_true, y_pred)
    segments = {}
    for seg, sa in seg_atoms.items():
        keys = dict(_ATOM_KEYS)
        seg_view = {k: _primitive(sa, spec) for k, spec in keys.items()}
        segments[seg] = seg_view
    e300_hat = np.asarray(y_pred, dtype=np.float64)[..., 1].sum(axis=1) * (10.0 / 3600.0)
    r2 = population_r2(e300_true, e300_hat) if e300_true is not None else None
    report = {
        "views": views,
        "segments": segments,
        "crossing_rate": crossing_rate(y_pred),
        "n_windows": int(np.asarray(y_true).shape[0]),
        "e300_hat_definition": "Σ q50_k·(10/3600)（kWh，E300=ȳ/12 等价）",
    }
    if e300_true is not None:
        report["e300"] = {"mae": views["split_group_macro"]["e300_mae"],
                          "rmse": views["split_group_macro"]["e300_rmse"],
                          "r2": r2}
    if y_std_per_lead is not None and y_std_global is not None:
        # 归一化次要指标（分母仅 train 派生；OOD 调用方禁 OOD 分母，FR-598）。
        # pool 层口径（明确标注）；per-lead 分母 = formal_norm_v3 y_std[k]。
        err = np.asarray(y_pred, dtype=np.float64)[..., 1] - np.asarray(y_true, dtype=np.float64)
        denom = np.asarray(y_std_per_lead, dtype=np.float64)[None, :]
        report["normalized"] = {
            "nmae_q50_pooled_per_lead_denom": float(np.mean(np.abs(err) / denom)),
            "nmae_q50_pooled_global_denom": float(np.mean(np.abs(err)) / float(y_std_global)),
            "nrmse_q50_pooled_global_denom": float(np.sqrt(np.mean(err ** 2)) / float(y_std_global)),
            "denominator_note": "per-lead = formal_norm_v3 y_std[k]；global = train 全局 y std"
                                "（eval_config 记录值）；pool 层口径，非主推断视图",
            "y_std_global": float(y_std_global),
        }
        report["coverage_error_80"] = abs(
            views["split_group_macro"]["picp80"] - PICP_LEVEL)
    return report

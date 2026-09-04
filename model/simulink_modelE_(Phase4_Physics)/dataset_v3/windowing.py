#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""windowing.py — anchor 网格 / 窗口合法性 / K-F vintage 对齐（DGN-07，FR-469~473，T-009）.

冻结语义（唯一事实源：design.md DGN-07、requirements FR-469~473、任务 Fixed Decisions）：

- 窗口几何：
  * 历史 300 个 1 s bin H_j = (t0−j, t0−j+1]，j=300..1；X_H 行序 = 时间升序
    （行 0 = j=300 最老，行 299 = j=1 最新）；
  * 未来 30 个 10 s bin F_k = (t0+(k−1)·10, t0+k·10]，k=1..30；
  * anchor 网格 stride=10 s：t0 ∈ {t_start+300, t_start+300+stride, …, ≤ t_end−300}；
    跨度 t_end−t_start < 600 s → 0 个窗口（合法，报告计数）；
  * 窗口不得跨航次——本模块按单航次输入工作，天然满足（跨航次丢弃由 E2E 按航次分块保证）。
- X_KF vintage 对齐：取 kf_snapshots 中 issue_time_s == t0（容差 1e-9）的那一行，
  lead 升序 10..300 → k=1..30；无该行 → 窗口 invalid（计数返回，不抛异常，由调用方决定）。
- 输入约定（1 Hz history 时间基）：``t_1hz`` 为聚合后 1 s bin 的**右端点时刻**升序向量，
  即 hist[col][m] 为 bin (t_1hz[m]−1, t_1hz[m]] 的聚合值（右闭语义与 DGN-05 一致）；
  航次 [t_start, t_end] 由 t_start = t_1hz[0]−1、t_end = t_1hz[-1] 推得。
  ``hist`` 列长度必须与 t_1hz 一致，否则 ValueError。
"""

from __future__ import annotations

import numpy as np

from .aggregate import aggregate_bin  # noqa: F401  （DGN-05 聚合在窗口化上游完成）

# 窗口几何冻结常数（FR-469/471/472，design DGN-07）
HISTORY_BINS = 300          # 历史 1 s bin 数
FUTURE_BINS = 30            # 未来 10 s bin 数
HISTORY_BIN_S = 1.0         # 历史 bin 宽（秒）
FUTURE_BIN_S = 10.0         # 未来 bin 宽（秒）= anchor stride
ANCHOR_STRIDE_S = 10.0      # anchor 网格步距（秒）
HISTORY_SPAN_S = HISTORY_BINS * HISTORY_BIN_S   # 300 s
EXPECTED_LEADS_S = FUTURE_BIN_S * np.arange(1, FUTURE_BINS + 1)  # 10..300 升序

# vintage 判等容差（秒；任务冻结 1e-9）
_VINTAGE_TOL_S = 1e-9


def anchor_grid(t_start_s, t_end_s, stride_s=ANCHOR_STRIDE_S) -> np.ndarray:
    """anchor 网格：t0 ∈ {t_start+300, …, ≤ t_end−300}，步距 stride_s。

    跨度 < 600 s → 返回空数组（合法，FR-472）。浮点安全：端点判等带 1e-9 容差，
    保证最后 anchor ≤ t_end−300（容差意义下）。
    """
    t_start_s = float(t_start_s)
    t_end_s = float(t_end_s)
    stride_s = float(stride_s)
    if stride_s <= 0:
        raise ValueError(f"stride_s 必须为正，实际 {stride_s}")
    first = t_start_s + HISTORY_SPAN_S
    last = t_end_s - HISTORY_SPAN_S
    if last < first - _VINTAGE_TOL_S:
        return np.array([], dtype=float)
    n = int(np.floor((last - first) / stride_s + _VINTAGE_TOL_S)) + 1
    anchors = first + stride_s * np.arange(n, dtype=float)
    # 防御：浮点余量导致末位越界时收缩（不应触发）
    while anchors.size and anchors[-1] > last + _VINTAGE_TOL_S:
        anchors = anchors[:-1]
    return anchors


def _history_slice(t_1hz: np.ndarray, t0: float) -> slice:
    """定位覆盖 (t0−300, t0] 的 300 个 1 s bin（右端点时间基）。

    右闭语义：右端点 == t0−300 的 bin 属于左侧相邻窗口，须排除（side='right' 语义，
    带 1e-9 容差）。切片必须恰好 300 个 bin 且末位右端点 == t0，否则几何损坏 → ValueError。
    """
    start_edge = t0 - HISTORY_SPAN_S
    i0 = int(np.searchsorted(t_1hz, start_edge + _VINTAGE_TOL_S, side="left"))
    i1 = int(np.searchsorted(t_1hz, t0 + _VINTAGE_TOL_S, side="right"))  # 含末位
    if i1 - i0 != HISTORY_BINS:
        raise ValueError(
            f"窗口 t0={t0} 历史 bin 数为 {i1 - i0}（要求 {HISTORY_BINS}）；"
            f"检查 t_1hz 网格连续性（1 s bin 右端点时间基约定）")
    if abs(t_1hz[i1 - 1] - t0) > _VINTAGE_TOL_S:
        raise ValueError(f"窗口 t0={t0} 历史末 bin 右端点 {t_1hz[i1 - 1]} != t0（网格错位）")
    return slice(i0, i1)


def _kf_row(kf: dict, t0: float):
    """vintage 对齐：返回 issue_time_s == t0（容差 1e-9）的行号；无则 None。

    lead 行向量若为乱序排列但取值集合恰为 10..300，按升序重排后返回列序；
    取值集合不符 → ValueError（契约违例 fail-fast）。
    """
    issue = np.asarray(kf["issue_time_s"], dtype=float).ravel()
    hits = np.nonzero(np.abs(issue - t0) <= _VINTAGE_TOL_S)[0]
    if hits.size == 0:
        return None
    if hits.size > 1:
        raise ValueError(f"issue_time_s 含重复 vintage 行 t0={t0}（{hits.size} 行），拒绝歧义消费")
    row = int(hits[0])
    leads = np.asarray(kf["lead_s"], dtype=float)[row]
    order = np.argsort(leads, kind="stable")
    if not np.allclose(leads[order], EXPECTED_LEADS_S, rtol=0.0, atol=_VINTAGE_TOL_S):
        raise ValueError(f"lead_s 集合与冻结网格 {EXPECTED_LEADS_S.tolist()} 不符（t0={t0} 行）")
    return row, order


def assemble_single(t0, t_1hz: np.ndarray, hist: dict, kf: dict | None) -> dict:
    """组装单窗口（FR-469~473）。

    Returns
    -------
    {"t0": float,
     "X_H": {col: (300,) float 数组}，行序时间升序（行 0 = j=300 最老），
     "X_KF": {col: (30,) 数组}（vintage 命中，lead 升序 k=1..30）或 None（无命中/无 kf 输入）}
    """
    t0 = float(t0)
    t_1hz = np.asarray(t_1hz, dtype=float)
    sl = _history_slice(t_1hz, t0)
    x_h = {name: np.asarray(col, dtype=float)[sl] for name, col in hist.items()}

    x_kf = None
    if kf is not None:
        hit = _kf_row(kf, t0)
        if hit is not None:
            row, order = hit
            x_kf = {}
            for name, mat in kf.items():
                if name in ("issue_time_s", "lead_s"):
                    continue
                mat = np.asarray(mat)
                if mat.shape[0] != np.asarray(kf["issue_time_s"]).size:
                    raise ValueError(f"kf 列 {name} 行数与 issue_time_s 不一致")
                x_kf[name] = np.asarray(mat[row, order], dtype=float)
    return {"t0": t0, "X_H": x_h, "X_KF": x_kf}


def build_windows(t_1hz: np.ndarray, hist: dict, kf: dict | None,
                  stride_s: float = ANCHOR_STRIDE_S) -> dict:
    """整航次窗口化（单航次输入 → 不跨航次，FR-472 天然满足）。

    Parameters
    ----------
    t_1hz : (M,) 1 s 聚合 bin 右端点时刻（升序）；t_start = t_1hz[0]−1，t_end = t_1hz[-1]
    hist : {col: (M,) 聚合后 1 Hz 列}（列序/白名单由调用方经 registry 决定）
    kf : kf_snapshots 结构（issue_time_s (M,)、lead_s (M,30)、各 K/F 列 (M,30)）或 None
    stride_s : anchor 步距（冻结 10 s）

    Returns
    -------
    {"anchors": (A,) 网格 anchor（含 K/F invalid 窗口）,
     "window_index": (A,) int——voyage 内 anchor 序号（0-based，FR-473 metadata）,
     "X_H": {col: (A,300)}——列键 = hist 键（调用方 registry 列序）,
     "X_KF": {col: (A,30)} 或 {}（kf=None 时）；vintage 缺失窗口对应行填 NaN,
     "n_invalid_kf": int——vintage 缺失窗口计数（DGN-07：不抛异常，由调用方决定处置）}
    """
    t_1hz = np.asarray(t_1hz, dtype=float)
    if t_1hz.ndim != 1 or t_1hz.size < 2 or not np.all(np.diff(t_1hz) > 0):
        raise ValueError("t_1hz 必须为长度 ≥2 的严格升序一维数组")
    for name, col in hist.items():
        if np.asarray(col).shape != t_1hz.shape:
            raise ValueError(
                f"hist 列 {name} 形状 {np.asarray(col).shape} 与 t_1hz {t_1hz.shape} 不一致"
                "（约定：t_1hz 为 1 s bin 右端点时间基）")

    t_start = float(t_1hz[0]) - HISTORY_BIN_S
    t_end = float(t_1hz[-1])
    anchors = anchor_grid(t_start, t_end, stride_s)
    n = anchors.size

    kf_cols = []
    if kf is not None:
        kf_cols = [k for k in kf.keys() if k not in ("issue_time_s", "lead_s")]

    x_h = {name: np.empty((n, HISTORY_BINS), dtype=float) for name in hist}
    x_kf = {name: np.full((n, FUTURE_BINS), np.nan, dtype=float) for name in kf_cols}
    n_invalid = 0

    for i, t0 in enumerate(anchors):
        win = assemble_single(float(t0), t_1hz, hist, kf)
        for name in hist:
            x_h[name][i, :] = win["X_H"][name]
        if win["X_KF"] is None:
            n_invalid += 1  # vintage 缺失：窗口 invalid，行保持 NaN，计数上报
        else:
            for name in kf_cols:
                x_kf[name][i, :] = win["X_KF"][name]

    return {
        "anchors": anchors,
        "window_index": np.arange(n, dtype=int),
        "X_H": x_h,
        "X_KF": x_kf,
        "n_invalid_kf": n_invalid,
    }


__all__ = ["anchor_grid", "build_windows", "assemble_single",
           "HISTORY_BINS", "FUTURE_BINS", "ANCHOR_STRIDE_S"]

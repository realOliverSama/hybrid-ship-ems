#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""targets.py — native-rate 能量差 target + E300（DGN-08，FR-476/477，T-010）.

冻结语义（唯一事实源：requirements FR-476/477、design DGN-08 + HANDOFF §5 权威口径）：

- 对 native (t, P_load_total_kW) 做累计梯形能量 E(τ)=∫ P dt（kW·s，E(voyage_start)=0）；
- bin 能量 E_k = E(t0+10k) − E(t0+10(k−1))；**y_k = E_k / 10 [kW]**（10 s 区间能量等价
  平均功率）。注意：design.md DGN-08 原文 "y_k = E_k·3600/10" 为笔误，以 requirements
  FR-476 与 HANDOFF §5 为准 y_k = E_k/10；
- E300 = ΣE_k/3600 [kWh] = Σ(y_k·10)/3600（两者恒等，e300_of 实现后者）；
- **targets.py 只消费 native stream**（代码层隔离：本模块接口不接受 1 Hz 降采样流，
  禁止从 1 Hz 流反算 target，DGN-08）；
- 交叉验证不变量（单测覆盖）：
  * Σ_k E_k ≡ E(t0+300)−E(t0)（机器精度，相对 <1e-12）；
  * y 轨迹 vs time_mean 聚合路径相对差 <1e-9（crosscheck_vs_aggregate）。

quantile 契约（FR-478/479）：数据集不物化 target_q10/q50/q90 标签；τ={0.10,0.50,0.90}
pinball 学习语义与 q10≤q50≤q90 不变量属 Phase 4-E 包（见 build_dataset_v3.assert_no_quantile_labels）。
"""

from __future__ import annotations

import numpy as np

from .aggregate import aggregate_bin

FUTURE_BINS = 30
FUTURE_BIN_S = 10.0
SECONDS_PER_HOUR = 3600.0

# 能量评估判等容差（秒），与 windowing/aggregate 一致
_TOL_S = 1e-9


def cumulative_energy(t: np.ndarray, p: np.ndarray) -> np.ndarray:
    """累计梯形能量 E(t) = ∫ P dt（kW·s）；E[0]=0，E[i]=E[i−1]+0.5(P[i−1]+P[i])Δt。"""
    t = np.asarray(t, dtype=float)
    p = np.asarray(p, dtype=float)
    if t.ndim != 1 or p.shape != t.shape:
        raise ValueError("t/p 必须为同形状一维数组")
    if t.size < 2 or not np.all(np.diff(t) > 0):
        raise ValueError("t 必须为长度 ≥2 的严格升序数组")
    seg = 0.5 * (p[1:] + p[:-1]) * np.diff(t)
    return np.concatenate((np.array([0.0]), np.cumsum(seg)))


def _energy_at(t: np.ndarray, E: np.ndarray, p: np.ndarray, tau: float) -> float:
    """梯形原函数在任意 τ 处的值（τ 落于样本间时取部分段梯形，P(τ) 线性内插）。

    一致性保证：Σ_k [E_at(τ_k) − E_at(τ_{k−1})] ≡ E_at(τ_n) − E_at(τ_0)
    （同一反导数，遥测可加，机器精度 <1e-12 相对）。
    """
    j = int(np.searchsorted(t, tau, side="left"))
    if j < t.size and abs(t[j] - tau) <= _TOL_S:
        return float(E[j])
    if j == 0 or j >= t.size:
        raise ValueError(f"能量评估时刻 {tau} 超出 native 数据覆盖范围 "
                         f"[{t[0]}, {t[-1]}]")
    v_tau = p[j - 1] + (p[j] - p[j - 1]) * (tau - t[j - 1]) / (t[j] - t[j - 1])
    return float(E[j - 1] + 0.5 * (p[j - 1] + v_tau) * (tau - t[j - 1]))


def y_trajectory(t, p, t0, n_bins=FUTURE_BINS, bin_s=FUTURE_BIN_S) -> np.ndarray:
    """30 个 10 s bin 的能量等价平均功率轨迹 y_k = E_k/bin_s [kW]（FR-476）。

    只接受 native-rate (t, p)（P_load_total_kW，dc_bus 域）；bin 边界
    t0+k·bin_s 须落在 native 数据覆盖范围内，越界 ValueError（禁静默外推）。
    """
    t = np.asarray(t, dtype=float)
    p = np.asarray(p, dtype=float)
    E = cumulative_energy(t, p)
    t0 = float(t0)
    edges = t0 + bin_s * np.arange(n_bins + 1, dtype=float)
    if edges[0] < t[0] - _TOL_S or edges[-1] > t[-1] + _TOL_S:
        raise ValueError(f"target 窗 [{edges[0]}, {edges[-1]}] 超出 native 覆盖 "
                         f"[{t[0]}, {t[-1]}]")
    E_edges = np.array([_energy_at(t, E, p, float(e)) for e in edges])
    return (E_edges[1:] - E_edges[:-1]) / bin_s


def e300_of(y: np.ndarray) -> float:
    """E300 = Σ(y_k·10)/3600 [kWh]（FR-477 mandatory derived scalar）。

    与 ΣE_k/3600 恒等（y_k·bin_s = E_k）。
    """
    y = np.asarray(y, dtype=float)
    if y.ndim != 1 or y.size == 0:
        raise ValueError("y 必须为非空一维数组")
    return float(np.sum(y) * FUTURE_BIN_S / SECONDS_PER_HOUR)


def crosscheck_vs_aggregate(t, p, t0, edges) -> float:
    """能量差分路径 vs time_mean 聚合路径的相对差（DGN-08 交叉验证，阈值 1e-9）。

    Parameters
    ----------
    t, p : native-rate 流（P_load_total_kW）
    t0 : anchor
    edges : (K+1,) 未来 bin 边界（t0 … t0+K·bin_s，升序）

    Returns
    -------
    max_k |y_k − P̄_k| / max_i |y_i|（y 全零时退化为绝对差）
    """
    t = np.asarray(t, dtype=float)
    p = np.asarray(p, dtype=float)
    edges = np.asarray(edges, dtype=float)
    if edges.ndim != 1 or edges.size < 2 or not np.all(np.diff(edges) > 0):
        raise ValueError("edges 必须为长度 ≥2 的严格升序一维数组")

    E = cumulative_energy(t, p)
    e_lo = np.array([_energy_at(t, E, p, float(e)) for e in edges[:-1]])
    e_hi = np.array([_energy_at(t, E, p, float(e)) for e in edges[1:]])
    dt = np.diff(edges)
    y = (e_hi - e_lo) / dt

    agg = np.array([aggregate_bin(t, p, float(edges[i]), float(edges[i + 1]), "time_mean")
                    for i in range(edges.size - 1)])

    scale = float(np.max(np.abs(y))) if np.max(np.abs(y)) > 0.0 else 1.0
    return float(np.max(np.abs(y - agg)) / scale)


__all__ = ["cumulative_energy", "y_trajectory", "e300_of", "crosscheck_vs_aggregate"]

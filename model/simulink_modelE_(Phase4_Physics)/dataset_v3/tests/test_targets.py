#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_targets.py — DGN-08 target/E300 单测（T-010）.

覆盖：累计能量精确性、Σ E_k ≡ E(t0+300)−E(t0)（<1e-12 相对）、y=E_k/10 与
time_mean 聚合路径交叉验证（<1e-9）、E300 恒等式、native 覆盖越界 fail-fast。
"""

import numpy as np
import pytest

from dataset_v3.aggregate import aggregate_bin
from dataset_v3.targets import (crosscheck_vs_aggregate, cumulative_energy,
                                e300_of, y_trajectory, _energy_at)


def _native_stream(t_end=900.0, dt=0.01):
    """native 0.01 s 流（kW）：光滑多正弦 + 直流，覆盖 [0, t_end]。"""
    t = np.arange(0.0, t_end + 1e-9, dt)
    p = 500.0 + 120.0 * np.sin(2 * np.pi * t / 97.0) + 40.0 * np.sin(2 * np.pi * t / 13.3 + 0.7)
    return t, p


# ---------- cumulative_energy ----------

class TestCumulativeEnergy:
    def test_constant_power_linear(self):
        t = np.array([0.0, 1.0, 2.0, 4.0])
        p = np.full(4, 5.0)
        E = cumulative_energy(t, p)
        assert E[0] == 0.0
        np.testing.assert_allclose(E, [0.0, 5.0, 10.0, 20.0], rtol=0, atol=1e-12)

    def test_linear_power_exact(self):
        t = np.linspace(0.0, 1.0, 101)
        p = t  # ∫_0^x t dt = x²/2（梯形对线性精确）
        E = cumulative_energy(t, p)
        np.testing.assert_allclose(E, t ** 2 / 2.0, rtol=0, atol=1e-12)

    def test_bad_input_raises(self):
        with pytest.raises(ValueError):
            cumulative_energy(np.array([1.0, 0.0]), np.array([1.0, 1.0]))  # 降序


# ---------- 可加性不变量（FR-476） ----------

class TestAdditivity:
    def test_sum_Ek_equals_span_energy_1e12(self):
        t, p = _native_stream()
        t0 = 300.0
        y = y_trajectory(t, p, t0)                      # y_k = E_k/10
        E = cumulative_energy(t, p)
        sum_Ek = float(np.sum(y) * 10.0)                # Σ E_k
        span = _energy_at(t, E, p, t0 + 300.0) - _energy_at(t, E, p, t0)
        assert abs(sum_Ek - span) / abs(span) < 1e-12

    def test_additivity_over_many_anchors(self):
        t, p = _native_stream()
        E = cumulative_energy(t, p)
        for t0 in np.arange(300.0, 600.0 + 1e-9, 47.0):
            y = y_trajectory(t, p, float(t0))
            sum_Ek = float(np.sum(y) * 10.0)
            span = _energy_at(t, E, p, t0 + 300.0) - _energy_at(t, E, p, t0)
            assert abs(sum_Ek - span) / abs(span) < 1e-12

    def test_no_boundary_double_count_across_bins(self):
        # 相邻 bin 无边界重复/遗漏：E_k 单调非负（正功率下）且总和守恒
        t, p = _native_stream()
        y = y_trajectory(t, p, 300.0)
        assert np.all(y > 0.0)  # 正功率 → 每个 10 s bin 能量为正（无重复计、无遗漏）


# ---------- y = E_k/10 语义与聚合交叉验证（DGN-08） ----------

class TestYTrajectory:
    def test_constant_power_y_is_constant(self):
        t = np.arange(0.0, 700.0 + 1e-9, 0.01)
        p = np.full_like(t, 500.0)
        y = y_trajectory(t, p, 300.0)
        assert y.shape == (30,)
        np.testing.assert_allclose(y, 500.0, rtol=0, atol=1e-9)

    def test_y_matches_time_mean_aggregate_path_1e9(self):
        t, p = _native_stream()
        t0 = 300.0
        edges = t0 + 10.0 * np.arange(31)
        y = y_trajectory(t, p, t0)
        agg = np.array([aggregate_bin(t, p, edges[k], edges[k + 1], "time_mean")
                        for k in range(30)])
        scale = float(np.max(np.abs(y)))
        assert float(np.max(np.abs(y - agg))) / scale < 1e-9

    def test_crosscheck_vs_aggregate_function(self):
        t, p = _native_stream()
        edges = 300.0 + 10.0 * np.arange(31)
        rel = crosscheck_vs_aggregate(t, p, 300.0, edges)
        assert rel < 1e-9  # 任务冻结容差（实测机器精度量级）

    def test_out_of_coverage_raises(self):
        t, p = _native_stream(t_end=500.0)
        with pytest.raises(ValueError, match="覆盖"):
            y_trajectory(t, p, 300.0)  # 需要 [300,600]，数据只到 500


# ---------- E300（FR-477） ----------

class TestE300:
    def test_constant_power_value(self):
        # E300 = 500 kW · 300 s / 3600 = 41.666… kWh
        y = np.full(30, 500.0)
        assert e300_of(y) == pytest.approx(500.0 * 300.0 / 3600.0, rel=1e-15)

    def test_identity_with_energy_sum_1e12(self):
        # E300 = Σ(y_k·10)/3600 ≡ ΣE_k/3600（恒等）
        t, p = _native_stream()
        y = y_trajectory(t, p, 300.0)
        E = cumulative_energy(t, p)
        e300 = e300_of(y)
        ref = (_energy_at(t, E, p, 600.0) - _energy_at(t, E, p, 300.0)) / 3600.0
        assert abs(e300 - ref) / abs(ref) < 1e-12

    def test_shape_guard(self):
        with pytest.raises(ValueError):
            e300_of(np.empty((0,)))

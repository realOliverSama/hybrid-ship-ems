#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_aggregate.py — DGN-05/06 聚合语义单测（T-008）.

覆盖：bin 右闭 off-by-one、time_mean 恒定/线性精确、能量守恒 1e-9、
interval_endpoint/last_mode/interval_any、aggregate_stream、zero-fill 双通道、
非法规则/边界 fail-fast。
"""

import numpy as np
import pytest

from dataset_v3.aggregate import (aggregate_bin, aggregate_stream,
                                  time_mean_energy_check, zero_fill_ctrl_channels)


# ---------- bin 右闭 (a,b] off-by-one ----------

class TestBinRightClosed:
    T = np.array([1.0, 2.0, 3.0, 4.0])
    # 阶跃：t ≤ 2 → 0，t ≥ 3 → 1（样本 t=2 处 v=0）
    V_STEP = np.array([0.0, 0.0, 1.0, 1.0])

    def test_endpoint_sample_at_a_excluded(self):
        # 样本 t=2 恰在左边界 a=2 上：属左侧 bin (1,2]，不属 (2,3]
        assert aggregate_bin(self.T, self.V_STEP, 1.0, 2.0, "interval_endpoint") == 0.0
        assert aggregate_bin(self.T, self.V_STEP, 2.0, 3.0, "interval_endpoint") == 1.0

    def test_endpoint_sample_at_b_included(self):
        # 样本 t=3 恰在右边界 b=3 上：属 (2,3]
        assert aggregate_bin(self.T, self.V_STEP, 2.0, 3.0, "interval_endpoint") == 1.0

    def test_last_mode_same_math_different_semantics(self):
        modes = np.array([0.0, 1.0, 1.0, 2.0])
        assert aggregate_bin(self.T, modes, 2.0, 4.0, "last_mode") == 2.0
        assert aggregate_bin(self.T, modes, 0.0, 2.0, "last_mode") == 1.0

    def test_interval_any_right_closed(self):
        flags = np.array([0.0, 0.0, 1.0, 0.0])
        assert aggregate_bin(self.T, flags, 0.0, 2.0, "interval_any") is False
        assert aggregate_bin(self.T, flags, 2.0, 3.0, "interval_any") is True
        assert aggregate_bin(self.T, flags, 3.0, 4.0, "interval_any") is False

    def test_time_mean_boundaries_are_integration_nodes(self):
        # time_mean 的积分节点含两端边界（v(a) 内插、v(b) 直取）——线性 v=t 时
        # bin (2,3] 均值 = (2+3)/2，精确
        t = np.arange(0.0, 11.0)
        v = t.copy()
        assert aggregate_bin(t, v, 2.0, 3.0, "time_mean") == pytest.approx(2.5, abs=1e-14)


# ---------- time_mean 精度 ----------

class TestTimeMean:
    def test_constant_value_exact(self):
        t = np.arange(0.0, 10.001, 0.01)
        v = np.full_like(t, 7.0)
        assert aggregate_bin(t, v, 2.0, 3.0, "time_mean") == 7.0

    def test_linear_exact(self):
        t = np.arange(0.0, 10.001, 0.01)
        v = 3.0 * t + 1.0
        # ∫_2^3 (3t+1) dt = [1.5t²+t]_2^3 = 16.5 − 8 = 8.5
        assert aggregate_bin(t, v, 2.0, 3.0, "time_mean") == pytest.approx(8.5, abs=1e-12)

    def test_empty_bin_interpolated_endpoints(self):
        # bin 内无样本：两端点线性内插的梯形。v(2)=1+1/4=1.25，v(3)=1.5 → 均值 1.375
        t = np.array([0.0, 1.0, 5.0, 6.0])
        v = np.array([1.0, 1.0, 2.0, 2.0])
        assert aggregate_bin(t, v, 2.0, 3.0, "time_mean") == pytest.approx(1.375, abs=1e-12)

    def test_naive_mean_would_differ(self):
        # 禁止 mean(samples)：25% 占空方波，naive 算术均值随占空相位取 2.475/2.574
        # （= 样本数占比），trapezoidal 时间加权恒为 2.5（能量一致、与相位无关）
        t = np.arange(0.0, 1.0001, 0.01)
        v = np.where((np.arange(t.size) % 4) == 0, 10.0, 0.0)  # 高电平在 bin 头部
        got = aggregate_bin(t, v, 0.0, 1.0, "time_mean")
        naive = float(np.mean(v))
        assert abs(got - naive) > 1e-3  # 两种路径可区分
        v2 = np.where((np.arange(t.size) % 4) == 3, 10.0, 0.0)  # 高电平贴近 bin 尾部
        got2 = aggregate_bin(t, v2, 0.0, 1.0, "time_mean")
        naive2 = float(np.mean(v2))
        # naive decimation（每 bin 抽单样本）会随抽取相位在 0/10 间跳变；
        # time_mean 对两个占空相位给出同一能量一致值 2.5
        assert got2 == pytest.approx(got, abs=1e-12)
        assert abs(got2 - naive2) > 1e-3

    def test_energy_conservation_within_1e9(self):
        # DGN-05 能量语义自检：Σ P̄_k·Δt ≈ ∫P dt（0.01 s native，1 s bin）
        t = np.arange(0.0, 600.0 + 1e-9, 0.01)
        p = 100.0 + 50.0 * np.sin(2 * np.pi * t / 97.0) + 20.0 * np.sin(2 * np.pi * t / 13.0 + 1.0)
        edges = np.arange(0.0, 601.0, 1.0)
        assert time_mean_energy_check(t, p, edges) < 1e-9

    def test_energy_conservation_global_additivity(self):
        # 全局可加性（跨 bin 无重复/遗漏，含恰落在边界上的尖峰样本）
        t = np.arange(0.0, 20.0 + 1e-9, 0.01)
        p = np.ones_like(t)
        p[t == 7.0] += 100.0  # 恰在 bin 边界 t=7 上的尖峰样本
        p += 5.0 * np.sin(2 * np.pi * t / 3.1)
        edges = np.arange(0.0, 21.0, 1.0)
        bars = np.array([aggregate_bin(t, p, edges[i], edges[i + 1], "time_mean")
                         for i in range(edges.size - 1)])
        total = float(np.sum(bars * np.diff(edges)))
        ref = float(np.trapezoid(p, t))
        assert abs(total - ref) / abs(ref) < 1e-12


# ---------- aggregate_stream ----------

class TestAggregateStream:
    def test_multi_column_multi_rule(self):
        t = np.arange(0.0, 11.0)
        cols = {"P": 100.0 + t, "SOC": 0.5 + 0.01 * t, "mode": np.array([0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0], dtype=float),
                "flag": np.array([0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0], dtype=float)}
        edges = np.arange(0.0, 11.0, 1.0)
        agg_map = {"P": "time_mean", "SOC": "interval_endpoint", "mode": "last_mode",
                   "flag": "interval_any"}
        out = aggregate_stream(t, cols, edges, agg_map)
        assert set(out) == set(agg_map)
        for name in out:
            assert out[name].shape == (edges.size - 1,)
        assert out["P"][2] == pytest.approx(102.5, abs=1e-12)  # 整数采样：∫_2^3 P dt=(102+103)/2
        assert out["SOC"][2] == pytest.approx(0.53, abs=1e-12)  # 右端点 t=3
        assert out["mode"][3] == 2.0
        assert out["flag"].dtype == bool          # interval_any 列为 bool dtype
        assert bool(out["flag"][3]) and not out["flag"][4]  # t=4 ∈ (3,4]
        # 与逐 bin aggregate_bin 一致
        for i in range(edges.size - 1):
            assert out["P"][i] == aggregate_bin(t, cols["P"], edges[i], edges[i + 1], "time_mean")

    def test_missing_column_raises(self):
        t = np.arange(0.0, 5.0)
        with pytest.raises(KeyError):
            aggregate_stream(t, {"A": t}, np.arange(0.0, 5.0, 1.0), {"B": "time_mean"})

    def test_bad_edges_raise(self):
        with pytest.raises(ValueError):
            aggregate_stream(np.arange(5.0), {"A": np.arange(5.0)},
                             np.array([3.0, 1.0]), {"A": "time_mean"})


# ---------- zero-fill（DGN-06） ----------

class TestZeroFill:
    def _hist(self):
        return {
            "ctrl_mode": np.array([0, 0, 1, 1, 0]),
            "v_ref_mps": np.array([3.0, 3.0, 4.0, 4.5, 2.0]),
            "manual_power_cmd_pct": np.array([50.0, 60.0, 70.0, 80.0, 10.0]),
        }

    def test_auto_zeroes_manual_manual_zeroes_vref(self):
        out = zero_fill_ctrl_channels(self._hist())
        mode = out["ctrl_mode"]
        assert np.all(out["manual_power_cmd_pct"][mode == 0] == 0.0)   # AUTO → manual 置 0
        assert np.all(out["v_ref_mps"][mode == 0] != 0.0)              # AUTO → v_ref 保留
        assert np.all(out["v_ref_mps"][mode == 1] == 0.0)              # MANUAL → v_ref 置 0
        assert np.all(out["manual_power_cmd_pct"][mode == 1] != 0.0)   # MANUAL → manual 保留
        # 逐元素精确核对
        assert out["v_ref_mps"].tolist() == [3.0, 3.0, 0.0, 0.0, 2.0]
        assert out["manual_power_cmd_pct"].tolist() == [0.0, 0.0, 70.0, 80.0, 0.0]

    def test_no_nan_deterministic(self):
        out = zero_fill_ctrl_channels(self._hist())
        for k in out:
            assert not np.any(np.isnan(np.asarray(out[k], dtype=float)))

    def test_input_not_mutated(self):
        h = self._hist()
        snap = {k: v.copy() for k, v in h.items()}
        zero_fill_ctrl_channels(h)
        for k in h:
            assert np.array_equal(h[k], snap[k])

    def test_bad_ctrl_mode_value_raises(self):
        h = self._hist()
        h["ctrl_mode"] = np.array([0, 1, 2, 0, 1])
        with pytest.raises(ValueError, match="ctrl_mode"):
            zero_fill_ctrl_channels(h)

    def test_missing_key_raises(self):
        h = self._hist()
        del h["v_ref_mps"]
        with pytest.raises(KeyError):
            zero_fill_ctrl_channels(h)


# ---------- fail-fast ----------

class TestFailFast:
    T = np.arange(0.0, 10.001, 0.01)

    def test_unknown_rule_raises(self):
        with pytest.raises(ValueError, match="规则非法"):
            aggregate_bin(self.T, self.T, 2.0, 3.0, "interval_mean")  # design R1 前旧名，须拒收

    def test_empty_bin_raises_for_endpoint(self):
        t = np.array([0.0, 1.0, 5.0])
        with pytest.raises(ValueError, match="无样本"):
            aggregate_bin(t, t, 2.0, 3.0, "interval_endpoint")

    def test_empty_bin_any_is_false(self):
        t = np.array([0.0, 1.0, 5.0])
        assert aggregate_bin(t, t, 2.0, 3.0, "interval_any") is False

    def test_bin_outside_coverage_raises(self):
        with pytest.raises(ValueError, match="覆盖"):
            aggregate_bin(self.T, self.T, 9.0, 11.0, "time_mean")

    def test_reversed_bin_raises(self):
        with pytest.raises(ValueError, match="b > a"):
            aggregate_bin(self.T, self.T, 3.0, 2.0, "time_mean")

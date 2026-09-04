#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_windowing.py — DGN-07 窗口化单测（T-009）.

覆盖：anchor 网格边界（<600 s 零窗、stride、末 anchor ≤ t_end−300）、
X_H 行序时间升序（右闭 bin 选择 off-by-one）、vintage 精确对齐（±1e-9 容差、
缺失 → invalid 计数）、lead 排序/契约校验、单窗口组装、输入形状 fail-fast。
"""

import numpy as np
import pytest

from dataset_v3.windowing import (anchor_grid, assemble_single, build_windows)


def _hist_stream(t_end=900.0):
    """1 Hz 聚合流：bin 右端点 1..t_end；col A=右端点值（行序可读），B=2×右端点。"""
    t_1hz = np.arange(1.0, t_end + 1e-9, 1.0)
    hist = {"A": t_1hz.copy(), "B": 2.0 * t_1hz}
    return t_1hz, hist


def _kf(issue_times, lead_val_fn=None):
    leads = np.tile(10.0 * np.arange(1, 31), (len(issue_times), 1))
    cols = {"v_plan_mps": np.empty((len(issue_times), 30))}
    for i, it in enumerate(issue_times):
        if lead_val_fn is not None:
            perm = np.random.default_rng(i).permutation(30)
            leads[i, :] = leads[i, perm]
            cols["v_plan_mps"][i, :] = lead_val_fn(i)[perm]  # 列值随 lead 同步置换
        else:
            cols["v_plan_mps"][i, :] = i + np.arange(30) / 100.0  # 值按 k=1..30 单调
    return {"issue_time_s": np.asarray(issue_times, dtype=float),
            "lead_s": leads, **cols}


# ---------- anchor 网格 ----------

class TestAnchorGrid:
    def test_exact_600s_single_anchor(self):
        np.testing.assert_array_equal(anchor_grid(0.0, 600.0), [300.0])

    def test_stride(self):
        np.testing.assert_allclose(anchor_grid(0.0, 620.0), [300.0, 310.0, 320.0])

    def test_partial_stride_no_exceed(self):
        # t_end−300 = 317 → 最后 anchor 310 ≤ 317，320 被排除
        np.testing.assert_allclose(anchor_grid(0.0, 617.0), [300.0, 310.0])

    def test_shorter_than_600s_zero_windows(self):
        assert anchor_grid(0.0, 599.9).size == 0
        assert anchor_grid(0.0, 100.0).size == 0

    def test_offset_start(self):
        np.testing.assert_allclose(anchor_grid(1000.0, 1620.0), [1300.0, 1310.0, 1320.0])

    def test_custom_stride(self):
        # stride=20：t0 ∈ {300, 320, 340}，末 anchor 340 ≤ t_end−300 = 340（含端点）
        np.testing.assert_allclose(anchor_grid(0.0, 640.0, stride_s=20.0), [300.0, 320.0, 340.0])


# ---------- X_H 几何与行序 ----------

class TestHistoryWindow:
    def test_row_order_time_ascending_right_closed(self):
        t_1hz, hist = _hist_stream()
        res = build_windows(t_1hz, hist, None)
        assert res["n_invalid_kf"] == res["anchors"].size  # kf=None → 全部 invalid 计数
        anchors = res["anchors"]
        np.testing.assert_allclose(anchors, np.arange(300.0, 600.0 + 1e-9, 10.0))
        A = res["X_H"]["A"]
        assert A.shape == (anchors.size, 300)
        for i, t0 in enumerate(anchors):
            # 行 0 = 最老 bin (t0−300, t0−299] 的聚合值 = 其右端点 t0−299；
            # 右端点 == t0−300 的 bin (t0−301, t0−300] 被排除（右闭 off-by-one）
            assert A[i, 0] == pytest.approx(t0 - 299.0, abs=1e-9)
            assert A[i, 299] == pytest.approx(t0, abs=1e-9)  # 行 299 = 最新 bin (t0−1, t0]
            assert np.all(np.diff(A[i, :]) > 0)  # 时间升序
            np.testing.assert_allclose(res["X_H"]["B"][i], 2.0 * A[i])

    def test_window_index_sequential(self):
        t_1hz, hist = _hist_stream()
        res = build_windows(t_1hz, hist, None)
        np.testing.assert_array_equal(res["window_index"], np.arange(res["anchors"].size))

    def test_short_voyage_zero_windows(self):
        t_1hz = np.arange(1.0, 500.0 + 1e-9, 1.0)  # 航次 [0, 500] < 600 s
        res = build_windows(t_1hz, {"A": t_1hz.copy()}, None)
        assert res["anchors"].size == 0
        assert res["X_H"]["A"].shape == (0, 300)
        assert res["n_invalid_kf"] == 0

    def test_shape_mismatch_raises(self):
        t_1hz, hist = _hist_stream()
        bad = dict(hist)
        bad["A"] = bad["A"][:-10]
        with pytest.raises(ValueError, match="不一致"):
            build_windows(t_1hz, bad, None)


# ---------- K/F vintage 对齐（DGN-07） ----------

class TestVintage:
    def test_exact_match_and_column_values(self):
        t_1hz, hist = _hist_stream()
        kf = _kf([300.0, 310.0])
        res = build_windows(t_1hz, hist, kf)
        assert res["n_invalid_kf"] == res["anchors"].size - 2
        np.testing.assert_allclose(res["X_KF"]["v_plan_mps"][0],
                                   np.arange(30) / 100.0 + 0.0)      # issue=300 → 行 0
        np.testing.assert_allclose(res["X_KF"]["v_plan_mps"][1],
                                   np.arange(30) / 100.0 + 1.0)      # issue=310 → 行 1
        # 命中行无 NaN
        assert not np.any(np.isnan(res["X_KF"]["v_plan_mps"][:2]))

    def test_tolerance_within_1e9(self):
        t_1hz, hist = _hist_stream()
        kf = _kf([300.0 + 5e-10])  # 容差内 → 命中 anchor 300
        res = build_windows(t_1hz, hist, kf)
        assert res["n_invalid_kf"] == res["anchors"].size - 1
        assert not np.any(np.isnan(res["X_KF"]["v_plan_mps"][0]))

    def test_outside_tolerance_is_invalid(self):
        t_1hz, hist = _hist_stream()
        kf = _kf([300.0 + 1e-6])  # 超出 1e-9 → 不命中
        res = build_windows(t_1hz, hist, kf)
        assert res["n_invalid_kf"] == res["anchors"].size
        assert np.all(np.isnan(res["X_KF"]["v_plan_mps"]))

    def test_missing_vintage_counted_not_raised(self):
        t_1hz, hist = _hist_stream()
        kf = _kf([300.0])  # 只有一个 vintage，其余 30 个 anchor 缺失
        res = build_windows(t_1hz, hist, kf)
        assert res["anchors"].size == 31
        assert res["n_invalid_kf"] == 30  # 计数返回，不抛异常
        assert np.all(np.isnan(res["X_KF"]["v_plan_mps"][1:]))

    def test_kf_none_all_invalid_empty_xkf(self):
        t_1hz, hist = _hist_stream()
        res = build_windows(t_1hz, hist, None)
        assert res["X_KF"] == {}
        assert res["n_invalid_kf"] == res["anchors"].size

    def test_unsorted_lead_row_reordered_ascending(self):
        t_1hz, hist = _hist_stream()
        # 行值按 k 单调，但 lead 列被置换 → 输出须按 lead 升序还原 k=1..30 顺序
        kf = _kf([300.0], lead_val_fn=lambda i: np.arange(30) / 100.0)
        res = build_windows(t_1hz, hist, kf)
        got = res["X_KF"]["v_plan_mps"][0]
        np.testing.assert_allclose(got, np.arange(30) / 100.0)  # 还原为 lead 升序
        # 与未置换版本逐元素一致
        res_ref = build_windows(t_1hz, hist, _kf([300.0]))
        np.testing.assert_allclose(got, res_ref["X_KF"]["v_plan_mps"][0])

    def test_bad_lead_set_raises(self):
        t_1hz, hist = _hist_stream()
        kf = _kf([300.0])
        kf["lead_s"] = np.tile(10.0 * np.arange(1, 31), (1, 1))
        kf["lead_s"][0, -1] = 305.0  # 300 → 305，取值集合违反冻结网格
        with pytest.raises(ValueError, match="lead_s"):
            build_windows(t_1hz, hist, kf)

    def test_duplicate_vintage_rows_raise(self):
        t_1hz, hist = _hist_stream()
        kf = _kf([300.0, 300.0])
        with pytest.raises(ValueError, match="重复"):
            build_windows(t_1hz, hist, kf)


# ---------- 单窗口组装 ----------

class TestAssembleSingle:
    def test_single_window_shapes_and_values(self):
        t_1hz, hist = _hist_stream()
        kf = _kf([450.0])
        win = assemble_single(450.0, t_1hz, hist, kf)
        assert win["t0"] == 450.0
        assert win["X_H"]["A"].shape == (300,)
        assert win["X_H"]["A"][0] == pytest.approx(151.0, abs=1e-9)   # (150, 151]
        assert win["X_H"]["A"][-1] == pytest.approx(450.0, abs=1e-9)  # (449, 450]
        assert win["X_KF"]["v_plan_mps"].shape == (30,)
        assert not np.any(np.isnan(win["X_KF"]["v_plan_mps"]))

    def test_single_window_missing_kf_returns_none(self):
        t_1hz, hist = _hist_stream()
        win = assemble_single(450.0, t_1hz, hist, None)
        assert win["X_KF"] is None

    def test_broken_grid_raises(self):
        # 网格缺 1 s bin → 300 bin 切片不完整 → fail-fast（禁 padding）
        t_1hz = np.arange(1.0, 900.0 + 1e-9, 1.0)
        t_broken = np.delete(t_1hz, 200)  # 缺右端点 201
        hist = {"A": t_broken.copy()}
        with pytest.raises(ValueError, match="300"):
            assemble_single(500.0, t_broken, hist, None)

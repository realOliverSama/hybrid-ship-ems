#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_views.py — FR-464/477~480 view 白名单单测（T-008/T-010）.

覆盖：H1/H2 白名单列序（registry 顺序、双次构建稳定）、FORBIDDEN/METADATA/
ORACLE/TARGET/未知列入载荷被拒（负向）、H1∪H2 缺列被拒、Y/E300 形状与
一致性校验、quantile 标签守卫。
"""

import numpy as np
import pytest

from dataset_v3 import contracts
from dataset_v3.build_dataset_v3 import assert_no_quantile_labels, build_views
from dataset_v3.targets import e300_of

N = 4


@pytest.fixture(scope="module")
def registry():
    return contracts.load_registry(validate=True)


def _payload(reg, roles, n_rows, n_cols, rng):
    names = []
    for role in roles:
        names += contracts.signal_names(reg, role)
    return {nm: rng.uniform(0.0, 1.0, (n_rows, n_cols)) for nm in names}


def _valid_inputs(reg):
    rng = np.random.default_rng(42)
    x_h = _payload(reg, ("INPUT_H1", "INPUT_H2"), N, 300, rng)
    x_kf = _payload(reg, ("INPUT_K", "INPUT_F"), N, 30, rng)
    y = rng.uniform(400.0, 600.0, (N, 30))
    e300 = np.array([e300_of(row) for row in y])
    return x_h, x_kf, y, e300


# ---------- 白名单列序 ----------

class TestWhitelistViews:
    def test_h1_view_columns(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        views = build_views(x_h, x_kf, y, e300, registry)
        assert list(views["h1_view"]) == contracts.signal_names(registry, "INPUT_H1")
        assert len(views["h1_view"]) == 11

    def test_h2_view_21_columns_registry_order(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        views = build_views(x_h, x_kf, y, e300, registry)
        expected = (contracts.signal_names(registry, "INPUT_H1")
                    + contracts.signal_names(registry, "INPUT_H2"))
        assert list(views["h2_view"]) == expected
        assert len(views["h2_view"]) == 21

    def test_column_order_stable_across_calls(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        v1 = build_views(x_h, x_kf, y, e300, registry)
        v2 = build_views(x_h, x_kf, y, e300, registry)
        assert list(v1["h1_view"]) == list(v2["h1_view"])
        assert list(v1["h2_view"]) == list(v2["h2_view"])

    def test_shapes_and_passthrough(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        views = build_views(x_h, x_kf, y, e300, registry)
        for col in views["h1_view"].values():
            assert col.shape == (N, 300)
        assert views["y_traj"] is y or np.array_equal(views["y_traj"], y)
        np.testing.assert_allclose(views["e300"], e300)


# ---------- 负向：白名单外 role 拒收（FR-464） ----------

class TestRoleRejection:
    def test_forbidden_column_in_xh_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        x_h["P_commit_kW"] = np.zeros((N, 300))  # role=FORBIDDEN（PMS audit 族）
        with pytest.raises(ValueError, match="FORBIDDEN"):
            build_views(x_h, x_kf, y, e300, registry)

    def test_metadata_column_in_xh_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        x_h["scenario_id"] = np.zeros((N, 300))  # role=METADATA
        with pytest.raises(ValueError, match="METADATA"):
            build_views(x_h, x_kf, y, e300, registry)

    def test_oracle_column_in_xkf_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        x_kf["u_actual_future_mps"] = np.zeros((N, 30))  # role=ORACLE（未来真值对应物）
        with pytest.raises(ValueError, match="ORACLE"):
            build_views(x_h, x_kf, y, e300, registry)

    def test_target_role_in_xkf_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        # P_load_total_kW 同名多 role {INPUT_H1, TARGET}——TARGET 禁入 K/F 载荷
        x_kf["P_load_total_kW"] = np.zeros((N, 30))
        with pytest.raises(ValueError, match="TARGET"):
            build_views(x_h, x_kf, y, e300, registry)

    def test_unknown_signal_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        x_h["made_up_col"] = np.zeros((N, 300))
        with pytest.raises(ValueError, match="未登记"):
            build_views(x_h, x_kf, y, e300, registry)

    def test_missing_h1_column_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        del x_h["v_ref_mps"]
        with pytest.raises(ValueError, match="缺少"):
            build_views(x_h, x_kf, y, e300, registry)

    def test_missing_h2_column_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        del x_h["SOC"]
        with pytest.raises(ValueError, match="缺少"):
            build_views(x_h, x_kf, y, e300, registry)


# ---------- Y / E300 一致性（FR-477） ----------

class TestE300Consistency:
    def test_inconsistent_e300_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        e300_bad = e300 + 1e-6
        with pytest.raises(ValueError, match="不一致"):
            build_views(x_h, x_kf, y, e300_bad, registry)

    def test_bad_y_shape_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        with pytest.raises(ValueError, match=r"\(N,30\)"):
            build_views(x_h, x_kf, y[:, :29], e300[:-1], registry)

    def test_bad_e300_shape_rejected(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        with pytest.raises(ValueError, match="E300"):
            build_views(x_h, x_kf, y, e300[:-1], registry)


# ---------- quantile 守卫（FR-478/479） ----------

class TestQuantileGuard:
    def test_valid_views_pass(self, registry):
        x_h, x_kf, y, e300 = _valid_inputs(registry)
        views = build_views(x_h, x_kf, y, e300, registry)
        assert_no_quantile_labels(views)  # 不抛即通过

    def test_q10_label_rejected(self):
        with pytest.raises(ValueError, match="q10"):
            assert_no_quantile_labels({"h1_view": {}, "y_traj": np.zeros((1, 30)),
                                       "target_q10": np.zeros(1)})

    def test_q50_nested_column_rejected(self):
        with pytest.raises(ValueError, match="q50"):
            assert_no_quantile_labels({"h1_view": {"P_bin_realized_q50": np.zeros((1, 300))}})

    def test_generic_quantile_word_rejected(self):
        with pytest.raises(ValueError, match="quantile"):
            assert_no_quantile_labels({"quantile_helper": 1})

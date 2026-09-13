#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_audit_ood.py — audit_ood T-018 重构专项测试（activation record 生效）.

覆盖（delegation 冻结验收面）：
- 轴 B 训练支持下界 1.5 生效（activation axis_b.train_support_min_ht）；
  1.5 边界：ht=1.5 → NONE（支持域），1.3 ≤ ht < 1.5 → OOD。
- H/T < 1.2 → PHYSICS_EXTRAPOLATION/STRESS_SET；[1.2, 1.3) → UNCLASSIFIED
  （margin 带不入 formal OOD）。
- 轴 C 组合由 config/mission_v3/heldout_token_registry.json 加载（D4）：
  与独立 json.load 的 factor 组合逐一对照；composition 词表 = registry
  factor 名；无手抄常量残留。
- classify_ood / validate_ood_records 接口向后兼容（签名与返回结构不变）。
"""

import json
import os

import pytest

from dataset_v3 import audit_ood as ood
from dataset_v3.audit_ood import (AXIS_B_PREREGISTERED_OOD_MIN_HT,
                                  AXIS_B_PREREGISTERED_TRAIN_SUPPORT_MIN_HT,
                                  AXIS_B_STRESS_BELOW_HT, classify_ood,
                                  load_heldout_token_registry,
                                  validate_ood_records)

REGISTRY_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "config", "mission_v3", "heldout_token_registry.json",
)


def _rec(axis, **kw):
    base = {"voyage_id": kw.pop("voyage_id", "ood_t001"),
            "ood_axis": axis, "ood_def_version": "DGN-11-v1",
            "physics_domain_ref": "modelE_physics_v1.1"}
    base.update(kw)
    return base


# ===========================================================================
# 轴 B 1.5 训练支持下界（activation record 生效）
# ===========================================================================
class TestAxisBTrainSupportMinHT15:
    def test_constant_is_15(self):
        assert AXIS_B_PREREGISTERED_TRAIN_SUPPORT_MIN_HT == 1.5

    def test_boundary_15_in_support_none(self):
        # 1.5 含于训练支持域（OOD 带上开端）
        r = classify_ood(_rec("B", ht_ratio=1.5))
        assert r["verdict"] == "NONE"

    def test_below_15_is_ood(self):
        assert classify_ood(_rec("B", ht_ratio=1.4999))["verdict"] == "OOD"
        assert classify_ood(_rec("B", ht_ratio=1.3))["verdict"] == "OOD"

    def test_former_20_boundary_now_support(self):
        # 旧框架值 2.0 一带全部回归支持域（activation 后 [1.5, 2.0) 数值
        # 上属 IID 支撑，谓词驱动判定不归本模块）
        assert classify_ood(_rec("B", ht_ratio=2.0))["verdict"] == "NONE"
        assert classify_ood(_rec("B", ht_ratio=1.7))["verdict"] == "NONE"

    def test_membership_lower_endpoint_inclusive(self):
        assert AXIS_B_PREREGISTERED_OOD_MIN_HT == 1.3
        assert classify_ood(_rec("B", ht_ratio=1.3))["verdict"] == "OOD"


# ===========================================================================
# 物理域 guard（STRESS / margin 带）
# ===========================================================================
class TestAxisBPhysicsGuard:
    def test_below_12_stress_set(self):
        r = classify_ood(_rec("B", ht_ratio=1.19))
        assert r["verdict"] == "PHYSICS_EXTRAPOLATION"
        assert r["stress_set"] is True

    def test_margin_band_not_formal_ood(self):
        # [1.2, 1.3)：UNCLASSIFIED，不入 formal OOD 亦不入支持域
        assert AXIS_B_STRESS_BELOW_HT == 1.2
        for ht in (1.2, 1.25, 1.299):
            r = classify_ood(_rec("B", ht_ratio=ht))
            assert r["verdict"] == "UNCLASSIFIED"
            assert r["stress_set"] is False

    def test_axis_a_upper_guard_stress(self):
        r = classify_ood(_rec("A", u_abs_mps=2.81))
        assert r["verdict"] == "PHYSICS_EXTRAPOLATION"
        assert r["stress_set"] is True


# ===========================================================================
# 轴 C registry 驱动（D4：canonical token，禁手抄）
# ===========================================================================
class TestAxisCRegistryDriven:
    def _registry_require_tokens(self):
        with open(REGISTRY_PATH, "r", encoding="utf-8") as f:
            reg = json.load(f)
        return [t for t in reg["tokens"] if t.get("ood_action") == "REQUIRE"]

    def test_registry_loads_three_require_tokens(self):
        toks = self._registry_require_tokens()
        assert {t["token_id"] for t in toks} == {
            "C1_BRIDGE_X_HIGH_ID_ADVERSE_CURRENT",
            "C2_HARBOR_X_LOW_ID_HT",
            "C3_EMERGENCY_SPRINT_X_LOW_ID_HT",
        }
        assert load_heldout_token_registry() == toks

    def test_combinations_match_independent_file_read(self):
        combos = ood.held_out_combinations()
        expect = tuple(frozenset((t["factor_a"], t["factor_b"]))
                       for t in self._registry_require_tokens())
        assert set(combos) == set(expect)
        assert len(combos) == 3

    def test_each_registry_combo_hits_ood(self):
        for tk in self._registry_require_tokens():
            rec = _rec("C", composition=sorted(
                (tk["factor_a"], tk["factor_b"])))
            r = classify_ood(rec)
            assert r["verdict"] == "OOD", tk["token_id"]

    def test_vocabulary_from_factors(self):
        vocab = ood.held_out_composition_vocabulary()
        expect = set()
        for t in self._registry_require_tokens():
            expect.update((t["factor_a"], t["factor_b"]))
        assert vocab == frozenset(expect)

    def test_cross_factor_mixture_is_none(self):
        # 不同 token 的 factor_a 混搭不构成任何 held-out 组合
        rec = _rec("C", composition=["BRIDGE_PASSAGE", "HARBOR"])
        assert classify_ood(rec)["verdict"] == "NONE"

    def test_old_handwritten_vocabulary_gone(self):
        # 手写常量已移除（D4）
        assert not hasattr(ood, "AXIS_C_HELD_OUT_COMBINATIONS")

    def test_no_independent_axis_c_threshold_constants(self):
        # 模块内不得再保留轴 C 独立数值阈值（阈值唯一事实源 = registry）
        src = open(os.path.abspath(
            ood.__file__.replace(".pyc", ".py")), encoding="utf-8").read()
        for legacy in ("bridge_zone", "strong_counter_current",
                       "port_area", "emergency_sprint", "shallow_water"):
            assert legacy not in src


# ===========================================================================
# 接口向后兼容
# ===========================================================================
class TestBackwardCompatibleInterface:
    def test_classify_signature_and_shape(self):
        r = classify_ood(_rec("A", u_abs_mps=2.4))
        assert set(("voyage_id", "ood_axis", "verdict", "stress_set",
                    "detail")) <= set(r)
        assert r["verdict"] == "OOD"

    def test_validate_records_pass(self):
        records = [
            _rec("A", voyage_id="a", u_abs_mps=2.2),
            _rec("B", voyage_id="b", ht_ratio=1.4),
            _rec("C", voyage_id="c", composition=["HARBOR", "LOW_ID_HT"]),
        ]
        assert validate_ood_records(records) == []

    def test_bad_axis_valueerror(self):
        with pytest.raises(ValueError, match="ood_axis"):
            classify_ood(_rec("D"))

    def test_missing_axis_value_valueerror(self):
        with pytest.raises(ValueError, match="u_abs_mps"):
            classify_ood(_rec("A"))

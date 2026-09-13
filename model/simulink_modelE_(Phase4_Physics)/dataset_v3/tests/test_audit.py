#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_audit.py — 泄漏审计 L1/L2/L3 + OOD 装载/域检查单元测试（T-012；DGN-10/11）.

覆盖（负向检出为 D0 硬要求，AC-427）：
- L1：三种泄漏形态构造必须检出（跨 split identity / raw artifact hash 重复 /
  窗口级 exact duplicate）；无缺陷时 PASS；content hash 内容敏感性。
- L2：candidate → CONFIRMED_DUPLICATE / LEGITIMATE_NEAR_REPLICATE / UNRESOLVED
  三分类路径；长度/容差不构成 candidate；报告-only 零副作用。
- L3：跨 split 近邻 flag 产生但不 FAIL；阈值 policy 数值由输入计算；统计来源声明。
- OOD：三轴判定全路径 + 越域 STRESS_SET + 装载器结构校验。
"""

import copy

import numpy as np
import pytest

from dataset_v3 import audit_leakage as al
from dataset_v3 import audit_ood as ood
from dataset_v3.audit_leakage import (L2_EPSILON_KW, run_l1, run_l2, run_l3,
                                      window_content_hash)
from dataset_v3.audit_ood import classify_ood, validate_ood_records


# ===========================================================================
# L1
# ===========================================================================
class TestL1:
    def test_clean_pass(self):
        split_of = {"v1": "train", "v2": "val", "v3": "test"}
        artifacts = {"v1": "a" * 64, "v2": "b" * 64, "v3": "c" * 64}
        windows = {"v1#w0": "h1", "v1#w1": "h2", "v2#w0": "h3"}
        res = run_l1(split_of, artifacts, windows)
        assert res["overall_l1"] is True
        assert res["violations"] == []

    def test_detects_identity_cross_split(self):
        # 形态①：同一 voyage identity（含子记录键）映射到 >1 个 split
        split_of = {"voyA#w0": "train", "voyA#w1": "test", "voyB#w0": "train"}
        res = run_l1(split_of, {"voyA": "x" * 64, "voyB": "y" * 64}, {})
        assert res["overall_l1"] is False
        types = [v["type"] for v in res["violations"]]
        assert "identity_cross_split" in types
        v = next(v for v in res["violations"] if v["type"] == "identity_cross_split")
        assert v["identity"] == "voyA"
        assert v["splits"] == ["test", "train"]

    def test_detects_cross_split_plain_and_mixed_keys(self):
        # 恒等键与子记录键混用同样归并到同一 identity 判定
        split_of = {"voyA": "val", "voyA#reimport": "train"}
        res = run_l1(split_of, {}, {})
        assert res["overall_l1"] is False
        assert any(v["type"] == "identity_cross_split" for v in res["violations"])

    def test_detects_duplicate_artifact_hash(self):
        # 形态②：不同 voyage 共享同一 raw artifact SHA256
        split_of = {"v1": "train", "v2": "val"}
        artifacts = {"v1": "deadbeef" * 8, "v2": "deadbeef" * 8}
        res = run_l1(split_of, artifacts, {})
        assert res["overall_l1"] is False
        v = next(v for v in res["violations"] if v["type"] == "duplicate_artifact_sha256")
        assert sorted(v["voyage_ids"]) == ["v1", "v2"]

    def test_detects_duplicate_window_content(self):
        # 形态③：两个窗口 content hash 完全一致（exact duplicate）
        rng = np.random.default_rng(7)
        x_h = rng.standard_normal((300, 3))
        x_kf = rng.standard_normal((30, 2))
        y = rng.standard_normal(30)
        h = window_content_hash(x_h, x_kf, y)
        res = run_l1({"v1": "train", "v2": "test"},
                     {"v1": "p" * 64, "v2": "q" * 64},
                     {"v1#w0": h, "v2#w5": h})
        assert res["overall_l1"] is False
        v = next(v for v in res["violations"] if v["type"] == "duplicate_window_content")
        assert sorted(v["windows"]) == ["v1#w0", "v2#w5"]

    def test_invalid_split_value_reported(self):
        res = run_l1({"v1": "holdout"}, {}, {})
        assert res["overall_l1"] is False
        assert any(v["type"] == "invalid_split_value" for v in res["violations"])


class TestWindowContentHash:
    def test_deterministic_and_metadata_free(self):
        rng = np.random.default_rng(1)
        x_h, x_kf, y = rng.standard_normal((300, 4)), rng.standard_normal((30, 2)), rng.standard_normal(30)
        # metadata identity 不参与：同一科学载荷（无论属于哪个 voyage）hash 一致
        assert window_content_hash(x_h, x_kf, y) == window_content_hash(x_h, x_kf, y)

    def test_sensitive_to_value_change(self):
        rng = np.random.default_rng(2)
        x_h, x_kf, y = rng.standard_normal((300, 3)), rng.standard_normal((30, 2)), rng.standard_normal(30)
        base = window_content_hash(x_h, x_kf, y)
        x_h2 = x_h.copy()
        x_h2[150, 1] += 1e-12
        assert window_content_hash(x_h2, x_kf, y) != base

    def test_sensitive_to_column_order(self):
        rng = np.random.default_rng(3)
        x_h = rng.standard_normal((300, 3))
        x_kf, y = rng.standard_normal((30, 2)), rng.standard_normal(30)
        base = window_content_hash(x_h, x_kf, y)
        swapped = window_content_hash(x_h[:, [2, 1, 0]], x_kf, y)  # 逐列固定列序破坏 → hash 变
        assert swapped != base

    def test_block_shape_disambiguation(self):
        # canonical 序列化带 shape 头：同一扁平字节流以不同 block 形状呈现 → hash 必不同
        rng = np.random.default_rng(4)
        flat = rng.standard_normal(600)
        h1 = window_content_hash(flat.reshape(300, 2), np.zeros((30, 2)), np.zeros(30))
        h2 = window_content_hash(flat.reshape(30, 20), np.zeros((30, 2)), np.zeros(30))
        assert h1 != h2


# ===========================================================================
# L2
# ===========================================================================
def _prov(seed, template):
    return {"generation_seed": seed, "route_template_id": template}


class TestL2:
    def test_confirmed_duplicate(self):
        s = np.linspace(100.0, 200.0, 60)
        res = run_l2({"vA": s, "vB": s.copy()},
                     {"vA": _prov(7, "route_T1"), "vB": _prov(7, "route_T1")})
        assert res["counts"]["l2_candidate"] == 1
        c = res["candidates"][0]
        assert c["classification"] == "CONFIRMED_DUPLICATE"
        assert c["max_abs_diff_kw"] < L2_EPSILON_KW
        assert res["counts"]["CONFIRMED_DUPLICATE"] == 1

    def test_legitimate_near_replicate(self):
        s = np.linspace(50.0, 80.0, 100)
        res = run_l2({"vA": s, "vB": s.copy()},
                     {"vA": _prov(7, "route_T1"), "vB": _prov(8, "route_T1")})
        c = res["candidates"][0]
        assert c["classification"] == "LEGITIMATE_NEAR_REPLICATE"

    def test_unresolved_missing_provenance(self):
        s = np.full(40, 12.0)
        res = run_l2({"vA": s, "vB": s.copy()}, {"vA": _prov(7, "T")})  # vB provenance 缺失
        assert res["candidates"][0]["classification"] == "UNRESOLVED"

    def test_length_mismatch_not_candidate(self):
        res = run_l2({"vA": np.ones(40), "vB": np.ones(41)}, {})
        assert res["counts"]["l2_candidate"] == 0

    def test_above_epsilon_not_candidate(self):
        s = np.full(50, 10.0)
        res = run_l2({"vA": s, "vB": s + 2e-6}, {})  # max|Δ| = 2e-6 ≥ ε
        assert res["counts"]["l2_candidate"] == 0
        assert res["n_pairs_compared"] == 1

    def test_report_only_no_mutation(self):
        s = np.linspace(1.0, 2.0, 30)
        series = {"vA": s, "vB": s.copy()}
        prov = {"vA": _prov(1, "T"), "vB": _prov(1, "T")}
        series_before = copy.deepcopy(series)
        prov_before = copy.deepcopy(prov)
        res = run_l2(series, prov)
        assert res["report_only"] is True
        for k in series:                      # 序列数据零改动（禁自动剔除）
            assert np.array_equal(series[k], series_before[k])
        assert set(series.keys()) == set(series_before.keys())
        assert prov == prov_before            # provenance 零改动


# ===========================================================================
# L3
# ===========================================================================
def _shape_series(kind: str, n: int = 400) -> np.ndarray:
    t = np.arange(n, dtype=float)
    if kind == "sine":
        return 100.0 + 20.0 * np.sin(2 * np.pi * t / 50.0)
    if kind == "rect":
        return 120.0 + 30.0 * np.abs(np.cos(2 * np.pi * t / 23.0))
    if kind == "ramp_step":
        return 80.0 + 0.1 * t + 40.0 * (t > n // 2)
    if kind == "sine_fast":
        return 110.0 + 15.0 * np.sin(2 * np.pi * t / 9.0)
    raise ValueError(kind)


class TestL3:
    def _dataset(self):
        train = {"A": _shape_series("sine"), "B": _shape_series("rect"),
                 "C": _shape_series("ramp_step")}
        # T = train A 的近同拷贝（微小噪声）→ 跨 split 近邻
        rng = np.random.default_rng(0)
        test = {"T": train["A"] + rng.normal(0, 1e-3, train["A"].size)}
        val = {"V": _shape_series("sine_fast")}
        series = {**train, **test, **val}
        split_of = {"A": "train", "B": "train", "C": "train",
                    "T": "test", "V": "val"}
        return series, split_of

    def test_flag_produced_but_never_fails(self):
        series, split_of = self._dataset()
        res = run_l3(series, split_of, train_stats=None)
        # similarity ≠ leakage：L3 只产 flag，永不 FAIL（无 overall/hard-fail 语义）
        assert res["flag_only"] is True
        assert res["policy"] == al.SBD_THRESHOLD_POLICY
        assert res["stats_source"] == "population_fallback_declared"  # 无 train 统计输入 → 全体 + 声明
        assert res["t_sbd"] is not None and 0.0 <= res["t_sbd"] <= 1.0
        assert res["n_cross_split_pairs"] == 7
        assert res["n_suspects"] >= 1
        suspect_pairs = {tuple(s["pair"]) for s in res["suspects"]}
        assert any("T" in p for p in suspect_pairs)   # 近同拷贝被标记
        for s in res["suspects"]:
            assert s["disposition"] == "unresolved"
            assert set(s["splits"]) != {"train"}       # 仅跨 split 对

    def test_train_stats_source_declared(self):
        series, split_of = self._dataset()
        res = run_l3(series, split_of, train_stats={"mean": 0.0, "std": 1.0})
        assert res["stats_source"] == "train_stats"
        assert res["flag_only"] is True

    def test_clean_data_no_suspects(self):
        series = {"A": _shape_series("sine"), "B": _shape_series("rect"),
                  "C": _shape_series("ramp_step"), "T": _shape_series("sine_fast")}
        split_of = {"A": "train", "B": "train", "C": "train", "T": "test"}
        res = run_l3(series, split_of, train_stats=None)
        assert res["flag_only"] is True   # 无论有无 suspect，L3 都不构成 FAIL
        assert res["n_suspects"] == 0

    def test_insufficient_train_returns_null_threshold(self):
        series = {"A": _shape_series("sine"), "T": _shape_series("rect")}
        split_of = {"A": "train", "T": "test"}  # train 仅 1 条 → NN 分布空
        res = run_l3(series, split_of, train_stats=None)
        assert res["t_sbd"] is None
        assert res["suspects"] == []

    def test_split_coverage_fail_fast(self):
        series = {"A": _shape_series("sine")}
        with pytest.raises(ValueError, match="split_of"):
            run_l3(series, {}, train_stats=None)


# ===========================================================================
# OOD（audit_ood）
# ===========================================================================
def _rec(axis, **kw):
    base = {"voyage_id": kw.pop("voyage_id", "ood_001"),
            "ood_axis": axis, "ood_def_version": "ood_def_v1",
            "physics_domain_ref": "modelE_physics_v1.1"}
    base.update(kw)
    return base


class TestOODAxisA:
    def test_paths(self):
        assert classify_ood(_rec("A", u_abs_mps=1.5))["verdict"] == "NONE"          # 支持域内
        assert classify_ood(_rec("A", u_abs_mps=2.0))["verdict"] == "NONE"          # 2.0 不含端点（严格大于）
        assert classify_ood(_rec("A", u_abs_mps=2.4))["verdict"] == "OOD"
        assert classify_ood(_rec("A", u_abs_mps=2.8))["verdict"] == "OOD"           # ≤2.8 含端点
        r = classify_ood(_rec("A", u_abs_mps=3.0))
        assert r["verdict"] == "PHYSICS_EXTRAPOLATION"
        assert r["stress_set"] is True                                              # 越域 STRESS_SET

    def test_negative_abs_value_uses_magnitude(self):
        # 轴 A 定义在 |U| 上：u_abs_mps 传入带符号值时按绝对值入带
        assert classify_ood(_rec("A", u_abs_mps=-2.4))["verdict"] == "OOD"


class TestOODAxisB:
    def test_paths(self):
        # T-018 重构：训练支持下界 = 1.5（T-432 activation record 生效）
        assert classify_ood(_rec("B", ht_ratio=2.5))["verdict"] == "NONE"           # 支持域
        assert classify_ood(_rec("B", ht_ratio=2.0))["verdict"] == "NONE"           # ≥1.5 支持域
        assert classify_ood(_rec("B", ht_ratio=1.5))["verdict"] == "NONE"           # 1.5 含于支持域（OOD 带上开端）
        assert classify_ood(_rec("B", ht_ratio=1.4999))["verdict"] == "OOD"
        assert classify_ood(_rec("B", ht_ratio=1.3))["verdict"] == "OOD"            # 含下端点
        assert classify_ood(_rec("B", ht_ratio=1.25))["verdict"] == "UNCLASSIFIED"  # 1.2~1.3 margin 带
        assert classify_ood(_rec("B", ht_ratio=1.2))["verdict"] == "UNCLASSIFIED"
        r = classify_ood(_rec("B", ht_ratio=1.0))
        assert r["verdict"] == "PHYSICS_EXTRAPOLATION"
        assert r["stress_set"] is True


class TestOODAxisC:
    """轴 C 词表 = registry factor 名（T-018 重构：canonical token registry
    驱动，D4；词表见 heldout_token_registry.json factor_a/factor_b）。"""

    def test_held_out_combo_hit(self):
        rec = _rec("C", composition=["BRIDGE_PASSAGE",
                                     "HIGH_ID_ADVERSE_CURRENT"])
        assert classify_ood(rec)["verdict"] == "OOD"

    def test_combo_with_extra_factors_still_hits(self):
        rec = _rec("C", composition=["HARBOR", "LOW_ID_HT", "FOG"])
        assert classify_ood(rec)["verdict"] == "OOD"

    def test_single_factors_only_none(self):
        rec = _rec("C", composition=["BRIDGE_PASSAGE", "HARBOR"])  # 均为单因子，无 held-out 组合
        assert classify_ood(rec)["verdict"] == "NONE"

    def test_all_registry_combos(self):
        for combo in ood.held_out_combinations():
            rec = _rec("C", composition=sorted(combo))
            assert classify_ood(rec)["verdict"] == "OOD"


class TestOODLoader:
    def test_valid_records_pass(self):
        records = [
            _rec("A", voyage_id="a1", u_abs_mps=2.4),
            _rec("B", voyage_id="b1", ht_ratio=1.5),
            _rec("C", voyage_id="c1", composition=["HARBOR"]),
        ]
        assert validate_ood_records(records) == []

    def test_missing_common_field(self):
        rec = _rec("A", u_abs_mps=2.4)
        del rec["physics_domain_ref"]
        errs = validate_ood_records([rec])
        assert any("physics_domain_ref" in e for e in errs)

    def test_bad_axis(self):
        errs = validate_ood_records([_rec("D")])
        assert any("ood_axis" in e for e in errs)

    def test_missing_axis_value(self):
        errs = validate_ood_records([_rec("A")])          # 缺 u_abs_mps
        assert any("u_abs_mps" in e for e in errs)
        errs = validate_ood_records([_rec("C", composition="not-a-list")])
        assert any("composition" in e for e in errs)

    def test_axis_field_on_wrong_axis(self):
        errs = validate_ood_records([_rec("B", ht_ratio=1.5, u_abs_mps=2.4)])
        assert any("u_abs_mps" in e for e in errs)

    def test_duplicate_voyage_id(self):
        records = [_rec("A", voyage_id="dup", u_abs_mps=2.4),
                   _rec("B", voyage_id="dup", ht_ratio=1.5)]
        errs = validate_ood_records(records)
        assert any("重复" in e for e in errs)

    def test_empty_composition_flagged(self):
        errs = validate_ood_records([_rec("C", composition=[])])
        assert errs

    def test_sg_canonical_identity_preserved(self):
        """T-020 裁决 A 回归：canonical `SG#<hash>` 身份在 L1 键解析后保持互异.

        formal split_group_id 自含 '#'（SG#<hash>），窗口级恒等键形如
        `SG#<hash>#voyage`。解析必须折叠同一 group 的多子记录、且不同 group
        互不折叠；裸 `SG#<hash>` 键（无子记录）恒等为其自身。
        """
        assert al.identity_of("SG#661a002f4c6a667f#voyage") == "SG#661a002f4c6a667f"
        assert al.identity_of("SG#3d237747a5b34afe#voyage") == "SG#3d237747a5b34afe"
        assert al.identity_of("SG#661a002f4c6a667f") == "SG#661a002f4c6a667f"
        # 不同 SG 身份不得折叠；voyage 级键不受影响
        assert (al.identity_of("SG#661a002f4c6a667f")
                != al.identity_of("SG#3d237747a5b34afe"))
        assert al.identity_of("FLTA_S1_D00000_M1#186") == "FLTA_S1_D00000_M1"

    def test_sg_distinct_identities_no_cross_split_violation(self):
        """两个不同 SG#<hash> group 分属不同 split → 不产生 identity 违规."""
        split_of = {
            "SG#aaa#voyage": "train", "SG#bbb#voyage": "test",
            "SG#aaa#win186": "train", "SG#bbb#win0": "test",
        }
        res = run_l1(split_of, {}, {})
        cross = [v for v in res["violations"]
                 if v["type"] == "identity_cross_split"]
        assert cross == []

    def test_sg_same_group_across_split_still_flagged(self):
        """同一 SG#<hash> group 跨 split 仍须违规（解析修复不得放水）."""
        split_of = {"SG#aaa#voyage": "train", "SG#aaa#win186": "test"}
        res = run_l1(split_of, {}, {})
        v = next(v for v in res["violations"]
                 if v["type"] == "identity_cross_split")
        assert v["identity"] == "SG#aaa"
        assert v["splits"] == ["test", "train"]


class TestL2FamilySibling:
    """T-020 裁决 B：同 split_group 内 provenance 撞车 → DECLARED_FAMILY_SIBLING.

    跨独立 split_group 的撞车维持 CONFIRMED_DUPLICATE；sibling 对交
    run_family_sibling_audit 验证真值共享与 member lineage 互异。
    """

    def test_same_group_provenance_collision_is_sibling(self):
        series = {"m1": [1.0, 2.0], "m2": [1.0, 2.0]}
        prov = {v: {"generation_seed": "s", "route_template_id": "r"}
                for v in series}
        res = run_l2(series, prov, group_of={"m1": "SG#a", "m2": "SG#a"})
        assert res["counts"]["DECLARED_FAMILY_SIBLING"] == 1
        assert res["counts"]["CONFIRMED_DUPLICATE"] == 0
        assert res["family_sibling_pairs"] == [["m1", "m2"]]

    def test_cross_group_provenance_collision_still_confirmed(self):
        series = {"m1": [1.0, 2.0], "m2": [1.0, 2.0]}
        prov = {v: {"generation_seed": "s", "route_template_id": "r"}
                for v in series}
        res = run_l2(series, prov, group_of={"m1": "SG#a", "m2": "SG#b"})
        assert res["counts"]["CONFIRMED_DUPLICATE"] == 1
        assert res["counts"]["DECLARED_FAMILY_SIBLING"] == 0

    def test_family_sibling_audit_invariants(self):
        series = {"m1": [1.0, 2.0], "m2": [1.0, 2.0], "m3": [1.0, 9.0]}
        pairs = [("m1", "m2"), ("m1", "m3")]
        audit = al.run_family_sibling_audit(
            pairs, series, member_of={"m1": "M1", "m2": "M2", "m3": "M1"})
        assert audit["overall"] is False
        types = [v["type"] for v in audit["violations"]]
        assert "truth_sharing_violation" in types   # m3 与 m1 真值不一致
        assert "lineage_not_distinct" in types      # m3 与 m1 member_id 撞车

        ok = al.run_family_sibling_audit(
            [("m1", "m2")], series,
            member_of={"m1": "M1", "m2": "M2"})
        assert ok["overall"] is True and ok["n_families"] == 1

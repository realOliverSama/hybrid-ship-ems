#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_manifest.py — manifest 构建 + deterministic payload hash 单元测试
（T-014；DGN-12 + R8；FR-474/488）.

覆盖：15 项 FR-488 字段齐全 / 缺项 FAIL（validate）与构建 fail-fast（build）/
sealed 双 True 强制 / stride 与 runtime 分列 / payload hash 确定性（时间戳不同
不影响）/ 自引用 hash 与 run_metadata 排除。
"""

import copy

import pytest

from dataset_v3.manifest import (PAYLOAD_HASH_FIELD, build_manifest,
                                 compute_payload_hash, validate_manifest)


def _payload() -> dict:
    """完整合法 payload（FR-488 十五项 + DGN-12 追加七项）。"""
    return {
        # ---- FR-488 十五项 ----
        "split_policy_version": "split_policy_v3_pilot",
        "split_seed": "seed-2026-pilot",
        "split_group_key": "voyage_id",
        "split_assignment_artifact_sha256": "a" * 64,
        "train_val_test_voyage_counts": {"train": 7, "val": 1, "test": 2},
        "normalization_source": "train_only",
        "preprocessing_artifact_sha256": "b" * 64,
        "ood_definition_version": "ood_def_v1",
        "ood_generation_seed": "ood-seed-001",
        "provenance_duplicate_count": 0,
        "exact_duplicate_count": 0,
        "sbd_threshold_policy": "T_SBD = Q0.01(train-vs-train NN SBD distance distribution)",
        "sbd_suspect_count_disposition": {"count": 0, "disposition": "none"},
        "test_sealed": True,
        "ood_sealed": True,
        # ---- DGN-12 追加 ----
        "dataset_anchor_stride_s": 10,
        "predictor_runtime_period_s": "UNDEFINED_IN_D",
        "window_geometry": {"L_H": 300, "H_F": 300, "K": 30, "delta_F": 10, "stride": 10},
        "registry_sha256": "c" * 64,
        "producer_schema_sha256": "d" * 64,
        "aggregation_table_version": "dgn05_frozen_v1",
        "target_definition": "y_k = E_k*3600/10 [kW], E300 = sum(E_k)/3600 [kWh]",
    }


def _meta(ts: str) -> dict:
    # R2-M-1 闭合：DGN-12 冻结的 run_metadata 必备槽位（脚本路径+SHA/版本/构建时间）
    return {"build_timestamp_utc": ts, "wall_time_s": 12.3, "host": "runner",
            "python_version": "3.12", "producer": "70_export_dataset_v3_pilot.m",
            "build_utc": ts,
            "generation_script_path": "dataset_v3/run_pipeline.py",
            "generation_script_sha256": "0" * 64,
            "matlab_version": "R2026a（70 producer）"}


# ---------------------------------------------------------------------------
# 构建与校验（正向）
# ---------------------------------------------------------------------------
def test_build_and_validate_clean():
    m = build_manifest(_payload(), _meta("2026-09-05T00:00:00Z"))
    assert m["schema"] == "dataset_v3_manifest_v1"
    assert validate_manifest(m) == []
    assert m[PAYLOAD_HASH_FIELD] == compute_payload_hash(_payload())
    # payload / run_metadata 原样保留（不 mutate 调用方输入）
    assert m["run_metadata"]["build_timestamp_utc"] == "2026-09-05T00:00:00Z"


# ---------------------------------------------------------------------------
# 缺项 / 非法值
# ---------------------------------------------------------------------------
def test_build_fail_fast_on_missing_field():
    p = _payload()
    del p["split_assignment_artifact_sha256"]
    with pytest.raises(ValueError, match="split_assignment_artifact_sha256"):
        build_manifest(p, _meta("t"))


def test_build_fail_fast_on_frozen_value_violation():
    p = _payload()
    p["normalization_source"] = "all_data"
    with pytest.raises(ValueError, match="normalization_source"):
        build_manifest(p, _meta("t"))
    p2 = _payload()
    p2["dataset_anchor_stride_s"] = 5
    with pytest.raises(ValueError, match="stride"):
        build_manifest(p2, _meta("t"))


def test_validate_reports_missing_field():
    m = build_manifest(_payload(), _meta("t"))
    del m["payload"]["ood_definition_version"]
    errs = validate_manifest(m)
    assert any("ood_definition_version" in e for e in errs)
    assert any(PAYLOAD_HASH_FIELD in e for e in errs)  # hash 随之失配，一并报告


def test_validate_reports_sealed_violation():
    m = build_manifest(_payload(), _meta("t"))
    m["payload"]["test_sealed"] = False
    errs = validate_manifest(m)
    assert any("test_sealed" in e for e in errs)

    m2 = build_manifest(_payload(), _meta("t"))
    m2["payload"]["ood_sealed"] = False
    errs2 = validate_manifest(m2)
    assert any("ood_sealed" in e for e in errs2)


def test_validate_reports_stride_runtime_separation_violation():
    # FR-474：stride 与 predictor runtime 分列且值冻结
    m = build_manifest(_payload(), _meta("t"))
    m["payload"]["predictor_runtime_period_s"] = 10  # 不得与 stride 混同
    errs = validate_manifest(m)
    assert any("predictor_runtime_period_s" in e for e in errs)

    m2 = build_manifest(_payload(), _meta("t"))
    m2["payload"]["window_geometry"]["stride"] = 20
    errs2 = validate_manifest(m2)
    assert any("window_geometry" in e for e in errs2)


def test_validate_rejects_missing_hash_or_schema():
    m = build_manifest(_payload(), _meta("t"))
    del m[PAYLOAD_HASH_FIELD]
    assert any(PAYLOAD_HASH_FIELD in e for e in validate_manifest(m))
    m2 = build_manifest(_payload(), _meta("t"))
    m2["schema"] = "wrong"
    assert any("schema" in e for e in validate_manifest(m2))


# ---------------------------------------------------------------------------
# 确定性（R8：payload / run_metadata 分离）
# ---------------------------------------------------------------------------
def test_payload_hash_deterministic_across_run_metadata():
    p = _payload()
    m1 = build_manifest(p, _meta("2026-09-05T01:00:00Z"))
    m2 = build_manifest(p, _meta("2026-09-05T23:59:59Z"))
    m2["run_metadata"]["wall_time_s"] = 99.9
    assert m1[PAYLOAD_HASH_FIELD] == m2[PAYLOAD_HASH_FIELD]
    assert m1["run_metadata"] != m2["run_metadata"]      # 时间戳不同确实存在
    # 除 run_metadata 外其余部分逐字节一致
    assert (m1["schema"], m1["payload"], m1[PAYLOAD_HASH_FIELD]) \
        == (m2["schema"], m2["payload"], m2[PAYLOAD_HASH_FIELD])


def test_payload_hash_excludes_self_hash_and_run_metadata():
    p = _payload()
    base = compute_payload_hash(p)
    p_self = dict(p)
    p_self[PAYLOAD_HASH_FIELD] = "junk-self-reference"
    p_self["run_metadata"] = {"build_timestamp_utc": "should-not-count"}
    assert compute_payload_hash(p_self) == base
    # 深拷贝隔离：compute 不 mutate 输入
    assert p_self[PAYLOAD_HASH_FIELD] == "junk-self-reference"


def test_payload_hash_changes_with_scientific_content():
    p1, p2 = _payload(), _payload()
    p2["exact_duplicate_count"] = 1          # 科学内容变化 → hash 必变
    assert compute_payload_hash(p1) != compute_payload_hash(p2)
    p3 = copy.deepcopy(p1)
    p3["sbd_suspect_count_disposition"] = {"count": 2, "disposition": "flagged-unresolved"}
    assert compute_payload_hash(p1) != compute_payload_hash(p3)

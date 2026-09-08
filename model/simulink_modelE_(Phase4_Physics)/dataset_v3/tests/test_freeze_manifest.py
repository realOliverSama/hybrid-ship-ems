#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_freeze_manifest.py — T-020 formal freeze sealed manifest 单元测试.

覆盖：全字段合法 payload 构建成功且 assert_sealed 通过 / 缺任一 FR-488 键
fail-fast（报错含缺失键名）/ test_sealed 非严格 True（False、缺失、字符串
"true"）与 ood_sealed=False 各自 fail-fast / stage != "FORMAL" fail-fast /
payload OOD 定义与 activation record 事实源不一致 fail-fast / 篡改 payload
hash（非 64 位十六进制、错值、缺失）→ assert_sealed 报错 / run_metadata 的
build_utc 变化不影响 payload hash（确定性，payload/run_metadata 分离）。
"""

import json

import pytest

from dataset_v3.freeze_manifest import (FORMAL_REQUIRED_PAYLOAD_KEYS,
                                        assert_sealed,
                                        build_formal_manifest)
from dataset_v3.manifest import (FR488_PAYLOAD_FIELDS, PAYLOAD_HASH_FIELD,
                                 validate_manifest)

# activation record 事实源（docs/phase4/d_dataset_v3/t432_activation_record.json
# 的 formal_fleet.ood 节点值；测试内固化为 fixture 数据，源文件只读不入测试依赖）
ACT_OOD_DEFINITION_VERSION = "DGN-11-v1"
ACT_OOD_SEED_RULE = ("condition c (0-based, order A1/A2/B1/C1/C2/C3) uses "
                     "base_draw_index in [10000+c*1000, 10000+(c+1)*1000), "
                     "k=0..9; same formal seed root")


def _activation(tmp_path, definition_version=ACT_OOD_DEFINITION_VERSION,
                seed_rule=ACT_OOD_SEED_RULE):
    """写一份与 t432_activation_record 同构的 activation record JSON。"""
    path = tmp_path / "t432_activation_record.json"
    path.write_text(json.dumps({
        "schema": "t432_activation_record_v1",
        "status": "FROZEN",
        "formal_fleet": {"ood": {"definition_version": definition_version,
                                 "seed_rule": seed_rule}},
    }, ensure_ascii=False), encoding="utf-8")
    return str(path)


def _payload() -> dict:
    """formal 全字段合法 payload（FR-488 十五项 + DGN-12 追加 + stage）。"""
    return {
        # ---- FR-488 十五项 ----
        "split_policy_version": "dgn09-v1-formal",
        "split_seed": "dataset-v3-formal-split-seed-v1",
        "split_group_key": "split_group_id",
        "split_assignment_artifact_sha256": "a" * 64,
        "train_val_test_voyage_counts": {"train": 404, "val": 86, "test": 86},
        "normalization_source": "train_only",
        "preprocessing_artifact_sha256": "b" * 64,
        "ood_definition_version": ACT_OOD_DEFINITION_VERSION,
        "ood_generation_seed": ACT_OOD_SEED_RULE,
        "provenance_duplicate_count": 0,
        "exact_duplicate_count": 0,
        "sbd_threshold_policy": "T_SBD=Q0.01(train-vs-train NN SBD)（formal）",
        "sbd_suspect_count_disposition": {"count": 0, "disposition": "none"},
        "test_sealed": True,
        "ood_sealed": True,
        # ---- DGN-12 追加 ----
        "dataset_anchor_stride_s": 10,
        "predictor_runtime_period_s": "UNDEFINED_IN_D",
        "window_geometry": {"L_H": 300, "H_F": 300, "K": 30,
                            "delta_F": 10, "stride": 10},
        "registry_sha256": "c" * 64,
        "producer_schema_sha256": "d" * 64,
        "aggregation_table_version": "dgn05-r1",
        "target_definition": "y_k=E_k/10 kW；E300=ΣE_k/3600 kWh",
        # ---- formal stage ----
        "stage": "FORMAL",
    }


def _meta(build_utc: str) -> dict:
    # DGN-12 冻结的 run_metadata 必备槽位（脚本路径+SHA/版本/构建时间）
    return {"build_utc": build_utc,
            "generation_script_path": "dataset_v3/run_formal.py",
            "generation_script_sha256": "0" * 64,
            "matlab_version": "R2026a",
            "python_version": "3.12.7"}


@pytest.fixture()
def act_json(tmp_path):
    return _activation(tmp_path)


# ---------------------------------------------------------------------------
# 正向：全字段合法 → 构建成功 + sealed 复核通过
# ---------------------------------------------------------------------------
def test_build_formal_and_assert_sealed_clean(act_json):
    m = build_formal_manifest(_payload(), _meta("2026-09-07T00:00:00Z"),
                              act_json)
    assert m["schema"] == "dataset_v3_manifest_v1"
    assert m["payload"]["stage"] == "FORMAL"
    assert validate_manifest(m) == []
    assert assert_sealed(m) == []
    assert set(FORMAL_REQUIRED_PAYLOAD_KEYS) <= set(m["payload"])


# ---------------------------------------------------------------------------
# 缺必填键（FR-488 任一键）→ ValueError 且报错含缺失键名
# ---------------------------------------------------------------------------
@pytest.mark.parametrize("key", FR488_PAYLOAD_FIELDS)
def test_missing_fr488_key_raises(act_json, key):
    p = _payload()
    del p[key]
    with pytest.raises(ValueError, match=key):
        build_formal_manifest(p, _meta("t"), act_json)


def test_missing_stage_raises(act_json):
    p = _payload()
    del p["stage"]
    with pytest.raises(ValueError, match="stage"):
        build_formal_manifest(p, _meta("t"), act_json)


# ---------------------------------------------------------------------------
# sealed 非严格 True → ValueError（无默认置 True 路径）
# ---------------------------------------------------------------------------
def test_test_sealed_false_raises(act_json):
    p = _payload()
    p["test_sealed"] = False
    with pytest.raises(ValueError, match="test_sealed"):
        build_formal_manifest(p, _meta("t"), act_json)


def test_test_sealed_missing_raises(act_json):
    p = _payload()
    del p["test_sealed"]
    with pytest.raises(ValueError, match="test_sealed"):
        build_formal_manifest(p, _meta("t"), act_json)


def test_test_sealed_string_true_raises(act_json):
    # 字符串 "true" 不满足严格布尔 True（`is True`）
    p = _payload()
    p["test_sealed"] = "true"
    with pytest.raises(ValueError, match="test_sealed"):
        build_formal_manifest(p, _meta("t"), act_json)


def test_ood_sealed_false_raises(act_json):
    p = _payload()
    p["ood_sealed"] = False
    with pytest.raises(ValueError, match="ood_sealed"):
        build_formal_manifest(p, _meta("t"), act_json)


# ---------------------------------------------------------------------------
# stage != FORMAL → ValueError
# ---------------------------------------------------------------------------
def test_stage_pilot_raises(act_json):
    p = _payload()
    p["stage"] = "PILOT"
    with pytest.raises(ValueError, match="stage"):
        build_formal_manifest(p, _meta("t"), act_json)


# ---------------------------------------------------------------------------
# OOD 定义事实源交叉核对（activation record）
# ---------------------------------------------------------------------------
def test_ood_definition_version_mismatch_raises(tmp_path):
    act = _activation(tmp_path, definition_version="DGN-11-v0-WRONG")
    p = _payload()
    with pytest.raises(ValueError, match="ood_definition_version"):
        build_formal_manifest(p, _meta("t"), act)


def test_ood_generation_seed_mismatch_raises(tmp_path):
    act = _activation(tmp_path, seed_rule="some-other-seed-rule")
    p = _payload()
    with pytest.raises(ValueError, match="ood_generation_seed"):
        build_formal_manifest(p, _meta("t"), act)


# ---------------------------------------------------------------------------
# 篡改 payload hash → assert_sealed 报错
# ---------------------------------------------------------------------------
def test_assert_sealed_rejects_wrong_hash_value(act_json):
    m = build_formal_manifest(_payload(), _meta("t"), act_json)
    m[PAYLOAD_HASH_FIELD] = "ab" * 32          # 64 位十六进制但值错误
    errs = assert_sealed(m)
    assert errs
    assert any(PAYLOAD_HASH_FIELD in e for e in errs)


def test_assert_sealed_rejects_non_hex64_hash(act_json):
    m = build_formal_manifest(_payload(), _meta("t"), act_json)
    m[PAYLOAD_HASH_FIELD] = "deadbeef"         # 非 64 位
    errs = assert_sealed(m)
    assert errs
    assert any("64 位十六进制" in e for e in errs)


def test_assert_sealed_rejects_missing_hash(act_json):
    m = build_formal_manifest(_payload(), _meta("t"), act_json)
    del m[PAYLOAD_HASH_FIELD]
    assert any(PAYLOAD_HASH_FIELD in e for e in assert_sealed(m))


def test_assert_sealed_rejects_unsealed_payload(act_json):
    m = build_formal_manifest(_payload(), _meta("t"), act_json)
    m["payload"]["test_sealed"] = False        # 事后篡改 → hash 失配 + sealed 违规
    errs = assert_sealed(m)
    assert any("test_sealed" in e for e in errs)


# ---------------------------------------------------------------------------
# 确定性：run_metadata 的 build_utc 变化 → validate 仍过、payload hash 不变
# ---------------------------------------------------------------------------
def test_build_utc_change_keeps_payload_hash(act_json):
    p = _payload()
    m1 = build_formal_manifest(p, _meta("2026-09-07T08:00:00Z"), act_json)
    m2 = build_formal_manifest(p, _meta("2026-09-07T23:59:59Z"), act_json)
    assert m1["run_metadata"]["build_utc"] != m2["run_metadata"]["build_utc"]
    assert m1[PAYLOAD_HASH_FIELD] == m2[PAYLOAD_HASH_FIELD]
    assert m1["payload"] == m2["payload"]
    assert validate_manifest(m1) == []
    assert validate_manifest(m2) == []
    assert assert_sealed(m1) == []
    assert assert_sealed(m2) == []

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""manifest.py — Dataset v3 manifest 构建 + deterministic payload hash
（DGN-12 + design 修订 R8；FR-474/488；T-014）.

唯一事实源：.specs/phase4-d-dataset-v3/design.md（DGN-12 + R8）+ requirements.md（FR-474/488）。

结构（R8：证据分离 + 自引用 hash 排除）：
  manifest = {
    "schema": "dataset_v3_manifest_v1",
    "payload":      {…确定性科学内容（FR-488 十五项 + 几何/契约 hash 等）…},
    "run_metadata": {…时间戳/wall_time/主机/版本等非确定性运行环境信息…},
    "scientific_payload_sha256": SHA256(canonical_json(payload，排除自身 hash 字段
                                 与全部 run_metadata))
  }
  canonical = json.dumps(sort_keys=True, separators=(',',':'), ensure_ascii=False)
  .encode("utf-8")（与 validate_contracts.canonical_json 同一配方）。

确定性：同 payload 两次构建（run_metadata 时间戳不同）→ scientific_payload_sha256
一致（build_timestamp 不入 payload hash——T-014 确定性要求）。
"""

from __future__ import annotations

import hashlib

from .validate_contracts import canonical_json

MANIFEST_SCHEMA = "dataset_v3_manifest_v1"
PAYLOAD_HASH_FIELD = "scientific_payload_sha256"

# ---- FR-488 十五项字段名（冻结） ----
FR488_PAYLOAD_FIELDS = (
    "split_policy_version",
    "split_seed",
    "split_group_key",
    "split_assignment_artifact_sha256",
    "train_val_test_voyage_counts",
    "normalization_source",
    "preprocessing_artifact_sha256",
    "ood_definition_version",
    "ood_generation_seed",
    "provenance_duplicate_count",
    "exact_duplicate_count",
    "sbd_threshold_policy",
    "sbd_suspect_count_disposition",
    "test_sealed",
    "ood_sealed",
)

# ---- DGN-12 追加字段（冻结） ----
EXTRA_PAYLOAD_FIELDS = (
    "dataset_anchor_stride_s",
    "predictor_runtime_period_s",
    "window_geometry",
    "registry_sha256",
    "producer_schema_sha256",
    "aggregation_table_version",
    "target_definition",
)

REQUIRED_PAYLOAD_FIELDS = FR488_PAYLOAD_FIELDS + EXTRA_PAYLOAD_FIELDS

# ---- DGN-12 冻结的 run_metadata 必备槽位（R2 审查 M-1 闭合：非自由字段） ----
REQUIRED_RUN_METADATA_FIELDS = (
    "generation_script_path",     # 生成脚本路径（仓库相对）
    "generation_script_sha256",   # 生成脚本 SHA256
    "matlab_version",             # MATLAB 版本（producer 侧；纯 Python 管线填 producer 记录）
    "python_version",             # Python 版本
    "build_utc",                  # 构建时间
)

NORMALIZATION_SOURCE_FROZEN = "train_only"          # FR-483/488
TEST_SEALED_FROZEN = True                            # FR-487/488
OOD_SEALED_FROZEN = True                             # FR-487/488
DATASET_ANCHOR_STRIDE_S_FROZEN = 10                  # FR-474（不得解释为部署推理周期）
PREDICTOR_RUNTIME_PERIOD_S_FROZEN = "UNDEFINED_IN_D"  # FR-474（D 包不定义）
WINDOW_GEOMETRY_FROZEN = {                           # FR-469/471/472 五元组
    "L_H": 300,        # 历史 300 s
    "H_F": 300,        # 未来 300 s
    "K": 30,           # 30 个 bin
    "delta_F": 10,     # bin 宽 10 s
    "stride": 10,      # anchor stride 10 s
}

__all__ = ["build_manifest", "validate_manifest", "compute_payload_hash",
           "MANIFEST_SCHEMA", "PAYLOAD_HASH_FIELD", "FR488_PAYLOAD_FIELDS",
           "EXTRA_PAYLOAD_FIELDS", "REQUIRED_PAYLOAD_FIELDS",
           "NORMALIZATION_SOURCE_FROZEN", "DATASET_ANCHOR_STRIDE_S_FROZEN",
           "PREDICTOR_RUNTIME_PERIOD_S_FROZEN", "WINDOW_GEOMETRY_FROZEN"]


def compute_payload_hash(payload: dict) -> str:
    """SHA256(canonical_json(payload，排除自身 hash 字段与全部 run_metadata))。

    排除规则（R8）：payload 内的 scientific_payload_sha256（自引用）与 run_metadata
    （若被误嵌）不参与 hash——payload 必须是确定性科学内容，时间戳等运行信息只入
    run_metadata。
    """
    body = {k: v for k, v in payload.items()
            if k != PAYLOAD_HASH_FIELD and k != "run_metadata"}
    return hashlib.sha256(canonical_json(body)).hexdigest()


def build_manifest(payload: dict, run_metadata: dict) -> dict:
    """构建 manifest（fail-fast：payload 缺冻结槽位/冻结值不符 → ValueError）。

    payload：确定性科学内容（FR-488 十五项 + DGN-12 追加七项，槽位名冻结）；
    run_metadata：构建时间戳/wall_time/主机/脚本/版本等运行环境信息（不入 payload hash）。
    """
    if not isinstance(payload, dict):
        raise ValueError("payload 必须为 dict")
    if not isinstance(run_metadata, dict):
        raise ValueError("run_metadata 必须为 dict")

    missing = [f for f in REQUIRED_PAYLOAD_FIELDS if f not in payload]
    if missing:
        raise ValueError(f"payload 缺冻结槽位: {missing}（FR-488 十五项 + DGN-12 追加，fail-fast）")
    missing_meta = [f for f in REQUIRED_RUN_METADATA_FIELDS if f not in run_metadata]
    if missing_meta:
        raise ValueError(f"run_metadata 缺冻结槽位: {missing_meta}"
                         f"（DGN-12：脚本路径+SHA/版本/构建时间，fail-fast）")

    if payload["normalization_source"] != NORMALIZATION_SOURCE_FROZEN:
        raise ValueError(f"normalization_source 必须为 {NORMALIZATION_SOURCE_FROZEN!r}（FR-483/488），"
                         f"实际 {payload['normalization_source']!r}")
    if payload["test_sealed"] is not TEST_SEALED_FROZEN:
        raise ValueError(f"test_sealed 必须为 True（FR-487/488），实际 {payload['test_sealed']!r}")
    if payload["ood_sealed"] is not OOD_SEALED_FROZEN:
        raise ValueError(f"ood_sealed 必须为 True（FR-487/488），实际 {payload['ood_sealed']!r}")
    if payload["dataset_anchor_stride_s"] != DATASET_ANCHOR_STRIDE_S_FROZEN:
        raise ValueError(f"dataset_anchor_stride_s 必须为 {DATASET_ANCHOR_STRIDE_S_FROZEN}（FR-474），"
                         f"实际 {payload['dataset_anchor_stride_s']!r}")
    if payload["predictor_runtime_period_s"] != PREDICTOR_RUNTIME_PERIOD_S_FROZEN:
        raise ValueError(f"predictor_runtime_period_s 必须为 {PREDICTOR_RUNTIME_PERIOD_S_FROZEN!r}（FR-474），"
                         f"实际 {payload['predictor_runtime_period_s']!r}")
    if payload["window_geometry"] != WINDOW_GEOMETRY_FROZEN:
        raise ValueError(f"window_geometry 必须为 {WINDOW_GEOMETRY_FROZEN}（FR-469/471/472），"
                         f"实际 {payload['window_geometry']!r}")

    return {
        "schema": MANIFEST_SCHEMA,
        "payload": dict(payload),                      # 浅拷贝：不 mutate 调用方输入
        "run_metadata": dict(run_metadata),
        PAYLOAD_HASH_FIELD: compute_payload_hash(payload),
    }


def validate_manifest(m: dict) -> list:
    """manifest 校验，返回违规清单（空 = PASS）。

    检查：schema；payload/run_metadata 存在；冻结槽位齐全（十五项 + 追加七项）；
    normalization_source=train_only；sealed 双 True；stride/runtime 分列且值冻结；
    window_geometry 五元组冻结值；scientific_payload_sha256 与 payload 自洽
    （即 hash 与 run_metadata 无关的确定性可被此检查锚定）。
    """
    errs: list = []

    def err(msg):
        errs.append(msg)

    if not isinstance(m, dict):
        return ["manifest 必须为 dict"]
    if m.get("schema") != MANIFEST_SCHEMA:
        err(f"schema 必须为 {MANIFEST_SCHEMA}，实际 {m.get('schema')!r}")
    payload = m.get("payload")
    if not isinstance(payload, dict):
        err("payload 必须为 dict")
        payload = {}
    if not isinstance(m.get("run_metadata"), dict):
        err("run_metadata 必须为 dict（与 payload 分离，R8）")
    else:
        miss_meta = [f for f in REQUIRED_RUN_METADATA_FIELDS if f not in m["run_metadata"]]
        if miss_meta:
            err(f"run_metadata 缺冻结槽位: {miss_meta}（DGN-12）")

    missing = [f for f in FR488_PAYLOAD_FIELDS if f not in payload]
    if missing:
        err(f"payload 缺 FR-488 字段: {missing}")
    missing_extra = [f for f in EXTRA_PAYLOAD_FIELDS if f not in payload]
    if missing_extra:
        err(f"payload 缺 DGN-12 追加字段: {missing_extra}")

    if "normalization_source" in payload and payload["normalization_source"] != NORMALIZATION_SOURCE_FROZEN:
        err(f"normalization_source 必须为 {NORMALIZATION_SOURCE_FROZEN!r}，实际 {payload['normalization_source']!r}")
    if "test_sealed" in payload and payload["test_sealed"] is not TEST_SEALED_FROZEN:
        err(f"test_sealed 必须为 True（sealed terminal evaluation，FR-487），实际 {payload['test_sealed']!r}")
    if "ood_sealed" in payload and payload["ood_sealed"] is not OOD_SEALED_FROZEN:
        err(f"ood_sealed 必须为 True（sealed terminal evaluation，FR-487），实际 {payload['ood_sealed']!r}")

    # stride / runtime 分列（FR-474：stride 不得被解释为部署推理周期）
    if "dataset_anchor_stride_s" in payload and payload["dataset_anchor_stride_s"] != DATASET_ANCHOR_STRIDE_S_FROZEN:
        err(f"dataset_anchor_stride_s 必须为 {DATASET_ANCHOR_STRIDE_S_FROZEN}，实际 {payload['dataset_anchor_stride_s']!r}")
    if "predictor_runtime_period_s" in payload \
            and payload["predictor_runtime_period_s"] != PREDICTOR_RUNTIME_PERIOD_S_FROZEN:
        err(f"predictor_runtime_period_s 必须为 {PREDICTOR_RUNTIME_PERIOD_S_FROZEN!r}，"
            f"实际 {payload['predictor_runtime_period_s']!r}")

    if "window_geometry" in payload and payload["window_geometry"] != WINDOW_GEOMETRY_FROZEN:
        err(f"window_geometry 与冻结五元组不一致: {payload['window_geometry']!r} != {WINDOW_GEOMETRY_FROZEN}")

    got = m.get(PAYLOAD_HASH_FIELD)
    if got is None:
        err(f"缺 {PAYLOAD_HASH_FIELD}")
    elif got != compute_payload_hash(payload):
        err(f"{PAYLOAD_HASH_FIELD} 不匹配: manifest={got} 计算={compute_payload_hash(payload)}")
    return errs

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""freeze_manifest.py — T-020 formal freeze：sealed manifest 构建与复核.

在 manifest.py（通用构建 + deterministic payload hash，T-014/DGN-12/R8）之上
叠加 formal 阶段语义强化（FR-487/488 + DGN-12）。hash 与冻结槽位校验完全
复用 manifest.py，本模块不重复实现任何 hash/字段逻辑。

formal 强化语义（相对 pilot 的 build_manifest 直调）：
  1. payload 必含 FORMAL_REQUIRED_PAYLOAD_KEYS 全集（缺 → ValueError 列出缺失键）；
  2. test_sealed / ood_sealed 必须严格布尔 True（`is True`）：sealed 状态只能
     来自调用方显式传参，本模块不提供任何默认置 True 的路径；
  3. payload["stage"] 必须为 "FORMAL"（dryrun 场景由调用方不调用本模块）；
  4. OOD definition version / OOD generation seed 的事实源是 activation record
     JSON 的 formal_fleet.ood 节点（docs/phase4/d_dataset_v3/
     t432_activation_record.json；definition_version="DGN-11-v1"），本模块从
     传入的 activation_json 路径读取并与 payload 交叉核对，禁 hard-code。

assert_sealed(manifest) 对已构建 manifest 复核 sealed 状态，返回错误清单
（空 = PASS）：sealed 双严格 True；payload hash 存在且为 64 位十六进制；
用 manifest.compute_payload_hash 重算锚定 payload hash，并验证 run_metadata
不在 hash 覆盖范围内（payload/run_metadata 分离仍成立，确定性可复验）。
"""

from __future__ import annotations

import json

from .manifest import (EXTRA_PAYLOAD_FIELDS, FR488_PAYLOAD_FIELDS,
                       PAYLOAD_HASH_FIELD, build_manifest,
                       compute_payload_hash)

FORMAL_STAGE = "FORMAL"
STAGE_PAYLOAD_KEY = "stage"

# formal 必填 payload 键全集：FR-488 十五项 + DGN-12 追加 + stage。
# （run_metadata 槽位——脚本路径+SHA / MATLAB、Python 版本 / 构建时间——由
#   manifest.build_manifest 的 REQUIRED_RUN_METADATA_FIELDS 强制，不在 payload。）
FORMAL_REQUIRED_PAYLOAD_KEYS: frozenset = (
    frozenset(FR488_PAYLOAD_FIELDS) | frozenset(EXTRA_PAYLOAD_FIELDS)
    | {STAGE_PAYLOAD_KEY}
)

__all__ = ["FORMAL_REQUIRED_PAYLOAD_KEYS", "FORMAL_STAGE",
           "STAGE_PAYLOAD_KEY", "build_formal_manifest", "assert_sealed"]


def build_formal_manifest(payload: dict, run_metadata: dict,
                          activation_json: str) -> dict:
    """构建 formal sealed manifest（fail-fast；hash/冻结值校验复用 manifest.py）。

    payload：确定性科学内容（FR-488 十五项 + DGN-12 追加 + stage="FORMAL"）；
    run_metadata：构建时间戳/版本/脚本路径+SHA 等运行信息（不入 payload hash）；
    activation_json：activation record JSON 路径（OOD definition version 与
    generation seed 事实源，读 formal_fleet.ood.definition_version /
    .seed_rule 与 payload 交叉核对）。
    """
    if not isinstance(payload, dict):
        raise ValueError("payload 必须为 dict")
    if not isinstance(run_metadata, dict):
        raise ValueError("run_metadata 必须为 dict")

    # 1) 必填键全集（缺 → ValueError，列出缺失键）
    missing = sorted(FORMAL_REQUIRED_PAYLOAD_KEYS - set(payload))
    if missing:
        raise ValueError(f"formal payload 缺必填键: {missing}"
                         f"（FR-488 十五项 + DGN-12 追加 + stage，fail-fast）")

    # 2) sealed 双严格布尔 True（只来自调用方显式传参，无默认置 True 路径）
    if payload["test_sealed"] is not True:
        raise ValueError(f"formal payload test_sealed 必须严格为布尔 True"
                         f"（FR-487/488 sealed terminal evaluation），"
                         f"实际 {payload['test_sealed']!r}")
    if payload["ood_sealed"] is not True:
        raise ValueError(f"formal payload ood_sealed 必须严格为布尔 True"
                         f"（FR-487/488 sealed terminal evaluation），"
                         f"实际 {payload['ood_sealed']!r}")

    # 3) stage 必须为 FORMAL（dryrun 由调用方不调用本模块）
    if payload[STAGE_PAYLOAD_KEY] != FORMAL_STAGE:
        raise ValueError(f"formal payload stage 必须为 {FORMAL_STAGE!r}，"
                         f"实际 {payload[STAGE_PAYLOAD_KEY]!r}")

    # 4) OOD 定义事实源交叉核对（activation record，禁 hard-code）
    with open(activation_json, encoding="utf-8") as f:
        activation = json.load(f)
    try:
        ood_node = activation["formal_fleet"]["ood"]
        act_version = ood_node["definition_version"]
        act_seed_rule = ood_node["seed_rule"]
    except (KeyError, TypeError) as exc:
        raise ValueError(
            f"activation record 缺 formal_fleet.ood.definition_version / "
            f"seed_rule 节点（事实源结构不符）: {activation_json}") from exc
    if payload["ood_definition_version"] != act_version:
        raise ValueError(f"ood_definition_version 与 activation record 不一致:"
                         f" payload={payload['ood_definition_version']!r} "
                         f"activation={act_version!r}（事实源 "
                         f"formal_fleet.ood.definition_version）")
    if payload["ood_generation_seed"] != act_seed_rule:
        raise ValueError(f"ood_generation_seed 与 activation record 不一致: "
                         f"payload={payload['ood_generation_seed']!r} "
                         f"activation.seed_rule={act_seed_rule!r}")

    # 5) 委托 manifest.build_manifest（冻结槽位/冻结值/payload hash 全部复用）
    return build_manifest(payload, run_metadata)


def assert_sealed(manifest: dict) -> list[str]:
    """对已构建 manifest 复核 sealed 状态，返回错误清单（空 = PASS）。

    检查：test_sealed / ood_sealed 严格布尔 True；payload hash 存在且为
    64 位十六进制；payload hash 与 manifest.compute_payload_hash 重算一致，
    且把 run_metadata 嵌入重算输入后 hash 不变（run_metadata 不在覆盖范围内
    ——若 manifest.py 的排除机制失效此检查即报错）。
    """
    errs: list[str] = []

    if not isinstance(manifest, dict):
        return ["manifest 必须为 dict"]
    payload = manifest.get("payload")
    if not isinstance(payload, dict):
        errs.append("payload 必须为 dict")
        payload = {}
    run_metadata = manifest.get("run_metadata")
    if not isinstance(run_metadata, dict):
        errs.append("run_metadata 必须为 dict（与 payload 分离，R8）")
        run_metadata = {}

    # sealed 双严格 True
    if payload.get("test_sealed") is not True:
        errs.append(f"test_sealed 必须严格为布尔 True（FR-487/488），"
                    f"实际 {payload.get('test_sealed')!r}")
    if payload.get("ood_sealed") is not True:
        errs.append(f"ood_sealed 必须严格为布尔 True（FR-487/488），"
                    f"实际 {payload.get('ood_sealed')!r}")

    # payload hash 存在且为 64 位十六进制
    stored = manifest.get(PAYLOAD_HASH_FIELD)
    if not isinstance(stored, str):
        errs.append(f"缺或非字符串 {PAYLOAD_HASH_FIELD}: {stored!r}")
        stored = None
    elif len(stored) != 64 or any(c not in "0123456789abcdefABCDEF" for c in stored):
        errs.append(f"{PAYLOAD_HASH_FIELD} 必须为 64 位十六进制，"
                    f"实际 {stored!r}")
        stored = None

    # 重算锚定：stored 必须等于 compute_payload_hash(payload)（该机制按定义
    # 排除 run_metadata 与自引用 hash 字段）
    recomputed = compute_payload_hash(payload)
    if stored is not None and stored != recomputed:
        errs.append(f"{PAYLOAD_HASH_FIELD} 与重算值不一致: manifest={stored} "
                    f"重算={recomputed}（payload 被篡改或 hash 未按"
                    f" manifest.compute_payload_hash 机制生成）")

    # run_metadata 不在覆盖范围内：嵌入 run_metadata 重算必须与 stored 一致
    # （compute_payload_hash 的排除机制仍生效；机制一旦漂移此处即报错）
    with_meta = dict(payload)
    with_meta["run_metadata"] = run_metadata
    if compute_payload_hash(with_meta) != recomputed:
        errs.append("run_metadata 泄漏进 payload hash 覆盖范围"
                    "（manifest.compute_payload_hash 的排除机制失效）")

    return errs

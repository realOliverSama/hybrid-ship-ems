# -*- coding: utf-8 -*-
"""registry.py — config 自哈希 + trial/run registry（T-005，DGN-E-16(2,3)）。

- config 哈希配方 = canonical JSON（sort_keys + 紧凑分隔符 + ensure_ascii=False）
  SHA256、排除自引用 hash 字段——与 dataset_v3.validate_contracts 同配方（DGN-E-16）；
- trial registry：append-only jsonl（hpo/<arch>_trials.jsonl），行 schema =
  DGN-E-16(2)：{trial_id, arch, config_sha256, seed_index, status, val:{...},
  duration_s, peak_mem_bytes?, env_manifest_sha256, git_commit, error?}；
  禁删（失败/低分 trial 全留账，FR-595）；
- budget 记账（R5）：已开始 result-bearing 训练的 trial 即使失败也耗预算位；
  纯基础设施中断可用同一 trial_id 重跑 + incident 记录。
"""
from __future__ import annotations

import hashlib
import json
import os
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
HPO_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "hpo")


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def config_hash(obj: dict, key: str = "config_sha256") -> str:
    """config 自哈希（canonical JSON 排除自引用字段）。"""
    body = {k: v for k, v in obj.items() if k != key}
    return hashlib.sha256(canonical_json(body)).hexdigest()


def utcnow_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def load_config_with_hash(path: str) -> tuple:
    """读 config JSON + 返回 (config, config_sha256)（排除 artifact_sha256 字段）。"""
    with open(path, encoding="utf-8") as fh:
        cfg = json.load(fh)
    return cfg, config_hash(cfg, "artifact_sha256")


class TrialRegistry:
    """append-only jsonl trial registry（DGN-E-16(2)；R5 预算记账）。"""

    def __init__(self, arch: str, hpo_dir: str | None = None):
        self.arch = arch
        self.hpo_dir = hpo_dir or HPO_DIR
        os.makedirs(self.hpo_dir, exist_ok=True)
        self.path = os.path.join(self.hpo_dir, f"{arch}_trials.jsonl")

    def append(self, trial: dict) -> None:
        """追加一行（禁删/禁改——只许追加）。"""
        required = ("trial_id", "arch", "config_sha256", "seed_index", "status")
        missing = [k for k in required if k not in trial]
        if missing:
            raise ValueError(f"trial 行缺必需字段 {missing}")
        if trial["status"] not in ("COMPLETED_ELIGIBLE", "COMPLETED_INELIGIBLE",
                                   "FAILED", "INFRA_INTERRUPTED"):
            raise ValueError(f"非法 trial status: {trial['status']}")
        with open(self.path, "a", encoding="utf-8") as fh:
            fh.write(json.dumps(trial, ensure_ascii=False,
                                sort_keys=True) + "\n")

    def rows(self) -> list:
        if not os.path.isfile(self.path):
            return []
        with open(self.path, encoding="utf-8") as fh:
            return [json.loads(line) for line in fh if line.strip()]

    def budget_consumed(self) -> int:
        """已消耗预算位（按唯一 trial_id）：COMPLETED_* / FAILED 耗位；
        INFRA_INTERRUPTED 不占；同一 trial_id 的重跑行不重复计数
        （DGN-E-08 R5：纯基础设施中断同 trial_id 重跑不产生新配置机会）。"""
        consumed = set()
        for r in self.rows():
            if r["status"] in ("COMPLETED_ELIGIBLE", "COMPLETED_INELIGIBLE",
                               "FAILED"):
                consumed.add(r["trial_id"])
        return len(consumed)

    def eligible_trials(self) -> list:
        """COMPLETED_ELIGIBLE = 有效有限 val 证据 + 有效冻结 checkpoint 的 trial
        （tasks gate R3：只有这类可成为 selected_config）。"""
        out = []
        for r in self.rows():
            if r["status"] != "COMPLETED_ELIGIBLE":
                continue
            val = r.get("val") or {}
            pb = val.get("pinball_sgmacro")
            if pb is None or not _is_finite(pb):
                continue
            out.append(r)
        return out


def _is_finite(x) -> bool:
    import math
    try:
        return math.isfinite(float(x))
    except (TypeError, ValueError):
        return False


def write_json_with_self_hash(obj: dict, path: str,
                              key: str = "artifact_sha256") -> str:
    """写 JSON + 自哈希字段（DGN-E-16 统一约定）；返回 artifact_sha256。"""
    body = {k: v for k, v in obj.items() if k != key}
    h = hashlib.sha256(canonical_json(body)).hexdigest()
    out = dict(obj)
    out[key] = h
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(out, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    return h

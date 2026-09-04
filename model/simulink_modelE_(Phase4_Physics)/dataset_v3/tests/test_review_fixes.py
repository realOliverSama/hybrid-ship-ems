#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_review_fixes.py — 交叉审查（R0/R1/R2）修复项回归测试（T-013 配套）.

覆盖：
- R1-MINOR-1：bin 掩码 1e-9 容差（fp 噪声不改变 bin 归属，右闭语义保持）；
- R2-M-1：manifest run_metadata 冻结槽位强制（build fail-fast + validate 报错）；
- R2-m-1：verify_partition 对缺 artifact_sha256 必须报错；
- R2-m-2/m-3：run_l3 provenance 接线 + formal require_train_stats fail-fast；
- R0-M-1：validate_contracts 写 hash 守卫（坏内容拒绝写入）。
"""

import copy
import json
import os
import sys

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from dataset_v3.aggregate import aggregate_bin
from dataset_v3.audit_leakage import run_l3
from dataset_v3.manifest import (REQUIRED_RUN_METADATA_FIELDS, build_manifest,
                                 validate_manifest)
from dataset_v3.split import assign_splits, verify_partition


# ---- R1-MINOR-1：fp 容差掩码 ----
class TestBinMaskTolerance:
    def test_right_edge_fp_noise_stays_in_bin(self):
        # 样本意图在 b=3.0，fp 噪声 +4.4e-16 → 容差内仍属 (2, 3]
        t = np.array([0.0, 1.0, 2.0, 3.0 + 4.4e-16, 4.0])
        v = np.array([1.0, 2.0, 3.0, 9.0, 4.0])
        assert aggregate_bin(t, v, 2.0, 3.0, "interval_endpoint") == 9.0

    def test_left_edge_fp_noise_excluded(self):
        # 样本意图在 a=2.0，fp 噪声 +ε → 容差内仍属左侧 bin（右闭：a 点排除）
        t = np.array([0.0, 1.0, 2.0 + 4.4e-16, 3.0, 4.0])
        v = np.array([1.0, 2.0, 3.0, 9.0, 4.0])
        assert aggregate_bin(t, v, 2.0, 3.0, "interval_endpoint") == 9.0

    def test_exact_boundary_semantics_preserved(self):
        t = np.array([0.0, 1.0, 2.0, 3.0, 4.0])
        v = np.array([1.0, 2.0, 3.0, 9.0, 4.0])
        assert aggregate_bin(t, v, 2.0, 3.0, "interval_endpoint") == 9.0

    def test_above_tolerance_is_true_next_bin(self):
        # 噪声超过容差（1e-3）→ 语义按真实数值走（落下一 bin）
        t = np.array([0.0, 1.0, 2.0, 3.0 + 1e-3, 4.0])
        v = np.array([1.0, 2.0, 3.0, 9.0, 4.0])
        with pytest.raises(ValueError):
            aggregate_bin(t, v, 2.0, 3.0, "interval_endpoint")  # bin 内无样本


# ---- R2-M-1：manifest run_metadata 冻结槽位 ----
def _valid_payload():
    from dataset_v3.manifest import REQUIRED_PAYLOAD_FIELDS, WINDOW_GEOMETRY_FROZEN
    p = {f: "x" for f in REQUIRED_PAYLOAD_FIELDS}
    p.update({"normalization_source": "train_only", "test_sealed": True,
              "ood_sealed": True, "dataset_anchor_stride_s": 10,
              "predictor_runtime_period_s": "UNDEFINED_IN_D",
              "window_geometry": dict(WINDOW_GEOMETRY_FROZEN)})
    return p


def _valid_meta():
    return {f: "v" for f in REQUIRED_RUN_METADATA_FIELDS}


class TestManifestRunMetadata:
    def test_build_missing_meta_raises(self):
        meta = _valid_meta()
        meta.pop("build_utc")
        with pytest.raises(ValueError, match="run_metadata 缺冻结槽位"):
            build_manifest(_valid_payload(), meta)

    def test_validate_missing_meta_reports(self):
        man = build_manifest(_valid_payload(), _valid_meta())
        man["run_metadata"].pop("generation_script_sha256")
        assert any("run_metadata 缺冻结槽位" in e for e in validate_manifest(man))

    def test_full_meta_passes(self):
        man = build_manifest(_valid_payload(), _valid_meta())
        assert validate_manifest(man) == []


# ---- R2-m-1：verify_partition 缺 hash 必须报错 ----
class TestVerifyPartitionMissingHash:
    def test_missing_artifact_sha256_reported(self):
        a = assign_splits(["g1", "g2"], {"s": ["g1", "g2"]}, "seed")
        a.pop("artifact_sha256")
        errs = verify_partition(a)
        assert any("artifact_sha256" in e for e in errs)

    def test_present_and_consistent_passes(self):
        a = assign_splits(["g1", "g2"], {"s": ["g1", "g2"]}, "seed")
        assert verify_partition(a) == []


# ---- R2-m-2/m-3：run_l3 provenance + require_train_stats ----
def _l3_series():
    rng = np.random.RandomState(0)
    return {f"v{i}": np.sin(np.linspace(0, 6, 256)) + 0.05 * rng.randn(256) + i * 0.1
            for i in range(1, 7)}


class TestRunL3Wiring:
    def test_provenance_joined_into_suspects(self):
        series = {k: v for k, v in _l3_series().items()}
        series["v1"] = series["v2"] * 1.0 + 1e-9      # 构造强相似对
        split_of = {"v1": "train", "v2": "test", "v3": "train", "v4": "val",
                    "v5": "test", "v6": "train"}
        prov = {k: {"generation_seed": "7"} for k in series}
        res = run_l3(series, split_of, provenance=prov)
        for s in res["suspects"]:
            assert set(s["provenance"]) == set(s["pair"])
            assert s["provenance"][s["pair"][0]] == {"generation_seed": "7"}

    def test_require_train_stats_fails_fast(self):
        series = _l3_series()
        split_of = {k: "train" for k in series}
        with pytest.raises(ValueError, match="train_stats"):
            run_l3(series, split_of, train_stats=None, require_train_stats=True)

    def test_backward_compat_default(self):
        series = _l3_series()
        split_of = {"v1": "train", "v2": "train", "v3": "test", "v4": "val",
                    "v5": "train", "v6": "test"}
        res = run_l3(series, split_of)          # 旧签名仍可用
        assert res["flag_only"] is True


# ---- R0-M-1：坏内容拒绝写 self_hash ----
class TestWriteHashGuard:
    def test_tampered_registry_rejects_write(self, tmp_path, monkeypatch):
        from dataset_v3 import validate_contracts as vc
        reg = vc._load_json(vc.REGISTRY_PATH)
        bad = copy.deepcopy(reg)
        bad["signals"] = [s for s in bad["signals"] if s.get("role") != "INPUT_H2"]
        p = tmp_path / "reg.json"
        p.write_text(json.dumps(bad), encoding="utf-8")
        monkeypatch.setattr(vc, "REGISTRY_PATH", str(p))
        rc = vc.main(["--registry", "--write-hash"])
        assert rc == 1
        after = json.loads(p.read_text(encoding="utf-8"))
        assert after["self_hash"]["value"] == bad["self_hash"]["value"]  # 未被改写

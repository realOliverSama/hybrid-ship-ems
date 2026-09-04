#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_negative.py — T-013 负向测试套件（FR-464/484/500、AC-427；DGN-10/13 + R6/R8）.

构造缺陷必须全部被检出（检出率 100%——AC-427），每缺陷独立断言：
  N1  L1-① voyage/split_group 跨 split；
  N2  L1-② raw artifact hash 重复；
  N3  L1-③ 窗口 exact duplicate（content hash 全载荷覆盖）；
  N4  forecast provenance `source_cutoff_time_s > issue_time_s`；
  N5  forbidden forecast provenance class（ORACLE_DERIVED）；
  N6  FORBIDDEN/H3 列混入 model view；
  N7  PILOT_ONLY 进 formal 路径（provenance class / fixture_kind 门）;
  N8  split 同 group 多 stratum 映射 fail-fast。
"""

import json
import os
import sys

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from dataset_v3.audit_leakage import run_l1
from dataset_v3.build_dataset_v3 import build_views
from dataset_v3.contracts import load_registry
from dataset_v3.split import assign_splits
from dataset_v3.validate_contracts import validate_forecast_provenance


# ---- 公共构造 ----
def _mk_series(n=64, jitter=0.0, seed=0):
    rng = np.random.RandomState(seed)
    t = np.arange(n, dtype=float)
    v = 300.0 + 20.0 * np.sin(t / 8.0) + jitter * rng.randn(n)
    return v


def _mk_windows(seed=0):
    """3 个窗口的合成数据（列名按 registry 白名单子集——审计不依赖列集合大小）。"""
    rng = np.random.RandomState(seed)
    reg = load_registry()
    h1 = [s["name"] for s in reg["signals"] if s["role"] == "INPUT_H1"]
    kf = [s["name"] for s in reg["signals"] if s["role"] in ("INPUT_K", "INPUT_F")]
    x_h = {c: rng.randn(300) + 300.0 for c in h1}
    x_kf = {c: rng.randn(30) for c in kf}
    y = rng.rand(30) * 10 + 100.0
    return x_h, x_kf, y


def _window_hashes(windows):
    """windows: [(x_h_dict, x_kf_dict, y), ...] → 列按 dict 序堆叠为 2D 后哈希
    （window_content_hash 的调用侧契约：逐列固定列序由调用方完成）。"""
    from dataset_v3.audit_leakage import window_content_hash

    def stack(cols):
        names = sorted(cols)
        return np.stack([cols[c] for c in names], axis=1)

    return {f"w{i}": window_content_hash(stack(xh), stack(xkf), y)
            for i, (xh, xkf, y) in enumerate(windows)}


# ---- N1：L1-① identity 跨 split ----
def test_n1_identity_across_splits_detected():
    violations = []
    split_of = {"vA#self": "train", "vA#dup": "test"}   # 同一 voyage 两记录分属两 split
    res = run_l1(split_of, {"vA": "sha_a", "vB": "sha_b"},
                 {"w0": "h0", "w1": "h1", "w2": "h2"})
    violations = res["violations"]
    assert res["overall_l1"] is False
    assert any(v.get("type") == "identity_cross_split" for v in violations)


# ---- N2：L1-② raw artifact hash 重复 ----
def test_n2_duplicate_artifact_hash_detected():
    split_of = {"vA#self": "train", "vB#self": "test"}
    res = run_l1(split_of, {"vA": "sha_same", "vB": "sha_same"},
                 {"w0": "h0", "w1": "h1"})
    assert res["overall_l1"] is False
    assert any(v.get("type") == "duplicate_artifact_sha256" for v in res["violations"])


# ---- N3：L1-③ 窗口 exact duplicate（全载荷 content hash）----
def test_n3_exact_duplicate_window_detected():
    xh, xkf, y = _mk_windows(seed=1)
    xh2, xkf2, y2 = _mk_windows(seed=1)          # 逐位复制（不同 voyage 名）
    xh3, xkf3, y3 = _mk_windows(seed=2)          # 唯一窗口
    hashes = _window_hashes([(xh, xkf, y), (xh2, xkf2, y2), (xh3, xkf3, y3)])
    # 内容相同 → hash 相同（键不同不影响 content hash：metadata 不参与）
    split_of = {"vA#self": "train", "vB#self": "test", "vC#self": "val"}
    res = run_l1(split_of, {"vA": "sha_a", "vB": "sha_b", "vC": "sha_c"}, hashes)
    assert res["overall_l1"] is False
    dup = [v for v in res["violations"] if v.get("type") == "duplicate_window_content"]
    assert dup and set(dup[0]["windows"]) >= {"w0", "w1"}


def test_n3b_metadata_only_change_not_flagged():
    # 仅 metadata 身份不同（这里由键名体现）、内容相同 → 仍算 exact duplicate
    # （L1 content hash 有意排除 metadata：复制窗口换 id 也必须被看见）
    xh, xkf, y = _mk_windows(seed=3)
    xh2, xkf2, y2 = _mk_windows(seed=3)
    hashes = _window_hashes([(xh, xkf, y), (xh2, xkf2, y2)])
    res = run_l1({"vA#self": "train", "vB#self": "test"},
                 {"vA": "sha_a", "vB": "sha_b"}, hashes)
    assert res["overall_l1"] is False


# ---- N4：source_cutoff > issue_time 必须拒收 ----
def test_n4_cutoff_after_issue_rejected():
    rec = {"forecast_generator_id": "g", "forecast_generator_version": "1",
           "forecast_generator_hash": "h",
           "issue_time_s": 300.0, "source_cutoff_time_s": 300.5,
           "forecast_provenance_class": "CAUSAL_GENERATOR"}
    errs = validate_forecast_provenance(rec, stage="formal")
    assert any("不变量违反" in e for e in errs)


# ---- N5：forbidden provenance class（ORACLE_DERIVED）永拒收 ----
def test_n5_oracle_derived_forecast_rejected():
    rec = {"forecast_generator_id": "g", "forecast_generator_version": "1",
           "forecast_generator_hash": "h",
           "issue_time_s": 300.0, "source_cutoff_time_s": 300.0,
           "forecast_provenance_class": "ORACLE_DERIVED"}
    assert any("拒收" in e for e in validate_forecast_provenance(rec, stage="pilot"))
    assert any("拒收" in e for e in validate_forecast_provenance(rec, stage="formal"))


def test_n5b_fixture_stub_in_formal_rejected():
    rec = {"forecast_generator_id": "g", "forecast_generator_version": "1",
           "forecast_generator_hash": "h",
           "issue_time_s": 300.0, "source_cutoff_time_s": 300.0,
           "forecast_provenance_class": "FIXTURE_CAUSAL_STUB"}
    assert validate_forecast_provenance(rec, stage="pilot") == []
    assert validate_forecast_provenance(rec, stage="formal")   # formal 拒收


# ---- N6：FORBIDDEN/H3 列混入 model view ----
def test_n6_forbidden_column_into_view_rejected():
    reg = load_registry()
    xh, xkf, y = _mk_windows(seed=4)
    xh_bad = dict(xh)
    xh_bad["P_commit_kW"] = np.zeros(300)            # H3→FORBIDDEN 列
    with pytest.raises(ValueError):
        build_views(xh_bad, xkf, np.stack([y]), np.array([5.0]), reg)


def test_n6b_metadata_column_into_view_rejected():
    reg = load_registry()
    xh, xkf, y = _mk_windows(seed=5)
    xkf_bad = dict(xkf)
    xkf_bad["resistance_multiplier"] = np.zeros(30)  # H3→METADATA 列
    with pytest.raises(ValueError):
        build_views(xh, xkf_bad, np.stack([y]), np.array([5.0]), reg)


# ---- N7：PILOT_ONLY 进 formal 路径 ----
def test_n7_pilot_only_fixture_in_formal_rejected():
    rec = {"forecast_generator_id": "pilot_persistence_fixture",
           "forecast_generator_version": "1", "forecast_generator_hash": "h",
           "issue_time_s": 300.0, "source_cutoff_time_s": 300.0,
           "forecast_provenance_class": "FIXTURE_CAUSAL_STUB"}
    errs = validate_forecast_provenance(rec, stage="formal")
    assert errs, "PILOT fixture provenance 必须被 formal 门拒收（FR-500）"


# ---- N8：split 同 group 多 stratum 映射 fail-fast ----
def test_n8_multi_stratum_mapping_fail_fast():
    with pytest.raises(ValueError):
        assign_splits(["g1"], {"sA": ["g1"], "sB": ["g1"]}, "seed")

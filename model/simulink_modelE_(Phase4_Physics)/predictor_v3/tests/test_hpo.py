# -*- coding: utf-8 -*-
"""HPO 引擎单测（DGN-E-08 七步算法；合成注册架构，不触正式 trial list）。"""
import json
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import hpo  # noqa: E402
from predictor_v3 import registry as reg  # noqa: E402


@pytest.fixture()
def fake_arch(monkeypatch, tmp_path):
    """合成架构：3 参数网格 × 显式参数量（绕过 TF build——测试清单算法本身）。"""
    def build(cfg, s0):
        raise RuntimeError("测试中不构建真实模型")

    class FakeMod:
        MODEL_ID = "fakearch"
        ARCH_ID = "fakearch"
        FEATURE_SURFACE = "H1"
        ENVELOPE = (0, 1000)

    fake = {"build": lambda cfg, s0: None, "search_space": {
        "a": [1, 2, 3], "lr": [1e-4, 1e-3]},
        "feature_surface": "H1", "model_id": "fakearch", "envelope": (0, 1000)}
    monkeypatch.setitem(hpo.ARCH_BUILDERS, "fakearch", fake)
    monkeypatch.setattr(hpo, "count_params_for",
                        lambda arch, cfg, s0: cfg["a"] * 100)
    # 参数量函数：a=1→~1001?, 让 a∈{1,2,3} 且 lr∈{1e-4,1e-3} 产生混合合法/非法
    # envelope (0,1000)：a=1 → 100 + 0.1/1 → 合法；a=2 → 200 合法；a=3 → 300 合法
    monkeypatch.setattr(hpo, "HPO_DIR", str(tmp_path))
    return tmp_path


def test_seven_step_generation_and_freeze(fake_arch):
    doc, audit = hpo.generate_trial_list("fakearch", budget_cap=4, s0=1.0)
    assert audit["n_enumerated"] == 6 and audit["n_rejected"] == 0
    assert doc["n_trials"] == 4 and doc["budget_cap"] == 4
    assert len(doc["trials"]) == 4
    # 每 trial = 完整 config + 自哈希 + trial_id（⑦ 冻结件）
    for t in doc["trials"]:
        assert t["config_sha256"] == reg.config_hash({**t["config"], "seed_index": 0})
        assert t["trial_id"].startswith("fakearch-t")
    # 冻结落盘
    p = fake_arch / "fakearch_trial_list.json"
    assert p.exists() and (fake_arch / "fakearch_trial_generation_audit.json").exists()
    # ⑦ 确定性：同种子重生一致的清单
    doc2, _ = hpo.generate_trial_list("fakearch", budget_cap=4, s0=0)
    assert [t["trial_id"] for t in doc["trials"]] == \
        [t["trial_id"] for t in doc2["trials"]]


def test_capacity_filter_and_audit(fake_arch, monkeypatch):
    """③④：超信封配置拒绝并记录 trial_generation_audit。"""
    monkeypatch.setattr(hpo, "count_params_for",
                        lambda arch, cfg, s0: cfg["a"] * 1000)  # a=1 →1000 边界内；2,3 超界
    doc, audit = hpo.generate_trial_list("fakearch", budget_cap=10, s0=1.0)
    assert audit["n_rejected"] == 4            # a∈{2,3} × lr∈{2} 超界
    assert audit["n_capacity_valid"] == 2      # a=1 × lr∈{2}
    assert audit["rejected"][0]["reason"].startswith("count_params")
    assert all(t["config"]["a"] == 1 for t in doc["trials"])


def test_budget_subsampling_deterministic(fake_arch):
    """⑥ 预算抽样：池 > 预算 → 均匀无放回抽样；同种子可复现。"""
    d1, _ = hpo.generate_trial_list("fakearch", budget_cap=5, s0=1.0)
    d2, _ = hpo.generate_trial_list("fakearch", budget_cap=5, s0=1.0)
    ids1 = sorted(t["config_sha256"] for t in d1["trials"])
    ids2 = sorted(t["config_sha256"] for t in d2["trials"])
    assert ids1 == ids2 and len(ids1) == 5


def test_registry_budget_accounting(tmp_path):
    """R5 记账：FAILED/COMPLETED 耗位；INFRA_INTERRUPTED 不耗；eligible 过滤。"""
    r = reg.TrialRegistry("fakearch2", hpo_dir=str(tmp_path))
    rows = [
        {"trial_id": "t1", "arch": "x", "config_sha256": "a", "seed_index": 0,
         "status": "COMPLETED_ELIGIBLE",
         "val": {"pinball_sgmacro": 10.0}},
        {"trial_id": "t2", "arch": "x", "config_sha256": "b", "seed_index": 0,
         "status": "FAILED", "error": "OOM"},
        {"trial_id": "t3", "arch": "x", "config_sha256": "c", "seed_index": 0,
         "status": "INFRA_INTERRUPTED", "error": "machine reboot"},
        {"trial_id": "t4", "arch": "x", "config_sha256": "d", "seed_index": 0,
         "status": "COMPLETED_INELIGIBLE",
         "val": {"pinball_sgmacro": float("nan")}},
    ]
    for row in rows:
        r.append(row)
    # R5：COMPLETED_* 与 FAILED 各耗 1 位；INFRA_INTERRUPTED 不耗（同 trial_id 重跑）
    assert r.budget_consumed() == 3            # t1 + t2 + t4
    el = r.eligible_trials()
    assert len(el) == 1 and el[0]["trial_id"] == "t1"
    with pytest.raises(ValueError):
        r.append({"trial_id": "t5", "arch": "x", "config_sha256": "e",
                  "seed_index": 0, "status": "MAGIC"})


def test_select_config_zero_eligible_stops(tmp_path, monkeypatch):
    """eligible_trial_count==0 ⇒ STOP（R3），不静默选低分/失败 trial。"""
    r = reg.TrialRegistry("fakearch3", hpo_dir=str(tmp_path))
    r.append({"trial_id": "t1", "arch": "x", "config_sha256": "a",
              "seed_index": 0, "status": "FAILED", "error": "NaN"})
    monkeypatch.setattr(hpo, "HPO_DIR", str(tmp_path))
    (tmp_path / "fakearch3_trial_list.json").write_text(json.dumps(
        {"schema": "predictor_v3_hpo_trial_list_v1", "budget_cap": 5,
         "trials": []}))
    with pytest.raises(RuntimeError, match="eligible_trial_count == 0"):
        hpo.select_config("fakearch3", s0=1.0, env_manifest_sha256="0" * 64)

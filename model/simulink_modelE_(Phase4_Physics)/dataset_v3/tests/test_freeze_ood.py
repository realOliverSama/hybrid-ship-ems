#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_freeze_ood.py — freeze_ood（T-020 OOD formal freeze 证据模块）单测.

全部使用合成 voyage .mat（scipy.io.savemat 构造 stream/kf/meta 三表）+
临时目录，不读真实 formal 语料目录。期望 allocation 由 activation JSON
（合成或真实 t432_activation_record.json）的 counts 派生
（len(conditions) × per_condition_voyages）；合成 activation 刻意含陈旧
冗余字段 total_voyages=240，以证明期望总数不由其派生。

canonical token 禁手抄（D4）：轴 C 测试的 ood_token_id 一律运行时从
config/mission_v3/heldout_token_registry.json 加载。
"""

import json
import os

import numpy as np
import pytest
from scipy.io import savemat

from dataset_v3.audit_ood import load_heldout_token_registry
from dataset_v3.freeze_ood import build_ood_evidence, load_ood_mat

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))        # tests → dataset_v3 → MODELE
REPO_ROOT = os.path.dirname(os.path.dirname(MODELE_ROOT))   # MODELE → model → repo root
REAL_ACTIVATION_JSON = os.path.join(
    REPO_ROOT, "docs", "phase4", "d_dataset_v3",
    "t432_activation_record.json")

DEF_VERSION = "DGN-11-v1"
PHYSICS_REF = "ModelE validated domain (T-412 v1.1)"


# --------------------------------------------------------------------------
# 合成语料构造
# --------------------------------------------------------------------------
def _write_voyage(out_dir, vid, axis, condition, *, u_sup_abs=1.5,
                  ht_effective=1.7, token_id=None, with_condition=True,
                  with_metric=True, dataset_domain="OOD",
                  def_version=DEF_VERSION):
    """写一个最小 stream/kf/meta 三表 voyage .mat（结构对齐 Stage A 样本）."""
    meta = {
        "voyage_id": vid,
        "ood_axis": axis,
        "ood_def_version": def_version,
        "physics_domain_ref": PHYSICS_REF,
        "dataset_domain": dataset_domain,
        "heldout_token_registry_sha": "0" * 64,
        "ood_token_id": token_id if token_id else [],
        "propulsion_check": {
            "feasible": 1,
            "inputs": {
                "u_sup_abs": float(u_sup_abs) if with_metric else np.nan,
                "ht_effective": float(ht_effective) if with_metric else np.nan,
            },
        },
    }
    if with_condition:
        meta["ood_condition_id"] = condition
    stream = {
        "time_s": np.arange(4, dtype=np.uint16),
        "v_current_mps": np.zeros(4),
        "water_depth_m": np.full(4, 5.0),
        "scenario_id": vid,
    }
    kf = {"issue_time_s": np.arange(2, dtype=np.uint16),
          "contract_status": "OK"}
    savemat(os.path.join(out_dir, vid + ".mat"),
            {"stream": stream, "kf": kf, "meta": meta})


def _write_activation(out_dir, *, per_condition=2,
                      conditions=("A1", "B1", "C1", "C2", "C3")):
    """写合成 activation JSON（结构对齐 formal_fleet.ood）.

    含陈旧冗余字段 total_voyages=240 —— 期望总数必须由
    len(conditions) × per_condition_voyages 派生，该字段必须被忽略。
    """
    act = {"formal_fleet": {"ood": {
        "total_voyages": 240,  # 陈旧冗余字段（amendment 01 前），禁用
        "definition_version": DEF_VERSION,
        "counts": {"per_condition_voyages": per_condition,
                   "conditions": list(conditions)},
        "splitter": "OOD records never enter the IID splitter "
                    "(independent evaluation collection)",
    }}}
    path = os.path.join(out_dir, "activation_synthetic.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(act, f)
    return path


def _require_tokens():
    """registry REQUIRE token（canonical，运行时加载，禁手抄）."""
    return load_heldout_token_registry()


def _write_plan(out_dir, per_condition, conditions=("A1", "B1", "C1", "C2", "C3")):
    """写冻结分配计划（T-020 裁决 C：planned identity 权威 = plan blocks）.

    quota（families/condition）= per_condition（合成语料每 voyage 独立 draw，
    无 member 复用 → families == voyages）。
    """
    plan = {
        "schema": "fleet_v3_draw_allocation_plan_v1",
        "corpus": "stage_b_ood",
        "face": "ood",
        "amendment": "T432_ACTIVATION_AMENDMENT_03",
        "blocks": [{"condition_id": c, "quota": per_condition,
                    "accepted_slots": per_condition, "slots": []}
                   for c in conditions],
    }
    path = os.path.join(out_dir, "formal_draw_allocation_plan_ood.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(plan, f)
    return path


def _build_full_corpus(out_dir, per_condition=2):
    """构造 A1/B1/C1/C2/C3 × per_condition 全量合成语料（全部 verdict=OOD）.

    轴 C 的 condition 与 token 全部由 registry 条目运行时派生
    （token_id 前缀段 = condition id），代码中无手抄 canonical token。
    voyage 文件名/vid 采用 formal 四段 plan linkage 命名
    （FLTB_OOD_<planned>_<draw>_M1；T-020 裁决 C planned identity），
    并落 formal_draw_allocation_plan_ood.json（planned quota 权威）。
    """
    tokens = _require_tokens()
    n = 0
    for i in range(per_condition):
        _write_voyage(out_dir, f"FLTB_OOD_A1_D{i:05d}_M1", "A", "A1", u_sup_abs=2.2)
        _write_voyage(out_dir, f"FLTB_OOD_B1_D1{i:04d}_M1", "B", "B1", ht_effective=1.4)
        n += 2
        for t in tokens:
            cond = str(t["token_id"]).split("_")[0]  # C1/C2/C3，registry 驱动
            _write_voyage(out_dir, f"FLTB_OOD_{cond}_D2{i:04d}_M1", "C", cond,
                          token_id=t["token_id"])
            n += 1
    _write_plan(out_dir, per_condition)
    return n


# --------------------------------------------------------------------------
# 用例
# --------------------------------------------------------------------------
def test_full_allocation_pass(tmp_path):
    """planned allocation 精确 + realized 一致性 → overall_pass=True（裁决 C）."""
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    n = _build_full_corpus(ood_dir, per_condition=2)
    act = _write_activation(str(tmp_path))
    ev = build_ood_evidence(ood_dir, act)
    assert ev["overall_pass"] is True
    assert ev["checks"]["planned_allocation_exact"] is True
    assert ev["checks"]["realized_ledger_meta_agreement"] is True
    # planned gate：逐 condition 精确 10 families / 40 voyages 语义（合成 2/2）
    assert ev["allocation_check"]["total"]["planned"] == n
    assert ev["allocation_check"]["total"]["expected_planned"] == 10
    assert ev["allocation_check"]["total"]["realized"] == n
    # total_voyages=240 陈旧字段被忽略
    assert ev["allocation_check"]["basis"].startswith("T-020 裁决 C")
    for cond, e in ev["allocation_check"]["per_condition"].items():
        assert e["planned_voyages"] == 2 and e["planned_families"] == 2, cond
        assert e["realized_voyages"] == 2 and e["planned_pass"] is True, cond
    # transition matrix：planned→realized 恒等（合成语料无域漂移）
    for pc, row in ev["allocation_check"]["transition_planned_to_realized"].items():
        assert row.get(pc, 0) == 2 and len(row) == 1, (pc, row)
    assert ev["stress_set"]["count"] == 0
    assert ev["stress_set"]["pass"] is True
    # 轴归属：A1→A，B1→B，C*→C
    assert ev["axes"]["A"]["formal_count"] == 2
    assert ev["axes"]["B"]["formal_count"] == 2
    assert ev["axes"]["C"]["formal_count"] == 6
    assert ev["per_condition"]["A1"]["axes_seen"] == ["A"]
    assert ev["per_condition"]["B1"]["metric_stats"]["mean"] == pytest.approx(1.4)
    assert ev["per_condition"]["C1"]["metric_stats"] is None  # composition 无数值统计
    # 固定 keys 齐备
    for k in ("allocation_check", "axes", "stress_set", "per_condition",
              "overall_pass"):
        assert k in ev


@pytest.mark.skipif(not os.path.exists(REAL_ACTIVATION_JSON),
                    reason="真实 t432_activation_record.json 不在仓库")
def test_real_activation_freeze_allocation(tmp_path):
    """真实 activation record：期望 = counts 派生 5 × 40 = 200（非 240）。"""
    with open(REAL_ACTIVATION_JSON, encoding="utf-8") as f:
        counts = json.load(f)["formal_fleet"]["ood"]["counts"]
    per = int(counts["per_condition_voyages"])
    conds = [str(c) for c in counts["conditions"]]
    assert len(conds) * per == 200  # amendment 01 后冻结值

    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=per)
    ev = build_ood_evidence(ood_dir, REAL_ACTIVATION_JSON)
    assert ev["allocation_check"]["total"]["planned"] == 200
    assert ev["allocation_check"]["total"]["expected_planned"] == 200
    assert ev["allocation_check"]["pass"] is True
    assert ev["overall_pass"] is True
    assert sorted(ev["per_condition"]) == sorted(conds)


def test_missing_one_voyage_allocation_fail(tmp_path):
    """某 condition 缺 1 条 → planned allocation FAIL（不足也 FAIL）。"""
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=2)
    os.remove(os.path.join(ood_dir, "FLTB_OOD_A1_D00000_M1.mat"))  # A1 缺 1
    act = _write_activation(str(tmp_path))
    ev = build_ood_evidence(ood_dir, act)
    assert ev["overall_pass"] is False
    assert ev["allocation_check"]["pass"] is False
    assert ev["checks"]["planned_allocation_exact"] is False
    row = ev["allocation_check"]["per_condition"]["A1"]
    assert row["planned_voyages"] == 1 and row["planned_families"] == 1
    assert row["planned_pass"] is False


def test_retired_condition_a2_rejected(tmp_path):
    """出现已退役 condition（amendment 01 退役的轴 A stress band 条件）→ FAIL.

    期望条件清单由 counts.conditions 派生，退役条件不在其中 →
    conditions_universe 拒收 + 排除出正式 counts + overall FAIL。
    """
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=2)
    # 注入退役条件 voyage（轴 A (2.4,2.8] retired band 的 realized 值）
    _write_voyage(ood_dir, "V_RETIRED_0", "A", "A2", u_sup_abs=2.6)
    act = _write_activation(str(tmp_path))
    ev = build_ood_evidence(ood_dir, act)
    assert ev["overall_pass"] is False
    assert ev["checks"]["conditions_universe"] is False
    assert ev["conditions_universe"]["unexpected_condition_ids"] == ["A2"]
    # 拒收记录不进 planned counts：A2 不在 planned gate 范围（overall 经
    # metadata FAIL）；realized 计数如实报告
    assert ev["allocation_check"]["total"]["realized"] == 10
    assert any("A2" in e["reason"] for e in ev["metadata_errors"])
    assert all("A2" != c for c in ev["per_condition"])


def test_stress_set_diverted_not_in_formal_counts(tmp_path):
    """STRESS_SET / UNCLASSIFIED 分流出证，不进正式 per_condition counts.

    FR-486：越物理域 voyage 单独出证；formal 冻结语料出现任一条 →
    domain_purity FAIL（语料必须物理域纯净），即使 allocation 本身满足。
    """
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=2)
    # B1 标签 + realized H/T < 1.2 → PHYSICS_EXTRAPOLATION/STRESS_SET
    _write_voyage(ood_dir, "V_STRESS_0", "B", "B1", ht_effective=1.1)
    # B1 标签 + realized H/T ∈ [1.2, 1.3) → UNCLASSIFIED margin 带
    _write_voyage(ood_dir, "V_MARGIN_0", "B", "B1", ht_effective=1.25)
    act = _write_activation(str(tmp_path))
    ev = build_ood_evidence(ood_dir, act)
    ss = ev["stress_set"]
    assert ss["diverted"] is True
    assert ss["excluded_from_formal_counts"] is True
    assert ss["count"] == 1 and "V_STRESS_0" in ss["voyage_ids"]
    assert ss["by_axis"] == {"B": ["V_STRESS_0"]}
    assert ss["unclassified_count"] == 1 \
        and "V_MARGIN_0" in ss["unclassified_voyage_ids"]
    # 分流生效：正式 per_condition counts 只含 verdict=OOD
    b1 = ev["per_condition"]["B1"]
    assert b1["actual"] == b1["expected"] == 2
    assert "V_STRESS_0" not in b1["voyage_ids"]
    assert "V_MARGIN_0" not in b1["voyage_ids"]
    # 注入的 V_* 文件名非 plan block linkage → planned_extra 非空，
    # planned gate 如实 FAIL（分流/域纯净断言见上与 domain_purity）
    assert ev["allocation_check"]["pass"] is False
    assert ev["checks"]["domain_purity_fr486"] is False
    # 正式语料混入越域样本 → purity FAIL → overall FAIL
    assert ev["checks"]["domain_purity_fr486"] is False
    assert ev["overall_pass"] is False


def test_splitter_declaration_false(tmp_path):
    """FR-485：OOD 绝不进入 IID splitter —— 常量声明字段为 False。"""
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=1)
    act = _write_activation(str(tmp_path))
    ev = build_ood_evidence(ood_dir, act)
    assert ev["ood_records_enter_iid_splitter"] is False
    assert ev["splitter_statement"].startswith(
        "OOD records never enter the IID splitter")


def test_signal_registry_dict_falls_back_to_frozen(tmp_path):
    """编排层直传 contracts.load_registry() 信号 registry（无 tokens 键）.

    T-020 编排层（run_formal_freeze）以 reg=load_registry() 作第三参；
    非 token registry 的 dict 必须回退冻结默认 registry（D4 唯一事实源），
    轴 C 判定不受影响，registry_source 出证回退事实。
    """
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=1)
    signal_registry_like = {"schema": "dataset_v3_signal_registry_v1",
                            "signals": [{"name": "P_load_total_kW"}]}
    act = _write_activation(str(tmp_path), per_condition=1)
    ev = build_ood_evidence(ood_dir, act, signal_registry_like)
    assert ev["overall_pass"] is True
    assert ev["registry_source"] == \
        "caller_dict_not_token_registry_fallback_default_frozen"
    assert ev["axes"]["C"]["formal_count"] == 3  # 轴 C 判定照常
    # list 形态（已装载 REQUIRE 条目）同样受支持
    ev2 = build_ood_evidence(ood_dir, act, _require_tokens())
    assert ev2["overall_pass"] is True
    assert ev2["registry_source"] == "caller_token_entries"


def test_missing_ood_condition_id_rejected(tmp_path):
    """meta 缺 ood_condition_id 的 record 被（metadata 层）validate 拒收."""
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=2)
    _write_voyage(ood_dir, "V_NOCOND_0", "B", "B1", ht_effective=1.4,
                  with_condition=False)
    act = _write_activation(str(tmp_path))
    ev = build_ood_evidence(ood_dir, act)
    assert ev["overall_pass"] is False
    assert any(e["voyage_id"] == "V_NOCOND_0"
               and "ood_condition_id" in e["reason"]
               for e in ev["metadata_errors"])
    # 拒收记录不计入 planned/realized counts（其 planned 身份无从解析）
    assert ev["allocation_check"]["per_condition"]["B1"]["realized_voyages"] == 2
    assert ev["allocation_check"]["total"]["realized"] == 10  # 10 正式 + 1 拒收
    assert ev["allocation_check"]["pass"] is True  # planned gate 对余下语料成立
    assert ev["checks"]["metadata"] is False       # overall FAIL 由 metadata 承载


def test_missing_realized_metric_rejected(tmp_path):
    """realized 域指标缺失（propulsion_check.inputs 损坏）→ 拒收非降级。"""
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=2)
    _write_voyage(ood_dir, "V_NOMETRIC_0", "A", "A1", u_sup_abs=2.2,
                  with_metric=False)
    act = _write_activation(str(tmp_path))
    ev = build_ood_evidence(ood_dir, act)
    assert ev["overall_pass"] is False
    assert any("u_sup_abs" in e["reason"] for e in ev["metadata_errors"])


def test_empty_and_missing_dir_gate_semantics(tmp_path):
    """空目录 / 目录不存在 → overall_pass=False 带原因，不抛异常。"""
    act_path = _write_activation(str(tmp_path))
    empty = str(tmp_path / "empty_ood")
    os.makedirs(empty)
    ev = build_ood_evidence(empty, act_path)
    assert ev["overall_pass"] is False
    assert ev["reasons"] and "无 .mat" in ev["reasons"][0]
    for k in ("allocation_check", "axes", "stress_set", "per_condition",
              "overall_pass"):
        assert k in ev
    ev2 = build_ood_evidence(str(tmp_path / "nonexistent"), act_path)
    assert ev2["overall_pass"] is False
    assert ev2["reasons"]


def test_bad_activation_json_gate_semantics(tmp_path):
    """activation record 缺 formal_fleet.ood.counts → gate 失败不抛异常。"""
    ood_dir = str(tmp_path / "ood")
    os.makedirs(ood_dir)
    _build_full_corpus(ood_dir, per_condition=1)
    bad = os.path.join(str(tmp_path), "bad.json")
    with open(bad, "w", encoding="utf-8") as f:
        json.dump({"formal_fleet": {}}, f)
    ev = build_ood_evidence(ood_dir, bad)
    assert ev["overall_pass"] is False
    assert ev["reasons"]


def test_load_ood_mat_roundtrip(tmp_path):
    """load_ood_mat：三表装载 + voyage_id 归一（独立函数，供 worker-1 复用）."""
    vid = "V_ROUNDTRIP_1"
    _write_voyage(str(tmp_path), vid, "B", "B1", ht_effective=1.4)
    payload = load_ood_mat(os.path.join(str(tmp_path), vid + ".mat"))
    assert payload["voyage_id"] == vid
    assert payload["meta"]["ood_axis"] == "B"
    assert payload["meta"]["ood_condition_id"] == "B1"
    assert payload["stream"]["scenario_id"] == vid
    assert isinstance(payload["kf"], dict)
    savemat(os.path.join(str(tmp_path), "bad.mat"),
            {"stream": {"time_s": np.arange(3)}, "kf": {}})
    with pytest.raises(ValueError, match="缺表"):
        load_ood_mat(os.path.join(str(tmp_path), "bad.mat"))

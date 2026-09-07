#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_cd_interface_gate.py — D-15 C/D Interface Gate v2 测试.

覆盖（Fixed Decisions A~E，DGN-16/FR-496）：
  A1  十项检查独立成键（键集合恰为 c01~c10）+ result 顶层 v2 结构 + 域开关回显；
  A2  FORMAL 缺 oracle 载体列 → c05 fail（⑤(iv) 真值对应物双存断言）；
  A3  kf 混入 ORACLE 列名（u_actual_future_mps）→ c05 fail（⑤(ii)/(iii)）；
  A4  kf 混入白名单外列 → c05 fail（⑤(i) 闭世界白名单）；
  A5  ⑩ fail-closed：缺快照 / 坏 JSON / 结构非法 FAIL；无 pass 键但结构合法 → SNAPSHOT_CONSUMED；
  A6  FORMAL embedded eligibility disposition=REJECT → c07 fail；
  A7  NON-FORMAL pilot 语义：producer_script_sha256 缺 → c06 fail；FIXTURE_CAUSAL_STUB 过 ④；
      c07 维持 PENDING_ACTIVATION_RECORD；
  A8  FORMAL fixture_kind=PILOT_ONLY → c06 fail（FR-500）；
  A9  FORMAL embedded schema_self_hash 与 D 侧不一致 → c01 fail。

两条途径结合：scipy.io.savemat 合成 voyage 写入 tmp 目录后走 run_gate 全链 +
直接调用 check 级函数（_check_baseline_integrity）。
"""

import json
import os
import sys

import numpy as np
from scipy.io import savemat

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from dataset_v3.contracts import load_producer_schema, load_registry
from dataset_v3.cd_interface_gate import (FORMAL, FORMALITY_NOTE, NON_FORMAL,
                                          _check_baseline_integrity, run_gate)

TEN_CHECK_KEYS = {
    "c01_schema_compliance", "c02_s_plan_causal_present", "c03_kf_snapshot_form",
    "c04_forecast_provenance_valid", "c05_oracle_physical_separation",
    "c06_metadata_complete", "c07_voyage_qualification", "c08_ood_config_surface",
    "c09_contract_validators", "c10_modelE_baseline_integrity"}
TOP_LEVEL_KEYS = {"schema", "formality", "formality_note", "generated_utc",
                  "n_voyages", "activation_provided", "overall", "checks"}
LEGAL_BASELINE_REF = "ModelE physical v1.1 + control v1.0"


# ---- 公共构造 ----
def _self_hashes():
    schema = load_producer_schema(validate=True)
    reg = load_registry(validate=True)
    return (schema["self_hash"]["value"],
            reg["self_hash"]["value"] if "self_hash" in reg else None)


def _mk_stream(with_carriers=True, extra_h12=True):
    """15 truth 列（FORMAL 面）+ H2 补充列（NON-FORMAL registry 面）。"""
    t = np.arange(0.0, 1201.0, 1.0)
    n = t.size
    cols = {
        "time_s": t,
        "P_load_total_kW": 200.0 + 10.0 * np.sin(t / 60.0),
        "v_ship_mps": 3.0 * np.ones(n),
        "acceleration_mps2": np.zeros(n),
        "n_prop_rpm": 180.0 * np.ones(n),
        "P_hotel_kW": 30.0 * np.ones(n),
        "v_current_mps": 0.5 * np.ones(n),
        "water_depth_m": 20.0 * np.ones(n),
        "P_prop_demand_kW": 150.0 * np.ones(n),
        "ctrl_mode": np.ones(n),
        "v_ref_mps": 3.0 * np.ones(n),
        "manual_power_cmd_pct": np.zeros(n),
        "P_gen_actual_kW": 190.0 * np.ones(n),
        "scenario_id": np.ones(n),
        "resistance_multiplier": np.ones(n),
    }
    if not with_carriers:   # A2：剥离 oracle 载体列（v_current_mps / water_depth_m）
        cols.pop("v_current_mps")
        cols.pop("water_depth_m")
    if extra_h12:           # NON-FORMAL c01 还要求 registry H1/H2 全列在 stream
        reg = load_registry(validate=False)
        skip = set() if with_carriers else {"v_current_mps", "water_depth_m"}
        h12 = [s["name"] for s in reg["signals"]
               if s["role"] in ("INPUT_H1", "INPUT_H2")
               and s["name"] not in cols and s["name"] not in skip]
        for c in h12:
            cols[c] = 100.0 * np.ones(n)
    return cols


def _mk_kf(extra_cols=None):
    issue = np.arange(300.0, 901.0, 10.0)                     # 61 行 ≥ 60
    lead = np.tile(np.arange(10.0, 301.0, 10.0), (issue.size, 1))   # (M,30) issue-major
    kf = {
        "issue_time_s": issue, "lead_s": lead,
        "v_plan_mps": 3.0 * np.ones(issue.size),
        "s_plan_m": np.cumsum(30.0 * np.ones(issue.size)),
        "h_chart_m": 20.0 * np.ones(issue.size),
        "P_hotel_plan_kW": 30.0 * np.ones(issue.size),
        "u_fcst_mps": 0.5 * np.ones(issue.size),
        "wl_fcst_m": 0.1 * np.ones(issue.size),
        "in_speed_limit_zone": np.zeros(issue.size),
        "in_bridge_zone": np.zeros(issue.size),
        "missing_mask": np.zeros(issue.size),
        "contract_status": np.zeros(issue.size),
        "provenance": np.zeros(issue.size),
    }
    for c, arr in (extra_cols or {}).items():
        kf[c] = arr
    return kf


def _mk_meta(formality, eligibility=True, **over):
    meta = {
        "voyage_id": "V1", "voyage_family_id": "FAM", "route_template_id": "RT",
        "mission_template_id": "MT", "split_group_id": "G1",
        "scenario_stratum": "S1", "generation_seed": "seed-1",
        "model_baseline_ref": LEGAL_BASELINE_REF,
        "ood_axis": "water_depth", "ood_def_version": "DGN-11-v1",
        "physics_domain_ref": "shallow_water_HT",
    }
    if formality == FORMAL:
        meta.update({"producer_script_sha": "abc123", "fixture_kind": "NONE"})
        prov = {"forecast_generator_id": "mission_v3_env_forecast",
                "forecast_generator_version": "1.0.0",
                "forecast_generator_hash": "h123",
                "issue_time_s": 900.0, "source_cutoff_time_s": 900.0,
                "forecast_provenance_class": "CAUSAL_GENERATOR"}
        if eligibility:
            sch_h, reg_h = _self_hashes()
            meta["eligibility"] = {
                "disposition": "ACCEPT",
                "criteria_values": {"min_duration_s": 1200.0,
                                    "min_canonical_windows": 60,
                                    "unexpected_invalid_max": 0},
                "windows": {"duration_s": 1200.0,
                            "expected_canonical_windows": 61,
                            "valid_canonical_windows": 61,
                            "provenance": {"schema_self_hash": sch_h,
                                           "registry_self_hash": reg_h}}}
    else:
        meta.update({"producer_script_sha256": "abc123", "fixture_kind": "PILOT_ONLY"})
        prov = {"forecast_generator_id": "pilot_persistence_fixture",
                "forecast_generator_version": "1",
                "forecast_generator_hash": "h123",
                "issue_time_s": 900.0, "source_cutoff_time_s": 900.0,
                "forecast_provenance_class": "FIXTURE_CAUSAL_STUB"}
    meta["forecast_provenance"] = prov
    meta.update(over)
    return meta


def _write_voyage(tmpdir, name, stream, kf, meta):
    savemat(os.path.join(tmpdir, name),
            {"stream": stream, "kf": kf, "meta": meta})


def _mk_formal_voyage(tmpdir, name="V1.mat", **kw):
    _write_voyage(tmpdir, name, _mk_stream(), _mk_kf(), _mk_meta(FORMAL, **kw))


# ---- A1：十项检查独立成键 + result v2 顶层结构 ----
def test_a1_ten_independent_keys_and_v2_envelope(tmp_path):
    _mk_formal_voyage(str(tmp_path))
    res = run_gate(str(tmp_path), formality=FORMAL)
    assert res["schema"] == "cd_interface_gate_result_v2"
    assert set(res.keys()) == TOP_LEVEL_KEYS
    assert set(res["checks"].keys()) == TEN_CHECK_KEYS
    assert res["formality"] == "FORMAL"
    assert res["formality_note"] == "正式 C/D Interface Gate（EXT-CD-GATE）"
    assert res["activation_provided"] is False
    assert res["n_voyages"] == 1
    # 良构 FORMAL 合成航次应整体 PASS（⑩ 消费仓库内 72 号快照，pass=true）
    assert res["overall"] is True, json.dumps(res["checks"], ensure_ascii=False)[:800]
    for k in ("c01_schema_compliance", "c02_s_plan_causal_present", "c03_kf_snapshot_form",
              "c04_forecast_provenance_valid", "c05_oracle_physical_separation",
              "c06_metadata_complete", "c08_ood_config_surface"):
        assert set(res["checks"][k].keys()) == {"pass", "per_voyage"}


def test_a1b_non_formal_default_note(tmp_path):
    _write_voyage(str(tmp_path), "V1.mat", _mk_stream(), _mk_kf(), _mk_meta(NON_FORMAL))
    res = run_gate(str(tmp_path), formality=NON_FORMAL)
    assert res["formality"] == "NON-FORMAL"
    assert res["formality_note"] == FORMALITY_NOTE[NON_FORMAL]
    assert "不构成 T-432 解锁" in res["formality_note"]


# ---- A2：FORMAL 缺 oracle 载体列 → c05 fail（⑤(iv)）----
def test_a2_formal_missing_oracle_carrier_c05_fail(tmp_path):
    _write_voyage(str(tmp_path), "V1.mat", _mk_stream(with_carriers=False),
                  _mk_kf(), _mk_meta(FORMAL))
    res = run_gate(str(tmp_path), formality=FORMAL)
    c05 = res["checks"]["c05_oracle_physical_separation"]
    assert c05["pass"] is False
    msgs = [m for vs in c05["per_voyage"].values() for m in vs]
    assert any(("v_current_mps" in m or "water_depth_m" in m) and "双存" in m for m in msgs)


# ---- A3：kf 混入 ORACLE 列名 → c05 fail（⑤(ii)/(iii)）----
def test_a3_oracle_column_in_kf_c05_fail(tmp_path):
    kf = _mk_kf(extra_cols={"u_actual_future_mps": np.zeros((61, 30))})
    _mk_formal_voyage(str(tmp_path))
    _write_voyage(str(tmp_path), "V2.mat", _mk_stream(), kf, _mk_meta(FORMAL))
    res = run_gate(str(tmp_path), formality=FORMAL)
    c05 = res["checks"]["c05_oracle_physical_separation"]
    assert c05["pass"] is False
    msgs = [m for vs in c05["per_voyage"].values() for m in vs]
    assert any("u_actual_future_mps" in m for m in msgs)
    assert "V2" in c05["per_voyage"] and "V1" not in c05["per_voyage"]


# ---- A4：kf 混入白名单外列 → c05 fail（⑤(i)）----
def test_a4_non_whitelist_column_in_kf_c05_fail(tmp_path):
    # resistance_multiplier = METADATA/metadata_only（不触发 ⑤(ii)），仅触发闭世界白名单
    kf = _mk_kf(extra_cols={"resistance_multiplier": np.ones(61)})
    _write_voyage(str(tmp_path), "V1.mat", _mk_stream(), kf, _mk_meta(FORMAL))
    res = run_gate(str(tmp_path), formality=FORMAL)
    c05 = res["checks"]["c05_oracle_physical_separation"]
    assert c05["pass"] is False
    msgs = [m for vs in c05["per_voyage"].values() for m in vs]
    assert any("白名单" in m and "resistance_multiplier" in m for m in msgs)


# ---- A5：⑩ fail-closed ----
def test_a5_c10_fail_closed_missing_snapshot(tmp_path):
    r = _check_baseline_integrity(str(tmp_path / "nope.json"))
    assert r["pass"] is False and r["verdict"] == "MISSING_BASELINE_SNAPSHOT"


def test_a5b_c10_fail_closed_corrupt_json(tmp_path):
    p = tmp_path / "baseline_integrity.json"
    p.write_text("{not-json", encoding="utf-8")
    r = _check_baseline_integrity(str(p))
    assert r["pass"] is False


def test_a5c_c10_snapshot_consumed_without_pass_key(tmp_path):
    p = tmp_path / "baseline_integrity.json"
    files = [{"path": f"f{i}", "sha256": "a" * 64} for i in range(5)]
    p.write_text(json.dumps({"check": "10_modelE_baseline_integrity", "files": files}),
                 encoding="utf-8")
    r = _check_baseline_integrity(str(p))
    assert r["pass"] is True and r["verdict"] == "SNAPSHOT_CONSUMED"


def test_a5d_c10_invalid_structure(tmp_path):
    p = tmp_path / "baseline_integrity.json"
    files = [{"path": "f0", "sha256": ""}]       # <5 个文件 + 空 sha256
    p.write_text(json.dumps({"files": files}), encoding="utf-8")
    r = _check_baseline_integrity(str(p))
    assert r["pass"] is False and r["verdict"] == "INVALID_BASELINE_SNAPSHOT"


# ---- A6：FORMAL eligibility disposition=REJECT → c07 fail ----
def test_a6_formal_reject_eligibility_c07_fail(tmp_path):
    # 覆写 disposition（_mk_meta 内 deep dict，直接改落盘前 meta）
    meta = _mk_meta(FORMAL)
    meta["eligibility"]["disposition"] = "REJECT"
    _write_voyage(str(tmp_path), "VR.mat", _mk_stream(), _mk_kf(), meta)
    res = run_gate(str(tmp_path), formality=FORMAL)
    c07 = res["checks"]["c07_voyage_qualification"]
    assert c07["pass"] is False
    assert c07["verdict"] == "VERIFIED_AGAINST_EMBEDDED_ELIGIBILITY"
    assert any("REJECT" in "; ".join(v) for v in c07["violations"].values())


def test_a6b_formal_without_eligibility_no_evidence(tmp_path):
    meta = _mk_meta(FORMAL)
    meta.pop("eligibility")
    _write_voyage(str(tmp_path), "V1.mat", _mk_stream(), _mk_kf(), meta)
    res = run_gate(str(tmp_path), formality=FORMAL)
    c07 = res["checks"]["c07_voyage_qualification"]
    assert c07["pass"] is False
    assert c07["verdict"] == "NO_QUALIFICATION_EVIDENCE"


# ---- A7：NON-FORMAL pilot 语义 ----
def test_a7_non_formal_missing_producer_sha256_c06_fail_but_c04_pass(tmp_path):
    meta = _mk_meta(NON_FORMAL)
    meta.pop("producer_script_sha256")           # pilot 语义 9 键之一缺失
    _write_voyage(str(tmp_path), "V1.mat", _mk_stream(), _mk_kf(), meta)
    res = run_gate(str(tmp_path), formality=NON_FORMAL)
    assert res["checks"]["c06_metadata_complete"]["pass"] is False
    # FIXTURE_CAUSAL_STUB 在 pilot stage 白名单内 → ④ 通过
    assert res["checks"]["c04_forecast_provenance_valid"]["pass"] is True
    # NON-FORMAL 无 activation → ⑦ PENDING（pass=true）
    c07 = res["checks"]["c07_voyage_qualification"]
    assert c07["pass"] is True and c07["verdict"] == "PENDING_ACTIVATION_RECORD"


# ---- A8：FORMAL fixture_kind=PILOT_ONLY → c06 fail（FR-500）----
def test_a8_formal_pilot_only_fixture_c06_fail(tmp_path):
    _write_voyage(str(tmp_path), "V1.mat", _mk_stream(), _mk_kf(),
                  _mk_meta(FORMAL, fixture_kind="PILOT_ONLY"))
    res = run_gate(str(tmp_path), formality=FORMAL)
    c06 = res["checks"]["c06_metadata_complete"]
    assert c06["pass"] is False
    msgs = [m for vs in c06["per_voyage"].values() for m in vs]
    assert any("PILOT_ONLY" in m for m in msgs)


# ---- A9：FORMAL embedded schema_self_hash 不匹配 → c01 fail ----
def test_a9_formal_embedded_schema_hash_mismatch_c01_fail(tmp_path):
    meta = _mk_meta(FORMAL)
    meta["eligibility"]["windows"]["provenance"]["schema_self_hash"] = "0" * 64
    _write_voyage(str(tmp_path), "V1.mat", _mk_stream(), _mk_kf(), meta)
    res = run_gate(str(tmp_path), formality=FORMAL)
    c01 = res["checks"]["c01_schema_compliance"]
    assert c01["pass"] is False
    msgs = [m for vs in c01["per_voyage"].values() for m in vs]
    assert any("schema_self_hash" in m for m in msgs)

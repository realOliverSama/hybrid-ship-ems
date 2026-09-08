#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_run_formal_freeze.py — T-020 formal freeze 编排单元测试.

覆盖（任务要求的最小负向集）：
- formal_voyage_gate：PILOT_ONLY 拒收（FR-500 反转门）+ provenance formal 白名单；
- verify_two_run_payload_hash：pass1/pass2 逐 voyage payload_hash 不一致 → FAIL；
  数量/覆盖不符 → FAIL；ledger 缺失 → MISSING；一致 → PASS；
- resolve_stage / build_formal_freeze dryrun 语义：STAGE_B 标记缺失时 dryrun=True
  不 FAIL 且 stage=="FORMAL_DRYRUN"；非 dryrun 缺标记 → 早退 FAIL；
- expected_allocation：activation record 派生 576/200（FR-465，禁 hard-code）。

说明：freeze_ood / freeze_manifest 为 T-020 并行 worker 接口，编排层对
ImportError 的处理（记 FAIL check 而非伪造 PASS）由 dryrun 全链测试间接覆盖
（断言与集成点无关，worker 就绪前后测试行为一致）。
"""

import json
import os

import numpy as np
import pytest
from scipy.io import savemat

from dataset_v3.run_formal_freeze import (expected_allocation, formal_voyage_gate,
                                          load_activation_record, resolve_stage,
                                          verify_two_run_payload_hash)

LEGAL_PROV = {"forecast_generator_id": "mission_v3_env_forecast",
              "forecast_generator_version": "1.0.0",
              "forecast_generator_hash": "h" * 64,
              "issue_time_s": 900.0, "source_cutoff_time_s": 900.0,
              "forecast_provenance_class": "CAUSAL_GENERATOR"}


def _meta(**over):
    meta = {"voyage_id": "V1", "split_group_id": "SG#aaa", "scenario_stratum": "S1",
            "generation_seed": "seed-1", "route_template_id": "RT",
            "fixture_kind": "NONE", "forecast_provenance": dict(LEGAL_PROV)}
    meta.update(over)
    return meta


# ===========================================================================
# (a) formal_voyage_gate：PILOT_ONLY 拒收 + provenance formal 白名单
# ===========================================================================
class TestFormalVoyageGate:
    def test_pilot_only_rejected(self):
        errs = formal_voyage_gate(_meta(fixture_kind="PILOT_ONLY"), dict(LEGAL_PROV))
        assert errs, "PILOT_ONLY 航次必须被 formal 门拒收"
        assert any("PILOT_ONLY" in e for e in errs)

    def test_none_fixture_clean_pass(self):
        assert formal_voyage_gate(_meta(), dict(LEGAL_PROV)) == []

    def test_fixture_stub_provenance_rejected_in_formal(self):
        prov = dict(LEGAL_PROV, forecast_provenance_class="FIXTURE_CAUSAL_STUB")
        errs = formal_voyage_gate(_meta(), prov)
        assert any("CAUSAL_GENERATOR" in e for e in errs)

    def test_oracle_derived_always_rejected(self):
        prov = dict(LEGAL_PROV, forecast_provenance_class="ORACLE_DERIVED")
        errs = formal_voyage_gate(_meta(), prov)
        assert any("ORACLE" in e for e in errs)

    def test_cutoff_after_issue_rejected(self):
        prov = dict(LEGAL_PROV, source_cutoff_time_s=1000.0)
        errs = formal_voyage_gate(_meta(), prov)
        assert any("source_cutoff_time_s" in e for e in errs)


# ===========================================================================
# (b) verify_two_run_payload_hash
# ===========================================================================
def _write_ledger(outdir, p, key, vids_hashes):
    with open(os.path.join(outdir, f"{p}_{key}_ledger.json"), "w", encoding="utf-8") as f:
        json.dump({"voyages": [{"voyage_id": v, "payload_hash": h}
                               for v, h in vids_hashes]}, f)


class TestVerifyTwoRunPayloadHash:
    def test_identical_pass(self, tmp_path):
        iid = [(f"V{i}", f"h{i}") for i in range(3)]
        ood = [("O0", "h0"), ("O1", "h1")]
        for p in ("pass1", "pass2"):
            _write_ledger(tmp_path, p, "iid", iid)
            _write_ledger(tmp_path, p, "ood", ood)
        res = verify_two_run_payload_hash(str(tmp_path), 3, 2)
        assert res["status"] == "PASS", res["errors"]
        assert res["errors"] == [] and res["missing"] == []

    def test_hash_mismatch_fail(self, tmp_path):
        for p in ("pass1", "pass2"):
            _write_ledger(tmp_path, p, "iid", [("V0", "h0"), ("V1", "h1")])
            _write_ledger(tmp_path, p, "ood", [("O0", "h0")])
        _write_ledger(tmp_path, "pass2", "iid", [("V0", "h0"), ("V1", "MUTATED")])
        res = verify_two_run_payload_hash(str(tmp_path), 2, 1)
        assert res["status"] == "FAIL"
        assert any("payload_hash 不一致" in e and "V1" in e for e in res["errors"])

    def test_count_mismatch_fail(self, tmp_path):
        _write_ledger(tmp_path, "pass1", "iid", [("V0", "h0")])
        _write_ledger(tmp_path, "pass2", "iid", [("V0", "h0")])
        _write_ledger(tmp_path, "pass1", "ood", [("O0", "h0")])
        _write_ledger(tmp_path, "pass2", "ood", [("O0", "h0")])
        res = verify_two_run_payload_hash(str(tmp_path), 576, 1)
        assert res["status"] == "FAIL"
        assert any("!= 期望 576" in e for e in res["errors"])

    def test_coverage_mismatch_fail(self, tmp_path):
        _write_ledger(tmp_path, "pass1", "iid", [("V0", "h0"), ("VX", "hx")])
        _write_ledger(tmp_path, "pass2", "iid", [("V0", "h0"), ("VY", "hy")])
        _write_ledger(tmp_path, "pass1", "ood", [("O0", "h0")])
        _write_ledger(tmp_path, "pass2", "ood", [("O0", "h0")])
        res = verify_two_run_payload_hash(str(tmp_path), 2, 1)
        assert res["status"] == "FAIL"
        assert any("覆盖不一致" in e for e in res["errors"])

    def test_missing_ledger_status_missing(self, tmp_path):
        res = verify_two_run_payload_hash(str(tmp_path), 3, 2)
        assert res["status"] == "MISSING"
        assert len(res["missing"]) == 4


# ===========================================================================
# (c) STAGE_B 标记门 + dryrun 语义
# ===========================================================================
class TestResolveStage:
    def test_dryrun_skips_marker(self, tmp_path):
        stage, errs = resolve_stage(str(tmp_path), dryrun=True)
        assert stage == "FORMAL_DRYRUN" and errs == []

    def test_marker_present_formal(self, tmp_path):
        (tmp_path / "STAGE_B_TWO_RUN_DONE").write_text("")
        stage, errs = resolve_stage(str(tmp_path), dryrun=False)
        assert stage == "FORMAL" and errs == []

    def test_marker_absent_blocked(self, tmp_path):
        stage, errs = resolve_stage(str(tmp_path), dryrun=False)
        assert stage == "FORMAL" and errs, "非 dryrun 缺 STAGE_B 标记必须拦截"


def _mk_voyage(vid, phase, group, stratum, seed):
    """最小合法 formal 航次（1 Hz 1200 s；anchors 300..900 每 10 s 一 vintage）。"""
    t = np.arange(0.0, 1201.0, 1.0)
    n = t.size
    stream = {"time_s": t,
              "P_load_total_kW": 200.0 + 10.0 * np.sin(t / 60.0 + phase)}
    for c, base in (("v_ship_mps", 3.0), ("acceleration_mps2", 0.0), ("n_prop_rpm", 180.0),
                    ("P_hotel_kW", 30.0), ("v_current_mps", 0.5), ("water_depth_m", 20.0),
                    ("P_prop_demand_kW", 150.0), ("v_ref_mps", 3.0),
                    ("manual_power_cmd_pct", 0.0), ("P_gen_actual_kW", 190.0),
                    ("P_bat_actual_kW", 10.0), ("SOC", 0.8), ("V_dc_V", 700.0),
                    ("state_DG1", 1.0), ("state_DG2", 0.0), ("V_bat_V", 500.0),
                    ("I_bat_A", 20.0), ("P_bat_dis_avail_kW", 100.0),
                    ("P_bat_chg_avail_kW", 100.0)):
        stream[c] = base * np.ones(n) + 0.01 * phase * np.sin(t / 97.0)
    stream["ctrl_mode"] = 1.0 * np.ones(n)   # DGN-06：编译期 0/1 编码列禁扰动
    issue = np.arange(300.0, 901.0, 10.0)
    kf = {"issue_time_s": issue,
          "lead_s": np.tile(np.arange(10.0, 301.0, 10.0), (issue.size, 1))}
    for c, base in (("v_plan_mps", 3.0), ("s_plan_m", 1000.0), ("h_chart_m", 20.0),
                    ("P_hotel_plan_kW", 30.0), ("u_fcst_mps", 0.5), ("wl_fcst_m", 0.1)):
        kf[c] = np.tile((base + phase) * np.ones(30), (issue.size, 1))
    meta = {"voyage_id": vid, "split_group_id": group, "scenario_stratum": stratum,
            "generation_seed": seed, "route_template_id": f"RT{phase}",
            "fixture_kind": "NONE", "forecast_provenance": dict(LEGAL_PROV)}
    return {"stream": stream, "kf": kf, "meta": meta}


class TestDryrunFullChain:
    def test_dryrun_stage_b_marker_absent_not_fail(self, tmp_path):
        """dryrun=True：STAGE_B 标记缺失不 FAIL，stage==FORMAL_DRYRUN，全链返回结果。"""
        voyage_dir = tmp_path / "voyages"
        voyage_dir.mkdir()
        for i, (vid, ph, grp, st) in enumerate(
                [("VA", 0.0, "SG#a", "S1"), ("VB", 1.0, "SG#b", "S2"),
                 ("VC", 2.0, "SG#c", "S3")]):
            v = _mk_voyage(vid, ph, grp, st, f"seed-{i}")
            savemat(voyage_dir / f"{vid}.mat", v)
        out_dir = tmp_path / "out"
        npz = tmp_path / "win.npz"
        from dataset_v3.run_formal_freeze import build_formal_freeze
        res = build_formal_freeze(str(voyage_dir), None, str(out_dir), str(npz),
                                  dryrun=True)
        assert res["schema"] == "dataset_v3_formal_freeze_result_v1"
        assert res["stage"] == "FORMAL_DRYRUN"
        assert res["dryrun"] is True
        gate = [c for c in res["checks"] if c["name"] == "stage_b_marker_gate"]
        assert gate and gate[0]["pass"] is True, gate
        # 全链应到达后段集成点（两跑复验以 dryrun SKIP 记录）
        two = [c for c in res["checks"] if c["name"] == "two_run_payload_hash"]
        assert two and two[0].get("skipped") is True
        # freeze_manifest 契约：dryrun（stage != FORMAL）由调用方不调用 build_formal_manifest
        man = [c for c in res["checks"] if c["name"] == "manifest_sealed"]
        assert man and man[0].get("skipped") is True
        assert not (out_dir / "dataset_v3_formal_manifest.json").exists()
        assert npz.is_file(), "dryrun 产物写调用方目录"

    def test_formal_marker_absent_fail_fast(self, tmp_path):
        """非 dryrun：STAGE_B 标记缺失 → fail-fast 早退，无产物写盘。"""
        voyage_dir = tmp_path / "voyages"
        voyage_dir.mkdir()
        v = _mk_voyage("VA", 0.0, "SG#a", "S1", "seed")
        savemat(voyage_dir / "VA.mat", v)
        out_dir = tmp_path / "out"
        from dataset_v3.run_formal_freeze import build_formal_freeze
        res = build_formal_freeze(str(voyage_dir), None, str(out_dir),
                                  str(tmp_path / "win.npz"), dryrun=False)
        assert res["overall"] is False
        assert res["stage"] == "FORMAL"
        assert any(c["name"] == "stage_b_marker_gate" and not c["pass"]
                   for c in res["checks"])
        assert res.get("outputs") == {}
        assert not (out_dir / "split_assignment_v3.json").exists()


# ===========================================================================
# activation record 派生（FR-465：split_seed / allocation 唯一事实源）
# ===========================================================================
class TestActivationDerived:
    def test_expected_allocation_from_repo_record(self):
        act, path = load_activation_record(None)
        assert act["split"]["split_seed"] == "dataset-v3-formal-split-seed-v1"
        alloc = expected_allocation(act)
        assert alloc == {"iid": 576, "ood": 200}, alloc

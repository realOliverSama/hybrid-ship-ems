"""T-001 config / fixture / env / E-guard unit tests."""
from __future__ import annotations

import json
import os
import re
import sys

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.dirname(HERE)
PYROOT = os.path.dirname(PKG)
if PYROOT not in sys.path:
    sys.path.insert(0, PYROOT)

from t445 import hashutil, paths  # noqa: E402
from t445.frozen_config import (  # noqa: E402
    N_DLAMBDA,
    N_LAMBDA_COLON,
    N_PBAR_COLON,
    N_SOC_B2,
    PROVENANCE_B1_NORM,
    build_frozen_config,
)


def load_json(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


@pytest.fixture(scope="module")
def cfg():
    assert os.path.isfile(paths.CONFIG_JSON), "run python -m t445.freeze_t001 first"
    return load_json(paths.CONFIG_JSON)


def test_schema_and_k_eq_provisional(cfg):
    assert cfg["schema"] == "phase4f_t445_config_v1"
    assert cfg["k_eq"]["value_kg_per_kWh"] == 0.5296
    assert cfg["k_eq"]["status"] == "PROVISIONAL_PENDING_QUALIFICATION"
    assert cfg["k_eq"]["bat_cap_kWh"] == 1071.36
    assert cfg["k_eq"]["bsfc_ref_E_g_per_kWh"] == 482.749
    assert cfg["k_eq"]["eta_chg_ref"] == 0.9687
    assert cfg["k_eq"]["eta_dis_ref"] == 0.9686
    assert cfg["k_eq"]["eta_path_E"] == 0.9111
    assert cfg["k_eq"]["robustness_0p8x"] == 0.4237
    assert cfg["k_eq"]["robustness_1p2x"] == 0.6355
    assert cfg["k_eq"]["no_double_counting_of_energy_loss"] is True
    assert cfg["k_eq"]["model_d_k_eq_0p499_revoked"] is True


def test_b1_norm_literals(cfg):
    assert cfg["b1_norm"]["E_ref_kWh"] == 27.781415
    assert cfg["b1_norm"]["E_scale_kWh"] == 13.671476
    assert cfg["b1_norm"]["provenance_sha256"] == PROVENANCE_B1_NORM
    assert cfg["b1_norm"]["per_voyage_refit_forbidden"] is True


def test_lambda_and_delta(cfg):
    assert cfg["lambda_law"]["lambda0"] == 0.425
    assert cfg["lambda_law"]["K_soc"] == 30.0
    assert cfg["lambda_law"]["K_S_B1"] == 0.01
    assert cfg["lambda_law"]["K_S_B2_num"] == 1
    assert cfg["lambda_law"]["K_S_B2_den"] == 30
    assert cfg["lambda_law"]["floor"] == 0.43
    assert cfg["lambda_law"]["cap"] == 0.80
    assert cfg["delta_lambda"]["min_kg_per_kWh"] == -0.10
    assert cfg["delta_lambda"]["max_kg_per_kWh"] == 0.10
    assert cfg["delta_lambda"]["step"] == 0.005
    assert cfg["delta_lambda"]["n_levels"] == N_DLAMBDA == 41
    assert cfg["delta_lambda"]["rate_limit"] is False


def test_v_term_and_terminal(cfg):
    assert cfg["v_term"]["delta_term"] == 0.01
    assert cfg["v_term"]["w_term"] == 1.13e5
    assert cfg["v_term"]["soc_target_policy"] == "SOC0_voyage"
    assert cfg["terminal_soc_admissibility"]["abs_dsoc_max"] == 0.005
    assert cfg["gate_thresholds"]["X_pct"] == 2.0
    assert cfg["gate_thresholds"]["Y_pct"] == 0.0
    assert cfg["seeds"]["corpus"] == "phase4f-t445-corpus-v1"
    assert cfg["seeds"]["bootstrap"] == "phase4f-t445-bootstrap-v1"
    assert cfg["bootstrap"]["B"] == 10000
    assert cfg["epsilon_num"]["rel_of_j_fec_b2"] == 0.005


def test_grids(cfg):
    assert cfg["b2_policy_table"]["lambda_colon"] == [0.43, 0.005, 0.80]
    assert cfg["b2_policy_table"]["n_lambda_derived"] == N_LAMBDA_COLON == 75
    assert cfg["b2_policy_table"]["n_pbar"] == N_PBAR_COLON == 66
    assert cfg["b2_policy_table"]["n_soc_regions"] == 3
    assert cfg["b2_dp"]["n_soc"] == N_SOC_B2 == 86
    assert cfg["b2_dp"]["eta_chg"] == 0.9687
    assert cfg["b2_dp"]["eta_dis"] == 0.9686
    assert cfg["oracle_window"]["refresh_s"] == 10
    assert cfg["oracle_window"]["K"] == 30
    assert cfg["oracle_window"]["shrinking_horizon"] is True
    assert cfg["oracle_window"]["zero_fill_forbidden"] is True


def test_audit_matrix_closed(cfg):
    rows = cfg["audit_capability_matrix"]
    assert len(rows) == 12
    statuses = {r["status"] for r in rows}
    assert statuses <= {"AVAILABLE_AND_CHECKED", "NOT_INSTRUMENTED", "NOT_APPLICABLE"}
    assert all(r["status"] == "AVAILABLE_AND_CHECKED" for r in rows)
    hard = cfg["hard_feasibility"]
    for key in (
        "task_complete", "load_shortfall", "soc_hard_bounds", "terminal_soc",
        "battery_sop_v_i", "genset_limits", "energy_closure_c5", "safety_hard_events",
    ):
        assert key in hard and hard[key]


def test_config_sha256_recomputes(cfg):
    body = {k: v for k, v in cfg.items() if k != "config_sha256"}
    assert hashutil.sha256_canonical(body) == cfg["config_sha256"]


def test_python_frozen_matches_json(cfg):
    expected = build_frozen_config()
    # baseline hashes are freeze-time; compare science fields only
    for key in expected:
        if key == "baseline_integrity":
            continue
        assert cfg[key] == expected[key], key


def test_matlab_config_asserts_literals():
    text = open(paths.CONFIG_M, encoding="utf-8").read()
    for needle in (
        "0.5296", "27.781415", "13.671476",
        "b9dcf268f533a9a93c73bbaeeed3a6dca6b15227cfd868fb459c46fcc81cf045",
        "PROVISIONAL_PENDING_QUALIFICATION",
        "1.13e5", "phase4f-t445-corpus-v1", "phase4f-t445-bootstrap-v1",
        "mfilename('fullpath')",
    ):
        assert needle in text, needle
    assert re.search(r"/Users/|/home/|[A-Za-z]:[\\/]", text) is None


def test_fixture_manifest_schema_and_overlap():
    man = load_json(paths.FIXTURE_MANIFEST)
    assert man["schema"] == "t445_qualification_fixture_manifest_v1"
    assert man["n_fixtures"] == len(man["fixtures"])
    assert man["n_fixtures"] >= 20
    purposes = {f["purpose"] for f in man["fixtures"]}
    for req in (
        "B0_REGRESSION", "SIMSTATE_EQUIVALENCE", "B2_OPTIMIZER_Q1",
        "B2_OPTIMIZER_Q2", "B2_OPTIMIZER_Q3", "B2_OPTIMIZER_Q4",
        "B2_OPTIMIZER_Q5", "B2_OPTIMIZER_Q6", "K_EQ_CHARGE_RESTORATION",
        "B3_PARITY", "ZOH_MACHINE_TEST",
    ):
        assert req in purposes, req
    ids = [f["fixture_id"] for f in man["fixtures"]]
    assert len(ids) == len(set(ids))
    for f in man["fixtures"]:
        assert f["formal_val_overlap"] is False
        assert f["iid_test_overlap"] is False
        assert f["ood_overlap"] is False
        for k in (
            "fixture_id", "source_type", "purpose",
            "generator_or_config_hash", "seed_or_hash_rule",
        ):
            assert f.get(k) not in (None, ""), k
    b0 = [f for f in man["fixtures"] if f["purpose"] == "B0_REGRESSION"]
    assert len(b0) == 3


def test_env_manifest_macos_local_no_cloud():
    env = load_json(paths.ENV_MANIFEST)
    assert env["schema"] == "t445_env_manifest_v1"
    assert env["host_category"] == "macos_local"
    assert env["platform"] == "Darwin"
    assert env["cloud_result_bearing_path_present"] is False
    assert env["autodl_result_bearing_path_present"] is False
    assert env["matlab_required"] == "R2026a"


def test_e_guard_and_baselines():
    guard = load_json(paths.E_GUARD)
    assert guard["pass"] is True
    assert guard["static_scan"]["n_hits"] == 0
    assert guard["snapshot"]["n_predictor_v3_files"] > 0
    base = load_json(paths.BASELINE_HASHES)
    for key in (
        "gate_baseline_modele.json",
        "gate_baseline_modele_control.json",
        "HybridShip_ModelE.slx",
        "build_model_e.m",
    ):
        rec = base["files"][key]
        assert rec["exists"] is True
        assert rec["sha256"] and len(rec["sha256"]) == 64


def test_readme_registration():
    readme = open(os.path.join(paths.MODELE_ROOT, "README.md"), encoding="utf-8").read()
    model = open(os.path.join(paths.MODELE_ROOT, "README_Model.md"), encoding="utf-8").read()
    py_readme = open(os.path.join(paths.MODELE_ROOT, "python", "README.md"), encoding="utf-8").read()
    t445_readme = open(os.path.join(PKG, "README.md"), encoding="utf-8").read()
    res_readme = open(os.path.join(paths.RESULTS_DIR, "README.md"), encoding="utf-8").read()
    for text in (readme, model, py_readme, t445_readme, res_readme):
        assert "t445" in text.lower() or "T-445" in text
    assert "phase4f_t445_config" in readme
    assert "t445_oracle_gate" in readme
    assert "artifacts/t445" in readme


def test_no_absolute_paths_in_t445_code():
    from t445.freeze_t001 import abs_path_scan
    scan = abs_path_scan()
    assert scan["pass"], scan["hits"]

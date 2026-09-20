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
    K_EQ_E_KG_PER_KWH,
    K_EQ_STATUS_PROVISIONAL,
    N_DLAMBDA,
    N_PBAR_COLON,
    N_SOC_B2,
    PROVENANCE_B1_NORM,
    SUPERSEDED_CONFIG_SHA256_PRE_A_PRIME,
    SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C,
    W_TERM,
    build_frozen_config,
    derive_keq_fec,
    derive_lambda_table,
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
    assert cfg["k_eq"]["value_kg_per_kWh"] == K_EQ_E_KG_PER_KWH == 0.5457
    assert cfg["k_eq"]["status"] in (
        K_EQ_STATUS_PROVISIONAL, "CANONICAL")
    assert cfg["k_eq"]["bat_cap_kWh"] == 1071.36
    assert cfg["k_eq"]["bsfc_ref_E_g_per_kWh"] == 482.749
    assert cfg["k_eq"]["eta_chg_FEC_ref"] == 0.9403
    assert cfg["k_eq"]["eta_chg_ocv_diagnostic"] == 0.9687
    assert "eta_chg_ref" not in cfg["k_eq"]
    assert cfg["k_eq"]["eta_dis_ref"] == 0.9686
    assert cfg["k_eq"]["eta_path_E"] == 0.8847
    assert cfg["k_eq"]["eta_path_FEC_E"] == 0.8847
    assert cfg["k_eq"]["fec_energy_basis"] == "nominal_SOC_kWh"
    assert cfg["k_eq"]["robustness_0p8x"] == 0.4366
    assert cfg["k_eq"]["robustness_1p2x"] == 0.6548
    assert cfg["k_eq"]["no_double_counting_of_energy_loss"] is True
    assert cfg["k_eq"]["model_d_k_eq_0p499_revoked"] is True
    assert cfg["k_eq"]["do_not_use_eta_chg_measured_0p7201"] is True


def test_keq_fec_derivation_nominal_soc():
    d = derive_keq_fec()
    eta = 0.98 * 576.0 / 600.33
    assert abs(d["eta_chg_FEC_ref_raw"] - eta) < 1e-12
    assert abs(d["eta_chg_FEC_ref"] - 0.9403) < 1e-12
    assert abs(d["eta_chg_FEC_ref_raw"] - 0.9403) < 5e-5
    path = 0.97 * 0.97 * 0.9403
    assert abs(path - 0.8847) < 5e-5
    keq = 0.482749 / 0.8847
    assert abs(keq - 0.5457) < 5e-5
    w = 2.0 * 0.5457 * 1071.36 / 0.01
    assert abs(w - 1.17e5) < 500.0
    # Must not absorb the mismatched-boundary 0.7201 measurement.
    assert abs(0.482749 / 0.7201 - 0.5457) > 0.05


def test_b1_norm_literals(cfg):
    assert cfg["b1_norm"]["E_ref_kWh"] == 27.781415
    assert cfg["b1_norm"]["E_scale_kWh"] == 13.671476
    assert cfg["b1_norm"]["provenance_sha256"] == PROVENANCE_B1_NORM
    assert cfg["b1_norm"]["per_voyage_refit_forbidden"] is True


def test_lambda_and_delta(cfg):
    assert cfg["lambda_law"]["lambda0"] == 0.425
    assert cfg["lambda_law"]["K_soc"] == 30.0
    assert cfg["lambda_law"]["SOC_ref"] == 0.60
    assert cfg["lambda_law"]["K_S_B1"] == 0.01
    assert cfg["lambda_law"]["K_S_B2_num"] == 1
    assert cfg["lambda_law"]["K_S_B2_den"] == 30
    assert cfg["lambda_law"]["floor"] == 0.43
    assert cfg["lambda_law"]["upper_cap"] is None
    assert "max(lambda_raw, 0.43)" in cfg["lambda_law"]["envelope"]
    assert "lambda_upper_cap_activation_fraction" in cfg["lambda_law"]["removed_diagnostics"]
    assert cfg["delta_lambda"]["min_kg_per_kWh"] == -0.10
    assert cfg["delta_lambda"]["max_kg_per_kWh"] == 0.10
    assert cfg["delta_lambda"]["step"] == 0.005
    assert cfg["delta_lambda"]["n_levels"] == N_DLAMBDA == 41
    assert cfg["delta_lambda"]["rate_limit"] is False


def test_lambda_table_derived_from_primitives(cfg):
    d = derive_lambda_table()
    pt = cfg["b2_policy_table"]
    recomputed = (
        pt["lambda0"]
        + pt["K_soc"] * (pt["SOC_ref"] - pt["SOC_DP_min"])
        + pt["delta_lambda_max"]
    )
    assert abs(recomputed - 15.525) < 1e-12
    assert abs(pt["lambda_table_max"] - recomputed) < 1e-12
    assert pt["lambda_table_min"] == 0.43
    assert pt["n_lambda"] == 3020 == d["n_lambda"]
    assert pt["lambda_colon"] == [0.43, 0.005, 15.525]
    assert pt["oob_query_count_required"] == 0
    assert cfg["amendment"]["superseded_config_sha256"] == SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C
    assert cfg["amendment"]["prior_a_prime_sha256"] == SUPERSEDED_CONFIG_SHA256_PRE_A_PRIME
    assert cfg["amendment"]["id"] == "C_fec_nominal_soc_2026-09-15"
    assert cfg["k_eq"]["status"] == "CANONICAL"


def test_v_term_and_terminal(cfg):
    assert cfg["v_term"]["delta_term"] == 0.01
    assert cfg["v_term"]["w_term"] == W_TERM == 1.17e5
    assert cfg["v_term"]["amended_with_k_eq"] is True
    assert cfg["v_term"]["soc_target_policy"] == "SOC0_voyage"
    assert cfg["terminal_soc_admissibility"]["abs_dsoc_max"] == 0.005
    assert cfg["gate_thresholds"]["X_pct"] == 2.0
    assert cfg["gate_thresholds"]["Y_pct"] == 0.0
    assert cfg["seeds"]["corpus"] == "phase4f-t445-corpus-v1"
    assert cfg["seeds"]["bootstrap"] == "phase4f-t445-bootstrap-v1"
    assert cfg["bootstrap"]["B"] == 10000
    assert cfg["epsilon_num"]["rel_of_j_fec_b2"] == 0.005


def test_grids(cfg):
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
        "0.5457", "27.781415", "13.671476",
        "b9dcf268f533a9a93c73bbaeeed3a6dca6b15227cfd868fb459c46fcc81cf045",
        "PROVISIONAL_CANDIDATE_PENDING_T007_REQUALIFICATION",
        "1.17e5", "phase4f-t445-corpus-v1", "phase4f-t445-bootstrap-v1",
        "mfilename('fullpath')", "15.525", "3020",
    ):
        assert needle in text, needle
    assert "T445:cap" not in text
    assert "lambda_eff in [0.43, 0.80]" not in text
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
        "K_EQ_ENERGY_LADDER", "K_EQ_IDENTITY_QUALIFICATION",
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
    keq_fail = [f for f in man["fixtures"] if f["purpose"] == "K_EQ_CHARGE_RESTORATION"]
    assert len(keq_fail) == 1
    assert keq_fail[0]["status"] == "PRESERVED_FAIL_AMENDMENT_C"
    ident = [f for f in man["fixtures"] if f["purpose"] == "K_EQ_IDENTITY_QUALIFICATION"]
    assert len(ident) == 1


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

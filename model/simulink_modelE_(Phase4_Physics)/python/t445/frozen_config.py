"""T-445 frozen parameter dictionary (design.md DGN-F-01~F-15 🔒).

This module is the Python-side freeze table used to export
``config/phase4f_t445_config.json``. Numerical values are the design-frozen
rounded literals, not live recomputations.
"""
from __future__ import annotations

PROVENANCE_B1_NORM = (
    "b9dcf268f533a9a93c73bbaeeed3a6dca6b15227cfd868fb459c46fcc81cf045"
)
SPLIT_ASSIGNMENT_SHA256 = (
    "ac4c4a7629534daf1c4d88b05848fbfe1bb1dc148679d0b7abbbe873f66c809f"
)

# colon n = floor((end-start)/step)+1
N_PBAR_COLON = int(round(1300 / 20)) + 1  # 66
N_DLAMBDA = int(round(0.20 / 0.005)) + 1  # 41
N_SOC_B2 = int(round((0.95 - 0.10) / 0.01)) + 1  # 86

# DGN-F-03/F-09 Amendment a′: native law=6 domain from frozen primitives
LAMBDA0 = 0.425
K_SOC = 30.0
SOC_REF = 0.60
SOC_DP_MIN = 0.10
SOC_DP_MAX = 0.95
DLAMBDA_MAX = 0.10
LAMBDA_FLOOR = 0.43
LAMBDA_TABLE_STEP = 0.005
SUPERSEDED_CONFIG_SHA256_PRE_A_PRIME = (
    "25605c03b9f528425398316d47531a6225b8c9ac4cb9e4d32145a808630f0f33"
)


def derive_lambda_table() -> dict:
    """Recompute strategy-table lambda domain from frozen primitives."""
    table_max = LAMBDA0 + K_SOC * (SOC_REF - SOC_DP_MIN) + DLAMBDA_MAX
    table_min = LAMBDA_FLOOR
    n_lambda = int(round((table_max - table_min) / LAMBDA_TABLE_STEP)) + 1
    return {
        "lambda0": LAMBDA0,
        "K_soc": K_SOC,
        "SOC_ref": SOC_REF,
        "SOC_DP_min": SOC_DP_MIN,
        "SOC_DP_max": SOC_DP_MAX,
        "delta_lambda_max": DLAMBDA_MAX,
        "lambda_floor": LAMBDA_FLOOR,
        "lambda_table_step": LAMBDA_TABLE_STEP,
        "lambda_table_min": table_min,
        "lambda_table_max": table_max,
        "lambda_colon": [table_min, LAMBDA_TABLE_STEP, table_max],
        "n_lambda": n_lambda,
        "derivation": (
            "lambda_table_max = lambda0 + K_soc*(SOC_ref-SOC_DP_min) + dlambda_max"
        ),
    }


def build_frozen_config() -> dict:
    return {
        "schema": "phase4f_t445_config_v1",
        "spec": "phase4-f-trajectory-oracle-gate",
        "task_created": "T-001",
        "amendment": {
            "id": "a_prime_dgn_f03_2026-09-15",
            "superseded_config_sha256": SUPERSEDED_CONFIG_SHA256_PRE_A_PRIME,
            "note": (
                "Removed design-only 0.80 cap; native law=6 envelope; "
                "policy table lambda from frozen primitives."
            ),
        },
        "k_eq": {
            "value_kg_per_kWh": 0.5296,
            "status": "PROVISIONAL_PENDING_QUALIFICATION",
            "canonical_after_task": "T-007",
            "bsfc_ref_E_g_per_kWh": 482.749,
            "eta_rect": 0.97,
            "eta_dcdc": 0.97,
            "eta_chg_ref": 0.9687,
            "eta_dis_ref": 0.9686,
            "eta_path_E": 0.9111,
            "robustness_0p8x": 0.4237,
            "robustness_1p2x": 0.6355,
            "bat_cap_kWh": 1071.36,
            "no_double_counting_of_energy_loss": True,
            "battery_aggregation_basis_check": True,
            "assumed_bat_eff_excluded_from_eta_path": True,
            "model_d_k_eq_0p499_revoked": True,
        },
        "b1_norm": {
            "transform": "affine_global_single",
            "E_ref_kWh": 27.781415,
            "E_scale_kWh": 13.671476,
            "unit": "kWh",
            "provenance_sha256": PROVENANCE_B1_NORM,
            "split_assignment_sha256": SPLIT_ASSIGNMENT_SHA256,
            "source_split": "train",
            "n_t0": 159472,
            "E300_min_kWh": 0.236,
            "E300_max_kWh": 75.041,
            "per_voyage_refit_forbidden": True,
        },
        "lambda_law": {
            "lambda0": LAMBDA0,
            "K_soc": K_SOC,
            "SOC_ref": SOC_REF,
            "K_S_B1": 0.01,
            "K_S_B2_num": 1,
            "K_S_B2_den": 30,
            "floor": LAMBDA_FLOOR,
            "upper_cap": None,
            "envelope": "lambda_eff = max(lambda_raw, 0.43)",
            "floor_applies_when": "law6_KS_nonzero",
            "b0_has_floor": False,
            "diagnostics": [
                "lambda_floor_activation_fraction",
                "lambda_eff_min",
                "lambda_eff_max",
            ],
            "removed_diagnostics": ["lambda_upper_cap_activation_fraction"],
        },
        "delta_lambda": {
            "min_kg_per_kWh": -0.10,
            "max_kg_per_kWh": 0.10,
            "step": 0.005,
            "n_levels": N_DLAMBDA,
            "rate_limit": False,
        },
        "v_term": {
            "delta_term": 0.01,
            "w_term": 1.13e5,
            "w_term_unit": "kg/SOC^2",
            "soc_target_policy": "SOC0_voyage",
            "d_cal": 0.02,
            "corpus_soc0_machine_assert": 0.60,
        },
        "terminal_soc_admissibility": {
            "abs_dsoc_max": 0.005,
            "rule_id": "TERMINAL_SOC_ADMISSIBILITY",
            "infeasible_label": "INFEASIBLE_OR_NONCOMPARABLE",
        },
        "gate_thresholds": {
            "X_pct": 2.0,
            "Y_pct": 0.0,
        },
        "seeds": {
            "corpus": "phase4f-t445-corpus-v1",
            "bootstrap": "phase4f-t445-bootstrap-v1",
        },
        "bootstrap": {
            "B": 10000,
            "initial_groups_per_tier": 3,
            "expanded_groups_per_tier": 4,
            "ci": "percentile_95_two_sided",
            "unit": "split_group_cluster",
        },
        "epsilon_num": {
            "rel_of_j_fec_b2": 0.005,
        },
        "oracle_window": {
            "refresh_s": 10,
            "horizon_s": 300,
            "K": 30,
            "partial_bin": "duration_weighted",
            "shrinking_horizon": True,
            "zero_fill_forbidden": True,
            "sample_and_hold": True,
        },
        "b2_policy_table": {
            **derive_lambda_table(),
            "pbar_colon_kW": [0, 20, 1300],
            "n_pbar": N_PBAR_COLON,
            "n_soc_regions": 3,
            "soc_regions": ["normal", "no_discharge_le_0.105", "no_charge_ge_0.945"],
            "oob_query_count_required": 0,
            "no_upper_clip_or_extrapolation": True,
        },
        "b2_dp": {
            "K": 30,
            "dt_s": 10,
            "soc_colon": [0.10, 0.01, 0.95],
            "n_soc": N_SOC_B2,
            "p_g_prev_colon_kW": [0, 20, 1000],
            "eta_chg": 0.9687,
            "eta_dis": 0.9686,
            "eta_dcdc": 0.97,
            "infeasible_fallback": "delta_lambda_zero",
            "persistent_infeas_share": 0.05,
            "window_budget_s": 2.0,
            "big_M": 10000.0,
        },
        "b3": {
            "p_grid": "union_0_and_125_20_1000",
            "ramp_kW_per_s_per_on": 20.0,
            "c_start_kg": 0.6,
            "soc_colon": [0.10, 0.005, 0.95],
            "dt_s": 1,
            "terminal_band": 0.005,
            "epsilon_num_rel": 0.005,
        },
        "hard_feasibility": {
            "task_complete": {
                "pass_rule": "Voyage_Stop_Logic_normal_and_abs_Tend_minus_Tfrozen_le_1s",
                "abs_t_end_tol_s": 1.0,
            },
            "load_shortfall": {
                "derate08_duration_s": 0.0,
                "bus_shortfall_energy_kWh": 0.0,
                "p_residual_neg_thresh_kW": -1.0,
            },
            "soc_hard_bounds": {
                "soc_min": 0.10,
                "soc_max": 0.95,
                "violation_duration_s": 0.0,
            },
            "terminal_soc": {
                "abs_dsoc_max": 0.005,
            },
            "battery_sop_v_i": {
                "p_over_avail_duration_s": 0.0,
                "v_min_V": 360.0,
                "v_max_V": 657.0,
                "i_abs_max_A": 1823.0,
                "duration_tol_s": 1e-6,
            },
            "genset_limits": {
                "p_min_kW": 125.0,
                "p_max_kW": 500.0,
                "ramp_kW_per_s": 20.0,
                "ramp_tol_kW_per_s": 1e-6,
                "on_out_of_band_duration_s": 0.0,
                "c2_source": "gate_baseline_modele_control.json_v1.0",
            },
            "energy_closure_c5": {
                "rel_residual_max": 0.001,
                "source": "gate_ctrl.check_c5",
            },
            "safety_hard_events": {
                "s1a_unexpected_loss_count": 0,
                "s1b_start_fail_count": 0,
                "s2_realized_deficit_count": 0,
                "source": "C6_gate_baseline_modele_control.json_v1.0",
            },
        },
        "audit_capability_matrix": [
            {"signal": "task_complete_duration", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream+Voyage_Stop_Logic", "hard": True},
            {"signal": "speed_tracking_rms_max", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream_v_ref_v_ship", "hard": False},
            {"signal": "load_shortfall_propulsion_gap", "status": "AVAILABLE_AND_CHECKED",
             "source": "09_bus_residual+08_derate", "hard": True},
            {"signal": "soc_hard_bound_time", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream_SOC", "hard": True},
            {"signal": "battery_sop_v_i", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream_P_bat_avail_V_I+ECM_flags", "hard": True},
            {"signal": "genset_pmin_pmax_ramp", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream_state_DG_P_gen_actual", "hard": True},
            {"signal": "terminal_soc_admissibility", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream_SOC", "hard": True},
            {"signal": "bus_energy_closure", "status": "AVAILABLE_AND_CHECKED",
             "source": "gate_ctrl.check_c5", "hard": True},
            {"signal": "safety_s1_s5_count_duration", "status": "AVAILABLE_AND_CHECKED",
             "source": "logsout_safety_flags", "hard": True},
            {"signal": "pms_override_count_duration", "status": "AVAILABLE_AND_CHECKED",
             "source": "logsout_nine_tuple", "hard": False},
            {"signal": "startstop_tv_pgen_ramp_activation", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream_derived", "hard": False},
            {"signal": "e_bat_throughput_efc_proxy", "status": "AVAILABLE_AND_CHECKED",
             "source": "stream_P_bat_actual", "hard": False},
        ],
        "corpus_policy": {
            "split": "validation",
            "n_val_split_groups_expected": 27,
            "n_val_voyages_expected": 108,
            "initial_groups": 9,
            "initial_voyages": 36,
            "reserve_groups": 3,
            "reserve_voyages": 12,
            "test_sealed_required": True,
            "iid_test_access_forbidden": True,
            "ood_access_forbidden": True,
            "shape_score": {
                "name": "shape_cv",
                "p_epsilon_kW": 1.0,
                "windows": "full_horizon_300s_only",
                "group_agg": "mean_of_four_members",
                "tiers": ["HIGH", "MID", "LOW"],
                "groups_per_tier": 9,
            },
        },
        "verdicts": {
            "trajectory_gate_verdict": [
                "GO_TRAJECTORY_VALUE",
                "NO_GO_TRAJECTORY_VALUE",
                "INCONCLUSIVE",
                "FINAL_INCONCLUSIVE",
                "GATE_VALIDITY_FAIL",
            ],
            "b3_context_status": [
                "UNQUALIFIED_CONTEXT",
                "QUALIFIED_PASS",
                "B3_DOMINANCE_AUDIT_FAIL",
            ],
            "priority": "GATE_VALIDITY_FAIL_first",
        },
        "environment_policy": {
            "platform_required": "macos_local",
            "matlab_required": "R2026a",
            "cloud_result_bearing_forbidden": True,
            "autodl_result_bearing_forbidden": True,
            "e_stage_readonly": True,
            "e_additional_retrain_forbidden": True,
            "winner_model_load_forbidden": True,
        },
        "simstate": {
            "segment_s": 10,
            "equiv_rel_tol": 1e-10,
            "identity": [
                "voyage_id", "arm", "config_hash", "seed",
                "frozen_stream_sha256", "model_checksum", "segment_t0",
            ],
        },
        "zoh": {
            "interpolate": "off",
            "mechanism": "SimulationInput.setBlockParameter_oracle_src",
            "fallback": "in_memory_set_param_restore",
            "persist_slx_forbidden": True,
        },
        "evidence_grade": "DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE",
        "small_n_gate_caution": True,
    }

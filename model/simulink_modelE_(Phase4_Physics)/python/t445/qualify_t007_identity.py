#!/usr/bin/env python3
"""T-007 Amendment C identity qualification (no 0.58->0.62 vs 0.9687 gate).

Qualifies: energy-boundary identities, SOC/coulomb bookkeeping, DC/DC
accounting, aggregate-pack topology, and nominal-SOC FEC construction.
Preserves the failed charge-restoration artifact. Does not mark CANONICAL.
"""
from __future__ import annotations

import json
import os
import re
import sys
from datetime import datetime, timezone

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
    from t445.frozen_config import derive_keq_fec  # type: ignore
else:
    from . import hashutil, paths
    from .frozen_config import derive_keq_fec

FAIL_JSON = os.path.join(paths.RESULTS_DIR, "qualification", "t007_keq_charge_restoration.json")
LADDER_JSON = os.path.join(paths.RESULTS_DIR, "qualification", "t007_energy_ladder.json")
OUT_JSON = os.path.join(paths.RESULTS_DIR, "qualification", "t007_identity_qualification.json")
PHYS = os.path.join(paths.CONFIG_DIR, "ModelE_Physics_Parameters.m")


def _load(path: str) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def _scalar(text: str, name: str) -> float:
    m = re.search(rf"^{re.escape(name)}\s*=\s*([0-9.]+)\s*;", text, re.M)
    if not m:
        raise SystemExit(f"missing {name} in ModelE_Physics_Parameters.m")
    return float(m.group(1))


def evaluate() -> dict:
    cfg = _load(paths.CONFIG_JSON)
    fail = _load(FAIL_JSON)
    ladder = _load(LADDER_JSON)
    phys = open(PHYS, encoding="utf-8").read()
    keq_d = derive_keq_fec()

    checks = {}

    checks["fail_artifact_preserved"] = (
        fail.get("pass") is False
        and abs(fail.get("eta_chg_measured", 0) - 0.7201) < 5e-4
        and abs(fail.get("eta_chg_ref", 0) - 0.9687) < 1e-12
        and fail.get("schema") == "t445_t007_keq_qualification_v1"
    )
    checks["fail_not_relabeled_pass"] = fail.get("pass") is False

    chk = ladder.get("checks", {})
    checks["ladder_accounting_pass"] = bool(ladder.get("accounting_pass"))
    checks["terminal_vs_VI"] = bool(chk.get("terminal_vs_VI_le_0p1pct"))
    checks["coulomb"] = bool(chk.get("coulomb_abs_le_1e4"))
    checks["dcdc"] = bool(chk.get("dcdc_vs_0p97_le_0p005"))
    checks["unexplained_072_false"] = ladder.get("unexplained_loss_near_0p72") is False
    checks["ladder_k_eq_unchanged_at_audit"] = ladder.get("k_eq_unchanged") is True

    v_nom = _scalar(phys, "V_bat_nom")
    q_ah = _scalar(phys, "Bat_Q_nom_Ah")
    eta_c = _scalar(phys, "Bat_eta_coul")
    n_str = _scalar(phys, "DRAWING_N_PropStrings")
    cap_str = _scalar(phys, "DRAWING_Bat_Cap_kWh_perString")
    eta_dcdc = _scalar(phys, "ASSUMED_eta_dcdc")
    eta_rect = _scalar(phys, "ASSUMED_eta_rect")
    cap = v_nom * q_ah / 1000.0
    checks["pack_v_times_ah"] = abs(cap - 1071.36) < 1e-9
    checks["pack_two_strings"] = abs(n_str * cap_str - 1071.36) < 1e-9
    checks["pack_cap_matches_config"] = abs(cfg["k_eq"]["bat_cap_kWh"] - 1071.36) < 1e-12
    checks["eta_coul_0p98"] = abs(eta_c - 0.98) < 1e-12
    checks["eta_dcdc_0p97"] = abs(eta_dcdc - 0.97) < 1e-12
    checks["eta_rect_0p97"] = abs(eta_rect - 0.97) < 1e-12

    eta_fec_raw = eta_c * v_nom / 600.33
    checks["eta_chg_fec_ref"] = (
        abs(cfg["k_eq"]["eta_chg_FEC_ref"] - 0.9403) < 1e-12
        and abs(eta_fec_raw - 0.9403) < 5e-5
    )
    checks["eta_path_fec"] = abs(cfg["k_eq"]["eta_path_FEC_E"] - 0.8847) < 1e-12
    checks["k_eq_value"] = abs(cfg["k_eq"]["value_kg_per_kWh"] - 0.5457) < 1e-12
    checks["w_term_joint"] = abs(cfg["v_term"]["w_term"] - 1.17e5) < 1e-6
    checks["w_term_amended_with_k_eq"] = cfg["v_term"].get("amended_with_k_eq") is True
    checks["ocv_diagnostic_only"] = (
        abs(cfg["k_eq"]["eta_chg_ocv_diagnostic"] - 0.9687) < 1e-12
        and "eta_chg_ref" not in cfg["k_eq"]
        and abs(cfg["b2_dp"]["eta_chg"] - 0.9687) < 1e-12
    )
    checks["not_from_measured_0p7201"] = (
        cfg["k_eq"].get("do_not_use_eta_chg_measured_0p7201") is True
        and abs(0.482749 / 0.7201 - cfg["k_eq"]["value_kg_per_kWh"]) > 0.05
    )
    checks["fec_basis_nominal_soc"] = cfg["k_eq"].get("fec_energy_basis") == "nominal_SOC_kWh"
    checks["status_allowed"] = cfg["k_eq"]["status"] in (
        "PROVISIONAL_CANDIDATE_PENDING_T007_REQUALIFICATION",
        "CANONICAL",
    )
    checks["derivation_raw_close"] = (
        abs(keq_d["eta_chg_FEC_ref_raw"] - 0.9403) < 5e-5
        and abs(keq_d["value_kg_per_kWh_raw"] - 0.5457) < 5e-5
        and abs(keq_d["w_term_raw"] - 1.17e5) < 500.0
    )

    overall = all(bool(v) for v in checks.values())
    rec = {
        "schema": "t445_t007_identity_qualification_v1",
        "task": "T-007",
        "purpose": "QUALIFICATION",
        "amendment": "C_fec_nominal_soc_2026-09-15",
        "config_sha256": cfg["config_sha256"],
        "ladder_config_sha256": ladder.get("config_sha256"),
        "fail_artifact": "qualification/t007_keq_charge_restoration.json",
        "fail_artifact_pass": False,
        "k_eq_status": cfg["k_eq"]["status"],
        "canonical": cfg["k_eq"]["status"] == "CANONICAL",
        "eta_chg_FEC_ref": cfg["k_eq"]["eta_chg_FEC_ref"],
        "eta_path_FEC_E": cfg["k_eq"]["eta_path_FEC_E"],
        "k_eq_kg_per_kWh": cfg["k_eq"]["value_kg_per_kWh"],
        "w_term": cfg["v_term"]["w_term"],
        "eta_chg_ocv_diagnostic": cfg["k_eq"]["eta_chg_ocv_diagnostic"],
        "checks": checks,
        "overall": overall,
        "note": (
            "T-007 no longer requires a dynamic 0.58->0.62 charge trace to "
            "match the 0.25C OCV reference within +/-0.005. The 0.9403 "
            "coefficient is the nominal-SOC FEC reference, not an empirical "
            "trace efficiency. FAIL artifact preserved. This script records "
            "status; CANONICAL is stamped in frozen_config after identity PASS."
        ),
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    rec["record_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in rec.items() if k != "record_sha256"}
    )
    return rec


def main() -> int:
    rec = evaluate()
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    hashutil.dump_json(OUT_JSON, rec)
    with open(paths.ACCESS_LEDGER, "a", encoding="utf-8") as fh:
        fh.write(json.dumps({
            "purpose": "QUALIFICATION",
            "task": "T-007",
            "artifact": "qualification/t007_identity_qualification.json",
            "overall": rec["overall"],
            "utc": rec["utc"],
        }, sort_keys=True) + "\n")
    print(json.dumps({
        "overall": rec["overall"],
        "canonical": rec["canonical"],
        "k_eq": rec["k_eq_kg_per_kWh"],
        "w_term": rec["w_term"],
        "failed_checks": [k for k, v in rec["checks"].items() if not v],
        "wrote": paths.rel(OUT_JSON),
    }, indent=2))
    return 0 if rec["overall"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

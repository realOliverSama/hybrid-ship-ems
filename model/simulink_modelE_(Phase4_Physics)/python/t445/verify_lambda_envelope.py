#!/usr/bin/env python3
"""T-003 R4: native law=6 envelope (Amendment a′).

Success → LAW6_NATIVE_ENVELOPE_CONFIRMED.
The prior R4_CAP_NOT_LEGALLY_IMPLEMENTABLE JSON is provenance and is not deleted.
"""
from __future__ import annotations

import os
import sys
from datetime import datetime, timezone

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
    from t445.frozen_config import derive_lambda_table  # type: ignore
else:
    from . import hashutil, paths
    from .frozen_config import derive_lambda_table


def _read(rel: str) -> str:
    with open(os.path.join(paths.MODELE_ROOT, rel), encoding="utf-8") as fh:
        return fh.read()


def law6_lambda_eff(lambda0, k_soc, soc_ref, k_s, soc, e_hat, floor=0.43):
    """Bit-replica of frozen law=6 lambda processing (floor only, no cap)."""
    soc_ref_eff = soc_ref + k_s * e_hat
    lam = lambda0 + k_soc * (soc_ref_eff - soc)
    if k_s != 0:
        lam = max(lam, floor)
    return lam


def main() -> int:
    oracle = _read(os.path.join("scripts", "+model", "build_model_e.m"))
    law = _read(os.path.join("scripts", "+ecms2", "ecms_law.m"))
    derived = derive_lambda_table()

    floor_oracle = "lam = max(lam, Lambda_Floor)" in oracle
    floor_law = "lambda = max(lambda, params.lambda_floor)" in law
    has_floor_const = "'Value', 'ECMS_Lambda_Floor'" in oracle
    cap_ident = any(
        s in oracle + "\n" + law
        for s in ("Lambda_Cap", "lambda_cap", "ECMS_Lambda_Cap", "min(lam, 0.80)", "min(lambda, 0.80)")
    )
    hidden_cap = cap_ident or ("'Value', 'ECMS_Lambda_Cap'" in oracle)

    # Fixture: maximum raw lambda in the B2 legal domain.
    lam_raw = derived["lambda_table_max"]  # 15.525
    lam_eff = law6_lambda_eff(
        derived["lambda0"], derived["K_soc"], derived["SOC_ref"],
        1.0 / 30.0, derived["SOC_DP_min"], derived["delta_lambda_max"],
        derived["lambda_floor"],
    )
    # B2: e_hat := Delta_lambda, K_S=1/30 → K_soc*K_S*e_hat = Delta_lambda
    # lambda_raw = lambda0 + K_soc*(SOC_ref-SOC) + Delta_lambda
    same = abs(lam_eff - lam_raw) < 1e-12
    stays_above = lam_eff > 0.80

    slx = os.path.join(paths.MODELE_ROOT, "model", "HybridShip_ModelE.slx")
    slx_sha = hashutil.sha256_file(slx)
    base_path = os.path.join(paths.RESULTS_DIR, "baseline_integrity_t001.json")
    with open(base_path, encoding="utf-8") as fh:
        import json as _json
        base = _json.load(fh)
    slx_base = base["files"]["HybridShip_ModelE.slx"]["sha256"]
    slx_unchanged = slx_sha == slx_base

    same_core = (
        "buildOracleECMSVariant" in oracle
        and oracle.count("function c = CODE_ECMS_ORACLE()") == 1
        and "EMS_Oracle_ECMS" in oracle
    )

    old_stop = os.path.join(paths.RESULTS_DIR, "lambda_envelope_r4.json")
    old_present = os.path.isfile(old_stop)

    checks = {
        "floor_0p43_in_CODE_ECMS_ORACLE": floor_oracle,
        "floor_0p43_in_ecms_law": floor_law,
        "floor_constant_block": has_floor_const,
        "no_hidden_upper_cap": not hidden_cap,
        "fixture_lambda_raw_gt_0p80": lam_raw > 0.80,
        "fixture_lambda_eff_stays_gt_0p80": stays_above and same,
        "b1_b2_same_law6_core": same_core,
        "slx_checksum_unchanged": slx_unchanged,
        "prior_r4_stop_artifact_retained": old_present,
    }
    overall = all(checks.values())
    rec = {
        "schema": "t445_lambda_envelope_r4_native_v1",
        "task": "T-003",
        "amendment": "a_prime_dgn_f03_2026-09-15",
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "status": "LAW6_NATIVE_ENVELOPE_CONFIRMED" if overall else "R4_NATIVE_ENVELOPE_FAIL",
        "envelope": "lambda_eff = max(lambda_raw, 0.43)",
        "upper_cap": None,
        "fixture": {
            "lambda_raw": lam_raw,
            "lambda_eff": lam_eff,
            "SOC": derived["SOC_DP_min"],
            "Delta_lambda": derived["delta_lambda_max"],
            "K_S": 1.0 / 30.0,
        },
        "slx_sha256": slx_sha,
        "slx_sha256_t001_baseline": slx_base,
        "prior_stop_artifact": "lambda_envelope_r4.json",
        "checks": checks,
        "overall": overall,
    }
    rec["record_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in rec.items() if k != "record_sha256"}
    )
    os.makedirs(paths.RESULTS_DIR, exist_ok=True)
    out = os.path.join(paths.RESULTS_DIR, "lambda_envelope_r4_native.json")
    hashutil.dump_json(out, rec)
    print(f"status={rec['status']} overall={overall}")
    print(f"lambda_raw={lam_raw} lambda_eff={lam_eff} slx_unchanged={slx_unchanged}")
    print(f"wrote {paths.rel(out)}")
    return 0 if overall else 1


if __name__ == "__main__":
    raise SystemExit(main())

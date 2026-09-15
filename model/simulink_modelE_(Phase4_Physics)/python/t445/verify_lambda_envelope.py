#!/usr/bin/env python3
"""T-003 R4: inspect frozen law=6 for floor 0.43 and cap 0.80.

Does not modify .slx or law=6 source. Writes machine-readable envelope JSON.
"""
from __future__ import annotations

import os
import re
import sys
from datetime import datetime, timezone

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
else:
    from . import hashutil, paths


def _read(rel_under_modele: str) -> str:
    p = os.path.join(paths.MODELE_ROOT, rel_under_modele)
    with open(p, encoding="utf-8") as fh:
        return fh.read()


def main() -> int:
    oracle = _read(os.path.join("scripts", "+model", "build_model_e.m"))
    law = _read(os.path.join("scripts", "+ecms2", "ecms_law.m"))

    floor_oracle = "lam = max(lam, Lambda_Floor)" in oracle
    floor_law = "lambda = max(lambda, params.lambda_floor)" in law
    cap_min_oracle = bool(re.search(r"min\(\s*lam\s*,\s*0\.80", oracle))
    cap_min_law = bool(re.search(r"min\(\s*lambda\s*,\s*0\.80", law))
    cap_ident = bool(re.search(r"Lambda_Cap|lambda_cap|ECMS_Lambda_Cap", oracle + "\n" + law))

    # Constant blocks on law=6 variant: Lambda_Floor exists, no Cap
    has_floor_const = "'Value', 'ECMS_Lambda_Floor'" in oracle
    has_cap_const = "ECMS_Lambda_Cap" in oracle

    # Saturation on P_gen only
    sat_is_pgen = (
        "[ch '/clamp']" in oracle
        and "'UpperLimit', 'Gen_P_max'" in oracle
    )

    cap_in_impl = cap_min_oracle or cap_min_law or cap_ident or has_cap_const
    floor_in_impl = floor_oracle and floor_law and has_floor_const

    # Approved run-local mechanisms cannot add a missing min() in MATLAB Function
    run_local = {
        "SimulationInput_setBlockParameter": {
            "can_override_Interpolate": True,
            "can_override_ECMS_Lambda_Floor_constant": True,
            "can_add_lambda_cap_min": False,
            "reason": (
                "setBlockParameter can only change existing block parameters. "
                "law=6 MATLAB Function has no Lambda_Cap input; the P clamp "
                "Saturation is on P_gen_ref not lambda."
            ),
        },
        "per_run_set_param_memory": {
            "can_add_lambda_cap_min": False,
            "reason": (
                "No existing cap parameter to set. Rewriting MATLAB Function "
                "script would modify law=6 core control logic (FR-721 forbidden)."
            ),
        },
        "assignin_workspace": {
            "can_add_lambda_cap_min": False,
            "reason": "ECMS_Lambda_Floor is a floor only; no cap workspace variable is consumed.",
        },
    }

    cap_legal = False
    stop = (not cap_in_impl) and (not cap_legal)

    rec = {
        "schema": "t445_lambda_envelope_r4_v1",
        "task": "T-003",
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "floor": {
            "declared": 0.43,
            "present_in_CODE_ECMS_ORACLE": floor_oracle,
            "present_in_ecms_law": floor_law,
            "constant_block_ECMS_Lambda_Floor": has_floor_const,
            "applies_when": "K_S ~= 0",
            "pass": floor_in_impl,
        },
        "cap": {
            "declared_dgn_f_03": 0.80,
            "present_in_CODE_ECMS_ORACLE": cap_min_oracle,
            "present_in_ecms_law": cap_min_law,
            "constant_block_ECMS_Lambda_Cap": has_cap_const,
            "identifier_hit": cap_ident,
            "pass": cap_in_impl,
        },
        "p_gen_saturation_not_lambda": sat_is_pgen,
        "run_local_mechanisms": run_local,
        "cap_implementable_via_approved_run_local": cap_legal,
        "logsout_reconstruction": {
            "ran": False,
            "reason": "Structural STOP fires before regression voyages: cap absent and not legally injectable.",
        },
        "stop": stop,
        "stop_code": "R4_CAP_NOT_LEGALLY_IMPLEMENTABLE" if stop else None,
        "decision_required": True,
        "candidates": [
            {
                "id": "a",
                "summary": (
                    "Revise DGN-F-03: drop the 0.80 upper-cap claim; extend the "
                    "B2 policy-table lambda axis to the reachable domain "
                    "(e.g. [0.43, 2.0])."
                ),
            },
            {
                "id": "b",
                "summary": "Other user-approved form (explicit Design revision).",
            },
        ],
        "do_not": [
            "Do not add a cap by editing law=6 MATLAB Function (core logic).",
            "Do not persist .slx structure (DGN-F-01).",
            "Do not let the optimizer wear a 0.80 hat while the live controller has none.",
            "Do not continue T-004+ until Design review.",
        ],
    }
    rec["record_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in rec.items() if k != "record_sha256"}
    )

    os.makedirs(paths.RESULTS_DIR, exist_ok=True)
    out = os.path.join(paths.RESULTS_DIR, "lambda_envelope_r4.json")
    hashutil.dump_json(out, rec)

    inc = {
        "utc": rec["utc"],
        "task": "T-003",
        "class": "DESIGN_STOP",
        "code": rec["stop_code"],
        "artifact": "lambda_envelope_r4.json",
        "record_sha256": rec["record_sha256"],
    }
    with open(paths.INCIDENTS, "a", encoding="utf-8") as fh:
        fh.write(hashutil.canonical_json_bytes(inc).decode("utf-8") + "\n")

    print(f"stop={stop} floor_pass={floor_in_impl} cap_pass={cap_in_impl}")
    print(f"wrote {paths.rel(out)}")
    return 2 if stop else 0


if __name__ == "__main__":
    raise SystemExit(main())

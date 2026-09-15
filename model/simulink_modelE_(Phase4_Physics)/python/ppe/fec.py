"""T-006 — J_FEC / fuel_raw / TERMINAL_SOC / feasibility status machine.

FR-840/840a/850/850a/854/857; AC-805/807/813/814. No pair-delete, no impute.
"""
from __future__ import annotations

import json

from t445 import hashutil

from . import paths

BAT_CAP_KWH = 1071.36
K_EQ_E = 0.5457
TERMINAL_DSOC_MAX = 0.005
HF_OBJECT_SHA256 = (
    "fcef573d06e414bef8b64f10864def24ad769195d32362b1f57d0b312aac9def")

ESTIMAND_ARMS = {"B_perfect": ("C0", "C1"), "B_predicted": ("C0", "C2"),
                 "L_prediction": ("C1", "C2")}  # L also requires C0 (FR-854)


class FeasibilityError(RuntimeError):
    pass


def j_fec(fuel_raw_kg: float, soc0: float, soc_end: float) -> float:
    """FR-840: fuel + charge-restoration correction on nominal-SOC basis."""
    return float(fuel_raw_kg) + (float(soc0) - float(soc_end)) * BAT_CAP_KWH * K_EQ_E


def terminal_soc_admissible(soc0: float, soc_end: float) -> bool:
    """FR-850a / AC-814: HARD, separate from ordinary [0.10, 0.95] bounds."""
    return abs(float(soc_end) - float(soc0)) <= TERMINAL_DSOC_MAX


def verify_hf_config() -> dict:
    """AC-813: study-local HF object == T-445 hard_feasibility canonical."""
    with open(paths.HF_CONFIG_JSON, encoding="utf-8") as fh:
        ppe = json.load(fh)
    with open(paths.T445_CONFIG_JSON, encoding="utf-8") as fh:
        t445 = json.load(fh)
    obj_ppe = hashutil.sha256_canonical(ppe["hard_feasibility"])
    obj_t445 = hashutil.sha256_canonical(t445["hard_feasibility"])
    ok = (obj_ppe == obj_t445 == HF_OBJECT_SHA256)
    if not ok:
        raise FeasibilityError(
            f"HF object mismatch: ppe={obj_ppe} t445={obj_t445}")
    ts = ppe["TERMINAL_SOC_ADMISSIBILITY"]
    if not (ts.get("hard") is True
            and ts.get("source_field") == "terminal_soc.abs_dsoc_max"):
        raise FeasibilityError("TERMINAL_SOC_ADMISSIBILITY record broken")
    return {"hf_object_sha256": obj_ppe, "verdict": "HF_CONFIG_EQUAL"}


def feasibility_record(arm: str, voyage_id: str, predicates: dict) -> dict:
    """Machine-readable per-arm-voyage feasibility (FR-855).

    predicates: {predicate_name: bool(pass)} covering the frozen HF config
    predicate set; overall HARD_INFEASIBLE iff any predicate fails.
    """
    hf = json.load(open(paths.HF_CONFIG_JSON, encoding="utf-8"))[
        "hard_feasibility"]
    missing = [k for k in hf if k not in predicates]
    if missing:
        raise FeasibilityError(f"predicates missing vs HF config: {missing}")
    offending = [k for k, v in predicates.items() if not v]
    return {
        "arm": arm, "voyage_id": voyage_id,
        "predicates": {k: bool(v) for k, v in predicates.items()},
        "offending": offending,
        "hard_infeasible": bool(offending),
    }


def pairwise_defined(recs: dict, estimand: str, j: dict) -> bool:
    """FR-854: defined iff required arms feasible, all J finite, J_C0>0."""
    if estimand not in ESTIMAND_ARMS:
        raise FeasibilityError(f"estimand {estimand} 非法")
    arms = set(ESTIMAND_ARMS[estimand]) | {"C0"}
    if any(recs[a]["hard_infeasible"] for a in arms):
        return False
    if any(j.get(a) is None or not (j[a] == j[a]) or abs(j[a]) == float("inf")
           for a in arms):
        return False
    return j["C0"] > 0


def estimand_status(defined_groups_by_tier: dict) -> str:
    """FR-866: FULL / CONDITIONAL / WITHHELD on defined-group counts."""
    counts = [len(set(defined_groups_by_tier.get(t, [])))
              for t in ("LOW", "MID", "HIGH")]
    if all(c == 4 for c in counts):
        return "FULL_COHORT_ESTIMATE"
    if all(c >= 3 for c in counts):
        return "CONDITIONAL_FEASIBLE_ESTIMATE"
    return "WITHHELD_INSUFFICIENT_FEASIBILITY"

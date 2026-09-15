"""Deterministic T-445 Gate verdict (DGN-F-11). Synthetic or later real stats."""
from __future__ import annotations

from typing import Any

X_PCT = 2.0
Y_PCT = 0.0

TRAJECTORY_VERDICTS = (
    "GO_TRAJECTORY_VALUE",
    "NO_GO_TRAJECTORY_VALUE",
    "INCONCLUSIVE",
    "FINAL_INCONCLUSIVE",
    "GATE_VALIDITY_FAIL",
)
B3_STATUS = (
    "UNQUALIFIED_CONTEXT",
    "QUALIFIED_PASS",
    "B3_DOMINANCE_AUDIT_FAIL",
)


def validity_failed(validity: dict | None) -> bool:
    if not validity:
        return False
    if validity.get("hard_feasibility_fail"):
        return True
    if float(validity.get("persistent_b1b2_infeasible", 0)) > 0:
        return True
    if validity.get("corpus_or_manifest_integrity_fail"):
        return True
    if validity.get("b2_optimizer_qualification_fail"):
        return True
    if validity.get("gate_validity_fail"):
        return True
    return False


def classify_effect(median_g: float, ci_lower: float, ci_upper: float) -> str:
    """Ordinary GO / NO-GO / INCONCLUSIVE. Boundaries are strict as frozen."""
    if median_g >= X_PCT and ci_lower > Y_PCT:
        return "GO_TRAJECTORY_VALUE"
    if ci_upper < X_PCT:
        return "NO_GO_TRAJECTORY_VALUE"
    return "INCONCLUSIVE"


def decide(
    *,
    median_g: float,
    ci_lower: float,
    ci_upper: float,
    mode: str = "initial",
    validity: dict | None = None,
    b3_context_status: str = "UNQUALIFIED_CONTEXT",
    extras: dict | None = None,
) -> dict[str, Any]:
    if b3_context_status not in B3_STATUS:
        raise ValueError(f"bad b3_context_status {b3_context_status}")
    if mode not in ("initial", "expanded"):
        raise ValueError(f"mode must be initial|expanded, got {mode}")

    if validity_failed(validity):
        out = {
            "schema": "t445_oracle_gate_v1",
            "trajectory_gate_verdict": "GATE_VALIDITY_FAIL",
            "b3_context_status": b3_context_status,
            "decision_required": True,
            "expansion_used": False,
            "expansion_eligible": False,
            "median_g": float(median_g),
            "ci_lower": float(ci_lower),
            "ci_upper": float(ci_upper),
            "mode": mode,
            "X_pct": X_PCT,
            "Y_pct": Y_PCT,
            "SMALL_N_GATE_CAUTION": True,
            "evidence_grade": "DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE",
        }
        if extras:
            out.update(extras)
        return out

    effect = classify_effect(median_g, ci_lower, ci_upper)
    expansion_eligible = False
    expansion_used = False
    decision_required = False
    verdict = effect
    if mode == "initial" and effect == "INCONCLUSIVE":
        expansion_eligible = True
        # no execution side effect: expansion_used remains false
    elif mode == "expanded" and effect == "INCONCLUSIVE":
        verdict = "FINAL_INCONCLUSIVE"
        decision_required = True
        expansion_used = True
    elif mode == "expanded":
        expansion_used = True

    out = {
        "schema": "t445_oracle_gate_v1",
        "trajectory_gate_verdict": verdict,
        "b3_context_status": b3_context_status,
        "decision_required": decision_required,
        "expansion_used": expansion_used,
        "expansion_eligible": expansion_eligible,
        "median_g": float(median_g),
        "ci_lower": float(ci_lower),
        "ci_upper": float(ci_upper),
        "mode": mode,
        "X_pct": X_PCT,
        "Y_pct": Y_PCT,
        "SMALL_N_GATE_CAUTION": True,
        "evidence_grade": "DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE",
    }
    if extras:
        out.update(extras)
    return out

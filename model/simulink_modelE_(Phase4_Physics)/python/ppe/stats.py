"""T-007 — pairwise estimands / COMMON_TRIPLET_DECOMPOSITION / equal-tier
stratified bootstrap (D5/D6, FR-842–844, FR-860–866, FR-880–881).

Synthetic inputs only in this task; no formal J_FEC is consumed here.
"""
from __future__ import annotations

import numpy as np

from t445.bootstrap_cluster import rng_from_seed

SEED = "phase4f-t445-bootstrap-v1"
B_DEFAULT = 10000
TIERS = ("LOW", "MID", "HIGH")
R_MIN_DENOM_PP = 0.10  # FR-881: |B_perfect_hat| < 0.10 pp -> R_UNDEFINED

IDENTITY_RTOL = 1e-9
IDENTITY_ATOL = 1e-10


class StatsError(RuntimeError):
    pass


def voyage_effects(j_c0: float, j_c1: float, j_c2: float) -> dict:
    """FR-842 voyage-level percentage effects + zero-safe identity check."""
    if not (np.isfinite([j_c0, j_c1, j_c2]).all() and j_c0 > 0):
        raise StatsError("voyage triplet not defined (finite + J_C0>0 required)")
    b_p = 100.0 * (j_c0 - j_c1) / j_c0
    b_q = 100.0 * (j_c0 - j_c2) / j_c0
    l_p = 100.0 * (j_c2 - j_c1) / j_c0
    residual = b_p - b_q - l_p
    bound = IDENTITY_ATOL + IDENTITY_RTOL * max(abs(b_p), abs(b_q), abs(l_p))
    if abs(residual) > bound:
        raise StatsError(f"identity residual {residual} > {bound}")
    return {"B_perfect": b_p, "B_predicted": b_q, "L_prediction": l_p,
            "DJ_perfect_kg": j_c0 - j_c1, "DJ_predicted_kg": j_c0 - j_c2,
            "DJ_gap_kg": j_c2 - j_c1, "identity_residual": residual}


def group_effects(voyage_vals: dict) -> dict:
    """voyage_vals: {split_group_id: [defined voyage effect values]}.

    Returns per-group mean over DEFINED voyages (n_defined recorded); groups
    with zero defined voyages are absent from the output.
    """
    out = {}
    for gid, vals in voyage_vals.items():
        vals = [v for v in vals if v is not None and np.isfinite(v)]
        if vals:
            out[gid] = {"effect": float(np.mean(vals)), "n_defined": len(vals)}
    return out


def tier_means(group_eff: dict, group_tier: dict) -> dict:
    """Mean of defined group effects within each tier (D6)."""
    out = {}
    for t in TIERS:
        vals = [g["effect"] for gid, g in group_eff.items()
                if group_tier[gid] == t]
        if vals:
            out[t] = float(np.mean(vals))
    return out


def theta_hat(group_eff: dict, group_tier: dict) -> tuple:
    """D6 estimator. Returns (status, hat or None, tier means)."""
    tm = tier_means(group_eff, group_tier)
    n_by_tier = {t: sum(1 for gid in group_eff if group_tier[gid] == t)
                 for t in TIERS}
    if all(n_by_tier[t] == 4 for t in TIERS):
        hat = float(np.mean([g["effect"] for g in group_eff.values()]))
        return "FULL_COHORT_ESTIMATE", hat, tm
    if all(t in tm and n_by_tier[t] >= 3 for t in TIERS):
        hat = float(np.mean([tm[t] for t in TIERS]))  # equal 1/3 tier weight
        return "CONDITIONAL_FEASIBLE_ESTIMATE", hat, tm
    return "WITHHELD_INSUFFICIENT_FEASIBILITY", None, tm


def bootstrap_ci(group_eff: dict, group_tier: dict, B: int = B_DEFAULT,
                 seed: str = SEED) -> tuple:
    """D6 bootstrap: per-tier resample defined groups with replacement
    (draw size = n_defined in that tier); equal 1/3 tier average."""
    rng = rng_from_seed(seed)
    pools = {t: [g["effect"] for gid, g in group_eff.items()
                 if group_tier[gid] == t] for t in TIERS}
    if any(len(pools[t]) < 3 for t in TIERS):
        raise StatsError("WITHHELD: tier with <3 defined groups")
    full = all(len(pools[t]) == 4 for t in TIERS)
    all_groups = [g["effect"] for g in group_eff.values()]
    draws = np.empty(B)
    for b in range(B):
        if full:
            sample = []
            for t in TIERS:
                pool = pools[t]
                idx = rng.integers(0, len(pool), size=len(pool))
                sample.extend([pool[i] for i in idx])
            draws[b] = float(np.mean(sample))
        else:
            tier_hats = []
            for t in TIERS:
                pool = pools[t]
                idx = rng.integers(0, len(pool), size=len(pool))
                tier_hats.append(float(np.mean([pool[i] for i in idx])))
            draws[b] = float(np.mean(tier_hats))
    lo, hi = np.percentile(draws, [2.5, 97.5], method="linear")
    return float(lo), float(hi), draws


def r_diagnostic(b_perfect_hat, b_predicted_hat, status) -> dict:
    """FR-880/881: study-level only, prospective undefined guards."""
    if status == "WITHHELD_INSUFFICIENT_FEASIBILITY":
        return {"R": None, "status": "R_UNDEFINED", "reason": "withheld"}
    if b_perfect_hat is None or not np.isfinite(b_perfect_hat):
        return {"R": None, "status": "R_UNDEFINED", "reason": "non-finite"}
    if b_perfect_hat <= 0 or abs(b_perfect_hat) < R_MIN_DENOM_PP:
        return {"R": None, "status": "R_UNDEFINED",
                "reason": "denominator <=0 or <0.10 pp"}
    return {"R": float(b_predicted_hat / b_perfect_hat),
            "status": "OK", "reason": ""}


def common_triplet_decomposition(defined_mask: dict) -> dict:
    """D5: study-level identity asserted only on the common triplet mask.

    defined_mask: {voyage_id: {"C0": bool, "C1": bool, "C2": bool}}.
    """
    common = sorted(v for v, m in defined_mask.items()
                    if m.get("C0") and m.get("C1") and m.get("C2"))
    return {"mask": common, "n": len(common),
            "identity": "COMMON_TRIPLET_DECOMPOSITION",
            "note": "B_perfect_hat = B_predicted_hat + L_prediction_hat "
                    "asserted ONLY on this mask; pair-specific estimates "
                    "from other masks are reported separately and never "
                    "algebraically combined across masks."}

"""Stratified paired split_group-cluster bootstrap (DGN-F-14)."""
from __future__ import annotations

import hashlib

import numpy as np

SEED = "phase4f-t445-bootstrap-v1"
B_DEFAULT = 10000
TIERS = ("HIGH", "MID", "LOW")
N_PER_TIER = {"initial": 3, "expanded": 4}


def rng_from_seed(seed: str = SEED) -> np.random.Generator:
    digest = hashlib.sha256(seed.encode("utf-8")).digest()
    ss = np.random.SeedSequence(int.from_bytes(digest[:16], "little"))
    return np.random.default_rng(ss)


def _tier_map(groups: list[dict]) -> dict[str, list[dict]]:
    m = {t: [] for t in TIERS}
    for g in groups:
        if g["tier"] not in m:
            raise ValueError(f"unknown tier {g['tier']}")
        m[g["tier"]].append(g)
    return m


def bootstrap_median(
    groups: list[dict],
    *,
    mode: str = "initial",
    B: int = B_DEFAULT,
    seed: str = SEED,
) -> dict:
    """Resample complete groups within each tier; median of resampled h_g.

    Does not resample windows or unpaired voyages. B1/B2 stay paired inside
    each group's members.
    """
    if mode not in N_PER_TIER:
        raise ValueError(f"mode must be initial|expanded, got {mode}")
    n_draw = N_PER_TIER[mode]
    by = _tier_map(groups)
    for t in TIERS:
        if len(by[t]) < n_draw:
            raise ValueError(
                f"tier {t} has {len(by[t])} groups, need >= {n_draw} for {mode}"
            )
    rng = rng_from_seed(seed)
    meds = np.empty(B, dtype=np.float64)
    for b in range(B):
        drawn_hg = []
        for t in TIERS:
            pool = by[t]
            idx = rng.integers(0, len(pool), size=n_draw)
            drawn_hg.extend(pool[i]["h_g"] for i in idx)
        meds[b] = float(np.median(np.asarray(drawn_hg, dtype=np.float64)))
    lo, hi = np.percentile(meds, [2.5, 97.5], method="linear")
    return {
        "B": int(B),
        "mode": mode,
        "n_per_tier": n_draw,
        "seed": seed,
        "median_g_hat": float(np.median(np.array([g["h_g"] for g in groups]))),
        "ci_lower": float(lo),
        "ci_upper": float(hi),
        "replicates_sha256": hashlib.sha256(meds.tobytes()).hexdigest(),
        "_replicates": meds,
    }

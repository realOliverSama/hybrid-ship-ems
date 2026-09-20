"""T-006 pairing statistics: h_v → h_g → median_g (DGN-F-14)."""
from __future__ import annotations

from typing import Iterable

import numpy as np


def h_v(j_b1: float, j_b2: float) -> float:
    """h_v = 100 * (J_B1 - J_B2) / J_B1. Requires finite J_B1 != 0."""
    if not np.isfinite(j_b1) or not np.isfinite(j_b2) or j_b1 == 0:
        raise ValueError("h_v requires finite J_B1 != 0 and finite J_B2")
    return 100.0 * (j_b1 - j_b2) / j_b1


def pair_voyages(rows: Iterable[dict]) -> list[dict]:
    """Each row: voyage_id, split_group_id, tier, J_B1, J_B2.

    Requires complete B1/B2 pairing; no unpaired drop.
    """
    out = []
    seen = set()
    for r in rows:
        vid = r["voyage_id"]
        if vid in seen:
            raise ValueError(f"duplicate voyage_id {vid}")
        seen.add(vid)
        if "J_B1" not in r or "J_B2" not in r:
            raise ValueError(f"unpaired voyage {vid}")
        hv = h_v(float(r["J_B1"]), float(r["J_B2"]))
        out.append({
            "voyage_id": vid,
            "split_group_id": r["split_group_id"],
            "tier": r["tier"],
            "h_v": hv,
            "J_B1": float(r["J_B1"]),
            "J_B2": float(r["J_B2"]),
        })
    return out


def group_h(paired: list[dict], family_size: int = 4) -> list[dict]:
    """h_g = mean of h_v in split_group. Family must be complete."""
    by: dict[str, list[dict]] = {}
    for r in paired:
        by.setdefault(r["split_group_id"], []).append(r)
    groups = []
    for gid, mem in sorted(by.items()):
        if len(mem) != family_size:
            raise ValueError(
                f"split_group {gid} family size {len(mem)} != {family_size}"
            )
        tiers = {m["tier"] for m in mem}
        if len(tiers) != 1:
            raise ValueError(f"split_group {gid} mixed tiers {tiers}")
        hv = np.array([m["h_v"] for m in mem], dtype=np.float64)
        groups.append({
            "split_group_id": gid,
            "tier": next(iter(tiers)),
            "h_g": float(np.mean(hv)),
            "n_members": len(mem),
            "members": mem,
        })
    return groups


def median_g(groups: list[dict]) -> float:
    hg = np.array([g["h_g"] for g in groups], dtype=np.float64)
    if hg.size == 0:
        raise ValueError("median_g of empty group set")
    return float(np.median(hg))


def summaries(groups: list[dict]) -> dict:
    hg = np.array([g["h_g"] for g in groups], dtype=np.float64)
    by_tier: dict[str, list[float]] = {"HIGH": [], "MID": [], "LOW": []}
    for g in groups:
        by_tier[g["tier"]].append(g["h_g"])
    return {
        "median_g": float(np.median(hg)),
        "macro_mean_g": float(np.mean(hg)),
        "iqr": float(np.percentile(hg, 75) - np.percentile(hg, 25)),
        "min_g": float(np.min(hg)),
        "max_g": float(np.max(hg)),
        "positive_group_frac": float(np.mean(hg > 0)),
        "n_groups": int(hg.size),
        "by_tier": {
            t: {
                "n": len(v),
                "median": float(np.median(v)) if v else None,
                "mean": float(np.mean(v)) if v else None,
            }
            for t, v in by_tier.items()
        },
    }

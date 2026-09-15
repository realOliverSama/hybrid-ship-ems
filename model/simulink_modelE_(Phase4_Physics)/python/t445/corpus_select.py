#!/usr/bin/env python3
"""T-005 corpus selection (DGN-F-07). Validation split only; no test/OOD data.

shape_cv(t0) = std_k(Pbar_k) / max(mu(t0), P_epsilon), P_epsilon=1 kW,
full 300 s windows only. Group score = mean of 4 members.
Tiers by score descending: HIGH/MID/LOW = 9/9/9.
Within tier: sha256(seed|split_group_id) hex ascending.
Initial = ranks 1..3 per tier; reserve = rank 4 per tier.
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
from datetime import datetime, timezone

import numpy as np
from scipy.io import loadmat

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
else:
    from . import hashutil, paths

SEED = "phase4f-t445-corpus-v1"


def corpus_identity_payload(manifest: dict) -> dict:
    """Config-independent corpus selection identity (groups/roles/voyages)."""
    return {
        "initial_group_ids": manifest["initial_group_ids"],
        "reserve_group_ids": manifest["reserve_group_ids"],
        "initial_voyage_ids": manifest["initial_voyage_ids"],
        "reserve_voyage_ids": manifest["reserve_voyage_ids"],
        "groups": [
            {
                "split_group_id": g["split_group_id"],
                "tier": g["tier"],
                "role": g["role"],
                "score_g": g["score_g"],
                "members": [
                    {
                        "voyage_id": m["voyage_id"],
                        "artifact_sha256": m["artifact_sha256"],
                    }
                    for m in g["members"]
                ],
            }
            for g in manifest["groups"]
        ],
    }


def corpus_identity_sha256(manifest: dict) -> str:
    return hashutil.sha256_canonical(corpus_identity_payload(manifest))


P_EPS = 1.0
REFRESH = 10.0
HORIZON = 300.0
N_BINS = 30
EXPECTED_SPLIT = (
    "ac4c4a7629534daf1c4d88b05848fbfe1bb1dc148679d0b7abbbe873f66c809f"
)
N_VAL_GROUPS = 27
N_VAL_VOYAGES = 108
FAMILY = 4
TIERS = ("HIGH", "MID", "LOW")


def sort_hash(split_group_id: str, seed: str = SEED) -> str:
    return hashlib.sha256(f"{seed}|{split_group_id}".encode("utf-8")).hexdigest()


def shape_cv(pbars: np.ndarray, p_eps: float = P_EPS) -> float:
    pbars = np.asarray(pbars, dtype=float).ravel()
    mu = float(np.mean(pbars))
    sd = float(np.std(pbars, ddof=0))
    return sd / max(mu, p_eps)


def voyage_score(t: np.ndarray, p: np.ndarray, t_end: float) -> dict:
    t = np.asarray(t, dtype=float).ravel()
    p = np.asarray(p, dtype=float).ravel()
    t0s = np.arange(0.0, t_end - REFRESH + 1e-12, REFRESH)
    cvs = []
    for t0 in t0s:
        rem = min(t0 + HORIZON, t_end) - t0
        if rem < HORIZON - 1e-9:
            continue
        pbars = np.empty(N_BINS, dtype=float)
        for k in range(N_BINS):
            tlo = t0 + REFRESH * k
            thi = t0 + REFRESH * (k + 1)
            mask = (t >= tlo - 1e-12) & (t < thi - 1e-12)
            if not np.any(mask):
                raise ValueError(f"empty bin [{tlo},{thi}) at t0={t0}")
            pbars[k] = float(np.mean(p[mask]))
        cvs.append(shape_cv(pbars))
    if not cvs:
        raise ValueError("no full-horizon 300 s window")
    arr = np.asarray(cvs, dtype=float)
    return {
        "score_v": float(np.mean(arr)),
        "n_full_windows": int(arr.size),
        "shape_cv_min": float(np.min(arr)),
        "shape_cv_max": float(np.max(arr)),
    }


def rank_groups(group_scores: dict[str, float], seed: str = SEED) -> list[dict]:
    """group_scores: split_group_id -> score_g. Returns 27 rows with tier/role."""
    gids = list(group_scores.keys())
    if len(gids) != N_VAL_GROUPS:
        raise SystemExit(f"val groups={len(gids)} expected {N_VAL_GROUPS}")
    by_score = sorted(gids, key=lambda g: (-group_scores[g], g))
    rows = []
    for i, gid in enumerate(by_score):
        tier = TIERS[i // 9]
        rows.append({
            "split_group_id": gid,
            "score_g": group_scores[gid],
            "tier": tier,
            "score_rank": i + 1,
            "sort_hash": sort_hash(gid, seed),
        })
    selected = []
    for tier in TIERS:
        chunk = [r for r in rows if r["tier"] == tier]
        chunk.sort(key=lambda r: r["sort_hash"])
        for j, r in enumerate(chunk):
            r = dict(r)
            r["rank_in_tier"] = j + 1
            if j < 3:
                r["role"] = "initial"
            elif j == 3:
                r["role"] = "reserve"
            else:
                r["role"] = "unselected"
            selected.append(r)
    return selected


def _t_end(st: dict, duration_s) -> float:
    tkey = "time_s" if "time_s" in st else "t"
    t = np.asarray(st[tkey], dtype=float).ravel()
    dt = float(t[1] - t[0]) if t.size > 1 else 1.0
    t_end = float(duration_s if duration_s is not None else t[-1])
    if abs(t[-1] + dt - t_end) < 1.5:
        t_end = float(t[-1] + dt)
    return t_end, t, np.asarray(st["P_load_total_kW"], dtype=float).ravel()


def load_val_only(assign: dict, ledger: dict) -> list[dict]:
    val_g = {gid for gid, rec in assign.items() if rec["split"] == "val"}
    if len(val_g) != N_VAL_GROUPS:
        raise SystemExit(f"GATE: val split_groups={len(val_g)} != {N_VAL_GROUPS}")
    rows = [v for v in ledger["voyages"] if v["split_group_id"] in val_g]
    by_g: dict[str, list] = {g: [] for g in val_g}
    for v in rows:
        by_g[v["split_group_id"]].append(v)
    for g, mem in by_g.items():
        vids = [m["voyage_id"] for m in mem]
        if len(mem) != FAMILY or len(set(vids)) != FAMILY:
            raise SystemExit(
                f"GATE: split_group {g} family size={len(mem)} unique={len(set(vids))}"
            )
    n_voy = sum(len(v) for v in by_g.values())
    if n_voy != N_VAL_VOYAGES:
        raise SystemExit(f"GATE: val voyages={n_voy} != {N_VAL_VOYAGES}")
    return rows


def main() -> int:
    cfg = json.load(open(paths.CONFIG_JSON, encoding="utf-8"))
    split_path = os.path.join(
        paths.MODELE_ROOT, "results", "dataset_v3", "formal",
        "split_assignment_v3.json")
    split = json.load(open(split_path, encoding="utf-8"))
    if split.get("artifact_sha256") != EXPECTED_SPLIT:
        raise SystemExit(f"split hash mismatch {split.get('artifact_sha256')}")
    formal_man_path = os.path.join(
        paths.MODELE_ROOT, "results", "dataset_v3", "formal",
        "dataset_v3_formal_manifest.json")
    formal_man = json.load(open(formal_man_path, encoding="utf-8"))
    payload = formal_man.get("payload", formal_man)
    test_sealed = bool(payload.get("test_sealed"))
    ood_sealed = bool(payload.get("ood_sealed"))
    if not test_sealed:
        raise SystemExit("GATE: test_sealed is not true")

    ledger_path = os.path.join(
        paths.MODELE_ROOT, "results", "dataset_v3", "formal",
        "pass1_iid_ledger.json")
    ledger = json.load(open(ledger_path, encoding="utf-8"))
    val_rows = load_val_only(split["assignment"], ledger)

    iid_dir = os.path.join(paths.MODELE_ROOT, "datasets_v3", "formal", "iid")
    voyage_scores = {}
    for v in val_rows:
        mat_path = os.path.join(iid_dir, v["voyage_id"] + ".mat")
        if not os.path.isfile(mat_path):
            raise SystemExit(f"missing val mat {v['voyage_id']}")
        m = loadmat(mat_path, simplify_cells=True)
        t_end, t, p = _t_end(m["stream"], v.get("duration_s"))
        sc = voyage_score(t, p, t_end)
        voyage_scores[v["voyage_id"]] = {
            **sc,
            "split_group_id": v["split_group_id"],
            "artifact_sha256": v["artifact_sha256"],
        }

    group_scores = {}
    families = {}
    for v in val_rows:
        families.setdefault(v["split_group_id"], []).append(v)
    for gid, mem in families.items():
        group_scores[gid] = float(np.mean([
            voyage_scores[m["voyage_id"]]["score_v"] for m in mem
        ]))

    ranked = rank_groups(group_scores, SEED)
    for r in ranked:
        mem = sorted(families[r["split_group_id"]], key=lambda m: m["voyage_id"])
        r["members"] = [
            {
                "voyage_id": m["voyage_id"],
                "score_v": voyage_scores[m["voyage_id"]]["score_v"],
                "n_full_windows": voyage_scores[m["voyage_id"]]["n_full_windows"],
                "artifact_sha256": m["artifact_sha256"],
            }
            for m in mem
        ]
        assert len(r["members"]) == FAMILY

    initial_g = [r for r in ranked if r["role"] == "initial"]
    reserve_g = [r for r in ranked if r["role"] == "reserve"]
    initial_v = [m["voyage_id"] for r in initial_g for m in r["members"]]
    reserve_v = [m["voyage_id"] for r in reserve_g for m in r["members"]]
    if len(initial_g) != 9 or len(initial_v) != 36:
        raise SystemExit("initial count fail")
    if len(reserve_g) != 3 or len(reserve_v) != 12:
        raise SystemExit("reserve count fail")
    if set(initial_v) & set(reserve_v):
        raise SystemExit("initial/reserve overlap")
    tier_n = {t: sum(1 for r in ranked if r["tier"] == t) for t in TIERS}
    if tier_n != {"HIGH": 9, "MID": 9, "LOW": 9}:
        raise SystemExit(f"tier counts {tier_n}")

    ranked2 = rank_groups(group_scores, SEED)
    if [r["split_group_id"] for r in ranked] != [r["split_group_id"] for r in ranked2]:
        raise SystemExit("seed rerun mismatch")

    body = {
        "schema": "t445_corpus_manifest_v1",
        "task": "T-005",
        "purpose": "QUALIFICATION",
        "evidence_grade": "DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE",
        "config_sha256": cfg["config_sha256"],
        "seed": SEED,
        "split_assignment_sha256": EXPECTED_SPLIT,
        "test_sealed": test_sealed,
        "ood_sealed": ood_sealed,
        "scientific_access": {
            "split": "validation",
            "iid_test_voyages_loaded": 0,
            "ood_voyages_loaded": 0,
        },
        "n_val_groups": N_VAL_GROUPS,
        "n_val_voyages": N_VAL_VOYAGES,
        "tier_counts": tier_n,
        "n_initial_groups": 9,
        "n_initial_voyages": 36,
        "n_reserve_groups": 3,
        "n_reserve_voyages": 12,
        "shape_score": {
            "name": "shape_cv",
            "p_epsilon_kW": P_EPS,
            "std_ddof": 0,
            "windows": "full_horizon_300s_only",
            "group_agg": "mean_of_four_members",
        },
        "initial_group_ids": [r["split_group_id"] for r in initial_g],
        "reserve_group_ids": [r["split_group_id"] for r in reserve_g],
        "initial_voyage_ids": initial_v,
        "reserve_voyage_ids": reserve_v,
        "groups": ranked,
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    body["manifest_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in body.items() if k != "manifest_sha256"}
    )
    os.makedirs(paths.RESULTS_DIR, exist_ok=True)
    out = os.path.join(paths.RESULTS_DIR, "corpus_manifest.json")
    hashutil.dump_json(out, body)
    with open(paths.ACCESS_LEDGER, "a", encoding="utf-8") as fh:
        fh.write(json.dumps({
            "purpose": "QUALIFICATION",
            "task": "T-005",
            "artifact": "corpus_manifest.json",
            "val_voyages_scored": N_VAL_VOYAGES,
            "iid_test_loaded": 0,
            "ood_loaded": 0,
            "utc": body["utc"],
        }, sort_keys=True) + "\n")
    print(json.dumps({
        "pass": True,
        "n_val_groups": N_VAL_GROUPS,
        "tiers": tier_n,
        "n_initial": len(initial_v),
        "n_reserve": len(reserve_v),
        "test_sealed": test_sealed,
        "manifest_sha256": body["manifest_sha256"],
        "config_sha256": cfg["config_sha256"],
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

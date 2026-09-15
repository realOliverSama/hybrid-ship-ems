#!/usr/bin/env python3
"""Reproduce B1 E_ref / E_scale from frozen train streams (T-003).

Uses Dataset v3 formal train split only (no val/test/OOD). Window semantics
match DGN-F-02 / t445.assemble_oracle_window (shrinking-horizon, no zero-fill).
"""
from __future__ import annotations

import hashlib
import json
import os
import sys

import numpy as np
from scipy.io import loadmat

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
else:
    from . import hashutil, paths

EXPECTED_EREF = 27.781415
EXPECTED_ESCALE = 13.671476
EXPECTED_PROV = (
    "b9dcf268f533a9a93c73bbaeeed3a6dca6b15227cfd868fb459c46fcc81cf045"
)
EXPECTED_SPLIT = (
    "ac4c4a7629534daf1c4d88b05848fbfe1bb1dc148679d0b7abbbe873f66c809f"
)
EXPECTED_N_T0 = 159472
EXPECTED_N_TRAIN = 396


def e300_series(t: np.ndarray, p: np.ndarray, t_end: float,
                refresh: float = 10.0, horizon: float = 300.0) -> np.ndarray:
    t = np.asarray(t, dtype=float).ravel()
    p = np.asarray(p, dtype=float).ravel()
    t0s = np.arange(0.0, t_end - refresh + 1e-12, refresh)
    out = np.empty(t0s.size, dtype=float)
    for i, t0 in enumerate(t0s):
        thi = min(t0 + horizon, t_end)
        mask = (t >= t0 - 1e-12) & (t < thi - 1e-12)
        # 1 s integral on 1 Hz grid
        out[i] = float(np.sum(p[mask])) / 3600.0
    return out


def main() -> int:
    split_path = os.path.join(
        paths.MODELE_ROOT, "results", "dataset_v3", "formal",
        "split_assignment_v3.json")
    with open(split_path, encoding="utf-8") as fh:
        split = json.load(fh)
    split_sha = hashutil.sha256_file(split_path)
    if split.get("artifact_sha256") != EXPECTED_SPLIT and split_sha != EXPECTED_SPLIT:
        # artifact embeds its own sha; compare that field
        if split.get("artifact_sha256") != EXPECTED_SPLIT:
            raise SystemExit(f"split hash mismatch: {split.get('artifact_sha256')}")

    assign = split["assignment"]

    ledger_path = os.path.join(
        paths.MODELE_ROOT, "results", "dataset_v3", "formal",
        "pass1_iid_ledger.json")
    if not os.path.isfile(ledger_path):
        ledger_path = os.path.join(
            paths.MODELE_ROOT, "datasets_v3", "formal", "iid", "_iid_ledger.json")
    with open(ledger_path, encoding="utf-8") as fh:
        ledger = json.load(fh)

    # Exact original generator (design-session 2026-09-15):
    # train_rows = ledger rows whose split_group is train (pass1_iid_ledger order)
    # sha_list = [(voyage_id, artifact_sha256), ...]
    # prov = sha256('|'.join(f'{v}:{h}' for v,h in sorted(sha_list)))
    train_rows = [
        v for v in ledger["voyages"]
        if assign[v["split_group_id"]]["split"] == "train"
    ]
    if len(train_rows) != EXPECTED_N_TRAIN:
        raise SystemExit(f"train voyages={len(train_rows)} expected {EXPECTED_N_TRAIN}")
    sha_list = [(v["voyage_id"], v["artifact_sha256"]) for v in train_rows]
    if len(set(sha_list)) != EXPECTED_N_TRAIN:
        raise SystemExit("duplicate train voyage identities")
    prov_bytes = "|".join(f"{v}:{h}" for v, h in sorted(sha_list)).encode("utf-8")
    prov = hashlib.sha256(prov_bytes).hexdigest()
    voyages = train_rows

    iid_dir = os.path.join(paths.MODELE_ROOT, "datasets_v3", "formal", "iid")
    e_all = []
    for v in voyages:
        mat_path = os.path.join(iid_dir, v["voyage_id"] + ".mat")
        if not os.path.isfile(mat_path):
            raise SystemExit(f"missing {mat_path}")
        m = loadmat(mat_path, simplify_cells=True)
        st = m["stream"]
        tkey = "time_s" if "time_s" in st else "t"
        t = np.asarray(st[tkey], dtype=float).ravel()
        p = np.asarray(st["P_load_total_kW"], dtype=float).ravel()
        t_end = float(v.get("duration_s") or t[-1])
        if abs(t[-1] + (t[1] - t[0]) - t_end) < 1.5:
            t_end = t[-1] + (t[1] - t[0])
        e_all.append(e300_series(t, p, t_end))
    e_cat = np.concatenate(e_all)
    eref = float(np.mean(e_cat))
    escale = float(np.std(e_cat, ddof=0))

    rec = {
        "schema": "t445_b1_norm_repro_v1",
        "task": "T-003",
        "n_train_voyages": len(voyages),
        "n_t0": int(e_cat.size),
        "E_ref_kWh": eref,
        "E_scale_kWh": escale,
        "E300_min_kWh": float(np.min(e_cat)),
        "E300_max_kWh": float(np.max(e_cat)),
        "provenance_sha256": prov,
        "expected": {
            "E_ref_kWh": EXPECTED_EREF,
            "E_scale_kWh": EXPECTED_ESCALE,
            "provenance_sha256": EXPECTED_PROV,
            "n_t0": EXPECTED_N_T0,
        },
        "match_ref": abs(eref - EXPECTED_EREF) < 5e-7,
        "match_scale": abs(escale - EXPECTED_ESCALE) < 5e-7,
        "match_prov": prov == EXPECTED_PROV,
        "match_n": int(e_cat.size) == EXPECTED_N_T0,
    }
    rec["pass"] = all(rec[k] for k in ("match_ref", "match_scale", "match_prov", "match_n"))
    rec["provenance_serialization"] = {
        "status": "PROVENANCE_SERIALIZATION_ALIGNMENT" if rec["match_prov"] else "MISMATCH",
        "join": "'|'.join(f'{voyage_id}:{artifact_sha256}' for voyage_id, artifact_sha256 in sorted(pairs))",
        "sort": "tuple (voyage_id, artifact_sha256) lexicographic",
        "encoding": "utf-8",
        "trailing_newline": False,
        "ledger": "pass1_iid_ledger.json",
        "n_bytes": len(prov_bytes),
        "source": "design-session original generator 2026-09-15",
    }
    with open(paths.CONFIG_JSON, encoding="utf-8") as fh:
        rec["config_sha256"] = json.load(fh)["config_sha256"]
    rec["record_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in rec.items() if k != "record_sha256"}
    )
    os.makedirs(paths.RESULTS_DIR, exist_ok=True)
    out = os.path.join(paths.RESULTS_DIR, "b1_norm_repro.json")
    hashutil.dump_json(out, rec)
    print(json.dumps({k: rec[k] for k in (
        "pass", "E_ref_kWh", "E_scale_kWh", "n_t0", "provenance_sha256",
        "match_ref", "match_scale", "match_prov", "match_n")}, indent=2))
    return 0 if rec["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

"""T-001 — canonical identities / T_eval table (TG6) / study preparation.

T_eval(voyage) = frozen Dataset-v3 native-stream t_end = floor(stream.time_s[-1]),
the same derivation as dataset_v3.run_formal_freeze (edges = arange(0,
floor(t[-1])+1, 1); t_1hz = edges[1:]; t_end = t_1hz[-1]).

Reads corpus-voyage .mat *metadata and time vectors only* (FREEZE_PREP);
no controller outcomes exist and none are observed.

Run: python -m ppe.run_t001   (requires numpy+scipy env, e.g. .venv_tf)
"""
from __future__ import annotations

import numpy as np
from scipy.io import loadmat

from t445 import hashutil

from . import ledger, paths

# Frozen C* numerics (Design DGN-PPE-03 / Requirements FR-811) — literals
CSTAR = {
    "law": 6,
    "delta_lambda_min": -0.10,
    "delta_lambda_max": 0.10,
    "delta_lambda_step": 0.005,
    "n_delta_lambda": 41,
    "decision_interval_s": 10.0,
    "horizon_s": 300.0,
    "horizon_bins": 30,
    "soc_grid_min": 0.10,
    "soc_grid_max": 0.95,
    "soc_grid_step": 0.005,
    "n_soc": 171,
    "k_eq_E_kg_per_kwh": 0.5457,
    "w_term_kg": 1.17e5,
    "lambda_eff_floor": 0.43,
    "lambda_eff_cap": None,
    "K_S": 1.0 / 30.0,
    "realize_bin_dt_internal_s": 1.0,
    "terminal_delta": 0.01,
    "bat_cap_kwh": 1071.36,
    "dg_c_start_kg": 0.6,
    "terminal_soc_abs_dsoc_max": 0.005,
}

ANCHOR_STRIDE_S = 10.0
WARMUP_S = 300.0
CORPUS_IDENTITY_SHA256 = (
    "245fa307cf36143259672f93cf801587eb701d721028a735b3cd91c97c42f115")
WINDOWS_NPZ_SHA256 = (
    "203dc92df1f1aad980480514ea6c1ef09c50299f5fc91274e11d63f5b2491497")
HF_OBJECT_SHA256 = (
    "fcef573d06e414bef8b64f10864def24ad769195d32362b1f57d0b312aac9def")

FR836 = {
    "winner_identity_sha256":
        "832720b0b0f39592fdeaa25957c1a61d0f0a1381d7220881e0b9af345eb37aa7",
    "winner_file_sha256":
        "3a46c848092f35d6da66f8e1dc931653b580547bc3716366f11110679c5c0ebc",
    "selected_config_sha256":
        "fea67b7ff0ce10af619037eade22c678425986af86c405024ebb9a341f163ae4",
    "architecture_config_sha256":
        "1d662ff03ed7ca042b731707501d6b254fc35b64ae858431c66745025e544b5d",
    "s0_evidence_sha256":
        "d8e3c82ddcdc36ced41459fe4b90d685176f297c0f72558e0cf80fdc1f07ab1b",
    "keras_sha256":
        "d8da6af4667875c45de7de90383d2b6b750420f5af62c7dcdcb1e5997897f894",
    "seed_index": 0,
    "seed_int": 1115655093,
    "trial_id": "reatt-t018",
    "formal_norm_v3_sha256":
        "b64210bc47a035afe9fd164c65ba63fca84aead53dc7e2dcaada79bd88f065ba",
    "predictor_norm_ext_v1_sha256":
        "a1cace1fb6f6eda58ad3be76723663c6a196b98bbc4c2c34c7b537dc4c2fe92c",
    "eval_config_sha256":
        "2bf077ba8045486a9d242b99ab842fe96009f0a29570b6dfbf1c1d370eac928a",
    "reat_t_arch_sha256":
        "ad5f1bdfbedfee74a739ebe30177d4efddae0df017c6aaa6c08faa92593a7ff0",
}

_TOL = 1e-9


class TevalStop(RuntimeError):
    """Fail-closed: T_eval cannot be bound arm-independently."""


def anchor_grid(t_start: float, t_end: float) -> np.ndarray:
    """Dataset-v3 anchor grid (windowing.py frozen semantics, re-derived)."""
    first = t_start + WARMUP_S
    last = t_end - WARMUP_S
    if last < first - _TOL:
        return np.array([], dtype=float)
    n = int(np.floor((last - first) / ANCHOR_STRIDE_S + _TOL)) + 1
    return first + ANCHOR_STRIDE_S * np.arange(n, dtype=float)


def load_corpus() -> dict:
    import json
    with open(paths.CORPUS_MANIFEST, encoding="utf-8") as fh:
        man = json.load(fh)
    if man["corpus_identity_sha256"] != CORPUS_IDENTITY_SHA256:
        raise TevalStop("corpus identity mismatch vs FR-860")
    with open(f"{paths.MODELE_ROOT}/results/dataset_v3/formal/"
              f"pass2_iid_ledger.json", encoding="utf-8") as fh:
        pass2 = {v["voyage_id"]: v["artifact_sha256"]
                 for v in json.load(fh)["voyages"]}
    groups = {g["split_group_id"]: g for g in man["groups"]}
    rows = []
    for gid in list(man["initial_group_ids"]) + list(man["reserve_group_ids"]):
        g = groups[gid]
        for m in g["members"]:
            rows.append({
                "voyage_id": m["voyage_id"],
                "split_group_id": gid,
                "tier": g["tier"],
                "role": g["role"],
                # selection-time lineage (split-assignment / pass1 bytes)
                "manifest_member_sha256": m["artifact_sha256"],
                # authoritative retained-copy file identity (pass2, on disk)
                "pass2_artifact_sha256": pass2.get(m["voyage_id"]),
            })
    rows.sort(key=lambda r: r["voyage_id"])
    if len(rows) != 48:
        raise TevalStop(f"corpus rows={len(rows)} != 48")
    if any(r["pass2_artifact_sha256"] is None for r in rows):
        raise TevalStop("corpus voyage missing from pass2 IID ledger")
    tiers = {}
    for r in rows:
        tiers.setdefault(r["tier"], set()).add(r["split_group_id"])
    if sorted(len(v) for v in tiers.values()) != [4, 4, 4]:
        raise TevalStop("corpus tiers are not 4/4/4")
    return {"manifest": man, "rows": rows}


def voyage_t_eval(vid: str, pass2_sha: str, manifest_sha: str) -> dict:
    """Extract T_eval from the frozen native stream (metadata/time only)."""
    path = f"{paths.FORMAL_IID_DIR}/{vid}.mat"
    sha = hashutil.sha256_file(path)
    if sha != pass2_sha:
        raise TevalStop(f"{vid}: disk artifact sha != pass2 ledger identity")
    m = loadmat(path, simplify_cells=True)
    st, meta = m["stream"], m["meta"]
    t = np.asarray(st["time_s"], dtype=float).ravel()
    soc = np.asarray(st["SOC"], dtype=float).ravel()
    t_eval = float(np.floor(t[-1]))
    t_start = 0.0
    anchors = anchor_grid(t_start, t_eval)
    if anchors.size == 0:
        raise TevalStop(f"{vid}: no legal anchors (T_eval={t_eval})")
    n_full_seg = int(np.floor(t_eval / 10.0 + _TOL))
    partial = t_eval - 10.0 * n_full_seg
    n_updates = n_full_seg + (1 if partial > _TOL else 0)
    ledger.log_access(
        artifact=f"datasets_v3/formal/iid/{vid}.mat",
        purpose="FREEZE_PREP",
        split_contact="val-metadata-timevector-only",
        note="T_eval/IC identity extraction; no controller outcome exists",
    )
    return {
        "voyage_id": vid,
        "t_start": t_start,
        "T_eval": t_eval,
        "T_eval_provenance": {
            "artifact": f"datasets_v3/formal/iid/{vid}.mat",
            "artifact_sha256": sha,
            "artifact_identity_source": "pass2_iid_ledger (retained canonical "
                                        "copy; two-run payload-identical)",
            "selection_lineage_sha256": manifest_sha,
            "derivation": "floor(stream.time_s[-1]) == t_1hz[-1] "
                          "(dataset_v3.run_formal_freeze aggregation edges)",
            "derivation_code": "python/ppe/teval.py + dataset_v3/windowing.py",
        },
        "initial_condition_identity": {
            "SOC0": float(soc[0]),
            "source": "stream.SOC[0]",
            "scenario_seed": str(meta.get("generation_seed", "")),
        },
        "mission_identity": {
            "scenario_id": str(meta.get("scenario_id", "")),
            "mission_template_id": str(meta.get("mission_template_id", "")),
            "route_template_id": str(meta.get("route_template_id", "")),
            "voyage_family_id": str(meta.get("voyage_family_id", "")),
            "scenario_stratum": str(meta.get("scenario_stratum", "")),
            "producer_script_sha": str(meta.get("producer_script_sha", "")),
        },
        "n_segments_10s": n_updates,
        "final_partial_bin_s": (partial if partial > _TOL else 0.0),
        "control_update_grid": {
            "start_s": 0.0, "step_s": ANCHOR_STRIDE_S,
            "n_updates": n_updates,
            "last_update_s": 10.0 * (n_updates - 1),
        },
        "differentiation_boundary_s": WARMUP_S,
        "n_differentiated_updates": int(anchors.size),
        "first_anchor_s": float(anchors[0]),
        "T_fcst_s": float(anchors[-1]),
        "target_exhaustion_boundary_s": float(anchors[-1]),
    }


def verify_against_npz(table_rows: list) -> dict:
    """Cross-check the computed anchor grid against the frozen npz per voyage."""
    sha = hashutil.sha256_file(paths.WINDOWS_NPZ)
    if sha != WINDOWS_NPZ_SHA256:
        raise TevalStop("windows_v3_formal.npz SHA mismatch vs Design D2")
    z = np.load(paths.WINDOWS_NPZ, allow_pickle=False)
    vid_all = z["voyage_id"]
    t0_all = z["anchor_t0_s"]
    splits_all = z["splits"]
    detail = {}
    for row in table_rows:
        vid = row["voyage_id"]
        mask = vid_all == vid
        if not np.any(mask):
            raise TevalStop(f"{vid}: absent from windows npz")
        if set(np.unique(splits_all[mask]).tolist()) != {"val"}:
            raise TevalStop(f"{vid}: npz split is not val")
        got = np.sort(t0_all[mask].astype(float))
        exp = anchor_grid(row["t_start"], row["T_eval"])
        if got.shape != exp.shape or not np.allclose(got, exp, rtol=0, atol=_TOL):
            raise TevalStop(f"{vid}: npz anchors != T_eval-derived anchors")
        detail[vid] = int(exp.size)
    return {"npz_sha256": sha, "anchors_per_voyage": detail,
            "verdict": "NPZ_ANCHOR_GRID_CONSISTENT"}


def build() -> dict:
    import json
    import os

    os.makedirs(paths.FREEZE_DIR, exist_ok=True)
    corpus = load_corpus()
    rows = []
    for r in corpus["rows"]:
        row = voyage_t_eval(r["voyage_id"], r["pass2_artifact_sha256"],
                            r["manifest_member_sha256"])
        row.update({
            "split_group_id": r["split_group_id"],
            "tier": r["tier"],
            "role": r["role"],
        })
        rows.append(row)
    npz_check = verify_against_npz(rows)

    table = {
        "schema": "phase4f_ppe_teval_table_v1",
        "study": "phase4-f-practical-predictive-ems",
        "n_voyages": len(rows),
        "warmup_s": WARMUP_S,
        "anchor_stride_s": ANCHOR_STRIDE_S,
        "npz_anchor_consistency": npz_check["verdict"],
        "corpus_identity_sha256": CORPUS_IDENTITY_SHA256,
        "voyages": rows,
    }
    table["teval_table_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in table.items() if k != "teval_table_sha256"})
    hashutil.dump_json(paths.TEVAL_TABLE_JSON, table)

    ledger_sha = hashutil.sha256_file(paths.CAUSALITY_LEDGER_JSON)
    study = {
        "schema": "phase4f_ppe_study_config_v1",
        "study": "phase4-f-practical-predictive-ems",
        "cstar": CSTAR,
        "artifacts": {
            "windows_npz": {"path": paths.rel(paths.WINDOWS_NPZ),
                            "sha256": WINDOWS_NPZ_SHA256},
            "corpus_manifest": {"path": paths.rel(paths.CORPUS_MANIFEST),
                                "corpus_identity_sha256": CORPUS_IDENTITY_SHA256,
                                "file_sha256": hashutil.sha256_file(
                                    paths.CORPUS_MANIFEST)},
            "hard_feasibility": {"path": paths.rel(paths.HF_CONFIG_JSON),
                                 "object_sha256": HF_OBJECT_SHA256,
                                 "file_sha256": hashutil.sha256_file(
                                     paths.HF_CONFIG_JSON)},
            "causality_ledger": {"path": paths.rel(paths.CAUSALITY_LEDGER_JSON),
                                 "file_sha256": ledger_sha},
            "t445_config": {"path": paths.rel(paths.T445_CONFIG_JSON),
                            "file_sha256": hashutil.sha256_file(
                                paths.T445_CONFIG_JSON)},
            "teval_table": {"path": paths.rel(paths.TEVAL_TABLE_JSON),
                            "teval_table_sha256": table["teval_table_sha256"]},
            "modele_slx": {"path": paths.rel(paths.MODELE_SLX),
                           "sha256": hashutil.sha256_file(paths.MODELE_SLX)},
            "predictor": {
                "keras": {"path": paths.rel(paths.KERAS_S0),
                          "sha256": FR836["keras_sha256"]},
                "winner_json": {"path": paths.rel(paths.WINNER_JSON),
                                "sha256": FR836["winner_file_sha256"]},
                "selected_config": {"path": paths.rel(paths.SELECTED_CONFIG_JSON),
                                    "sha256": FR836["selected_config_sha256"]},
                "s0_evidence": {"path": paths.rel(paths.S0_EVIDENCE_JSON),
                                "sha256": FR836["s0_evidence_sha256"]},
                "formal_norm_v3": {"path": paths.rel(paths.FORMAL_NORM_V3),
                                   "sha256": FR836["formal_norm_v3_sha256"]},
                "predictor_norm_ext_v1": {
                    "path": paths.rel(paths.PREDICTOR_NORM_EXT),
                    "sha256": FR836["predictor_norm_ext_v1_sha256"]},
                "eval_config": {"path": paths.rel(paths.EVAL_CONFIG),
                                "sha256": FR836["eval_config_sha256"]},
                "reat_t_arch": {"path": paths.rel(paths.REAT_T_ARCH),
                                "sha256": FR836["reat_t_arch_sha256"]},
                "winner_identity_sha256": FR836["winner_identity_sha256"],
                "architecture_config_sha256": FR836["architecture_config_sha256"],
                "seed_index": FR836["seed_index"],
                "seed_int": FR836["seed_int"],
                "trial_id": FR836["trial_id"],
            },
        },
        "bootstrap": {"seed": "phase4f-t445-bootstrap-v1", "B": 10000,
                      "percentile_method": "linear", "ci": [2.5, 97.5]},
        "access_policy": {"iid_test": "SEALED",
                          "ood": "NOT_FOR_SELECTION",
                          "formal_before_freeze": "FORBIDDEN"},
        "terminal_label": "DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE",
    }
    study["config_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in study.items() if k != "config_sha256"})
    hashutil.dump_json(paths.STUDY_CONFIG_JSON, study)

    evidence = {
        "schema": "ppe_t001_freeze_prep_v1",
        "task": "T-001",
        "overall": True,
        "n_voyages": len(rows),
        "tiers": {t: sorted({r['split_group_id'] for r in rows
                             if r['tier'] == t}) for t in ("LOW", "MID", "HIGH")},
        "teval_table_sha256": table["teval_table_sha256"],
        "study_config_sha256": study["config_sha256"],
        "npz_anchor_consistency": npz_check["verdict"],
        "soc0_all_0p60": bool(all(
            abs(r["initial_condition_identity"]["SOC0"] - 0.60) < 1e-9
            for r in rows)),
        "t_eval_range_s": [min(r["T_eval"] for r in rows),
                           max(r["T_eval"] for r in rows)],
        "n_differentiated_updates_range": [
            min(r["n_differentiated_updates"] for r in rows),
            max(r["n_differentiated_updates"] for r in rows)],
        "access_ledger": ledger.ledger_counts(),
    }
    hashutil.dump_json(f"{paths.FREEZE_DIR}/t001_freeze_prep.json", evidence)
    return evidence


if __name__ == "__main__":
    out = build()
    print("T-001 overall:", out["overall"])
    print("teval_table_sha256:", out["teval_table_sha256"])
    print("study_config_sha256:", out["study_config_sha256"])
    print("T_eval range:", out["t_eval_range_s"])

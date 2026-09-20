"""T-FC3-4 — locked replay engine (IMPLEMENTATION ONLY; execution gated).

The engine reads ONLY frozen sanitized views (plus guarded trajectory/audit
access through fc3_core allow-lists) and would emit one artifact per run
into results/ppe_practical/readjudication_epoch1/. Execution is hard-gated:
it refuses to run unless FC3_REPLAY_AUTHORIZED=YES is set in the environment
(T-FC3-7 remains BLOCKED; the human Replay Gate must set this).
"""
from __future__ import annotations

import json
import os
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _PPE)
import fc3_core as C  # noqa: E402

OUT_DIR = os.path.join(C._MODELE, "results", "ppe_practical",
                       "readjudication_epoch1")
REPLAY_TOOL_SHA = None  # filled at freeze time


def replay_one(view_path: str) -> dict:
    """Adjudicate one run from its materialized sanitized view.

    Returns the would-be replay artifact (D4 field family + TG5 status
    vocabulary). Pure: reads views/traj/audit, writes nothing.
    """
    view = json.load(open(view_path))
    ident = view["identity"]
    arm = _arm_from_run_filename(view_path)
    traj = C.load_traj_guarded(ident["voyage_id"], arm,
                               ident["source_trajectory_sha"])
    audit = C.load_audit_guarded(ident["voyage_id"], arm,
                                 ident["source_audit_sha"])
    res = C.fc3_check(view, traj, audit)
    rec = json.load(open(os.path.join(
        C.EVID, ident["voyage_id"], arm + ".json")))
    return {
        "schema": "fc3_replay_artifact_v1",
        "original_terminal_status": rec["terminal_status"],
        "original_checker_sha": rec["checker_v2"]["sha256"],
        "readjudicated_status": res["readjudicated_status"],
        "fc3_checker_sha": C.sha256_file(os.path.join(_PPE, "fc3_core.py")),
        "original_epoch2_freeze_root": ident[
            "original_epoch2_freeze_root"],
        "original_run_id": rec["run_id"],
        "original_trajectory_sha": rec["trajectory_sha256"],
        "original_audit_sha": rec["audit_sha256"],
        "sanitized_view_sha": C.sha256_file(view_path),
        "replay_identity": {
            "tool": "ppe.fc3_replay",
            "tool_sha": C.sha256_file(os.path.abspath(__file__)),
            "input_view_schema_sha": None,  # bound at freeze
        },
        "predicate_items": res["items"],
        "predicate_provenance": res["provenance"],
        "diagnostics": res["diagnostics"],
        "genset_diagnostics": res["genset_diagnostics"],
    }


def _arm_from_run_filename(view_path: str) -> str:
    # views are named <run_id>.json where run_id encodes arm|voyage in the
    # original evidence tree; recover it from the manifest mapping instead
    # of guessing: load manifest once (module-level cache).
    global _MANIFEST_CACHE
    try:
        m = _MANIFEST_CACHE
    except NameError:
        mp = os.path.join(C._MODELE, "results", "ppe_practical",
                          "fc3_replay_input_manifest.json")
        m = {r["view_sha256"]: r for r in
             json.load(open(mp))["rows"]}
        _MANIFEST_CACHE = m
    sha = C.sha256_file(view_path)
    row = _MANIFEST_CACHE.get(sha)
    if row is None:
        raise RuntimeError("view not present in frozen manifest")
    return row["arm"]


def main(argv=None) -> int:
    if os.environ.get("FC3_REPLAY_AUTHORIZED") != "YES":
        print("[fc3-replay] REFUSED: T-FC3-7 (FEASIBILITY_RE-ADJUDICATION_"
              "EPOCH_1) is BLOCKED pending the human FC-3 Replay Gate. "
              "Set FC3_REPLAY_AUTHORIZED=YES only under that authorization.")
        return 2
    os.makedirs(OUT_DIR, exist_ok=True)
    n = 0
    for fn in sorted(os.listdir(C.VIEW_DIR)):
        if not fn.endswith(".json"):
            continue
        art = replay_one(os.path.join(C.VIEW_DIR, fn))
        with open(os.path.join(OUT_DIR, fn), "w") as f:
            json.dump(art, f, indent=1, sort_keys=True)
        n += 1
    print(f"[fc3-replay] wrote {n} readjudication artifacts -> {OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

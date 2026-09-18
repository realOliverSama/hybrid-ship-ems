"""T-FC3-7 executor — transactional layer over the FROZEN adjudication engine.

The adjudication core (ppe.fc3_replay, bound by the FC-3 replay freeze SHA)
is not modified. This executor adds the TG8 execution requirements:

  - atomic writes (tmp + os.replace);
  - skip-if-identical: an existing output counts as complete only when its
    complete identity/hash bindings match the frozen replay identity;
    a mismatched/stale output stops execution (integrity);
  - forbidden-field scan of every output before commit;
  - pre/post immutability snapshot of the original epoch-2 evidence;
  - deterministic repeat check on a pre-declared non-result-selective subset
    (the first three run_ids in sorted order — fixed before execution).

Requires FC3_REPLAY_AUTHORIZED=YES (human FC-3 Replay Gate authorization).
"""
from __future__ import annotations

import json
import os
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _PPE)
import fc3_core as C  # noqa: E402
import fc3_replay as R  # noqa: E402

RPP = os.path.join(C._MODELE, "results", "ppe_practical")
MANIFEST = os.path.join(RPP, "fc3_replay_input_manifest.json")
FREEZE = os.path.join(RPP, "freeze", "fc3_readjudication_freeze.json")
GATE = os.path.join(RPP, "freeze", "FC3_READJUDICATION_GATE.json")
OUT = os.path.join(RPP, "readjudication_epoch1")
DETERMINISM_SUBSET_N = 3  # predeclared, non-result-selective (sorted prefix)

# key-based forbidden check (field NAMES, not substrings; the frozen
# fc3_core module is NOT modified — this guard lives in the executor)
FORBIDDEN_KEYS = (
    "fuel_raw", "fuel_total", "fuel_total_kg", "J_FEC", "J_FEC_kg",
    "B_perfect", "B_predicted", "L_prediction", "R_diagnostic",
    "bootstrap", "k_eq", "fuel_saving", "arm_savings", "ranking",
)


def forbidden_keys_in(obj, path="") -> list:
    bad = []
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k in FORBIDDEN_KEYS:
                bad.append(f"{path}/{k}")
            bad += forbidden_keys_in(v, f"{path}/{k}")
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            bad += forbidden_keys_in(v, f"{path}[{i}]")
    return bad


def tree_hash(root: str) -> str:
    import hashlib
    h = hashlib.sha256()
    for dp, _d, fs in os.walk(root):
        for fn in sorted(fs):
            p = os.path.join(dp, fn)
            h.update(os.path.relpath(p, root).encode())
            h.update(C.sha256_file(p).encode())
    return h.hexdigest()


def main() -> int:
    if os.environ.get("FC3_REPLAY_AUTHORIZED") != "YES":
        print("[fc3-exec] REFUSED (authorization missing)")
        return 2

    fz = json.load(open(FREEZE))
    gate = json.load(open(GATE))
    if not gate.get("overall"):
        print("[fc3-exec] STOP: freeze gate overall != true")
        return 3
    b = fz["bindings"]
    if C.sha256_file(os.path.join(_PPE, "fc3_replay.py")) != \
            b["fc3_replay_tool_sha256"]:
        print("[fc3-exec] STOP: replay tool bytes != frozen binding")
        return 3
    if C.sha256_file(os.path.join(_PPE, "fc3_core.py")) != \
            b["fc3_checker_sha256"]:
        print("[fc3-exec] STOP: checker bytes != frozen binding")
        return 3
    man = json.load(open(MANIFEST))
    if man["manifest_sha256"] != b["sanitized_input_manifest_sha256"]:
        print("[fc3-exec] STOP: manifest identity mismatch")
        return 3
    root = gate["freeze_file_sha256"]
    check_root = C.sha256_file(FREEZE)
    if check_root != root:
        print("[fc3-exec] STOP: freeze file bytes mismatch")
        return 3

    pre_tree = tree_hash(C.EVID)

    os.makedirs(OUT, exist_ok=True)
    rows = sorted(man["rows"], key=lambda r: r["run_id"])
    n_written = n_skipped = 0
    # deterministic subset: predeclared sorted prefix
    det_records = []
    for i, r in enumerate(rows):
        vp = os.path.join(C.VIEW_DIR, r["run_id"] + ".json")
        art = R.replay_one(vp)
        # bind the frozen tool + freeze root explicitly
        art["fc3_replay_tool_sha"] = b["fc3_replay_tool_sha256"]
        art["fc3_replay_freeze_root"] = root
        art["replay_identity"] = {
            "tool": "ppe.fc3_replay",
            "tool_sha": b["fc3_replay_tool_sha256"],
            "freeze_root": root,
            "input_view_schema_sha": b["input_view_schema_sha256"],
        }
        # forbidden-field scan before commit (key-based)
        blob = json.dumps(art, sort_keys=True)
        bad = forbidden_keys_in(art)
        if bad:
            print(f"[fc3-exec] STOP: forbidden fields in output {r['run_id']}:"
                  f" {bad}")
            return 4
        fp = os.path.join(OUT, r["run_id"] + ".json")
        if os.path.isfile(fp):
            old = json.load(open(fp))
            same = all(old.get(k) == art.get(k) for k in (
                "readjudicated_status", "original_run_id",
                "original_trajectory_sha", "original_audit_sha",
                "sanitized_view_sha", "fc3_checker_sha",
                "fc3_replay_tool_sha", "fc3_replay_freeze_root"))
            if not same:
                print(f"[fc3-exec] STOP: stale/mismatched output "
                      f"{r['run_id']}")
                return 4
            n_skipped += 1
        else:
            tmp = fp + ".writing"
            with open(tmp, "w") as f:
                json.dump(art, f, indent=1, sort_keys=True)
            os.replace(tmp, fp)
            n_written += 1
        if i < DETERMINISM_SUBSET_N:
            det_records.append((r["run_id"], blob))

    # deterministic repeat on the predeclared subset (fresh in-memory pass)
    det_ok = True
    for run_id, blob in det_records:
        vp = os.path.join(C.VIEW_DIR, run_id + ".json")
        art2 = R.replay_one(vp)
        art2["fc3_replay_tool_sha"] = b["fc3_replay_tool_sha256"]
        art2["fc3_replay_freeze_root"] = root
        art2["replay_identity"] = {
            "tool": "ppe.fc3_replay",
            "tool_sha": b["fc3_replay_tool_sha256"],
            "freeze_root": root,
            "input_view_schema_sha": b["input_view_schema_sha256"],
        }
        if json.dumps(art2, sort_keys=True) != blob:
            det_ok = False

    post_tree = tree_hash(C.EVID)
    exec_doc = {
        "schema": "fc3_replay_execution_record_v1",
        "freeze_root": root,
        "n_written": n_written, "n_skipped_identical": n_skipped,
        "determinism_subset": [r for r, _ in det_records],
        "determinism_ok": det_ok,
        "original_evidence_tree_before": pre_tree,
        "original_evidence_tree_after": post_tree,
        "original_evidence_unchanged": pre_tree == post_tree,
        "executor_sha256": C.sha256_file(os.path.abspath(__file__)),
    }
    json.dump(exec_doc, open(os.path.join(OUT,
                                          "_execution_record.json"), "w"),
              indent=1, sort_keys=True)
    print(f"[fc3-exec] written={n_written} skipped={n_skipped} "
          f"determinism={'PASS' if det_ok else 'FAIL'} "
          f"evidence_unchanged={pre_tree == post_tree}")
    return 0 if (det_ok and pre_tree == post_tree
                 and n_written + n_skipped == len(rows)) else 1


if __name__ == "__main__":
    sys.exit(main())

"""T-FC3-5 replay qualification + T-FC3-6 FC-3 pre-execution freeze.

Assembles: 192/192 identity/immutability/blindness qualification for the
replay tool, then the FC-3 readjudication freeze with all TG7 bindings and
explicit assertions. Emits freeze/fc3_readjudication_freeze.json +
freeze/FC3_READJUDICATION_GATE.json.

T-FC3-7 execution is NOT performed here.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _PPE)
import fc3_core as C  # noqa: E402

RPP = os.path.join(C._MODELE, "results", "ppe_practical")
QF = os.path.join(RPP, "qualification")
FZ = os.path.join(RPP, "freeze")
SPECS = os.path.join(os.path.dirname(os.path.dirname(C._MODELE)), ".specs",
                     "phase4-f-practical-predictive-ems")
ORIG_ROOT = json.load(open(os.path.join(
    RPP, "freeze", "EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json")))[
    "freeze_file_sha256"]   # authoritative, read from the gate artifact


def sha(p):
    return C.sha256_file(p)


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
    checks = []

    def check(name, ok, detail=""):
        checks.append({"name": name, "pass": bool(ok), "detail": str(detail)})
        if not ok:
            print("  GATE-FAIL", name, str(detail)[:200])

    man = json.load(open(os.path.join(RPP, "fc3_replay_input_manifest.json")))
    census = json.load(open(os.path.join(QF,
                                          "fc3_locked_evidence_census.json")))
    diff = json.load(open(os.path.join(QF,
                                        "fc3_non_target_differential.json")))
    qual = json.load(open(os.path.join(QF, "fc3_qual.json")))

    # ---- T-FC3-5: 192/192 identity / immutability / blindness ----
    n_rows = len(man["rows"])
    check("manifest_192_rows", n_rows == 192, n_rows)
    uniq = len({r["run_id"] for r in man["rows"]})
    check("run_id_uniqueness_192", uniq == 192, uniq)
    bad_hash = bad_root = bad_view = 0
    for r in man["rows"]:
        if sha(os.path.join(C.EVID, r["voyage_id"],
                            r["arm"] + ".traj.v1.mat")) != \
                r["source_trajectory_sha"]:
            bad_hash += 1
        if sha(os.path.join(C.EVID, r["voyage_id"],
                            r["arm"] + ".audit.v1.json")) != \
                r["source_audit_sha"]:
            bad_hash += 1
        if r["freeze_root"] != ORIG_ROOT:
            bad_root += 1
        vp = os.path.join(C.VIEW_DIR, r["run_id"] + ".json")
        if not os.path.isfile(vp) or sha(vp) != r["view_sha256"]:
            bad_view += 1
    check("trajectory_audit_sha_resolve_192", bad_hash == 0, bad_hash)
    check("freeze_root_192", bad_root == 0, bad_root)
    check("view_sha_resolve_192", bad_view == 0, bad_view)
    forbidden_total = 0
    for fn in os.listdir(C.VIEW_DIR):
        txt = open(os.path.join(C.VIEW_DIR, fn)).read()
        forbidden_total += sum(1 for f in C.FORBIDDEN_VIEW_FIELDS if f in txt)
    check("zero_forbidden_fields_192", forbidden_total == 0, forbidden_total)
    # guard: replay tool refuses without authorization (execution blocked)
    env = dict(os.environ)
    env.pop("FC3_REPLAY_AUTHORIZED", None)
    proc = subprocess.run([sys.executable, "-m", "ppe.fc3_replay"],
                          capture_output=True,
                          cwd=os.path.dirname(_PPE), env=env)
    check("replay_tool_refuses_uninstructed", proc.returncode == 2,
          proc.returncode)
    pre_tree = tree_hash(C.EVID)
    # exercise replay_one in memory (no writes) once for tool qualification
    import importlib
    rp = importlib.import_module("fc3_replay")
    v0 = sorted(os.listdir(C.VIEW_DIR))[0]
    art = rp.replay_one(os.path.join(C.VIEW_DIR, v0))
    check("replay_one_smoke_shape", art["schema"] == "fc3_replay_artifact_v1"
          and art["readjudicated_status"] in (
              "READJUDICATED_FEASIBLE", "READJUDICATED_HARD_INFEASIBLE"),
          art["readjudicated_status"])
    check("replay_one_no_original_mutation", tree_hash(C.EVID) == pre_tree)
    check("readjudication_output_not_emitted_yet",
          not os.path.isdir(rp.OUT_DIR) or
          len(os.listdir(rp.OUT_DIR)) == 0)

    replay_qual = {
        "schema": "fc3_replay_qualification_v1",
        "pass": all(c["pass"] for c in checks),
        "checks": checks,
        "n_rows": n_rows,
        "note": ("T-FC3-5: exhaustive 192/192 (no sampling), identity hash "
                 "resolution, immutability and blindness proofs; engine "
                 "smoke-shape only; T-FC3-7 not executed"),
    }
    json.dump(replay_qual, open(os.path.join(
        QF, "fc3_replay_qual.json"), "w"), indent=1, sort_keys=True)

    # ---- TG7 assertion proofs ----
    fz6 = json.load(open(os.path.join(
        FZ, "epoch2_pre_execution_freeze_v2.json")))
    b = fz6["bindings"]
    import hashlib
    current = {
        "modele_slx_sha256": sha(os.path.join(C._MODELE, "model",
                                              "HybridShip_ModelE.slx")),
        "cstar_impl_sha256": None,
    }
    # C* composite via the python reconstruction (same algorithm)
    from ppe import freeze as fzmod  # noqa: E402
    current["cstar_impl_sha256"] = fzmod.cstar_impl_sha_py()
    unchanged = (current["modele_slx_sha256"] == b["modele_slx_sha256"]
                 and current["cstar_impl_sha256"]
                 == b["cstar_composite_sha256"]
                 and sha(os.path.join(C._MODELE, "scripts", "+ppe",
                                      "run_cstar.m"))
                 == b["run_cstar_amended_sha256"]
                 and sha(os.path.join(C._MODELE, "scripts", "+ppe",
                                      "feasibility_contract_v2.m"))
                 == b["checker_v2_sha256"])
    check("assert_no_online_change_vs_epoch2", unchanged)
    check("assert_no_effect_inputs",
          qual["pass"] and diff["all_equal"]
          and forbidden_total == 0)
    check("assert_evidence_immutable",
          tree_hash(C.EVID) == pre_tree)

    freeze = {
        "schema": "fc3_readjudication_freeze_v1",
        "utc": __import__("datetime").datetime.now(
            __import__("datetime").timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"),
        "bindings": {
            "fc3_checker_sha256": sha(os.path.join(_PPE, "fc3_core.py")),
            "fc3_replay_tool_sha256": sha(os.path.join(_PPE, "fc3_replay.py")),
            "view_pipeline_sha256": sha(os.path.join(_PPE,
                                                     "fc3_pipeline.py")),
            "pmin_derivation_artifact_sha256": sha(os.path.join(
                SPECS, "fc3_pmin_envelope_derivation.json")),
            "discrete_policy_sha256": C.sha256_canonical(
                C.DISCRETE_POLICY),
            "input_view_schema_sha256": C.sha256_canonical(C.VIEW_SCHEMA),
            "sanitized_input_manifest_sha256": man["manifest_sha256"],
            "sanitized_input_manifest_file_sha256": sha(os.path.join(
                RPP, "fc3_replay_input_manifest.json")),
            "locked_evidence_census_sha256": sha(os.path.join(
                QF, "fc3_locked_evidence_census.json")),
            "qualification_artifact_sha256": sha(os.path.join(
                QF, "fc3_qual.json")),
            "non_target_differential_sha256": sha(os.path.join(
                QF, "fc3_non_target_differential.json")),
            "replay_qualification_sha256": sha(os.path.join(
                QF, "fc3_replay_qual.json")),
            "freeze_generator_sha256": sha(os.path.abspath(__file__)),
            "original_epoch2_freeze_root": ORIG_ROOT,
            "original_warmup_parity_binding": {
                "paradigm": "bound by SHA, not recomputed (D5)",
                "ta9_acceptance_sha256": sha(os.path.join(
                    RPP, "archival", "T_A9_EPOCH2_FORMAL_ACCEPTANCE.json")),
                "epoch2_acceptance_sha256": sha(os.path.join(
                    C.EVID, "T010_ACCEPTANCE_V2.json")),
                "parity_result": "48/48 PASS (as accepted)",
            },
            "corpus_manifest_sha256": b[
                "formal_execution_manifest_file_sha256"],
            "access_ledger_sha256": sha(os.path.join(
                RPP, "access_ledger.jsonl")),
            "epoch2_evidence_tree_sha256": pre_tree,
        },
        "assertions": {
            "SIMULINK_RERUN_REQUIRED": False,
            "LOCKED_REPLAY_ELIGIBLE": True,
            "FUEL_OUTCOMES_ACCESSIBLE_TO_FC3_CHECKER": False,
            "ORIGINAL_EPOCH2_EVIDENCE_MUTABLE": False,
        },
        "checks": checks,
    }
    overall = all(c["pass"] for c in checks)
    gate = {
        "schema": "FC3_READJUDICATION_GATE_v1",
        "task": "T-FC3-6",
        "overall": overall,
        "n_checks": len(checks),
        "n_pass": sum(1 for c in checks if c["pass"]),
        "freeze_file": "results/ppe_practical/freeze/"
                       "fc3_readjudication_freeze.json",
        "t_fc3_7": "BLOCKED — human FC-3 Replay Gate required",
        "utc": freeze["utc"],
    }
    json.dump(freeze, open(os.path.join(
        FZ, "fc3_readjudication_freeze.json"), "w"),
        indent=1, sort_keys=True)
    gate["freeze_file_sha256"] = sha(os.path.join(
        FZ, "fc3_readjudication_freeze.json"))
    json.dump(gate, open(os.path.join(
        FZ, "FC3_READJUDICATION_GATE.json"), "w"),
        indent=1, sort_keys=True)
    print(f"[fc3-freeze] overall={overall} checks={len(checks)} "
          f"freeze_sha={gate['freeze_file_sha256'][:16]}")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())

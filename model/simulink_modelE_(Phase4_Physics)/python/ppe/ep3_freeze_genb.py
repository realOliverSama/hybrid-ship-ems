#!/usr/bin/env python3
"""Generation-B epoch-3 pre-execution freeze generator (FC-3 contract recovery).

Differences from ep3_freeze.py (generation A, root 092f995d..., historical):
  - binds the FULL result-bearing execution closure, collected mechanically:
      * MATLAB transitive closure of ppe.run_epoch3_voyage via
        matlab.codetools.requiredFilesAndProducts (repo files only);
      * python import closure of the FC-3 replay toolchain and the C2 bridge
        server chain (modulefinder + explicit list);
    a file outside this closure can no longer change formal outputs undetected
    by the loader's byte check.
  - snapshots MATLAB `which -all` resolution for every critical function.
  - asserts a clean tracked working tree and records the exact commit.
  - requires the NEW recovery qualification artifacts produced on the exact
    candidate bytes (qual_ep3_recovery + qual_ep3b_runner_smoke), which
    regression-test the five defects generation A missed: absent C2 bridge,
    absent P0 formal branch, missing ctrace/audit, cross-voyage trajectory
    overwrite, wrong FC-3 invocation form.

Fails closed: any blocker => writes only a _BLOCKED artifact, no freeze file.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from modulefinder import ModuleFinder

_HERE = os.path.dirname(os.path.abspath(__file__))
_MODEL = os.path.dirname(_HERE)              # python/ppe -> python -> ModelE
REPO = os.path.dirname(_MODEL)               # git repo root
OUT = os.path.join(_MODEL, "results", "ppe_practical", "freeze")
RECOV = os.path.join(_MODEL, "results", "ppe_practical", "execution_path_recovery")


def sha(p: str) -> str:
    import hashlib
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def rel(p: str) -> str:
    return os.path.relpath(p, REPO).replace("\\", "/")


def header(p: str):
    if not os.path.exists(p):
        return {"path": rel(p), "present": False, "sha256": None}
    return {"path": rel(p), "present": True, "sha256": sha(p)}


# python modules that carry formal/scientific behaviour (explicit floor; the
# modulefinder sweep below adds anything these transitively import from the repo)
PY_FLOOR = [
    "python/ppe/fc3_core.py",
    "python/ppe/fc3_pipeline.py",
    "python/ppe/fc3_replay_execute.py",
    "python/ppe/fc3_replay_acceptance.py",
    "python/ppe/causality.py",
    "python/ppe/ledger.py",
    "python/ppe/paths.py",
    "python/ppe/c2_server.py",
    "python/ppe/c2_inference.py",
    "python/ppe/h1_assembly.py",
    "python/t445/hashutil.py",
]


def python_closure():
    """Union of the explicit floor and the repo-local import closure of the
    FC-3 replay toolchain and the C2 bridge server chain."""
    found = {os.path.join(_MODEL, p) for p in PY_FLOOR}
    seeds = [os.path.join(_MODEL, "python", "ppe", "fc3_pipeline.py"),
             os.path.join(_MODEL, "python", "ppe", "c2_server.py"),
             os.path.join(_MODEL, "python", "ppe", "fc3_core.py")]
    for seed in seeds:
        mf = ModuleFinder()
        try:
            mf.run_script(seed)
        except Exception:
            pass  # import-time side effects are not needed for the name graph
        for name, mod in mf.modules.items():
            f = getattr(mod, "__file__", None)
            if not f:
                continue
            f = os.path.abspath(f)
            if f.startswith(_MODEL + os.sep) and f.endswith(".py"):
                found.add(f)
    return sorted(found)


def matlab_closure_snapshot():
    """Collect (a) the repo-file transitive MATLAB closure of the runner and
    (b) a `which -all` snapshot of the critical functions, via one -batch call.
    Returns (closure_paths, which_all) as repo-relative structures."""
    snap = os.path.join(OUT, "_genb_matlab_snapshot.json")
    script = os.path.join(OUT, "_genb_matlab_snapshot.m")
    os.makedirs(OUT, exist_ok=True)
    with open(script, "w") as fh:
        fh.write(f"""
ROOT = '{_MODEL}';
addpath(fullfile(ROOT, 'scripts'), fullfile(ROOT, 'config'));
r = matlab.codetools.requiredFilesAndProducts( ...
    fullfile(ROOT, 'scripts', '+ppe', 'run_epoch3_voyage.m'));
files = r{{1}};
keep = {{}};
for i = 1:numel(files)
    if startsWith(files{{i}}, ROOT)
        keep{{end + 1, 1}} = files{{i}};
    end
end
crit = {{'ppe.run_epoch3_voyage', 'ppe.run_cstar', 'ppe.winfun_plan', ...
    'ppe.arm_runtime_bind', 'ppe.assert_control_bindings', 'ppe.c2_bridge', ...
    'ppe.write_sourcesel_v1', 'ppe.write_traj_v1', 'ppe.write_audit_v1', ...
    'ppe.write_ctrace_v1', 'ppe.feasibility_contract_v2', ...
    'ppe.load_epoch3b_freeze_bindings', 'ppe.inject_voyage', ...
    'ppe.rebuild_voyage', 'ppe.checkpoint_mgr', 'ppe.ppe_config', ...
    'ppe.ppe_root', 't445.metrics_fec', 't445.b2_window_dp', ...
    't445.make_simin_zoh', 't445.gen0_from_ds', 't445.prep_ehat_zoh', ...
    't445.sha256_doubles', 'model.extract_logsout', 'model.sha256_file', ...
    'model.set_source_selection_logging', 'model.set_ai_logging', ...
    'fleet_v3.plan_load', 'gate_ctrl.get_ds'}};
wa = struct();
for i = 1:numel(crit)
    w = which(crit{{i}}, '-all');
    wa.(crit{{i}}) = w;
end
jsonencode_ok = true;
doc = struct('closure', {{keep}}, 'which_all', {{wa}});
fid = fopen('{snap}', 'w');
fwrite(fid, jsonencode(doc, 'PrettyPrint', true));
fclose(fid);
disp('GENB_SNAPSHOT_OK');
""")
    subprocess.run(["matlab", "-batch", f"run('{script}')"], check=True,
                   capture_output=True, text=True, timeout=1800)
    doc = json.load(open(snap))
    return doc["closure"], doc["which_all"]


def main(argv) -> int:
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    blockers, evidence = [], {}

    # ---- git identity: exact commit + clean tracked tree ----
    head = subprocess.check_output(["git", "-C", REPO, "rev-parse", "HEAD"]
                                   ).decode().strip()
    dirty = subprocess.check_output(
        ["git", "-C", REPO, "status", "--porcelain"]).decode()
    dirty_tracked = [ln for ln in dirty.splitlines()
                     if ln.strip() and not ln.startswith("??")]
    if dirty_tracked:
        blockers.append("tracked working tree not clean: %d entries"
                        % len(dirty_tracked))

    # ---- NEW recovery qualification artifacts (final freeze only) ----
    provisional = "--provisional" in argv
    qrec = os.path.join(RECOV, "qual_ep3_recovery", "qual_ep3_recovery_result.json")
    qsmoke = os.path.join(RECOV, "qual_ep3b_runner_smoke",
                          "qual_ep3b_runner_smoke_final.json")
    if not provisional:
        for p, label in ((qrec, "qual_ep3_recovery"), (qsmoke, "qual_ep3b_runner_smoke")):
            if not os.path.exists(p):
                blockers.append(f"missing {label}: {rel(p)}")
            else:
                d = json.load(open(p))
                evidence[label] = {"path": rel(p), "sha256": sha(p),
                                   "pass": bool(d.get("pass"))}
                if not d.get("pass"):
                    blockers.append(f"{label} FAIL")

    # ---- historical qualification evidence (generation A; referenced) ----
    for lbl, p in (("EP3_P1_non_saturated_propagation",
                    os.path.join(RECOV, "qual_ep3_p1", "qual_ep3_p1_result.json")),
                   ("impact_scope_audit",
                    os.path.join(RECOV, "impact_scope_audit.json")),
                   ("runtime_writer_audit",
                    os.path.join(RECOV, "runtime_writer_audit.json")),
                   ("h2_interface",
                    os.path.join(RECOV, "h2_interface", "h2_interface_result.json"))):
        if os.path.exists(p):
            evidence[lbl] = {"path": rel(p), "sha256": sha(p)}
        else:
            blockers.append(f"missing historical evidence {lbl}")

    # ---- attempt-A exclusion ledger must exist (campaign invalidity recorded) ----
    ledger = os.path.join(_MODEL, "results", "ppe_practical", "formal_epoch3",
                          "_audit_stop_gate", "ATTEMPT_A_EXCLUSION_LEDGER.json")
    if not os.path.exists(ledger):
        blockers.append("Attempt-A exclusion ledger missing")

    # ---- execution closure (mechanical) ----
    try:
        mfiles, which_all = matlab_closure_snapshot()
    except Exception as exc:  # noqa: BLE001
        blockers.append(f"MATLAB closure snapshot failed: {exc}")
        mfiles, which_all = [], {}
    pfiles = python_closure()
    closure = sorted([header(p) for p in mfiles] + [header(p) for p in pfiles],
                     key=lambda h: h["path"])
    missing = [h["path"] for h in closure if not h["present"]]
    if missing:
        blockers.append(f"closure files missing: {missing}")

    # ---- frozen thresholds / checker identity assertions ----
    fc3 = os.path.join(_MODEL, "python", "ppe", "fc3_core.py")
    FC3_AUTH_SHA = "33a8309ad6f585bed5203cd0223484a0df3eda5ba8d4eeb8193e1ba85c34a4a9"
    if not os.path.exists(fc3) or sha(fc3) != FC3_AUTH_SHA:
        blockers.append("authoritative FC-3 checker identity drifted from "
                        "33a8309a (fc3_readjudication_freeze binding)")
    if not os.path.exists(os.path.join(
            _MODEL, "results", "dataset_v3", "formal", "windows_v3_formal.npz")):
        blockers.append("frozen corpus npz missing")

    # ---- formal namespace of generation A must carry the exclusion ledger and
    #      must NOT be reusable as the gen-B output namespace ----
    genb_namespace_note = ("generation-B campaign namespace is "
                           "results/ppe_practical/formal_epoch3_genB; "
                           "generation-A namespace formal_epoch3 is sealed "
                           "recovery evidence (ATTEMPT_A_EXCLUSION_LEDGER)")

    ok = not blockers
    assertions = {
        "EXECUTION_CLOSURE_BOUND": not missing and bool(mfiles),
        "CLEAN_TRACKED_TREE": not dirty_tracked,
        "FC3_AUTHORITATIVE_IDENTITY": os.path.exists(fc3) and sha(fc3) == FC3_AUTH_SHA,
        "ATTEMPT_A_EXCLUDED": os.path.exists(ledger),
        "FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS": 0,
    }
    if not provisional:
        assertions["QUAL_RECOVERY_PASS"] = bool(
            evidence.get("qual_ep3_recovery", {}).get("pass"))
        assertions["QUAL_RUNNER_SMOKE_PASS"] = bool(
            evidence.get("qual_ep3b_runner_smoke", {}).get("pass"))
    schema = ("ppe_epoch3_pre_execution_freeze_genb_provisional_v1" if provisional
              else "ppe_epoch3_pre_execution_freeze_genb_v1")
    fname = ("epoch3_pre_execution_freeze_genb_provisional.json" if provisional
             else "epoch3_pre_execution_freeze_genb.json")
    if not ok:
        out = {"schema": "ppe_epoch3_pre_execution_freeze_GENB_BLOCKED_v1",
               "utc": now, "overall": False, "provisional": provisional,
               "blockers": blockers or
               ["assertions not all true"], "assertions": assertions,
               "git_head": head,
               "execution_closure_n": len(closure),
               "note": "fail-closed; no freeze file written"}
        os.makedirs(OUT, exist_ok=True)
        p = os.path.join(OUT, "epoch3_pre_execution_freeze_genb_BLOCKED.json")
        json.dump(out, open(p, "w"), indent=1, ensure_ascii=False, sort_keys=True)
        print("[EP3-GENB] BLOCKED —", "; ".join(out["blockers"])[:500])
        print("wrote", rel(p))
        return 2

    freeze = {
        "schema": schema,
        "provisional": provisional,
        "formal_epoch": 3,
        "campaign_generation": "B",
        "campaign_namespace": "results/ppe_practical/formal_epoch3_genB",
        "utc": now,
        "overall": True,
        "assertions": assertions,
        "blockers": [],
        "git_head": head,
        "git_tree_clean": True,
        "superseded_roots": ["092f995d7555b6ff96ab361c330d941d15d21f5f9f1e047f8c2e50a32f9a74be"],
        "attempt_a_exclusion_ledger": rel(ledger),
        "execution_closure": closure,
        "matlab_which_all": which_all,
        "fc3_authoritative": {"path": "python/ppe/fc3_core.py",
                              "sha256": FC3_AUTH_SHA,
                              "role": "authoritative readjudication checker "
                                      "(runner records v2 classification; "
                                      "final scientific status is the "
                                      "authoritative readjudication)"},
        "corpus_policy": "RETAIN_SAME_FROZEN_CORPUS (48 voyages; corpus identity unchanged)",
        "run_manifest": "192 runs = 48 voyages x {P0,C0,C1,C2}",
        "concurrency_policy": "12-15 workers after infrastructure preflight; 20 not authorized by default",
        "provenance": {"generator": "python/ppe/ep3_freeze_genb.py",
                       "generator_sha256": sha(os.path.abspath(__file__))},
        "note": genb_namespace_note,
    }
    os.makedirs(OUT, exist_ok=True)
    p = os.path.join(OUT, fname)
    json.dump(freeze, open(p, "w"), indent=1, ensure_ascii=False, sort_keys=True)
    print("[EP3-GENB] built", rel(p), "(provisional)" if provisional else "")
    print("closure files bound:", len(closure))
    print("git_head:", head)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

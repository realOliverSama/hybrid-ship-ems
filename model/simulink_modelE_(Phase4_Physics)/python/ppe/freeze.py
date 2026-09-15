"""T-009 — PRE_EXECUTION_FREEZE_GATE (TG2/TG3/TG7/TG8/TG9).

Produces:
  freeze/formal_execution_manifest.json   (TG3: 192 arm-voyage identities)
  freeze/ppe_pre_execution_freeze.json    (TG7: exact executable identities)
  freeze/spec_validation_waiver.json      (TG8: single known waiver)
  freeze/PRE_EXECUTION_FREEZE_GATE.json   (TG9: per-prerequisite PASS/FAIL)

overall=true|false. BOTH outcomes STOP this session: false -> report;
true -> return freeze package for human Pre-Execution Gate review. T-010 is
NOT authorized here.

Run: python -m ppe.freeze   (requires numpy+scipy env, e.g. .venv_tf)
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
_MODELE_ROOT = _MODELE
_REPO = os.path.dirname(os.path.dirname(_MODELE))
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from t445 import hashutil  # noqa: E402

from ppe import ledger, paths  # noqa: E402
from ppe.teval import FR836  # noqa: E402

SLX_T445_BASELINE = (
    "190de549e38303823a1cae44d253fc1e4c32fdfb3d4b8f625a21507632d442b9")
TASKS_APPROVAL_COMMIT = "1c822062"
WAIVER_CHECK = "brainstorm.md captures user intent"
VALIDATOR = "pi-spec 0.1.0 (spec_validate)"

FREEZE_OUTPUTS = [
    "freeze/formal_execution_manifest.json",
    "freeze/ppe_pre_execution_freeze.json",
    "freeze/spec_validation_waiver.json",
    "freeze/PRE_EXECUTION_FREEZE_GATE.json",
]

ASSEMBLER_FILES = [
    "python/ppe/forecast_window.py",
    "python/ppe/c1_index.py",
    "python/ppe/formal_windows.py",
    "python/ppe/h1_assembly.py",
    "python/ppe/c2_inference.py",
    "python/ppe/c2_server.py",
]
RUNNER_FILES = [
    "scripts/+ppe/run_cstar.m",
    "scripts/+ppe/checkpoint_mgr.m",
    "scripts/+ppe/winfun_plan.m",
    "scripts/+ppe/c2_bridge.m",
    "scripts/+ppe/cstar_impl_files.m",
    "scripts/+ppe/cstar_impl_sha.m",
]
STATS_FILES = [
    "python/ppe/stats.py",
    "python/ppe/fec.py",
    "python/t445/bootstrap_cluster.py",
]

# TG9 gate items mapped to required pytest node ids (T-004/T-006/T-007)
PYTEST_ITEMS = {
    "c0_no_leakage": [
        "test_c0_t0_zero_uses_current_sample",
        "test_c0_completed_bin_mean",
        "test_c0_ignores_incomplete_current_bin",
        "test_c0_future_injection_noop",
        "test_c0_no_zeropad_and_no_post_t0",
    ],
    "warmup_causality": [
        "test_warmup_all_arms_use_c0_before_300",
        "test_differentiation_starts_at_first_anchor",
        "test_after_tfcst_is_zoh_not_new_window",
        "test_c2_never_demands_h1_during_warmup",
    ],
    "forecastwindow_parity": [
        "test_schema_equal_shape_across_sources",
        "test_invalid_windows_rejected",
        "test_partial_last_bin_weighted",
    ],
    "c1_arm_independence": [
        "test_c1_lookup_matches_store_and_arm_independent",
        "test_c1_missing_key_fail_closed",
    ],
    "plan_assembler": [
        "test_plan_structure",
        "test_warmup_collapses_all_arms_to_c0",
        "test_c0_updates_everywhere_including_after_tfcst",
        "test_c2_requires_q50_and_anchor",
        "test_c1_requires_store",
    ],
    "h1_path_parity": [
        "test_h1_matches_frozen_npz_row",
        "test_h1_causality_cutoff",
    ],
    "fec_reconstruction": ["test_fec_identity_ac805"],
    "terminal_soc_predicate": ["test_terminal_soc_admissibility_ac814"],
    "hf_config_equality": ["test_hf_config_equality_ac813"],
    "feasibility_machine": [
        "test_feasibility_record_and_withhold_ac807",
        "test_pairwise_requires_positive_jc0",
        "test_estimand_status_machine",
    ],
    "common_triplet_stats": [
        "test_voyage_effects_identity_ac806",
        "test_full_cohort_hat_equals_plain_mean",
        "test_conditional_equal_tier_weighting",
        "test_withheld_when_tier_below_3",
        "test_common_triplet_mask",
    ],
    "equal_tier_bootstrap": ["test_bootstrap_reproducible_and_tier_balanced"],
    "r_undefined": ["test_r_diagnostic_guards_ac809"],
}


def _git(args, cwd=_REPO):
    return subprocess.run(["git", "-C", cwd] + args,
                          capture_output=True, text=True, check=True)


def _file_sha(rel_or_abs):
    return hashutil.sha256_file(rel_or_abs)


def _m(rel):
    return os.path.join(_MODELE_ROOT, rel)


def _parse_cstar_files():
    src = open(_m("scripts/+ppe/cstar_impl_files.m"), encoding="utf-8").read()
    return re.findall(r"'((?:scripts|python)/[^']+)'", src)


def cstar_impl_sha_py() -> str:
    import hashlib
    h = hashlib.sha256()
    for rel in _parse_cstar_files():
        fs = _file_sha(_m(rel))
        h.update(f"{rel}:{fs}\n".encode())
    return h.hexdigest()


def build_manifest(teval: dict) -> dict:
    runs = []
    i = 0
    for row in teval["voyages"]:
        row_sha = hashutil.sha256_canonical(row)
        for arm in ("P0", "C0", "C1", "C2"):
            rid = hashutil.sha256_bytes(
                f"ppe|{arm}|{row['voyage_id']}".encode())[:16]
            runs.append({
                "exec_order": i,
                "run_id": rid,
                "arm": arm,
                "voyage_id": row["voyage_id"],
                "split_group_id": row["split_group_id"],
                "tier": row["tier"],
                "T_eval": row["T_eval"],
                "teval_row_sha256": row_sha,
                "initial_condition_identity":
                    row["initial_condition_identity"],
                "mission_identity": row["mission_identity"],
                "runner_type": ("continuous_law5" if arm == "P0"
                                else "cstar_segmented_simstate_10s"),
                "checkpoint_identity_schema": "ppe_cstar_checkpoint_identity_v1",
            })
            i += 1
    man = {
        "schema": "ppe_formal_execution_manifest_v1",
        "study": "phase4f-practical-predictive-ems",
        "n_voyages": len(teval["voyages"]),
        "arms": ["P0", "C0", "C1", "C2"],
        "n_runs": len(runs),
        "execution_order_rule": "voyage_id ascending x arm [P0,C0,C1,C2]; "
                                "fixed; no result-dependent change (TG3)",
        "teval_table_sha256": teval["teval_table_sha256"],
        "runs": runs,
    }
    man["manifest_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in man.items() if k != "manifest_sha256"})
    return man


def build_waiver() -> dict:
    bpath = os.path.join(paths.SPEC_DIR, "brainstorm.md")
    text = open(bpath, encoding="utf-8").read()
    lacks_heading = re.search(r"^##\s+User Intent", text, re.M) is None
    return {
        "schema": "KNOWN_NONBLOCKING_SPEC_VALIDATION_WAIVER",
        "validator": VALIDATOR,
        "failing_check": WAIVER_CHECK,
        "artifact": ".specs/phase4-f-practical-predictive-ems/brainstorm.md",
        "artifact_sha256": _file_sha(bpath),
        "reason": "approved closed Brainstorm artifact uses '## Intent'; lacks "
                  "the template-only '## User Intent' heading",
        "scientific_content_impact": "none",
        "approval_source": "Tasks Gate TG8",
        "waiver_condition_still_exact": lacks_heading,
        "scope": "only this exact failure is waived; any additional or "
                 "changed validator failure is blocking",
    }


def pytest_map() -> dict:
    env = dict(os.environ)
    env["PPE_LEDGER_QUIET"] = "1"   # freeze-internal pytest: no ledger growth
    env["COLUMNS"] = "220"
    proc = subprocess.run(
        [sys.executable, "-m", "pytest", "ppe/tests/", "-v", "--tb=no"],
        cwd=_PY, capture_output=True, text=True, env=env)
    passed = set()
    for line in proc.stdout.splitlines():
        m = re.search(r"ppe/tests/\S+::(\S+)\s+PASSED", line)
        if m:
            passed.add(m.group(1))
    out = {}
    for item, tests in PYTEST_ITEMS.items():
        out[item] = {"ok": all(t in passed for t in tests)
                     and len(passed) > 0,
                     "tests": tests}
    return out, proc.returncode == 0


def run() -> dict:
    os.makedirs(paths.FREEZE_DIR, exist_ok=True)
    checks = []

    def add(name, ok, detail=""):
        checks.append({"name": name, "pass": bool(ok), "detail": detail})
        return bool(ok)

    # ---------- load frozen inputs ----------
    teval = json.load(open(paths.TEVAL_TABLE_JSON, encoding="utf-8"))
    study = json.load(open(paths.STUDY_CONFIG_JSON, encoding="utf-8"))
    qual = json.load(open(f"{paths.QUAL_DIR}/ppe_qual_suite.json",
                          encoding="utf-8"))
    t005 = json.load(open(f"{paths.QUAL_DIR}/t005_runner.json",
                          encoding="utf-8"))
    t005b = json.load(open(f"{paths.QUAL_DIR}/t005b_bridge.json",
                           encoding="utf-8"))
    c2q = json.load(open(f"{paths.QUAL_DIR}/c2_inference_qual.json",
                         encoding="utf-8"))
    cov = json.load(open(f"{paths.FREEZE_DIR}/c1_coverage.json",
                         encoding="utf-8"))
    parity = json.load(open(f"{paths.QUAL_DIR}/c1_parity.json",
                            encoding="utf-8"))
    caus = json.load(open(f"{paths.QUAL_DIR}/c2_causality_verification.json",
                          encoding="utf-8"))
    c1_index = json.load(open(f"{paths.FREEZE_DIR}/c1_target_index.json",
                              encoding="utf-8"))

    # ---------- evidence chain ----------
    t001 = json.load(open(f"{paths.FREEZE_DIR}/t001_freeze_prep.json",
                          encoding="utf-8"))
    t002 = json.load(open(f"{paths.FREEZE_DIR}/t002_result.json",
                          encoding="utf-8"))
    add("T-001 complete", t001["overall"])
    add("T-002 complete", t002["overall"])
    add("T-003 complete", caus["overall"])
    add("T-008 qualification suite overall PASS", qual["overall"])

    add("C1 target coverage = 100%", cov["coverage"] == 1.0,
        f"{cov['n_present_keys']}/{cov['n_required_keys']}")
    add("C1 stored/reconstruction parity PASS", parity["verdict"] == "PASS",
        f"exact float32 {parity['n_exact_float32']}/{parity['n_anchors_checked']}")
    add("K/F causality ledger PASS", caus["overall"])
    add("C2 bridge transport PASS", t005b["pass"])
    add("C2 inference determinism PASS", c2q["verdict"] == "PASS")

    # ---------- pytest-mapped items ----------
    pm, pytest_all = pytest_map()
    for item, res in pm.items():
        add(f"pytest:{item}", res["ok"])

    # ---------- runner qualification ----------
    add("C* determinism PASS", t005["determinism_bitwise"])
    add("action bounds/grid PASS (SOC n=171)", t005["soc_grid_n171"])
    add("current-state initialization PASS", t005["resume_ok"])
    add("SimState equivalence PASS", t005["simstate_ok"],
        str(t005["simstate_continuous_vs_segmented_max_rel"]))
    add("checkpoint mismatch refusal PASS",
        t005["checkpoint_mismatch_refuse_arm"]
        and t005["checkpoint_mismatch_refuse_teval"])
    add("hard-feasibility instrumentation PASS",
        isinstance(t005["hard_feasibility_pass"], bool))
    add("terminal-SOC predicate live", t005["hf_detail"]["dSOC"] > 0)

    # ---------- TG3 manifest ----------
    manifest = build_manifest(teval)
    add("formal execution manifest = 192 identities",
        manifest["n_runs"] == 192, f"{manifest['n_runs']}")
    tiers = {}
    groups = set()
    for r in manifest["runs"]:
        groups.add(r["split_group_id"])
        if r["arm"] == "P0":
            tiers.setdefault(r["tier"], set()).add(r["split_group_id"])
    add("frozen corpus = 48 voyages / 12 groups / 4-4-4 tiers",
        manifest["n_voyages"] == 48 and len(groups) == 12
        and sorted(len(v) for v in tiers.values()) == [4, 4, 4])

    # ---------- TG7 identity bindings ----------
    head = _git(["rev-parse", "HEAD"]).stdout.strip()
    spec_shas = {}
    for f in ("brainstorm.md", "requirements.md", "design.md", "tasks.md"):
        spec_shas[f] = _file_sha(os.path.join(paths.SPEC_DIR, f))
    tasks_approved = _git(
        ["show", f"{TASKS_APPROVAL_COMMIT}:.specs/"
         f"phase4-f-practical-predictive-ems/tasks.md"]).stdout
    tasks_approved_sha = hashutil.sha256_bytes(tasks_approved.encode())

    bindings = {
        "spec_brainstorm_sha256": spec_shas["brainstorm.md"],
        "spec_requirements_sha256": spec_shas["requirements.md"],
        "spec_design_sha256": spec_shas["design.md"],
        "spec_tasks_approved_sha256": tasks_approved_sha,
        "spec_tasks_approval_commit": TASKS_APPROVAL_COMMIT,
        "spec_tasks_current_sha256": spec_shas["tasks.md"],
        "code_commit_sha": head,
        "cstar_implementation_sha256": cstar_impl_sha_py(),
        "assembler_file_sha256": {f: _file_sha(_m(f)) for f in ASSEMBLER_FILES},
        "runner_checkpoint_file_sha256": {f: _file_sha(_m(f))
                                          for f in RUNNER_FILES},
        "stats_bootstrap_file_sha256": {f: _file_sha(_m(f))
                                        for f in STATS_FILES},
        "modele_slx_sha256": _file_sha(paths.MODELE_SLX),
        "study_config_sha256": study["config_sha256"],
        "hard_feasibility_file_sha256": _file_sha(paths.HF_CONFIG_JSON),
        "hard_feasibility_object_sha256":
            study["artifacts"]["hard_feasibility"]["object_sha256"],
        "c1_target_store_sha256": cov["store_sha256"],
        "c1_coverage_index_sha256": c1_index["index_sha256"],
        "teval_table_sha256": teval["teval_table_sha256"],
        "predictor": study["artifacts"]["predictor"],
        "causality_ledger_sha256": _file_sha(paths.CAUSALITY_LEDGER_JSON),
        "corpus_manifest_file_sha256": _file_sha(paths.CORPUS_MANIFEST),
        "corpus_identity_sha256": study["artifacts"]["corpus_manifest"][
            "corpus_identity_sha256"],
        "formal_execution_manifest_sha256": manifest["manifest_sha256"],
        "bootstrap_seed": study["bootstrap"]["seed"],
    }

    # live re-hash of the FR-836 artifacts (fail-closed)
    keras_ok = (os.path.exists(paths.KERAS_S0)
                and _file_sha(paths.KERAS_S0) == FR836["keras_sha256"])
    add("REAT-T artifact identities match", keras_ok)
    add("Model-E .slx identity matches T-445 baseline",
        bindings["modele_slx_sha256"] == SLX_T445_BASELINE,
        bindings["modele_slx_sha256"][:16])

    # ---------- TG8 waiver ----------
    waiver = build_waiver()
    add("spec_validate waiver: exactly the TG8-known failure",
        waiver["waiver_condition_still_exact"])

    # ---------- access ledger / sealing ----------
    lc = ledger.ledger_counts()
    add("FORMAL access ledger count = 0", lc["by_purpose"]["FORMAL"] == 0)
    add("IID-test scientific access = 0", lc["split_contact"]["test"] == 0)
    add("OOD scientific/controller-selection access = 0",
        lc["split_contact"]["ood"] == 0)

    # ---------- working tree (evaluated BEFORE writing outputs) ----------
    # Freeze-tree semantics (documented): the tree is clean iff every entry
    # is committed except (i) st_edge_ai_upload/ (user-owned, untouched),
    # (ii) this T-009 run's own freeze output files (written after the
    # check), (iii) the append-only access ledger (audit log; bound by SHA in
    # the freeze record instead of by cleanliness).
    st = _git(["status", "--porcelain"]).stdout.splitlines()
    def _allowed(line):
        p = line[3:]
        return (p.startswith("st_edge_ai_upload/")
                or p == 'model/simulink_modelE_(Phase4_Physics)/results/'
                        'ppe_practical/access_ledger.jsonl'
                or p.startswith(
                    'model/simulink_modelE_(Phase4_Physics)/results/'
                    'ppe_practical/freeze/'))
    dirty = [l for l in st if l.strip() and not _allowed(l)]
    add("scientific working tree clean", not dirty, "; ".join(dirty[:5]))
    add("no result-bearing controller tuning (no formal runs exist)",
        lc["by_purpose"]["FORMAL"] == 0)

    overall = all(c["pass"] for c in checks)

    freeze = {
        "schema": "ppe_pre_execution_freeze_v1",
        "study": "phase4f-practical-predictive-ems",
        "formal_epoch": 1,
        "epoch_policy": "TG2 FORMAL_EPOCH: any identity change after freeze "
                        "invalidates the epoch; QUARANTINED_INVALIDATED_EPOCH; "
                        "never mix epochs",
        "bindings": bindings,
        "access_ledger_state": lc,
        "access_ledger_sha256": _file_sha(paths.ACCESS_LEDGER),
    }
    freeze["freeze_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in freeze.items() if k != "freeze_sha256"})

    gate = {
        "schema": "PRE_EXECUTION_FREEZE_GATE",
        "task": "T-009",
        "overall": overall,
        "n_checks": len(checks),
        "n_pass": sum(1 for c in checks if c["pass"]),
        "checks": checks,
        "freeze_sha256": freeze["freeze_sha256"],
        "manifest_sha256": manifest["manifest_sha256"],
        "stop_rule": "overall=false -> STOP and report; overall=true -> "
                     "ALSO STOP: human Pre-Execution Gate decides T-010 (TG9)",
    }

    hashutil.dump_json(f"{paths.FREEZE_DIR}/formal_execution_manifest.json",
                       manifest)
    hashutil.dump_json(f"{paths.FREEZE_DIR}/ppe_pre_execution_freeze.json",
                       freeze)
    hashutil.dump_json(f"{paths.FREEZE_DIR}/spec_validation_waiver.json",
                       waiver)
    hashutil.dump_json(f"{paths.FREEZE_DIR}/PRE_EXECUTION_FREEZE_GATE.json",
                       gate)
    return gate


if __name__ == "__main__":
    g = run()
    print("PRE_EXECUTION_FREEZE_GATE overall:", g["overall"],
          f"({g['n_pass']}/{g['n_checks']})")
    for c in g["checks"]:
        if not c["pass"]:
            print("FAIL -", c["name"], c["detail"])

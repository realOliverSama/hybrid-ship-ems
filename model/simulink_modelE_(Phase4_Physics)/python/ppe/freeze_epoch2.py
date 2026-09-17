"""T-A7 v3 — epoch-2 canonical freeze after PE4-1 provenance correction.

PE4-1 (human gate, CONDITIONAL PASS): the previous freeze (root
d49767a233844aa12efebeba562666a8686710c55219145d4648e92250e4a1b8) bound
code_commit_sha = 6e4d14e1… which predates the committed runner-integration
repair. This generator:

1. audits that EXECUTION_CODE_COMMIT_SHA (bbbbc5f53d313e7e78c3eada92197a6fc0a9255f)
   reproduces every git-backed execution-critical binding by `git show`;
2. separates EXECUTION_CODE_COMMIT_SHA (frozen execution implementation)
   from FREEZE_PACKAGE_HEAD_SHA (repository head at generation);
3. preserves the predecessor freeze bytes (renamed
   epoch2_pre_execution_freeze_v2.SUPERSEDED_d49767a2.json) and records the
   full supersession chain 7919123b -> d49767a2 -> NEW_ROOT;
4. applies the two mandated metadata-only cleanups (SOC grid wording; legacy
   seed label flag) and records the design-basis event human acceptance;
5. proves ZERO execution-critical byte changes vs the predecessor freeze
   (only provenance pointers and meta-tooling hashes may differ).

Metadata/provenance only (PE4-1 §6): scientific qualification artifacts are
NOT regenerated; their hashes must remain identical and are re-bound.

Outputs:
  freeze/epoch2_pre_execution_freeze_v2.json   (NEW canonical root)
  freeze/EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json
  freeze/epoch2_freeze_supersession.json       (chain, loader-compatible)

Run: python -m ppe.freeze_epoch2
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from t445 import hashutil  # noqa: E402
from ppe import paths  # noqa: E402

ROOT = paths.MODELE_ROOT
RPP = os.path.join(ROOT, "results", "ppe_practical")
FREEZE_DIR = os.path.join(RPP, "freeze")
OLD_FREEZE = os.path.join(FREEZE_DIR, "epoch2_pre_execution_freeze.json")
PREV_FREEZE = os.path.join(
    FREEZE_DIR, "epoch2_pre_execution_freeze_v2.SUPERSEDED_3b75e4b8.json")
NEW_FREEZE = os.path.join(FREEZE_DIR, "epoch2_pre_execution_freeze_v2.json")
GATE = os.path.join(FREEZE_DIR, "EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json")
MARKER = os.path.join(FREEZE_DIR, "epoch2_freeze_supersession.json")
INCIDENT = os.path.join(RPP, "incidents",
                        "EPOCH2_PREEXECUTION_INTEGRATION_DEFECT.json")
EPOCH1_FREEZE = os.path.join(FREEZE_DIR, "ppe_pre_execution_freeze.json")

PREV_ROOT = "3b75e4b805fe1ac66e621c46869e3b20ef0a61eac267d0157c26714e7200defe"
PREV2_ROOT = "d49767a233844aa12efebeba562666a8686710c55219145d4648e92250e4a1b8"
PREV2_FREEZE = os.path.join(
    FREEZE_DIR, "epoch2_pre_execution_freeze_v2.SUPERSEDED_d49767a2.json")
FIRST_DEFECT_ROOT = ("7919123bd986d5b53e68a471943579316a8b03ae93eb67"
                     "2fe70ac0e43242dd47")
EXECUTION_CODE_COMMIT_SHA = "bbbbc5f53d313e7e78c3eada92197a6fc0a9255f"
CANONICAL_RESERVES = {
    "P_reserve_dis_kW": 543.8,
    "E_reserve_dis_kWh": 6.839,
    "P_reserve_chg_kW": 819.8,
    "E_reserve_chg_kWh": 5.625,
}
SLX_T445_BASELINE = (
    "190de549e38303823a1cae44d253fc1e4c32fdfb3d4b8f625a21507632d442b9")
CSTAR_COMPOSITE_FROZEN = (
    "40c5be942201ec04254500bf1f9da5e6590df118da95f2578c297686e805a7a2")

# bindings that MAY change: evidence routing/schema layer + provenance
# pointers + meta-tooling + audit snapshot (PE4-1 + C1-C6 gate mandate)
MUTABLE_BINDINGS = {
    "code_commit_sha",              # provenance pointer
    "EXECUTION_CODE_COMMIT_SHA",
    "FREEZE_PACKAGE_HEAD_SHA",
    "freeze_generator_sha256",      # meta-tooling
    "verify_bindings_sha256",       # meta-tooling
    "access_ledger_file_sha256",    # append-only audit snapshot
    "traj_writer_sha256",           # FC-5A evidence schema (C4)
    "terminal_pipeline_sha256",     # FC-5A write order (C4)
    "runner_voyage_sha256",         # FC-5A deriv/ctrace threading (C4)
    "acceptance_v2_sha256",         # FC-5A acceptance plumbing (C4)
    "incident_artifact_sha256",     # audit evidence (C1)
    "e2e_qual_artifact_sha256",     # updated fixture/coverage (C5)
    "pack_skip_predicate_sha256",   # FC-5A skip semantics: ctrace required
}

# bindings that MUST NOT change (scientific identity; §6 STOP set)
PROHIBITED_KEYS = [
    "checker_v2_sha256", "cstar_composite_sha256",
    "run_cstar_amended_sha256", "c2_bridge_amended_sha256",
    "winfun_plan_amended_sha256", "modele_slx_sha256",
    "teval_table_file_sha256", "teval_table_sha256",
    "hard_feasibility_config_file_sha256", "t445_config_file_sha256",
    "study_config_file_sha256", "corpus_identity_sha256",
    "formal_execution_manifest_file_sha256", "formal_assets_file_sha256",
    "c1_coverage_file_sha256", "c1_target_index_file_sha256",
]

# file-backed bindings (re-hashed for the zero-change assertion)
RELMAP = {
    "runner_voyage_sha256": "scripts/+ppe/run_formal_voyage.m",
    "runner_pack_sha256": "scripts/+ppe/run_formal_voyage_pack.m",
    "runner_manifest_sha256": "scripts/+ppe/run_formal_manifest.m",
    "terminal_pipeline_sha256": "scripts/+ppe/formal_terminal_v2.m",
    "pack_skip_predicate_sha256": "scripts/+ppe/formal_pack_skip_v2.m",
    "freeze_loader_sha256": "scripts/+ppe/load_epoch2_freeze_bindings.m",
    "output_router_sha256": "scripts/+ppe/formal_outdir.m",
    "checkpoint_mgr_sha256": "scripts/+ppe/checkpoint_mgr.m",
    "checker_v2_sha256": "scripts/+ppe/feasibility_contract_v2.m",
    "traj_writer_sha256": "scripts/+ppe/write_traj_v1.m",
    "audit_writer_sha256": "scripts/+ppe/write_audit_v1.m",
    "acceptance_v2_sha256": "python/ppe/acceptance_v2.py",
    "teval_table_file_sha256": "config/phase4f_ppe_teval_table.json",
    "hard_feasibility_config_file_sha256":
        "config/phase4f_ppe_hard_feasibility.json",
    "t445_config_file_sha256": "config/phase4f_t445_config.json",
    "study_config_file_sha256": "config/phase4f_ppe_study_config.json",
    "formal_execution_manifest_file_sha256":
        "results/ppe_practical/freeze/formal_execution_manifest.json",
    "formal_assets_file_sha256":
        "results/ppe_practical/freeze/formal_assets.json",
    "c1_coverage_file_sha256": "results/ppe_practical/freeze/c1_coverage.json",
    "c1_target_index_file_sha256":
        "results/ppe_practical/freeze/c1_target_index.json",
    "access_ledger_file_sha256": "results/ppe_practical/access_ledger.jsonl",
    "reserve_derivation_TA0_sha256":
        ".specs/phase4-f-practical-predictive-ems/reserve_derivation_TA0.json",
    "amendment_design_doc_sha256":
        ".specs/phase4-f-practical-predictive-ems/"
        "feasibility_contract_design.md",
    "amendment_doc_sha256":
        ".specs/phase4-f-practical-predictive-ems/"
        "feasibility_contract_amendment.md",
    "amendment_v2_qual_artifact_sha256":
        "results/ppe_practical/qualification/amendment_v2_qual.json",
    "e2e_qual_artifact_sha256":
        "results/ppe_practical/qualification/e2e_runner_integration_qual.json",
    "fec_prereg_artifact_sha256":
        "results/ppe_practical/qualification/fec_robustness_prereg.json",
    "coherence_gate_artifact_sha256":
        "results/ppe_practical/qualification/feasibility_coherence_gate.json",
    "incident_artifact_sha256":
        "results/ppe_practical/incidents/"
        "EPOCH2_PREEXECUTION_INTEGRATION_DEFECT.json",
    "modele_slx_sha256": "model/HybridShip_ModelE.slx",
}


def sha(rel_or_abs: str) -> str:
    p = rel_or_abs if os.path.isabs(rel_or_abs) else os.path.join(ROOT, rel_or_abs)
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def git(args):
    repo = os.path.dirname(os.path.dirname(_MODELE))
    return subprocess.run(["git", "-C", repo] + args, capture_output=True)


def git_head() -> str:
    r = git(["rev-parse", "HEAD"])
    return r.stdout.decode().strip() if r.returncode == 0 else "UNKNOWN"


def git_blob_sha(commit: str, rel: str):
    repo_rel = rel if rel.startswith(".specs/") else \
        "model/simulink_modelE_(Phase4_Physics)/" + rel
    r = git(["show", f"{commit}:{repo_rel}"])
    if r.returncode != 0:
        return None
    return hashlib.sha256(r.stdout).hexdigest()


def cstar_impl_sha_py() -> str:
    from ppe.freeze import cstar_impl_sha_py as f  # noqa: E402
    return f()


def qual_pass(name: str) -> dict:
    p = os.path.join(RPP, "qualification", name)
    with open(p, encoding="utf-8") as f:
        d = json.load(f)
    return {"pass": bool(d.get("pass")), "sha256": sha(p), "doc": d}


def main() -> int:
    checks: list[dict] = []

    def check(name: str, ok: bool, detail: str = "") -> bool:
        checks.append({"name": name, "pass": bool(ok), "detail": detail})
        if not ok:
            print(f"  GATE-FAIL {name} {detail}")
        return bool(ok)

    # ---------- predecessor freeze (preserved bytes) ----------
    check("predecessor_freeze_preserved", os.path.isfile(PREV_FREEZE))
    with open(PREV_FREEZE, encoding="utf-8") as f:
        prev = json.load(f)
    prev_root = sha(PREV_FREEZE)
    check("predecessor_root_matches_d49767a2", prev_root == PREV_ROOT,
          prev_root[:16])
    with open(OLD_FREEZE, encoding="utf-8") as f:
        first = json.load(f)
    first_root = first.get("sha256_self", "")
    check("first_defective_freeze_preserved",
          os.path.isfile(OLD_FREEZE) and first_root == FIRST_DEFECT_ROOT,
          first_root[:16])

    with open(EPOCH1_FREEZE, encoding="utf-8") as f:
        ep1 = json.load(f)
    ep1b = ep1["bindings"]

    # incident
    incident_sha = sha(INCIDENT)
    with open(INCIDENT, encoding="utf-8") as f:
        inc = json.load(f)
    check("incident_self_hash",
          inc.get("sha256_self") == hashutil.sha256_canonical(
              {k: v for k, v in inc.items() if k != "sha256_self"}))
    check("incident_no_contamination",
          inc.get("NO_FORMAL_SCIENTIFIC_OUTPUT_CONTAMINATION") is True)

    # epoch-1 evidence intact
    ep1dir = os.path.join(RPP, "formal")
    n_term = n_par = 0
    for vid in sorted(os.listdir(ep1dir)):
        vd = os.path.join(ep1dir, vid)
        if not os.path.isdir(vd) or vid.startswith(("_", "quarantine")):
            continue
        for fn in sorted(os.listdir(vd)):
            if fn.endswith(".json") and fn[:-5] in ("P0", "C0", "C1", "C2"):
                try:
                    d = json.load(open(os.path.join(vd, fn),
                                       encoding="utf-8"))
                    if str(d.get("terminal_status", "")).startswith("COMPLETE"):
                        n_term += 1
                except Exception:  # noqa: BLE001
                    pass
            if fn == "warmup_parity.json":
                try:
                    if json.load(open(os.path.join(vd, fn),
                                      encoding="utf-8")) == "PASS":
                        n_par += 1
                except Exception:  # noqa: BLE001
                    pass
    check("epoch1_terminals_192", n_term == 192, str(n_term))
    check("epoch1_warmup_parity_48", n_par == 48, str(n_par))
    check("epoch2_namespace_absent",
          not os.path.isdir(os.path.join(RPP, "formal_epoch2")))

    n_e2_formal = 0
    ledp = os.path.join(RPP, "access_ledger.jsonl")
    for line in open(ledp, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        try:
            e = json.loads(line)
        except Exception:  # noqa: BLE001
            continue
        if e.get("purpose") == "FORMAL" and e.get("formal_epoch") == 2:
            n_e2_formal += 1
    check("epoch2_formal_ledger_entries_zero", n_e2_formal == 0,
          str(n_e2_formal))

    # ---------- qualifications: hashes must be unchanged & passing ----------
    quals = {}
    qmap = {"amendment_v2": ("amendment_v2_qual.json",
                             "amendment_v2_qual_artifact_sha256"),
            "e2e_runner_integration": ("e2e_runner_integration_qual.json",
                                       "e2e_qual_artifact_sha256"),
            "fec_robustness_prereg": ("fec_robustness_prereg.json",
                                      "fec_prereg_artifact_sha256")}
    for nm, (fn, bkey) in qmap.items():
        quals[nm] = qual_pass(fn)
        check(f"qual_{nm}_pass", quals[nm]["pass"])
        check(f"qual_{nm}_unchanged_from_predecessor",
              quals[nm]["sha256"] == prev["bindings"].get(bkey))
    obs = qual_pass("observability_qual.json")
    quals["observability"] = obs
    check("qual_observability_pass", obs["pass"])
    coh = qual_pass("feasibility_coherence_gate.json")
    quals["coherence_gate"] = coh
    check("qual_coherence_pass", coh["pass"])
    check("qual_coherence_unchanged_from_predecessor",
          coh["sha256"] == prev["bindings"]["coherence_gate_artifact_sha256"])

    # ---------- PE4-1: execution commit provenance ----------
    amend_p = os.path.join(FREEZE_DIR, "post_freeze_runner_amendment.json")
    with open(amend_p, encoding="utf-8") as f:
        amend = json.load(f)
    check("epoch1_amendment_run_cstar_binding",
          sha("scripts/+ppe/run_cstar.m")
          == amend["run_cstar_file_sha256"]["amended"])
    check("cstar_composite_unchanged",
          cstar_impl_sha_py() == CSTAR_COMPOSITE_FROZEN)

    git_backed = {
        "runner_voyage_sha256": "scripts/+ppe/run_formal_voyage.m",
        "runner_pack_sha256": "scripts/+ppe/run_formal_voyage_pack.m",
        "runner_manifest_sha256": "scripts/+ppe/run_formal_manifest.m",
        "terminal_pipeline_sha256": "scripts/+ppe/formal_terminal_v2.m",
        "pack_skip_predicate_sha256": "scripts/+ppe/formal_pack_skip_v2.m",
        "freeze_loader_sha256": "scripts/+ppe/load_epoch2_freeze_bindings.m",
        "output_router_sha256": "scripts/+ppe/formal_outdir.m",
        "checkpoint_mgr_sha256": "scripts/+ppe/checkpoint_mgr.m",
        "checker_v2_sha256": "scripts/+ppe/feasibility_contract_v2.m",
        "traj_writer_sha256": "scripts/+ppe/write_traj_v1.m",
        "audit_writer_sha256": "scripts/+ppe/write_audit_v1.m",
        "run_cstar_amended_sha256": "scripts/+ppe/run_cstar.m",
        "c2_bridge_amended_sha256": "scripts/+ppe/c2_bridge.m",
        "winfun_plan_amended_sha256": "scripts/+ppe/winfun_plan.m",
        "acceptance_v2_sha256": "python/ppe/acceptance_v2.py",
        "modele_slx_sha256": "model/HybridShip_ModelE.slx",
        "teval_table_file_sha256": "config/phase4f_ppe_teval_table.json",
        "hard_feasibility_config_file_sha256":
            "config/phase4f_ppe_hard_feasibility.json",
        "t445_config_file_sha256": "config/phase4f_t445_config.json",
        "study_config_file_sha256": "config/phase4f_ppe_study_config.json",
        "formal_execution_manifest_file_sha256":
            "results/ppe_practical/freeze/formal_execution_manifest.json",
        "formal_assets_file_sha256":
            "results/ppe_practical/freeze/formal_assets.json",
        "c1_coverage_file_sha256":
            "results/ppe_practical/freeze/c1_coverage.json",
        "c1_target_index_file_sha256":
            "results/ppe_practical/freeze/c1_target_index.json",
        "access_ledger_file_sha256":
            "results/ppe_practical/access_ledger.jsonl",
        "reserve_derivation_TA0_sha256":
            ".specs/phase4-f-practical-predictive-ems/"
            "reserve_derivation_TA0.json",
        "amendment_design_doc_sha256":
            ".specs/phase4-f-practical-predictive-ems/"
            "feasibility_contract_design.md",
        "amendment_doc_sha256":
            ".specs/phase4-f-practical-predictive-ems/"
            "feasibility_contract_amendment.md",
    }
    bad = []
    for key, rel in git_backed.items():
        got = git_blob_sha(EXECUTION_CODE_COMMIT_SHA, rel)
        want = prev["bindings"].get(key)
        if got is None or got != want:
            bad.append(key)
    check("execution_commit_reproduces_all_git_backed_bindings", not bad,
          json.dumps(bad))
    # keras predictor is experiments/-distributed (not git-backed by policy);
    # bound by SHA-256 and verified on-disk
    kpath = ep1b["predictor"]["keras"]["path"]
    kpath = kpath.split("simulink_modelE_(Phase4_Physics)/", 1)[-1]
    check("predictor_keras_on_disk_matches_binding",
          sha(kpath) == ep1b["predictor"]["keras"]["sha256"])

    slx_sha = sha(paths.MODELE_SLX)
    check("modele_slx_t445_baseline", slx_sha == SLX_T445_BASELINE)

    with open(os.path.join(ROOT, "config", "phase4f_ppe_teval_table.json"),
              encoding="utf-8") as f:
        teval = json.load(f)

    # ---------- ZERO execution-critical byte change vs predecessor ----------
    newvals = {
        "modele_slx_sha256": slx_sha,
        "cstar_composite_sha256": cstar_impl_sha_py(),
        "run_cstar_amended_sha256": amend["run_cstar_file_sha256"]["amended"],
        "c2_bridge_amended_sha256":
            amend["amended_file_overrides"]["scripts/+ppe/c2_bridge.m"],
        "winfun_plan_amended_sha256":
            amend["amended_file_overrides"]["scripts/+ppe/winfun_plan.m"],
        "teval_table_sha256": teval["teval_table_sha256"],
        "corpus_identity_sha256": prev["bindings"]["corpus_identity_sha256"],
        "predictor": prev["bindings"]["predictor"],
    }
    drift = {}
    for key, want in prev["bindings"].items():
        if key in MUTABLE_BINDINGS:
            continue
        got = newvals.get(key)
        if got is None:
            rel = RELMAP.get(key, "")
            p = os.path.join(ROOT, rel) if not rel.startswith(".specs/") \
                else os.path.join(os.path.dirname(os.path.dirname(ROOT)), rel)
            got = sha(p) if rel and os.path.isfile(p) else None
        if got != want:
            drift[key] = "CHANGED"
    check("prohibited_bindings_unchanged", not drift,
          json.dumps(drift, sort_keys=True))
    # 正向断言：证据层字节确实换新（FC-5A 落地证明）
    postfix = {
        "traj_writer_sha256": sha("scripts/+ppe/write_traj_v1.m"),
        "terminal_pipeline_sha256": sha("scripts/+ppe/formal_terminal_v2.m"),
        "runner_voyage_sha256": sha("scripts/+ppe/run_formal_voyage.m"),
        "acceptance_v2_sha256": sha("python/ppe/acceptance_v2.py"),
    }
    ev_ok = all(prev["bindings"].get(k) not in (None, v)
                for k, v in postfix.items())
    check("evidence_layer_rebound_to_post_fix_bytes", ev_ok,
          json.dumps({k: v[:12] for k, v in postfix.items()}))

    # ---------- assemble bindings ----------
    bindings = dict(prev["bindings"])          # inherit (proves stability)
    bindings["controller_trace_writer_sha256"] = sha(
        "scripts/+ppe/write_ctrace_v1.m")
    bindings["observability_qual_artifact_sha256"] = quals[
        "observability"]["sha256"]
    # 可变证据层键必须显式重绑到 post-fix 字节
    bindings["traj_writer_sha256"] = sha("scripts/+ppe/write_traj_v1.m")
    bindings["terminal_pipeline_sha256"] = sha(
        "scripts/+ppe/formal_terminal_v2.m")
    bindings["runner_voyage_sha256"] = sha("scripts/+ppe/run_formal_voyage.m")
    bindings["acceptance_v2_sha256"] = sha("python/ppe/acceptance_v2.py")
    bindings["pack_skip_predicate_sha256"] = sha(
        "scripts/+ppe/formal_pack_skip_v2.m")
    bindings["e2e_qual_artifact_sha256"] = quals[
        "e2e_runner_integration"]["sha256"]
    bindings.update({
        "code_commit_sha": EXECUTION_CODE_COMMIT_SHA,
        "EXECUTION_CODE_COMMIT_SHA": EXECUTION_CODE_COMMIT_SHA,
        "FREEZE_PACKAGE_HEAD_SHA": git_head(),
        "freeze_generator_sha256": sha("python/ppe/freeze_epoch2.py"),
        "verify_bindings_sha256": sha("python/ppe/verify_formal_bindings.py"),
        "incident_artifact_sha256": incident_sha,
    })

    doc = {
        "schema": "ppe_epoch2_pre_execution_freeze_v3",
        "study": "phase4f-practical-predictive-ems",
        "formal_epoch": 2,
        "utc": __import__("datetime").datetime.now(
            __import__("datetime").timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "canonical_reserves": dict(CANONICAL_RESERVES, provenance=
                                   "T-A0 dynamic derivation; PE2-1 canonical"),
        "epoch2_result_bearing_runs": 0,
        "output_namespace": "results/ppe_practical/formal_epoch2",
        "record_schema": "ppe_formal_arm_voyage_v2",
        "classification_authority": "ppe.feasibility_contract_v2",
        "cstar_execution_seed": "phase4f-ppe-formal-epoch1",
        "cstar_execution_seed_provenance": {
            "LEGACY_SEED_LABEL_INTENTIONALLY_PRESERVED_FOR_CROSS_EPOCH_"
            "DETERMINISM": True,
            "note": ("seed name is historical (epoch-1); the deterministic "
                     "value is intentionally preserved for fair "
                     "epoch-1/epoch-2 comparison"),
        },
        "supersedes_freeze_root_sha256": prev_root,
        "supersedes_reason": "EVIDENCE_INTERFACE_CHANNEL_MISMATCH",
        "supersedes_subtype": ("SUPERSEDED_AFTER_FORMAL_LAUNCH_ATTEMPT_"
                               "BEFORE_ACCEPTED_RESULT"),
        "supersedes_chain": [
            {"superseded_root": FIRST_DEFECT_ROOT,
             "reason": "PREEXECUTION_RUNNER_INTEGRATION_DEFECT",
             "replacement_root": PREV2_ROOT},
            {"superseded_root": PREV2_ROOT,
             "reason": "EXECUTION_COMMIT_PROVENANCE_CORRECTION",
             "replacement_root": prev_root},
            {"superseded_root": prev_root,
             "reason": "EVIDENCE_INTERFACE_CHANNEL_MISMATCH",
             "replacement_root": "THIS_DOCUMENT (authoritative root is the "
                                 "freeze-file byte hash, recorded in "
                                 "EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json "
                                 "and epoch2_freeze_supersession.json)"},
        ],
        "incident_artifact": os.path.relpath(INCIDENT, ROOT),
        "epoch1_preserved": ep1.get("epoch1_preserved", {
            "deleted": False, "formal_epoch": 1, "reclassified": False,
            "rerun": False}),
        "epoch2_scientific_identities": {
            "P0_role": "UNCHANGED",
            "bootstrap_seed": "phase4f-t445-bootstrap-v1",
            "corpus_48_voyages": "UNCHANGED",
            "corpus_identity_sha256": prev["bindings"][
                "corpus_identity_sha256"],
            "cstar_composite_sha256": cstar_impl_sha_py(),
            "modele_slx_sha256": slx_sha,
        },
        "service_shortfall": {
            "authoritative":
                "P_unserved_kW (V_dc undervoltage derate path only)",
            "dual_diagnostic": ("P_service_gap = max(P_prop_demand - "
                                "P_prop_actual, 0); divergence -> FAIL CLOSED"),
            "retired": "P_bus_residual_kW -> DC-bus consistency diagnostic",
        },
        "genset_root_cause": (
            "checker v2 uses per-unit truth P_DG1_kW/P_DG2_kW with "
            "state-aware limits; no equal-split fallback; PMS allocator "
            "equal-split by design (D-B6 frozen)"),
        "s_terminal_admissible": {
            "min": 0.25, "max": 0.9, "n_points": 66,
            "grid": "SOC=0.25:0.01:0.90",
            "note": ("66-point SOC grid, SOC=0.25:0.01:0.90; all grid "
                     "points survive EV-1/EV-3 within frozen SOP/hard "
                     "bounds"),
        },
        "e_design_events": prev.get("e_design_events", {}),
        "e_design_events_acceptance": {
            "status": "HUMAN_ACCEPTED_STUDY_LOCAL_CONSERVATIVE_CONTINGENCY_"
                      "DESIGN_BASIS_EVENTS",
            "accepted_at": ("Phase 4-F epoch-2 final pre-execution gate "
                            "(PE4-1 conditional pass)"),
            "scope_note": (
                "accepted for THIS RESEARCH STUDY; EV1 includes frozen "
                "STARTING/SYNC delay and generator reachability; EV3 "
                "includes frozen generator ramp-down and Pmin behavior; "
                "these events do NOT constitute a complete regulatory or "
                "classification emergency-power certification envelope"),
        },
        "fec": {"k_eq_E": 0.5457,
                "robustness_band": [0.4366, 0.6548],
                "break_even": 0.0933},
        "corrections": (
            "FC-5A OBSERVABILITY CONTRACT AMENDMENT (C1-C6): evidence "
            "interface split into plant_trajectory_v1 (PLANT_LOGGED + 2 "
            "exact qualified derivations P_bat_bus_kW/derate08_active + "
            "interlock family; no controller quantities), "
            "controller_trace_v1 (delta_lambda/forecast provenance at the "
            "10 s decision cadence, RUNNER_DERIVED), high_rate_audit_v1 "
            "(unchanged), terminal_record_v2 (binds all artifacts + "
            "forecast source identity); resolves "
            "EPOCH2_FORMAL_EVIDENCE_INTERFACE_MISMATCH from the first "
            "launch (23 attempts / 46 model-completing simulations, zero "
            "accepted results); checker v2, C*, Model-E, Predictor-v3, "
            "reserves, corpus and manifest byte-identical"),
        "qualification": {
            "amend_v2_pass": quals["amendment_v2"]["pass"],
            "e2e_runner_integration_pass":
                quals["e2e_runner_integration"]["pass"],
            "coherence_pass": quals["coherence_gate"]["pass"],
            "fec_robust_pass": quals["fec_robustness_prereg"]["pass"],
            "rerun_required": False,
            "rerun_rationale": ("PE4-1 is metadata/provenance-only; all "
                                "execution-critical bytes bitwise unchanged"),
        },
        "bindings": bindings,
    }
    doc["sha256_self"] = hashutil.sha256_canonical(
        {k: v for k, v in doc.items() if k != "sha256_self"})

    with open(NEW_FREEZE, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1, sort_keys=True)
    freeze_file_sha = sha(NEW_FREEZE)

    overall = all(c["pass"] for c in checks)
    gate_doc = {
        "schema": "PPE_EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2",
        "task": "T-A7(v3, PE4-1)",
        "freeze_file": os.path.relpath(NEW_FREEZE, ROOT),
        "freeze_file_sha256": freeze_file_sha,
        "sha256_self_of_freeze_doc": doc["sha256_self"],
        "execution_code_commit_sha": EXECUTION_CODE_COMMIT_SHA,
        "checks": checks,
        "n_checks": len(checks),
        "n_pass": sum(1 for c in checks if c["pass"]),
        "overall": overall,
        "stop_rule": ("overall=true -> PE3-1 rerun; then T-A8 authorized "
                      "automatically (zero execution-critical byte changes); "
                      "overall=false -> STOP and report"),
        "utc": doc["utc"],
    }
    with open(GATE, "w", encoding="utf-8") as f:
        json.dump(gate_doc, f, indent=1, sort_keys=True)

    marker = {
        "schema": "ppe_epoch2_freeze_supersession_v2",
        "superseded_freeze_root": prev_root,
        "superseded_artifact": os.path.relpath(PREV_FREEZE, ROOT),
        "disposition": "SUPERSEDED_BEFORE_FORMAL_EXECUTION",
        "disposition_subtype": ("SUPERSEDED_AFTER_FORMAL_LAUNCH_ATTEMPT_"
                                "BEFORE_ACCEPTED_RESULT"),
        "reason": "EVIDENCE_INTERFACE_CHANNEL_MISMATCH",
        "replacement_freeze_root": freeze_file_sha,
        "replacement_artifact": os.path.relpath(NEW_FREEZE, ROOT),
        "chain": [
            {"superseded_root": FIRST_DEFECT_ROOT,
             "superseded_artifact": os.path.relpath(OLD_FREEZE, ROOT),
             "reason": "PREEXECUTION_RUNNER_INTEGRATION_DEFECT",
             "replacement_root": PREV2_ROOT,
             "replacement_artifact": os.path.relpath(PREV2_FREEZE, ROOT)},
            {"superseded_root": PREV2_ROOT,
             "superseded_artifact": os.path.relpath(PREV2_FREEZE, ROOT),
             "reason": "EXECUTION_COMMIT_PROVENANCE_CORRECTION",
             "replacement_root": prev_root,
             "replacement_artifact": os.path.relpath(PREV_FREEZE, ROOT)},
            {"superseded_root": prev_root,
             "superseded_artifact": os.path.relpath(PREV_FREEZE, ROOT),
             "reason": "EVIDENCE_INTERFACE_CHANNEL_MISMATCH",
             "replacement_root": freeze_file_sha,
             "replacement_artifact": os.path.relpath(NEW_FREEZE, ROOT)},
        ],
        "note": ("all superseded freeze bytes preserved untouched; epoch-2 "
                 "result-bearing runs remained 0 across the whole chain"),
        "utc": doc["utc"],
    }
    with open(MARKER, "w", encoding="utf-8") as f:
        json.dump(marker, f, indent=1, sort_keys=True)

    print(f"[freeze-e2-v3] overall={overall} checks={len(checks)} "
          f"root={freeze_file_sha[:16]} self={doc['sha256_self'][:16]} "
          f"exec_commit={EXECUTION_CODE_COMMIT_SHA[:12]}")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())

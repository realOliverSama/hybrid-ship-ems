"""T-A7 v2 — epoch-2 pre-execution freeze regeneration (recovery auth §11).

Generates, after the repaired runner integration and all affected
qualifications PASS:

  freeze/epoch2_pre_execution_freeze_v2.json   (schema ppe_epoch2_pre_execution_freeze_v3)
  freeze/EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json
  freeze/epoch2_freeze_supersession.json       (old defective freeze -> SUPERSEDED_BEFORE_FORMAL_EXECUTION)

The freeze binds, by exact SHA-256, every execution-critical identity
mandated by §11: runner (manifest/pack/voyage), epoch-2 output router,
terminal pipeline, checkpoint/resume, checker v2, trajectory writer,
high-rate audit writer, acceptance v2, reserve derivation, terminal
admissibility, service-shortfall semantics, generator state semantics, FEC,
Model-E, C* composite, C0/C1/C2 adapters, Predictor-v3 assets, C1 target
store/index, T_eval, formal corpus, 192-run manifest, qualification
artifacts (amendment v2 + E2E), coherence gate, access ledger, incident
artifact, and the superseded defective freeze root.

T-A8 is NOT started here. Overall=true only re-opens the human Epoch-2
Pre-Execution Gate.

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
NEW_FREEZE = os.path.join(FREEZE_DIR, "epoch2_pre_execution_freeze_v2.json")
GATE = os.path.join(FREEZE_DIR, "EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json")
MARKER = os.path.join(FREEZE_DIR, "epoch2_freeze_supersession.json")
INCIDENT = os.path.join(RPP, "incidents",
                        "EPOCH2_PREEXECUTION_INTEGRATION_DEFECT.json")
EPOCH1_FREEZE = os.path.join(FREEZE_DIR, "ppe_pre_execution_freeze.json")

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


def sha(rel_or_abs: str) -> str:
    p = rel_or_abs if os.path.isabs(rel_or_abs) else os.path.join(ROOT, rel_or_abs)
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def cstar_impl_sha_py() -> str:
    sys.path.insert(0, _PY)
    from ppe.freeze import cstar_impl_sha_py as f  # noqa: E402
    return f()


def git_head() -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=os.path.dirname(_MODELE), stderr=subprocess.DEVNULL)
        return out.decode().strip()
    except Exception:  # noqa: BLE001
        return "UNKNOWN"


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

    # ---------- prerequisites ----------
    with open(EPOCH1_FREEZE, encoding="utf-8") as f:
        ep1 = json.load(f)
    ep1b = ep1["bindings"]

    incident_sha = ""
    if os.path.isfile(INCIDENT):
        incident_sha = sha(INCIDENT)
        with open(INCIDENT, encoding="utf-8") as f:
            inc = json.load(f)
        check("incident_self_hash",
              inc.get("sha256_self") == hashutil.sha256_canonical(
                  {k: v for k, v in inc.items() if k != "sha256_self"}))
        check("incident_no_contamination",
              inc.get("NO_FORMAL_SCIENTIFIC_OUTPUT_CONTAMINATION") is True)
        check("incident_runs_zero",
              inc.get("epoch2_result_bearing_runs_before_detection") == 0)
    else:
        check("incident_present", False, INCIDENT)

    check("defective_freeze_preserved", os.path.isfile(OLD_FREEZE))
    old_root = ""
    if os.path.isfile(OLD_FREEZE):
        with open(OLD_FREEZE, encoding="utf-8") as f:
            old = json.load(f)
        old_root = old.get("sha256_self", "")
        check("defective_freeze_root_recorded",
              old_root == ("7919123bd986d5b53e68a471943579316a8b03ae93eb67"
                           "2fe70ac0e43242dd47"), old_root[:16])

    # epoch-1 evidence intact (192 terminal + 48 parity)
    ep1dir = os.path.join(RPP, "formal")
    n_term = n_par = 0
    if os.path.isdir(ep1dir):
        for vid in sorted(os.listdir(ep1dir)):
            vd = os.path.join(ep1dir, vid)
            if not os.path.isdir(vd) or vid.startswith(("_", "quarantine")):
                continue
            for fn in sorted(os.listdir(vd)):
                if fn.endswith(".json") and fn[:-5] in (
                        "P0", "C0", "C1", "C2"):
                    try:
                        with open(os.path.join(vd, fn), encoding="utf-8") as f:
                            d = json.load(f)
                        if str(d.get("terminal_status", "")).startswith(
                                "COMPLETE"):
                            n_term += 1
                    except Exception:  # noqa: BLE001
                        pass
                if fn == "warmup_parity.json":
                    try:
                        with open(os.path.join(vd, fn), encoding="utf-8") as f:
                            if json.load(f) == "PASS":
                                n_par += 1
                    except Exception:  # noqa: BLE001
                        pass
    check("epoch1_terminals_192", n_term == 192, str(n_term))
    check("epoch1_warmup_parity_48", n_par == 48, str(n_par))

    # epoch-2 namespace must not exist yet
    check("epoch2_namespace_absent",
          not os.path.isdir(os.path.join(RPP, "formal_epoch2")))

    # zero epoch-2 FORMAL ledger entries
    n_e2_formal = 0
    ledp = os.path.join(RPP, "access_ledger.jsonl")
    if os.path.isfile(ledp):
        with open(ledp, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    e = json.loads(line)
                except Exception:  # noqa: BLE001
                    continue
                if e.get("purpose") == "FORMAL" and \
                        e.get("formal_epoch") == 2:
                    n_e2_formal += 1
    check("epoch2_formal_ledger_entries_zero", n_e2_formal == 0, str(n_e2_formal))

    # ---------- qualification artifacts (must pass and bind current bytes) --
    quals = {}
    for nm, fn in (("amendment_v2", "amendment_v2_qual.json"),
                   ("e2e_runner_integration", "e2e_runner_integration_qual.json"),
                   ("fec_robustness_prereg", "fec_robustness_prereg.json")):
        quals[nm] = qual_pass(fn)
        check(f"qual_{nm}_pass", quals[nm]["pass"])
    coh = qual_pass("feasibility_coherence_gate.json")
    quals["coherence_gate"] = coh
    check("qual_coherence_pass", coh["pass"])
    with open(os.path.join(RPP, "qualification",
                           "feasibility_coherence_gate.json"),
              encoding="utf-8") as f:
        cohd = json.load(f)
    dr = cohd.get("derived_reserves", {})
    check("coherence_reserves_canonical",
          abs(dr.get("P_reserve_dis_kW", -1) - 543.8) < 1e-9
          and abs(dr.get("E_reserve_dis_kWh", -1) - 6.839) < 1e-9
          and abs(dr.get("P_reserve_chg_kW", -1) - 819.8) < 1e-9
          and abs(dr.get("E_reserve_chg_kWh", -1) - 5.625) < 1e-9)

    # e2e artifact must bind the CURRENT implementation bytes
    e2e = quals["e2e_runner_integration"]["doc"]
    isha = e2e.get("implementation_sha256", {})
    drift = {}
    for k, rel in (
            ("run_formal_voyage_m", "scripts/+ppe/run_formal_voyage.m"),
            ("run_formal_voyage_pack_m", "scripts/+ppe/run_formal_voyage_pack.m"),
            ("run_formal_manifest_m", "scripts/+ppe/run_formal_manifest.m"),
            ("formal_terminal_v2_m", "scripts/+ppe/formal_terminal_v2.m"),
            ("formal_pack_skip_v2_m", "scripts/+ppe/formal_pack_skip_v2.m"),
            ("write_traj_v1_m", "scripts/+ppe/write_traj_v1.m"),
            ("write_audit_v1_m", "scripts/+ppe/write_audit_v1.m"),
            ("load_epoch2_freeze_bindings_m",
             "scripts/+ppe/load_epoch2_freeze_bindings.m"),
            ("formal_outdir_m", "scripts/+ppe/formal_outdir.m"),
            ("acceptance_v2_py", "python/ppe/acceptance_v2.py")):
        if isha.get(k) != sha(rel):
            drift[k] = rel
    check("e2e_binds_current_implementation", not drift, json.dumps(drift))

    # ---------- assemble bindings ----------
    amend_p = os.path.join(FREEZE_DIR, "post_freeze_runner_amendment.json")
    with open(amend_p, encoding="utf-8") as f:
        amend = json.load(f)
    check("epoch1_amendment_run_cstar_binding",
          sha("scripts/+ppe/run_cstar.m")
          == amend["run_cstar_file_sha256"]["amended"])
    check("cstar_composite_unchanged",
          cstar_impl_sha_py() == CSTAR_COMPOSITE_FROZEN)

    slx_sha = sha(paths.MODELE_SLX)
    check("modele_slx_t445_baseline", slx_sha == SLX_T445_BASELINE)

    with open(os.path.join(ROOT, "config", "phase4f_ppe_teval_table.json"),
              encoding="utf-8") as f:
        teval = json.load(f)

    bindings = {
        "code_commit_sha": git_head(),
        "modele_slx_sha256": slx_sha,
        "cstar_composite_sha256": cstar_impl_sha_py(),
        "run_cstar_amended_sha256": amend["run_cstar_file_sha256"]["amended"],
        "c2_bridge_amended_sha256":
            amend["amended_file_overrides"]["scripts/+ppe/c2_bridge.m"],
        "winfun_plan_amended_sha256":
            amend["amended_file_overrides"]["scripts/+ppe/winfun_plan.m"],
        "checker_v2_sha256": sha("scripts/+ppe/feasibility_contract_v2.m"),
        "traj_writer_sha256": sha("scripts/+ppe/write_traj_v1.m"),
        "audit_writer_sha256": sha("scripts/+ppe/write_audit_v1.m"),
        "runner_manifest_sha256": sha("scripts/+ppe/run_formal_manifest.m"),
        "runner_pack_sha256": sha("scripts/+ppe/run_formal_voyage_pack.m"),
        "runner_voyage_sha256": sha("scripts/+ppe/run_formal_voyage.m"),
        "terminal_pipeline_sha256": sha("scripts/+ppe/formal_terminal_v2.m"),
        "pack_skip_predicate_sha256": sha("scripts/+ppe/formal_pack_skip_v2.m"),
        "freeze_loader_sha256": sha("scripts/+ppe/load_epoch2_freeze_bindings.m"),
        "output_router_sha256": sha("scripts/+ppe/formal_outdir.m"),
        "checkpoint_mgr_sha256": sha("scripts/+ppe/checkpoint_mgr.m"),
        "acceptance_v2_sha256": sha("python/ppe/acceptance_v2.py"),
        "freeze_generator_sha256": sha("python/ppe/freeze_epoch2.py"),
        "verify_bindings_sha256": sha("python/ppe/verify_formal_bindings.py"),
        "teval_table_file_sha256": sha(os.path.join(
            ROOT, "config", "phase4f_ppe_teval_table.json")),
        "teval_table_sha256": teval["teval_table_sha256"],
        "hard_feasibility_config_file_sha256": sha(paths.HF_CONFIG_JSON),
        "t445_config_file_sha256": sha(os.path.join(
            ROOT, "config", "phase4f_t445_config.json")),
        "study_config_file_sha256": sha(os.path.join(
            ROOT, "config", "phase4f_ppe_study_config.json")),
        "formal_execution_manifest_file_sha256": sha(os.path.join(
            FREEZE_DIR, "formal_execution_manifest.json")),
        "formal_assets_file_sha256": sha(os.path.join(
            FREEZE_DIR, "formal_assets.json")),
        "c1_coverage_file_sha256": sha(os.path.join(
            FREEZE_DIR, "c1_coverage.json")),
        "c1_target_index_file_sha256": sha(os.path.join(
            FREEZE_DIR, "c1_target_index.json")),
        "corpus_identity_sha256": ep1b["corpus_identity_sha256"],
        "reserve_derivation_TA0_sha256": sha(os.path.join(
            os.path.dirname(os.path.dirname(_MODELE)),
            ".specs", "phase4-f-practical-predictive-ems",
            "reserve_derivation_TA0.json")),
        "amendment_design_doc_sha256": sha(os.path.join(
            os.path.dirname(os.path.dirname(_MODELE)),
            ".specs", "phase4-f-practical-predictive-ems",
            "feasibility_contract_design.md")),
        "amendment_doc_sha256": sha(os.path.join(
            os.path.dirname(os.path.dirname(_MODELE)),
            ".specs", "phase4-f-practical-predictive-ems",
            "feasibility_contract_amendment.md")),
        "amendment_v2_qual_artifact_sha256": quals["amendment_v2"]["sha256"],
        "e2e_qual_artifact_sha256": quals["e2e_runner_integration"]["sha256"],
        "fec_prereg_artifact_sha256": quals["fec_robustness_prereg"]["sha256"],
        "coherence_gate_artifact_sha256": quals["coherence_gate"]["sha256"],
        "access_ledger_file_sha256": sha(ledp),
        "incident_artifact_sha256": incident_sha,
        "predictor": ep1b["predictor"],
    }

    doc = {
        "schema": "ppe_epoch2_pre_execution_freeze_v3",
        "study": "phase4-f-practical-predictive-ems",
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
        "supersedes_freeze_root_sha256": old_root,
        "supersedes_reason": "PREEXECUTION_RUNNER_INTEGRATION_DEFECT",
        "incident_artifact": os.path.relpath(INCIDENT, ROOT),
        "epoch1_preserved": ep1.get("epoch1_preserved", {
            "deleted": False, "formal_epoch": 1, "reclassified": False,
            "rerun": False}),
        "epoch2_scientific_identities": {
            "P0_role": "UNCHANGED",
            "bootstrap_seed": "phase4f-t445-bootstrap-v1",
            "corpus_48_voyages": "UNCHANGED",
            "corpus_identity_sha256": ep1b["corpus_identity_sha256"],
            "cstar_composite_sha256": cstar_impl_sha_py(),
            "modele_slx_sha256": slx_sha,
        },
        "service_shortfall": {
            "authoritative": "P_unserved_kW (V_dc undervoltage derate path only)",
            "dual_diagnostic": ("P_service_gap = max(P_prop_demand - "
                                "P_prop_actual, 0); divergence -> FAIL CLOSED"),
            "retired": "P_bus_residual_kW -> DC-bus consistency diagnostic",
        },
        "genset_root_cause": (
            "checker v2 uses per-unit truth P_DG1_kW/P_DG2_kW with "
            "state-aware limits; no equal-split fallback; PMS allocator "
            "equal-split by design (D-B6 frozen)"),
        "s_terminal_admissible": {
            "min": 0.25, "max": 0.9,
            "note": ("all SOC in [0.25,0.90] (1 Hz grid) survive EV-1/EV-3 "
                     "within frozen SOP/hard bounds"),
        },
        "e_design_events": old.get("e_design_events", {}) if os.path.isfile(
            OLD_FREEZE) else {},
        "fec": {"k_eq_E": 0.5457,
                "robustness_band": [0.4366, 0.6548],
                "break_even": 0.0933},
        "corrections": (
            "EPOCH2_PREEXECUTION_INTEGRATION_DEFECT recovery: completed "
            "T-A5 integration (formal_epoch=2 explicit parameter, epoch-2 "
            "output namespace, identity-safe skip semantics, checker v2 as "
            "classification authority, transactional "
            "trajectory/audit/terminal write order, high-rate audit writer); "
            "trajectory channel list aligned to approved design FC-5 §5.1 "
            "(derived safety_hard_count in, P_prop_limit_kW out); all "
            "affected qualifications re-run against post-fix bytes"),
        "qualification": {
            "amend_v2_pass": quals["amendment_v2"]["pass"],
            "e2e_runner_integration_pass":
                quals["e2e_runner_integration"]["pass"],
            "coherence_pass": quals["coherence_gate"]["pass"],
            "fec_robust_pass": quals["fec_robustness_prereg"]["pass"],
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
        "task": "T-A7(v2)",
        "freeze_file": os.path.relpath(NEW_FREEZE, ROOT),
        "freeze_file_sha256": freeze_file_sha,
        "sha256_self_of_freeze_doc": doc["sha256_self"],
        "checks": checks,
        "n_checks": len(checks),
        "n_pass": sum(1 for c in checks if c["pass"]),
        "overall": overall,
        "stop_rule": ("overall=true -> ALSO STOP: human Epoch-2 "
                      "Pre-Execution Gate re-review decides T-A8; "
                      "overall=false -> STOP and report"),
        "t_a8": "REMAINS BLOCKED",
        "utc": doc["utc"],
    }
    with open(GATE, "w", encoding="utf-8") as f:
        json.dump(gate_doc, f, indent=1, sort_keys=True)

    marker = {
        "schema": "ppe_epoch2_freeze_supersession_v1",
        "superseded_freeze_root": old_root,
        "superseded_artifact": os.path.relpath(OLD_FREEZE, ROOT),
        "disposition": "SUPERSEDED_BEFORE_FORMAL_EXECUTION",
        "reason": "PREEXECUTION_RUNNER_INTEGRATION_DEFECT",
        "replacement_freeze_root": freeze_file_sha,
        "replacement_artifact": os.path.relpath(NEW_FREEZE, ROOT),
        "note": ("old freeze bytes preserved untouched as historical "
                 "pre-execution evidence; epoch-2 result-bearing runs "
                 "remained 0 before supersession"),
        "utc": doc["utc"],
    }
    with open(MARKER, "w", encoding="utf-8") as f:
        json.dump(marker, f, indent=1, sort_keys=True)

    print(f"[freeze-e2-v2] overall={overall} checks={len(checks)} "
          f"root={freeze_file_sha[:16]} self={doc['sha256_self'][:16]}")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())

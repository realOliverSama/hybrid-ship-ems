"""T-445 validity-only closure. No corpus, no bootstrap, no headroom stats."""
from __future__ import annotations

import json
import os
import subprocess
from datetime import datetime, timezone

from t445.gate_verdict import decide
from t445.hashutil import dump_json, sha256_file
from t445.paths import ACCESS_LEDGER, MODELE_ROOT, REPO_ROOT, RESULTS_DIR, rel


UTC = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
CONFIG_SHA = "52377c626f7e9ccb527846250f1d360ad63238c44ec33ac1184e6f399da07c33"
CORPUS_SHA = "245fa307cf36143259672f93cf801587eb701d721028a735b3cd91c97c42f115"
SLX_SHA = "190de549e38303823a1cae44d253fc1e4c32fdfb3d4b8f625a21507632d442b9"


def _git_head() -> str:
    p = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return p.stdout.strip()


def _walk_json_files(root: str) -> list[str]:
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in {".git", "__pycache__"}]
        for fn in filenames:
            if fn.endswith((".json", ".jsonl", ".md")):
                out.append(os.path.join(dirpath, fn))
    return sorted(out)


def main() -> None:
    os.makedirs(RESULTS_DIR, exist_ok=True)
    qdir = os.path.join(RESULTS_DIR, "qualification")
    skip_dir = os.path.join(RESULTS_DIR, "skipped")
    os.makedirs(skip_dir, exist_ok=True)
    head = _git_head()

    extras = {
        "task": "T-445",
        "closure_mode": "validity_only",
        "effect_statistics_computed": False,
        "formal_corpus_observed": False,
        "reserve_used": False,
        "failure_stage": "PRE_FORMAL_B2_OPTIMIZER_QUALIFICATION",
        "failure_code": "B2_OPTIMIZER_QUALIFICATION_FAILED",
        "b3_context_reason": "NOT_EXECUTED_DUE_MAIN_GATE_VALIDITY_FAILURE",
        "DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE": "NOT_ESTABLISHED",
        "scientific_question_status": "UNRESOLVED",
        "scientific_question": (
            "does perfect 300 s trajectory information provide >=2% exploitable "
            "control value relative to scalar E300?"
        ),
        "interpretation": (
            "Under the frozen Model-E / ECMS architecture, the project did not "
            "establish a sufficiently qualified trajectory-oracle reference. "
            "Therefore T-445 cannot support a scientific GO/NO-GO inference "
            "regarding the control value of 300 s load-trajectory information."
        ),
        "methodological_validity_failure": True,
        "not_evidence_trajectory_prediction_has_no_value": True,
        "predictor_v3_phase4e_unaffected": True,
        "t446_blocked": True,
        "continuation_requires_roadmap_amendment": True,
        "config_sha256": CONFIG_SHA,
        "corpus_identity_sha256": CORPUS_SHA,
        "k_eq_E": 0.5457,
        "w_term": 1.17e5,
        "slx_sha256": SLX_SHA,
        "code_commit_at_write": head,
        "utc": UTC,
        "skipped_tasks": {
            "T-011": "SKIPPED_DUE_GATE_VALIDITY_FAILURE",
            "T-012": "SKIPPED_DUE_GATE_VALIDITY_FAILURE",
            "T-013": "SKIPPED_DUE_GATE_VALIDITY_FAILURE",
            "T-014_ordinary_effect_statistics": "SKIPPED_DUE_GATE_VALIDITY_FAILURE",
            "T-015": "SKIPPED_DUE_GATE_VALIDITY_FAILURE",
        },
        "qualification_matrix": {
            "Q1": {"pass": True, "note": "policy-table parity 597960, OOB=0"},
            "Q2": {"pass": False, "rel": 0.015342091690099056, "threshold": 0.005},
            "Q3": {"pass": True, "rel": 0.0},
            "Q4": {
                "pass": False,
                "fuel_rel": 0.013143757194834934,
                "threshold": 0.01,
                "dsoc_err": 0.0007791812999826675,
                "dsoc_pass": True,
            },
            "Q5": {"pass": True},
            "Q6": {
                "pass": False,
                "fuel_rel": 0.01001229771173236,
                "threshold": 0.01,
                "dsoc_err": 0.0010018624561910494,
                "dsoc_pass": True,
            },
            "SimState": {"pass": True, "nseg": 552},
        },
        "qualification_error_evolution": [
            {
                "stage": "initial_T010",
                "artifact": "qualification/t010_b2_optimizer_qual.json",
                "Q4_fuel_rel": "~0.092",
                "Q6_fuel_rel": "~0.099",
                "note": "aggregate ECMS BSFC vs dual-genset FuelModelE",
            },
            {
                "stage": "Amendment_D",
                "artifact": "qualification/t010_b2_optimizer_qual_amd_d.json",
                "Q4_fuel_rel": 0.038372119408367343,
                "Q6_fuel_rel": 0.073789534767009141,
                "note": "PMS-aware FuelModelE surrogate; still FAIL",
            },
            {
                "stage": "E0_diagnostic",
                "artifact": "qualification/t010_e0_fuel_audit.json",
                "matched_Q4": 0.038372119408367343,
                "matched_Q6": 0.073789534767009141,
                "jensen_meanDG": "~0.004",
                "note": "matched-input residual is surrogate vs plant, not 10s load representation",
            },
            {
                "stage": "Amendment_E_final",
                "artifact": "qualification/t010_b2_optimizer_qual_amd_e.json",
                "Q4_fuel_rel": 0.013143757194834934,
                "Q6_fuel_rel": 0.01001229771173236,
                "note": "generator-realization surrogate; final authorized fidelity amendment; still FAIL",
            },
        ],
        "preserved_fail_artifacts": [
            "qualification/t010_b2_optimizer_qual.json",
            "qualification/t010_b2_optimizer_qual_amd_d.json",
            "qualification/t010_fuel_gap_d1.json",
            "qualification/t010_e0_fuel_audit.json",
            "qualification/t010_b2_optimizer_qual_amd_e.json",
            "qualification/t007_keq_charge_restoration.json",
            "lambda_envelope_r4.json",
        ],
        "st_edge_ai_upload_untouched": True,
    }

    gate = decide(
        median_g=0.0,
        ci_lower=0.0,
        ci_upper=0.0,
        mode="initial",
        validity={"b2_optimizer_qualification_fail": True, "gate_validity_fail": True},
        b3_context_status="UNQUALIFIED_CONTEXT",
        extras=extras,
    )
    # Schema keeps numeric slots; they are not computed effect statistics.
    gate["median_g_is_placeholder"] = True
    gate["ci_is_placeholder"] = True
    gate["headroom_statistics"] = "NOT_COMPUTED"
    gate["trajectory_gate_verdict"] = "GATE_VALIDITY_FAIL"
    gate["expansion_used"] = False
    gate["expansion_eligible"] = False
    gate["decision_required"] = True

    gate_path = os.path.join(RESULTS_DIR, "gate_result.json")
    dump_json(gate_path, gate)

    skip_payload = {
        "schema": "t445_task_skip_v1",
        "reason": "SKIPPED_DUE_GATE_VALIDITY_FAILURE",
        "failure_code": "B2_OPTIMIZER_QUALIFICATION_FAILED",
        "formal_corpus_observed": False,
        "reserve_used": False,
        "utc": UTC,
        "config_sha256": CONFIG_SHA,
    }
    for tid, name in [
        ("T-011", "t011_b3_skipped.json"),
        ("T-012", "t012_pre_execution_freeze_skipped.json"),
        ("T-013", "t013_formal_corpus_skipped.json"),
        ("T-014", "t014_ordinary_effect_statistics_skipped.json"),
        ("T-015", "t015_reserve_expansion_skipped.json"),
    ]:
        rec = dict(skip_payload)
        rec["task"] = tid
        dump_json(os.path.join(skip_dir, name), rec)

    # Hash chain of existing + new artifacts (except evidence_index/pointer/report first)
    files = _walk_json_files(RESULTS_DIR)
    chain = []
    for fp in files:
        bn = os.path.basename(fp)
        if bn in {
            "evidence_index.json",
            "final_gate_pointer.json",
            "final_report.md",
        }:
            continue
        chain.append(
            {
                "path": rel(fp),
                "sha256": sha256_file(fp),
                "size_bytes": os.path.getsize(fp),
            }
        )

    index = {
        "schema": "t445_evidence_index_v1",
        "task": "T-445",
        "closure_mode": "validity_only",
        "trajectory_gate_verdict": "GATE_VALIDITY_FAIL",
        "config_sha256": CONFIG_SHA,
        "corpus_identity_sha256": CORPUS_SHA,
        "code_commit_at_write": head,
        "utc": UTC,
        "n_artifacts": len(chain),
        "artifacts": chain,
    }
    index_path = os.path.join(RESULTS_DIR, "evidence_index.json")
    dump_json(index_path, index)
    index_sha = sha256_file(index_path)
    gate_sha = sha256_file(gate_path)

    pointer = {
        "schema": "t445_final_gate_pointer_v1",
        "canonical_source": "validity-fail",
        "source_artifact": rel(gate_path),
        "source_artifact_sha256": gate_sha,
        "evidence_index": rel(index_path),
        "evidence_index_sha256": index_sha,
        "trajectory_gate_verdict": "GATE_VALIDITY_FAIL",
        "b3_context_status": "UNQUALIFIED_CONTEXT",
        "b3_context_reason": "NOT_EXECUTED_DUE_MAIN_GATE_VALIDITY_FAILURE",
        "expansion_used": False,
        "formal_corpus_observed": False,
        "reserve_used": False,
        "decision_required": True,
        "config_sha256": CONFIG_SHA,
        "corpus_identity_sha256": CORPUS_SHA,
        "code_commit_at_write": head,
        "failure_code": "B2_OPTIMIZER_QUALIFICATION_FAILED",
        "utc": UTC,
    }
    pointer_path = os.path.join(RESULTS_DIR, "final_gate_pointer.json")
    dump_json(pointer_path, pointer)

    report = _report(head, gate_sha, index_sha)
    report_path = os.path.join(RESULTS_DIR, "final_report.md")
    with open(report_path, "w", encoding="utf-8") as fh:
        fh.write(report)

    with open(ACCESS_LEDGER, "a", encoding="utf-8") as fh:
        fh.write(
            json.dumps(
                {
                    "purpose": "QUALIFICATION",
                    "task": "T-445-CLOSE",
                    "artifact": "gate_result.json",
                    "utc": UTC,
                },
                ensure_ascii=False,
            )
            + "\n"
        )

    print("T-445 validity closure written")
    print("gate", rel(gate_path), gate_sha)
    print("pointer", rel(pointer_path), sha256_file(pointer_path))
    print("index", rel(index_path), index_sha)
    print("report", rel(report_path), sha256_file(report_path))


def _report(head: str, gate_sha: str, index_sha: str) -> str:
    return f"""# T-445 Trajectory Oracle Gate — Final Report

Status: **CLOSED**  
Verdict: **`GATE_VALIDITY_FAIL`**  
Not: `NO_GO_TRAJECTORY_VALUE`  
UTC: {UTC}  
Canonical pointer: `results/t445_oracle_gate/final_gate_pointer.json`

## Scientific interpretation

Under the frozen Model-E / ECMS architecture, the project did not establish a
sufficiently qualified trajectory-oracle reference. Therefore T-445 cannot
support a scientific GO/NO-GO inference regarding the control value of 300 s
load-trajectory information.

This is a **methodological validity failure**.

It is **not** evidence that trajectory prediction has no value.

- Predictor-v3 / Phase 4-E remains valid and unaffected.
- T-446 as previously defined is **blocked**.
- Continuation requires a **roadmap amendment**.
- The scientific question “does perfect 300 s trajectory information provide
  ≥2% exploitable control value relative to scalar E300?” remains **UNRESOLVED**.

`DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE = NOT_ESTABLISHED`

## Canonical identities

| Field | Value |
|---|---|
| `trajectory_gate_verdict` | `GATE_VALIDITY_FAIL` |
| `b3_context_status` | `UNQUALIFIED_CONTEXT` |
| `b3_context_reason` | `NOT_EXECUTED_DUE_MAIN_GATE_VALIDITY_FAILURE` |
| `decision_required` | true |
| `expansion_used` | false |
| `formal_corpus_observed` | false |
| `reserve_used` | false |
| `failure_stage` | `PRE_FORMAL_B2_OPTIMIZER_QUALIFICATION` |
| `failure_code` | `B2_OPTIMIZER_QUALIFICATION_FAILED` |
| `config_sha256` | `{CONFIG_SHA}` |
| `corpus_identity_sha256` | `{CORPUS_SHA}` |
| `k_eq_E` | 0.5457 kg/kWh CANONICAL |
| `w_term` | 1.17e5 kg/SOC² CANONICAL |
| `.slx` sha256 | `{SLX_SHA}` (unchanged) |
| code HEAD at write | `{head}` |
| `gate_result.json` sha256 | `{gate_sha}` |
| `evidence_index.json` sha256 | `{index_sha}` |

No h_v / h_g / bootstrap / CI were generated. No formal 36-voyage corpus. No
reserve. No B3.

## Task DAG

| Task | Status |
|---|---|
| T-001..T-009 | COMPLETE |
| T-010 | COMPLETE attempt, **FAIL** (Q2/Q4/Q6) |
| T-011 | `SKIPPED_DUE_GATE_VALIDITY_FAILURE` |
| T-012 | `SKIPPED_DUE_GATE_VALIDITY_FAILURE` |
| T-013 | `SKIPPED_DUE_GATE_VALIDITY_FAILURE` |
| T-014 ordinary effect statistics | `SKIPPED_DUE_GATE_VALIDITY_FAILURE` |
| T-014 validity-only Gate JSON | **issued** (`gate_result.json`) |
| T-015 | `SKIPPED_DUE_GATE_VALIDITY_FAILURE` |
| T-016 | this report + evidence index |

## Final Q1–Q6 + SimState matrix (Amendment E identity)

| Item | Result | Detail |
|---|---|---|
| Q1 policy-table parity | PASS | 597960/597960, maxAbs=0, OOB=0, LAMBDA_DOMAIN_COVERAGE |
| Q2 SOC-grid convergence | FAIL | rel=0.0153 > 0.005 |
| Q3 Δλ-grid convergence | PASS | rel=0 |
| Q4 proxy vs Model-E | FAIL | fuel_rel=1.31% > 1.0%; SOC err=7.8e-4 ≤ 0.002 |
| Q5 first-action determinism | PASS | |
| Q6 B2_ZERO_CORRECTION_SANITY | FAIL | fuel_rel=1.001% > 1.0%; SOC err=1.00e-3 ≤ 0.002 |
| SimState continuous vs segmented | PASS | S20, nseg=552 |

Source: `qualification/t010_b2_optimizer_qual_amd_e.json` (do not relabel PASS).

## Qualification-error evolution

| Stage | Artifact | Q4 fuel_rel | Q6 fuel_rel | Note |
|---|---|---:|---:|---|
| Initial T-010 | `t010_b2_optimizer_qual.json` | ~9.2% | ~9.9% | aggregate ECMS BSFC vs dual-genset FuelModelE |
| Amendment D | `t010_b2_optimizer_qual_amd_d.json` | 3.84% | 7.38% | PMS-aware FuelModelE; still FAIL |
| E0 diagnostic | `t010_e0_fuel_audit.json` | 3.84% matched | 7.38% matched | 10 s mean-DG Jensen ~0.4%; residual is surrogate vs plant |
| Amendment E (final) | `t010_b2_optimizer_qual_amd_e.json` | 1.31% | 1.001% | generator-realization dynamics; still FAIL |

Historical FAIL artifacts were **not overwritten** and **not relabeled PASS**.

## Incident / amendment history (summary)

- Amendment a′: remove illegal 0.80 λ cap; native law=6 envelope.
- Amendment C: k_eq^E=0.5457 nominal-SOC FEC; w_term=1.17e5; OCV 0.9687 diagnostic only.
- Amendment D: PMS-aware fuel surrogate; Q6 vs law=6 zero-correction.
- E0: 10 s nonlinear-fuel diagnostic (non-result-bearing).
- Amendment E: generator ramp/inertia realization; **final** authorized surrogate-fidelity amendment.

Incidents ledger: `incidents.jsonl` (do not delete). Access ledger: `access_ledger.jsonl`.

## Downstream

- T-446 (previous definition) remains blocked.
- No scientific GO/NO-GO about trajectory information value is permitted from T-445.
- A new Phase 4-F practical predictive-EMS evaluation route requires a roadmap amendment.

`st_edge_ai_upload/` was not touched.
"""


if __name__ == "__main__":
    main()

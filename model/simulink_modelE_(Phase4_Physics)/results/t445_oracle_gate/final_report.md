# T-445 Trajectory Oracle Gate — Final Report

Status: **CLOSED**  
Verdict: **`GATE_VALIDITY_FAIL`**  
Not: `NO_GO_TRAJECTORY_VALUE`  
UTC: 2026-09-15T12:33:31Z  
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
| `config_sha256` | `52377c626f7e9ccb527846250f1d360ad63238c44ec33ac1184e6f399da07c33` |
| `corpus_identity_sha256` | `245fa307cf36143259672f93cf801587eb701d721028a735b3cd91c97c42f115` |
| `k_eq_E` | 0.5457 kg/kWh CANONICAL |
| `w_term` | 1.17e5 kg/SOC² CANONICAL |
| `.slx` sha256 | `190de549e38303823a1cae44d253fc1e4c32fdfb3d4b8f625a21507632d442b9` (unchanged) |
| code HEAD at write | `b5cdf80989c46172c2e47f8b5efde37e5d2c34a6` |
| `gate_result.json` sha256 | `f6a71fdd86dc0d05e89bdb8ba0ebcb2b59a2d9923ad3d6126cb6d64beef1b9b6` |
| `evidence_index.json` sha256 | `a719fe41e0b13f81be485a81fabdd21be89e84f58d4c31c9b7913b46c68a1aed` |

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

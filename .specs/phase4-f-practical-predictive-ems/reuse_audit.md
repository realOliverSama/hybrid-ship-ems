# T-445 / Phase-E reuse audit

Status: **brainstorm-approved** (Gate R1–R10 applied)  
Date: 2026-09-15  
Scope: static inspection only. No C0/C1/C2 formal corpus runs.

Classification:

| Tag | Meaning |
|---|---|
| REUSE_AS_IS | keep code and scientific meaning |
| REUSE_WITH_RENAMED_SEMANTICS | keep code; strip oracle-upper-bound claims |
| REUSE_AFTER_MODIFICATION | keep after explicit interface/accounting change |
| DIAGNOSTIC_ONLY | useful for debug, not a primary outcome |
| RETIRE | do not use for the new scientific claim |
| OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH | never modify, add, move, delete, or clean up |

## Matrix

| Component | Path | Class | Note |
|---|---|---|---|
| 10 s oracle-window assembler | `scripts/+t445/assemble_oracle_window.m` | REUSE_AS_IS | C1 truth; also C0 persistence assembly target is the same `ForecastWindow` shape |
| True-ZOH adapter | `make_simin_zoh.m` / `prep_ehat_zoh.m` | REUSE_AFTER_MODIFICATION | Keep Interpolate=off. Inject Δλ from C*, not scalar ê as the scientific input |
| Dataset-v3 corpus manifest | `results/t445_oracle_gate/corpus_manifest.json` | REUSE_AFTER_MODIFICATION | Promote previous initial 9 + reserve 3 to **one 12-group / 48-voyage formal corpus**; retire sequential expansion |
| Validation split/group identities | D-package `split_assignment` `ac4c4a76…` | REUSE_AS_IS | Do not re-split. IID test remains sealed |
| FEC machinery | `scripts/+t445/metrics_fec.m` | REUSE_AS_IS | Primary J_FEC; not a GO threshold |
| k_eq_E=0.5457 | `phase4f_t445_config` | REUSE_AS_IS | CANONICAL. Frozen in C* |
| w_term=1.17e5 | same config | REUSE_AS_IS | Frozen in C* (R3). Not an oracle-certification weight |
| SOC grid | config `b2_dp.soc_colon` | REUSE_AFTER_MODIFICATION | Formal C* grid **0.005** (was 0.01). 0.0025 diagnostic only |
| Hard-feasibility | `hard_feasibility.m` | REUSE_AFTER_MODIFICATION | Keep checks; add R9 pair-retention / infeasible-arm policy |
| Intervention / burden metrics | `audit_matrix.m` | REUSE_AS_IS | Secondary |
| SimState segmented runner | `run_arm_b2.m` / `simstate_mgr.m` | REUSE_AFTER_MODIFICATION | Receding 10 s plant execution. Not “oracle Gate runner” |
| Checkpoint/resume | `simstate_mgr` | REUSE_AS_IS | Qualification: reproducibility |
| law=6 Δλ channel | `law6_lambda_eff.m` / `oracle_src` | REUSE_WITH_RENAMED_SEMANTICS | Action remains Δλ |
| B2 DP / window optimizer | `b2_window_dp.m` | REUSE_WITH_RENAMED_SEMANTICS | C* internals. Failed 1% oracle replica. Practical plant evaluation authorized |
| Generator/PMS dynamic surrogate | `realize_bin.m` / `realize_phys.m` | REUSE_WITH_RENAMED_SEMANTICS | Internal C* cost. Outcomes = Model-E logs |
| Policy table | `build_policy_table.m` | REUSE_AS_IS | Q1 parity held |
| Bootstrap / pairing | `stats_pairing.py` / `bootstrap_cluster.py` | REUSE_AFTER_MODIFICATION | 12 groups; mean group effect primary; **retire** X=2%/Y=0% GO machine |
| `gate_verdict.py` GO/NO-GO | T-445 | RETIRE as scientific decision | Integrity validity-fail path may remain |
| T-445 2% GO threshold | `gate_thresholds` | RETIRE | No replacement GO threshold |
| Predictor-v3 winner | `results/predictor_v3/winner/` + REAT-T t018 seed0 | REUSE_AS_IS | C2. Verify in-repo checksums. Do not retrain |
| Predictor-v3 contracts | `input_contract.json`, `eval_config.json` | REUSE_AS_IS | Controller input = q50 |
| T-445 Q2/Q4/Q6 FAIL artifacts | `qualification/t010_*` | DIAGNOSTIC_ONLY | Methodological limitations. Do not select new fixtures by those outcomes |
| B3 full-voyage DP | never implemented | RETIRE | Not in P0/C0/C1/C2 |
| Oracle-upper-bound wording | T-445 reports/specs | RETIRE | Forbidden |
| `st_edge_ai_upload/` | user-owned untracked | **OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH** | Not a Phase-4F scientific asset. Never modify, add, move, delete, or clean up |

## Implication

T-445 failure: C* was not accurate enough to **certify an oracle upper bound**.

Authorized now: evaluate the same practical C* on the **real Model-E plant**
under C0/C1/C2 forecast sources, with outcomes from plant logs.

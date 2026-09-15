# T-445 / Phase-E reuse audit

Status: **draft**（Brainstorm Gate 前，非批准设计）  
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

## Matrix

| Component | Path | Class | Note |
|---|---|---|---|
| 10 s oracle-window assembler | `scripts/+t445/assemble_oracle_window.m` | REUSE_AS_IS | Dataset-v3 compatible 30×10 s Pbar / shrinking / E300 derived. C1 truth source. |
| True-ZOH adapter | `make_simin_zoh.m` / `prep_ehat_zoh.m` | REUSE_AFTER_MODIFICATION | Keep Interpolate=off. May need a 30-bin trajectory injection path, not only scalar ê. |
| Dataset-v3 corpus manifest | `results/t445_oracle_gate/corpus_manifest.json` | REUSE_AS_IS | identity `245fa307…`; T-445 never observed formal controller results. Candidate reuse for new route after Brainstorm freeze. |
| Validation split/group identities | D-package `split_assignment` `ac4c4a76…` | REUSE_AS_IS | Do not re-split. IID test remains sealed. |
| FEC machinery | `scripts/+t445/metrics_fec.m` | REUSE_AS_IS | J_FEC = fuel + ΔSOC·Cap·k_eq. Accounting identity, not a GO threshold. |
| k_eq_E=0.5457 | `phase4f_t445_config` | REUSE_AS_IS | CANONICAL nominal-SOC FEC. Do not re-open. |
| w_term=1.17e5 | same config | REUSE_WITH_RENAMED_SEMANTICS | Keep if C0/C1/C2 share the same DP terminal. Not an oracle-certification weight. |
| Hard-feasibility | `hard_feasibility.m` | REUSE_AS_IS | Mission/service feasibility remains a primary outcome. |
| Intervention / burden metrics | `audit_matrix.m` | REUSE_AS_IS | PMS/Safety starts, TV_Pgen, throughput. |
| SimState segmented runner | `run_arm_b2.m` / `simstate_mgr.m` | REUSE_AFTER_MODIFICATION | Engineering useful for receding 10 s plant execution. Rename: not “oracle Gate runner”. |
| Checkpoint/resume | `simstate_mgr` save/load | REUSE_AS_IS | Operational, not a scientific claim. |
| law=6 Δλ channel | `law6_lambda_eff.m` / `oracle_src` | REUSE_WITH_RENAMED_SEMANTICS | Action remains Δλ. Not a trajectory-information upper bound. |
| B2 DP / window optimizer | `b2_window_dp.m` | REUSE_WITH_RENAMED_SEMANTICS | Failed 1% **oracle-reference** qualification. Still a candidate **practical** receding-horizon controller to evaluate on Model E. |
| Generator/PMS dynamic surrogate | `realize_bin.m` / `realize_phys.m` | REUSE_WITH_RENAMED_SEMANTICS | Use inside the controller cost, not as a certified plant replica. Plant outcomes remain Model-E logs. |
| Policy table | `build_policy_table.m` | REUSE_AS_IS | Q1 parity vs `ecms_law` held. Aggregate P_g* map only. |
| Bootstrap / statistics | `python/t445/{stats_pairing,bootstrap_cluster,gate_verdict}.py` | REUSE_AFTER_MODIFICATION | Pairing/bootstrap reusable. **Retire** X=2%/Y=0% GO machine as default. Validity-fail path stays for integrity failures. |
| T-445 2% GO threshold | config `gate_thresholds` | RETIRE | Do not inherit as the new scientific decision rule. |
| Predictor-v3 winner | `results/predictor_v3/winner/` + REAT-T t018 | REUSE_AS_IS | Frozen Phase-E package. Do not retrain. C2 input. |
| Predictor-v3 contracts | `candidates/*/input_contract.json`, `eval_config.json` | REUSE_AS_IS | 30×3 [q10,q50,q90]; recommended controller input = q50. |
| T-445 qualification fixtures | `qualification/t010_*` | DIAGNOSTIC_ONLY | Historical FAIL/E0/E evidence. Do not select new fixtures by those outcomes. |
| B3 full-voyage DP | never implemented | RETIRE | Not required for the new four-way comparison. |
| Oracle-upper-bound wording | reports/specs T-445 | RETIRE | Explicitly forbidden in the new route. |
| `st_edge_ai_upload/` | user-owned untracked | RETIRE from this spec | Do not touch. |

## Implication

T-445 failure means: this controller was **not accurate enough to certify an oracle upper bound**.

It does **not** automatically mean: the same receding-horizon Δλ controller is unusable as a **practical** C0/C1/C2 architecture on the real Model-E plant, provided final outcomes are measured on Model E (not on the surrogate).

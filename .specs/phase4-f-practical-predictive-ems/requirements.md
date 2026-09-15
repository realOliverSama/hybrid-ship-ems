# Phase 4-F Practical Predictive EMS — Requirements

Spec: `phase4-f-practical-predictive-ems`  
Status: **requirements-approved**  
Created: 2026-09-15  
Approved: 2026-09-15（Requirements Gate **APPROVED WITH REQUIRED CORRECTIONS** — RG1–RG9 applied; no second Requirements Gate）  
Brainstorm: `./brainstorm.md`（**brainstorm-approved / CLOSED**, R1–R10; not reopened）  
Predecessor: T-445 CLOSED `GATE_VALIDITY_FAIL`

This file translates the approved Brainstorm (R1–R10) into FR/NFR/AC.
It does **not** reopen Brainstorm decisions.
It is **not** a Design and **not** an implementation license.

---

## Overview

Under **one fixed practical** predictive supervisory controller **C\***, quantify
practical Model-E control benefit of:

- no future information (**C0** persistence);
- exact legal 30×10 s future trajectory truth (**C1**);
- frozen Phase-4E REAT-T q50 (**C2**).

Primary attribution is C0/C1/C2. **P0** (frozen law=5) is **external context
only** because its controller architecture differs.

This is an **effect-estimation** study. There is **no** 2% GO/NO-GO and **no**
inherited T-445 1% proxy-vs-plant oracle-replica Gate.

Terminal scientific label: **`DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE`**.

C1 canonical name: `PERFECT_INFORMATION_CONTROLLER_REFERENCE`.

---

## Scope

### In Scope

- Freeze P0/C0/C1/C2 identities, `ForecastWindow`, C* algorithm, FEC, feasibility
  policy, 48-voyage formal corpus, estimands, bootstrap/CI, pre-execution freeze,
  sealing, and terminal scientific outputs;
- Practical-controller qualification (R7) on synthetic/nonformal fixtures;
- Later (after Design/Tasks Gates, **not this Gate**): formal 48-voyage runs.

### Out of Scope

- Claiming a trajectory-information theoretical upper bound;
- Formal C0/C1/C2/P0 execution before Design/Tasks Gates and pre-execution freeze;
- IID test scientific access; OOD for controller selection;
- Retraining or re-selecting Predictor-v3;
- Persistent `.slx` modification;
- Result-driven retune of C*;
- STM32N6 deployment as a scientific Gate;
- Any operation on `st_edge_ai_upload/`.

---

## Terminology

| Term | Definition |
|---|---|
| **C\*** | Practical Δλ receding-horizon supervisor frozen in FR-810 |
| **ForecastWindow** | Canonical 30-bin bus-side Pbar object (FR-820) |
| **J_FEC** | `fuel_raw + (SOC0 − SOC_end)·Bat_Cap·k_eq_E` (FR-840) |
| **HARD_INFEASIBLE** | Any hard-feasibility predicate true (FR-850) |
| **Formal corpus** | 12 split_groups / 48 voyages (FR-860) |

Forbidden strings in scientific outputs referring to C1:
`theoretical upper bound`, `global optimum`, `trajectory oracle upper bound`,
`maximum trajectory-information value`.

---

## User Stories

- As a researcher, I need C0/C1/C2 to differ **only** by forecast source so that
  B_perfect / B_predicted / L_prediction are attributable.
- As a researcher, I need C1 named as a perfect-information **controller
  reference**, not an information-theoretic ceiling.
- As a reviewer, I need the 48-voyage set and estimands frozen **before** any
  formal controller outcome is observed.
- As a later deployment engineer, I need this study to finish on Model E
  without N6 constraints rewriting the comparison.

---

## Functional Requirements

### A. Arm identities (R1)

- **FR-800**: The comparison SHALL comprise exactly four arms: P0, C0, C1, C2.
- **FR-801**: **P0** SHALL be the frozen Model-E **law=5** SOC-Adaptive ECMS
  (no prediction). P0 is **contextual**. P0 SHALL NOT enter B_perfect,
  B_predicted, or L_prediction.
- **FR-802**: **C0** SHALL be C* with the C0 persistence `ForecastWindow` (FR-821).
- **FR-803**: **C1** SHALL be C* with the exact legal 30×10 s future trajectory
  truth `ForecastWindow` (FR-822). C1 SHALL be referred to only as
  `PERFECT_INFORMATION_CONTROLLER_REFERENCE` (or “C1”).
- **FR-804**: **C2** SHALL be C* with the frozen REAT-T q50 `ForecastWindow`
  (FR-823–FR-825).
- **FR-805**: Primary scientific attribution SHALL be C0/C1/C2. Documents SHALL
  state that P0 uses a different controller architecture.

### B. C* algorithm freeze (R3)

- **FR-810**: C* SHALL be the current practical receding-horizon **Δλ**
  controller (T-445 B2 implementation lineage including Amendment E generator
  / PMS realization surrogate), evaluated on the **real Model-E plant**.
- **FR-811**: C* SHALL include, frozen, all of:
  - law=6 core;
  - Δλ ∈ [−0.10, +0.10] kg/kWh, step 0.005, n=41, no rate limit;
  - decision interval 10 s;
  - horizon 300 s / K=30 bins;
  - **SOC grid 0.10:0.005:0.95** (n=171);
  - k_eq_E = 0.5457 kg/kWh CANONICAL;
  - w_term = 1.17e5 kg/SOC²;
  - frozen `realize_bin` / PMS split / min_up / FuelModelE cost;
  - frozen terminal treatment (δ_term=0.01, same w_term);
  - frozen Safety / PMS plant interfaces;
  - λ_eff = max(λ_raw, 0.43), no artificial upper cap.
- **FR-812**: C* parameters in FR-811 SHALL NOT be retuned from C0/C1/C2
  validation outcomes.
- **FR-813**: SOC grid **0.0025** MAY be run only as a **predeclared**
  numerical-robustness diagnostic on nonformal fixtures or as a post-hoc
  sensitivity that **SHALL NOT** change the formal controller.
- **FR-814**: The T-445 Q2 continuous-approximation criterion (0.01 vs 0.005
  rel≤0.5%) is **retired as a scientific Gate**. The T-445 Q2 failure
  (rel=0.0153) SHALL remain documented as a limitation of the discrete
  algorithm now frozen at 0.005.
- **FR-815**: T-445 Q4/Q6 1% proxy-vs-plant failures SHALL remain documented
  methodological limitations. They SHALL NOT be the formal qualification Gate
  of this route (see FR-870).

### C. ForecastWindow and parity (R4)

- **FR-820**: After all source-specific assembly and de-normalization, C0, C1,
  and C2 SHALL enter C* through **one** canonical `ForecastWindow` object:
  `t0_s`, `Pbar_kW[1:K]` (K≤30, bus-side kW), `bin_duration_s[1:K]`,
  `K_remain`, `T_end_s`, `source_id` ∈
  {`C0_PERSISTENCE`,`C1_TRUTH`,`C2_REATT_Q50`}, plus provenance hashes.
  Trailing bins beyond shrinking horizon SHALL be omitted (not zero-filled as
  fake future load).
- **FR-821**: **C0 persistence.** At update t0:
  - if t0 ≥ 10 s: `P_persist = mean` of realized bus-side `P_load_total` on the
    most recently **completed** interval **[t0−10, t0)**;
  - else: `P_persist = P_load_total(t0)` (causal current measurement).
  All K future bins SHALL equal `P_persist`. C0 SHALL NOT use the eventual
  mean of an incompletely observed current bin. C0 SHALL contain **no**
  future information.
- **FR-822**: **C1 truth (arm-independent).** `Pbar_k` SHALL be the frozen
  Dataset-v3 legal target trajectory `y_k = E_k/10` [kW] for that
  `(voyage_id, t0)` computed from the **dataset-generation native stream** of
  `P_load_total_kW` (`dataset_v3/targets.py`; FR-476). This truth channel is
  **independent of C0/C1/C2 controller actions**. C1 SHALL NOT use the
  future closed-loop realized `P_load_total` of the arm under test.
  Live Model-E `P_load_total` MAY depend on speed-tracking / EMS shortfall;
  that path is circular and forbidden. C* SHALL NOT receive 1 Hz privileged
  future samples. Shrinking-horizon metadata MAY differ by remaining time;
  the underlying truth series for a given voyage SHALL be identical across
  arms.
- **FR-823**: **C2 forecast.** `Pbar_k` SHALL be the de-normalized REAT-T
  **q50** 30-bin output at issue time t0, truncated to shrinking horizon.
  q10/q90 SHALL NOT enter C* on this route.
- **FR-824**: C* control law SHALL NOT branch on `source_id`.
- **FR-825**: C0/C1/C2 SHALL use identical: controller code; objective; action
  space; SOC/grid semantics; update cadence; horizon; terminal cost; plant;
  PMS; Safety; initial conditions; FEC accounting; hard-feasibility
  definitions. **Only the forecast source** MAY differ.

### D. C2 predictor freeze and causality (R8)

- **FR-830**: C2 SHALL use frozen Phase-4E winner **REAT-T** with the
  **full** identities in FR-836. No ellipsis hashes.
- **FR-831**: This route SHALL NOT retrain, fine-tune, or select another
  predictor.
- **FR-832**: Before formal execution, a machine-readable **C2 causality
  audit** SHALL classify **every** C2 input feature at t0 as exactly one of:
  `observed_history` | `current_measurable_state` |
  `legitimately_known_future_exogenous_plan` | `prohibited_future_information`.
- **FR-833**: No feature classified `prohibited_future_information` or
  unavailable at t0 SHALL enter C2. Known-future route/speed/mission (K) and
  exogenous plan/forecast (F) tensors are permitted **only if** classified
  `legitimately_known_future_exogenous_plan` **and** that availability
  assumption is written in the audit artifact.
- **FR-834**: The audit SHALL freeze: normalization hashes (`formal_norm_v3`,
  `predictor_norm_ext_v1`); history length 300 s @ 1 Hz; inference cadence
  10 s; horizon/end policy (shrinking); model file checksum; deterministic
  inference (seed0 weights, no dropout at eval).
- **FR-835**: C2 SHALL NOT read `st_edge_ai_upload/`.
- **FR-836**: Canonical C2 identity (machine-verifiable; Design SHALL re-hash
  at freeze). `canonical_delivery_seed_index=0`.
  | Artifact | Path / field | SHA256 |
  |---|---|---|
  | winner identity | `results/predictor_v3/winner/WINNER_SELECTED_PRETERMINAL.json` `winner_identity_sha256` | `832720b0b0f39592fdeaa25957c1a61d0f0a1381d7220881e0b9af345eb37aa7` |
  | winner file | same file | `3a46c848092f35d6da66f8e1dc931653b580547bc3716366f11110679c5c0ebc` |
  | selected config | `results/predictor_v3/candidates/reatt/selected_config.json` | `fea67b7ff0ce10af619037eade22c678425986af86c405024ebb9a341f163ae4` |
  | architecture config hash | `config_sha256` in selected_config / winner | `1d662ff03ed7ca042b731707501d6b254fc35b64ae858431c66745025e544b5d` |
  | confirm s0 evidence | `results/predictor_v3/confirm/reatt/s0_evidence.json` | `d8e3c82ddcdc36ced41459fe4b90d685176f297c0f72558e0cf80fdc1f07ab1b` |
  | canonical keras | `experiments/predictor_v3/confirm/reatt/reatt_s0.keras` (local; recorded in s0_evidence `artifact_sha256`) | `d8da6af4667875c45de7de90383d2b6b750420f5af62c7dcdcb1e5997897f894` |
  | seed_int | s0_evidence | `1115655093` |
  | `formal_norm_v3.json` | `results/dataset_v3/formal/formal_norm_v3.json` | `b64210bc47a035afe9fd164c65ba63fca84aead53dc7e2dcaada79bd88f065ba` |
  | `predictor_norm_ext_v1.json` | `config/predictor_v3/predictor_norm_ext_v1.json` | `a1cace1fb6f6eda58ad3be76723663c6a196b98bbc4c2c34c7b537dc4c2fe92c` |
  | `eval_config.json` | `config/predictor_v3/eval_config.json` | `2bf077ba8045486a9d242b99ab842fe96009f0a29570b6dfbf1c1d370eac928a` |
  | `reat_t_arch.json` | `config/predictor_v3/reat_t_arch.json` | `ad5f1bdfbedfee74a739ebe30177d4efddae0df017c6aaa6c08faa92593a7ff0` |
  Pre-execution freeze SHALL fail if the keras file is missing or its SHA256
  differs from the recorded value. Trial id `reatt-t018` is the HPO selection
  identity; canonical **delivery weights** are seed0 confirm `reatt_s0.keras`.

### E. FEC (R5)

- **FR-840**: Primary outcome per arm-voyage SHALL be
  `J_FEC = fuel_raw_kg + (SOC0 − SOC_end) * 1071.36 * 0.5457`
  (Bat_Cap_kWh=1071.36, k_eq_E=0.5457 kg/kWh, nominal-SOC basis).
- **FR-840a**: `fuel_raw_kg` SHALL be the canonical Model-E log
  `fuel_total_kg` at voyage end from `10_Fuel_Consumption`: the time integral
  of per-DG FuelModelE running fuel (PCHIP BSFC on ON units) **plus** startup
  fuel. Canonical start cost is **`DG_C_start_kg = 0.6 kg/start`**, amortized
  over STARTING+SYNC (`ModelE_Physics_Parameters.m` §5). OFF/COOLDOWN fuel=0.
  All arms P0/C0/C1/C2 SHALL use this **same** Model-E reconstruction path.
  Surrogate/`realize_bin` fuel SHALL NOT replace `fuel_raw_kg` on any arm.
- **FR-841**: Raw fuel and terminal SOC SHALL be reported as secondary /
  diagnostic. They SHALL NOT replace J_FEC as the primary estimand.
- **FR-842**: For every voyage where the pairwise effect is **defined**
  (FR-854):
  ```
  B_perfect    = 100 * (J_C0 − J_C1) / J_C0
  B_predicted  = 100 * (J_C0 − J_C2) / J_C0
  L_prediction = 100 * (J_C2 − J_C1) / J_C0
  ```
  Let `residual = B_perfect − B_predicted − L_prediction` (percentage points).
  Require the **zero-safe** identity
  `abs(residual) <= 1e-10 + 1e-9 * max(abs(B_perfect), abs(B_predicted), abs(L_prediction))`
  when all three J are finite (including B_perfect == 0).
- **FR-843**: Absolute differences `ΔJ_perfect = J_C0−J_C1`,
  `ΔJ_predicted = J_C0−J_C2`, `ΔJ_gap = J_C2−J_C1` [kg] SHALL be reported
  alongside percentages.
- **FR-844**: This route SHALL NOT introduce a 2% (or any) GO/NO-GO threshold
  on B_* or ΔJ_*. Integrity failures use validity-fail (FR-875), not NO-GO.

### F. Hard feasibility (R9)

- **FR-850**: HARD predicates SHALL be those in the **study-local immutable
  hard-feasibility config** (FR-857), including at least: mission/service
  failure (task complete); load/propulsion shortfall; SOC hard bound
  violation; battery SOP / converter / V / I violation; generator
  Pmin/Pmax/ramp hard violation; Safety S1/S2 abnormal counts beyond frozen
  zeros; and **TERMINAL_SOC_ADMISSIBILITY**.
- **FR-850a**: **TERMINAL_SOC_ADMISSIBILITY**: `abs(SOC_end − SOC0) <= 0.005`.
  This is separate from ordinary SOC hard bounds `[0.10, 0.95]`. Purpose: FEC
  economically corrects small residual ΔSOC; this predicate prevents apparent
  fuel benefit via material battery depletion. Violation is HARD_INFEASIBLE
  for that arm-voyage. Threshold SHALL NOT be altered from formal outcomes.
- **FR-851**: Voyages SHALL NOT be pair-deleted because one arm is poor or
  infeasible.
- **FR-852**: Arm-specific HARD_INFEASIBLE is a **scientific outcome**. J_FEC
  MAY be logged as diagnostic but SHALL be flagged `INFEASIBLE`.
- **FR-853**: The project SHALL NOT fabricate J_FEC savings for an infeasible
  arm (no imputation from another arm, no clipping to C0, no invented fuel).
- **FR-854**: Pairwise B_* / ΔJ_* for a voyage are **defined** iff C0 and the
  other arm(s) required by that estimand are all **not** HARD_INFEASIBLE and
  all J_FEC are finite and J_C0>0:
  - B_perfect / ΔJ_perfect: C0 and C1 feasible;
  - B_predicted / ΔJ_predicted: C0 and C2 feasible;
  - L_prediction / ΔJ_gap: C1 and C2 feasible **and** C0 feasible
    (so the identity in FR-842 is on the same triplet).
  Otherwise the estimand is `WITHHELD_INFEASIBLE` for that voyage.
- **FR-855**: Every arm-voyage SHALL emit a machine-readable feasibility
  record: pass/fail per predicate, overall HARD_INFEASIBLE, offending
  identity. Formal outputs SHALL include counts and the list of offending
  voyage_ids per arm.
- **FR-856**: P0 feasibility is reported contextually and does not gate C*
  estimands.
- **FR-857**: Design SHALL create a study-local immutable hard-feasibility
  JSON that is an **exact field-for-field copy** of
  `config/phase4f_t445_config.json` object `hard_feasibility`. Record:
  canonical T-445 source path; canonical source SHA256 of that object;
  copied path; copied SHA256; machine equality check. No threshold may be
  retuned from C0/C1/C2 outcomes. `TERMINAL_SOC_ADMISSIBILITY` maps to
  copied field `terminal_soc.abs_dsoc_max = 0.005`.

### G. Formal corpus and statistics (R6)

- **FR-860**: The formal validation corpus SHALL be frozen **prospectively** as
  the T-445 `corpus_manifest.json` **initial 9 + reserve 3** split_groups
  (**12 groups, 48 voyages**). Identity SHA256
  `245fa307cf36143259672f93cf801587eb701d721028a735b3cd91c97c42f115`
  plus an explicit machine list of the 12 `split_group_id`s.
- **FR-861**: Sequential reserve expansion is **retired**. All 48 voyages are
  one fixed evaluation corpus. No result-dependent expansion.
- **FR-862**: Design SHALL reuse paired C0/C1/C2 (and contextual P0) on the
  same voyages; LOW/MID/HIGH stratification already on those groups;
  resampling unit = split_group; B=10000; seed `phase4f-t445-bootstrap-v1`.
- **FR-863**: Primary point estimator: **mean** of split_group-level paired
  effects (group mean of **defined** voyage-level B_* or ΔJ_*). Denote the
  study-level hats `B_perfect_hat`, `B_predicted_hat`, `L_prediction_hat`.
- **FR-864**: Report 95% **percentile** stratified group-bootstrap CI on the
  primary estimator **only** when the estimand status allows (FR-866). Also
  report **median** split_group effect as secondary.
- **FR-865**: No CI-crossing rule SHALL produce GO/NO-GO.
- **FR-866**: Each estimand SHALL have exactly one status:
  - `FULL_COHORT_ESTIMATE`: all required arm-voyage pairs for that estimand
    are feasible/defined on the complete 48-voyage corpus.
  - `CONDITIONAL_FEASIBLE_ESTIMATE`: one or more required pairs are
    infeasible, but each LOW/MID/HIGH stratum has **at least 3 of its 4**
    split_groups with a defined group-level effect. Any reported effect is
    **explicitly conditional on feasible voyages** and SHALL NOT be described
    as the unconditional 48-voyage effect. A group with only a subset of
    voyages defined MAY contribute only to this status; report `n_defined`.
  - `WITHHELD_INSUFFICIENT_FEASIBILITY`: any stratum has fewer than 3 defined
    groups. **No** overall bootstrap CI or effect claim is produced for that
    estimand.
  Always report per-arm feasibility counts and identities **before** effect
  estimates. Never pair-delete or impute.
- **FR-867**: IID test remains sealed. OOD SHALL NOT be used for controller
  selection, C* tuning, or predictor selection.

### H. Practical qualification and freeze (R7)

- **FR-870**: Formal scientific outcomes SHALL be measured on **Model-E logs**,
  not on C* surrogate J. The T-445 1% proxy-vs-plant fuel replica is **not**
  a qualification Gate.
- **FR-871**: Before formal execution, qualification SHALL establish, with
  machine-readable PASS/FAIL:
  1. deterministic two-run equality on a synthetic or nonformal fixture;
  2. no hidden future leakage (C0 uses only t<t0 completed bin or t0 sample;
     C2 causality audit FR-832);
  3. ForecastWindow schema/contract tests;
  4. C0/C1/C2 controller-code identity (same binary/source hash);
  5. identical non-forecast configuration dump across C0/C1/C2;
  6. Δλ action-bound correctness;
  7. current-state initialization from plant at t0;
  8. SimState/checkpoint reproducibility if segmented execution is used;
  9. hard-feasibility instrumentation live;
  10. FEC accounting identity on a fixture;
  11. C2 causal feature availability (FR-832–834);
  12. zero scientific test/OOD access in the access ledger;
  13. no result-bearing controller tuning after freeze.
- **FR-872**: A pre-execution freeze record SHALL hash: C* code, config,
  ForecastWindow schema, C0/C1/C2 assemblers, predictor checksums, corpus
  12-group list, bootstrap seed, estimand formulas, feasibility policy.
- **FR-873**: After freeze, changing any FR-811 parameter from formal
  outcomes is forbidden. Finer-grid diagnostics (FR-813) SHALL NOT retune
  the formal controller after result observation.
- **FR-874**: Access ledger purpose tags SHALL distinguish QUALIFICATION /
  SYNTHETIC_SMOKE / FORMAL. FORMAL count SHALL be 0 until freeze overall=true.
- **FR-875**: Integrity/leakage/sealing/hash failures SHALL yield
  `STUDY_VALIDITY_FAIL` (methodological), not a scientific claim that
  prediction has no value.

### I. Realization ratio (R10)

- **FR-880**: Realization ratio is **DIAGNOSTIC ONLY**, not a primary metric
  and not a GO/NO-GO criterion. Do **not** compute or average per-voyage
  ratios. The study-level diagnostic is
  `R_diagnostic = B_predicted_hat / B_perfect_hat`
  using the FR-863 hats.
- **FR-881**: Emit `R_UNDEFINED` (no numeric R) if: the relevant effect
  estimate is withheld (`WITHHELD_INSUFFICIENT_FEASIBILITY`);
  `B_perfect_hat <= 0`; `abs(B_perfect_hat) < 0.10` percentage points; or the
  denominator is non-finite/invalid. Guard is prospective and SHALL NOT be
  changed after seeing formal results.

### J. Terminal scientific outputs

- **FR-890**: Terminal package SHALL include: per-arm-voyage J_FEC, fuel,
  SOC0/SOC_end, feasibility records; defined B_*/ΔJ_*; group-level means and
  medians; bootstrap CI; withheld/infeasible lists; C2 causality audit;
  freeze record; T-445 limitation appendix (Q2/Q4/Q6); explicit non-claims
  for C1.
- **FR-891**: Terminal text SHALL use the label
  `DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE` and state that: these are
  development/validation-corpus estimates; they quantify practical closed-loop
  effects **within frozen C\***; they are **not** an independent final
  generalization claim; sealed IID test remains reserved for later
  confirmatory evaluation; OOD remains separate and is not used for
  controller/model selection; they are not a trajectory-information
  theoretical upper bound; they are not a 2% GO/NO-GO.

---

## Non-Functional Requirements

- **NFR-800**: Dataset-v3 bytes SHALL remain unchanged.
- **NFR-801**: Model-E `.slx` SHALL NOT be persistently modified.
- **NFR-802**: `st_edge_ai_upload/` is
  `OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH`. SHALL NOT be modified,
  added to, moved, deleted, or cleaned.
- **NFR-803**: No absolute paths in executable scripts.
- **NFR-804**: Formal execution remains Mac-local MATLAB R2026a (same class
  as T-445 NFR-712) unless a later Gate says otherwise.
- **NFR-805**: Predictor-v3 winner artifacts consumed **read-only**.

---

## Acceptance Criteria (machine-testable)

- **AC-800**: A linter/test rejects C1 descriptions matching the forbidden
  upper-bound strings in FR terminology.
- **AC-801**: C0 unit tests: t0=0 uses P_load(t0); t0=10 uses mean over
  [0,10); an incomplete current bin is not used; injecting future samples
  into C0 assembler does not change output.
- **AC-802**: C1 unit tests: Pbar matches frozen Dataset-v3 `y_k` for
  `(voyage_id, t0)`; two assemblers with different fake closed-loop future
  loads produce **identical** C1 Pbar (arm-independence); only shrinking
  metadata may differ.
- **AC-803**: ForecastWindow schema validator accepts C0/C1/C2 payloads of
  equal shape; C* hash identical across three config dumps except
  `source_id` + Pbar provenance.
- **AC-804**: C* SOC grid assertion: numel(0.10:0.005:0.95)==171.
- **AC-805**: FEC identity test: J_FEC reconstruction from fuel and ΔSOC
  matches FR-840 within 1e-12 relative.
- **AC-806**: Estimand identity test uses FR-842 zero-safe residual bound,
  including a case with B_perfect == 0.
- **AC-807**: Feasibility policy test: marking C1 HARD_INFEASIBLE withholds
  B_perfect and does not copy J_C0 into J_C1.
- **AC-808**: Corpus freeze test: exactly 12 split_group_ids = union of
  manifest roles `initial` and `reserve`; 48 voyage_ids; no `unselected`.
- **AC-809**: `R_diagnostic` undefined test: withheld status, hats 0, −1, and
  +0.05 all yield `R_UNDEFINED`; no per-voyage R is emitted.
- **AC-810**: Pre-execution freeze refuses FORMAL ledger entries if freeze
  overall≠true.
- **AC-811**: Static scan: C2 loader paths do not include `st_edge_ai_upload`.
- **AC-812**: Sealed-access test: scientific IID-test and OOD controller
  selection counters remain 0.
- **AC-813**: Hard-feasibility copy: canonical JSON of T-445
  `hard_feasibility` object SHA256 equals the study-local file SHA256
  (sort_keys canonical dump).
- **AC-814**: TERMINAL_SOC_ADMISSIBILITY: `|SOC_end-SOC0|>0.005` marks
  HARD_INFEASIBLE even if SOC stayed in [0.10, 0.95].

---

## Open / bounded (not reopened Brainstorm)

These are **implementation/Design** obligations, not new scientific claims:

1. Exact per-channel C2 causality table (FR-832) is **produced at
   pre-execution freeze**. Requirements freeze the classification enum.
2. Keras file lives under `experiments/` (not always in the git tree);
   freeze SHALL verify SHA256 `d8da6af4…` before formal C2.
3. Formal runner architecture is a **Design** freeze (one architecture for
   C0/C1/C2; SimState equivalence mandatory if segmented).

No Brainstorm decision remains open. RG1–RG9 are closed in this file.

---

## Traceability

| Brainstorm / Gate | Requirements |
|---|---|
| R1 arms / naming | FR-800–805 |
| R2 C0 persistence | FR-821, AC-801 |
| R3 C* freeze | FR-810–815, AC-804 |
| R4 ForecastWindow | FR-820–825, AC-803 |
| R5 estimands | FR-840–844, AC-805–806 |
| R6 48-voyage / stats | FR-860–867, AC-808 |
| R7 qualification | FR-870–875, AC-810 |
| R8 C2 causality | FR-830–836, AC-811 |
| R9 feasibility | FR-850–857, AC-807/813/814 |
| R10 R diagnostic; st_edge | FR-880–881, NFR-802, AC-809/811 |
| RG1 C1 arm-independent truth | FR-822, AC-802 |
| RG2 TERMINAL_SOC_ADMISSIBILITY | FR-850a, AC-814 |
| RG3 stratified coverage | FR-866 |
| RG4 HF local copy | FR-857, AC-813 |
| RG5 fuel_raw | FR-840a |
| RG6 full predictor identity | FR-836 |
| RG7 zero-safe identity | FR-842, AC-806 |
| RG8 R_diagnostic hats | FR-880–881 |
| RG9 evidence label | FR-891 |

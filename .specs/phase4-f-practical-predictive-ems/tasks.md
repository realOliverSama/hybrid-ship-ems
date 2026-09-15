# Phase 4-F Practical Predictive EMS — Tasks

Spec: `phase4-f-practical-predictive-ems`  
Status: **tasks-approved**  
Created: 2026-09-15  
Approved: 2026-09-15（Tasks Gate **APPROVED WITH REQUIRED CORRECTIONS** TG1–TG9 applied; no second Tasks Gate）  
Design: `./design.md`（CLOSED / APPROVED, D1–D8）  
Requirements: `./requirements.md`（CLOSED / APPROVED, RG1–RG9）  
Brainstorm: `./brainstorm.md`（CLOSED / APPROVED; not reopened）

Umbrella roadmap id: **T-456**. Local ids T-001–T-012. Do not reuse T-445 task ids.

**Implementation authorization（Tasks Gate ruling）:** T-001 … T-009 only.
T-010 formal result-bearing execution is **NOT** authorized by this Gate, even
if T-009 returns `overall=true`. STOP after T-009
`PRE_EXECUTION_FREEZE_GATE` and return the freeze package for human review.

---

## Execution constraints

- Branch `feat/phase4-f`; no `git add -A`; Git 红线; stage explicit files only.
- **No formal P0/C0/C1/C2 validation-corpus runs** until T-009 gate
  `overall=true` **and** a later human Pre-Execution Gate authorization
  (TG9). This session stops at T-009 regardless.
- IID test / OOD scientific access forbidden.
- No C* retune from outcomes; no REAT-T retrain/reselect.
- No persistent `.slx` change; `st_edge_ai_upload/`
  OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH (never stage/hash-into-study/
  clean/modify/move/delete).
- Dataset-v3 bytes read-only.
- Formal C1 = stored npz lookup only; missing key FAIL CLOSED (TG5).
- STOP and escalate if: C1 store unbound; warm-up/H1 causality broken; K/F
  undefendable; T_eval not arm-independent.

---

## TG2 — FORMAL_EPOCH policy (binding on T-009/T-010)

After T-009 freeze `overall=true`, any change to scientific execution identity
— including but not limited to C* code; ForecastWindow assemblers; runner;
Model-E model identity; study config; hard-feasibility config;
predictor/model/norm artifacts; T_eval table; C1 target source/index;
causality contract; estimand/statistics code where relevant to formal
extraction — **invalidates the current formal epoch**.

If a defect is discovered after any formal run has occurred:

1. STOP further formal execution if the defect can affect scientific identity;
2. preserve all existing outputs;
3. mark them `QUARANTINED_INVALIDATED_EPOCH`;
4. never overwrite/delete/relabel them as valid;
5. fix the defect prospectively;
6. rerun qualification as affected;
7. create a new PRE_EXECUTION_FREEZE identity;
8. start a new formal epoch;
9. rerun every affected formal run under one consistent identity.

Never mix formal outcomes from different scientific code/config epochs.
Ordinary checkpoint resume **without** identity change does not create a new
epoch.

---

## TG4 — Scientific infeasibility is NOT an execution STOP

During formal execution (T-010, when later authorized): HARD_INFEASIBLE is a
scientific controller outcome (mission/service failure; load/propulsion
shortfall; terminal-SOC inadmissibility; SOC/battery/converter/V/I hard
violation; generator hard violation; Safety/PMS abnormal intervention per the
frozen policy). On HARD_INFEASIBLE: preserve the complete run; mark the
arm-voyage infeasible; record offending predicates; **continue** the remaining
formal execution plan.

Do NOT: retune C*; delete/replace the voyage; rerun hoping for feasibility;
fabricate J_FEC savings; terminate the study because the controller performed
poorly.

Formal execution STOPs only for methodological/integrity failures:
identity/hash mismatch; information leakage; corrupted runner/checkpoint
semantics; wrong model/config loaded; scientific code mutation; sealed-data
violation; non-deterministic infrastructure defect invalidating the run.

---

## TG1 — Formal execution vs effect interpretation

T-010 (when later authorized) is **result-generation only**: frozen
per-arm/per-voyage Model-E outputs; raw fuel; SOC; J_FEC; feasibility
predicates; intervention/audit logs; provenance/checkpoint identities.

T-010 SHALL NOT perform or interpret: B_perfect; B_predicted; L_prediction;
R_diagnostic; bootstrap CI; comparative scientific conclusions. Those belong
to T-011 after the complete fixed formal corpus exists. Formal execution must
not be adapted based on emerging effect sizes.

---

## DAG

```text
T-001 canonical identities / T_eval table (TG6) / study preparation
   → T-002 C1 canonical target index / 100% coverage (TG5) / parity
   → T-003 K/F + H1 causality-ledger verification
   → T-004 ForecastWindow / C0 persistence / common warm-up tests
   → T-005 C* frozen grid / segmented SimState runner / D8 checkpoint identity
   → T-006 Model-E FEC / raw-fuel / hard-feasibility machine
   → T-007 pairwise/common-triplet estimands + equal-tier bootstrap (synthetic)
   → T-008 complete NONFORMAL qualification suite
   → T-009 PRE_EXECUTION_FREEZE_GATE (TG2/TG3/TG7/TG8/TG9)
   ═══ HUMAN GATE ═══
   → T-010 formal fixed 48×4 result generation     [NOT AUTHORIZED]
   → T-011 effect estimation / bootstrap / report  [NOT AUTHORIZED]
   → T-012 final evidence sealing                  [NOT AUTHORIZED]
```

T-010 is blocked on T-009 `overall=true` **and** explicit human Pre-Execution
Gate authorization. No result-bearing skip of qualification.

---

## A — Freeze and interfaces (nonformal)

- [ ] 1. Canonical identities / T_eval table / study preparation
  - ID: T-001
  - Requirement(s): FR-810–815, FR-836, FR-857, FR-860, DGN-PPE-02b/D4, TG6
  - depends_on: none
  - Files/areas: `config/phase4f_ppe_study_config.json`;
    `config/phase4f_ppe_teval_table.json` (new, hashed);
    `results/ppe_practical/` study dir
  - Scope: Freeze C* numerics (SOC 0.005 n=171, Δλ grid, k_eq_E=0.5457,
    w_term=1.17e5) referencing T-445 frozen config; record FR-836 artifact
    SHAs (keras/norms/winner/config); corpus 12-group list with tiers;
    causality-ledger SHA. **TG6 T_eval table** — for each of the 48 formal
    voyages: voyage_id; split_group_id; tier; t_start; T_eval; T_eval
    provenance (Dataset-v3 native-stream t_end + extraction code identity);
    initial-condition identity; mission/reference identity; expected 10 s
    segment count; legal control-update t0 sequence; t0=300 s
    forecast-source differentiation boundary; expected number of
    differentiated C1/C2 updates; final partial-bin duration if any; target
    exhaustion boundary. Hash the table; carry identity into T-009.
    **No voyage control runs.**
  - Non-scope: no C* code change beyond config identity files
  - Validation: T_eval table 48 rows, all fields populated; per-voyage
    T_eval equals the frozen Dataset-v3 native t_end (machine-checked against
    the same extraction path as the frozen pipeline); HF object SHA equals
    `fcef573d…`; corpus identity `245fa307…`; table SHA recorded
  - Gate/STOP: cannot bind T_eval for a formal voyage → STOP
  - Completion evidence: hashed freeze-prep JSON + T_eval table JSON

- [ ] 2. C1 canonical target index / 100% coverage / parity
  - ID: T-002
  - Requirement(s): FR-822, AC-802, DGN-PPE-01b/D2, TG5
  - depends_on: T-001
  - Scope: Index `(voyage_id, t0) → y_k` from
    `datasets_v3/formal/windows_v3_formal.npz` (SHA `203dc92d…`). Formal path
    = lookup only; runtime reconstruction **forbidden**. **TG5:** enumerate
    every legal C1-differentiated issue time per formal voyage
    (`t0 ≥ 300 s`, on the frozen 10 s grid, `t0 < T_eval`, shrinking-horizon
    semantics from the T-001 table) and prove **100%** of required
    `(voyage_id, t0)` keys exist in the canonical store; any missing key →
    FAIL CLOSED + escalate. Emit machine-readable coverage/index artifact
    bound to canonical target-store SHA + T_eval-table SHA. QUALIFICATION
    (required): stored-vs-`targets.py` reconstruction parity on a
    nonformal/synthetic or non-corpus native-stream fixture; fail-closed demo
    on missing key. **Do not read formal C* outcomes.**
  - Validation: coverage = 48/48 voyages, 100% keys; parity rel < 1e-9;
    missing-key raises
  - Gate/STOP: npz missing or SHA mismatch → STOP; coverage < 100% → STOP
  - Completion evidence: `c1_target_index.json` + coverage JSON +
    QUALIFICATION JSON

- [ ] 3. C2 K/F + H1 causality verification
  - ID: T-003
  - Requirement(s): FR-830–836, DGN-PPE-04/D7
  - depends_on: T-001
  - Scope: Machine-check `phase4f_ppe_causality_ledger.json` against
    registry + producer schema + `ORACLE_DERIVED` forbidden + formal
    `arms_forecast=GOOD_V1` provenance + `source_cutoff ≤ issue_time` on
    **fixture/metadata** (formal-voyage KF provenance records, not controller
    fuel). Do not zero channels; do not retrain.
  - Gate/STOP: any required K/F channel undefendable → STOP escalate
  - Completion evidence: verification JSON `verdict=PASS`

- [ ] 4. ForecastWindow / C0 persistence / common warm-up tests
  - ID: T-004
  - Requirement(s): FR-820–825, DGN-PPE-02/D3, AC-801, AC-803
  - depends_on: T-001
  - Scope: Unit/synthetic: ForecastWindow schema validator (C0/C1/C2 payloads
    equal shape; C* config dumps identical except source_id/Pbar provenance);
    C0 persistence completed-bin rule, no future, t0<10 s current-sample
    fallback; Option-B warm-up `[0,300)` all C* arms use C0 ForecastWindow;
    t0<300 C2 never demands 300 s H1; no zero-pad; no post-t0 history;
    C1/C2 differentiation only at legal anchors ≥300 s.
  - Validation: AC-801, AC-803, D3 tests
  - Gate/STOP: cannot define causal 300 s H1 → STOP
  - Completion evidence: unit-test JSON

---

## B — Controller/runner/metrics (nonformal)

- [ ] 5. C* frozen grid / segmented SimState runner / D8 checkpoint identity
  - ID: T-005
  - Requirement(s): FR-810–815, FR-871, DGN-PPE-07/D8, AC-804
  - depends_on: T-001, T-004
  - Scope: SOC grid 0.005 assertion (n=171); segmented 10 s SimState runner
    shared by C0/C1/C2 (single architecture); checkpoint binds the full D8
    field list (arm; voyage_id; t0/segment index; study config SHA; code
    commit SHA; Model-E .slx SHA; C* implementation SHA; ForecastWindow
    provenance/hash; hard-feasibility config SHA; corpus identity SHA;
    canonical T_eval identity; Predictor-v3 model/config/norm SHAs for C2 or
    explicit `N/A` for C0/C1); mismatch refuse; resume identity test on
    **toy/nonformal** fixture; SimState continuous-vs-segmented equivalence
    on the C* runner (toy). P0 continuous runner type declared with
    StopTime=T_eval. **No 48-voyage formal runs.**
  - Validation: n_soc=171; SimState equivalence PASS on toy; checkpoint
    mismatch refuse PASS; resume == uninterrupted PASS
  - Gate/STOP: SimState equivalence fail on runner → STOP
  - Completion evidence: runner QUALIFICATION JSON

- [ ] 6. Model-E FEC / raw-fuel / hard-feasibility machine
  - ID: T-006
  - Requirement(s): FR-840–857, AC-805/807/813/814
  - depends_on: T-001
  - Scope: J_FEC from Model-E `fuel_total_kg` + ΔSOC (1071.36 × 0.5457);
    0.6 kg/start; TERMINAL_SOC_ADMISSIBILITY HARD; HF config object SHA
    equality (AC-813); FULL_COHORT/CONDITIONAL_FEASIBLE/
    WITHHELD_INSUFFICIENT_FEASIBILITY status machine; no pair-delete/impute.
    Synthetic feasibility fixtures only.
  - Validation: AC-805/807/813/814
  - Completion evidence: metrics QUALIFICATION JSON

- [ ] 7. Pairwise/common-triplet estimands + equal-tier bootstrap (synthetic)
  - ID: T-007
  - Requirement(s): FR-860–866, FR-880–881, DGN-PPE-08/D5/D6, AC-806/809
  - depends_on: T-006
  - Scope: Voyage pairwise estimands (FR-842 zero-safe identity incl.
    B_perfect==0 case); COMMON_TRIPLET_DECOMPOSITION mask; hats only on the
    common mask; conditional θ=(m_L+m_M+m_H)/3; per-tier bootstrap
    (draw = n_defined within tier), B=10000, seed
    `phase4f-t445-bootstrap-v1`, percentile 2.5/97.5 `method="linear"`;
    R_diagnostic study-level + R_UNDEFINED guards; pair-specific estimates
    labeled separately, never combined across masks. **Synthetic group
    tables only — no formal J_FEC.**
  - Validation: identity residual; R_UNDEFINED cases; 3-of-4 coverage;
    equal-tier weighting proof (tier with 3 defined groups not down-weighted)
  - Completion evidence: stats unit JSON

---

## C — Qualification + freeze (entry ticket)

- [ ] 8. Complete NONFORMAL qualification suite
  - ID: T-008
  - Requirement(s): FR-871, DGN-PPE-09 (items 1–8 plus prior), AC-800–814
  - depends_on: T-002, T-003, T-004, T-005, T-006, T-007
  - Scope: Run the full nonformal suite: determinism two-run; C0 leakage;
    C1 stored-target parity + fail-closed + arm-independence; C2 warm-up
    causality; T_eval arm-equality; ForecastWindow parity; action bounds;
    current-state init; SimState equivalence; checkpoint mismatch refusal;
    FEC reconstruction; HF instrumentation; terminal-SOC predicate live;
    common-triplet statistics; equal-tier bootstrap; R_UNDEFINED; predictor
    checksum verification; access-ledger sealing; estimand identity.
    Record T-445 Q2/Q4/Q6 as documented limitations.
  - Gate/STOP: any mandatory item FAIL → no T-009
  - Completion evidence: `qualification/ppe_qual_suite.json`

- [ ] 9. PRE_EXECUTION_FREEZE_GATE
  - ID: T-009
  - Requirement(s): FR-872–875, AC-810, TG2, TG3, TG7, TG8, TG9
  - depends_on: T-008 PASS
  - Scope:
    - **TG3 formal execution manifest** (deterministic, prospective): exactly
      48 voyages × 4 arms (P0/C0/C1/C2) = **192 arm-voyage run identities**;
      per identity: voyage_id; split_group_id; tier; arm; run_id; T_eval
      identity; initial-condition identity; expected runner type; expected
      checkpoint identity policy; deterministic execution order. No
      voyage/arm may be removed/replaced/reordered because of observed
      scientific performance. P0 contextual but listed.
    - **TG7 bind exact identities**: approved Brainstorm SHA; approved
      Requirements SHA; approved Design SHA; approved Tasks SHA; exact code
      commit SHA; C* implementation SHA; ForecastWindow C0/C1/C2 assembler
      SHAs; runner/checkpoint implementation SHA; Model-E .slx SHA; study
      config SHA; hard-feasibility config SHA; C1 target store +
      coverage-index SHA; canonical T_eval table SHA; REAT-T keras SHA;
      predictor/config identity; normalization SHAs; causality-ledger SHA;
      corpus manifest/identity SHA; formal execution-manifest SHA;
      bootstrap/statistics implementation SHA; access-ledger state.
    - **TG8 waiver artifact** `KNOWN_NONBLOCKING_SPEC_VALIDATION_WAIVER`:
      validator `pi-spec` 0.1.0 (`spec_validate` structure); failing check
      `brainstorm.md captures user intent`; closed Brainstorm artifact SHA;
      reason = approved artifact lacks template-only `## User Intent`
      heading; scientific-content impact = none; approval source = Tasks
      Gate TG8. **Only this exact failure is waived.** Any additional or
      changed validator failure is blocking → STOP and investigate.
    - Scientific working tree clean; the only permitted unrelated untracked
      content is `st_edge_ai_upload/` (untouched, not hashed into study).
    - **TG9 gate artifact** `PRE_EXECUTION_FREEZE_GATE` with
      `overall = true|false` and machine PASS/FAIL for every prerequisite:
      T-001…T-008 complete; all mandatory qualification PASS; C1 target
      coverage = 100%; C1 stored/reconstruction parity PASS; K/F causality
      ledger PASS; C2 300 s warm-up causality PASS; ForecastWindow parity
      PASS; C0 no-leakage PASS; C1 arm-independence PASS; C* determinism
      PASS; action bounds/grid PASS; current-state initialization PASS;
      SimState equivalence PASS; checkpoint mismatch refusal PASS; FEC
      reconstruction PASS; hard-feasibility instrumentation PASS;
      terminal-SOC predicate live; common-triplet statistics synthetic PASS;
      equal-tier conditional bootstrap PASS; R_UNDEFINED PASS; formal
      execution manifest = exactly 192 identities; frozen corpus = 48
      voyages / 12 groups / 4-4-4 tiers; REAT-T artifact identities match;
      Model-E .slx identity matches; FORMAL access-ledger count = 0; IID
      scientific access = 0; OOD scientific/controller-selection access = 0;
      no result-bearing controller tuning; scientific working tree clean;
      only approved spec_validate waiver exists.
  - Gate/STOP: `overall=false` → STOP and report. `overall=true` → **ALSO
    STOP**: return the complete freeze package for human/ChatGPT
    Pre-Execution Gate review. Do **not** enter T-010.
  - Completion evidence: `ppe_pre_execution_freeze.json` +
    `PRE_EXECUTION_FREEZE_GATE` artifact + formal execution manifest +
    waiver record

---

## D — Formal execution (NOT AUTHORIZED in this session)

- [ ] 10. Formal 48-voyage × 4-arm result generation
  - ID: T-010
  - Requirement(s): FR-800–805, FR-860, DGN-PPE-07, TG1, TG4
  - depends_on: T-009 `overall=true` **and** human Pre-Execution Gate
    authorization (neither suffices alone)
  - Scope (**result-generation only**, TG1): execute the frozen 192-identity
    manifest; same T_eval/IC/mission per voyage; C* segmented runner;
    C1 stored lookup fail-closed; C2 seed0 keras; P0 continuous contextual.
    Produce per-arm-voyage Model-E outputs: raw fuel, SOC0/SOC_end, J_FEC,
    feasibility predicates, intervention/audit logs, provenance/checkpoint
    identities. **No** B_*/L_prediction/R_diagnostic/CI/comparative
    conclusions (TG1 → T-011). HARD_INFEASIBLE = scientific outcome, not a
    STOP (TG4); integrity failures → abort `STUDY_VALIDITY_FAIL` (FR-875).
    FORMAL_EPOCH policy TG2 applies throughout.
  - Gate/STOP: missing C1 target; keras mismatch; sealing breach;
    identity/hash mismatch; leakage → abort `STUDY_VALIDITY_FAIL`
  - Completion evidence: per-arm-voyage JSON + feasibility lists

- [ ] 11. Effect estimation / bootstrap / development-stage report
  - ID: T-011
  - Requirement(s): FR-840–844, FR-866, FR-880–881, FR-890–891, TG1
  - depends_on: T-010 complete (full fixed corpus)
  - Scope: Feasibility first; pairwise estimates on their own masks;
    COMMON_TRIPLET hats + CI or WITHHELD; equal-tier estimator; R_diagnostic
    with R_UNDEFINED guards; label
    `DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE`.
  - Completion evidence: `ppe_effect_report.json` + `final_report.md`

- [ ] 12. Final evidence sealing
  - ID: T-012
  - Requirement(s): FR-890–891, NFR-800–805
  - depends_on: T-011
  - Scope: Hash chain; README sync; no T-445 relabel.
  - Completion evidence: `evidence_index.json`

---

## Formal preconditions (T-010)

1. T-008 all mandatory PASS
2. T-009 `PRE_EXECUTION_FREEZE_GATE` overall=true
3. **Human Pre-Execution Gate authorization granted** (TG9)
4. keras SHA match
5. C1 npz SHA match and 100% coverage of required (voyage,t0) keys
6. T_eval table complete and hashed
7. causality ledger verification PASS
8. access ledger FORMAL=0 / test=0 / OOD=0 at freeze
9. `.slx` SHA unchanged vs T-445 baseline `190de549…`
10. formal execution manifest = 192 identities, frozen prospectively

## STOP conditions (escalate, do not “fix” with val outcomes)

- Legal C1 stored target cannot be bound / coverage < 100%
- Causal 300 s H1 policy broken
- Required K/F channel undefendable
- T_eval not arm-independent
- Freeze FAIL
- spec_validate degrades beyond the single TG8-waived failure

---

## Verification checklist

- [x] TG1–TG9 incorporated (T-010 result-only; FORMAL_EPOCH; 192-manifest;
      infeasibility≠STOP; C1 100% coverage; TG6 T_eval fields; TG7 bind list;
      TG8 waiver; TG9 human gate)
- [x] D1–D8 mapped into task Scope
- [x] Qualification/freeze before formal
- [x] No 2% GO; no 1% oracle replica Gate
- [x] IID/OOD sealed
- [x] st_edge_ai_upload not in any task Files/areas

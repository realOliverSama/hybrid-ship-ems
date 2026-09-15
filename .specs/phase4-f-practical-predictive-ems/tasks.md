# Phase 4-F Practical Predictive EMS — Tasks

Spec: `phase4-f-practical-predictive-ems`  
Status: **tasks-draft** — **NOT APPROVED** (Tasks Gate pending)  
Created: 2026-09-15  
Design: `./design.md`（**design-approved**, D1–D8）  
Requirements: `./requirements.md`（CLOSED / APPROVED）  
Brainstorm: `./brainstorm.md`（CLOSED / APPROVED）

Umbrella roadmap id: **T-456**. Local ids T-001–T-012. Do not reuse T-445 task ids.

This file is **not** implementation authorization.

---

## Execution constraints

- Branch `feat/phase4-f`; no `git add -A`; Git 红线.
- **No formal P0/C0/C1/C2 validation-corpus runs** until T-009 freeze `overall=true`.
- IID test / OOD scientific access forbidden.
- No C* retune from outcomes; no REAT-T retrain/reselect.
- No persistent `.slx` change; `st_edge_ai_upload/` DO NOT TOUCH.
- Dataset-v3 bytes read-only.
- Formal C1 = stored npz lookup only; missing key FAIL CLOSED.
- STOP and escalate if: C1 store unbound; warm-up/H1 causality broken; K/F
  undefendable; T_eval not arm-independent.

---

## DAG

```text
T-001 freeze identities / T_eval table / HF+causality ledgers
   → T-002 C1 stored-target index + stored-vs-recomputed QUALIFICATION
   → T-003 C2 causality ledger verification (no silent zero)
   → T-004 ForecastWindow + Option-B warm-up unit tests
   → T-005 C* SOC=0.005 + segmented runner + checkpoint identity (D8)
   → T-006 FEC / fuel_raw / TERMINAL_SOC / feasibility status machine
   → T-007 COMMON_TRIPLET + equal-tier bootstrap (synthetic)
   → T-008 qualification suite (all DGN-PPE-09 items)
   → T-009 pre-execution freeze (entry ticket)
   → T-010 formal 48-voyage P0/C0/C1/C2  [ONLY IF T-009 PASS]
   → T-011 effect report + DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE
   → T-012 evidence index / sealing
```

T-010 is blocked on T-009. No result-bearing skip of qualification.

---

## A — Freeze and interfaces (nonformal)

- [ ] 1. Study config + T_eval table + ledger hashes
  - ID: T-001
  - Requirement(s): FR-810–815, FR-836, FR-857, FR-860, DGN-PPE-02b/D4, D5, D8
  - depends_on: none
  - Files/areas: `config/phase4f_ppe_*.json` (HF already present); T_eval table
    under `results/` study dir (to be named in implementation, not created now)
  - Scope: Freeze C* numerics (SOC 0.005 n=171, Δλ grid, k_eq, w_term);
    record keras/norm SHAs (FR-836); extract per-voyage `T_eval` from frozen
    Dataset-v3 native `t_end` for the 48 corpus ids; hash causality ledger;
    corpus 12-group list. **No voyage control runs.**
  - Non-scope: no C* code change beyond config identity files
  - Validation: T_eval table 48 rows; HF object SHA equals `fcef573d…`;
    keras path recorded; corpus identity `245fa307…`
  - Gate/STOP: cannot bind T_eval for a formal voyage → STOP
  - Completion evidence: hashed freeze-prep JSON

- [ ] 2. C1 stored-target index + QUALIFICATION parity
  - ID: T-002
  - Requirement(s): FR-822, AC-802, DGN-PPE-01b/D2
  - depends_on: T-001
  - Scope: Index `(voyage_id, t0) → y_k` from
    `datasets_v3/formal/windows_v3_formal.npz` (SHA `203dc92d…`). Formal path
    = lookup only. QUALIFICATION: stored vs `targets.py` reconstruction on a
    **nonformal/synthetic or non-corpus** native stream fixture; fail-closed
    demo on missing key. **Do not read formal C* outcomes.**
  - Validation: parity bound; missing-key raises
  - Gate/STOP: npz missing or SHA mismatch → STOP
  - Completion evidence: index schema + QUALIFICATION JSON

- [ ] 3. C2 K/F causality verification
  - ID: T-003
  - Requirement(s): FR-830–836, DGN-PPE-04/D7
  - depends_on: T-001
  - Scope: Machine-check `phase4f_ppe_causality_ledger.json` against
    registry + producer schema + `ORACLE_DERIVED` forbidden +
    `source_cutoff ≤ issue_time` on **fixture/metadata** (not controller
    fuel). Do not zero channels; do not retrain.
  - Gate/STOP: any required K/F channel undefendable → STOP escalate
  - Completion evidence: verification JSON `verdict=PASS`

- [ ] 4. ForecastWindow + Option-B warm-up tests
  - ID: T-004
  - Requirement(s): FR-820–825, DGN-PPE-02/D3
  - depends_on: T-001
  - Scope: Unit/synthetic: C0 persistence no future; warm-up [0,300) all C*
    use C0 ForecastWindow; t0<300 C2 does not demand 300 s H1 from zeros;
    no zero-pad; no post-t0 history.
  - Validation: AC-801 and D3 tests
  - Gate/STOP: cannot define causal 300 s H1 → STOP
  - Completion evidence: unit-test JSON

---

## B — Controller/runner/metrics (nonformal)

- [ ] 5. C* grid + segmented runner + D8 checkpoint identity
  - ID: T-005
  - Requirement(s): FR-810–815, FR-871, DGN-PPE-07/D8
  - depends_on: T-001, T-004
  - Scope: SOC grid 0.005; segmented 10 s SimState runner shared by C0/C1/C2;
    checkpoint binds D8 field list; mismatch refuse; resume identity test on
    **toy/nonformal** fixture. P0 continuous StopTime=T_eval (when T_eval
    known). **No 48-voyage formal runs.**
  - Validation: n_soc=171; SimState equivalence on toy; mismatch refuse
  - Gate/STOP: SimState fail on runner → STOP
  - Completion evidence: runner QUALIFICATION JSON

- [ ] 6. FEC, fuel_raw, TERMINAL_SOC, feasibility statuses
  - ID: T-006
  - Requirement(s): FR-840–857, AC-805/807/814
  - depends_on: T-001
  - Scope: Wire J_FEC from Model-E `fuel_total_kg` + ΔSOC; 0.6 kg/start;
    TERMINAL_SOC_ADMISSIBILITY; FULL/CONDITIONAL/WITHHELD machine; no
    pair-delete/impute. Synthetic feasibility fixtures only.
  - Validation: AC-805/807/814
  - Completion evidence: metrics QUALIFICATION JSON

- [ ] 7. COMMON_TRIPLET + equal-tier bootstrap (synthetic)
  - ID: T-007
  - Requirement(s): FR-860–866, FR-880–881, DGN-PPE-08/D5/D6
  - depends_on: T-006
  - Scope: Implement hats on common-triplet mask; conditional
    θ=(m_L+m_M+m_H)/3; bootstrap 1/3 tiers; R_diagnostic study-level;
    withheld path. **Synthetic group tables only — no formal J_FEC.**
  - Validation: identity residual; R_UNDEFINED cases; 3-of-4 coverage
  - Completion evidence: stats unit JSON

---

## C — Qualification + freeze (entry ticket)

- [ ] 8. Qualification suite
  - ID: T-008
  - Requirement(s): FR-871, DGN-PPE-09 (all items 1–8 plus prior)
  - depends_on: T-002, T-003, T-004, T-005, T-006, T-007
  - Scope: Run the full nonformal suite. Record T-445 Q2/Q4/Q6 as limitations.
  - Gate/STOP: any mandatory item FAIL → no T-009
  - Completion evidence: `qualification/ppe_qual_suite.json`

- [ ] 9. Pre-execution freeze
  - ID: T-009
  - Requirement(s): FR-872–875, AC-810
  - depends_on: T-008 PASS
  - Scope: Hash C* code, configs, ForecastWindow schema, assemblers, keras
    SHA match, corpus 48, T_eval table, bootstrap seed, HF SHA, causality
    ledger, `.slx` SHA. Access ledger FORMAL=0, test/OOD=0.
  - Gate/STOP: freeze overall≠true → **no T-010**
  - Completion evidence: `ppe_pre_execution_freeze.json` overall=true

---

## D — Formal execution (blocked on freeze)

- [ ] 10. Formal 48-voyage P0/C0/C1/C2
  - ID: T-010
  - Requirement(s): FR-800–805, FR-860, DGN-PPE-07
  - depends_on: T-009 overall=true
  - Scope: Same T_eval/IC/mission; C* segmented; C1 stored lookup fail-closed;
    C2 seed0 keras; P0 continuous contextual. Feasibility records. No
    pair-delete. No IID/OOD.
  - Gate/STOP: missing C1 target; keras mismatch; sealing breach → abort
    `STUDY_VALIDITY_FAIL`
  - Completion evidence: per-arm-voyage JSON + feasibility lists

- [ ] 11. Effect report
  - ID: T-011
  - Requirement(s): FR-840–844, FR-866, FR-880–881, FR-890–891
  - depends_on: T-010
  - Scope: Feasibility first; COMMON_TRIPLET hats + CI or WITHHELD;
    pair-specific estimates labeled as such; R_diagnostic; label
    `DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE`.
  - Completion evidence: `ppe_effect_report.json` + `final_report.md`

- [ ] 12. Evidence index / sealing
  - ID: T-012
  - Requirement(s): FR-890–891, NFR-800–805
  - depends_on: T-011
  - Scope: Hash chain; README sync; no T-445 relabel.
  - Completion evidence: `evidence_index.json`

---

## Formal preconditions (T-010)

1. T-008 all mandatory PASS  
2. T-009 overall=true  
3. keras SHA match  
4. C1 npz SHA match and complete keys for all formal (voyage,t0)  
5. T_eval table complete  
6. causality ledger verification PASS  
7. access ledger FORMAL=0 / test=0 / OOD=0 at freeze  
8. `.slx` SHA unchanged vs T-445 baseline `190de549…`

---

## STOP conditions (escalate, do not “fix” with val outcomes)

- Legal C1 stored target cannot be bound  
- Causal 300 s H1 policy broken  
- Required K/F channel undefendable  
- T_eval not arm-independent  
- Freeze FAIL  

---

## Verification checklist

- [x] D1–D8 mapped into task Scope  
- [x] Qualification/freeze before formal  
- [x] No 2% GO; no 1% oracle replica Gate  
- [x] IID/OOD sealed  
- [x] st_edge_ai_upload not in any task Files/areas  

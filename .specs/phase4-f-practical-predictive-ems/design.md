# Phase 4-F Practical Predictive EMS — Design

Spec: `phase4-f-practical-predictive-ems`  
Status: **design-approved**  
Created: 2026-09-15  
Approved: 2026-09-15（Design Gate **APPROVED WITH REQUIRED CORRECTIONS** D1–D8 applied; no second Design Gate）  
Requirements: `./requirements.md`（CLOSED / APPROVED, RG1–RG9）  
Brainstorm: `./brainstorm.md`（CLOSED / APPROVED; not reopened）

This Design is **not** an implementation license. Tasks Gate is next.

Decision IDs: **DGN-PPE-01 … DGN-PPE-12**.

No listed STOP condition was triggered: C1 canonical target is bound; C2 300 s
history uses a common causal warm-up; K/F channels are defended from frozen
Dataset-v3 contracts; T_eval is arm-independent.

---

## D1–D8 closure mapping

| ID | Decision |
|---|---|
| D1 | C1 = perfect knowledge of **frozen Dataset-v3 reference-target** `y_k`, not own future closed-loop `P_load_total`. Same target semantics as C2. |
| D2 | Formal C1 = **stored** npz lookup only. Missing key → fail-closed. Reconstruction only in QUALIFICATION parity. |
| D3 | **Option B**: common C0-persistence warm-up on `[0, 300)` for all C* arms; forecast-source differentiation at `t0 ≥ 300 s`. No zero-pad / no future-as-history. |
| D4 | `T_eval(voyage)` = frozen Dataset-v3 native-stream `t_end`. Same t_start/T_eval/IC/mission for all arms. |
| D5 | Study-level identity only on **COMMON_TRIPLET_DECOMPOSITION** mask. |
| D6 | Conditional hat = equal-stratum mean of tier means; bootstrap 1/3 per tier. |
| D7 | K/F defended: F = causal issue-time generator per voyage — formal corpus mixes **GOOD_V1 (24) / DEGRADED_V1 (24)**, both `CAUSAL_GENERATOR` with `source_cutoff ≤ issue_time` (GOOD: =t0; DEGRADED: =t0−1800 s); dual-stored vs ORACLE, `ORACLE_DERIVED` forbidden. Ledger: `config/phase4f_ppe_causality_ledger.json`. |
| D8 | Segmented checkpoint identity binds arm, voyage, t0, hashes listed below; mismatch refused. |

---

## DGN-PPE-01 / D1 — C1 information scope

Canonical name: `PERFECT_INFORMATION_CONTROLLER_REFERENCE`.

C1 knows the **frozen Dataset-v3 reference load target** `y_k = E_k/10` [kW]
for `(voyage_id, t0)` — the same prediction-target semantics C2 is trained to
emit (q50 of that target).

C1 does **not** know its own counterfactual future closed-loop realized
`P_load_total`. Live Model-E `P_load_total` may be controller-dependent
(speed tracking / shortfall).

**C1−C2** = effect of replacing perfect Dataset-v3 **target information** with
REAT-T prediction **inside the same C\***.

---

## DGN-PPE-01b / D2 — Formal C1 stored target only

Formal C1 SHALL read precomputed target bytes from

`datasets_v3/formal/windows_v3_formal.npz`

keyed by `(voyage_id, t0)` (anchor grid). SHA256
`203dc92df1f1aad980480514ea6c1ef09c50299f5fc91274e11d63f5b2491497`.

**No runtime reconstruction** of formal C1 targets.

Missing required `(voyage_id, t0)` → **FAIL CLOSED** before/at that step.

`targets.py` reconstruction from the frozen native stream is **QUALIFICATION
only**, to prove stored vs recomputed parity (relative error bound to be
machine-tested, target `<1e-9` as Dataset-v3 crosscheck).

Formal evidence records target-store SHA and per-window identity.

---

## DGN-PPE-02 / D3 — 300 s C2 history (Option B)

Dataset-v3 anchors: `t0 ∈ {t_start+300, …, ≤ t_end−300}` (`windowing.py`).
There is **no** genuine pre-voyage ≥300 s history before mission `t=0`.

**Frozen policy (Option B).**

All C* arms (C0, C1, C2) use the **same** causal warm-up on **[0, 300) s**:

- controller = C*;
- ForecastWindow = **C0 persistence** (FR-821);
- plant/history semantics identical.

At `t0 ≥ 300 s`, forecast-source differentiation begins (C0 persistence /
C1 stored target / C2 REAT-T q50).

H1 at the first differentiated update is 300 s of **arm-local** observed
history accumulated under the common warm-up (deterministic ⇒ common plant
state at t=300).

**Forbidden:** zero-pad; repeat future samples backward; post-t0 samples as
history; Dataset-v3 generation-run history masquerading as this arm’s H1.

**Interpretation.** Forecast-source treatment is identified **after** the
common 300 s warm-up. Whole-voyage J_FEC still includes the warm-up; that
segment is common across C0/C1/C2 under determinism.

C* Δλ updates that consume C1/C2 windows are exactly the Dataset-v3 anchors
`{t_start+300, t_start+310, …, T_fcst}` where `T_fcst` = the voyage's **last
anchor** (`≤ T_eval − 300`; equality when `(T_eval − t_start − 600)` is a
multiple of 10 s). After `T_fcst` until `T_eval`, C1/C2 hold last Δλ (ZOH)
— no unregistered future target time. **C0 keeps updating** every 10 s in the
tail (FR-821 persistence is always causal-legal); only C1/C2 hold.

---

## DGN-PPE-02b / D4 — Canonical T_eval

For each formal `voyage_id`:

| Symbol | Definition |
|---|---|
| `t_start` | 0 (mission start of the frozen fixture) |
| `T_eval` | frozen Dataset-v3 native-stream `t_end` (last 1 Hz right endpoint) |
| `T_warm` | 300 s |
| `T_fcst` | last Dataset-v3 anchor of the voyage (`≤ T_eval − 300`) |

All arms share t_start, T_eval, ICs, mission reference. `K_remain` / partial
bins derive from **canonical T_eval**, not arm-specific completion time.

Sim `StopTime = T_eval`. Do not extend C1/C2 past stored anchors. Do not
silently shorten T_eval if an arm finishes early.

If Voyage_Stop_Logic stops before `T_eval` by more than
`task_complete.abs_t_end_tol_s` (1 s), **HARD_INFEASIBLE** (task_complete).
If the ship reaches distance exactly at T_eval, that is the frozen mission
end. Holding last Δλ on `(T_fcst, T_eval]` is the already-frozen terminal
control behavior (ZOH), not a new action.

A freeze-time table `T_eval(voyage_id)` SHALL be hashed (Tasks T-001).

---

## DGN-PPE-03 — C* algorithm

Unchanged from Requirements FR-810–815. SOC grid **0.005**. Outcomes = Model-E
logs. Q2/Q4/Q6 remain documented limitations.

---

## DGN-PPE-04 / D7 — C2 inference and K/F causality

Weights and hashes: Requirements FR-836. Delivery file
`experiments/predictor_v3/confirm/reatt/reatt_s0.keras`
SHA256 `d8da6af4667875c45de7de90383d2b6b750420f5af62c7dcdcb1e5997897f894`.

**Pipeline at t0 ≥ 300 s:** H1 from **this arm’s** last 300 s @ 1 Hz
(`formal_norm_v3`); KF from frozen Dataset-v3 `kf_snapshots` row
`issue_time_s == t0` (`predictor_norm_ext_v1`); q50 → ForecastWindow.

**Causality verdict: DEFENDED (no STOP).**

Evidence (frozen contracts, not controller outcomes):

| Channel | Class | Evidence |
|---|---|---|
| H1 × 11 | `observed_history` | registry INPUT_H1; arm-local logs t≤t0 |
| `v_plan_mps` | known-future plan | registry INPUT_K `fixture:planned_alongtrack_sog`; producer “计划语义”; 禁 realized v_ref |
| `s_plan_m` | known-future plan | 前向积分；禁 s_actual,future |
| `h_chart_m` | chart/static | 海图已知量 |
| `P_hotel_plan_kW` | planned hotel | 仅 planned component |
| `u_fcst_mps` | issue-time forecast | formal corpus mixes **GOOD_V1 (24 voyages)** / **DEGRADED_V1 (24)** (pass2 ledger `arms_forecast`): GOOD_V1 `u_fcst(t0+h\|t0)=u_reach+(u_obs(t0)−u_reach)·exp(−h/900)`, `u_obs` OBSERVED-class at `source_cutoff=t0`; DEGRADED_V1 `u_obs(t0)=U(t0−1800)+b_voyage+η(t0)` with `source_cutoff=t0−1800` — both consume only ≤t0 information, both `CAUSAL_GENERATOR` (`forecast_issue.m` SHA256 embedded in every voyage provenance, machine-matched); pilot pure-persistence fixture is lineage only; `F_{t0}=g(I_{<=t0})`; **ORACLE counterpart** `u_actual_future_mps` dual-stored `oracle_only`; `ORACLE_DERIVED` **forbidden** |
| `wl_fcst_m` | issue-time forecast | GOOD_V1: voyage-constant `ΔWL` causal estimate (persistence-degenerate); DEGRADED_V1: same + voyage bias — both ≤t0; same dual-storage pattern vs `wl_actual_future_m` |

Ledger (channel-level, Design artifact):
`config/phase4f_ppe_causality_ledger.json`

Qualification SHALL re-check generator provenance records
`source_cutoff_time_s ≤ issue_time_s` on formal voyage KF metadata.

Do not silently zero. Do not replace/retrain REAT-T.

---

## DGN-PPE-05 — Hard-feasibility config

Unchanged: `config/phase4f_ppe_hard_feasibility.json`, object SHA
`fcef573d06e414bef8b64f10864def24ad769195d32362b1f57d0b312aac9def`.
TERMINAL_SOC_ADMISSIBILITY `|ΔSOC|≤0.005` HARD.

---

## DGN-PPE-06 — fuel_raw

Model-E `fuel_total_kg(end)` including **0.6 kg/start**. All arms same path.

---

## DGN-PPE-07 / D8 — Runner and checkpoint identity

C0/C1/C2: **one** segmented SimState runner, 10 s, resumable.

Checkpoint identity SHALL bind:

- arm; voyage_id; t0 / segment index;
- study config SHA; code commit SHA; Model-E `.slx` SHA; C* implementation SHA;
- ForecastWindow provenance/hash;
- hard-feasibility config SHA; corpus identity SHA;
- canonical T_eval identity;
- Predictor-v3 model/config/norm SHAs for **C2**, or explicit `N/A` for C0/C1.

Resume **refuses** any mismatch. Resumed run scientifically indistinguishable
from uninterrupted.

P0: continuous law=5 (contextual). StopTime = same T_eval.

Qualification: SimState equivalence on the C* runner (T-010 S20 PASS is
lineage, not a substitute).

---

## DGN-PPE-08 / D5–D6 — Statistics

**Seed:** `phase4f-t445-bootstrap-v1` (inherited; no T-445 effect observed).
RNG as `bootstrap_cluster.rng_from_seed`. B=10000. Percentile 2.5/97.5
`method="linear"`.

**Pairwise voyage estimands** keep FR-854 feasibility (defined iff required
arms feasible).

**COMMON_TRIPLET_DECOMPOSITION** (explicit analysis identity): the set of
voyages where **C0 and C1 and C2** are all feasible, all J_FEC finite, and
`J_C0 > 0` (FR-854 denominator guard). Study-level

`B_perfect_hat`, `B_predicted_hat`, `L_prediction_hat`

and `R_diagnostic`, and the claim
`B_perfect_hat = B_predicted_hat + L_prediction_hat`,
SHALL be computed **only on this common mask** (then aggregated by D6).

Pair-specific conditional estimates (e.g. C0 vs C1 on a larger set) MAY be
reported separately and SHALL NOT be algebraically combined across different
masks.

**FULL_COHORT_ESTIMATE:** common-triplet defined on all 48 voyages. Hat =
mean of 12 group effects (= equal-stratum because 4/4/4).

**CONDITIONAL_FEASIBLE_ESTIMATE:** common-triplet missing some voyages, but
each LOW/MID/HIGH has ≥3 of 4 groups with a defined group-level effect
(group effect = mean of that group’s defined voyages; report `n_defined`).

Hat:

`theta_hat = (mean_LOW + mean_MID + mean_HIGH) / 3`

each tier mean over **defined** group effects in that tier. **Do not**
down-weight a tier because it has 3 vs 4 defined groups.

Bootstrap (conditional): independently resample defined groups with
replacement **within each tier** (draw size = number of defined groups in
that tier); compute three tier means; average with equal 1/3; repeat B=10000.

If any tier has <3 defined groups: `WITHHELD_INSUFFICIENT_FEASIBILITY` — no
overall hat/CI.

Always print per-arm feasibility counts/identities first.

Zero-safe voyage-triplet identity remains FR-842.

`R_diagnostic = B_predicted_hat / B_perfect_hat` on COMMON_TRIPLET hats only
(FR-880–881).

---

## DGN-PPE-09 — Qualification (additions)

Previous items retained. Add non-result-bearing tests:

1. stored C1 target vs `targets.py` reconstruction parity;
2. C1 formal lookup fail-closed on missing key;
3. C2 warm-up causality (no future-as-H1; no zero-pad);
4. T_eval equality across arms;
5. pairwise-mask vs COMMON_TRIPLET statistics;
6. equal-tier conditional estimator/bootstrap;
7. channel-level K/F ledger vs registry/producer/provenance;
8. checkpoint identity mismatch refusal.

---

## DGN-PPE-10 — Reporting label

`DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE`.

---

## DGN-PPE-11 — Sealing

IID sealed. OOD not for selection. `.slx` unchanged.
`st_edge_ai_upload/` OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH.

---

## DGN-PPE-12 — ForecastWindow after D1–D4

Unchanged schema (FR-820). C0 persistence uses **this arm’s** completed-bin
mean (causal). C1 uses **stored Dataset-v3 y_k**. C2 uses REAT-T q50 of the
**same y_k semantics**. C* does not branch on `source_id`.

---

## Requirements Traceability

| Design decision | Gate correction | Requirements / AC |
|---|---|---|
| DGN-PPE-01 (C1 scope) | D1 | FR-803, FR-822, AC-802 |
| DGN-PPE-01b (stored target only) | D2 | FR-822, AC-802; npz SHA `203dc92d…` |
| DGN-PPE-02 (Option-B warm-up) | D3 | FR-820–825, AC-801; `windowing.py` anchor contract |
| DGN-PPE-02b (canonical T_eval) | D4 | FR-850 `task_complete`; freeze table T-001 |
| DGN-PPE-03 (C* freeze) | — | FR-810–815, AC-804 |
| DGN-PPE-04 (K/F causality) | D7 | FR-830–836, AC-811; ledger `phase4f_ppe_causality_ledger.json` |
| DGN-PPE-05 (HF config) | — | FR-850–857, AC-813/814; object SHA `fcef573d…` |
| DGN-PPE-06 (fuel_raw) | — | FR-840/840a, AC-805 |
| DGN-PPE-07 (runner/checkpoint) | D8 | FR-871.8, FR-872 |
| DGN-PPE-08 (statistics) | D5, D6 | FR-842–844, FR-854, FR-860–866, FR-880–881, AC-806/809 |
| DGN-PPE-09 (qualification additions) | §12 ruling | FR-871, AC-800–814 |
| DGN-PPE-10/11 (label/sealing) | — | FR-890–891, NFR-800–805 |
| DGN-PPE-12 (ForecastWindow) | D1–D4 | FR-820–825, AC-803 |

---

## Testing Strategy

Non-result-bearing qualification **before** any formal run (FR-871 +
DGN-PPE-09 items 1–8): determinism; C0 leakage; C1 stored-target parity and
fail-closed; C2 warm-up causality; T_eval arm-equality; pairwise-mask vs
COMMON_TRIPLET statistics on synthetic tables; equal-tier bootstrap; K/F
ledger vs registry/producer/provenance; checkpoint identity mismatch refusal;
SimState equivalence; FEC reconstruction; HF instrumentation; predictor
checksums; access-ledger sealing; estimand identity; R_UNDEFINED.

Execution order (tasks.md DAG): T-001…T-008 qualification → T-009
pre-execution freeze (`overall=true`) → T-010 formal 48-voyage P0/C0/C1/C2 →
T-011 effect estimation → T-012 sealing. **No formal controller outcome may
be observed before T-009 PASS.**

---

## Open (operational, not STOP)

1. Keras file is local `experiments/`; freeze fail-closed if missing/mismatch.
2. Freeze-time extract of `T_eval(voyage_id)` table from Dataset-v3 streams
   (Tasks T-001). Geometry is frozen here.

No remaining D1–D8 STOP condition.

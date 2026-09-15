# Phase 4-F Practical Predictive EMS — Design

Spec: `phase4-f-practical-predictive-ems`  
Status: **design-draft** — **NOT APPROVED** (Design Gate pending)  
Created: 2026-09-15  
Requirements: `./requirements.md`（**requirements-approved**, RG1–RG9）  
Brainstorm: `./brainstorm.md`（CLOSED / APPROVED; not reopened）

This Design resolves the Requirements Gate design obligations **without**
observing formal P0/C0/C1/C2 outcomes. It is **not** an implementation license.

Decision IDs: **DGN-PPE-01 … DGN-PPE-11**.

---

## DGN-PPE-01 — C1 arm-independent truth channel

**Decision.** C1 `ForecastWindow.Pbar` SHALL be the frozen Dataset-v3 target
`y_k = E_k/10` [kW] for `(voyage_id, t0)` from the **dataset-generation
native stream**, not from the live C* closed-loop `P_load_total`.

**Provenance.**

| Item | Identity |
|---|---|
| Target definition | `dataset_v3/targets.py` (`y_trajectory`; FR-476) |
| Native channel | `P_load_total_kW` on the **frozen voyage stream** used to build Dataset-v3 |
| Window store | `datasets_v3/formal/windows_v3_formal.npz` SHA256 `203dc92df1f1aad980480514ea6c1ef09c50299f5fc91274e11d63f5b2491497` |
| Manifest payload | `ee285091027a5bda50652832dbf7ba780302feca8897b9195715570a1ea7d712` |
| Pipeline invariant | “targets.py 只消费 native stream”；禁从 1 Hz 降采样反算 |

**Why live closed-loop P_load is illegal.** Model-E
`P_load_total = max(P_prop_actual,0)+P_hotel`. `P_prop_actual` can change if
EMS/PMS/Safety fail to hold speed. Using the arm’s own future realization as
C1 truth would be `C1 action → future load → C1 ForecastWindow`.

**Proof of arm-independence (to be machine-tested, AC-802).** For a fixed
`(voyage_id, t0)`, C1 Pbar is a lookup/recompute from the frozen stream /
npz window. It does not read the live plant’s future. Two C1 assemblies that
are given different fake live future loads MUST emit identical Pbar; only
`K_remain` / `bin_duration_s` shrinking metadata MAY differ.

If the npz window is missing for an exact t0, recompute `y_trajectory` from
the same frozen native `(t, P_load_total_kW)` series. Do not fall back to
live logs.

**STOP condition (Requirements).** If this frozen channel cannot be bound at
pre-execution freeze (missing npz **and** missing native stream), Design
implementation SHALL stop and escalate. At this Design draft, the channel is
identified and is structurally arm-independent.

---

## DGN-PPE-02 — ForecastWindow assembly and timing

Canonical object after source-specific work (FR-820):

```text
ForecastWindow
  t0_s              : 10 s grid, t0 ∈ {0,10,20,…} ∩ [0, T_end)
  Pbar_kW[1:K]      : bus-side kW, K ≤ 30
  bin_duration_s[k] : 10, except possibly last bin < 10 (duration-weighted)
  K_remain, T_end_s
  source_id         : C0_PERSISTENCE | C1_TRUTH | C2_REATT_Q50
  provenance        : hashes of stream/model/norm
```

**C0 (FR-821).** t0 ≥ 10 s: mean of **this arm’s** realized bus
`P_load_total` on completed **[t0−10, t0)** (causal past, not future truth).
t0 = 0: `P_load_total(0)`. Repeat scalar onto all remaining bins. Samples
with t ≥ t0 SHALL NOT enter the mean.

**C1.** Lookup DGN-PPE-01. Truncate to shrinking horizon.

**C2.** DGN-PPE-04 q50, de-normalized kW, truncate to shrinking horizon.

C* SHALL NOT branch on `source_id`.

**Boundary.** Control update at time t0 uses information with timestamps
`< t0` for C0 completed-bin means, `= t0` only for the t0 < 10 s current
sample case, and C1/C2 future bins that are **forecast/truth inputs** (C1 from
frozen dataset, C2 from predictor). Plant integration of Δλ_k holds over
`[t0, t0+10)`.

---

## DGN-PPE-03 — C* algorithm (Requirements freeze)

Reuse T-445 B2 + Amendment E `realize_bin` as C*.

| Item | Frozen value |
|---|---|
| law | 6 |
| Δλ | [−0.10, +0.10] × 0.005, n=41 |
| decision / horizon | 10 s / 300 s / K=30 |
| SOC grid | **0.10:0.005:0.95**, n=171 |
| k_eq_E | 0.5457 kg/kWh |
| w_term | 1.17e5 kg/SOC² |
| λ_eff | max(λ_raw, 0.43), no upper cap |
| K_S | 1/30 |
| surrogate | `realize_bin` dt_internal=1 s |
| diagnostic grid | 0.0025 only, nonformal |

T-445 Q2 (rel=0.0153 on 0.01 vs 0.005) is a **limitation**, not a Gate.
Q4/Q6 1% replica failures are limitations. Outcomes = Model-E logs.

---

## DGN-PPE-04 — C2 online inference and causality

**Weights.** Canonical delivery file:

`experiments/predictor_v3/confirm/reatt/reatt_s0.keras`  
SHA256 `d8da6af4667875c45de7de90383d2b6b750420f5af62c7dcdcb1e5997897f894`  
`seed_index=0`, `seed_int=1115655093`  
HPO trial id `reatt-t018` is selection identity; **delivery = confirm s0**.

Full hash table: Requirements FR-836.

**Pipeline at t0 (no dropout):**

1. H1 `[300,11]`: last 300 s @ 1 Hz of **this arm’s** observed history
   (causal), `formal_norm_v3` transform.
2. KF `[30,6]` from **frozen Dataset-v3** issue-time tensors for that
   voyage/t0, `predictor_norm_ext_v1` transform. Channels (confirm s0):
   K-plan `v_plan_mps, s_plan_m, h_chart_m, P_hotel_plan_kW`;
   F-forecast `u_fcst_mps, wl_fcst_m`.
3. Forward REAT-T; take q50; inverse-norm to kW → ForecastWindow.

**Causality classification (prospective; freeze fills the table):**

| Feature class | Channels | Class |
|---|---|---|
| H1 history | P_load_total, v_ship, accel, n_prop, P_hotel, v_current, water_depth, P_prop_demand, ctrl_mode, v_ref, manual_power | `observed_history` / current measurable (t ≤ t0) |
| K | v_plan, s_plan, h_chart, P_hotel_plan | `legitimately_known_future_exogenous_plan` **iff** Dataset-v3 issue-time KF contract (mission plan available at t0). Assumption **documented**. |
| F | u_fcst, wl_fcst | same: issue-time exogenous forecast in Dataset-v3 KF; **documented** as available to the controller at t0 (charted current / water-level forecast). |
| Live future P_load | — | `prohibited_future_information` |

If freeze cannot defend K/F availability at t0, those channels SHALL be
zeroed as in the H1 ablation **only after an approved Design amendment**.
Default: use frozen KF as Dataset-v3 legal issue-time inputs.

Never read `st_edge_ai_upload/`.

---

## DGN-PPE-05 — Study-local hard-feasibility config

**Created at Design draft** (config freeze, not controller code):

Path: `model/simulink_modelE_(Phase4_Physics)/config/phase4f_ppe_hard_feasibility.json`

| Record | Value |
|---|---|
| T-445 source | `config/phase4f_t445_config.json` |
| T-445 file SHA256 | `34b68c41a56db23394f32632fdbc04a540cdc1d3181d09ea52c44551a026f820` |
| T-445 `hard_feasibility` object SHA256 | `fcef573d06e414bef8b64f10864def24ad769195d32362b1f57d0b312aac9def` |
| Copied object SHA256 | `fcef573d06e414bef8b64f10864def24ad769195d32362b1f57d0b312aac9def` (equal) |
| Copied file SHA256 | `c7e8cb41879dc8ba5d1df28a0de8afc2969278dc61712a8cc9fa05f5904f7b10` |

`TERMINAL_SOC_ADMISSIBILITY` = `abs(SOC_end-SOC0)<=0.005` =
`terminal_soc.abs_dsoc_max`. HARD. Separate from `[0.10,0.95]` bounds.

No retune from outcomes.

---

## DGN-PPE-06 — fuel_raw

Canonical: Model-E `logsout.fuel_total_kg(end)` from `10_Fuel_Consumption`.

Components: ∫ FuelModelE(P_DG1,state1)+FuelModelE(P_DG2,state2) dt,
including **0.6 kg/start** (`DG_C_start_kg`) during STARTING+SYNC.
OFF/COOLDOWN = 0.

All arms use this path. Surrogate fuel is internal to C* only.

J_FEC = `fuel_raw_kg + (SOC0-SOC_end)*1071.36*0.5457`.

---

## DGN-PPE-07 — Formal runner (single architecture)

**C0/C1/C2:** **segmented SimState**, 10 s segments, same runner.

Rationale: C* must apply a new Δλ at each t0 from current plant state
(FR-871.7). T-010 SimState continuous-vs-segmented already **PASS** (nseg=552)
on Model E. Qualification SHALL re-run SimState identity on the C* runner
before formal corpus.

Resumable checkpoints: `simstate_mgr` identity includes arm, voyage_id, t0,
config hash, ForecastWindow provenance. Resume SHALL NOT change scientific
identity.

**P0:** single continuous law=5 run (architecture differs; contextual). Same
Model-E plant, same voyage IC, same feasibility/FEC accounting.

Do not mix surrogate J into any arm’s reported J_FEC.

---

## DGN-PPE-08 — Stratified bootstrap

**Seed (inherited, not renamed):** `phase4f-t445-bootstrap-v1`  
Provenance: T-445 DGN-F-14; **no formal T-445 effect result was observed**.
Keep the string.

**RNG:** SHA256(seed)[:16] little-endian → `numpy.random.SeedSequence` →
Generator (same as `python/t445/bootstrap_cluster.py` `rng_from_seed`).

**Corpus:** 12 groups = 4 LOW + 4 MID + 4 HIGH (T-445 initial 9 + reserve 3).
Identity `245fa307cf36143259672f93cf801587eb701d721028a735b3cd91c97c42f115`.

**Group effect:** mean of defined voyage-level B_* (or ΔJ_*) in that group.
Record `n_defined` / 4.

**Study-level hat (primary):** mean of the 12 group effects when
`FULL_COHORT_ESTIMATE`; mean of **defined** group effects when
`CONDITIONAL_FEASIBLE_ESTIMATE`.

**Coverage (FR-866):** each stratum needs ≥3 of 4 groups defined, else
`WITHHELD_INSUFFICIENT_FEASIBILITY` (no CI, no hat claim).

**Bootstrap (only if not withheld):**

- FULL_COHORT: independently, for each tier, draw **4** groups **with
  replacement** from that tier’s 4 groups; compute mean of the 12 drawn
  group effects; repeat B=10000.
- CONDITIONAL: for each tier, let n_t = number of defined groups in that
  tier (3 or 4); draw **n_t** with replacement from the defined pool;
  mean of drawn effects; B=10000.

**CI:** 2.5 and 97.5 percentiles, NumPy `percentile(..., method="linear")`.

**Secondary:** median of defined group effects (point only).

**Identity residual (percentage points):**
`abs(B_perfect - B_predicted - L_prediction) <= 1e-10 + 1e-9*max(|Bp|,|Bpred|,|L|)`
on each finite voyage triplet and on the hats when all three hats exist.

**R_diagnostic** = `B_predicted_hat / B_perfect_hat` at study level only
(FR-880–881).

---

## DGN-PPE-09 — Qualification plan (pre-formal)

Nonformal / synthetic only. No val-corpus C* outcomes.

1. Deterministic two-run C* toy.
2. C0 leakage: future samples do not change persistence.
3. C1 arm-independence (AC-802).
4. ForecastWindow schema + C* config dump parity (except source_id/Pbar).
5. Δλ bounds; SOC grid n=171.
6. Receding init from plant.
7. SimState equivalence on C* runner.
8. FEC identity on logs; fuel_raw path shared.
9. HF config object SHA equality (AC-813); TERMINAL_SOC_ADMISSIBILITY.
10. C2 keras SHA256 match; causality table filled; no `st_edge_ai_upload`.
11. Access ledger: FORMAL=0, test/OOD scientific=0.
12. Estimand identity and R_UNDEFINED unit tests (AC-806/809).

Document T-445 Q2/Q4/Q6 as limitations in the freeze record.

---

## DGN-PPE-10 — Estimand status and reporting order

Always print per-arm feasibility counts and voyage_ids first.

Then estimand status ∈ {FULL_COHORT_ESTIMATE, CONDITIONAL_FEASIBLE_ESTIMATE,
WITHHELD_INSUFFICIENT_FEASIBILITY}.

Conditional text SHALL say the effect is **conditional on feasible voyages**,
not the unconditional 48-voyage effect.

Label: `DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE`.

---

## DGN-PPE-11 — Sealing

IID test sealed. OOD not for selection. Dataset-v3 bytes unchanged.
`.slx` unchanged. `st_edge_ai_upload/` OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH.

---

## Open Design items (non-scientific)

1. Keras weights are local (`experiments/`); freeze fails closed if missing.
2. If Dataset-v3 KF issue-time availability for F-channels is rejected at
   freeze audit, **escalate** (do not silently zero).
3. P0 runner is continuous; C* is segmented — documented architecture
   difference, not a C0/C1/C2 parity break.

No remaining obligation that requires observing formal C0/C1/C2 outcomes.

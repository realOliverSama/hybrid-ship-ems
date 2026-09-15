# Phase 4-F practical predictive-EMS — Brainstorm

Spec: `phase4-f-practical-predictive-ems`  
Status: **brainstorm-approved**  
Gate: **APPROVED WITH REQUIRED REVISIONS** (R1–R10 applied; no second Brainstorm Gate)  
Approved: 2026-09-15  
Predecessor: T-445 CLOSED `GATE_VALIDITY_FAIL`

This file is the frozen Brainstorm after the Gate ruling. Requirements translate these
decisions; they do not reopen them.

---

## Intent

T-445 tried to certify a receding-horizon Δλ controller as a **strict
trajectory-oracle upper reference** versus scalar E300. That certification
failed. No formal 36-voyage B1/B2 corpus was observed.

This route asks a **different** question:

> Under **one fixed practical** predictive supervisory controller C*, how much
> practical Model-E control benefit is obtained from no future information,
> perfect 30-bin future trajectory information, and Predictor-v3 trajectory
> information?

Primary attribution is **C0 / C1 / C2**. P0 is contextual because its
controller architecture differs.

C1 canonical name: `PERFECT_INFORMATION_CONTROLLER_REFERENCE`.

**Forbidden** for C1: theoretical upper bound; global optimum; trajectory
oracle upper bound; maximum trajectory-information value.

---

## Frozen arms (R1)

| Arm | Forecast | Controller | Role |
|---|---|---|---|
| **P0** | none | frozen law=5 SOC-Adaptive ECMS | external conventional **context only** |
| **C0** | strictly causal persistence (R2) | C* | no future information |
| **C1** | exact legal 30×10 s future trajectory truth | C* | `PERFECT_INFORMATION_CONTROLLER_REFERENCE` |
| **C2** | frozen Phase-4E REAT-T q50 | C* | learned-prediction realization |

C* = current practical Δλ receding-horizon controller (T-445 B2 + Amendment E
realization surrogate), evaluated on the **real Model-E plant**.

T-445 1% oracle-reference failure does **not** prohibit this practical use.

---

## R1–R10 closure mapping

| ID | Decision frozen here |
|---|---|
| R1 | Claim boundary + P0/C0/C1/C2 identities; C1 naming ban |
| R2 | C0 persistence: completed [t0−10, t0) mean; t0<10 s uses P_load_total(t0) |
| R3 | C* algorithm freeze including SOC grid **0.005**; 0.0025 diagnostic only; Q2 retired as Gate |
| R4 | Forecast-source parity + single `ForecastWindow` interface |
| R5 | J_FEC primary; B_perfect / B_predicted / L_prediction; no 2% GO |
| R6 | 12 groups / 48 voyages; no sequential reserve; mean group effect + bootstrap CI |
| R7 | Practical qualification (not 1% replica); Model-E outcomes |
| R8 | REAT-T t018/seed0/q50 + t0 causality audit |
| R9 | No pair-delete; infeasibility is an outcome; no fabricated J_FEC |
| R10 | R ratio diagnostic only; `st_edge_ai_upload/` OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH |

---

## Q1 — Controller identity — CLOSED (R3)

C* is the current practical Δλ receding-horizon controller.

Frozen with C*:

- law=6 core;
- Δλ range/grid (T-445 frozen: [−0.10, +0.10] × 0.005, 41 levels);
- 10 s decision interval;
- 300 s / 30-bin horizon;
- **SOC grid = 0.005** (0.10:0.005:0.95);
- k_eq_E = 0.5457 kg/kWh;
- w_term = 1.17e5 kg/SOC²;
- frozen generator/PMS realization surrogate;
- frozen terminal treatment;
- frozen Safety/PMS interfaces.

Do not retune from C0/C1/C2 validation outcomes.

SOC grid 0.0025 is a **predeclared numerical-robustness diagnostic only**.

The old T-445 Q2 continuous-approximation Gate is **retired**. Document Q2
sensitivity (rel=0.0153 on 0.01 vs 0.005) as a **limitation/diagnostic**, not
as a live Gate.

Distinction retained: failed oracle-reference fidelity ≠ prohibited practical
plant evaluation.

---

## Q2 — C0 persistence — CLOSED (R2)

At control update t0:

- if t0 ≥ 10 s: persistence value = mean realized **bus-side** load on the
  most recently **completed** interval **[t0−10 s, t0)**; repeat that scalar
  into all 30 future 10 s bins;
- if no completed 10 s interval exists: use causal measured
  `P_load_total(t0)` as the repeated value.

Do **not** use the eventual mean of an incompletely observed current bin.
C0 contains **no future information**.

---

## Q3 — C1 truth — CLOSED (R1, R4)

C1 forecast = exact legal Dataset-v3-compatible 30×10 s future trajectory
truth (10 s bin means of realized bus-side load; shrinking horizon;
duration-weighted partial last bin). No 1 Hz privileged future inside C*.
Same `ForecastWindow` interface as C0/C2.

---

## Q4 — C2 Predictor-v3 — CLOSED (R8)

Frozen winner: **REAT-T t018 / seed0 / q50**, subject to repository artifact
verification. Do not retrain or select another predictor.

Before formal execution: audit every C2 input at t0 into
{observed history, current measurable state, legitimately known future
exogenous plan, prohibited future information}. Nothing unavailable at t0
may enter C2.

Known-future route/speed/mission features are permitted **only if** they are
genuinely available at t0 and that assumption is documented.

Freeze normalization, history handling, inference cadence, horizon/end
policy, model checksum, deterministic inference.

---

## Q5 — Parity — CLOSED (R4)

C0/C1/C2 share controller code, objective, action space, SOC/grid, cadence,
horizon, terminal cost, plant, PMS, Safety, ICs, FEC, hard-feasibility.
**Only the forecast source differs.**

One canonical `ForecastWindow` after all source-specific assembly /
de-normalization. All three arms enter C* through it.

---

## Q6 — Outcomes — CLOSED (R5, R10)

Primary outcome: **J_FEC**. Raw fuel and terminal SOC are secondary/diagnostic.

For every paired voyage (feasible triplet; see R9):

```text
B_perfect   = 100 * (J_C0 − J_C1) / J_C0
B_predicted = 100 * (J_C0 − J_C2) / J_C0
L_prediction= 100 * (J_C2 − J_C1) / J_C0
```

Identity: `B_perfect = B_predicted + L_prediction`.

Also report absolute kg-FEC differences.

**No replacement 2% GO/NO-GO.** This is an **effect-estimation** study.

Realization ratio `R = B_predicted / B_perfect` is **DIAGNOSTIC ONLY**,
undefined when the denominator is zero, near-zero, negative, or otherwise
unsuitable. Guard is prospective (Requirements), not result-selected.

---

## Q7 — Corpus / statistics — CLOSED (R6)

Reuse frozen validation identities (T-445 observed no formal controller
outcomes).

**Retire sequential reserve.** Formal corpus is frozen prospectively as:

- **12 split_groups / 48 voyages**
- previous initial 9 groups + previously frozen 3 reserve groups
- one fixed evaluation corpus; **no result-dependent expansion**

Reuse: paired design; LOW/MID/HIGH stratification; split_group resampling
unit; B=10000; frozen deterministic bootstrap seed
`phase4f-t445-bootstrap-v1`.

Primary point estimator: **mean of split_group-level paired effects**.
Report 95% percentile stratified group-bootstrap CI.
Also report **median** split_group effect as robust secondary.

No CI-crossing GO/NO-GO rule.

IID test remains sealed. OOD remains separate and SHALL NOT be used for
controller selection.

---

## Q8 — Qualification — CLOSED (R7)

Do **not** inherit T-445 1% proxy-vs-plant fuel replica as a Gate.
Final scientific outcomes are measured on **Model E**.

Mandatory pre-execution qualification: determinism; no hidden future leakage;
ForecastWindow contract; C0/C1/C2 code parity; identical non-forecast config;
action bounds; current-state init; SimState/checkpoint reproducibility;
hard-feasibility instrumentation; FEC accounting; C2 causal features; no
test/OOD access; no result-bearing tuning.

Retain T-445 Q4/Q6/Q2 failures as documented methodological limitations.
Finer-grid sensitivity may be reported as a robustness diagnostic and must
not tune the formal controller after result observation.

---

## Q9 — Decomposition — CLOSED (R5, R10)

Primary: B_perfect, B_predicted, L_prediction (and kg counterparts).
R is diagnostic only with a prospective undefined-guard.

---

## Q10 — Deployment — CLOSED

Model-E scientific comparison first. STM32N6 later. N6 constraints SHALL NOT
reshape C0/C1/C2 unless separately approved.

`st_edge_ai_upload/` is **OUT_OF_SCOPE_USER_OWNED_UNTRACKED_DO_NOT_TOUCH**.

---

## Risks / limitations (retained)

- Value is **within C***, not over all possible controllers.
- T-445 Q2 SOC-grid sensitivity is a documented limitation of the discrete
  algorithm (now frozen at 0.005).
- Amendment E 1 s surrogate vs 0.01 s plant remains inside C* cost only.
- Small n=12 groups: report CI; do not over-claim.
- C2 requires verified frozen REAT-T artifacts in-repo; never
  `st_edge_ai_upload/`.

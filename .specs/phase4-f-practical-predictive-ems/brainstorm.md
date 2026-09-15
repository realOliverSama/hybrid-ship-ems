# Phase 4-F practical predictive-EMS — Brainstorm

Spec: `phase4-f-practical-predictive-ems`  
Status: **brainstorm-draft** — **NOT APPROVED**  
Created: 2026-09-15  
Predecessor: T-445 CLOSED `GATE_VALIDITY_FAIL`

Do not treat any recommendation below as a frozen requirement.

---

## Intent

T-445 tried to certify a receding-horizon Δλ controller as a **strict
trajectory-oracle upper reference** versus scalar E300, then infer whether
perfect 300 s trajectory information is worth ≥2%. That certification failed
(Q4/Q6 fuel replica >1%). No formal 36-voyage B1/B2 corpus was observed.

This route asks a **different** question:

> Under one fixed predictive supervisory-controller architecture, how much
> practical Model-E control benefit is obtained from no future information,
> perfect 30-bin future trajectory information, and Predictor-v3 trajectory
> information?

Separate:

1. controller-architecture value (P0 vs C0, contextual);
2. perfect-information value **within** that architecture (C1 − C0);
3. learned-prediction realization value (C2 − C0);
4. prediction-loss gap (C1 − C2).

Forbidden language for C1: theoretical upper bound, global optimum,
trajectory-information upper bound. Use
`PERFECT_INFORMATION_CONTROLLER_REFERENCE`.

---

## Recommended arm structure

See `architecture.md`.

| Arm | Forecast | Controller |
|---|---|---|
| **P0** | none | frozen law=5 SOC-Adaptive ECMS |
| **C0** | no-future construction (Q2) | shared C* |
| **C1** | Dataset-v3 30×10 s truth | shared C* |
| **C2** | Predictor-v3 q50 30-bin | shared C* |

Recommended C\* = existing T-445 receding-horizon Δλ controller (B2 code +
Amendment E `realize_bin` cost), **renamed**: practical predictive supervisor,
not an oracle bound. Final outcomes = Model-E logs.

---

## Q1 — Controller identity

**Question.** What exact predictive supervisory controller is shared by C0/C1/C2?

**Recommendation.** Reuse the T-445 receding-horizon Δλ controller as C\*.

Distinguish:

| Claim | Status |
|---|---|
| Accurate enough to **certify an oracle upper-bound** (proxy vs plant ≤1%) | **Failed** (T-010 Q4/Q6) |
| Good enough to **operate and be evaluated on the real Model-E plant** | **Open, recommended to try** |

Do not silently equate them. Plant measurement replaces surrogate qualification
as the scientific outcome. Surrogate remains an internal cost model.

**Unresolved.** Whether C\* should keep `w_term` / SOC-grid 0.01 / Δλ ±0.10
unchanged. Recommendation: keep frozen unless a **prospective** (pre-result)
change is approved.

---

## Q2 — C0 no-future definition

**Question.** What should C0 receive?

**Recommendation (pre-result).** **Persistence / repeated current-load forecast:**
at each legal t0, fill the 30-bin input with the last completed 10 s mean
\( \bar P \) of **already observed** P_load (or the current-bin mean if the bin
is open, using only t≤t0 samples). Shrinking horizon at end-of-voyage matches
C1/C2.

Why this and not the others (prospective, not from val outcomes):

- Isolates *future-shape* information: C0 knows the present level, not the
  future path.
- Scalar-only E300 would mix a different information *type* (energy vs
  trajectory) into C0 vs C1, confounding Q3.
- Neutral/flat zero trajectory injects a biased load level.
- Persistence is arm-independent and uses no future samples.

**Unresolved.** Open-bin mean vs last closed bin; Brainstorm should pick one
rule and freeze it in Requirements.

---

## Q3 — C1 perfect-information definition

**Recommendation.** Freeze C1 truth as:

- 30 × 10 s Pbar from `assemble_oracle_window` on the realized 1 Hz P_load
  stream, Dataset-v3 target semantics;
- refresh 10 s; shrinking horizon; partial last bin duration-weighted;
- **no 1 Hz privileged future** inside the controller;
- same cadence/end-of-voyage as C0/C2.

C1 is only the perfect-information **input** to C\*, not a different optimizer.

---

## Q4 — C2 Predictor-v3 interface

**Frozen Phase-E identity (inspect-only):**

| Item | Value |
|---|---|
| Winner | REAT-T (`reatt`) |
| Trial | `reatt-t018` |
| `canonical_delivery_seed_index` | 0 |
| `winner_identity_sha256` | `832720b0b0f39592fdeaa25957c1a61d0f0a1381d7220881e0b9af345eb37aa7` |
| `config_sha256` | `1d662ff03ed7ca042b731707501d6b254fc35b64ae858431c66745025e544b5d` |
| Feature surface | H1 + K + F |
| Output | `[B,30,3]` = `[q10,q50,q90]` kW |
| Recommended C\* input | **q50** (eval_config primary point prediction) |
| E300 | derived from q50, not a model head |
| Cadence | one window per 10 s issue time (Dataset-v3) |
| Retrain | **forbidden** on this route unless separately authorized |

C2 must obey the frozen input contract (hist 300×H1, KF future tensors, formal
norm + predictor_norm_ext). History/state: only information legally available
at issue time.

**Unresolved.** Whether q10/q90 may be used later for uncertainty-aware C\*
(T-451-like). This route should freeze **q50-only** unless Brainstorm expands.

---

## Q5 — Fair controller parity

**Recommendation.** Identical across C0/C1/C2 unless an exception is written
into Requirements **before** any formal run:

- controller code path;
- Δλ action space and grid;
- window DP objective + terminal (`w_term`, δ);
- `realize_bin` / PMS / Safety / Plant;
- 10 s refresh;
- feasibility handling;
- k_eq / FEC accounting;
- voyage initial conditions.

**Only the 30-bin forecast source differs.**

P0 is allowed to differ internally (law=5).

---

## Q6 — Scientific outcomes

**Recommendation.** This route should be **estimation / effect-size reporting
with confidence intervals**, not a hard GO/NO-GO cloned from T-445’s 2%.

Reasons:

- T-445’s 2% was the oracle-headroom *decision* threshold. That claim is
  closed as invalid.
- A new hard threshold chosen after seeing T-010 residuals would be
  result-tainted.
- Practical control value can be small, negative, or heterogeneous across
  groups; CI + pre-registered estimands are more defensible.

Pre-register (in later Requirements, not now as approved):

Primary estimands (kg and % of C0 J_FEC and of C0 raw fuel):

- Δ_perfect = C0 − C1
- Δ_pred = C0 − C2
- Gap = C1 − C2   (or C2 − C1; sign frozen later)

Secondary: terminal SOC, hard-feasibility, Safety/PMS interventions,
starts/stops, TV_Pgen, battery throughput, floor activation, Δλ activity.

Integrity failures (leakage, OOB, SimState mismatch, unpaired groups) still
produce a **validity fail**, not a numerical NO-GO.

**Unresolved.** Cluster bootstrap B=10000 on the same 9 groups is recommended
to reuse T-445 stats code, but the GO machine is retired.

---

## Q7 — Statistical corpus

**Recommendation.** Reuse the frozen T-445 val 9+3 `split_group` manifest
(`corpus_identity_sha256=245fa307…`) **if** Brainstorm/Requirements freeze it
**before** the first C0/C1/C2 observation.

Why reuse is scientifically clean *enough*:

- T-445 **never observed** formal B1/B2 controller outcomes on those voyages.
- Selection of the 9 groups was pre-registered on **load-shape** scores, not
  on C\* fuel.
- Re-selecting groups now, after seeing T-010 fixture behaviour, would be
  worse.

Caveat: T-010 used S01 settled/cold fixtures (not val voyages) to debug the
surrogate. That does not score the 36-voyage set, but it did inform controller
implementation. Freeze the **code+config** before looking at val C\* results.

IID test: **remain sealed** for a later final controller evaluation unless the
roadmap explicitly changes this.

OOD: separate, not for C\* selection.

Reserve 12: keep unused until a pre-registered expansion rule exists. Default:
do not expand.

---

## Q8 — T-445 qualification failure implication

**Recommendation.** Do **not** blindly reuse the 1% proxy-vs-plant oracle
replica as the new scientific claim.

Still require, prospectively:

- deterministic two-run equality on a synthetic/nonformal fixture;
- action/information boundary tests (no 1 Hz future; C0 uses only t≤t0);
- Model-E logs as the outcome;
- numerical robustness appropriate to C\* (e.g. Q3-style Δλ grid, Q5 first
  action) without treating Q2/Q4/Q6 1% as a Gate for *this* claim;
- OOB=0 on the policy table;
- SimState continuous-vs-segmented if segmented execution is used.

If a new numeric replica tolerance is ever introduced, it must be justified
**before** seeing val C0/C1/C2 fuel, and it may only bound engineering
sanity—not resurrect an oracle-upper-bound certificate.

---

## Q9 — Predictor-vs-perfect decomposition

**Recommendation.** Report three signed effects (Q6) first.

Investigate a **prediction realization ratio** only as a derived diagnostic:

R = Δ_pred / Δ_perfect = (C0−C2)/(C0−C1)

Do **not** freeze R as a primary metric until review. Edge cases:

- |Δ_perfect| below a pre-registered ε → R **undefined**;
- Δ_perfect < 0 (perfect info hurts C\*) → R not a “realization fraction”;
- Δ_pred and Δ_perfect opposite signs → report both, do not clip R to [0,1].

---

## Q10 — Deployment relationship

**Recommendation.** First establish practical control effect on **Model E**.
Later profile/port the chosen online C\* + Predictor-v3 to STM32N6.

Do not let N6 RAM/latency reshape C0/C1/C2 unless a separate approved
constraint. `st_edge_ai_upload/` remains user-owned and out of this spec.

---

## Risks and limitations

- C\* may still be a mediocre supervisor; then C1−C0 understates the value of
  trajectory information **for a better architecture**. That is inherent to
  “value within a fixed controller” and must be stated in the paper.
- Persistence C0 is not a unique no-future definition.
- Amendment E 1 s Euler vs 0.01 s plant remains inside C\* cost; outcomes on
  Model E do not require 1% replica.
- Small n=9 groups: keep SMALL_N caution; do not over-claim.
- Winner weights live in experiments/local artifacts; C2 needs a frozen
  loadable identity. Do not consume `st_edge_ai_upload/`.

---

## Traceability (Phase-E / T-445 artifacts)

| Artifact | Role in new route |
|---|---|
| T-445 `final_gate_pointer.json` | predecessor closed; not a GO ticket |
| `assemble_oracle_window` | C1 truth |
| `b2_window_dp` + `realize_bin` | C\* internals |
| `corpus_manifest.json` | candidate val set |
| `k_eq_E` / `metrics_fec` | accounting |
| Predictor-v3 winner `reatt` / t018 / q50 | C2 |
| T-445 Q1–Q6 FAIL JSON | diagnostic history only |
| T-445 2% GO rule | **not** inherited |

---

## Gate request

Submit this brainstorm for **human/ChatGPT Brainstorm Gate review**.

Not self-approved. No Requirements/Design/Tasks are in force.
No formal C0/C1/C2 validation-corpus run was executed while drafting this.

# Phase 4-F scientific roadmap amendment v1

Status: **brainstorm-approved** (Gate R1–R10 applied; Requirements draft next)  
Date: 2026-09-15  
Depends: T-445 CLOSED `GATE_VALIDITY_FAIL` (`f36cdcbc` / `final_gate_pointer.json`)

## What is closed

T-445 is a **methodological validity failure**. The original F-package claim
“prove a trajectory-oracle upper bound, then build predictive EMS” **stops**.
T-446 as previously defined remains blocked.

## Working question (frozen)

Under **one fixed practical** predictive supervisory controller C*, how much
practical Model-E control benefit is obtained from:

1. no future information (C0 persistence),
2. exact legal 30-bin future trajectory truth (C1),
3. frozen Predictor-v3 q50 (C2)?

C1 name: `PERFECT_INFORMATION_CONTROLLER_REFERENCE` only.

## Formal corpus (R6)

12 split_groups / 48 voyages = previous T-445 initial 9 + reserve 3.
One fixed formal evaluation set. **No sequential expansion.**

## Suggested later tasks (after Requirements/Design Gates; local IDs)

Umbrella **T-456**. Do not reuse T-445 task IDs.

- T-456a — freeze C* / ForecastWindow / C0 / C1 / C2 contracts
- T-456b — nonformal interface + causality smoke (synthetic / nonformal only)
- T-456c — practical-controller qualification (R7; **not** 1% oracle replica)
- T-456d — 48-voyage P0/C0/C1/C2 after pre-execution freeze
- T-456e — effect-size report (B_perfect / B_predicted / L_prediction + CI)
- T-456f — optional N6 profile **after** Model-E comparison

IID test remains sealed. OOD remains separate. No result-driven retune.

## Explicitly not in this amendment

- Retraining Predictor-v3
- Changing Model E `.slx`
- Accessing IID test / OOD for controller selection
- Calling C1 an upper bound
- Starting formal C0/C1/C2 runs before Requirements/Design/Tasks Gates
- Touching `st_edge_ai_upload/`

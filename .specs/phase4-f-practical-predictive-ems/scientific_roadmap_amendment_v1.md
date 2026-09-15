# Phase 4-F scientific roadmap amendment v1 (draft)

Status: **draft — not approved**  
Date: 2026-09-15  
Depends: T-445 CLOSED `GATE_VALIDITY_FAIL` (`f36cdcbc` / `final_gate_pointer.json`)

## What is closed

T-445 Trajectory Oracle Gate is closed as a **methodological validity failure**.
The original F-package claim “prove a trajectory-oracle upper bound, then build
predictive EMS” **stops**. T-446 as previously defined is blocked.

## New working question

Under **one fixed** predictive supervisory-controller architecture, how much
practical Model-E control benefit is obtained from:

1. no future information,
2. perfect 30-bin future trajectory information,
3. Predictor-v3 trajectory information?

## Proposed roadmap insertion (after T-445)

Replace the blocked T-446→T-450 “oracle then AI” sequence with a new local spec
`phase4-f-practical-predictive-ems` (umbrella number **T-456** suggested).

Do not reuse T-445 task IDs.

Suggested later (only after Brainstorm/Requirements/Design Gates):

- T-456a — freeze C* / C0 forecast / C1 truth / C2 contract
- T-456b — nonformal interface smoke (synthetic only)
- T-456c — practical-controller qualification (not 1% oracle replica)
- T-456d — val corpus C0/C1/C2/P0 (after freeze; no result-driven retune)
- T-456e — effect-size report + CI (no inherited 2% GO unless later approved)
- T-456f — optional N6 profile **after** Model-E scientific comparison

IID test remains sealed. OOD remains separate.

## Explicitly not in this amendment

- Retraining Predictor-v3
- Changing Model E `.slx`
- Accessing IID test / OOD for controller selection
- Calling C1 an upper bound
- Starting formal C0/C1/C2 runs before Gates

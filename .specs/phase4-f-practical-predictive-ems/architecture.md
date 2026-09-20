# Architecture sketch

Status: **brainstorm-approved** (Gate R1–R10)  
Not a substitute for Design.

```text
                    ┌─────────────────────────────────────┐
                    │  Frozen Model E plant + PMS + Safety │
                    │  law=6 Δλ channel (true ZOH)         │
                    └──────────────▲──────────────────────┘
                                   │ Δλ_k every 10 s
                    ┌──────────────┴──────────────────────┐
                    │  Shared practical supervisor C*      │
                    │  receding-horizon Δλ DP              │
                    │  SOC grid 0.005 (algorithm freeze)   │
                    │  k_eq=0.5457  w_term=1.17e5          │
                    │  realize_bin cost; terminal frozen   │
                    └──────────────▲──────────────────────┘
                                   │ ForecastWindow
                                   │ 30 × 10 s Pbar [kW]
          ┌────────────────────────┼────────────────────────┐
          │                        │                        │
          C0                       C1                       C2
   persistence of last      exact legal 30×10 s      frozen REAT-T
   completed 10 s mean      bus-load truth           t018/seed0 q50
   (t0<10 s: P_load(t0))    PERFECT_INFORMATION      de-norm → window
                            CONTROLLER_REFERENCE
                            (not an upper bound)

P0  law=5 SOC-Adaptive ECMS — contextual only (architecture differs)
```

## ForecastWindow (R4)

After source-specific assembly / de-normalization, C0, C1, and C2 SHALL
present the same object to C*:

- `t0` [s]
- `Pbar[1:30]` [kW] bus-side 10 s means (trailing unused bins empty at
  shrinking horizon, duration-weighted last bin)
- `K_remain` / horizon metadata
- `source_id` ∈ {C0_PERSISTENCE, C1_TRUTH, C2_REATT_Q50}
- provenance hashes

C* SHALL NOT branch on `source_id` for control law.

## Primary estimands (R5)

Measured on Model-E logs, J_FEC:

```text
B_perfect    = 100*(J_C0 − J_C1)/J_C0
B_predicted  = 100*(J_C0 − J_C2)/J_C0
L_prediction = 100*(J_C2 − J_C1)/J_C0
```

P0 is not in the primary decomposition.

## Non-claims

C1 is not a theoretical upper bound, global optimum, trajectory oracle upper
bound, or maximum trajectory-information value.

Final fuel/SOC/feasibility are Model-E plant logs, not surrogate J.

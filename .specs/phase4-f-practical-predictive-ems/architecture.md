# Architecture sketch (draft, not approved)

```text
                    ┌─────────────────────────────────────┐
                    │  Frozen Model E plant + PMS + Safety │
                    │  law=6 Δλ channel (true ZOH)         │
                    └──────────────▲──────────────────────┘
                                   │ Δλ_k every 10 s
                    ┌──────────────┴──────────────────────┐
                    │  Shared predictive supervisory C*    │
                    │  receding-horizon Δλ DP (T-445 B2    │
                    │  code, renamed: not an oracle bound) │
                    │  same action / objective / FEC /     │
                    │  realize_bin cost / terminal         │
                    └──────────────▲──────────────────────┘
                                   │ 30 × 10 s Pbar forecast
          ┌────────────────────────┼────────────────────────┐
          │                        │                        │
     C0 no-future             C1 perfect               C2 Predictor-v3
     (persistence /           Dataset-v3               frozen REAT-T
      repeated current        30-bin truth             q50 trajectory
      — Brainstorm Q2)        same cadence             same cadence
                                                       no retune

P0  law=5 SOC-Adaptive ECMS
    conventional external baseline
    not required to share C* internals
```

## Arm roles

| Arm | Future information | Controller | Role |
|---|---|---|---|
| P0 | none (native law=5) | frozen SOC-Adaptive ECMS | external conventional context |
| C0 | no learned/perfect trajectory | C* | architecture without future info |
| C1 | perfect 30×10 s Pbar | C* | `PERFECT_INFORMATION_CONTROLLER_REFERENCE` |
| C2 | Predictor-v3 q50 30-bin | C* | learned-prediction realization |

## Primary contrasts (if Brainstorm adopts this)

- C1 − C0: practical value of perfect trajectory **inside C\***
- C2 − C0: realized practical value of learned prediction
- C1 − C2: prediction-realization gap
- P0 vs C*: context only unless later approved as primary

## Non-claims

C1 is **not**: theoretical upper bound, global optimum, trajectory-information upper bound.
Final fuel/SOC/feasibility are **Model-E plant logs**, not surrogate J.

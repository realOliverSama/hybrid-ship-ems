# D1-B - TRAFFIC overlap / pure-host diagnostic

> **DEVELOPMENT_DIAGNOSTIC_NOT_GATE_EVIDENCE** - post-fail development
> diagnostic (T429_D1_diagnostics_spec.md v2, section D1-B). Not Gate
> evidence; must not be spliced into any confirmation corpus nor
> written back into any v1 artifact.

Generated: 2026-09-06T15:24:09

## Question

Day-3 Gate FAIL (T-022) showed the `traffic_follow vs free_cruise`
contrast below frozen thresholds. Hypothesis tested here: the A-class
(`traffic_follow`) slices are host-contaminated - a 120 s slice cannot
sit fully inside a follow-event platform interval because the
platform (event minus 10% ramps each side) is too short.

## Inputs and replay

- Export: MATLAB mechanical replay of the 27 `GOOD_V1+ON` manifest
  voyages (`dag_sample('draw')` + `env_realize('realize')`, seed
  `MISSION_V3_PREQUAL_SEED_V1_2026_09_05`, cache by `stratum#draw`),
  lineage check (ACCEPT + stratum + base_draw_index) passed for all.
- Cruise segments: 18 (EMERGENCY_RESPONSE has no cruise role and is
  naturally empty). Cruise-segment traffic events: 5.

## Slice generation and overlap

Convention: 120 s window, 30 s side buffers, 60 s stride, windows
fully inside `[t_start+30, t_end-30]`; per-slice
`overlap_fraction = max over that segment's events of
|slice ∩ [t+0.1d, t+0.9d]| / 120`.

| bucket | count | share |
|---|---|---|
| of == 0 | 163 | 94.2% |
| 0 < of < 1 | 10 | 5.8% |
| of == 1 | 0 | 0.0% |
| of < 1 (total) | 173 | 100.0% |

Total candidate A-class slices on this subset: 173.

## Follow platform length (L_follow = 0.8 x duration)

- n = 5; min = 58.0 s, median = 76.2 s, max = 113.1 s
- n(L_follow >= 120 s) = 0; n(L_follow >= 180 s) = 0
- Design upper bound: duration ~ U[60,180] s -> L_follow in [48,144] s,
  i.e. **no event can ever reach 180 s of platform** (spec-expected 0,
  design mismatch quantified).

## Pure slice supply back-solve

- Demand: 458 pure slices (T-022 `n_slices_A`, class_A = `traffic_follow`).
- Conservative host definition (120 s window + 2 x 30 s buffer):
  **458 follow hosts with L_follow >= 180 s are required**.
- Grid-capacity floor (60 s stride, no buffer): L = 180 s hosts at
  least 229 are required.
- Current supply from the exported events: 0 slices (grid capacity),
  n(L>=180) = 0 -> conservative shortfall 458 (100% of demand).

## Verdict

**HOST_CONTAMINATION_CONFIRMED**

Rule: of<1 share = 100.0% (>50%) and n(L_follow>=180) = 0.

In-class P contrast (`of>=0.99` vs `of<0.99` slice mean-P direction):
NOT_AVAILABLE - branch B Step 1 export carries no P stream (spec
allows branch B to omit it); interval statistics only.

## Notes / limitations

- This diagnostic replays the 27-voyage `GOOD_V1+ON` manifest subset;
  the T-022 demand constant (458) counts the full 108-draw corpus.
- Statistics computed in Python (numpy available, not required for
  these closed-form interval statistics); MATLAB did mechanical
  export only (v2 iron law: zero local functions).

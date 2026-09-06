# T-022 T429 EVALUATE - Identifiability Gate Report

generated_at: 2026-09-06T04:31:46Z
implementation sha256 (scripts/+mission_v3/+audits/identifiability.m): 4c18237c270bbda49d6b5c05ed613e25f327d690027ab55620e95dd0efb032c3
wall_s: 18.0

## 1. Freeze verification (machine, fail-closed)

- freeze self_hash: f8152ffeffa228a4371c0a43d6aa987df523931ce23f91c24acfdfff3ac5a4cb (recomputed f8152ffeffa228a4371c0a43d6aa987df523931ce23f91c24acfdfff3ac5a4cb, ok=1)
- contrast registry hash: ok=1 (expected by freeze b980a457bf45930274fb13263b497e29d426bbec26777d969026a04118a766ad)
- heldout token registry (nuisance config) hash: ok=1
- prequal manifest sha256: ok=1 (108 voyages)
- qualified generation stack composite (Strict re-collect): ok=1

## 2. Corpus and slice construction

- corpus: results/mission_v3/prequal_manifest.json (108 voyages, 27 families, 27 unique split groups); artifact sha256 verified for 108 artifacts
- SEGMENT_PURE_DIAGNOSTIC_SLICE pool: 5612 slices (frozen geometry: 120 s slice / 30 s edge buffer / 60 s stride)
- slice encoding: 12-dim frozen encoding: mean/std/min/max/p95/rms + six a-priori transient count features (jump up/down >10% of slice mean; 10 s ramp up/down >5% of slice mean; excursions beyond mean +/- 2 sd); fixed a priori, never tuned

## 3. Required contrasts (gate-bearing)

| contrast_id | n_slices A/B | n_groups A/B (min cov 10 per class) | R_point | R_lcb95 (>= 1.5) | SBD | BA_point | BA_lcb95 (>= 0.80) | probe | status |
|---|---|---|---|---|---|---|---|---|---|
| SEG_BRIDGE_VS_HARBOR | 76/212 | 9/9 | NaN | NaN | 0 | NaN | NaN | 0 | INSUFFICIENT_IDENTIFIABILITY_EVIDENCE |
| SEG_BERTH_VS_DEPARTURE | 56/92 | 9/9 | NaN | NaN | 0 | NaN | NaN | 0 | INSUFFICIENT_IDENTIFIABILITY_EVIDENCE |
| DIR_UPSTREAM_VS_DOWNSTREAM | 1296/2380 | 27/27 | 0.6970 | 0.6819 | 0 | 0.7392 | 0.6614 | 0 | METRIC_FAIL |
| DEPTH_LOW_ID_VS_NORMAL_ID | 0/0 | 0/0 | NaN | NaN | 0 | NaN | NaN | 0 | INSUFFICIENT_IDENTIFIABILITY_EVIDENCE |
| SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE | 228/792 | 9/9 | NaN | NaN | 0 | NaN | NaN | 0 | INSUFFICIENT_IDENTIFIABILITY_EVIDENCE |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | 458/1384 | 25/25 | 0.5340 | 0.4927 | 0 | 0.8164 | 0.7708 | 0 | METRIC_FAIL |

### Per-contrast detail (required)

#### SEG_BRIDGE_VS_HARBOR (INSUFFICIENT_IDENTIFIABILITY_EVIDENCE)

- factor axis: segment_role; class_A=bridge; class_B=harbor; gate_role=REQUIRED
- matched cohort: 5 mixed cells; applied: regime (same); direction (same); template (same_when_feasible: kept); operator (same_when_feasible: kept); relaxed: none
- FAIL-CLOSED: group coverage below the frozen minimum 10 per class (A=9, B=9 unique split groups)

#### SEG_BERTH_VS_DEPARTURE (INSUFFICIENT_IDENTIFIABILITY_EVIDENCE)

- factor axis: segment_role; class_A=berth; class_B=departure; gate_role=REQUIRED
- matched cohort: 1 mixed cells; applied: template (same); relaxed: none
- FAIL-CLOSED: group coverage below the frozen minimum 10 per class (A=9, B=9 unique split groups)

#### DIR_UPSTREAM_VS_DOWNSTREAM (METRIC_FAIL)

- factor axis: direction; class_A=upstream; class_B=downstream; gate_role=REQUIRED
- matched cohort: 3 mixed cells; applied: regime (same); reach (same); relaxed: none
- pairs: cross 1031040, within_A 279784, within_B 947058
- SBD: median_cross 0.4615, Q0.95 within_A 0.6621, Q0.95 within_B 0.6536, R_point 0.6970, R_lcb95 0.6819 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.7392, BA_lcb95 0.6614 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.7647, 0.7511, 0.6440, 0.7450, 0.7910]

#### DEPTH_LOW_ID_VS_NORMAL_ID (INSUFFICIENT_IDENTIFIABILITY_EVIDENCE)

- factor axis: depth; class_A=low_id; class_B=normal_id; gate_role=REQUIRED
- matched cohort: 0 mixed cells; applied: template (same); regime (same); relaxed: none
- FAIL-CLOSED: empty slice pool for at least one class inside mixed cohort cells

#### SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE (INSUFFICIENT_IDENTIFIABILITY_EVIDENCE)

- factor axis: mission_intensity; class_A=rush; class_B=return; gate_role=REQUIRED
- matched cohort: 3 mixed cells; applied: template==EMERGENCY_RESPONSE (hard filter); regime (same); operator (same); reach (same_when_feasible: kept); relaxed: none
- FAIL-CLOSED: group coverage below the frozen minimum 10 per class (A=9, B=9 unique split groups)

#### SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE (METRIC_FAIL)

- factor axis: traffic; class_A=traffic_follow; class_B=free_cruise; gate_role=REQUIRED
- matched cohort: 25 mixed cells; applied: cohort cell = family (same base_draw traffic ON vs OFF physical counterfactual); relaxed: none
- pairs: cross 29836, within_A 4949, within_B 50728
- SBD: median_cross 0.5340, Q0.95 within_A 1.0000, Q0.95 within_B 1.0000, R_point 0.5340, R_lcb95 0.4927 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.8164, BA_lcb95 0.7708 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.8706, 0.8829, 0.7099, 0.8255, 0.7930]

## 4. Secondary diagnostics (non-gate; DGN-12 R9)

| contrast_id | n_slices A/B | n_groups A/B | R_point | R_lcb95 | BA_point | BA_lcb95 | status |
|---|---|---|---|---|---|---|---|
| SEG_HARBOR_VS_SHALLOW_CRUISE | 0/0 | 0/0 | NaN | NaN | NaN | NaN | INSUFFICIENT_IDENTIFIABILITY_EVIDENCE |
| SEG_BRIDGE_VS_SHALLOW_CRUISE | 0/0 | 0/0 | NaN | NaN | NaN | NaN | INSUFFICIENT_IDENTIFIABILITY_EVIDENCE |

## 5. Gate verdict

- overall: 0
- verdict: FAIL_INSUFFICIENT_EVIDENCE
- required contrasts PASS: 0 / 6

FR-547/FR-551: insufficient evidence or a failed required contrast is fail-closed; scenario redesign is the only legal remediation; thresholds/geometry/encoding are frozen and never adjusted after a failure

## 6. Method provenance

- SBD semantics: dataset_v3/audit_leakage.py L3 (D-side); SBD = 1 - max NCC, NUMERIC_FLOOR 1e-12
- probe: base-MATLAB IRLS logistic (Statistics Toolbox functions probed absent, FD10); frozen encoding, grouped 5-fold CV by split_group_id, train-only standard scaler
- quantile rule: nearest-rank order statistic (point and bootstrap consistent; bootstrap carries group multiplicity)
- replay binding: env_realize(formal_seed, base_draw_index) + plan_compose; PREQUAL-AMEND-01 bitwise reuse evidence; per-artifact cross-checks in identifiability.m


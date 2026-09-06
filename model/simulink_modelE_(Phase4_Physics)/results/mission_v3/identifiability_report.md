# T-022 T429 EVALUATE - Identifiability Gate Report

generated_at: 2026-09-06T16:23:34Z
implementation sha256 (scripts/+mission_v3/+audits/identifiability.m): 5420a448665931b010656e512e9f1a750a0aaf32b8f618719d0c584f7ed5af08
wall_s: 49.2

## 1. Freeze verification (machine, fail-closed)

- freeze self_hash: 213127b267e0e210540ad7188fdfc0d4afc0d2ff03bf91663d8407b0978290f0 (recomputed 213127b267e0e210540ad7188fdfc0d4afc0d2ff03bf91663d8407b0978290f0, ok=1)
- contrast registry hash: ok=1 (expected by freeze 2274da4aeb7784f4026f51f03f5c0e020a5a192ffc59044f2970850c66f50c2b)
- heldout token registry (nuisance config) hash: ok=1
- prequal manifest sha256: ok=1 (144 voyages)
- qualified generation stack composite (Strict re-collect): ok=1

## 2. Corpus and slice construction

- corpus: results/mission_v3/prequal_manifest.json (144 voyages, 60 families, 60 unique split groups); artifact sha256 verified for 144 artifacts
- SEGMENT_PURE_DIAGNOSTIC_SLICE pool: 7631 slices (frozen geometry: 120 s slice / 30 s edge buffer / 60 s stride)
- slice encoding: 12-dim frozen encoding: mean/std/min/max/p95/rms + six a-priori transient count features (jump up/down >10% of slice mean; 10 s ramp up/down >5% of slice mean; excursions beyond mean +/- 2 sd); fixed a priori, never tuned

## 3. Required contrasts (gate-bearing)

| contrast_id | n_slices A/B | n_groups A/B (min cov 10 per class) | R_point | R_lcb95 (>= 1.5) | SBD | BA_point | BA_lcb95 (>= 0.80) | probe | status |
|---|---|---|---|---|---|---|---|---|---|
| SEG_BRIDGE_VS_HARBOR | 108/280 | 12/12 | 0.7212 | 0.6815 | 0 | 0.7656 | 0.6844 | 0 | METRIC_FAIL |
| SEG_BERTH_VS_DEPARTURE | 72/104 | 12/12 | 0.8576 | 0.8231 | 0 | 0.9900 | 0.9712 | 1 | METRIC_FAIL |
| DIR_UPSTREAM_VS_DOWNSTREAM | 1801/3247 | 60/60 | 0.6899 | 0.6780 | 0 | 0.6390 | 0.5863 | 0 | METRIC_FAIL |
| DEPTH_LOW_ID_VS_NORMAL_ID | 825/1958 | 13/23 | 0.5416 | 0.5308 | 0 | 0.5391 | 0.5027 | 0 | METRIC_FAIL |
| SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE | 312/1088 | 12/12 | 0.6907 | 0.6217 | 0 | 0.9547 | 0.9161 | 1 | METRIC_FAIL |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | 895/1934 | 35/35 | 0.5404 | 0.4982 | 0 | 0.7600 | 0.7147 | 0 | METRIC_FAIL |

### Per-contrast detail (required)

#### SEG_BRIDGE_VS_HARBOR (METRIC_FAIL)

- factor axis: segment_role; class_A=bridge; class_B=harbor; gate_role=REQUIRED
- matched cohort: 5 mixed cells; applied: regime (same); direction (same); template (same_when_feasible: kept); operator (same_when_feasible: kept); relaxed: none
- pairs: cross 7520, within_A 1506, within_B 9204
- SBD: median_cross 0.5508, Q0.95 within_A 0.7637, Q0.95 within_B 0.7328, R_point 0.7212, R_lcb95 0.6815 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.7656, BA_lcb95 0.6844 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.7846, 0.7946, 0.7448, 0.6250, 0.8792]

#### SEG_BERTH_VS_DEPARTURE (METRIC_FAIL)

- factor axis: segment_role; class_A=berth; class_B=departure; gate_role=REQUIRED
- matched cohort: 1 mixed cells; applied: template (same); relaxed: none
- pairs: cross 7488, within_A 2556, within_B 5356
- SBD: median_cross 0.8576, Q0.95 within_A 1.0000, Q0.95 within_B 0.6674, R_point 0.8576, R_lcb95 0.8231 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.9900, BA_lcb95 0.9712 (bootstrap 1000/1000 valid, seed 42902); fold BAs [1.0000, 1.0000, 0.9500, 1.0000, 1.0000]

#### DIR_UPSTREAM_VS_DOWNSTREAM (METRIC_FAIL)

- factor axis: direction; class_A=upstream; class_B=downstream; gate_role=REQUIRED
- matched cohort: 3 mixed cells; applied: regime (same); reach (same); relaxed: none
- pairs: cross 2017629, within_A 556208, within_B 1827331
- SBD: median_cross 0.4650, Q0.95 within_A 0.6741, Q0.95 within_B 0.6622, R_point 0.6899, R_lcb95 0.6780 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.6390, BA_lcb95 0.5863 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.5664, 0.7660, 0.5707, 0.6038, 0.6883]

#### DEPTH_LOW_ID_VS_NORMAL_ID (METRIC_FAIL)

- factor axis: depth; class_A=low_id; class_B=normal_id; gate_role=REQUIRED
- matched cohort: 2 mixed cells; applied: template (same); regime (same); relaxed: none
- pairs: cross 650100, within_A 251826, within_B 1033087
- SBD: median_cross 0.5416, Q0.95 within_A 1.0000, Q0.95 within_B 0.7469, R_point 0.5416, R_lcb95 0.5308 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.5391, BA_lcb95 0.5027 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.5163, 0.5594, 0.5337, 0.5154, 0.5708]

#### SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE (METRIC_FAIL)

- factor axis: mission_intensity; class_A=rush; class_B=return; gate_role=REQUIRED
- matched cohort: 3 mixed cells; applied: template==EMERGENCY_RESPONSE (hard filter); regime (same); operator (same); reach (same_when_feasible: kept); relaxed: none
- pairs: cross 113088, within_A 16116, within_B 197168
- SBD: median_cross 0.4633, Q0.95 within_A 0.6707, Q0.95 within_B 0.6596, R_point 0.6907, R_lcb95 0.6217 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.9547, BA_lcb95 0.9161 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.9583, 0.9563, 0.9615, 0.9446, 0.9528]

#### SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE (METRIC_FAIL)

- factor axis: traffic; class_A=traffic_follow; class_B=free_cruise; gate_role=REQUIRED
- matched cohort: 35 mixed cells; applied: cohort cell = family (same base_draw traffic ON vs OFF physical counterfactual); relaxed: none
- pairs: cross 58850, within_A 14136, within_B 63342
- SBD: median_cross 0.5404, Q0.95 within_A 1.0000, Q0.95 within_B 1.0000, R_point 0.5404, R_lcb95 0.4982 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.7600, BA_lcb95 0.7147 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.6990, 0.7042, 0.7802, 0.8444, 0.7722]

## 4. Secondary diagnostics (non-gate; DGN-12 R9)

| contrast_id | n_slices A/B | n_groups A/B | R_point | R_lcb95 | BA_point | BA_lcb95 | status |
|---|---|---|---|---|---|---|---|
| SEG_HARBOR_VS_SHALLOW_CRUISE | 280/142 | 12/13 | 0.6836 | 0.6373 | 0.9880 | 0.9773 | METRIC_FAIL |
| SEG_BRIDGE_VS_SHALLOW_CRUISE | 224/142 | 24/13 | 0.6966 | 0.6694 | 0.9714 | 0.9397 | METRIC_FAIL |

## 5. Gate verdict

- overall: 0
- verdict: FAIL_METRIC
- required contrasts PASS: 0 / 6

FR-547/FR-551: insufficient evidence or a failed required contrast is fail-closed; scenario redesign is the only legal remediation; thresholds/geometry/encoding are frozen and never adjusted after a failure

## 6. Method provenance

- SBD semantics: dataset_v3/audit_leakage.py L3 (D-side); SBD = 1 - max NCC, NUMERIC_FLOOR 1e-12
- probe: base-MATLAB IRLS logistic (Statistics Toolbox functions probed absent, FD10); frozen encoding, grouped 5-fold CV by split_group_id, train-only standard scaler
- quantile rule: nearest-rank order statistic (point and bootstrap consistent; bootstrap carries group multiplicity)
- replay binding: env_realize(formal_seed, base_draw_index) + plan_compose; PREQUAL-AMEND-01 bitwise reuse evidence; per-artifact cross-checks in identifiability.m


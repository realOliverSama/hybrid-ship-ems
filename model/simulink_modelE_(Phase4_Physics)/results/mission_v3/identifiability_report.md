# T-022 T429 EVALUATE - Identifiability Gate Report

generated_at: 2026-09-06T18:33:58Z
implementation sha256 (scripts/+mission_v3/+audits/identifiability.m): c2091d4bf27f3ba1ae060071856effd9da65997ed61f09c526b44199adbab405
wall_s: 19.3

## 1. Freeze verification (machine, fail-closed)

- freeze self_hash: 213127b267e0e210540ad7188fdfc0d4afc0d2ff03bf91663d8407b0978290f0 (recomputed 213127b267e0e210540ad7188fdfc0d4afc0d2ff03bf91663d8407b0978290f0, ok=1)
- amendment_v2 self_hash: 796a8b07d0b3c521d10c41f4291ca8b8a54fb32925e93f926c02ec10c08427d8 (recomputed 796a8b07d0b3c521d10c41f4291ca8b8a54fb32925e93f926c02ec10c08427d8, ok=1)
- contrast registry hash: ok=1 (expected by freeze 2274da4aeb7784f4026f51f03f5c0e020a5a192ffc59044f2970850c66f50c2b)
- heldout token registry (nuisance config) hash: ok=1
- confirmation corpus manifest sha256: ok=1 (144 voyages)
- qualified generation stack composite (Strict re-collect): ok=1

## 2. Structural conformance C1~C3 (EXECFIX-01; pre-metric)

- C1 (paired-effect registry profile + complete same-base_draw matched pairs): ok=1
  - DIR_UPSTREAM_VS_DOWNSTREAM: profile=PAIRED_LEVEL_SHIFT; sbd_gate_bearing=0; paired groups=60 (min cov 10 reached); single-side excluded=0; alpha=0.05
- C2 (TRAFFIC class-A pure-host purity): ok=1
  - SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE: n_pure_A=214 / 214 A slices; min STEADY_FOLLOW overlap=1.000000000; ON groups with pure A=35
- C3 (DEPTH strict selection-only audit): ok=1
  - DEPTH_LOW_ID_VS_NORMAL_ID: template=WORK_MISSION x regime=DRY; dataset_role=T429_IID_DIAGNOSTIC; LOW groups=12 (HT range [1.51, 2]); NORMAL groups=12 (HT range [2.23, 2.55] inside band [2.2, 2.6]); token status=none; manifest/replay labels consistent=1
    - LOW group ids: SG#a63f51aa645a8cc0; SG#0c6d8b2603584732; SG#465dd59970c9527d; SG#4ac1ccf76b5fd66f; SG#035a91b15e04aea5; SG#06761123c3673e2a; SG#484558bed5dbd4ce; SG#25690f190669b12b; SG#730143c4da39426b; SG#98cda786f85d3147; SG#db0cb702e4b18c47; SG#01fb33821543c8a8
    - NORMAL group ids: SG#f2771ff24a8682d5; SG#ab4ea6530e267c32; SG#586138f3e56bb487; SG#1c558bf22ac199a6; SG#586cd5e8bc40e06f; SG#e87eee5edbf84cf0; SG#ec77eb64a7e6c0da; SG#1c41b865068c5845; SG#ace8cad1b8c60291; SG#66fcc83e73b0893f; SG#e8e65c4edc779c1b; SG#e0ba912c49a5a171

## 3. Corpus and slice construction

- corpus: results/mission_v3/t429conf_v2_manifest.json (144 voyages, 60 families, 60 unique split groups); artifact sha256 verified for 144 artifacts
- SEGMENT_PURE_DIAGNOSTIC_SLICE pool: 7631 slices (frozen geometry: 120 s slice / 30 s edge buffer / 60 s stride)
- slice encoding: 12-dim frozen encoding: mean/std/min/max/p95/rms + six a-priori transient count features (jump up/down >10% of slice mean; 10 s ramp up/down >5% of slice mean; excursions beyond mean +/- 2 sd); fixed a priori, never tuned

## 4. Required contrasts (gate-bearing)

| contrast_id | metric_profile | n_slices A/B | n_groups A/B (min cov 10 per class) | R_lcb95 (SBD, reported) | paired_effect (gate in PAIRED_LEVEL_SHIFT) | BA_point | BA_lcb95 (>= 0.80) | probe | status |
|---|---|---|---|---|---|---|---|---|---|
| SEG_BRIDGE_VS_HARBOR | STANDARD_SHAPE | 108/280 | 12/12 | 0.6815 | n/a (STANDARD_SHAPE) | 0.7656 | 0.6844 | 0 | METRIC_FAIL |
| SEG_BERTH_VS_DEPARTURE | STANDARD_SHAPE | 72/104 | 12/12 | 0.8231 | n/a (STANDARD_SHAPE) | 0.9900 | 0.9712 | 1 | METRIC_FAIL |
| DIR_UPSTREAM_VS_DOWNSTREAM | PAIRED_LEVEL_SHIFT | 3928/3247 | 60/60 | 0.6252 | pass=0 | 0.6866 | 0.6354 | 0 | METRIC_FAIL |
| DEPTH_LOW_ID_VS_NORMAL_ID | STANDARD_SHAPE | 699/704 | 12/12 | 0.5461 | n/a (STANDARD_SHAPE) | NaN | NaN | 0 | INSUFFICIENT_IDENTIFIABILITY_EVIDENCE |
| SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE | STANDARD_SHAPE | 312/1088 | 12/12 | 0.6217 | n/a (STANDARD_SHAPE) | 0.9547 | 0.9161 | 1 | METRIC_FAIL |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | STANDARD_SHAPE | 214/1532 | 35/35 | 0.5049 | n/a (STANDARD_SHAPE) | 0.7305 | 0.6648 | 0 | METRIC_FAIL |

### Per-contrast detail (required)

#### SEG_BRIDGE_VS_HARBOR (METRIC_FAIL)

- factor axis: segment_role; class_A=bridge; class_B=harbor; gate_role=REQUIRED
- metric profile (registry-routed): STANDARD_SHAPE; sbd_gate_bearing=1; judges: sbd; probe
- matched cohort: 5 mixed cells; applied: regime (same); direction (same); template (same_when_feasible: kept); operator (same_when_feasible: kept); relaxed: none
- pairs: cross 7520, within_A 1506, within_B 9204
- SBD (reported): median_cross 0.5508, Q0.95 within_A 0.7637, Q0.95 within_B 0.7328, R_point 0.7212, R_lcb95 0.6815 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.7656, BA_lcb95 0.6844 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.7846, 0.7946, 0.7448, 0.6250, 0.8792]
- SBD gate: R_lcb95 0.6815 >= R_min 1.5 -> 0

#### SEG_BERTH_VS_DEPARTURE (METRIC_FAIL)

- factor axis: segment_role; class_A=berth; class_B=departure; gate_role=REQUIRED
- metric profile (registry-routed): STANDARD_SHAPE; sbd_gate_bearing=1; judges: sbd; probe
- matched cohort: 1 mixed cells; applied: template (same); relaxed: none
- pairs: cross 7488, within_A 2556, within_B 5356
- SBD (reported): median_cross 0.8576, Q0.95 within_A 1.0000, Q0.95 within_B 0.6674, R_point 0.8576, R_lcb95 0.8231 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.9900, BA_lcb95 0.9712 (bootstrap 1000/1000 valid, seed 42902); fold BAs [1.0000, 1.0000, 0.9500, 1.0000, 1.0000]
- SBD gate: R_lcb95 0.8231 >= R_min 1.5 -> 0

#### DIR_UPSTREAM_VS_DOWNSTREAM (METRIC_FAIL)

- factor axis: direction; class_A=upstream; class_B=downstream; gate_role=REQUIRED
- metric profile (registry-routed): PAIRED_LEVEL_SHIFT; sbd_gate_bearing=0; judges: paired_physical_effect; probe
- SBD reported as: SECONDARY_DIAGNOSTIC_NOT_APPLICABLE_TO_LEVEL_SHIFT_FACTOR
- gate profile: PASS_DIR = PASS_paired_physical_effect AND PASS_probe (both required; DIR is not BA-only)
- matched cohort: 60 mixed cells; applied: template (same); regime (same); cohort cell = family (registry pairing: same base_draw directional counterfactual); relaxed: none
- pairs: cross 255729, within_A 147389, within_B 129552
- SBD (reported): median_cross 0.4881, Q0.95 within_A 0.7459, Q0.95 within_B 0.6593, R_point 0.6544, R_lcb95 0.6252 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.6866, BA_lcb95 0.6354 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.6753, 0.6384, 0.7537, 0.7132, 0.6526]
- paired physical effect (gate): delta_point -94.3218 kW over 60 split groups (min -261.0049, max 208.4939); bootstrap 95% CI [-123.8021, -61.3464] excludes_zero=1; sign test k=14/60, p=1.000e+00 (alpha 0.05, ok=0)
- paired effect PASS = sign_ok=0 AND CI excludes zero=1 AND sign test ok=0 -> 0
- pure-host note: TRAFFIC class-A pure slices are asserted in structural conformance C2; DIR SBD magnitude carries no threshold (D1-A 83.8 kW is diagnostic only)

#### DEPTH_LOW_ID_VS_NORMAL_ID (INSUFFICIENT_IDENTIFIABILITY_EVIDENCE)

- factor axis: depth; class_A=low_id; class_B=normal_id; gate_role=REQUIRED
- metric profile (registry-routed): STANDARD_SHAPE; sbd_gate_bearing=1; judges: sbd; probe
- matched cohort: 1 mixed cells; applied: manifest layer==depth only (T429_IID_DIAGNOSTIC strict selection); template==WORK_MISSION (hard filter); regime==DRY (hard filter); relaxed: none
- pairs: cross 492096, within_A 243951, within_B 247456
- SBD (reported): median_cross 0.5531, Q0.95 within_A 1.0000, Q0.95 within_B 0.8504, R_point 0.5531, R_lcb95 0.5461 (bootstrap 1000/1000 valid, seed 42901)
- probe: status INSUFFICIENT
- SBD gate: R_lcb95 0.5461 >= R_min 1.5 -> 0
- FAIL-CLOSED: probe: grouped 5-fold CV produced an invalid fold (a class is absent in a test fold)

#### SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE (METRIC_FAIL)

- factor axis: mission_intensity; class_A=rush; class_B=return; gate_role=REQUIRED
- metric profile (registry-routed): STANDARD_SHAPE; sbd_gate_bearing=1; judges: sbd; probe
- matched cohort: 3 mixed cells; applied: template==EMERGENCY_RESPONSE (hard filter); regime (same); operator (same); reach (same_when_feasible: kept); relaxed: none
- pairs: cross 113088, within_A 16116, within_B 197168
- SBD (reported): median_cross 0.4633, Q0.95 within_A 0.6707, Q0.95 within_B 0.6596, R_point 0.6907, R_lcb95 0.6217 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.9547, BA_lcb95 0.9161 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.9583, 0.9563, 0.9615, 0.9446, 0.9528]
- SBD gate: R_lcb95 0.6217 >= R_min 1.5 -> 0

#### SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE (METRIC_FAIL)

- factor axis: traffic; class_A=traffic_follow; class_B=free_cruise; gate_role=REQUIRED
- metric profile (registry-routed): STANDARD_SHAPE; sbd_gate_bearing=1; judges: sbd; probe
- matched cohort: 35 mixed cells; applied: cohort cell = family (same base_draw traffic ON vs OFF physical counterfactual); relaxed: none
- pairs: cross 12069, within_A 855, within_B 43667
- SBD (reported): median_cross 0.5572, Q0.95 within_A 1.0000, Q0.95 within_B 1.0000, R_point 0.5572, R_lcb95 0.5049 (bootstrap 1000/1000 valid, seed 42901)
- probe: BA_point 0.7305, BA_lcb95 0.6648 (bootstrap 1000/1000 valid, seed 42902); fold BAs [0.7579, 0.6836, 0.6518, 0.7502, 0.8087]
- SBD gate: R_lcb95 0.5049 >= R_min 1.5 -> 0

## 5. Secondary diagnostics (non-gate; DGN-12 R9)

| contrast_id | metric_profile | n_slices A/B | n_groups A/B | R_point | R_lcb95 | BA_point | BA_lcb95 | status |
|---|---|---|---|---|---|---|---|---|
| SEG_HARBOR_VS_SHALLOW_CRUISE | STANDARD_SHAPE | 280/142 | 12/13 | 0.6836 | 0.6373 | 0.9880 | 0.9773 | METRIC_FAIL |
| SEG_BRIDGE_VS_SHALLOW_CRUISE | STANDARD_SHAPE | 224/142 | 24/13 | 0.6966 | 0.6694 | 0.9714 | 0.9397 | METRIC_FAIL |

## 6. Gate verdict

- overall: 0
- verdict: FAIL_INSUFFICIENT_EVIDENCE
- required contrasts PASS: 0 / 6

FR-547/FR-551: insufficient evidence or a failed required contrast is fail-closed; scenario redesign is the only legal remediation; thresholds/geometry/encoding are frozen and never adjusted after a failure

## 7. Method provenance

- metric routing: contrast registry rows (SBD_required / paired_physical_effect_required / gate_profile / paired_effect_profile); the contrast id is never a routing input (unit test: scripts/+mission_v3/tests/test_metric_routing.m)
- SBD semantics: dataset_v3/audit_leakage.py L3 (D-side); SBD = 1 - max NCC, NUMERIC_FLOOR 1e-12
- probe: base-MATLAB IRLS logistic (Statistics Toolbox functions probed absent, FD10); frozen encoding, grouped 5-fold CV by split_group_id, train-only standard scaler
- quantile rule: nearest-rank order statistic (point and bootstrap consistent; bootstrap carries group multiplicity)
- replay binding: env_realize(formal_seed, base_draw_index) + plan_compose; PREQUAL-AMEND-01 bitwise reuse evidence; per-artifact cross-checks in identifiability.m


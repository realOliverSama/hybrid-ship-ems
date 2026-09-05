# T-004 Segment Duration Census and T-429 Slice Geometry Freeze Proposal

Generated: 2026-09-05T08:14:14Z (UTC)
Method: pure registry arithmetic census; no Model E simulation; no T-429 metric execution.
Authority: .specs/phase4-c-mission-v3/{requirements,design,tasks}.md (FR-547, DGN-12 R8);
delegation spec experiments/mission_v3/T-004_delegation_spec.md.
All values below are PROPOSALS for main-agent adjudication; the freeze happens via T-007.

## 1. Inputs

| registry | declared self_hash (first 16) | computed file sha256 (first 16) |
|---|---|---|
| mission_template_registry.json | 81efdbb8d0aa0ea5...... | 373e58db31dbad51...... |
| identifiability_contrast_registry.json | 65f1051a95679b40...... | 0d08580a72a1c694...... |
| conditional_dag_config.json | 297c1dc8e9968eb8...... | 7eee82239b91bef7...... |

- Stretch factor lower bound: 0.8000 (source: config/mission_v3/conditional_dag_config.json, nodes[node_id=segment_stretch].params.low); high = 1.2500
- Geometry candidate: slice 120 s / edge buffer 30 s (both sides) / stride 60 s; hard constraint requires 180 s
- Usable duration formula: usable_s(role) = nominal_distance_m * stretch_low / v_role; standby: usable_s = nominal_duration_s
- Candidate speed table (m/s, PROPOSAL inputs, not frozen):

| role | speed m/s | basis |
|---|---|---|
| departure | 2.500 | census proposal candidate (main agent) |
| harbor | 2.500 | census proposal candidate (main agent) |
| berth | 2.000 | census proposal candidate (main agent) |
| bridge | 3.500 | census proposal candidate (main agent) |
| cruise | 5.500 | census proposal candidate (main agent) |
| patrol | 5.000 | census proposal candidate (main agent) |
| return | 5.500 | census proposal candidate (main agent) |
| rush | 7.522 | FR-510/T-412 G2 value_kn=14.620 (read-only reference); value fixed by delegation spec |
| mission | 5.000 | census proposal candidate (main agent) |
| work | 1.200 | low-speed work semantics (T-010 plan-layer candidate) |
| standby | n/a (duration-defined) | duration-defined segment; usable = nominal_duration_s (registry, 600 s); speed conversion not applicable (distance=0) |

## 2. Per-template role census (usable duration at stretch low)

Template legend: RP = ROUTINE_PATROL, ER = EMERGENCY_RESPONSE, WM = WORK_MISSION.

| template | role | seq | dist m | dur s | speed m/s | usable s | direction |
|---|---|---|---|---|---|---|---|
| RP | departure | 1 | 700 | - | 2.5 | 224.000 | upstream |
| RP | harbor | 2 | 1200 | - | 2.5 | 384.000 | upstream |
| RP | cruise | 3 | 4000 | - | 5.5 | 581.818 | upstream |
| RP | bridge | 4 | 1000 | - | 3.5 | 228.571 | upstream |
| RP | patrol | 5 | 2500 | - | 5 | 400.000 | upstream |
| RP | return | 6 | 9400 | - | 5.5 | 1367.273 | downstream |
| RP | berth | 7 | 500 | - | 2 | 200.000 | none |
| ER | standby | 1 | 0 | 600 | - | 600.000 | upstream |
| ER | departure | 2 | 700 | - | 2.5 | 224.000 | upstream |
| ER | rush | 3 | 4000 | - | 7.522 | 425.419 | upstream |
| ER | bridge | 4 | 1000 | - | 3.5 | 228.571 | upstream |
| ER | mission | 5 | 2000 | - | 5 | 320.000 | upstream |
| ER | return | 6 | 7700 | - | 5.5 | 1120.000 | downstream |
| WM | departure | 1 | 700 | - | 2.5 | 224.000 | upstream |
| WM | cruise | 2 | 4000 | - | 5.5 | 581.818 | upstream |
| WM | work | 3 | 2000 | - | 1.2 | 1333.333 | upstream |
| WM | return | 4 | 6700 | - | 5.5 | 974.545 | downstream |

## 3. Role minimum usable duration (cross-template)

| role | min usable s | achieved in | present in |
|---|---|---|---|
| departure | 224.000 | ROUTINE_PATROL, EMERGENCY_RESPONSE, WORK_MISSION | ROUTINE_PATROL, EMERGENCY_RESPONSE, WORK_MISSION |
| harbor | 384.000 | ROUTINE_PATROL | ROUTINE_PATROL |
| cruise | 581.818 | ROUTINE_PATROL, WORK_MISSION | ROUTINE_PATROL, WORK_MISSION |
| bridge | 228.571 | ROUTINE_PATROL, EMERGENCY_RESPONSE | ROUTINE_PATROL, EMERGENCY_RESPONSE |
| patrol | 400.000 | ROUTINE_PATROL | ROUTINE_PATROL |
| return | 974.545 | WORK_MISSION | ROUTINE_PATROL, EMERGENCY_RESPONSE, WORK_MISSION |
| berth | 200.000 | ROUTINE_PATROL | ROUTINE_PATROL |
| standby | 600.000 | EMERGENCY_RESPONSE | EMERGENCY_RESPONSE |
| rush | 425.419 | EMERGENCY_RESPONSE | EMERGENCY_RESPONSE |
| mission | 320.000 | EMERGENCY_RESPONSE | EMERGENCY_RESPONSE |
| work | 1333.333 | WORK_MISSION | WORK_MISSION |

## 4. Required contrast coverage and hard-constraint verdict (candidate geometry)

Hard constraint: slice + 2 x buffer = 180 s must be <= min usable duration per contrast class.

| contrast | class | hosts (role: templates) | min usable s | required s | verdict |
|---|---|---|---|---|---|
| SEG_BRIDGE_VS_HARBOR | A (bridge) | bridge (RP, ER) | 228.571 | 180 | PASS |
| SEG_BRIDGE_VS_HARBOR | B (harbor) | harbor (RP) | 384.000 | 180 | PASS |
| SEG_BERTH_VS_DEPARTURE | A (berth) | berth (RP) | 200.000 | 180 | PASS |
| SEG_BERTH_VS_DEPARTURE | B (departure) | departure (RP, ER, WM) | 224.000 | 180 | PASS |
| DIR_UPSTREAM_VS_DOWNSTREAM | A (upstream) | departure (RP, ER, WM); harbor (RP); cruise (RP, WM); bridge (RP, ER); patrol (RP); standby (ER); rush (ER); mission (ER); work (WM) | 224.000 | 180 | PASS |
| DIR_UPSTREAM_VS_DOWNSTREAM | B (downstream) | return (RP, ER, WM) | 974.545 | 180 | PASS |
| DEPTH_LOW_ID_VS_NORMAL_ID | A (low_id) | return (RP, ER); work (WM) | 1120.000 | 180 | PASS |
| DEPTH_LOW_ID_VS_NORMAL_ID | B (normal_id) | return (RP, ER); work (WM) | 1120.000 | 180 | PASS |
| SEG_EMERGENCY_SPRINT_VS_CRUISE | A (rush) | rush (ER) | 425.419 | 180 | PASS |
| SEG_EMERGENCY_SPRINT_VS_CRUISE | B (cruise) | cruise (RP, WM) | 581.818 | 180 | PASS |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | A (traffic_follow) | cruise (RP, WM) | 581.818 | 180 | PASS |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | B (free_cruise) | cruise (RP, WM) | 581.818 | 180 | PASS |

Excluded (not REQUIRED, no constraint obligation):
- SEG_HARBOR_VS_SHALLOW_CRUISE (SECONDARY_DIAGNOSTIC)
- SEG_BRIDGE_VS_SHALLOW_CRUISE (SECONDARY_DIAGNOSTIC)

## 5. Window arithmetic at candidate geometry

Slices per host at stride: n = floor((usable - 2*buffer - slice) / stride) + 1, 0 if ineligible.

| template | role | usable s | eligible | slices |
|---|---|---|---|---|
| RP | departure | 224.000 | yes | 1 |
| RP | harbor | 384.000 | yes | 4 |
| RP | cruise | 581.818 | yes | 7 |
| RP | bridge | 228.571 | yes | 1 |
| RP | patrol | 400.000 | yes | 4 |
| RP | return | 1367.273 | yes | 20 |
| RP | berth | 200.000 | yes | 1 |
| ER | standby | 600.000 | yes | 8 |
| ER | departure | 224.000 | yes | 1 |
| ER | rush | 425.419 | yes | 5 |
| ER | bridge | 228.571 | yes | 1 |
| ER | mission | 320.000 | yes | 3 |
| ER | return | 1120.000 | yes | 16 |
| WM | departure | 224.000 | yes | 1 |
| WM | cruise | 581.818 | yes | 7 |
| WM | work | 1333.333 | yes | 20 |
| WM | return | 974.545 | yes | 14 |

## 6. Proposal

Candidate geometry 120/30/60 overall verdict: **PASS**.

### Remediation candidates

#### R1 geometry-only global revision (slice 90 / buffer 30 / stride 45)

- Geometry: slice 90 s / buffer 30 s / stride 45 s (required 150 s)
- Verdict: ALL_SIX_CONTRASTS_PASS (check flags for freeze risk)
- Arithmetic:
  - required = 90 + 2*30 = 150 s

#### R2 geometry-only global revision (slice 60 / buffer 15 / stride 30)

- Geometry: slice 60 s / buffer 15 s / stride 30 s (required 90 s)
- Verdict: ALL_SIX_CONTRASTS_PASS (check flags for freeze risk)
- Flags: fragile_margin_on_bridge
- Arithmetic:
  - required = 60 + 2*15 = 90 s
  - bridge margin at R2 = usable 228.571429 s - required 90 s = 138.571429 s; in speed terms v_threshold = 1000*0.8/90 = 8.888889 m/s vs candidate 3.500000 m/s (+153.97%) - margin consumed by candidate-speed uncertainty

#### R3 keep candidate 120/30/60; T-010-level template/plan revisions for bridge and departure

- Geometry: slice 120 s / buffer 30 s / stride 60 s (required 180 s)
- Verdict: NOT_EVALUABLE_AS_IS (conditional on revisions)
- Fails: candidate geometry itself still fails for bridge/departure hosts until the registry/plan revisions land
- Flags: requires_template_revision; requires_main_agent_decision
- Arithmetic:
  - R3a bridge: usable >= 180 s at v=3.500000 m/s requires nominal_distance >= 180*3.500000/0.8 = 787.5 m (registry 1000 m; +-212.5 m); OR at 1000 m requires v <= 1000*0.8/180 = 4.444444 m/s (not a plausible bridge-transit plan vs candidate 3.500000 m/s)
  - R3b departure: at 700 m requires v <= 700*0.8/180 = 3.111111 m/s (candidate 2.500000 m/s; -24.44% slower plan); OR at v=2.500000 requires nominal_distance >= 180*2.500000/0.8 = 562.5 m (registry 700 m)
  - R3c all other hosts already pass at 120/30/60 (see contrast_coverage)

#### R4 per-contrast (or per-axis) geometry split

- Geometry: slice 120 s / buffer 30 s / stride 60 s (required 180 s)
- Verdict: NOT_DECIDABLE_IN_THIS_TASK
- Flags: requires_design_interpretation; possible_spec_amendment
- Arithmetic:
  - FR-547 defines the main analysis unit as SEGMENT_PURE_DIAGNOSTIC_SLICE with FIXED duration / FIXED edge buffer; whether one global geometry or per-axis geometries is allowed is a design interpretation question reserved to the main agent

### Freeze note

census proposes only; nothing is frozen here. Final geometry values are decided by the main agent and frozen via T-007 (tasks.md item 7).

## 7. Items for main-agent adjudication

- D1 (escalation per delegation spec Escalate-If): bridge host cannot satisfy the 180 s hard constraint under any plausible bridge-transit speed at registry nominal 1000 m (v_threshold = 4.444444 m/s). Choose between R2 (tighter global geometry, fragile margin), R3a (template-level bridge distance/speed revision at T-010), or R4 (per-contrast geometry; design interpretation needed).
- D2 departure host fails at 224.000000 s vs 180 s but is fixable by R1 (slice 90: 150 <= 224.000000), or a modest departure plan-speed reduction (<= 3.111111 m/s), or distance increase (>= 562.5 m).
- D3 standby is included as a DIR upstream host per the mechanical non-berth outbound rule, but it is a stationary segment; if the main agent excludes it from direction hosts, no numeric verdict changes (its 600 s is not the set minimum).
- D4 standby usable duration applies no stretch (duration-defined, distance=0). If T-007 prefers a conservative 0.8x duration stretch, usable = 480 s - still >= 180 s; no verdict change.
- D5 berth exists only in ROUTINE_PATROL; SEG_BERTH_VS_DEPARTURE declares same-template pairing. This census covers geometry only; cohort feasibility belongs to later tasks.
- D6 cruise does not appear in the EMERGENCY_RESPONSE segment sequence while SEG_EMERGENCY_SPRINT_VS_CRUISE cohort predicate pins template == EMERGENCY_RESPONSE; class_B host rows therefore live in ROUTINE_PATROL / WORK_MISSION (usable 581.818182 s, geometrically feasible). The cross-template pairing semantics belong to the main agent; the registry is read-only here.
- D7 the candidate speed table itself is a proposal (Fixed Decision 1); rush = 7.522 m/s is quoted as-is from the delegation spec (FR-510/T-412 G2 value_kn=14.620, read-only reference). Use SpeedOverride to run sensitivity variants.

## 8. Read-only statement

registries, Model E files and .specs are read-only for this task; all proposed values are proposals for main-agent adjudication (T-007 freeze)
Guards enforced (fail-closed, error ids T004:G*):
- G1 stretch lower bound must exist (segment_stretch.params.low)
- G2 every role needs a positive candidate speed or duration-defined standby
- G3 exactly six REQUIRED contrasts expected
- G4 REQUIRED contrast ids and class values must match the delegation spec host rules
- hard-constraint comparison tolerance 1e-9 s

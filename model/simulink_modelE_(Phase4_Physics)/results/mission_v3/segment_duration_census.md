# T-004 Segment Duration Census and T-429 Slice Geometry Freeze Proposal

Generated: 2026-09-05T07:39:20Z (UTC)
Method: pure registry arithmetic census; no Model E simulation; no T-429 metric execution.
Authority: .specs/phase4-c-mission-v3/{requirements,design,tasks}.md (FR-547, DGN-12 R8);
delegation spec experiments/mission_v3/T-004_delegation_spec.md.
All values below are PROPOSALS for main-agent adjudication; the freeze happens via T-007.

## 1. Inputs

| registry | declared self_hash (first 16) | computed file sha256 (first 16) |
|---|---|---|
| mission_template_registry.json | b2da90cb37262d1e...... | 750ffcf9018e0498...... |
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
| RP | departure | 1 | 500 | - | 2.5 | 160.000 | upstream |
| RP | harbor | 2 | 1200 | - | 2.5 | 384.000 | upstream |
| RP | cruise | 3 | 4000 | - | 5.5 | 581.818 | upstream |
| RP | bridge | 4 | 400 | - | 3.5 | 91.429 | upstream |
| RP | patrol | 5 | 2500 | - | 5 | 400.000 | upstream |
| RP | return | 6 | 8600 | - | 5.5 | 1250.909 | downstream |
| RP | berth | 7 | 500 | - | 2 | 200.000 | none |
| ER | standby | 1 | 0 | 600 | - | 600.000 | upstream |
| ER | departure | 2 | 500 | - | 2.5 | 160.000 | upstream |
| ER | rush | 3 | 4000 | - | 7.522 | 425.419 | upstream |
| ER | bridge | 4 | 400 | - | 3.5 | 91.429 | upstream |
| ER | mission | 5 | 2000 | - | 5 | 320.000 | upstream |
| ER | return | 6 | 6900 | - | 5.5 | 1003.636 | downstream |
| WM | departure | 1 | 500 | - | 2.5 | 160.000 | upstream |
| WM | cruise | 2 | 4000 | - | 5.5 | 581.818 | upstream |
| WM | work | 3 | 2000 | - | 1.2 | 1333.333 | upstream |
| WM | return | 4 | 6500 | - | 5.5 | 945.455 | downstream |

## 3. Role minimum usable duration (cross-template)

| role | min usable s | achieved in | present in |
|---|---|---|---|
| departure | 160.000 | ROUTINE_PATROL, EMERGENCY_RESPONSE, WORK_MISSION | ROUTINE_PATROL, EMERGENCY_RESPONSE, WORK_MISSION |
| harbor | 384.000 | ROUTINE_PATROL | ROUTINE_PATROL |
| cruise | 581.818 | ROUTINE_PATROL, WORK_MISSION | ROUTINE_PATROL, WORK_MISSION |
| bridge | 91.429 | ROUTINE_PATROL, EMERGENCY_RESPONSE | ROUTINE_PATROL, EMERGENCY_RESPONSE |
| patrol | 400.000 | ROUTINE_PATROL | ROUTINE_PATROL |
| return | 945.455 | WORK_MISSION | ROUTINE_PATROL, EMERGENCY_RESPONSE, WORK_MISSION |
| berth | 200.000 | ROUTINE_PATROL | ROUTINE_PATROL |
| standby | 600.000 | EMERGENCY_RESPONSE | EMERGENCY_RESPONSE |
| rush | 425.419 | EMERGENCY_RESPONSE | EMERGENCY_RESPONSE |
| mission | 320.000 | EMERGENCY_RESPONSE | EMERGENCY_RESPONSE |
| work | 1333.333 | WORK_MISSION | WORK_MISSION |

## 4. Required contrast coverage and hard-constraint verdict (candidate geometry)

Hard constraint: slice + 2 x buffer = 180 s must be <= min usable duration per contrast class.

| contrast | class | hosts (role: templates) | min usable s | required s | verdict |
|---|---|---|---|---|---|
| SEG_BRIDGE_VS_HARBOR | A (bridge) | bridge (RP, ER) | 91.429 | 180 | FAIL |
| SEG_BRIDGE_VS_HARBOR | B (harbor) | harbor (RP) | 384.000 | 180 | PASS |
| SEG_BERTH_VS_DEPARTURE | A (berth) | berth (RP) | 200.000 | 180 | PASS |
| SEG_BERTH_VS_DEPARTURE | B (departure) | departure (RP, ER, WM) | 160.000 | 180 | FAIL |
| DIR_UPSTREAM_VS_DOWNSTREAM | A (upstream) | departure (RP, ER, WM); harbor (RP); cruise (RP, WM); bridge (RP, ER); patrol (RP); standby (ER); rush (ER); mission (ER); work (WM) | 91.429 | 180 | FAIL |
| DIR_UPSTREAM_VS_DOWNSTREAM | B (downstream) | return (RP, ER, WM) | 945.455 | 180 | PASS |
| DEPTH_LOW_ID_VS_NORMAL_ID | A (low_id) | return (RP, ER); work (WM) | 1003.636 | 180 | PASS |
| DEPTH_LOW_ID_VS_NORMAL_ID | B (normal_id) | return (RP, ER); work (WM) | 1003.636 | 180 | PASS |
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
| RP | departure | 160.000 | no | 0 |
| RP | harbor | 384.000 | yes | 4 |
| RP | cruise | 581.818 | yes | 7 |
| RP | bridge | 91.429 | no | 0 |
| RP | patrol | 400.000 | yes | 4 |
| RP | return | 1250.909 | yes | 18 |
| RP | berth | 200.000 | yes | 1 |
| ER | standby | 600.000 | yes | 8 |
| ER | departure | 160.000 | no | 0 |
| ER | rush | 425.419 | yes | 5 |
| ER | bridge | 91.429 | no | 0 |
| ER | mission | 320.000 | yes | 3 |
| ER | return | 1003.636 | yes | 14 |
| WM | departure | 160.000 | no | 0 |
| WM | cruise | 581.818 | yes | 7 |
| WM | work | 1333.333 | yes | 20 |
| WM | return | 945.455 | yes | 13 |

## 6. Proposal

Candidate geometry 120/30/60 overall verdict: **FAIL**.

### Violating hosts (candidate geometry)

| contrast | class | template | role | usable s | deficit s | v max for pass (m/s) | dist needed at candidate speed (m) |
|---|---|---|---|---|---|---|---|
| SEG_BRIDGE_VS_HARBOR | A | RP | bridge | 91.429 | 88.571 | 1.777778 | 787.5 |
| SEG_BRIDGE_VS_HARBOR | A | ER | bridge | 91.429 | 88.571 | 1.777778 | 787.5 |
| SEG_BERTH_VS_DEPARTURE | B | RP | departure | 160.000 | 20.000 | 2.222222 | 562.5 |
| SEG_BERTH_VS_DEPARTURE | B | ER | departure | 160.000 | 20.000 | 2.222222 | 562.5 |
| SEG_BERTH_VS_DEPARTURE | B | WM | departure | 160.000 | 20.000 | 2.222222 | 562.5 |
| DIR_UPSTREAM_VS_DOWNSTREAM | A | RP | departure | 160.000 | 20.000 | 2.222222 | 562.5 |
| DIR_UPSTREAM_VS_DOWNSTREAM | A | RP | bridge | 91.429 | 88.571 | 1.777778 | 787.5 |
| DIR_UPSTREAM_VS_DOWNSTREAM | A | ER | departure | 160.000 | 20.000 | 2.222222 | 562.5 |
| DIR_UPSTREAM_VS_DOWNSTREAM | A | ER | bridge | 91.429 | 88.571 | 1.777778 | 787.5 |
| DIR_UPSTREAM_VS_DOWNSTREAM | A | WM | departure | 160.000 | 20.000 | 2.222222 | 562.5 |

### Remediation candidates

#### R1 geometry-only global revision (slice 90 / buffer 30 / stride 45)

- Geometry: slice 90 s / buffer 30 s / stride 45 s (required 150 s)
- Verdict: STILL_FAILS for the contrasts listed in fails_note
- Fails: still failing: SEG_BRIDGE_VS_HARBOR, DIR_UPSTREAM_VS_DOWNSTREAM
- Arithmetic:
  - required = 90 + 2*30 = 150 s

#### R2 geometry-only global revision (slice 60 / buffer 15 / stride 30)

- Geometry: slice 60 s / buffer 15 s / stride 30 s (required 90 s)
- Verdict: ALL_SIX_CONTRASTS_PASS (check flags for freeze risk)
- Flags: fragile_margin_on_bridge
- Arithmetic:
  - required = 60 + 2*15 = 90 s
  - bridge margin at R2 = usable 91.428571 s - required 90 s = 1.428571 s; in speed terms v_threshold = 400*0.8/90 = 3.555556 m/s vs candidate 3.500000 m/s (+1.59%) - margin consumed by candidate-speed uncertainty

#### R3 keep candidate 120/30/60; T-010-level template/plan revisions for bridge and departure

- Geometry: slice 120 s / buffer 30 s / stride 60 s (required 180 s)
- Verdict: NOT_EVALUABLE_AS_IS (conditional on revisions)
- Fails: candidate geometry itself still fails for bridge/departure hosts until the registry/plan revisions land
- Flags: requires_template_revision; requires_main_agent_decision
- Arithmetic:
  - R3a bridge: usable >= 180 s at v=3.500000 m/s requires nominal_distance >= 180*3.500000/0.8 = 787.5 m (registry 400 m; +387.5 m); OR at 400 m requires v <= 400*0.8/180 = 1.777778 m/s (not a plausible bridge-transit plan vs candidate 3.500000 m/s)
  - R3b departure: at 500 m requires v <= 500*0.8/180 = 2.222222 m/s (candidate 2.500000 m/s; 11.11% slower plan); OR at v=2.500000 requires nominal_distance >= 180*2.500000/0.8 = 562.5 m (registry 500 m)
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

- D1 (escalation per delegation spec Escalate-If): bridge host cannot satisfy the 180 s hard constraint under any plausible bridge-transit speed at registry nominal 400 m (v_threshold = 1.777778 m/s). Choose between R2 (tighter global geometry, fragile margin), R3a (template-level bridge distance/speed revision at T-010), or R4 (per-contrast geometry; design interpretation needed).
- D2 departure host fails at 160.000000 s vs 180 s but is fixable by R1 (slice 90: 150 <= 160.000000), or a modest departure plan-speed reduction (<= 2.222222 m/s), or distance increase (>= 562.5 m).
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

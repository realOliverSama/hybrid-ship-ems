%% test_propulsion_feasibility.m - fleet_v3 conservative 700 kW propulsion
% guard suite (T-018 Phase 4-D; activation D5 pipeline segment 4).
% Monotonicity (larger adverse flow / shallower water always demands more
% power), the rating boundary, and parameter-consumption fail-closed
% behavior.  Failures raise GATE:FLEETV3:TEST:PROPULSION.
% Discipline: ASCII-only; no absolute paths.

ra_root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ra_root) || ~exist(fullfile(ra_root, 'config'), 'dir')
    ra_root = pwd;
end
evalin('base', sprintf("run('%s')", ...
    fullfile(ra_root, 'config', 'ModelE_Physics_Parameters.m')));

% ---- monotonicity in the adverse flow (deep water) ----
ra_p0 = fleet_v3.propulsion_feasibility_check(0.0, 3.2);
ra_p1 = fleet_v3.propulsion_feasibility_check(0.5, 3.2);
ra_p2 = fleet_v3.propulsion_feasibility_check(1.0, 3.2);
assert(ra_p1.p_demand_kW > ra_p0.p_demand_kW ...
    && ra_p2.p_demand_kW > ra_p1.p_demand_kW, ...
    'GATE:FLEETV3:TEST:PROPULSION', 'power demand must grow with flow');

% ---- monotonicity in shallowness (fixed flow) ----
ra_q0 = fleet_v3.propulsion_feasibility_check(0.5, 3.2);  % deep asymptote
ra_q1 = fleet_v3.propulsion_feasibility_check(0.5, 2.0);
ra_q2 = fleet_v3.propulsion_feasibility_check(0.5, 1.4);
assert(ra_q1.p_demand_kW > ra_q0.p_demand_kW ...
    && ra_q2.p_demand_kW > ra_q1.p_demand_kW, ...
    'GATE:FLEETV3:TEST:PROPULSION', 'power demand must grow as H/T drops');
assert(ra_q0.domain_warning == false, ...
    'GATE:FLEETV3:TEST:PROPULSION', 'deep water must not warn');
assert(ra_q2.domain_warning == false, ...
    'GATE:FLEETV3:TEST:PROPULSION', 'ht=1.4 is inside the shallow domain');
ra_q3 = fleet_v3.propulsion_feasibility_check(0.5, 1.1);
assert(ra_q3.domain_warning == true ...
    && abs(ra_q3.ht_eff_used - 1.2) < 1e-12, ...
    'GATE:FLEETV3:TEST:PROPULSION', ...
    'ht below 1.2 must clamp with a domain warning (hull-block semantics)');

% ---- rating boundary: cruise flow feasible, A2-level flow not ----
ra_ok = fleet_v3.propulsion_feasibility_check(2.2, 3.2);
assert(ra_ok.feasible && ra_ok.p_demand_kW < ra_ok.p_rating_kW, ...
    'GATE:FLEETV3:TEST:PROPULSION', 'A1-level flow must stay inside 700 kW');
assert(abs(ra_ok.p_rating_kW - 700.0) < 1e-9, ...
    'GATE:FLEETV3:TEST:PROPULSION', 'rating must be the 700 kW Mot_P_nom');
ra_bad = fleet_v3.propulsion_feasibility_check(2.6, 3.2);
assert(~ra_bad.feasible && ra_bad.p_demand_kW > ra_bad.p_rating_kW, ...
    'GATE:FLEETV3:TEST:PROPULSION', ...
    'A2-level flow must exceed the 700 kW rating (conservative estimate)');

% ---- zero-flow cruise sanity (deep water, service speed) ----
% K_R anchored ~890 with the frozen margin/efficiency chain lands the
% clean-water 11 kn cruise around 330 kW - well inside the rating
assert(ra_p0.p_demand_kW > 250 && ra_p0.p_demand_kW < 700, ...
    'GATE:FLEETV3:TEST:PROPULSION', ...
    '11 kn cruise demand drifted outside the plausible band');

% ---- argument guards fail closed ----
ra_threw = false;
try
    fleet_v3.propulsion_feasibility_check(-0.1, 2.0);
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:PROP:ARG');
end
assert(ra_threw, 'GATE:FLEETV3:TEST:PROPULSION', ...
    'negative flow must fail closed');
ra_threw = false;
try
    fleet_v3.propulsion_feasibility_check(1.0, 0.0);
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:PROP:ARG');
end
assert(ra_threw, 'GATE:FLEETV3:TEST:PROPULSION', ...
    'non-positive H/T must fail closed');

fprintf('test_propulsion_feasibility: PASS (monotonicity + rating + clamp warn + arg guards)\n');

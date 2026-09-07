% test_scenario_compile.m - T-011: scenario_compile harness (DGN-08/09)
%   (delegation spec Required Work B, Fixed Decisions FD1-FD10)
%   P1 (spec B.1) field completeness: three archetypes (ROUTINE_PATROL /
%       EMERGENCY_RESPONSE / WORK_MISSION on NORMAL, WORK_MISSION on DRY)
%       full-chain compiles (dag_sample -> env_realize -> plan_compose ->
%       scenario_compile); the compiled scn field set equals the surveyed
%       existing Model E SCN contract field set exactly (constant below,
%       surveyed in config/ModelE_TestScenarios.m); the engine seed mirror
%       re-derives from seed_tree and matches the realization lineage.
%   P2 (spec B.2, FD3) hotel identity: per draw, scn.hotel_kW ==
%       p_hotel_base(template) + delta_p_hotel_unplanned(draw) with
%       tolerance 0 (the compiler re-derivation is bitwise-identical to
%       the base draw's dedicated hotel_residual substream value).
%   P3 (spec B.3, FD4) water depth identity: scn.water_depth_m ==
%       h_chart + delta_wl == h_effective (tolerance 0).
%   P4 (spec B.4, FD2) current sign mapping: inside a return segment
%       window current_value_mps == +U_river (downstream), inside an
%       outbound window == -U_river (adverse, S03 semantics), berth/standby
%       windows are exactly zero; the current grid is the 1 s grid 0:t_end
%       equal to the plan grid and consistent with the realization.
%   P5 (spec B.5, FD8) injection whitelist: every manifest var_name is in
%       the surveyed whitelist; the compiler whitelist gate passes the
%       manifest and fails closed with GATE:MISSION_V3:SCN:INJECT on a
%       hand-built out-of-whitelist entry (and on duplicates).
%   P6 (spec B.6) PMS_SOURCE_MODE and EMS_CTRL_LAW are explicitly present
%       in the manifest with value 0 (BASELINE_POLICY / baseline law).
%   P7 (spec B.7) determinism: two compile runs on the same inputs give
%       field-identical outputs (pure function, no randomness).
%   P8 (spec B.8) time mode: speed_profile_mode == 'time' and the v table
%       equals plan.t_grid_s / plan.v_plan_mps (full equality plus sampled
%       point checks); the manifest VOYAGE_V_* entries carry the same
%       arrays as the SCN fields.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_scenario_compile.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing; no
%   global RNG; no simulation is run; no file is written.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;

% surveyed existing Model E SCN contract fields (config/ModelE_TestScenarios.m
% struct constructor + the per-scenario appended time-mode fields; consumers
% verified in scripts/+model/run_scenario_sim.m and build_model_e.m)
SURVEYED_SCN_FIELDS = {'id', 'name', 'target_speed_mps', ...
    'voyage_distance_m', 'river_current_mps', 'resistance_profile', ...
    'profile_code', 'resistance_change_position_m', ...
    'resistance_change_amplitude', 'resistance_change_ramp_time_s', ...
    'hotel_kW', 'initial_SOC', 'random_seed', 'notes', ...
    'v_knots_x', 'v_knots_v', 'accel_ramp_mps2', 'decel_ramp_mps2', ...
    'resistance_bands', 'current_knots_x', 'current_knots_v', ...
    'water_depth_m', 'speed_profile_mode', 'v_time_s', 'v_value_mps', ...
    'current_time_s', 'current_value_mps', 'stop_time_s'};

% surveyed injection whitelist (mirror of the compiler whitelist constant;
% origins: ModelE_TestScenarios.m VOYAGE_* channels + run_scenario_sim.m
% assignin contract + ModelE_Control_Parameters.m PMS_SOURCE_MODE)
SURVEYED_INJECTION_NAMES = {'VOYAGE_SPEED_MODE', 'VOYAGE_V_TIME_S', ...
    'VOYAGE_V_VALUE_MPS', 'VOYAGE_CURRENT_MODE', 'VOYAGE_CURRENT_TIME_S', ...
    'VOYAGE_CURRENT_VALUE_MPS', 'VOYAGE_WATER_DEPTH_M', ...
    'PMS_SOURCE_MODE', 'EMS_CTRL_LAW'};

STRAT_R = 'STRAT_ROUTINE_PATROL__NORMAL';
STRAT_E = 'STRAT_EMERGENCY_RESPONSE__NORMAL';
STRAT_WN = 'STRAT_WORK_MISSION__NORMAL';
STRAT_WD = 'STRAT_WORK_MISSION__DRY';

%% P1 (B.1): field completeness over three archetypes x NORMAL/DRY
ok_p1 = false;
try
    ch_routine = local_chain('t011_p1_routine_01', 'ROUTINE_PATROL', ...
        'NORMAL', STRAT_R);
    ch_emerg = local_chain('t011_p1_emerg_01', 'EMERGENCY_RESPONSE', ...
        'NORMAL', STRAT_E);
    ch_work_n = local_chain('t011_p1_work_n_01', 'WORK_MISSION', ...
        'NORMAL', STRAT_WN);
    ch_work_d = local_chain('t011_p1_work_d_01', 'WORK_MISSION', ...
        'DRY', STRAT_WD);
    chains = {ch_routine, ch_emerg, ch_work_n, ch_work_d};

    ok_all = true;
    for q = 1:numel(chains)
        ch = chains{q};
        scn = ch.out.scn;
        want = sort(SURVEYED_SCN_FIELDS);
        got = sort(fieldnames(scn));
        ok_all = ok_all && isequal(want(:), got(:));
        ok_all = ok_all && isa(scn.random_seed, 'uint32') ...
            && (scn.random_seed == ch.seed_current_truth);
        ok_all = ok_all && (scn.initial_SOC == 0.6);   % FD6 default
        ok_all = ok_all && ischar(scn.name) && ischar(scn.notes);
        ok_all = ok_all && isfinite(scn.hotel_kW) ...
            && isfinite(scn.water_depth_m) ...
            && isfinite(scn.accel_ramp_mps2) && (scn.accel_ramp_mps2 > 0) ...
            && isfinite(scn.decel_ramp_mps2) && (scn.decel_ramp_mps2 > 0);
    end

    % emergency chain keeps the prime segments compileable (rush / standby)
    segs_e = ch_emerg.plan.segments;
    i_rush_e = local_find_role(segs_e, 'rush');
    ok_emerg = (i_rush_e > 0) ...
        && (segs_e(i_rush_e).v_platform_mps > 0) ...
        && isfield(ch_emerg.out.scn, 'hotel_kW');
    ok_p1 = ok_all && ok_emerg;
    fprintf(['[T] P1 field completeness: 4 chains, field set == survey ', ...
        'and seed mirror OK: %s\n'], local_verdict(ok_p1));
catch p1_err
    fprintf('[T] P1 exception: %s\n', p1_err.message);
end
n_fail = n_fail + ~ok_p1;

%% P2 (B.2, FD3): hotel identity per draw (tolerance 0)
ok_p2 = false;
try
    chains_p2 = {ch_routine, ch_emerg, ch_work_n, ch_work_d};
    ok_all = true;
    hotel_log = zeros(1, numel(chains_p2));
    for q = 1:numel(chains_p2)
        ch = chains_p2{q};
        scn = ch.out.scn;
        expect = ch.dag.p_hotel_base_kW + ch.dag.delta_p_hotel_unplanned;
        ok_all = ok_all && (scn.hotel_kW == expect);   % tolerance 0
        prov_h = ch.out.provenance.hotel_values;
        ok_all = ok_all && (prov_h.p_hotel_base_kW == ch.dag.p_hotel_base_kW) ...
            && (prov_h.delta_p_hotel_unplanned_kW ...
            == ch.dag.delta_p_hotel_unplanned);
        hotel_log(q) = scn.hotel_kW;
    end
    ok_p2 = ok_all;
    fprintf(['[T] P2 hotel identity: hotel_kW = [%.3f %.3f %.3f %.3f] ', ...
        'exact per draw: %s\n'], hotel_log, local_verdict(ok_p2));
catch p2_err
    fprintf('[T] P2 exception: %s\n', p2_err.message);
end
n_fail = n_fail + ~ok_p2;

%% P3 (B.3, FD4): water depth identity (tolerance 0)
ok_p3 = false;
try
    chains_p3 = {ch_routine, ch_emerg, ch_work_n, ch_work_d};
    ok_all = true;
    depth_log = zeros(1, numel(chains_p3));
    for q = 1:numel(chains_p3)
        ch = chains_p3{q};
        scn = ch.out.scn;
        ok_all = ok_all ...
            && (scn.water_depth_m == ch.rl.water.h_chart_m ...
            + ch.rl.water.delta_wl_m) ...
            && (scn.water_depth_m == ch.rl.water.h_effective_m);
        depth_log(q) = scn.water_depth_m;
    end
    ok_p3 = ok_all;
    fprintf(['[T] P3 water depth identity: depth_m = [%.3f %.3f %.3f ', ...
        '%.3f] exact: %s\n'], depth_log, local_verdict(ok_p3));
catch p3_err
    fprintf('[T] P3 exception: %s\n', p3_err.message);
end
n_fail = n_fail + ~ok_p3;

%% P4 (B.4, FD2): current sign mapping (return + / outbound -)
ok_p4 = false;
try
    ch = ch_routine;
    scn = ch.out.scn;
    tg = scn.current_time_s;
    cur = scn.current_value_mps;
    u = ch.rl.u_river_mps;
    i0 = find(ch.rl.t_grid_s >= 0, 1);

    % grid: 1 s, 0:t_end, equal to the plan grid (realization-consistent)
    ok_grid = all(diff(tg) == 1) && (tg(1) == 0) ...
        && (tg(end) == ch.rl.ou.t_end_s) ...
        && isequal(tg, ch.plan.t_grid_s) ...
        && (numel(cur) == numel(tg));

    % return window: along_sign = +1 -> current == +U_river exactly
    segs = ch.plan.segments;
    i_ret = local_find_role(segs, 'return');
    sg = segs(i_ret);
    idx = (tg >= sg.t_start_s) & (tg < sg.t_end_s);
    ok_ret = (sg.along_sign == 1) && any(idx) ...
        && isequal(cur(idx), u(i0 + find(idx) - 1));

    % outbound cruise window: along_sign = -1 -> current == -U_river
    i_cru = local_find_role(segs, 'cruise');
    sg = segs(i_cru);
    idx = (tg >= sg.t_start_s) & (tg < sg.t_end_s);
    ok_cru = (sg.along_sign == -1) && any(idx) ...
        && isequal(cur(idx), -u(i0 + find(idx) - 1));

    % berth window: along_sign = 0 -> current exactly zero
    i_ber = local_find_role(segs, 'berth');
    sg = segs(i_ber);
    idx = (tg >= sg.t_start_s) & (tg < sg.t_end_s);
    ok_ber = (sg.along_sign == 0) && any(idx) && all(cur(idx) == 0);

    % the manifest entry carries the same series as the SCN field
    ok_inj = isequal(local_inj_value(ch.out.injection_list, ...
        'VOYAGE_CURRENT_VALUE_MPS'), cur);
    ok_p4 = ok_grid && ok_ret && ok_cru && ok_ber && ok_inj;
    fprintf(['[T] P4 current mapping: grid_1s=%d return==+U=%d ', ...
        'cruise==-U=%d berth==0=%d manifest_same=%d: %s\n'], ...
        ok_grid, ok_ret, ok_cru, ok_ber, ok_inj, local_verdict(ok_p4));
catch p4_err
    fprintf('[T] P4 exception: %s\n', p4_err.message);
end
n_fail = n_fail + ~ok_p4;

%% P5 (B.5, FD8): injection whitelist fail-closed
ok_p5 = false;
try
    inj = ch_routine.out.injection_list;
    ok_names = true;
    for k = 1:numel(inj)
        ok_names = ok_names ...
            && ismember(inj(k).var_name, SURVEYED_INJECTION_NAMES);
    end
    ok_gate_pass = false;
    if ok_names
        ok_gate_pass = mission_v3.scenario_compile('check_injection', inj);
    end

    % hand-built out-of-whitelist entry -> GATE:MISSION_V3:SCN:INJECT
    bad = inj;
    bad(end + 1) = struct('var_name', 'VOYAGE_NOT_A_CHANNEL', 'value', 0);
    [trip_bad, id_bad] = local_expect_error_id(@() ...
        mission_v3.scenario_compile('check_injection', bad), ...
        'GATE:MISSION_V3:SCN:INJECT');

    % duplicate entries are also rejected (machine gate, fail-closed)
    dup = inj;
    dup(2) = inj(1);
    [trip_dup, ~] = local_expect_error_id(@() ...
        mission_v3.scenario_compile('check_injection', dup), ...
        'GATE:MISSION_V3:SCN:INJECT');

    ok_p5 = ok_names && ok_gate_pass && trip_bad && trip_dup;
    fprintf(['[T] P5 whitelist: %d entries all in survey, gate pass=%d ', ...
        'bad=%d (%s) dup=%d: %s\n'], numel(inj), ok_gate_pass, ...
        trip_bad, id_bad, trip_dup, local_verdict(ok_p5));
catch p5_err
    fprintf('[T] P5 exception: %s\n', p5_err.message);
end
n_fail = n_fail + ~ok_p5;

%% P6 (B.6): PMS_SOURCE_MODE / EMS_CTRL_LAW explicitly present
ok_p6 = false;
try
    inj = ch_routine.out.injection_list;
    v_pms = local_inj_value(inj, 'PMS_SOURCE_MODE');
    v_law = local_inj_value(inj, 'EMS_CTRL_LAW');
    ok_p6 = ~isempty(v_pms) && ~isempty(v_law) ...
        && (v_pms == 0) && (v_law == 0);
    fprintf(['[T] P6 explicit entries: PMS_SOURCE_MODE=%.1f ', ...
        'EMS_CTRL_LAW=%.1f: %s\n'], v_pms, v_law, local_verdict(ok_p6));
catch p6_err
    fprintf('[T] P6 exception: %s\n', p6_err.message);
end
n_fail = n_fail + ~ok_p6;

%% P7 (B.7): determinism (two compile runs, same inputs)
ok_p7 = false;
try
    out_a = mission_v3.scenario_compile('compile', ch_routine.plan, ...
        ch_routine.rl);
    out_b = mission_v3.scenario_compile('compile', ch_routine.plan, ...
        ch_routine.rl);
    ok_p7 = isequal(out_a, out_b);
    fprintf('[T] P7 determinism: field-identical compile outputs: %s\n', ...
        local_verdict(ok_p7));
catch p7_err
    fprintf('[T] P7 exception: %s\n', p7_err.message);
end
n_fail = n_fail + ~ok_p7;

%% P8 (B.8): time mode and v-table agreement with the plan
ok_p8 = false;
try
    chains_p8 = {ch_routine, ch_emerg, ch_work_n, ch_work_d};
    ok_all = true;
    for q = 1:numel(chains_p8)
        ch = chains_p8{q};
        scn = ch.out.scn;
        ok_all = ok_all && strcmp(scn.speed_profile_mode, 'time') ...
            && isequal(scn.v_time_s, ch.plan.t_grid_s) ...
            && isequal(scn.v_value_mps, ch.plan.v_plan_mps);
        % sampled multi-point comparison (5 interior samples)
        n = numel(ch.plan.t_grid_s);
        for s = round(linspace(2, n - 1, 5))
            ok_all = ok_all ...
                && (scn.v_time_s(s) == ch.plan.t_grid_s(s)) ...
                && (scn.v_value_mps(s) == ch.plan.v_plan_mps(s));
        end
        % manifest carries the same arrays as the SCN fields
        ok_all = ok_all && isequal(local_inj_value( ...
            ch.out.injection_list, 'VOYAGE_V_TIME_S'), scn.v_time_s) ...
            && isequal(local_inj_value( ...
            ch.out.injection_list, 'VOYAGE_V_VALUE_MPS'), scn.v_value_mps) ...
            && (local_inj_value(ch.out.injection_list, ...
            'VOYAGE_SPEED_MODE') == 1) ...
            && (local_inj_value(ch.out.injection_list, ...
            'VOYAGE_CURRENT_MODE') == 1) ...
            && (local_inj_value(ch.out.injection_list, ...
            'VOYAGE_WATER_DEPTH_M') == scn.water_depth_m);
    end
    ok_p8 = ok_all;
    fprintf(['[T] P8 time mode: mode+tables+manifest agree on 4 chains ', ...
        '(sampled points included): %s\n'], local_verdict(ok_p8));
catch p8_err
    fprintf('[T] P8 exception: %s\n', p8_err.message);
end
n_fail = n_fail + ~ok_p8;

%% summary
fprintf('[T] test_scenario_compile: %d/8 cases passed\n', 8 - n_fail);
if n_fail > 0
    error('GATE:test_scenario_compile %d case(s) failed', n_fail);
end
fprintf('test_scenario_compile: ALL PASS (8/8)\n');

%% ======================== local functions ==========================

function ch = local_chain(seed, template_id, regime_id, stratum_id)
% Full chain for one accepted draw: dag_sample (via the env_realize
% ledger, which keeps the accepted base draw inspectable) -> env_realize
% -> plan_compose -> scenario_compile.  Bounded draw scan per test
% precedent (acceptance rates are high; the scan advances base draws).
    max_scan = 60;
    rl = [];
    dag = [];
    for k = 0:(max_scan - 1)
        r = mission_v3.env_realize('sample', seed, k, template_id, ...
            regime_id, stratum_id, 'MaxAttempts', 1, ...
            'ExhaustAction', 'return_ledger');
        if ~r.accepted
            continue;
        end
        rl = r.realization;
        row_ok = r.ledger(r.n_attempts);
        dag = row_ok.draw;
        break;
    end
    if isempty(rl)
        error('GATE:test_scenario_compile', ...
            ['no accepted realization within %d draws (%s %s %s)'], ...
            max_scan, template_id, regime_id, stratum_id);
    end
    plan = mission_v3.plan_compose('compose', rl);
    out = mission_v3.scenario_compile('compile', plan, rl);

    ch = struct();
    ch.dag = dag;
    ch.rl = rl;
    ch.plan = plan;
    ch.out = out;

    % current_truth engine seed (independent seed_tree derivation)
    lin = mission_v3.seed_tree('branches', plan.formal_seed, ...
        plan.base_draw_index);
    e = [];
    for k = 1:numel(lin)
        if strcmp(lin(k).branch_name, 'current_truth')
            e = lin(k);
            break;
        end
    end
    if isempty(e)
        error('GATE:test_scenario_compile', ...
            'lineage lacks the current_truth branch');
    end
    ch.seed_current_truth = e.engine_seed_uint32;
end

function v = local_inj_value(inj, var_name)
% Value of one manifest entry by var_name ([] when absent).
    v = [];
    for k = 1:numel(inj)
        if strcmp(inj(k).var_name, var_name)
            v = inj(k).value;
            return;
        end
    end
end

function i = local_find_role(segs, role)
    i = 0;
    for k = 1:numel(segs)
        if strcmp(segs(k).role, role)
            i = k;
            return;
        end
    end
end

function [ok, id] = local_expect_error_id(fh, want_id)
    ok = false;
    id = 'NO_ERROR_RAISED';
    try
        fh();
    catch err
        id = char(string(err.identifier));
        ok = strcmp(id, want_id);
    end
end

function s = local_verdict(ok)
    if ok
        s = 'PASS';
    else
        s = 'FAIL';
    end
end

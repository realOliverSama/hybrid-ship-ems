% test_plan_compose.m - T-010: nav_constraints + plan_compose harness
%   (DGN-01/08; delegation spec Required Work C, FD1-FD10 v2)
%   P1 (spec C.1) per-condition plan structure: three archetypes (all
%       templates) x sampled regimes (ROUTINE/EMERGENCY/WORK on NORMAL,
%       WORK on DRY) - plan fields, t_grid 1 s from 0 to the realized
%       t_end, v_plan finite and >= 0, per-segment windows equal the T-009
%       windows, contiguous coverage, prime segments (standby window at
%       v = 0 before the rush, rush window at a constant positive
%       platform, bridge sub-phases present), operator scales, and a full
%       pointwise rebuild of v_plan from the plan's own segment platforms,
%       sub-phases, and zone tables (composition + grid bookkeeping).
%   P2 (spec C.2) bridge vs harbor semantic difference (tasks.md): same
%       template/regime - the bridge segment window carries the five
%       BRIDGE_OPERATION sub-phases with frozen fractions and a
%       v_transit = 0.7 * v_host plateau (modal grid value), the harbor
%       segment is a constant v_host with no sub-phases; zone structures
%       differ (bridge_zones covers exactly the bridge segment).
%   P3 (spec C.3, FD5 v2) emergency sprint joint feasibility:
%       platform == min(tier, W_max_interp(ht) + u_signed_min) exactly
%       (tier and W_max anchors read from the frozen files, floor parsed
%       from the sprint amendment sentence); platform <= tier always;
%       platform > sprint floor for normal accepted draws with visible
%       frontier clamping (platform < tier); a strong-current low-HT
%       injected draw (ParamOverride; realize mode directly - the OOD-side
%       path of phase1_freeze_amendment_sprint evidence) drives the
%       platform below the floor and compose fails closed with
%       GATE:MISSION_V3:PLAN:INFEASIBLE_SPRINT.
%   P4 (spec C.4) UKC limit: HT in [1.5, 2.0) injected (ParamOverride +
%       DraftM) on WORK_MISSION -> one speed_limit_zones entry on the SHOAL
%       work segment with v_limit == the FD3 formula exactly and the plan
%       respecting it; HT >= 2.0 -> no UKC zone.
%   P5 (spec C.5) traffic transform: single-event fixture (test-local
%       copy of the realization traffic table) on the cruise segment with
%       a STANDARD operator - plateau v_plan = ratio * host exactly, 10%
%       linear ramps (midpoint = (host + follow)/2), recovery to host
%       after the window, all other cruise points at host.
%   P6 (spec C.6, FR-511) departure/berth non-mirror: phase counts, name
%       sequences, fraction vectors, first/last phase speeds, and the
%       in-window velocity profiles are pairwise different.
%   P7 (spec C.7, FR-515) operator layering: URGENT/CALM cruise platform
%       = v_host x 1.05 / x 0.95 exactly, ramp scales 1.2 / 0.8; harbor,
%       bridge transit, and berth phase values identical between the two
%       runs (mission constraints precede style).
%   P8 (spec C.8) determinism: two compose runs on the same realization
%       give field-identical plans (pure function, no randomness).
%   P9 (spec C.9) plan profile example images: one v_plan(t) PNG per
%       archetype (segment-colored, zone overlays) into
%       results/mission_v3/plan_profile_examples/ (the only results/
%       output of this test, per the delegation instruction).
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_plan_compose.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing; no
%   global RNG; frozen values are read from the frozen files (never
%   hard-coded); only P9 writes to results/.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;

% frozen registry reads (values consumed, never hard-coded)
frz = local_read_json(fullfile(ROOT, 'results', 'mission_v3', ...
    'phase1_freeze.json'));
d6 = local_read_json(fullfile(ROOT, 'results', 'mission_v3', ...
    'phase1_freeze_amendment_d6.json'));
spr = local_read_json(fullfile(ROOT, 'results', 'mission_v3', ...
    'phase1_freeze_amendment_sprint.json'));

w_max_tbl = d6.signed_frontier_convention_accepted.w_max_table;
w_anchors = [local_w_max_anchor(w_max_tbl, '^HT_1_5$'), ...
    local_w_max_anchor(w_max_tbl, '^HT_2_0$'), ...
    local_w_max_anchor(w_max_tbl, '^HT_3_0$')];
tier_in = local_rush_tier_value(frz.plan_speed_table_frozen.rush_tiers, 'HT_in');
tier_ge = local_rush_tier_value(frz.plan_speed_table_frozen.rush_tiers, 'HT_ge');
sprint_floor = local_parse_floor(spr.rush_policy_refinement.infeasible_sprint_gate);

STRAT_R = 'STRAT_ROUTINE_PATROL__NORMAL';
STRAT_E = 'STRAT_EMERGENCY_RESPONSE__NORMAL';
STRAT_WN = 'STRAT_WORK_MISSION__NORMAL';
STRAT_WD = 'STRAT_WORK_MISSION__DRY';

%% P1 (C.1): per-condition plan structure (3 templates x sampled regimes)
ok_p1 = false;
rl_routine = [];
plan_routine = [];
rl_work_n = [];
plan_work_n = [];
try
    [rl_routine, plan_routine] = local_sample_and_compose( ...
        't010_p1_routine_01', 0, 'ROUTINE_PATROL', 'NORMAL', STRAT_R, 2.188, ...
        struct(), '');
    [rl_emerg, plan_emerg] = local_sample_and_compose( ...
        't010_p1_emerg_01', 0, 'EMERGENCY_RESPONSE', 'NORMAL', STRAT_E, ...
        2.188, struct(), '');
    [rl_work_n, plan_work_n] = local_sample_and_compose( ...
        't010_p1_work_n_01', 0, 'WORK_MISSION', 'NORMAL', STRAT_WN, 2.188, ...
        struct(), '');
    [rl_work_d, plan_work_d] = local_sample_and_compose( ...
        't010_p1_work_d_01', 0, 'WORK_MISSION', 'DRY', STRAT_WD, 2.188, ...
        struct(), '');

    plans = {plan_routine, plan_emerg, plan_work_n, plan_work_d};
    rls = {rl_routine, rl_emerg, rl_work_n, rl_work_d};
    ok_all = true;
    for q = 1:numel(plans)
        pl = plans{q};
        rw = rls{q};
        ok_all = ok_all && local_check_plan_structure(pl, rw, sprint_floor);
        ok_all = ok_all && (max(abs(local_rebuild_v(pl) - pl.v_plan_mps)) ...
            <= 1e-9);
    end

    % prime segments on the EMERGENCY plan: standby window at zero,
    % rush window at a constant positive platform, bridge sub-phases
    tg = plan_emerg.t_grid_s;
    vp = plan_emerg.v_plan_mps;
    segs = plan_emerg.segments;
    i_std = local_find_role(segs, 'standby');
    i_rush = local_find_role(segs, 'rush');
    i_brg = local_find_role(segs, 'bridge');
    ok_prime = (i_std > 0) && (i_rush > 0) && (i_brg > 0);
    if ok_prime
        sg_s = segs(i_std);
        sg_r = segs(i_rush);
        sg_b = segs(i_brg);
        vp_std = vp((tg >= sg_s.t_start_s) & (tg < sg_s.t_end_s));
        vp_rush = vp((tg >= sg_r.t_start_s) & (tg < sg_r.t_end_s));
        % rush window: constant platform except where traffic windows
        % min-compose lower (FD4 applies to every segment); the modal
        % value must be the platform and nothing may exceed it
        [mode_r, share_r] = local_mode_value(vp_rush);
        ok_prime = (numel(vp_std) > 0) && all(vp_std == 0) ...
            && (numel(vp_rush) > 0) ...
            && (max(vp_rush) <= sg_r.v_platform_mps + 1e-9) ...
            && (abs(mode_r - sg_r.v_platform_mps) <= 1e-9) ...
            && (share_r > 0.4) ...
            && (sg_r.v_platform_mps > 0) ...
            && (numel(sg_b.sub_phases) == 5);
        ok_prime = ok_prime && (vp(1) == 0);   % t = 0 inside standby
    end
    ok_p1 = ok_all && ok_prime;
    fprintf(['[T] P1 structure: 4 plans struct+rebuild=%d prime=%d ', ...
        '(routine/emerg/workN/workD): %s\n'], ok_all, ok_prime, ...
        local_verdict(ok_p1));
catch p1_err
    fprintf('[T] P1 exception: %s\n', p1_err.message);
end
n_fail = n_fail + ~ok_p1;

%% P2 (C.2): bridge vs harbor semantic difference
ok_p2 = false;
try
    tg = plan_routine.t_grid_s;
    vp = plan_routine.v_plan_mps;
    segs = plan_routine.segments;
    i_brg = local_find_role(segs, 'bridge');
    i_har = local_find_role(segs, 'harbor');
    ok = (i_brg > 0) && (i_har > 0);
    sg_b = segs(i_brg);
    sg_h = segs(i_har);
    v_host_b = sg_b.planned_speed_mps;   % frozen table host speed (3.5)
    v_host_h = sg_h.planned_speed_mps;   % frozen table host speed (2.5)

    % bridge: five sub-phases with the frozen fractions; plateau at
    % v_transit = 0.7 * v_host (modal value; traffic ramps are rare and
    % only ever lower isolated points)
    fr = [sg_b.sub_phases.frac];
    ok = ok && (numel(sg_b.sub_phases) == 5) ...
        && max(abs(fr - [0.15, 0.10, 0.55, 0.05, 0.15])) <= 1e-12;
    idx_b = (tg >= sg_b.t_start_s) & (tg < sg_b.t_end_s);
    [mode_b, share_b] = local_mode_value(vp(idx_b));
    ok = ok && abs(mode_b - 0.7 * v_host_b) <= 1e-9 && (share_b > 0.4);

    % harbor: constant v_host, no sub-phases
    ok = ok && isempty(sg_h.sub_phases);
    idx_h = (tg >= sg_h.t_start_s) & (tg < sg_h.t_end_s);
    [mode_h, share_h] = local_mode_value(vp(idx_h));
    ok = ok && abs(mode_h - v_host_h) <= 1e-9 && (share_h > 0.4);

    % zone structures differ: bridge_zones covers exactly the bridge
    % segment and never the harbor segment
    bzs = plan_routine.bridge_zones;
    ok = ok && (numel(bzs) == 1) && (bzs(1).segment_index == i_brg) ...
        && abs(bzs(1).v_transit_mps - 0.7 * v_host_b) <= 1e-12;
    ok_p2 = ok;
    fprintf(['[T] P2 bridge-vs-harbor: phases=%d plateau=%.3f ', ...
        '(0.7*%.1f) harbor_mode=%.3f zones=%d: %s\n'], ...
        numel(sg_b.sub_phases), mode_b, v_host_b, mode_h, numel(bzs), ...
        local_verdict(ok_p2));
catch p2_err
    fprintf('[T] P2 exception: %s\n', p2_err.message);
end
n_fail = n_fail + ~ok_p2;

%% P3 (C.3, FD5 v2): emergency sprint joint feasibility
ok_p3 = false;
try
    % (a) normal accepted draw with visible frontier clamping
    found = false;
    found_k = -1;
    plan_sp = [];
    rl_sp = [];
    for k = 0:29
        [rl_k, pl_k] = local_sample_and_compose('t010_p3_emerg_01', k, ...
            'EMERGENCY_RESPONSE', 'NORMAL', STRAT_E, 2.188, struct(), '');
        sg_r = rl_k.segments(local_find_role(rl_k.segments, 'rush'));
        ht_k = rl_k.water.ht_effective;
        tier_k = local_tier_for_ht(ht_k, tier_in, tier_ge);
        w_k = local_w_max_interp(ht_k, w_anchors);
        frontier_k = w_k + sg_r.u_signed_min;
        if frontier_k < tier_k
            found = true;
            found_k = k;
            rl_sp = rl_k;
            plan_sp = pl_k;
            break;
        end
    end
    ok_a = false;
    if found
        segs_sp = plan_sp.segments;
        i_r = local_find_role(segs_sp, 'rush');
        sg_r = segs_sp(i_r);
        ht = plan_sp.ht_effective;
        tier = local_tier_for_ht(ht, tier_in, tier_ge);
        w_max = local_w_max_interp(ht, w_anchors);
        u_min = rl_sp.segments(i_r).u_signed_min;
        expect = min(tier, w_max + u_min);
        rp = sg_r.rush_policy;
        ok_a = abs(sg_r.v_platform_mps - expect) <= 1e-12 ...
            && (sg_r.v_platform_mps <= tier + 1e-12) ...
            && (sg_r.v_platform_mps > sprint_floor) ...
            && (sg_r.v_platform_mps < tier - 1e-9) ...
            && abs(rp.tier_mps - tier) <= 1e-12 ...
            && abs(rp.w_max_mps - w_max) <= 1e-12 ...
            && abs(rp.u_signed_min_mps - u_min) <= 1e-12 ...
            && abs(plan_sp.sprint_floor_mps - sprint_floor) <= 1e-12 ...
            && rp.clamped_to_frontier;
    end

    % (b) strong-current low-HT injected draw -> fail-closed
    ov_sprint = struct('chart_depth_voyage_base', ...
        struct('mu', 3.6, 'sigma', 0.02, 'lower', 3.4), ...
        'regimes', struct('NORMAL', ...
        struct('u_river_mag', struct('mu', 1.45, 'sigma', 0.02, ...
        'lower', 0), ...
        'water_level_offset', struct('mu', 0.0, 'sigma', 0.02))));
    tripped = false;
    pre_ok = false;
    trip_ht = NaN;
    trip_frontier = NaN;
    for k = 0:9
        dag_k = mission_v3.dag_sample('draw', 't010_p3_sprint_01', k, ...
            'EMERGENCY_RESPONSE', 'NORMAL', STRAT_E, 'DraftM', 2.2, ...
            'ParamOverride', ov_sprint);
        rl_k = mission_v3.env_realize('realize', dag_k, 'auto', ...
            'ParamOverride', ov_sprint);
        ht_k = rl_k.water.ht_effective;
        segs_k = rl_k.segments;
        i_rush_k = local_find_role(segs_k, 'rush');
        sg_r = segs_k(i_rush_k);
        if (ht_k < 1.5) || ~sg_r.on_grid
            continue;
        end
        w_k = local_w_max_interp(ht_k, w_anchors);
        frontier_k = w_k + sg_r.u_signed_min;
        if frontier_k < sprint_floor
            pre_ok = true;
            trip_ht = ht_k;
            trip_frontier = frontier_k;
            [tripped] = local_expect_error_id(@() ...
                mission_v3.plan_compose('compose', rl_k), ...
                'GATE:MISSION_V3:PLAN:INFEASIBLE_SPRINT');
            break;
        end
    end
    ok_b = pre_ok && tripped;
    ok_p3 = ok_a && ok_b;
    fprintf(['[T] P3 sprint: clamp_draw=%d platform_ok=%d ', ...
        'infeasible(pre frontier=%.3f ht=%.2f floor=%.1f)=%d: %s\n'], ...
        found_k, ok_a, trip_frontier, trip_ht, sprint_floor, ok_b, ...
        local_verdict(ok_p3));
catch p3_err
    fprintf('[T] P3 exception: %s\n', p3_err.message);
end
n_fail = n_fail + ~ok_p3;

%% P4 (C.4): UKC speed limit on the SHOAL work segment
ok_p4 = false;
try
    % (a) HT in [1.5, 2.0): one zone with the exact FD3 formula value
    ov_ukc = struct('chart_depth_voyage_base', ...
        struct('mu', 3.6, 'sigma', 0.02, 'lower', 3.4), ...
        'regimes', struct('NORMAL', ...
        struct('water_level_offset', struct('mu', 0.0, 'sigma', 0.02))));
    [rl_ukc, plan_ukc] = local_sample_and_compose('t010_p4_ukc_01', 0, ...
        'WORK_MISSION', 'NORMAL', STRAT_WN, 2.2, ov_ukc, '');
    ht_u = plan_ukc.ht_effective;
    segs_u = plan_ukc.segments;
    i_w = local_find_role(segs_u, 'work');
    sg_w = segs_u(i_w);
    v_host_w = sg_w.planned_speed_mps;
    zl = plan_ukc.speed_limit_zones;
    ok_a = (ht_u >= 1.5) && (ht_u < 2.0) && (numel(zl) == 1) ...
        && (zl(1).segment_index == i_w) ...
        && strcmp(zl(1).reason, 'UKC_SHALLOW') ...
        && abs(zl(1).t_start_s - sg_w.t_start_s) <= 1e-9 ...
        && abs(zl(1).t_end_s - sg_w.t_end_s) <= 1e-9;
    v_expect = v_host_w * (0.5 + 0.5 * (ht_u - 1.5) / 0.5);
    ok_a = ok_a && abs(zl(1).v_limit_mps - v_expect) <= 1e-12;
    tg = plan_ukc.t_grid_s;
    vp = plan_ukc.v_plan_mps;
    idx_w = (tg >= sg_w.t_start_s) & (tg < sg_w.t_end_s);
    ok_a = ok_a && all(vp(idx_w) <= v_expect + 1e-9) ...
        && all(vp(idx_w) >= 0) ...
        && (max(vp(idx_w)) >= v_expect - 1e-9);

    % (b) HT >= 2.0: no UKC zone
    ok_b = isempty(plan_work_n.speed_limit_zones) ...
        && (plan_work_n.ht_effective >= 2.0);
    ok_p4 = ok_a && ok_b;
    fprintf(['[T] P4 UKC: ht=%.3f v_limit=%.4f (formula exact=%d) ', ...
        'shallow_zone=%d deep_zone_empty=%d: %s\n'], ht_u, ...
        zl(1).v_limit_mps, ok_a, ok_a, ok_b, local_verdict(ok_p4));
catch p4_err
    fprintf('[T] P4 exception: %s\n', p4_err.message);
end
n_fail = n_fail + ~ok_p4;

%% P5 (C.5): traffic event transform (single-event fixture)
ok_p5 = false;
try
    [rl_tr, plan_tr] = local_sample_and_compose('t010_p5_traffic_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 2.188, struct(), 'STANDARD');
    segs_tr = plan_tr.segments;
    i_c = local_find_role(segs_tr, 'cruise');
    sg_c = segs_tr(i_c);
    rl_sgc = rl_tr.segments(i_c);
    v_host_c = sg_c.planned_speed_mps;
    v_base_c = v_host_c * plan_tr.operator_speed_scale;   % STANDARD: v_host

    % build the single-event fixture on a test-local copy
    rl2 = rl_tr;
    t_abs = ceil(sg_c.t_start_s + 0.5 * sg_c.window_s);
    ev = struct('segment_index', i_c, 'role', rl_sgc.role, ...
        'reach', rl_sgc.reach, 't_start_rel_s', t_abs - rl_sgc.t_start_nominal_s, ...
        't_start_abs_s', double(t_abs), 'duration_s', 100.0, ...
        'speed_ratio', 0.6);
    rl2.traffic = ev;
    plan_ev = mission_v3.plan_compose('compose', rl2);

    tws = plan_ev.traffic_windows;
    ok = numel(tws) == 1;
    tw = tws(1);
    v_follow = 0.6 * v_host_c;
    ok = ok && (tw.segment_index == i_c) ...
        && abs(tw.t_start_abs_s - t_abs) <= 1e-9 ...
        && abs(tw.duration_s - 100.0) <= 1e-9 ...
        && abs(tw.speed_ratio - 0.6) <= 1e-12 ...
        && abs(tw.v_host_mps - v_host_c) <= 1e-12 ...
        && abs(tw.v_follow_mps - v_follow) <= 1e-12 ...
        && abs(tw.t_ramp_start_s - (t_abs - 10)) <= 1e-9 ...
        && abs(tw.t_plateau_start_s - t_abs) <= 1e-9 ...
        && abs(tw.t_plateau_end_s - (t_abs + 100)) <= 1e-9 ...
        && abs(tw.t_ramp_end_s - (t_abs + 110)) <= 1e-9;

    tg = plan_ev.t_grid_s;
    vp = plan_ev.v_plan_mps;
    ok = ok && local_val_at(tg, vp, t_abs + 50) == v_follow;
    ok = ok && abs(local_val_at(tg, vp, t_abs - 5) ...
        - 0.5 * (v_base_c + v_follow)) <= 1e-9;
    ok = ok && abs(local_val_at(tg, vp, t_abs + 105) ...
        - 0.5 * (v_base_c + v_follow)) <= 1e-9;
    ok = ok && local_val_at(tg, vp, t_abs - 12) == v_base_c;
    ok = ok && local_val_at(tg, vp, t_abs + 112) == v_base_c;
    % every cruise point outside the event influence stays at v_host
    idx_c = (tg >= sg_c.t_start_s) & (tg < sg_c.t_end_s);
    out_idx = idx_c & ((tg < tw.t_ramp_start_s) | (tg > tw.t_ramp_end_s));
    ok = ok && all(vp(out_idx) == v_base_c);
    ok_p5 = ok;
    fprintf(['[T] P5 traffic: plateau=%.3f (0.6*%.1f) ramp_mid=%.3f ', ...
        'recover=%.3f others_host=%d: %s\n'], ...
        local_val_at(tg, vp, t_abs + 50), v_host_c, ...
        local_val_at(tg, vp, t_abs - 5), ...
        local_val_at(tg, vp, t_abs + 112), all(vp(out_idx) == v_base_c), ...
        local_verdict(ok_p5));
catch p5_err
    fprintf('[T] P5 exception: %s\n', p5_err.message);
end
n_fail = n_fail + ~ok_p5;

%% P6 (C.6, FR-511): departure/berth non-mirror
ok_p6 = false;
try
    segs = plan_routine.segments;
    i_d = local_find_role(segs, 'departure');
    i_b = local_find_role(segs, 'berth');
    sg_d = segs(i_d);
    sg_b = segs(i_b);
    v_host_d = sg_d.planned_speed_mps;   % frozen table (2.5)
    v_host_b = sg_b.planned_speed_mps;   % frozen table (2.0, NOT mirrored)
    fr_d = [sg_d.sub_phases.frac];
    fr_b = [sg_b.sub_phases.frac];
    ok = (numel(sg_d.sub_phases) == 3) && (numel(sg_b.sub_phases) == 3);
    ok = ok && max(abs(fr_d - [0.10, 0.30, 0.60])) <= 1e-12 ...
        && max(abs(fr_b - [0.40, 0.40, 0.20])) <= 1e-12;
    % direction and end values differ: departure starts at 1.0 and ramps
    % UP to its host speed; berth starts at its own (lower) host speed and
    % ramps DOWN to 0.5
    ok = ok && abs(sg_d.sub_phases(1).v_start_mps - 1.0) <= 1e-12 ...
        && abs(sg_d.sub_phases(3).v_end_mps - v_host_d) <= 1e-12 ...
        && (sg_d.sub_phases(3).v_end_mps > sg_d.sub_phases(3).v_start_mps);
    ok = ok && abs(sg_b.sub_phases(1).v_start_mps - v_host_b) <= 1e-12 ...
        && abs(sg_b.sub_phases(3).v_end_mps - 0.5) <= 1e-12 ...
        && (sg_b.sub_phases(1).v_end_mps < sg_b.sub_phases(1).v_start_mps);
    ok = ok && (v_host_b ~= v_host_d);   % non-mirror also in host speeds
    ok = ok && ~isequal(fr_d, fr_b) ...
        && ~isequal({sg_d.sub_phases.name}, {sg_b.sub_phases.name});
    % the two in-window velocity profiles are not elementwise equal
    tg = plan_routine.t_grid_s;
    vp_d = vp_of(tg, plan_routine.v_plan_mps, sg_d.t_start_s, sg_d.t_end_s);
    vp_b = vp_of(tg, plan_routine.v_plan_mps, sg_b.t_start_s, sg_b.t_end_s);
    n_cmp = min(numel(vp_d), numel(vp_b));
    ok = ok && ~isequal(vp_d(1:n_cmp), vp_b(1:n_cmp));
    ok_p6 = ok;
    fprintf(['[T] P6 non-mirror: dep[%.1f->%.1f] berth[%.1f->%.1f] ', ...
        'fracs_diff=%d profiles_diff=%d: %s\n'], ...
        sg_d.sub_phases(1).v_start_mps, sg_d.sub_phases(3).v_end_mps, ...
        sg_b.sub_phases(1).v_start_mps, sg_b.sub_phases(3).v_end_mps, ...
        ~isequal(fr_d, fr_b), ~isequal(vp_d(1:n_cmp), vp_b(1:n_cmp)), ...
        local_verdict(ok_p6));
catch p6_err
    fprintf('[T] P6 exception: %s\n', p6_err.message);
end
n_fail = n_fail + ~ok_p6;

%% P7 (C.7, FR-515): operator layering
ok_p7 = false;
try
    [~, plan_urg] = local_sample_and_compose('t010_p7_urgent_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 2.188, struct(), 'URGENT');
    [~, plan_calm] = local_sample_and_compose('t010_p7_calm_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 2.188, struct(), 'CALM');
    ok = strcmp(plan_urg.operator_profile, 'URGENT') ...
        && strcmp(plan_calm.operator_profile, 'CALM') ...
        && abs(plan_urg.operator_speed_scale - 1.05) <= 1e-12 ...
        && abs(plan_calm.operator_speed_scale - 0.95) <= 1e-12 ...
        && abs(plan_urg.operator_ramp_scale - 1.2) <= 1e-12 ...
        && abs(plan_calm.operator_ramp_scale - 0.8) <= 1e-12;

    % cruise platform = v_host x scale exactly, on both runs
    s_u = plan_urg.segments;
    s_c = plan_calm.segments;
    i_cu = local_find_role(s_u, 'cruise');
    i_cc = local_find_role(s_c, 'cruise');
    v_h_u = s_u(i_cu).planned_speed_mps;
    v_h_c = s_c(i_cc).planned_speed_mps;
    ok = ok && abs(s_u(i_cu).v_platform_mps - v_h_u * 1.05) <= 1e-12 ...
        && abs(s_c(i_cc).v_platform_mps - v_h_c * 0.95) <= 1e-12;

    % harbor stays at the raw v_host even for URGENT
    i_hu = local_find_role(s_u, 'harbor');
    ok = ok && abs(s_u(i_hu).v_platform_mps ...
        - s_u(i_hu).planned_speed_mps) <= 1e-12;

    % bridge transit and berth phase values identical between the runs
    bz_u = plan_urg.bridge_zones;
    bz_c = plan_calm.bridge_zones;
    ok = ok && (numel(bz_u) == 1) && (numel(bz_c) == 1) ...
        && abs(bz_u(1).v_transit_mps - bz_c(1).v_transit_mps) <= 1e-12;
    ph_u = [bz_u(1).phases.v_start_mps];
    ph_c = [bz_c(1).phases.v_start_mps];
    ok = ok && isequal(ph_u, ph_c);
    i_bu = local_find_role(s_u, 'berth');
    i_bc = local_find_role(s_c, 'berth');
    ok = ok && isequal([s_u(i_bu).sub_phases.v_start_mps], ...
        [s_c(i_bc).sub_phases.v_start_mps]) ...
        && isequal([s_u(i_bu).sub_phases.v_end_mps], ...
        [s_c(i_bc).sub_phases.v_end_mps]);
    ok_p7 = ok;
    fprintf(['[T] P7 operator: urgent_cruise=%.4f (1.05x%.1f) ', ...
        'calm_cruise=%.4f (0.95x%.1f) harbor_raw=%d bridge_ident=%d ', ...
        'berth_ident=%d: %s\n'], s_u(i_cu).v_platform_mps, v_h_u, ...
        s_c(i_cc).v_platform_mps, v_h_c, ...
        abs(s_u(i_hu).v_platform_mps - s_u(i_hu).planned_speed_mps) <= 1e-12, ...
        abs(bz_u(1).v_transit_mps - bz_c(1).v_transit_mps) <= 1e-12, ...
        isequal([s_u(i_bu).sub_phases.v_end_mps], ...
        [s_c(i_bc).sub_phases.v_end_mps]), local_verdict(ok_p7));
catch p7_err
    fprintf('[T] P7 exception: %s\n', p7_err.message);
end
n_fail = n_fail + ~ok_p7;

%% P8 (C.8): determinism (two compose runs, same realization)
ok_p8 = false;
try
    plan_a = mission_v3.plan_compose('compose', rl_routine);
    plan_b = mission_v3.plan_compose('compose', rl_routine);
    ok_p8 = isequal(plan_a, plan_b);
    fprintf('[T] P8 determinism: field-identical plans: %s\n', ...
        local_verdict(ok_p8));
catch p8_err
    fprintf('[T] P8 exception: %s\n', p8_err.message);
end
n_fail = n_fail + ~ok_p8;

%% P9 (C.9): plan profile example images (three archetypes)
ok_p9 = false;
try
    out_dir = fullfile(ROOT, 'results', 'mission_v3', ...
        'plan_profile_examples');
    if ~exist(out_dir, 'dir')
        mkdir(out_dir);
    end
    f1 = local_plot_plan(plan_routine, 'plan_profile_routine_patrol', ...
        out_dir);
    f2 = local_plot_plan(plan_emerg, 'plan_profile_emergency_response', ...
        out_dir);
    f3 = local_plot_plan(plan_work_n, 'plan_profile_work_mission', out_dir);
    ok_p9 = local_png_ok(f1) && local_png_ok(f2) && local_png_ok(f3);
    fprintf(['[T] P9 examples: routine=%d bytes, emergency=%d bytes, ', ...
        'work=%d bytes: %s\n'], local_png_bytes(f1), ...
        local_png_bytes(f2), local_png_bytes(f3), local_verdict(ok_p9));
catch p9_err
    fprintf('[T] P9 exception: %s\n', p9_err.message);
end
n_fail = n_fail + ~ok_p9;

%% summary
fprintf('[T] test_plan_compose: %d/9 cases passed\n', 9 - n_fail);
if n_fail > 0
    error('GATE:test_plan_compose %d case(s) failed', n_fail);
end
fprintf('test_plan_compose: ALL PASS (9/9)\n');

%% ======================== local functions ==========================

function obj = local_read_json(ppath)
% Frozen files are consumed with jsondecode (seed_tree.m precedent).
    obj = jsondecode(fileread(ppath));
end

function [rl, plan] = local_sample_and_compose(seed, draw_start, ...
        template_id, regime_id, stratum_id, draft, ov, want_op)
% Sample one accepted realization (single base draw attempt per call
% site scan) and compose the plan.  want_op '' accepts any operator
% profile; otherwise the scan advances base draws until the requested
% operator shows up (bounded; ROUTINE priors make URGENT the rarest).
    max_scan = 60;
    rl = [];
    for k = 0:(max_scan - 1)
        bdi = draw_start + k;
        r = mission_v3.env_realize('sample', seed, bdi, template_id, ...
            regime_id, stratum_id, 'DraftM', draft, 'MaxAttempts', 1, ...
            'ExhaustAction', 'return_ledger', 'ParamOverride', ov);
        if ~r.accepted
            continue;
        end
        z = r.realization;
        if isempty(want_op) || strcmp(z.operator_profile, want_op)
            rl = z;
            break;
        end
    end
    if isempty(rl)
        error('GATE:test_plan_compose', ...
            ['no accepted realization with the requested properties ', ...
             'within %d draws (%s %s %s)'], max_scan, template_id, ...
            regime_id, want_op);
    end
    plan = mission_v3.plan_compose('compose', rl);
end

function ok = local_check_plan_structure(pl, rw, sprint_floor)
% Structural contract of one plan against its realization.
    want_fields = {'kind', 'formal_seed', 'base_draw_index', 'stratum_id', ...
        'template_id', 'regime_id', 'operator_profile', ...
        'operator_speed_scale', 'operator_ramp_scale', 't_grid_s', ...
        'v_plan_mps', 'segments', 'speed_limit_zones', 'bridge_zones', ...
        'traffic_windows', 'ht_effective', 'sprint_floor_mps', ...
        'provenance'};
    ok = isstruct(pl);
    for j = 1:numel(want_fields)
        ok = ok && isfield(pl, want_fields{j});
    end
    if ~ok
        return;
    end
    ok = strcmp(pl.kind, 'mission_v3_plan') ...
        && strcmp(pl.formal_seed, rw.formal_seed) ...
        && (pl.base_draw_index == rw.base_draw_index) ...
        && strcmp(pl.template_id, rw.template_id) ...
        && strcmp(pl.regime_id, rw.regime_id) ...
        && strcmp(pl.operator_profile, rw.operator_profile) ...
        && abs(pl.ht_effective - rw.water.ht_effective) <= 1e-12 ...
        && abs(pl.sprint_floor_mps - 5.5) <= 1e-12;
    tg = pl.t_grid_s;
    vp = pl.v_plan_mps;
    ok = ok && (numel(tg) == numel(vp)) ...
        && (tg(1) == 0) ...
        && (numel(tg) == round(rw.ou.t_end_s) + 1) ...
        && (tg(end) == rw.ou.t_end_s) ...
        && all(diff(tg) == 1) ...
        && all(isfinite(vp)) && all(vp >= 0);
    segs = pl.segments;
    rsegs = rw.segments;
    ok = ok && (numel(segs) == numel(rsegs));
    t_acc = 0.0;
    for i = 1:numel(segs)
        sg = segs(i);
        rs = rsegs(i);
        ok = ok && strcmp(sg.role, rs.role) && strcmp(sg.reach, rs.reach) ...
            && strcmp(sg.direction, rs.direction) ...
            && (sg.along_sign == rs.along_sign) ...
            && abs(sg.window_s - rs.window_s) <= 1e-9 ...
            && abs(sg.t_start_s - t_acc) <= 1e-9;
        t_acc = t_acc + sg.window_s;
        ok = ok && abs(sg.t_end_s - t_acc) <= 1e-9;
        if ~isempty(rs.planned_speed_mps)
            ok = ok && abs(sg.planned_speed_mps - rs.planned_speed_mps) ...
                <= 1e-12;
        end
    end
    % operator scales (FD7 named values)
    op = pl.operator_profile;
    if strcmp(op, 'URGENT')
        ok = ok && (pl.operator_speed_scale == 1.05) ...
            && (pl.operator_ramp_scale == 1.2);
    elseif strcmp(op, 'CALM')
        ok = ok && (pl.operator_speed_scale == 0.95) ...
            && (pl.operator_ramp_scale == 0.8);
    else
        ok = ok && (pl.operator_speed_scale == 1.0) ...
            && (pl.operator_ramp_scale == 1.0);
    end
end

function v_rec = local_rebuild_v(pl)
% Rebuild v_plan from the plan's own segment platforms / sub-phases and
% zone tables (verifies grid bookkeeping, window coverage, tail hold, and
% the min-composition without re-deriving the role semantics).
    tg = pl.t_grid_s;
    n = numel(tg);
    v_base = NaN(1, n);
    segs = pl.segments;
    t_last_end = 0.0;
    for i = 1:numel(segs)
        sg = segs(i);
        idx = (tg >= sg.t_start_s) & (tg < sg.t_end_s);
        if isempty(sg.sub_phases)
            v_seg = sg.v_platform_mps * ones(1, sum(idx));
        else
            v_seg = mission_v3.nav_constraints('eval_phases', ...
                sg.sub_phases, tg(idx));
        end
        v_base(idx) = v_seg;
        t_last_end = sg.t_end_s;
    end
    tail = tg >= t_last_end;
    if any(tail)
        first_tail = find(tail, 1);
        if first_tail > 1
            v_base(tail) = v_base(first_tail - 1);
        end
    end
    v_lim = inf(1, n);
    zl = pl.speed_limit_zones;
    for k = 1:numel(zl)
        z = zl(k);
        idx = (tg >= z.t_start_s) & (tg < z.t_end_s);
        v_lim(idx) = min(v_lim(idx), z.v_limit_mps);
    end
    tws = pl.traffic_windows;
    for k = 1:numel(tws)
        tw = tws(k);
        idx = (tg >= tw.t_ramp_start_s) & (tg <= tw.t_ramp_end_s);
        for q = find(idx)
            t = tg(q);
            if t < tw.t_plateau_start_s
                f = (t - tw.t_ramp_start_s) ...
                    / (tw.t_plateau_start_s - tw.t_ramp_start_s);
                lim = tw.v_host_mps + (tw.v_follow_mps - tw.v_host_mps) * f;
            elseif t <= tw.t_plateau_end_s
                lim = tw.v_follow_mps;
            else
                f = (t - tw.t_plateau_end_s) ...
                    / (tw.t_ramp_end_s - tw.t_plateau_end_s);
                lim = tw.v_follow_mps + (tw.v_host_mps - tw.v_follow_mps) * f;
            end
            v_lim(q) = min(v_lim(q), lim);
        end
    end
    v_rec = min(v_base, v_lim);
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

function [m, frac] = local_mode_value(v)
% Modal value of a numeric vector (first minimum on ties) and its share.
    u = unique(v);
    cnt = zeros(1, numel(u));
    for k = 1:numel(u)
        cnt(k) = sum(v == u(k));
    end
    [mx, j] = max(cnt);
    m = u(j);
    frac = mx / numel(v);
end

function v = local_val_at(tg, vp, t)
    [~, j] = min(abs(tg - t));
    v = vp(j);
end

function w = vp_of(tg, vp, t0, t1)
    w = vp((tg >= t0) & (tg < t1));
end

function a = local_w_max_anchor(tbl, pat)
% Frozen W_max anchor by decoded key pattern (HT_1_5 / HT_2_0 / HT_3_0);
% the numeric values come from the frozen file, never from this test.
    fn = fieldnames(tbl);
    a = NaN;
    for j = 1:numel(fn)
        if ~isempty(regexp(fn{j}, pat, 'once'))
            a = tbl.(fn{j});
            return;
        end
    end
    error('GATE:test_plan_compose', 'w_max anchor key missing: %s', pat);
end

function v = local_rush_tier_value(rt, which)
    fn = fieldnames(rt);
    v = NaN;
    for j = 1:numel(fn)
        if ~isempty(strfind(fn{j}, which))
            v = rt.(fn{j});
            return;
        end
    end
    error('GATE:test_plan_compose', 'rush tier key missing: %s', which);
end

function v = local_tier_for_ht(ht, tier_in, tier_ge)
% Tier boundary HT = 2.0 per the frozen key names (dag_sample precedent).
    if ht >= 2.0
        v = tier_ge;
    else
        v = tier_in;
    end
end

function w = local_w_max_interp(ht, anchors)
% Piecewise-linear W_max on the frozen anchor tiers {1.5, 2.0, 3.0},
% capped at the HT_3.0 anchor for HT >= 3.0 (FD5 v2).
    x = [1.5, 2.0, 3.0];
    if ht < x(1)
        error('GATE:test_plan_compose', 'ht below the lowest W_max anchor');
    end
    if ht <= x(2)
        w = anchors(1) + (ht - x(1)) * (anchors(2) - anchors(1)) ...
            / (x(2) - x(1));
    elseif ht <= x(3)
        w = anchors(2) + (ht - x(2)) * (anchors(3) - anchors(2)) ...
            / (x(3) - x(2));
    else
        w = anchors(3);
    end
end

function floor_mps = local_parse_floor(txt)
% Sprint semantic floor parsed from the frozen amendment sentence (same
% contract as plan_compose.local_sprint_floor; never hard-coded here).
    tok = regexp(txt, 'v_rush_platform\s*<\s*([0-9]+(?:\.[0-9]+)?)\s*m/s', ...
        'tokens', 'once');
    if isempty(tok)
        error('GATE:test_plan_compose', ...
            'sprint amendment gate sentence lacks a parseable bound');
    end
    floor_mps = str2double(tok{1});
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

function c = local_role_color(role)
% Fixed ASCII-safe color per segment role (deterministic plotting only).
    switch role
        case 'standby'
            c = [0.55, 0.55, 0.55];
        case 'departure'
            c = [0.85, 0.55, 0.10];
        case 'rush'
            c = [0.85, 0.10, 0.10];
        case 'bridge'
            c = [0.30, 0.30, 0.85];
        case 'mission'
            c = [0.75, 0.20, 0.75];
        case 'harbor'
            c = [0.10, 0.60, 0.30];
        case 'cruise'
            c = [0.10, 0.60, 0.85];
        case 'patrol'
            c = [0.20, 0.75, 0.75];
        case 'return'
            c = [0.45, 0.45, 0.85];
        case 'work'
            c = [0.60, 0.45, 0.20];
        case 'berth'
            c = [0.40, 0.65, 0.15];
        otherwise
            c = [0.30, 0.30, 0.30];
    end
end

function fpath = local_plot_plan(pl, stem, out_dir)
% One v_plan(t) PNG per archetype: per-role background shading, UKC and
% traffic zone overlays, bridge sub-phase boundaries.
    fig = figure('Visible', 'off', 'Color', 'w', 'Position', ...
        [80, 80, 1180, 460]);
    ax = axes(fig);
    % R2026a headless -batch defaults to the dark figure theme (dark axes,
    % light text): alpha 0.18 shading composited over it renders nearly
    % black. Pin light theme explicitly.
    ax.Color = 'w';
    ax.XColor = 'k';
    ax.YColor = 'k';
    ax.Title.Color = 'k';
    ax.XLabel.Color = 'k';
    ax.YLabel.Color = 'k';
    hold(ax, 'on');
    tg = pl.t_grid_s;
    vp = pl.v_plan_mps;
    t_end = tg(end);
    y_hi = 1.3 * max(vp) + 0.5;
    segs = pl.segments;
    plotted_roles = {};
    for i = 1:numel(segs)
        sg = segs(i);
        c = local_role_color(sg.role);
        patch(ax, [sg.t_start_s, sg.t_end_s, sg.t_end_s, sg.t_start_s], ...
            [0, 0, y_hi, y_hi], c, 'FaceAlpha', 0.18, ...
            'EdgeColor', 'none');
        if ~ismember(sg.role, plotted_roles)
            plotted_roles{end + 1} = sg.role;  %#ok<AGROW>
        end
    end
    zl = pl.speed_limit_zones;
    for k = 1:numel(zl)
        z = zl(k);
        patch(ax, [z.t_start_s, z.t_end_s, z.t_end_s, z.t_start_s], ...
            [0, 0, y_hi, y_hi], [1.0, 0.9, 0.2], 'FaceAlpha', 0.35, ...
            'EdgeColor', 'none');
    end
    tws = pl.traffic_windows;
    for k = 1:numel(tws)
        tw = tws(k);
        patch(ax, [tw.t_ramp_start_s, tw.t_ramp_end_s, tw.t_ramp_end_s, ...
            tw.t_ramp_start_s], [0, 0, y_hi, y_hi], [0.5, 0.5, 0.5], ...
            'FaceAlpha', 0.25, 'EdgeColor', 'none');
    end
    bzs = pl.bridge_zones;
    for k = 1:numel(bzs)
        bz = bzs(k);
        ph = bz.phases;
        for q = 2:numel(ph)
            pq = ph(q);
            plot(ax, [pq.t_start_s, pq.t_start_s], [0, y_hi], 'k--', ...
                'LineWidth', 0.5);
        end
    end
    plot(ax, tg, vp, 'k-', 'LineWidth', 1.4);
    ylim(ax, [0, y_hi]);
    xlim(ax, [0, t_end]);
    grid(ax, 'on');
    xlabel(ax, 't (s)', 'Interpreter', 'none');
    ylabel(ax, 'v_{plan} (m/s)');
    title(ax, sprintf(['%s / %s / op=%s / HT=%.2f / rush platform ', ...
        'floor=%.1f m/s'], pl.template_id, pl.regime_id, ...
        pl.operator_profile, pl.ht_effective, pl.sprint_floor_mps), ...
        'Interpreter', 'none');
    leg_names = {};
    leg_handles = [];
    for k = 1:numel(plotted_roles)
        rl_k = plotted_roles{k};
        leg_names{end + 1} = rl_k;  %#ok<AGROW>
        leg_handles(end + 1) = constant_patch(ax, ...
            local_role_color(rl_k), 0.18);  %#ok<AGROW>
    end
    if ~isempty(zl)
        leg_names{end + 1} = 'UKC limit zone';
        leg_handles(end + 1) = constant_patch(ax, [1.0, 0.9, 0.2], 0.35);  %#ok<AGROW>
    end
    if ~isempty(tws)
        leg_names{end + 1} = 'traffic window';
        leg_handles(end + 1) = constant_patch(ax, [0.5, 0.5, 0.5], 0.25);  %#ok<AGROW>
    end
    leg_names{end + 1} = 'v_{plan}';
    leg_handles(end + 1) = constant_line(ax);  %#ok<AGROW>
    lg = legend(leg_handles, leg_names, 'Location', 'best', 'FontSize', 7);
    lg.Color = 'w';           % R2026a headless dark theme: pin legend to light
    lg.TextColor = 'k';
    lg.EdgeColor = [0.15, 0.15, 0.15];
    fpath = fullfile(out_dir, [stem, '.png']);
    % exportgraphics honors FaceAlpha at high resolution (print -dpng can
    % switch to the painters renderer, which drops patch transparency).
    exportgraphics(fig, fpath, 'Resolution', 300);
    close(fig);
end

function h = constant_patch(ax, c, face_alpha)
% Legend swatch with the SAME FaceAlpha as the plotted background shading,
% so the legend matches the figure (user-reported color mismatch fix).
    h = patch(ax, NaN, NaN, c, 'EdgeColor', 'none', ...
        'FaceAlpha', face_alpha);
end

function h = constant_line(ax)
    h = plot(ax, NaN, NaN, 'k-', 'LineWidth', 1.4);
end

function ok = local_png_ok(fpath)
    d = dir(fpath);
    ok = (numel(d) == 1) && (d(1).bytes > 0);
end

function nb = local_png_bytes(fpath)
    d = dir(fpath);
    if numel(d) == 1
        nb = d(1).bytes;
    else
        nb = -1;
    end
end

function verdict = local_verdict(ok)
    if ok
        verdict = 'PASS';
    else
        verdict = 'FAIL';
    end
end

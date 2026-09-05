function plan = plan_compose(mode, varargin)
% mission_v3.plan_compose - T-010 time-mode speed plan composer (DGN-01/08)
%
%   plan = mission_v3.plan_compose('compose', realization)
%
% 'compose': realization is a mission_v3.env_realize('realize', ...) output
%   struct (T-009).  The seven mission condition families are compiled into
%   a time-mode planned speed table v_plan(t) on a 1 s grid over t >= 0
%   (prehistory excluded), plus per-segment role metadata for the token
%   predicates and the T-429 label source.  Pure deterministic transform of
%   the realization + frozen files: no randomness, no environment re-draw,
%   no SCN compilation (that is T-011), no file writes.
%
%   Segment role semantics (fixed decision 8; host speed = T-009
%   segments(i).planned_speed_mps, the frozen plan speed table estimate):
%     standby  -> v_plan = 0 for the whole window (in-place holding; window
%                 length = nominal_duration_s, fixed decision 5);
%     rush     -> platform = min(rush_tier(ht_effective), W_max_interp(ht)
%                 + u_signed_min_of_rush_segment)  (fixed decision 5 v2;
%                 phase1_freeze_amendment_sprint.json rush_policy_refinement).
%                 The frozen 6.5/7.0 m/s tiers are the task EXPECTATION
%                 upper bound; the W_max frontier clamp enforces joint
%                 feasibility over the whole rush window (u_signed_min is
%                 the most-adverse instantaneous along-track component).
%                 If the clamped platform falls below the sprint semantic
%                 floor read from the amendment (5.5 m/s = return host
%                 speed, v1 proposal) the composer fails closed with
%                 GATE:MISSION_V3:PLAN:INFEASIBLE_SPRINT.  The plan table
%                 steps at the window start; rapid acceleration is executed
%                 by the Model E ramp at the SCN layer (T-011).  Operator
%                 coefficients never touch the rush platform (physical
%                 clamp precedes style);
%     bridge   -> five BRIDGE_OPERATION sub-phases inside the host window
%                 (fixed decision 2; table built by nav_constraints and
%                 reused here - single source of truth);
%     departure/berth -> fixed decision 6 maneuver profiles (non-mirror,
%                 phase tables built by nav_constraints('phases', ...));
%     harbor   -> constant v_host (long steady low-speed leg);
%     work     -> constant v_host (frozen 1.2 m/s low-speed work);
%     cruise/patrol/return/mission -> constant v_host * operator speed
%                 scale (fixed decision 7: URGENT x1.05 / CALM x0.95 /
%                 STANDARD x1.0, v1 proposal).  Bridge/berth/departure/
%                 standby/rush/harbor/work are operator-independent.
%   Zone composition: the UKC speed-limit zones (fixed decision 3) and the
%   traffic slowdown windows (fixed decision 4) are min-composed onto the
%   base profile (mission constraints precede operator style, FD7).
%
%   Window conservation (fixed decision 1): every plan window equals the
%   T-009 segment window exactly.  In-window deceleration makes the
%   integrated distance smaller than the nominal distance; that drift is a
%   registered v1 known limitation (time-mode plan, position by
%   integration) - windows are NEVER adjusted to recover distance.
%
%   Input contract: the realization structure is validated fail-closed
%   (kind, fields, host speeds cross-checked against the frozen plan speed
%   table, contiguity, grid).  The disposition policy is owned by the
%   caller: the IID chain feeds ACCEPT realizations, while the OOD
%   generator may realize C3-shaped (heldout) draws whose segments must
%   still pass the physical sprint floor (phase1_freeze_amendment_sprint
%   evidence.secondary); plan_compose therefore validates physics, not the
%   disposition field.
%
%   Output plan struct (fixed decision 9, T-011/producer consumption):
%     kind='mission_v3_plan', formal_seed, base_draw_index, stratum_id,
%     template_id, regime_id, operator_profile, operator_speed_scale,
%     operator_ramp_scale (URGENT 1.2 / CALM 0.8 / STANDARD 1.0, FD7),
%     t_grid_s (1 s, 0:t_end), v_plan_mps, segments (per segment
%     role/reach/direction/along_sign/window_s/t_start/t_end/
%     planned_speed_mps/v_platform_mps/sub_phases/u_signed_min_mps/
%     u_signed_mean_mps/u_adverse_abs_sup_mps/rush_policy),
%     speed_limit_zones, bridge_zones, traffic_windows, ht_effective,
%     sprint_floor_mps, provenance (freeze hash references + adjudication
%     ids + distance-drift note).
%
%   Determinism: identical inputs give bitwise-identical output.  The
%   global RNG is never touched.
%
%   Error ids: GATE:MISSION_V3:PLAN:<REASON>.
%
%   Discipline: ASCII-only source; no absolute paths; no chained indexing;
%   ROOT resolved from mfilename with pwd fallback; no global RNG.

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:PLAN:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'compose'
            if nargin < 2
                error('GATE:MISSION_V3:PLAN:ARG', ...
                    '''compose'' expects: realization');
            end
            plan = local_compose(varargin{1});
        otherwise
            error('GATE:MISSION_V3:PLAN:ARG', ...
                'unknown mode: %s (plan_compose implements ''compose'' only)', ...
                char(mode));
    end
end

%% =========================== composer ===============================

function plan = local_compose(rl)
    rl = local_check_realization(rl);

    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    frz = local_load_frozen(ROOT);

    % ---- operator profile (fixed decision 7) ----
    op = local_char(rl.operator_profile, 'operator_profile');
    if ~ismember(op, local_enum_list(frz.tpl.operator_profile_enum))
        error('GATE:MISSION_V3:PLAN:OPERATOR', ...
            'operator profile %s outside the frozen enum', op);
    end
    op_speed = OPERATOR_SPEED_SCALE(op);
    op_ramp = OPERATOR_RAMP_SCALE(op);

    % ---- voyage HT guard (FD3: ht < 1.5 cannot reach the plan layer) ----
    ht = rl.water.ht_effective;
    if ~isnumeric(ht) || ~isscalar(ht) || ~isfinite(ht) ...
            || (ht < PLAN_CONST_HT_SUPPORT_MIN())
        error('GATE:MISSION_V3:PLAN:HT_BELOW_SUPPORT', ...
            'ht_effective %.6f below the 1.5 IID support floor', ht);
    end

    % ---- plan grid (FD9: 1 s, t >= 0 only) ----
    dt = rl.ou.dt_s;
    if ~isnumeric(dt) || ~isscalar(dt) || ~isfinite(dt) ...
            || (abs(dt - PLAN_CONST_GRID_S()) > 1e-12)
        error('GATE:MISSION_V3:PLAN:GRID', ...
            'plan table requires the frozen 1 s injection grid');
    end
    t_end = rl.ou.t_end_s;
    if ~isnumeric(t_end) || ~isscalar(t_end) || ~isfinite(t_end) ...
            || (t_end < 0) || (abs(t_end - round(t_end)) > 1e-9)
        error('GATE:MISSION_V3:PLAN:GRID', ...
            'realized t_end_s must be a non-negative integer on the 1 s grid');
    end
    t_grid = 0.0:round(t_end);
    n_grid = numel(t_grid);

    % ---- navigation constraint zones (single source of truth) ----
    zones = mission_v3.nav_constraints('zones', rl);

    % ---- host speed cross-check against the frozen plan speed table ----
    local_check_host_speeds(rl, frz, ht);

    % ---- rush platform policy (fixed decision 5 v2) ----
    sprint_floor = local_sprint_floor(frz.sprint);
    rush_platforms = zeros(1, numel(rl.segments));
    rush_meta = repmat(blank_rush_meta(), 1, numel(rl.segments));
    for i = 1:numel(rl.segments)
        sg = rl.segments(i);
        rush_meta(i).active = false;
        if ~strcmp(sg.role, 'rush')
            continue;
        end
        if ~sg.on_grid || ~isfinite_field(sg, 'u_signed_min')
            error('GATE:MISSION_V3:PLAN:SEG_NOT_ON_GRID', ...
                'rush segment %d is not on the realized grid', i);
        end
        tier = local_rush_tier(frz.freeze.plan_speed_table_frozen.rush_tiers, ht);
        w_max = local_w_max_interp(ht, frz);
        u_min = sg.u_signed_min;
        frontier = w_max + u_min;
        platform = min(tier, frontier);
        if platform < sprint_floor
            error('GATE:MISSION_V3:PLAN:INFEASIBLE_SPRINT', ...
                ['rush segment %d platform %.6f m/s below the sprint ', ...
                 'semantic floor %.6f m/s (tier %.6f, W_max %.6f, ', ...
                 'u_signed_min %.6f) - joint feasibility fail-closed'], ...
                i, platform, sprint_floor, tier, w_max, u_min);
        end
        rush_platforms(i) = platform;
        rush_meta(i).active = true;
        rush_meta(i).tier_mps = tier;
        rush_meta(i).w_max_mps = w_max;
        rush_meta(i).frontier_mps = frontier;
        rush_meta(i).u_signed_min_mps = u_min;
        rush_meta(i).sprint_floor_mps = sprint_floor;
        rush_meta(i).clamped_to_frontier = frontier < tier;
    end

    % ---- per-segment base profile on the plan grid (FD1 window set) ----
    v_base = NaN(1, n_grid);
    blank_seg = blank_plan_segment();
    seg_out = repmat(blank_seg, 1, numel(rl.segments));
    t_last_end = 0.0;
    for i = 1:numel(rl.segments)
        sg = rl.segments(i);
        t0 = sg.t_start_nominal_s;
        w = sg.window_s;
        if ~isfinite_field(sg, 't_start_nominal_s') ...
                || (abs(t0 - t_last_end) > 1e-9)
            error('GATE:MISSION_V3:PLAN:SEGMENT_GAP', ...
                'segment %d does not start where the previous one ends', i);
        end
        t1 = t0 + w;
        t_last_end = t1;
        idx = find((t_grid >= t0) & (t_grid < t1));
        v_host = local_host_speed(sg);
        sub_ph = repmat(blank_phase(), 1, 0);

        switch sg.role
            case 'standby'
                v_seg = zeros(1, numel(idx));
                v_platform = 0.0;
            case 'rush'
                v_seg = rush_platforms(i) * ones(1, numel(idx));
                v_platform = rush_platforms(i);
            case 'bridge'
                bz = local_find_bridge_zone(zones, i);
                v_seg = mission_v3.nav_constraints('eval_phases', ...
                    bz.phases, t_grid(idx));
                v_platform = bz.v_host_mps;
                sub_ph = bz.phases;
            case 'departure'
                sub_ph = mission_v3.nav_constraints('phases', 'departure', ...
                    t0, w, v_host);
                v_seg = mission_v3.nav_constraints('eval_phases', sub_ph, ...
                    t_grid(idx));
                v_platform = v_host;
            case 'berth'
                sub_ph = mission_v3.nav_constraints('phases', 'berth', ...
                    t0, w, v_host);
                v_seg = mission_v3.nav_constraints('eval_phases', sub_ph, ...
                    t_grid(idx));
                v_platform = v_host;
            case {'harbor', 'work'}
                v_seg = v_host * ones(1, numel(idx));
                v_platform = v_host;
            case {'cruise', 'patrol', 'return', 'mission'}
                v_seg = (v_host * op_speed) * ones(1, numel(idx));
                v_platform = v_host * op_speed;
            otherwise
                error('GATE:MISSION_V3:PLAN:ROLE', ...
                    'segment role %s has no plan semantics', sg.role);
        end
        v_base(idx) = v_seg;

        row = blank_seg;
        row.index = i;
        row.role = sg.role;
        row.reach = sg.reach;
        row.direction = sg.direction;
        row.along_sign = sg.along_sign;
        row.window_s = w;
        row.t_start_s = t0;
        row.t_end_s = t1;
        row.planned_speed_mps = sg.planned_speed_mps;
        row.v_platform_mps = v_platform;
        row.sub_phases = sub_ph;
        row.u_signed_min_mps = sg.u_signed_min;
        row.u_signed_mean_mps = sg.u_signed_mean;
        row.u_adverse_abs_sup_mps = sg.u_adverse_abs_sup;
        if rush_meta(i).active
            row.rush_policy = struct('tier_mps', rush_meta(i).tier_mps, ...
                'w_max_mps', rush_meta(i).w_max_mps, ...
                'frontier_mps', rush_meta(i).frontier_mps, ...
                'u_signed_min_mps', rush_meta(i).u_signed_min_mps, ...
                'sprint_floor_mps', rush_meta(i).sprint_floor_mps, ...
                'clamped_to_frontier', rush_meta(i).clamped_to_frontier);
        end
        seg_out(i) = row;
    end

    % ---- tail margin beyond the last segment window (v1 known limit) ----
    tail_idx = find(t_grid >= t_last_end);
    if ~isempty(tail_idx)
        first_tail = tail_idx(1);
        if first_tail > 1
            v_base(tail_idx) = v_base(first_tail - 1);
        end
    end
    if any(isnan(v_base))
        error('GATE:MISSION_V3:PLAN:GRID_COVERAGE', ...
            'plan grid has points not covered by any segment window');
    end

    % ---- zone min-composition (FD3 UKC + FD4 traffic) ----
    v_lim = inf(1, n_grid);
    for k = 1:numel(zones.speed_limit_zones)
        z = zones.speed_limit_zones(k);
        idx = find((t_grid >= z.t_start_s) & (t_grid < z.t_end_s));
        v_lim(idx) = min(v_lim(idx), z.v_limit_mps);
    end
    for k = 1:numel(zones.traffic_windows)
        tw = zones.traffic_windows(k);
        idx = find((t_grid >= tw.t_ramp_start_s) ...
            & (t_grid <= tw.t_ramp_end_s));
        for q = 1:numel(idx)
            t = t_grid(idx(q));
            if t < tw.t_plateau_start_s
                f = (t - tw.t_ramp_start_s) ...
                    / (tw.t_plateau_start_s - tw.t_ramp_start_s);
                lim = tw.v_host_mps ...
                    + (tw.v_follow_mps - tw.v_host_mps) * f;
            elseif t <= tw.t_plateau_end_s
                lim = tw.v_follow_mps;
            else
                f = (t - tw.t_plateau_end_s) ...
                    / (tw.t_ramp_end_s - tw.t_plateau_end_s);
                lim = tw.v_follow_mps ...
                    + (tw.v_host_mps - tw.v_follow_mps) * f;
            end
            v_lim(idx(q)) = min(v_lim(idx(q)), lim);
        end
    end
    v_plan = min(v_base, v_lim);

    % ---- plan-level assertions ----
    if ~all(isfinite(v_plan))
        error('GATE:MISSION_V3:PLAN:NONFINITE', ...
            'composed plan contains non-finite speeds');
    end
    if any(v_plan < 0)
        error('GATE:MISSION_V3:PLAN:NEGATIVE_SPEED', ...
            'composed plan contains negative speeds');
    end

    % ---- assemble the FD9 plan struct ----
    plan = struct();
    plan.kind = 'mission_v3_plan';
    plan.formal_seed = local_char(rl.formal_seed, 'formal_seed');
    plan.base_draw_index = rl.base_draw_index;
    plan.stratum_id = local_char(rl.stratum_id, 'stratum_id');
    plan.template_id = local_char(rl.template_id, 'template_id');
    plan.regime_id = local_char(rl.regime_id, 'regime_id');
    plan.operator_profile = op;
    plan.operator_speed_scale = op_speed;
    plan.operator_ramp_scale = op_ramp;
    plan.t_grid_s = t_grid;
    plan.v_plan_mps = v_plan;
    plan.segments = seg_out;
    plan.speed_limit_zones = zones.speed_limit_zones;
    plan.bridge_zones = zones.bridge_zones;
    plan.traffic_windows = zones.traffic_windows;
    plan.ht_effective = ht;
    plan.sprint_floor_mps = sprint_floor;
    plan.provenance = local_provenance(frz, sprint_floor);
end

%% ======================== frozen file loading =======================

function frz = local_load_frozen(ROOT)
% Load the effective phase-1 freeze (base + D6 amendment + sprint amendment,
% immutable chain) and the frozen template registry with fail-closed
% binding checks.
    cfg = fullfile(ROOT, 'config', 'mission_v3');
    res = fullfile(ROOT, 'results', 'mission_v3');
    frz = struct();
    frz.cfg_dir = cfg;
    frz.freeze = local_read_json(res, 'phase1_freeze.json', ...
        'GATE:MISSION_V3:PLAN:REGISTRY');
    frz.d6 = local_read_json(res, 'phase1_freeze_amendment_d6.json', ...
        'GATE:MISSION_V3:PLAN:REGISTRY');
    frz.sprint = local_read_json(res, 'phase1_freeze_amendment_sprint.json', ...
        'GATE:MISSION_V3:PLAN:REGISTRY');
    frz.tpl = local_read_json(cfg, 'mission_template_registry.json', ...
        'GATE:MISSION_V3:PLAN:REGISTRY');

    base_hash = local_self_hash(frz.freeze);
    if ~strcmp(local_amends_hash(frz.d6), base_hash) ...
            || ~strcmp(local_amends_hash(frz.sprint), base_hash)
        error('GATE:MISSION_V3:PLAN:FREEZE_MISMATCH', ...
            'amendments do not bind to the base phase1_freeze self_hash');
    end
    chain = local_as_list(frz.sprint.amendment_chain, 'amendment_chain');
    if (numel(chain) ~= 3) ...
            || ~isfield(chain{2}, 'self_hash') ...
            || ~strcmp(chain{2}.self_hash, local_self_hash(frz.d6))
        error('GATE:MISSION_V3:PLAN:FREEZE_MISMATCH', ...
            'sprint amendment chain does not bind the D6 amendment');
    end
    if ~isfield(frz.sprint, 'rush_policy_refinement')
        error('GATE:MISSION_V3:PLAN:REGISTRY', ...
            'sprint amendment lacks rush_policy_refinement');
    end

    % W_max anchors: structured values from the frozen signed-frontier
    % table; cross-checked against the base freeze copy (fail-closed).
    w6 = frz.d6.signed_frontier_convention_accepted.w_max_table;
    wb = frz.freeze.joint_feasibility_evidence.w_max_table;
    frz.w_max_anchors = zeros(1, 3);
    for k = 1:3
        key = local_w_max_key(w6, k);
        v6 = w6.(key);
        vb = local_w_max_key_value(wb, key);
        if ~isfinite(v6) || ~isfinite(vb) || (abs(v6 - vb) > 1e-12)
            error('GATE:MISSION_V3:PLAN:FREEZE_MISMATCH', ...
                'w_max anchor %s disagrees between freeze and D6 amendment', ...
                key);
        end
        frz.w_max_anchors(k) = v6;
    end
end

function h = local_self_hash(obj)
    if ~isfield(obj, 'self_hash') || ~isfield(obj.self_hash, 'value')
        error('GATE:MISSION_V3:PLAN:FREEZE_MISMATCH', ...
            'frozen object lacks self_hash.value');
    end
    h = obj.self_hash.value;
end

function h = local_amends_hash(am)
    if ~isfield(am, 'amends') || ~isfield(am.amends, 'self_hash')
        error('GATE:MISSION_V3:PLAN:FREEZE_MISMATCH', ...
            'amendment lacks amends.self_hash');
    end
    h = am.amends.self_hash;
end

function key = local_w_max_key(tbl, k)
% Anchor keys after jsondecode map the frozen names HT_1.5 / HT_2.0 /
% HT_3.0 to HT_1_5 / HT_2_0 / HT_3_0.  Match by pattern, fail-closed.
    pats = {'^HT_1_5$', '^HT_2_0$', '^HT_3_0$'};
    fn = fieldnames(tbl);
    key = '';
    for j = 1:numel(fn)
        if ~isempty(regexp(fn{j}, pats{k}, 'once'))
            key = fn{j};
            return;
        end
    end
    error('GATE:MISSION_V3:PLAN:FREEZE_MISMATCH', ...
        'frozen w_max_table lacks anchor key for pattern %s', pats{k});
end

function v = local_w_max_key_value(tbl, key)
    fn = fieldnames(tbl);
    for j = 1:numel(fn)
        if strcmp(fn{j}, key)
            v = tbl.(key);
            return;
        end
    end
    error('GATE:MISSION_V3:PLAN:FREEZE_MISMATCH', ...
        'base freeze w_max_table lacks anchor key %s', key);
end

function w = local_w_max_interp(ht, frz)
% Piecewise-linear W_max(HT) on the frozen anchor tiers
% {HT 1.5, 2.0, 3.0}; HT >= 3.0 capped at the HT_3.0 anchor (conservative;
% deep_20m stays a G2 anchor reference only).  X breakpoints are the frozen
% HT tier boundaries encoded in the anchor key names; Y values are read
% from the frozen files (never hard-coded).
    x = PLAN_CONST_W_MAX_X();
    a = frz.w_max_anchors;
    if ht < x(1)
        error('GATE:MISSION_V3:PLAN:HT_BELOW_SUPPORT', ...
            'ht %.6f below the lowest W_max anchor tier', ht);
    end
    if ht <= x(2)
        w = a(1) + (ht - x(1)) * (a(2) - a(1)) / (x(2) - x(1));
    elseif ht <= x(3)
        w = a(2) + (ht - x(2)) * (a(3) - a(2)) / (x(3) - x(2));
    else
        w = a(3);
    end
end

function v = local_rush_tier(rt, ht)
% Frozen rush tiers by voyage HT; the tier boundary (HT = 2.0) is encoded
% in the frozen key names (dag_sample precedent; boundary pinned here with
% provenance: phase1_freeze plan_speed_table_frozen.rush_tiers).
    fn = fieldnames(rt);
    v_in = NaN;
    v_ge = NaN;
    for k = 1:numel(fn)
        nm = fn{k};
        if ~isempty(strfind(nm, 'HT_ge'))
            v_ge = rt.(nm);
        elseif ~isempty(strfind(nm, 'HT_in'))
            v_in = rt.(nm);
        end
    end
    if ~isfinite(v_in) || ~isfinite(v_ge)
        error('GATE:MISSION_V3:PLAN:REGISTRY', ...
            'rush_tiers keys not recognized in the frozen plan speed table');
    end
    if ht >= 2.0
        v = v_ge;
    else
        v = v_in;
    end
end

function floor_mps = local_sprint_floor(sprint)
% Sprint semantic floor parsed from the frozen amendment gate sentence
% ("v_rush_platform < 5.5 m/s ..."), never hard-coded; fail-closed when the
% sentence does not carry a parseable bound.
    txt = sprint.rush_policy_refinement.infeasible_sprint_gate;
    tok = regexp(txt, 'v_rush_platform\s*<\s*([0-9]+(?:\.[0-9]+)?)\s*m/s', ...
        'tokens', 'once');
    if isempty(tok)
        error('GATE:MISSION_V3:PLAN:AMENDMENT_PARSE', ...
            'sprint amendment infeasible_sprint_gate lacks a parseable bound');
    end
    floor_mps = str2double(tok{1});
    if ~isfinite(floor_mps) || (floor_mps <= 0)
        error('GATE:MISSION_V3:PLAN:AMENDMENT_PARSE', ...
            'sprint floor parse is not a positive finite number');
    end
end

function local_check_host_speeds(rl, frz, ht)
% Cross-check every T-009 host speed against the frozen plan speed table
% (fail-closed traceability of all speed values to the freeze).
    pst = frz.freeze.plan_speed_table_frozen;
    for i = 1:numel(rl.segments)
        sg = rl.segments(i);
        if strcmp(sg.role, 'standby')
            continue;
        end
        if isempty(sg.planned_speed_mps)
            error('GATE:MISSION_V3:PLAN:HOST_SPEED_MISMATCH', ...
                'segment %d (%s) carries no host speed', i, sg.role);
        end
        if strcmp(sg.role, 'rush')
            v_ref = local_rush_tier(pst.rush_tiers, ht);
        else
            if ~isfield(pst.role_speeds_mps, sg.role)
                error('GATE:MISSION_V3:PLAN:HOST_SPEED_MISMATCH', ...
                    'frozen plan speed table lacks role %s', sg.role);
            end
            v_ref = pst.role_speeds_mps.(sg.role);
        end
        if abs(sg.planned_speed_mps - v_ref) > 1e-9
            error('GATE:MISSION_V3:PLAN:HOST_SPEED_MISMATCH', ...
                ['segment %d (%s) host speed %.6f disagrees with the ', ...
                 'frozen table value %.6f'], i, sg.role, ...
                sg.planned_speed_mps, v_ref);
        end
    end
end

%% ======================== validation helpers ========================

function rl = local_check_realization(rl)
    if ~isstruct(rl) || ~isfield(rl, 'kind') || ~strcmp(rl.kind, 'realize')
        error('GATE:MISSION_V3:PLAN:ARG', ...
            'realization must be a mission_v3.env_realize ''realize'' struct');
    end
    need = {'formal_seed', 'base_draw_index', 'stratum_id', 'template_id', ...
        'regime_id', 'operator_profile', 'segments', 'traffic', 'water', 'ou'};
    for j = 1:numel(need)
        if ~isfield(rl, need{j})
            error('GATE:MISSION_V3:PLAN:ARG', ...
                'realization lacks field: %s', need{j});
        end
    end
    if ~isfield(rl.water, 'ht_effective') || ~isfield(rl.ou, 't_end_s') ...
            || ~isfield(rl.ou, 'dt_s')
        error('GATE:MISSION_V3:PLAN:ARG', ...
            'realization lacks water.ht_effective or ou grid fields');
    end
    seg_need = {'index', 'role', 'reach', 'direction', 'along_sign', ...
        'window_s', 't_start_nominal_s', 'planned_speed_mps', ...
        'u_signed_min', 'u_signed_mean', 'u_adverse_abs_sup', 'on_grid'};
    for i = 1:numel(rl.segments)
        sg = rl.segments(i);
        for j = 1:numel(seg_need)
            if ~isfield(sg, seg_need{j})
                error('GATE:MISSION_V3:PLAN:ARG', ...
                    'realization segment %d lacks field: %s', i, seg_need{j});
            end
        end
        if ~isfinite_field(sg, 'window_s') || (sg.window_s <= 0)
            error('GATE:MISSION_V3:PLAN:ARG', ...
                'realization segment %d has invalid window_s', i);
        end
    end
end

function bz = local_find_bridge_zone(zones, seg_index)
    for k = 1:numel(zones.bridge_zones)
        z = zones.bridge_zones(k);
        if z.segment_index == seg_index
            bz = z;
            return;
        end
    end
    error('GATE:MISSION_V3:PLAN:ZONE_MISSING', ...
        'bridge segment %d has no bridge_zones entry', seg_index);
end

function v = local_host_speed(sg)
    v = 0.0;
    if ~isempty(sg.planned_speed_mps)
        v = sg.planned_speed_mps;
        if ~isnumeric(v) || ~isscalar(v) || ~isfinite(v) || (v < 0)
            error('GATE:MISSION_V3:PLAN:ARG', ...
                'segment %d has invalid planned_speed_mps', sg.index);
        end
    end
end

function ok = isfinite_field(s, name)
    ok = isfield(s, name) && isnumeric(s.(name)) && isscalar(s.(name)) ...
        && isfinite(s.(name));
end

function c = local_char(v, name)
    if ~ischar(v) && ~isstring(v)
        error('GATE:MISSION_V3:PLAN:ARG', '%s must be a string', name);
    end
    c = char(v);
end

function lst = local_enum_list(v)
% jsondecode enum arrays arrive as cellstr or string arrays.
    if iscell(v)
        lst = v(:)';
    elseif isstring(v)
        lst = cellstr(v);
    elseif ischar(v)
        lst = {v};
    else
        lst = {};
    end
end

function lst = local_as_list(v, what)
    if iscell(v)
        lst = v(:)';
    elseif isstruct(v)
        n = numel(v);
        lst = cell(1, n);
        for k = 1:n
            lst{k} = v(k);
        end
    else
        error('GATE:MISSION_V3:PLAN:REGISTRY', ...
            'expected JSON array for %s', what);
    end
end

function obj = local_read_json(dirpath, fname, err_id)
    ppath = fullfile(dirpath, fname);
    if ~exist(ppath, 'file')
        error(err_id, 'frozen file not found: %s', fname);
    end
    try
        obj = jsondecode(fileread(ppath));
    catch dec_err
        error(err_id, 'jsondecode failed for %s: %s', fname, dec_err.message);
    end
    if ~isstruct(obj)
        error(err_id, 'jsondecode did not return a struct for %s', fname);
    end
end

%% ======================= blank struct builders ======================

function s = blank_plan_segment()
    s = struct('index', 0, 'role', '', 'reach', '', 'direction', '', ...
        'along_sign', 0, 'window_s', 0.0, 't_start_s', 0.0, 't_end_s', 0.0, ...
        'planned_speed_mps', [], 'v_platform_mps', 0.0, ...
        'sub_phases', {repmat(blank_phase(), 1, 0)}, ...
        'u_signed_min_mps', NaN, 'u_signed_mean_mps', NaN, ...
        'u_adverse_abs_sup_mps', NaN, 'rush_policy', []);
end

function s = blank_rush_meta()
    s = struct('active', false, 'tier_mps', NaN, 'w_max_mps', NaN, ...
        'frontier_mps', NaN, 'u_signed_min_mps', NaN, ...
        'sprint_floor_mps', NaN, 'clamped_to_frontier', false);
end

function p = blank_phase()
    p = struct('name', '', 'frac', 0.0, 't_start_s', 0.0, 't_end_s', 0.0, ...
        'speed_law', '', 'v_start_mps', 0.0, 'v_end_mps', 0.0);
end

%% ===================== v1 proposal constants ========================
% Fixed numeric values live here as named constants with provenance notes
% (fixed decision 10).  Sources: T-010 delegation spec Fixed Decisions and
% the frozen files; never re-derived quantities.

function c = PLAN_CONST_GRID_S()
% FD9: plan table on the frozen 1 s injection grid.
    c = 1.0;
end

function c = PLAN_CONST_HT_SUPPORT_MIN()
% FD3: HT below 1.5 cannot reach the plan layer (T-009 rejects it).
    c = 1.5;
end

function x = PLAN_CONST_W_MAX_X()
% FD5 v2: W_max interpolation HT breakpoints = the frozen anchor tier
% boundaries encoded in the w_max_table key names (HT_1.5 / HT_2.0 /
% HT_3.0).  Y values are read from the frozen files.
    x = [1.5, 2.0, 3.0];
end

function c = OPERATOR_SPEED_SCALE(op)
% FD7 v1 proposal: voyage-level planned-speed style coefficients.
    switch op
        case 'URGENT'
            c = 1.05;
        case 'CALM'
            c = 0.95;
        case 'STANDARD'
            c = 1.0;
        otherwise
            error('GATE:MISSION_V3:PLAN:OPERATOR', ...
                'no speed scale for operator profile %s', op);
    end
end

function c = OPERATOR_RAMP_SCALE(op)
% FD7 v1 proposal: acceleration ramp coefficients recorded for T-011 SCN
% consumption (plan.operator_ramp_scale).
    switch op
        case 'URGENT'
            c = 1.2;
        case 'CALM'
            c = 0.8;
        case 'STANDARD'
            c = 1.0;
        otherwise
            error('GATE:MISSION_V3:PLAN:OPERATOR', ...
                'no ramp scale for operator profile %s', op);
    end
end

%% ============================ provenance ============================

function prov = local_provenance(frz, sprint_floor)
    prov = struct();
    prov.generator = ['mission_v3.plan_compose (T-010; design DGN-01/08; ', ...
        'delegation spec experiments/mission_v3/T-010_delegation_spec.md ', ...
        'Fixed Decisions FD1-FD10 v2)'];
    prov.hash_phase1_freeze = local_self_hash(frz.freeze);
    prov.hash_freeze_amendment_d6 = local_self_hash(frz.d6);
    prov.hash_freeze_amendment_sprint = local_self_hash(frz.sprint);
    d6e = frz.sprint.effective_phase1_configuration;
    if isfield(d6e, 'composite_hash')
        prov.hash_freeze_composite = d6e.composite_hash;
    else
        prov.hash_freeze_composite = '';
    end
    prov.hash_template_registry = local_self_hash(frz.tpl);
    prov.rush_policy = ['FD5 v2 (adjudication 2026-09-05): rush platform = ', ...
        'min(rush_tier(ht), W_max_interp(ht) + u_signed_min); tiers 6.5/7.0 ', ...
        'm/s unchanged = task expectation upper bound; W_max piecewise-', ...
        'linear on frozen anchors {1.5, 2.0, 3.0} capped at the HT_3.0 ', ...
        'anchor; u_signed_min = most-adverse instantaneous along-track ', ...
        'component (T-009 segment summary) conservatively feasible over ', ...
        'the whole window; fail-closed below the sprint semantic floor ', ...
        sprintf('%.6f', sprint_floor), ' m/s parsed from ', ...
        'phase1_freeze_amendment_sprint.json rush_policy_refinement.', ...
        'infeasible_sprint_gate; operator coefficients never apply to the ', ...
        'rush platform'];
    prov.adjudications_v1 = ['named constants are v1 proposals: bridge ', ...
        'transit factor 0.7 + phase fractions 0.15/0.10/0.55/0.05/0.15 ', ...
        '(FD2, Day-1 section A3); UKC formula (FD3); traffic 10% ramps ', ...
        '(FD4); departure/berth maneuver profiles (FD6, FR-511); operator ', ...
        'speed 1.05/0.95/1.0 and ramp 1.2/0.8/1.0 (FD7, FR-515) - ', ...
        'source: T-010 delegation spec Fixed Decisions'];
    prov.window_conservation = ['FD1: every plan window equals the T-009 ', ...
        'segment window (host speed estimate); in-window deceleration ', ...
        'makes the integrated distance smaller than the nominal distance ', ...
        '(time-mode plan, position by integration); the drift is a ', ...
        'registered v1 known limitation and windows are never adjusted ', ...
        'to recover distance'];
    prov.tail_margin = ['grid extends to the realized t_end (auto = ', ...
        'window sum + 60 s margin); beyond the last segment window the ', ...
        'last planned speed is held (v1 known limitation)'];
    prov.traffic_note = ['traffic windows may overrun the source segment; ', ...
        'limits are min-composed globally on the voyage time axis (v1 ', ...
        'simplification); traffic never modifies the Plant (DGN-04)'];
    prov.prehistory = ['plan defines t >= 0 only; the causal prehistory ', ...
        '(-1800 s) stays in the environment layer and never enters the ', ...
        'plan (FD9)'];
    prov.rapid_accel = ['plan-table steps (rush entry, maneuver phase ', ...
        'boundaries) are executed by the Model E rate limiter at the SCN ', ...
        'layer; accel_ramp consumption belongs to T-011 (FD5)'];
    prov.determinism = ['pure deterministic transform of (realization, ', ...
        'frozen files); no randomness, no environment re-draw, no file ', ...
        'writes'];
end

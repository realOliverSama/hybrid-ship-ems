function out = scenario_compile(mode, varargin)
% mission_v3.scenario_compile - T-011 runtime SCN compiler (DGN-08/09, R1 Path B)
%
%   out = mission_v3.scenario_compile('compile', plan, realization, ...)
%   ok  = mission_v3.scenario_compile('check_injection', injection_list)
%
% 'compile': plan is a mission_v3.plan_compose('compose', ...) struct (T-010)
%   and realization is a mission_v3.env_realize('realize', ...) struct
%   (T-009).  The compiler transforms them into a runtime scenario struct
%   SCN that is field-for-field aligned with the EXISTING Model E SCN
%   contract (config/ModelE_TestScenarios.m survey; consumers verified in
%   scripts/+model/run_scenario_sim.m and scripts/+model/build_model_e.m),
%   plus a VOYAGE_* injection manifest for the T-015 producer.  Zero Model E
%   changes: the compiler never writes to Model E files and never runs a
%   simulation (pure transform of its inputs + frozen files).
%
%   Compiled SCN fields (fixed decision 1; surveyed consumption sites):
%     speed_profile_mode = 'time' with v_time_s = plan.t_grid_s (1 s, t >= 0)
%       and v_value_mps = plan.v_plan_mps (Model E time-mode table, ZOH
%       lookup + downstream rate limiter);
%     current_time_s = the same 1 s grid, current_value_mps = the R1
%       signed along-track river series (fixed decision 2): per plan
%       segment, along_sign x U_river(t) with U_river the realization
%       river-coordinate truth (downstream positive); outbound -1, return
%       +1, berth/standby 0; negative = adverse (S03 semantics); windows
%       stitched exactly on the T-010 plan windows; beyond the last
%       segment window the last segment's sign mapping is held;
%     water_depth_m = h_chart + delta_wl = h_effective (voyage scalar,
%       DGN-06 R4 identity, fixed decision 4);
%     accel_ramp_mps2 / decel_ramp_mps2 = the frozen Model E
%       base ramp ASSUMED_Speed_Ramp_Up (parsed from
%       config/ModelE_Physics_Parameters.m, never hard-coded) multiplied by
%       plan.operator_ramp_scale (T-010 FD7 tiers 1.2/0.8/1.0; the existing
%       decel default is the same RAMP_NORM constant, S01 precedent)
%       (fixed decision 5);
%     hotel_kW = p_hotel_base(template) + delta_p_hotel_unplanned(draw)
%       (DGN-09 Path B identity, fixed decision 3).  p_hotel_base comes
%       from the frozen template registry by template_id;
%       delta_p_hotel_unplanned is re-derived DETERMINISTICALLY from the
%       dedicated hotel_residual seed-tree branch (single truncated-normal
%       draw, rejection resampling, same procedure and frozen node params
%       as dag_sample; bitwise-identical to the base draw's value; no new
%       randomness - the plan/realization contract carries no hotel field
%       and the compile signature is fixed);
%     initial_SOC from the 'InitialSOC' name-value (default 0.6, T-005
%       scan precedent; provenance recorded, fixed decision 6);
%     random_seed = the current_truth branch engine_seed_uint32 (first 8
%       hex of the lineage_id, seed_tree engine mapping) cross-checked
%       against the realization lineage summary (fixed decision 7); the
%       full digest stays in plan/realization lineage, SCN mirrors only
%       the engine seed;
%     all remaining surveyed SCN fields carry the neutral existing-scenario
%       defaults (constant resistance profile, distance-mode waypoint
%       sentinels, benign notes) so the struct satisfies the existing
%       consumers field-for-field (field_map in provenance).
%
%   Injection manifest (fixed decision 8): the compiler ONLY PRODUCES the
%   list {var_name, value}; execution (assignin / setVariable) belongs to
%   the T-015 producer.  Entries (minimal surveyed set):
%     VOYAGE_SPEED_MODE        = 1 (time mode)
%     VOYAGE_V_TIME_S          = plan.t_grid_s
%     VOYAGE_V_VALUE_MPS       = plan.v_plan_mps
%     VOYAGE_CURRENT_MODE      = 1 (time mode)
%     VOYAGE_CURRENT_TIME_S    = 1 s grid
%     VOYAGE_CURRENT_VALUE_MPS = compiled signed current series
%     VOYAGE_WATER_DEPTH_M     = h_effective
%     PMS_SOURCE_MODE          = 0 (BASELINE_POLICY, bench convention,
%                                config/ModelE_Control_Parameters.m)
%     EMS_CTRL_LAW             = 0 (baseline law, run_scenario_sim contract)
%   The manifest is machine-whitelisted: every var_name must belong to the
%   compiler whitelist constant below or the compiler fails closed with
%   GATE:MISSION_V3:SCN:INJECT.  'check_injection' re-runs the whitelist
%   gate on an arbitrary manifest (defense in depth for the producer).
%
%   Determinism: pure function of (plan, realization, frozen files,
%   name-values); identical inputs give bitwise-identical output.  The
%   global RNG is never touched; the only stochastic-form computation is
%   the seeded, deterministic hotel_residual re-derivation described
%   above.
%
%   Error ids: GATE:MISSION_V3:SCN:<REASON>.
%
%   Discipline: ASCII-only source; no absolute paths; no chained indexing;
%   ROOT resolved from mfilename with pwd fallback; frozen values are read
%   from the frozen files (never hard-coded); no simulation is run.

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:SCN:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'compile'
            if nargin < 3
                error('GATE:MISSION_V3:SCN:ARG', ...
                    '''compile'' expects: plan, realization [, name-values]');
            end
            opts = local_parse_named(varargin(3:end));
            out = local_compile(varargin{1}, varargin{2}, opts);
        case 'check_injection'
            if nargin < 2
                error('GATE:MISSION_V3:SCN:ARG', ...
                    '''check_injection'' expects: injection_list');
            end
            out = local_check_injection(varargin{1});
        otherwise
            error('GATE:MISSION_V3:SCN:ARG', ...
                'unknown mode: %s', char(mode));
    end
end

%% ========================== injector whitelist ======================

function wl = INJECTION_WHITELIST()
% Surveyed Model E injection channels (fixed decision 8).  Origins:
%   VOYAGE_SPEED_MODE / VOYAGE_V_TIME_S / VOYAGE_V_VALUE_MPS /
%   VOYAGE_CURRENT_MODE / VOYAGE_CURRENT_TIME_S /
%   VOYAGE_CURRENT_VALUE_MPS / VOYAGE_WATER_DEPTH_M
%     -> config/ModelE_TestScenarios.m base variables; the per-run
%        assignment contract is scripts/+model/run_scenario_sim.m
%        (assignin('base', ...) per SCN switch) and the Model E blocks
%        read them at simulation compile time (build_model_e.m
%        01_Operator_Interface Constants).
%   PMS_SOURCE_MODE -> config/ModelE_Control_Parameters.m
%     (0 = BASELINE_POLICY, 1 = EMS_AGGREGATE; bench convention assigns 0).
%   EMS_CTRL_LAW -> scripts/+model/run_scenario_sim.m assignment contract
%     (double(mode); 0 = baseline law).
    wl = {'VOYAGE_SPEED_MODE'; ...
        'VOYAGE_V_TIME_S'; ...
        'VOYAGE_V_VALUE_MPS'; ...
        'VOYAGE_CURRENT_MODE'; ...
        'VOYAGE_CURRENT_TIME_S'; ...
        'VOYAGE_CURRENT_VALUE_MPS'; ...
        'VOYAGE_WATER_DEPTH_M'; ...
        'PMS_SOURCE_MODE'; ...
        'EMS_CTRL_LAW'};
end

function tf = local_in_whitelist(var_name)
    tf = ismember(var_name, INJECTION_WHITELIST());
end

function ok = local_check_injection(injection_list)
% Fail-closed whitelist gate over a manifest (fixed decision 8).
    if ~isstruct(injection_list) || isempty(injection_list)
        error('GATE:MISSION_V3:SCN:INJECT', ...
            'injection_list must be a non-empty struct array');
    end
    if ~isfield(injection_list, 'var_name') || ~isfield(injection_list, 'value')
        error('GATE:MISSION_V3:SCN:INJECT', ...
            'injection_list entries need var_name and value fields');
    end
    n = numel(injection_list);
    seen = cell(1, n);
    for k = 1:n
        nm = injection_list(k).var_name;
        if ~ischar(nm) && ~isstring(nm)
            error('GATE:MISSION_V3:SCN:INJECT', ...
                'entry %d var_name must be a string', k);
        end
        nm = char(nm);
        if ~local_in_whitelist(nm)
            error('GATE:MISSION_V3:SCN:INJECT', ...
                ['injection var_name ''%s'' is outside the surveyed ', ...
                 'Model E whitelist (fail-closed)'], nm);
        end
        seen{k} = nm;
    end
    if numel(unique(seen)) ~= n
        error('GATE:MISSION_V3:SCN:INJECT', ...
            'injection_list contains duplicate var_name entries');
    end
    ok = true;
end

%% ============================= compiler =============================

function out = local_compile(plan, rl, opts)
    plan = local_check_plan(plan);
    rl = local_check_realization(rl);
    local_check_pair(plan, rl);

    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    frz = local_load_frozen(ROOT);

    % ---- grid alignment (FD1/FD2: 1 s grid, t >= 0, one truth series) ----
    tg = plan.t_grid_s;
    n_grid = numel(tg);
    tg_real = rl.t_grid_s;
    i0 = find(tg_real >= -1e-9, 1);
    if isempty(i0) || (abs(tg_real(i0)) > 1e-9)
        error('GATE:MISSION_V3:SCN:GRID', ...
            'realization grid does not contain a t = 0 sample');
    end
    if (numel(tg_real) - i0 + 1) ~= n_grid
        error('GATE:MISSION_V3:SCN:GRID', ...
            'plan grid length %d disagrees with the realized grid', n_grid);
    end
    if any(abs(diff(tg) - rl.ou.dt_s) > 1e-9) || (abs(tg(1)) > 1e-9)
        error('GATE:MISSION_V3:SCN:GRID', ...
            'plan grid is not the frozen uniform injection grid');
    end

    % ---- FD2: R1 signed along-track current series ----
    u_river = rl.u_river_mps;
    cur = zeros(1, n_grid);
    segs = plan.segments;
    t_last_end = segs(end).t_end_s;
    for i = 1:numel(segs)
        sg = segs(i);
        if i < numel(segs)
            idx = find((tg >= sg.t_start_s) & (tg < sg.t_end_s));
        else
            % tail beyond the last window holds the last sign mapping
            idx = find(tg >= sg.t_start_s);
        end
        if isempty(idx)
            continue;
        end
        cur(idx) = sg.along_sign .* u_river(i0 + idx - 1);
    end
    if ~all(isfinite(cur))
        error('GATE:MISSION_V3:SCN:CURRENT', ...
            'compiled current series contains non-finite values');
    end

    % ---- FD3: hotel identity (DGN-09 Path B) ----
    p_base = local_hotel_base(frz.tpl, plan.template_id);
    d_hotel = local_hotel_delta(frz.dagcfg, plan.formal_seed, ...
        plan.base_draw_index);
    hotel_kW = p_base + d_hotel;

    % ---- FD4: water depth identity (DGN-06 R4) ----
    h_eff = rl.water.h_effective_m;

    % ---- FD5: ramps = frozen base x operator scale ----
    ramp_base = local_parse_ramp_base(ROOT);
    accel = ramp_base * plan.operator_ramp_scale;
    decel = ramp_base * plan.operator_ramp_scale;

    % ---- FD7: engine seed mirror of the current_truth lineage ----
    lin = mission_v3.seed_tree('branches', plan.formal_seed, ...
        plan.base_draw_index);
    e_cur = local_branch(lin, 'current_truth');
    seed_u32 = e_cur.engine_seed_uint32;
    lin_id = local_lineage_id(rl);
    seed_from_hex = uint32(hex2dec(lin_id(1:8)));
    if seed_from_hex ~= seed_u32
        error('GATE:MISSION_V3:SCN:LINEAGE', ...
            ['current_truth engine seed disagrees with the realization ', ...
             'lineage_id (plan/realization not the same draw?)']);
    end

    % ---- assemble the SCN struct (field-for-field survey alignment) ----
    scn = struct();
    scn.id = double(plan.base_draw_index);
    scn.name = ['mission_v3_', plan.template_id, '_', plan.regime_id, ...
        '_d', num2str(plan.base_draw_index)];
    scn.target_speed_mps = max(plan.v_plan_mps);
    scn.voyage_distance_m = inf;
    scn.river_current_mps = 0.0;
    scn.resistance_profile = 'const';
    scn.profile_code = 0;
    scn.resistance_change_position_m = 0;
    scn.resistance_change_amplitude = 0;
    scn.resistance_change_ramp_time_s = 0;
    scn.hotel_kW = hotel_kW;
    scn.initial_SOC = opts.initial_soc;
    scn.random_seed = seed_u32;
    scn.notes = ['mission_v3 compiled scenario (T-011); time-mode plan ', ...
        'table governs; distance stop disabled (time termination primary); ', ...
        'neutral constant resistance profile'];
    scn.v_knots_x = [0 -1 -1 -1 -1 -1];
    scn.v_knots_v = [plan.v_plan_mps(1) 0 0 0 0 0];
    scn.accel_ramp_mps2 = accel;
    scn.decel_ramp_mps2 = decel;
    scn.resistance_bands = [-1 0 0; -1 0 0; -1 0 0; -1 0 0];
    scn.current_knots_x = [0 -1 -1 -1];
    scn.current_knots_v = [0.0 0 0 0];
    scn.water_depth_m = h_eff;
    scn.speed_profile_mode = 'time';
    scn.v_time_s = tg;
    scn.v_value_mps = plan.v_plan_mps;
    scn.current_time_s = tg;
    scn.current_value_mps = cur;
    scn.stop_time_s = tg(end);

    % ---- FD8: injection manifest (list only; producer executes) ----
    names = {'VOYAGE_SPEED_MODE'; 'VOYAGE_V_TIME_S'; 'VOYAGE_V_VALUE_MPS'; ...
        'VOYAGE_CURRENT_MODE'; 'VOYAGE_CURRENT_TIME_S'; ...
        'VOYAGE_CURRENT_VALUE_MPS'; 'VOYAGE_WATER_DEPTH_M'; ...
        'PMS_SOURCE_MODE'; 'EMS_CTRL_LAW'};
    vals = {1.0; tg; plan.v_plan_mps; 1.0; tg; cur; h_eff; 0.0; 0.0};
    n_inj = numel(names);
    injection_list = struct('var_name', names, 'value', vals);
    local_check_injection(injection_list);
    for k = 1:n_inj
        if ~local_in_whitelist(names{k})
            % unreachable unless the whitelist constant above diverges
            error('GATE:MISSION_V3:SCN:INJECT', ...
                'compiler manifest entry outside whitelist: %s', names{k});
        end
    end

    % ---- provenance ----
    out = struct();
    out.kind = 'mission_v3_scn';
    out.scn = scn;
    out.injection_list = injection_list;
    out.provenance = local_provenance(frz, plan, rl, p_base, d_hotel, ...
        ramp_base, accel, decel, seed_u32, lin_id, e_cur, opts);
end

%% ========================= input validation =========================

function plan = local_check_plan(plan)
    if ~isstruct(plan) || ~isfield(plan, 'kind') ...
            || ~strcmp(plan.kind, 'mission_v3_plan')
        error('GATE:MISSION_V3:SCN:ARG', ...
            'plan must be a mission_v3.plan_compose ''compose'' struct');
    end
    need = {'formal_seed', 'base_draw_index', 'template_id', 'regime_id', ...
        'operator_profile', 'operator_ramp_scale', 't_grid_s', 'v_plan_mps', ...
        'segments', 'ht_effective'};
    for j = 1:numel(need)
        if ~isfield(plan, need{j})
            error('GATE:MISSION_V3:SCN:ARG', 'plan lacks field: %s', need{j});
        end
    end
    if ~isnumeric(plan.v_plan_mps) || ~isvector(plan.v_plan_mps) ...
            || isempty(plan.v_plan_mps) ...
            || (numel(plan.v_plan_mps) ~= numel(plan.t_grid_s))
        error('GATE:MISSION_V3:SCN:ARG', ...
            'plan v_plan_mps/t_grid_s length mismatch');
    end
end

function rl = local_check_realization(rl)
    if ~isstruct(rl) || ~isfield(rl, 'kind') || ~strcmp(rl.kind, 'realize')
        error('GATE:MISSION_V3:SCN:ARG', ...
            'realization must be a mission_v3.env_realize ''realize'' struct');
    end
    need = {'formal_seed', 'base_draw_index', 'template_id', 'regime_id', ...
        'operator_profile', 't_grid_s', 'u_river_mps', 'ou', 'water', ...
        'segments', 'disposition'};
    for j = 1:numel(need)
        if ~isfield(rl, need{j})
            error('GATE:MISSION_V3:SCN:ARG', ...
                'realization lacks field: %s', need{j});
        end
    end
    if ~isfield(rl.water, 'h_effective_m') ...
            || ~isfield(rl.water, 'h_chart_m') ...
            || ~isfield(rl.water, 'delta_wl_m')
        error('GATE:MISSION_V3:SCN:ARG', ...
            'realization water block lacks depth fields');
    end
end

function local_check_pair(plan, rl)
% Fail-closed draw identity between plan and realization.
    if ~strcmp(plan.formal_seed, rl.formal_seed) ...
            || (plan.base_draw_index ~= rl.base_draw_index) ...
            || ~strcmp(plan.template_id, rl.template_id) ...
            || ~strcmp(plan.regime_id, rl.regime_id) ...
            || ~strcmp(plan.operator_profile, rl.operator_profile)
        error('GATE:MISSION_V3:SCN:DRAW_MISMATCH', ...
            'plan and realization do not describe the same base draw');
    end
    if abs(plan.ht_effective - rl.water.ht_effective) > 1e-9
        error('GATE:MISSION_V3:SCN:DRAW_MISMATCH', ...
            'plan and realization ht_effective disagree');
    end
    if numel(plan.segments) ~= numel(rl.segments)
        error('GATE:MISSION_V3:SCN:DRAW_MISMATCH', ...
            'plan and realization segment counts disagree');
    end
    for i = 1:numel(plan.segments)
        sg = plan.segments(i);
        rs = rl.segments(i);
        if ~strcmp(sg.role, rs.role) || (sg.along_sign ~= rs.along_sign) ...
                || (abs(sg.window_s - rs.window_s) > 1e-9)
            error('GATE:MISSION_V3:SCN:DRAW_MISMATCH', ...
                'plan segment %d disagrees with the realized segment', i);
        end
    end
end

%% ========================= frozen file loading ======================

function frz = local_load_frozen(ROOT)
% Effective phase-1 freeze (base + D6 + sprint, immutable chain), the
% frozen template registry (hotel base by template_id) and the frozen
% conditional-DAG config (hotel node params) with fail-closed binding
% checks (plan_compose.local_load_frozen precedent).
    cfg = fullfile(ROOT, 'config', 'mission_v3');
    res = fullfile(ROOT, 'results', 'mission_v3');
    frz = struct();
    frz.cfg_dir = cfg;
    frz.freeze = local_read_json(res, 'phase1_freeze.json', ...
        'GATE:MISSION_V3:SCN:REGISTRY');
    frz.d6 = local_read_json(res, 'phase1_freeze_amendment_d6.json', ...
        'GATE:MISSION_V3:SCN:REGISTRY');
    frz.sprint = local_read_json(res, 'phase1_freeze_amendment_sprint.json', ...
        'GATE:MISSION_V3:SCN:REGISTRY');
    frz.tpl = local_read_json(cfg, 'mission_template_registry.json', ...
        'GATE:MISSION_V3:SCN:REGISTRY');
    frz.dagcfg = local_read_json(cfg, 'conditional_dag_config.json', ...
        'GATE:MISSION_V3:SCN:REGISTRY');

    base_hash = local_self_hash(frz.freeze);
    if ~strcmp(local_amends_hash(frz.d6), base_hash) ...
            || ~strcmp(local_amends_hash(frz.sprint), base_hash)
        error('GATE:MISSION_V3:SCN:FREEZE_MISMATCH', ...
            'amendments do not bind to the base phase1_freeze self_hash');
    end
    chain = local_as_list(frz.sprint.amendment_chain, 'amendment_chain');
    if (numel(chain) ~= 3) ...
            || ~isfield(chain{2}, 'self_hash') ...
            || ~strcmp(chain{2}.self_hash, local_self_hash(frz.d6))
        error('GATE:MISSION_V3:SCN:FREEZE_MISMATCH', ...
            'sprint amendment chain does not bind the D6 amendment');
    end
end

function h = local_self_hash(obj)
    if ~isfield(obj, 'self_hash') || ~isfield(obj.self_hash, 'value')
        error('GATE:MISSION_V3:SCN:FREEZE_MISMATCH', ...
            'frozen object lacks self_hash.value');
    end
    h = obj.self_hash.value;
end

function h = local_amends_hash(am)
    if ~isfield(am, 'amends') || ~isfield(am.amends, 'self_hash')
        error('GATE:MISSION_V3:SCN:FREEZE_MISMATCH', ...
            'amendment lacks amends.self_hash');
    end
    h = am.amends.self_hash;
end

%% ======================= hotel identity pieces ======================

function p = local_hotel_base(tpl, template_id)
% p_hotel_base(template) from the frozen template registry (FD3).
    lst = local_as_list(tpl.mission_templates, 'mission_templates');
    for k = 1:numel(lst)
        t = lst{k};
        if strcmp(t.mission_template_id, template_id)
            p = t.p_hotel_base_kW;
            if ~isnumeric(p) || ~isscalar(p) || ~isfinite(p) || (p < 0)
                error('GATE:MISSION_V3:SCN:REGISTRY', ...
                    'template %s p_hotel_base_kW is not a finite scalar >= 0', ...
                    template_id);
            end
            return;
        end
    end
    error('GATE:MISSION_V3:SCN:REGISTRY', ...
        'template not in frozen registry: %s', template_id);
end

function d = local_hotel_delta(dagcfg, formal_seed, base_draw_index)
% delta_p_hotel_unplanned(draw) re-derived DETERMINISTICALLY from the
% dedicated hotel_residual seed-tree branch (FD3).  dag_sample draws the
% hotel delta as the ONLY consumer of that branch substream: one symmetric
% truncated normal (rejection resampling, no clamping, FR-520) with the
% frozen conditional_dag_config node params.  Re-running the identical
% procedure on the identical engine seed reproduces the base draw's value
% bitwise; no new randomness is introduced.  The plan/realization contract
% carries no hotel field and the compile signature is fixed, so this
% lineage re-derivation is the traceable route to the identity.
    hp = local_node_params(dagcfg, 'delta_p_hotel_unplanned');
    mu = hp.mu;
    sigma = hp.sigma;
    abs_bound = hp.trunc_abs_kw;
    if ~isscalar(mu) || ~isfinite(mu)
        error('GATE:MISSION_V3:SCN:REGISTRY', 'hotel mu must be finite scalar');
    end
    if ~isscalar(sigma) || ~isfinite(sigma) || (sigma <= 0)
        error('GATE:MISSION_V3:SCN:REGISTRY', 'hotel sigma must be finite > 0');
    end
    if ~isscalar(abs_bound) || ~isfinite(abs_bound) || (abs_bound <= 0)
        error('GATE:MISSION_V3:SCN:REGISTRY', ...
            'hotel trunc_abs_kw must be finite > 0');
    end
    lin = mission_v3.seed_tree('branches', formal_seed, base_draw_index);
    e = local_branch(lin, 'hotel_residual');
    rs = RandStream('Threefry4x64', 'Seed', e.engine_seed_uint32);
    n = 0;
    while true
        z = randn(rs);
        n = n + 1;
        d = mu + sigma * z;
        if abs(d) <= abs_bound
            return;
        end
        if n >= 100000
            error('GATE:MISSION_V3:SCN:HOTEL_SAMPLING', ...
                'hotel truncation retry cap reached (rejection resampling)');
        end
    end
end

%% ======================= ramp base parsing (FD5) ====================

function r = local_parse_ramp_base(ROOT)
% Frozen Model E base ramp ASSUMED_Speed_Ramp_Up, parsed from
% config/ModelE_Physics_Parameters.m (read-only; never hard-coded here;
% parsing precedent: plan_compose.local_sprint_floor).  The existing
% scenario decel default is the same constant (RAMP_NORM =
% ASSUMED_Speed_Ramp_Up, ModelE_TestScenarios.m S01 precedent), so both
% compiled ramps scale from this single parsed base.
    ppath = fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m');
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:SCN:RAMP', ...
            'Model E parameters file not found: ModelE_Physics_Parameters.m');
    end
    txt = fileread(ppath);
    tok = regexp(txt, 'ASSUMED_Speed_Ramp_Up\s*=\s*([0-9]+(?:\.[0-9]+)?)', ...
        'tokens', 'once');
    if isempty(tok)
        error('GATE:MISSION_V3:SCN:RAMP', ...
            'ASSUMED_Speed_Ramp_Up not parseable in ModelE_Physics_Parameters.m');
    end
    r = str2double(tok{1});
    if ~isfinite(r) || (r <= 0)
        error('GATE:MISSION_V3:SCN:RAMP', ...
            'parsed ASSUMED_Speed_Ramp_Up is not a positive finite number');
    end
end

%% ============================ lineage helpers =======================

function e = local_branch(lin, branch_name)
    e = [];
    for k = 1:numel(lin)
        row = lin(k);
        if strcmp(row.branch_name, branch_name)
            e = row;
            return;
        end
    end
    error('GATE:MISSION_V3:SCN:SEED', 'lineage lacks branch: %s', branch_name);
end

function id = local_lineage_id(rl)
% current_truth lineage_id from the realization disposition summary.
    ds = rl.disposition;
    if ~isfield(ds, 'lineage_summary') ...
            || ~isfield(ds.lineage_summary, 'current_truth_lineage_id')
        error('GATE:MISSION_V3:SCN:SEED', ...
            'realization disposition lacks the current_truth lineage id');
    end
    id = ds.lineage_summary.current_truth_lineage_id;
    if ~ischar(id) || isempty(regexp(id, '^[0-9a-f]{64}$', 'once'))
        error('GATE:MISSION_V3:SCN:SEED', ...
            'current_truth lineage_id is not 64 lowercase hex chars');
    end
end

function p = local_node_params(dagcfg, node_id)
    nodes = local_as_list(dagcfg.nodes, 'nodes');
    for k = 1:numel(nodes)
        nd = nodes{k};
        if strcmp(nd.node_id, node_id)
            if ~isfield(nd, 'params')
                error('GATE:MISSION_V3:SCN:REGISTRY', ...
                    'dag node %s lacks params', node_id);
            end
            p = nd.params;
            return;
        end
    end
    error('GATE:MISSION_V3:SCN:REGISTRY', 'dag node not found: %s', node_id);
end

%% ========================= name-value parsing =======================

function opts = local_parse_named(arg_cell)
    opts = struct('initial_soc', 0.6);
    k = 1;
    while k <= numel(arg_cell)
        if (k + 1) > numel(arg_cell)
            error('GATE:MISSION_V3:SCN:ARG', ...
                'missing value for option %s', string(arg_cell{k}));
        end
        opt = arg_cell{k};
        val = arg_cell{k + 1};
        if strcmpi(opt, 'InitialSOC')
            if ~isnumeric(val) || ~isscalar(val) || ~isfinite(val) ...
                    || (val <= 0) || (val > 1)
                error('GATE:MISSION_V3:SCN:ARG', ...
                    'InitialSOC must be a finite scalar in (0, 1]');
            end
            opts.initial_soc = val;
        else
            error('GATE:MISSION_V3:SCN:ARG', ...
                'unknown option: %s', string(opt));
        end
        k = k + 2;
    end
end

%% ============================= provenance ===========================

function prov = local_provenance(frz, plan, rl, p_base, d_hotel, ...
        ramp_base, accel, decel, seed_u32, lin_id, e_cur, opts)
    prov = struct();
    prov.generator = ['mission_v3.scenario_compile (T-011; design ', ...
        'DGN-08/09; delegation spec ', ...
        'experiments/mission_v3/T-011_delegation_spec.md ', ...
        'Fixed Decisions FD1-FD10)'];

    % frozen source hash references
    prov.hash_phase1_freeze = local_self_hash(frz.freeze);
    prov.hash_freeze_amendment_d6 = local_self_hash(frz.d6);
    prov.hash_freeze_amendment_sprint = local_self_hash(frz.sprint);
    d6e = frz.d6.effective_phase1_configuration;
    if isfield(d6e, 'composite_hash')
        prov.hash_freeze_composite = d6e.composite_hash;
    else
        prov.hash_freeze_composite = '';
    end
    prov.hash_template_registry = local_self_hash(frz.tpl);
    prov.hash_conditional_dag = local_self_hash(frz.dagcfg);

    % identities (numbered, test-asserted)
    prov.identity_hotel = ['FD3 / DGN-09 Path B: SCN.hotel_kW = ', ...
        'p_hotel_base(template) + delta_p_hotel_unplanned(draw); ', ...
        'p_hotel_base read from mission_template_registry.json by ', ...
        'template_id; delta re-derived deterministically from the ', ...
        'dedicated hotel_residual lineage branch (single truncated ', ...
        'normal, rejection resampling, frozen node params; bitwise ', ...
        'identical to the dag_sample draw; no new randomness)'];
    prov.identity_water_depth = ['FD4 / DGN-06 R4: SCN.water_depth_m = ', ...
        'h_chart + delta_wl = realization.water.h_effective_m (voyage ', ...
        'scalar)'];
    prov.identity_current = ['FD2 (R1): current_value_mps(k) = ', ...
        'along_sign(segment(t_k)) x U_river(t_k) on the 1 s grid, ', ...
        'windows stitched on the T-010 plan windows; outbound -1 / ', ...
        'return +1 / berth,standby 0; negative = adverse (S03 ', ...
        'semantics); beyond the last window the last segment sign ', ...
        'mapping is held (berth for all templates -> 0)'];
    prov.identity_ramp = ['FD5: accel = decel = ASSUMED_Speed_Ramp_Up ', ...
        '(parsed read-only from config/ModelE_Physics_Parameters.m) x ', ...
        'plan.operator_ramp_scale (URGENT 1.2 / CALM 0.8 / STANDARD ', ...
        '1.0, T-010 FD7); existing decel default = RAMP_NORM = the ', ...
        'same ASSUMED_Speed_Ramp_Up constant (ModelE_TestScenarios.m ', ...
        'S01 precedent)'];
    prov.identity_seed = ['FD7: SCN.random_seed = current_truth ', ...
        'engine_seed_uint32 (first 8 hex of lineage_id, seed_tree ', ...
        'engine mapping), cross-checked against the realization ', ...
        'disposition lineage summary; full digest stays in ', ...
        'plan/realization lineage; SCN mirrors only the engine seed'];

    % field mapping table (survey alignment, FD1; consumption sites
    % verified in config/ModelE_TestScenarios.m, scripts/+model/
    % run_scenario_sim.m, scripts/+model/build_model_e.m)
    fm = struct();
    fm.id = 'numeric scenario id = base_draw_index (ScenID constant)';
    fm.name = ['ASCII identifier mission_v3_<template>_<regime>_d<draw> ', ...
        '(reporting only)'];
    fm.target_speed_mps = ['max(v_plan) nominal peak; time-mode table ', ...
        'governs v_ref (C_vtarget / v_def fallback only)'];
    fm.voyage_distance_m = ['inf: distance stop disabled, time ', ...
        'termination primary via stop_time_s (S20 relaxed-distance ', ...
        'precedent; operator upscale can exceed the nominal distance)'];
    fm.river_current_mps = '0.0 (distance-mode fallback; inert in time mode)';
    fm.resistance_profile = '''const'' neutral (no mission v3 channel)';
    fm.profile_code = '0 (const resistance multiplier = 1)';
    fm.resistance_change_position_m = '0 (neutral)';
    fm.resistance_change_amplitude = '0 (neutral)';
    fm.resistance_change_ramp_time_s = '0 (neutral)';
    fm.hotel_kW = 'FD3 identity (p_hotel_base + delta_p_hotel_unplanned)';
    fm.initial_SOC = 'InitialSOC name-value, default 0.6 (FD6; T-005 scan precedent)';
    fm.random_seed = 'FD7 current_truth engine seed mirror (uint32)';
    fm.notes = 'ASCII provenance summary (reporting only)';
    fm.v_knots_x = 'distance-mode sentinel [0 -1 -1 -1 -1 -1] (inert in time mode)';
    fm.v_knots_v = 'distance-mode sentinel, first value v_plan(1) (inert in time mode)';
    fm.accel_ramp_mps2 = 'FD5 identity (parsed base x operator_ramp_scale)';
    fm.decel_ramp_mps2 = 'FD5 identity (parsed base x operator_ramp_scale)';
    fm.resistance_bands = 'neutral 4x3 sentinel rows (inert, profile const)';
    fm.current_knots_x = 'distance-mode sentinel [0 -1 -1 -1] (inert in time mode)';
    fm.current_knots_v = 'distance-mode sentinel zeros (inert in time mode)';
    fm.water_depth_m = 'FD4 identity (h_chart + delta_wl = h_effective)';
    fm.speed_profile_mode = '''time'' (FD1; Model E ZOH table + rate limiter)';
    fm.v_time_s = 'plan.t_grid_s (1 s, t >= 0)';
    fm.v_value_mps = 'plan.v_plan_mps';
    fm.current_time_s = '1 s grid 0:t_end (FD2)';
    fm.current_value_mps = 'FD2 R1 signed along-track series';
    fm.stop_time_s = 'plan horizon end = t_end (producer StopTime source)';
    prov.field_map = fm;

    % injection manifest origins (FD8)
    prov.injection_whitelist = ['FD8 machine whitelist; channels: ', ...
        'VOYAGE_SPEED_MODE / VOYAGE_V_TIME_S / VOYAGE_V_VALUE_MPS / ', ...
        'VOYAGE_CURRENT_MODE / VOYAGE_CURRENT_TIME_S / ', ...
        'VOYAGE_CURRENT_VALUE_MPS / VOYAGE_WATER_DEPTH_M from ', ...
        'config/ModelE_TestScenarios.m + the run_scenario_sim.m ', ...
        'assignin contract; PMS_SOURCE_MODE = 0 = BASELINE_POLICY ', ...
        '(config/ModelE_Control_Parameters.m, bench convention); ', ...
        'EMS_CTRL_LAW = 0 (run_scenario_sim.m contract); the ', ...
        'compiler only produces the list, execution belongs to the ', ...
        'T-015 producer; whitelist violations fail closed with ', ...
        'GATE:MISSION_V3:SCN:INJECT'];

    % determinism note
    prov.hotel_values = struct('p_hotel_base_kW', p_base, ...
        'delta_p_hotel_unplanned_kW', d_hotel, ...
        'hotel_kW', p_base + d_hotel);
    prov.ramp_values = struct('base_mps2', ramp_base, ...
        'operator_ramp_scale', plan.operator_ramp_scale, ...
        'accel_mps2', accel, 'decel_mps2', decel);
    prov.seed_record = struct('branch_name', e_cur.branch_name, ...
        'lineage_id', lin_id, 'engine_seed_uint32', seed_u32);
    prov.initial_soc = struct('value', opts.initial_soc, ...
        'source', 'InitialSOC name-value (default 0.6, FD6)');
    prov.draw = struct('formal_seed', plan.formal_seed, ...
        'base_draw_index', plan.base_draw_index, ...
        'template_id', plan.template_id, 'regime_id', plan.regime_id, ...
        'operator_profile', plan.operator_profile, ...
        'ht_effective', rl.water.ht_effective);
    prov.determinism = ['pure deterministic transform of (plan, ', ...
        'realization, frozen files, name-values); no global RNG; no ', ...
        'simulation run; zero Model E file changes'];
end

%% ============================ misc helpers ==========================

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

function lst = local_as_list(v, what)
% Normalize a JSON array (jsondecode: struct array when fields are
% uniform, cell array of structs otherwise) into a cell list.
    if iscell(v)
        lst = v(:)';
    elseif isstruct(v)
        n = numel(v);
        lst = cell(1, n);
        for k = 1:n
            lst{k} = v(k);
        end
    else
        error('GATE:MISSION_V3:SCN:REGISTRY', ...
            'expected JSON array for %s', what);
    end
end

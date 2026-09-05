function dag = dag_sample(mode, varargin)
% mission_v3.dag_sample - T-009 conditional-DAG base-draw sampler (DGN-03)
%
%   dag = mission_v3.dag_sample('draw', formalSeed, baseDrawIndex, ...
%                               templateId, regimeId, stratumId, ...)
%
%   Name-values (both optional):
%     'DraftM'        voyage draft [m], default 2.188 = Model E Hull_T_m
%                     (T-005 precedent; delegation spec fixed decision 7)
%     'ParamOverride' struct recursively merged over the loaded
%                     environment regime params AFTER the frozen file is
%                     read; TEST-ONLY injection surface (existing keys
%                     only; schema/self_hash keys are not overridable);
%                     the frozen registry file itself is never modified.
%
% L0/L1 inputs (template / regime / stratum) are quota-driven caller
% inputs, not random draws here (fixed decision 1).  The reach sequence is
% derived deterministically from the frozen template registry.
%
% Randomness: every draw comes from one dedicated Threefry4x64 substream
% per seed-tree branch (mission_v3.seed_tree, DGN-07).  The global RNG is
% never touched.  Branch-to-draw mapping (fixed decision 2):
%   current_truth     u_river_mean (river coordinate, downstream positive;
%                     TruncNormal(mu_regime, sigma_regime, lower = 0)); the
%                     SAME substream is later continued inside env_realize
%                     for the OU increment sequence (fixed decision 4), so
%                     the number of normal draws consumed here is recorded
%                     in dag.current_truth_ff_n for a deterministic
%                     fast-forward
%   water_level       h_chart (chart_depth_voyage_base per regime,
%                     lower-bounded TruncNormal) then delta_wl
%                     (water_level_offset per regime, plain normal; the
%                     frozen registry declares no explicit truncation
%                     bound for the offset, so its truncation semantics
%                     live in the env_realize HT support-bound loop)
%   segment_geometry  per-segment stretch ~ U[0.8, 1.25] in template order
%   traffic           per segment [Poisson count with lambda(regime x reach)
%                     over the estimated segment window -> per event
%                     start_frac ~ U(0,1), duration ~ U[60,180] s,
%                     speed_ratio ~ U[0.5,0.8]]
%   operator_residual operator_profile category (inverse CDF over the
%                     frozen operator_prior with one uniform; category
%                     order = operator_prior field order)
%   hotel_residual    delta_p_hotel_unplanned ~ N(0, 10^2) truncated to
%                     +/- 20 kW (rejection resampling; clamping forbidden,
%                     FR-520)
%   resistance        resistance_mult ~ U[0.95, 1.05]
%   forecast_obs_noise  registered for lineage completeness; consumed by
%                     the forecast generator (T-011), never by truth
%                     generation at this layer
%
% Cross-branch execution order is exactly the listing above (fixed
% decision 2).  Each branch owns an independent substream, so the order
% cannot change any value; it is fixed anyway so draw ledgers reproduce
% step by step.  Within-branch order is documented at each draw site.
%
% v1 registered simplifications (delegation spec fixed decisions 3 and 6;
% also recorded in dag.provenance):
%   - u_river_mean is ONE draw per voyage with per-regime parameters; the
%     frozen DAG edge [regime x reach_sequence] degenerates to this
%     per-regime parameterization (reach still conditions the traffic
%     lambda table; the river flow is a voyage-level, spatially uniform
%     time process shared by outbound and return segments, R1).
%   - traffic event windows use frozen plan-speed-table estimates:
%     window = nominal_distance x stretch / frozen role speed (rush tiers
%     by voyage HT; standby by nominal_duration_s).
%   - segments flagged presence = CONDITIONAL in the template registry
%     (the EMERGENCY_RESPONSE bridge segment) are realized as PRESENT at
%     this layer; conditional presence consumption belongs to the plan
%     layer (T-010), per the registry note.
%
% Poisson counts use the exact Knuth product sampler (exact, not an
% approximation); it consumes a variable, reproducible number of uniforms
% from the traffic substream.
%
% Truncation semantics: every truncated distribution uses rejection
% resampling; clamping is forbidden (FR-520).
%
% Determinism: pure function of (formalSeed, baseDrawIndex, frozen
% registries, name-values); identical inputs give bitwise-identical
% output.
%
% Error ids: GATE:MISSION_V3:DAG:<REASON>; base-freeze binding mismatch:
% GATE:MISSION_V3:DAG:FREEZE_MISMATCH.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% no global RNG; ROOT resolved from mfilename with pwd fallback.

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:DAG:ARG', 'mode must be a char vector');
    end
    if ~strcmp(char(mode), 'draw')
        error('GATE:MISSION_V3:DAG:ARG', ...
            'unknown mode: %s (dag_sample implements ''draw'' only)', char(mode));
    end
    if nargin < 6
        error('GATE:MISSION_V3:DAG:ARG', ...
            ['''draw'' expects: formalSeed, baseDrawIndex, templateId, ', ...
             'regimeId, stratumId']);
    end
    formal_seed = varargin{1};
    draw_index = varargin{2};
    template_id = varargin{3};
    regime_id = varargin{4};
    stratum_id = varargin{5};
    if ~ischar(template_id) && ~isstring(template_id)
        error('GATE:MISSION_V3:DAG:ARG', 'templateId must be a string');
    end
    if ~ischar(regime_id) && ~isstring(regime_id)
        error('GATE:MISSION_V3:DAG:ARG', 'regimeId must be a string');
    end
    if ~ischar(stratum_id) && ~isstring(stratum_id)
        error('GATE:MISSION_V3:DAG:ARG', 'stratumId must be a string');
    end
    template_id = char(template_id);
    regime_id = char(regime_id);
    stratum_id = char(stratum_id);
    if ~isnumeric(draw_index) || ~isscalar(draw_index) || ~isfinite(draw_index) ...
            || draw_index < 0 || draw_index ~= floor(draw_index)
        error('GATE:MISSION_V3:DAG:ARG', ...
            'baseDrawIndex must be a whole number >= 0');
    end

    draft_m = 2.188;   % Model E Hull_T_m (fixed decision 7; T-005 precedent)
    ov = struct();
    k = 6;
    while k <= numel(varargin)
        if (k + 1) > numel(varargin)
            error('GATE:MISSION_V3:DAG:ARG', ...
                'missing value for option %s', string(varargin{k}));
        end
        opt = varargin{k};
        val = varargin{k + 1};
        if strcmpi(opt, 'DraftM')
            draft_m = local_pos_scalar(val, 'DraftM');
        elseif strcmpi(opt, 'ParamOverride')
            if ~isstruct(val)
                error('GATE:MISSION_V3:DAG:ARG', 'ParamOverride must be a struct');
            end
            ov = val;
        else
            error('GATE:MISSION_V3:DAG:ARG', 'unknown option: %s', string(opt));
        end
        k = k + 2;
    end

    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    reg = local_load_registries(ROOT);
    env = local_apply_override(reg.env, ov);

    tpl = local_get_template(reg.tpl, template_id);
    env_r = local_regime_entry(env, regime_id, 'GATE:MISSION_V3:DAG:REGIME');
    urm = env_r.u_river_mag;
    wl = env_r.water_level_offset;
    cdb = env.chart_depth_voyage_base;
    cdb_r = cdb.per_regime;
    if ~isfield(cdb_r, regime_id)
        error('GATE:MISSION_V3:DAG:REGIME', ...
            'chart_depth_voyage_base lacks regime: %s', regime_id);
    end
    ch = cdb_r.(regime_id);

    % ---- one Threefry4x64 substream per seed-tree branch (DGN-07) ----
    lin = mission_v3.seed_tree('branches', formal_seed, draw_index);
    rs_cur = local_stream(lin, 'current_truth');
    rs_wl = local_stream(lin, 'water_level');
    rs_geo = local_stream(lin, 'segment_geometry');
    rs_trf = local_stream(lin, 'traffic');
    rs_op = local_stream(lin, 'operator_residual');
    rs_htl = local_stream(lin, 'hotel_residual');
    rs_res = local_stream(lin, 'resistance');

    % ---- (1) current_truth: u_river_mean, lower-bounded TruncNormal ----
    [u_river_mean, ff_n] = local_trunc_normal_lower(rs_cur, urm.mu, urm.sigma, urm.lower);

    % ---- (2) water_level: h_chart first, then delta_wl (fixed order) ----
    [h_chart, ~] = local_trunc_normal_lower(rs_wl, ch.mu, ch.sigma, ch.lower);
    delta_wl = wl.mu + wl.sigma * randn(rs_wl);
    h_eff = h_chart + delta_wl;
    ht = h_eff / draft_m;

    % ---- (3) segment_geometry: one stretch per segment, template order ----
    segs = local_as_list(tpl.segment_sequence, 'segment_sequence');
    nseg = numel(segs);
    st_p = local_node_params(reg.dagcfg, 'segment_stretch');
    u_geo = rand(rs_geo, [1, nseg]);
    stretch = st_p.low + (st_p.high - st_p.low) .* u_geo;

    % ---- (4) traffic: per segment [count -> events], template order ----
    pst = reg.freeze.plan_speed_table_frozen;
    speeds = pst.role_speeds_mps;
    trp = env.traffic;
    rates = trp.rate_per_hour;
    evd = trp.event_duration_s;
    spr = trp.speed_ratio;

    blank_ev = struct('start_frac', 0.0, 'duration_s', 0.0, 'speed_ratio', 0.0);
    blank_trf = struct('count', 0, 'lambda_per_hour', 0.0, 'window_hours', 0.0, ...
        'events', {repmat(blank_ev, 1, 0)});
    blank_seg = struct('role', '', 'reach', '', 'nominal_distance_m', 0.0, ...
        'nominal_duration_s', [], 'stretch', 0.0, 'direction', '', ...
        'along_sign', 0, 'planned_speed_mps', [], 'window_s', 0.0);
    seg_out = repmat(blank_seg, 1, nseg);
    trf_out = repmat(blank_trf, 1, nseg);
    win = zeros(1, nseg);

    for i = 1:nseg
        sg = segs{i};
        if ~isfield(sg, 'role') || ~isfield(sg, 'reach') || ~isfield(sg, 'nominal_distance_m')
            error('GATE:MISSION_V3:DAG:REGISTRY', ...
                'template %s segment %d lacks role/reach/nominal_distance_m', template_id, i);
        end
        role_i = sg.role;
        reach_i = sg.reach;
        [dir_i, sgn_i] = local_direction(role_i);
        row = blank_seg;
        row.role = role_i;
        row.reach = reach_i;
        row.nominal_distance_m = sg.nominal_distance_m;
        row.stretch = stretch(i);
        row.direction = dir_i;
        row.along_sign = sgn_i;
        if strcmp(role_i, 'standby')
            % duration-defined segment: no speed, no stretch scaling
            if ~isfield(sg, 'nominal_duration_s')
                error('GATE:MISSION_V3:DAG:REGISTRY', ...
                    'standby segment %d lacks nominal_duration_s', i);
            end
            row.nominal_duration_s = sg.nominal_duration_s;
            win(i) = sg.nominal_duration_s;
        else
            if strcmp(role_i, 'rush')
                v_i = local_rush_speed(pst, ht);
            else
                if ~isfield(speeds, role_i)
                    error('GATE:MISSION_V3:DAG:SPEED', ...
                        'frozen plan speed table lacks role: %s', role_i);
                end
                v_i = speeds.(role_i);
            end
            row.planned_speed_mps = v_i;
            win(i) = sg.nominal_distance_m * stretch(i) / v_i;
        end
        row.window_s = win(i);
        seg_out(i) = row;

        % traffic branch draws: Poisson count first, then per event
        % (start_frac, duration, speed_ratio) in event order
        if ~isfield(rates, reach_i)
            error('GATE:MISSION_V3:DAG:REGISTRY', ...
                'traffic rate table lacks reach: %s', reach_i);
        end
        lam_r = rates.(reach_i);
        if ~isfield(lam_r, regime_id)
            error('GATE:MISSION_V3:DAG:REGIME', ...
                'traffic rate table lacks regime %s for reach %s', regime_id, reach_i);
        end
        lam = lam_r.(regime_id);
        lam_h = lam * win(i) / 3600.0;
        cnt = local_poisson(rs_trf, lam_h);
        evs = repmat(blank_ev, 1, cnt);
        for e = 1:cnt
            e_row = blank_ev;
            e_row.start_frac = rand(rs_trf);
            e_row.duration_s = evd.low + (evd.high - evd.low) * rand(rs_trf);
            e_row.speed_ratio = spr.low + (spr.high - spr.low) * rand(rs_trf);
            evs(e) = e_row;
        end
        t_row = blank_trf;
        t_row.count = cnt;
        t_row.lambda_per_hour = lam;
        t_row.window_hours = lam_h;
        t_row.events = evs;
        trf_out(i) = t_row;
    end

    % ---- (5) operator_residual: one uniform, inverse CDF ----
    u_op = rand(rs_op);
    op_profile = local_inverse_cdf(tpl.operator_prior, u_op);

    % ---- (6) hotel_residual: symmetric truncated normal ----
    hp = local_node_params(reg.dagcfg, 'delta_p_hotel_unplanned');
    [dph, ~] = local_trunc_normal_sym(rs_htl, hp.mu, hp.sigma, hp.trunc_abs_kw);

    % ---- (7) resistance: uniform ----
    rp = local_node_params(reg.dagcfg, 'resistance_mult');
    rmu = rp.low + (rp.high - rp.low) * rand(rs_res);

    reach_seq = cell(1, nseg);
    for i = 1:nseg
        reach_seq{i} = seg_out(i).reach;
    end

    % ---- provenance (formulas, v1 registrations, frozen hashes) ----
    prov = struct();
    prov.generator = ['mission_v3.dag_sample (T-009; design DGN-03; ', ...
        'delegation spec experiments/mission_v3/T-009_delegation_spec.md)'];
    prov.lineage = ['mission_v3.seed_tree branch substreams (Threefry4x64; ', ...
        'frozen policy config/mission_v3/seed_tree_policy.json)'];
    prov.draw_order = ['fixed decision 2 listing: current_truth, water_level, ', ...
        'segment_geometry, traffic, operator_residual, hotel_residual, ', ...
        'resistance; forecast_obs_noise reserved for T-011'];
    prov.truncation = 'rejection resampling only; clamping forbidden (FR-520)';
    prov.v1_u_river = ['one u_river_mean per voyage, per-regime ', ...
        'TruncNormal(lower=0); frozen edge regime x reach_sequence ', ...
        'degenerates in v1 (fixed decision 3); river flow = voyage-level ', ...
        'spatially uniform time process shared by outbound/return (R1)'];
    prov.ou_continuation = ['current_truth substream continues in ', ...
        'env_realize for the OU increments; dag.current_truth_ff_n = ', ...
        'normal draws to fast-forward (single continuous stream)'];
    prov.v1_traffic_windows = ['window = nominal_distance x stretch / ', ...
        'frozen plan speed; rush tiers by voyage HT; standby by ', ...
        'nominal_duration_s (fixed decision 6)'];
    prov.v1_conditional_presence = ['presence=CONDITIONAL segments ', ...
        '(EMERGENCY_RESPONSE bridge) realized as PRESENT at this layer; ', ...
        'conditional consumption belongs to the plan layer (T-010)'];
    prov.water_level = ['water_level_offset drawn as plain normal (no ', ...
        'explicit truncation bound in the frozen registry); HT truncation ', ...
        'lives in the env_realize support-bound loop'];
    prov.draft = ['draft_m = ', sprintf('%.4f', draft_m), ...
        ' (Model E Hull_T_m; fixed decision 7)'];
    prov = local_add_hash(prov, 'hash_env_params', reg.env);
    prov = local_add_hash(prov, 'hash_template_registry', reg.tpl);
    prov = local_add_hash(prov, 'hash_conditional_dag', reg.dagcfg);
    prov = local_add_hash(prov, 'hash_id_support_bounds', reg.bounds);
    prov = local_add_hash(prov, 'hash_heldout_tokens', reg.tokens);
    prov = local_add_hash(prov, 'hash_phase1_freeze', reg.freeze);
    d6e = reg.d6.effective_phase1_configuration;
    if ~isfield(d6e, 'composite_hash')
        error('GATE:MISSION_V3:DAG:FREEZE_MISMATCH', ...
            'd6 amendment lacks effective_phase1_configuration.composite_hash');
    end
    prov.hash_freeze_composite = d6e.composite_hash;

    % ---- assemble the base-draw record ----
    dag = struct();
    dag.kind = 'dag_draw';
    dag.formal_seed = formal_seed;
    dag.base_draw_index = draw_index;
    dag.stratum_id = stratum_id;
    dag.template_id = template_id;
    dag.regime_id = regime_id;
    dag.operator_profile = op_profile;
    dag.p_hotel_base_kW = tpl.p_hotel_base_kW;
    dag.u_river_mean = u_river_mean;
    dag.current_truth_ff_n = ff_n;
    dag.h_chart = h_chart;
    dag.delta_wl = delta_wl;
    dag.h_effective = h_eff;
    dag.draft_m = draft_m;
    dag.ht_effective = ht;
    dag.delta_p_hotel_unplanned = dph;
    dag.resistance_mult = rmu;
    dag.reach_sequence = reach_seq;
    dag.segments = seg_out;
    dag.traffic = trf_out;
    dag.lineage = lin;
    dag.provenance = prov;
end

%% ======================== local functions ==========================

function reg = local_load_registries(ROOT)
% Load the frozen consumption registries + the effective phase-1 freeze
% (base snapshot + D6 amendment) with a fail-closed binding check.
    cfg = fullfile(ROOT, 'config', 'mission_v3');
    res = fullfile(ROOT, 'results', 'mission_v3');
    reg = struct();
    reg.env = local_read_json(cfg, 'environment_regime_params.json');
    reg.tpl = local_read_json(cfg, 'mission_template_registry.json');
    reg.dagcfg = local_read_json(cfg, 'conditional_dag_config.json');
    reg.bounds = local_read_json(cfg, 'id_support_bounds.json');
    reg.tokens = local_read_json(cfg, 'heldout_token_registry.json');
    reg.freeze = local_read_json(res, 'phase1_freeze.json');
    reg.d6 = local_read_json(res, 'phase1_freeze_amendment_d6.json');
    am = reg.d6.amends;
    if ~isstruct(am) || ~isfield(am, 'self_hash')
        error('GATE:MISSION_V3:DAG:FREEZE_MISMATCH', ...
            'phase1_freeze_amendment_d6 lacks amends.self_hash');
    end
    frz = reg.freeze;
    base_hash = '';
    if isfield(frz, 'self_hash')
        sh = frz.self_hash;
        if isfield(sh, 'value')
            base_hash = sh.value;
        end
    end
    if ~strcmp(am.self_hash, base_hash)
        error('GATE:MISSION_V3:DAG:FREEZE_MISMATCH', ...
            'phase1_freeze_amendment_d6 amends a different base freeze');
    end
end

function obj = local_read_json(dirpath, fname)
% jsondecode is the sanctioned consumption parser here (seed_tree.m
% precedent): the frozen files are validator-clean (strict parser +
% self_hash cross-check in mission_v3.validate_registries).
    ppath = fullfile(dirpath, fname);
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:DAG:REGISTRY', 'frozen file not found: %s', fname);
    end
    try
        obj = jsondecode(fileread(ppath));
    catch dec_err
        error('GATE:MISSION_V3:DAG:REGISTRY', ...
            'jsondecode failed for %s: %s', fname, dec_err.message);
    end
    if ~isstruct(obj)
        error('GATE:MISSION_V3:DAG:REGISTRY', ...
            'jsondecode did not return a struct for %s', fname);
    end
end

function out = local_apply_override(params, ov)
% Recursive TEST-ONLY override of the loaded env params; existing keys
% only, schema/self_hash never overridable (fail-closed whitelist by
% existence).
    out = params;
    if ~isstruct(ov)
        return;
    end
    fn = fieldnames(ov);
    for k = 1:numel(fn)
        key = fn{k};
        if strcmp(key, 'self_hash') || strcmp(key, 'schema')
            error('GATE:MISSION_V3:DAG:OVERRIDE', ...
                'override of %s is not permitted', key);
        end
        if ~isfield(out, key)
            error('GATE:MISSION_V3:DAG:OVERRIDE', ...
                'override key not present in frozen params: %s', key);
        end
        v = ov.(key);
        if isstruct(v) && isstruct(out.(key))
            out.(key) = local_apply_override(out.(key), v);
        else
            out.(key) = v;
        end
    end
end

function tpl = local_get_template(tplreg, template_id)
    lst = local_as_list(tplreg.mission_templates, 'mission_templates');
    for k = 1:numel(lst)
        t = lst{k};
        if strcmp(t.mission_template_id, template_id)
            tpl = t;
            return;
        end
    end
    error('GATE:MISSION_V3:DAG:TEMPLATE', ...
        'template not in frozen registry: %s', template_id);
end

function r = local_regime_entry(env, regime_id, err_id)
    if ~isfield(env, 'regimes') || ~isfield(env.regimes, regime_id)
        error(err_id, 'regime not in frozen env params: %s', regime_id);
    end
    r = env.regimes.(regime_id);
    need = {'u_river_mag', 'delta_u', 'water_level_offset'};
    for k = 1:numel(need)
        if ~isfield(r, need{k})
            error(err_id, 'regime %s lacks %s', regime_id, need{k});
        end
    end
end

function rs = local_stream(lin, branch_name)
    e = local_branch(lin, branch_name);
    rs = RandStream('Threefry4x64', 'Seed', e.engine_seed_uint32);
end

function e = local_branch(lin, branch_name)
    e = [];
    for k = 1:numel(lin)
        row = lin(k);
        if strcmp(row.branch_name, branch_name)
            e = row;
            return;
        end
    end
    error('GATE:MISSION_V3:DAG:SEED', 'lineage lacks branch: %s', branch_name);
end

function [dir_name, sgn] = local_direction(role)
% Fixed decision 5 mapping: outbound segments = upstream (along-track
% relative component = -U_river), return = downstream (+U_river),
% berth/standby = none (0).  Reversing heading never re-draws the
% environment (R1).
    if strcmp(role, 'return')
        dir_name = 'downstream';
        sgn = 1;
    elseif strcmp(role, 'berth') || strcmp(role, 'standby')
        dir_name = 'none';
        sgn = 0;
    else
        dir_name = 'upstream';
        sgn = -1;
    end
end

function v = local_rush_speed(pst, ht)
% Rush tier speeds come from the frozen table; the tier boundary (HT = 2.0)
% is encoded in the frozen key names 'HT_in_[1.5,2.0)' / 'HT_ge_2.0'
% (phase1_freeze.plan_speed_table_frozen.rush_tiers); the boundary number
% is not machine-readable as a JSON value, so it is pinned here with this
% provenance comment.
    rt = pst.rush_tiers;
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
    if isnan(v_in) || isnan(v_ge)
        error('GATE:MISSION_V3:DAG:SPEED', ...
            'rush_tiers keys not recognized in the frozen plan speed table');
    end
    if ht >= 2.0
        v = v_ge;
    else
        v = v_in;
    end
end

function [x, n] = local_trunc_normal_lower(rs, mu, sigma, lower)
% Lower-bounded truncated normal via rejection resampling (no clamp).
% Returns the value and the number of normal draws consumed (needed for
% the deterministic current_truth fast-forward in env_realize).
    local_norm_args(mu, sigma);
    n = 0;
    while true
        z = randn(rs);
        n = n + 1;
        x = mu + sigma * z;
        if x >= lower
            return;
        end
        if n >= 100000
            error('GATE:MISSION_V3:DAG:SAMPLING', ...
                'truncation retry cap reached (lower-bounded normal)');
        end
    end
end

function [x, n] = local_trunc_normal_sym(rs, mu, sigma, abs_bound)
% Symmetrically truncated normal via rejection resampling (no clamp).
    local_norm_args(mu, sigma);
    if ~isscalar(abs_bound) || ~isfinite(abs_bound) || abs_bound <= 0
        error('GATE:MISSION_V3:DAG:SAMPLING', 'abs_bound must be finite > 0');
    end
    n = 0;
    while true
        z = randn(rs);
        n = n + 1;
        x = mu + sigma * z;
        if abs(x) <= abs_bound
            return;
        end
        if n >= 100000
            error('GATE:MISSION_V3:DAG:SAMPLING', ...
                'truncation retry cap reached (symmetric normal)');
        end
    end
end

function k = local_poisson(rs, lam)
% Exact Knuth product sampler for Poisson counts (exact, not an
% approximation); consumes a variable, reproducible number of uniforms.
    if ~isscalar(lam) || ~isfinite(lam) || lam < 0
        error('GATE:MISSION_V3:DAG:SAMPLING', 'lambda must be finite >= 0');
    end
    L = exp(-lam);
    k = 0;
    p = 1.0;
    while true
        k = k + 1;
        p = p * rand(rs);
        if p <= L
            break;
        end
        if k >= 1000000
            error('GATE:MISSION_V3:DAG:SAMPLING', 'poisson sampler cap reached');
        end
    end
    k = k - 1;
end

function cls = local_inverse_cdf(prior, u)
% Inverse CDF over the frozen operator_prior; category order = field order
% of the decoded registry object.
    fn = fieldnames(prior);
    cum = 0.0;
    for k = 1:numel(fn)
        cum = cum + prior.(fn{k});
        if u < cum
            cls = fn{k};
            return;
        end
    end
    cls = fn{end};   % numerical tail fallback
end

function p = local_node_params(dagcfg, node_id)
    nodes = local_as_list(dagcfg.nodes, 'nodes');
    for k = 1:numel(nodes)
        nd = nodes{k};
        if strcmp(nd.node_id, node_id)
            if ~isfield(nd, 'params')
                error('GATE:MISSION_V3:DAG:REGISTRY', ...
                    'dag node %s lacks params', node_id);
            end
            p = nd.params;
            return;
        end
    end
    error('GATE:MISSION_V3:DAG:REGISTRY', 'dag node not found: %s', node_id);
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
        error('GATE:MISSION_V3:DAG:REGISTRY', 'expected JSON array for %s', what);
    end
end

function prov = local_add_hash(prov, name, obj)
    if ~isfield(obj, 'self_hash')
        error('GATE:MISSION_V3:DAG:REGISTRY', ...
            'frozen object lacks self_hash for %s', name);
    end
    sh = obj.self_hash;
    if ~isfield(sh, 'value')
        error('GATE:MISSION_V3:DAG:REGISTRY', ...
            'frozen object self_hash lacks value for %s', name);
    end
    prov.(name) = sh.value;
end

function v = local_pos_scalar(val, name)
    if ~isnumeric(val) || ~isscalar(val) || ~isfinite(val) || val <= 0
        error('GATE:MISSION_V3:DAG:ARG', '%s must be a positive scalar', name);
    end
    v = val;
end

function local_norm_args(mu, sigma)
    if ~isscalar(mu) || ~isfinite(mu)
        error('GATE:MISSION_V3:DAG:SAMPLING', 'mu must be finite scalar');
    end
    if ~isscalar(sigma) || ~isfinite(sigma) || sigma <= 0
        error('GATE:MISSION_V3:DAG:SAMPLING', 'sigma must be finite > 0');
    end
end

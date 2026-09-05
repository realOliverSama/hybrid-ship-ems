function out = env_realize(mode, varargin)
% mission_v3.env_realize - T-009 environment truth realization (DGN-04, R1/R5)
%
%   out = mission_v3.env_realize('realize', dag, t_end_s, ...)
%   out = mission_v3.env_realize('sample', formalSeed, baseDrawStart, ...
%           templateId, regimeId, stratumId, ...)
%
% 'realize': dag is a mission_v3.dag_sample('draw', ...) struct; t_end_s is
%   numeric seconds or 'auto' (template window sum + 60 s margin, the
%   margin taken verbatim from delegation spec Required Work B).
% 'sample': retry loop (fixed decision 8) over base_draw_index increments;
%   each attempt re-draws via dag_sample and realizes.  Shared name-values:
%     'ParamOverride'   test-only env-params override (see dag_sample)
%   'sample' name-values:
%     'TEndS'           'auto' (default) or numeric seconds
%     'DraftM'          forwarded to dag_sample (default 2.188, Model E
%                       Hull_T_m, fixed decision 7)
%     'MaxAttempts'     retry budget, default 200 (fixed decision 8)
%     'ExhaustAction'   'error' (default): no accepted draw within the
%                       budget raises GATE:MISSION_V3:ENV:MAX_RETRY;
%                       'return_ledger': return the full all-rejected
%                       ledger instead (audit/testing path that keeps the
%                       forbidden-to-drop rejected draws inspectable)
%
% OU realization (fixed decision 4), exact discretization on the frozen
% 1 s ZOH grid:
%   X[k+1] = X[k]*phi + sigma*sqrt(1-phi^2)*Z[k],  phi = exp(-dt/tau)
% starting X(t = -prehistory_s) = 0 (R5 prehistory, read from the frozen
% forecast quality profiles; must be >= 2*tau).  Z values come from the
% current_truth substream AFTER fast-forwarding the dag.current_truth_ff_n
% normal draws consumed by dag_sample's truncated-mean loop: the whole
% voyage shares ONE continuous per-draw stream.  U_river(t) = u_river_mean
% + X(t) in river coordinates (downstream positive), grid
% -prehistory_s : dt : t_end.
%
% R1 sign mapping (fixed decision 5): one U_river(t) per voyage; the
% per-segment along-track relative component is along_sign * U_river(t)
% with upstream (outbound) -1, downstream (return) +1, berth/standby 0.
% Reversing heading never re-draws the environment.
%
% Judgment order per candidate (fixed decision 8):
%   (1) HT <= ht domain floor   -> REJECTED_SAMPLING_ELIGIBILITY /
%                                  PHYSICAL_DOMAIN_BOUND
%   (2) HT < ht iid support min -> REJECTED_SAMPLING_ELIGIBILITY /
%                                  SUPPORT_BOUND
%   (3) sup|U_river(t)| over the whole realized grid (prehistory included;
%       v1 conservative whole-voyage sup; segment-level refinement belongs
%       to T-010/T-011) against the strictest axis bound of the voyage
%       (following bound when a return segment exists, else adverse bound)
%                               -> REJECTED_SAMPLING_ELIGIBILITY /
%                                  SUPPORT_BOUND
%   (4) structural heldout tokens (IID EXCLUDE; evaluated in frozen
%       registry order C1, C2, C3 on generation-side metadata only)
%                               -> REJECTED_STRUCTURAL_HELDOUT(token_id)
%   (5) otherwise ACCEPT.
%
% Every rejected candidate is recorded in the result ledger in full (draw
% + disposition + evidence; no draw is dropped, FR-537/DGN-10 audit); the
% accept record carries the attempt count.  The disposition struct carries
% the full six-class taxonomy; FAILED_CONTRACT / FAILED_GENERATION /
% FAILED_PHYSICAL_DOMAIN are structural placeholders at this layer (later
% tasks trigger them).
%
% Token evaluation is registry-driven: for each frozen token, clause 1
% must be the segment-role equality and clause 2 selects the numeric
% factor ('segment.u_adverse_abs' against the realized per-segment adverse
% sup, or 'voyage.ht_effective' against the voyage scalar); band bounds
% and inclusivity flags are read from the registry support_band.  Any
% other predicate field fails closed (GATE:MISSION_V3:ENV:PREDICATE_FIELD).
%
% Determinism: pure function of (dag / formalSeed + draw range, frozen
% registries, name-values); identical inputs give bitwise-identical
% output.  This function writes nothing to disk.
%
% Error ids: GATE:MISSION_V3:ENV:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% no global RNG; ROOT resolved from mfilename with pwd fallback.

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:ENV:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'realize'
            if nargin < 3
                error('GATE:MISSION_V3:ENV:ARG', ...
                    '''realize'' expects: dag, t_end_s [, name-values]');
            end
            dag = varargin{1};
            t_end_in = varargin{2};
            opts = local_parse_named(varargin(3:end), false);
            out = local_realize(dag, t_end_in, opts.ov);
        case 'sample'
            if nargin < 6
                error('GATE:MISSION_V3:ENV:ARG', ...
                    ['''sample'' expects: formalSeed, baseDrawStart, ', ...
                     'templateId, regimeId, stratumId [, name-values]']);
            end
            formal_seed = varargin{1};
            draw_start = varargin{2};
            template_id = varargin{3};
            regime_id = varargin{4};
            stratum_id = varargin{5};
            opts = local_parse_named(varargin(6:end), true);
            out = local_sample(formal_seed, draw_start, template_id, ...
                regime_id, stratum_id, opts);
        otherwise
            error('GATE:MISSION_V3:ENV:ARG', 'unknown mode: %s', char(mode));
    end
end

%% ========================== sample loop ============================

function out = local_sample(formal_seed, draw_start, template_id, regime_id, ...
        stratum_id, opts)
    if ~isnumeric(draw_start) || ~isscalar(draw_start) || ~isfinite(draw_start) ...
            || draw_start < 0 || draw_start ~= floor(draw_start)
        error('GATE:MISSION_V3:ENV:ARG', ...
            'baseDrawStart must be a whole number >= 0');
    end
    n_max = opts.max_attempts;
    blank_row = struct('attempt', 0, 'base_draw_index', 0, ...
        'disposition', {local_blank_disposition()}, 'draw', {struct()});
    ledger = repmat(blank_row, 1, n_max);
    accepted = false;
    att = 0;
    rl = [];
    for att = 1:n_max
        bdi = draw_start + (att - 1);
        dag = mission_v3.dag_sample('draw', formal_seed, bdi, template_id, ...
            regime_id, stratum_id, 'DraftM', opts.draft, ...
            'ParamOverride', opts.ov);
        rl = local_realize(dag, opts.t_end, opts.ov);
        row = blank_row;
        row.attempt = att;
        row.base_draw_index = bdi;
        row.disposition = rl.disposition;
        row.draw = dag;
        ledger(att) = row;
        if strcmp(rl.disposition.disposition, 'ACCEPT')
            accepted = true;
            break;
        end
    end
    out = struct();
    out.kind = 'sample';
    out.formal_seed = formal_seed;
    out.base_draw_start = draw_start;
    out.template_id = char(template_id);
    out.regime_id = char(regime_id);
    out.stratum_id = char(stratum_id);
    out.disposition_enum = local_disposition_enum();
    out.reason_enum = local_reason_enum();
    if accepted
        out.accepted = true;
        out.n_attempts = att;
        out.realization = rl;
        out.ledger = ledger(1:att);
    elseif strcmp(opts.exhaust, 'return_ledger')
        out.accepted = false;
        out.n_attempts = n_max;
        out.realization = [];
        out.ledger = ledger;
    else
        error('GATE:MISSION_V3:ENV:MAX_RETRY', ...
            'no accepted environment realization within %d attempts (stratum %s)', ...
            n_max, char(stratum_id));
    end
end

%% ========================== realization ============================

function out = local_realize(dag, t_end_in, ov)
    if ~isstruct(dag) || ~isfield(dag, 'kind') || ~strcmp(dag.kind, 'dag_draw')
        error('GATE:MISSION_V3:ENV:ARG', ...
            'dag must be a mission_v3.dag_sample ''draw'' struct');
    end
    need_f = {'formal_seed', 'base_draw_index', 'stratum_id', 'template_id', ...
        'regime_id', 'operator_profile', 'u_river_mean', 'current_truth_ff_n', ...
        'h_chart', 'delta_wl', 'draft_m', 'ht_effective', 'segments', ...
        'traffic', 'lineage'};
    for j = 1:numel(need_f)
        if ~isfield(dag, need_f{j})
            error('GATE:MISSION_V3:ENV:ARG', 'dag lacks field: %s', need_f{j});
        end
    end
    ht_re = (dag.h_chart + dag.delta_wl) / dag.draft_m;
    if abs(ht_re - dag.ht_effective) > 1e-9
        error('GATE:MISSION_V3:ENV:DAG_INCONSISTENT', ...
            'dag ht_effective disagrees with h_chart/delta_wl/draft_m');
    end

    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    reg = local_load_registries(ROOT);
    env = local_apply_override(reg.env, ov);
    prof = local_read_json(reg.cfg_dir, 'forecast_quality_profiles.json');

    inj = env.ou_process.injection;
    if ~isfield(inj, 'grid_s') || ~isnumeric(inj.grid_s) || isempty(inj.grid_s) ...
            || inj.grid_s <= 0
        error('GATE:MISSION_V3:ENV:GRID', 'injection.grid_s must be numeric > 0');
    end
    grid_dt = inj.grid_s;
    if ~isfield(prof, 'prehistory_s') || ~isnumeric(prof.prehistory_s) ...
            || isempty(prof.prehistory_s)
        error('GATE:MISSION_V3:ENV:PREHISTORY', ...
            'forecast profiles lack numeric prehistory_s');
    end
    t_pre = prof.prehistory_s;

    env_r = local_regime_entry(env, dag.regime_id);
    du = env_r.delta_u;
    tau = du.tau_s;
    sig = du.sigma_mps;
    if ~isnumeric(tau) || ~isscalar(tau) || ~isfinite(tau) || tau <= 0
        error('GATE:MISSION_V3:ENV:PARAM', 'delta_u.tau_s must be numeric > 0');
    end
    if ~isnumeric(sig) || ~isscalar(sig) || ~isfinite(sig) || sig <= 0
        error('GATE:MISSION_V3:ENV:PARAM', 'delta_u.sigma_mps must be numeric > 0');
    end
    if t_pre < 2 * tau
        error('GATE:MISSION_V3:ENV:PREHISTORY', ...
            'prehistory %g s < 2*tau %g s (R5/DGN-06: >= 1800 s = 2*tau)', ...
            t_pre, 2 * tau);
    end

    nseg = numel(dag.segments);
    if nseg < 1
        error('GATE:MISSION_V3:ENV:ARG', 'dag has no segments');
    end
    win = [dag.segments.window_s];

    % ---- t_end resolution ('auto' = window sum + spec margin) ----
    if ischar(t_end_in) || isstring(t_end_in)
        if ~strcmpi(char(t_end_in), 'auto')
            error('GATE:MISSION_V3:ENV:ARG', ...
                't_end_s must be numeric or ''auto''');
        end
        t_end_src = 'auto';
        t_end = sum(win) + 60.0;   % 60 s margin: delegation spec Required Work B
    else
        if ~isnumeric(t_end_in) || ~isscalar(t_end_in) || ~isfinite(t_end_in) ...
                || t_end_in < 0
            error('GATE:MISSION_V3:ENV:ARG', ...
                't_end_s must be numeric >= 0 or ''auto''');
        end
        t_end_src = 'given';
        t_end = t_end_in;
    end

    % ---- OU continuation of the single current_truth substream ----
    cur = local_branch(dag.lineage, 'current_truth');
    rs = RandStream('Threefry4x64', 'Seed', cur.engine_seed_uint32);
    ff = dag.current_truth_ff_n;
    if ff > 0
        z_ff = randn(rs, [1, ff]);   % fast-forward (same draws dag_sample used)
        %#ok<NASGU> z_ff
    end
    i0 = round(t_pre / grid_dt);          % t = 0 sits at linear index i0 + 1
    npts = i0 + floor(t_end / grid_dt) + 1;
    if npts < i0 + 1
        error('GATE:MISSION_V3:ENV:GRID', 't_end below grid resolution');
    end
    t_end_eff = (npts - 1 - i0) * grid_dt;
    phi = exp(-grid_dt / tau);
    g = sig * sqrt(1 - phi^2);
    z = randn(rs, [1, npts - 1]);
    x = zeros(1, npts);
    for kk = 1:npts - 1
        x(kk + 1) = x(kk) * phi + g * z(kk);
    end
    u_river = dag.u_river_mean + x;
    t_grid = (-t_pre) + (0:npts - 1) * grid_dt;

    % ---- per-segment windows and R1 sign-mapped summaries ----
    t_start = zeros(1, nseg);
    acc_t = 0.0;
    for i = 1:nseg
        t_start(i) = acc_t;
        acc_t = acc_t + win(i);
    end
    blank_rseg = struct('index', 0, 'role', '', 'reach', '', 'direction', '', ...
        'along_sign', 0, 'stretch', 0.0, 'planned_speed_mps', [], ...
        'window_s', 0.0, 't_start_nominal_s', 0.0, 'idx_start', 0, ...
        'idx_end', 0, 't_start_s', NaN, 't_end_s', NaN, 'u_abs_sup', NaN, ...
        'u_signed_min', NaN, 'u_signed_max', NaN, 'u_signed_mean', NaN, ...
        'u_adverse_abs_sup', NaN, 'u_following_abs_sup', NaN, 'on_grid', false);
    seg_out = repmat(blank_rseg, 1, nseg);
    for i = 1:nseg
        sg_i = dag.segments(i);
        row = blank_rseg;
        row.index = i;
        row.role = sg_i.role;
        row.reach = sg_i.reach;
        row.direction = sg_i.direction;
        row.along_sign = sg_i.along_sign;
        row.stretch = sg_i.stretch;
        row.planned_speed_mps = sg_i.planned_speed_mps;
        row.window_s = sg_i.window_s;
        row.t_start_nominal_s = t_start(i);
        i1 = i0 + round(t_start(i) / grid_dt) + 1;
        i2 = i0 + round((t_start(i) + sg_i.window_s) / grid_dt) + 1;
        i1 = max(i1, 1);
        i2 = min(i2, npts);
        if i2 >= i1
            w_seg = u_river(i1:i2);
            along = sg_i.along_sign * w_seg;
            row.idx_start = i1;
            row.idx_end = i2;
            row.t_start_s = t_grid(i1);
            row.t_end_s = t_grid(i2);
            row.u_abs_sup = max(abs(w_seg));
            row.u_signed_min = min(along);
            row.u_signed_max = max(along);
            row.u_signed_mean = mean(along);
            row.u_adverse_abs_sup = max(0.0, -min(along));
            row.u_following_abs_sup = max(0.0, max(along));
            row.on_grid = true;
        end
        seg_out(i) = row;
    end

    % ---- traffic events with absolute and segment-relative times ----
    total_ev = 0;
    for i = 1:nseg
        trf_i = dag.traffic(i);
        total_ev = total_ev + trf_i.count;
    end
    blank_ev_abs = struct('segment_index', 0, 'role', '', 'reach', '', ...
        't_start_rel_s', 0.0, 't_start_abs_s', 0.0, 'duration_s', 0.0, ...
        'speed_ratio', 0.0);
    traffic_abs = repmat(blank_ev_abs, 1, total_ev);
    w_ev = 0;
    for i = 1:nseg
        trf_i = dag.traffic(i);
        evs = trf_i.events;
        for e = 1:numel(evs)
            ev = evs(e);
            w_ev = w_ev + 1;
            e_row = blank_ev_abs;
            e_row.segment_index = i;
            e_row.role = dag.segments(i).role;
            e_row.reach = dag.segments(i).reach;
            e_row.t_start_rel_s = ev.start_frac * dag.segments(i).window_s;
            e_row.t_start_abs_s = t_start(i) + e_row.t_start_rel_s;
            e_row.duration_s = ev.duration_s;
            e_row.speed_ratio = ev.speed_ratio;
            traffic_abs(w_ev) = e_row;
        end
    end

    % ---- support-bound evidence ----
    bnd = reg.bounds.axes;
    has_return = false;
    for i = 1:nseg
        if strcmp(dag.segments(i).role, 'return')
            has_return = true;
        end
    end
    if has_return
        ax_flow = bnd.following_current;
        axis_name = 'following_current';
    else
        ax_flow = bnd.adverse_current;
        axis_name = 'adverse_current';
    end
    iid = ax_flow.iid_support;
    flow_bound = iid.max;
    sup_flow = max(abs(u_river));   % whole grid incl. prehistory (v1 sup)
    ht_ax = bnd.ht_ratio;
    ht_floor = ht_ax.domain_floor;
    ht_min = ht_ax.iid_support.min;

    % ---- structural heldout token evaluation (registry-driven) ----
    toks = local_as_list(reg.tokens.tokens, 'tokens');
    blank_tok = struct('token_id', '', 'triggered', false, ...
        'trigger_segment_index', 0, 'trigger_segment_role', '');
    tok_out = repmat(blank_tok, 1, numel(toks));
    trig_id = '';
    trig_seg = 0;
    trig_role = '';
    for j = 1:numel(toks)
        tk = toks{j};
        pred = local_as_list(tk.predicate, 'predicate');
        if numel(pred) ~= 2
            error('GATE:MISSION_V3:ENV:PREDICATE', ...
                'token %s: expected two predicate clauses', tk.token_id);
        end
        p1 = pred{1};
        p2 = pred{2};
        if ~strcmp(p1.field, 'segment.role') || ~strcmp(p1.operator, '==')
            error('GATE:MISSION_V3:ENV:PREDICATE', ...
                'token %s: first clause must be the segment-role equality', ...
                tk.token_id);
        end
        role_v = p1.value;
        band = tk.support_band;
        use_per_seg = false;
        switch p2.field
            case 'segment.u_adverse_abs'
                use_per_seg = true;
            case 'voyage.ht_effective'
                use_per_seg = false;
            otherwise
                error('GATE:MISSION_V3:ENV:PREDICATE_FIELD', ...
                    'token %s predicate field not implemented (whitelist guard): %s', ...
                    tk.token_id, p2.field);
        end
        t_row = blank_tok;
        t_row.token_id = tk.token_id;
        if use_per_seg
            for i = 1:nseg
                if strcmp(dag.segments(i).role, role_v) ...
                        && local_in_band(seg_out(i).u_adverse_abs_sup, band)
                    t_row.triggered = true;
                    t_row.trigger_segment_index = i;
                    t_row.trigger_segment_role = role_v;
                    break;
                end
            end
        else
            if local_in_band(ht_re, band)
                for i = 1:nseg
                    if strcmp(dag.segments(i).role, role_v)
                        t_row.triggered = true;
                        t_row.trigger_segment_index = i;
                        t_row.trigger_segment_role = role_v;
                        break;
                    end
                end
            end
        end
        tok_out(j) = t_row;
        if t_row.triggered && isempty(trig_id)
            trig_id = tk.token_id;
            trig_seg = t_row.trigger_segment_index;
            trig_role = t_row.trigger_segment_role;
        end
    end

    % ---- disposition (fixed decision 8 judgment order 1-5) ----
    disp = local_blank_disposition();
    ev = disp.evidence;
    ev.ht_effective = ht_re;
    ev.ht_domain_floor = ht_floor;
    ev.ht_support_min = ht_min;
    ev.flow_sup_abs = sup_flow;
    ev.flow_axis = axis_name;
    ev.flow_bound = flow_bound;
    ev.has_return_segment = has_return;
    if ht_re <= ht_floor
        disp.disposition = 'REJECTED_SAMPLING_ELIGIBILITY';
        disp.reason = 'PHYSICAL_DOMAIN_BOUND';
    elseif ht_re < ht_min
        disp.disposition = 'REJECTED_SAMPLING_ELIGIBILITY';
        disp.reason = 'SUPPORT_BOUND';
    elseif sup_flow > flow_bound
        disp.disposition = 'REJECTED_SAMPLING_ELIGIBILITY';
        disp.reason = 'SUPPORT_BOUND';
    elseif ~isempty(trig_id)
        disp.disposition = 'REJECTED_STRUCTURAL_HELDOUT';
        disp.reason = 'none';
        disp.token_id = trig_id;
        ev.trigger_segment_index = trig_seg;
        ev.trigger_segment_role = trig_role;
    else
        disp.disposition = 'ACCEPT';
        disp.reason = 'none';
    end
    disp.evidence = ev;
    disp.base_draw_index = dag.base_draw_index;
    disp.stratum_id = dag.stratum_id;
    ls = disp.lineage_summary;
    ls.formal_seed = dag.formal_seed;
    ls.branch_paths = local_paths(dag.lineage);
    ls.current_truth_lineage_id = cur.lineage_id;
    disp.lineage_summary = ls;
    if ~ismember(disp.disposition, local_disposition_enum()) ...
            || ~ismember(disp.reason, local_reason_enum())
        error('GATE:MISSION_V3:ENV:TAXONOMY', ...
            'disposition/reason outside the frozen six-class taxonomy');
    end

    % ---- assemble ----
    out = struct();
    out.kind = 'realize';
    out.formal_seed = dag.formal_seed;
    out.base_draw_index = dag.base_draw_index;
    out.stratum_id = dag.stratum_id;
    out.template_id = dag.template_id;
    out.regime_id = dag.regime_id;
    out.operator_profile = dag.operator_profile;
    out.t_grid_s = t_grid;
    out.u_river_mps = u_river;
    out.x_ou_mps = x;
    out.u_river_mean = dag.u_river_mean;
    ou = struct();
    ou.tau_s = tau;
    ou.sigma_mps = sig;
    ou.phi = phi;
    ou.gain = g;
    ou.dt_s = grid_dt;
    ou.prehistory_s = t_pre;
    ou.n_points = npts;
    ou.ff_draws = ff;
    ou.t_end_s = t_end_eff;
    ou.t_end_source = t_end_src;
    ou.current_truth_lineage_id = cur.lineage_id;
    ou.continuation_note = ['single current_truth substream, fast-forwarded ', ...
        'by ff_draws normals, then the OU increments (R1: one realization ', ...
        'per voyage, zero redraw at heading reversal)'];
    out.ou = ou;
    out.segments = seg_out;
    out.traffic = traffic_abs;
    water = struct();
    water.h_chart_m = dag.h_chart;
    water.delta_wl_m = dag.delta_wl;
    water.h_effective_m = dag.h_chart + dag.delta_wl;
    water.draft_m = dag.draft_m;
    water.ht_effective = ht_re;
    out.water = water;
    bse = struct();
    bse.flow_axis = axis_name;
    bse.flow_bound = flow_bound;
    bse.flow_sup_abs = sup_flow;
    bse.flow_within = sup_flow <= flow_bound;
    bse.ht_domain_floor = ht_floor;
    bse.ht_support_min = ht_min;
    bse.ht_within = ht_re >= ht_min;
    out.bounds = bse;
    out.tokens = tok_out;
    out.disposition = disp;
    out.disposition_enum = local_disposition_enum();
    out.reason_enum = local_reason_enum();
end

%% ======================== local functions ==========================

function opts = local_parse_named(arg_cell, sample_mode)
    opts = struct('ov', struct(), 't_end', 'auto', 'draft', 2.188, ...
        'max_attempts', 200, 'exhaust', 'error');
    k = 1;
    while k <= numel(arg_cell)
        if (k + 1) > numel(arg_cell)
            error('GATE:MISSION_V3:ENV:ARG', ...
                'missing value for option %s', string(arg_cell{k}));
        end
        opt = arg_cell{k};
        val = arg_cell{k + 1};
        if strcmpi(opt, 'ParamOverride')
            if ~isstruct(val)
                error('GATE:MISSION_V3:ENV:ARG', 'ParamOverride must be a struct');
            end
            opts.ov = val;
        elseif sample_mode && strcmpi(opt, 'TEndS')
            opts.t_end = val;
        elseif sample_mode && strcmpi(opt, 'DraftM')
            if ~isnumeric(val) || ~isscalar(val) || ~isfinite(val) || val <= 0
                error('GATE:MISSION_V3:ENV:ARG', 'DraftM must be a positive scalar');
            end
            opts.draft = val;
        elseif sample_mode && strcmpi(opt, 'MaxAttempts')
            if ~isnumeric(val) || ~isscalar(val) || ~isfinite(val) ...
                    || val < 1 || val ~= floor(val)
                error('GATE:MISSION_V3:ENV:ARG', ...
                    'MaxAttempts must be a positive integer');
            end
            opts.max_attempts = val;
        elseif sample_mode && strcmpi(opt, 'ExhaustAction')
            if ~ischar(val) && ~isstring(val)
                error('GATE:MISSION_V3:ENV:ARG', 'ExhaustAction must be a string');
            end
            if ~ismember(char(val), {'error', 'return_ledger'})
                error('GATE:MISSION_V3:ENV:ARG', ...
                    'ExhaustAction must be ''error'' or ''return_ledger''');
            end
            opts.exhaust = char(val);
        else
            error('GATE:MISSION_V3:ENV:ARG', ...
                'unknown or mode-incompatible option: %s', string(opt));
        end
        k = k + 2;
    end
end

function ok = local_in_band(v, band)
    if ~isnumeric(v) || ~isscalar(v) || isnan(v)
        ok = false;
        return;
    end
    ok_min = (v > band.min) || (band.min_inclusive && v == band.min);
    ok_max = (v < band.max) || (band.max_inclusive && v == band.max);
    ok = ok_min && ok_max;
end

function d = local_blank_disposition()
% Six-class failure taxonomy (DGN-10; delegation spec fixed decision 8).
% FAILED_CONTRACT / FAILED_GENERATION / FAILED_PHYSICAL_DOMAIN are
% structural placeholders at this layer.
    ev = struct('ht_effective', NaN, 'ht_domain_floor', NaN, ...
        'ht_support_min', NaN, 'flow_sup_abs', NaN, 'flow_axis', '', ...
        'flow_bound', NaN, 'has_return_segment', false, ...
        'trigger_segment_index', 0, 'trigger_segment_role', '');
    ls = struct('formal_seed', '', 'branch_paths', {cell(1, 0)}, ...
        'current_truth_lineage_id', '');
    d = struct('disposition', '', 'reason', '', 'token_id', '', ...
        'base_draw_index', 0, 'stratum_id', '', 'lineage_summary', {ls}, ...
        'evidence', {ev});
end

function e = local_disposition_enum()
    e = {'ACCEPT', 'REJECTED_SAMPLING_ELIGIBILITY', ...
        'REJECTED_STRUCTURAL_HELDOUT', 'FAILED_CONTRACT', ...
        'FAILED_GENERATION', 'FAILED_PHYSICAL_DOMAIN'};
end

function e = local_reason_enum()
    e = {'SUPPORT_BOUND', 'PHYSICAL_DOMAIN_BOUND', 'DURATION_BELOW_MIN', 'none'};
end

function reg = local_load_registries(ROOT)
% Load the frozen consumption registries + the effective phase-1 freeze
% (base snapshot + D6 amendment) with a fail-closed binding check.
    cfg = fullfile(ROOT, 'config', 'mission_v3');
    res = fullfile(ROOT, 'results', 'mission_v3');
    reg = struct();
    reg.cfg_dir = cfg;
    reg.env = local_read_json(cfg, 'environment_regime_params.json');
    reg.bounds = local_read_json(cfg, 'id_support_bounds.json');
    reg.tokens = local_read_json(cfg, 'heldout_token_registry.json');
    reg.freeze = local_read_json(res, 'phase1_freeze.json');
    reg.d6 = local_read_json(res, 'phase1_freeze_amendment_d6.json');
    am = reg.d6.amends;
    if ~isstruct(am) || ~isfield(am, 'self_hash')
        error('GATE:MISSION_V3:ENV:FREEZE_MISMATCH', ...
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
        error('GATE:MISSION_V3:ENV:FREEZE_MISMATCH', ...
            'phase1_freeze_amendment_d6 amends a different base freeze');
    end
end

function obj = local_read_json(dirpath, fname)
% jsondecode is the sanctioned consumption parser here (seed_tree.m
% precedent): the frozen files are validator-clean (strict parser +
% self_hash cross-check in mission_v3.validate_registries).
    ppath = fullfile(dirpath, fname);
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:ENV:REGISTRY', 'frozen file not found: %s', fname);
    end
    try
        obj = jsondecode(fileread(ppath));
    catch dec_err
        error('GATE:MISSION_V3:ENV:REGISTRY', ...
            'jsondecode failed for %s: %s', fname, dec_err.message);
    end
    if ~isstruct(obj)
        error('GATE:MISSION_V3:ENV:REGISTRY', ...
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
            error('GATE:MISSION_V3:ENV:OVERRIDE', ...
                'override of %s is not permitted', key);
        end
        if ~isfield(out, key)
            error('GATE:MISSION_V3:ENV:OVERRIDE', ...
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

function r = local_regime_entry(env, regime_id)
    if ~isfield(env, 'regimes') || ~isfield(env.regimes, regime_id)
        error('GATE:MISSION_V3:ENV:REGIME', ...
            'regime not in frozen env params: %s', regime_id);
    end
    r = env.regimes.(regime_id);
    if ~isfield(r, 'delta_u')
        error('GATE:MISSION_V3:ENV:REGIME', ...
            'regime %s lacks delta_u', regime_id);
    end
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
    error('GATE:MISSION_V3:ENV:SEED', 'lineage lacks branch: %s', branch_name);
end

function paths = local_paths(lin)
    paths = cell(1, numel(lin));
    for k = 1:numel(lin)
        row = lin(k);
        paths{k} = row.branch_path;
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
        error('GATE:MISSION_V3:ENV:REGISTRY', 'expected JSON array for %s', what);
    end
end

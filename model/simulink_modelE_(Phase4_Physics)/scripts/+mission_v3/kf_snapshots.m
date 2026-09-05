function kf = kf_snapshots(mode, varargin)
% mission_v3.kf_snapshots - T-013 K/F issue-major snapshot builder (DGN-08)
%
%   kf = mission_v3.kf_snapshots('build', plan, t0_s, Fblk, ...
%           'HChartM', h_chart [, name-values])
%   kf = mission_v3.kf_snapshots('build_grid', plan, t0_vec, Fblk_all, ...
%           'HChartM', h_chart [, name-values])
%
% K/F INPUT_K/INPUT_F issue-major snapshots built by PLANNED forward
% integration of the T-010 plan (FR-558~562, tasks Gate R7).
%
% CAUSAL API BOUNDARY (fixed decision 2, R7): the positional signature is
% exactly (plan, t0_s, Fblk) plus name-values.  There is NO
% realization/stream/u_river positional argument: the realized future
% cannot enter this function, so mutating the realized future can never
% perturb the K/F snapshot (Delta-actual_future does not imply
% Delta-s_plan).  Any extra positional argument raises
% GATE:MISSION_V3:KF:BAD_ARG.  NOTE: a high statistical correlation
% between the plan and the realized execution is the EXPECTED signature of
% faithful plan execution and is never by itself evidence of leakage;
% causality here is guaranteed structurally by the interface boundary.
%
% MODES (fixed decision 7):
%   'build'      one issue; plan is a mission_v3.plan_compose('compose')
%                product, t0_s is a scalar issue time, Fblk is one
%                mission_v3.forecast_issue('issue', ...) output issued at
%                the same t0_s on the frozen lead grid.  Returns the M=1
%                issue-major layout (1x30 lead columns).
%   'build_grid' t0_vec is an (M x 1) issue-time vector (caller strategy,
%                e.g. 60:60:1800); Fblk_all is a struct array with
%                numel(Fblk_all) == numel(t0_vec), one forecast_issue
%                output per issue, each issued at its own t0 (per-issue
%                causal slices).  Loops 'build' per issue - the per-issue
%                independent-call semantics are preserved, so causality
%                holds issue by issue.
%
% NAME-VALUES:
%   'HChartM'        required finite positive scalar: voyage chart depth.
%                    Static INPUT_K (HANDOFF: h_chart goes to K, the
%                    DeltaWL forecast goes to F, the actual depth stays
%                    ORACLE).  Never a realized future array.
%   'InitialXPlanM'  default 0: planned along-track position at the issue
%                    time (fixed decision 2).  The v1 plan origin is s=0,
%                    so the issue position may equivalently be given by
%                    the plan-forward integral from 0; the caller choice
%                    is recorded in provenance.initial_x_plan_source.
%
% CONVENTIONS (fixed decision 3, export_pilot time-mode precedent):
%   s_plan[k] = x(t0) + sum_{j=1..10k} v_plan(t0 + j s) * 1 s
%   (1 s substeps, right-endpoint cumulative sum; 10 substeps per 10 s
%   lead bin).  v_plan[k] = plan table value at the bin right endpoint
%   t0 + lead_k.  The lead grid is strictly 10:10:300 (30 points, FR-561);
%   the grid is a named constant and is never taken from the inputs.
%
% INPUT_K (fixed decision 4, FR-560 interval-aware sampling): position
% dependent constraints are sampled along the PLANNED trajectory at the
% bin right-endpoint time (never along realized future positions):
%   h_chart_m         voyage scalar (name-value 'HChartM')
%   in_speed_limit_zone / in_bridge_zone
%                     membership flags from plan.speed_limit_zones /
%                     plan.bridge_zones ONLY (the plan is the only plan
%                     fact source; zones are never re-fetched from a
%                     realization); half-open windows
%                     [t_start_s, t_end_s) at t = t0 + lead_k.
% P_hotel_plan_kW (DGN-09 Path B): the plan component
% P_hotel_base(template), read from the frozen
% config/mission_v3/mission_template_registry.json via plan.template_id;
% delta_hotel never enters here (truth/ORACLE side only).
%
% INPUT_F (fixed decision 5): the F columns are assembled verbatim from
% the caller-supplied Fblk (u_fcst_mps, wl_fcst_m); this function never
% recomputes forecasts.  Asserts: the Fblk lead grid equals the frozen
% grid (GATE:MISSION_V3:KF:GRID), the F content shapes are consistent
% with it (GATE:MISSION_V3:KF:FGRID), and the Fblk vintage equals the
% issue time of the row it feeds (GATE:MISSION_V3:KF:VINTAGE).
%
% MISSING SNAPSHOTS (fixed decision 6, FR-562): when t0 + lead_k exceeds
% plan.t_grid_s(end) the whole (issue, lead) entry is missing: all six
% content columns are NaN, missing_mask is true, the zone flags are false,
% and the row contract_status is 'FAILED_CONTRACT'.  No extrapolation, no
% zero fill, no reconstruction from future truth.
%
% LAYOUT (fixed decision 1, export_pilot-aligned, issue-major):
%   kf.issue_time_s       (M x 1)
%   kf.lead_s             (M x 30), every row 10:10:300
%   kf.v_plan_mps         (M x 30)   planned SOG at the bin right endpoint
%   kf.s_plan_m           (M x 30)   planned position (forward integral)
%   kf.h_chart_m          (M x 30)   chart depth (voyage scalar)
%   kf.P_hotel_plan_kW    (M x 30)   hotel plan component (DGN-09)
%   kf.u_fcst_mps         (M x 30)   INPUT_F pass-through
%   kf.wl_fcst_m          (M x 30)   INPUT_F pass-through
%   kf.in_speed_limit_zone (M x 30 logical)
%   kf.in_bridge_zone      (M x 30 logical)
%   kf.missing_mask        (M x 30 logical)
%   kf.contract_status     (M x 1 cellstr, 'OK' | 'FAILED_CONTRACT')
%   kf.provenance          struct (generator, conventions, sources)
%
% GATE ERROR IDS: GATE:MISSION_V3:KF:{BAD_ARG, GRID, FGRID, VINTAGE,
% REGISTRY}.
%
% Determinism: pure function of (plan, issue times, Fblk, name-values,
% frozen registry); identical inputs give bitwise-identical output.  No
% RNG, no file writes (the frozen registry is read only).
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% ROOT resolved from mfilename with pwd fallback; thin main + local
% functions (style ref: +mission_v3/plan_compose.m, forecast_issue.m).

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:KF:BAD_ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'build'
            if nargin < 4
                error('GATE:MISSION_V3:KF:BAD_ARG', ...
                    ['''build'' expects: plan, t0_s, Fblk [, ', ...
                     'name-values]']);
            end
            kf = local_assemble(varargin{1}, varargin{2}, varargin{3}, ...
                local_parse_named(varargin(4:end)));
        case 'build_grid'
            if nargin < 4
                error('GATE:MISSION_V3:KF:BAD_ARG', ...
                    ['''build_grid'' expects: plan, t0_vec, Fblk_all [, ', ...
                     'name-values]']);
            end
            kf = local_assemble(varargin{1}, varargin{2}, varargin{3}, ...
                local_parse_named(varargin(4:end)));
        otherwise
            error('GATE:MISSION_V3:KF:BAD_ARG', ...
                'unknown mode: %s (kf_snapshots implements ''build'' and ''build_grid'')', ...
                char(mode));
    end
end

%% ============================ assembly ==============================

function kf = local_assemble(plan, t0_in, fblk_in, opts)
% Validate once, then loop the per-issue 'build' semantics (fixed
% decision 7) and assemble the issue-major (M x 30) layout.
    local_check_plan(plan);
    t0_vec = local_check_t0_vec(t0_in);
    n_issue = numel(t0_vec);
    if ~isstruct(fblk_in) || (numel(fblk_in) ~= n_issue)
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            ['Fblk must be a struct array with one forecast_issue ', ...
             '''issue'' output per issue time (numel(Fblk) == ', ...
             'numel(t0_vec)); the signature admits exactly (plan, ', ...
             't0_s/t0_vec, Fblk) plus name-values']);
    end

    root = local_root();
    hb = local_hotel_base(root, plan.template_id);

    leads = KF_CONST_LEAD_GRID();
    n_lead = numel(leads);

    kf = struct();
    kf.issue_time_s = t0_vec;
    kf.lead_s = repmat(leads, n_issue, 1);
    kf.v_plan_mps = zeros(n_issue, n_lead);
    kf.s_plan_m = zeros(n_issue, n_lead);
    kf.h_chart_m = zeros(n_issue, n_lead);
    kf.P_hotel_plan_kW = zeros(n_issue, n_lead);
    kf.u_fcst_mps = zeros(n_issue, n_lead);
    kf.wl_fcst_m = zeros(n_issue, n_lead);
    kf.in_speed_limit_zone = false(n_issue, n_lead);
    kf.in_bridge_zone = false(n_issue, n_lead);
    kf.missing_mask = false(n_issue, n_lead);
    kf.contract_status = cell(n_issue, 1);

    prof_ids = cell(n_issue, 1);
    for m = 1:n_issue
        t0_m = t0_vec(m);
        fb = fblk_in(m);
        fbc = local_check_fblk(fb, t0_m);
        row = local_build_row(plan, t0_m, fbc, leads, opts, hb.value);
        kf.v_plan_mps(m, :) = row.v_plan_mps;
        kf.s_plan_m(m, :) = row.s_plan_m;
        kf.h_chart_m(m, :) = row.h_chart_m;
        kf.P_hotel_plan_kW(m, :) = row.P_hotel_plan_kW;
        kf.u_fcst_mps(m, :) = row.u_fcst_mps;
        kf.wl_fcst_m(m, :) = row.wl_fcst_m;
        kf.in_speed_limit_zone(m, :) = row.in_speed_limit_zone;
        kf.in_bridge_zone(m, :) = row.in_bridge_zone;
        kf.missing_mask(m, :) = row.missing_mask;
        kf.contract_status{m} = row.contract_status;
        prof_ids{m} = fbc.profile_id;
    end
    kf.provenance = local_provenance(n_issue, opts, hb.registry_hash, ...
        prof_ids);
end

function row = local_build_row(plan, t0_s, fbc, leads, opts, p_hotel)
% One issue: planned forward integration + interval-aware INPUT_K/F
% sampling at the bin right endpoints (export_pilot time-mode convention).
    tg = plan.t_grid_s;
    t_end = tg(end);
    n_lead = numel(leads);
    t_k = t0_s + leads;
    good = t_k <= (t_end + KF_CONST_TOL());
    n_good = sum(good);

    v_row = nan(1, n_lead);
    s_row = nan(1, n_lead);
    if n_good > 0
        n_sub = KF_CONST_SUBSTEPS_PER_LEAD() * n_good;
        t_sub = t0_s + KF_CONST_DT_SUB_S() * (1:n_sub);
        v_sub = local_plan_value(plan, t_sub);
        s_steps = opts.initial_x ...
            + cumsum(v_sub * KF_CONST_DT_SUB_S());
        for k = 1:n_good
            j = k * KF_CONST_SUBSTEPS_PER_LEAD();
            v_row(k) = v_sub(j);
            s_row(k) = s_steps(j);
        end
    end

    slz = plan.speed_limit_zones;
    brz = plan.bridge_zones;
    tol = KF_CONST_TOL();
    in_slz = false(1, n_lead);
    in_brg = false(1, n_lead);
    for k = 1:n_good
        tk = t_k(k);
        in_slz(k) = local_in_any_zone(slz, tk, tol);
        in_brg(k) = local_in_any_zone(brz, tk, tol);
    end

    u_row = fbc.fb_u;              % already one value per lead (30 points)
    w_row = repmat(fbc.fb_w, 1, n_lead);       % voyage scalar broadcast
    h_row = repmat(opts.h_chart, 1, n_lead);
    hot_row = repmat(p_hotel, 1, n_lead);
    if n_good < n_lead
        miss = ~good;
        v_row(miss) = NaN;
        s_row(miss) = NaN;
        h_row(miss) = NaN;
        hot_row(miss) = NaN;
        u_row(miss) = NaN;
        w_row(miss) = NaN;
    end

    row = struct();
    row.v_plan_mps = v_row;
    row.s_plan_m = s_row;
    row.h_chart_m = h_row;
    row.P_hotel_plan_kW = hot_row;
    row.u_fcst_mps = u_row;
    row.wl_fcst_m = w_row;
    row.in_speed_limit_zone = in_slz;
    row.in_bridge_zone = in_brg;
    row.missing_mask = ~good;
    if n_good == n_lead
        row.contract_status = 'OK';
    else
        row.contract_status = 'FAILED_CONTRACT';
    end
end

function v = local_plan_value(plan, tq)
% Linear interpolation on the 1 s plan grid.  Callers guarantee
% 0 <= tq <= plan.t_grid_s(end), so no extrapolation ever happens here
% (missing leads are NaN-masked before this path).
    tg = plan.t_grid_s;
    vp = plan.v_plan_mps;
    v = interp1(tg, vp(:), tq(:), 'linear');
    v = v(:).';
    if any(~isfinite(v))
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'plan interpolation produced non-finite values (grid invariant violated)');
    end
end

%% ======================== validation helpers ========================

function local_check_plan(plan)
    if ~isstruct(plan) || ~isscalar(plan)
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'plan must be a scalar struct');
    end
    if ~isfield(plan, 'kind') || ~strcmp(plan.kind, 'mission_v3_plan')
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            ['plan must be a mission_v3.plan_compose ''compose'' ', ...
             'product (kind = ''mission_v3_plan'')']);
    end
    need = {'t_grid_s', 'v_plan_mps', 'template_id', ...
        'speed_limit_zones', 'bridge_zones'};
    for j = 1:numel(need)
        if ~isfield(plan, need{j})
            error('GATE:MISSION_V3:KF:BAD_ARG', ...
                'plan lacks field: %s', need{j});
        end
    end
    tg = plan.t_grid_s;
    if ~isnumeric(tg) || ~isreal(tg) || ~isvector(tg) || (numel(tg) < 2) ...
            || ~all(isfinite(tg))
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'plan.t_grid_s must be a finite numeric vector with >= 2 points');
    end
    tg = double(tg(:)).';
    if abs(tg(1)) > KF_CONST_TOL() ...
            || ~all(abs(diff(tg) - 1.0) <= KF_CONST_TOL())
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            ['plan.t_grid_s must start at 0 and be uniform on the ', ...
             'frozen 1 s injection grid']);
    end
    vp = plan.v_plan_mps;
    if ~isnumeric(vp) || ~isreal(vp) || ~isvector(vp) ...
            || (numel(vp) ~= numel(tg)) || ~all(isfinite(vp)) ...
            || any(vp < 0)
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            ['plan.v_plan_mps must be a finite non-negative vector of ', ...
             'the same length as plan.t_grid_s']);
    end
    tpl = plan.template_id;
    if (~ischar(tpl) && ~isstring(tpl)) || isempty(char(tpl))
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'plan.template_id must be a non-empty string');
    end
    local_check_zones(plan.speed_limit_zones, 'speed_limit_zones');
    local_check_zones(plan.bridge_zones, 'bridge_zones');
end

function local_check_zones(z, name)
% Zones arrive as struct arrays (or empty); only the membership window
% fields are consumed here.
    if isempty(z)
        return;
    end
    if ~isstruct(z)
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'plan.%s must be a struct array or empty', name);
    end
    for i = 1:numel(z)
        zi = z(i);
        if ~isfield(zi, 't_start_s') || ~isfield(zi, 't_end_s') ...
                || ~isnumeric(zi.t_start_s) || ~isscalar(zi.t_start_s) ...
                || ~isnumeric(zi.t_end_s) || ~isscalar(zi.t_end_s) ...
                || ~isfinite(zi.t_start_s) || ~isfinite(zi.t_end_s) ...
                || (zi.t_end_s <= zi.t_start_s)
            error('GATE:MISSION_V3:KF:BAD_ARG', ...
                'plan.%s(%d) lacks valid t_start_s/t_end_s window fields', ...
                name, i);
        end
    end
end

function in_z = local_in_any_zone(z, tk, tol)
% Half-open window membership [t_start_s, t_end_s) at the bin right
% endpoint time (same window semantics as the plan min-composition).
    in_z = false;
    if isempty(z)
        return;
    end
    for i = 1:numel(z)
        zi = z(i);
        if (tk >= zi.t_start_s - tol) && (tk < zi.t_end_s - tol)
            in_z = true;
            return;
        end
    end
end

function t0_vec = local_check_t0_vec(t0_in)
    if ~isnumeric(t0_in) || ~isreal(t0_in) || ~isvector(t0_in) ...
            || isempty(t0_in) || ~all(isfinite(t0_in))
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'issue time(s) must be a non-empty finite real vector');
    end
    t0_vec = double(t0_in(:));
    if any(t0_vec < 0) || any(abs(t0_vec - round(t0_vec)) > KF_CONST_TOL())
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            ['every issue time must be a non-negative integer on the ', ...
             'frozen 1 s plan grid']);
    end
end

function out = local_check_fblk(fb, t0_s)
% Fblk contract (fixed decision 5): a mission_v3.forecast_issue 'issue'
% output; grid/vintage/content consistency is asserted, values are only
% assembled, never recomputed.
    if ~isstruct(fb) || ~isscalar(fb)
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'Fblk must be a scalar forecast_issue ''issue'' output struct');
    end
    need = {'t0_s', 'h_lead_s', 'u_fcst_mps', 'wl_fcst_m', 'provenance'};
    for j = 1:numel(need)
        if ~isfield(fb, need{j})
            error('GATE:MISSION_V3:KF:BAD_ARG', ...
                'Fblk lacks field: %s (forecast_issue ''issue'' output required)', ...
                need{j});
        end
    end
    pv = fb.provenance;
    if ~isstruct(pv) || ~isfield(pv, 'forecast_quality_profile_id') ...
            || (~ischar(pv.forecast_quality_profile_id) ...
                && ~isstring(pv.forecast_quality_profile_id))
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'Fblk.provenance must carry forecast_quality_profile_id');
    end
    ft0 = fb.t0_s;
    if ~isnumeric(ft0) || ~isscalar(ft0) || ~isfinite(ft0)
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'Fblk.t0_s must be a finite scalar');
    end
    if abs(double(ft0) - t0_s) > KF_CONST_TOL()
        error('GATE:MISSION_V3:KF:VINTAGE', ...
            ['Fblk was issued at %.10g s but feeds the row with issue ', ...
             'time %.10g s (vintage mismatch; FR-561 forbids vintage ', ...
             'inversion)'], double(ft0), t0_s);
    end
    hl = fb.h_lead_s;
    if ~isnumeric(hl) || ~isreal(hl) || ~isvector(hl) || isempty(hl) ...
            || ~all(isfinite(hl))
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'Fblk.h_lead_s must be a non-empty finite numeric vector');
    end
    fu = fb.u_fcst_mps;
    if ~isnumeric(fu) || ~isreal(fu) || ~isvector(fu) || isempty(fu) ...
            || ~all(isfinite(fu))
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'Fblk.u_fcst_mps must be a non-empty finite numeric vector');
    end
    % frozen lead-grid invariant first (FR-561), then content consistency
    % with that grid
    leads = KF_CONST_LEAD_GRID();
    hl_row = double(hl(:)).';
    if (numel(hl_row) ~= numel(leads)) ...
            || (max(abs(hl_row - leads)) > KF_CONST_TOL())
        error('GATE:MISSION_V3:KF:GRID', ...
            ['the Fblk lead grid is not the frozen {10,20,...,300} ', ...
             'grid (FR-561): %d points spanning %.10g..%.10g s'], ...
            numel(hl_row), min(hl_row), max(hl_row));
    end
    if numel(fu) ~= numel(hl)
        error('GATE:MISSION_V3:KF:FGRID', ...
            ['Fblk.u_fcst_mps length %d disagrees with the Fblk lead ', ...
             'grid length %d (F content/grid mismatch)'], numel(fu), ...
            numel(hl));
    end
    fw = fb.wl_fcst_m;
    if ~isnumeric(fw) || ~isscalar(fw) || ~isreal(fw) || ~isfinite(fw)
        error('GATE:MISSION_V3:KF:FGRID', ...
            'Fblk.wl_fcst_m must be a finite scalar for one issue');
    end
    out = struct();
    out.fb_u = double(fu(:)).';
    out.fb_w = double(fw);
    out.profile_id = char(pv.forecast_quality_profile_id);
end

%% ======================= frozen registry read =======================

function hb = local_hotel_base(root, template_id)
% P_hotel_base(template) from the frozen mission template registry
% (DGN-09 Path B plan component); single source of truth, fail-closed.
    hb = struct('value', NaN, 'registry_hash', 'UNKNOWN');
    ppath = fullfile(root, 'config', 'mission_v3', ...
        'mission_template_registry.json');
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:KF:REGISTRY', ...
            'frozen file not found: mission_template_registry.json');
    end
    try
        obj = jsondecode(fileread(ppath));
    catch dec_err
        error('GATE:MISSION_V3:KF:REGISTRY', ...
            'jsondecode failed for mission_template_registry.json: %s', ...
            dec_err.message);
    end
    if ~isstruct(obj)
        error('GATE:MISSION_V3:KF:REGISTRY', ...
            'jsondecode did not return a struct for mission_template_registry.json');
    end
    if isfield(obj, 'self_hash') && isfield(obj.self_hash, 'value')
        shv = obj.self_hash;
        hb.registry_hash = char(shv.value);
    end
    if ~isfield(obj, 'mission_templates')
        error('GATE:MISSION_V3:KF:REGISTRY', ...
            'frozen registry lacks mission_templates');
    end
    lst = local_as_list(obj.mission_templates, 'mission_templates');
    for k = 1:numel(lst)
        t = lst{k};
        if isfield(t, 'mission_template_id') ...
                && strcmp(t.mission_template_id, template_id)
            if ~isfield(t, 'p_hotel_base_kW')
                error('GATE:MISSION_V3:KF:REGISTRY', ...
                    'template %s lacks p_hotel_base_kW in the frozen registry', ...
                    template_id);
            end
            v = t.p_hotel_base_kW;
            if ~isnumeric(v) || ~isscalar(v) || ~isfinite(v) || (v < 0)
                error('GATE:MISSION_V3:KF:REGISTRY', ...
                    'template %s has a non-finite p_hotel_base_kW', ...
                    template_id);
            end
            hb.value = double(v);
            return;
        end
    end
    error('GATE:MISSION_V3:KF:REGISTRY', ...
        'template %s not in the frozen mission_template_registry', ...
        template_id);
end

function lst = local_as_list(v, what)
% jsondecode list handling (dag_sample.m local_get_template precedent).
    if iscell(v)
        lst = v(:)';
    elseif isstruct(v)
        n = numel(v);
        lst = cell(1, n);
        for k = 1:n
            lst{k} = v(k);
        end
    else
        error('GATE:MISSION_V3:KF:REGISTRY', ...
            'expected a JSON array for %s', what);
    end
end

function root = local_root()
    root = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(root) || ~exist(fullfile(root, 'config'), 'dir')
        root = pwd;
    end
end

%% ========================= name-value parse =========================

function opts = local_parse_named(arg_cell)
    opts = struct('initial_x', 0.0, 'has_initial_x', false, ...
        'h_chart', NaN, 'has_h_chart', false);
    if mod(numel(arg_cell), 2) ~= 0
        first = arg_cell{1};
        if ~ischar(first) && ~isstring(first)
            error('GATE:MISSION_V3:KF:BAD_ARG', ...
                ['unexpected positional argument after (plan, t0_s, ', ...
                 'Fblk): the causal signature admits no realization/', ...
                 'stream/u_river argument (fixed decision 2)']);
        end
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            'missing value for option %s', char(first));
    end
    k = 1;
    while k <= numel(arg_cell)
        opt = arg_cell{k};
        val = arg_cell{k + 1};
        if ~ischar(opt) && ~isstring(opt)
            error('GATE:MISSION_V3:KF:BAD_ARG', ...
                ['unexpected positional argument after (plan, t0_s, ', ...
                 'Fblk): the causal signature admits no realization/', ...
                 'stream/u_river argument (fixed decision 2)']);
        end
        if strcmpi(opt, 'InitialXPlanM')
            if ~isnumeric(val) || ~isscalar(val) || ~isreal(val) ...
                    || ~isfinite(val) || (val < 0)
                error('GATE:MISSION_V3:KF:BAD_ARG', ...
                    'InitialXPlanM must be a finite non-negative scalar');
            end
            opts.initial_x = double(val);
            opts.has_initial_x = true;
        elseif strcmpi(opt, 'HChartM')
            if ~isnumeric(val) || ~isscalar(val) || ~isreal(val) ...
                    || ~isfinite(val) || (val <= 0)
                error('GATE:MISSION_V3:KF:BAD_ARG', ...
                    'HChartM must be a finite positive scalar');
            end
            opts.h_chart = double(val);
            opts.has_h_chart = true;
        else
            error('GATE:MISSION_V3:KF:BAD_ARG', ...
                'unknown option: %s (accepted: InitialXPlanM, HChartM)', ...
                char(opt));
        end
        k = k + 2;
    end
    if ~opts.has_h_chart
        error('GATE:MISSION_V3:KF:BAD_ARG', ...
            ['''HChartM'' is required: the voyage chart depth scalar ', ...
             '(static INPUT_K; h_chart goes to K per the HANDOFF ', ...
             'epistemics)']);
    end
    if opts.has_initial_x
        opts.initial_x_source = 'caller_provided';
    else
        opts.initial_x_source = 'default_zero';
    end
end

%% ============================ provenance ============================

function prov = local_provenance(n_issue, opts, tpl_hash, prof_ids)
    prov = struct();
    prov.generator = ['mission_v3.kf_snapshots (T-013; design DGN-08/09; ', ...
        'delegation spec experiments/mission_v3/T-013_delegation_spec.md ', ...
        'Fixed Decisions FD1-FD9)'];
    prov.layout = ['issue-major, aligned with +dataset_v3/export_pilot.m: ', ...
        'issue_time_s (Mx1), lead_s (Mx30), content columns (Mx30); ', ...
        sprintf('M=%d for this call', n_issue)];
    prov.lead_grid = ['strictly 10:10:300 (30 points); FR-561 frozen ', ...
        '{10,20,...,300} s lead grid, 30x10 s target grid; the grid is a ', ...
        'named constant and is never taken from the inputs'];
    prov.integration = ['s_plan: 1 s substeps, right-endpoint cumulative ', ...
        'sum, v_plan at the bin right endpoint (export_pilot time-mode ', ...
        'convention); x(issue) = InitialXPlanM'];
    prov.initial_x_plan_source = opts.initial_x_source;
    prov.initial_x_plan_m = opts.initial_x;
    prov.initial_x_note = ['fixed decision 2: the v1 plan origin is s=0, ', ...
        'so the issue position may equivalently be the plan-forward ', ...
        'integral from 0; the caller choice is recorded here'];
    prov.h_chart_source = ['caller name-value ''HChartM'' (voyage-static ', ...
        'INPUT_K; HANDOFF epistemics: h_chart to K, DeltaWL forecast to ', ...
        'F, actual depth to ORACLE)'];
    prov.hotel_source = ['P_hotel_base(template) read from the frozen ', ...
        'mission_template_registry via plan.template_id (DGN-09 Path B ', ...
        'plan component; delta_hotel stays truth/ORACLE-side)'];
    prov.hash_template_registry = tpl_hash;
    prov.zones_source = ['plan.speed_limit_zones / plan.bridge_zones only ', ...
        '(fixed decision 4: the plan is the only plan fact source; zones ', ...
        'are never re-fetched from a realization); membership at the bin ', ...
        'right-endpoint time, half-open windows; FR-560 interval-aware ', ...
        'sampling along the planned trajectory'];
    prov.fblk_assembly = ['INPUT_F assembled verbatim from the ', ...
        'caller-supplied forecast_issue outputs, never recomputed ', ...
        '(fixed decision 5); per-issue vintage equality enforced ', ...
        '(GATE:MISSION_V3:KF:VINTAGE)'];
    prov.fblk_profile_ids = prof_ids;
    prov.missing_policy = ['FR-562: a lead beyond plan.t_grid_s(end) ', ...
        'makes the whole (issue, lead) entry missing: all content ', ...
        'columns NaN, missing_mask true, zone flags false, ', ...
        'contract_status FAILED_CONTRACT; no extrapolation, no zero ', ...
        'fill, no reconstruction from future truth'];
    prov.causality = ['R7 API boundary: the signature (plan, t0_s, Fblk, ', ...
        'name-values) admits no realized-future argument, so mutating ', ...
        'the realized future cannot change the output; a high ', ...
        'statistical correlation between plan and realized execution is ', ...
        'the expected signature of faithful execution and is never by ', ...
        'itself evidence of leakage'];
end

%% ====================== v1 constants (provenance) ===================
% Fixed numeric values live here as named constants with provenance notes
% (fixed decision 9).  Sources: FR-561 / DGN-08, the T-013 delegation
% spec Fixed Decisions, and the export_pilot reference implementation;
% never re-derived quantities.

function leads = KF_CONST_LEAD_GRID()
% FR-561 / DGN-08: the frozen lead grid {10,20,...,300} s (30 points, the
% 30x10 s target grid); identical to +dataset_v3/export_pilot.m
% `leads = (10:10:300)'`.
    leads = 10:10:300;
end

function c = KF_CONST_DT_SUB_S()
% FD3 / DGN-08: 1 s forward-integration substep = the frozen 1 s
% injection/plan grid.
    c = 1.0;
end

function c = KF_CONST_SUBSTEPS_PER_LEAD()
% export_pilot convention: 10 substeps per 10 s lead bin (`for sub = 1:10`).
    c = 10;
end

function c = KF_CONST_TOL()
% Float tolerance of the causal gates (forecast_issue fixed decision 4
% precedent).
    c = 1e-9;
end

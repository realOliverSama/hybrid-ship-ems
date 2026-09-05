function res = forecast_issue(mode, varargin)
% mission_v3.forecast_issue - T-012 causal forecast generator (DGN-06, FR-526~531)
%
%   res = mission_v3.forecast_issue('issue', u_along, t_grid, t0_s, ...
%           h_lead, profileId [, name-values])
%   res = mission_v3.forecast_issue('self_hash')
%
% INPUT_F causal conditional-mean forecast (FR-531: single track, one
% deterministic point conditional mean per lead, no ensemble/PDF/confidence).
%
% CAUSALITY IS MACHINE-ENFORCED AT THE INTERFACE (fixed decision 4):
% u_along/t_grid are the caller-side truth slice TRUNCATED TO t <= t0
% (R1 derived along-track component for one segment direction; the
% forecaster itself is direction-agnostic and acts on the passed series).
% The function asserts max(t_grid) <= t0_s (1e-9 tolerance) and raises
% GATE:MISSION_V3:FCST:FUTURE_INPUT on any future-truth injection
% (AC-436 negative test).  It further asserts that the causal source
% time t0 - source_cutoff_lag_s (and, for DEGRADED_V1, t0 - delta_lag_s)
% is covered by the provided slice and raises
% GATE:MISSION_V3:FCST:NO_PREHISTORY otherwise (R5: no lag whose causal
% source history does not exist).
%
% PROFILES (every numeric parameter is read from the frozen
% config/mission_v3/forecast_quality_profiles.json and
% config/mission_v3/environment_regime_params.json; nothing hard-coded):
%   GOOD_V1:
%     u_fcst(t0+h|t0) = U_reach + (u_obs(t0) - U_reach)*exp(-h/tau_u)
%     u_obs(t0) = slice value at t0 (OBSERVED, deployment-measurable);
%     U_reach = time mean of the provided truncated slice (numerical
%             realization of along_sign * u_river_mean; the rule is
%             recorded in provenance.u_reach_mean_rule);
%     tau_u = env params regimes.<RegimeId>.delta_u.tau_s
%             ('RegimeId' name-value, default 'NORMAL');
%     wl_fcst = DeltaWL (voyage-constant water-level offset persisted as
%             the causal estimate; OBSERVED class, DGN-06 R4).
%   DEGRADED_V1 (the degradation lives entirely in u_obs; the forecast
%   form is identical to GOOD_V1, fixed decision 3):
%     u_obs(t0) = U(t0 - delta_lag) + b_voyage + eta(t0)
%       delta_lag from the profile (1800 s candidate; consumes the R5
%       prehistory so source_cutoff = t0 - 1800 may be negative);
%       b_voyage ~ N(mu, sigma^2) voyage-level constant (profile);
%       eta: exact-discretization AR(1) observation-error path
%       eta[k+1] = eta[k]*exp(-dt/tau_eta)
%                  + sigma_eta*sqrt(1 - exp(-2*dt/tau_eta))*Z
%       evolved over the provided slice grid with stationary
%       initialization eta[1] = sigma_eta*Z; eta(t0) = eta at the last
%       slice grid point (ZOH discipline of the frozen injection grid);
%     wl_fcst = DeltaWL + b_wl with b_wl ~ N(0, sigma_bvoyage^2) (same
%       mechanism as b_voyage; the refined wl-side parameter freeze is
%       T-007-owned, recorded in provenance.wl_note).
%
% NOISE (fixed decision 7): every random draw comes from the
% forecast_obs_noise seed-tree branch (DGN-07).  The caller either passes
% a live RandStream ('NoiseStream') or 'FormalSeed' + 'BaseDrawIndex'
% (a Threefry4x64 stream is then built internally via
% mission_v3.seed_tree('lineage', ...) against the frozen policy file);
% passing both forms is an ARG error and DEGRADED_V1 without either form
% is an ARG error.  The global RNG is never touched.  Draw order
% (DEGRADED_V1, each draw only when not caller-supplied): b_voyage,
% b_wl, then one bulk block of n_grid normals (stationary eta init, then
% one Z per grid step).  b_voyage/b_wl are returned in res.obs so the
% caller can cache them per (formal_seed, base_draw_index, segment
% direction) and pass them back via 'BVoyage'/'BWl' on later issues.
%
% OBSERVABILITY (fixed decision 5 / FR-527): source_class must be in the
% frozen whitelist {OBSERVED, CAUSALLY_ESTIMATED, STATIC_KNOWN_PARAMETER,
% PREDECLARED_MODEL_PARAMETER}; any FORBIDDEN class (FUTURE_TRUTH,
% ORACLE_STATE, SIMULATOR_LATENT_PRIVILEGED) or unknown class raises
% GATE:MISSION_V3:FCST:SOURCE_CLASS.  Defaults: GOOD_V1 -> the profile's
% u_obs_source_class (OBSERVED); DEGRADED_V1 -> CAUSALLY_ESTIMATED (the
% degraded observation is a causal estimate formed from the lagged
% measurement plus the declared degradation model).  The formal
% provenance class is fail-closed-asserted to CAUSAL_GENERATOR (FR-530).
%
% PROVENANCE (FR-530 / fixed decision 6): generator_id, version, hash
% (SHA256 of this file, byte-exact), issue_time_s, source_cutoff_s
% (GOOD = t0, DEGRADED = t0 - lag; may be negative, always <= issue
% time), forecast_quality_profile_id, source_class,
% provenance_class_formal, forecast_prehistory_s (R5), u_reach_mean_rule
% (fixed decision 1), plus wl_note for DEGRADED_V1.
%
% NAME-VALUES ('issue' mode):
%   'NoiseStream'    RandStream for the forecast_obs_noise branch
%   'FormalSeed'     formal seed string (together with 'BaseDrawIndex')
%   'BaseDrawIndex'  whole number >= 0 (together with 'FormalSeed')
%   'BVoyage'        caller-cached voyage bias (DEGRADED_V1)
%   'BWl'            caller-cached water-level bias (DEGRADED_V1)
%   'DeltaWL'        realized voyage water-level offset (default 0)
%   'RegimeId'       env regime selecting tau_u (default 'NORMAL')
%   'SourceClass'    observability class override (whitelist-enforced)
%
% GATE ERROR IDS: GATE:MISSION_V3:FCST:{ARG, PROFILE, SOURCE_CLASS,
% FUTURE_INPUT, NO_PREHISTORY, REGIME, PARAM, REGISTRY, SEED, PROVENANCE}.
%
% 'self_hash' returns struct('algorithm','sha256','value',<64 hex>,
% 'file','forecast_issue.m'); the provenance hash and the T-016 stack
% identity (DGN-14) reuse this value.
%
% Determinism: pure function of (slice, t0, leads, profile, frozen
% registries, name-values, noise stream state); identical inputs give
% bitwise-identical output.  This function writes nothing to disk.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% no global RNG; ROOT resolved from mfilename with pwd fallback; thin
% main + local functions (style ref: +mission_v3/env_realize.m).

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:FCST:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'self_hash'
            if nargin ~= 1
                error('GATE:MISSION_V3:FCST:ARG', ...
                    '''self_hash'' takes no further arguments');
            end
            res = local_self_hash();
        case 'issue'
            if nargin < 6
                error('GATE:MISSION_V3:FCST:ARG', ...
                    ['''issue'' expects: u_along, t_grid, t0_s, h_lead, ', ...
                     'profileId [, name-values]']);
            end
            u_along = varargin{1};
            t_grid = varargin{2};
            t0_s = varargin{3};
            h_lead = varargin{4};
            profile_id = varargin{5};
            opts = local_parse_named(varargin(6:end));
            res = local_issue(u_along, t_grid, t0_s, h_lead, profile_id, opts);
        otherwise
            error('GATE:MISSION_V3:FCST:ARG', 'unknown mode: %s', char(mode));
    end
end

%% ============================ issue ================================

function res = local_issue(u_along, t_grid, t0_s, h_lead, profile_id, opts)
    tol = 1e-9;   % fixed decision 4: floating tolerance of the causal gates

    % ---- basic input validation ----
    if ~isnumeric(u_along) || ~isreal(u_along) || ~isvector(u_along)
        error('GATE:MISSION_V3:FCST:ARG', 'u_along must be a real numeric vector');
    end
    if ~isnumeric(t_grid) || ~isreal(t_grid) || ~isvector(t_grid)
        error('GATE:MISSION_V3:FCST:ARG', 't_grid must be a real numeric vector');
    end
    u_along = double(u_along(:)).';
    t_grid = double(t_grid(:)).';
    if numel(u_along) ~= numel(t_grid)
        error('GATE:MISSION_V3:FCST:ARG', ...
            'u_along and t_grid must have the same number of elements');
    end
    n_pts = numel(t_grid);
    if n_pts < 2
        error('GATE:MISSION_V3:FCST:ARG', ...
            'the history slice must contain at least 2 grid points');
    end
    if ~all(isfinite(u_along)) || ~all(isfinite(t_grid))
        error('GATE:MISSION_V3:FCST:ARG', 'slice times and values must be finite');
    end
    if ~all(diff(t_grid) > 0)
        error('GATE:MISSION_V3:FCST:ARG', 't_grid must be strictly increasing');
    end
    if ~isnumeric(t0_s) || ~isscalar(t0_s) || ~isreal(t0_s) || ~isfinite(t0_s)
        error('GATE:MISSION_V3:FCST:ARG', 't0_s must be a finite real scalar');
    end
    t0_s = double(t0_s);
    if ~isnumeric(h_lead) || ~isreal(h_lead) || ~isvector(h_lead) || isempty(h_lead)
        error('GATE:MISSION_V3:FCST:ARG', 'h_lead must be a non-empty numeric vector');
    end
    h_lead = double(h_lead(:)).';
    if ~all(isfinite(h_lead)) || any(h_lead < 0)
        error('GATE:MISSION_V3:FCST:ARG', 'h_lead values must be finite and >= 0');
    end
    if ~ischar(profile_id) && ~isstring(profile_id)
        error('GATE:MISSION_V3:FCST:ARG', 'profileId must be a string');
    end
    profile_id = char(profile_id);

    % ---- causal gate 1: no future truth may enter the interface ----
    if max(t_grid) > t0_s + tol
        error('GATE:MISSION_V3:FCST:FUTURE_INPUT', ...
            ['future-truth injection detected: max(t_grid)=%.10g exceeds ', ...
             'the issue time t0=%.10g; the interface accepts only t <= t0 ', ...
             'slices (AC-436)'], max(t_grid), t0_s);
    end

    % ---- frozen forecast quality profiles ----
    prof = local_read_profiles();
    prof_all = local_field(prof, 'profiles', ...
        'forecast_quality_profiles.profiles');
    if ~isfield(prof_all, profile_id)
        error('GATE:MISSION_V3:FCST:PROFILE', ...
            'profileId not in the frozen forecast quality profiles: %s', ...
            profile_id);
    end
    pf = prof_all.(profile_id);
    if ~isstruct(pf) || ~isscalar(pf)
        error('GATE:MISSION_V3:FCST:PARAM', ...
            'profile entry must be a single struct: %s', profile_id);
    end
    is_degraded = isfield(pf, 'u_obs_at_issue');
    is_good = isfield(pf, 'u_forecast');
    if is_degraded == is_good
        error('GATE:MISSION_V3:FCST:PARAM', ...
            ['profile %s matches neither/both of the implemented profile ', ...
             'structures (u_obs_at_issue / u_forecast)'], profile_id);
    end
    use_noise = is_degraded;

    % profile numeric parameters (frozen registry; nothing hard-coded)
    what_cut = ['profiles.', profile_id, '.source_cutoff_lag_s'];
    cutoff_lag = local_req_nonneg(local_field(pf, 'source_cutoff_lag_s', ...
        what_cut), what_cut);
    if use_noise
        delta_lag = local_req_nonneg(local_field(pf, 'delta_lag_s', ...
            'profiles.DEGRADED_V1.delta_lag_s'), ...
            'profiles.DEGRADED_V1.delta_lag_s');
        bvp = local_field(pf, 'b_voyage', 'profiles.DEGRADED_V1.b_voyage');
        mu_b = local_req_num(local_field(bvp, 'mu', ...
            'profiles.DEGRADED_V1.b_voyage.mu'), ...
            'profiles.DEGRADED_V1.b_voyage.mu');
        sigma_b = local_req_pos(local_field(bvp, 'sigma', ...
            'profiles.DEGRADED_V1.b_voyage.sigma'), ...
            'profiles.DEGRADED_V1.b_voyage.sigma');
        eap = local_field(pf, 'eta_ar1', 'profiles.DEGRADED_V1.eta_ar1');
        tau_eta = local_req_pos(local_field(eap, 'tau_s', ...
            'profiles.DEGRADED_V1.eta_ar1.tau_s'), ...
            'profiles.DEGRADED_V1.eta_ar1.tau_s');
        sigma_eta = local_req_pos(local_field(eap, 'sigma_mps', ...
            'profiles.DEGRADED_V1.eta_ar1.sigma_mps'), ...
            'profiles.DEGRADED_V1.eta_ar1.sigma_mps');
        obs_lag = delta_lag;
    else
        delta_lag = 0.0;
        mu_b = 0.0;
        sigma_b = 0.0;
        tau_eta = 0.0;
        sigma_eta = 0.0;
        obs_lag = 0.0;
    end
    prehist_s = local_req_nonneg(local_field(prof, 'prehistory_s', ...
        'forecast_quality_profiles.prehistory_s'), ...
        'forecast_quality_profiles.prehistory_s');

    % ---- observability whitelist (fixed decision 5 / FR-527) ----
    wl_ok = local_cellstr(local_field(prof, 'observability_whitelist', ...
        'forecast_quality_profiles.observability_whitelist'), ...
        'observability_whitelist');
    wl_no = local_cellstr(local_field(prof, 'observability_forbidden', ...
        'forecast_quality_profiles.observability_forbidden'), ...
        'observability_forbidden');
    if opts.has_source_class
        source_class = opts.source_class;
    else
        source_class = local_default_source_class(pf, is_degraded);
    end
    if ismember(source_class, wl_no) || ~ismember(source_class, wl_ok)
        error('GATE:MISSION_V3:FCST:SOURCE_CLASS', ...
            ['source_class violates the observability whitelist: %s ', ...
             '(forbidden: FUTURE_TRUTH / ORACLE_STATE / ', ...
             'SIMULATOR_LATENT_PRIVILEGED)'], source_class);
    end
    prov_pr = local_field(prof, 'provenance', ...
        'forecast_quality_profiles.provenance');
    pcf = char(local_field(prov_pr, 'provenance_class_formal', ...
        'forecast_quality_profiles.provenance.provenance_class_formal'));
    if ~strcmp(pcf, 'CAUSAL_GENERATOR')
        error('GATE:MISSION_V3:FCST:PROVENANCE', ...
            'frozen provenance_class_formal is not CAUSAL_GENERATOR: %s', pcf);
    end

    % ---- causal gate 2: causal source coverage (R5 prehistory) ----
    source_time = t0_s - cutoff_lag;
    if source_time > t0_s + tol
        error('GATE:MISSION_V3:FCST:PROVENANCE', ...
            'source_cutoff_s must satisfy <= issue_time_s (R5 invariant)');
    end
    local_check_coverage(source_time, t_grid, tol);
    u_source_time = t0_s - obs_lag;
    local_check_coverage(u_source_time, t_grid, tol);

    % ---- tau_u from the frozen env params (fixed decision 2) ----
    envj = local_read_env_params();
    if ~isfield(envj, 'regimes') || ~isfield(envj.regimes, opts.regime_id)
        error('GATE:MISSION_V3:FCST:REGIME', ...
            'regime not in the frozen env params: %s', opts.regime_id);
    end
    reg_e = envj.regimes.(opts.regime_id);
    du = local_field(reg_e, 'delta_u', ...
        ['environment_regime_params.regimes.', opts.regime_id]);
    what_tau = 'environment_regime_params.delta_u.tau_s';
    tau_u = local_req_pos(local_field(du, 'tau_s', what_tau), what_tau);

    % ---- reach mean: time mean of the provided slice (fixed decision 1) ----
    u_reach = mean(u_along);

    % ---- noise stream resolution (fixed decision 7) ----
    rs = [];
    lineage_id = '';
    noise_source = 'none';
    if use_noise
        if ~isempty(opts.noise_stream)
            rs = opts.noise_stream;
            noise_source = 'caller_stream';
        elseif ~isempty(opts.formal_seed)
            bst = local_build_stream(opts.formal_seed, opts.base_draw_index);
            rs = bst.rs;
            lineage_id = bst.lineage_id;
            noise_source = 'formal_seed';
        else
            error('GATE:MISSION_V3:FCST:ARG', ...
                ['DEGRADED_V1 draws from the forecast_obs_noise branch: ', ...
                 'pass ''NoiseStream'' or ''FormalSeed''+''BaseDrawIndex''']);
        end
    end

    % ---- observation at issue and water-level forecast ----
    u_lag_val = interp1(t_grid, u_along, u_source_time, 'linear');
    if use_noise
        n_draws = 0;
        if opts.has_b_voyage
            b_voyage = opts.b_voyage;
            bv_supplied = true;
        else
            b_voyage = mu_b + sigma_b * randn(rs, 1);
            bv_supplied = false;
            n_draws = n_draws + 1;
        end
        if opts.has_b_wl
            b_wl = opts.b_wl;
            bw_supplied = true;
        else
            b_wl = sigma_b * randn(rs, 1);   % N(0, sigma_b^2), mu = 0
            bw_supplied = false;
            n_draws = n_draws + 1;
        end
        z_all = randn(rs, [1, n_pts]);
        eta = zeros(1, n_pts);
        eta(1) = sigma_eta * z_all(1);       % stationary initialization
        for kk = 2:n_pts
            dt_k = t_grid(kk) - t_grid(kk - 1);
            a_k = exp(-dt_k / tau_eta);
            s_k = sigma_eta * sqrt(1 - exp(-2 * dt_k / tau_eta));
            eta(kk) = eta(kk - 1) * a_k + s_k * z_all(kk);
        end
        n_draws = n_draws + n_pts;
        eta_t0 = eta(n_pts);
        u_obs = u_lag_val + b_voyage + eta_t0;
        wl_fcst = opts.delta_wl + b_wl;
    else
        b_voyage = [];
        b_wl = [];
        eta = [];
        eta_t0 = [];
        n_draws = 0;
        bv_supplied = false;
        bw_supplied = false;
        u_obs = u_lag_val;
        wl_fcst = opts.delta_wl;
    end

    % ---- single-track conditional mean forecast (FR-531) ----
    u_fcst = u_reach + (u_obs - u_reach) .* exp(-h_lead / tau_u);

    % ---- provenance (FR-530 / fixed decision 6) ----
    self_h = local_self_hash();
    provenance = struct();
    provenance.generator_id = char(local_field(prov_pr, 'generator_id', ...
        'forecast_quality_profiles.provenance.generator_id'));
    provenance.version = char(local_field(prof, 'version', ...
        'forecast_quality_profiles.version'));
    provenance.hash = self_h.value;
    provenance.issue_time_s = t0_s;
    provenance.source_cutoff_s = source_time;
    provenance.forecast_quality_profile_id = profile_id;
    provenance.source_class = source_class;
    provenance.provenance_class_formal = pcf;
    provenance.forecast_prehistory_s = prehist_s;
    provenance.u_reach_mean_rule = ...
        'time_mean_of_provided_truncated_alongtrack_slice';
    if use_noise
        provenance.wl_note = ['wl degradation is bias-dominated; sigma ', ...
            'shared with b_voyage; refined wl parameter freeze owned by ', ...
            'the T-007 snapshot (fixed decision 3)'];
    end

    % ---- observation block (audit + caller-side b caching) ----
    obs = struct();
    obs.source_class = source_class;
    obs.u_source_time_s = u_source_time;
    obs.delta_lag_s = obs_lag;
    obs.u_lagged_mps = u_lag_val;
    obs.b_voyage = b_voyage;
    obs.b_wl = b_wl;
    obs.eta_t0 = eta_t0;
    obs.eta_path = eta;
    obs.n_draws = n_draws;
    obs.b_voyage_caller_supplied = bv_supplied;
    obs.b_wl_caller_supplied = bw_supplied;
    obs.noise_source = noise_source;
    obs.noise_lineage_id = lineage_id;

    res = struct();
    res.mode = 'issue';
    res.profile_id = profile_id;
    res.t0_s = t0_s;
    res.h_lead_s = h_lead;
    res.u_fcst_mps = u_fcst;
    res.wl_fcst_m = wl_fcst;
    res.u_obs_mps = u_obs;
    res.u_reach_mean_mps = u_reach;
    res.tau_u_s = tau_u;
    res.regime_id = opts.regime_id;
    res.delta_wl_m = opts.delta_wl;
    res.source_cutoff_lag_s = cutoff_lag;
    res.prehistory_s = prehist_s;
    res.provenance = provenance;
    res.obs = obs;
end

%% ======================== local functions ==========================

function opts = local_parse_named(arg_cell)
    opts = struct('noise_stream', [], 'formal_seed', '', ...
        'base_draw_index', [], 'b_voyage', [], 'b_wl', [], ...
        'has_b_voyage', false, 'has_b_wl', false, 'delta_wl', 0.0, ...
        'regime_id', 'NORMAL', 'source_class', '', 'has_source_class', false);
    k = 1;
    while k <= numel(arg_cell)
        if (k + 1) > numel(arg_cell)
            error('GATE:MISSION_V3:FCST:ARG', ...
                'missing value for option %s', string(arg_cell{k}));
        end
        opt = arg_cell{k};
        val = arg_cell{k + 1};
        if ~ischar(opt) && ~isstring(opt)
            error('GATE:MISSION_V3:FCST:ARG', 'option name must be a string');
        end
        if strcmpi(opt, 'NoiseStream')
            if ~isa(val, 'RandStream')
                error('GATE:MISSION_V3:FCST:ARG', ...
                    'NoiseStream must be a RandStream object');
            end
            opts.noise_stream = val;
        elseif strcmpi(opt, 'FormalSeed')
            if ~ischar(val) && ~isstring(val)
                error('GATE:MISSION_V3:FCST:ARG', 'FormalSeed must be a string');
            end
            opts.formal_seed = char(val);
        elseif strcmpi(opt, 'BaseDrawIndex')
            if ~isnumeric(val) || ~isscalar(val) || ~isfinite(val) ...
                    || val < 0 || val ~= floor(val)
                error('GATE:MISSION_V3:FCST:ARG', ...
                    'BaseDrawIndex must be a whole number >= 0');
            end
            opts.base_draw_index = double(val);
        elseif strcmpi(opt, 'BVoyage')
            if ~isnumeric(val) || ~isscalar(val) || ~isreal(val) || ~isfinite(val)
                error('GATE:MISSION_V3:FCST:ARG', ...
                    'BVoyage must be a finite real scalar');
            end
            opts.b_voyage = double(val);
            opts.has_b_voyage = true;
        elseif strcmpi(opt, 'BWl')
            if ~isnumeric(val) || ~isscalar(val) || ~isreal(val) || ~isfinite(val)
                error('GATE:MISSION_V3:FCST:ARG', ...
                    'BWl must be a finite real scalar');
            end
            opts.b_wl = double(val);
            opts.has_b_wl = true;
        elseif strcmpi(opt, 'DeltaWL')
            if ~isnumeric(val) || ~isscalar(val) || ~isreal(val) || ~isfinite(val)
                error('GATE:MISSION_V3:FCST:ARG', ...
                    'DeltaWL must be a finite real scalar');
            end
            opts.delta_wl = double(val);
        elseif strcmpi(opt, 'RegimeId')
            if (~ischar(val) && ~isstring(val)) || isempty(char(val))
                error('GATE:MISSION_V3:FCST:ARG', 'RegimeId must be a string');
            end
            opts.regime_id = char(val);
        elseif strcmpi(opt, 'SourceClass')
            if (~ischar(val) && ~isstring(val)) || isempty(char(val))
                error('GATE:MISSION_V3:FCST:ARG', 'SourceClass must be a string');
            end
            opts.source_class = char(val);
            opts.has_source_class = true;
        else
            error('GATE:MISSION_V3:FCST:ARG', ...
                'unknown option: %s', char(opt));
        end
        k = k + 2;
    end
    has_stream = ~isempty(opts.noise_stream);
    has_seed = ~isempty(opts.formal_seed) || ~isempty(opts.base_draw_index);
    if has_stream && has_seed
        error('GATE:MISSION_V3:FCST:ARG', ...
            ['''NoiseStream'' and ''FormalSeed''/''BaseDrawIndex'' are ', ...
             'mutually exclusive stream specifications (fixed decision 7)']);
    end
    if xor(isempty(opts.formal_seed), isempty(opts.base_draw_index))
        error('GATE:MISSION_V3:FCST:ARG', ...
            '''FormalSeed'' and ''BaseDrawIndex'' must be passed together');
    end
end

function cls = local_default_source_class(pf, is_degraded)
% Default observability class per profile: GOOD_V1 reads the frozen
% u_obs_source_class; DEGRADED_V1 reconstructs u_obs(t0) from the lagged
% measurement plus the declared degradation model, i.e. a causal
% estimate (FR-527).
    if is_degraded
        cls = 'CAUSALLY_ESTIMATED';
    else
        uf = local_field(pf, 'u_forecast', 'profiles.<profile>.u_forecast');
        cls = char(local_field(uf, 'u_obs_source_class', ...
            'profiles.<profile>.u_forecast.u_obs_source_class'));
    end
end

function bst = local_build_stream(formal_seed, base_draw_index)
% Build the forecast_obs_noise branch stream from the frozen seed policy
% via mission_v3.seed_tree (DGN-07); the branch name is cross-checked
% against the frozen policy branch list (fail-closed).
    pol = local_read_seed_policy();
    pol_pv = char(local_field(pol, 'policy_version', ...
        'seed_tree_policy.policy_version'));
    pol_br = local_cellstr(local_field(pol, 'branches', ...
        'seed_tree_policy.branches'), 'seed_tree_policy.branches');
    branch_name = 'forecast_obs_noise';
    if ~ismember(branch_name, pol_br)
        error('GATE:MISSION_V3:FCST:SEED', ...
            'frozen seed tree policy lacks the forecast_obs_noise branch');
    end
    lin = mission_v3.seed_tree('lineage', pol_pv, formal_seed, ...
        base_draw_index, branch_name);
    rs = RandStream('Threefry4x64', 'Seed', lin.engine_seed_uint32);
    bst = struct();
    bst.rs = rs;
    bst.lineage_id = lin.lineage_id;
end

function local_check_coverage(src_t, t_grid, tol)
% R5/DGN-06: no lag whose causal source history does not exist.  The
% causal source time must lie inside the provided history slice.
    if (src_t < min(t_grid) - tol) || (src_t > max(t_grid) + tol)
        error('GATE:MISSION_V3:FCST:NO_PREHISTORY', ...
            ['causal source time %.10g s is not covered by the provided ', ...
             'history slice [%.10g, %.10g] (R5/DGN-06: no lag whose ', ...
             'causal source history does not exist)'], ...
            src_t, min(t_grid), max(t_grid));
    end
end

function out = local_self_hash()
% SHA256 of this file, byte-exact (read own source via mfilename).
    self_path = [mfilename('fullpath'), '.m'];
    fid = fopen(self_path, 'r');
    if fid < 0
        error('GATE:MISSION_V3:FCST:REGISTRY', ...
            'cannot open own source file for hashing');
    end
    raw = fread(fid, Inf, 'uint8=>uint8');
    fclose(fid);
    md = java.security.MessageDigest.getInstance('SHA-256');
    dg = typecast(md.digest(typecast(raw(:), 'int8')), 'uint8');
    out = struct();
    out.algorithm = 'sha256';
    out.value = lower(reshape(dec2hex(dg, 2)', 1, []));
    out.file = 'forecast_issue.m';
end

function prof = local_read_profiles()
    root = local_root();
    prof = local_read_json(root, 'forecast_quality_profiles.json');
end

function envj = local_read_env_params()
    root = local_root();
    envj = local_read_json(root, 'environment_regime_params.json');
end

function pol = local_read_seed_policy()
    root = local_root();
    pol = local_read_json(root, 'seed_tree_policy.json');
end

function obj = local_read_json(root, fname)
% jsondecode is the sanctioned consumption parser here (seed_tree.m /
% env_realize.m precedent); the frozen files are validator-clean.
    ppath = fullfile(root, 'config', 'mission_v3', fname);
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:FCST:REGISTRY', ...
            'frozen file not found: %s', fname);
    end
    try
        obj = jsondecode(fileread(ppath));
    catch dec_err
        error('GATE:MISSION_V3:FCST:REGISTRY', ...
            'jsondecode failed for %s: %s', fname, dec_err.message);
    end
    if ~isstruct(obj)
        error('GATE:MISSION_V3:FCST:REGISTRY', ...
            'jsondecode did not return a struct for %s', fname);
    end
end

function root = local_root()
    root = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(root) || ~exist(fullfile(root, 'config'), 'dir')
        root = pwd;
    end
end

function v = local_field(s, name, what)
    if ~isstruct(s) || ~isfield(s, name)
        error('GATE:MISSION_V3:FCST:PARAM', ...
            'frozen registry lacks field: %s', what);
    end
    v = s.(name);
end

function v = local_req_num(v, what)
    if ~isnumeric(v) || ~isscalar(v) || ~isreal(v) || ~isfinite(v)
        error('GATE:MISSION_V3:FCST:PARAM', ...
            'frozen parameter must be a finite real scalar: %s', what);
    end
    v = double(v);
end

function v = local_req_pos(v, what)
    v = local_req_num(v, what);
    if v <= 0
        error('GATE:MISSION_V3:FCST:PARAM', ...
            'frozen parameter must be > 0: %s', what);
    end
end

function v = local_req_nonneg(v, what)
    v = local_req_num(v, what);
    if v < 0
        error('GATE:MISSION_V3:FCST:PARAM', ...
            'frozen parameter must be >= 0: %s', what);
    end
end

function lst = local_cellstr(v, what)
    if ischar(v)
        lst = {char(v)};
    elseif iscell(v)
        lst = cell(1, numel(v));
        for k = 1:numel(v)
            e = v{k};
            if ~ischar(e) && ~isstring(e)
                error('GATE:MISSION_V3:FCST:REGISTRY', ...
                    '%s must be a cellstr', what);
            end
            lst{k} = char(e);
        end
    else
        error('GATE:MISSION_V3:FCST:REGISTRY', '%s must be a cellstr', what);
    end
end

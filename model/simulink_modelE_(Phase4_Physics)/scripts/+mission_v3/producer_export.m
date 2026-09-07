function res = producer_export(mode, varargin)
% mission_v3.producer_export - T-015 Mission v3 voyage artifact assembly
% and FR-500 stage-isolation assertion face (FR-501/504; design DGN-15;
% delegation spec experiments/mission_v3/T-015_delegation_spec.md Fixed
% Decisions FD5-FD7).
%
% The artifact layout is the D-side frozen pilot layout
% (+dataset_v3/export_pilot.m, surveyed per FD5): three top-level
% structs {stream, kf, meta} in one MAT file.  The '-v7' save option is
% the D design deviation D-2 frozen value: it is CONSUMED from the D
% producer contract (tasks.md T-015: "artifact representation format
% consumes the D producer contract frozen values; this task layer must
% not restate or change format assumptions") and never restated as a new
% assumption here.  Field-name authority: the machine producer schema
% (FR-501: on conflict the machine schema wins) - e.g. the schema field
% producer_script_sha is required even though export_pilot carries the
% historical name producer_script_sha256.
%
% CALLER CONTRACT (T-020 pre-qualification main loop; upstream guard
% transmitted with the T-015 delegation spec): the main loop must assert
% per voyage that scn.hotel_kW == p_hotel_base(template) +
% dag.delta_p_hotel_unplanned (replay-drift machine guard for the
% deterministic scenario_compile hotel delta).  That guard belongs to
% the caller: 'export_artifact' receives already-realized stream/kf/meta
% facts and never re-derives scenario hotel power.
%
% MODES
%   res = mission_v3.producer_export('export_artifact', stream, kf, ...
%           meta, elig, out_file)
%       Validates the artifact against the frozen D producer schema and
%       the export_pilot layout, enforces the FR-500 stage-isolation
%       face, cross-checks the caller eligibility record against the
%       artifact itself (FR-536), embeds it as meta.eligibility (C-side
%       extension field; FR-536 requires the counts to travel with the
%       artifact), writes MAT v7, and self-checks by reading the file
%       back field by field.  No simulation happens here; the formal
%       write path is T-020.
%   res = mission_v3.producer_export('stage_check', meta)
%       FR-500 assertion face only (FD6): dataset_domain='FORMAL'
%       requires meta.fixture_kind present and exactly 'NONE' - a
%       missing or non-NONE fixture_kind raises the frozen gate id
%       GATE:MISSION_V3:EXPORT:STAGE_ISOLATION.  The PILOT domain is
%       unrestricted (PILOT_ONLY/NONE dual-track parallel storage is
%       legal).  Determining the FR-500 face needs dataset_domain to be
%       present and one of {FORMAL, PILOT}.
%
% REQUIRED meta FIELDS (presence-checked; schema families driven from
% the machine JSON + C-D1 extensions from design DGN-15 / FD5):
%   voyage_metadata family (schema): voyage_id, voyage_family_id,
%     route_template_id, mission_template_id, split_group_id,
%     scenario_stratum
%   generation_provenance family (schema): generation_seed,
%     model_baseline_ref, producer_script_sha, fixture_kind
%   ood_config_surface family (schema): ood_axis, ood_def_version,
%     physics_domain_ref
%   static_constraints face (export_pilot layout; empty sets legal and
%     must be recorded as empty, DGN-13): speed_limit_zones,
%     bridge_zones
%   export_pilot layout: voyage_duration_s, scenario_id
%   forecast_provenance (schema family struct with its six fields)
%   C-D1 extensions: base_draw_id, intervention,
%     forecast_quality_profile_id, stratum, dataset_domain
%
% kf LAYOUT (export_pilot/kf_snapshots issue-major freeze): issue_time_s
% (M x 1), lead_s (M x 30, every row the frozen {10,...,300} grid), six
% content columns v_plan_mps/s_plan_m/h_chart_m/P_hotel_plan_kW/
% u_fcst_mps/wl_fcst_m (M x 30).  M = 0 is legal (a voyage shorter than
% the window span produces zero windows/snapshots, FR-472).
%
% stream LAYOUT: time_s + every schema-required truth_stream field
% (names read from the machine schema, cross-anchored to the signal
% registry by the schema itself); per-field types enforced from the
% schema type strings.
%
% INTEGRITY (FR-536): the caller-supplied elig record ('judge' output;
% required for FORMAL, optional for PILOT) is cross-checked against the
% window arithmetic recomputed from meta.voyage_duration_s and the kf
% snapshot coverage actually present in the artifact; a mismatch raises
% GATE:MISSION_V3:EXPORT:ELIG_RECORD so a stale/masked qualification
% record can never ship.
%
% GATE ERROR IDS: GATE:MISSION_V3:EXPORT:{BAD_ARG, META, LAYOUT,
% STAGE_ISOLATION, ELIG_RECORD, ROUNDTRIP}.
%
% Determinism: pure assembly of caller facts; no RNG; identical inputs
% give identical structs and thus field-identical round-trips.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% ROOT resolved from mfilename with pwd fallback; schema/registry
% constants read from the frozen JSON files (never hard-coded).

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
            'mode must be a char vector');
    end
    switch char(mode)
        case 'export_artifact'
            if nargin < 6
                error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
                    ['''export_artifact'' expects: stream, kf, meta, ', ...
                     'elig, out_file']);
            end
            res = local_export(varargin{1}, varargin{2}, varargin{3}, ...
                varargin{4}, varargin{5});
        case 'stage_check'
            if nargin < 2
                error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
                    '''stage_check'' expects: meta');
            end
            res = local_stage_check(varargin{1});
        otherwise
            error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
                ['unknown mode: %s (producer_export implements ', ...
                 '''export_artifact'' and ''stage_check'')'], char(mode));
    end
end

%% ======================== stage_check (FR-500) ======================

function res = local_stage_check(meta)
    if ~isstruct(meta) || ~isscalar(meta)
        error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
            'meta must be a scalar struct');
    end
    if ~isfield(meta, 'dataset_domain') || isempty(meta.dataset_domain)
        error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
            ['meta lacks dataset_domain: the FR-500 face cannot ', ...
             'certify stage isolation without it']);
    end
    dom = local_as_char(meta.dataset_domain, 'meta.dataset_domain');
    if ~any(strcmp(dom, {'FORMAL', 'PILOT'}))
        error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
            'unknown dataset_domain: %s (FORMAL | PILOT)', dom);
    end
    fk = 'MISSING';
    if isfield(meta, 'fixture_kind') && ~isempty(meta.fixture_kind)
        fk = local_as_char(meta.fixture_kind, 'meta.fixture_kind');
    end
    if strcmp(dom, 'FORMAL') && ~strcmp(fk, 'NONE')
        % FD6 (FR-500): a missing or non-NONE fixture_kind never enters
        % the formal path (frozen gate id).
        error('GATE:MISSION_V3:EXPORT:STAGE_ISOLATION', ...
            ['FR-500: a formal artifact requires fixture_kind=NONE ', ...
             '(got %s); PILOT_ONLY/fixture content is pilot-domain only'], ...
            fk);
    end
    res = struct('ok', true, 'dataset_domain', dom, 'fixture_kind', fk);
end

%% ========================= export_artifact ==========================

function res = local_export(stream, kf, meta, elig, out_file)
    if ~ischar(out_file) && ~isstring(out_file)
        error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
            'out_file must be a char/string path');
    end
    fpath = char(out_file);

    % 1. FR-500 face first (fail-closed before any write)
    sc = local_stage_check(meta);

    % 2. frozen machine schema
    root = local_root();
    sch = local_read_json(fullfile(root, 'config', ...
        'dataset_v3_producer_schema.json'));

    % 3. conformance: stream / kf / meta
    local_check_stream(stream, sch);
    w0 = mission_v3.eligibility('windows', 0);   % frozen K/F grid constants
    local_check_kf(kf, w0);
    local_check_meta(meta, sch);

    % 4. eligibility record rule (FR-536)
    if strcmp(sc.dataset_domain, 'FORMAL') && isempty(elig)
        error('GATE:MISSION_V3:EXPORT:META', ...
            ['a FORMAL artifact requires the FR-536 eligibility record ', ...
             '(mission_v3.eligibility ''judge'' output)']);
    end
    if ~isempty(elig)
        if ~isstruct(elig) || ~isscalar(elig) || ~isfield(elig, 'windows')
            error('GATE:MISSION_V3:EXPORT:ELIG_RECORD', ...
                'elig must be a mission_v3.eligibility ''judge'' output');
        end
    end

    % 5. record/artifact integrity cross-check (FR-536): recompute the
    % window counts from the artifact itself
    w2 = local_windows_from_artifact(meta, kf);
    if ~isempty(elig)
        local_check_elig_consistency(elig, w2);
    end

    % 6. embed + write (MAT v7 = D design deviation D-2 frozen value,
    % consumed from the D contract, not restated)
    meta.eligibility = elig;
    [fdir, ~] = fileparts(fpath);
    if ~isempty(fdir) && ~exist(fdir, 'dir')
        mkdir(fdir);
    end
    save(fpath, 'stream', 'kf', 'meta', EXPORT_CONST_MAT_OPTION());

    % 7. self-check read-back (field-level)
    winfo = whos('-file', fpath);
    names = sort({winfo.name});
    if ~isequal(names, sort({'stream', 'kf', 'meta'}))
        error('GATE:MISSION_V3:EXPORT:ROUNDTRIP', ...
            'file %s does not hold exactly {stream, kf, meta}', fpath);
    end
    s2 = load(fpath);
    if ~isequal(s2.stream, stream) || ~isequal(s2.kf, kf) ...
            || ~isequal(s2.meta, meta)
        error('GATE:MISSION_V3:EXPORT:ROUNDTRIP', ...
            'read-back of %s differs from the assembled artifact', fpath);
    end

    res = struct();
    res.ok = true;
    res.file = fpath;
    res.dataset_domain = sc.dataset_domain;
    res.fixture_kind = sc.fixture_kind;
    res.mat_version = EXPORT_CONST_MAT_OPTION();
    res.n_stream_samples = numel(stream.time_s);
    res.n_kf_rows = numel(kf.issue_time_s);
    res.expected_canonical_windows = w2.expected_canonical_windows;
    res.valid_canonical_windows = w2.valid_canonical_windows;
    res.invalid_canonical_windows = w2.invalid_canonical_windows;
    res.elig_record_embedded = ~isempty(elig);
    res.roundtrip_ok = true;
end

%% ======================= artifact windows ===========================

function [w2, row_ok] = local_windows_from_artifact(meta, kf)
% Recompute the canonical window counts from the artifact: expected from
% meta.voyage_duration_s, valid/invalid from the K/F snapshot coverage
% actually present (DGN-07 strict vintage alignment; a row with
% non-finite content in any of the six columns counts not covered,
% matching the kf_snapshots missing-lead NaN policy).
    m = numel(kf.issue_time_s);
    if m > 0
        six = local_six_columns();
        row_ok = true(m, 1);
        for c = 1:numel(six)
            col = kf.(six{c});
            bad = any(~isfinite(col), 2);
            row_ok = row_ok & ~bad;
        end
    else
        row_ok = logical([]);
    end
    cov = struct('kf_issue_time_s', kf.issue_time_s, 'row_ok', row_ok);
    w2 = mission_v3.eligibility('windows', meta.voyage_duration_s, cov);
end

function local_check_elig_consistency(elig, w2)
    ew = elig.windows;
    need = {'expected_canonical_windows', 'valid_canonical_windows', ...
        'invalid_canonical_windows'};
    for k = 1:numel(need)
        if ~isfield(ew, need{k})
            error('GATE:MISSION_V3:EXPORT:ELIG_RECORD', ...
                'elig.windows lacks %s', need{k});
        end
    end
    if (ew.expected_canonical_windows ~= w2.expected_canonical_windows) ...
            || (ew.valid_canonical_windows ~= w2.valid_canonical_windows) ...
            || (ew.invalid_canonical_windows ...
                ~= w2.invalid_canonical_windows)
        error('GATE:MISSION_V3:EXPORT:ELIG_RECORD', ...
            ['eligibility record (exp=%d val=%d inv=%d) disagrees ', ...
             'with the artifact (exp=%d val=%d inv=%d): FR-536 forbids ', ...
             'shipping a stale/masked qualification record'], ...
            ew.expected_canonical_windows, ...
            ew.valid_canonical_windows, ...
            ew.invalid_canonical_windows, ...
            w2.expected_canonical_windows, ...
            w2.valid_canonical_windows, ...
            w2.invalid_canonical_windows);
    end
end

%% ======================= conformance checks =========================

function local_check_stream(stream, sch)
    if ~isstruct(stream) || ~isscalar(stream)
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'stream must be a scalar struct');
    end
    if ~isfield(stream, 'time_s')
        error('GATE:MISSION_V3:EXPORT:LAYOUT', 'stream lacks time_s');
    end
    ts = stream.time_s;
    if ~isnumeric(ts) || ~isreal(ts) || ~isvector(ts) || isempty(ts) ...
            || ~all(isfinite(ts(:)))
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'stream.time_s must be a non-empty finite numeric vector');
    end
    if any(diff(double(ts(:))) < -EXPORT_CONST_GRID_TOL())
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'stream.time_s must be monotone non-decreasing');
    end
    n = numel(ts);

    if ~isfield(sch, 'capability_families')
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'producer schema lacks capability_families');
    end
    fam = sch.capability_families;
    if ~isfield(fam, 'truth_stream')
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'producer schema lacks capability_families.truth_stream');
    end
    tstr = fam.truth_stream;
    fl = local_fields_list(tstr.fields);
    for i = 1:numel(fl)
        f = fl{i};
        if ~isfield(f, 'required') || ~f.required
            continue;
        end
        if ~isfield(stream, f.name)
            error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
                'stream lacks the schema-required truth_stream field %s', ...
                f.name);
        end
        v = stream.(f.name);
        tyn = f.type;
        if ~isempty(regexp(tyn, '\[N\]\s*$', 'once'))
            % float64[N] / uint8[N] realization-time column
            if ~isnumeric(v) || ~isreal(v) || ~isvector(v) ...
                    || (numel(v) ~= n)
                error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
                    'stream.%s must be a numeric [N] column (N=%d)', ...
                    f.name, n);
            end
            if ~isempty(regexpi(tyn, 'uint', 'once')) ...
                    && any(v(:) ~= round(v(:)))
                error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
                    'stream.%s must be integer-valued (%s)', f.name, tyn);
            end
        elseif ~isempty(strfind(tyn, 'string|int'))
            if ~(ischar(v) || (isstring(v) && (numel(v) == 1))) ...
                    && ~(isnumeric(v) && isscalar(v))
                error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
                    'stream.%s must be a scalar id (%s)', f.name, tyn);
            end
        end
    end
end

function local_check_kf(kf, w0)
% export_pilot/kf_snapshots issue-major freeze; the lead grid is the
% frozen {10,20,...,300} grid (FR-561), never taken from the inputs.
    if ~isstruct(kf) || ~isscalar(kf)
        error('GATE:MISSION_V3:EXPORT:LAYOUT', 'kf must be a scalar struct');
    end
    need = {'issue_time_s', 'lead_s'};
    six = local_six_columns();
    need = [need, six];
    for k = 1:numel(need)
        if ~isfield(kf, need{k})
            error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
                'kf lacks the export_pilot layout field %s', need{k});
        end
    end
    its = kf.issue_time_s;
    m = numel(its);
    if m == 0
        return;   % zero-window voyage legal (FR-472)
    end
    if ~isnumeric(its) || ~isreal(its) || ~all(isfinite(its(:))) ...
            || any(its(:) < 0)
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'kf.issue_time_s must be finite and non-negative');
    end
    if any(diff(double(its(:))) <= 0)
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'kf.issue_time_s must be strictly increasing (anchor grid)');
    end
    leads = (1:w0.n_lead_bins) * w0.lead_bin_s;
    lds = kf.lead_s;
    if ~isnumeric(lds) || ~isequal(size(lds), [m, w0.n_lead_bins])
        error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
            'kf.lead_s must be numeric (%d x %d)', m, w0.n_lead_bins);
    end
    for r = 1:m
        if max(abs(double(lds(r, :)) - leads)) > EXPORT_CONST_GRID_TOL()
            error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
                'kf.lead_s row %d is not the frozen {10,...,300} grid', r);
        end
    end
    for c = 1:numel(six)
        col = kf.(six{c});
        if ~isnumeric(col) || ~isequal(size(col), [m, w0.n_lead_bins])
            error('GATE:MISSION_V3:EXPORT:LAYOUT', ...
                'kf.%s must be numeric (%d x %d)', six{c}, m, ...
                w0.n_lead_bins);
        end
    end
end

function local_check_meta(meta, sch)
    if ~isfield(sch, 'capability_families')
        error('GATE:MISSION_V3:EXPORT:META', ...
            'producer schema lacks capability_families');
    end
    fam = sch.capability_families;

    % presence: schema families + static face + export_pilot layout +
    % C-D1 extensions (design DGN-15 / delegation spec FD5)
    req = {};
    req = local_collect_required(req, fam, 'voyage_metadata');
    req = local_collect_required(req, fam, 'generation_provenance');
    req = local_collect_required(req, fam, 'ood_config_surface');
    req{end+1} = 'speed_limit_zones';    % static_constraints face,
    req{end+1} = 'bridge_zones';         % empty sets legal (DGN-13)
    req{end+1} = 'voyage_duration_s';    % export_pilot layout
    req{end+1} = 'scenario_id';
    req{end+1} = 'base_draw_id';         % C-D1 extensions (DGN-15/FD5)
    req{end+1} = 'intervention';
    req{end+1} = 'forecast_quality_profile_id';
    req{end+1} = 'stratum';
    req{end+1} = 'dataset_domain';
    for k = 1:numel(req)
        if ~isfield(meta, req{k})
            error('GATE:MISSION_V3:EXPORT:META', ...
                'meta lacks the required field %s', req{k});
        end
    end

    % forecast_provenance family struct with its schema fields
    if ~isfield(meta, 'forecast_provenance') ...
            || ~isstruct(meta.forecast_provenance) ...
            || ~isscalar(meta.forecast_provenance)
        error('GATE:MISSION_V3:EXPORT:META', ...
            'meta.forecast_provenance must be a scalar struct (schema family)');
    end
    prov = meta.forecast_provenance;
    pfl = local_fields_list(fam.forecast_provenance.fields);
    for k = 1:numel(pfl)
        f = pfl{k};
        if isfield(f, 'required') && f.required ...
                && ~isfield(prov, f.name)
            error('GATE:MISSION_V3:EXPORT:META', ...
                'meta.forecast_provenance lacks the schema field %s', ...
                f.name);
        end
    end

    % value domains (machine schema enums)
    dom = local_as_char(meta.dataset_domain, 'meta.dataset_domain');
    if ~any(strcmp(dom, {'FORMAL', 'PILOT'}))
        error('GATE:MISSION_V3:EXPORT:META', ...
            'meta.dataset_domain must be FORMAL or PILOT (got %s)', dom);
    end
    fk = local_as_char(meta.fixture_kind, 'meta.fixture_kind');
    if ~any(strcmp(fk, {'NONE', 'PILOT_ONLY'}))
        error('GATE:MISSION_V3:EXPORT:META', ...
            ['meta.fixture_kind must be NONE or PILOT_ONLY per the ', ...
             'producer schema (got %s)'], fk);
    end

    % light types: schema string-typed fields must be non-empty chars
    local_check_char_fields(meta, fam, 'voyage_metadata');
    local_check_char_fields(meta, fam, 'generation_provenance');
    local_check_char_fields(meta, fam, 'ood_config_surface');
    cd1_chars = {'base_draw_id', 'forecast_quality_profile_id', ...
        'stratum', 'scenario_id'};
    for k = 1:numel(cd1_chars)
        v = meta.(cd1_chars{k});
        if ~(ischar(v) || (isstring(v) && (numel(v) == 1))) ...
                || isempty(char(v))
            error('GATE:MISSION_V3:EXPORT:META', ...
                'meta.%s must be a non-empty string', cd1_chars{k});
        end
    end
    vd = meta.voyage_duration_s;
    if ~isnumeric(vd) || ~isscalar(vd) || ~isreal(vd) || ~isfinite(vd) ...
            || (double(vd) <= 0)
        error('GATE:MISSION_V3:EXPORT:META', ...
            'meta.voyage_duration_s must be a finite positive scalar');
    end
    gs = meta.generation_seed;
    if ~isnumeric(gs) || ~isscalar(gs) || ~isreal(gs) || ~isfinite(gs) ...
            || (double(gs) ~= floor(double(gs)))
        error('GATE:MISSION_V3:EXPORT:META', ...
            'meta.generation_seed must be an integer scalar');
    end
end

function req = local_collect_required(req, fam, family_name)
    if ~isfield(fam, family_name)
        error('GATE:MISSION_V3:EXPORT:META', ...
            'producer schema lacks capability_families.%s', family_name);
    end
    fam_i = fam.(family_name);
    fl = local_fields_list(fam_i.fields);
    for k = 1:numel(fl)
        f = fl{k};
        if isfield(f, 'required') && f.required
            req{end+1} = f.name; %#ok<AGROW>
        end
    end
end

function local_check_char_fields(meta, fam, family_name)
    fam_i = fam.(family_name);
    fl = local_fields_list(fam_i.fields);
    for k = 1:numel(fl)
        f = fl{k};
        if ~isfield(f, 'required') || ~f.required
            continue;
        end
        if isfield(f, 'type') && ~isempty(strfind(f.type, 'string'))
            v = meta.(f.name);
            if ~(ischar(v) || (isstring(v) && (numel(v) == 1))) ...
                    || isempty(char(v))
                error('GATE:MISSION_V3:EXPORT:META', ...
                    'meta.%s must be a non-empty string (schema family %s)', ...
                    f.name, family_name);
            end
        end
    end
end

%% ========================== small helpers ===========================

function six = local_six_columns()
% export_pilot/kf_snapshots issue-major content columns
    six = {'v_plan_mps', 's_plan_m', 'h_chart_m', 'P_hotel_plan_kW', ...
        'u_fcst_mps', 'wl_fcst_m'};
end

function fl = local_fields_list(v)
% jsondecode list handling (export_pilot.m precedent: homogeneous JSON
% object arrays decode to struct arrays, heterogeneous to cells).
    if isstruct(v)
        n = numel(v);
        fl = cell(1, n);
        for k = 1:n
            fl{k} = v(k);
        end
    elseif iscell(v)
        fl = v(:)';
    else
        error('GATE:MISSION_V3:EXPORT:META', ...
            'expected a JSON array of field objects in the producer schema');
    end
end

function obj = local_read_json(ppath)
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:EXPORT:META', ...
            'frozen file not found: %s', ppath);
    end
    obj = jsondecode(fileread(ppath));
end

function root = local_root()
    root = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(root) || ~exist(fullfile(root, 'config'), 'dir')
        root = pwd;
    end
end

function s = local_as_char(v, what)
    if ischar(v)
        s = v;
    elseif isstring(v) && (numel(v) == 1)
        s = char(v);
    else
        error('GATE:MISSION_V3:EXPORT:BAD_ARG', ...
            '%s must be a char/string scalar', what);
    end
end

%% ==================== v1 constants (provenance) =====================

function c = EXPORT_CONST_MAT_OPTION()
% D design deviation D-2 frozen value (tasks.md T-015): the artifact
% format is CONSUMED from the D producer contract, not restated or
% changed at this layer.  Layout reference: +dataset_v3/export_pilot.m
% `save(fmat, 'stream', 'kf', 'meta', '-v7')`.
    c = '-v7';
end

function c = EXPORT_CONST_GRID_TOL()
% Float tolerance for grid/time monotonicity checks (env layer
% fixed-decision-4 precedent magnitude).
    c = 1e-6;
end

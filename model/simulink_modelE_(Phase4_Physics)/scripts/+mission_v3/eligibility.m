function out = eligibility(mode, varargin)
% mission_v3.eligibility - T-015 voyage qualification judge, canonical
% window arithmetic, six-class failure taxonomy, FAILED-slot retry rows
% (FR-535~540; design DGN-10/R8; delegation spec
% experiments/mission_v3/T-015_delegation_spec.md Fixed Decisions FD1-FD4).
%
% Pure judge over caller-supplied voyage facts: NO simulation, NO batch
% generation, NO RNG, no file writes (the frozen schema/registry JSONs
% are read only).  The pre-qualification main loop is T-020 (out of
% scope here).
%
% MODES
%   w = mission_v3.eligibility('windows', duration_s [, coverage])
%   j = mission_v3.eligibility('judge', voy [, 'MinDurationS', v, ...
%           'MinCanonicalWindows', v])
%   t = mission_v3.eligibility('taxonomy')
%   r = mission_v3.eligibility('retry_record', slot_id, attempt, ...
%           disposition, retry_of)
%
% 'windows' (FD1, pure arithmetic; every constant is READ from the frozen
% machine files and cross-checked, never hard-coded):
%   Window arithmetic derivation (contract-conforming, verified against
%   the tasks.md T-015 acceptance anchor 1590 s -> 100 windows):
%     - canonical window span = 600 s = 300 s history + 300 s future
%       (docs/phase4/d_dataset_v3/c_producer_interface_contract.md
%       section 3: "600 s is only the geometric minimal history+future
%       span (300+300)"; registry bin_semantics history bins j=300..1)
%     - future span = n_lead * lead_bin = 30 * 10 s = 300 s
%       (registry bin_semantics "F_k=(t0+(k-1)*10, t0+k*10], k=1..30";
%       producer schema input_k_snapshots.lead_s shape float64[M x 30])
%     - anchor stride = 10 s = the future bin width (producer schema
%       planned_future_trajectory 30x10 s K/F grid; issue_time_s = the
%       anchor t0 candidate grid; D-side design DGN-07: t0 in
%       {t_start+300, t_start+310, ...} <= t_end-300; +dataset_v3/
%       export_pilot.m anchors = (300:10:(t_end-300)))
%     - N(D) = floor((D - 600)/10) + 1 for D >= 600 s, else 0
%       (requirements.md FR-535 "N(D)=floor((D-600)/10)+1"; complete
%       windows only, trailing partial window NOT counted, DGN-07
%       "dropped, no padding")
%     - anchor verification (FD1): N(1590) = floor(990/10) + 1 = 100 =
%       tasks.md/brainstorm.md anchor table (1200->61, 1500->91,
%       1590->100, 1800->121).  The contract derivation AGREES with the
%       1590->100 acceptance anchor; no conflict, no escalation.
%   Optional coverage struct: kf_issue_time_s (M x 1 snapshot issue
%   times) + optional row_ok (M x 1 logical, default all true).  A
%   window is invalid when its anchor t0 has no strictly vintage-aligned
%   K/F snapshot row, or that row is not fully covered (DGN-07 strict
%   alignment; FR-562 missing snapshot = producer data-quality defect).
%   Without coverage the expected count is returned unverified
%   (coverage_assessed = false); 'judge' applies the fail-closed policy.
%
% 'judge' (FD2/DGN-10 frozen candidate criteria, three-condition AND):
%   min_duration_s = 1200  (exact boundary: 1199.99 rejects, 1200 accepts)
%   min_canonical_windows = 60
%   unexpected_invalid_canonical_windows = 0
%   The producer schema formal_voyage_qualification deliberately does NOT
%   pre-fill activation values (contract section 3); these defaults are
%   the frozen Phase 4-C candidates (FD2) and may be overridden per call
%   by name-values under the FR-540 activation discipline (T-020 must
%   inject activation-record values only).
%   voy required field: duration_s.
%   voy optional fields:
%     kf_issue_time_s     M x 1 K/F snapshot issue times (coverage)
%     kf_row_ok           M x 1 logical (row fully covered, default true)
%     heldout_token_id    non-empty triggers REJECTED_STRUCTURAL_HELDOUT
%     generation_failed   logical; runtime generation/export failure
%     generation_fail_reason   free-text detail (goes to j.detail)
%     contract_defects    scalar struct with logical flags
%                         missing_schema_fields / missing_forecast_provenance /
%                         missing_stream_data (FR-536 defect classes)
%     physical_domain_bound         pre-acceptance sampling rejection
%     physical_domain_violation     unexpected post-hoc domain failure
%     physical_domain_violation_reason  free-text detail
%   Judgment precedence (documented; env_realize judgment-order
%   precedent; FAILED_* are slot failures evaluated before the normal
%   sampling events; FR-536 defects are never masked by window counts):
%     1 FAILED_GENERATION                      (runtime failure)
%     2 FAILED_CONTRACT                        (defect flags, K/F
%                                               coverage not supplied
%                                               while expected > 0, or
%                                               invalid windows > 0)
%     3 FAILED_PHYSICAL_DOMAIN                 (unexpected post-hoc)
%     4 REJECTED_SAMPLING_ELIGIBILITY /
%       PHYSICAL_DOMAIN_BOUND                  (env_realize order 1)
%     5 REJECTED_SAMPLING_ELIGIBILITY /
%       DURATION_BELOW_MIN                     (FR-535/538)
%     6 REJECTED_SAMPLING_ELIGIBILITY /
%       SUPPORT_BOUND                          (expected windows below
%                                               the frozen minimum)
%     7 REJECTED_STRUCTURAL_HELDOUT(token_id)  (env_realize order 4)
%     8 ACCEPT
%
% 'taxonomy' (FD3; structure follows the T-009 env-layer enum
% (+mission_v3/env_realize.m local_disposition_enum / local_reason_enum),
% extended to voyage level):
%   disposition_enum = {ACCEPT, REJECTED_SAMPLING_ELIGIBILITY,
%     REJECTED_STRUCTURAL_HELDOUT, FAILED_CONTRACT, FAILED_GENERATION,
%     FAILED_PHYSICAL_DOMAIN}
%   reason_enum = {SUPPORT_BOUND, PHYSICAL_DOMAIN_BOUND,
%     DURATION_BELOW_MIN, none}
%   plus per-class slot-failure / retry discrimination (FD4):
%     FAILED_*     slot failure, retry allowed, policy
%                  SAME_SLOT_SAME_CONFIG_SAME_SCIENCE_SEED (FR-537/R8:
%                  never a fresh unrelated seed)
%     REJECTED_*   normal sampling/structural event, never occupies a
%                  slot, never produces a retry row; resampling follows
%                  the pre-registered policy (T-020 main loop)
%     ACCEPT       occupies the slot, no retry
%
% 'retry_record' (FD4, FR-537/R8): builds one FAILED-slot retry record
% row {record_kind, slot_id, attempt, disposition, retry_of, seed_policy,
% provenance}.  Calling with a REJECTED_*/ACCEPT disposition raises
% GATE:MISSION_V3:ELIGIBILITY:RETRY (REJECTED_* never occupy a slot).
% The T-020 main loop consumes this record structure.
%
% GATE ERROR IDS: GATE:MISSION_V3:ELIGIBILITY:{BAD_ARG, REGISTRY,
% TAXONOMY, RETRY}.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% ROOT resolved from mfilename with pwd fallback; pure function, zero
% randomness; schema/registry constants read from the frozen JSON files
% (never hard-coded).

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'mode must be a char vector');
    end
    switch char(mode)
        case 'windows'
            if nargin < 2
                error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                    '''windows'' expects: duration_s [, coverage]');
            end
            cov = [];
            if nargin >= 3
                cov = varargin{2};
            end
            out = local_windows(varargin{1}, cov);
        case 'judge'
            if nargin < 2
                error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                    '''judge'' expects: voy [, name-values]');
            end
            out = local_judge(varargin{1}, ...
                local_parse_named(varargin(2:end)));
        case 'taxonomy'
            out = local_taxonomy();
        case 'retry_record'
            if nargin < 5
                error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                    ['''retry_record'' expects: slot_id, attempt, ', ...
                     'disposition, retry_of']);
            end
            out = local_retry_record(varargin{1}, varargin{2}, ...
                varargin{3}, varargin{4});
        otherwise
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                ['unknown mode: %s (eligibility implements ''windows'', ', ...
                 '''judge'', ''taxonomy'', ''retry_record'')'], char(mode));
    end
end

%% ======================= window arithmetic ==========================

function w = local_windows(duration_s, cov)
    cst = local_constants();
    if ~isnumeric(duration_s) || ~isscalar(duration_s) ...
            || ~isreal(duration_s) || ~isfinite(duration_s) ...
            || (double(duration_s) < 0)
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'duration_s must be a finite non-negative scalar');
    end
    d = double(duration_s);
    n = 0;
    first = NaN;
    last = NaN;
    if d >= cst.window_span_s
        n = floor((d - cst.window_span_s) / cst.anchor_stride_s) + 1;
        first = cst.history_span_s;
        last = first + (n - 1) * cst.anchor_stride_s;
    end
    w = struct();
    w.duration_s = d;
    w.window_span_s = cst.window_span_s;
    w.history_span_s = cst.history_span_s;
    w.future_span_s = cst.future_span_s;
    w.lead_bin_s = cst.lead_bin_s;
    w.n_lead_bins = cst.n_lead_bins;
    w.anchor_stride_s = cst.anchor_stride_s;
    w.anchor_first_s = first;
    w.anchor_last_s = last;
    w.expected_canonical_windows = n;
    w.valid_canonical_windows = n;
    w.invalid_canonical_windows = 0;
    w.invalid_reason_counts = struct();
    w.coverage_assessed = false;
    if (nargin >= 2) && ~isempty(cov)
        w.valid_canonical_windows = local_apply_coverage_valid(w, cov);
        w.invalid_canonical_windows = ...
            (w.expected_canonical_windows - w.valid_canonical_windows);
        w.invalid_reason_counts = struct('MISSING_KF_SNAPSHOT', ...
            w.invalid_canonical_windows);
        w.coverage_assessed = true;
    end
    w.provenance = local_window_provenance(cst);
end

function valid = local_apply_coverage_valid(w, cov)
% DGN-07 strict vintage alignment: an anchor counts valid only when a
% K/F snapshot row exists at that exact issue time AND that row is fully
% covered (row_ok).  Missing/not-covered anchors are invalid windows.
    if ~isstruct(cov) || ~isscalar(cov) || ~isfield(cov, 'kf_issue_time_s')
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'coverage must be a scalar struct with field kf_issue_time_s');
    end
    kit = cov.kf_issue_time_s;
    if ~isnumeric(kit) || ~isreal(kit) || ~isvector(kit) ...
            || ~all(isfinite(kit(:)))
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'coverage.kf_issue_time_s must be a finite numeric vector');
    end
    kit = double(kit(:));
    m = numel(kit);
    row_ok = true(m, 1);
    if isfield(cov, 'row_ok')
        rk = cov.row_ok;
        if ~islogical(rk) && ~isnumeric(rk)
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'coverage.row_ok must be a logical/numeric vector');
        end
        rk = logical(rk(:));
        if (numel(rk) ~= m)
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'coverage.row_ok must align with kf_issue_time_s');
        end
        row_ok = rk;
    end
    tol = ELIG_CONST_TOL();
    valid = w.expected_canonical_windows;
    for a = 1:w.expected_canonical_windows
        t0 = w.anchor_first_s + (a - 1) * w.anchor_stride_s;
        hit = find(abs(kit - t0) <= tol, 1);
        if isempty(hit) || ~row_ok(hit)
            valid = valid - 1;
        end
    end
end

%% ============================ judge =================================

function j = local_judge(voy, opts)
    if ~isstruct(voy) || ~isscalar(voy)
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'voy must be a scalar struct');
    end
    if ~isfield(voy, 'duration_s')
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'voy lacks the required field duration_s');
    end

    cov = [];
    if isfield(voy, 'kf_issue_time_s')
        cov = struct('kf_issue_time_s', voy.kf_issue_time_s);
        if isfield(voy, 'kf_row_ok')
            cov.row_ok = voy.kf_row_ok;
        end
    end
    w = local_windows(voy.duration_s, cov);

    dflags = struct('missing_schema_fields', false, ...
        'missing_forecast_provenance', false, 'missing_stream_data', false);
    if isfield(voy, 'contract_defects') && ~isempty(voy.contract_defects)
        cd = voy.contract_defects;
        if ~isstruct(cd) || ~isscalar(cd)
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'voy.contract_defects must be a scalar struct');
        end
        dflags = local_merge_defect_flags(dflags, cd);
    end

    gen_fail = false;
    if isfield(voy, 'generation_failed')
        gen_fail = local_as_logical(voy.generation_failed, ...
            'voy.generation_failed');
    end
    gen_detail = 'none';
    if isfield(voy, 'generation_fail_reason') ...
            && ~isempty(voy.generation_fail_reason)
        gen_detail = local_as_char(voy.generation_fail_reason, ...
            'voy.generation_fail_reason');
    end
    dom_bound = false;
    if isfield(voy, 'physical_domain_bound')
        dom_bound = local_as_logical(voy.physical_domain_bound, ...
            'voy.physical_domain_bound');
    end
    dom_viol = false;
    if isfield(voy, 'physical_domain_violation')
        dom_viol = local_as_logical(voy.physical_domain_violation, ...
            'voy.physical_domain_violation');
    end
    viol_detail = 'none';
    if isfield(voy, 'physical_domain_violation_reason') ...
            && ~isempty(voy.physical_domain_violation_reason)
        viol_detail = local_as_char(voy.physical_domain_violation_reason, ...
            'voy.physical_domain_violation_reason');
    end
    token = '';
    if isfield(voy, 'heldout_token_id') ...
            && ~isempty(voy.heldout_token_id)
        token = local_as_char(voy.heldout_token_id, 'voy.heldout_token_id');
    end

    % FR-536: producer data-quality defects (missing schema fields /
    % provenance / stream data; K/F coverage not supplied while windows
    % are expected; invalid windows) are contract failures and are NEVER
    % masked by remaining window counts (FR-537/FR-562).
    defect_sources = {};
    if dflags.missing_schema_fields
        defect_sources{end+1} = 'MISSING_SCHEMA_FIELDS'; %#ok<AGROW>
    end
    if dflags.missing_forecast_provenance
        defect_sources{end+1} = 'MISSING_FORECAST_PROVENANCE'; %#ok<AGROW>
    end
    if dflags.missing_stream_data
        defect_sources{end+1} = 'MISSING_STREAM_DATA'; %#ok<AGROW>
    end
    if (w.expected_canonical_windows > 0) && ~w.coverage_assessed
        defect_sources{end+1} = 'KF_COVERAGE_NOT_SUPPLIED'; %#ok<AGROW>
    end
    if w.invalid_canonical_windows > 0
        defect_sources{end+1} = 'INVALID_WINDOWS'; %#ok<AGROW>
    end
    defect = ~isempty(defect_sources);
    detail = 'none';
    if defect
        detail = local_join(defect_sources, '+');
    end

    % ---- judgment precedence (documented in the header) ----
    tax = local_taxonomy();
    disp_name = '';
    reason = 'none';
    token_id = '';
    if gen_fail
        disp_name = 'FAILED_GENERATION';
        reason = 'none';
        detail = gen_detail;
    elseif defect
        disp_name = 'FAILED_CONTRACT';
        reason = 'none';
    elseif dom_viol
        disp_name = 'FAILED_PHYSICAL_DOMAIN';
        reason = 'none';
        detail = viol_detail;
    elseif dom_bound
        disp_name = 'REJECTED_SAMPLING_ELIGIBILITY';
        reason = 'PHYSICAL_DOMAIN_BOUND';
    elseif ~(w.duration_s >= opts.min_duration_s)
        disp_name = 'REJECTED_SAMPLING_ELIGIBILITY';
        reason = 'DURATION_BELOW_MIN';
    elseif ~(w.expected_canonical_windows >= opts.min_canonical_windows)
        disp_name = 'REJECTED_SAMPLING_ELIGIBILITY';
        reason = 'SUPPORT_BOUND';
    elseif ~isempty(token)
        disp_name = 'REJECTED_STRUCTURAL_HELDOUT';
        reason = 'none';
        token_id = token;
    else
        disp_name = 'ACCEPT';
        reason = 'none';
    end
    idx = find(strcmp(tax.disposition_enum, disp_name), 1);
    if isempty(idx) || ~ismember(reason, tax.reason_enum)
        error('GATE:MISSION_V3:ELIGIBILITY:TAXONOMY', ...
            'disposition/reason outside the frozen six-class taxonomy');
    end

    duration_ok = w.duration_s >= opts.min_duration_s;
    windows_ok = w.expected_canonical_windows >= opts.min_canonical_windows;
    invalid_ok = (w.invalid_canonical_windows == ELIG_CONST_INVALID_MAX()) ...
        && ~defect;
    accepted = strcmp(disp_name, 'ACCEPT');

    j = struct();
    j.disposition = disp_name;
    j.reason = reason;
    j.detail = detail;
    j.token_id = token_id;
    j.criteria = struct('duration_ok', duration_ok, ...
        'windows_ok', windows_ok, 'invalid_ok', invalid_ok, ...
        'accepted', accepted);
    j.windows = w;
    j.criteria_values = struct('min_duration_s', opts.min_duration_s, ...
        'min_canonical_windows', opts.min_canonical_windows, ...
        'unexpected_invalid_max', ELIG_CONST_INVALID_MAX());
    j.defect_flags = dflags;
    j.retry = struct('allowed', tax.retry_allowed(idx), ...
        'policy', tax.retry_policy{idx}, 'record', []);
    j.provenance = struct( ...
        'criteria_source', ['FD2/DGN-10/FR-535 frozen candidates: ', ...
         'min_duration_s=1200 AND min_canonical_windows=60 AND ', ...
         'unexpected_invalid=0; three-condition AND; the producer ', ...
         'schema formal_voyage_qualification does not pre-fill ', ...
         'activation values (FR-540 discipline); name-value ', ...
         'overrides exist for activation-record injection only'], ...
        'precedence', ['FAILED_GENERATION > FAILED_CONTRACT > ', ...
         'FAILED_PHYSICAL_DOMAIN > REJECTED_SAMPLING_ELIGIBILITY ', ...
         '(PHYSICAL_DOMAIN_BOUND > DURATION_BELOW_MIN > ', ...
         'SUPPORT_BOUND) > REJECTED_STRUCTURAL_HELDOUT > ACCEPT'], ...
        'schema_self_hash', w.provenance.schema_self_hash, ...
        'registry_self_hash', w.provenance.registry_self_hash);
end

function dflags = local_merge_defect_flags(dflags, cd)
    names = fieldnames(dflags);
    for k = 1:numel(names)
        nm = names{k};
        if isfield(cd, nm) && ~isempty(cd.(nm))
            dflags.(nm) = local_as_logical(cd.(nm), ...
                ['voy.contract_defects.' nm]);
        end
    end
    extra = fieldnames(cd);
    allowed = names;
    for k = 1:numel(extra)
        if ~any(strcmp(allowed, extra{k}))
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'unknown contract_defects flag: %s', extra{k});
        end
    end
end

%% =========================== taxonomy ===============================

function t = local_taxonomy()
% FD3: structure follows the T-009 env-layer enum (+mission_v3/
% env_realize.m local_disposition_enum / local_reason_enum), extended to
% voyage level.  FD4 discrimination: FAILED_* = slot failure with
% same-seed retry; REJECTED_* = normal event, no slot, no retry row.
    t = struct();
    t.disposition_enum = {'ACCEPT', 'REJECTED_SAMPLING_ELIGIBILITY', ...
        'REJECTED_STRUCTURAL_HELDOUT', 'FAILED_CONTRACT', ...
        'FAILED_GENERATION', 'FAILED_PHYSICAL_DOMAIN'};
    t.reason_enum = {'SUPPORT_BOUND', 'PHYSICAL_DOMAIN_BOUND', ...
        'DURATION_BELOW_MIN', 'none'};
    n = numel(t.disposition_enum);
    t.slot_failure = false(1, n);
    t.retry_allowed = false(1, n);
    t.occupies_slot = true(1, n);
    t.retry_policy = cell(1, n);
    for k = 1:n
        d = t.disposition_enum{k};
        if strncmp(d, 'FAILED_', 7)
            t.slot_failure(k) = true;
            t.retry_allowed(k) = true;
            t.retry_policy{k} = 'SAME_SLOT_SAME_CONFIG_SAME_SCIENCE_SEED';
        elseif strncmp(d, 'REJECTED_', 9)
            t.occupies_slot(k) = false;
            t.retry_policy{k} = 'PREREGISTERED_RESAMPLE';
        else
            t.retry_policy{k} = 'NONE';
        end
    end
    t.provenance = struct('source', ...
        ['FR-537 failure/exclusion taxonomy; design DGN-10 + R8; ', ...
         'delegation spec T-015 FD3/FD4; enum structure follows ', ...
         'env_realize.m (T-009 env layer)'], ...
        'retry_note', ['FR-537: contract/generation/unexpected-domain ', ...
         'failures never silently substitute an unrelated new seed; ', ...
         'formal retry uses the same slot/config/science-seed policy; ', ...
         'expected constrained-sampling rejection is not a defect']);
end

%% ========================= retry_record =============================

function r = local_retry_record(slot_id, attempt, disposition, retry_of)
    tax = local_taxonomy();
    sid = local_as_char(slot_id, 'slot_id');
    if isempty(sid)
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'slot_id must be a non-empty string');
    end
    if ~isnumeric(attempt) || ~isscalar(attempt) || ~isfinite(attempt) ...
            || (double(attempt) < 1) ...
            || (double(attempt) ~= floor(double(attempt)))
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'attempt must be a whole number >= 1');
    end
    dname = local_as_char(disposition, 'disposition');
    idx = find(strcmp(tax.disposition_enum, dname), 1);
    if isempty(idx)
        error('GATE:MISSION_V3:ELIGIBILITY:TAXONOMY', ...
            'unknown disposition: %s', dname);
    end
    if ~tax.retry_allowed(idx)
        error('GATE:MISSION_V3:ELIGIBILITY:RETRY', ...
            ['%s never produces a retry row (FD4/FR-537: REJECTED_* ', ...
             'are normal sampling/structural events that do not occupy ', ...
             'a slot; resampling follows the pre-registered T-020 ', ...
             'policy; ACCEPT needs no retry)'], dname);
    end
    if isnumeric(retry_of)
        if ~isscalar(retry_of) || ~isfinite(retry_of) ...
                || (double(retry_of) < 0)
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'retry_of must be a whole number >= 0 or a record id');
        end
        r_of = double(retry_of);
    else
        r_of = local_as_char(retry_of, 'retry_of');
        if isempty(r_of)
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'retry_of must be a whole number >= 0 or a record id');
        end
    end
    r = struct();
    r.record_kind = 'RETRY';
    r.slot_id = sid;
    r.attempt = double(attempt);
    r.disposition = dname;
    r.retry_of = r_of;
    r.seed_policy = 'SAME_SLOT_SAME_CONFIG_SAME_SCIENCE_SEED';
    r.provenance = struct('source', ...
        ['FR-537/R8; delegation spec T-015 FD4; record structure only - ', ...
         'the retry execution and the pre-registered resampling policy ', ...
         'for REJECTED_* belong to the T-020 main loop'], ...
        'slot_semantics', ['REJECTED_* never occupy a slot and never ', ...
         'produce retry rows; ACCEPT occupies the slot without retry']);
end

%% ====================== frozen constant reads =======================
% All window-geometry constants are parsed from the frozen machine files
% and cross-checked (FD8: schema/registry constants read, never
% hard-coded).  The qualification thresholds are the FD2 frozen
% candidates (the D schema deliberately does not pre-fill activation
% values; contract section 3).

function cst = local_constants()
    root = local_root();
    reg = local_read_json(fullfile(root, 'config', ...
        'dataset_v3_signal_registry.json'));
    sch = local_read_json(fullfile(root, 'config', ...
        'dataset_v3_producer_schema.json'));
    if ~isfield(reg, 'bin_semantics') || ~ischar(reg.bin_semantics)
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'signal registry lacks bin_semantics');
    end
    bs = reg.bin_semantics;
    t_hist = regexp(bs, 'j=(\d+)\.\.1', 'tokens');
    t_nlead = regexp(bs, 'k=1\.\.(\d+)', 'tokens');
    t_bin = regexp(bs, '\(k-1\)\*(\d+)', 'tokens');
    if isempty(t_hist) || isempty(t_nlead) || isempty(t_bin)
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'cannot parse the history/future bin geometry from registry bin_semantics');
    end
    hist_s = str2double(t_hist{1}{1});
    n_lead = str2double(t_nlead{1}{1});
    bin_s = str2double(t_bin{1}{1});
    if ~isfinite(hist_s) || ~isfinite(n_lead) || ~isfinite(bin_s) ...
            || (hist_s <= 0) || (n_lead <= 0) || (bin_s <= 0)
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'registry bin_semantics parsed to non-positive geometry');
    end

    if ~isfield(sch, 'capability_families')
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'producer schema lacks capability_families');
    end
    fam = sch.capability_families;
    if ~isfield(fam, 'input_k_snapshots')
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'producer schema lacks capability_families.input_k_snapshots');
    end
    ksnaps = fam.input_k_snapshots;
    fl = local_fields_list(ksnaps.fields);
    n_shape = NaN;
    for k = 1:numel(fl)
        f = fl{k};
        if isfield(f, 'name') && strcmp(f.name, 'lead_s')
            tt = regexp(f.type, '(\d+)\]\s*$', 'tokens');
            if isempty(tt)
                error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
                    'cannot parse the lead_s shape from the producer schema');
            end
            n_shape = str2double(tt{1}{1});
        end
    end
    if isnan(n_shape) || (n_shape ~= n_lead)
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'schema lead_s shape (%g) disagrees with registry bins (k=1..%g)', ...
            n_shape, n_lead);
    end
    if ~isfield(fam, 'planned_future_trajectory')
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'producer schema lacks planned_future_trajectory (30x10 s K/F grid)');
    end

    cst = struct();
    cst.history_span_s = hist_s;
    cst.n_lead_bins = n_lead;
    cst.lead_bin_s = bin_s;
    cst.future_span_s = n_lead * bin_s;
    cst.window_span_s = cst.history_span_s + cst.future_span_s;
    % The anchor t0 candidate grid stride equals the future bin width
    % (schema issue_time_s = anchor t0 grid; DGN-07 stride 10 s;
    % export_pilot anchors = (300:10:(t_end-300))).
    cst.anchor_stride_s = bin_s;
    cst.schema_self_hash = local_self_hash(sch);
    cst.registry_self_hash = local_self_hash(reg);
end

function h = local_self_hash(obj)
    h = 'UNKNOWN';
    if isfield(obj, 'self_hash') && isfield(obj.self_hash, 'value')
        h = char(obj.self_hash.value);
    end
end

function prov = local_window_provenance(cst)
    prov = struct();
    prov.formula = ['N(D) = floor((D - 600)/10) + 1 for D >= 600 s; ', ...
        '0 otherwise (FR-535; complete windows only, trailing partial ', ...
        'window not counted, no padding per DGN-07)'];
    prov.span_source = ['window span = 300 s history + 300 s future = ', ...
        '600 s (c_producer_interface_contract.md section 3; registry ', ...
        'bin_semantics j=300..1 / k=1..30)'];
    proj = sprintf('%d x %g s', cst.n_lead_bins, cst.lead_bin_s);
    prov.grid_source = ['future grid = ', proj, ...
        ' (registry bin_semantics (k-1)*10; producer schema ', ...
        'input_k_snapshots.lead_s shape and planned_future_trajectory ', ...
        'K/F grid; cross-checked equal)'];
    prov.stride_source = ['anchor stride = future bin width = ', ...
        '10 s (schema issue_time_s anchor t0 grid; DGN-07; ', ...
        'export_pilot anchors = (300:10:(t_end-300)))'];
    prov.anchor_check = ['FD1 anchor verified: N(1590) = floor(990/10) ', ...
        '+ 1 = 100 (tasks.md T-015); contract derivation agrees with ', ...
        'the anchor'];
    prov.schema_self_hash = cst.schema_self_hash;
    prov.registry_self_hash = cst.registry_self_hash;
end

%% ======================== name-value parse ==========================

function opts = local_parse_named(arg_cell)
    opts = struct('min_duration_s', ELIG_CONST_MIN_DURATION_S(), ...
        'min_canonical_windows', ELIG_CONST_MIN_WINDOWS());
    if mod(numel(arg_cell), 2) ~= 0
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            'missing value for option %s', local_opt_name(arg_cell));
    end
    k = 1;
    while k <= numel(arg_cell)
        opt = arg_cell{k};
        val = arg_cell{k + 1};
        if ~ischar(opt) && ~isstring(opt)
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'option name must be a char vector');
        end
        if strcmpi(opt, 'MinDurationS')
            if ~isnumeric(val) || ~isscalar(val) || ~isreal(val) ...
                    || ~isfinite(val) || (double(val) <= 0)
                error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                    'MinDurationS must be a finite positive scalar');
            end
            opts.min_duration_s = double(val);
        elseif strcmpi(opt, 'MinCanonicalWindows')
            if ~isnumeric(val) || ~isscalar(val) || ~isreal(val) ...
                    || ~isfinite(val) || (double(val) <= 0) ...
                    || (double(val) ~= floor(double(val)))
                error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                    'MinCanonicalWindows must be a whole number >= 1');
            end
            opts.min_canonical_windows = double(val);
        else
            error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
                'unknown option: %s (accepted: MinDurationS, MinCanonicalWindows)', ...
                char(opt));
        end
        k = k + 2;
    end
end

function nm = local_opt_name(arg_cell)
    nm = 'UNKNOWN';
    if ~isempty(arg_cell) && (ischar(arg_cell{1}) || isstring(arg_cell{1}))
        nm = char(arg_cell{1});
    end
end

%% ========================== small helpers ===========================

function fl = local_fields_list(v)
% jsondecode list handling (export_pilot.m local_unique_logsout_sources
% precedent: JSON object arrays decode to struct arrays when homogeneous
% and to cell arrays otherwise).
    if isstruct(v)
        n = numel(v);
        fl = cell(1, n);
        for k = 1:n
            fl{k} = v(k);
        end
    elseif iscell(v)
        fl = v(:)';
    else
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
            'expected a JSON array of field objects');
    end
end

function obj = local_read_json(ppath)
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:ELIGIBILITY:REGISTRY', ...
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
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            '%s must be a char/string scalar', what);
    end
end

function b = local_as_logical(v, what)
    if islogical(v) && isscalar(v)
        b = v;
    elseif isnumeric(v) && isscalar(v) && isreal(v) && ~isnan(v)
        b = (v ~= 0);
    else
        error('GATE:MISSION_V3:ELIGIBILITY:BAD_ARG', ...
            '%s must be a scalar logical', what);
    end
end

function out = local_join(parts, sep)
    out = '';
    for k = 1:numel(parts)
        if k > 1
            out = [out, sep];
        end
        out = [out, parts{k}];
    end
end

%% ==================== v1 constants (provenance) =====================
% FD2/DGN-10/FR-535 frozen candidate thresholds.  The D producer schema
% formal_voyage_qualification deliberately does NOT pre-fill activation
% values (contract section 3 activation-time value discipline); these
% are the Phase 4-C frozen candidates and per-call name-value overrides
% exist for the FR-540 activation-record injection (T-020 must inject
% activation values only).

function v = ELIG_CONST_MIN_DURATION_S()
% FD2/DGN-10/FR-535: min_duration_s = 1200 (boundary 1199.99 rejects,
% 1200 accepts; FR-538 forbids silent duration patching).
    v = 1200.0;
end

function v = ELIG_CONST_MIN_WINDOWS()
% FD2/DGN-10/FR-535: min_canonical_windows = 60.
    v = 60.0;
end

function v = ELIG_CONST_INVALID_MAX()
% FD2/DGN-10/FR-535: unexpected_invalid_canonical_windows = 0.
    v = 0;
end

function c = ELIG_CONST_TOL()
% Float tolerance for anchor/snapshot vintage matching (env_realize
% fixed-decision-4 precedent).
    c = 1e-9;
end

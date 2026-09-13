function plan = plan_load(activation_json, mode)
% fleet_v3.plan_load - T-018 Phase 4-D Stage A plan loader (73 driver data
% face).  Parses the FROZEN T-432 activation record (machine copy
% docs/phase4/d_dataset_v3/t432_activation_record.json; the .md record is
% the legal authority, this JSON is its machine twin and the only file the
% 73 pipeline consumes) into the plan struct consumed by gen_iid / gen_ood
% / manifest_stage_a, and computes the frozen provenance hashes recorded
% in the Stage A manifest (D5: the OOD configuration carries its own
% frozen provenance).
%
%   plan = fleet_v3.plan_load(activation_json_path, mode)
%     mode: 'stage_a' | 'smoke'
%       stage_a  IID: all 9 strata x 2 draws, base_draw_index 900..901
%                OOD: 6 conditions x 1 family, base_draw_index 900
%                (activation record stage_a.draw_index)
%       smoke    IID: first 2 strata x 1 draw, base_draw_index 950
%                OOD: 6 conditions x 1 family, base_draw_index 950
%                (T-018 Phase 1 micro-smoke segment k=950 on the SAME
%                stage_a seed; disjoint from the formal k=900..901 and
%                the formal Stage B segments by construction)
%     Stage B modes (formal IID base 0 k=0..15, formal OOD base 10000
%     k=0..9) are fully parameterized here (seeds/draws/counts) but their
%     execution is enabled in Phase 2 only (73 gate NOT_IMPLEMENTED).
%
% Everything numeric is READ from the activation JSON (bands, guards,
% sampler parameters, seeds, retry, rejection gates, qualification
% criteria); nothing is re-hard-coded here.  Numeric bands arrive as
% interval strings ("(2.0, 2.4]", "[1.2, 1.3)", "1.3 <= ht_effective
% < 1.5"); local_parse_band decodes the brackets mechanically and fails
% closed on anything unexpected (GATE:FLEETV3:PLAN:*).
%
% OOD host assignment (mechanical, from the frozen record):
%   A1     ROUTINE_PATROL x FLOOD  (proposal_samplers: FLOOD host
%          u_river_mag override; A2 retired - amendment 01)
%          u_river_mag override, per the T-018 delegation ruling text)
%   B1     WORK_MISSION x DRY    (proposal_samplers B1: "targeting
%          E[ht_effective | DRY dWL ~ TN(-0.8, 0.4)] ~= 1.40" - the DRY
%          water_level_offset is the only regime consistent with it)
%   C1~C3  parsed from axis_c.conditions.<id>.host ("ROUTINE_PATROL x
%          FLOOD" style strings in the frozen record itself)
%
% Proposal sampler parameters (mu/sigma/lower) are parsed from the
% frozen proposal_samplers strings and injected as ParamOverride key
% paths (existing keys only; schema/self_hash never overridable).  The
% upper bound of each truncated proposal has NO key in the frozen env
% registry, so it cannot be injected; it is enforced by the realized-
% value acceptance band (scientific membership always from realized
% values, per the record's own proposal_samplers note).
%
% Provenance hashes (plan.provenance):
%   activation_sha256       SHA-256 of the activation JSON file bytes
%   ood_override_sha256     SHA-256 of the canonical serialization of
%                           the parsed OOD configuration (hosts, sampler
%                           params, bands, guards, stress bounds, token
%                           registry sha) - stable across runs
%   heldout_registry_sha    self_hash.value read from the frozen
%                           config/mission_v3/heldout_token_registry.json
%
% Determinism: pure function of the frozen files + mode; identical inputs
% give a bitwise-identical plan (hash stability is unit-tested).
%
% Error ids: GATE:FLEETV3:PLAN:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; ROOT resolved from
% mfilename with pwd fallback; thin dispatch + local functions.

    if ~(ischar(activation_json) || isstring(activation_json))
        error('GATE:FLEETV3:PLAN:ARG', ...
            'activation_json must be a char vector path');
    end
    mode = local_check_mode(mode);

    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    apath = char(activation_json);
    if ~exist(apath, 'file')
        error('GATE:FLEETV3:PLAN:ARG', ...
            'activation record not found: %s', apath);
    end
    act = local_read_json(apath);
    if ~isfield(act, 'schema') || ~strcmp(char(act.schema), ...
            't432_activation_record_v1')
        error('GATE:FLEETV3:PLAN:SCHEMA', ...
            'activation record schema mismatch: %s', ...
            local_s(char(local_field_or(act, 'schema', 'MISSING'))));
    end
    if ~isfield(act, 'status') || ~strcmp(char(act.status), 'FROZEN')
        error('GATE:FLEETV3:PLAN:STATUS', ...
            'activation record is not FROZEN (status=%s)', ...
            local_s(char(local_field_or(act, 'status', 'MISSING'))));
    end
    % ---- amendment chain (fail-closed; T432_ACTIVATION_AMENDMENT_01) --
    act_sha = local_sha256_file(apath);
    amendment_sha = '';
    if isfield(act, 'amendment_chain')
        chain = local_cellstr_of(act.amendment_chain);
        for ai = 1:numel(chain)
            arel = char(chain{ai});
            am = local_read_json(fullfile(fileparts(fileparts(ROOT)), ...
                arel));
            % lineage anchor: the amendment records the record sha it was
            % frozen against; the record may legitimately evolve after the
            % amendment (e.g. qualified-stack backfill), so only presence
            % + parseability are enforced here and the amendment sha is
            % recorded in provenance for audit (original_activation_sha256
            % inside the amendment pins the lineage).
            if ~isfield(am, 'original_activation_sha256')
                error('GATE:FLEETV3:PLAN:AMEND', ...
                    'amendment %s lacks original_activation_sha256', arel);
            end
            amendment_sha = local_sha256_file(fullfile( ...
                fileparts(fileparts(ROOT)), arel));
        end
    end

    plan = struct();
    plan.mode = mode;
    plan.modele_root = ROOT;
    plan.repo_root = fileparts(fileparts(ROOT));
    plan.activation_rel = ['docs/phase4/d_dataset_v3/' ...
        't432_activation_record.json'];
    plan.activation = act;

    % ---- formal voyage qualification criteria (injected into
    %      mission_v3.eligibility('judge') per FR-540; never hard-coded) --
    q = act.formal_voyage_qualification;
    plan.qual = struct( ...
        'min_duration_s', local_pos_num(q.min_duration_s, ...
        'min_duration_s'), ...
        'min_canonical_windows', local_pos_num(q.min_canonical_windows, ...
        'min_canonical_windows'), ...
        'unexpected_invalid_max', local_nonneg_num( ...
        q.unexpected_invalid_max, 'unexpected_invalid_max'));

    % ---- IID plan (formal Stage B parameterization + stage segment) ----
    iid = act.formal_fleet.iid;
    iid_strata = local_strata_from_prequal(ROOT);
    draws_rule = local_parse_k_rule(char(iid.draw_index_rule), ...
        'iid.draw_index_rule');
    % formal: stratum i (0-based) uses [i*stride, (i+1)*stride), k=0..N-1
    plan.iid = struct();
    plan.iid.seed_formal = local_s(char(iid.seed_root));
    plan.iid.n_strata = local_pos_num(iid.n_strata, 'iid.n_strata');
    plan.iid.members_per_family = local_pos_num( ...
        iid.members_per_family, 'iid.members_per_family');
    plan.iid.families_per_stratum = local_pos_num( ...
        iid.families_per_stratum, 'iid.families_per_stratum');
    plan.iid.draw_stride = draws_rule.stride;
    plan.iid.k_formal = draws_rule.k;
    % stage_b formal segments (FR-497 Stage B pre-registration): stratum i
    % (0-based) draws k=0..k_formal-1 starting at i*draw_stride; OOD
    % condition c families k=0..families_formal-1 starting at
    % ood.seed_base + c*ood.seed_stride
    plan.iid.retry_max = local_nonneg_num( ...
        iid.retry.slot_failed_retry_max, 'iid.retry.slot_failed_retry_max');
    plan.iid.thr_stratum = local_frac01( ...
        iid.rejection_rate_gate.per_stratum_max, 'per_stratum_max');
    plan.iid.thr_overall = local_frac01( ...
        iid.rejection_rate_gate.overall_max, 'overall_max');
    plan.iid.strata = iid_strata;
    if numel(iid_strata) ~= double(plan.iid.n_strata)
        error('GATE:FLEETV3:PLAN:STRATA', ...
            ['activation n_strata %d != prequal_plan strata list %d ', ...
             '(frozen registry drift)'], plan.iid.n_strata, ...
            numel(iid_strata));
    end
    % stage segment resolution (activation stage_a.draw_index: "IID:
    % stratum k=900..901")
    sa = act.stage_a;
    sa_rule = local_parse_stage_draw(char(sa.draw_index));
    plan.iid.seed = local_s(char(sa.seed_root));
    plan.iid.draw_base = sa_rule.iid_base;
    plan.iid.draws = sa_rule.iid_draws;
    plan.iid.max_attempts = OOD_CONST_MAX_ATTEMPTS();

    % ---- OOD plan ----
    ood = act.formal_fleet.ood;
    ood_rule = local_parse_ood_seed_rule(char(ood.seed_rule));
    plan.ood = struct();
    plan.ood.seed_formal = plan.iid.seed_formal;   % "same formal seed root"
    plan.ood.seed = plan.iid.seed;                 % stage_a segment
    plan.ood.def_version = local_s(char(ood.definition_version));
    plan.ood.conditions = local_cellstr_of(ood.counts.conditions);
    plan.ood.per_condition_families = local_pos_num( ...
        ood.counts.per_condition_families, 'per_condition_families');
    plan.ood.per_condition_voyages = local_pos_num( ...
        ood.counts.per_condition_voyages, 'per_condition_voyages');
    plan.ood.seed_base = ood_rule.base;
    plan.ood.seed_stride = ood_rule.stride;
    plan.ood.families_formal = ood_rule.k;
    plan.ood.max_attempts = OOD_CONST_MAX_ATTEMPTS();
    % stage_a OOD segment ("OOD: condition segment k=900"; smoke uses
    % the disjoint k=950 segment)
    plan.ood.draw_base = sa_rule.ood_base;
    if ~contains(char(ood.seed_rule), 'same formal seed root')
        error('GATE:FLEETV3:PLAN:OODSEED', ...
            ['ood.seed_rule no longer declares "same formal seed root"; ', ...
             'plan_load binding is stale: %s'], char(ood.seed_rule));
    end

    % axis A bands/guards (interval strings decoded mechanically)
    axa = ood.axis_a;
    % post-amendment-01: A1 is the only formal Axis-A condition; the
    % retired A2 (2.4,2.8] survives as the stress/domain-boundary
    % investigation band (documented, never a membership target)
    plan.ood.band_A1 = local_parse_band( ...
        char(local_field_or(axa.conditions, 'A1', '')), 'A1');
    plan.ood.stress_investigation_band = local_parse_band( ...
        char(local_field_or(axa, 'stress_investigation_band', '')), ...
        'stress_investigation_band');
    plan.ood.iid_support_max_adverse = local_pos_num( ...
        axa.iid_support_max.adverse, 'iid_support_max.adverse');
    plan.ood.iid_support_max_following = local_pos_num( ...
        axa.iid_support_max.following, 'iid_support_max.following');
    % guard_band_rule "GUARD(direction) = (IID_SUPPORT_MAX(direction), 2.0]"
    guard_top = local_parse_guard_top(char(axa.guard_band_rule));
    plan.ood.guard_adverse = struct( ...
        'min', plan.ood.iid_support_max_adverse, 'min_incl', false, ...
        'max', guard_top, 'max_incl', true);
    plan.ood.guard_following = struct( ...
        'min', plan.ood.iid_support_max_following, 'min_incl', false, ...
        'max', guard_top, 'max_incl', true);
    % cross-check against the record's own guard_bands strings
    gb = axa.guard_bands;
    if ~local_band_equal(local_parse_band(char(gb.adverse), 'guard_adverse'), ...
            plan.ood.guard_adverse) ...
            || ~local_band_equal( ...
            local_parse_band(char(gb.following), 'guard_following'), ...
            plan.ood.guard_following)
        error('GATE:FLEETV3:PLAN:GUARD', ...
            ['guard_band_rule construction disagrees with the ', ...
             'guard_bands strings in the frozen record']);
    end
    % hard domain bound |U| <= 2.8 (D5 "hard bounds |U|<=2.8"): the
    % retired A2 investigation-band top is that hard bound (closed), so
    % the numeric source of the guard is the frozen interval itself
    plan.ood.sup_domain_max = plan.ood.stress_investigation_band.max;

    % axis B (membership judged on realized ht_effective)
    axb = ood.axis_b;
    plan.ood.band_B1 = local_parse_ht_definition( ...
        char(axb.scientific_definition));
    plan.ood.train_support_min_ht = local_pos_num( ...
        axb.train_support_min_ht, 'train_support_min_ht');
    db = axb.domain_bounds;
    plan.ood.stress_band = local_parse_band( ...
        char(db.stress_margin_band), 'stress_margin_band');
    plan.ood.ht_domain_floor = local_pos_num(db.hard_floor, 'hard_floor');
    if abs(plan.ood.stress_band.min - plan.ood.ht_domain_floor) > 1e-12
        error('GATE:FLEETV3:PLAN:HTFLOOR', ...
            ['stress_margin_band min %.17g != hard_floor %.17g'], ...
            plan.ood.stress_band.min, plan.ood.ht_domain_floor);
    end
    if abs(plan.ood.band_B1.min - plan.ood.stress_band.max) > 1e-12
        error('GATE:FLEETV3:PLAN:HTBAND', ...
            ['axis B membership min %.17g != stress margin top %.17g ', ...
             '(bands must tile)'], plan.ood.band_B1.min, ...
            plan.ood.stress_band.max);
    end

    % axis C tokens (canonical, consumed - never hand-copied; D4)
    [reg_path, reg_sha] = local_registry_ref(ROOT);
    plan.ood.registry_rel = reg_path;
    plan.ood.registry_sha = reg_sha;
    axc = ood.axis_c;
    if ~contains(char(axc.token_source), 'heldout_token_registry.json')
        error('GATE:FLEETV3:PLAN:TOKENSRC', ...
            'axis_c.token_source no longer names the frozen registry');
    end
    plan.ood.tokens = local_tokens_of(ROOT, reg_sha);
    cond_ids = fieldnames(axc.conditions);
    plan.ood.hosts = struct();
    plan.ood.proposal = struct();
    for c = 1:numel(cond_ids)
        cid = cond_ids{c};
        row = axc.conditions.(cid);
        tok_id = local_s(char(row.token));
        tok = local_find_token(plan.ood.tokens, tok_id, cid);
        if ~strcmp(char(tok.ood_action), 'REQUIRE')
            error('GATE:FLEETV3:PLAN:TOKENACTION', ...
                'token %s for condition %s is not REQUIRE', tok_id, cid);
        end
        h = local_parse_host(char(row.host));
        h.token_id = tok_id;
        plan.ood.hosts.(cid) = h;
        % C conditions: natural sampling per the host regime frozen
        % distributions - no ParamOverride, no sampler numbers
        plan.ood.proposal.(cid) = struct('key_path', '', ...
            'override', struct(), 'mu', NaN, 'sigma', NaN, ...
            'lower', NaN, 'upper_uninjected', NaN, ...
            'note', 'natural sampling per host regime (frozen record)');
    end
    % A/B hosts + proposal sampler overrides (parsed, injected as
    % existing-key paths only; upper stays uninjected - band-enforced)
    plan.ood.hosts.A1 = local_parse_host(char(act.formal_fleet.ood.axis_a.hosts.A1));
    plan.ood.hosts.B1 = local_parse_host(char(act.formal_fleet.ood.axis_b.host));
    plan.ood.proposal.A1 = local_parse_sampler( ...
        char(act.formal_fleet.ood.proposal_samplers.A1), ...
        'regimes.FLOOD.u_river_mag', 'A1');
    plan.ood.proposal.B1 = local_parse_sampler( ...
        char(act.formal_fleet.ood.proposal_samplers.B1), ...
        'chart_depth_voyage_base', 'B1');

    % ---- smoke segment (T-018 Phase 1 micro-smoke; k=950 on the
    %      stage_a seed; products disposable) ----
    plan.smoke = struct();
    plan.smoke.iid_strata = 2;      % first two strata of the frozen list
    plan.smoke.iid_draws = 1;
    plan.smoke.ood_families = 1;
    plan.smoke.draw_base = SMOKE_CONST_DRAW_BASE();
    plan.smoke.disposable = true;

    % ---- stage_a section binding ----
    plan.stage_a_seed = local_s(char(sa.seed_root));
    plan.stage_a_disposable = logical(sa.disposable);

    % ---- provenance hashes (manifest-recorded; D5) ----
    act_sha = local_sha256_file(apath);
    ood_cfg = local_ood_config_canon(plan);
    plan.provenance = struct();
    plan.provenance.activation_sha256 = act_sha;
    plan.provenance.amendment_sha256 = amendment_sha;
    plan.provenance.ood_override_sha256 = local_sha256_text(ood_cfg);
    plan.provenance.heldout_registry_sha = reg_sha;
    plan.provenance.ood_override_canon = ood_cfg;
end

%% ====================== constants (provenance) ======================

function v = OOD_CONST_MAX_ATTEMPTS()
% Reject-resample loop budget per OOD family (T-018 delegation: loop
% max 200; matches env_realize default max attempts).
    v = 200;
end

function v = SMOKE_CONST_DRAW_BASE()
% T-018 Phase 1 micro-smoke draw segment k=950 on the stage_a seed;
% disjoint from the formal stage_a segment (k=900..901 / k=900) and from
% every formal Stage B segment.
    v = 950;
end

%% ========================== strata source ===========================

function strata = local_strata_from_prequal(ROOT)
% The 9-stratum list is frozen in the C-side registry
% config/mission_v3/prequal_plan.json (read-only input; the D pipeline
% consumes it, never restates it).
    p = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'prequal_plan.json');
    v = p.scale.strata;
    strata = local_cellstr_of(v);
    if isempty(strata)
        error('GATE:FLEETV3:PLAN:STRATA', ...
            'prequal_plan.json strata list is empty');
    end
    for i = 1:numel(strata)
        parts = strsplit(strata{i}, '__');
        if numel(parts) ~= 2 || ~strncmp(strata{i}, 'STRAT_', 6)
            error('GATE:FLEETV3:PLAN:STRATA', ...
                'stratum id not of the form STRAT_<TPL>__<REG>: %s', ...
                strata{i});
        end
    end
end

%% ======================= rule string parsing ========================

function r = local_parse_k_rule(s, what)
% "stratum i (0-based) uses base_draw_index in [i*1000, (i+1)*1000),
%  k=0..15" -> stride + formal k count.
    t = regexp(s, '\[i\*(\d+),', 'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:RULE', ...
            'cannot parse the stride from %s: %s', what, s);
    end
    stride = str2double(t{1}{1});
    t = regexp(s, 'k=(\d+)\.\.(\d+)', 'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:RULE', ...
            'cannot parse the k range from %s: %s', what, s);
    end
    k = str2double(t{1}{2}) - str2double(t{1}{1}) + 1;
    r = struct('stride', local_pos_num(stride, [what '.stride']), ...
        'k', local_pos_num(k, [what '.k']));
end

function r = local_parse_ood_seed_rule(s)
% "condition c (0-based, order ...) uses base_draw_index in
%  [10000+c*1000, 10000+(c+1)*1000), k=0..9; ..."
    t = regexp(s, '\[(\d+)\+c\*(\d+),', 'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:RULE', ...
            'cannot parse the OOD seed base/stride: %s', s);
    end
    base = str2double(t{1}{1});
    stride = str2double(t{1}{2});
    t = regexp(s, 'k=(\d+)\.\.(\d+)', 'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:RULE', ...
            'cannot parse the OOD k range: %s', s);
    end
    k = str2double(t{1}{2}) - str2double(t{1}{1}) + 1;
    r = struct('base', local_nonneg_num(base, 'ood.seed_base'), ...
        'stride', local_pos_num(stride, 'ood.seed_stride'), ...
        'k', local_pos_num(k, 'ood.families'));
end

function r = local_parse_stage_draw(s)
% stage_a.draw_index: "IID: stratum k=900..901; OOD: condition segment
% k=900" -> iid base/draws + ood base.
    t = regexp(s, 'IID:\s*stratum k=(\d+)\.\.(\d+)', 'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:RULE', ...
            'cannot parse the stage_a IID draw segment: %s', s);
    end
    b = str2double(t{1}{1});
    e = str2double(t{1}{2});
    t2 = regexp(s, 'OOD:\s*condition segment k=(\d+)', 'tokens');
    if isempty(t2)
        error('GATE:FLEETV3:PLAN:RULE', ...
            'cannot parse the stage_a OOD draw segment: %s', s);
    end
    r = struct('iid_base', local_nonneg_num(b, 'stage iid_base'), ...
        'iid_draws', local_pos_num(e - b + 1, 'stage iid_draws'), ...
        'ood_base', local_nonneg_num(str2double(t2{1}{1}), ...
        'stage ood_base'));
end

function v = local_parse_guard_top(s)
% "GUARD(direction) = (IID_SUPPORT_MAX(direction), 2.0]" -> 2.0
    t = regexp(s, 'IID_SUPPORT_MAX\(direction\),\s*(\d*\.?\d+)\]', 'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:RULE', ...
            'cannot parse the guard band top from: %s', s);
    end
    v = local_pos_num(str2double(t{1}{1}), 'guard_top');
end

%% ========================== band parsing ============================

function h = local_parse_host(s)
% "ROUTINE_PATROL x FLOOD" or "WORK_MISSION x DRY" -> struct
    t = regexp(s, '^([A-Z_]+)\s+x\s*([A-Z_]+)$', 'tokens', 'once');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:HOST', ...
            'cannot parse host string: %s', s);
    end
    % 'tokens','once' returns the token cell directly: t{1}=template,
    % t{2}=regime (no double brace level)
    h = struct('template', char(t{1}), 'regime', char(t{2}), ...
        'stratum', sprintf('STRAT_%s__%s', upper(char(t{1})), ...
        upper(char(t{2}))), 'token_id', '');
end

function b = local_parse_band(s, what)
% Decode an interval string "(a, b]" / "[a, b)" / "[a, b]" / "(a, b)".
    % trailing annotation after the interval (e.g. the record's
    % "[1.2, 1.3) -> STRESS_SET if generated") is tolerated
    t = regexp(s, '([\[\(])\s*(\d*\.?\d+)\s*,\s*(\d*\.?\d+)\s*([\]\)])', ...
        'tokens', 'once');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:BAND', ...
            'cannot parse interval string for %s: %s', what, s);
    end
    % 'tokens','once' returns the token group directly: t{1}=left
    % bracket, t{2}=min, t{3}=max, t{4}=right bracket
    b = struct( ...
        'min', str2double(t{2}), ...
        'min_incl', strcmp(t{1}, '['), ...
        'max', str2double(t{3}), ...
        'max_incl', strcmp(t{4}, ']'), ...
        'src', s);
    if ~(b.min < b.max)
        error('GATE:FLEETV3:PLAN:BAND', ...
            'interval for %s is empty or reversed: %s', what, s);
    end
end

function b = local_parse_ht_definition(s)
% "1.3 <= ht_effective < 1.5" -> [1.3, 1.5); comparison operators are
% decoded (<= lower-inclusive, < upper-exclusive).
    t = regexp(s, '(\d*\.?\d+)\s*<=\s*ht_effective\s*<\s*(\d*\.?\d+)', ...
        'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:BAND', ...
            'cannot parse the axis B membership definition: %s', s);
    end
    b = struct('min', str2double(t{1}{1}), 'min_incl', true, ...
        'max', str2double(t{1}{2}), 'max_incl', false, 'src', s);
    if ~(b.min < b.max)
        error('GATE:FLEETV3:PLAN:BAND', ...
            'axis B membership band is empty: %s', s);
    end
end

function tf = local_band_equal(a, b)
    tf = abs(a.min - b.min) < 1e-12 && abs(a.max - b.max) < 1e-12 ...
        && a.min_incl == b.min_incl && a.max_incl == b.max_incl;
end

%% ====================== sampler parsing =============================

function p = local_parse_sampler(s, key_path, cond)
% "u_river_mag ~ TN(2.2, 0.15, lower=2.05, upper=2.4)" (or the B1
% "h_chart ~ TN(3.86, 0.15, lower=3.64, upper=4.08) targeting ...")
% -> ParamOverride struct at the existing key path + the recorded upper.
    t = regexp(s, ['TN\((\d*\.?\d+),\s*(\d*\.?\d+),\s*' ...
        'lower=(\d*\.?\d+),\s*upper=(\d*\.?\d+)\)'], 'tokens');
    if isempty(t)
        error('GATE:FLEETV3:PLAN:SAMPLER', ...
            'cannot parse the proposal sampler for %s: %s', cond, s);
    end
    mu = str2double(t{1}{1});
    sigma = str2double(t{1}{2});
    lower = str2double(t{1}{3});
    upper = str2double(t{1}{4});
    parts = strsplit(key_path, '.');
    ov = local_set_path(struct(), parts, struct('mu', mu, ...
        'sigma', sigma, 'lower', lower));
    p = struct('key_path', key_path, 'override', ov, ...
        'mu', mu, 'sigma', sigma, 'lower', lower, ...
        'upper_uninjected', upper, ...
        'note', ['upper not injectable (no such key in the frozen env ', ...
        'registry); scientific membership is band-enforced on realized ', ...
        'values per the frozen proposal_samplers note']);
    if ~(p.lower < p.upper_uninjected && p.mu > p.lower ...
            && p.mu < p.upper_uninjected)
        error('GATE:FLEETV3:PLAN:SAMPLER', ...
            'proposal sampler for %s is not inside its own bounds', cond);
    end
end

function obj = local_set_path(obj, parts, leaf)
% Build a nested struct along an existing dotted key path (the path is
% validated against the frozen env registry at generation time by the
% fail-closed env_realize/dag_sample override whitelist).  Written
% without chained indexed assignment (package discipline): every level
% is a plain variable assignment.
    if numel(parts) == 1
        sub = struct();
        fn = fieldnames(leaf);
        for i = 1:numel(fn)
            sub.(fn{i}) = leaf.(fn{i});
        end
        obj.(parts{1}) = sub;
        return;
    end
    if isfield(obj, parts{1})
        sub = obj.(parts{1});
    else
        sub = struct();
    end
    sub = local_set_path(sub, parts(2:end), leaf);
    obj.(parts{1}) = sub;
end

%% ========================== token loading ===========================

function [rel, sha] = local_registry_ref(ROOT)
    rel = 'config/mission_v3/heldout_token_registry.json';
    obj = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'heldout_token_registry.json');
    if ~isfield(obj, 'self_hash') || ~isfield(obj.self_hash, 'value')
        error('GATE:FLEETV3:PLAN:REGISTRY', ...
            'heldout_token_registry lacks self_hash.value');
    end
    sha = local_s(char(obj.self_hash.value));
end

function toks = local_tokens_of(ROOT, expect_sha)
    obj = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'heldout_token_registry.json');
    if ~strcmp(local_s(char(obj.self_hash.value)), expect_sha)
        error('GATE:FLEETV3:PLAN:REGISTRY', ...
            'registry self_hash changed between reads');
    end
    v = obj.tokens;
    if isstruct(v)
        n = numel(v);
        toks = cell(1, n);
        for i = 1:n
            toks{i} = v(i);
        end
    elseif iscell(v)
        toks = v(:)';
    else
        error('GATE:FLEETV3:PLAN:REGISTRY', ...
            'registry tokens is neither a struct array nor a cell');
    end
end

function tok = local_find_token(toks, token_id, cond)
    for i = 1:numel(toks)
        if strcmp(char(toks{i}.token_id), token_id)
            tok = toks{i};
            return;
        end
    end
    error('GATE:FLEETV3:PLAN:TOKEN', ...
        'condition %s token %s not in the frozen registry', cond, token_id);
end

%% ======================== provenance canon ==========================

function s = local_ood_config_canon(plan)
% Canonical, order-stable serialization of every OOD configuration value
% the pipeline consumes (hash-stability unit-tested).
    parts = {};
    parts{end+1} = ['def_version=' plan.ood.def_version]; %#ok<AGROW>
    cids = plan.ood.conditions;
    for c = 1:numel(cids)
        cid = cids{c};
        h = plan.ood.hosts.(cid);
        pr = plan.ood.proposal.(cid);
        parts{end+1} = sprintf('cond=%s;tpl=%s;reg=%s;token=%s', ...
            cid, h.template, h.regime, h.token_id); %#ok<AGROW>
        parts{end+1} = sprintf( ...
            'cond=%s;ov=%s;mu=%.17g;sigma=%.17g;lower=%.17g;upper=%.17g', ...
            cid, pr.key_path, pr.mu, pr.sigma, pr.lower, ...
            pr.upper_uninjected); %#ok<AGROW>
    end
    parts{end+1} = local_band_canon('A1', plan.ood.band_A1); %#ok<AGROW>
    parts{end+1} = local_band_canon('stress_inv', ...
        plan.ood.stress_investigation_band); %#ok<AGROW>
    parts{end+1} = local_band_canon('B1', plan.ood.band_B1); %#ok<AGROW>
    parts{end+1} = sprintf('guard_adverse=%.17g,%.17g,%d,%d', ...
        plan.ood.guard_adverse.min, plan.ood.guard_adverse.max, ...
        plan.ood.guard_adverse.min_incl, plan.ood.guard_adverse.max_incl); %#ok<AGROW>
    parts{end+1} = sprintf('guard_following=%.17g,%.17g,%d,%d', ...
        plan.ood.guard_following.min, plan.ood.guard_following.max, ...
        plan.ood.guard_following.min_incl, ...
        plan.ood.guard_following.max_incl); %#ok<AGROW>
    parts{end+1} = sprintf('stress=%.17g,%.17g,%d,%d', ...
        plan.ood.stress_band.min, plan.ood.stress_band.max, ...
        plan.ood.stress_band.min_incl, plan.ood.stress_band.max_incl); %#ok<AGROW>
    parts{end+1} = sprintf('sup_domain_max=%.17g', ...
        plan.ood.sup_domain_max); %#ok<AGROW>
    parts{end+1} = sprintf('ht_domain_floor=%.17g', ...
        plan.ood.ht_domain_floor); %#ok<AGROW>
    parts{end+1} = ['registry_sha=' plan.ood.registry_sha]; %#ok<AGROW>
    for i = 1:numel(plan.ood.tokens)
        tk = plan.ood.tokens{i};
        parts{end+1} = sprintf('token=%s;action=%s', ...
            char(tk.token_id), char(tk.ood_action)); %#ok<AGROW>
    end
    s = strjoin(parts, newline);
end

function s = local_band_canon(name, b)
    s = sprintf('%s=%.17g,%.17g,%d,%d', name, b.min, b.max, ...
        b.min_incl, b.max_incl);
end

%% ============================= helpers ==============================

function m = local_check_mode(mode)
    if ~(ischar(mode) || isstring(mode))
        error('GATE:FLEETV3:PLAN:ARG', 'mode must be a char vector');
    end
    m = char(mode);
    if ~ismember(m, {'stage_a', 'smoke', 'stage_b_iid', 'stage_b_ood'})
        error('GATE:FLEETV3:PLAN:ARG', ...
            'mode must be stage_a|smoke|stage_b_iid|stage_b_ood (got %s)', m);
    end
end

function v = local_field_or(s, name, def)
    v = def;
    if isfield(s, name)
        v = s.(name);
    end
end

function s = local_s(v)
    if ischar(v)
        s = v;
    elseif isstring(v) && numel(v) == 1
        s = char(v);
    else
        error('GATE:FLEETV3:PLAN:ARG', 'expected a char/string scalar');
    end
end

function c = local_cellstr_of(v)
    c = {};
    if iscell(v)
        for i = 1:numel(v)
            c{end+1} = local_s(char(v{i})); %#ok<AGROW>
        end
    elseif isstruct(v)
        fn = fieldnames(v);
        for i = 1:numel(fn)
            c{end+1} = fn{i}; %#ok<AGROW>
        end
    elseif ischar(v)
        c{1} = char(v);
    else
        error('GATE:FLEETV3:PLAN:ARG', 'expected a cellstr value');
    end
end

function v = local_pos_num(x, what)
    if ~isnumeric(x) || ~isscalar(x) || ~isfinite(x) || x <= 0
        error('GATE:FLEETV3:PLAN:NUM', ...
            '%s must be a finite positive scalar (got %s)', what, ...
            mat2str(double(x)));
    end
    v = double(x);
end

function v = local_nonneg_num(x, what)
    if ~isnumeric(x) || ~isscalar(x) || ~isfinite(x) || x < 0
        error('GATE:FLEETV3:PLAN:NUM', ...
            '%s must be a finite non-negative scalar (got %s)', what, ...
            mat2str(double(x)));
    end
    v = double(x);
end

function v = local_frac01(x, what)
    v = local_nonneg_num(x, what);
    if v > 1
        error('GATE:FLEETV3:PLAN:NUM', ...
            '%s must lie in [0, 1] (got %.17g)', what, v);
    end
end

function obj = local_read_json(p1, varargin)
    if nargin == 1
        p = p1;   % single-argument form: p1 is the full file path
    elseif nargin == 2
        p = fullfile(p1, varargin{1});
    else
        p = fullfile(p1, varargin{1}, varargin{2});
    end
    if ~exist(p, 'file')
        error('GATE:FLEETV3:PLAN:IO', 'frozen file not found: %s', p);
    end
    try
        obj = jsondecode(fileread(p));
    catch dec_err
        error('GATE:FLEETV3:PLAN:IO', ...
            'jsondecode failed for %s: %s', p, dec_err.message);
    end
    if ~isstruct(obj)
        error('GATE:FLEETV3:PLAN:IO', ...
            'jsondecode did not return a struct for %s', p);
    end
end

function h = local_sha256_text(s)
    bytes = unicode2native(s, 'UTF-8');
    h = local_sha256_bytes(bytes);
end

function h = local_sha256_file(path)
    fid = fopen(path, 'r');
    if fid < 0
        error('GATE:FLEETV3:PLAN:IO', ...
            'cannot open file for hashing: %s', path);
    end
    raw = fread(fid, inf, 'uint8=>uint8');
    fclose(fid);
    h = local_sha256_bytes(raw);
end

function h = local_sha256_bytes(bytes)
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(bytes(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function res = family_expand(mode, varargin)
% mission_v3.family_expand - T-014 L4 intervention expansion + grouping
%   (FR-518/521~525/529; DGN-01/DGN-07; C-D1 grouping ruling)
%
%   M   = mission_v3.family_expand('default_matrix')
%   fam = mission_v3.family_expand('expand', dag [, name-values])
%   h   = mission_v3.family_expand('truth_hash', dag, rl)
%   res = mission_v3.family_expand('audit', registry)
%   res = mission_v3.family_expand('closure', ids, edges [, digestKeys])
%
% Modes:
%   'default_matrix'  v1 built-in intervention registry (fixed decision 1,
%                     source DGN-01 / C-D1): traffic PHYSICAL [ON OFF] x
%                     forecast INFORMATION [GOOD_V1 DEGRADED_V1] (2x2
%                     factorial; C-D1 allows factorial families).  The
%                     matrix is pre-declared before generation (FR-518/521).
%   'expand'          dag is a mission_v3.dag_sample('draw', ...) struct;
%                     expands the base draw into prod(numel(arms)) family
%                     members (v1 default: 4).  Name-values:
%       'Matrix'      pre-declared intervention matrix: non-empty struct
%                     array with fields axis (valid MATLAB identifier,
%                     seed_tree-safe), itype ('PHYSICAL'|'INFORMATION'),
%                     arms (non-empty unique cellstr; arms{1} is the base
%                     arm).  Default: the 'default_matrix' registry.
%       'FamilySeq'   whole number >= 1 (default 1; v1 uses one family per
%                     base draw, fixed decision 3).
%       'Realization' optional mission_v3.env_realize('realize', ...) output
%                     of the base draw; when given, every member carries
%                     truth_hash(Realization).  All members of one
%                     base-draw realization hash identically at this layer.
%   'truth_hash'      SHA-256 of the deterministic environment realization
%                     payload (fixed decision 4, FR-529): u_river_mps
%                     (full), per-segment windows, traffic events, the
%                     three water quantities (h_chart_m / delta_wl_m /
%                     h_effective_m), resistance_mult and
%                     delta_p_hotel_unplanned.  Forecast/plan-derived
%                     quantities are excluded.  dag and rl must be the same
%                     draw (formal seed + base draw index, fail-closed).
%   'audit'           member registry scan (fixed decision 5).  Each record
%                     needs member_id, family_id, base_draw_id,
%                     truth_lineage (cellstr of 64-hex truth-branch lineage
%                     ids) and truth_hash ('' or 64-hex).  Detections:
%                     identical truth lineage + different declared family
%                       -> GATE:MISSION_V3:FAMILY:UNDECLARED_REUSE (FR-524)
%                     same family + identical truth lineage + different
%                       declared truth hashes
%                       -> GATE:MISSION_V3:FAMILY:TRUTH_DRIFT (FR-529)
%                     family declaring mixed base draws
%                       -> GATE:MISSION_V3:FAMILY:FAMILY_BASE_DRAW (C-D1)
%   'closure'         union-find transitive closure of declared must-link
%                     edges over ids (FR-523 / C-D1).  edges is a Kx2
%                     cell array with endpoints in ids (empty = no edges);
%                     digestKeys (optional, default ids) are the per-node
%                     lineage identities feeding the group digest.
%                     Component id = 'SG#' + first 16 hex of SHA-256 over
%                     the sorted unique member keys joined by '|'.  Same
%                     template identity alone is never an edge (FR-525).
%
% Member lineage registration (all derived via mission_v3.seed_tree against
% the frozen policy; this layer adds no randomness): the base member (every
% arm = first arm) is the original draw registry.  For every axis where a
% member's arm is not the base arm: PHYSICAL adds the declared policy
% intervention branch 'intervention_<axis>'; INFORMATION replaces the
% standard 'forecast_obs_noise' branch by the declared instance
% 'forecast_obs_noise@<axis>'.  The truth branches stay standard in every
% member (INFORMATION truth sharing, FR-529; PHYSICAL changes only the
% declared intervention branch, FR-518).
%
% traffic_override semantics (fixed decision 2): traffic ON = base-draw
% traffic events as drawn; traffic OFF = member-level marker consumed
% downstream (that member's traffic events are zeroed there).  This layer
% never re-draws the environment, so traffic_override is member metadata
% outside the truth-hash payload: it cannot contradict FR-529 hashing, and
% all members of one base-draw realization carry the identical truth hash.
%
% split_group assignment itself (in-group train/test split) is D-side and
% out of scope; this layer only produces grouping metadata (C-D1).  The
% manifest is a scalar struct; callers concatenate manifests of multiple
% base draws into a struct array for T-015/T-019 consumption.
%
% Determinism: pure function of the inputs and the frozen policy; identical
% inputs give identical output.  Writes nothing to disk.
%
% Error ids: GATE:MISSION_V3:FAMILY:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; no chained-indexed
% assignment; thin main + local functions (style ref: seed_tree.m).

    if ~ischar(mode) && ~isstring(mode)
        error('GATE:MISSION_V3:FAMILY:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'default_matrix'
            if nargin ~= 1
                error('GATE:MISSION_V3:FAMILY:ARG', ...
                    '''default_matrix'' takes no further arguments');
            end
            res = local_default_matrix_res();
        case 'expand'
            if nargin < 2
                error('GATE:MISSION_V3:FAMILY:ARG', ...
                    '''expand'' expects: dag [, name-values]');
            end
            opts = local_parse_expand_opts(varargin(2:end));
            res = local_expand(varargin{1}, opts);
        case 'truth_hash'
            if nargin ~= 3
                error('GATE:MISSION_V3:FAMILY:ARG', ...
                    '''truth_hash'' expects: dag, realization');
            end
            res = local_truth_hash(varargin{1}, varargin{2});
        case 'audit'
            if nargin ~= 2
                error('GATE:MISSION_V3:FAMILY:ARG', ...
                    '''audit'' expects 1 argument: registry');
            end
            res = local_audit(varargin{1});
        case 'closure'
            if nargin < 3
                error('GATE:MISSION_V3:FAMILY:ARG', ...
                    '''closure'' expects: ids, edges [, digestKeys]');
            end
            if nargin >= 4
                dkeys = varargin{3};
            else
                dkeys = [];
            end
            res = local_closure(varargin{1}, varargin{2}, dkeys);
        otherwise
            error('GATE:MISSION_V3:FAMILY:ARG', 'unknown mode: %s', char(mode));
    end
end

%% ======================== local functions ==========================

function out = local_default_matrix_res()
% v1 built-in intervention registry (fixed decision 1; DGN-01 / C-D1).
    M = struct('axis', '', 'itype', '', 'arms', {cell(1, 0)});
    M(1).axis = 'traffic';
    M(1).itype = 'PHYSICAL';
    M(1).arms = {'ON', 'OFF'};
    M(2).axis = 'forecast';
    M(2).itype = 'INFORMATION';
    M(2).arms = {'GOOD_V1', 'DEGRADED_V1'};
    M = local_matrix_validate(M);
    out = struct();
    out.kind = 'intervention_matrix_default';
    out.matrix = M;
    out.n_axes = 2;
    out.n_members = 4;
    out.source = ['DGN-01 architecture / C-D1 ruling (T-014 delegation ', ...
        'spec fixed decision 1); pre-declared before generation ', ...
        '(FR-518/521)'];
    out.matrix_note = ['2x2 factorial family (C-D1 allows factorial); ', ...
        'traffic ON/OFF is a member-level marker (fixed decision 2); ', ...
        'forecast GOOD_V1/DEGRADED_V1 is implemented via the ', ...
        'forecast_obs_noise instance registration'];
end

function opts = local_parse_expand_opts(arg_cell)
    dm = local_default_matrix_res();
    opts = struct('matrix', {dm.matrix}, 'family_seq', 1, ...
        'realization', []);
    k = 1;
    while k <= numel(arg_cell)
        if (k + 1) > numel(arg_cell)
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                'missing value for option %s', string(arg_cell{k}));
        end
        opt = arg_cell{k};
        val = arg_cell{k + 1};
        if strcmpi(opt, 'Matrix')
            opts.matrix = val;
        elseif strcmpi(opt, 'FamilySeq')
            opts.family_seq = val;
        elseif strcmpi(opt, 'Realization')
            opts.realization = val;
        else
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                'unknown option: %s', string(opt));
        end
        k = k + 2;
    end
end

function M = local_matrix_validate(M)
% Fail-closed validation of a caller-declared intervention matrix.
    if ~isstruct(M) || isempty(M) || numel(M) < 1
        error('GATE:MISSION_V3:FAMILY:MATRIX', ...
            'matrix must be a non-empty struct array');
    end
    fn = fieldnames(M);
    need = {'axis', 'itype', 'arms'};
    for j = 1:numel(need)
        if ~ismember(need{j}, fn)
            error('GATE:MISSION_V3:FAMILY:MATRIX', ...
                'matrix axis %d lacks field: %s', j, need{j});
        end
    end
    n = numel(M);
    names = cell(1, n);
    for a = 1:n
        row = M(a);
        ax = row.axis;
        if ~ischar(ax) && ~isstring(ax)
            error('GATE:MISSION_V3:FAMILY:MATRIX', ...
                'matrix axis %d: axis must be a string', a);
        end
        ax = char(ax);
        if isempty(regexp(ax, '^[A-Za-z][A-Za-z0-9_]*$', 'once'))
            error('GATE:MISSION_V3:FAMILY:MATRIX', ...
                ['matrix axis "%s" must be a valid MATLAB identifier ', ...
                 '(usable as seed-tree axis and struct field)'], ax);
        end
        it = row.itype;
        if ~ischar(it) && ~isstring(it)
            error('GATE:MISSION_V3:FAMILY:MATRIX', ...
                'matrix axis %s: itype must be a string', ax);
        end
        it = upper(char(it));
        if ~ismember(it, {'PHYSICAL', 'INFORMATION'})
            error('GATE:MISSION_V3:FAMILY:MATRIX', ...
                'matrix axis %s: itype must be PHYSICAL or INFORMATION', ax);
        end
        arms_c = local_cellstr_arg(row.arms, ...
            sprintf('matrix axis %s arms', ax));
        for k = 1:numel(arms_c)
            if isempty(regexp(arms_c{k}, '^[A-Za-z0-9_.-]+$', 'once'))
                error('GATE:MISSION_V3:FAMILY:MATRIX', ...
                    'matrix axis %s arm "%s" must match [A-Za-z0-9_.-]+', ...
                    ax, arms_c{k});
            end
        end
        if numel(unique(arms_c)) ~= numel(arms_c)
            error('GATE:MISSION_V3:FAMILY:MATRIX', ...
                'matrix axis %s has duplicate arms', ax);
        end
        names{a} = ax;
        M(a).itype = it;
        M(a).arms = arms_c;
    end
    if numel(unique(names)) ~= n
        error('GATE:MISSION_V3:FAMILY:MATRIX', ...
            'matrix contains duplicate axis names');
    end
end

function fam = local_expand(dag, opts)
    if ~isstruct(dag) || isempty(dag) || ~isfield(dag, 'kind') ...
            || ~strcmp(dag.kind, 'dag_draw')
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'dag must be a mission_v3.dag_sample ''draw'' struct');
    end
    need_d = {'formal_seed', 'base_draw_index', 'template_id', ...
        'regime_id', 'stratum_id', 'lineage'};
    for j = 1:numel(need_d)
        if ~isfield(dag, need_d{j})
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                'dag lacks field: %s', need_d{j});
        end
    end
    if ~ischar(dag.formal_seed) && ~isstring(dag.formal_seed)
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'dag.formal_seed must be a string');
    end
    fs = char(dag.formal_seed);
    di = dag.base_draw_index;
    if ~isnumeric(di) || ~isscalar(di) || ~isfinite(di) || di < 0 ...
            || di ~= floor(di)
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'dag.base_draw_index must be a whole number >= 0');
    end
    M = local_matrix_validate(opts.matrix);
    fseq = opts.family_seq;
    if ~isnumeric(fseq) || ~isscalar(fseq) || ~isfinite(fseq) || fseq < 1 ...
            || fseq ~= floor(fseq)
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'FamilySeq must be a whole number >= 1');
    end

    pol = local_policy_ref();

    % canonical ids (fixed decision 3)
    base_draw_id = [fs, '#', sprintf('%d', di)];
    family_id = [base_draw_id, '@F', sprintf('%d', fseq)];

    % the standard registry must reproduce the base draw's own lineage
    std_reg = mission_v3.seed_tree('branches', fs, di);
    local_check_dag_lineage(std_reg, dag.lineage);

    % truth branch names = standard names minus the noise branch
    % (policy-derived via seed_tree, never hard-coded)
    truth_names = local_truth_names(std_reg);

    n_axes = numel(M);
    counts = zeros(1, n_axes);
    for a = 1:n_axes
        counts(a) = numel(M(a).arms);
    end
    n_members = prod(counts);

    traffic_axis = 0;
    for a = 1:n_axes
        if strcmp(M(a).axis, 'traffic') && strcmp(M(a).itype, 'PHYSICAL')
            traffic_axis = a;
        end
    end

    truth_hash_v = '';
    if ~isempty(opts.realization)
        truth_hash_v = local_truth_hash(dag, opts.realization);
    end

    blank_arm = struct();
    for a = 1:n_axes
        blank_arm.(M(a).axis) = '';
    end
    blank_mem = struct('member_id', '', 'member_index', 0, ...
        'is_base', false, 'arms', {blank_arm}, 'traffic_override', '', ...
        'lineage_names', {cell(1, 0)}, 'lineage_ids', {cell(1, 0)}, ...
        'truth_lineage_ids', {cell(1, 0)}, 'lineage_digest', '', ...
        'truth_hash', '', 'split_group_id', '');
    members = repmat(blank_mem, 1, n_members);

    for k = 1:n_members
        remv = k - 1;
        combo = zeros(1, n_axes);
        for a = n_axes:-1:1
            combo(a) = mod(remv, counts(a)) + 1;
            remv = floor(remv / counts(a));
        end
        lin = std_reg;
        for a = 1:n_axes
            if combo(a) <= 1
                continue;
            end
            axname = M(a).axis;
            if strcmp(M(a).itype, 'PHYSICAL')
                reg_ax = mission_v3.seed_tree('member_branches', fs, di, ...
                    'PHYSICAL', axname);
                row_i = local_find_branch(reg_ax, ...
                    ['intervention_', axname]);
                lin = local_upsert_branch(lin, row_i);
            else
                reg_ax = mission_v3.seed_tree('member_branches', fs, di, ...
                    'INFORMATION', axname);
                row_i = local_find_branch(reg_ax, ...
                    ['forecast_obs_noise@', axname]);
                lin = local_remove_branch(lin, 'forecast_obs_noise');
                lin = local_upsert_branch(lin, row_i);
            end
        end

        arms_v = blank_arm;
        for a = 1:n_axes
            arms_v.(M(a).axis) = M(a).arms{combo(a)};
        end

        n_lin = numel(lin);
        lnames = cell(1, n_lin);
        lids = cell(1, n_lin);
        for b = 1:n_lin
            rb = lin(b);
            lnames{b} = rb.branch_name;
            lids{b} = rb.lineage_id;
        end
        n_tn = numel(truth_names);
        tlids = cell(1, n_tn);
        for b = 1:n_tn
            rb = local_find_branch(lin, truth_names{b});
            tlids{b} = rb.lineage_id;
        end

        row = blank_mem;
        row.member_id = [family_id, '#M', sprintf('%d', k)];
        row.member_index = k;
        row.is_base = all(combo == 1);
        row.arms = arms_v;
        if traffic_axis > 0
            row.traffic_override = M(traffic_axis).arms{combo(traffic_axis)};
        end
        row.lineage_names = lnames;
        row.lineage_ids = lids;
        row.truth_lineage_ids = tlids;
        row.lineage_digest = local_short_digest(strjoin(sort(lids), '|'));
        row.truth_hash = truth_hash_v;
        members(k) = row;
    end

    % split_group: must-link closure over the family members; the group
    % digest key is the base_draw lineage identity (fixed decision 3)
    member_ids = cell(1, n_members);
    dkeys = cell(1, n_members);
    for k = 1:n_members
        member_ids{k} = members(k).member_id;
        dkeys{k} = base_draw_id;
    end
    n_pairs = n_members * (n_members - 1) / 2;
    edges = cell(n_pairs, 2);
    e = 0;
    for i = 1:n_members
        for j = (i + 1):n_members
            e = e + 1;
            edges{e, 1} = members(i).member_id;
            edges{e, 2} = members(j).member_id;
        end
    end
    cl = local_closure(member_ids, edges, dkeys);
    for k = 1:n_members
        row = members(k);
        row.split_group_id = cl.labels{k};
        members(k) = row;
    end
    sg = cl.labels{1};

    % audit-ready registry (fixed decision 5 consumption shape)
    blank_rec = struct('member_id', '', 'family_id', '', ...
        'base_draw_id', '', 'truth_lineage', {cell(1, 0)}, ...
        'truth_hash', '');
    reg = repmat(blank_rec, 1, n_members);
    for k = 1:n_members
        row = blank_rec;
        row.member_id = members(k).member_id;
        row.family_id = family_id;
        row.base_draw_id = base_draw_id;
        row.truth_lineage = members(k).truth_lineage_ids;
        row.truth_hash = members(k).truth_hash;
        reg(k) = row;
    end

    dsa = struct();
    dsa.base_draw_id = base_draw_id;
    dsa.declaration = ['all members share the base draw realized ancestry ', ...
        '(C-D1/FR-522); only the pre-declared matrix instances differ'];
    dsa.shared_truth_branch_names = truth_names;
    dsa.n_members = n_members;

    prov = struct();
    prov.generator = ['mission_v3.family_expand (T-014; DGN-01/DGN-07; ', ...
        'delegation spec experiments/mission_v3/T-014_delegation_spec.md)'];
    prov.policy_version = pol.policy_version;
    prov.policy_self_hash = pol.self_hash;
    prov.matrix_source = ['pre-declared intervention matrix (FR-518/521); ', ...
        'v1 default registry per DGN-01 / C-D1: traffic PHYSICAL ', ...
        '[ON OFF] x forecast INFORMATION [GOOD_V1 DEGRADED_V1]'];
    prov.lineage_note = ['member lineage derived exclusively via ', ...
        'mission_v3.seed_tree against the frozen policy; this layer adds ', ...
        'no randomness (pure metadata derivation)'];
    prov.v1_traffic_note = ['traffic ON = drawn events as-is; traffic ', ...
        'OFF = member-level traffic_override marker consumed downstream ', ...
        '(events zeroed there); no environment re-draw at this layer; ', ...
        'intervention_traffic is registered (declared axis) but not ', ...
        'consumed here (fixed decision 2)'];
    prov.truth_hash_note = ['truth hash = SHA-256 over the deterministic ', ...
        'realization payload (u_river_mps full, segment windows, traffic ', ...
        'events, water h_chart/delta_wl/h_effective, resistance_mult, ', ...
        'delta_p_hotel_unplanned); forecast/plan-derived quantities ', ...
        'excluded (FR-529); traffic_override is member metadata outside ', ...
        'the payload, so all members of one base-draw realization hash ', ...
        'identically'];
    prov.closure_note = ['split_group_id = SG# + 16-hex SHA-256 prefix ', ...
        'over the sorted unique must-link component keys (base_draw_id ', ...
        'per member); C-D1: one family -> exactly one split_group; same ', ...
        'template alone is never a must-link (FR-525)'];

    fam = struct();
    fam.kind = 'family_manifest';
    fam.formal_seed = fs;
    fam.base_draw_index = di;
    fam.template_id = char(dag.template_id);
    fam.regime_id = char(dag.regime_id);
    fam.stratum_id = char(dag.stratum_id);
    fam.base_draw_id = base_draw_id;
    fam.family_id = family_id;
    fam.split_group_id = sg;
    fam.matrix = M;
    fam.n_members = n_members;
    fam.members = members;
    fam.audit_registry = reg;
    fam.declared_shared_ancestry = dsa;
    fam.provenance = prov;
end

function h = local_truth_hash(dag, rl)
% SHA-256 over the structured serialization of the deterministic
% environment realization payload (fixed decision 4).  Forecast/plan
% derived quantities are excluded by construction: the payload contains
% exactly the fields listed below.
    if ~isstruct(dag) || ~isfield(dag, 'kind') || ~strcmp(dag.kind, 'dag_draw')
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'dag must be a mission_v3.dag_sample ''draw'' struct');
    end
    need_d = {'formal_seed', 'base_draw_index', 'resistance_mult', ...
        'delta_p_hotel_unplanned'};
    for j = 1:numel(need_d)
        if ~isfield(dag, need_d{j})
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                'dag lacks field: %s', need_d{j});
        end
    end
    if ~isstruct(rl) || ~isfield(rl, 'kind') || ~strcmp(rl.kind, 'realize')
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'realization must be a mission_v3.env_realize ''realize'' struct');
    end
    need_r = {'formal_seed', 'base_draw_index', 'u_river_mps', ...
        'segments', 'traffic', 'water'};
    for j = 1:numel(need_r)
        if ~isfield(rl, need_r{j})
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                'realization lacks field: %s', need_r{j});
        end
    end
    if ~ischar(rl.formal_seed) && ~isstring(rl.formal_seed)
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'realization formal_seed must be a string');
    end
    if ~strcmp(char(rl.formal_seed), char(dag.formal_seed)) ...
            || (double(rl.base_draw_index) ~= double(dag.base_draw_index))
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'realization and dag belong to different base draws');
    end
    if ~isfield(rl.water, 'h_chart_m') || ~isfield(rl.water, 'delta_wl_m') ...
            || ~isfield(rl.water, 'h_effective_m')
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'realization water lacks h_chart_m/delta_wl_m/h_effective_m');
    end

    lines = cell(1, 0);
    lines{end+1} = 'MISSION_V3_TRUTH_HASH_V1'; %#ok<AGROW>
    lines{end+1} = sprintf('u_river_mps n=%d', numel(rl.u_river_mps)); %#ok<AGROW>
    lines{end+1} = sprintf('%.17g ', rl.u_river_mps); %#ok<AGROW>
    nseg = numel(rl.segments);
    lines{end+1} = sprintf('segments n=%d', nseg); %#ok<AGROW>
    for i = 1:nseg
        sg = rl.segments(i);
        lines{end+1} = sprintf('segment %d window_s %.17g', ...
            sg.index, sg.window_s); %#ok<AGROW>
    end
    nev = numel(rl.traffic);
    lines{end+1} = sprintf('traffic_events n=%d', nev); %#ok<AGROW>
    for i = 1:nev
        ev = rl.traffic(i);
        lines{end+1} = sprintf('event %d %.17g %.17g %.17g %.17g', ...
            ev.segment_index, ev.t_start_rel_s, ev.t_start_abs_s, ...
            ev.duration_s, ev.speed_ratio); %#ok<AGROW>
    end
    wt = rl.water;
    lines{end+1} = sprintf('water %.17g %.17g %.17g', ...
        wt.h_chart_m, wt.delta_wl_m, wt.h_effective_m); %#ok<AGROW>
    lines{end+1} = sprintf('resistance_mult %.17g', dag.resistance_mult); %#ok<AGROW>
    lines{end+1} = sprintf('delta_p_hotel_unplanned %.17g', ...
        dag.delta_p_hotel_unplanned); %#ok<AGROW>
    h = local_sha256_text(strjoin(lines, newline));
end

function res = local_audit(registry)
% Fixed decision 5: undeclared cross-family truth reuse (FR-524) and
% same-family truth drift (FR-529) detection, fail-closed.
    if ~isstruct(registry) || isempty(registry)
        error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
            'registry must be a non-empty struct array');
    end
    fn = fieldnames(registry);
    need = {'member_id', 'family_id', 'base_draw_id', 'truth_lineage', ...
        'truth_hash'};
    for j = 1:numel(need)
        if ~ismember(need{j}, fn)
            error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
                'registry lacks required field: %s', need{j});
        end
    end
    n = numel(registry);
    ids = cell(1, n);
    for i = 1:n
        row = registry(i);
        ids{i} = local_id_field(row.member_id, ...
            sprintf('registry row %d member_id', i));
        local_id_field(row.family_id, ...
            sprintf('registry row %d family_id', i));
        local_id_field(row.base_draw_id, ...
            sprintf('registry row %d base_draw_id', i));
        tl = row.truth_lineage;
        if ~iscell(tl) || isempty(tl)
            error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
                'row %d: truth_lineage must be a non-empty cellstr', i);
        end
        for k = 1:numel(tl)
            e = tl{k};
            if ~ischar(e) && ~isstring(e)
                error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
                    'row %d: truth_lineage entry %d must be a string', i, k);
            end
            if isempty(regexp(char(e), '^[0-9a-f]{64}$', 'once'))
                error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
                    'row %d: truth_lineage entry %d is not 64 lowercase hex', ...
                    i, k);
            end
        end
        th = row.truth_hash;
        if ~ischar(th) && ~isstring(th)
            error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
                'row %d: truth_hash must be a string', i);
        end
        if ~isempty(char(th)) ...
                && isempty(regexp(char(th), '^[0-9a-f]{64}$', 'once'))
            error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
                'row %d: truth_hash is not 64 lowercase hex', i);
        end
    end
    if numel(unique(ids)) ~= n
        error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
            'duplicate member_id in registry');
    end

    % C-D1 hard invariant: one family = one base draw
    fam_ids = unique({registry.family_id}, 'stable');
    for f = 1:numel(fam_ids)
        bds = cell(1, 0);
        for i = 1:n
            row = registry(i);
            if strcmp(row.family_id, fam_ids{f}) ...
                    && ~ismember(row.base_draw_id, bds)
                bds{end+1} = row.base_draw_id; %#ok<AGROW>
            end
        end
        if numel(bds) > 1
            error('GATE:MISSION_V3:FAMILY:FAMILY_BASE_DRAW', ...
                ['family %s declares %d distinct base draws ', ...
                 '(C-D1: one family = one base draw)'], ...
                fam_ids{f}, numel(bds));
        end
    end

    % rule 1 (FR-524): identical truth lineage across different declared
    % families = undeclared cross-family truth reuse
    n_same_truth_pairs = 0;
    for i = 1:n
        for j = (i + 1):n
            ri = registry(i);
            rj = registry(j);
            if isequal(sort(ri.truth_lineage), sort(rj.truth_lineage))
                n_same_truth_pairs = n_same_truth_pairs + 1;
                if ~strcmp(ri.family_id, rj.family_id)
                    error('GATE:MISSION_V3:FAMILY:UNDECLARED_REUSE', ...
                        ['members %s (%s) and %s (%s) share identical ', ...
                         'truth-branch lineage but declare different ', ...
                         'families: undeclared cross-family truth reuse ', ...
                         '(FR-524)'], ...
                        ri.member_id, ri.family_id, ...
                        rj.member_id, rj.family_id);
                end
            end
        end
    end

    % rule 2 (FR-529): within a (family, truth lineage) group, declared
    % truth hashes must agree
    keys = cell(1, n);
    for i = 1:n
        row = registry(i);
        keys{i} = [row.family_id, '::', ...
            strjoin(sort(row.truth_lineage), ',')];
    end
    ukeys = unique(keys, 'stable');
    for g = 1:numel(ukeys)
        hashs = cell(1, 0);
        mids = cell(1, 0);
        for i = 1:n
            if strcmp(keys{i}, ukeys{g})
                row = registry(i);
                if ~isempty(char(row.truth_hash)) ...
                        && ~ismember(char(row.truth_hash), hashs)
                    hashs{end+1} = char(row.truth_hash); %#ok<AGROW>
                    mids{end+1} = row.member_id; %#ok<AGROW>
                end
            end
        end
        if numel(hashs) > 1
            error('GATE:MISSION_V3:FAMILY:TRUTH_DRIFT', ...
                ['members %s and %s declare different truth hashes for ', ...
                 'identical truth lineage (FR-529)'], ...
                mids{1}, mids{2});
        end
    end

    res = struct();
    res.ok = true;
    res.n_members = n;
    res.n_families = numel(fam_ids);
    res.n_same_truth_pairs = n_same_truth_pairs;
    res.n_lineage_groups = numel(ukeys);
end

function res = local_closure(ids, edges, dkeys)
% Union-find transitive closure of declared must-link edges (FR-523/C-D1).
    ids = local_closure_ids(ids, 'ids');
    n = numel(ids);
    if isempty(dkeys)
        dkeys = ids;
    end
    % digest keys are intentionally NON-unique: all members of one base
    % draw share the same lineage key; only format is validated here
    dkeys = local_closure_keys(dkeys, 'digestKeys');
    if numel(dkeys) ~= n
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            'digestKeys must have one entry per id');
    end

    n_edges = 0;
    ea = [];
    eb = [];
    if ~isempty(edges)
        if ~iscell(edges) || size(edges, 2) ~= 2
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                'edges must be a Kx2 cell array of id pairs');
        end
        n_edges = size(edges, 1);
        ea = zeros(1, n_edges);
        eb = zeros(1, n_edges);
        for k = 1:n_edges
            a1 = edges{k, 1};
            b1 = edges{k, 2};
            if (~ischar(a1) && ~isstring(a1)) || (~ischar(b1) && ~isstring(b1))
                error('GATE:MISSION_V3:FAMILY:ARG', ...
                    'edge %d endpoints must be strings', k);
            end
            ea(k) = local_id_index(char(a1), ids, k);
            eb(k) = local_id_index(char(b1), ids, k);
        end
    end

    parent = 1:n;
    for k = 1:n_edges
        [ra, parent] = local_uf_find(parent, ea(k));
        [rb, parent] = local_uf_find(parent, eb(k));
        if ra ~= rb
            parent(rb) = ra;
        end
    end

    root_ids = cell(1, 0);
    group_of = zeros(1, n);
    for i = 1:n
        [r, parent] = local_uf_find(parent, i);
        g = local_value_index(r, root_ids);
        if g == 0
            root_ids{end+1} = r; %#ok<AGROW>
            g = numel(root_ids);
        end
        group_of(i) = g;
    end
    n_groups = numel(root_ids);

    labels = cell(1, n);
    groups = repmat(struct('group_id', '', 'member_ids', {cell(1, 0)}, ...
        'digest_source', ''), 1, n_groups);
    for g = 1:n_groups
        mem = cell(1, 0);
        dk = cell(1, 0);
        for i = 1:n
            if group_of(i) == g
                mem{end+1} = ids{i}; %#ok<AGROW>
                dk{end+1} = dkeys{i}; %#ok<AGROW>
            end
        end
        ukeys = unique(dk);
        src = strjoin(ukeys, '|');
        gid = ['SG#', local_short_digest(src)];
        row = groups(g);
        row.group_id = gid;
        row.member_ids = mem;
        row.digest_source = src;
        groups(g) = row;
        for i = 1:n
            if group_of(i) == g
                labels{i} = gid;
            end
        end
    end

    res = struct();
    res.labels = labels;
    res.groups = groups;
    res.n_groups = n_groups;
    res.n_edges = n_edges;
    res.node_count = n;
end

function [r, parent] = local_uf_find(parent, x)
% Union-find find with path compression (parent returned by value).
    root = x;
    while parent(root) ~= root
        root = parent(root);
    end
    while parent(x) ~= root
        nxt = parent(x);
        parent(x) = root;
        x = nxt;
    end
    r = root;
end

function lst = local_closure_ids(v, what)
    lst = local_closure_keys(v, what);
    if numel(unique(lst)) ~= numel(lst)
        error('GATE:MISSION_V3:FAMILY:ARG', '%s contains duplicates', what);
    end
end

function lst = local_closure_keys(v, what)
% Format-only validation (uniqueness is enforced for node ids only; digest
% keys may repeat across members of the same lineage).
    if ~iscell(v)
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            '%s must be a cell array of id strings', what);
    end
    lst = cell(1, numel(v));
    for k = 1:numel(v)
        e = v{k};
        if ~ischar(e) && ~isstring(e)
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                '%s element %d must be a string', what, k);
        end
        s = char(e);
        if isempty(s) || ~isempty(regexp(s, '[|\s]', 'once'))
            error('GATE:MISSION_V3:FAMILY:ARG', ...
                '%s element %d must be non-empty without "|" or whitespace', ...
                what, k);
        end
        lst{k} = s;
    end
    if isempty(lst)
        error('GATE:MISSION_V3:FAMILY:ARG', '%s must be non-empty', what);
    end
end

function idx = local_id_index(s, ids, edge_no)
    for k = 1:numel(ids)
        if strcmp(ids{k}, s)
            idx = k;
            return;
        end
    end
    error('GATE:MISSION_V3:FAMILY:ARG', ...
        'edge %d endpoint not in ids: %s', edge_no, s);
end

function idx = local_value_index(v, lst)
    idx = 0;
    for k = 1:numel(lst)
        if lst{k} == v
            idx = k;
            return;
        end
    end
end

function s = local_id_field(v, what)
    if ~ischar(v) && ~isstring(v)
        error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
            '%s must be a string', what);
    end
    s = char(v);
    if isempty(s) || ~isempty(regexp(s, '[|\s]', 'once'))
        error('GATE:MISSION_V3:FAMILY:REGISTRY', ...
            '%s must be non-empty without "|" or whitespace', what);
    end
end

function lst = local_cellstr_arg(v, what)
    if ischar(v)
        lst = {char(v)};
    elseif isstring(v) && isscalar(v)
        lst = {char(v)};
    elseif iscell(v)
        lst = cell(1, numel(v));
        for k = 1:numel(v)
            e = v{k};
            if ~ischar(e) && ~isstring(e)
                error('GATE:MISSION_V3:FAMILY:ARG', ...
                    '%s element %d must be a string', what, k);
            end
            lst{k} = char(e);
        end
    else
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            '%s must be a cellstr', what);
    end
    if isempty(lst)
        error('GATE:MISSION_V3:FAMILY:ARG', ...
            '%s must be non-empty', what);
    end
end

function local_check_dag_lineage(std_reg, dag_lin)
    if ~isstruct(dag_lin) || numel(dag_lin) ~= numel(std_reg)
        error('GATE:MISSION_V3:FAMILY:DAG_LINEAGE', ...
            'dag.lineage size differs from the derived standard registry');
    end
    for k = 1:numel(std_reg)
        a = std_reg(k);
        b = dag_lin(k);
        if ~strcmp(a.branch_name, b.branch_name) ...
                || ~strcmp(a.lineage_id, b.lineage_id)
            error('GATE:MISSION_V3:FAMILY:DAG_LINEAGE', ...
                ['dag.lineage row %d does not re-derive from the frozen ', ...
                 'policy (base draw record integrity, FR-522)'], k);
        end
    end
end

function names = local_truth_names(std_reg)
    names = cell(1, 0);
    for k = 1:numel(std_reg)
        row = std_reg(k);
        if ~strcmp(row.branch_name, 'forecast_obs_noise')
            names{end+1} = row.branch_name; %#ok<AGROW>
        end
    end
end

function row = local_find_branch(reg, name)
    for k = 1:numel(reg)
        e = reg(k);
        if strcmp(e.branch_name, name)
            row = e;
            return;
        end
    end
    error('GATE:MISSION_V3:FAMILY:LINEAGE', ...
        'registry lacks branch: %s', name);
end

function out = local_remove_branch(reg, name)
    keep = true(1, numel(reg));
    for k = 1:numel(reg)
        e = reg(k);
        if strcmp(e.branch_name, name)
            keep(k) = false;
        end
    end
    out = reg(keep);
end

function out = local_upsert_branch(reg, row)
    for k = 1:numel(reg)
        e = reg(k);
        if strcmp(e.branch_name, row.branch_name)
            error('GATE:MISSION_V3:FAMILY:LINEAGE', ...
                'member registry already has branch: %s', row.branch_name);
        end
    end
    out = [reg, row];
end

function h = local_short_digest(s)
    h = local_sha256_text(s);
    h = h(1:16);
end

function h = local_sha256_text(s)
% SHA-256 lowercase hex of the UTF-8 bytes of s (java digest pipeline, same
% pattern as seed_tree.m / validate_registries.m).
    bytes = unicode2native(s, 'UTF-8');
    md = java.security.MessageDigest.getInstance('SHA-256');
    dg = typecast(md.digest(typecast(bytes(:), 'int8')), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function pol = local_policy_ref()
% Read policy_version + self_hash from the frozen seed tree policy for the
% manifest provenance reference (fixed decision 7).  jsondecode is the
% sanctioned consumption parser (seed_tree.m precedent: the frozen file is
% validator-clean via mission_v3.validate_registries).
    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    ppath = fullfile(ROOT, 'config', 'mission_v3', 'seed_tree_policy.json');
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:FAMILY:POLICY', ...
            'seed tree policy file not found');
    end
    try
        obj = jsondecode(fileread(ppath));
    catch dec_err
        error('GATE:MISSION_V3:FAMILY:POLICY', ...
            'seed tree policy jsondecode failed: %s', dec_err.message);
    end
    if ~isstruct(obj) || ~isfield(obj, 'policy_version') ...
            || ~isfield(obj, 'self_hash')
        error('GATE:MISSION_V3:FAMILY:POLICY', ...
            'seed tree policy lacks policy_version or self_hash');
    end
    sh = obj.self_hash;
    if ~isstruct(sh) || ~isfield(sh, 'value')
        error('GATE:MISSION_V3:FAMILY:POLICY', ...
            'seed tree policy self_hash lacks value');
    end
    pol = struct();
    pol.policy_version = char(obj.policy_version);
    pol.self_hash = char(sh.value);
end

function res = seed_tree(mode, varargin)
% mission_v3.seed_tree - T-008 scientific seed tree (DGN-07 / FR-532, R6)
%
%   r   = mission_v3.seed_tree('lineage', policyVersion, formalSeed, baseDrawIndex, branchName)
%   reg = mission_v3.seed_tree('branches', formalSeed, baseDrawIndex)
%   reg = mission_v3.seed_tree('branches', formalSeed, baseDrawIndex, axes)
%   res = mission_v3.seed_tree('verify', registry)
%   mem = mission_v3.seed_tree('member_branches', formalSeed, baseDrawIndex, interventionType, axes)
%
% Authoritative lineage (frozen policy lineage_algorithm, verbatim):
%   lineage_id = SHA256(UTF8(policy_version + '|' + formal_generation_seed
%                             + '|' + base_draw_index + '|' + branch_name))
%   lowercase hex, 64 chars.  Separator is ASCII '|'.  policy_version must
%   equal the value in the frozen policy file
%   config/mission_v3/seed_tree_policy.json ("seed_tree_v1"); the branch
%   list is read from that file (machine registry, never hard-coded).
%
% Engine mapping (DGN-07 R6): the uint32 engine seed is the FIRST FOUR
% DIGEST BYTES interpreted big-endian.  It is ONLY the
% RandStream('Threefry4x64','Seed',s) input.  The scientific lineage
% identity is the full lineage_id; scientific lineage identity != 32-bit
% engine seed.
%
% branch_path convention: '<formal_seed>/<base_draw_index>/<branch_name>'.
% formalSeed must match [0-9A-Za-z_.:-]+; baseDrawIndex must be a whole
% number >= 0 (canonical decimal string); branch/axis names must match
% [A-Za-z0-9_.-]+ so branch_path stays parseable ('@' and 'intervention_'
% prefixes are reserved for member instances below).
%
% Member semantics (policy invariants, machine implemented):
%   REGISTRY / 'branches': all policy branches (standard lineage) plus, if
%     axes are given, one 'intervention_<axis>' branch per declared axis.
%   'member_branches' PHYSICAL: all policy branches standard PLUS one
%     'intervention_<axis>' branch per declared axis.  Versus the truth
%     member, only the declared intervention branches differ.
%   'member_branches' INFORMATION: all truth policy branches standard; the
%     'forecast_obs_noise' branch is REPLACED by one member-specific
%     instance 'forecast_obs_noise@<axis>' per declared observation axis.
%     Two INFORMATION members share every truth branch lineage and differ
%     exactly in their forecast_obs_noise instances (FR-529 truth-hash
%     basis; truth-hash comparison itself is T-014 consumption).
%
% 'verify' is fail-closed: duplicate engine seed across different
% branch_path, duplicate lineage_id, duplicate branch_path, missing or
% malformed fields, or any row whose (branch_path -> lineage_id ->
% engine_seed_uint32) mapping does not re-derive from the frozen policy
% raise error('GATE:MISSION_V3:SEED:<CLASS>', ...).  Collision class:
% GATE:MISSION_V3:SEED:COLLISION.
%
% Reproducibility: pure derivation, no global RNG touched, no hidden seed.
% ROOT is resolved from mfilename with pwd fallback; policy path is the
% relative config/mission_v3/seed_tree_policy.json.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% thin main + local functions (style ref: +mission_v3/validate_registries.m).

    if ~ischar(mode) && ~isstring(mode)
        error('GATE:MISSION_V3:SEED:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'lineage'
            if nargin ~= 5
                error('GATE:MISSION_V3:SEED:ARG', ...
                    '''lineage'' expects 4 arguments: policyVersion, formalSeed, baseDrawIndex, branchName');
            end
            res = local_lineage_entry(varargin{1}, varargin{2}, varargin{3}, varargin{4});
        case 'branches'
            if nargin == 3
                axes_in = {};
            elseif nargin == 4
                axes_in = local_axes_cell(varargin{3}, 'axes');
            else
                error('GATE:MISSION_V3:SEED:ARG', ...
                    '''branches'' expects: formalSeed, baseDrawIndex [, axes]');
            end
            res = local_registry(varargin{1}, varargin{2}, axes_in, 'REGISTRY');
        case 'verify'
            if nargin ~= 2
                error('GATE:MISSION_V3:SEED:ARG', ...
                    '''verify'' expects 1 argument: registry');
            end
            res = local_verify(varargin{1});
        case 'member_branches'
            if nargin ~= 5
                error('GATE:MISSION_V3:SEED:ARG', ...
                    '''member_branches'' expects: formalSeed, baseDrawIndex, interventionType, axes');
            end
            itype = local_itype(varargin{3});
            axes_in = local_axes_cell(varargin{4}, 'axes');
            res = local_registry(varargin{1}, varargin{2}, axes_in, itype);
        otherwise
            error('GATE:MISSION_V3:SEED:ARG', 'unknown mode: %s', char(mode));
    end
end

%% ======================== local functions ==========================

function pol = local_policy()
% Load the frozen seed tree policy (single source of the branch list and
% policy_version).  jsondecode is allowed here per delegation spec A: this
% file has a plain string array and no duplicate-key / number-literal risk.
    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    ppath = fullfile(ROOT, 'config', 'mission_v3', 'seed_tree_policy.json');
    if ~exist(ppath, 'file')
        error('GATE:MISSION_V3:SEED:POLICY', ...
            'seed tree policy file not found: %s', ppath);
    end
    try
        obj = jsondecode(fileread(ppath));
    catch dec_err
        error('GATE:MISSION_V3:SEED:POLICY', ...
            'seed tree policy jsondecode failed: %s', dec_err.message);
    end
    if ~isstruct(obj) || ~isfield(obj, 'policy_version') || ~isfield(obj, 'branches')
        error('GATE:MISSION_V3:SEED:POLICY', ...
            'seed tree policy lacks policy_version or branches');
    end
    pv = obj.policy_version;
    if ~ischar(pv) && ~isstring(pv)
        error('GATE:MISSION_V3:SEED:POLICY', 'policy_version must be a string');
    end
    pol = struct();
    pol.policy_version = char(pv);
    pol.branches = local_axes_cell(obj.branches, 'policy branches');
end

function out = local_registry(formalSeed, baseDrawIndex, axes_in, itype)
% Build the full branch registry (REGISTRY) or a family member map
% (PHYSICAL / INFORMATION) as a struct array.
    pol = local_policy();
    seed_str = local_canon_seed(formalSeed);
    draw_str = local_canon_draw(baseDrawIndex);

    names = pol.branches;
    is_interv = false(1, numel(names));
    if strcmp(itype, 'INFORMATION')
        noise_branch = 'forecast_obs_noise';
        if ~ismember(noise_branch, names)
            error('GATE:MISSION_V3:SEED:POLICY', ...
                'INFORMATION invariant requires a forecast_obs_noise policy branch');
        end
        keep = ~strcmp(names, noise_branch);
        names = names(keep);
        is_interv = is_interv(keep);
        n_ax = numel(axes_in);
        inst = cell(1, n_ax);
        for a = 1:n_ax
            inst{a} = ['forecast_obs_noise@', axes_in{a}];
        end
        names = [names, inst];
        is_interv = [is_interv, true(1, n_ax)];
    end
    if ~isempty(axes_in)
        n_ax = numel(axes_in);
        ivn = cell(1, n_ax);
        for a = 1:n_ax
            ivn{a} = ['intervention_', axes_in{a}];
        end
        names = [names, ivn];
        is_interv = [is_interv, true(1, n_ax)];
    end

    n = numel(names);
    entries = repmat(local_blank_entry(), 1, n);
    for k = 1:n
        lin = local_lineage_core(pol.policy_version, seed_str, draw_str, names{k});
        e = local_blank_entry();
        e.branch_path = [seed_str, '/', draw_str, '/', names{k}];
        e.branch_name = names{k};
        e.lineage_id = lin.lineage_id;
        e.engine_seed_uint32 = lin.engine_seed_uint32;
        e.is_intervention = is_interv(k);
        entries(k) = e;
    end
    out = entries;
end

function e0 = local_blank_entry()
    e0 = struct('branch_path', '', 'branch_name', '', 'lineage_id', '', ...
        'engine_seed_uint32', uint32(0), 'is_intervention', false);
end

function entry = local_lineage_entry(policyVersion, formalSeed, baseDrawIndex, branchName)
% 'lineage' mode: derive a single branch entry; policyVersion must match
% the frozen policy file (fail-closed binding to the machine registry).
    pol = local_policy();
    if ~ischar(policyVersion) && ~isstring(policyVersion)
        error('GATE:MISSION_V3:SEED:ARG', 'policyVersion must be a string');
    end
    pv = char(policyVersion);
    if ~strcmp(pv, pol.policy_version)
        error('GATE:MISSION_V3:SEED:ARG', ...
            'policyVersion "%s" deviates from frozen policy file value "%s"', ...
            pv, pol.policy_version);
    end
    seed_str = local_canon_seed(formalSeed);
    draw_str = local_canon_draw(baseDrawIndex);
    name = local_one_name(branchName, 'branchName');
    lin = local_lineage_core(pv, seed_str, draw_str, name);
    entry = local_blank_entry();
    entry.branch_path = [seed_str, '/', draw_str, '/', name];
    entry.branch_name = name;
    entry.lineage_id = lin.lineage_id;
    entry.engine_seed_uint32 = lin.engine_seed_uint32;
    entry.is_intervention = false;
end

function lin = local_lineage_core(policy_version, seed_str, draw_str, name)
% The frozen formula, verbatim:
%   SHA256(UTF8(policy_version + '|' + formal_generation_seed + '|'
%           + base_draw_index + '|' + branch_name))
% Engine seed = first 4 digest bytes, big-endian (engine input only; the
% scientific lineage identity is the full digest).
    sep = '|';
    s = [policy_version, sep, seed_str, sep, draw_str, sep, name];
    bytes = unicode2native(s, 'UTF-8');
    md = java.security.MessageDigest.getInstance('SHA-256');
    dg = typecast(md.digest(typecast(bytes(:), 'int8')), 'uint8');
    lin = struct();
    lin.lineage_id = lower(reshape(dec2hex(dg, 2)', 1, []));
    e = double(dg(1)) * 16777216 + double(dg(2)) * 65536 ...
        + double(dg(3)) * 256 + double(dg(4));
    lin.engine_seed_uint32 = uint32(e);
end

function res = local_verify(registry)
% Fail-closed registry validation: field/format checks, duplicate checks,
% then full re-derivation of every row from branch_path under the frozen
% policy.  Any violation raises GATE:MISSION_V3:SEED:<CLASS>.
    if ~isstruct(registry) || isempty(registry)
        error('GATE:MISSION_V3:SEED:REGISTRY_FIELD', ...
            'registry must be a non-empty struct array');
    end
    need = {'branch_path', 'lineage_id', 'engine_seed_uint32'};
    fn = fieldnames(registry);
    for j = 1:numel(need)
        if ~ismember(need{j}, fn)
            error('GATE:MISSION_V3:SEED:REGISTRY_FIELD', ...
                'registry missing required field: %s', need{j});
        end
    end

    n = numel(registry);
    paths = cell(1, n);
    lins = cell(1, n);
    seeds = zeros(1, n);
    for i = 1:n
        row = registry(i);
        if ~ischar(row.branch_path) && ~isstring(row.branch_path)
            error('GATE:MISSION_V3:SEED:REGISTRY_FIELD', ...
                'row %d: branch_path must be a string', i);
        end
        if ~ischar(row.lineage_id) && ~isstring(row.lineage_id)
            error('GATE:MISSION_V3:SEED:REGISTRY_FIELD', ...
                'row %d: lineage_id must be a string', i);
        end
        lin_s = char(row.lineage_id);
        if isempty(regexp(lin_s, '^[0-9a-f]{64}$', 'once'))
            error('GATE:MISSION_V3:SEED:REGISTRY_FIELD', ...
                'row %d: lineage_id is not 64 lowercase hex chars', i);
        end
        v = row.engine_seed_uint32;
        if ~isnumeric(v) || ~isscalar(v) || ~isfinite(v) ...
                || v < 0 || v > 4294967295 || v ~= floor(v)
            error('GATE:MISSION_V3:SEED:REGISTRY_FIELD', ...
                'row %d: engine_seed_uint32 must be an integer in [0, 2^32-1]', i);
        end
        paths{i} = char(row.branch_path);
        lins{i} = lin_s;
        seeds(i) = double(v);
    end

    if numel(unique(paths)) ~= n
        error('GATE:MISSION_V3:SEED:PATH_DUP', ...
            'duplicate branch_path in registry (%d rows, %d unique paths)', ...
            n, numel(unique(paths)));
    end
    if numel(unique(lins)) ~= n
        error('GATE:MISSION_V3:SEED:LINEAGE_DUP', ...
            'duplicate lineage_id in registry (%d rows, %d unique lineages)', ...
            n, numel(unique(lins)));
    end
    if numel(unique(seeds)) ~= n
        err_id = 'GATE:MISSION_V3:SEED:COLLISION';
        error(err_id, ...
            'seed collision: %d rows map onto %d distinct engine seeds (fail-closed)', ...
            n, numel(unique(seeds)));
    end

    pol = local_policy();
    for i = 1:n
        row = registry(i);
        path_s = char(row.branch_path);
        parts = strsplit(path_s, '/');
        if numel(parts) ~= 3 || isempty(parts{1}) || isempty(parts{2}) ...
                || isempty(parts{3})
            error('GATE:MISSION_V3:SEED:DERIVATION', ...
                'row %d: branch_path must be <seed>/<draw>/<branch>: %s', ...
                i, path_s);
        end
        if isempty(regexp(parts{2}, '^[0-9]+$', 'once'))
            error('GATE:MISSION_V3:SEED:DERIVATION', ...
                'row %d: base_draw_index component not canonical: %s', ...
                i, parts{2});
        end
        lin2 = local_lineage_core(pol.policy_version, parts{1}, parts{2}, parts{3});
        want_path = [parts{1}, '/', parts{2}, '/', parts{3}];
        if ~strcmp(lin2.lineage_id, lins{i}) ...
                || lin2.engine_seed_uint32 ~= row.engine_seed_uint32 ...
                || ~strcmp(want_path, path_s)
            error('GATE:MISSION_V3:SEED:DERIVATION', ...
                'row %d (%s): lineage/seed does not re-derive from the frozen policy', ...
                i, path_s);
        end
    end

    res = struct();
    res.ok = true;
    res.n_entries = n;
    res.policy_version = pol.policy_version;
    res.n_intervention = 0;
    if ismember('is_intervention', fn)
        cnt = 0;
        for i = 1:n
            row = registry(i);
            if row.is_intervention
                cnt = cnt + 1;
            end
        end
        res.n_intervention = cnt;
    end
end

function t = local_itype(v)
% Normalize the intervention type: REGISTRY members are INFORMATION-legal
% only via 'member_branches'; here only INFORMATION / PHYSICAL are valid.
    if ~ischar(v) && ~isstring(v)
        error('GATE:MISSION_V3:SEED:ARG', 'interventionType must be a string');
    end
    t = upper(char(v));
    if ~ismember(t, {'INFORMATION', 'PHYSICAL'})
        error('GATE:MISSION_V3:SEED:ARG', ...
            'interventionType must be INFORMATION or PHYSICAL, got: %s', t);
    end
end

function lst = local_axes_cell(v, what)
% Normalize a policy branch list or caller axes into a validated unique
% non-empty cellstr of machine names.
    if ischar(v)
        lst = {char(v)};
    elseif isstring(v) && numel(v) == 1
        lst = {char(v)};
    elseif iscell(v)
        lst = cell(1, numel(v));
        for k = 1:numel(v)
            e = v{k};
            if ~ischar(e) && ~isstring(e)
                error('GATE:MISSION_V3:SEED:ARG', ...
                    '%s element %d must be a string', what, k);
            end
            lst{k} = char(e);
        end
    else
        error('GATE:MISSION_V3:SEED:ARG', '%s must be a cellstr', what);
    end
    if isempty(lst)
        error('GATE:MISSION_V3:SEED:ARG', '%s must be non-empty', what);
    end
    for k = 1:numel(lst)
        lst{k} = local_one_name(lst{k}, sprintf('%s element %d', what, k));
    end
    if numel(unique(lst)) ~= numel(lst)
        error('GATE:MISSION_V3:SEED:ARG', '%s contains duplicates', what);
    end
end

function s = local_one_name(v, what)
% Machine name for the digest input and path: ASCII letters/digits/_/./-;
% '|', '/', '@' and whitespace are reserved (separators and member tags).
    if ~ischar(v) && ~isstring(v)
        error('GATE:MISSION_V3:SEED:ARG', '%s must be a string', what);
    end
    s = char(v);
    if isempty(regexp(s, '^[A-Za-z0-9_.-]+$', 'once'))
        error('GATE:MISSION_V3:SEED:ARG', ...
            '%s must match [A-Za-z0-9_.-]+, got: %s', what, s);
    end
end

function s = local_canon_seed(v)
% formal_generation_seed canonical form: printable machine token.
    if ~ischar(v) && ~isstring(v)
        error('GATE:MISSION_V3:SEED:ARG', ...
            'formalSeed must be a string (frozen as text), got class: %s', class(v));
    end
    s = char(v);
    if isempty(regexp(s, '^[0-9A-Za-z_.:-]+$', 'once'))
        error('GATE:MISSION_V3:SEED:ARG', ...
            'formalSeed must match [0-9A-Za-z_.:-]+, got: %s', s);
    end
end

function s = local_canon_draw(v)
% base_draw_index canonical decimal form (whole number >= 0).
    if ~isnumeric(v) || ~isscalar(v) || ~isfinite(v) ...
            || v < 0 || v ~= floor(v) || v > 2^53
        error('GATE:MISSION_V3:SEED:ARG', ...
            'baseDrawIndex must be a whole number >= 0');
    end
    s = sprintf('%d', v);
end

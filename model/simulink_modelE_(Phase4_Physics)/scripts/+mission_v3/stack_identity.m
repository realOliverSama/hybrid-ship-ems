function man = stack_identity(mode, a1, a2, varargin)
% mission_v3.stack_identity - Phase 4-C T-016: qualified generation-stack
% identity (design.md DGN-14 / FR-534; delegation spec
% experiments/mission_v3/T-016_delegation_spec.md).
%
%   man = mission_v3.stack_identity('collect', ROOT, ...)
%   man = mission_v3.stack_identity('verify',  MAN, ROOT, ...)
%
% Ten frozen components (DGN-14; T-016 FD1), each recorded as
% {component, version, sha256, source_path, status, provenance}:
%   mission_v3_generator       composite SHA256 over scripts/+mission_v3/*.m
%                              TOP-LEVEL package functions (FD2 hash-domain
%                              ruling: non-recursive; tests/ subdirectory and
%                              +audits/ audit assets excluded - verification
%                              assets, not the scientific generation chain)
%   mission_template_registry  config/mission_v3/mission_template_registry.json
%   conditional_dag_config     config/mission_v3/conditional_dag_config.json
%   seed_tree_policy           config/mission_v3/seed_tree_policy.json
%   forecast_generator         scripts/+mission_v3/forecast_issue.m (T-012)
%   scenario_stratum_registry  config/mission_v3/scenario_stratum_registry.json
%   heldout_token_registry     config/mission_v3/heldout_token_registry.json
%   modele_physical_baseline   config/gate_baseline_modele_candidate_v1_1.json (v1.1)
%   modele_control_baseline    config/gate_baseline_modele_control.json (v1.0)
%   d_producer_schema          config/dataset_v3_producer_schema.json; the
%                              sha256 field records the embedded
%                              self_hash.value (canonical JSON, D convention)
%
% Composite hash = SHA256(canonical JSON of {component: {version, sha256}})
% (FD4). Canonical encoding follows the D convention used by
% +mission_v3/validate_registries.m (local_canonical_json): sort_keys, compact
% separators, ensure_ascii=False, UTF-8 bytes; a minimal encoder for this
% exact data shape is embedded below.
%
% The two baseline components reuse the item-(10) baseline integrity
% mechanism of scripts/72_run_cd_interface_gate.m: per-file raw-byte SHA-256
% with MISSING tolerance (FD3); embedded version fields are additionally
% recorded.
%
% 'collect' name-values:
%   'GeneratorDir'  DIR  override for scripts/+mission_v3 (fixture use)
%   'RegistriesDir' DIR  override for config/mission_v3        (fixture use)
%   'Strict'        LOGICAL, default true. When forecast_issue.m is absent
%                   (T-012 in flight): Strict=true -> MISSING FAIL;
%                   Strict=false -> the item is recorded PENDING
%                   (version='pending', sha256='') and the composite is still
%                   produced (FD3/FD5). Missing files of all other components
%                   always FAIL MISSING, regardless of Strict.
%   'OutPath'       PATH  optional pretty-printed JSON manifest sink
%                         (collect only)
% 'verify' name-values:
%   'GeneratorDir' / 'RegistriesDir' overrides (fixture use)
%
% verify() re-collects the current stack and compares version+sha256 per
% component plus the composite hash against the manifest; any deviation
% raises error('GATE:MISSION_V3:STACK:MISMATCH') (qualified_stack !=
% current stack => qualification invalid, machine-enforced). Required files
% absent at verify time raise error('GATE:MISSION_V3:STACK:MISSING').
% A PENDING manifest entry for forecast_generator is tolerated only against
% an equally PENDING re-collection (PENDING==PENDING); the appearance of the
% file is a MISMATCH (fail-closed).
%
% Read-only collection: no hashed file is ever modified. Manifests carry no
% absolute paths; source_path labels use the canonical repo layout.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% ROOT via mfilename + pwd fallback (AGENTS.md 20.2).

    % ---- mode dispatch and positional args ------------------------------
    if nargin < 1 || ~ischar(mode)
        error('GATE:MISSION_V3:STACK:BAD_ARG', 'mode (char) is required');
    end
    is_collect = strcmp(mode, 'collect');
    is_verify = strcmp(mode, 'verify');
    if ~is_collect && ~is_verify
        error('GATE:MISSION_V3:STACK:BAD_ARG', ...
            'mode must be ''collect'' or ''verify''');
    end
    if is_verify
        man_in = a1;
        root_in = a2;
        if nargin < 3
            error('GATE:MISSION_V3:STACK:BAD_ARG', ...
                'verify requires (MANIFEST, ROOT)');
        end
        opt_args = varargin;
    else
        man_in = [];
        root_in = a1;
        if nargin < 2
            error('GATE:MISSION_V3:STACK:BAD_ARG', 'collect requires ROOT');
        end
        % in collect mode the first name-value pair lands in a2
        opt_args = {};
        if nargin >= 3
            opt_args = [{a2}, varargin];
        end
    end

    % ---- name-value options ---------------------------------------------
    opt = local_parse_opts(opt_args{:});
    if is_verify && ~isempty(opt.out_path)
        error('GATE:MISSION_V3:STACK:BAD_ARG', ...
            'OutPath is a collect-only option');
    end

    root = local_resolve_root(root_in);

    % ---- verify preparation ---------------------------------------------
    if is_verify
        local_check_manifest(man_in);
        old_map = struct();
        mc = man_in.components;
        for i = 1:numel(mc)
            c = mc(i);
            old_map.(c.component) = c;
        end
        old_fc_pending = isfield(old_map, 'forecast_generator') ...
            && local_is_pending(old_map.forecast_generator);
    else
        old_map = struct();
        old_fc_pending = false;
    end

    % ---- component collection -------------------------------------------
    gen_dir = opt.generator_dir;
    if isempty(gen_dir)
        gen_dir = fullfile(root, 'scripts', '+mission_v3');
    end
    reg_dir = opt.registries_dir;
    if isempty(reg_dir)
        reg_dir = fullfile(root, 'config', 'mission_v3');
    end

    specs = local_component_specs();
    comps = cell(1, 0);
    gen_files = cell(1, 0);
    for i = 1:numel(specs)
        sp = specs(i);
        switch sp.kind
            case 'package'
                [comp, gen_files] = local_collect_package(sp, gen_dir);
            case 'mfile'
                comp = local_collect_mfile(sp, gen_dir, mode, opt.strict, ...
                    old_fc_pending);
            case 'json'
                comp = local_collect_json(sp, root, reg_dir);
            otherwise
                error('GATE:MISSION_V3:STACK:BAD_ARG', ...
                    'unknown component kind: %s', sp.kind);
        end
        comps{end + 1} = comp; %#ok<SAGROW>
    end
    comps = [comps{:}];

    % ---- generator_hash_domain ruling field (FD2, Tasks Gate R6) ---------
    ghd = struct();
    ghd.component = 'mission_v3_generator';
    ghd.ruling = ['FD2 (Tasks Gate R6): the mission_v3_generator composite ', ...
        'hash domain = top-level package functions scripts/+mission_v3/*.m ', ...
        '(non-recursive); the tests/ subdirectory and the +audits/ audit ', ...
        'assets are excluded'];
    ghd.rationale = ['test files are verification assets, not scientific ', ...
        'generation logic; including them would freeze the generator ', ...
        'identity on test additions and block T-017/T-018 test growth; ', ...
        '+audits/ members produce evidence, not the generation chain'];
    ghd.included_files = {gen_files{:}};
    ghd.excluded_dirs = {'tests', '+audits'};

    composite_hash = local_composite(comps);
    pending_names = local_pending_names(comps);

    if is_verify
        man = local_verify(man_in, comps, ghd, composite_hash, old_map, specs);
        return;
    end

    % ---- manifest assembly ----------------------------------------------
    man = struct();
    man.schema = 'mission_v3_stack_identity_v1';
    man.mode = 'collect';
    man.generated_at = char(datetime('now', 'TimeZone', 'local', ...
        'Format', 'yyyy-MM-dd''T''HH:mm:ssZ'));
    man.components = comps;
    man.generator_hash_domain = ghd;
    man.pending = {pending_names{:}};
    man.composite_hash = composite_hash;

    if ~isempty(opt.out_path)
        local_write_manifest(man, opt.out_path);
    end
end

%% ======================== local functions ==========================

function opt = local_parse_opts(varargin)
    opt.generator_dir = '';
    opt.registries_dir = '';
    opt.strict = true;
    opt.out_path = '';
    k = 1;
    while k <= numel(varargin)
        if k + 1 > numel(varargin)
            error('GATE:MISSION_V3:STACK:BAD_ARG', ...
                'missing value for option %s', string(varargin{k}));
        end
        o = varargin{k};
        v = varargin{k + 1};
        if strcmpi(o, 'GeneratorDir')
            opt.generator_dir = char(v);
        elseif strcmpi(o, 'RegistriesDir')
            opt.registries_dir = char(v);
        elseif strcmpi(o, 'Strict')
            opt.strict = logical(v);
        elseif strcmpi(o, 'OutPath')
            opt.out_path = char(v);
        else
            error('GATE:MISSION_V3:STACK:BAD_ARG', ...
                'unknown option: %s', string(o));
        end
        k = k + 2;
    end
end

function root = local_resolve_root(root_in)
% ROOT via mfilename + pwd fallback (AGENTS.md 20.2). Package function depth:
% stack_identity.m -> +mission_v3 -> scripts -> workspace root.
    root = char(root_in);
    if isempty(root)
        root = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    if isempty(root) || exist(fullfile(root, 'config'), 'dir') ~= 7
        root = pwd;
    end
end

function specs = local_component_specs()
% DGN-14 frozen ten-component list (delegation spec T-016 FD1 provenance).
    tpl = struct('name', '', 'rel_dir', '', 'rel_file', '', 'kind', '', ...
        'sha_mode', 'bytes', 'dir_kind', '', 'provenance', '');
    specs = repmat(tpl, 1, 10);
    s = specs;
    s(1).name = 'mission_v3_generator';
    s(1).rel_dir = 'scripts/+mission_v3';
    s(1).rel_file = '';
    s(1).kind = 'package';
    s(1).provenance = ['DGN-14 item 1; FD2 hash-domain ruling (Tasks Gate ', ...
        'R6): top-level package functions only; tests/ and +audits/ ', ...
        'excluded (verification assets, not the generation chain)'];
    s(2).name = 'mission_template_registry';
    s(2).rel_dir = 'config/mission_v3';
    s(2).rel_file = 'mission_template_registry.json';
    s(2).kind = 'json';
    s(2).dir_kind = 'registries';
    s(2).provenance = ...
        'DGN-14 item 2; T-001 frozen mission template registry (DGN-02 triple)';
    s(3).name = 'conditional_dag_config';
    s(3).rel_dir = 'config/mission_v3';
    s(3).rel_file = 'conditional_dag_config.json';
    s(3).kind = 'json';
    s(3).dir_kind = 'registries';
    s(3).provenance = ...
        'DGN-14 item 3; T-001 frozen conditional DAG config (DGN-03 node table)';
    s(4).name = 'seed_tree_policy';
    s(4).rel_dir = 'config/mission_v3';
    s(4).rel_file = 'seed_tree_policy.json';
    s(4).kind = 'json';
    s(4).dir_kind = 'registries';
    s(4).provenance = 'DGN-14 item 4; T-001 frozen seed tree policy (DGN-07)';
    s(5).name = 'forecast_generator';
    s(5).rel_dir = 'scripts/+mission_v3';
    s(5).rel_file = 'forecast_issue.m';
    s(5).kind = 'mfile';
    s(5).provenance = ['DGN-14 item 5; T-012 forecast generator; PENDING ', ...
        'allowed while T-012 is in flight (FD3)'];
    s(6).name = 'scenario_stratum_registry';
    s(6).rel_dir = 'config/mission_v3';
    s(6).rel_file = 'scenario_stratum_registry.json';
    s(6).kind = 'json';
    s(6).dir_kind = 'registries';
    s(6).provenance = 'DGN-14 item 6; T-002 frozen stratum registry (DGN-13)';
    s(7).name = 'heldout_token_registry';
    s(7).rel_dir = 'config/mission_v3';
    s(7).rel_file = 'heldout_token_registry.json';
    s(7).kind = 'json';
    s(7).dir_kind = 'registries';
    s(7).provenance = 'DGN-14 item 7; T-002 frozen heldout token registry (DGN-11)';
    s(8).name = 'modele_physical_baseline';
    s(8).rel_dir = 'config';
    s(8).rel_file = 'gate_baseline_modele_candidate_v1_1.json';
    s(8).kind = 'json';
    s(8).dir_kind = 'config';
    s(8).provenance = ['DGN-14 item 8; Model E frozen physical baseline ', ...
        'v1.1; 72 item-(10) baseline mechanism reuse (FD3)'];
    s(9).name = 'modele_control_baseline';
    s(9).rel_dir = 'config';
    s(9).rel_file = 'gate_baseline_modele_control.json';
    s(9).kind = 'json';
    s(9).dir_kind = 'config';
    s(9).provenance = ['DGN-14 item 9; Model E frozen control baseline ', ...
        'v1.0; 72 item-(10) baseline mechanism reuse (FD3)'];
    s(10).name = 'd_producer_schema';
    s(10).rel_dir = 'config';
    s(10).rel_file = 'dataset_v3_producer_schema.json';
    s(10).kind = 'json';
    s(10).sha_mode = 'self_hash';
    s(10).dir_kind = 'config';
    s(10).provenance = ['DGN-14 item 10; dataset v3 producer schema; ', ...
        'sha256 field records embedded self_hash.value (canonical JSON, ', ...
        'D convention)'];
    specs = s;
end

function comp = local_blank_comp(sp)
    comp = struct('component', sp.name, 'version', '', 'sha256', '', ...
        'source_path', '', 'status', '', 'provenance', sp.provenance);
    if isempty(sp.rel_file)
        comp.source_path = sp.rel_dir;
    else
        comp.source_path = [sp.rel_dir, '/', sp.rel_file];
    end
end

function [comp, files] = local_collect_package(sp, gen_dir)
% FD2: top-level package functions only. dir('*.m') is non-recursive by
% construction (tests/ and +audits/ are subdirectories, never listed);
% isdir filtering is kept as a defensive guard. Empty/absent dir -> MISSING.
    lst = dir(fullfile(gen_dir, '*.m'));
    files = cell(1, 0);
    hashes = cell(1, 0);
    for i = 1:numel(lst)
        if lst(i).isdir
            continue;
        end
        files{end + 1} = lst(i).name; %#ok<SAGROW>
        hashes{end + 1} = local_sha256_file(fullfile(gen_dir, lst(i).name)); %#ok<SAGROW>
    end
    if isempty(files)
        error('GATE:MISSION_V3:STACK:MISSING', ...
            'component %s: generator dir empty or absent (%s)', ...
            sp.name, sp.rel_dir);
    end
    [sn, si] = sort(files);
    sh = hashes(si);
    vals = cell(1, numel(sn));
    for i = 1:numel(sn)
        vals{i} = ['"', sh{i}, '"'];
    end
    % composite generator hash = SHA256(canonical JSON {filename: hash}) (FD3)
    canon = local_canonical_object(sn, vals);
    comp = local_blank_comp(sp);
    comp.version = 'package';
    comp.sha256 = local_sha256_bytes(unicode2native(canon, 'UTF-8'));
    comp.status = 'PRESENT';
end

function comp = local_collect_mfile(sp, gen_dir, mode, strict, old_fc_pending)
% forecast_generator: .m files carry no embedded version declaration, so the
% version field is empty when PRESENT and 'pending' when PENDING (FD3).
    abs_p = fullfile(gen_dir, sp.rel_file);
    if exist(abs_p, 'file') ~= 2
        if strcmp(mode, 'verify') && old_fc_pending
            comp = local_pending_comp(sp);
            return;
        end
        if strcmp(mode, 'collect') && ~strict
            comp = local_pending_comp(sp);
            return;
        end
        error('GATE:MISSION_V3:STACK:MISSING', ...
            'component %s missing: %s/%s', sp.name, sp.rel_dir, sp.rel_file);
    end
    comp = local_blank_comp(sp);
    comp.version = '';
    comp.sha256 = local_sha256_file(abs_p);
    comp.status = 'PRESENT';
end

function comp = local_collect_json(sp, root, reg_dir)
    if strcmp(sp.dir_kind, 'registries')
        abs_p = fullfile(reg_dir, sp.rel_file);
    else
        abs_p = fullfile(root, sp.rel_dir, sp.rel_file);
    end
    if exist(abs_p, 'file') ~= 2
        error('GATE:MISSION_V3:STACK:MISSING', ...
            'component %s missing: %s/%s', sp.name, sp.rel_dir, sp.rel_file);
    end
    try
        txt = fileread(abs_p);
    catch read_err
        error('GATE:MISSION_V3:STACK:MISSING', ...
            'component %s unreadable: %s (%s)', ...
            sp.name, sp.rel_file, read_err.message);
    end
    try
        obj = jsondecode(txt);
    catch decode_err
        error('GATE:MISSION_V3:STACK:MISSING', ...
            'component %s not parseable JSON: %s (%s)', ...
            sp.name, sp.rel_file, decode_err.message);
    end
    if ~isstruct(obj) || ~isfield(obj, 'version') || ~ischar(obj.version)
        error('GATE:MISSION_V3:STACK:MISSING', ...
            'component %s lacks a version string: %s', ...
            sp.name, sp.rel_file);
    end
    if strcmp(sp.sha_mode, 'self_hash')
        if ~isfield(obj, 'self_hash') || ~isfield(obj.self_hash, 'value') ...
                || ~ischar(obj.self_hash.value)
            error('GATE:MISSION_V3:STACK:MISSING', ...
                'component %s lacks self_hash.value: %s', ...
                sp.name, sp.rel_file);
        end
        sha = obj.self_hash.value;
    else
        sha = local_sha256_file(abs_p);
    end
    comp = local_blank_comp(sp);
    comp.version = obj.version;
    comp.sha256 = sha;
    comp.status = 'PRESENT';
end

function comp = local_pending_comp(sp)
% FD3/FD5: forecast_issue.m absent (T-012 in flight) and Strict=false -> the
% item is recorded PENDING and the composite is still produced.
    comp = local_blank_comp(sp);
    comp.version = 'pending';
    comp.sha256 = '';
    comp.status = 'PENDING';
    comp.provenance = [sp.provenance, ' | PENDING at collection time (FD3)'];
end

function tf = local_is_pending(c)
% PENDING shape per FD3: version='pending' and empty sha256.
    tf = isstruct(c) && isfield(c, 'version') && isfield(c, 'sha256') ...
        && ischar(c.version) && strcmp(c.version, 'pending') ...
        && ischar(c.sha256) && isempty(c.sha256);
end

function names = local_pending_names(comps)
    names = cell(1, 0);
    for i = 1:numel(comps)
        if local_is_pending(comps(i))
            names{end + 1} = comps(i).component; %#ok<AGROW>
        end
    end
end

function h = local_composite(comps)
% Composite hash = SHA256(canonical JSON of {component: {version, sha256}})
% (FD4). Consistency basis: +mission_v3/validate_registries.m
% local_canonical_json D convention - sort_keys, compact separators,
% ensure_ascii=False, UTF-8 bytes. The nested entries emit keys sorted
% ('sha256' < 'version') to match Python sort_keys.
    keys = cell(1, numel(comps));
    vals = cell(1, numel(comps));
    for i = 1:numel(comps)
        c = comps(i);
        keys{i} = c.component;
        vals{i} = ['{"sha256":"', local_escape(c.sha256), ...
            '","version":"', local_escape(c.version), '"}'];
    end
    canon = local_canonical_object(keys, vals);
    h = local_sha256_bytes(unicode2native(canon, 'UTF-8'));
end

function canon = local_canonical_object(keys, vals)
% Canonical JSON object from keys and pre-encoded JSON value fragments.
% Byte-identical to Python json.dumps(..., sort_keys=True,
% separators=(',',':'), ensure_ascii=False) for the same data (FD4).
    [sk, si] = sort(keys);
    for i = 2:numel(sk)
        if strcmp(sk{i}, sk{i - 1})
            error('GATE:MISSION_V3:STACK:CANON', ...
                'duplicate key in canonical object');
        end
    end
    parts = cell(1, numel(sk));
    for i = 1:numel(sk)
        parts{i} = ['"', local_escape(sk{i}), '":', vals{si(i)}];
    end
    canon = ['{', strjoin(parts, ','), '}'];
end

function out = local_escape(s)
% Minimal JSON string escaping, Python ensure_ascii=False semantics
% (same rules as +mission_v3/validate_registries.m local_encode_string).
    n = numel(s);
    out = '';
    for j = 1:n
        ch = s(j);
        code = abs(ch);
        if code == 34
            out = [out, '\"'];
        elseif code == 92
            out = [out, '\\\\'];
        elseif code == 8
            out = [out, '\b'];
        elseif code == 9
            out = [out, '\t'];
        elseif code == 10
            out = [out, '\n'];
        elseif code == 12
            out = [out, '\f'];
        elseif code == 13
            out = [out, '\r'];
        elseif code < 32
            out = [out, sprintf('\u%04x', code)];
        else
            out = [out, ch];
        end
    end
end

function h = local_sha256_bytes(bytes)
% SHA-256 hex of raw bytes (bit-preserving path, same as
% +model/sha256_file.m and validate_registries local_sha256_hex).
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(bytes(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function h = local_sha256_file(path)
% 72_run_cd_interface_gate.m item-(10) mechanism reuse (FD3): per-file
% SHA-256 over raw bytes (fread uint8 => uint8, no int8 saturation).
    fid = fopen(path, 'r');
    if fid < 0
        error('GATE:MISSION_V3:STACK:MISSING', ...
            'cannot open file for hashing: %s', path);
    end
    raw = fread(fid, inf, 'uint8=>uint8');
    fclose(fid);
    h = local_sha256_bytes(raw);
end

function local_check_manifest(man_in)
    if ~isstruct(man_in) || ~isfield(man_in, 'components') ...
            || ~isfield(man_in, 'composite_hash')
        error('GATE:MISSION_V3:STACK:BAD_MANIFEST', ...
            'manifest struct with components + composite_hash required');
    end
    if ~isfield(man_in, 'schema') ...
            || ~strcmp(man_in.schema, 'mission_v3_stack_identity_v1')
        error('GATE:MISSION_V3:STACK:BAD_MANIFEST', ...
            'unexpected manifest schema');
    end
    if ~ischar(man_in.composite_hash) ...
            || isempty(regexp(man_in.composite_hash, '^[0-9a-f]{64}$', 'once'))
        error('GATE:MISSION_V3:STACK:BAD_MANIFEST', ...
            'composite_hash is not 64 lowercase hex chars');
    end
    mc = man_in.components;
    if ~isstruct(mc)
        error('GATE:MISSION_V3:STACK:BAD_MANIFEST', ...
            'components must be a struct array');
    end
    for i = 1:numel(mc)
        c = mc(i);
        if ~isfield(c, 'component') || ~isfield(c, 'version') ...
                || ~isfield(c, 'sha256')
            error('GATE:MISSION_V3:STACK:BAD_MANIFEST', ...
                'component entry %d lacks component/version/sha256', i);
        end
    end
end

function res = local_verify(man_in, comps, ghd, composite_hash, old_map, specs)
    spec_names = cell(1, numel(specs));
    for i = 1:numel(specs)
        spec_names{i} = specs(i).name;
    end
    mismatch = cell(1, 0);
    cmps = cell(1, 0);
    for i = 1:numel(comps)
        c = comps(i);
        cmp = struct('component', c.component, 'verdict', '', ...
            'manifest_version', '', 'current_version', c.version, ...
            'manifest_sha256', '', 'current_sha256', c.sha256);
        if ~isfield(old_map, c.component)
            cmp.verdict = 'MISMATCH';
            mismatch{end + 1} = sprintf('%s: absent from manifest', ...
                c.component); %#ok<AGROW>
            cmps{end + 1} = cmp; %#ok<AGROW>
            continue;
        end
        o = old_map.(c.component);
        cmp.manifest_version = o.version;
        cmp.manifest_sha256 = o.sha256;
        if local_is_pending(o) && local_is_pending(c)
            cmp.verdict = 'MATCH_PENDING';
        elseif local_is_pending(o) ~= local_is_pending(c)
            cmp.verdict = 'MISMATCH';
            mismatch{end + 1} = sprintf( ...
                '%s: pending state changed (manifest version %s vs current %s)', ...
                c.component, o.version, c.version); %#ok<AGROW>
        elseif ~strcmp(o.version, c.version) || ~strcmp(o.sha256, c.sha256)
            cmp.verdict = 'MISMATCH';
            mismatch{end + 1} = sprintf(['%s: version/sha256 deviates ', ...
                '(manifest %s/%s vs current %s/%s)'], c.component, ...
                o.version, o.sha256, c.version, c.sha256); %#ok<AGROW>
        else
            cmp.verdict = 'MATCH';
        end
        cmps{end + 1} = cmp; %#ok<AGROW>
    end
    for i = 1:numel(man_in.components)
        c = man_in.components(i);
        if ~ismember(c.component, spec_names)
            mismatch{end + 1} = sprintf( ...
                '%s: manifest component outside the DGN-14 ten', ...
                c.component); %#ok<AGROW>
        end
    end
    if isempty(mismatch) && ~strcmp(composite_hash, man_in.composite_hash)
        mismatch{end + 1} = ...
            'composite_hash deviates although all ten components match';
    end
    if ~isempty(mismatch)
        error('GATE:MISSION_V3:STACK:MISMATCH', ...
            'qualified stack identity does not match the current stack: %s', ...
            strjoin(mismatch, '; '));
    end
    res = struct();
    res.schema = 'mission_v3_stack_identity_v1';
    res.mode = 'verify';
    res.overall = true;
    res.generated_at = char(datetime('now', 'TimeZone', 'local', ...
        'Format', 'yyyy-MM-dd''T''HH:mm:ssZ'));
    res.composite_hash = composite_hash;
    res.comparisons = [cmps{:}];
    res.generator_hash_domain = ghd;
    res.pending = local_pending_names(comps);
end

function local_write_manifest(man, out_path)
    pdir = fileparts(out_path);
    if ~isempty(pdir) && exist(pdir, 'dir') ~= 7
        mkdir(pdir);
    end
    fid = fopen(out_path, 'w');
    if fid < 0
        error('GATE:MISSION_V3:STACK:OUT_WRITE', ...
            'cannot open manifest sink: %s', out_path);
    end
    fwrite(fid, jsonencode(man, 'PrettyPrint', true));
    fclose(fid);
end

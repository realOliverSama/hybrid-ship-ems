function res = validate_registries(ROOT, varargin)
% mission_v3.validate_registries - Phase 4-C T-001 registry validator (MATLAB side)
%
%   res = mission_v3.validate_registries(ROOT)
%   res = mission_v3.validate_registries(ROOT, 'ConfigDir', DIR, 'RecordPath', P)
%
% Validates the nine candidate Mission v3 registries under config/mission_v3:
%   mission_template_registry.json / conditional_dag_config.json /
%   environment_regime_params.json / id_support_bounds.json /
%   forecast_quality_profiles.json / seed_tree_policy.json /
%   scenario_stratum_registry.json (T-002, DGN-13) /
%   heldout_token_registry.json (T-002, DGN-11) /
%   identifiability_contrast_registry.json (T-002, DGN-12)
%
% Checks: JSON parseable (jsondecode + strict local parser), required header
% fields, schema names, enums, structural rules, frozen DAG node table,
% cross references, support-bound arithmetic, and self_hash recomputation
% (canonical JSON + SHA-256, byte-identical to the Python authority:
% json.dumps(obj_minus_self_hash, sort_keys=True, separators=(',',':'),
% ensure_ascii=False).encode('utf-8')).
%
% On any FAIL: writes a machine-readable record (overall:false + issue list)
% to RecordPath first, then error('GATE:MISSION_V3:REGISTRY:<REASON>', ...).
% RecordPath 'none' disables disk output (used by the negative tests).
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% thin main + local functions (style ref: scripts/+dataset_v3/export_pilot.m).

    % ---- ROOT resolution (delegation spec B: mfilename + pwd fallback) ----
    if nargin < 1
        ROOT = '';
    end
    if isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end

    % ---- name-value args ----
    cfg_dir = fullfile(ROOT, 'config', 'mission_v3');
    rec_path = fullfile(ROOT, 'results', 'mission_v3', 'registry_validation.json');
    k = 1;
    while k <= numel(varargin)
        if k + 1 > numel(varargin)
            error('GATE:MISSION_V3:REGISTRY:BAD_ARG', ...
                'missing value for option %s', string(varargin{k}));
        end
        opt = varargin{k};
        val = varargin{k + 1};
        if strcmpi(opt, 'ConfigDir')
            cfg_dir = char(val);
        elseif strcmpi(opt, 'RecordPath')
            rec_path = char(val);
        else
            error('GATE:MISSION_V3:REGISTRY:BAD_ARG', ...
                'unknown option: %s', string(opt));
        end
        k = k + 2;
    end

    files = {'mission_template_registry.json', 'conditional_dag_config.json', ...
        'environment_regime_params.json', 'id_support_bounds.json', ...
        'forecast_quality_profiles.json', 'seed_tree_policy.json', ...
        'scenario_stratum_registry.json', 'heldout_token_registry.json', ...
        'identifiability_contrast_registry.json'};
    stems = {'mission_template_registry', 'conditional_dag_config', ...
        'environment_regime_params', 'id_support_bounds', ...
        'forecast_quality_profiles', 'seed_tree_policy', ...
        'scenario_stratum_registry', 'heldout_token_registry', ...
        'identifiability_contrast_registry'};
    schemas = {'mission_v3_template_registry_v1', 'mission_v3_conditional_dag_config_v1', ...
        'mission_v3_environment_regime_params_v1', 'mission_v3_id_support_bounds_v1', ...
        'mission_v3_forecast_quality_profiles_v1', 'mission_v3_seed_tree_policy_v1', ...
        'mission_v3_scenario_stratum_registry_v1', 'mission_v3_heldout_token_registry_v1', ...
        'mission_v3_identifiability_contrast_registry_v1'};
    n_files = numel(files);

    % ---- phase 1: read + parse all six ----
    trees = struct();
    numtoks = struct();
    detail = struct();
    issues = struct('file', {}, 'class', {}, 'msg', {});
    for i = 1:n_files
        stem = stems{i};
        d = struct();
        d.schema = schemas{i};
        d.status = 'FAILED';
        d.self_hash = 'UNAVAILABLE';
        d.checks = cell(1, 0);
        detail.(stem) = d;
        [txt, err] = local_read_text(fullfile(cfg_dir, files{i}));
        if ~isempty(err)
            issues = local_add_issue(issues, stem, 'FILE_MISSING', err);
            continue;
        end
        ok_dec = true;
        parse_err = '';
        try
            obj = jsondecode(txt);
            ok_dec = isstruct(obj);
            if ~ok_dec
                parse_err = 'jsondecode result is not a struct (top object expected)';
            end
        catch dec_err
            ok_dec = false;
            parse_err = ['jsondecode failed: ', dec_err.message];
        end
        if ~ok_dec
            issues = local_add_issue(issues, stem, 'JSON_PARSE', parse_err);
            continue;
        end
        ctx = struct();
        ctx.numtok = cell(1, 0);
        try
            [t, pos] = local_typed_value(txt, 1, ctx);
            pos = local_skip_ws(txt, pos);
            if pos <= numel(txt)
                error('MISSION_V3:TRAILING', 'trailing content after JSON value');
            end
            trees.(stem) = t;
            numtoks.(stem) = ctx.numtok;
            detail.(stem).status = 'PARSED';
        catch par_err
            issues = local_add_issue(issues, stem, 'JSON_PARSE', ...
                ['strict parse failed: ', par_err.message]);
        end
    end

    % ---- phase 2: number-literal discipline (all parsed files) ----
    for i = 1:n_files
        stem = stems{i};
        if ~isfield(numtoks, stem), continue; end
        d = detail.(stem);
        d.checks{end+1} = 'number_literals';
        detail.(stem) = d;
        toks = numtoks.(stem);
        for j = 1:numel(toks)
            tok = toks{j};
            is_plain = ~isempty(regexp(tok, '^-?[0-9]+(\.[0-9]+)?$', 'once'));
            v = str2double(tok);
            if ~is_plain || ~isfinite(v) || abs(v) >= 1e15
                issues = local_add_issue(issues, stem, 'NUMBER_LITERAL', ...
                    sprintf('non-tidy number literal: %s', tok));
            end
        end
    end

    % ---- phase 3: per-file semantic checks (hash excluded) ----
    for i = 1:n_files
        stem = stems{i};
        if ~isfield(trees, stem), continue; end
        t = trees.(stem);
        switch stem
            case 'mission_template_registry'
                [issues, detail] = local_check_template(t, stem, issues, detail);
            case 'conditional_dag_config'
                [issues, detail] = local_check_dag_frozen(t, stem, issues, detail);
            case 'environment_regime_params'
                [issues, detail] = local_check_env(t, stem, issues, detail);
            case 'id_support_bounds'
                [issues, detail] = local_check_bounds(t, stem, issues, detail);
            case 'forecast_quality_profiles'
                [issues, detail] = local_check_forecast(t, stem, issues, detail);
            case 'seed_tree_policy'
                [issues, detail] = local_check_seed(t, stem, issues, detail);
            case 'scenario_stratum_registry'
                [issues, detail] = local_check_strata(t, stem, issues, detail);
            case 'heldout_token_registry'
                [issues, detail] = local_check_tokens(t, stem, issues, detail);
            case 'identifiability_contrast_registry'
                [issues, detail] = local_check_contrasts(t, stem, issues, detail);
        end
    end

    % ---- phase 4: cross-file references ----
    have_all = true;
    for i = 1:n_files
        if ~isfield(trees, stems{i})
            have_all = false;
        end
    end
    if have_all
        [issues, detail] = local_check_cross(trees, stems, issues, detail);
        [issues, detail] = local_check_cross_c(trees, issues, detail);
    end

    % ---- phase 5: self_hash recomputation (last, so semantic defects ----
    % ---- report their own class when both are present)               ----
    for i = 1:n_files
        stem = stems{i};
        if ~isfield(numtoks, stem), continue; end
        d = detail.(stem);
        d.checks{end+1} = 'self_hash';
        detail.(stem) = d;
        [txt, err] = local_read_text(fullfile(cfg_dir, files{i}));
        if ~isempty(err)
            issues = local_add_issue(issues, stem, 'SELF_HASH', err);
            continue;
        end
        t = trees.(stem);
        stored = '';
        if isfield(t, 'self_hash')
            sh = t.self_hash;
            if isfield(sh, 'value')
                stored = sh.value;
            end
        end
        if isempty(regexp(stored, '^[0-9a-f]{64}$', 'once'))
            issues = local_add_issue(issues, stem, 'SELF_HASH', ...
                'self_hash.value is not 64 lowercase hex chars');
            continue;
        end
        try
            canon = local_canonical_json(txt);
            h = local_sha256_hex(unicode2native(canon, 'UTF-8'));
        catch hh_err
            issues = local_add_issue(issues, stem, 'SELF_HASH', ...
                ['canonical recompute failed: ', hh_err.message]);
            continue;
        end
        d = detail.(stem);
        d.self_hash = h;
        detail.(stem) = d;
        if ~strcmp(h, stored)
            issues = local_add_issue(issues, stem, 'SELF_HASH', ...
                sprintf('self_hash mismatch: stored %s vs recomputed %s', stored, h));
        end
    end

    % ---- phase 6: status reflection + record + gate ----
    for i = 1:n_files
        stem = stems{i};
        if ~isfield(trees, stem), continue; end
        ok_file = true;
        for j = 1:numel(issues)
            if strcmp(issues(j).file, stem)
                ok_file = false;
            end
        end
        d = detail.(stem);
        if ok_file
            d.status = 'PASSED';
        else
            d.status = 'FAILED';
        end
        detail.(stem) = d;
    end

    rec = struct();
    rec.overall = isempty(issues);
    rec.generated_at = char(datetime('now', 'TimeZone', 'local', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZ'));
    rec.validator = fullfile('scripts', '+mission_v3', 'validate_registries.m');
    cfg_char = char(cfg_dir);
    root_char = char(ROOT);
    if length(cfg_char) > length(root_char) && strncmp(cfg_char, root_char, length(root_char))
        rec.config_dir = cfg_char(length(root_char) + 2:end);
    else
        rec.config_dir = cfg_char;
    end
    rec.files = detail;
    issue_rows = cell(1, 0);
    for j = 1:numel(issues)
        issue_rows{end+1} = sprintf('%s [%s] %s', issues(j).file, ...
            issues(j).class, issues(j).msg); %#ok<AGROW>
    end
    rec.issues = issue_rows;

    if ~strcmp(rec_path, 'none')
        rec_dir = fileparts(rec_path);
        if ~exist(rec_dir, 'dir')
            mkdir(rec_dir);
        end
        fid = fopen(rec_path, 'w');
        if fid < 0
            error('GATE:MISSION_V3:REGISTRY:RECORD_WRITE', ...
                'cannot open record path: %s', rec_path);
        end
        fwrite(fid, jsonencode(rec, 'PrettyPrint', true));
        fclose(fid);
    end

    res = struct();
    res.overall = rec.overall;
    res.n_issues = numel(issues);
    res.issues = issue_rows;
    res.files = detail;
    res.record_path = rec_path;
    res.config_dir = cfg_dir;

    if ~isempty(issues)
        err_id = ['GATE:MISSION_V3:REGISTRY:', issues(1).class];
        err_msg = sprintf('%d registry validation issue(s); first: %s [%s] %s', ...
            numel(issues), issues(1).file, issues(1).class, issues(1).msg);
        error(err_id, '%s', err_msg);
    end
end

%% ======================== local functions ==========================

function issues = local_add_issue(issues, stem, cls, msg)
    entry.file = stem;
    entry.class = cls;
    entry.msg = char(msg);
    if isempty(issues)
        issues = entry;
    else
        issues(end+1) = entry; %#ok<AGROW>
    end
end

function [txt, err] = local_read_text(path)
    txt = '';
    err = '';
    if ~exist(path, 'file')
        err = sprintf('file not found: %s', path);
        return;
    end
    try
        txt = fileread(path);
    catch read_err
        err = ['read failed: ', read_err.message];
    end
end

%% ===================== typed JSON parser (checks) ===================
% Returns typed values: object -> struct, array -> cell, string -> char,
% number -> double (token kept in ctx.numtok), true/false -> logical,
% null -> double([]).

function [out, pos, ctx] = local_typed_value(txt, pos, ctx)
    pos = local_skip_ws(txt, pos);
    if pos > numel(txt)
        error('MISSION_V3:PARSE', 'unexpected end of input');
    end
    c = txt(pos);
    switch c
        case '{'
            [out, pos, ctx] = local_typed_object(txt, pos, ctx);
        case '['
            [out, pos, ctx] = local_typed_array(txt, pos, ctx);
        case '"'
            [s, pos] = local_scan_string(txt, pos);
            out = s;
        case 't'
            pos = local_expect_literal(txt, pos, 'true');
            out = true;
        case 'f'
            pos = local_expect_literal(txt, pos, 'false');
            out = false;
        case 'n'
            pos = local_expect_literal(txt, pos, 'null');
            out = double([]);
        otherwise
            [tok, pos] = local_scan_number_token(txt, pos);
            ctx.numtok{end+1} = tok;
            out = str2double(tok);
            if ~isfinite(out)
                error('MISSION_V3:PARSE', 'non-finite number: %s', tok);
            end
    end
end

function [out, pos, ctx] = local_typed_object(txt, pos, ctx)
    pos = pos + 1;  % consume '{'
    out = struct();
    keys = cell(1, 0);
    pos = local_skip_ws(txt, pos);
    if pos <= numel(txt) && txt(pos) == '}'
        pos = pos + 1;
        return;
    end
    while true
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt) || txt(pos) ~= '"'
            error('MISSION_V3:PARSE', 'object key expected');
        end
        [key, pos] = local_scan_string(txt, pos);
        if any(strcmp(keys, key))
            error('MISSION_V3:PARSE', 'duplicate object key: %s', key);
        end
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt) || txt(pos) ~= ':'
            error('MISSION_V3:PARSE', 'object colon expected');
        end
        pos = pos + 1;
        [val, pos, ctx] = local_typed_value(txt, pos, ctx);
        keys{end+1} = key;
        out.(key) = val;
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt)
            error('MISSION_V3:PARSE', 'unterminated object');
        end
        if txt(pos) == ','
            pos = pos + 1;
        elseif txt(pos) == '}'
            pos = pos + 1;
            break;
        else
            error('MISSION_V3:PARSE', 'object comma/brace expected');
        end
    end
end

function [out, pos, ctx] = local_typed_array(txt, pos, ctx)
    pos = pos + 1;  % consume '['
    out = cell(1, 0);
    pos = local_skip_ws(txt, pos);
    if pos <= numel(txt) && txt(pos) == ']'
        pos = pos + 1;
        return;
    end
    while true
        [val, pos, ctx] = local_typed_value(txt, pos, ctx);
        out{end+1} = val;
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt)
            error('MISSION_V3:PARSE', 'unterminated array');
        end
        if txt(pos) == ','
            pos = pos + 1;
        elseif txt(pos) == ']'
            pos = pos + 1;
            break;
        else
            error('MISSION_V3:PARSE', 'array comma/bracket expected');
        end
    end
end

%% ================ canonical JSON encoder (hash) =====================
% Byte-identical to Python:
%   json.dumps(obj_minus_self_hash, sort_keys=True, separators=(',',':'),
%              ensure_ascii=False).encode('utf-8')
% Works on raw text with a strict parser; the top-level "self_hash" member
% is skipped. Numbers keep their lexical int/float class.

function canon = local_canonical_json(txt)
    [canon, pos] = local_canon_value(txt, 1, true);
    pos = local_skip_ws(txt, pos);
    if pos <= numel(txt)
        error('MISSION_V3:CANON', 'trailing content after JSON value');
    end
end

function [out, pos] = local_canon_value(txt, pos, is_top)
    pos = local_skip_ws(txt, pos);
    if pos > numel(txt)
        error('MISSION_V3:CANON', 'unexpected end of input');
    end
    c = txt(pos);
    switch c
        case '{'
            [out, pos] = local_canon_object(txt, pos, is_top);
        case '['
            [out, pos] = local_canon_array(txt, pos);
        case '"'
            [s, pos] = local_scan_string(txt, pos);
            out = local_encode_string(s);
        case 't'
            pos = local_expect_literal(txt, pos, 'true');
            out = 'true';
        case 'f'
            pos = local_expect_literal(txt, pos, 'false');
            out = 'false';
        case 'n'
            pos = local_expect_literal(txt, pos, 'null');
            out = 'null';
        otherwise
            [tok, pos] = local_scan_number_token(txt, pos);
            out = local_canon_number(tok);
    end
end

function [out, pos] = local_canon_object(txt, pos, is_top)
    pos = pos + 1;  % consume '{'
    keys = cell(1, 0);
    vals = cell(1, 0);
    pos = local_skip_ws(txt, pos);
    if pos <= numel(txt) && txt(pos) == '}'
        pos = pos + 1;
        out = '{}';
        return;
    end
    while true
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt) || txt(pos) ~= '"'
            error('MISSION_V3:CANON', 'object key expected');
        end
        [key, pos] = local_scan_string(txt, pos);
        if any(strcmp(keys, key))
            error('MISSION_V3:CANON', 'duplicate object key: %s', key);
        end
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt) || txt(pos) ~= ':'
            error('MISSION_V3:CANON', 'object colon expected');
        end
        pos = pos + 1;
        drop = is_top && strcmp(key, 'self_hash');
        if drop
            [~, pos] = local_canon_value(txt, pos, false);
        else
            [v, pos] = local_canon_value(txt, pos, false);
            keys{end+1} = key;
            vals{end+1} = v;
        end
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt)
            error('MISSION_V3:CANON', 'unterminated object');
        end
        if txt(pos) == ','
            pos = pos + 1;
        elseif txt(pos) == '}'
            pos = pos + 1;
            break;
        else
            error('MISSION_V3:CANON', 'object comma/brace expected');
        end
    end
    if isempty(keys)
        out = '{}';
        return;
    end
    for j = 1:numel(keys)
        kj = keys{j};
        if ~isempty(kj) && max(abs(kj)) > 127
            error('MISSION_V3:CANON', 'non-ASCII object key: %s', kj);
        end
    end
    [skeys, idx] = sort(keys);
    for j = 2:numel(skeys)
        if strcmp(skeys{j}, skeys{j-1})
            error('MISSION_V3:CANON', 'duplicate key after sort');
        end
    end
    parts = cell(1, numel(skeys));
    for j = 1:numel(skeys)
        parts{j} = ['"', skeys{j}, '":', vals{idx(j)}];
    end
    out = ['{', strjoin(parts, ','), '}'];
end

function [out, pos] = local_canon_array(txt, pos)
    pos = pos + 1;  % consume '['
    vals = cell(1, 0);
    pos = local_skip_ws(txt, pos);
    if pos <= numel(txt) && txt(pos) == ']'
        pos = pos + 1;
        out = '[]';
        return;
    end
    while true
        [v, pos] = local_canon_value(txt, pos, false);
        vals{end+1} = v;
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt)
            error('MISSION_V3:CANON', 'unterminated array');
        end
        if txt(pos) == ','
            pos = pos + 1;
        elseif txt(pos) == ']'
            pos = pos + 1;
            break;
        else
            error('MISSION_V3:CANON', 'array comma/bracket expected');
        end
    end
    out = ['[', strjoin(vals, ','), ']'];
end

function out = local_canon_number(tok)
% Python parity: int tokens -> %d; float tokens -> shortest round-trip
% representation in Python repr style ('0.3', '20.0', '1e-05').
    is_float = ~isempty(regexp(tok, '[.eE]', 'once'));
    v = str2double(tok);
    if ~isfinite(v)
        error('MISSION_V3:CANON', 'non-finite number: %s', tok);
    end
    if ~is_float
        if abs(v) > 2^53 || v ~= round(v)
            error('MISSION_V3:CANON', 'unsafe integer token: %s', tok);
        end
        out = sprintf('%d', v);
        return;
    end
    digits = '';
    esign = '';
    eexp = 0;
    for p = 0:16
        s = sprintf('%.*e', p, v);
        if str2double(s) == v
            tok2 = s;
            sgn = '';
            if tok2(1) == '-'
                sgn = '-';
                tok2 = tok2(2:end);
            end
            epos = strfind(tok2, 'e');
            mant = tok2(1:epos-1);
            eexp = str2double(tok2(epos+1:end));
            digits = regexprep(mant, '\.', '');
            esign = sgn;
            break;
        end
    end
    if isempty(digits)
        error('MISSION_V3:CANON', 'no round-trip representation for %s', tok);
    end
    if eexp >= 16 || eexp <= -5
        if numel(digits) > 1
            mant = [digits(1), '.', digits(2:end)];
        else
            mant = digits(1);
        end
        out = [esign, mant, 'e', sprintf('%+03d', eexp)];
    else
        if eexp >= 0
            if numel(digits) >= eexp + 1
                ip = digits(1:eexp+1);
                fp = digits(eexp+2:end);
            else
                ip = [digits, repmat('0', 1, eexp + 1 - numel(digits))];
                fp = '';
            end
            if isempty(fp)
                fp = '0';
            end
            out = [esign, ip, '.', fp];
        else
            out = [esign, '0.', repmat('0', 1, -eexp - 1), digits];
        end
    end
end

function out = local_encode_string(s)
% Minimal JSON escaping, Python ensure_ascii=False semantics.
    n = numel(s);
    out = '"';
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
    out = [out, '"'];
end

%% ======================= shared scan helpers ========================

function pos = local_skip_ws(txt, pos)
    n = numel(txt);
    while pos <= n
        c = txt(pos);
        if c == char(9) || c == char(10) || c == char(13) || c == char(32)
            pos = pos + 1;
        else
            break;
        end
    end
end

function pos = local_expect_literal(txt, pos, lit)
    n = numel(lit);
    if pos + n - 1 > numel(txt)
        error('MISSION_V3:PARSE', 'literal too short');
    end
    seg = txt(pos:pos+n-1);
    if ~strcmp(seg, lit)
        error('MISSION_V3:PARSE', 'invalid literal near position %d', pos);
    end
    pos = pos + n;
    if pos <= numel(txt)
        nxt = txt(pos);
        delim = [',', '}', ']', ':', ' ', char(9), char(10), char(13)];
        if ~ismember(nxt, delim)
            error('MISSION_V3:PARSE', 'literal not delimited');
        end
    end
end

function [tok, pos] = local_scan_number_token(txt, pos)
    allowed = '0123456789+-.eE';
    startp = pos;
    n = numel(txt);
    while pos <= n && ismember(txt(pos), allowed)
        pos = pos + 1;
    end
    tok = txt(startp:pos-1);
    if isempty(tok)
        bad = txt(min(startp, n));
        error('MISSION_V3:PARSE', 'unexpected char at %d: %s', startp, bad);
    end
    pat = '^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$';
    if isempty(regexp(tok, pat, 'once'))
        error('MISSION_V3:PARSE', 'invalid JSON number: %s', tok);
    end
end

function [s, pos] = local_scan_string(txt, pos)
% pos at opening quote; returns decoded char row and pos after closing quote.
    pos = pos + 1;
    n = numel(txt);
    out = '';
    while true
        if pos > n
            error('MISSION_V3:PARSE', 'unterminated string');
        end
        ch = txt(pos);
        if ch == '"'
            pos = pos + 1;
            break;
        elseif ch == '\\'
            if pos + 1 > n
                error('MISSION_V3:PARSE', 'dangling escape');
            end
            esc = txt(pos+1);
            switch esc
                case '"'
                    out = [out, '"'];
                    pos = pos + 2;
                case '\\'
                    out = [out, '\\'];
                    pos = pos + 2;
                case '/'
                    out = [out, '/'];
                    pos = pos + 2;
                case 'b'
                    out = [out, char(8)];
                    pos = pos + 2;
                case 'f'
                    out = [out, char(12)];
                    pos = pos + 2;
                case 'n'
                    out = [out, char(10)];
                    pos = pos + 2;
                case 'r'
                    out = [out, char(13)];
                    pos = pos + 2;
                case 't'
                    out = [out, char(9)];
                    pos = pos + 2;
                case 'u'
                    if pos + 5 > n
                        error('MISSION_V3:PARSE', 'short backslash-u escape');
                    end
                    hexs = txt(pos+2:pos+5);
                    code = hex2dec(hexs);
                    if isnan(code) || (code >= 55296 && code <= 57343)
                        error('MISSION_V3:PARSE', 'bad backslash-u escape: %s', hexs);
                    end
                    out = [out, char(code)];
                    pos = pos + 6;
                otherwise
                    error('MISSION_V3:PARSE', 'bad escape: %s', esc);
            end
        elseif abs(ch) < 32
            error('MISSION_V3:PARSE', 'raw control char in string');
        else
            out = [out, ch];
            pos = pos + 1;
        end
    end
    s = out;
end

function h = local_sha256_hex(bytes)
% SHA-256 hex of raw bytes (bit-preserving path, same as model.sha256_file).
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(bytes(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function lst = local_as_list(v, what)
% Normalize a JSON array (jsondecode: struct array when fields are uniform,
% cell array of structs otherwise) into a cell list.
    if iscell(v)
        lst = v(:)';
    elseif isstruct(v)
        n = numel(v);
        lst = cell(1, n);
        for k = 1:n
            lst{k} = v(k);
        end
    elseif isempty(v) && ~ischar(v) && ~isstring(v)
        lst = cell(1, 0);
    else
        error('MISSION_V3:CHECK', 'expected JSON array for %s', what);
    end
end

function cls = local_value_class(v)
    if islogical(v) && ~isempty(v)
        cls = 'bool';
    elseif ischar(v)
        cls = 'string';
    elseif isnumeric(v)
        if isempty(v)
            cls = 'null_or_empty';
        else
            cls = 'number';
        end
    elseif iscell(v)
        cls = 'array';
    elseif isstruct(v)
        cls = 'object';
    else
        cls = 'other';
    end
end

function [ok, msg] = local_have(t, fname, cls, what)
    ok = isfield(t, fname);
    if ok
        ok = strcmp(local_value_class(t.(fname)), cls);
    end
    if ok
        msg = '';
    else
        msg = sprintf('missing or wrong type: %s (%s)', what, cls);
    end
end

%% ===================== per-file semantic checks =====================

function [issues, detail] = local_check_template(t, stem, issues, detail)
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    [ok, msg] = local_have(t, 'schema', 'string', 'schema');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'version', 'string', 'version');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'status', 'string', 'status');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'value_basis', 'string', 'value_basis');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    if ~isfield(t, 'self_hash') || ~strcmp(local_value_class(t.self_hash), 'object')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'missing self_hash object');
    end

    d.checks{end+1} = 'header';
    if ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end
    if isfield(t, 'self_hash')
        sh = t.self_hash;
        if ~isfield(sh, 'algorithm') || ~strcmp(sh.algorithm, 'sha256')
            issues = local_add_issue(issues, stem, 'HEADER', 'self_hash.algorithm must be sha256');
        end
        want_canon = local_d_canonicalization();
        if ~isfield(sh, 'canonicalization') || ~strcmp(sh.canonicalization, want_canon)
            issues = local_add_issue(issues, stem, 'HEADER', ...
                'self_hash.canonicalization deviates from D convention');
        end
    end

    d.checks{end+1} = 'schema_name';
    if ~strcmp(t.schema, 'mission_v3_template_registry_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'enums';
    role_enum = local_str_cell(t.segment_role_enum);
    reach_enum = local_str_cell(t.reach_type_enum);
    op_enum = local_str_cell(t.operator_profile_enum);
    if isempty(role_enum) || isempty(reach_enum) || isempty(op_enum)
        issues = local_add_issue(issues, stem, 'ENUM', ...
            'enum lists must be non-empty string arrays');
    end

    d.checks{end+1} = 'structure';
    tpls = local_as_list(t.mission_templates, 'mission_templates');
    want_ids = {'ROUTINE_PATROL', 'EMERGENCY_RESPONSE', 'WORK_MISSION'};
    if numel(tpls) ~= 3
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'exactly 3 mission templates expected');
    end
    got_ids = cell(1, numel(tpls));
    for a = 1:numel(tpls)
        tpl = tpls{a};
        tid = '';
        if isfield(tpl, 'mission_template_id')
            tid = tpl.mission_template_id;
        end
        got_ids(a) = {tid};
        if ~ismember(tid, want_ids)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('mission_template_id not in frozen set: %s', tid));
        end
        segs = local_as_list(tpl.segment_sequence, 'segment_sequence');
        if isempty(segs)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: empty segment_sequence', tid));
        end
        for b = 1:numel(segs)
            seg = segs{b};
            if ~isfield(seg, 'role') || ~isfield(seg, 'nominal_distance_m') || ~isfield(seg, 'reach')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s segment %d: role/nominal_distance_m/reach required', tid, b));
                continue;
            end
            role = seg.role;
            if ~ismember(role, role_enum)
                issues = local_add_issue(issues, stem, 'ENUM', ...
                    sprintf('%s segment %d: role not in enum: %s', tid, b, role));
            end
            if ~ismember(seg.reach, reach_enum)
                issues = local_add_issue(issues, stem, 'ENUM', ...
                    sprintf('%s segment %d: reach not in enum: %s', tid, b, seg.reach));
            end
            dist = seg.nominal_distance_m;
            if ~isnumeric(dist) || isempty(dist) || dist < 0
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s segment %d: nominal_distance_m must be numeric >= 0', tid, b));
            elseif ~strcmp(role, 'standby') && dist <= 0
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s segment %d: non-standby segment needs distance > 0', tid, b));
            end
            if isfield(seg, 'nominal_duration_s') && ~strcmp(role, 'standby')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s segment %d: nominal_duration_s only allowed on standby', tid, b));
            end
            if isfield(seg, 'presence') && ~strcmp(seg.presence, 'CONDITIONAL')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s segment %d: presence only allows CONDITIONAL', tid, b));
            end
        end
        if ~isfield(tpl, 'operator_prior') || ~strcmp(local_value_class(tpl.operator_prior), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: operator_prior object required', tid));
        else
            prior = tpl.operator_prior;
            pkeys = fieldnames(prior);
            psum = 0;
            for c = 1:numel(pkeys)
                if ~ismember(pkeys{c}, op_enum)
                    issues = local_add_issue(issues, stem, 'ENUM', ...
                        sprintf('%s operator_prior key not in enum: %s', tid, pkeys{c}));
                end
                pv = prior.(pkeys{c});
                if ~isnumeric(pv) || isempty(pv) || pv < 0 || pv > 1
                    issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                        sprintf('%s operator_prior.%s must be in [0,1]', tid, pkeys{c}));
                else
                    psum = psum + pv;
                end
            end
            if abs(psum - 1) > 1e-9
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s operator_prior must sum to 1 (tol 1e-9), got %.12g', tid, psum));
            end
        end
        if ~isfield(tpl, 'p_hotel_base_kW') || ~isnumeric(tpl.p_hotel_base_kW) ...
                || isempty(tpl.p_hotel_base_kW) || tpl.p_hotel_base_kW <= 0
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: p_hotel_base_kW must be numeric > 0', tid));
        end
        if ~isfield(tpl, 'emergency_sprint_eligible') || ~islogical(tpl.emergency_sprint_eligible)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: emergency_sprint_eligible must be boolean', tid));
        end
        if ~isfield(tpl, 'description_zh') || ~ischar(tpl.description_zh)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: description_zh string required', tid));
        end
    end
    if numel(got_ids) == 3
        if ~isempty(setdiff(got_ids, want_ids)) || ~isempty(setdiff(want_ids, got_ids))
            issues = local_add_issue(issues, stem, 'ENUM', ...
                'template id set deviates from DGN-02 frozen triple');
        end
    end
    detail.(stem) = d;
end

function [issues, detail] = local_check_dag_frozen(t, stem, issues, detail)
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    need = {'semantics', 'river_coordinate_semantics', 'levels', 'nodes', ...
        'independence_invariant', 'hash_note'};
    for j = 1:numel(need)
        if ~isfield(t, need{j})
            issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
                sprintf('missing field: %s', need{j}));
        end
    end

    d.checks{end+1} = 'header';
    if ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if ~strcmp(t.schema, 'mission_v3_conditional_dag_config_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'structure';
    lvl_keys = fieldnames(t.levels);
    want_lvls = {'L0', 'L1', 'L2', 'L3', 'BASE_DRAW', 'L4'};
    for j = 1:numel(want_lvls)
        if ~ismember(want_lvls{j}, lvl_keys)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('levels missing: %s', want_lvls{j}));
        end
    end

    % frozen node table (DGN-03 / delegation spec A2): id, level, parents, distribution
    frozen_ids = {'template', 'regime', 'reach_sequence', 'operator_profile', ...
        'u_river_mean', 'delta_u_process', 'water_level_offset', ...
        'traffic_rate_lambda', 'heading', 'segment_stretch', 'hotel_plan', ...
        'delta_p_hotel_unplanned', 'resistance_mult', 'intervention'};
    frozen_lvl = local_frozen_levels();
    frozen_dist = local_frozen_dists();
    frozen_par = local_frozen_parents();

    nodes = local_as_list(t.nodes, 'nodes');
    got_ids = cell(1, numel(nodes));
    for a = 1:numel(nodes)
        node = nodes{a};
        if ~isfield(node, 'node_id') || ~ischar(node.node_id)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('node %d: node_id string required', a));
            got_ids(a) = {''};
            continue;
        end
        nid = node.node_id;
        got_ids(a) = {nid};
        if ~isKey(frozen_lvl, nid)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('node not in frozen DGN-03 table: %s', nid));
            continue;
        end
        if ~isfield(node, 'level') || ~strcmp(node.level, frozen_lvl(nid))
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: level deviates from frozen table', nid));
        end
        if ~isfield(node, 'distribution') || ~strcmp(node.distribution, frozen_dist(nid))
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: distribution deviates from frozen table', nid));
        end
        if ~isfield(node, 'parents')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: parents field required', nid));
        else
            par = node.parents;
            if isempty(par)
                par_list = cell(1, 0);
            elseif iscell(par)
                par_list = par(:)';
                for c = 1:numel(par_list)
                    if ~ischar(par_list{c})
                        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                            sprintf('%s: parents must be strings', nid));
                    end
                end
            else
                par_list = cell(1, 0);
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: parents must be a JSON array of ids', nid));
            end
            want = frozen_par(nid);
            if ~isempty(setdiff(par_list, want)) || ~isempty(setdiff(want, par_list))
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: parent edge set deviates from frozen table', nid));
            end
        end
        if ~isfield(node, 'params') && ~isfield(node, 'params_ref') ...
                && ~strcmp(nid, 'template') && ~strcmp(nid, 'intervention')
            % template (quota) and intervention (predeclared matrix) carry no
            % params_ref in the frozen DGN-03 table; all others must
            % anchor their parameter source.
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: params or params_ref required', nid));
        end
    end
    if numel(got_ids) == numel(frozen_ids)
        if ~isempty(setdiff(got_ids, frozen_ids)) || ~isempty(setdiff(frozen_ids, got_ids))
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'node id set deviates from frozen DGN-03 table');
        end
    else
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'node count deviates from frozen DGN-03 table (14)');
    end
    detail.(stem) = d;
end

function m = local_frozen_levels()
    m = containers.Map();
    m('template') = 'L0';
    m('regime') = 'L1';
    m('reach_sequence') = 'L1';
    m('operator_profile') = 'L2';
    m('u_river_mean') = 'L3';
    m('delta_u_process') = 'L3';
    m('water_level_offset') = 'L3';
    m('traffic_rate_lambda') = 'L3';
    m('heading') = 'L3';
    m('segment_stretch') = 'L3';
    m('hotel_plan') = 'L3';
    m('delta_p_hotel_unplanned') = 'BASE_DRAW';
    m('resistance_mult') = 'BASE_DRAW';
    m('intervention') = 'L4';
end

function m = local_frozen_dists()
    m = containers.Map();
    m('template') = 'QUOTA_DRIVEN';
    m('regime') = 'QUOTA_DRIVEN';
    m('reach_sequence') = 'DETERMINISTIC';
    m('operator_profile') = 'CATEGORICAL';
    m('u_river_mean') = 'TRUNCATED_NORMAL';
    m('delta_u_process') = 'OU';
    m('water_level_offset') = 'TRUNCATED_NORMAL';
    m('traffic_rate_lambda') = 'POISSON_RATE';
    m('heading') = 'DETERMINISTIC';
    m('segment_stretch') = 'UNIFORM';
    m('hotel_plan') = 'LOOKUP';
    m('delta_p_hotel_unplanned') = 'TRUNCATED_NORMAL';
    m('resistance_mult') = 'UNIFORM';
    m('intervention') = 'PREDECLARED_MATRIX';
end

function m = local_frozen_parents()
    m = containers.Map();
    m('template') = {};
    m('regime') = {};
    m('reach_sequence') = {'template'};
    m('operator_profile') = {'template'};
    m('u_river_mean') = {'regime', 'reach_sequence'};
    m('delta_u_process') = {'regime'};
    m('water_level_offset') = {'regime'};
    m('traffic_rate_lambda') = {'regime', 'reach_sequence'};
    m('heading') = {'template'};
    m('segment_stretch') = {'template'};
    m('hotel_plan') = {'template'};
    m('delta_p_hotel_unplanned') = {};
    m('resistance_mult') = {};
    m('intervention') = {'base_draw'};
end

function [issues, detail] = local_check_env(t, stem, issues, detail)
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    need = {'regimes', 'ou_process', 'traffic', 'chart_depth_by_reach', 'notes'};
    for j = 1:numel(need)
        if ~isfield(t, need{j})
            issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
                sprintf('missing field: %s', need{j}));
        end
    end

    d.checks{end+1} = 'header';
    if ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if ~strcmp(t.schema, 'mission_v3_environment_regime_params_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'enums';
    regimes_obj = t.regimes;
    regime_keys = fieldnames(regimes_obj);
    want_regimes = {'NORMAL', 'FLOOD', 'DRY'};
    if ~isempty(setdiff(regime_keys, want_regimes)) || ~isempty(setdiff(want_regimes, regime_keys))
        issues = local_add_issue(issues, stem, 'ENUM', ...
            'regime keys must be exactly NORMAL/FLOOD/DRY');
    end

    d.checks{end+1} = 'structure';
    for j = 1:numel(regime_keys)
        rk = regime_keys{j};
        r = regimes_obj.(rk);
        if ~isfield(r, 'description_zh') || ~ischar(r.description_zh)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: description_zh required', rk));
        end
        if ~isfield(r, 'u_river_mag') || ~strcmp(local_value_class(r.u_river_mag), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: u_river_mag object required', rk));
        else
            urm = r.u_river_mag;
            if ~isfield(urm, 'mu') || ~isfield(urm, 'sigma') || ~isfield(urm, 'lower')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: u_river_mag mu/sigma/lower required', rk));
            else
                if ~isnumeric(urm.mu) || ~isnumeric(urm.sigma) || isempty(urm.sigma) || urm.sigma <= 0
                    issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                        sprintf('%s: u_river_mag sigma must be > 0', rk));
                end
                if urm.lower ~= 0
                    issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                        sprintf('%s: u_river_mag lower must be 0', rk));
                end
            end
        end
        if ~isfield(r, 'delta_u') || ~strcmp(local_value_class(r.delta_u), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: delta_u object required', rk));
        else
            du = r.delta_u;
            if ~isfield(du, 'tau_s') || ~isnumeric(du.tau_s) || isempty(du.tau_s) || du.tau_s ~= 900
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: delta_u.tau_s must be 900 (DGN-04 frozen candidate)', rk));
            end
            if ~isfield(du, 'sigma_mps') || ~isnumeric(du.sigma_mps) || isempty(du.sigma_mps) || du.sigma_mps <= 0
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: delta_u.sigma_mps must be > 0', rk));
            end
        end
        if ~isfield(r, 'water_level_offset') || ~strcmp(local_value_class(r.water_level_offset), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: water_level_offset object required', rk));
        else
            wlo = r.water_level_offset;
            if ~isfield(wlo, 'mu') || ~isfield(wlo, 'sigma') || ~isnumeric(wlo.sigma) ...
                    || isempty(wlo.sigma) || wlo.sigma <= 0
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: water_level_offset sigma must be > 0', rk));
            end
        end
    end

    ou = t.ou_process;
    if ~isfield(ou, 'boundedness') || ~strcmp(local_value_class(ou.boundedness), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'ou_process.boundedness required');
    else
        bnd = ou.boundedness;
        if ~isfield(bnd, 'clamp_forbidden') || ~islogical(bnd.clamp_forbidden) || ~bnd.clamp_forbidden
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'boundedness.clamp_forbidden must be true (FR-520)');
        end
    end
    if ~isfield(ou, 'tau_s') || ~isnumeric(ou.tau_s) || isempty(ou.tau_s) || ou.tau_s ~= 900
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'ou_process.tau_s must be 900 (DGN-04 frozen candidate)');
    end
    if ~isfield(ou, 'injection') || ~strcmp(local_value_class(ou.injection), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'ou_process.injection required');
    else
        inj = ou.injection;
        if ~isfield(inj, 'grid_s') || ~isnumeric(inj.grid_s) || isempty(inj.grid_s) || inj.grid_s ~= 1
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'injection.grid_s must be 1 (1 s ZOH grid)');
        end
        if ~isfield(inj, 'hold') || ~strcmp(inj.hold, 'ZOH')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'injection.hold must be ZOH');
        end
    end

    tr = t.traffic;
    if ~isfield(tr, 'event_process') || ~strcmp(tr.event_process, 'POISSON')
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'traffic.event_process must be POISSON');
    end
    if ~isfield(tr, 'event_duration_s') || ~strcmp(local_value_class(tr.event_duration_s), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'traffic.event_duration_s object required');
    else
        evd = tr.event_duration_s;
        % T429-AMENDMENT-V2 (Day-3 Ruling section 3): U[60,180] superseded by
        % U[225,320] so the STEADY_FOLLOW platform (0.8*d) guarantees >= 180 s
        % = slice + 2*edge_buffer (D1-B HOST_CONTAMINATION_CONFIRMED basis).
        if ~isfield(evd, 'low') || ~isfield(evd, 'high') || evd.low ~= 225 || evd.high ~= 320
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'traffic.event_duration_s must be U[225,320] (T429-AMENDMENT-V2 frozen)');
        end
    end
    if ~isfield(tr, 'speed_ratio') || ~strcmp(local_value_class(tr.speed_ratio), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'traffic.speed_ratio object required');
    else
        spr = tr.speed_ratio;
        if ~isfield(spr, 'low') || ~isfield(spr, 'high') || spr.low ~= 0.5 || spr.high ~= 0.8
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'traffic.speed_ratio must be U[0.5,0.8] (DGN-04 frozen)');
        end
    end
    if ~isfield(tr, 'rate_per_hour') || ~strcmp(local_value_class(tr.rate_per_hour), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'traffic.rate_per_hour object required');
    else
        rates = tr.rate_per_hour;
        rkeys = fieldnames(rates);
        for j = 1:numel(rkeys)
            rr = rates.(rkeys{j});
            if ~strcmp(local_value_class(rr), 'object')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('rate_per_hour.%s must be an object', rkeys{j}));
            else
                rsub = fieldnames(rr);
                if ~isempty(setdiff(rsub, want_regimes)) || ~isempty(setdiff(want_regimes, rsub))
                    issues = local_add_issue(issues, stem, 'ENUM', ...
                        sprintf('rate_per_hour.%s regime keys must be NORMAL/FLOOD/DRY', rkeys{j}));
                end
            end
        end
    end
    cdb = t.chart_depth_by_reach;
    if ~strcmp(local_value_class(cdb), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'chart_depth_by_reach object required');
    end
    detail.(stem) = d;
end

function [issues, detail] = local_check_bounds(t, stem, issues, detail)
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    need = {'axis_semantics', 'axes', 'joint_feasibility_rule', ...
        'boundary_sanity_obligation', 'guard_band_principle'};
    for j = 1:numel(need)
        if ~isfield(t, need{j})
            issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
                sprintf('missing field: %s', need{j}));
        end
    end

    d.checks{end+1} = 'header';
    if ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if ~strcmp(t.schema, 'mission_v3_id_support_bounds_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'enums';
    axes_obj = t.axes;
    ax_keys = fieldnames(axes_obj);
    want_axes = {'adverse_current', 'following_current', 'ht_ratio'};
    if ~isempty(setdiff(ax_keys, want_axes)) || ~isempty(setdiff(want_axes, ax_keys))
        issues = local_add_issue(issues, stem, 'ENUM', ...
            'axes must be exactly adverse_current/following_current/ht_ratio');
    end

    d.checks{end+1} = 'support_bounds';
    for j = 1:numel(ax_keys)
        ax = axes_obj.(ax_keys{j});
        if ~isfield(ax, 'direction')
            issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                sprintf('%s: direction required', ax_keys{j}));
            continue;
        end
        if ~isfield(ax, 'ood_status') || ~strcmp(ax.ood_status, 'ACTIVATION_CANDIDATE')
            issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                sprintf('%s: ood_status must be ACTIVATION_CANDIDATE', ax_keys{j}));
        end
        if ~isfield(ax, 'ood_freeze_authority') || ~strcmp(ax.ood_freeze_authority, 'T-432 activation')
            issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                sprintf('%s: ood_freeze_authority must be T-432 activation', ax_keys{j}));
        end
        if ~isfield(ax, 'iid_support') || ~isfield(ax, 'guard_band') || ~isfield(ax, 'ood_activation_candidate')
            issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                sprintf('%s: iid_support/guard_band/ood_activation_candidate required', ax_keys{j}));
            continue;
        end
        iid = ax.iid_support;
        grd = ax.guard_band;
        ood = ax.ood_activation_candidate;
        if strcmp(ax.direction, 'HIGH_IS_OOD')
            % flow axes: iid < guard < ood, endpoint-aware adjacency
            [ok1, msg1] = local_band_lt(iid.max, iid.max_inclusive, grd.min, grd.min_inclusive);
            if ~ok1
                issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                    sprintf('%s: iid/guard ordering: %s', ax_keys{j}, msg1));
            end
            [ok2, msg2] = local_band_lt(grd.max, grd.max_inclusive, ood.min, ood.min_inclusive);
            if ~ok2
                issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                    sprintf('%s: guard/ood ordering: %s', ax_keys{j}, msg2));
            end
        elseif strcmp(ax.direction, 'LOW_IS_OOD')
            % H/T axis: floor < ood < guard < iid (reversed), endpoint-aware
            if ~isfield(ax, 'domain_floor') || ~isnumeric(ax.domain_floor) || isempty(ax.domain_floor)
                issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                    sprintf('%s: domain_floor required for LOW_IS_OOD axis', ax_keys{j}));
            else
                [ok0, msg0] = local_band_lt(ax.domain_floor, false, ood.min, ood.min_inclusive);
                if ~ok0
                    issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                        sprintf('%s: domain_floor vs ood ordering: %s', ax_keys{j}, msg0));
                end
            end
            [ok1, msg1] = local_band_lt(ood.max, ood.max_inclusive, grd.min, grd.min_inclusive);
            if ~ok1
                issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                    sprintf('%s: ood/guard ordering: %s', ax_keys{j}, msg1));
            end
            [ok2, msg2] = local_band_lt(grd.max, grd.max_inclusive, iid.min, iid.min_inclusive);
            if ~ok2
                issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                    sprintf('%s: guard/iid ordering: %s', ax_keys{j}, msg2));
            end
        else
            issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                sprintf('%s: unknown direction', ax_keys{j}));
        end
        if isfield(grd, 'min') && isfield(grd, 'max') && ~(grd.max > grd.min)
            issues = local_add_issue(issues, stem, 'SUPPORT_BOUNDS', ...
                sprintf('%s: guard band width must be > 0 (delta > 0)', ax_keys{j}));
        end
    end
    detail.(stem) = d;
end

function [ok, msg] = local_band_lt(ahi, ainc, bmin, binc)
% True when band a lies strictly below band b (overlap-free; touching
% endpoints allowed when not both inclusive).
    if ~isnumeric(ahi) || ~isnumeric(bmin) || isempty(ahi) || isempty(bmin)
        ok = false;
        msg = 'band endpoints must be numeric';
        return;
    end
    if ahi < bmin
        ok = true;
        msg = '';
    elseif ahi == bmin && ~(ainc && binc)
        ok = true;
        msg = '';
    else
        ok = false;
        msg = sprintf('bands overlap or misordered at %g', ahi);
    end
end

function [issues, detail] = local_check_forecast(t, stem, issues, detail)
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    need = {'prehistory_s', 'provenance', 'observability_whitelist', ...
        'observability_forbidden', 'profiles', 'single_track_note'};
    for j = 1:numel(need)
        if ~isfield(t, need{j})
            issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
                sprintf('missing field: %s', need{j}));
        end
    end

    d.checks{end+1} = 'header';
    if ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if ~strcmp(t.schema, 'mission_v3_forecast_quality_profiles_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'structure';
    if ~isfield(t, 'prehistory_s') || ~isnumeric(t.prehistory_s) || isempty(t.prehistory_s) ...
            || t.prehistory_s ~= 1800
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'prehistory_s must be 1800 (R5 Path A frozen)');
    end
    if ~isfield(t, 'provenance') || ~strcmp(local_value_class(t.provenance), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'provenance object required');
    else
        prov = t.provenance;
        if ~isfield(prov, 'generator_id') || ~strcmp(prov.generator_id, 'mission_v3_env_forecast')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'provenance.generator_id must be mission_v3_env_forecast');
        end
        if ~isfield(prov, 'provenance_class_formal') || ~strcmp(prov.provenance_class_formal, 'CAUSAL_GENERATOR')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'provenance_class_formal must be CAUSAL_GENERATOR');
        end
    end
    wl = local_str_cell(t.observability_whitelist);
    fl = local_str_cell(t.observability_forbidden);
    want_wl = {'OBSERVED', 'CAUSALLY_ESTIMATED', 'STATIC_KNOWN_PARAMETER', 'PREDECLARED_MODEL_PARAMETER'};
    want_fl = {'FUTURE_TRUTH', 'ORACLE_STATE', 'SIMULATOR_LATENT_PRIVILEGED'};
    if ~isempty(setdiff(wl, want_wl)) || ~isempty(setdiff(want_wl, wl))
        issues = local_add_issue(issues, stem, 'ENUM', ...
            'observability_whitelist deviates from frozen set');
    end
    if ~isempty(setdiff(fl, want_fl)) || ~isempty(setdiff(want_fl, fl))
        issues = local_add_issue(issues, stem, 'ENUM', ...
            'observability_forbidden deviates from frozen set');
    end
    if ~isfield(t, 'profiles') || ~strcmp(local_value_class(t.profiles), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'profiles object required');
        detail.(stem) = d;
        return;
    end
    profs = t.profiles;
    if ~isfield(profs, 'GOOD_V1') || ~strcmp(local_value_class(profs.GOOD_V1), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'profiles.GOOD_V1 required');
    else
        g = profs.GOOD_V1;
        if ~isfield(g, 'source_cutoff_lag_s') || isempty(g.source_cutoff_lag_s) ...
                || g.source_cutoff_lag_s ~= 0
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'GOOD_V1.source_cutoff_lag_s must be 0 (cutoff = t0)');
        end
        if ~isfield(g, 'u_forecast') || ~strcmp(local_value_class(g.u_forecast), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', 'GOOD_V1.u_forecast required');
        else
            uf = g.u_forecast;
            if ~isfield(uf, 'u_obs_source_class') || ~strcmp(uf.u_obs_source_class, 'OBSERVED')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    'GOOD_V1 u_obs_source_class must be OBSERVED');
            end
        end
        if ~isfield(g, 'wl_forecast') || ~strcmp(local_value_class(g.wl_forecast), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', 'GOOD_V1.wl_forecast required');
        else
            wf = g.wl_forecast;
            if ~isfield(wf, 'source_class') || ~strcmp(wf.source_class, 'OBSERVED')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    'GOOD_V1 wl source_class must be OBSERVED');
            end
        end
    end
    if ~isfield(profs, 'DEGRADED_V1') || ~strcmp(local_value_class(profs.DEGRADED_V1), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'profiles.DEGRADED_V1 required');
    else
        dgp = profs.DEGRADED_V1;
        if ~isfield(dgp, 'delta_lag_s') || isempty(dgp.delta_lag_s) || dgp.delta_lag_s ~= 1800
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'DEGRADED_V1.delta_lag_s must be 1800 (DGN-06 frozen candidate)');
        end
        if ~isfield(dgp, 'source_cutoff_lag_s') || isempty(dgp.source_cutoff_lag_s) ...
                || dgp.source_cutoff_lag_s ~= 1800
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'DEGRADED_V1.source_cutoff_lag_s must be 1800');
        end
        if ~isfield(dgp, 'seed_branch_ref') || ~strcmp(dgp.seed_branch_ref, 'forecast_obs_noise')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'DEGRADED_V1.seed_branch_ref must be forecast_obs_noise');
        end
        if ~isfield(dgp, 'b_voyage') || ~strcmp(local_value_class(dgp.b_voyage), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', 'DEGRADED_V1.b_voyage required');
        else
            bv = dgp.b_voyage;
            if ~isfield(bv, 'sigma') || ~isnumeric(bv.sigma) || isempty(bv.sigma) || bv.sigma <= 0
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    'DEGRADED_V1.b_voyage.sigma must be > 0');
            end
        end
        if ~isfield(dgp, 'eta_ar1') || ~strcmp(local_value_class(dgp.eta_ar1), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', 'DEGRADED_V1.eta_ar1 required');
        end
    end
    detail.(stem) = d;
end

function [issues, detail] = local_check_seed(t, stem, issues, detail)
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    need = {'lineage_algorithm', 'branches', 'intervention_branch_rule', ...
        'invariants', 'reproducibility'};
    for j = 1:numel(need)
        if ~isfield(t, need{j})
            issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
                sprintf('missing field: %s', need{j}));
        end
    end

    d.checks{end+1} = 'header';
    if ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if ~strcmp(t.schema, 'mission_v3_seed_tree_policy_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'structure';
    if ~isfield(t, 'lineage_algorithm') || ~strcmp(local_value_class(t.lineage_algorithm), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'lineage_algorithm object required');
    else
        la = t.lineage_algorithm;
        need_la = {'formula', 'digest', 'encoding', 'hex_case', 'identity_status'};
        for j = 1:numel(need_la)
            if ~isfield(la, need_la{j})
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('lineage_algorithm.%s required', need_la{j}));
            end
        end
        if ~isfield(la, 'engine_mapping') || ~strcmp(local_value_class(la.engine_mapping), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', 'engine_mapping object required');
        else
            em = la.engine_mapping;
            if ~isfield(em, 'rng') || ~strcmp(em.rng, 'Threefry4x64')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    'engine_mapping.rng must be Threefry4x64');
            end
        end
        if ~isfield(la, 'collision_detection') || ~strcmp(local_value_class(la.collision_detection), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', 'collision_detection object required');
        else
            cdx = la.collision_detection;
            if ~isfield(cdx, 'policy') || ~strcmp(cdx.policy, 'fail-closed')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    'collision_detection.policy must be fail-closed');
            end
        end
    end
    if ~isfield(t, 'branches')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'branches required');
    else
        br = local_str_cell(t.branches);
        want_br = {'current_truth', 'water_level', 'traffic', 'operator_residual', ...
            'hotel_residual', 'resistance', 'segment_geometry', 'forecast_obs_noise'};
        if isempty(br)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                'branch list must be non-empty');
        end
        if ~isempty(setdiff(br, want_br)) || ~isempty(setdiff(want_br, br))
            issues = local_add_issue(issues, stem, 'ENUM', ...
                'branch list deviates from DGN-07 frozen eight-branch set');
        end
    end
    if ~isfield(t, 'invariants') || ~strcmp(local_value_class(t.invariants), 'object')
        issues = local_add_issue(issues, stem, 'STRUCTURE', 'invariants object required');
    else
        inv = t.invariants;
        need_inv = {'information_counterfactual', 'physical_counterfactual', 'undeclared_reuse'};
        for j = 1:numel(need_inv)
            if ~isfield(inv, need_inv{j})
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('invariants.%s required', need_inv{j}));
            end
        end
    end
    detail.(stem) = d;
end

%% ================ T-002 per-file semantic checks ====================

function [issues, detail] = local_check_strata(t, stem, issues, detail)
% T-002 / DGN-13 / FR-553: nine-cell scenario stratum registry, all ACTIVE
% (DGN-02 orthogonality adjudication), ids STRAT_<TEMPLATE>__<REGIME>.
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    [ok, msg] = local_have(t, 'schema', 'string', 'schema');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'version', 'string', 'version');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'status', 'string', 'status');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'value_basis', 'string', 'value_basis');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    if ~isfield(t, 'self_hash') || ~strcmp(local_value_class(t.self_hash), 'object')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'missing self_hash object');
    end
    if ~isfield(t, 'strata')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'strata array required');
    end
    if ~isfield(t, 'status_enum')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'status_enum required');
    end
    if ~isfield(t, 'dgn02_adjudication') || ~ischar(t.dgn02_adjudication) || isempty(t.dgn02_adjudication)
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
            'dgn02_adjudication note required (FR-553 orthogonality record)');
    end

    d.checks{end+1} = 'header';
    if isfield(t, 'version') && ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if isfield(t, 'status') && ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if isfield(t, 'schema') && ~strcmp(t.schema, 'mission_v3_scenario_stratum_registry_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'status_enum';
    se = local_str_cell(t.status_enum);
    want_se = {'ACTIVE', 'STRUCTURAL_NOT_APPLICABLE', 'OUT_OF_SCIENTIFIC_SCOPE'};
    if ~isempty(setdiff(se, want_se)) || ~isempty(setdiff(want_se, se))
        issues = local_add_issue(issues, stem, 'ENUM', ...
            'status_enum deviates from FR-553 triple');
    end

    d.checks{end+1} = 'strata_cells';
    want_ids = local_strata_ids();
    want_tpl = {'ROUTINE_PATROL', 'EMERGENCY_RESPONSE', 'WORK_MISSION'};
    want_reg = {'NORMAL', 'FLOOD', 'DRY'};
    cells = local_as_list(t.strata, 'strata');
    if numel(cells) ~= 9
        issues = local_add_issue(issues, stem, 'STRATA', ...
            sprintf('exactly 9 strata cells expected, got %d (DGN-02: full 3x3 grid, no STRUCTURAL_NOT_APPLICABLE)', ...
            numel(cells)));
    end
    got_ids = cell(1, numel(cells));
    for a = 1:numel(cells)
        c = cells{a};
        cid = '';
        if isfield(c, 'scenario_stratum_id') && ischar(c.scenario_stratum_id)
            cid = c.scenario_stratum_id;
        elseif isfield(c, 'scenario_stratum_id')
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %d: scenario_stratum_id must be a string', a));
        else
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %d: scenario_stratum_id required', a));
        end
        got_ids(a) = {cid};
        if ~isfield(c, 'mission_template_id') || ~isfield(c, 'environment_regime') ...
                || ~isfield(c, 'status') || ~isfield(c, 'reason') || ~isfield(c, 'version')
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %d: mission_template_id/environment_regime/status/reason/version required', a));
            continue;
        end
        tpl = c.mission_template_id;
        reg = c.environment_regime;
        if ~ischar(tpl) || ~ischar(reg)
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %d: template/regime must be strings', a));
            continue;
        end
        if ~ismember(tpl, want_tpl)
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %d: template not in frozen DGN-02 triple: %s', a, tpl));
        end
        if ~ismember(reg, want_reg)
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %d: regime not in NORMAL/FLOOD/DRY: %s', a, reg));
        end
        want_cid = ['STRAT_', tpl, '__', reg];
        if ~strcmp(cid, want_cid)
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %d: id deviates from STRAT_<TEMPLATE>__<REGIME> pattern (expected %s, got %s)', ...
                a, want_cid, cid));
        end
        if ~strcmp(c.status, 'ACTIVE')
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %s: status must be ACTIVE (DGN-02 adjudication), got %s', cid, c.status));
        end
        if ~ischar(c.reason) || isempty(c.reason)
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %s: reason must be a non-empty string', cid));
        end
        if ~strcmp(c.version, '1.0.0-candidate')
            issues = local_add_issue(issues, stem, 'STRATA', ...
                sprintf('stratum %s: version must be 1.0.0-candidate', cid));
        end
    end
    if numel(got_ids) == 9
        if ~isempty(setdiff(got_ids, want_ids)) || ~isempty(setdiff(want_ids, got_ids))
            issues = local_add_issue(issues, stem, 'STRATA', ...
                'stratum id set deviates from frozen 3x3 grid');
        end
    end
    detail.(stem) = d;
end

function ids = local_strata_ids()
% The nine expected stratum ids (3 templates x 3 regimes, DGN-02/DGN-13).
    tpl = {'ROUTINE_PATROL', 'EMERGENCY_RESPONSE', 'WORK_MISSION'};
    reg = {'NORMAL', 'FLOOD', 'DRY'};
    ids = cell(1, 9);
    k = 0;
    for i = 1:numel(tpl)
        for j = 1:numel(reg)
            k = k + 1;
            ids{k} = ['STRAT_', tpl{i}, '__', reg{j}];
        end
    end
end

function [lbl, band_id, axis] = local_token_expect(tid)
% Frozen per-token expectations (DGN-11 / C-D5 machine names).
    lbl = '';
    band_id = '';
    axis = '';
    if strcmp(tid, 'C1_BRIDGE_X_HIGH_ID_ADVERSE_CURRENT')
        lbl = 'BRIDGE_PASSAGE';
        band_id = 'HIGH_ID_BAND';
        axis = 'adverse_current';
    elseif strcmp(tid, 'C2_HARBOR_X_LOW_ID_HT')
        lbl = 'HARBOR';
        band_id = 'LOW_ID_BAND';
        axis = 'ht_ratio';
    elseif strcmp(tid, 'C3_EMERGENCY_SPRINT_X_LOW_ID_HT')
        lbl = 'EMERGENCY_SPRINT';
        band_id = 'LOW_ID_BAND';
        axis = 'ht_ratio';
    end
end

function wl = local_token_whitelist()
% Frozen predicate field whitelist (DGN-11 / FR-542: generation-side causal
% metadata only; realized/simulator output signals are forbidden).
    wl = {'voyage.mission_template_id', 'voyage.environment_regime', ...
        'voyage.operator_profile', 'voyage.ht_effective', ...
        'segment.role', 'segment.reach', 'segment.direction', ...
        'segment.u_adverse_abs'};
end

function [issues, detail] = local_check_tokens(t, stem, issues, detail)
% T-002 / DGN-11 / FR-541~546: three held-out tokens with structured
% whitelist-constrained predicates and candidate support bands.
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    [ok, msg] = local_have(t, 'schema', 'string', 'schema');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'version', 'string', 'version');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'status', 'string', 'status');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'value_basis', 'string', 'value_basis');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    if ~isfield(t, 'self_hash') || ~strcmp(local_value_class(t.self_hash), 'object')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'missing self_hash object');
    end
    if ~isfield(t, 'predicate_field_whitelist') || ~iscell(t.predicate_field_whitelist)
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
            'predicate_field_whitelist array of {field, description} entries required');
    end
    if ~isfield(t, 'tokens')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'tokens array required');
    end

    d.checks{end+1} = 'header';
    if isfield(t, 'version') && ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if isfield(t, 'status') && ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if isfield(t, 'schema') && ~strcmp(t.schema, 'mission_v3_heldout_token_registry_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'predicate_whitelist';
    wl = local_token_whitelist();
    if isfield(t, 'predicate_field_whitelist') && iscell(t.predicate_field_whitelist)
        wl_entries = t.predicate_field_whitelist;
        wl_keys = cell(1, numel(wl_entries));
        for j = 1:numel(wl_entries)
            entry = wl_entries{j};
            key = '';
            if isstruct(entry) && isfield(entry, 'field') && ischar(entry.field)
                key = entry.field;
            else
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('whitelist entry %d needs a string field', j));
            end
            wl_keys(j) = {key};
            if ~isstruct(entry) || ~isfield(entry, 'description') ...
                    || ~ischar(entry.description) || isempty(entry.description)
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('whitelist %s: description string required', key));
            end
        end
        if numel(wl_keys) ~= numel(wl) || ~isempty(setdiff(wl_keys, wl)) ...
                || ~isempty(setdiff(wl, wl_keys))
            issues = local_add_issue(issues, stem, 'ENUM', ...
                'predicate_field_whitelist deviates from frozen eight-field set');
        end
    elseif isfield(t, 'predicate_field_whitelist')
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'predicate_field_whitelist must be an array of {field, description} entries');
    end

    d.checks{end+1} = 'tokens';
    toks = local_as_list(t.tokens, 'tokens');
    want_ids = {'C1_BRIDGE_X_HIGH_ID_ADVERSE_CURRENT', 'C2_HARBOR_X_LOW_ID_HT', ...
        'C3_EMERGENCY_SPRINT_X_LOW_ID_HT'};
    role_vals = {'bridge', 'harbor', 'rush'};
    if numel(toks) ~= 3
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            'exactly 3 tokens expected (C1/C2/C3, DGN-11 frozen triple)');
    end
    got_ids = cell(1, numel(toks));
    for a = 1:numel(toks)
        tk = toks{a};
        tid = '';
        if isfield(tk, 'token_id') && ischar(tk.token_id)
            tid = tk.token_id;
        end
        got_ids(a) = {tid};
        if ~ismember(tid, want_ids)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('token_id not in frozen C-D5 triple: %s', tid));
            continue;
        end
        [lbl, band_id, axis] = local_token_expect(tid);
        simple_need = {'dgn11_label', 'predicate_version', 'factor_a', ...
            'factor_b', 'predicate_scope', 'physical_feasibility_rule'};
        for j = 1:numel(simple_need)
            fname = simple_need{j};
            if ~isfield(tk, fname) || ~ischar(tk.(fname)) || isempty(tk.(fname))
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: %s non-empty string required', tid, fname));
            end
        end
        if isfield(tk, 'dgn11_label') && ~strcmp(tk.dgn11_label, lbl)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('%s: dgn11_label must be %s', tid, lbl));
        end
        if ~isfield(tk, 'id_action') || ~strcmp(tk.id_action, 'EXCLUDE')
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('%s: id_action must be EXCLUDE (IID generator rejects)', tid));
        end
        if ~isfield(tk, 'ood_action') || ~strcmp(tk.ood_action, 'REQUIRE')
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('%s: ood_action must be REQUIRE (OOD generator requires)', tid));
        end
        if ~isfield(tk, 'support_band') || ~strcmp(local_value_class(tk.support_band), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: support_band object required', tid));
        else
            sb = tk.support_band;
            issues = local_check_band(sb, band_id, tid, stem, issues);
        end
        if ~isfield(tk, 'min_unique_split_groups_per_marginal') ...
                || ~isnumeric(tk.min_unique_split_groups_per_marginal) ...
                || isempty(tk.min_unique_split_groups_per_marginal) ...
                || tk.min_unique_split_groups_per_marginal < 10
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: min_unique_split_groups_per_marginal must be numeric >= 10', tid));
        end
        if ~isfield(tk, 'ood_config_surface') || ~strcmp(local_value_class(tk.ood_config_surface), 'object')
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: ood_config_surface object required', tid));
        else
            ocs = tk.ood_config_surface;
            if ~isfield(ocs, 'ood_axis') || ~strcmp(ocs.ood_axis, axis)
                issues = local_add_issue(issues, stem, 'ENUM', ...
                    sprintf('%s: ood_axis must be %s', tid, axis));
            end
            if ~isfield(ocs, 'ood_def_version') || ~strcmp(ocs.ood_def_version, 'C-D5-v1')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: ood_def_version must be C-D5-v1', tid));
            end
            if ~isfield(ocs, 'physics_domain_ref') || ~ischar(ocs.physics_domain_ref) ...
                    || ~strncmp(ocs.physics_domain_ref, 'ModelE validated domain', 23)
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: physics_domain_ref must reference the ModelE validated domain', tid));
            end
        end
        if ~isfield(tk, 'predicate') || ~iscell(tk.predicate) || isempty(tk.predicate)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: predicate must be a non-empty structured AND list', tid));
        else
            prd = tk.predicate;
            for b = 1:numel(prd)
                it = prd{b};
                if ~isstruct(it) || ~isfield(it, 'field') || ~ischar(it.field)
                    issues = local_add_issue(issues, stem, 'PREDICATE_FIELD', ...
                        sprintf('%s: predicate item %d needs a string field', tid, b));
                    continue;
                end
                if ~ismember(it.field, wl)
                    issues = local_add_issue(issues, stem, 'PREDICATE_FIELD', ...
                        sprintf('%s: predicate field outside whitelist: %s', tid, it.field));
                end
                if ~isfield(it, 'operator') || ~ischar(it.operator)
                    issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                        sprintf('%s: predicate item %d needs a string operator', tid, b));
                    continue;
                end
                if strcmp(it.operator, '==')
                    if ~isfield(it, 'value') || ~ischar(it.value)
                        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                            sprintf('%s: predicate operator == needs a string value', tid));
                    elseif strcmp(it.field, 'segment.role') && ~ismember(it.value, role_vals)
                        issues = local_add_issue(issues, stem, 'ENUM', ...
                            sprintf('%s: role value not in frozen bridge/harbor/rush triple: %s', tid, it.value));
                    end
                elseif strcmp(it.operator, 'in_band')
                    if ~isfield(it, 'value') || ~ischar(it.value) || ~strcmp(it.value, band_id)
                        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                            sprintf('%s: in_band value must reference the token band %s', tid, band_id));
                    end
                else
                    issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                        sprintf('%s: predicate operator must be == or in_band, got %s', tid, it.operator));
                end
            end
        end
    end
    if numel(got_ids) == 3
        if ~isempty(setdiff(got_ids, want_ids)) || ~isempty(setdiff(want_ids, got_ids))
            issues = local_add_issue(issues, stem, 'ENUM', ...
                'token id set deviates from frozen C-D5 triple');
        end
    end
    detail.(stem) = d;
end

function issues = local_check_band(sb, band_id, tid, stem, issues)
% Candidate band arithmetic (T-002 Fixed Decision 2):
%   HIGH_ID_BAND = adverse |U| in [1.4, 1.8] closed, inside [0, 1.8]
%   LOW_ID_BAND  = H/T in [1.5, 2.0] closed
    if ~isfield(sb, 'band_id') || ~ischar(sb.band_id) || ~strcmp(sb.band_id, band_id)
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            sprintf('%s: support_band.band_id must be %s', tid, band_id));
    end
    if ~isfield(sb, 'min') || ~isnumeric(sb.min) || isempty(sb.min) ...
            || ~isfield(sb, 'max') || ~isnumeric(sb.max) || isempty(sb.max)
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            sprintf('%s: support_band.min/max numeric required', tid));
        return;
    end
    inc_ok = isfield(sb, 'min_inclusive') && islogical(sb.min_inclusive) && sb.min_inclusive ...
        && isfield(sb, 'max_inclusive') && islogical(sb.max_inclusive) && sb.max_inclusive;
    if ~inc_ok
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            sprintf('%s: band endpoints must be inclusive (closed candidate band)', tid));
    end
    if strcmp(band_id, 'HIGH_ID_BAND')
        if sb.min ~= 1.4 || sb.max ~= 1.8
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: HIGH_ID_BAND must be [1.4, 1.8] (DGN-11 candidate)', tid));
        end
        if sb.min < 0 || sb.max > 1.8
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: HIGH_ID_BAND must stay inside [0, 1.8] (adverse IID support)', tid));
        end
    elseif strcmp(band_id, 'LOW_ID_BAND')
        if sb.min ~= 1.5 || sb.max ~= 2.0
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: LOW_ID_BAND must be [1.5, 2.0] (DGN-11 candidate)', tid));
        end
    end
    if ~isfield(sb, 'support_relation') || ~ischar(sb.support_relation) || isempty(sb.support_relation)
        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
            sprintf('%s: support_band.support_relation note required (id_support_bounds relation)', tid));
    end
end

function ids = local_contrast_required_ids()
% The six REQUIRED contrasts (DGN-12 R9 list, Design Gate frozen; D6 host
% rebinding per Day-1 Gate Ruling 2026-09-05 section D).
    ids = {'SEG_BRIDGE_VS_HARBOR', 'SEG_BERTH_VS_DEPARTURE', ...
        'DIR_UPSTREAM_VS_DOWNSTREAM', 'DEPTH_LOW_ID_VS_NORMAL_ID', ...
        'SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE', 'SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE'};
end

function ids = local_contrast_secondary_ids()
% The two SECONDARY_DIAGNOSTIC contrasts (R9: mixed-axis, off the gate).
    ids = {'SEG_HARBOR_VS_SHALLOW_CRUISE', 'SEG_BRIDGE_VS_SHALLOW_CRUISE'};
end

function [role, axis, cls_a, cls_b, cfields] = local_contrast_expect(cid)
% Frozen per-contrast expectations (DGN-12 R9 list / T-002 Fixed Decision 4).
    role = '';
    axis = '';
    cls_a = '';
    cls_b = '';
    cfields = {};
    switch cid
        case 'SEG_BRIDGE_VS_HARBOR'
            role = 'REQUIRED'; axis = 'segment_role';
            cls_a = 'bridge'; cls_b = 'harbor';
            cfields = {'voyage.environment_regime', 'segment.direction', ...
                'voyage.mission_template_id', 'voyage.operator_profile'};
        case 'SEG_BERTH_VS_DEPARTURE'
            role = 'REQUIRED'; axis = 'segment_role';
            cls_a = 'berth'; cls_b = 'departure';
            cfields = {'voyage.mission_template_id'};
        case 'DIR_UPSTREAM_VS_DOWNSTREAM'
            role = 'REQUIRED'; axis = 'direction';
            cls_a = 'upstream'; cls_b = 'downstream';
            cfields = {'voyage.environment_regime', 'segment.reach'};
        case 'DEPTH_LOW_ID_VS_NORMAL_ID'
            role = 'REQUIRED'; axis = 'depth';
            cls_a = 'low_id'; cls_b = 'normal_id';
            cfields = {'voyage.mission_template_id', 'voyage.environment_regime'};
        case 'SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE'
            role = 'REQUIRED'; axis = 'mission_intensity';
            cls_a = 'rush'; cls_b = 'return';
            cfields = {'voyage.mission_template_id', 'voyage.environment_regime', ...
                'voyage.operator_profile', 'segment.reach'};
        case 'SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE'
            role = 'REQUIRED'; axis = 'traffic';
            cls_a = 'traffic_follow'; cls_b = 'free_cruise';
            cfields = {};
        case 'SEG_HARBOR_VS_SHALLOW_CRUISE'
            role = 'SECONDARY_DIAGNOSTIC'; axis = 'mixed_segment_depth';
            cls_a = 'harbor'; cls_b = 'shallow_cruise';
            cfields = {};
        case 'SEG_BRIDGE_VS_SHALLOW_CRUISE'
            role = 'SECONDARY_DIAGNOSTIC'; axis = 'mixed_segment_depth';
            cls_a = 'bridge'; cls_b = 'shallow_cruise';
            cfields = {};
    end
end

function [issues, detail] = local_check_contrasts(t, stem, issues, detail)
% T-002 / DGN-12 / FR-550: identifiability contrast registry (R9 list).
    d = detail.(stem);
    d.checks{end+1} = 'required_fields';
    [ok, msg] = local_have(t, 'schema', 'string', 'schema');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'version', 'string', 'version');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'status', 'string', 'status');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    [ok, msg] = local_have(t, 'value_basis', 'string', 'value_basis');
    if ~ok, issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', msg); end
    if ~isfield(t, 'self_hash') || ~strcmp(local_value_class(t.self_hash), 'object')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'missing self_hash object');
    end
    if ~isfield(t, 'contrasts')
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', 'contrasts array required');
    end
    if ~isfield(t, 'r9_adjudication') || ~ischar(t.r9_adjudication) || isempty(t.r9_adjudication)
        issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
            'r9_adjudication note required (R9 cleanup record)');
    end

    d.checks{end+1} = 'header';
    if isfield(t, 'version') && ~strcmp(t.version, '1.0.0-candidate')
        issues = local_add_issue(issues, stem, 'HEADER', 'version must be 1.0.0-candidate');
    end
    if isfield(t, 'status') && ~strcmp(t.status, 'CANDIDATE')
        issues = local_add_issue(issues, stem, 'HEADER', 'status must be CANDIDATE');
    end

    d.checks{end+1} = 'schema_name';
    if isfield(t, 'schema') && ~strcmp(t.schema, 'mission_v3_identifiability_contrast_registry_v1')
        issues = local_add_issue(issues, stem, 'SCHEMA_NAME', 'unexpected schema name');
    end

    d.checks{end+1} = 'contrast_table';
    req_ids = local_contrast_required_ids();
    sec_ids = local_contrast_secondary_ids();
    all_ids = [req_ids, sec_ids];
    sig_view = '1 Hz P_load_total_kW slice-level summary statistics (H1-derived)';
    wl = local_token_whitelist();
    cts = local_as_list(t.contrasts, 'contrasts');
    if numel(cts) ~= 8
        issues = local_add_issue(issues, stem, 'CONTRAST', ...
            sprintf('exactly 8 contrasts expected (6 REQUIRED + 2 SECONDARY_DIAGNOSTIC), got %d', numel(cts)));
    end
    got_req = cell(1, 0);
    got_sec = cell(1, 0);
    for a = 1:numel(cts)
        ct = cts{a};
        cid = '';
        if isfield(ct, 'contrast_id') && ischar(ct.contrast_id)
            cid = ct.contrast_id;
        end
        if ~ismember(cid, all_ids)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('contrast_id not in frozen R9 list: %s', cid));
            continue;
        end
        [role, axis, cls_a, cls_b, cfields] = local_contrast_expect(cid);
        need = {'contrast_id', 'factor_axis', 'class_A', 'class_B', 'cohort_predicate', ...
            'nuisance_controls', 'signal_view', 'SBD_required', 'probe_required', ...
            'minimum_group_coverage', 'gate_role'};
        for j = 1:numel(need)
            if ~isfield(ct, need{j})
                issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
                    sprintf('%s: missing field %s', cid, need{j}));
            end
        end
        if ~isfield(ct, 'factor_axis') || ~strcmp(ct.factor_axis, axis)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('%s: factor_axis must be %s', cid, axis));
        end
        if ~isfield(ct, 'class_A') || ~strcmp(ct.class_A, cls_a)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('%s: class_A must be %s', cid, cls_a));
        end
        if ~isfield(ct, 'class_B') || ~strcmp(ct.class_B, cls_b)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('%s: class_B must be %s', cid, cls_b));
        end
        if ~isfield(ct, 'nuisance_controls') || ~ischar(ct.nuisance_controls) || isempty(ct.nuisance_controls)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: nuisance_controls non-empty string required (FR-550)', cid));
        end
        if ~isfield(ct, 'signal_view') || ~strcmp(ct.signal_view, sig_view)
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: signal_view deviates from frozen H1-derived P_load view', cid));
        end
        if ~isfield(ct, 'SBD_required') || ~islogical(ct.SBD_required) || ~ct.SBD_required
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: SBD_required must be true', cid));
        end
        if ~isfield(ct, 'probe_required') || ~islogical(ct.probe_required) || ~ct.probe_required
            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                sprintf('%s: probe_required must be true', cid));
        end
        if ~isfield(ct, 'minimum_group_coverage') || ~isnumeric(ct.minimum_group_coverage) ...
                || isempty(ct.minimum_group_coverage) || ct.minimum_group_coverage ~= 10
            issues = local_add_issue(issues, stem, 'CONTRAST', ...
                sprintf('%s: minimum_group_coverage must be 10 (candidate; T-004 evidence then T-007 freeze)', cid));
        end
        if ~isfield(ct, 'gate_role') || ~strcmp(ct.gate_role, role)
            issues = local_add_issue(issues, stem, 'ENUM', ...
                sprintf('%s: gate_role must be %s', cid, role));
        end
        if isfield(ct, 'cohort_predicate')
            cp = ct.cohort_predicate;
            if ~iscell(cp)
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: cohort_predicate must be a structured AND list (JSON array)', cid));
            else
                used_fields = cell(1, numel(cp));
                for b = 1:numel(cp)
                    it = cp{b};
                    if ~isstruct(it) || ~isfield(it, 'field') || ~ischar(it.field)
                        issues = local_add_issue(issues, stem, 'PREDICATE_FIELD', ...
                            sprintf('%s: cohort item %d needs a string field', cid, b));
                        used_fields(b) = {''};
                        continue;
                    end
                    used_fields(b) = {it.field};
                    if ~ismember(it.field, wl)
                        issues = local_add_issue(issues, stem, 'PREDICATE_FIELD', ...
                            sprintf('%s: cohort field outside whitelist: %s', cid, it.field));
                    end
                    if ~isfield(it, 'operator') || ~ischar(it.operator) || isempty(it.operator)
                        issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                            sprintf('%s: cohort item %d needs a string operator', cid, b));
                    elseif strcmp(it.operator, '==')
                        if ~isfield(it, 'value') || ~ischar(it.value) || isempty(it.value)
                            issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                                sprintf('%s: cohort operator == needs a string value', cid));
                        end
                    end
                end
                used_fields = used_fields(~cellfun(@isempty, used_fields));
                if ~isempty(setdiff(used_fields, cfields)) || ~isempty(setdiff(cfields, used_fields))
                    issues = local_add_issue(issues, stem, 'CONTRAST', ...
                        sprintf('%s: cohort_predicate field set deviates from frozen R9 declaration', cid));
                end
            end
        end
        if strcmp(cid, 'SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE')
            if ~isfield(ct, 'preferred_mechanism') || ~strcmp(ct.preferred_mechanism, 'same_base_draw_counterfactual')
                issues = local_add_issue(issues, stem, 'STRUCTURE', ...
                    sprintf('%s: preferred_mechanism must be same_base_draw_counterfactual', cid));
            end
        end
        if strcmp(role, 'SECONDARY_DIAGNOSTIC')
            if ~isfield(ct, 'reason') || ~ischar(ct.reason) || isempty(ct.reason)
                issues = local_add_issue(issues, stem, 'REQUIRED_FIELD', ...
                    sprintf('%s: reason (R9 removal basis) required', cid));
            end
        end
        if strcmp(role, 'REQUIRED')
            got_req{end+1} = cid; %#ok<AGROW>
        else
            got_sec{end+1} = cid; %#ok<AGROW>
        end
    end
    if ~isempty(setdiff(got_req, req_ids)) || ~isempty(setdiff(req_ids, got_req))
        issues = local_add_issue(issues, stem, 'CONTRAST', ...
            'REQUIRED contrast id set deviates from frozen R9 six');
    end
    if ~isempty(setdiff(got_sec, sec_ids)) || ~isempty(setdiff(sec_ids, got_sec))
        issues = local_add_issue(issues, stem, 'CONTRAST', ...
            'SECONDARY_DIAGNOSTIC id set deviates from frozen R9 pair');
    end
    detail.(stem) = d;
end

%% ======================= cross-file checks ==========================

function [issues, detail] = local_check_cross(trees, stems, issues, detail)
    d = detail.conditional_dag_config;
    d.checks{end+1} = 'cross_refs';
    detail.conditional_dag_config = d;

    dag = trees.conditional_dag_config;
    env = trees.environment_regime_params;
    tpl = trees.mission_template_registry;
    seed = trees.seed_tree_policy;
    fcst = trees.forecast_quality_profiles;

    % 1. DAG parent references resolvable (node ids or declared level handles)
    nodes = local_as_list(dag.nodes, 'nodes');
    node_ids = cell(1, numel(nodes));
    for a = 1:numel(nodes)
        node = nodes{a};
        if isfield(node, 'node_id')
            node_ids(a) = {node.node_id};
        else
            node_ids(a) = {''};
        end
    end
    lvl_keys = fieldnames(dag.levels);
    for a = 1:numel(nodes)
        node = nodes{a};
        if ~isfield(node, 'parents'), continue; end
        par = node.parents;
        if isempty(par) || ~iscell(par), continue; end
        for b = 1:numel(par)
            tok = par{b};
            if ~ischar(tok)
                issues = local_add_issue(issues, 'conditional_dag_config', ...
                    'CROSS_REF', 'parent token must be a string');
                continue;
            end
            if ismember(tok, node_ids)
                continue;
            end
            hit = false;
            for c = 1:numel(lvl_keys)
                if strcmpi(tok, lvl_keys{c})
                    hit = true;
                end
            end
            if ~hit
                issues = local_add_issue(issues, 'conditional_dag_config', ...
                    'CROSS_REF', sprintf('parent reference unresolved: %s', tok));
            end
        end
    end

    % 2. DAG params_ref / support_ref targets resolvable
    reg_by_stem = struct();
    for c = 1:numel(stems)
        reg_by_stem.(stems{c}) = trees.(stems{c});
    end
    for a = 1:numel(nodes)
        node = nodes{a};
        if ~isfield(node, 'node_id'), continue; end
        nid = node.node_id;
        if isfield(node, 'params_ref')
            [ok, msg] = local_resolve_ref(node.params_ref, reg_by_stem);
            if ~ok
                issues = local_add_issue(issues, 'conditional_dag_config', ...
                    'CROSS_REF', sprintf('%s params_ref unresolved: %s (%s)', ...
                    nid, node.params_ref, msg));
            end
        end
        if isfield(node, 'support_ref')
            if ~isfield(reg_by_stem, node.support_ref)
                issues = local_add_issue(issues, 'conditional_dag_config', ...
                    'CROSS_REF', sprintf('%s support_ref unresolved: %s', nid, node.support_ref));
            end
        end
    end

    % 3. DAG regime enum vs env regimes keys
    for a = 1:numel(nodes)
        node = nodes{a};
        if isfield(node, 'node_id') && strcmp(node.node_id, 'regime') && isfield(node, 'params')
            npr = node.params;
            en = local_str_cell(npr.enum);
            env_regimes = env.regimes;
            env_keys = fieldnames(env_regimes);
            if ~isempty(setdiff(en, env_keys)) || ~isempty(setdiff(env_keys, en))
                issues = local_add_issue(issues, 'conditional_dag_config', ...
                    'CROSS_REF', 'dag regime enum deviates from environment_regime_params.regimes keys');
            end
        end
    end

    % 4. env traffic/chart keys vs template reach enum
    reach_enum = local_str_cell(tpl.reach_type_enum);
    traffic_obj = env.traffic;
    rates_obj = traffic_obj.rate_per_hour;
    tr_keys = fieldnames(rates_obj);
    if ~isempty(setdiff(tr_keys, reach_enum)) || ~isempty(setdiff(reach_enum, tr_keys))
        issues = local_add_issue(issues, 'environment_regime_params', ...
            'CROSS_REF', 'traffic.rate_per_hour keys deviate from reach_type_enum');
    end
    cd_keys = fieldnames(env.chart_depth_by_reach);
    if ~isempty(setdiff(cd_keys, reach_enum)) || ~isempty(setdiff(reach_enum, cd_keys))
        issues = local_add_issue(issues, 'environment_regime_params', ...
            'CROSS_REF', 'chart_depth_by_reach keys deviate from reach_type_enum');
    end

    % 5. forecast DEGRADED seed_branch_ref in seed tree branches
    seed_br = local_str_cell(seed.branches);
    profs = fcst.profiles;
    deg = profs.DEGRADED_V1;
    if isfield(deg, 'seed_branch_ref') && ~ismember(deg.seed_branch_ref, seed_br)
        issues = local_add_issue(issues, 'forecast_quality_profiles', ...
            'CROSS_REF', sprintf('seed_branch_ref not in seed tree branches: %s', ...
            deg.seed_branch_ref));
    end
    if ~ismember('forecast_obs_noise', seed_br)
        issues = local_add_issue(issues, 'seed_tree_policy', ...
            'CROSS_REF', 'branch list must contain forecast_obs_noise');
    end
end

function [issues, detail] = local_check_cross_c(trees, issues, detail)
% T-002 cross-registry closure (delegation spec B):
%   - strata cells vs template registry ids and env regime keys
%   - token ood_axis / support bands vs id_support_bounds numerics
%   - token predicate role values vs segment_role_enum
    d = detail.scenario_stratum_registry;
    d.checks{end+1} = 'cross_refs';
    detail.scenario_stratum_registry = d;
    d2 = detail.heldout_token_registry;
    d2.checks{end+1} = 'cross_refs';
    detail.heldout_token_registry = d2;

    tpl = trees.mission_template_registry;
    env = trees.environment_regime_params;
    bnd = trees.id_support_bounds;
    tok_reg = trees.heldout_token_registry;
    strata_reg = trees.scenario_stratum_registry;

    % 1. strata cells vs template registry ids and env regime keys
    tpl_list = local_as_list(tpl.mission_templates, 'mission_templates');
    tpl_ids = cell(1, numel(tpl_list));
    for a = 1:numel(tpl_list)
        titem = tpl_list{a};
        if isfield(titem, 'mission_template_id')
            tpl_ids(a) = {titem.mission_template_id};
        else
            tpl_ids(a) = {''};
        end
    end
    reg_obj = env.regimes;
    reg_keys = fieldnames(reg_obj);
    strata = local_as_list(strata_reg.strata, 'strata');
    for a = 1:numel(strata)
        c = strata{a};
        if isfield(c, 'mission_template_id') && ischar(c.mission_template_id)
            if ~ismember(c.mission_template_id, tpl_ids)
                issues = local_add_issue(issues, 'scenario_stratum_registry', 'CROSS_REF', ...
                    sprintf('mission_template_id not in template registry: %s', c.mission_template_id));
            end
        end
        if isfield(c, 'environment_regime') && ischar(c.environment_regime)
            if ~ismember(c.environment_regime, reg_keys)
                issues = local_add_issue(issues, 'scenario_stratum_registry', 'CROSS_REF', ...
                    sprintf('environment_regime not in environment_regime_params keys: %s', ...
                    c.environment_regime));
            end
        end
    end

    % 2. token axes and bands vs id_support_bounds
    axes_obj = bnd.axes;
    axis_keys = fieldnames(axes_obj);
    toks = local_as_list(tok_reg.tokens, 'tokens');
    for a = 1:numel(toks)
        tk = toks{a};
        if ~isfield(tk, 'token_id') || ~ischar(tk.token_id), continue; end
        tid = tk.token_id;
        if isfield(tk, 'ood_config_surface') && strcmp(local_value_class(tk.ood_config_surface), 'object')
            ocs = tk.ood_config_surface;
            if isfield(ocs, 'ood_axis') && ischar(ocs.ood_axis)
                if ~ismember(ocs.ood_axis, axis_keys)
                    issues = local_add_issue(issues, 'heldout_token_registry', 'CROSS_REF', ...
                        sprintf('%s: ood_axis not in id_support_bounds axes: %s', tid, ocs.ood_axis));
                end
            end
        end
        if ~isfield(tk, 'support_band') || ~strcmp(local_value_class(tk.support_band), 'object')
            continue;
        end
        sb = tk.support_band;
        if ~isfield(sb, 'min') || ~isnumeric(sb.min) || isempty(sb.min) ...
                || ~isfield(sb, 'max') || ~isnumeric(sb.max) || isempty(sb.max)
            continue;
        end
        if ~isempty(regexp(tid, '^C1_', 'once')) && isfield(axes_obj, 'adverse_current')
            adv = axes_obj.adverse_current;
            if isfield(adv, 'iid_support')
                iid = adv.iid_support;
                if isfield(iid, 'min') && isfield(iid, 'max') && (sb.min < iid.min || sb.max > iid.max)
                    issues = local_add_issue(issues, 'heldout_token_registry', 'CROSS_REF', ...
                        sprintf('%s: HIGH_ID_BAND escapes adverse_current.iid_support', tid));
                end
            end
        end
        if ~isempty(regexp(tid, '^C[23]_', 'once')) && isfield(axes_obj, 'ht_ratio')
            ht = axes_obj.ht_ratio;
            if isfield(ht, 'iid_support')
                iid = ht.iid_support;
                if isfield(iid, 'min') && sb.min ~= iid.min
                    issues = local_add_issue(issues, 'heldout_token_registry', 'CROSS_REF', ...
                        sprintf('%s: LOW_ID_BAND.min must equal ht_ratio.iid_support.min (%g)', tid, iid.min));
                end
            end
        end
    end

    % 3. token predicate role values vs segment_role_enum
    role_enum = local_str_cell(tpl.segment_role_enum);
    for a = 1:numel(toks)
        tk = toks{a};
        if ~isfield(tk, 'token_id') || ~isfield(tk, 'predicate'), continue; end
        if ~iscell(tk.predicate), continue; end
        tid = tk.token_id;
        prd = tk.predicate;
        for b = 1:numel(prd)
            it = prd{b};
            if isstruct(it) && isfield(it, 'field') && ischar(it.field) ...
                    && strcmp(it.field, 'segment.role') && isfield(it, 'operator') ...
                    && strcmp(it.operator, '==') && isfield(it, 'value') && ischar(it.value)
                if ~ismember(it.value, role_enum)
                    issues = local_add_issue(issues, 'heldout_token_registry', 'CROSS_REF', ...
                        sprintf('%s: role value not in segment_role_enum: %s', tid, it.value));
                end
            end
        end
    end
end

function [ok, msg] = local_resolve_ref(ref, reg_by_stem)
% Resolve "stem.path[].to.field" against the typed registry trees.
% Path segments are field names; [] expands (struct arrays and cells).
    ok = false;
    msg = '';
    parts = strsplit(char(ref), '.');
    stem = parts{1};
    if ~isfield(reg_by_stem, stem)
        msg = sprintf('unknown registry stem: %s', stem);
        return;
    end
    first = reg_by_stem.(stem);
    cur = {first};
    for i = 2:numel(parts)
        seg = parts{i};
        expand = false;
        if numel(seg) >= 2 && strcmp(seg(end-1:end), '[]')
            expand = true;
            seg = seg(1:end-2);
        end
        nxt = cell(1, 0);
        for j = 1:numel(cur)
            node = cur{j};
            if isstruct(node) && isfield(node, seg)
                val = node.(seg);
            else
                msg = sprintf('%s: missing field %s', ref, seg);
                return;
            end
            if expand
                % typed tree: arrays are cells, dict-like objects are structs
                if iscell(val)
                    for k2 = 1:numel(val)
                        nxt{end+1} = val{k2};
                    end
                elseif isstruct(val) && isempty(val)
                    msg = sprintf('%s: [] over empty at %s', ref, seg);
                    return;
                elseif isstruct(val)
                    vfn = fieldnames(val);
                    for k2 = 1:numel(vfn)
                        nxt{end+1} = val.(vfn{k2});
                    end
                else
                    msg = sprintf('%s: [] over non-array at %s', ref, seg);
                    return;
                end
            else
                nxt{end+1} = val;
            end
        end
        cur = nxt;
        if isempty(cur)
            msg = sprintf('%s: empty traversal at %s', ref, seg);
            return;
        end
    end
    ok = ~isempty(cur);
    if ~ok
        msg = sprintf('%s: resolves to empty', ref);
    end
end

function out = local_str_cell(v)
% JSON string array -> cellstr (empty on failure).
    out = cell(1, 0);
    if ischar(v)
        out = {v};
    elseif iscell(v)
        keep = cell(1, 0);
        for k = 1:numel(v)
            if ischar(v{k})
                keep{end+1} = v{k};
            end
        end
        out = keep;
    end
end

function s = local_d_canonicalization()
% The D-convention canonicalization string, byte-identical to
% config/dataset_v3_producer_schema.json self_hash.canonicalization.
    q = char(39);  % single quote
    s = ['json.dumps(obj_minus_self_hash, sort_keys=True, separators=(', ...
        q, ',', q, ',', q, ':', q, ...
        '), ensure_ascii=False).encode(', q, 'utf-8', q, ')'];
end
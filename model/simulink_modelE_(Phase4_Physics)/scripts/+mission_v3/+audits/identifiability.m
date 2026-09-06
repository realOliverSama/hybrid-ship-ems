function out = identifiability(mode, varargin)
% mission_v3.audits.identifiability - T-022 T429_EVALUATE identifiability
% Gate execution (FR-547~551; AC-437; design DGN-12 R8/R9; Tasks Gate R3;
% delegation spec experiments/mission_v3/T-022_delegation_spec.md).
%
% Blind-freeze discipline (FR-547/549/550): every evaluation parameter is
% consumed from the immutable blind-freeze artifact
% results/mission_v3/t429_freeze.json (T-021, self_hash f8152ffe...).
% BEFORE any metric is computed the freeze is machine-verified:
%   - freeze self_hash recomputation (canonical JSON minus self_hash,
%     D convention: sort_keys, compact separators, ensure_ascii=False,
%     UTF-8 bytes; byte-identical port of the
%     +mission_v3/validate_registries.m authority algorithm);
%   - referenced objects zero-change checks: contrast registry self_hash,
%     heldout token registry self_hash (= the frozen nuisance-control
%     config hash), prequal corpus manifest raw SHA-256, and the
%     qualified generation stack identity re-collected Strict with
%     composite comparison (mission_v3.stack_identity);
%   - any mismatch raises GATE:MISSION_V3:T429:FREEZE_VIOLATION and the
%     Gate result is void.
%
% Main analysis unit: SEGMENT_PURE_DIAGNOSTIC_SLICE (FR-547).  A slice is
% fully contained in ONE generation-side segment-role window, frozen
% geometry (slice/buffer/stride read from the freeze, never restated),
% 1 Hz P_load_total_kW.  Labels are inherited from generation-side
% metadata only (FR-548): the per-family realization/plan is
% deterministically replayed (mission_v3.env_realize from the frozen
% formal seed at the manifest base_draw_index + mission_v3.plan_compose;
% PREQUAL-AMEND-01 bitwise reuse evidence) and cross-checked against each
% artifact (voyage id, stratum, split group, arms, duration).  Realized
% responses are never used as labels.
%
% Metrics (FR-549), per contrast:
%   SBD separation ratio
%     R = median(D_cross) / max(Q0.95(D_within_A), Q0.95(D_within_B))
%   with SBD = 1 - max NCC on z-normalized equal-length 1 Hz slices
%   (D-side dataset_v3/audit_leakage.py L3 semantics: Paparrizos &
%   Gravano 2015, zero-padded 2n FFT cross-correlation, NUMERIC_FLOOR
%   1e-12 zero-norm defense -> distance 1.0), and a fixed-capacity
%   logistic probe on the frozen 12-dim H1-derived slice summary
%   encoding (six summary statistics mean/std/min/max/p95/rms of the
%   1 Hz P_load_total_kW slice + six a-priori transient count features;
%   L2 C=1.0, class_weight balanced; grouped 5-fold CV by
%   split_group_id; train-only standard scaler; balanced accuracy).
%   Decision rule (frozen): LCB95 >= threshold, group bootstrap over
%   split_group_id (resample count and seeds read from the freeze;
%   SBD seed 42901, probe seed 42902).  Quantiles are nearest-rank order
%   statistics, applied consistently to point estimates and bootstrap
%   resamples; bootstrap resamples carry group multiplicity exactly
%   (weighted order statistic).
%
% Coverage (DGN-12): each REQUIRED contrast needs >= minimum_group_coverage
% (read per contrast from the registry) unique split groups PER CLASS
% plus non-empty slice pools and non-empty cross/within pair sets;
% shortfall = fail-closed INSUFFICIENT_IDENTIFIABILITY_EVIDENCE for that
% contrast (no metrics computed; FR-551: scenario redesign is the only
% legal remediation).
%
% Cohort control (FR-550) is executed at slice-pooling time per the
% registry cohort_predicate declarations:
%   - operator '==' is a hard value filter;
%   - operator 'same' fields enter the cohort key; cross AND within
%     distances are pooled only inside mixed cohort cells (cells
%     containing BOTH classes);
%   - operators 'same_when_feasible' / 'same_when_possible' fields are
%     appended to the key only if >= 2 mixed cells survive, else the
%     field is recorded relaxed (feasibility fallback);
%   - SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE uses the same-base-draw physical
%     counterfactual (registry preferred_mechanism): cohort cell =
%     family (base draw); class A = traffic-ON member slices overlapping
%     an injected traffic window of the replayed plan; class B =
%     traffic-OFF member slices from the SAME segment indices that host
%     class A slices in that family (role/reach/direction matched by
%     construction; both forecast arms contribute symmetrically).
%
% SECONDARY_DIAGNOSTIC contrasts are computed and reported, never
% gate-bearing (registry gate_role; DGN-12 R9).
%
% MODES
%   out = identifiability('verify_freeze')
%       Machine freeze verification only.  Raises
%       GATE:MISSION_V3:T429:FREEZE_VIOLATION on any check failure.
%   out = identifiability('build_slices')
%       verify_freeze + deterministic corpus replay + slice construction.
%       Returns the master slice pool struct S.
%   out = identifiability('sbd', S, idxA, idxB, cellA, cellB)
%       SBD separation ratio + group bootstrap LCB95 for one contrast.
%   out = identifiability('probe', S, idxA, idxB)
%       Logistic probe (12-dim encoding), grouped 5-fold CV, BA point +
%       group bootstrap LCB95 for one contrast.
%   out = identifiability('evaluate')
%       Full Gate orchestration: verify_freeze -> build_slices -> all
%       registry contrasts -> overall verdict -> writes
%       results/mission_v3/identifiability_gate_result.json and
%       results/mission_v3/identifiability_report.md, then raises
%       GATE:MISSION_V3:T429:FAIL when overall=false (controlled FAIL by
%       design; artifacts are always written first).
%
% GATE error ids: GATE:MISSION_V3:T429:{FREEZE_VIOLATION, ARG, REGISTRY,
% REPLAY, COVERAGE, STATS, FAIL, IO}.
%
% Product naming discipline (FR-547 / Tasks Gate R8): products are named
% identifiability_*; the D-side leakage_sbd_* naming is never reused.
%
% Implementation provenance: base MATLAB only, no Statistics Toolbox
% dependency (fitglm/fitclinear probed absent on this host, delegation
% spec FD10 probe-first rule); the logistic probe is a fixed-iteration
% IRLS implementation of the frozen objective (sum of logistic losses +
% (1/(2C))*||w||^2, intercept unpenalized, balanced class weights); the
% availability probe result is recorded in the Gate JSON provenance.
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% no global RNG (local seeded RandStream objects only); ROOT resolved
% from mfilename (4 x fileparts for the +audits depth) with pwd fallback
% (workspace AGENTS.md 20.2).

    if ~(ischar(mode) || isstring(mode))
        error('GATE:MISSION_V3:T429:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'verify_freeze'
            ROOT = local_root();
            out = local_verify_freeze(ROOT);
        case 'build_slices'
            ROOT = local_root();
            frz = local_verify_freeze(ROOT);
            par = local_eval_params(frz);
            out = local_build_slices(ROOT, frz, par);
        case 'sbd'
            if nargin < 6
                error('GATE:MISSION_V3:T429:ARG', ...
                    ['''sbd'' expects: S, idxA, idxB, cellA, cellB']);
            end
            ROOT = local_root();
            frz = local_verify_freeze(ROOT);
            par = local_eval_params(frz);
            S = varargin{1};
            idxA = varargin{2};
            idxB = varargin{3};
            cellA = varargin{4};
            cellB = varargin{5};
            out = local_sbd_eval(S, idxA, idxB, cellA, cellB, par);
        case 'probe'
            if nargin < 4
                error('GATE:MISSION_V3:T429:ARG', ...
                    '''probe'' expects: S, idxA, idxB');
            end
            ROOT = local_root();
            frz = local_verify_freeze(ROOT);
            par = local_eval_params(frz);
            S = varargin{1};
            idxA = varargin{2};
            idxB = varargin{3};
            out = local_probe_eval(S, idxA, idxB, par);
        case 'evaluate'
            ROOT = local_root();
            out = local_evaluate(ROOT);
        otherwise
            error('GATE:MISSION_V3:T429:ARG', ...
                ['unknown mode: %s (implement: verify_freeze / ', ...
                 'build_slices / sbd / probe / evaluate)'], char(mode));
    end
end

%% ======================================================================
%  ROOT + shared io
%% ======================================================================
function root = local_root()
% +audits depth: identifiability.m -> +audits -> +mission_v3 -> scripts ->
% workspace root (4 x fileparts); pwd fallback per AGENTS.md 20.2.
    root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
    if isempty(root) || ~exist(fullfile(root, 'config'), 'dir')
        root = pwd;
    end
end

function obj = local_read_json(ppath)
    if exist(ppath, 'file') ~= 2
        error('GATE:MISSION_V3:T429:IO', ...
            'required file not found: %s', ppath);
    end
    obj = jsondecode(fileread(ppath));
end

function s = local_utc_now()
    s = char(datetime('now', 'TimeZone', 'UTC', ...
        'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'''));
end

function h = local_sha256_bytes(bytes)
% SHA-256 hex of raw bytes (bit-preserving path; same mechanism as
% +mission_v3/stack_identity.m and validate_registries.m).
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(bytes(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function h = local_sha256_file(ppath)
    fid = fopen(ppath, 'r');
    if fid < 0
        error('GATE:MISSION_V3:T429:IO', ...
            'cannot open file for hashing: %s', ppath);
    end
    raw = fread(fid, inf, 'uint8=>uint8');
    fclose(fid);
    h = local_sha256_bytes(raw);
end

function local_write_text(ppath, txt)
    fdir = fileparts(ppath);
    if ~isempty(fdir) && exist(fdir, 'dir') ~= 7
        mkdir(fdir);
    end
    fid = fopen(ppath, 'w');
    if fid < 0
        error('GATE:MISSION_V3:T429:IO', ...
            'cannot open output file: %s', ppath);
    end
    fwrite(fid, txt, 'char');
    fclose(fid);
end

function local_write_json(ppath, obj)
    txt = jsonencode(obj, 'PrettyPrint', true);
    local_write_text([ppath, '.tmp'], txt);
    movefile([ppath, '.tmp'], ppath, 'f');
end

%% ======================================================================
%  canonical JSON (self_hash recompute; byte-identical port of the
%  +mission_v3/validate_registries.m authority algorithm)
%% ======================================================================
function canon = local_canon_json(txt)
% Byte-identical to Python:
%   json.dumps(obj_minus_self_hash, sort_keys=True, separators=(',',':'),
%              ensure_ascii=False).encode('utf-8')
% Works on raw text with a strict parser; the top-level "self_hash"
% member is skipped; numbers keep their lexical int/float class.
    [canon, pos] = local_canon_value(txt, 1, true);
    pos = local_skip_ws(txt, pos);
    if pos <= numel(txt)
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: trailing content after JSON value');
    end
end

function [out, pos] = local_canon_value(txt, pos, is_top)
    pos = local_skip_ws(txt, pos);
    if pos > numel(txt)
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: unexpected end of input');
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
    pos = pos + 1;
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
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: object key expected');
        end
        [key, pos] = local_scan_string(txt, pos);
        if any(strcmp(keys, key))
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: duplicate object key: %s', key);
        end
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt) || txt(pos) ~= ':'
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: object colon expected');
        end
        pos = pos + 1;
        drop = is_top && strcmp(key, 'self_hash');
        if drop
            [~, pos] = local_canon_value(txt, pos, false);
        else
            [v, pos] = local_canon_value(txt, pos, false);
            keys{end+1} = key; %#ok<AGROW>
            vals{end+1} = v; %#ok<AGROW>
        end
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt)
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: unterminated object');
        end
        if txt(pos) == ','
            pos = pos + 1;
        elseif txt(pos) == '}'
            pos = pos + 1;
            break;
        else
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: object comma/brace expected');
        end
    end
    if isempty(keys)
        out = '{}';
        return;
    end
    for j = 1:numel(keys)
        kj = keys{j};
        if ~isempty(kj) && max(abs(kj)) > 127
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: non-ASCII object key');
        end
    end
    [skeys, idx] = sort(keys);
    for j = 2:numel(skeys)
        if strcmp(skeys{j}, skeys{j-1})
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: duplicate key after sort');
        end
    end
    parts = cell(1, numel(skeys));
    for j = 1:numel(skeys)
        parts{j} = ['"', skeys{j}, '":', vals{idx(j)}];
    end
    out = ['{', strjoin(parts, ','), '}'];
end

function [out, pos] = local_canon_array(txt, pos)
    pos = pos + 1;
    vals = cell(1, 0);
    pos = local_skip_ws(txt, pos);
    if pos <= numel(txt) && txt(pos) == ']'
        pos = pos + 1;
        out = '[]';
        return;
    end
    while true
        [v, pos] = local_canon_value(txt, pos, false);
        vals{end+1} = v; %#ok<AGROW>
        pos = local_skip_ws(txt, pos);
        if pos > numel(txt)
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: unterminated array');
        end
        if txt(pos) == ','
            pos = pos + 1;
        elseif txt(pos) == ']'
            pos = pos + 1;
            break;
        else
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: array comma/bracket expected');
        end
    end
    out = ['[', strjoin(vals, ','), ']'];
end

function out = local_canon_number(tok)
% Python parity: int tokens -> %d; float tokens -> shortest round-trip
% representation in Python repr style.
    is_float = ~isempty(regexp(tok, '[.eE]', 'once'));
    v = str2double(tok);
    if ~isfinite(v)
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: non-finite number');
    end
    if ~is_float
        if abs(v) > 2^53 || v ~= round(v)
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: unsafe integer token');
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
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: no round-trip representation for %s', tok);
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
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: literal too short');
    end
    seg = txt(pos:pos+n-1);
    if ~strcmp(seg, lit)
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: invalid literal near position %d', pos);
    end
    pos = pos + n;
    if pos <= numel(txt)
        nxt = txt(pos);
        delim = [',', '}', ']', ':', ' ', char(9), char(10), char(13)];
        if ~ismember(nxt, delim)
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: literal not delimited');
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
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: unexpected char at %d', startp);
    end
    pat = '^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$';
    if isempty(regexp(tok, pat, 'once'))
        error('GATE:MISSION_V3:T429:IO', ...
            'canonical JSON: invalid JSON number: %s', tok);
    end
end

function [out, pos] = local_scan_string(txt, pos)
% pos at opening quote; returns decoded char row and pos after close.
    pos = pos + 1;
    n = numel(txt);
    out = '';
    while true
        if pos > n
            error('GATE:MISSION_V3:T429:IO', ...
                'canonical JSON: unterminated string');
        end
        ch = txt(pos);
        if ch == '"'
            pos = pos + 1;
            break;
        elseif ch == '\\'
            if pos + 1 > n
                error('GATE:MISSION_V3:T429:IO', ...
                    'canonical JSON: dangling escape');
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
                        error('GATE:MISSION_V3:T429:IO', ...
                            'canonical JSON: truncated unicode escape');
                    end
                    cp = hex2dec(txt(pos+2:pos+5));
                    pos = pos + 6;
                    if cp >= 55296 && cp <= 56319 ...
                            && pos + 5 <= n && txt(pos) == '\\' ...
                            && txt(pos+1) == 'u'
                        lo = hex2dec(txt(pos+2:pos+5));
                        if lo >= 56320 && lo <= 57343
                            out = [out, char(cp), char(lo)];
                            pos = pos + 6;
                        else
                            out = [out, char(cp)];
                        end
                    else
                        out = [out, char(cp)];
                    end
                otherwise
                    error('GATE:MISSION_V3:T429:IO', ...
                        'canonical JSON: unknown escape');
            end
        else
            out = [out, ch];
            pos = pos + 1;
        end
    end
end

%% ======================================================================
%  freeze verification (FD1; machine; fail-closed)
%% ======================================================================
function frz = local_verify_freeze(root)
    res_dir = fullfile(root, 'results', 'mission_v3');
    cfg_m3 = fullfile(root, 'config', 'mission_v3');
    frz = struct();
    frz.root = root;
    frz.generated_at = local_utc_now();

    % ---- C1/C2: freeze artifact exists + self_hash recompute ----------
    freeze_path = fullfile(res_dir, 't429_freeze.json');
    frz.freeze_file = 'results/mission_v3/t429_freeze.json';
    frz.freeze_exists = exist(freeze_path, 'file') == 2;
    if ~frz.freeze_exists
        local_freeze_violation(frz, 'freeze artifact t429_freeze.json missing');
    end
    frz.freeze_sha256_raw = local_sha256_file(freeze_path);
    freeze_txt = fileread(freeze_path);
    freeze_obj = local_read_json(freeze_path);
    stored_sh = '';
    if isfield(freeze_obj, 'self_hash') && isfield(freeze_obj.self_hash, 'value')
        stored_sh = char(freeze_obj.self_hash.value);
    end
    canon = local_canon_json(freeze_txt);
    frz.freeze_self_hash_stored = stored_sh;
    frz.freeze_self_hash_recomputed = ...
        local_sha256_bytes(unicode2native(canon, 'UTF-8'));
    frz.freeze_self_hash_ok = strcmp(frz.freeze_self_hash_recomputed, ...
        stored_sh);
    if ~frz.freeze_self_hash_ok
        local_freeze_violation(frz, 'freeze self_hash recompute mismatch');
    end

    % ---- C3: contrast registry ----------------------------------------
    reg_path = fullfile(cfg_m3, 'identifiability_contrast_registry.json');
    reg_txt = fileread(reg_path);
    reg_obj = local_read_json(reg_path);
    reg_stored = char(reg_obj.self_hash.value);
    reg_canon = local_canon_json(reg_txt);
    reg_recomp = local_sha256_bytes(unicode2native(reg_canon, 'UTF-8'));
    frz.registry_hash_stored = reg_stored;
    frz.registry_hash_recomputed = reg_recomp;
    frz.registry_hash_expected_by_freeze = ...
        char(freeze_obj.contrast_registry_hash);
    frz.registry_hash_ok = strcmp(reg_recomp, reg_stored) ...
        && strcmp(reg_recomp, frz.registry_hash_expected_by_freeze);
    if ~frz.registry_hash_ok
        local_freeze_violation(frz, ...
            'contrast registry self_hash mismatch (zero-change violated)');
    end
    frz.registry = reg_obj;

    % ---- C4: heldout token registry = nuisance control config ---------
    tok_path = fullfile(cfg_m3, 'heldout_token_registry.json');
    tok_txt = fileread(tok_path);
    tok_obj = local_read_json(tok_path);
    tok_stored = char(tok_obj.self_hash.value);
    tok_canon = local_canon_json(tok_txt);
    tok_recomp = local_sha256_bytes(unicode2native(tok_canon, 'UTF-8'));
    frz.token_registry_hash_stored = tok_stored;
    frz.token_registry_hash_recomputed = tok_recomp;
    frz.nuisance_config_hash_expected_by_freeze = ...
        char(freeze_obj.nuisance_control_config_hash);
    frz.token_registry_hash_ok = strcmp(tok_recomp, tok_stored) ...
        && strcmp(tok_recomp, frz.nuisance_config_hash_expected_by_freeze);
    if ~frz.token_registry_hash_ok
        local_freeze_violation(frz, ...
            'heldout token registry (nuisance config) self_hash mismatch');
    end

    % ---- C5: prequal corpus manifest ----------------------------------
    man_path = fullfile(res_dir, 'prequal_manifest.json');
    frz.manifest_file = 'results/mission_v3/prequal_manifest.json';
    frz.manifest_sha256_stored = local_sha256_file(man_path);
    frz.manifest_sha256_expected_by_freeze = ...
        char(freeze_obj.prequal_corpus_manifest.sha256);
    frz.manifest_sha256_ok = strcmp(frz.manifest_sha256_stored, ...
        frz.manifest_sha256_expected_by_freeze);
    man = local_read_json(man_path);
    frz.manifest = man;
    n_voy = numel(man.voyages);
    frz.manifest_voyages = double(n_voy);
    frz.manifest_voyages_expected_by_freeze = ...
        double(freeze_obj.prequal_corpus_manifest.voyages);
    frz.manifest_voyages_ok = frz.manifest_voyages == ...
        frz.manifest_voyages_expected_by_freeze;
    if ~frz.manifest_sha256_ok || ~frz.manifest_voyages_ok
        local_freeze_violation(frz, ...
            'prequal corpus manifest sha256/voyage count mismatch');
    end

    % ---- C6: qualified generation stack identity (Strict re-collect) --
    stack_path = fullfile(res_dir, 'qualification_stack_identity.json');
    frz.stack_file_sha256_stored = local_sha256_file(stack_path);
    frz.stack_file_sha256_expected_by_freeze = ...
        char(freeze_obj.qualified_generation_stack_hash.sha256);
    frz.stack_file_sha256_ok = strcmp(frz.stack_file_sha256_stored, ...
        frz.stack_file_sha256_expected_by_freeze);
    fresh = mission_v3.stack_identity('collect', root, 'Strict', true);
    frz.stack_composite_stored = char(fresh.composite_hash);
    frz.stack_composite_expected_by_freeze = ...
        char(freeze_obj.qualified_generation_stack_hash.composite);
    frz.stack_composite_ok = strcmp(frz.stack_composite_stored, ...
        frz.stack_composite_expected_by_freeze);
    if ~frz.stack_file_sha256_ok || ~frz.stack_composite_ok
        local_freeze_violation(frz, ...
            'qualified generation stack identity changed since freeze');
    end

    % ---- C7: frozen evaluation parameters presence (read-back) --------
    geom = freeze_obj.slice_geometry;
    thr = freeze_obj.thresholds;
    sbd_cfg = freeze_obj.sbd_config;
    frz.geometry = struct( ...
        'slice_duration_s', double(geom.slice_duration_s), ...
        'edge_buffer_s', double(geom.edge_buffer_s), ...
        'stride_s', double(geom.stride_s));
    frz.thresholds = struct('R_min', double(thr.R_min), ...
        'A_min', double(thr.A_min));
    frz.sbd_bootstrap = struct( ...
        'n_resamples', double(sbd_cfg.bootstrap.n_resamples), ...
        'seed', double(sbd_cfg.bootstrap.bootstrap_seed));
    frz.probe_seed_expected = 42902;
    frz.overall = true;
end

function local_freeze_violation(frz, why) %#ok<INUSD>
    error('GATE:MISSION_V3:T429:FREEZE_VIOLATION', ...
        ['T429 freeze verification failed: %s; the Gate result is ', ...
         'void (FD1: any referenced-object drift voids the Gate)'], why);
end

%% ======================================================================
%  frozen evaluation parameter extraction (all values from the freeze)
%% ======================================================================
function par = local_eval_params(frz)
    par = struct();
    par.R_min = frz.thresholds.R_min;
    par.A_min = frz.thresholds.A_min;
    par.slice_s = frz.geometry.slice_duration_s;
    par.buffer_s = frz.geometry.edge_buffer_s;
    par.stride_s = frz.geometry.stride_s;
    par.boot_n = frz.sbd_bootstrap.n_resamples;
    par.sbd_seed = frz.sbd_bootstrap.seed;
    par.probe_seed = frz.probe_seed_expected;

    % probe hyperparameters parsed from the frozen probe_config string
    % (fail-closed when the frozen sentence is not machine-parseable)
    frz_obj = local_read_json(fullfile(frz.root, 'results', ...
        'mission_v3', 't429_freeze.json'));
    pcs = char(frz_obj.probe_config.model);
    tok_c = regexp(pcs, 'C=([0-9.]+)', 'tokens', 'once');
    tok_m = regexp(pcs, 'max_iter\s+([0-9]+)', 'tokens', 'once');
    if isempty(tok_c) || isempty(tok_m)
        error('GATE:MISSION_V3:T429:IO', ...
            'frozen probe_config.model is not machine-parseable for C/max_iter');
    end
    par.probe_C = str2double(tok_c{1});
    par.probe_max_iter = str2double(tok_m{1});
    pm = char(frz_obj.probe_config.metric);
    tok_s = regexp(pm, 'seed\s+([0-9]+)', 'tokens', 'once');
    tok_n = regexp(pm, '([0-9]+)\s+resamples', 'tokens', 'once');
    if isempty(tok_s) || isempty(tok_n)
        error('GATE:MISSION_V3:T429:IO', ...
            'frozen probe_config.metric is not machine-parseable for seed/resamples');
    end
    par.probe_seed = str2double(tok_s{1});
    par.boot_n = str2double(tok_n{1});
    if par.probe_seed ~= frz.probe_seed_expected
        error('GATE:MISSION_V3:T429:IO', ...
            'frozen probe seed deviates from the freeze record');
    end

    % evidence-quality guards (implementation constants, recorded)
    par.boot_min_valid_fraction = 0.5;
    par.fold_count = 5;
    par.znorm_floor = 1e-12;      % D-side NUMERIC_FLOOR semantics
    par.time_tol = 1e-9;
end

%% ======================================================================
%  slice construction (FD2; SEGMENT_PURE_DIAGNOSTIC_SLICE)
%% ======================================================================
function S = local_build_slices(root, frz, par)
    man = frz.manifest;
    rows = man.voyages;
    rows = rows(:);
    n_rows = numel(rows);
    if n_rows == 0
        error('GATE:MISSION_V3:T429:REPLAY', 'manifest has no voyages');
    end

    % ---- family grouping (deterministic: sorted family_id) ------------
    fam_ids = cell(1, 0);
    fam_map = containers.Map('KeyType', 'char', 'ValueType', 'double');
    fam_rows = {};
    for i = 1:n_rows
        r = rows(i);
        fid = char(r.family_id);
        if ~isKey(fam_map, fid)
            fam_map(fid) = numel(fam_ids) + 1;
            fam_ids{end+1} = fid; %#ok<AGROW>
            fam_rows{end+1} = {}; %#ok<AGROW>
        end
        fi = fam_map(fid);
        fam_rows{fi}{end+1} = i; %#ok<AGROW>
    end
    [fam_ids, ord] = sort(fam_ids);
    fam_rows = fam_rows(ord);
    nf = numel(fam_ids);

    planj = local_read_json(fullfile(root, 'config', 'mission_v3', ...
        'prequal_plan.json'));
    formal_seed = char(planj.seed_plan.formal_generation_seed);

    % ---- accumulate slices ---------------------------------------------
    P_cells = {};
    meta_cells = {};
    n_slices_total = 0;
    n_artifact_sha_ok = 0;

    for fi = 1:nf
        ridx = fam_rows{fi};
        r0 = rows(ridx{1});
        st = char(r0.stratum_id);
        parts = strsplit(st, '__');
        if numel(parts) ~= 2
            error('GATE:MISSION_V3:T429:REPLAY', ...
                'stratum id not of the form <TPL>__<REG>: %s', st);
        end
        tpl = strrep(char(parts{1}), 'STRAT_', '');
        reg = char(parts{2});
        bdi = double(r0.base_draw_index);
        for jj = 2:numel(ridx)
            rj = rows(ridx{jj});
            same_bdi = double(rj.base_draw_index) == bdi;
            same_st = strcmp(char(rj.stratum_id), st);
            if ~same_bdi || ~same_st
                error('GATE:MISSION_V3:T429:REPLAY', ...
                    'family %s rows disagree on base_draw_index/stratum', ...
                    fam_ids{fi});
            end
        end

        % deterministic replay at the recorded base draw
        samp = mission_v3.env_realize('sample', formal_seed, bdi, ...
            tpl, reg, st, 'MaxAttempts', 1);
        rl = samp.realization;
        if ~strcmp(char(rl.disposition.disposition), 'ACCEPT') ...
                || double(rl.base_draw_index) ~= bdi
            error('GATE:MISSION_V3:T429:REPLAY', ...
                ['family %s: deterministic replay at base_draw_index %d ', ...
                 'did not reproduce an accepted realization'], fam_ids{fi}, ...
                bdi);
        end
        plan_on = mission_v3.plan_compose('compose', rl);
        rl_off = rl;
        rl_off.traffic = rl.traffic([]);
        plan_off = mission_v3.plan_compose('compose', rl_off);
        local_assert_same_segments(plan_on, plan_off, fam_ids{fi});

        ht = double(rl.water.ht_effective);
        if ht >= 1.5 && ht <= 2.0
            depth_class = 'low_id';
        else
            depth_class = 'normal_id';
        end

        % member artifacts of this family
        for jj = 1:numel(ridx)
            ri = ridx{jj};
            r = rows(ri);
            vid = char(r.voyage_id);
            art_rel = char(r.artifact_rel_path);
            art_path = fullfile(root, art_rel);
            if exist(art_path, 'file') ~= 2
                error('GATE:MISSION_V3:T429:REPLAY', ...
                    'artifact missing: %s', art_rel);
            end
            art_sha = local_sha256_file(art_path);
            if strcmp(art_sha, char(r.artifact_sha256))
                n_artifact_sha_ok = n_artifact_sha_ok + 1;
            else
                error('GATE:MISSION_V3:T429:REPLAY', ...
                    'artifact sha256 mismatch vs manifest: %s', vid);
            end
            loaded = load(art_path, 'stream', 'meta');
            meta = loaded.meta;
            stream = loaded.stream;
            local_check_member(meta, r, plan_on, vid, fam_ids{fi});

            arms_traffic = char(r.arms_traffic);
            pl = plan_on;
            if strcmp(arms_traffic, 'OFF')
                pl = plan_off;
            end

            [P_c, meta_c, n_new] = local_member_slices(stream, pl, par, ...
                vid, fi, fam_ids{fi}, char(r.split_group_id), arms_traffic, ...
                char(r.arms_forecast), tpl, reg, ht, depth_class);
            P_cells{end+1} = P_c; %#ok<AGROW>
            meta_cells{end+1} = meta_c; %#ok<AGROW>
            n_slices_total = n_slices_total + n_new;
        end
    end

    % ---- assemble the pool struct --------------------------------------
    P_all = cat(1, P_cells{:});
    M_all = cat(2, meta_cells{:});
    n = size(P_all, 1);
    if n == 0
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'no SEGMENT_PURE_DIAGNOSTIC_SLICE could be constructed');
    end
    S = struct();
    S.n = n;
    S.P = single(P_all);
    % z-normalized representation for SBD (D-side _z semantics: population
    % std; std < NUMERIC_FLOOR -> zero vector -> distance 1.0 defense)
    mu = mean(double(S.P), 2);
    sd = std(double(S.P), 0, 2);
    Zd = (double(S.P) - mu) ./ max(sd, par.znorm_floor);
    Zd(sd < par.znorm_floor, :) = 0;
    S.Z = single(Zd);
    S.znrm = single(sqrt(sum(double(S.Z).^2, 2)));
    S.FZ = fft(S.Z, 2 * par.slice_s, 2);
    S.F = local_slice_features(double(S.P));
    S.slice_s = par.slice_s;
    S.voyage_id = {M_all.voyage_id};
    S.family_idx = [M_all.family_idx];
    S.family_id = fam_ids;
    [ug, ~, gidx] = unique({M_all.split_group_id}, 'stable');
    S.group_ids = ug;
    S.group_idx = gidx(:);
    S.arms_traffic = {M_all.arms_traffic};
    S.arms_forecast = {M_all.arms_forecast};
    S.seg_index = [M_all.seg_index];
    S.role = {M_all.role};
    S.direction = {M_all.direction};
    S.reach = {M_all.reach};
    S.template = {M_all.template};
    S.regime = {M_all.regime};
    S.operator = {M_all.operator};
    S.ht = [M_all.ht];
    S.depth_class = {M_all.depth_class};
    S.traffic_overlap = [M_all.traffic_overlap];
    S.t0 = [M_all.t0];

    S.corpus = struct( ...
        'manifest_file', 'results/mission_v3/prequal_manifest.json', ...
        'voyage_rows', double(n_rows), ...
        'families', double(nf), ...
        'unique_split_groups', double(numel(ug)), ...
        'artifacts_sha_verified', double(n_artifact_sha_ok), ...
        'slices_total', double(n));
end

function local_assert_same_segments(plan_on, plan_off, fid)
    sa = plan_on.segments;
    sb = plan_off.segments;
    if numel(sa) ~= numel(sb)
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'family %s: traffic stripping changed the segment count', fid);
    end
    for i = 1:numel(sa)
        a = sa(i);
        b = sb(i);
        if ~strcmp(char(a.role), char(b.role)) ...
                || abs(double(a.t_start_s) - double(b.t_start_s)) > 1e-9 ...
                || abs(double(a.t_end_s) - double(b.t_end_s)) > 1e-9 ...
                || ~strcmp(char(a.direction), char(b.direction)) ...
                || ~strcmp(char(a.reach), char(b.reach))
            error('GATE:MISSION_V3:T429:REPLAY', ...
                ['family %s segment %d: ON/OFF plans disagree on the ', ...
                 'segment window/role/direction/reach'], fid, i);
        end
    end
end

function local_check_member(meta, r, plan_on, vid, fid)
    if ~strcmp(char(meta.voyage_id), vid)
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'family %s: artifact voyage_id mismatch (%s)', fid, vid);
    end
    if ~strcmp(char(meta.stratum), char(r.stratum_id))
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'family %s member %s: stratum mismatch vs manifest', fid, vid);
    end
    if ~strcmp(char(meta.split_group_id), char(r.split_group_id))
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'family %s member %s: split_group mismatch vs manifest', ...
            fid, vid);
    end
    if ~strcmp(char(meta.mission_template_id), char(plan_on.template_id))
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'family %s member %s: template mismatch (artifact vs replay)', ...
            fid, vid);
    end
    dur_art = double(meta.voyage_duration_s);
    dur_plan = double(plan_on.t_grid_s(end));
    if abs(dur_art - dur_plan) > 1e-6
        error('GATE:MISSION_V3:T429:REPLAY', ...
            ['family %s member %s: artifact duration %.6f vs replayed ', ...
             'plan t_end %.6f'], fid, vid, dur_art, dur_plan);
    end
    iv = char(meta.intervention);
    expect_iv = ['traffic=' char(r.arms_traffic) ...
        ';forecast=' char(r.arms_forecast)];
    if ~strcmp(iv, expect_iv)
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'family %s member %s: intervention %s vs manifest arms', ...
            fid, vid, iv);
    end
end

function [P_c, meta_c, n_new] = local_member_slices(stream, pl, par, vid, ...
        fi, fid, sg_id, arms_traffic, arms_forecast, tpl, reg, ht, ...
        depth_class)
% Frozen-geometry slice extraction over pure segment windows.  All
% geometry values come from the freeze via par (never restated here).
    t = double(stream.time_s);
    n_t = numel(t);
    if n_t < 2
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'member %s: empty stream grid', vid);
    end
    dt = diff(t);
    if max(abs(dt - 1.0)) > par.time_tol || abs(t(1)) > par.time_tol
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'member %s: stream grid is not the 1 Hz grid from t=0', vid);
    end
    Pw = double(stream.P_load_total_kW);
    if numel(Pw) ~= n_t || any(~isfinite(Pw))
        error('GATE:MISSION_V3:T429:REPLAY', ...
            'member %s: P_load_total_kW not a finite [N] column', vid);
    end
    tw = pl.traffic_windows;
    n_tw = numel(tw);
    segs = pl.segments;
    n_seg = numel(segs);
    slice_len = par.slice_s;
    P_c = zeros(0, slice_len);
    meta_c = repmat(blank_slice_meta(), 1, 0);
    n_new = 0;
    for si = 1:n_seg
        sg = segs(si);
        t_lo = double(sg.t_start_s) + par.buffer_s;
        t_hi = double(sg.t_end_s) - par.buffer_s;
        if t_hi - t_lo + par.time_tol < slice_len
            continue;
        end
        k = 0;
        while true
            t0 = double(sg.t_start_s) + par.buffer_s + par.stride_s * k;
            if t0 + slice_len > t_hi + par.time_tol
                break;
            end
            sel = (t >= t0 - par.time_tol) ...
                & (t < t0 + slice_len - par.time_tol);
            idx = find(sel);
            if numel(idx) ~= slice_len
                k = k + 1;
                continue;
            end
            ov = false;
            for w = 1:n_tw
                wv = tw(w);
                a = double(wv.t_ramp_start_s);
                b = double(wv.t_ramp_end_s);
                if (t0 < b) && (t0 + slice_len > a)
                    ov = true;
                    break;
                end
            end
            row = blank_slice_meta();
            row.voyage_id = vid;
            row.family_idx = fi;
            row.family_id = fid;
            row.split_group_id = sg_id;
            row.arms_traffic = arms_traffic;
            row.arms_forecast = arms_forecast;
            row.seg_index = double(sg.index);
            row.role = char(sg.role);
            row.direction = char(sg.direction);
            row.reach = char(sg.reach);
            row.template = tpl;
            row.regime = reg;
            row.operator = char(pl.operator_profile);
            row.ht = ht;
            row.depth_class = depth_class;
            row.traffic_overlap = ov;
            row.t0 = t0;
            meta_c(end+1) = row; %#ok<AGROW>
            P_c(end+1, :) = Pw(idx)'; %#ok<AGROW>
            n_new = n_new + 1;
            k = k + 1;
        end
    end
end

function m = blank_slice_meta()
    m = struct('voyage_id', '', 'family_idx', 0, 'family_id', '', ...
        'split_group_id', '', 'arms_traffic', '', 'arms_forecast', '', ...
        'seg_index', 0, 'role', '', 'direction', '', 'reach', '', ...
        'template', '', 'regime', '', 'operator', '', 'ht', 0, ...
        'depth_class', '', 'traffic_overlap', false, 't0', 0);
end

%% ======================================================================
%  12-dim frozen probe encoding (FD4; freeze probe_config.features)
%% ======================================================================
function F = local_slice_features(P)
% Six summary statistics + six transient count features of the 1 Hz
% P_load_total_kW slice.  The six summary statistics are named in the
% frozen encoding (slice mean/std/min/max/p95/rms); the six transient
% count semantics are fixed here a priori by T-022 implementation
% authority (freeze: implementation per T-022 actual measurement; the
% dimension and type are frozen by the freeze) and are NEVER tuned
% against results (FR-551):
%   f7  n_jump_up_10pct        : 1 s upward steps > 10% of slice mean
%   f8  n_jump_down_10pct      : 1 s downward steps > 10% of slice mean
%   f9  n_ramp_up_5pct_10s     : 10 s rises > 5% of slice mean
%   f10 n_ramp_down_5pct_10s   : 10 s drops > 5% of slice mean
%   f11 n_excursions_above_2sd : entries above mean + 2 sd
%   f12 n_excursions_below_2sd : entries below mean - 2 sd
% All features are causal deployable functions of the slice signal only
% (FR-548: no label-leaking metadata is consumed).
    n = size(P, 1);
    F = zeros(n, 12);
    for i = 1:n
        x = P(i, :);
        mu = mean(x);
        sd = std(x, 0);
        F(i, 1) = mu;
        F(i, 2) = sd;
        F(i, 3) = min(x);
        F(i, 4) = max(x);
        F(i, 5) = local_pctl(x, 0.95);
        F(i, 6) = sqrt(mean(x.^2));
        jump_thr = 0.10 * max(mu, 1e-12);
        ramp_thr = 0.05 * max(mu, 1e-12);
        dx = diff(x);
        F(i, 7) = sum(dx > jump_thr);
        F(i, 8) = sum(-dx > jump_thr);
        if numel(x) > 10
            r10 = x(11:end) - x(1:end-10);
            F(i, 9) = sum(r10 > ramp_thr);
            F(i, 10) = sum(-r10 > ramp_thr);
        end
        if sd > 1e-12
            up = x > mu + 2 * sd;
            dn = x < mu - 2 * sd;
            F(i, 11) = sum(up(2:end) & ~up(1:end-1));
            F(i, 12) = sum(dn(2:end) & ~dn(1:end-1));
        end
    end
end

function q = local_pctl(v, p)
% Linear-interpolation percentile on sorted values (base MATLAB; same
% shape as the producer_prequal local_quantile precedent).
    vs = sort(v(:));
    n = numel(vs);
    if n == 0
        q = NaN;
        return;
    end
    if n == 1
        q = vs(1);
        return;
    end
    x = (0:(n-1))' / (n - 1);
    q = interp1(x, vs, p, 'linear');
end

%% ======================================================================
%  contrast specs (registry cross-checked; FR-550/DGN-12 R8/R9)
%% ======================================================================
function specs = local_contrast_specs()
% Internal pool definitions per contrast_id.  Fields:
%   id, class_A, class_B, hosts ('role'|'direction'|'depth'|'traffic'|
%   'secondary_harbor'|'secondary_bridge'), hard_filter {field, value}
%   (or []), cohort list of {field, op} with op in same/feasible.
    specs = local_spec_row('', '', '', 'role', [], {});
    specs = repmat(specs, 1, 0);
    specs(end+1) = local_spec_row('SEG_BRIDGE_VS_HARBOR', 'bridge', ...
        'harbor', 'role', [], { ...
        local_pred('regime', 'same'), ...
        local_pred('direction', 'same'), ...
        local_pred('template', 'feasible'), ...
        local_pred('operator', 'feasible')});
    specs(end+1) = local_spec_row('SEG_BERTH_VS_DEPARTURE', 'berth', ...
        'departure', 'role', [], {local_pred('template', 'same')});
    specs(end+1) = local_spec_row('DIR_UPSTREAM_VS_DOWNSTREAM', ...
        'upstream', 'downstream', 'direction', [], { ...
        local_pred('regime', 'same'), local_pred('reach', 'same')});
    specs(end+1) = local_spec_row('DEPTH_LOW_ID_VS_NORMAL_ID', 'low_id', ...
        'normal_id', 'depth', [], { ...
        local_pred('template', 'same'), local_pred('regime', 'same')});
    specs(end+1) = local_spec_row('SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE', ...
        'rush', 'return', 'role', {'template', 'EMERGENCY_RESPONSE'}, { ...
        local_pred('regime', 'same'), local_pred('operator', 'same'), ...
        local_pred('reach', 'feasible')});
    specs(end+1) = local_spec_row('SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE', ...
        'traffic_follow', 'free_cruise', 'traffic', [], {});
    specs(end+1) = local_spec_row('SEG_HARBOR_VS_SHALLOW_CRUISE', ...
        'harbor', 'shallow_cruise', 'secondary_harbor', [], {});
    specs(end+1) = local_spec_row('SEG_BRIDGE_VS_SHALLOW_CRUISE', ...
        'bridge', 'shallow_cruise', 'secondary_bridge', [], {});
end

function s = local_spec_row(id, a, b, hosts, hard_filter, cohort)
    s = struct('id', id, 'class_A', a, 'class_B', b, 'hosts', hosts, ...
        'hard_filter', {hard_filter}, 'cohort', {cohort});
end

function p = local_pred(field, op)
    p = struct('field', field, 'op', op);
end

function rows = local_to_list(v)
    rows = {};
    if isstruct(v)
        for i = 1:numel(v)
            rows{end+1} = v(i); %#ok<AGROW>
        end
    elseif iscell(v)
        for i = 1:numel(v)
            if isstruct(v{i})
                rows{end+1} = v{i}; %#ok<AGROW>
            end
        end
    end
end

function local_crosscheck_registry_rows(frz, specs)
% Cross-check class values and gate roles against the frozen registry
% (fail-closed; the internal spec table must mirror the registry).
    reg = frz.registry;
    crows = local_to_list(reg.contrasts);
    if numel(crows) ~= numel(specs)
        error('GATE:MISSION_V3:T429:REGISTRY', ...
            'registry contrast count %d vs implementation %d', ...
            numel(crows), numel(specs));
    end
    spec_by_id = struct();
    for i = 1:numel(specs)
        spec_by_id.(specs(i).id) = specs(i);
    end
    role_expect = containers.Map( ...
        {'SEG_BRIDGE_VS_HARBOR', 'SEG_BERTH_VS_DEPARTURE', ...
         'DIR_UPSTREAM_VS_DOWNSTREAM', 'DEPTH_LOW_ID_VS_NORMAL_ID', ...
         'SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE', ...
         'SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE'}, ...
        repmat({'REQUIRED'}, 1, 6));
    role_expect('SEG_HARBOR_VS_SHALLOW_CRUISE') = 'SECONDARY_DIAGNOSTIC';
    role_expect('SEG_BRIDGE_VS_SHALLOW_CRUISE') = 'SECONDARY_DIAGNOSTIC';
    for i = 1:numel(crows)
        c = crows{i};
        cid = char(c.contrast_id);
        if ~isfield(spec_by_id, cid)
            error('GATE:MISSION_V3:T429:REGISTRY', ...
                'registry contrast %s has no implementation spec', cid);
        end
        sp = spec_by_id.(cid);
        if ~strcmp(char(c.class_A), sp.class_A) ...
                || ~strcmp(char(c.class_B), sp.class_B)
            error('GATE:MISSION_V3:T429:REGISTRY', ...
                'registry class values drifted for %s', cid);
        end
        if ~strcmp(char(c.gate_role), role_expect(cid))
            error('GATE:MISSION_V3:T429:REGISTRY', ...
                'registry gate_role drifted for %s', cid);
        end
    end
end

%% ======================================================================
%  cohort pooling (FD5; FR-550)
%% ======================================================================
function pool = local_contrast_pool(S, sp, min_cov)
% Build the matched-cohort slice pools for one contrast.
    pool = struct();
    pool.id = sp.id;
    pool.class_A = sp.class_A;
    pool.class_B = sp.class_B;
    pool.hosts = sp.hosts;
    pool.min_cov = min_cov;
    pool.applied = {};
    pool.relaxed = {};
    pool.status = '';
    pool.reason = '';

    selA = [];
    selB = [];
    switch sp.hosts
        case 'role'
            selA = find(strcmp(S.role, sp.class_A));
            selB = find(strcmp(S.role, sp.class_B));
        case 'direction'
            selA = find(strcmp(S.direction, sp.class_A));
            selB = find(strcmp(S.direction, sp.class_B));
        case 'depth'
            selA = find(strcmp(S.depth_class, sp.class_A));
            selB = find(strcmp(S.depth_class, sp.class_B));
        case 'secondary_harbor'
            selA = find(strcmp(S.role, 'harbor'));
            selB = find(strcmp(S.role, 'cruise') ...
                & strcmp(S.depth_class, 'low_id'));
        case 'secondary_bridge'
            selA = find(strcmp(S.role, 'bridge'));
            selB = find(strcmp(S.role, 'cruise') ...
                & strcmp(S.depth_class, 'low_id'));
        case 'traffic'
            [selA, selB] = local_traffic_pools(S);
        otherwise
            error('GATE:MISSION_V3:T429:ARG', ...
                'unknown host rule: %s', sp.hosts);
    end
    selA = selA(:);
    selB = selB(:);

    % hard filter (== predicate)
    if ~isempty(sp.hard_filter)
        fld = sp.hard_filter{1};
        val = sp.hard_filter{2};
        vA = local_field_of(S, fld);
        vA = vA(:);
        mask_a = strcmp(vA(selA), val);
        mask_b = strcmp(vA(selB), val);
        selA = selA(mask_a);
        selB = selB(mask_b);
        pool.applied{end+1} = sprintf('%s==%s (hard filter)', fld, val); %#ok<AGROW>
    end

    % cohort key construction
    base_fields = {};
    feas_fields = {};
    for j = 1:numel(sp.cohort)
        pr = sp.cohort{j};
        if strcmp(pr.op, 'same')
            base_fields{end+1} = pr.field; %#ok<AGROW>
        elseif strcmp(pr.op, 'feasible')
            feas_fields{end+1} = pr.field; %#ok<AGROW>
        else
            error('GATE:MISSION_V3:T429:REGISTRY', ...
                'unsupported cohort operator: %s', pr.op);
        end
    end
    for j = 1:numel(base_fields)
        pool.applied{end+1} = sprintf('%s (same)', base_fields{j}); %#ok<AGROW>
    end

    if strcmp(sp.hosts, 'traffic')
        % same-base-draw physical counterfactual: the cohort cell is the
        % family (base draw); cross and within distances are pooled only
        % inside one family (the traffic pools above already restrict
        % class B to the same eventful segment indices per family)
        fam_all = S.family_idx;
        fam_all = fam_all(:);
        keyA = fam_all(selA);
        keyA = keyA(:);
        keyB = fam_all(selB);
        keyB = keyB(:);
        mixed = local_mixed_keys(keyA, keyB);
        pool.applied{end+1} = ['cohort cell = family (same base_draw ', ...
            'traffic ON vs OFF physical counterfactual)']; %#ok<AGROW>
    else
        keyA = local_cohort_key(S, selA, base_fields);
        keyB = local_cohort_key(S, selB, base_fields);
        mixed = local_mixed_keys(keyA, keyB);
    end
    for j = 1:numel(feas_fields)
        cand = [base_fields, feas_fields(j)];
        kA = local_cohort_key(S, selA, cand);
        kB = local_cohort_key(S, selB, cand);
        mix_cand = local_mixed_keys(kA, kB);
        if numel(mix_cand) >= 2
            base_fields = cand;
            keyA = kA;
            keyB = kB;
            mixed = mix_cand;
            pool.applied{end+1} = sprintf( ...
                '%s (same_when_feasible: kept)', feas_fields{j}); %#ok<AGROW>
        else
            pool.relaxed{end+1} = sprintf( ...
                '%s (same_when_feasible: relaxed, mixed cells < 2)', ...
                feas_fields{j}); %#ok<AGROW>
        end
    end

    keepA = ismember(keyA, mixed);
    keepB = ismember(keyB, mixed);
    idxA = selA(keepA);
    idxB = selB(keepB);
    keyA_m = keyA(keepA);
    keyB_m = keyB(keepB);

    pool.n_slices_A = numel(idxA);
    pool.n_slices_B = numel(idxB);
    pool.n_groups_A = numel(unique(S.group_idx(idxA)));
    pool.n_groups_B = numel(unique(S.group_idx(idxB)));
    pool.n_mixed_cells = numel(mixed);

    % numeric cell ids aligned with idxA/idxB (forced column orientation)
    all_keys = [keyA_m(:); keyB_m(:)];
    [uk, ~, cid] = unique(all_keys);
    pool.cells = uk;
    cellA_tmp = cid(1:numel(idxA));
    pool.cellA = cellA_tmp(:);
    cellB_tmp = cid(numel(idxA)+1:end);
    pool.cellB = cellB_tmp(:);
    pool.idxA = idxA(:);
    pool.idxB = idxB(:);
    pool.keyA = keyA_m;
    pool.keyB = keyB_m;

    if pool.n_slices_A == 0 || pool.n_slices_B == 0
        pool.status = 'INSUFFICIENT';
        pool.reason = 'empty slice pool for at least one class inside mixed cohort cells';
        return;
    end
    if pool.n_groups_A < min_cov || pool.n_groups_B < min_cov
        pool.status = 'INSUFFICIENT';
        pool.reason = sprintf(['group coverage below the frozen minimum ', ...
            '%d per class (A=%d, B=%d unique split groups)'], ...
            min_cov, pool.n_groups_A, pool.n_groups_B);
        return;
    end
    pool.status = 'OK';
end

function [selA, selB] = local_traffic_pools(S)
% Same-base-draw physical counterfactual (registry preferred mechanism).
% Class A: traffic-ON member slices overlapping an injected traffic
% window.  Class B: traffic-OFF member slices from the SAME segment
% indices that host class A slices within the same family (role/reach/
% direction matched by construction; cell = family).
    fam_all = S.family_idx;
    fam_all = fam_all(:);
    seg_all = S.seg_index;
    seg_all = seg_all(:);
    arms_all = S.arms_traffic;
    arms_all = arms_all(:);
    ov_all = S.traffic_overlap;
    ov_all = ov_all(:);
    selA = find(strcmp(arms_all, 'ON') & ov_all);
    selA = selA(:);
    selB = zeros(0, 1);
    fams = unique(fam_all(selA));
    fams = fams(:)';
    for f = fams
        in_f = (fam_all == f);
        a_in_f = selA(in_f(selA));
        segs_f = unique(seg_all(a_in_f));
        cand = find(strcmp(arms_all, 'OFF') & in_f ...
            & ismember(seg_all, segs_f));
        selB = [selB; cand(:)]; %#ok<AGROW>
    end
    selB = sort(selB);
end

function v = local_field_of(S, fld)
    switch fld
        case 'regime'
            v = S.regime;
        case 'direction'
            v = S.direction;
        case 'template'
            v = S.template;
        case 'operator'
            v = S.operator;
        case 'reach'
            v = S.reach;
        case 'depth'
            v = S.depth_class;
        case 'role'
            v = S.role;
        otherwise
            error('GATE:MISSION_V3:T429:REGISTRY', ...
                'unsupported cohort field: %s', fld);
    end
end

function key = local_cohort_key(S, sel, fields)
% Deterministic cohort key per slice: 'field1|field2|...' (cellstr
% column); the empty field list collapses to the single global cell
% 'ALL' (no cohort restriction declared).
    n = numel(sel);
    if isempty(fields)
        key = repmat({'ALL'}, n, 1);
        return;
    end
    parts = cell(n, numel(fields));
    for j = 1:numel(fields)
        v = local_field_of(S, fields{j});
        vc = v(sel);
        parts(:, j) = vc(:);
    end
    key = cell(n, 1);
    for i = 1:n
        key{i} = strjoin(parts(i, :), '|');
    end
end

function mixed = local_mixed_keys(keyA, keyB)
    ua = unique(keyA(:));
    ub = unique(keyB(:));
    mixed = intersect(ua, ub);
end

%% ======================================================================
%  SBD metric (FD3; D-side audit_leakage.py semantics)
%% ======================================================================
function res = local_sbd_eval(S, idxA, idxB, cellA, cellB, par)
    res = struct();
    res.metric = 'SBD separation ratio';
    res.definition = ['R = median(D_cross) / max(Q0.95(D_within_A), ', ...
        'Q0.95(D_within_B)); SBD = 1 - max NCC (z-normalized equal-length ', ...
        '1 Hz P_load_total_kW slices; zero-padded 2n FFT; zero-norm ', ...
        'defense -> distance 1.0; D-side audit_leakage.py semantics)'];

    % ---- pair sets inside mixed cohort cells ---------------------------
    [xi, xj] = local_pairs_cross(idxA, idxB, cellA, cellB);
    [ai, aj] = local_pairs_within(idxA, cellA);
    [bi, bj] = local_pairs_within(idxB, cellB);
    res.n_cross_pairs = numel(xi);
    res.n_within_pairs_A = numel(ai);
    res.n_within_pairs_B = numel(bi);
    if isempty(xi) || isempty(ai) || isempty(bi)
        res.status = 'INSUFFICIENT';
        res.reason = 'empty cross or within pair set inside mixed cohort cells';
        res.R_point = NaN;
        res.R_lcb95 = NaN;
        res.pass = false;
        return;
    end

    dx = local_sbd_dists(S, xi, xj);
    da = local_sbd_dists(S, ai, aj);
    db = local_sbd_dists(S, bi, bj);

    % point estimate (nearest-rank order statistics)
    med_x = local_rank_pctl(sort(dx), 0.5);
    q_a = local_rank_pctl(sort(da), 0.95);
    q_b = local_rank_pctl(sort(db), 0.95);
    denom_pt = max(q_a, q_b);
    if ~(denom_pt > 0) || ~isfinite(denom_pt)
        res.status = 'INSUFFICIENT';
        res.reason = 'degenerate within-class Q0.95 (zero or non-finite)';
        res.R_point = NaN;
        res.R_lcb95 = NaN;
        res.pass = false;
        return;
    end
    res.R_point = med_x / denom_pt;
    res.q95_within_A = q_a;
    res.q95_within_B = q_b;
    res.median_cross = med_x;

    % ---- group bootstrap (frozen seed / resample count) ----------------
    gA = S.group_idx(xi);
    gB = S.group_idx(xj);
    ga1 = S.group_idx(ai);
    ga2 = S.group_idx(aj);
    gb1 = S.group_idx(bi);
    gb2 = S.group_idx(bj);
    groups_all = [ga1; ga2; gb1; gb2; gA; gB];
    ug = unique(groups_all);
    n_g = numel(ug);
    lut = zeros(double(max(groups_all)), 1);
    for k = 1:n_g
        lut(double(ug(k))) = k;
    end
    mA1 = lut(ga1);
    mA2 = lut(ga2);
    mB1 = lut(gb1);
    mB2 = lut(gb2);
    mXA = lut(gA);
    mXB = lut(gB);

    [dx_s, o1] = sort(dx);
    wXA_s = mXA(o1);
    wXB_s = mXB(o1);
    [da_s, o2] = sort(da);
    wA1_s = mA1(o2);
    wA2_s = mA2(o2);
    [db_s, o3] = sort(db);
    wB1_s = mB1(o3);
    wB2_s = mB2(o3);

    st = RandStream('mrg32k3a', 'Seed', par.sbd_seed);
    n_boot = par.boot_n;
    R_b = zeros(1, n_boot);
    n_valid = 0;
    for b = 1:n_boot
        r = randi(st, n_g, n_g, 1);
        m = accumarray(r, 1, [n_g, 1]);
        wX = m(wXA_s) .* m(wXB_s);
        med_b = local_wquantile(dx_s, wX, 0.5);
        wA = m(wA1_s) .* m(wA2_s);
        qa_b = local_wquantile(da_s, wA, 0.95);
        wB = m(wB1_s) .* m(wB2_s);
        qb_b = local_wquantile(db_s, wB, 0.95);
        den = max(qa_b, qb_b);
        if ~(den > 0) || ~isfinite(den) || ~isfinite(med_b)
            continue;
        end
        n_valid = n_valid + 1;
        R_b(n_valid) = med_b / den;
    end
    res.n_boot = n_boot;
    res.n_boot_valid = n_valid;
    res.bootstrap_seed = par.sbd_seed;
    res.bootstrap_unit = 'split_group_id';
    if n_valid < par.boot_min_valid_fraction * n_boot
        res.status = 'INSUFFICIENT';
        res.reason = sprintf(['group bootstrap unstable: only %d/%d ', ...
            'valid resamples'], n_valid, n_boot);
        res.R_lcb95 = NaN;
        res.pass = false;
        return;
    end
    R_valid = sort(R_b(1:n_valid));
    res.R_lcb95 = local_rank_pctl(R_valid, 0.025);
    res.status = 'OK';
    res.pass = res.R_lcb95 >= par.R_min;
    res.threshold_R_min = par.R_min;
end

function [pi_, pj_] = local_pairs_cross(idxA, idxB, cellA, cellB)
    pi_ = zeros(0, 1);
    pj_ = zeros(0, 1);
    uc = unique(cellA);
    uc = uc(:)';
    for c = uc
        ia = idxA(cellA == c);
        ib = idxB(cellB == c);
        if isempty(ia) || isempty(ib)
            continue;
        end
        [G1, G2] = ndgrid(ia, ib);
        pi_ = [pi_; G1(:)]; %#ok<AGROW>
        pj_ = [pj_; G2(:)]; %#ok<AGROW>
    end
end

function [pi_, pj_] = local_pairs_within(idx, cellOf)
    pi_ = zeros(0, 1);
    pj_ = zeros(0, 1);
    uc = unique(cellOf);
    uc = uc(:)';
    for c = uc
        ia = idx(cellOf == c);
        if numel(ia) < 2
            continue;
        end
        pr = nchoosek(ia, 2);
        pi_ = [pi_; pr(:, 1)]; %#ok<AGROW>
        pj_ = [pj_; pr(:, 2)]; %#ok<AGROW>
    end
end

function d = local_sbd_dists(S, i1, i2)
% Batched SBD = 1 - max NCC (D-side semantics, equal-length slices).
    n_pair = numel(i1);
    d = ones(n_pair, 1, 'single');
    batch = 20000;
    L2 = 2 * S.slice_s;
    floor_v = single(1e-12);
    for s0 = 1:batch:n_pair
        s1 = min(s0 + batch - 1, n_pair);
        ii = i1(s0:s1);
        jj = i2(s0:s1);
        prod = S.FZ(ii, :) .* conj(S.FZ(jj, :));
        cc = real(ifft(prod, L2, 2));
        ncc_max = max(cc, [], 2);
        denom = S.znrm(ii) .* S.znrm(jj);
        dd = 1 - ncc_max ./ denom;
        dd(denom <= floor_v) = single(1);
        d(s0:s1) = dd;
    end
    d = double(d);
end

function v = local_wquantile(vals_sorted, w, q)
% Weighted nearest-rank order statistic (multiplicity-exact for the
% duplicated bootstrap multiset).
    c = cumsum(double(w(:)));
    total = c(end);
    if ~(total > 0)
        v = NaN;
        return;
    end
    target = q * total;
    idx = find(c >= target - 1e-9, 1, 'first');
    if isempty(idx)
        idx = numel(c);
    end
    v = double(vals_sorted(idx));
end

function v = local_rank_pctl(sorted_vals, q)
% Nearest-rank percentile of a sorted vector (consistent with the
% weighted bootstrap order statistic).
    n = numel(sorted_vals);
    if n == 0
        v = NaN;
        return;
    end
    idx = ceil(q * n);
    if idx < 1
        idx = 1;
    end
    if idx > n
        idx = n;
    end
    v = double(sorted_vals(idx));
end

%% ======================================================================
%  logistic probe (FD4; grouped 5-fold; base MATLAB IRLS)
%% ======================================================================
function res = local_probe_eval(S, idxA, idxB, par)
    res = struct();
    res.metric = 'balanced accuracy (grouped 5-fold logistic probe)';
    sel = [idxA(:); idxB(:)];
    y = [zeros(numel(idxA), 1); ones(numel(idxB), 1)];
    X = S.F(sel, :);
    g = S.group_idx(sel);
    % remap group ids to the dense 1..n_g range actually present in this
    % contrast's pools (the pool spans a subset of the global groups)
    [~, ~, gremap] = unique(g);
    g = gremap;
    n = numel(sel);
    res.n_slices = n;
    res.n_class_A = numel(idxA);
    res.n_class_B = numel(idxB);

    % ---- grouped 5-fold assignment (deterministic greedy balance) ------
    ug = unique(g);
    n_g = numel(ug);
    cnt_all = accumarray(g, 1);
    cnt = cnt_all(ug);
    ord = sortrows([cnt, ug], [-1, 2]);
    fmap = containers.Map('KeyType', 'double', 'ValueType', 'double');
    fold_cnt = zeros(1, par.fold_count);
    for k = 1:n_g
        [~, fi] = min(fold_cnt);
        fmap(double(ord(k, 2))) = fi;
        fold_cnt(fi) = fold_cnt(fi) + ord(k, 1);
    end
    fold_of = zeros(n, 1);
    for k = 1:n
        fold_of(k) = fmap(double(g(k)));
    end

    % ---- CV: train-only scaler + IRLS logistic -------------------------
    lambda = 1.0 / par.probe_C;
    penmask = [0; ones(size(X, 2), 1)];
    yhat = zeros(n, 1);
    fold_ba = nan(par.fold_count, 1);
    fold_valid = false(par.fold_count, 1);
    for fi = 1:par.fold_count
        te = find(fold_of == fi);
        tr = find(fold_of ~= fi);
        if isempty(te) || isempty(tr)
            continue;
        end
        mu = mean(X(tr, :), 1);
        sd = std(X(tr, :), 0, 1);
        sd(sd < 1e-12) = 1;
        Xtr = [ones(numel(tr), 1), (X(tr, :) - mu) ./ sd];
        Xte = [ones(numel(te), 1), (X(te, :) - mu) ./ sd];
        ytr = y(tr);
        n_tr = numel(tr);
        n_pos = sum(ytr == 1);
        n_neg = n_tr - n_pos;
        if n_pos == 0 || n_neg == 0
            continue;
        end
        w = zeros(n_tr, 1);
        w(ytr == 1) = n_tr / (2 * n_pos);
        w(ytr == 0) = n_tr / (2 * n_neg);
        beta = local_irls(Xtr, ytr, w, lambda, par.probe_max_iter, penmask);
        eta = Xte * beta;
        eta = min(eta, 700);
        eta = max(eta, -700);
        p1 = 1 ./ (1 + exp(-eta));
        yhat(te) = double(p1 > 0.5);
        yte = y(te);
        tp = sum(yte == 1 & yhat(te) == 1);
        fn = sum(yte == 1 & yhat(te) == 0);
        tn = sum(yte == 0 & yhat(te) == 0);
        fp = sum(yte == 0 & yhat(te) == 1);
        if (tp + fn) == 0 || (tn + fp) == 0
            continue;
        end
        fold_ba(fi) = 0.5 * (tp / (tp + fn) + tn / (tn + fp));
        fold_valid(fi) = true;
    end
    res.fold_ba = fold_ba;
    res.fold_valid = fold_valid;
    if ~all(fold_valid)
        res.status = 'INSUFFICIENT';
        res.reason = ['grouped 5-fold CV produced an invalid fold ', ...
            '(a class is absent in a test fold)'];
        res.BA_point = NaN;
        res.BA_lcb95 = NaN;
        res.pass = false;
        return;
    end
    res.BA_point = mean(fold_ba);

    % ---- group bootstrap LCB95 over the pooled OOF predictions ---------
    tp_g = accumarray(g, double(y == 1 & yhat == 1), [n_g, 1]);
    fn_g = accumarray(g, double(y == 1 & yhat == 0), [n_g, 1]);
    tn_g = accumarray(g, double(y == 0 & yhat == 0), [n_g, 1]);
    fp_g = accumarray(g, double(y == 0 & yhat == 1), [n_g, 1]);
    st = RandStream('mrg32k3a', 'Seed', par.probe_seed);
    n_boot = par.boot_n;
    BA_b = zeros(1, n_boot);
    n_valid = 0;
    for b = 1:n_boot
        r = randi(st, n_g, n_g, 1);
        m = accumarray(r, 1, [n_g, 1]);
        TP = m' * tp_g;
        FN = m' * fn_g;
        TN = m' * tn_g;
        FP = m' * fp_g;
        if (TP + FN) == 0 || (TN + FP) == 0
            continue;
        end
        n_valid = n_valid + 1;
        BA_b(n_valid) = 0.5 * (TP / (TP + FN) + TN / (TN + FP));
    end
    res.n_boot = n_boot;
    res.n_boot_valid = n_valid;
    res.bootstrap_seed = par.probe_seed;
    res.bootstrap_unit = 'split_group_id (over pooled out-of-fold predictions)';
    if n_valid < par.boot_min_valid_fraction * n_boot
        res.status = 'INSUFFICIENT';
        res.reason = sprintf(['probe group bootstrap unstable: only %d/%d ', ...
            'valid resamples'], n_valid, n_boot);
        res.BA_lcb95 = NaN;
        res.pass = false;
        return;
    end
    res.BA_lcb95 = local_rank_pctl(sort(BA_b(1:n_valid)), 0.025);
    res.status = 'OK';
    res.pass = res.BA_lcb95 >= par.A_min;
    res.threshold_A_min = par.A_min;
    res.model = sprintf(['logistic regression, base-MATLAB IRLS (no ', ...
        'Statistics Toolbox dependency; fitglm/fitclinear probed absent), ', ...
        'L2 C=%.3f (intercept unpenalized), class_weight balanced, ', ...
        'fixed iteration cap %d'], par.probe_C, par.probe_max_iter);
    res.features = ['frozen 12-dim H1-derived slice summary encoding: ', ...
        'mean/std/min/max/p95/rms of the 1 Hz P_load_total_kW slice + six ', ...
        'a-priori transient counts (jump up/down > 10% of mean; 10 s ramp ', ...
        'up/down > 5% of mean; excursions above/below mean +/- 2 sd); ', ...
        'train-only standard scaler'];
end

function beta = local_irls(X1, y, w, lambda, maxit, penmask)
% Ridge-penalized logistic IRLS: minimize sum_i w_i*logloss_i +
% (lambda/2)*||beta_slopes||^2 (intercept unpenalized).  Deterministic;
% fixed iteration cap; fails closed when the cap is exhausted without
% convergence (GATE:MISSION_V3:T429:STATS).
    p_dim = size(X1, 2);
    beta = zeros(p_dim, 1);
    pen = diag(penmask);
    converged = false;
    for it = 1:maxit
        eta = X1 * beta;
        eta = min(eta, 700);
        eta = max(eta, -700);
        pr = 1 ./ (1 + exp(-eta));
        v = w .* pr .* (1 - pr);
        H = X1' * (X1 .* v) + lambda * pen;
        gr = X1' * (w .* (y - pr)) - lambda * (penmask .* beta);
        step = [];
        try
            step = H \ gr;
        catch
            step = pinv(H) * gr;
        end
        if any(~isfinite(step))
            error('GATE:MISSION_V3:T429:STATS', ...
                'IRLS produced a non-finite step at iteration %d', it);
        end
        beta = beta + step;
        if max(abs(step)) < 1e-10
            converged = true;
            break;
        end
    end
    if ~converged
        error('GATE:MISSION_V3:T429:STATS', ...
            ['IRLS did not converge within the frozen iteration cap %d ', ...
             '(stopping per the escalation rule; no result is emitted)'], ...
            maxit);
    end
end

%% ======================================================================
%  evaluate orchestration (FD6/FD7; Gate)
%% ======================================================================
function out = local_evaluate(root)
    t0 = tic;
    % 1. freeze verification (machine; fail-closed before any metric)
    frz = local_verify_freeze(root);
    par = local_eval_params(frz);
    specs = local_contrast_specs();
    local_crosscheck_registry_rows(frz, specs);

    % 2. corpus replay + slice construction
    S = local_build_slices(root, frz, par);

    % 3. per-contrast evaluation
    reg = frz.registry;
    crows = local_to_list(reg.contrasts);
    min_cov_by_id = struct();
    gate_role_by_id = struct();
    for i = 1:numel(crows)
        c = crows{i};
        min_cov_by_id.(char(c.contrast_id)) = double(c.minimum_group_coverage);
        gate_role_by_id.(char(c.contrast_id)) = char(c.gate_role);
    end

    spec_by_id = struct();
    for i = 1:numel(specs)
        spec_by_id.(specs(i).id) = specs(i);
    end

    rows_out = {};
    n_required = 0;
    n_required_pass = 0;
    any_insufficient = false;
    any_metric_fail = false;
    for i = 1:numel(crows)
        c = crows{i};
        cid = char(c.contrast_id);
        sp = spec_by_id.(cid);
        min_cov = min_cov_by_id.(cid);
        gate_role = gate_role_by_id.(cid);
        row = struct();
        row.contrast_id = cid;
        row.factor_axis = char(c.factor_axis);
        row.class_A = sp.class_A;
        row.class_B = sp.class_B;
        row.gate_role = gate_role;
        row.gate_bearing = strcmpi(gate_role, 'REQUIRED');
        row.minimum_group_coverage = min_cov;

        pool = local_contrast_pool(S, sp, min_cov);
        row.n_slices_A = pool.n_slices_A;
        row.n_slices_B = pool.n_slices_B;
        row.n_groups_A = pool.n_groups_A;
        row.n_groups_B = pool.n_groups_B;
        row.n_mixed_cells = pool.n_mixed_cells;
        row.cohort_applied = pool.applied;
        row.cohort_relaxed = pool.relaxed;

        if ~strcmp(pool.status, 'OK')
            row.status = 'INSUFFICIENT_IDENTIFIABILITY_EVIDENCE';
            row.insufficient_reason = pool.reason;
            row.R_point = NaN;
            row.R_lcb95 = NaN;
            row.sbd_pass = false;
            row.BA_point = NaN;
            row.BA_lcb95 = NaN;
            row.probe_pass = false;
            row.sbd = struct('status', 'NOT_COMPUTED');
            row.probe = struct('status', 'NOT_COMPUTED');
            if row.gate_bearing
                any_insufficient = true;
            end
        else
            sbd = local_sbd_eval(S, pool.idxA, pool.idxB, pool.cellA, ...
                pool.cellB, par);
            prb = local_probe_eval(S, pool.idxA, pool.idxB, par);
            row.n_cross_pairs = sbd.n_cross_pairs;
            row.n_within_pairs_A = sbd.n_within_pairs_A;
            row.n_within_pairs_B = sbd.n_within_pairs_B;
            row.R_point = sbd.R_point;
            row.R_lcb95 = sbd.R_lcb95;
            row.sbd_pass = sbd.pass;
            row.sbd_status = sbd.status;
            row.sbd = sbd;
            row.BA_point = prb.BA_point;
            row.BA_lcb95 = prb.BA_lcb95;
            row.probe_pass = prb.pass;
            row.probe_status = prb.status;
            row.probe = local_strip_probe(prb);
            if strcmp(sbd.status, 'INSUFFICIENT') ...
                    || strcmp(prb.status, 'INSUFFICIENT')
                row.status = 'INSUFFICIENT_IDENTIFIABILITY_EVIDENCE';
                if strcmp(sbd.status, 'INSUFFICIENT')
                    row.insufficient_reason = ['SBD: ' sbd.reason];
                else
                    row.insufficient_reason = ['probe: ' prb.reason];
                end
                if row.gate_bearing
                    any_insufficient = true;
                end
            else
                both_pass = sbd.pass && prb.pass;
                if both_pass
                    row.status = 'PASS';
                else
                    row.status = 'METRIC_FAIL';
                    if row.gate_bearing
                        any_metric_fail = true;
                    end
                end
            end
        end
        if row.gate_bearing
            n_required = n_required + 1;
            if strcmp(row.status, 'PASS')
                n_required_pass = n_required_pass + 1;
            end
        end
        rows_out{end+1} = row; %#ok<AGROW>
    end

    overall = (n_required_pass == n_required) && (n_required > 0);
    if overall
        verdict = 'PASS';
    elseif any_insufficient
        verdict = 'FAIL_INSUFFICIENT_EVIDENCE';
    else
        verdict = 'FAIL_METRIC';
    end

    % 4. implementation hash + toolbox probe provenance (FD9/FD10)
    impl_path = [mfilename('fullpath'), '.m'];
    impl_sha = local_sha256_file(impl_path);
    tb_license = license('test', 'Statistics_Toolbox');
    tb_fitglm = exist('fitglm', 'file');
    tb_fitclinear = exist('fitclinear', 'file');
    toolbox_probe = struct( ...
        'statistics_toolbox_license_test', double(tb_license), ...
        'fitglm_available', double(tb_fitglm), ...
        'fitclinear_available', double(tb_fitclinear), ...
        'conclusion', ['Statistics Toolbox functions are absent on this ', ...
            'host (FD10 probe-first); the probe uses the base-MATLAB IRLS ', ...
            'implementation recorded in provenance']);

    % 5. write-ahead self-check + artifact write
    write_ahead = local_write_ahead_check(root, frz);
    gate = struct();
    gate.schema = 'mission_v3_identifiability_gate_result_v1';
    gate.task = ['T-022 T429_EVALUATE identifiability Gate ', ...
        '(FR-547~551; AC-437; DGN-12 R8/R9)'];
    gate.generated_at = local_utc_now();
    gate.freeze_verification = local_frz_summary(frz);
    gate.frozen_parameters = struct( ...
        'R_min', par.R_min, 'A_min', par.A_min, ...
        'slice_duration_s', par.slice_s, 'edge_buffer_s', par.buffer_s, ...
        'stride_s', par.stride_s, ...
        'sbd_bootstrap', struct('n_resamples', par.boot_n, ...
        'seed', par.sbd_seed, 'unit', 'split_group_id'), ...
        'probe_bootstrap_seed', par.probe_seed, ...
        'probe_model', sprintf( ...
        'logistic L2 C=%.3f balanced, grouped 5-fold, train-only scaler', ...
        par.probe_C), ...
        'source', 'results/mission_v3/t429_freeze.json (values read, never restated)');
    gate.corpus = S.corpus;
    gate.slice_encoding = ['12-dim frozen encoding: mean/std/min/max/', ...
        'p95/rms + six a-priori transient count features (jump up/down ', ...
        '>10% of slice mean; 10 s ramp up/down >5% of slice mean; ', ...
        'excursions beyond mean +/- 2 sd); fixed a priori, never tuned'];
    gate.contrasts = rows_out;
    gate.overall = overall;
    gate.verdict = verdict;
    gate.required_contrasts_total = n_required;
    gate.required_contrasts_pass = n_required_pass;
    gate.fail_closed_note = ['FR-547/FR-551: insufficient evidence or a ', ...
        'failed required contrast is fail-closed; scenario redesign is the ', ...
        'only legal remediation; thresholds/geometry/encoding are frozen ', ...
        'and never adjusted after a failure'];
    gate.implementation = struct( ...
        'file', 'scripts/+mission_v3/+audits/identifiability.m', ...
        'sha256', impl_sha);
    gate.statistics_provenance = struct( ...
        'toolbox_probe', toolbox_probe, ...
        'quantile_rule', ['nearest-rank order statistic (point and ', ...
        'bootstrap consistent; bootstrap carries group multiplicity)'], ...
        'sbd_semantics_source', ['dataset_v3/audit_leakage.py L3 (D-side); ', ...
        'SBD = 1 - max NCC, NUMERIC_FLOOR 1e-12'], ...
        'replay_binding', ['env_realize(formal_seed, base_draw_index) + ', ...
        'plan_compose; PREQUAL-AMEND-01 bitwise reuse evidence; ', ...
        'per-artifact cross-checks in identifiability.m']);
    gate.write_ahead_self_check = write_ahead;
    gate.wall_s = toc(t0);

    res_dir = fullfile(root, 'results', 'mission_v3');
    json_path = fullfile(res_dir, 'identifiability_gate_result.json');
    md_path = fullfile(res_dir, 'identifiability_report.md');
    local_write_json(json_path, gate);
    local_write_text(md_path, local_build_md(gate));

    out = struct();
    out.overall = overall;
    out.verdict = verdict;
    out.json_path = json_path;
    out.md_path = md_path;
    out.gate = gate;

    fprintf('[T429] freeze verification: PASS (self_hash + references zero-change)\n');
    fprintf('[T429] slices: %d over %d voyages / %d families / %d split groups\n', ...
        S.n, S.corpus.voyage_rows, S.corpus.families, ...
        S.corpus.unique_split_groups);
    for i = 1:numel(rows_out)
        r = rows_out{i};
        fprintf('[T429] %-36s %-38s groups A/B=%d/%d R_lcb95=%.4f BA_lcb95=%.4f\n', ...
            r.contrast_id, r.status, r.n_groups_A, r.n_groups_B, ...
            double(r.R_lcb95), double(r.BA_lcb95));
    end
    fprintf('[T429] overall=%d verdict=%s (required %d/%d PASS)\n', ...
        overall, verdict, n_required_pass, n_required);
    fprintf('[T429] JSON: %s\n', json_path);
    fprintf('[T429] MD:   %s\n', md_path);
    if ~overall
        error('GATE:MISSION_V3:T429:FAIL', ...
            ['T429 identifiability Gate overall=false (verdict %s); ', ...
             'artifacts written to %s and %s; escalation required ', ...
             '(FR-551: scenario redesign is the only legal remediation)'], ...
            verdict, json_path, md_path);
    end
end

function p = local_strip_probe(prb)
% Compact probe record for the per-contrast row (full scalars are kept;
% only the redundant verbose strings are dropped).
    p = struct();
    p.status = prb.status;
    p.fold_ba = prb.fold_ba;
    p.n_boot = prb.n_boot;
    p.n_boot_valid = prb.n_boot_valid;
    p.bootstrap_seed = prb.bootstrap_seed;
    p.model = prb.model;
    p.features = prb.features;
    if isfield(prb, 'reason')
        p.reason = prb.reason;
    end
end

function w = local_write_ahead_check(root, frz)
% T429 write-ahead self-check (delegation spec product item): freeze
% binding re-confirmed immediately before the artifact write and product
% naming discipline asserted (identifiability_*; never leakage_sbd_*).
    freeze_path = fullfile(root, 'results', 'mission_v3', 't429_freeze.json');
    txt = fileread(freeze_path);
    canon = local_canon_json(txt);
    h = local_sha256_bytes(unicode2native(canon, 'UTF-8'));
    ok_hash = strcmp(h, frz.freeze_self_hash_stored);
    w = struct();
    w.freeze_self_hash_reverified_before_write = ok_hash;
    w.artifact_basename_prefix = 'identifiability_';
    w.artifact_basename_discipline_ok = true;
    w.note = ['products are identifiability_gate_result.json and ', ...
        'identifiability_report.md; the D-side leakage_sbd_* naming is ', ...
        'never reused (FR-547 / Tasks Gate R8)'];
    if ~ok_hash
        error('GATE:MISSION_V3:T429:FREEZE_VIOLATION', ...
            'freeze self_hash changed during evaluation (write-ahead check)');
    end
end

function s = local_frz_summary(frz)
    s = struct();
    s.freeze_file = frz.freeze_file;
    s.freeze_self_hash = frz.freeze_self_hash_stored;
    s.freeze_self_hash_recomputed = frz.freeze_self_hash_recomputed;
    s.freeze_self_hash_ok = frz.freeze_self_hash_ok;
    s.contrast_registry = struct( ...
        'stored', frz.registry_hash_stored, ...
        'recomputed', frz.registry_hash_recomputed, ...
        'expected_by_freeze', frz.registry_hash_expected_by_freeze, ...
        'ok', frz.registry_hash_ok);
    s.heldout_token_registry = struct( ...
        'stored', frz.token_registry_hash_stored, ...
        'recomputed', frz.token_registry_hash_recomputed, ...
        'expected_by_freeze', frz.nuisance_config_hash_expected_by_freeze, ...
        'ok', frz.token_registry_hash_ok);
    s.prequal_manifest = struct( ...
        'file', frz.manifest_file, ...
        'sha256_stored', frz.manifest_sha256_stored, ...
        'sha256_expected_by_freeze', frz.manifest_sha256_expected_by_freeze, ...
        'sha256_ok', frz.manifest_sha256_ok, ...
        'voyages', frz.manifest_voyages, ...
        'voyages_expected_by_freeze', frz.manifest_voyages_expected_by_freeze, ...
        'voyages_ok', frz.manifest_voyages_ok);
    s.qualified_generation_stack = struct( ...
        'file_sha256_stored', frz.stack_file_sha256_stored, ...
        'file_sha256_expected_by_freeze', frz.stack_file_sha256_expected_by_freeze, ...
        'file_sha256_ok', frz.stack_file_sha256_ok, ...
        'composite_stored', frz.stack_composite_stored, ...
        'composite_expected_by_freeze', frz.stack_composite_expected_by_freeze, ...
        'composite_ok', frz.stack_composite_ok, ...
        'recollected_mode', 'Strict');
    s.overall = frz.overall;
end

%% ======================================================================
%  human-readable report
%% ======================================================================
function txt = local_build_md(gate)
    L = {};
    L{end+1} = '# T-022 T429 EVALUATE - Identifiability Gate Report';
    L{end+1} = '';
    L{end+1} = sprintf('generated_at: %s', gate.generated_at);
    L{end+1} = sprintf(['implementation sha256 ', ...
        '(scripts/+mission_v3/+audits/identifiability.m): %s'], ...
        gate.implementation.sha256);
    L{end+1} = sprintf('wall_s: %.1f', gate.wall_s);
    L{end+1} = '';
    L{end+1} = '## 1. Freeze verification (machine, fail-closed)';
    L{end+1} = '';
    fv = gate.freeze_verification;
    L{end+1} = sprintf('- freeze self_hash: %s (recomputed %s, ok=%d)', ...
        fv.freeze_self_hash, fv.freeze_self_hash_recomputed, ...
        fv.freeze_self_hash_ok);
    L{end+1} = sprintf('- contrast registry hash: ok=%d (expected by freeze %s)', ...
        fv.contrast_registry.ok, fv.contrast_registry.expected_by_freeze);
    L{end+1} = sprintf('- heldout token registry (nuisance config) hash: ok=%d', ...
        fv.heldout_token_registry.ok);
    L{end+1} = sprintf('- prequal manifest sha256: ok=%d (%d voyages)', ...
        fv.prequal_manifest.sha256_ok, fv.prequal_manifest.voyages);
    L{end+1} = sprintf(['- qualified generation stack composite ', ...
        '(Strict re-collect): ok=%d'], ...
        fv.qualified_generation_stack.composite_ok);
    L{end+1} = '';
    L{end+1} = '## 2. Corpus and slice construction';
    L{end+1} = '';
    cp = gate.corpus;
    L{end+1} = sprintf(['- corpus: %s (%d voyages, %d families, %d unique ', ...
        'split groups); artifact sha256 verified for %d artifacts'], ...
        cp.manifest_file, cp.voyage_rows, cp.families, ...
        cp.unique_split_groups, cp.artifacts_sha_verified);
    L{end+1} = sprintf(['- SEGMENT_PURE_DIAGNOSTIC_SLICE pool: %d slices ', ...
        '(frozen geometry: 120 s slice / 30 s edge buffer / 60 s stride)'], ...
        cp.slices_total);
    L{end+1} = sprintf('- slice encoding: %s', gate.slice_encoding);
    L{end+1} = '';
    L{end+1} = '## 3. Required contrasts (gate-bearing)';
    L{end+1} = '';
    L{end+1} = ['| contrast_id | n_slices A/B | n_groups A/B (min cov 10 ', ...
        'per class) | R_point | R_lcb95 (>= 1.5) | SBD | BA_point | ', ...
        'BA_lcb95 (>= 0.80) | probe | status |'];
    L{end+1} = '|---|---|---|---|---|---|---|---|---|---|';
    for i = 1:numel(gate.contrasts)
        r = gate.contrasts{i};
        if ~r.gate_bearing
            continue;
        end
        L{end+1} = sprintf( ...
            '| %s | %d/%d | %d/%d | %.4f | %.4f | %d | %.4f | %.4f | %d | %s |', ...
            r.contrast_id, r.n_slices_A, r.n_slices_B, r.n_groups_A, ...
            r.n_groups_B, double(r.R_point), double(r.R_lcb95), ...
            r.sbd_pass, double(r.BA_point), double(r.BA_lcb95), ...
            r.probe_pass, r.status);
    end
    L{end+1} = '';
    L{end+1} = '### Per-contrast detail (required)';
    L{end+1} = '';
    for i = 1:numel(gate.contrasts)
        r = gate.contrasts{i};
        if ~r.gate_bearing
            continue;
        end
        L{end+1} = sprintf('#### %s (%s)', r.contrast_id, r.status);
        L{end+1} = '';
        L{end+1} = sprintf(['- factor axis: %s; class_A=%s; class_B=%s; ', ...
            'gate_role=%s'], r.factor_axis, r.class_A, r.class_B, ...
            r.gate_role);
        L{end+1} = sprintf(['- matched cohort: %d mixed cells; applied: ', ...
            '%s; relaxed: %s'], r.n_mixed_cells, ...
            local_join_or_none(r.cohort_applied), ...
            local_join_or_none(r.cohort_relaxed));
        if isfield(r, 'n_cross_pairs')
            L{end+1} = sprintf('- pairs: cross %d, within_A %d, within_B %d', ...
                r.n_cross_pairs, r.n_within_pairs_A, r.n_within_pairs_B);
            L{end+1} = sprintf(['- SBD: median_cross %.4f, Q0.95 within_A ', ...
                '%.4f, Q0.95 within_B %.4f, R_point %.4f, R_lcb95 %.4f ', ...
                '(bootstrap %d/%d valid, seed %d)'], ...
                r.sbd.median_cross, r.sbd.q95_within_A, r.sbd.q95_within_B, ...
                r.R_point, r.R_lcb95, r.sbd.n_boot_valid, r.sbd.n_boot, ...
                r.sbd.bootstrap_seed);
            L{end+1} = sprintf(['- probe: BA_point %.4f, BA_lcb95 %.4f ', ...
                '(bootstrap %d/%d valid, seed %d); fold BAs %s'], ...
                r.BA_point, r.BA_lcb95, r.probe.n_boot_valid, ...
                r.probe.n_boot, r.probe.bootstrap_seed, ...
                local_vec_str(r.probe.fold_ba));
        end
        if isfield(r, 'insufficient_reason')
            L{end+1} = sprintf('- FAIL-CLOSED: %s', r.insufficient_reason);
        end
        L{end+1} = '';
    end
    L{end+1} = '## 4. Secondary diagnostics (non-gate; DGN-12 R9)';
    L{end+1} = '';
    L{end+1} = ['| contrast_id | n_slices A/B | n_groups A/B | R_point | ', ...
        'R_lcb95 | BA_point | BA_lcb95 | status |'];
    L{end+1} = '|---|---|---|---|---|---|---|---|';
    for i = 1:numel(gate.contrasts)
        r = gate.contrasts{i};
        if r.gate_bearing
            continue;
        end
        L{end+1} = sprintf( ...
            '| %s | %d/%d | %d/%d | %.4f | %.4f | %.4f | %.4f | %s |', ...
            r.contrast_id, r.n_slices_A, r.n_slices_B, r.n_groups_A, ...
            r.n_groups_B, double(r.R_point), double(r.R_lcb95), ...
            double(r.BA_point), double(r.BA_lcb95), r.status);
    end
    L{end+1} = '';
    L{end+1} = '## 5. Gate verdict';
    L{end+1} = '';
    L{end+1} = sprintf('- overall: %d', gate.overall);
    L{end+1} = sprintf('- verdict: %s', gate.verdict);
    L{end+1} = sprintf('- required contrasts PASS: %d / %d', ...
        gate.required_contrasts_pass, gate.required_contrasts_total);
    L{end+1} = '';
    L{end+1} = gate.fail_closed_note;
    L{end+1} = '';
    L{end+1} = '## 6. Method provenance';
    L{end+1} = '';
    sp = gate.statistics_provenance;
    L{end+1} = sprintf('- SBD semantics: %s', sp.sbd_semantics_source);
    L{end+1} = sprintf('- probe: %s', ...
        ['base-MATLAB IRLS logistic (Statistics Toolbox functions probed ', ...
         'absent, FD10); frozen encoding, grouped 5-fold CV by ', ...
         'split_group_id, train-only standard scaler']);
    L{end+1} = sprintf('- quantile rule: %s', sp.quantile_rule);
    L{end+1} = sprintf('- replay binding: %s', sp.replay_binding);
    L{end+1} = '';
    txt = strjoin(L, newline);
    txt = [txt, newline];
end

function s = local_join_or_none(c)
    if isempty(c)
        s = 'none';
        return;
    end
    parts = cell(1, numel(c));
    for i = 1:numel(c)
        parts{i} = char(c{i});
    end
    s = strjoin(parts, '; ');
end

function s = local_vec_str(v)
    parts = cell(1, numel(v));
    for i = 1:numel(v)
        parts{i} = sprintf('%.4f', double(v(i)));
    end
    s = ['[' strjoin(parts, ', ') ']'];
end

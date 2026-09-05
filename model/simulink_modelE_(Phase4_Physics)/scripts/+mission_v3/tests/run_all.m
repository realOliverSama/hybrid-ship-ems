function overall = run_all(mode)
% mission_v3.tests.run_all - T-018: unit-test integration + deterministic
%   two-run regression entry (Phase 4-C; NFR-428/432; AC-438; delegation
%   spec experiments/mission_v3/T-018_delegation_spec.md FD1-FD6).
%
%   run_all()                 'run' mode (default)
%   run_all('run')            ten suites + Model E zero-change snapshot
%   run_all('tworun')         same + full-chain determinism two-run check
%   overall = run_all(...)    true iff everything passed
%
% Script-style invocation (zero-arg default 'run') from the Model E
% workspace root is supported (run executes this zero-arg function file):
%   matlab -batch "run('scripts/+mission_v3/tests/run_all.m')"
% Function-style invocation (tests/ is a plain subdirectory of +mission_v3,
% not a +tests package, so the entry is the function name on the path):
%   matlab -batch "addpath(fullfile(pwd,'scripts','+mission_v3','tests')); run_all('tworun');"
%
% FD1 suite list (ten, in order, each executed via run(...), suite scripts
% are never modified - run_all is an entry, not a mutator):
%   validate_registries / seed_tree / env_realize / plan_compose /
%   forecast_issue / kf_snapshots / family_expand / stack_identity /
%   producer_export / negative
% Each suite carries its own PASS/FAIL summary and GATE error.  A suite
% failure is caught and recorded; the remaining suites still execute (audit
% completeness).  At the end: all pass -> print and return overall=true;
% any failure -> error('GATE:run_all %d suite(s) failed: ...').
%
% FD2 determinism ('tworun' mode): the full scientific-product chain
% (fixed seed, T-017 chain assembly style: draw -> realize -> compose ->
% compile -> forecast -> kf) is executed twice and compared level by level
% via SHA-256 (java MessageDigest byte path) over a structured
% serialization in the family_expand truth_hash style:
%   realization.u_river_mps / scn fields / kf all fields / plan all fields
% Any level mismatch -> error('GATE:run_all DETERMINISM').
%
% FD3 zero-change snapshot: mission_v3.stack_identity('collect', ROOT,
% 'Strict', true) - the ten-component composite hash is recorded in the
% regression report (forecast_issue.m exists, Strict passes; the suite
% test_stack_identity itself runs against tempdir fixtures and never
% hashes the live tree).
%
% FD4 regression record: results/mission_v3/regression_report.json -
% {schema, generated_at, mode, suites: [{name, passed, n_cases | error}],
% determinism: {hashes: {run1, run2}, identical} (tworun only),
% stack_identity: {composite_hash, generator_hash_domain}, overall, notes}.
%
% FD5 modes: 'run' (default) = ten suites + zero-change snapshot;
% 'tworun' = appended determinism two-run.  Both modes write the report.
%
% FD6 discipline: ASCII-only source; no absolute paths; no chained
% indexing (multi-level reads via temp variables); ROOT via mfilename +
% pwd fallback; no global RNG.  Body locals are ra_-prefixed: the suites
% are executed via evalc(run(...)) and their script variables land in this
% workspace - the prefix prevents any clobbering of the runner state.
%
% Registered side effects of collected suites (not suppressed, recorded in
% the report notes): test_plan_compose writes its example profile PNGs
% under results/mission_v3/plan_profile_examples/, test_negative writes
% results/mission_v3/negative_detection_report.json.  run_all itself
% writes only results/mission_v3/regression_report.json.

    if nargin < 1
        mode = 'run';
    end
    if ~ischar(mode) && ~isstring(mode)
        error('GATE:run_all ARG: mode must be a char vector');
    end
    ra_mode = char(mode);
    if ~strcmp(ra_mode, 'run') && ~strcmp(ra_mode, 'tworun')
        error('GATE:run_all ARG: unknown mode: %s (expected ''run'' or ''tworun'')', ...
            ra_mode);
    end
    ra_is_tworun = strcmp(ra_mode, 'tworun');

    % ROOT via mfilename + pwd fallback (AGENTS.md 20.2; script depth =
    % tests -> +mission_v3 -> scripts -> workspace root = 4 fileparts).
    ra_root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
    if isempty(ra_root) || exist(fullfile(ra_root, 'config'), 'dir') ~= 7
        ra_root = pwd;
    end
    ra_tests_dir = fileparts(mfilename('fullpath'));
    addpath(fullfile(ra_root, 'scripts'));

    RA_SUITES = {'validate_registries', 'seed_tree', 'env_realize', ...
        'plan_compose', 'forecast_issue', 'kf_snapshots', ...
        'family_expand', 'stack_identity', 'producer_export', 'negative'};

    % ---------------- FD1: ten suites, catch - record - continue ---------
    ra_suites = cell(1, numel(RA_SUITES));
    for ra_i = 1:numel(RA_SUITES)
        ra_sname = RA_SUITES{ra_i};
        ra_spath = fullfile(ra_tests_dir, ['test_', ra_sname, '.m']);
        fprintf('[R] suite %d/%d: %s\n', ra_i, numel(RA_SUITES), ra_sname);
        if exist(ra_spath, 'file') ~= 2
            ra_row = struct('name', ra_sname, 'passed', false, ...
                'error', ['suite script missing: test_', ra_sname, '.m']);
            ra_suites{ra_i} = ra_row;
            fprintf('[R] suite %s FAILED: %s\n', ra_sname, ra_row.error);
            continue;
        end
        try
            % evalc keeps each suite's own log for n_cases parsing; the
            % captured log is echoed verbatim below (audit completeness).
            ra_out = evalc('run(ra_spath)');
        catch ra_err
            ra_row = struct('name', ra_sname, 'passed', false, ...
                'error', local_err_text(ra_err));
            ra_suites{ra_i} = ra_row;
            fprintf('[R] suite %s FAILED: %s\n', ra_sname, ra_row.error);
            continue;
        end
        fprintf('%s', ra_out);
        % n_cases from the suite's own success summary line (ALL PASS /
        % ALL DETECTED (n/n)); NaN = suite passed without a parseable
        % summary (recorded as JSON null, never fabricated).
        ra_tok = regexp(ra_out, 'ALL (PASS|DETECTED) \((\d+)/(\d+)\)', ...
            'tokens', 'once');
        ra_ncases = NaN;
        if ~isempty(ra_tok)
            ra_ncases = str2double(ra_tok{2});
        end
        ra_row = struct('name', ra_sname, 'passed', true, ...
            'n_cases', ra_ncases);
        ra_suites{ra_i} = ra_row;
        fprintf('[R] suite %s PASSED (cases=%s)\n', ra_sname, ...
            local_num_str(ra_ncases));
    end

    % ---------------- FD2: determinism two-run ('tworun' only) -----------
    ra_det = [];
    if ra_is_tworun
        fprintf('[R] determinism two-run: chain pass 1 ...\n');
        ra_ch1 = local_run_chain();
        fprintf('[R] determinism two-run: chain pass 2 ...\n');
        ra_ch2 = local_run_chain();
        ra_h1 = struct('realization_u_river_mps', ...
            local_vec_hash(ra_ch1.u_river), ...
            'scn', local_ser_hash('scn', ra_ch1.scn), ...
            'kf', local_ser_hash('kf', ra_ch1.kf), ...
            'plan', local_ser_hash('plan', ra_ch1.plan));
        ra_h2 = struct('realization_u_river_mps', ...
            local_vec_hash(ra_ch2.u_river), ...
            'scn', local_ser_hash('scn', ra_ch2.scn), ...
            'kf', local_ser_hash('kf', ra_ch2.kf), ...
            'plan', local_ser_hash('plan', ra_ch2.plan));
        fprintf('[R] ---- determinism two-run comparison ----\n');
        ra_lvls = fieldnames(ra_h1);
        ra_det_ok = true;
        ra_bad = cell(1, 0);
        for ra_i = 1:numel(ra_lvls)
            ra_lv = ra_lvls{ra_i};
            ra_a = ra_h1.(ra_lv);
            ra_b = ra_h2.(ra_lv);
            ra_eq = strcmp(ra_a, ra_b);
            ra_det_ok = ra_det_ok && ra_eq;
            if ~ra_eq
                ra_bad{end+1} = ra_lv; %#ok<AGROW>
            end
            fprintf('[R] %-24s run1=%s\n', ra_lv, ra_a);
            fprintf('[R] %-24s run2=%s equal=%d\n', '', ra_b, ra_eq);
        end
        fprintf('[R] determinism identical=%d\n', ra_det_ok);
        ra_det = struct('hashes', struct('run1', ra_h1, 'run2', ra_h2), ...
            'identical', ra_det_ok);
        ra_det_bad = ra_bad;
    end

    % ---------------- FD3: Model E zero-change snapshot ------------------
    fprintf('[R] Model E zero-change snapshot: stack_identity collect Strict ...\n');
    try
        ra_man = mission_v3.stack_identity('collect', ra_root, ...
            'Strict', true);
        ra_stack_ok = true;
        ra_stack = struct('composite_hash', ra_man.composite_hash, ...
            'generator_hash_domain', ra_man.generator_hash_domain);
    catch ra_stk_err
        ra_stack_ok = false;
        ra_stack = struct('error', local_err_text(ra_stk_err));
    end
    if ra_stack_ok
        fprintf('[R] stack_identity composite=%s\n', ra_stack.composite_hash);
    else
        fprintf('[R] stack_identity FAILED: %s\n', ra_stack.error);
    end

    % ---------------- FD4: regression record -----------------------------
    ra_n_pass = 0;
    ra_failed_names = cell(1, 0);
    for ra_i = 1:numel(ra_suites)
        ra_row = ra_suites{ra_i};
        if ra_row.passed
            ra_n_pass = ra_n_pass + 1;
        else
            ra_failed_names{end+1} = ra_row.name; %#ok<AGROW>
        end
    end
    ra_n_fail = numel(ra_suites) - ra_n_pass;
    ra_overall = (ra_n_fail == 0) && ra_stack_ok;
    if ra_is_tworun
        ra_overall = ra_overall && ra_det.identical;
    end

    ra_report_path = fullfile(ra_root, 'results', 'mission_v3', ...
        'regression_report.json');
    ra_rep = struct();
    ra_rep.schema = 'mission_v3_regression_report_v1';
    ra_rep.generated_at = local_iso_utc();
    ra_rep.mode = ra_mode;
    ra_rep.suites = ra_suites;
    if ra_is_tworun
        ra_rep.determinism = ra_det;
    end
    ra_rep.stack_identity = ra_stack;
    ra_rep.overall = ra_overall;
    ra_rep.notes = ['T-018 integration + regression entry (FD1-FD6); ', ...
        'suites are executed unmodified via run(); captured suite logs ', ...
        'are echoed above; n_cases comes from each suite''s own ALL ', ...
        'PASS/DETECTED (n/n) summary and is null when not parseable. ', ...
        'Registered suite side effects (not suppressed): test_plan_compose ', ...
        'writes its example profile PNGs under ', ...
        'results/mission_v3/plan_profile_examples/, test_negative writes ', ...
        'results/mission_v3/negative_detection_report.json; run_all ', ...
        'itself writes only this regression report. Zero-change snapshot ', ...
        '(FD3): mission_v3.stack_identity collect Strict=true over the ', ...
        'live tree (the test_stack_identity suite itself runs on tempdir ', ...
        'fixtures). Determinism (FD2, tworun only): T-017 chain assembly ', ...
        'style on fixture seed t018_regression_seed_01 (test fixture, not ', ...
        'a frozen scientific value); levels realization.u_river_mps / scn ', ...
        'fields / kf all fields / plan all fields; SHA-256 via the java ', ...
        'MessageDigest byte path over a structured serialization in the ', ...
        'family_expand truth_hash style'];
    ra_dir = fileparts(ra_report_path);
    if ~exist(ra_dir, 'dir')
        mkdir(ra_dir);
    end
    ra_fid = fopen(ra_report_path, 'w');
    if ra_fid < 0
        error('GATE:run_all cannot write the regression report: %s', ...
            ra_report_path);
    end
    fwrite(ra_fid, jsonencode(ra_rep, 'PrettyPrint', true), 'char');
    fclose(ra_fid);
    fprintf('[R] report written: results/mission_v3/regression_report.json\n');

    % ---------------- summary + gate (FD1/FD2) ---------------------------
    fprintf('[R] ---- run_all summary (mode=%s) ----\n', ra_mode);
    for ra_i = 1:numel(ra_suites)
        ra_row = ra_suites{ra_i};
        if ra_row.passed
            fprintf('[R] suite %-20s PASS cases=%s\n', ra_row.name, ...
                local_num_str(ra_row.n_cases));
        else
            fprintf('[R] suite %-20s FAIL error=%s\n', ra_row.name, ...
                ra_row.error);
        end
    end
    if ra_is_tworun
        fprintf('[R] determinism identical=%d\n', ra_det.identical);
    else
        fprintf('[R] determinism identical=n/a (mode=run)\n');
    end
    fprintf('[R] suites %d/%d passed\n', ra_n_pass, numel(ra_suites));
    fprintf('[R] overall=%d\n', ra_overall);

    if ra_overall
        fprintf('run_all: ALL GREEN (mode=%s, suites=%d/%d, stack=%s)\n', ...
            ra_mode, ra_n_pass, numel(ra_suites), ...
            local_bool_str(ra_stack_ok));
        overall = true;
        return;
    end

    overall = false;
    if ra_is_tworun && ~ra_det.identical
        error(['GATE:run_all DETERMINISM: scientific payload hashes ', ...
            'differ at level(s): %s; see ', ...
            'results/mission_v3/regression_report.json'], ...
            strjoin(ra_det_bad, ', '));
    end
    if ra_n_fail > 0
        error(['GATE:run_all %d suite(s) failed: %s; see ', ...
            'results/mission_v3/regression_report.json'], ra_n_fail, ...
            strjoin(ra_failed_names, ', '));
    end
    error(['GATE:run_all stack identity collect failed: %s; see ', ...
        'results/mission_v3/regression_report.json'], ra_stack.error);
end

%% ======================== local functions ===========================

function ch = local_run_chain()
% T-017 chain assembly style (test_negative.m positive chain, FD2):
% fixed-seed scan for an accepted draw, then compose -> compile ->
% forecast -> kf.  Deterministic regression fixture, not a frozen value.
    FS = 't018_regression_seed_01';
    TPL = 'ROUTINE_PATROL';
    REG = 'NORMAL';
    STRAT = 'STRAT_ROUTINE_PATROL__NORMAL';
    LEADS = 10:10:300;
    T0 = 600;
    MAX_SCAN = 60;

    rl = [];
    dag = [];
    for k = 0:(MAX_SCAN - 1)
        r = mission_v3.env_realize('sample', FS, k, TPL, REG, STRAT, ...
            'MaxAttempts', 1, 'ExhaustAction', 'return_ledger');
        if ~r.accepted
            continue;
        end
        rl = r.realization;
        row_ok = r.ledger(r.n_attempts);
        dag = row_ok.draw;
        break;
    end
    if isempty(rl)
        error(['GATE:run_all CHAIN: no accepted %s x %s draw within ', ...
            '%d scans'], TPL, REG, MAX_SCAN);
    end
    disp_s = rl.disposition;
    if ~strcmp(disp_s.disposition, 'ACCEPT')
        error('GATE:run_all CHAIN: draw disposition %s', ...
            disp_s.disposition);
    end

    plan_s = mission_v3.plan_compose('compose', rl);
    comp = mission_v3.scenario_compile('compile', plan_s, rl);
    scn_s = comp.scn;

    tg = plan_s.t_grid_s;
    D = tg(end);
    if (D < 910) || (T0 + LEADS(end) > D)
        error(['GATE:run_all CHAIN: plan horizon %d s too short for ', ...
            'the T-017 window geometry'], D);
    end
    ur = rl.u_river_mps;
    anchors = (300:10:(D - 300))';
    fb_all = [];
    for m = 1:numel(anchors)
        a_m = anchors(m);
        sl_m = tg <= a_m;
        fb_m = mission_v3.forecast_issue('issue', -ur(sl_m), tg(sl_m), ...
            a_m, LEADS, 'GOOD_V1');
        fb_all = [fb_all; fb_m]; %#ok<AGROW>
    end
    wtr = rl.water;
    kf_s = mission_v3.kf_snapshots('build_grid', plan_s, anchors, ...
        fb_all, 'HChartM', wtr.h_chart_m);
    cs_last = kf_s.contract_status{end};
    if ~strcmp(cs_last, 'OK')
        error('GATE:run_all CHAIN: K/F snapshots not fully covered (%s)', ...
            cs_last);
    end

    ch = struct();
    ch.dag = dag;
    ch.u_river = ur;
    ch.scn = scn_s;
    ch.plan = plan_s;
    ch.kf = kf_s;
end

function h = local_vec_hash(v)
% FD2 level 1: SHA-256 over realization.u_river_mps (full vector,
% %.17g lossless round-trip format, family_expand truth_hash style).
    if ~isnumeric(v) || ~isreal(v)
        error('GATE:run_all SERIALIZER: u_river_mps must be real numeric');
    end
    lines = {'MISSION_V3_DET_REALIZATION_U_RIVER_V1'};
    lines{end+1} = sprintf('n=%d', numel(v)); %#ok<AGROW>
    lines{end+1} = sprintf('%.17g ', v); %#ok<AGROW>
    h = local_sha256_text(strjoin(lines, newline));
end

function h = local_ser_hash(name, s)
% FD2 levels 2-4: SHA-256 over the structured serialization of a whole
% struct (scn fields / kf all fields / plan all fields).
    lines = {'MISSION_V3_DET_STRUCT_HASH_V1'};
    lines{end+1} = ['object=', name]; %#ok<AGROW>
    local_ser(s, name, lines);
    h = local_sha256_text(strjoin(lines, newline));
end

function local_ser(val, path, lines)
% Recursive deterministic serializer (fieldnames order is canonical in
% MATLAB; numeric arrays via %.17g lossless text; fail-closed on
% unsupported classes).  No chained indexing: every read is single-level
% through a temp variable.
    if isstruct(val)
        fn = fieldnames(val);
        fnc = '';
        if ~isempty(fn)
            fnc = strjoin(fn.', ',');
        end
        lines{end+1} = sprintf('%s struct n=%d fields=[%s]', path, ...
            numel(val), fnc); %#ok<AGROW>
        for i = 1:numel(val)
            el = val(i);
            for j = 1:numel(fn)
                f = fn{j};
                fv = el.(f);
                local_ser(fv, sprintf('%s(%d).%s', path, i, f), lines);
            end
        end
    elseif iscell(val)
        lines{end+1} = sprintf('%s cell n=%d', path, numel(val)); %#ok<AGROW>
        for i = 1:numel(val)
            el = val{i};
            local_ser(el, sprintf('%s{%d}', path, i), lines);
        end
    elseif ischar(val)
        lines{end+1} = sprintf('%s char[%d]=%s', path, numel(val), val); %#ok<AGROW>
    elseif isstring(val)
        lines{end+1} = sprintf('%s string n=%d', path, numel(val)); %#ok<AGROW>
        for i = 1:numel(val)
            el = val(i);
            if ismissing(el)
                lines{end+1} = sprintf('%s(%d) missing', path, i); %#ok<AGROW>
            else
                sv = char(el);
                lines{end+1} = sprintf('%s(%d) char[%d]=%s', path, i, ...
                    numel(sv), sv); %#ok<AGROW>
            end
        end
    elseif islogical(val)
        lv = double(val);
        lines{end+1} = sprintf('%s logical size=[%d %d] numel=%d', path, ...
            size(val, 1), size(val, 2), numel(val)); %#ok<AGROW>
        lines{end+1} = sprintf('%.17g ', lv); %#ok<AGROW>
    elseif isnumeric(val)
        if ~isreal(val)
            error('GATE:run_all SERIALIZER: complex numeric at %s', path);
        end
        lines{end+1} = sprintf('%s %s size=[%d %d] numel=%d', path, ...
            class(val), size(val, 1), size(val, 2), numel(val)); %#ok<AGROW>
        lines{end+1} = sprintf('%.17g ', val); %#ok<AGROW>
    else
        error('GATE:run_all SERIALIZER: unsupported class %s at %s', ...
            class(val), path);
    end
end

function h = local_sha256_text(s)
% SHA-256 lowercase hex of the UTF-8 bytes of s (java digest pipeline,
% same pattern as family_expand.m / seed_tree.m).
    bytes = unicode2native(s, 'UTF-8');
    md = java.security.MessageDigest.getInstance('SHA-256');
    dg = typecast(md.digest(typecast(bytes(:), 'int8')), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function txt = local_err_text(err)
% One-line ASCII error record: identifier + message.
    id = char(string(err.identifier));
    msg = char(err.message);
    msg = strrep(msg, newline, ' ');
    txt = strtrim([id, ': ', msg]);
end

function s = local_num_str(v)
% 'n/a' for NaN (unparsed case count), integer text otherwise.
    if isnan(v)
        s = 'n/a';
    else
        s = sprintf('%d', v);
    end
end

function s = local_bool_str(v)
    if v
        s = 'true';
    else
        s = 'false';
    end
end

function s = local_iso_utc()
% ISO-8601 UTC timestamp (report provenance only, never hashed).
    dt = datetime('now', 'TimeZone', 'UTC');
    s = sprintf('%04d-%02d-%02dT%02d:%02d:%02dZ', year(dt), month(dt), ...
        day(dt), hour(dt), minute(dt), floor(second(dt)));
end

% test_stack_identity.m - T-016: qualified generation-stack identity harness
%   Fixture-driven (tempdir only; never touches real hashed files, never
%   writes results/). Independent of whether T-011/T-012 have landed.
%   F1 fixture collect: ten components PRESENT, composite stable, OutPath
%      pretty-printed JSON round-trip
%   F2 tamper detection: one-byte registry mutation -> composite changes;
%      verify of the stale manifest -> GATE:MISSION_V3:STACK:MISMATCH
%   F3 missing component: deleted registry -> collect (Strict default)
%      -> GATE:MISSION_V3:STACK:MISSING
%   F4 generator hash domain (FD2 ruling, machine-enforced): mutation inside
%      the tests/ subdirectory does NOT change the generator composite or the
%      stack composite; mutation of a top-level .m DOES; included_files lists
%      exactly the top-level package functions
%   F5 forecast_issue.m absent + Strict=false -> item PENDING
%      (version='pending', sha256=''), composite still produced; re-adding
%      the file flips the item to PRESENT and changes the composite
%   F6 real-ROOT smoke (read-only): ten components, 64-hex composite,
%      generator_hash_domain populated, d_producer_schema sha256 = embedded
%      self_hash starting with 7993e141; verify on the real ROOT passes
%   F7 determinism: two collects on the same fixture are identical except
%      generated_at
% Run (from the Model E workspace root):
%   matlab -batch "run('scripts/+mission_v3/tests/test_stack_identity.m')"
% Discipline: ASCII only; no absolute paths in source; no chained indexing.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || exist(fullfile(ROOT, 'config'), 'dir') ~= 7, ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;

%% F1: fixture collect - ten components + OutPath round-trip
[reg_dir, gen_dir, base] = local_stage_fixture(true);
ok1 = false;
det1 = 'unknown';
try
    out1 = fullfile(tempdir, 'mission_v3_stack_identity_test_out.json');
    man1 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir, 'OutPath', out1);
    statuses = {man1.components.status};
    ghd1 = man1.generator_hash_domain;
    ok1 = isstruct(man1) ...
        && strcmp(man1.schema, 'mission_v3_stack_identity_v1') ...
        && numel(man1.components) == 10 ...
        && all(strcmp(statuses, 'PRESENT')) ...
        && isempty(regexp(man1.composite_hash, '^[0-9a-f]{64}$', 'once')) == false ...
        && isequal(ghd1.included_files, ...
            {'a_core.m', 'b_core.m', 'forecast_issue.m'}) ...
        && strcmp(ghd1.excluded_dirs(1), 'tests') ...
        && strcmp(ghd1.excluded_dirs(2), '+audits') ...
        && exist(out1, 'file') == 2;
    if ok1
        ro = jsondecode(fileread(out1));
        ok1 = numel(ro.components) == 10 ...
            && strcmp(ro.composite_hash, man1.composite_hash);
    end
    det1 = sprintf('n_components=%d composite_prefix=%s', ...
        numel(man1.components), man1.composite_hash(1:12));
    if exist(out1, 'file'), delete(out1); end
catch f1_err
    det1 = ['error: ', f1_err.message];
end
local_cleanup(base);
fprintf('[T] F1 fixture collect + OutPath round-trip: %s (%s)\n', ...
    local_verdict(ok1), det1);
n_fail = n_fail + ~ok1;

%% F2: tamper detection (one byte in a registry) -> composite change + MISMATCH
[reg_dir, gen_dir, base] = local_stage_fixture(true);
ok2 = false;
det2 = 'unknown';
try
    man1 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
    ok_mut = local_mutate_first(fullfile(reg_dir, ...
        'mission_template_registry.json'), ...
        '"1.0.0-candidate"', '"1.0.0-candidatE"');
    man2 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
    ok2 = ok_mut && ~strcmp(man1.composite_hash, man2.composite_hash);
    [ok_v, id_v] = local_expect_verify_error(man1, ROOT, gen_dir, reg_dir, ...
        'STACK:MISMATCH');
    ok2 = ok2 && ok_v;
    det2 = sprintf('composite_changed=%d verify_id=%s', ...
        ~strcmp(man1.composite_hash, man2.composite_hash), id_v);
catch f2_err
    det2 = ['error: ', f2_err.message];
end
local_cleanup(base);
fprintf('[T] F2 tamper detection (composite + MISMATCH): %s (%s)\n', ...
    local_verdict(ok2), det2);
n_fail = n_fail + ~ok2;

%% F3: missing component (deleted registry) -> collect Strict -> MISSING
[reg_dir, gen_dir, base] = local_stage_fixture(true);
ok3 = false;
det3 = 'unknown';
try
    delete(fullfile(reg_dir, 'heldout_token_registry.json'));
    [ok3, id3] = local_expect_collect_error(ROOT, gen_dir, reg_dir, true, ...
        'STACK:MISSING');
    det3 = id3;
catch f3_err
    det3 = ['error: ', f3_err.message];
end
local_cleanup(base);
fprintf('[T] F3 missing registry (Strict): %s (id=%s)\n', ...
    local_verdict(ok3), det3);
n_fail = n_fail + ~ok3;

%% F4: generator hash domain ruling (FD2) - tests/ excluded from the domain
[reg_dir, gen_dir, base] = local_stage_fixture(true);
ok4 = false;
det4 = 'unknown';
try
    man1 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
    g1 = local_comp_sha(man1, 'mission_v3_generator');
    % (a) mutate a test asset inside tests/ -> generator and stack composites
    %     must NOT change (tests are verification assets, FD2)
    ok_a = local_mutate_first(fullfile(gen_dir, 'tests', 'test_probe.m'), ...
        '%% fixture test probe', '%% fixture test probe v2');
    man2 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
    g2 = local_comp_sha(man2, 'mission_v3_generator');
    ok_a = ok_a && strcmp(g1, g2) ...
        && strcmp(man1.composite_hash, man2.composite_hash);
    % (b) mutate a top-level package function -> both composites must change
    ok_b = local_mutate_first(fullfile(gen_dir, 'a_core.m'), ...
        '%% fixture generator core A', '%% fixture generator core A v2');
    man3 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
    g3 = local_comp_sha(man3, 'mission_v3_generator');
    ok_b = ok_b && ~strcmp(g1, g3) ...
        && ~strcmp(man1.composite_hash, man3.composite_hash);
    % (c) the domain manifest lists exactly the top-level functions
    ok_c = isequal(man1.generator_hash_domain.included_files, ...
        {'a_core.m', 'b_core.m', 'forecast_issue.m'});
    ok4 = ok_a && ok_b && ok_c;
    det4 = sprintf('tests_neutral=%d top_level_sensitive=%d domain_files_exact=%d', ...
        ok_a, ok_b, ok_c);
catch f4_err
    det4 = ['error: ', f4_err.message];
end
local_cleanup(base);
fprintf('[T] F4 generator hash domain (FD2): %s (%s)\n', ...
    local_verdict(ok4), det4);
n_fail = n_fail + ~ok4;

%% F5: forecast_issue.m absent + Strict=false -> PENDING, composite produced
[reg_dir, gen_dir, base] = local_stage_fixture(false);
ok5 = false;
det5 = 'unknown';
try
    man1 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir, 'Strict', false);
    fc1 = local_comp(man1, 'forecast_generator');
    ok5 = isstruct(fc1) && strcmp(fc1.status, 'PENDING') ...
        && strcmp(fc1.version, 'pending') && ischar(fc1.sha256) ...
        && isempty(fc1.sha256) ...
        && ismember('forecast_generator', man1.pending) ...
        && isempty(regexp(man1.composite_hash, '^[0-9a-f]{64}$', 'once')) == false;
    % re-adding the file flips the item to PRESENT and changes the composite
    fid = fopen(fullfile(gen_dir, 'forecast_issue.m'), 'w');
    fwrite(fid, '%% placeholder');
    fclose(fid);
    man2 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir, 'Strict', false);
    fc2 = local_comp(man2, 'forecast_generator');
    ok5 = ok5 && strcmp(fc2.status, 'PRESENT') ...
        && ~strcmp(man1.composite_hash, man2.composite_hash);
    det5 = sprintf('pending_ok=%d reappearance_flips=%d', ...
        strcmp(fc1.status, 'PENDING'), strcmp(fc2.status, 'PRESENT'));
catch f5_err
    det5 = ['error: ', f5_err.message];
end
local_cleanup(base);
fprintf('[T] F5 forecast PENDING tolerance (Strict=false): %s (%s)\n', ...
    local_verdict(ok5), det5);
n_fail = n_fail + ~ok5;

%% F6: real-ROOT smoke (read-only) - structure + producer schema self_hash
ok6 = false;
det6 = 'unknown';
try
    man6 = mission_v3.stack_identity('collect', ROOT, 'Strict', false);
    dp = local_comp(man6, 'd_producer_schema');
    ok6 = isstruct(man6) && numel(man6.components) == 10 ...
        && isempty(regexp(man6.composite_hash, '^[0-9a-f]{64}$', 'once')) == false ...
        && ~isempty(man6.generator_hash_domain.included_files) ...
        && isstruct(dp) && ischar(dp.sha256) && numel(dp.sha256) == 64 ...
        && strcmp(dp.sha256(1:8), '7993e141');
    det6 = sprintf('schema_self_hash_prefix=%s pending=%d', ...
        dp.sha256(1:12), ismember('forecast_generator', man6.pending));
    % verify round-trip on the real ROOT (self-consistency)
    ok_v6 = false;
    try
        res6 = mission_v3.stack_identity('verify', man6, ROOT);
        ok_v6 = isstruct(res6) && res6.overall;
    catch err6
        id6 = char(string(err6.identifier));
        if contains(id6, 'STACK:MISMATCH')
            % Wave-1 race tolerance: forecast_issue.m may appear between the
            % two calls; re-collect once and re-verify.
            try
                man6b = mission_v3.stack_identity('collect', ROOT, ...
                    'Strict', false);
                res6b = mission_v3.stack_identity('verify', man6b, ROOT);
                ok_v6 = isstruct(res6b) && res6b.overall;
            catch
                ok_v6 = false;
            end
        end
    end
    ok6 = ok6 && ok_v6;
catch f6_err
    det6 = ['error: ', f6_err.message];
end
fprintf('[T] F6 real-ROOT smoke (7993e141 self_hash + verify): %s (%s)\n', ...
    local_verdict(ok6), det6);
n_fail = n_fail + ~ok6;

%% F7: determinism - two collects on the same fixture identical (minus generated_at)
[reg_dir, gen_dir, base] = local_stage_fixture(true);
ok7 = false;
det7 = 'unknown';
try
    man1 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
    man2 = mission_v3.stack_identity('collect', ROOT, ...
        'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
    c1 = man1;
    c2 = man2;
    c1.generated_at = '';
    c2.generated_at = '';
    ok7 = isequal(c1, c2);
    det7 = sprintf('bitwise_identical=%d', ok7);
catch f7_err
    det7 = ['error: ', f7_err.message];
end
local_cleanup(base);
fprintf('[T] F7 determinism (two collects): %s (%s)\n', ...
    local_verdict(ok7), det7);
n_fail = n_fail + ~ok7;

%% summary
fprintf('[T] test_stack_identity: %d/7 cases passed\n', 7 - n_fail);
if n_fail > 0
    error('GATE:test_stack_identity %d case(s) failed', n_fail);
end
fprintf('test_stack_identity: ALL PASS (7/7)\n');

%% ======================== local functions ==========================

function verdict = local_verdict(ok)
    if ok
        verdict = 'PASS';
    else
        verdict = 'FAIL';
    end
end

function [reg_dir, gen_dir, base] = local_stage_fixture(with_forecast)
% Fresh tempdir fixture: nine registries (only the five hashed ones are read,
% the rest ride along per the delegation spec) + a fake generator dir with
% top-level .m files, a tests/ subdirectory and a +audits/ subdirectory,
% optionally a placeholder forecast_issue.m.
    base = tempname();
    mkdir(base);
    reg_dir = fullfile(base, 'registries');
    gen_dir = fullfile(base, 'generator');
    mkdir(reg_dir);
    mkdir(gen_dir);
    names = {'mission_template_registry.json', 'conditional_dag_config.json', ...
        'environment_regime_params.json', 'id_support_bounds.json', ...
        'forecast_quality_profiles.json', 'seed_tree_policy.json', ...
        'scenario_stratum_registry.json', 'heldout_token_registry.json', ...
        'identifiability_contrast_registry.json'};
    src = fullfile(fileparts(fileparts(fileparts(fileparts( ...
        mfilename('fullpath'))))), 'config', 'mission_v3');
    for k = 1:numel(names)
        copyfile(fullfile(src, names{k}), fullfile(reg_dir, names{k}));
    end
    local_write(fullfile(gen_dir, 'a_core.m'), '%% fixture generator core A');
    local_write(fullfile(gen_dir, 'b_core.m'), '%% fixture generator core B');
    mkdir(fullfile(gen_dir, 'tests'));
    local_write(fullfile(gen_dir, 'tests', 'test_probe.m'), ...
        '%% fixture test probe (outside the FD2 hash domain)');
    mkdir(fullfile(gen_dir, '+audits'));
    local_write(fullfile(gen_dir, '+audits', 'audit_probe.m'), ...
        '%% fixture audit probe (outside the FD2 hash domain)');
    if with_forecast
        local_write(fullfile(gen_dir, 'forecast_issue.m'), '%% placeholder');
    end
end

function local_write(path, txt)
    fid = fopen(path, 'w');
    if fid < 0
        error('GATE:test_stack_identity cannot write fixture file: %s', path);
    end
    fwrite(fid, txt);
    fclose(fid);
end

function ok = local_mutate_first(path, old_txt, new_txt)
% Replace only the FIRST occurrence (single mutation, same-length payload).
    txt = fileread(path);
    hits = strfind(txt, old_txt);
    if isempty(hits)
        fprintf('[T] mutation anchor missing in %s\n', path);
        ok = false;
        return;
    end
    p = hits(1);
    txt = [txt(1:p - 1), new_txt, txt(p + numel(old_txt):end)];
    fid = fopen(path, 'w', 'n', 'UTF-8');
    fwrite(fid, txt);
    fclose(fid);
    ok = true;
end

function c = local_comp(man, name)
    c = [];
    for i = 1:numel(man.components)
        if strcmp(man.components(i).component, name)
            c = man.components(i);
            return;
        end
    end
end

function sha = local_comp_sha(man, name)
    c = local_comp(man, name);
    if isstruct(c)
        sha = c.sha256;
    else
        sha = 'COMPONENT_NOT_FOUND';
    end
end

function [ok, id] = local_expect_collect_error(ROOT, gen_dir, reg_dir, strict, want_tok)
    try
        mission_v3.stack_identity('collect', ROOT, ...
            'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir, ...
            'Strict', strict);
        ok = false;
        id = 'NO_ERROR_RAISED';
    catch err
        id = char(string(err.identifier));
        ok = startsWith(id, 'GATE:') && contains(id, want_tok);
    end
end

function [ok, id] = local_expect_verify_error(man, ROOT, gen_dir, reg_dir, want_tok)
    try
        mission_v3.stack_identity('verify', man, ROOT, ...
            'GeneratorDir', gen_dir, 'RegistriesDir', reg_dir);
        ok = false;
        id = 'NO_ERROR_RAISED';
    catch err
        id = char(string(err.identifier));
        ok = startsWith(id, 'GATE:') && contains(id, want_tok);
    end
end

function local_cleanup(base)
    if exist(base, 'dir')
        rmdir(base, 's');
    end
end

% test_validate_registries.m - T-001/T-002: mission_v3 registry validator harness
%   Positive: real config/mission_v3 passes (record write disabled).
%   Negative (8 classes, each on a tempdir copy via ConfigDir override, never
%   touching the real files, never writing results/):
%     N1 illegal enum (regime -> TIDAL)            -> FAIL
%     N2 missing required field (operator_prior)   -> FAIL
%     N3 tampered value with stale self_hash       -> FAIL
%     N4 dangling params_ref (dag -> env)          -> FAIL
%     N5 self_hash.value flipped one hex digit     -> FAIL
%     N6 token predicate field outside whitelist   -> FAIL (PREDICATE_FIELD)
%     N7 strata cell count violation (10th cell)   -> FAIL (STRATA)
%     N8 contrast minimum_group_coverage != 10     -> FAIL (CONTRAST)
%   N6/N7/N8 additionally assert the targeted issue class, so a stale-hash
%   failure cannot mask a missing targeted check.
%   All failures must raise error identifiers with the GATE: prefix.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_validate_registries.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;

%% P1: positive - real config dir passes
ok_p = false;
try
    res = mission_v3.validate_registries(ROOT, 'RecordPath', 'none');
    ok_p = isstruct(res) && (res.overall == true) && (res.n_issues == 0);
    if ok_p
        fprintf('[T] P1 positive real config: PASS (9 files, self_hash cross-verified)\n');
    end
catch p_err
    fprintf('[T] P1 positive real config: FAIL (%s)\n', p_err.message);
end
n_fail = n_fail + ~ok_p;

%% N1: illegal regime enum
tmp1 = local_stage_tmp();
ok1 = local_mutate(tmp1, 'environment_regime_params.json', '"NORMAL"', '"TIDAL"');
if ok1
    [ok1, id1] = local_expect_gate_fail(ROOT, tmp1);
    fprintf('[T] N1 illegal enum: %s (id=%s)\n', local_verdict(ok1), id1);
else
    id1 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok1;
local_cleanup(tmp1);

%% N2: missing required field (operator_prior deleted from first template)
tmp2 = local_stage_tmp();
ok2 = local_mutate(tmp2, 'mission_template_registry.json', ...
    '"operator_prior": {"CALM": 0.3, "STANDARD": 0.6, "URGENT": 0.1},', '');
if ok2
    [ok2, id2] = local_expect_gate_fail(ROOT, tmp2);
    fprintf('[T] N2 missing field: %s (id=%s)\n', local_verdict(ok2), id2);
else
    id2 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok2;
local_cleanup(tmp2);

%% N3: tampered numeric value, stale self_hash kept
tmp3 = local_stage_tmp();
ok3 = local_mutate(tmp3, 'environment_regime_params.json', ...
    '"sigma_mps": 0.1', '"sigma_mps": 0.10001');
if ok3
    [ok3, id3] = local_expect_gate_fail(ROOT, tmp3);
    fprintf('[T] N3 tamper+stale-hash: %s (id=%s)\n', local_verdict(ok3), id3);
else
    id3 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok3;
local_cleanup(tmp3);

%% N4: dangling cross-file reference (params_ref points nowhere)
tmp4 = local_stage_tmp();
ok4 = local_mutate(tmp4, 'conditional_dag_config.json', ...
    '"environment_regime_params.regimes[].water_level_offset"', ...
    '"environment_regime_params.regimes[].water_level_offset_x"');
if ok4
    [ok4, id4] = local_expect_gate_fail(ROOT, tmp4);
    fprintf('[T] N4 dangling ref: %s (id=%s)\n', local_verdict(ok4), id4);
else
    id4 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok4;
local_cleanup(tmp4);

%% N5: self_hash value tampered (one hex digit flipped)
tmp5 = local_stage_tmp();
ok5 = local_flip_hash(tmp5, 'seed_tree_policy.json');
if ok5
    [ok5, id5] = local_expect_gate_fail(ROOT, tmp5);
    fprintf('[T] N5 self_hash tamper: %s (id=%s)\n', local_verdict(ok5), id5);
else
    id5 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok5;
local_cleanup(tmp5);

%% N6: token predicate field outside the whitelist (realized-signal style)
tmp6 = local_stage_tmp();
ok6 = local_mutate(tmp6, 'heldout_token_registry.json', ...
    '"field": "segment.u_adverse_abs", "operator": "in_band"', ...
    '"field": "segment.p_load_realized_kw", "operator": "in_band"');
if ok6
    [ok6, id6] = local_expect_gate_fail_class(ROOT, tmp6, 'PREDICATE_FIELD');
    fprintf('[T] N6 off-whitelist predicate field: %s (id=%s)\n', local_verdict(ok6), id6);
else
    id6 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok6;
local_cleanup(tmp6);

%% N7: strata cell count violation (synthetic 10th cell inserted)
tmp7 = local_stage_tmp();
anchor7 = sprintf('[\n    {"scenario_stratum_id": "STRAT_ROUTINE_PATROL__NORMAL"');
extra7 = sprintf(['[\n    {"scenario_stratum_id": "STRAT_EXTRA__EXTRA", ' ...
    '"mission_template_id": "EXTRA", "environment_regime": "EXTRA", ' ...
    '"status": "ACTIVE", "reason": "count violation probe", ' ...
    '"version": "1.0.0-candidate"},\n    ' ...
    '{"scenario_stratum_id": "STRAT_ROUTINE_PATROL__NORMAL"']);
ok7 = local_mutate(tmp7, 'scenario_stratum_registry.json', anchor7, extra7);
if ok7
    [ok7, id7] = local_expect_gate_fail_class(ROOT, tmp7, 'STRATA');
    fprintf('[T] N7 strata count violation: %s (id=%s)\n', local_verdict(ok7), id7);
else
    id7 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok7;
local_cleanup(tmp7);

%% N8: contrast minimum_group_coverage lowered below the frozen candidate
tmp8 = local_stage_tmp();
ok8 = local_mutate(tmp8, 'identifiability_contrast_registry.json', ...
    '"minimum_group_coverage": 10', '"minimum_group_coverage": 5');
if ok8
    [ok8, id8] = local_expect_gate_fail_class(ROOT, tmp8, 'CONTRAST');
    fprintf('[T] N8 contrast coverage violation: %s (id=%s)\n', local_verdict(ok8), id8);
else
    id8 = 'MUTATION_FAILED';
end
n_fail = n_fail + ~ok8;
local_cleanup(tmp8);

%% summary
fprintf('[T] test_validate_registries: %d/9 cases passed\n', 9 - n_fail);
if n_fail > 0
    error('GATE:test_validate_registries %d case(s) failed', n_fail);
end
fprintf('test_validate_registries: ALL PASS (9/9)\n');

%% ======================== local functions ==========================

function verdict = local_verdict(ok)
    if ok
        verdict = 'PASS';
    else
        verdict = 'FAIL';
    end
end

function tmp = local_stage_tmp()
% Copy the nine registry files into a fresh tempdir workspace.
    tmp = tempname();
    mkdir(tmp);
    names = {'mission_template_registry.json', 'conditional_dag_config.json', ...
        'environment_regime_params.json', 'id_support_bounds.json', ...
        'forecast_quality_profiles.json', 'seed_tree_policy.json', ...
        'scenario_stratum_registry.json', 'heldout_token_registry.json', ...
        'identifiability_contrast_registry.json'};
    src = fullfile(fileparts(fileparts(fileparts(fileparts(mfilename('fullpath'))))), ...
        'config', 'mission_v3');
    for k = 1:numel(names)
        copyfile(fullfile(src, names{k}), fullfile(tmp, names{k}));
    end
end

function ok = local_mutate(tmp, fname, old_txt, new_txt)
% String-replace mutation; fails the case when the anchor is missing.
    path = fullfile(tmp, fname);
    txt = fileread(path);
    if isempty(strfind(txt, old_txt))
        fprintf('[T] mutation anchor missing in %s: %s\n', fname, old_txt);
        ok = false;
        return;
    end
    txt = strrep(txt, old_txt, new_txt);
    fid = fopen(path, 'w', 'n', 'UTF-8');
    fwrite(fid, txt);
    fclose(fid);
    ok = true;
end

function ok = local_flip_hash(tmp, fname)
% Flip the first hex digit of self_hash.value.
    path = fullfile(tmp, fname);
    txt = fileread(path);
    tok = regexp(txt, '"value": "[0-9a-f]{64}"', 'match', 'once');
    if isempty(tok)
        fprintf('[T] self_hash value token not found in %s\n', fname);
        ok = false;
        return;
    end
    hits = strfind(tok, '"');
    hex_pos = hits(numel(hits));  % position of closing quote
    c = tok(hex_pos - 1);
    if c == '0'
        c2 = '1';
    else
        c2 = '0';
    end
    tok2 = tok;
    tok2(hex_pos - 1) = c2;
    txt = strrep(txt, tok, tok2);
    fid = fopen(path, 'w', 'n', 'UTF-8');
    fwrite(fid, txt);
    fclose(fid);
    ok = true;
end

function [ok, id] = local_expect_gate_fail(ROOT, tmp)
% Run the validator on the tempdir copy; expect a GATE-prefixed error.
    try
        mission_v3.validate_registries(ROOT, 'ConfigDir', tmp, 'RecordPath', 'none');
        ok = false;
        id = 'NO_ERROR_RAISED';
    catch err
        ident = string(err.identifier);
        if startsWith(ident, 'GATE:')
            ok = true;
            id = ident;
        else
            ok = false;
            id = ident;
        end
    end
end

function [ok, id] = local_expect_gate_fail_class(ROOT, tmp, want_class)
% Like local_expect_gate_fail, but additionally requires the raised error
% identifier to carry the targeted issue class (so a stale self_hash failure
% cannot mask a missing targeted check).
    [ok, id] = local_expect_gate_fail(ROOT, tmp);
    if ok && ~contains(char(id), want_class)
        ok = false;
    end
end

function local_cleanup(tmp)
    if exist(tmp, 'dir')
        delete(fullfile(tmp, '*.json'));
        rmdir(tmp, 's');
    end
end
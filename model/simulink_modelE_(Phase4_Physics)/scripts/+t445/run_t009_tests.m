function r = run_t009_tests()
% t445.run_t009_tests — T-009 toy B2 分段 smoke + checkpoint/resume
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
cfg = phase4f_t445_config();
assert(strcmp(cfg.k_eq.status, 'CANONICAL'), 'T445:T009:KEQ');
slx = fullfile(ROOT, 'model', 'HybridShip_ModelE.slx');
sha0 = model.sha256_file(slx);

SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.60;
stopT = 40;
t = (0:stopT-1).';
stream = struct('t', t, 'P_load_kW', 400*ones(size(t)), 'voyage_id', 'T445_TOY_SHORT');
tbl = t445.policy_table_cached(cfg);
ckdir = fullfile(ROOT, 'artifacts', 't445', 'toy_ckpt');
if ~exist(ckdir, 'dir'), mkdir(ckdir); end

[dsA, metaA] = t445.run_arm_b2(SCN, stream, 'StopTime', stopT, 'cfg', cfg, ...
    'tbl', tbl, 'Seed', 'phase4f-t445-toy-smoke-v1');
[ds1, meta1] = t445.run_arm_b2(SCN, stream, 'StopTime', 20, 'cfg', cfg, ...
    'tbl', tbl, 'CheckpointDir', ckdir, 'Seed', 'phase4f-t445-toy-smoke-v1');
ckpt = fullfile(ckdir, 'ckpt_T445_TOY_SHORT_t20.mat');
assert(exist(ckpt, 'file') == 2, 'T445:T009:CKPT');
rec = t445.simstate_mgr('load', ckpt);
id0 = rec.identity;
assert(strcmp(id0.voyage_id, 'T445_TOY_SHORT') && strcmp(id0.arm, 'B2'), 'T445:T009:ID');
assert(strcmp(id0.config_hash, cfg.config_sha256), 'T445:T009:CFG');
[dsB, metaB] = t445.run_arm_b2(SCN, stream, 'StopTime', stopT, 'cfg', cfg, ...
    'tbl', tbl, 'StartT0', 20, 'InitialOp', rec.op, 'InitialSOC', rec.extra.SOC, ...
    'Seed', 'phase4f-t445-toy-smoke-v1');

sha1 = model.sha256_file(slx);
slx_ok = strcmp(sha0, sha1);
if bdIsLoaded('HybridShip_ModelE')
    close_system('HybridShip_ModelE', 0);
end

% overlap t>=20
ta = dsA.t; tb = dsB.t;
mA = ta >= 20 - 1e-9;
mB = tb >= 20 - 1e-9;
sA = dsA.SOC(mA); sB = dsB.SOC(mB);
n = min(numel(sA), numel(sB));
rel = max(abs(sA(1:n) - sB(1:n)) ./ max(abs(sA(1:n)), 1e-12));
oob_ok = metaA.oob_query_count == 0 && metaB.oob_query_count == 0 && meta1.oob_query_count == 0;
id_ok = numel(fieldnames(id0)) >= 7 ...
    && isfield(id0, 'voyage_id') && isfield(id0, 'arm') ...
    && isfield(id0, 'config_hash') && isfield(id0, 'seed') ...
    && isfield(id0, 'frozen_stream_sha256') && isfield(id0, 'model_checksum') ...
    && isfield(id0, 'segment_t0');
pass_resume = rel <= 1e-10;
overall = slx_ok && oob_ok && id_ok && pass_resume && metaA.n_segments == 4;

r = struct();
r.schema = 't445_t009_evidence_v1';
r.task = 'T-009';
r.purpose = 'SYNTHETIC_SMOKE';
r.config_sha256 = cfg.config_sha256;
r.overall = overall;
r.slx_unchanged = slx_ok;
r.slx_sha256 = sha1;
r.oob_query_count = metaA.oob_query_count;
r.resume_max_rel_SOC = rel;
r.resume_pass = pass_resume;
r.identity_seven_fields = id_ok;
r.n_segments = metaA.n_segments;
r.n_retry = metaA.n_retry;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
outDir = fullfile(ROOT, 'results', 't445_oracle_gate');
fid = fopen(fullfile(outDir, 't009_unit_test_result.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fid = fopen(fullfile(outDir, 'access_ledger.jsonl'), 'a');
fprintf(fid, '%s\n', jsonencode(struct('purpose','SYNTHETIC_SMOKE','task','T-009', ...
    'artifact','t009_unit_test_result.json','utc',r.utc)));
fclose(fid);
fprintf('[T-009] overall=%d resume_rel=%g slx_ok=%d oob=%d\n', ...
    overall, rel, slx_ok, metaA.oob_query_count);
if ~overall
    error('T445:T009:FAIL', 'T-009 toy/resume failed rel=%g', rel);
end
end

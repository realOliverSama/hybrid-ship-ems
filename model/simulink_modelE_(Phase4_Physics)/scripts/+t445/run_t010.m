function r = run_t010()
% t445.run_t010 — T-010 资格化编排：Q1~Q6 然后 SimState 等价（Amendment E）
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
cfg = phase4f_t445_config();
qb_ok = true;
qb = struct('pass', false, 'config_sha256', cfg.config_sha256);
try
    qb = t445.qual_b2_optimizer();
catch ME
    qb_ok = false;
    qb.pass = false;
    qb.message = ME.message;
    fprintf('[T-010] optimizer Q threw: %s\n', ME.message);
end
qs = struct('pass', false);
qs_ok = true;
try
    qs = t445.qual_simstate_equiv();
catch ME
    qs_ok = false;
    qs.pass = false;
    qs.message = ME.message;
    fprintf('[T-010] SimState threw: %s\n', ME.message);
end
r = struct();
r.schema = 't445_t010_complete_v1';
r.task = 'T-010';
r.amendment = 'E_generator_realization';
r.purpose = 'QUALIFICATION';
r.config_sha256 = cfg.config_sha256;
r.b2_optimizer_pass = qb.pass;
r.simstate_pass = qs.pass;
r.overall = qb.pass && qs.pass;
r.preserved_fail = { ...
    'qualification/t010_b2_optimizer_qual.json'; ...
    'qualification/t010_b2_optimizer_qual_amd_d.json'; ...
    'qualification/t010_e0_fuel_audit.json'};
r.amd_e_artifact = 'qualification/t010_b2_optimizer_qual_amd_e.json';
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
outDir = fullfile(ROOT, 'results', 't445_oracle_gate');
qdir = fullfile(outDir, 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
if r.overall
    fid = fopen(fullfile(outDir, 't010_complete_amd_e.json'), 'w');
    fwrite(fid, jsonencode(r, 'PrettyPrint', true));
    fclose(fid);
else
    r.verdict = 'B2_OPTIMIZER_QUALIFICATION_FAILED';
    r.recommend_t445_closure = 'GATE_VALIDITY_FAIL';
    r.not_no_go_trajectory_value = true;
    r.formal_gate_produced = false;
    fid = fopen(fullfile(qdir, 't010_amendment_e_stop.json'), 'w');
    fwrite(fid, jsonencode(r, 'PrettyPrint', true));
    fclose(fid);
end
fid = fopen(fullfile(outDir, 'access_ledger.jsonl'), 'a');
fprintf(fid, '%s\n', jsonencode(struct('purpose','QUALIFICATION','task','T-010-E', ...
    'artifact', r.amd_e_artifact, 'utc', r.utc)));
fclose(fid);
fprintf('[T-010] overall=%d optimizer=%d simstate=%d\n', r.overall, qb.pass, qs.pass);
if ~r.overall
    error('T445:T010:FAIL', 'B2_OPTIMIZER_QUALIFICATION_FAILED');
end
end

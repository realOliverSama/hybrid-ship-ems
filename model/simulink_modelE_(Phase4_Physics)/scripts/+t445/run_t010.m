function r = run_t010()
% t445.run_t010 — T-010 资格化编排：Q1~Q6 然后 SimState 等价（Amendment D）
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
qb = t445.qual_b2_optimizer();
if ~qb.pass
    error('T445:T010:B2QUAL', 'optimizer Q FAIL — skip SimState');
end
qs = t445.qual_simstate_equiv();
r = struct();
r.schema = 't445_t010_complete_v1';
r.task = 'T-010';
r.amendment = 'D_pms_fuel_surrogate';
r.purpose = 'QUALIFICATION';
r.config_sha256 = qb.config_sha256;
r.b2_optimizer_pass = qb.pass;
r.simstate_pass = qs.pass;
r.overall = qb.pass && qs.pass;
r.preserved_fail = 'qualification/t010_b2_optimizer_qual.json';
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
outDir = fullfile(ROOT, 'results', 't445_oracle_gate');
fid = fopen(fullfile(outDir, 't010_complete.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fid = fopen(fullfile(outDir, 'access_ledger.jsonl'), 'a');
fprintf(fid, '%s\n', jsonencode(struct('purpose','QUALIFICATION','task','T-010', ...
    'artifact','t010_complete.json','utc',r.utc)));
fclose(fid);
fprintf('[T-010] overall=%d optimizer=%d simstate=%d\n', r.overall, qb.pass, qs.pass);
if ~r.overall
    error('T445:T010:FAIL', 'T-010 qualification FAIL');
end
end

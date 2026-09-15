function r = qual_t005()
% ppe.qual_t005 — T-005 NONFORMAL qualification on a toy fixture (S01, 40 s).
%
%   (a) SOC grid 0.10:0.005:0.95 -> n=171 (AC-804)
%   (b) determinism: two identical segmented runs -> identical dlambda + SOC
%   (c) continuous-vs-segmented: replay segmented dlambda as ZOH in one
%       continuous law=6 run -> trajectory rel <= 1e-10 (SimState inertness)
%   (d) checkpoint identity mismatch refusal (D8)
%   (e) resume == uninterrupted (segmented 0-40 vs 0-20 + resume 20-40)
%
% Toy ForecastWindow: constant 400 kW (synthetic provider; no store, no
% keras, no corpus). Writes results/ppe_practical/qualification/t005_runner.json.
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
pc = ppe.ppe_config();
cfg = pc.t445;
SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.60;
T_EVAL = 40;
slx = fullfile(ROOT, 'model', 'HybridShip_ModelE.slx');
sha0 = model.sha256_file(slx);

% ---- identity context (toy identities; formal fields bound at T-009) ----
ctxid = struct();
ctxid.study_config_sha256 = pc.study_config_sha256;
ctxid.code_commit_sha = local_git_head(ROOT);
ctxid.slx_sha256 = sha0;
ctxid.cstar_impl_sha256 = ppe.cstar_impl_sha();
hfj = jsondecode(fileread(fullfile(ROOT, 'config', 'phase4f_ppe_hard_feasibility.json')));
ctxid.hf_config_sha256 = hfj.canonical_hard_feasibility_object_sha256;
ctxid.corpus_identity_sha256 = ...
    '245fa307cf36143259672f93cf801587eb701d721028a735b3cd91c97c42f115';
ctxid.teval_identity_sha256 = 'TOY_FIXTURE_S01_40S';
ctxid.voyage_id = 'PPE_TOY_S01_40S';
ctxid.predictor_identity = 'N/A';

winfun = @(t0, ds, p0) deal(400 * ones(30, 1), ...
    struct('source_id', 'C0_PERSISTENCE', 'synthetic', true), false);

ckdir = fullfile(ROOT, 'artifacts', 'ppe_toy_ckpt');
if ~exist(ckdir, 'dir'), mkdir(ckdir); end
d = dir(fullfile(ckdir, 'ckpt_*.mat'));
for i = 1:numel(d), delete(fullfile(ckdir, d(i).name)); end

% ---- (b) determinism: two full segmented runs ----
[dsA, mA] = ppe.run_cstar(SCN, winfun, 'C0', T_EVAL, ctxid, 'Seed', 'phase4f-ppe-toy-v1');
[dsB, mB] = ppe.run_cstar(SCN, winfun, 'C0', T_EVAL, ctxid, 'Seed', 'phase4f-ppe-toy-v1');
det_ok = isequal(mA.dlambda_1, mB.dlambda_1) && isequal(dsA.SOC, dsB.SOC) ...
    && isequal(dsA.fuel_total_kg, dsB.fuel_total_kg);

% ---- (c) continuous replay of the segmented dlambda schedule ----
t0g = (0:10:(T_EVAL - 10)).';
ehat_tbl = [t0g, zeros(size(t0g)), mA.dlambda_1];
outC = t445.run_arm_b0b1('B1', SCN, 'StopTime', T_EVAL, ...
    'EHatData', ehat_tbl, 'K_S', 1/30);
dsC = model.extract_logsout(outC);
sigs = {'SOC', 'P_gen_actual_kW', 'P_bat_actual_kW', 'v_ship_mps'};
rel = struct();
simstate_ok = true;
for i = 1:numel(sigs)
    nm = sigs{i};
    n = min(numel(dsC.(nm)), numel(dsA.(nm)));
    a = dsC.(nm)(1:n); b = dsA.(nm)(1:n);
    rr = max(abs(a - b) ./ max(abs(a), 1e-12));
    rel.(nm) = rr;
    if ~(rr <= 1e-10), simstate_ok = false; end
end

% ---- (d) checkpoint mismatch refusal ----
[dsP, mP] = ppe.run_cstar(SCN, winfun, 'C0', T_EVAL, ctxid, ...
    'EndT0', 20, 'CheckpointDir', ckdir, 'Seed', 'phase4f-ppe-toy-v1');
ckpt = fullfile(ckdir, 'ckpt_C0_PPE_TOY_S01_40S_seg0001.mat');
assert(exist(ckpt, 'file') == 2, 'PPE:T005:CKPT');
rec = ppe.checkpoint_mgr('load', ckpt);   % no expectation -> accept
tampered = rec.identity;
tampered.arm = 'C1';
tampered.identity_sha256 = '';
refuse_ok = false;
try
    ppe.checkpoint_mgr('load', ckpt, tampered); %#ok<NASGU>
catch ME
    refuse_ok = contains(ME.identifier, 'PPE:CKPT:MISMATCH');
end
refuse_ok2 = false;
bad = rec.identity; bad.teval_identity_sha256 = 'WRONG_TEVAL';
bad.identity_sha256 = '';
try
    ppe.checkpoint_mgr('load', ckpt, bad);
catch ME
    refuse_ok2 = contains(ME.identifier, 'PPE:CKPT:MISMATCH');
end

% ---- (e) resume == uninterrupted ----
[dsR, mR] = ppe.run_cstar(SCN, winfun, 'C0', T_EVAL, ctxid, ...
    'StartT0', 20, 'InitialOp', rec.op, 'InitialSOC', rec.extra.SOC, ...
    'InitialGen', rec.extra.gen, 'InitialN', rec.extra.N, ...
    'InitialDLam', rec.extra.last_dlam, 'Seed', 'phase4f-ppe-toy-v1');
% part1 on t<=20, resume on t>=20; both must match the uninterrupted run
mP20 = dsP.t <= 20 + 1e-9;
mA20 = dsA.t <= 20 + 1e-9;
mR20 = dsR.t >= 20 - 1e-9;
mA20b = dsA.t >= 20 - 1e-9;
s1 = dsP.SOC(mP20); s1r = dsA.SOC(mA20);
s2 = dsR.SOC(mR20); s2r = dsA.SOC(mA20b);
n1 = min(numel(s1), numel(s1r)); n2 = min(numel(s2), numel(s2r));
resume_rel = max([max(abs(s1(1:n1) - s1r(1:n1)) ./ max(abs(s1r(1:n1)), 1e-12)), ...
                  max(abs(s2(1:n2) - s2r(1:n2)) ./ max(abs(s2r(1:n2)), 1e-12))]);
resume_ok = resume_rel <= 1e-10 && isequal(mA.dlambda_1, [mP.dlambda_1; mR.dlambda_1]);

if bdIsLoaded('HybridShip_ModelE')
    close_system('HybridShip_ModelE', 0);
end
sha1 = model.sha256_file(slx);

r = struct();
r.schema = 'ppe_t005_runner_qual_v1';
r.task = 'T-005';
r.purpose = 'QUALIFICATION';
r.fixture_id = 'TOY-S01-40S-CONST400';
r.soc_grid_n171 = (numel(0.10:0.005:0.95) == 171);
r.determinism_bitwise = det_ok;
r.simstate_continuous_vs_segmented_max_rel = rel;
r.simstate_ok = simstate_ok;
r.checkpoint_mismatch_refuse_arm = refuse_ok;
r.checkpoint_mismatch_refuse_teval = refuse_ok2;
r.resume_max_rel = resume_rel;
r.resume_ok = resume_ok;
r.fuel_total_kg = mA.fec.fuel_raw_kg;
r.hard_feasibility_pass = mA.hard_feasibility.pass;
r.hf_detail = mA.hard_feasibility.detail;
r.oob_query_count = mA.oob_query_count;
r.slx_unchanged = strcmp(sha0, sha1);
r.pass = r.soc_grid_n171 && det_ok && simstate_ok && refuse_ok && ...
    refuse_ok2 && resume_ok && r.slx_unchanged;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 't005_runner.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[T-005] pass=%d det=%d simstate=%d refuse=%d/%d resume=%d\n', ...
    r.pass, det_ok, simstate_ok, refuse_ok, refuse_ok2, resume_ok);
if ~r.pass
    error('PPE:T005:FAIL', 'T-005 runner qualification FAIL');
end
end

function sha = local_git_head(ROOT)
[s, out] = system(sprintf('git -C "%s" rev-parse HEAD', fileparts(ROOT)));
if s ~= 0
    sha = 'UNKNOWN';
else
    sha = strtrim(out);
end
end

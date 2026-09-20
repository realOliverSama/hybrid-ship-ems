function r = qual_runner_amendment()
% ppe.qual_runner_amendment — NONFORMAL synthetic smoke for the post-freeze
% PRE-EXECUTION runner amendment (SYNTHETIC_SMOKE; toy S01 fixture, no
% corpus, no store, no keras, no formal outputs).
%
% Qualifies the amended ppe.run_cstar bytes:
%   (a) SkipInject=true (caller pre-injected) bitwise == default path;
%   (b) resume via checkpoint payload.extra.* (the exact run_formal_voyage
%       ResumeFrom construction) bitwise == uninterrupted run;
%   (c) D3 warm-up boundary snapshot at t_stop=300: captured, finite,
%       internally consistent (SOC == ds.SOC(end)), deterministic repeat.
%
% Writes results/ppe_practical/qualification/runner_amendment_smoke.json.
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
pc = ppe.ppe_config();
SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.60;

ctxid = struct();
ctxid.study_config_sha256 = pc.study_config_sha256;
ctxid.code_commit_sha = 'AMENDMENT_SMOKE';
ctxid.slx_sha256 = model.sha256_file(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));
ctxid.cstar_impl_sha256 = ppe.cstar_impl_sha();
hfj = jsondecode(fileread(fullfile(ROOT, 'config', 'phase4f_ppe_hard_feasibility.json')));
ctxid.hf_config_sha256 = hfj.canonical_hard_feasibility_object_sha256;
ctxid.corpus_identity_sha256 = ...
    '245fa307cf36143259672f93cf801587eb701d721028a735b3cd91c97c42f115';
ctxid.teval_identity_sha256 = 'TOY_FIXTURE_S01';
ctxid.voyage_id = 'PPE_TOY_AMEND_SMOKE';
ctxid.predictor_identity = 'N/A';

winfun = @(t0, ds, p0) deal(400 * ones(30, 1), ...
    struct('source_id', 'C0_PERSISTENCE', 'synthetic', true), false);
ckdir = fullfile(ROOT, 'artifacts', 'ppe_toy_ckpt');
if ~exist(ckdir, 'dir'), mkdir(ckdir); end
local_clean(ckdir);

% ---- (a) SkipInject bitwise equivalence (T_EVAL=40) ----
[dsA, mA] = ppe.run_cstar(SCN, winfun, 'C0', 40, ctxid);
local_clean(ckdir);
MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
end
t445.inject_scenario(MDL, SCN, 6, 40);   % caller pre-injects the SAME way
[dsB, mB] = ppe.run_cstar(SCN, winfun, 'C0', 40, ctxid, 'SkipInject', true);
a_ok = isequal(mA.dlambda_1, mB.dlambda_1) && isequal(dsA.SOC, dsB.SOC) ...
    && isequal(dsA.fuel_total_kg, dsB.fuel_total_kg);

% ---- (b) resume equivalence via payload.extra.* (SkipInject path) ----
local_clean(ckdir);
[dsF, mF] = ppe.run_cstar(SCN, winfun, 'C0', 40, ctxid, ...
    'SkipInject', true, 'CheckpointDir', ckdir);
local_clean(ckdir);
[dsP, mP] = ppe.run_cstar(SCN, winfun, 'C0', 40, ctxid, ...
    'SkipInject', true, 'CheckpointDir', ckdir, ...
    'EndT0', 20);
rf = local_latest(ckdir, 'C0');
b_ok = false;
if abs(rf.extra.t_stop - 20) < 1e-9
    local_clean(ckdir);
    [dsR, mR] = ppe.run_cstar(SCN, winfun, 'C0', 40, ctxid, ...
        'SkipInject', true, 'CheckpointDir', ckdir, ...
        'StartT0', rf.extra.t_stop, 'InitialOp', rf.op, ...
        'InitialSOC', rf.extra.SOC, 'InitialGen', rf.extra.gen, ...
        'InitialN', rf.extra.N, 'InitialDLam', rf.extra.last_dlam, ...
        'P0Load', rf.extra.p0);
    % window-aligned bitwise comparison: both windows include the t=20
    % boundary sample (naive concatenation would duplicate it)
    wf = dsF.t >= 20 - 1e-9;
    wp = dsF.t <= 20 + 1e-9;
    b_ok = isequal([mP.dlambda_1; mR.dlambda_1], mF.dlambda_1) ...
        && isequal(dsP.t, dsF.t(wp)) && isequal(dsR.t, dsF.t(wf)) ...
        && isequal(dsP.SOC, dsF.SOC(wp)) ...
        && isequal(dsR.SOC, dsF.SOC(wf)) ...
        && isequal(dsP.fuel_total_kg, dsF.fuel_total_kg(wp)) ...
        && isequal(dsR.fuel_total_kg, dsF.fuel_total_kg(wf));
end
fu_ok = isequal(dsF.SOC, dsA.SOC);   % fixture sanity vs (a) baseline

% ---- (c) warm-up boundary snapshot (T_EVAL=300) ----
local_clean(ckdir);
[~, mW1] = ppe.run_cstar(SCN, winfun, 'C0', 300, ctxid, ...
    'SkipInject', true, 'CheckpointDir', ckdir);
local_clean(ckdir);
[dsW2, mW2] = ppe.run_cstar(SCN, winfun, 'C0', 300, ctxid, ...
    'SkipInject', true, 'CheckpointDir', ckdir);
w = mW1.warmup_state;
c_ok = ~isempty(w) && isfield(w, 'SOC') && isfield(w, 'fuel_total_kg') ...
    && isfield(w, 'P_gen_actual_kW') && isfield(w, 'P_bat_actual_kW') ...
    && isfield(w, 'P_load_total_kW') && isfield(w, 'state_DG1') ...
    && isfield(w, 'state_DG2') && isfield(w, 'N_on') ...
    && isfield(w, 'last_dlam') && isfield(w, 't') ...
    && abs(w.t - 300) < 1e-9 ...
    && all(isfinite([w.SOC, w.fuel_total_kg, w.P_gen_actual_kW, ...
        w.P_bat_actual_kW, w.P_load_total_kW, w.N_on, w.last_dlam])) ...
    && abs(w.SOC - dsW2.SOC(find(dsW2.t <= 300 + 1e-9, 1, 'last'))) ...
        < 1e-12 ...
    && isequal(mW1.warmup_state, mW2.warmup_state);

r = struct();
r.schema = 'ppe_runner_amendment_smoke_v1';
r.purpose = 'SYNTHETIC_SMOKE';
r.amendment_scope = 'run_cstar.m SkipInject guard + D3 warmup_state snapshot';
r.skipinject_bitwise_equal = a_ok;
r.resume_extra_payload_equal = b_ok;
r.fixture_matches_baseline = fu_ok;
r.warmup_snapshot_ok = c_ok;
r.warmup_state = mW1.warmup_state;
r.cstar_impl_sha256 = ctxid.cstar_impl_sha256;
r.pass = a_ok && b_ok && fu_ok && c_ok;
r.utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
fid = fopen(fullfile(qdir, 'runner_amendment_smoke.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[amend-smoke] pass=%d a=%d b=%d fu=%d c=%d\n', ...
    r.pass, a_ok, b_ok, fu_ok, c_ok);
if ~r.pass
    error('PPE:AMEND:SMOKE', 'runner amendment smoke FAIL');
end
end

function local_clean(ckdir)
d = dir(fullfile(ckdir, 'ckpt_*.mat'));
for i = 1:numel(d)
    delete(fullfile(ckdir, d(i).name));
end
end

function rf = local_latest(ckdir, arm)
rf = [];
d = dir(fullfile(ckdir, sprintf('ckpt_%s_*_seg*.mat', arm)));
if isempty(d)
    error('PPE:AMEND:SMOKE', 'no checkpoint for %s', arm);
end
segs = zeros(numel(d), 1);
for i = 1:numel(d)
    tok = regexp(d(i).name, 'seg(\d+)\.mat$', 'tokens');
    segs(i) = str2double(tok{1}{1});
end
[~, ix] = max(segs);
S = load(fullfile(ckdir, d(ix).name));
rf = S.payload;
end

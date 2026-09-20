function r = qual_simstate_equiv()
% t445.qual_simstate_equiv — T-010 SimState Gate：B1 连续 vs 分段，相对差 ≤ 1e-10
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
cfg = phase4f_t445_config();
SCN = evalin('base', 'SCENARIOS(20)');
stopT = SCN.stop_time_s;
dt = 10;
t0 = (0:dt:(stopT-dt)).';
ehat_tbl = [t0, zeros(size(t0)), zeros(size(t0))];  % 同一 ZOH 输入
sha0 = model.sha256_file(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));

% continuous B1
assignin('base', 'ECMS_EHat_Data', ehat_tbl);
outC = t445.run_arm_b0b1('B1', SCN, 'StopTime', stopT, 'EHatData', ehat_tbl, ...
    'K_S', cfg.lambda_law.K_S_B1);
dsC = model.extract_logsout(outC);

% segmented B1
[dsS, metaS] = t445.run_arm_b1_seg(SCN, ehat_tbl, 'StopTime', stopT);
if bdIsLoaded('HybridShip_ModelE')
    close_system('HybridShip_ModelE', 0);
end
sha1 = model.sha256_file(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));

sigs = {'SOC','P_gen_actual_kW','P_bat_actual_kW','v_ship_mps'};
rel = struct();
ok = true;
ncmp = 0;
for i = 1:numel(sigs)
    nm = sigs{i};
    if ~isfield(dsC, nm) || ~isfield(dsS, nm)
        rel.(nm) = NaN;
        continue;
    end
    n = min(numel(dsC.(nm)), numel(dsS.(nm)));
    a = dsC.(nm)(1:n); b = dsS.(nm)(1:n);
    rr = max(abs(a - b) ./ max(abs(a), 1e-12));
    rel.(nm) = rr;
    ncmp = ncmp + 1;
    if ~(rr <= 1e-10)
        ok = false;
    end
end
r = struct();
r.schema = 't445_t010_simstate_equiv_v1';
r.task = 'T-010';
r.purpose = 'QUALIFICATION';
r.fixture_id = 'SIMSTATE-REF-S20';
r.config_sha256 = cfg.config_sha256;
r.stop_time_s = stopT;
r.n_segments = metaS.n_segments;
r.max_rel = rel;
r.n_signals_compared = ncmp;
r.slx_unchanged = strcmp(sha0, sha1);
r.pass = ok && r.slx_unchanged && ncmp >= 2;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 't445_oracle_gate', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 't010_simstate_equiv.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[T-010 simstate] pass=%d nseg=%d\n', r.pass, r.n_segments);
if ~r.pass
    error('T445:T010:SIMSTATE', 'SimState equivalence FAIL');
end
end

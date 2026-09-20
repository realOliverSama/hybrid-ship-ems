function r = run_t003_b0_regression()
% t445.run_t003_b0_regression — B0 law=5 三参考航次两跑逐样本一致（容差 0）
% fixtures: T01C_S01_still / T04C_S04_res_inc / T10C_S10_current_reversal（10b 距离）
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
end
cfg = phase4f_t445_config();
TESTS = struct('name', {'T01C_S01_still', 'T04C_S04_res_inc', 'T10C_S10_current_reversal'}, ...
    'sid', {1, 4, 10}, 'dist', {1500, 2500, 5000});
rows = struct('name', {}, 'pass', {}, 'n', {}, 'maxAbs', {});
allPass = true;
for k = 1:numel(TESTS)
    T = TESTS(k);
    SCN = evalin('base', sprintf('SCENARIOS(%d)', T.sid));
    SCN.voyage_distance_m = T.dist;
    stopT = 4000;
    out1 = t445.run_arm_b0b1('B0', SCN, 'StopTime', stopT);
    ds1 = model.extract_logsout(out1);
    out2 = t445.run_arm_b0b1('B0', SCN, 'StopTime', stopT);
    ds2 = model.extract_logsout(out2);
    names = intersect(fieldnames(ds1), fieldnames(ds2));
    maxAbs = 0; n = 0;
    for i = 1:numel(names)
        a = ds1.(names{i}); b = ds2.(names{i});
        if ~isnumeric(a) || ~isnumeric(b), continue; end
        a = a(:); b = b(:);
        m = min(numel(a), numel(b));
        d = max(abs(double(a(1:m)) - double(b(1:m))));
        if d > maxAbs, maxAbs = d; end
        n = n + m;
    end
    pass = maxAbs == 0;
    if ~pass, allPass = false; end
    rows(k) = struct('name', T.name, 'pass', pass, 'n', n, 'maxAbs', maxAbs); %#ok<AGROW>
    fprintf('[T-003 B0] %s maxAbs=%g n=%d -> %d\n', T.name, maxAbs, n, pass);
end
r = struct();
r.schema = 't445_t003_b0_regression_v1';
r.task = 'T-003';
r.purpose = 'REGRESSION';
r.arm = 'B0';
r.config_sha256 = cfg.config_sha256;
r.overall = allPass;
r.cases = rows;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
outDir = fullfile(ROOT, 'results', 't445_oracle_gate');
fid = fopen(fullfile(outDir, 't003_b0_regression.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
assert(allPass, 'T445:B0REG', 'B0 two-run identity failed');
end

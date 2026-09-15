function r = run_b1_simulink_parity()
% t445.run_b1_simulink_parity — T-003 B1 核心：Model E 上 law=4/5/6 块 vs 离线容差 0
% 不修改 +ecms2/test_ecms_law（其 T5/T7/T9 绑定 Model D）。
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
end
SCN = evalin('base', 'SCENARIOS(1)');
SCN.voyage_distance_m = 2000;
stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
r = struct('t5', NaN, 't7', NaN, 't9', NaN);

r.t5 = local_cmp(MDL, SCN, stopTimeLimit, 4, []);
r.t7 = local_cmp(MDL, SCN, stopTimeLimit, 5, []);
tTest = (0:stopTimeLimit + 20).';
ehatTest = 1.5 * sin(tTest / 40) + 0.6 * cos(tTest / 120);
assignin('base', 'ECMS_EHat_Data', [tTest, 100 * ones(size(tTest)), ehatTest]);
assignin('base', 'ECMS_K_S', 0.05);
r.t9 = local_cmp(MDL, SCN, stopTimeLimit, 6, ehatTest, tTest);
assignin('base', 'ECMS_K_S', 0.01);
r.pass = (r.t5 == 0) && (r.t7 == 0) && (r.t9 == 0);
end

function maxDiff = local_cmp(MDL, SCN, stopTimeLimit, law, ehatTest, tTest)
if nargin < 6, tTest = []; end
out = model.run_scenario_sim(MDL, SCN, law, stopTimeLimit);
ds = model.extract_logsout(out);
assert(isfield(ds, 'P_gen_ref_kW'), 'T445:B1PARITY:LOG', 'missing P_gen_ref_kW law=%d', law);
ti = ds.t;
idx = find(abs(ti - round(ti)) < 1e-6);
P_load_s = ds.P_load_total_kW(idx);
SOC_s = ds.SOC(idx);
P_ref_blk = ds.P_gen_ref_kW(idx);
lam = evalin('base', 'ECMS_Lambda');
p = ecms2.ecms_params();
maxDiff = 0;
if law == 6
    ehat_s = interp1(tTest, ehatTest, ti(idx), 'linear');
end
for k = 1:numel(idx)
    if law == 4
        Pg_off = ecms2.ecms_law(P_load_s(k), SOC_s(k), lam, p);
    elseif law == 5
        Pg_off = ecms2.ecms_law(P_load_s(k), SOC_s(k), lam, p, 5);
    else
        Pg_off = ecms2.ecms_law(P_load_s(k), SOC_s(k), lam, p, 6, ehat_s(k));
    end
    d = abs(Pg_off - P_ref_blk(k));
    if d > maxDiff, maxDiff = d; end
end
fprintf('[T-003 parity law=%d] n=%d max|dP|=%.6g kW\n', law, numel(idx), maxDiff);
assert(maxDiff == 0, 'T445:B1PARITY', 'law=%d maxDiff=%g', law, maxDiff);
end

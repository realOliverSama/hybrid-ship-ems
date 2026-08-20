%% T-005 验收：make_oracle_pred + tff_metrics + P_gen_rampout_kW 记录
ROOT = '/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelC';
cd(ROOT);
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

MDL = 'HybridShip_ModelC_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

% ---- 1) oracle 生成 ----
SCN = SCENARIOS(4);   % S04 阻力渐增，完整航程
D = model.make_oracle_pred(MDL, SCN, fullfile(ROOT, 'results', 'phase1', 'oracle_S04.mat'));
fprintf('AC1: D size=[%d %d]\n', size(D,1), size(D,2));

% ---- 2) 独立手工抽查 3 个时刻（独立重跑 C0，用 resample_1s 再算窗口峰值/首个峰值位置）----
stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1200;
out2 = model.run_scenario_sim(MDL, SCN, 0, stopTimeLimit);
ds2 = model.extract_logsout(out2);
ds1 = model.resample_1s(ds2);
tf = ds1.t; Pf = ds1.P_load_total_kW;
checkTs = [200 500 900];
fprintf('AC2: P_load range=%.1f..%.1f kW, t_end=%.0f s\n', min(Pf), max(Pf), tf(end));
allOK = true;
for c = 1:numel(checkTs)
    ti = checkTs(c);
    idx = find(tf == ti, 1, 'first');
    winIdx = find(tf > tf(idx) & tf <= tf(idx) + 60);
    win = Pf(winIdx);
    pExp = max(win);
    kf = find(win == pExp, 1, 'first');
    tExp = tf(winIdx(kf)) - tf(idx);
    dRow = D(idx, :);
    okP = abs(dRow(2) - pExp) <= 0.5;
    okT = abs(dRow(3) - tExp) <= 1;
    allOK = allOK && okP && okT;
    fprintf('AC2 t=%d: D=[%.1f %.1f %.1f] expect=[%.1f %.1f %.1f] -> %s\n', ...
        ti, dRow(1), dRow(2), dRow(3), tf(idx), pExp, tExp, string(okP && okT));
end
fprintf('AC2 overall: %s\n', string(allOK));

% ---- 3) tff_metrics 全部字段 ----
m = model.tff_metrics(ds2);
fprintf('AC3: t_sat_s=%.2f s, has_rampout=%d, fuel=%.3f kg, Vdc_min=%.1f, SOC_end=%.4f, E_bat_dis=%.3f kWh, P_bat_rms=%.2f kW, P_bat_peak=%.1f kW, E_unserved=%.4f kWh, v_rmse=%.4f\n', ...
    m.t_sat_s, isfield(ds2,'P_gen_rampout_kW'), m.fuel_kg, m.Vdc_min, m.SOC_end, ...
    m.E_bat_dis_kWh, m.P_bat_rms_kW, m.P_bat_peak_abs_kW, m.E_unserved_kWh, m.v_rmse_mps);
fprintf('AC3: P_ff_activated_s=%s (C0 应为 NaN)\n', mat2str(m.P_ff_activated_s));

% ---- 4) t_sat_s 合理性：应远小于航程时长 ----
fprintf('AC4: t_sat_s=%.2f s vs voyage=%.0f s -> 占比=%.2f%%\n', m.t_sat_s, tf(end), 100*m.t_sat_s/tf(end));

fprintf('T005_ACCEPT: %s\n', string(allOK));

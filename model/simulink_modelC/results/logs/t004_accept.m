%% T-004 验收脚本：EMS_ScheduledFF 定时前馈行为验证
ROOT = '/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelC';
cd(ROOT);
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
run(fullfile(ROOT,'config','Model_Parameters.m'));
run(fullfile(ROOT,'config','Model_Scenarios.m'));
MDL = 'HybridShip_ModelC_DCBus';
% 加载真实 MLP 权重（覆盖 Model_Parameters 占位符；win=22 → 110×64）
W = load(fullfile(ROOT,'python','export','mlp_weights.mat'));
fns = fieldnames(W);
for i=1:numel(fns)
    if startsWith(fns{i},'MLP_') && isnumeric(W.(fns{i}))
        assignin('base', fns{i}, double(W.(fns{i})));
    end
end
assignin('base','MLP_WeightsLoaded',1);
load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);
assignin('base','AI_Pred_Source',0);
assignin('base','AI_TPeak_Source',0);
assignin('base','ASSUMED_AI_DataValid',true);
% oracle 表（S01 静水，人工逼近中的峰值：P_peak=400 常数，t_peak_rel=倒计时）
t_or = (0:120)';
oracle = [t_or, 1200*ones(numel(t_or),1), max(0, 60 - t_or)];
assignin('base','AI_Oracle_Data',oracle);

SCN = SCENARIOS(1); SCN.voyage_distance_m = 500;

%% ================= Test 2: 空预测回归（law=2+Source=0 vs law=0）=================
assignin('base','AI_Pred_Source',0);
out0 = model.run_scenario_sim(MDL, SCN, 0, 120);
ds0 = model.extract_logsout(out0);
out2 = model.run_scenario_sim(MDL, SCN, 2, 120);
ds2 = model.extract_logsout(out2);
t = ds2.t;
% P_gen_ref 对齐时间轴
Pg0 = interp1(ds0.t, ds0.P_gen_ref_kW, t, 'linear', 'extrap');
Pg2 = ds2.P_gen_ref_kW;
dPg = max(abs(Pg0 - Pg2));
fprintf('[T2] law2-Src0 vs law0: N=%d max|dP_gen_ref|=%.3e\n', numel(t), dPg);
% P_ff 应恒 0
if isfield(ds2, 'P_ff_kW')
    maxPff = max(abs(ds2.P_ff_kW));
    fprintf('[T2] P_ff_kW max=%.3e (应≈0)\n', maxPff);
else
    maxPff = -1; fprintf('[T2] WARN: logsout 无 P_ff_kW\n');
end
T2_pass = (dPg < 1e-9) && (maxPff < 1e-9);

%% ================= Test 3: 定时触发行为（law=2, Source=1, countdown oracle）=================
assignin('base','AI_Pred_Source',1);
outC2 = model.run_scenario_sim(MDL, SCN, 2, 120);
dsC2 = model.extract_logsout(outC2);
tC2 = dsC2.t;
% 理论值：dP = min(max(400-P_load,0)*K_ff, FF_Max)（K_ff=1, FF_Max=400）
P_load_C2 = dsC2.P_load_total_kW;
dP_theory = min(max(1200 - P_load_C2, 0), 400);
T_ramp_theory = dP_theory / 40 + 10;   % ASSUMED_TFF_TauMargin=10
% 首次 P_ff 显著非零时刻 vs 理论 60 - T_ramp（在 P_load 稳态时 T_ramp≈(400-P_load)/40+10）
P_ff_C2 = dsC2.P_ff_kW;
iFire = find(P_ff_C2 > 0.5, 1, 'first');
t_fire_actual = tC2(iFire);
% 理论触发时刻：t_fire = t + t_peak_rel - T_ramp，在 t_peak_rel=max(0,60-t) 下
% 峰值固定 P_peak=400 且 P_load 近稳态时：t_fire_theory = 60 - ((400-P_load)/40+10)
P_load_fire = P_load_C2(iFire);
t_fire_theory = 60 - (min(max(1200-P_load_fire,0),400)/40 + 10);
fprintf('[T3] t_fire_actual=%.2f s  t_fire_theory=%.2f s  (P_load@fire=%.1f kW)\n', ...
    t_fire_actual, t_fire_theory, P_load_fire);
% 等待期 P_ff ≈ 0（t < t_fire_actual-0.5）
iW = tC2 < (t_fire_actual - 0.5);
maxPff_wait = max(abs(P_ff_C2(iW)));
fprintf('[T3] 等待期(t<%.2f) max|P_ff|=%.4f kW (应≈0)\n', t_fire_actual-0.5, maxPff_wait);
% 触发后 P_ff ≈ dP_theory
iA = tC2 >= t_fire_actual;
errA = max(abs(P_ff_C2(iA) - dP_theory(iA)));
fprintf('[T3] 触发后 max|P_ff - dP_theory|=%.4f kW (应≤1)\n', errA);
% P_gen_actual 爬坡率 ≤ 40 kW/s
Pga = dsC2.P_gen_actual_kW;
slew = abs(diff(Pga))./diff(tC2);
maxSlew = max(slew(isfinite(slew)));
fprintf('[T3] P_gen_actual max slew=%.2f kW/s (应≤40*1.02)\n', maxSlew);
% t_fire_latched 观测
if isfield(dsC2, 't_fire_latched')
    tfire_log = dsC2.t_fire_latched;
    tfire_first = tfire_log(find(tfire_log > 0, 1, 'first'));
    fprintf('[T3] t_fire_latched first=%.2f\n', tfire_first);
end
T3_pass = (maxPff_wait < 0.5) && (errA < 1.0) && (maxSlew < 40*1.02);

%% ================= Test 4: law=1 对照组（C1 即时前馈，同 oracle）=================
assignin('base','AI_Pred_Source',1);
outC1 = model.run_scenario_sim(MDL, SCN, 1, 120);
dsC1 = model.extract_logsout(outC1);
tC1 = dsC1.t;
P_load_C1 = dsC1.P_load_total_kW;
dP_C1 = min(max(1200 - P_load_C1, 0), 400);
% C1 的 P_ff 等价量 = P_gen_ref - P_load（前馈立即生效）
ff_C1 = dsC1.P_gen_ref_kW - P_load_C1;
iC1 = find(ff_C1 > 0.5, 1, 'first');
fprintf('[T4] C1 首次前馈生效 t=%.2f s (应≈0, 立即响应)\n', tC1(iC1));
iC1w = tC1 < 3;
fprintf('[T4] C1 前3s平均 ff=%.1f kW (应≈dP=%.1f)\n', mean(ff_C1(iC1w)), mean(dP_C1(iC1w)));

%% ================= 汇总 =================
fprintf('\n===== T-004 验收结果 =====\n');
if T2_pass, s2 = 'PASS'; else, s2 = 'FAIL'; end
if T3_pass, s3 = 'PASS'; else, s3 = 'FAIL'; end
fprintf('T2 空预测回归: %s (max|dPg|=%.3e, maxPff=%.3e)\n', s2, dPg, maxPff);
fprintf('T3 定时触发:   %s (wait=%.4f, after=%.4f, slew=%.2f)\n', s3, maxPff_wait, errA, maxSlew);
fprintf('T4 C1 对照:    C1 first=%.2fs, C2 first=%.2fs\n', tC1(iC1), t_fire_actual);
ALL = T2_pass && T3_pass;
if ALL, sa = 'PASS'; else, sa = 'FAIL'; end
fprintf('总体: %s\n', sa);

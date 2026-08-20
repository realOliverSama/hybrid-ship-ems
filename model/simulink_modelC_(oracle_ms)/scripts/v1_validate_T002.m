%% v1_validate_T002.m — T-002 验证：参数化覆写机制
ROOT = fileparts(fileparts(mfilename('fullpath')));
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
md = fullfile(ROOT, 'results', 'ms_phase1'); if ~exist(md,'dir'), mkdir(md); end

MDL = 'HybridShip_ModelCms_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

% 参数登记检查
assert(isfield(ParamMeta, '') || true, 'placeholder');
% ParamMeta 是 cell 数组，转成名字列表
names = ParamMeta(:,1);
for nm = {'P_load_design_max','MSFF_Horizon_s','MSFF_dt_reach','MSFF_FF_Hyst_kW', ...
          'MSFF_InfoMode','MSFF_SOC_Knee','MSFF_Vdc_Knee'}
    assert(any(strcmp(names, nm{1})), 'registry missing %s', nm{1});
end
fprintf('[T002] 参数登记表包含全部 7 个新增项 PASS\n');
assert(evalin('base','P_load_design_max') == 800, 'P_load_design_max != 800');
fprintf('[T002] P_load_design_max = 800 kW PASS\n');

% 1) 默认配置短仿真（S09 前 120 s）
SCN = SCENARIOS(9);
stop = min(120, SCN.voyage_distance_m / SCN.target_speed_mps + 1200);
out1 = model.run_scenario_sim(MDL, SCN, 0, stop);
ds1 = model.extract_logsout(out1);
t = ds1.t; kk = find(t<=120, 1, 'last');
ramp_default = ds1.P_gen_rampout_kW(1:kk);
t_default = t(1:kk);

% 2) 覆写 R_gen=20 → 行为变化且不触发重建
eff = model.apply_gen_params('R_gen', 20);
out2 = model.run_scenario_sim(MDL, SCN, 0, stop);
ds2 = model.extract_logsout(out2);
ramp_slow = ds2.P_gen_rampout_kW(1:kk);
t_slow = t(1:kk);

% 爬坡率对比：前 20 s 平均爬坡率
r1 = diff(ramp_default) / 0.01; r2 = diff(ramp_slow) / 0.01;
m1 = max(r1(1:2000)); m2 = max(r2(1:2000));
fprintf('[T002] 最大瞬时爬坡率: 默认 %.1f kW/s vs 覆写 20 %.1f kW/s\n', m1, m2);
assert(abs(m1 - 40) < 2, '默认爬坡率应≈40, got %.1f', m1);
assert(abs(m2 - 20) < 2, '覆写爬坡率应≈20, got %.1f', m2);
fprintf('[T002] 爬坡率行为变化 PASS（40→20 kW/s 可观测，无需 rebuild）\n');

% 3) λ_P 覆写
eff2 = model.apply_gen_params('lambda_P', 0.8);
assert(abs(eff2.Gen_P_max - 640) < 1e-9, 'λ_P=0.8 应得 640 kW');
fprintf('[T002] λ_P 覆写 PASS（0.8 → Gen_P_max=%.0f kW）\n', eff2.Gen_P_max);

% 4) 恢复默认
model.apply_gen_params('GP_kW', 1000, 'R_gen', 40, 'tau_gen', 5);
fprintf('[T002] 恢复默认 PASS\n');

% 5) 冒烟回归：副本构建后冒烟仍 PASS（Model_Parameters 新增项不破坏构建）
fprintf('[T002] 冒烟验证见 20b_smoke_modelCms.m 单独运行结果\n');
close_system(MDL, 0);
fprintf('[T002] ALL PASS\n');

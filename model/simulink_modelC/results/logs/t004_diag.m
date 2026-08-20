%% T-004 诊断：检查 T3 场景下 P_load / P_ff / valid / t_peak 信号
ROOT = '/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelC';
cd(ROOT);
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
run(fullfile(ROOT,'config','Model_Parameters.m'));
run(fullfile(ROOT,'config','Model_Scenarios.m'));
W = load(fullfile(ROOT,'python','export','mlp_weights.mat'));
fns = fieldnames(W);
for i=1:numel(fns)
    if startsWith(fns{i},'MLP_') && isnumeric(W.(fns{i}))
        assignin('base', fns{i}, double(W.(fns{i})));
    end
end
assignin('base','MLP_WeightsLoaded',1);
MDL = 'HybridShip_ModelC_DCBus';
load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);
assignin('base','AI_Pred_Source',1);
assignin('base','AI_TPeak_Source',0);
assignin('base','ASSUMED_AI_DataValid',true);
t_or = (0:120)';
oracle = [t_or, 400*ones(numel(t_or),1), max(0, 60 - t_or)];
assignin('base','AI_Oracle_Data',oracle);
SCN = SCENARIOS(1); SCN.voyage_distance_m = 500;
out = model.run_scenario_sim(MDL, SCN, 2, 120);
ds = model.extract_logsout(out);
fprintf('fields: %s\n', strjoin(fieldnames(ds)', ', '));
if isfield(ds,'P_ff_kW')
    fprintf('P_ff:  max=%.2f min=%.2f\n', max(ds.P_ff_kW), min(ds.P_ff_kW));
end
if isfield(ds,'P_pred_max_kW')
    fprintf('P_pred: max=%.2f min=%.2f\n', max(ds.P_pred_max_kW), min(ds.P_pred_max_kW));
end
if isfield(ds,'t_peak_pred_s')
    fprintf('t_peak_pred_s: max=%.2f min=%.2f\n', max(ds.t_peak_pred_s), min(ds.t_peak_pred_s));
end
if isfield(ds,'AI_valid')
    fprintf('AI_valid: max=%.2f min=%.2f\n', max(ds.AI_valid), min(ds.AI_valid));
end
fprintf('P_load: max=%.2f min=%.2f (t=0:%.1f t=30:%.1f t=60:%.1f)\n', ...
    max(ds.P_load_total_kW), min(ds.P_load_total_kW), ...
    ds.P_load_total_kW(1), ds.P_load_total_kW(3001), ds.P_load_total_kW(6001));
fprintf('P_gen_ref: max=%.2f\n', max(ds.P_gen_ref_kW));
fprintf('t_fire_latched: max=%.2f (非-1样本数=%d)\n', max(ds.t_fire_latched), sum(ds.t_fire_latched > 0));
% 理论 dP / t_fire 演化
iT = 1:100:numel(ds.t);
for i = iT(1:3:end)
    dP = min(max(400 - ds.P_load_total_kW(i), 0), 400);
    fprintf('t=%.0f P_load=%.1f dP=%.1f T_ramp=%.1f 60-T_ramp=%.1f\n', ...
        ds.t(i), ds.P_load_total_kW(i), dP, dP/40+10, 60-(dP/40+10));
end

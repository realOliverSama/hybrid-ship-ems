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
assignin('base','EMS_WORK_MODE',2);
MDL='HybridShip_ModelC_DCBus';
load_system(fullfile(ROOT,'model',[MDL '.slx']));
SCN = SCENARIOS(1);
SCN.voyage_distance_m = 500;
% ---- 测试1：Source=1 + N×3 oracle 表 ----
t_or = (0:10:100)';
oracle = [t_or, 800 + 20*t_or, 30*ones(numel(t_or),1)];  % P_peak 800..1000, t_peak 常数30
assignin('base','AI_Pred_Source',1);
assignin('base','AI_TPeak_Source',0);
assignin('base','AI_Pred_Data',oracle);
assignin('base','ASSUMED_AI_DataValid',true);
out = model.run_scenario_sim(MDL, SCN, 1, 120);
ds = model.extract_logsout(out);
fprintf('TEST1 fields: t_peak=%d P_pred=%d\n', isfield(ds,'t_peak_pred_s'), isfield(ds,'P_pred_max_kW'));
if isfield(ds,'t_peak_pred_s')
    tp = ds.t_peak_pred_s;
    fprintf('TEST1 t_peak_pred_s: mean=%g min=%g max=%g (expect 30)\n', mean(tp), min(tp), max(tp));
    % 检查 t 落在表内时的插值（t=10..100 线性）
    i1 = find(abs(ds.t-10)<0.01,1); i2 = find(abs(ds.t-50)<0.01,1); i3 = find(abs(ds.t-100)<0.01,1);
    if ~isempty(i1), fprintf('TEST1 P_pred@t=10: %g (expect 1000)\n', ds.P_pred_max_kW(i1)); end
    if ~isempty(i2), fprintf('TEST1 P_pred@t=50: %g (expect 1800)\n', ds.P_pred_max_kW(i2)); end
    if ~isempty(i3), fprintf('TEST1 P_pred@t=100: %g (expect 2800)\n', ds.P_pred_max_kW(i3)); end
end
% ---- 测试2：Source=3 + AI_TPeak_Source=1（MLP 幅值 + oracle t_peak 真值）----
assignin('base','AI_Pred_Source',3);
assignin('base','AI_TPeak_Source',1);
assignin('base','AI_Pred_Data',oracle);
out2 = model.run_scenario_sim(MDL, SCN, 1, 120);
ds2 = model.extract_logsout(out2);
if isfield(ds2,'t_peak_pred_s')
    tp2 = ds2.t_peak_pred_s;
    fprintf('TEST2 t_peak_pred_s (override): mean=%g min=%g max=%g (expect ~30)\n', mean(tp2), min(tp2), max(tp2));
end
% ---- 测试3：Baseline 冒烟（EMS_MODE=0）----
out3 = model.run_scenario_sim(MDL, SCN, 0, 120);
ds3 = model.extract_logsout(out3);
fprintf('TEST3 baseline smoke: t=%d pts, V_dc range [%g,%g]\n', numel(ds3.t), min(ds3.V_dc_V), max(ds3.V_dc_V));
fprintf('ALL NEWFUNC TESTS COMPLETE\n');

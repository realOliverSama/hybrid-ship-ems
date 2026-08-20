%% 回归：确认 C1 (mode=1) 路径与 set_ai_logging 重构未破坏（Source=3 环内推理）
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
assignin('base','AI_Pred_Source',3);   % 环内 MLP
assignin('base','AI_TPeak_Source',0);
SCN = SCENARIOS(1); SCN.voyage_distance_m = 500;
out = model.run_scenario_sim(MDL, SCN, 1, 120);
ds = model.extract_logsout(out);
ok = isfield(ds,'P_pred_max_kW') && isfield(ds,'AI_valid') && isfield(ds,'P_gen_ref_kW');
fprintf('C1 mode=1 regress: has P_pred=%d AI_valid=%d P_gen_ref=%d\n', ...
    isfield(ds,'P_pred_max_kW'), isfield(ds,'AI_valid'), isfield(ds,'P_gen_ref_kW'));
fprintf('P_pred max=%.1f valid max=%.1f\n', max(ds.P_pred_max_kW), max(ds.AI_valid));
fprintf('C1-REGRESS: %s\n', string(ok));

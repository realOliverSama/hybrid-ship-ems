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
assignin('base','AI_Pred_Source',3);
assignin('base','EMS_WORK_MODE',2);
MDL='HybridShip_ModelC_DCBus';
load_system(fullfile(ROOT,'model',[MDL '.slx']));
SCN = SCENARIOS(4);
out = model.run_scenario_sim(MDL, SCN, 1, 1500);
ds = model.extract_logsout(out);
save(fullfile(ROOT,'results','phase1','t002_after_C1_S04.mat'),'ds');
fprintf('AFTER done: t=%d pts, has P_pred=%d, t_peak=%d, AI_valid=%d\n', ...
    numel(ds.t), isfield(ds,'P_pred_max_kW'), isfield(ds,'t_peak_pred_s'), isfield(ds,'AI_valid'));

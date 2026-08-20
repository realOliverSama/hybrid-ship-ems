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
SCN1 = SCENARIOS(1); SCN1.voyage_distance_m = 500;
% oracle P_peak = 1500 kW 常数（超过负荷）→ delta>0 → FF 应抬高 P_gen_ref
t_or = (0:10:110)';
oracle = [t_or, 1500*ones(numel(t_or),1), 30*ones(numel(t_or),1)];
assignin('base','AI_Pred_Source',1);
assignin('base','AI_TPeak_Source',0);
assignin('base','AI_Oracle_Data',oracle);
assignin('base','ASSUMED_AI_DataValid',true);
out = model.run_scenario_sim(MDL, SCN1, 1, 120);
ds = model.extract_logsout(out);
% 统计 FF 生效（P_gen_ref > P_load + 1）的时间占比与最大前馈
ff_act = ds.P_gen_ref_kW > ds.P_load_total_kW + 1;
fprintf('FF active fraction=%.3f  max(P_gen_ref-P_load)=%.1f kW\n', mean(ff_act), max(ds.P_gen_ref_kW-ds.P_load_total_kW));
i = find(ff_act, 1, 'first');
if ~isempty(i)
    fprintf('example t=%.2f: P_load=%.1f P_pred=%.1f P_gen_ref=%.1f delta=%.1f\n', ...
        ds.t(i), ds.P_load_total_kW(i), ds.P_pred_max_kW(i), ds.P_gen_ref_kW(i), ds.P_pred_max_kW(i)-ds.P_load_total_kW(i));
end
fprintf('FFCHK DONE\n');

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
SCN = SCENARIOS(4);

%% ---- 验收1: 回归（Source=3 环内 MLP，S04 1500s）----
assignin('base','AI_Pred_Source',3);
assignin('base','AI_TPeak_Source',0);
out = model.run_scenario_sim(MDL, SCN, 1, 1500);
ds = model.extract_logsout(out);
save(fullfile(ROOT,'results','phase1','t002_regr2_C1_S04.mat'),'ds');
B = load(fullfile(ROOT,'results','phase1','t002_before_C1_S04.mat'));
dP = max(abs(ds.P_pred_max_kW - B.ds.P_pred_max_kW));
dG = max(abs(ds.P_gen_ref_kW - B.ds.P_gen_ref_kW));
fprintf('REGR: pts=%d max|dP_pred|=%g max|dP_gen_ref|=%g\n', numel(ds.t), dP, dG);

%% ---- 验收2: Source=1 + N×3 oracle（斜坡 P_peak, t_peak=30 常数）----
t_or = (0:10:110)';
oracle = [t_or, 100+t_or, 30*ones(numel(t_or),1)];
assignin('base','AI_Pred_Source',1);
assignin('base','AI_TPeak_Source',0);
assignin('base','AI_Oracle_Data',oracle);
assignin('base','ASSUMED_AI_DataValid',true);
SCN1 = SCENARIOS(1); SCN1.voyage_distance_m = 500;
out1 = model.run_scenario_sim(MDL, SCN1, 1, 120);
ds1 = model.extract_logsout(out1);
fprintf('SRC1 fields: t_peak=%d P_pred=%d AI_valid=%d\n', ...
    isfield(ds1,'t_peak_pred_s'), isfield(ds1,'P_pred_max_kW'), isfield(ds1,'AI_valid'));
if isfield(ds1,'t_peak_pred_s')
    fprintf('SRC1 t_peak: mean=%g min=%g max=%g\n', mean(ds1.t_peak_pred_s), min(ds1.t_peak_pred_s), max(ds1.t_peak_pred_s));
end
for tt=[10 30 60 90 110]
    i1=find(abs(ds1.t-tt)<0.01,1);
    if ~isempty(i1)
        fprintf('SRC1 P_pred@t=%d: %g (expect ~%d)\n', tt, ds1.P_pred_max_kW(i1), 100+tt);
    end
end
fprintf('SRC1 vmean=%g (expect ~1) P_load_max=%g\n', mean(ds1.AI_valid), max(ds1.P_load_total_kW));

%% ---- 验收3: 覆盖（Source=3 + AI_TPeak_Source=1 → t_peak=30，P_pred=MLP）----
assignin('base','AI_Pred_Source',3);
assignin('base','AI_TPeak_Source',1);
out2 = model.run_scenario_sim(MDL, SCN1, 1, 120);
ds2 = model.extract_logsout(out2);
if isfield(ds2,'t_peak_pred_s')
    fprintf('OVR t_peak: mean=%g min=%g max=%g (expect 30)\n', mean(ds2.t_peak_pred_s), min(ds2.t_peak_pred_s), max(ds2.t_peak_pred_s));
end
fprintf('OVR vmean=%g\n', mean(ds2.AI_valid));

%% ---- 验收4: Baseline 冒烟（EMS_MODE=0）----
assignin('base','AI_Pred_Source',0);
out3 = model.run_scenario_sim(MDL, SCN1, 0, 120);
ds3 = model.extract_logsout(out3);
fprintf('BASE smoke: pts=%d Vdc=[%g,%g] fuel=%.3f\n', numel(ds3.t), min(ds3.V_dc_V), max(ds3.V_dc_V), ds3.fuel_total_kg(end));
fprintf('ALL T-002 ACCEPT TESTS COMPLETE\n');

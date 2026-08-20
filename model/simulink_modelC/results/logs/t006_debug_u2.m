%% debug U2: dump first samples
ROOT='/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelC';
cd(ROOT);
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
run(fullfile(ROOT,'config','Model_Parameters.m'));
run(fullfile(ROOT,'config','Model_Scenarios.m'));
load_system(fullfile(ROOT,'model','HybridShip_ModelC_DCBus.slx'));
W=load(fullfile(ROOT,'python','export','mlp_weights.mat')); fns=fieldnames(W);
for i=1:numel(fns)
    if startsWith(fns{i},'MLP_') && isnumeric(W.(fns{i}))
        assignin('base',fns{i},double(W.(fns{i})));
    end
end
assignin('base','MLP_WeightsLoaded',1);
assignin('base','AI_Pred_Source',1); assignin('base','AI_TPeak_Source',0);
assignin('base','ASSUMED_AI_DataValid',true); assignin('base','EMS_WORK_MODE',2);
tU2=(0:30)'; oracleU2=[tU2, 950*ones(31,1), max(0,10-tU2)];
assignin('base','AI_Oracle_Data',double(oracleU2));
SCN=SCENARIOS(1); SCN.voyage_distance_m=2000; SCN.initial_SOC=0.60;
out=model.run_scenario_sim('HybridShip_ModelC_DCBus', SCN, 2, 30);
ds=model.extract_logsout(out);
dP=min(max(950-ds.P_load_total_kW,0),400);
rm=zeros(size(dP)); a=-inf;
for k=1:numel(dP), a=max(a,dP(k)); rm(k)=a; end
fprintf('k t P_load P_pred t_peak P_ff dP_theory rmax\n');
n=min(12,numel(ds.t));
for k=1:n
    fprintf('%d %.2f %.1f %.1f %.1f %.1f %.1f %.1f\n', k, ds.t(k), ds.P_load_total_kW(k), ...
        ds.P_pred_max_kW(k), ds.t_peak_pred_s(k), ds.P_ff_kW(k), dP(k), rm(k));
end
err = max(abs(ds.P_ff_kW(1:n) - rm(1:n)));
fprintf('max|P_ff-rmax| (first %d) = %.2f\n', n, err);

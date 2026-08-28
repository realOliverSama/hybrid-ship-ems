%% ========================================================================
%  inject_generated.m — controlled experiment: inject the EXACT text that
%  the product generator (CODE_ECMS_NN sprintf) produced — dumped in
%  ecms_step_generated.m — into the real model's law=7 chart, then update.
%
%  Outcome A (PASS):  text is innocent -> product-path failure is caused by
%                     something else in setMFCode/build sequencing.
%  Outcome B (FAIL):  the sprintf-expanded text itself triggers the parse
%                     failure (vs the strjoin-built text that passed) ->
%                     bisect the difference (likely comment encoding).
%% ========================================================================
clearvars; close all; clc;
ROOT = 'E:/User/Tommy/Desktop/hybrid-ship-ems/model/simulink_modelD';
MDL  = 'HybridShip_ModelD_DCBus';
slx  = fullfile(ROOT, 'model', [MDL '.slx']);
genFile = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7', 'ecms_step_generated.m');
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7');

cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'INJ:WEIGHTS', 'weights missing');

codeText = fileread(genFile);
fprintf('[inj] loaded generated text: %d chars\n', numel(codeText));

paramNames = {'ASSUMED_eta_rect','ASSUMED_eta_dcdc','ASSUMED_Bat_Eff_Discharge', ...
    'ASSUMED_Bat_Eff_Charge','Gen_P_max','ECMS_SOC_Discharge_Max','ECMS_SOC_Charge_Min', ...
    'ECMS_Idle_Fuel_kgph','ECMS_Grid_dP_kW','ECMS_dt_ctrl_s','Pgen_init', ...
    'ECMS_NN_W1','ECMS_NN_b1','ECMS_NN_W2','ECMS_NN_b2','ECMS_NN_W3','ECMS_NN_b3', ...
    'ECMS_NN_XMean','ECMS_NN_XStd','ECMS_NN_YMean','ECMS_NN_YStd', ...
    'ECMS_NN_Ebar_H_kWh','ECMS_NN_Escale_kWh','ECMS_NN_WeightsOK'};

try
    assignin('base', 'EMS_CTRL_LAW', 7);
    if bdIsLoaded(MDL), close_system(MDL, 0); end
    load_system(slx);
    chPath = [MDL '/05_EMS_Controller/EMS_NN_AECMS/ecms_step'];
    ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', chPath);
    assert(~isempty(ch), 'INJ:CHART', 'law=7 chart not found');
    ch = ch(1);
    ch.Script = codeText;
    ds = ch.find('-isa', 'Stateflow.Data');
    for i = 1:numel(paramNames)
        d = [];
        for j = 1:numel(ds)
            if strcmp(ds(j).Name, paramNames{i}), d = ds(j); break; end
        end
        if isempty(d)
            d = Stateflow.Data(ch);
            d.Name = paramNames{i};
        end
        d.Scope = 'Parameter';
    end
    chEC = ch;
    chEC.ChartUpdate = 'DISCRETE';
    chEC.SampleTime = '0.01';
    set_param(MDL, 'SimulationCommand', 'update');
    fprintf('[inj] RESULT: GENERATED TEXT COMPILES OK on real model chart\n');
    res = struct('pass', true, 'note', 'generated text compiles');
catch err
    fprintf('[inj] RESULT: FAILED - %s | %s\n', err.identifier, err.message);
    res = struct('pass', false, 'identifier', err.identifier, 'message', err.message);
end
try
    set_param(MDL, 'SimulationCommand', 'stop');
catch
end
try
    close_system(MDL, 0);
catch
end

jPath = fullfile(outDir, 'inject_generated_result.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(res), 'char');
fclose(fid);
fprintf('[inj] JSON written: %s\n', jPath);
fprintf('[inj] DONE\n');

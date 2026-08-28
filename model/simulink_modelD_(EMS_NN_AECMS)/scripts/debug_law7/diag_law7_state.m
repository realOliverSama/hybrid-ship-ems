%% ========================================================================
%  diag_law7_state.m — hypothesis test:
%    H: the current rebuilt model's law=7 runs with e_hat == 0 permanently
%       (weights not reaching the forward pass), i.e. behaves like law=5.
%    Test: run S01 with law=5 and law=7 on the SAME rebuilt model and
%          compare fuel. fuel5 ~= fuel7  => H confirmed.
%    Also probes the first-gate behavior by logging P_gen_ref statistics.
%% ========================================================================
clearvars; close all; clc;
ROOT = 'E:/User/Tommy/Desktop/hybrid-ship-ems/model/simulink_modelD';
MDL  = 'HybridShip_ModelD_DCBus';
slx  = fullfile(ROOT, 'model', [MDL '.slx']);

cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'DG:WEIGHTS', 'weights missing');

SCN = evalin('base', 'SCENARIOS(1)');
stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;

if bdIsLoaded(MDL), close_system(MDL, 0); end
load_system(slx);
set_param(MDL, 'SimulationCommand', 'update');   % one compile for both runs

%% ---- run law=5 ------------------------------------------------------------
assignin('base', 'EMS_CTRL_LAW', 5);
assignin('base', 'EMS_MODE', 1);
out5 = model.run_scenario_sim(MDL, SCN, 5, stopTimeLimit);
ds5 = model.extract_logsout(out5);

%% ---- run law=7 ------------------------------------------------------------
assignin('base', 'EMS_CTRL_LAW', 7);
out7 = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
ds7 = model.extract_logsout(out7);

fuel5 = ds5.fuel_total_kg(end);
fuel7 = ds7.fuel_total_kg(end);
dP = max(abs(ds5.P_gen_ref_kW(1:min(numel(ds5.t),numel(ds7.t))) - ...
             ds7.P_gen_ref_kW(1:min(numel(ds5.t),numel(ds7.t)))));
close_system(MDL, 0);

fprintf('\n===== DIAG =====\n');
fprintf('fuel law=5 : %.17g kg\n', fuel5);
fprintf('fuel law=7 : %.17g kg\n', fuel7);
fprintf('max|dP_gen_ref| between law5/law7 : %.6g kW\n', dP);
h = struct('fuel_law5', fuel5, 'fuel_law7', fuel7, 'dP_max', dP, ...
           'same_behavior', double(abs(fuel5-fuel7) < 1e-9));
jPath = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7', 'diag_law7_state.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(h), 'char');
fclose(fid);
fprintf('[dg] JSON written: %s\n', jPath);
fprintf('[dg] DONE\n');

%% ========================================================================
%  diag_bisect_flow.m — bisect WHICH stage poisons the law=7 simulation.
%
%  Known facts:
%    - Disk .slx produced by the rebuild is CORRECT (a fresh process loading
%      it gets bitwise-golden fuel 91.2317939047896).
%    - The in-process sequence rebuild -> smoke -> sim yields 85.8253569207292.
%
%  This script runs, IN ONE PROCESS, in order:
%    Part X: rebuild -> close -> fresh load -> law=7 S01 -> fuel_X
%            (if fuel_X == golden, rebuild itself is innocent in-process)
%    Part S: 10b smoke -> fresh reload -> law=7 S01 -> fuel_S
%            (if fuel_S degrades, the smoke stage is the poison)
%% ========================================================================
clearvars; close all; clc;
ROOT = 'E:/User/Tommy/Desktop/hybrid-ship-ems/model/simulink_modelD';
MDL  = 'HybridShip_ModelD_DCBus';
slx  = fullfile(ROOT, 'model', [MDL '.slx']);
GOLDEN_FUEL = 91.2317939047896;

cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'DB:WEIGHTS', 'weights missing');

%% ---- Part X: rebuild then immediately simulate ------------------------------
fprintf('[db] ===== Part X: rebuild -> sim =====\n');
eval(fileread(fullfile(ROOT, 'scripts', '10_build_modelD.m')));
fprintf('[db] rebuild done\n');
if bdIsLoaded(MDL), close_system(MDL, 0); end
assignin('base', 'EMS_CTRL_LAW', 7);
load_system(slx);
set_param(MDL, 'SimulationCommand', 'update');
SCN = evalin('base', 'SCENARIOS(1)');
stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
outX = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
dsX = model.extract_logsout(outX);
fuelX = dsX.fuel_total_kg(end);
fprintf('[db] Part X fuel=%.17g (golden %.17g)\n', fuelX, GOLDEN_FUEL);

%% ---- Part S: smoke then simulate again --------------------------------------
fprintf('[db] ===== Part S: smoke -> sim =====\n');
eval(fileread(fullfile(ROOT, 'scripts', '10b_smoke_modelD.m')));
if bdIsLoaded(MDL), close_system(MDL, 0); end
% reload pristine scenarios (smoke mutates SCENARIOS(1))
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
assignin('base', 'EMS_CTRL_LAW', 7);
load_system(slx);
set_param(MDL, 'SimulationCommand', 'update');
SCN = evalin('base', 'SCENARIOS(1)');
stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
outS = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
dsS = model.extract_logsout(outS);
fuelS = dsS.fuel_total_kg(end);
fprintf('[db] Part S fuel=%.17g (golden %.17g)\n', fuelS, GOLDEN_FUEL);
close_system(MDL, 0);

%% ---- verdict ------------------------------------------------------------------
sameXS = (abs(fuelX - fuelS) < 1e-9);
verdict = 'REBUILD-STAGE POISON (X already degraded)';
if ~sameXS
    verdict = 'SMOKE-STAGE POISON (X clean, S degraded)';
end
if abs(fuelX - GOLDEN_FUEL) < 1e-9 && abs(fuelS - GOLDEN_FUEL) < 1e-9
    verdict = 'NO POISON (both golden — earlier failure was transient)';
end
fprintf('\n===== BISECT VERDICT =====\n');
fprintf('fuel X (rebuild->sim)        : %.17g\n', fuelX);
fprintf('fuel S (smoke->sim)          : %.17g\n', fuelS);
fprintf('golden                       : %.17g\n', GOLDEN_FUEL);
fprintf('VERDICT                      : %s\n', verdict);

h = struct('fuel_X', fuelX, 'fuel_S', fuelS, 'golden', GOLDEN_FUEL, 'verdict', verdict);
jPath = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7', 'diag_bisect_flow.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(h), 'char');
fclose(fid);
fprintf('[db] JSON written: %s\n', jPath);
fprintf('[db] DONE\n');

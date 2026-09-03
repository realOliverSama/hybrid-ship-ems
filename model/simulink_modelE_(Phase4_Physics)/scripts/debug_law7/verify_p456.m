%% ========================================================================
%  verify_p456.m — P4 smoke / P5 golden-fuel S01 / P6 test_law7 for the
%  product switch to the inline law=7. (P3 structural diff already PASSED:
%  charts_old_head.json vs charts_new_inline.json — only EMS_NN_AECMS/
%  ecms_step differs, all other 19 charts bitwise identical.)
%% ========================================================================
clearvars; close all; clc;
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
MDL  = 'HybridShip_ModelD_DCBus';
slx  = fullfile(ROOT, 'model', [MDL '.slx']);
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7');
GOLDEN_FUEL = 91.2317939047896;

cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));
fprintf('[p456] MATLAB %s\n', version);

run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'P456:WEIGHTS', 'weight injection failed');

R = struct('p4_smoke_pass', false, 'p5_fuel_kg', NaN, 'p5_bitwise', false, ...
    'p6_testlaw7_pass', false, 'pass', false);

%% ---- P4: 10b smoke ----------------------------------------------------------
fprintf('[p456] P4 running 10b smoke ...\n');
eval(fileread(fullfile(ROOT, 'scripts', '10b_smoke_modelD.m')));   % defines allPass in base
R.p4_smoke_pass = evalin('base', 'exist(''allPass'', ''var'') == 1 && allPass');
fprintf('[p456] P4 smoke: %s\n', string(R.p4_smoke_pass));
assert(R.p4_smoke_pass, 'P456:P4', 'smoke failed');

%% ---- P5: product-level law=7 S01 golden check -------------------------------
fprintf('[p456] P5 product law=7 S01 vs golden fuel ...\n');
if bdIsLoaded(MDL), close_system(MDL, 0); end
assignin('base', 'EMS_CTRL_LAW', 7);
load_system(slx);
set_param(MDL, 'SimulationCommand', 'update');
SCN = evalin('base', 'SCENARIOS(1)');
stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
out = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
ds = model.extract_logsout(out);
R.p5_fuel_kg = ds.fuel_total_kg(end);
R.p5_bitwise = (R.p5_fuel_kg == GOLDEN_FUEL);
fprintf('[p456] P5 fuel=%.17g | bitwise==golden: %s\n', R.p5_fuel_kg, string(R.p5_bitwise));
close_system(MDL, 0);
assert(R.p5_bitwise, 'P456:P5', 'fuel mismatch vs golden (%.17g)', GOLDEN_FUEL);

%% ---- P6: test_law7 full suite -----------------------------------------------
fprintf('[p456] P6 running ecms2.test_law7 (T1/T2/T3) ...\n');
r7 = ecms2.test_law7(ROOT);
R.p6_testlaw7_pass = r7.pass;
fprintf('[p456] P6 test_law7: %s\n', string(r7.pass));

%% ---- verdict -----------------------------------------------------------------
R.pass = R.p4_smoke_pass && R.p5_bitwise && R.p6_testlaw7_pass;
outJ = struct();
outJ.timestamp = char(datetime('now','Format','yyyy-MM-dd HH:mm:ss'));
outJ.matlab_version = version;
outJ.model_file_md5 = model.md5file(slx);
outJ.golden_fuel_kg = GOLDEN_FUEL;
outJ.result = R;
jPath = fullfile(outDir, 'product_switch_verification.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(outJ), 'char');
fclose(fid);

fprintf('\n===== PRODUCT SWITCH VERIFICATION (P4-P6) =====\n');
fprintf('P4 10b smoke                    : %s\n', string(R.p4_smoke_pass));
fprintf('P5 S01 fuel bitwise==golden     : %s (%.17g)\n', string(R.p5_bitwise), R.p5_fuel_kg);
fprintf('P6 test_law7 T1/T2/T3           : %s\n', string(R.p6_testlaw7_pass));
fprintf('OVERALL                         : %s\n', string(R.pass));
fprintf('[p456] JSON written: %s\n', jPath);
fprintf('[p456] DONE\n');

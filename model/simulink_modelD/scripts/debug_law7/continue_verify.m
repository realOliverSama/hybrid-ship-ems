%% ========================================================================
%  continue_verify.m — resume product-switch verification from P3.
%  The rebuild (P2) already completed and saved the new .slx before the
%  previous run timed out. This script:
%    P3' structural diff vs the PRE-SWITCH model extracted from git HEAD
%    P4  10b smoke (3 scenarios)
%    P5  product law=7 S01 closed loop; fuel bitwise == golden
%        91.2317939047896 kg
%    P6  ecms2.test_law7 full suite (T1/T2/T3)
%% ========================================================================
clearvars; close all; clc;
ROOT = 'E:/User/Tommy/Desktop/hybrid-ship-ems/model/simulink_modelD';
REPO = 'E:/User/Tommy/Desktop/hybrid-ship-ems';
MDL  = 'HybridShip_ModelD_DCBus';
slx  = fullfile(ROOT, 'model', [MDL '.slx']);
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7');
GOLDEN_FUEL = 91.2317939047896;

cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));
fprintf('[cv] MATLAB %s\n', version);

run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'CV:WEIGHTS', 'weight injection failed');

R = struct('p3_struct_ok', false, 'p3_diff', {''}, 'p4_smoke_pass', false, ...
    'p5_fuel_kg', NaN, 'p5_bitwise', false, 'p6_testlaw7_pass', false, 'pass', false);

%% ---- P3': structural diff vs git HEAD (pre-switch) model -------------------
oldSlx = fullfile(tempdir, [MDL '_pre_inline_head.slx']);
cmd = sprintf('git -c core.quotepath=false show HEAD:"model/simulink_modelD/model/%s.slx" > "%s"', MDL, oldSlx);
[st, out] = system(cmd);
assert(st == 0, 'CV:P3', 'git extract failed: %s', out);

mapOld = snapshotCharts(oldSlx);
mapNew = snapshotCharts(slx);
diffList = {};
allKeys = union(keys(mapOld), keys(mapNew));
for i = 1:numel(allKeys)
    k = allKeys{i};
    if isKey(mapOld, k) && isKey(mapNew, k)
        if ~strcmp(mapOld(k), mapNew(k))
            diffList{end+1} = ['CHANGED ' k];      %#ok<*AGROW>
        end
    elseif isKey(mapOld, k)
        diffList{end+1} = ['REMOVED ' k];
    else
        diffList{end+1} = ['ADDED   ' k];
    end
end
fprintf('[cv] P3 diff entries: %d\n', numel(diffList));
for i = 1:numel(diffList), fprintf('       %s\n', diffList{i}); end
suffix = '/05_EMS_Controller/EMS_NN_AECMS/ecms_step';
p3ok = ~isempty(diffList) && numel(diffList) == 1 ...
    && ~isempty(strfind(diffList{1}, suffix)) && strncmp(diffList{1}, 'CHANGED', 7);
R.p3_struct_ok = p3ok;
R.p3_diff = diffList;
fprintf('[cv] P3 structural check: %s\n', string(p3ok));
delete(oldSlx);
assert(p3ok, 'CV:P3', 'unexpected structural changes beyond law=7 ecms_step');

%% ---- P4: 10b smoke ----------------------------------------------------------
fprintf('[cv] P4 running 10b smoke ...\n');
eval(fileread(fullfile(ROOT, 'scripts', '10b_smoke_modelD.m')));   % defines allPass in base
R.p4_smoke_pass = evalin('base', 'exist(''allPass'', ''var'') == 1 && allPass');
fprintf('[cv] P4 smoke: %s\n', string(R.p4_smoke_pass));
assert(R.p4_smoke_pass, 'CV:P4', 'smoke failed');

%% ---- P5: product-level law=7 S01 golden check -------------------------------
fprintf('[cv] P5 product law=7 S01 vs golden fuel ...\n');
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
fprintf('[cv] P5 fuel=%.17g | bitwise==golden: %s\n', R.p5_fuel_kg, string(R.p5_bitwise));
close_system(MDL, 0);
assert(R.p5_bitwise, 'CV:P5', 'fuel mismatch vs golden (%.17g)', GOLDEN_FUEL);

%% ---- P6: test_law7 full suite -----------------------------------------------
fprintf('[cv] P6 running ecms2.test_law7 (T1/T2/T3) ...\n');
r7 = ecms2.test_law7(ROOT);
R.p6_testlaw7_pass = r7.pass;
fprintf('[cv] P6 test_law7: %s\n', string(r7.pass));

%% ---- verdict -----------------------------------------------------------------
R.pass = R.p3_struct_ok && R.p4_smoke_pass && R.p5_bitwise && R.p6_testlaw7_pass;
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

fprintf('\n===== PRODUCT SWITCH VERIFICATION (resumed) =====\n');
fprintf('P3 structure (only law=7 changed): %s\n', string(R.p3_struct_ok));
fprintf('P4 10b smoke                    : %s\n', string(R.p4_smoke_pass));
fprintf('P5 S01 fuel bitwise==golden     : %s (%.17g)\n', string(R.p5_bitwise), R.p5_fuel_kg);
fprintf('P6 test_law7 T1/T2/T3           : %s\n', string(R.p6_testlaw7_pass));
fprintf('OVERALL                         : %s\n', string(R.pass));
fprintf('[cv] JSON written: %s\n', jPath);
fprintf('[cv] DONE\n');

%% ============================ local functions ==============================
function map = snapshotCharts(slxPath)
    MDL = 'HybridShip_ModelD_DCBus';
    try, bdclose('all'); catch, end   % purge ALL loaded models & stale SF machines
    load_system(slxPath);
    chs = sfroot().find('-isa', 'Stateflow.EMChart', '-and', 'Model', MDL);
    map = containers.Map('KeyType', 'char', 'ValueType', 'char');
    for i = 1:numel(chs)
        h = sprintf('%02X', typecast(sum(double(uint16(chs(i).Script))) + numel(chs(i).Script), 'uint64'));
        map(chs(i).Path) = h;
    end
    fprintf('[snapshot] %s : %d EMCharts\n', slxPath, numel(chs));
    close_system(MDL, 0);
end

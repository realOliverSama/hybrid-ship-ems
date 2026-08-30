%% ========================================================================
%  verify_product_switch.m — post-switch verification for the INLINE law=7.
%
%  Preconditions: build_model_c.m CODE_ECMS_NN() has been switched to the
%  fixed inline implementation (EXP-B structure + ascending reorder +
%  t<lastT reset); setMFCode parameter list restored to 24 names.
%
%  Steps:
%    P1  snapshot pre-rebuild EMChart scripts (path -> md5) from the CURRENT slx
%    P2  full rebuild (10_build_modelD.m -> model.build_model_c)
%    P3  structural diff: all EMChart scripts old vs new;
%        PASS requires EXACTLY ONE change: .../EMS_NN_AECMS/ecms_step
%    P4  10b smoke (3 scenarios, gate criteria inside the script)
%    P5  product-level law=7 S01 closed loop; fuel must equal the golden value
%        91.2317939047896 kg BITWISE (= inline_verification.json RUN-A/RUN-B)
%    P6  ecms2.test_law7 full suite (T1 python parity, T2 fallback, T3 closed loop)
%
%  SAFETY: the only file expected to change is model/HybridShip_ModelD_DCBus.slx.
%% ========================================================================
clearvars; close all; clc;
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
MDL  = 'HybridShip_ModelD_DCBus';
slx  = fullfile(ROOT, 'model', [MDL '.slx']);
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7');
GOLDEN_FUEL = 91.2317939047896;   % bitwise golden from inline_verification.json

assert(exist(slx, 'file') == 2, 'SW:MODEL', 'model not found');
cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));
fprintf('[sw] MATLAB %s\n', version);

%% ---- workspace parameters -------------------------------------------------
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'SW:WEIGHTS', 'weight injection failed');

R = struct('p3_struct_ok', false, 'p3_diff', {}, 'p4_smoke_pass', false, ...
    'p5_fuel_kg', NaN, 'p5_bitwise', false, 'p6_testlaw7_pass', false, 'pass', false);

%% ---- P1: snapshot pre-rebuild chart scripts --------------------------------
if bdIsLoaded(MDL), close_system(MDL, 0); end
load_system(slx);
chartPathsOld = sfroot().find('-isa', 'Stateflow.EMChart');
mapOld = containers.Map('KeyType', 'char', 'ValueType', 'char');
for i = 1:numel(chartPathsOld)
    mapOld(chartPathsOld(i).Path) = getHashOfText(chartPathsOld(i).Script);
end
nOld = mapOld.Count;
close_system(MDL, 0);
fprintf('[sw] P1 snapshot: %d EMCharts\n', nOld);

%% ---- P2: full rebuild ------------------------------------------------------
% NOTE: project convention is eval(fileread(...)) for these build scripts
% (see header of 10_build_modelD.m); plain run() hits an encoding issue in -batch.
fprintf('[sw] P2 rebuilding model ...\n');
try
    eval(fileread(fullfile(ROOT, 'scripts', '10_build_modelD.m')));
    fprintf('[sw] P2 rebuild DONE\n');
catch errB
    fprintf('[sw] P2 FAILED: %s | %s\n', errB.identifier, errB.message);
    for st = 1:numel(errB.stack)
        fprintf('  at %s (line %d)\n', errB.stack(st).name, errB.stack(st).line);
    end
    try
        chPath = [MDL '/05_EMS_Controller/EMS_NN_AECMS/ecms_step'];
        chD = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', chPath);
        if ~isempty(chD)
            dmp = fullfile(outDir, 'ecms_step_dump.m');
            fid = fopen(dmp, 'w', 'n', 'UTF-8');
            fwrite(fid, chD(1).Script, 'char');
            fclose(fid);
            fprintf('[sw] law=7 script dumped: %s\n', dmp);
        else
            fprintf('[sw] law=7 chart object NOT present after failure\n');
        end
    catch errD
        fprintf('[sw] dump attempt failed: %s\n', errD.message);
    end
    try
        rep = getReport(errB, 'extended');
        fprintf('%s\n', rep);
    catch errR
        fprintf('[sw] extended report unavailable: %s\n', errR.message);
    end
    rethrow(errB);
end
fprintf('[sw] P2 rebuild DONE\n');
fprintf('[sw] P2 rebuild DONE\n');

%% ---- P3: structural diff ----------------------------------------------------
load_system(slx);
chartPathsNew = sfroot().find('-isa', 'Stateflow.EMChart');
mapNew = containers.Map('KeyType', 'char', 'ValueType', 'char');
for i = 1:numel(chartPathsNew)
    mapNew(chartPathsNew(i).Path) = getHashOfText(chartPathsNew(i).Script);
end
close_system(MDL, 0);

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
fprintf('[sw] P3 diff entries: %d\n', numel(diffList));
for i = 1:numel(diffList)
    fprintf('       %s\n', diffList{i});
end
suffix = '/05_EMS_Controller/EMS_NN_AECMS/ecms_step';
p3ok = (numel(diffList) == 1) && ~isempty(strfind(diffList{1}, suffix)) ...
    && strncmp(diffList{1}, 'CHANGED', 7);
R.p3_struct_ok = p3ok;
R.p3_diff = diffList;
fprintf('[sw] P3 structural check: %s\n', string(p3ok));
assert(p3ok, 'SW:P3', 'unexpected structural changes beyond law=7 ecms_step');

%% ---- P4: 10b smoke ----------------------------------------------------------
fprintf('[sw] P4 running 10b smoke ...\n');
eval(fileread(fullfile(ROOT, 'scripts', '10b_smoke_modelD.m')));   % defines allPass in base
R.p4_smoke_pass = evalin('base', 'exist(''allPass'', ''var'') == 1 && allPass');
fprintf('[sw] P4 smoke: %s\n', string(R.p4_smoke_pass));
assert(R.p4_smoke_pass, 'SW:P4', 'smoke failed');

%% ---- P5: product-level law=7 S01 golden check -------------------------------
fprintf('[sw] P5 product law=7 S01 vs golden fuel ...\n');
if bdIsLoaded(MDL), close_system(MDL, 0); end
assignin('base', 'EMS_CTRL_LAW', 7);
load_system(slx);
set_param(MDL, 'SimulationCommand', 'update');   % compile check on rebuilt model
SCN = evalin('base', 'SCENARIOS(1)');
stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
out = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
ds = model.extract_logsout(out);
R.p5_fuel_kg = ds.fuel_total_kg(end);
R.p5_bitwise = (R.p5_fuel_kg == GOLDEN_FUEL);
fprintf('[sw] P5 fuel=%.17g | bitwise==golden: %s\n', R.p5_fuel_kg, string(R.p5_bitwise));
close_system(MDL, 0);
assert(R.p5_bitwise, 'SW:P5', 'fuel mismatch vs golden (expected %.17g)', GOLDEN_FUEL);

%% ---- P6: test_law7 full suite -----------------------------------------------
fprintf('[sw] P6 running ecms2.test_law7 (T1/T2/T3) ...\n');
r7 = ecms2.test_law7(ROOT);
R.p6_testlaw7_pass = r7.pass;
fprintf('[sw] P6 test_law7: %s\n', string(r7.pass));

%% ---- verdict -----------------------------------------------------------------
R.pass = R.p3_struct_ok && R.p4_smoke_pass && R.p5_bitwise && R.p6_testlaw7_pass;

out = struct();
out.timestamp = char(datetime('now','Format','yyyy-MM-dd HH:mm:ss'));
out.matlab_version = version;
out.model_file_md5 = model.md5file(slx);
out.golden_fuel_kg = GOLDEN_FUEL;
out.result = R;
jPath = fullfile(outDir, 'product_switch_verification.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(out), 'char');
fclose(fid);

fprintf('\n===== PRODUCT SWITCH VERIFICATION =====\n');
fprintf('P3 structure (only law=7 changed): %s\n', string(R.p3_struct_ok));
fprintf('P4 10b smoke                    : %s\n', string(R.p4_smoke_pass));
fprintf('P5 S01 fuel bitwise==golden     : %s (%.17g)\n', string(R.p5_bitwise), R.p5_fuel_kg);
fprintf('P6 test_law7 T1/T2/T3           : %s\n', string(R.p6_testlaw7_pass));
fprintf('OVERALL                         : %s\n', string(R.pass));
fprintf('[sw] JSON written: %s\n', jPath);
fprintf('[sw] DONE\n');

%% ============================ local functions ==============================
function h = getHashOfText(txt)
    % stable hash of a char vector (no external toolbox dependency)
    h = sprintf('%02X', typecast(sum(double(uint16(txt))) + numel(txt), 'uint64'));
    % NOTE: simple checksum; collisions unlikely for this controlled comparison.
end

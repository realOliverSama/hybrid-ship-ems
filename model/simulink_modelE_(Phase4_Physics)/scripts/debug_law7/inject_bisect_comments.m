%% ========================================================================
%  inject_bisect_comments.m — is the non-ASCII comment content the trigger?
%
%  Facts so far (same chart, same injection method, same update):
%    RUN-A  strjoin text, English-only comments          -> COMPILES OK
%    GEN    sprintf text, Chinese/Unicode comments       -> ErrorsParsingEmlFcn
%    Code logic of both is identical.
%
%  Variants under test (both derived from the failing generated text):
%    G1_STRIP : remove every comment-only line entirely
%    G2_ASCII : keep all lines but strip every non-ASCII char from them
%% ========================================================================
clearvars; close all; clc;
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
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
assert(sNN.ok == 1, 'BI:WEIGHTS', 'weights missing');

codeText = fileread(genFile);
lines = regexp(codeText, '\r?\n', 'split');

% G1: drop comment-only lines (they all start with optional spaces then '%')
g1 = lines(~strncmp(strtrim(lines), '%', 1));
codeG1 = strjoin(g1, newline);

% G2: keep every line, delete non-ASCII characters anywhere
g2 = regexprep(lines, '[^\x00-\x7F]', '');
codeG2 = strjoin(g2, newline);

fprintf('[bi] G1 %d chars / %d lines | G2 %d chars / %d lines\n', ...
    numel(codeG1), numel(g1), numel(codeG2), numel(g2));

paramNames = {'ASSUMED_eta_rect','ASSUMED_eta_dcdc','ASSUMED_Bat_Eff_Discharge', ...
    'ASSUMED_Bat_Eff_Charge','Gen_P_max','ECMS_SOC_Discharge_Max','ECMS_SOC_Charge_Min', ...
    'ECMS_Idle_Fuel_kgph','ECMS_Grid_dP_kW','ECMS_dt_ctrl_s','Pgen_init', ...
    'ECMS_NN_W1','ECMS_NN_b1','ECMS_NN_W2','ECMS_NN_b2','ECMS_NN_W3','ECMS_NN_b3', ...
    'ECMS_NN_XMean','ECMS_NN_XStd','ECMS_NN_YMean','ECMS_NN_YStd', ...
    'ECMS_NN_Ebar_H_kWh','ECMS_NN_Escale_kWh','ECMS_NN_WeightsOK'};

res1 = tryInject('G1_strip_comments', codeG1, MDL, slx, paramNames);
res2 = tryInject('G2_ascii_only',     codeG2, MDL, slx, paramNames);

outJ = struct();
outJ.timestamp = char(datetime('now','Format','yyyy-MM-dd HH:mm:ss'));
outJ.g1 = res1;
outJ.g2 = res2;
jPath = fullfile(outDir, 'inject_bisect_result.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(outJ), 'char');
fclose(fid);
fprintf('\n[bi] G1 pass=%s | G2 pass=%s\n', string(res1.pass), string(res2.pass));
fprintf('[bi] JSON written: %s\n', jPath);
fprintf('[bi] DONE\n');

%% ============================ local functions ==============================
function res = tryInject(name, codeText, MDL, slxPath, paramNames)
    res = struct('name', name, 'pass', false, 'identifier', '', 'message', '');
    fprintf('\n[bi] >>> %s\n', name);
    try
        assignin('base', 'EMS_CTRL_LAW', 7);
        if bdIsLoaded(MDL), close_system(MDL, 0); end
        load_system(slxPath);
        chPath = [MDL '/05_EMS_Controller/EMS_NN_AECMS/ecms_step'];
        ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', chPath);
        assert(~isempty(ch), 'BI:CHART', 'law=7 chart not found');
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
        res.pass = true;
        fprintf('[bi] %s COMPILE OK\n', name);
    catch err
        res.identifier = err.identifier;
        res.message = err.message;
        fprintf('[bi] %s FAILED - %s\n', name, err.identifier);
    end
    try
        set_param(MDL, 'SimulationCommand', 'stop');
    catch
    end
    try
        close_system(MDL, 0);
    catch
    end
end

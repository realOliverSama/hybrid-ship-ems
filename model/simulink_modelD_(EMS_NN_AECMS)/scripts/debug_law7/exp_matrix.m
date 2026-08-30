%% ========================================================================
%  exp_matrix.m — law=7 inline MLP forward Stateflow EML parse failure bisect
%  Branch: debug/MATLAB-debug | Designed by main agent | One MATLAB launch runs all.
%
%  Experiments (single-factor isolation, real model only):
%    EXP-0a  wip inline code as-is                    -> expect FAIL (baseline)
%    EXP-0b  same but forward replaced by y_norm=0    -> expect PASS (control)
%    EXP-1   wip code + full explicit paramSizes      -> ? (size-inference suspect)
%    EXP-2   pre-transposed weights, no .' no (:)     -> ? (transpose/broadcast suspect)
%    EXP-3a  forward keeps only xnorm line            -> minimal failing statement set
%    EXP-3b  forward keeps xnorm+h1 lines
%    EXP-3c  forward keeps xnorm+h1+h2 lines
%
%  SAFETY: all edits in memory; close_system(MDL, 0) NEVER saves the .slx.
%  Run:
%    matlab -batch "run(fullfile(ROOT,'scripts','debug_law7','exp_matrix.m'))"
% ========================================================================
clearvars; close all; clc;
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
MDL    = 'HybridShip_ModelD_DCBus';
slx    = fullfile(ROOT, 'model', [MDL '.slx']);
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7');

assert(exist(slx, 'file') == 2, 'EXP:MODEL', 'model not found: %s', slx);
if ~exist(outDir, 'dir'), mkdir(outDir); end
cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));

fprintf('[exp] MATLAB %s\n', version);

%% ---- workspace parameters -------------------------------------------------
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);

matFile = fullfile(ROOT, 'config', 'ecms_nn_weights.mat');
if exist(matFile, 'file') ~= 2
    fprintf('[exp] weights mat missing -> rebuild from JSON (52_integrate_law7 logic)\n');
    wJson = fullfile(ROOT, 'results', 'ecms_phase2', 'models', 'selected_model_weights.json');
    kJson = fullfile(ROOT, 'results', 'ecms_phase1', 'kS_calibration.json');
    WJ = jsondecode(fileread(wJson));
    KJ = jsondecode(fileread(kJson));
    W1j = WJ.weights.W1;  b1j = WJ.weights.b1(:).';
    W2j = WJ.weights.W2;  b2j = WJ.weights.b2(:).';
    W3j = WJ.weights.W3;  b3j = WJ.weights.b3;
    Xmj = WJ.norm.X_mean(:);  Xsj = WJ.norm.X_std(:);
    Ymj = WJ.norm.y_mean;     Ysj = WJ.norm.y_std;
    Ebj = KJ.oracle.E_bar_H_kWh;  Esj = KJ.oracle.E_scale_kWh;
    assert(isequal(size(W1j),[302 128]) && isequal(size(W2j),[128 64]) && isequal(size(W3j),[64 1]), ...
        'EXP:WEIGHTS', 'unexpected weight shapes');
    Sj = struct('ECMS_NN_W1',W1j,'ECMS_NN_b1',b1j,'ECMS_NN_W2',W2j,'ECMS_NN_b2',b2j, ...
        'ECMS_NN_W3',W3j,'ECMS_NN_b3',b3j,'ECMS_NN_XMean',Xmj,'ECMS_NN_XStd',Xsj, ...
        'ECMS_NN_YMean',Ymj,'ECMS_NN_YStd',Ysj,'ECMS_NN_Ebar_H_kWh',Ebj, ...
        'ECMS_NN_Escale_kWh',Esj,'ECMS_NN_WeightsOK',1);
    save(matFile, '-struct', 'Sj', '-v7');
    clear Sj W1j b1j W2j b2j W3j b3j Xmj Xsj Ymj Ysj Ebj Esj;
end
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'EXP:WEIGHTS', 'weight injection failed');

% EXP-2 pre-transposed weight variants (mathematically equivalent, removes .' and (:))
% NOTE: top-level script variables live in base, so direct assignment is enough
% (avoids quote-escaping traps inside evalin strings).
ECMS_NN_WT1 = ECMS_NN_W1.';                 % [128 302]
ECMS_NN_WT2 = ECMS_NN_W2.';                 % [64 128]
ECMS_NN_WT3 = ECMS_NN_W3.';                 % [1 64]
ECMS_NN_b1c = reshape(ECMS_NN_b1.', [], 1); % [128 1]
ECMS_NN_b2c = reshape(ECMS_NN_b2.', [], 1); % [64 1]
WT1 = ECMS_NN_WT1; WT2 = ECMS_NN_WT2; WT3 = ECMS_NN_WT3;
b1c = ECMS_NN_b1c; b2c = ECMS_NN_b2c;
fprintf('[exp] WT shapes: WT1 %s WT2 %s WT3 %s b1c %s b2c %s\n', ...
    mat2str(size(WT1)), mat2str(size(WT2)), mat2str(size(WT3)), mat2str(size(b1c)), mat2str(size(b2c)));

%% ---- code text builders ---------------------------------------------------
fwdFull = ecms2.nn_forward_code();   % current repo version: W1.'*xnorm form

codeFull = build_body(fwdFull, false);
codeZero = build_body('y_norm = 0;', false);
codeNoTr = build_body(sprintf([ ...
    'xnorm = (x - XMean) ./ XStd;\n' ...
    'h1 = max(WT1 * xnorm + b1c, 0);\n' ...
    'h2 = max(WT2 * h1 + b2c, 0);\n' ...
    'y_norm = WT3 * h2 + b3;\n']), true);
code3a = build_body(sprintf(['xnorm = (x - XMean(:)) ./ XStd(:);\n' 'y_norm = 0;\n']), false);
code3b = build_body(sprintf([ ...
    'xnorm = (x - XMean(:)) ./ XStd(:);\n' ...
    'h1 = max(W1.'' * xnorm + b1(:), 0);\n' ...
    'y_norm = 0;\n']), false);
code3c = build_body(sprintf([ ...
    'xnorm = (x - XMean(:)) ./ XStd(:);\n' ...
    'h1 = max(W1.'' * xnorm + b1(:), 0);\n' ...
    'h2 = max(W2.'' * h1 + b2(:), 0);\n' ...
    'y_norm = 0;\n']), false);

%% ---- parameter name/size sets --------------------------------------------
namesScalar = {'ASSUMED_eta_rect','ASSUMED_eta_dcdc','ASSUMED_Bat_Eff_Discharge', ...
    'ASSUMED_Bat_Eff_Charge','Gen_P_max','ECMS_SOC_Discharge_Max','ECMS_SOC_Charge_Min', ...
    'ECMS_Idle_Fuel_kgph','ECMS_Grid_dP_kW','ECMS_dt_ctrl_s','Pgen_init'};
namesNN = {'ECMS_NN_W1','ECMS_NN_b1','ECMS_NN_W2','ECMS_NN_b2','ECMS_NN_W3','ECMS_NN_b3', ...
    'ECMS_NN_XMean','ECMS_NN_XStd','ECMS_NN_YMean','ECMS_NN_YStd', ...
    'ECMS_NN_Ebar_H_kWh','ECMS_NN_Escale_kWh','ECMS_NN_WeightsOK'};
sizesNN = {'[302 128]','[1 128]','[128 64]','[1 64]','[64 1]','[1 1]', ...
    '[302 1]','[302 1]','[1 1]','[1 1]','[1 1]','[1 1]','[1 1]'};

names24 = [namesScalar, namesNN];
sizesEmpty = cell(1, numel(names24));                 % all empty -> inherit
sizesExplicit = [repmat({'[1 1]'}, 1, numel(namesScalar)), sizesNN];

names29 = [names24, {'ECMS_NN_WT1','ECMS_NN_b1c','ECMS_NN_WT2','ECMS_NN_b2c','ECMS_NN_WT3'}];
sizesExplicit29 = [sizesExplicit, {'[128 302]','[128 1]','[64 128]','[64 1]','[1 64]'}];

%% ---- experiment matrix ----------------------------------------------------
results = struct('id',{},'pass',{},'identifier',{},'message',{});
results(end+1) = run_exp('EXP0a_wip_baseline',   codeFull, names24, sizesEmpty, slx);     %#ok<*SAGROW>
print_one(results(end));
results(end+1) = run_exp('EXP0b_zerofwd_control',codeZero, names24, sizesEmpty, slx);
print_one(results(end));
resExp1 = run_exp('EXP1_explicit_sizes',         codeFull, names24, sizesExplicit, slx);
results(end+1) = resExp1;
print_one(results(end));
results(end+1) = run_exp('EXP2_notrans',         codeNoTr, names29, sizesExplicit29, slx);
print_one(results(end));

if resExp1.pass
    sizes3 = sizesExplicit;   % keep size factor fixed at the passing config
else
    sizes3 = sizesEmpty;      % stay on the failing baseline config
end
results(end+1) = run_exp('EXP3a_fwd_xnorm_only', code3a, names24, sizes3, slx);
print_one(results(end));
results(end+1) = run_exp('EXP3b_fwd_xnorm_h1',   code3b, names24, sizes3, slx);
print_one(results(end));
results(end+1) = run_exp('EXP3c_fwd_xnorm_h1_h2',code3c, names24, sizes3, slx);
print_one(results(end));

%% ---- machine-readable output ----------------------------------------------
try, mdlMd5 = model.md5file(slx); catch, mdlMd5 = 'unavailable'; end
out = struct();
out.timestamp = char(datetime('now','Format','yyyy-MM-dd HH:mm:ss'));
out.matlab_version = version;
out.model_file_md5 = mdlMd5;
out.experiments = results;
jPath = fullfile(outDir, 'exp_matrix_results.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(out), 'char');
fclose(fid);

fprintf('\n===== SUMMARY =====\n');
for i = 1:numel(results)
    print_one(results(i));
end
fprintf('\n[exp] JSON written: %s\n', jPath);
fprintf('[exp] DONE\n');

%% ============================ local functions ==============================
function print_one(r)
    msg = strrep(r.message, newline, ' ');
    if numel(msg) > 80, msg = [msg(1:80) '...']; end
    fprintf('%-22s %-5s %-42s %s\n', r.id, string(r.pass), r.identifier, msg);
end

function body = build_body(fwdText, notransAlias)
% Full wip inline law=7 function body (comments stripped; logic identical to
% git show c0e83863:model/simulink_modelD/scripts/+model/build_model_c.m CODE_ECMS_NN).
% notransAlias=true switches the alias lines to the pre-transposed set.
    L = {};
    L{end+1} = 'function P = ecms_nn(t, P_load, v_ship, SOC, lambda, K_soc, SOC_ref, K_S, Lambda_Floor, bsfc_bp, bsfc_tbl)';
    L{end+1} = 'persistent lastT Pheld buf k vprev';
    L{end+1} = 'if isempty(lastT)';
    L{end+1} = '    lastT = -1e9;';
    L{end+1} = '    Pheld = Pgen_init;';
    L{end+1} = '    k = 1;';
    L{end+1} = '    buf = zeros(300, 1);';
    L{end+1} = '    buf(:) = P_load;';
    L{end+1} = '    vprev = v_ship;';
    L{end+1} = 'end';
    if notransAlias
        L{end+1} = 'W1 = ECMS_NN_W1; b1 = ECMS_NN_b1; W2 = ECMS_NN_W2; b2 = ECMS_NN_b2;';
        L{end+1} = 'W3 = ECMS_NN_W3; b3 = ECMS_NN_b3;';
        L{end+1} = 'XMean = ECMS_NN_XMean; XStd = ECMS_NN_XStd;';
        L{end+1} = 'YMean = ECMS_NN_YMean; YStd = ECMS_NN_YStd;';
        L{end+1} = 'Ebar = ECMS_NN_Ebar_H_kWh; Escale = ECMS_NN_Escale_kWh;';
        L{end+1} = 'nn_ok = ECMS_NN_WeightsOK;';
        L{end+1} = 'WT1 = ECMS_NN_WT1; b1c = ECMS_NN_b1c; WT2 = ECMS_NN_WT2; b2c = ECMS_NN_b2c; WT3 = ECMS_NN_WT3;';
    else
        L{end+1} = 'W1 = ECMS_NN_W1; b1 = ECMS_NN_b1; W2 = ECMS_NN_W2; b2 = ECMS_NN_b2;';
        L{end+1} = 'W3 = ECMS_NN_W3; b3 = ECMS_NN_b3;';
        L{end+1} = 'XMean = ECMS_NN_XMean; XStd = ECMS_NN_XStd;';
        L{end+1} = 'YMean = ECMS_NN_YMean; YStd = ECMS_NN_YStd;';
        L{end+1} = 'Ebar = ECMS_NN_Ebar_H_kWh; Escale = ECMS_NN_Escale_kWh;';
        L{end+1} = 'nn_ok = ECMS_NN_WeightsOK;';
    end
    L{end+1} = 'P = Pheld;';
    L{end+1} = 'if t - lastT < ECMS_dt_ctrl_s - 1e-9';
    L{end+1} = '    return;';
    L{end+1} = 'end';
    L{end+1} = 'lastT = t;';
    L{end+1} = 'if isfinite(P_load)';
    L{end+1} = '    buf(k) = P_load;';
    L{end+1} = 'end';
    L{end+1} = 'k = mod(k, 300) + 1;';
    L{end+1} = 'accel = v_ship - vprev;';
    L{end+1} = 'vprev = v_ship;';
    L{end+1} = 'ok = nn_ok > 0.5 && isfinite(P_load) && isfinite(v_ship) && isfinite(accel);';
    L{end+1} = 'if ok';
    L{end+1} = '    xP = zeros(300, 1);';
    L{end+1} = '    for j = 1:300';
    L{end+1} = '        i = k - j;';
    L{end+1} = '        if i < 1';
    L{end+1} = '            i = i + 300;';
    L{end+1} = '        end';
    L{end+1} = '        xP(j) = buf(i);';
    L{end+1} = '    end';
    L{end+1} = '    x = [xP; v_ship; accel];';
    L{end+1} = '    ok = all(isfinite(x));';
    L{end+1} = 'end';
    L{end+1} = 'if ok';
    L{end+1} = fwdText;   % may be multi-line already ending with \n
    L{end+1} = '    Ehat_kWh = y_norm * YStd + YMean;';
    L{end+1} = '    e_hat = (Ehat_kWh - Ebar) / Escale;';
    L{end+1} = 'else';
    L{end+1} = '    e_hat = 0;';
    L{end+1} = 'end';
    L{end+1} = 'soc_ref_eff = SOC_ref + K_S * e_hat;';
    L{end+1} = 'lam = lambda + K_soc * (soc_ref_eff - SOC);';
    L{end+1} = 'if K_S ~= 0';
    L{end+1} = '    lam = max(lam, Lambda_Floor);';
    L{end+1} = 'end';
    L{end+1} = 'N = floor(Gen_P_max / ECMS_Grid_dP_kW) + 1;';
    L{end+1} = 'Pbest = 0;';
    L{end+1} = 'Jbest = inf;';
    L{end+1} = 'nb = numel(bsfc_bp);';
    L{end+1} = 'for i = 1:N';
    L{end+1} = '    Pg = (i - 1) * ECMS_Grid_dP_kW;';
    L{end+1} = '    Pbat = P_load - ASSUMED_eta_rect * Pg;';
    L{end+1} = '    if SOC <= ECMS_SOC_Discharge_Max';
    L{end+1} = '        if Pbat > 1e-9';
    L{end+1} = '            continue;';
    L{end+1} = '        end';
    L{end+1} = '        Pbest = Pg;';
    L{end+1} = '        break;';
    L{end+1} = '    end';
    L{end+1} = '    if SOC >= ECMS_SOC_Charge_Min && Pbat < 0';
    L{end+1} = '        continue;';
    L{end+1} = '    end';
    L{end+1} = '    if Pg < 1e-9';
    L{end+1} = '        mf = ECMS_Idle_Fuel_kgph / 3600;';
    L{end+1} = '    else';
    L{end+1} = '        bsfc = bsfc_tbl(1);';
    L{end+1} = '        if Pg >= bsfc_bp(nb)';
    L{end+1} = '            bsfc = bsfc_tbl(nb);';
    L{end+1} = '        elseif Pg > bsfc_bp(1)';
    L{end+1} = '            ii = 1;';
    L{end+1} = '            while ii < nb - 1 && bsfc_bp(ii + 1) < Pg';
    L{end+1} = '                ii = ii + 1;';
    L{end+1} = '            end';
    L{end+1} = '            w = (Pg - bsfc_bp(ii)) / (bsfc_bp(ii + 1) - bsfc_bp(ii));';
    L{end+1} = '            bsfc = bsfc_tbl(ii) + w * (bsfc_tbl(ii + 1) - bsfc_tbl(ii));';
    L{end+1} = '        end';
    L{end+1} = '        mf = Pg * bsfc / 3.6e6;';
    L{end+1} = '    end';
    L{end+1} = '    if Pbat > 0';
    L{end+1} = '        mbe = Pbat / (ASSUMED_eta_dcdc * ASSUMED_Bat_Eff_Discharge) / 3600;';
    L{end+1} = '    else';
    L{end+1} = '        mbe = Pbat * (ASSUMED_eta_dcdc * ASSUMED_Bat_Eff_Charge) / 3600;';
    L{end+1} = '    end';
    L{end+1} = '    J = mf + lam * mbe;';
    L{end+1} = '    if J < Jbest';
    L{end+1} = '        Jbest = J;';
    L{end+1} = '        Pbest = Pg;';
    L{end+1} = '    end';
    L{end+1} = 'end';
    L{end+1} = 'if SOC <= ECMS_SOC_Discharge_Max && Pbest < 1e-9 && P_load > ASSUMED_eta_rect * Gen_P_max';
    L{end+1} = '    Pbest = Gen_P_max;';
    L{end+1} = 'end';
    L{end+1} = 'P = Pbest;';
    L{end+1} = 'Pheld = Pbest;';
    L{end+1} = 'end';
    body = strjoin(L, newline);
end

function r = run_exp(id, codeText, paramNames, paramSizes, slxPath)
% Load model fresh (full .slx path - the model dir is NOT on the MATLAB path),
% patch law=7 EMChart in memory, attempt update-diagram compile.
% NEVER saves the model. Returns {id, pass, identifier, message}.
    MDL = 'HybridShip_ModelD_DCBus';
    r = struct('id', id, 'pass', false, 'identifier', '', 'message', '');
    fprintf('\n[exp] >>> %s\n', id);
    try
        assignin('base', 'EMS_CTRL_LAW', 7);
        if bdIsLoaded(MDL)
            close_system(MDL, 0);
        end
        load_system(slxPath);
        chPath = [MDL '/05_EMS_Controller/EMS_NN_AECMS/ecms_step'];
        ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', chPath);
        assert(~isempty(ch), 'EXP:CHART', 'law=7 EMChart not found: %s', chPath);
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
            if i <= numel(paramSizes) && ~isempty(paramSizes{i})
                d.Props.Array.Size = paramSizes{i};
            end
        end
        set_param(MDL, 'SimulationCommand', 'update');
        r.pass = true;
        fprintf('[exp] %s COMPILE OK\n', id);
    catch err
        r.identifier = err.identifier;
        r.message = err.message;
        if numel(r.message) > 500
            r.message = [r.message(1:500) ' ...(truncated)'];
        end
        fprintf('[exp] %s FAILED: %s | %s\n', id, err.identifier, strrep(err.message, newline, ' '));
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

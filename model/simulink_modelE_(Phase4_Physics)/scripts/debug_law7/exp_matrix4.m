%% ========================================================================
%  exp_matrix4.m — law=7 bisect round 4 (final attribution).
%
%  Round-3 facts (exp_matrix3_results.json):
%    R3_EXP_B_x_outside (x assembly moved out of the first if-block, full wip
%    forward kept) COMPILES OK -> viable fix direction.
%    Everything else fails at UPDATE stage, even scalar index y_norm = x(1).
%
%  Round-4 questions:
%    F1: keep x inside the if-block but REMOVE the in-block reassignment of
%        the condition variable (ok = all(isfinite(x))) -> does it compile?
%    F2: move x assembly out AND KEEP an in-block reassignment of ok -> does
%        the in-block ok reassignment alone break it again?
%    MIN: minimal standalone reproduction with ZERO chart parameters and no
%        grid search — proves/disproves a pure R2026a EMChart compiler bug.
%    B2: repeat of the round-3 passing variant B (stability of the fix).
%
%  SAFETY: all edits in memory; close_system(MDL, 0) NEVER saves the .slx.
%% ========================================================================
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

fprintf('[exp4] MATLAB %s\n', version);

%% ---- workspace parameters -------------------------------------------------
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'EXP:WEIGHTS', 'weight injection failed');

%% ---- code variants ---------------------------------------------------------
fwdFull = ecms2.nn_forward_code();
codeF1  = build_body4('f1',  fwdFull);
codeF2  = build_body4('f2',  fwdFull);
codeB   = build_body4('b',   fwdFull);
codeMIN = build_body4('min', '');

%% ---- parameter sets --------------------------------------------------------
namesScalar = {'ASSUMED_eta_rect','ASSUMED_eta_dcdc','ASSUMED_Bat_Eff_Discharge', ...
    'ASSUMED_Bat_Eff_Charge','Gen_P_max','ECMS_SOC_Discharge_Max','ECMS_SOC_Charge_Min', ...
    'ECMS_Idle_Fuel_kgph','ECMS_Grid_dP_kW','ECMS_dt_ctrl_s','Pgen_init'};
namesNN = {'ECMS_NN_W1','ECMS_NN_b1','ECMS_NN_W2','ECMS_NN_b2','ECMS_NN_W3','ECMS_NN_b3', ...
    'ECMS_NN_XMean','ECMS_NN_XStd','ECMS_NN_YMean','ECMS_NN_YStd', ...
    'ECMS_NN_Ebar_H_kWh','ECMS_NN_Escale_kWh','ECMS_NN_WeightsOK'};
namesAll = [namesScalar, namesNN];

%% ---- experiment matrix ----------------------------------------------------
results = struct('id',{},'pass',{},'setter_ok',{},'update_ok',{},'identifier',{},'message',{});
results(end+1) = run_exp4('R4_EXP_F1_no_ok_reassign', codeF1,  namesAll, cell(1,numel(namesAll)), slx);   %#ok<*SAGROW>
print_one(results(end));
results(end+1) = run_exp4('R4_EXP_F2_out_keep_reassign', codeF2, namesAll, cell(1,numel(namesAll)), slx);
print_one(results(end));
results(end+1) = run_exp4('R4_EXP_MIN_repro',   codeMIN, {}, cell(1,0), slx);
print_one(results(end));
results(end+1) = run_exp4('R4_EXP_B2_fix_repeat', codeB, namesAll, cell(1,numel(namesAll)), slx);
print_one(results(end));

%% ---- machine-readable output ----------------------------------------------
try, mdlMd5 = model.md5file(slx); catch, mdlMd5 = 'unavailable'; end
out = struct();
out.timestamp = char(datetime('now','Format','yyyy-MM-dd HH:mm:ss'));
out.matlab_version = version;
out.model_file_md5 = mdlMd5;
out.experiments = results;
jPath = fullfile(outDir, 'exp_matrix4_results.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(out), 'char');
fclose(fid);

fprintf('\n===== SUMMARY (round 4) =====\n');
for i = 1:numel(results)
    print_one(results(i));
end
fprintf('\n[exp4] JSON written: %s\n', jPath);
fprintf('[exp4] DONE\n');

%% ============================ local functions ==============================
function print_one(r)
    msg = strrep(r.message, newline, ' ');
    if numel(msg) > 60, msg = [msg(1:60) '...']; end
    fprintf('%-26s pass=%-5s setter=%-5s update=%-5s %-34s %s\n', ...
        r.id, string(r.pass), string(r.setter_ok), string(r.update_ok), r.identifier, msg);
end

function L = common_head()
    L = {};
    L{end+1} = 'W1 = ECMS_NN_W1; b1 = ECMS_NN_b1; W2 = ECMS_NN_W2; b2 = ECMS_NN_b2;';
    L{end+1} = 'W3 = ECMS_NN_W3; b3 = ECMS_NN_b3;';
    L{end+1} = 'XMean = ECMS_NN_XMean; XStd = ECMS_NN_XStd;';
    L{end+1} = 'YMean = ECMS_NN_YMean; YStd = ECMS_NN_YStd;';
    L{end+1} = 'Ebar = ECMS_NN_Ebar_H_kWh; Escale = ECMS_NN_Escale_kWh;';
    L{end+1} = 'nn_ok = ECMS_NN_WeightsOK;';
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
end

function L = tail_grid()
    L = {};
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
end

function L = head_sig_full()
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
end

function body = build_body4(mode, fwdText)
% mode:
%   f1  : x assembled INSIDE first if-block, NO in-block reassignment of ok,
%         forward block uses x afterwards (tests necessity of ok-reassignment)
%   f2  : x assembled OUTSIDE (unconditional) but the first if-block STILL
%         contains "ok = all(isfinite(x));" (tests sufficiency of reassignment)
%   b   : round-3 passing fix candidate (x unconditional, ok = ok && ...)
%   min : zero-parameter minimal standalone reproduction of the compiler crash
    switch mode
        case 'f1'
            L = head_sig_full();
            L = [L, common_head()];
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
            L{end+1} = 'end';
            L{end+1} = 'if ok';
            L{end+1} = fwdText;
            L{end+1} = '    Ehat_kWh = y_norm * YStd + YMean;';
            L{end+1} = '    e_hat = (Ehat_kWh - Ebar) / Escale;';
            L{end+1} = 'else';
            L{end+1} = '    e_hat = 0;';
            L{end+1} = 'end';
            L = [L, tail_grid()];
            body = strjoin(L, newline);
        case 'f2'
            L = head_sig_full();
            L = [L, common_head()];
            L{end+1} = 'xP = zeros(300, 1);';
            L{end+1} = 'for j = 1:300';
            L{end+1} = '    i = k - j;';
            L{end+1} = '    if i < 1';
            L{end+1} = '        i = i + 300;';
            L{end+1} = '    end';
            L{end+1} = '    xP(j) = buf(i);';
            L{end+1} = 'end';
            L{end+1} = 'x = [xP; v_ship; accel];';
            L{end+1} = 'if ok';
            L{end+1} = '    ok = all(isfinite(x));';
            L{end+1} = 'end';
            L{end+1} = 'if ok';
            L{end+1} = fwdText;
            L{end+1} = '    Ehat_kWh = y_norm * YStd + YMean;';
            L{end+1} = '    e_hat = (Ehat_kWh - Ebar) / Escale;';
            L{end+1} = 'else';
            L{end+1} = '    e_hat = 0;';
            L{end+1} = 'end';
            L = [L, tail_grid()];
            body = strjoin(L, newline);
        case 'b'
            L = head_sig_full();
            L = [L, common_head()];
            L{end+1} = 'xP = zeros(300, 1);';
            L{end+1} = 'for j = 1:300';
            L{end+1} = '    i = k - j;';
            L{end+1} = '    if i < 1';
            L{end+1} = '        i = i + 300;';
            L{end+1} = '    end';
            L{end+1} = '    xP(j) = buf(i);';
            L{end+1} = 'end';
            L{end+1} = 'x = [xP; v_ship; accel];';
            L{end+1} = 'ok = ok && all(isfinite(x));';
            L{end+1} = 'if ok';
            L{end+1} = fwdText;
            L{end+1} = '    Ehat_kWh = y_norm * YStd + YMean;';
            L{end+1} = '    e_hat = (Ehat_kWh - Ebar) / Escale;';
            L{end+1} = 'else';
            L{end+1} = '    e_hat = 0;';
            L{end+1} = 'end';
            L = [L, tail_grid()];
            body = strjoin(L, newline);
        case 'min'
            % Zero chart parameters, no grid search, no aliasing: pure structure.
            L = {};
            L{end+1} = 'function P = ecms_nn_min(t, P_load, v_ship)';
            L{end+1} = 'persistent lastT buf k vprev';
            L{end+1} = 'if isempty(lastT)';
            L{end+1} = '    lastT = -1e9;';
            L{end+1} = '    k = 1;';
            L{end+1} = '    buf = zeros(300, 1);';
            L{end+1} = '    buf(:) = P_load;';
            L{end+1} = '    vprev = v_ship;';
            L{end+1} = 'end';
            L{end+1} = 'P = 0;';
            L{end+1} = 'if t - lastT < 1.0 - 1e-9';
            L{end+1} = '    return;';
            L{end+1} = 'end';
            L{end+1} = 'lastT = t;';
            L{end+1} = 'if isfinite(P_load)';
            L{end+1} = '    buf(k) = P_load;';
            L{end+1} = 'end';
            L{end+1} = 'k = mod(k, 300) + 1;';
            L{end+1} = 'accel = v_ship - vprev;';
            L{end+1} = 'vprev = v_ship;';
            L{end+1} = 'ok = isfinite(P_load) && isfinite(v_ship) && isfinite(accel);';
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
            L{end+1} = '    P = sum(x);';
            L{end+1} = 'else';
            L{end+1} = '    P = 0;';
            L{end+1} = 'end';
            L{end+1} = 'end';
            body = strjoin(L, newline);
    end
end

function r = run_exp4(id, codeText, paramNames, paramSizes, slxPath)
% Same stage-aware runner as round 3 (extended report on failure).
    MDL = 'HybridShip_ModelD_DCBus';
    r = struct('id', id, 'pass', false, 'setter_ok', false, 'update_ok', false, ...
        'identifier', '', 'message', '');
    fprintf('\n[exp4] >>> %s\n', id);
    failId = '';
    failMsg = '';
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
        try
            ch.Script = codeText;
            r.setter_ok = true;
        catch errS
            failId = errS.identifier;
            failMsg = ['SETTER-STAGE] ' getReport(errS, 'extended')];
        end
        if r.setter_ok
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
            try
                set_param(MDL, 'SimulationCommand', 'update');
                r.update_ok = true;
            catch errU
                failId = errU.identifier;
                failMsg = ['UPDATE-STAGE] ' getReport(errU, 'extended')];
            end
        end
    catch err0
        failId = err0.identifier;
        failMsg = ['SETUP] ' getReport(err0, 'extended')];
    end
    if isempty(failId)
        r.pass = true;
        fprintf('[exp4] %s COMPILE OK\n', id);
    else
        r.identifier = failId;
        r.message = failMsg;
        if numel(r.message) > 2000
            r.message = [r.message(1:2000) ' ...(truncated)'];
        end
        fprintf('[exp4] %s FAILED: %s\n%s\n', id, failId, strrep(failMsg, newline, ' | '));
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

%% ========================================================================
%  exp_matrix3.m — law=7 bisect round 3.
%
%  Round-2 facts (exp_matrix2_results.json):
%    - ALL failures are in the UPDATE stage (script text parse always passes)
%    - EXP-7 (pure local ones(302,1), no chart params) still fails with the
%      same Stateflow:cdr:ErrorsParsingEmlFcn -> chart parameter objects are
%      INNOCENT. The trigger is: vector math on x inside the second if-block,
%      where x was assembled in the FIRST if-block as [xP; v_ship; accel].
%    - simple assignments (xnorm = XMean / x / x+XMean) give a wrapped
%      MATLAB:MException:MultipleErrors whose causes were not expanded yet.
%
%  Round-3 experiments (diagnostics upgraded: full extended report captured):
%    R3_EXP_wip_baseline : wip code, inherit input sizes (control, expect FAIL)
%    R3_EXP_D_insize     : wip code + all 11 function inputs explicit Size='1'
%    R3_EXP_A_scalar_idx : forward replaced by  y_norm = x(1);
%    R3_EXP_C_expl_x     : x preallocated zeros(302,1) then segment-assigned
%    R3_EXP_B_x_outside  : x assembly moved OUT of the first if-block
%
%  SAFETY: all edits in memory; close_system(MDL, 0) NEVER saves the .slx.
%% ========================================================================
clearvars; close all; clc;
ROOT   = 'E:/User/Tommy/Desktop/hybrid-ship-ems/model/simulink_modelD';
MDL    = 'HybridShip_ModelD_DCBus';
slx    = fullfile(ROOT, 'model', [MDL '.slx']);
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7');

assert(exist(slx, 'file') == 2, 'EXP:MODEL', 'model not found: %s', slx);
if ~exist(outDir, 'dir'), mkdir(outDir); end
cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));

fprintf('[exp3] MATLAB %s\n', version);

%% ---- workspace parameters -------------------------------------------------
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'EXP:WEIGHTS', 'weight injection failed');

%% ---- input port names (function signature order) --------------------------
inNames = {'t','P_load','v_ship','SOC','lambda','K_soc','SOC_ref','K_S', ...
           'Lambda_Floor','bsfc_bp','bsfc_tbl'};
inSizesScalar = repmat({'1'}, 1, 9);          % t..Lambda_Floor scalars
inSizesTables = {'[1 50]','[1 50]'};          % bsfc_bp / bsfc_tbl rows (actual width from base)
bpW = numel(evalin('base','BSFC_Breakpoints_kW'));
tblW = numel(evalin('base','BSFC_Table_gkWh'));
inSizes = [inSizesScalar, {sprintf('[1 %d]', bpW)}, {sprintf('[1 %d]', tblW)}];

%% ---- parameter name sets (same as round 1/2) -------------------------------
namesScalar = {'ASSUMED_eta_rect','ASSUMED_eta_dcdc','ASSUMED_Bat_Eff_Discharge', ...
    'ASSUMED_Bat_Eff_Charge','Gen_P_max','ECMS_SOC_Discharge_Max','ECMS_SOC_Charge_Min', ...
    'ECMS_Idle_Fuel_kgph','ECMS_Grid_dP_kW','ECMS_dt_ctrl_s','Pgen_init'};
namesNN = {'ECMS_NN_W1','ECMS_NN_b1','ECMS_NN_W2','ECMS_NN_b2','ECMS_NN_W3','ECMS_NN_b3', ...
    'ECMS_NN_XMean','ECMS_NN_XStd','ECMS_NN_YMean','ECMS_NN_YStd', ...
    'ECMS_NN_Ebar_H_kWh','ECMS_NN_Escale_kWh','ECMS_NN_WeightsOK'};
sizesNN = {'[302 128]','[1 128]','[128 64]','[1 64]','[64 1]','[1 1]', ...
    '[302 1]','[302 1]','[1 1]','[1 1]','[1 1]','[1 1]','[1 1]'};
namesAll = [namesScalar, namesNN];
sizesEmpty = cell(1, numel(namesAll));

%% ---- code variants ---------------------------------------------------------
fwdFull = ecms2.nn_forward_code();

codeWip = build_body3('wip',   fwdFull);
codeA   = build_body3('ascalar', fwdFull);
codeC   = build_body3('explx', fwdFull);
codeB   = build_body3('xout',  fwdFull);

%% ---- experiment matrix ----------------------------------------------------
results = struct('id',{},'pass',{},'setter_ok',{},'update_ok',{},'identifier',{},'message',{});
results(end+1) = run_exp3('R3_EXP_wip_baseline', codeWip, namesAll, sizesEmpty, false, slx);   %#ok<*SAGROW>
print_one(results(end));
results(end+1) = run_exp3('R3_EXP_D_insize',     codeWip, namesAll, sizesEmpty, true,  slx);
print_one(results(end));
results(end+1) = run_exp3('R3_EXP_A_scalar_idx', codeA,   namesAll, sizesEmpty, false, slx);
print_one(results(end));
results(end+1) = run_exp3('R3_EXP_C_expl_x',     codeC,   namesAll, sizesEmpty, false, slx);
print_one(results(end));
results(end+1) = run_exp3('R3_EXP_B_x_outside',  codeB,   namesAll, sizesEmpty, false, slx);
print_one(results(end));

%% ---- machine-readable output ----------------------------------------------
try, mdlMd5 = model.md5file(slx); catch, mdlMd5 = 'unavailable'; end
out = struct();
out.timestamp = char(datetime('now','Format','yyyy-MM-dd HH:mm:ss'));
out.matlab_version = version;
out.model_file_md5 = mdlMd5;
out.experiments = results;
jPath = fullfile(outDir, 'exp_matrix3_results.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(out), 'char');
fclose(fid);

fprintf('\n===== SUMMARY (round 3) =====\n');
for i = 1:numel(results)
    print_one(results(i));
end
fprintf('\n[exp3] JSON written: %s\n', jPath);
fprintf('[exp3] DONE\n');

%% ============================ local functions ==============================
function print_one(r)
    msg = strrep(r.message, newline, ' ');
    if numel(msg) > 60, msg = [msg(1:60) '...']; end
    fprintf('%-22s pass=%-5s setter=%-5s update=%-5s %-34s %s\n', ...
        r.id, string(r.pass), string(r.setter_ok), string(r.update_ok), r.identifier, msg);
end

function body = build_body3(mode, fwdText)
% Variants of the wip inline law=7 body:
%   wip     : identical to round 1/2 baseline
%   ascalar : forward block replaced by  y_norm = x(1);
%   explx   : x built by explicit preallocation instead of concatenation
%   xout    : x assembly moved out of the first if-block (unconditional)
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
    if strcmp(mode, 'xout')
        % unconditional x assembly (variant B)
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
    elseif strcmp(mode, 'explx')
        % explicit preallocated x inside first if-block (variant C)
        L{end+1} = 'if ok';
        L{end+1} = '    xP = zeros(300, 1);';
        L{end+1} = '    for j = 1:300';
        L{end+1} = '        i = k - j;';
        L{end+1} = '        if i < 1';
        L{end+1} = '            i = i + 300;';
        L{end+1} = '        end';
        L{end+1} = '        xP(j) = buf(i);';
        L{end+1} = '    end';
        L{end+1} = '    x = zeros(302, 1);';
        L{end+1} = '    x(1:300) = xP;';
        L{end+1} = '    x(301) = v_ship;';
        L{end+1} = '    x(302) = accel;';
        L{end+1} = '    ok = all(isfinite(x));';
        L{end+1} = 'end';
        L{end+1} = 'if ok';
        L{end+1} = fwdText;
        L{end+1} = '    Ehat_kWh = y_norm * YStd + YMean;';
        L{end+1} = '    e_hat = (Ehat_kWh - Ebar) / Escale;';
        L{end+1} = 'else';
        L{end+1} = '    e_hat = 0;';
        L{end+1} = 'end';
    else
        % wip / ascolar share the original first-if structure
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
        if strcmp(mode, 'ascalar')
            L{end+1} = '    y_norm = x(1);';
        else
            L{end+1} = fwdText;
        end
        L{end+1} = '    Ehat_kWh = y_norm * YStd + YMean;';
        L{end+1} = '    e_hat = (Ehat_kWh - Ebar) / Escale;';
        L{end+1} = 'else';
        L{end+1} = '    e_hat = 0;';
        L{end+1} = 'end';
    end
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

function r = run_exp3(id, codeText, paramNames, paramSizes, setInportSizes, slxPath)
% Round-3 runner: stage-aware + FULL extended error report (cause chain).
% setInportSizes=true additionally pins all 11 function inputs to explicit sizes.
    MDL = 'HybridShip_ModelD_DCBus';
    r = struct('id', id, 'pass', false, 'setter_ok', false, 'update_ok', false, ...
        'identifier', '', 'message', '');
    fprintf('\n[exp3] >>> %s\n', id);
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
            if setInportSizes
                for i = 1:numel(ds)
                    if strcmp(ds(i).Scope, 'Input Data')
                        ds(i).Props.Array.Size = '1';
                    end
                end
                % table inputs need row-vector sizes; find by name again
                for i = 1:numel(ds)
                    if strcmp(ds(i).Name, 'bsfc_bp')
                        ds(i).Props.Array.Size = sprintf('[1 %d]', numel(evalin('base','BSFC_Breakpoints_kW')));
                    elseif strcmp(ds(i).Name, 'bsfc_tbl')
                        ds(i).Props.Array.Size = sprintf('[1 %d]', numel(evalin('base','BSFC_Table_gkWh')));
                    end
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
        fprintf('[exp3] %s COMPILE OK\n', id);
    else
        r.identifier = failId;
        r.message = failMsg;
        if numel(r.message) > 2000
            r.message = [r.message(1:2000) ' ...(truncated)'];
        end
        fprintf('[exp3] %s FAILED: %s\n%s\n', id, failId, strrep(failMsg, newline, ' | '));
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

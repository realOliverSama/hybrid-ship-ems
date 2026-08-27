%% ========================================================================
%  exp_verify_inline.m — numerical verification of the FIXED inline law=7
%  against the shipping extrinsic fallback on the same closed-loop scenario.
%
%  Fix under test (round-3/4 verified compile-passing structure):
%      x assembly moved OUT of the if-block; ok = ok && all(isfinite(x)).
%
%  Evidence chain covered by this single comparison:
%    inline block  vs  ecms2.nn_predict_ehat (same MLP/weights/buffer logic)
%    and ecms2.nn_predict_ehat vs Python already proven |Dy|<1e-5 (test_law7 T1)
%    => transitive consistency across MATLAB-interp / compiled-EML / Python.
%
%  Scenario: S01 (SCENARIOS(1), 5000 m), EMS_CTRL_LAW=7, two runs:
%    RUN-A  fixed INLINE implementation (in-memory patched chart)
%    RUN-B  shipping EXTRINSIC fallback (product text, in-memory patched back)
%  Both charts are patched IN MEMORY; the .slx file is NEVER saved.
%
%  Acceptance:
%    fuel relative diff < 1e-3 AND |SOC_end diff| < 1e-3
%    AND max|dP_gen_ref| <= 5 kW (one grid step)
%% ========================================================================
clearvars; close all; clc;
ROOT   = 'E:/User/Tommy/Desktop/hybrid-ship-ems/model/simulink_modelD';
MDL    = 'HybridShip_ModelD_DCBus';
slx    = fullfile(ROOT, 'model', [MDL '.slx']);
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'debug_law7');
assert(exist(slx, 'file') == 2, 'VER:MODEL', 'model not found');
if ~exist(outDir, 'dir'), mkdir(outDir); end
cd(ROOT);
addpath(fullfile(ROOT, 'scripts'));
fprintf('[ver] MATLAB %s\n', version);

%% ---- workspace parameters -------------------------------------------------
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'VER:WEIGHTS', 'weight injection failed');

%% ---- chart parameter names (identical list for both variants) --------------
paramNames = {'ASSUMED_eta_rect','ASSUMED_eta_dcdc','ASSUMED_Bat_Eff_Discharge', ...
    'ASSUMED_Bat_Eff_Charge','Gen_P_max','ECMS_SOC_Discharge_Max','ECMS_SOC_Charge_Min', ...
    'ECMS_Idle_Fuel_kgph','ECMS_Grid_dP_kW','ECMS_dt_ctrl_s','Pgen_init', ...
    'ECMS_NN_W1','ECMS_NN_b1','ECMS_NN_W2','ECMS_NN_b2','ECMS_NN_W3','ECMS_NN_b3', ...
    'ECMS_NN_XMean','ECMS_NN_XStd','ECMS_NN_YMean','ECMS_NN_YStd', ...
    'ECMS_NN_Ebar_H_kWh','ECMS_NN_Escale_kWh','ECMS_NN_WeightsOK'};

%% ---- code texts ------------------------------------------------------------
codeInlineFix = build_inline_fixed(ecms2.nn_forward_code());
codeExtrinsic = build_extrinsic_shipping();

%% ---- RUN-A: fixed inline ---------------------------------------------------
[dsA, okA] = run_case('RUN-A_inline_fix', MDL, slx, codeInlineFix, paramNames);
assert(okA, 'VER:RUNA', 'RUN-A failed');

%% ---- RUN-B: shipping extrinsic --------------------------------------------
[dsB, okB] = run_case('RUN-B_extrinsic', MDL, slx, codeExtrinsic, paramNames);
assert(okB, 'VER:RUNB', 'RUN-B failed');

%% ---- comparison ------------------------------------------------------------
n = min(numel(dsA.t), numel(dsB.t));
dt = max(abs(dsA.t(1:n) - dsB.t(1:n)));
assert(dt < 1e-6, 'VER:TGRID', 'time grids differ (max %g s)', dt);
dP   = abs(dsA.P_gen_ref_kW(1:n) - dsB.P_gen_ref_kW(1:n));
dSOC = abs(dsA.SOC(1:n) - dsB.SOC(1:n));
fuelA = dsA.fuel_total_kg(end);  fuelB = dsB.fuel_total_kg(end);
dfuelRel = abs(fuelA - fuelB) / fuelA;
vdcDiff = abs(min(dsA.V_dc_V) - min(dsB.V_dc_V));

stats = struct();
stats.n_samples   = n;
stats.dP_max_kW   = max(dP);
stats.dP_mean_kW  = mean(dP);
stats.dSOC_max    = max(dSOC);
stats.fuel_inline_kg  = fuelA;
stats.fuel_extrin_kg  = fuelB;
stats.dfuel_rel       = dfuelRel;
stats.SOC_end_inline  = dsA.SOC(end);
stats.SOC_end_extrin  = dsB.SOC(end);
stats.Vdc_min_inline  = min(dsA.V_dc_V);
stats.Vdc_min_extrin  = min(dsB.V_dc_V);
stats.Vdc_min_diff    = vdcDiff;
stats.pass = (dfuelRel < 1e-3) && (abs(dsA.SOC(end)-dsB.SOC(end)) < 1e-3) ...
          && (max(dP) <= 5.0);

fprintf('\n===== VERIFICATION SUMMARY =====\n');
fprintf('samples            : %d\n', stats.n_samples);
fprintf('dP_gen_ref max     : %.6g kW (mean %.6g)\n', stats.dP_max_kW, stats.dP_mean_kW);
fprintf('dSOC max           : %.6g\n', stats.dSOC_max);
fprintf('fuel  inline/extrin: %.4f / %.4f kg  (rel diff %.3g)\n', fuelA, fuelB, dfuelRel);
fprintf('SOC_end            : %.6f / %.6f\n', stats.SOC_end_inline, stats.SOC_end_extrin);
fprintf('Vdc_min            : %.1f / %.1f V (diff %.1f)\n', stats.Vdc_min_inline, stats.Vdc_min_extrin, vdcDiff);
fprintf('VERDICT            : %s\n', string(stats.pass));

out = struct();
out.timestamp = char(datetime('now','Format','yyyy-MM-dd HH:mm:ss'));
out.matlab_version = version;
out.model_file_md5 = 'ada8a732ba2e8ad47e904d5c76880efc';
out.scenario = 'S01 (SCENARIOS(1))';
out.stats = stats;
jPath = fullfile(outDir, 'inline_verification.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(out), 'char');
fclose(fid);
fprintf('[ver] JSON written: %s\n', jPath);
fprintf('[ver] DONE\n');

%% ============================ local functions ==============================
function [ds, ok] = run_case(name, MDL, slxPath, codeText, paramNames)
% Patch law=7 chart in memory, compile-check, run S01 closed loop, extract logs.
% NEVER saves the model.
    ds = [];
    ok = false;
    fprintf('\n[ver] >>> %s\n', name);
    try
        if bdIsLoaded(MDL)
            close_system(MDL, 0);
        end
        load_system(slxPath);
        chPath = [MDL '/05_EMS_Controller/EMS_NN_AECMS/ecms_step'];
        ch = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', chPath);
        assert(~isempty(ch), 'VER:CHART', 'law=7 EMChart not found');
        ch = ch(1);
        ch.Script = codeText;
        dsx = ch.find('-isa', 'Stateflow.Data');
        for i = 1:numel(paramNames)
            d = [];
            for j = 1:numel(dsx)
                if strcmp(dsx(j).Name, paramNames{i}), d = dsx(j); break; end
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
        set_param(MDL, 'SimulationCommand', 'update');   % compile check
        fprintf('[ver] %s compile OK, running S01 ...\n', name);
        SCN = evalin('base', 'SCENARIOS(1)');
        stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
        tic
        out = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
        wall = toc;
        ds = model.extract_logsout(out);
        assert(isfield(ds, 'P_gen_ref_kW'), 'VER:LOGS', 'logsout missing P_gen_ref_kW');
        fprintf('[ver] %s SIM DONE: t_end=%.0f s fuel=%.2f kg SOC_end=%.4f wall=%.0f s\n', ...
            name, ds.t(end), ds.fuel_total_kg(end), ds.SOC(end), wall);
        ok = true;
    catch err
        fprintf('[ver] %s FAILED: %s | %s\n', name, err.identifier, strrep(err.message, newline, ' '));
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

function body = build_inline_fixed(fwdText)
% Fixed inline law=7 (round-3 EXP-B structure): unconditional x assembly,
% ok merged with short-circuit and, full NN forward + full grid search.
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
    L{end+1} = 'if t < lastT';
    L{end+1} = '    % new simulation started (same semantics as ecms2.nn_predict_ehat)';
    L{end+1} = '    k = 1;';
    L{end+1} = '    buf(:) = P_load;';
    L{end+1} = '    vprev = v_ship;';
    L{end+1} = '    lastT = t;';
    L{end+1} = 'end';
    L{end+1} = 'P = Pheld;';;
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
    L{end+1} = '% time-ASCENDING reorder, identical to ecms2.nn_predict_ehat:';
    L{end+1} = '% xP(1)=P(t-299) oldest ... xP(300)=P(t) newest (fleet_norm feature_layout)';
    L{end+1} = 'xP = [buf(k:300); buf(1:k-1)];';
    L{end+1} = 'x = [xP; v_ship; accel];';
    L{end+1} = 'ok = ok && all(isfinite(x));';
    L{end+1} = 'if ok';
    L{end+1} = fwdText;
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

function body = build_extrinsic_shipping()
% Faithful copy of the SHIPPING product text: build_model_c.m HEAD CODE_ECMS_NN()
% (Amendment v1.2 extrinsic fallback). Only comments stripped where harmless.
    c = sprintf([ ...
        'function P = ecms_nn(t, P_load, v_ship, SOC, lambda, K_soc, SOC_ref, K_S, Lambda_Floor, bsfc_bp, bsfc_tbl)\n' ...
        'coder.extrinsic(''ecms2.nn_predict_ehat'');\n' ...
        'persistent lastT Pheld e_hat\n' ...
        'if isempty(lastT)\n' ...
        '    lastT = -1e9;\n' ...
        '    Pheld = Pgen_init;\n' ...
        '    e_hat = 0;\n' ...
        'end\n' ...
        'P = Pheld;\n' ...
        'if t - lastT < ECMS_dt_ctrl_s - 1e-9\n' ...
        '    return;\n' ...
        'end\n' ...
        'lastT = t;\n' ...
        'eh_new = 0;\n' ...
        'eh_new = ecms2.nn_predict_ehat(t, P_load, v_ship);\n' ...
        'e_hat = eh_new;\n' ...
        'soc_ref_eff = SOC_ref + K_S * e_hat;\n' ...
        'lam = lambda + K_soc * (soc_ref_eff - SOC);\n' ...
        'if K_S ~= 0\n' ...
        '    lam = max(lam, Lambda_Floor);\n' ...
        'end\n' ...
        'N = floor(Gen_P_max / ECMS_Grid_dP_kW) + 1;\n' ...
        'Pbest = 0;\n' ...
        'Jbest = inf;\n' ...
        'nb = numel(bsfc_bp);\n' ...
        'for i = 1:N\n' ...
        '    Pg = (i - 1) * ECMS_Grid_dP_kW;\n' ...
        '    Pbat = P_load - ASSUMED_eta_rect * Pg;\n' ...
        '    if SOC <= ECMS_SOC_Discharge_Max\n' ...
        '        if Pbat > 1e-9\n' ...
        '            continue;\n' ...
        '        end\n' ...
        '        Pbest = Pg;\n' ...
        '        break;\n' ...
        '    end\n' ...
        '    if SOC >= ECMS_SOC_Charge_Min && Pbat < 0\n' ...
        '        continue;\n' ...
        '    end\n' ...
        '    if Pg < 1e-9\n' ...
        '        mf = ECMS_Idle_Fuel_kgph / 3600;\n' ...
        '    else\n' ...
        '        bsfc = bsfc_tbl(1);\n' ...
        '        if Pg >= bsfc_bp(nb)\n' ...
        '            bsfc = bsfc_tbl(nb);\n' ...
        '        elseif Pg > bsfc_bp(1)\n' ...
        '            ii = 1;\n' ...
        '            while ii < nb - 1 && bsfc_bp(ii + 1) < Pg\n' ...
        '                ii = ii + 1;\n' ...
        '            end\n' ...
        '            w = (Pg - bsfc_bp(ii)) / (bsfc_bp(ii + 1) - bsfc_bp(ii));\n' ...
        '            bsfc = bsfc_tbl(ii) + w * (bsfc_tbl(ii + 1) - bsfc_tbl(ii));\n' ...
        '        end\n' ...
        '        mf = Pg * bsfc / 3.6e6;\n' ...
        '    end\n' ...
        '    if Pbat > 0\n' ...
        '        mbe = Pbat / (ASSUMED_eta_dcdc * ASSUMED_Bat_Eff_Discharge) / 3600;\n' ...
        '    else\n' ...
        '        mbe = Pbat * (ASSUMED_eta_dcdc * ASSUMED_Bat_Eff_Charge) / 3600;\n' ...
        '    end\n' ...
        '    J = mf + lam * mbe;\n' ...
        '    if J < Jbest\n' ...
        '        Jbest = J;\n' ...
        '        Pbest = Pg;\n' ...
        '    end\n' ...
        'end\n' ...
        'if SOC <= ECMS_SOC_Discharge_Max && Pbest < 1e-9 && P_load > ASSUMED_eta_rect * Gen_P_max\n' ...
        '    Pbest = Gen_P_max;\n' ...
        'end\n' ...
        'P = Pbest;\n' ...
        'Pheld = Pbest;\n' ...
        'end\n']);
    body = c;
end

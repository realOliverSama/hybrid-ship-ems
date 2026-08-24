function res = calibrate_kE(ROOT)
% ecms2.calibrate_kE — K_E 标定（T-106，FR-107，design §3.2）
%
%  law=6 Oracle Predictive A-ECMS：λ(t) = λ₀ + K_soc·(SOC_ref − SOC) + K_E·ê_future(t)
%  在 S20 复合航次上网格搜索 K_E ∈ {0, 0.02, 0.05, 0.1, 0.2}：
%    合格 = SOC_end ∈ [0.595, 0.605] 且 fuel_corr 不劣于 law=5（≤ law5 + 1e-9）；
%    选择 = 合格集内 fuel_corr 最小（并列取更小 K_E）；无合格 -> Escalate（不强行调参）。
%  前瞻信号：ecms2.oracle_future_energy 对 S20 Baseline 剖面（T-102 产物）预计算
%  ECMS_EHat_Data（N×3：[t, E_future, e_hat]，H=300 s），经 From Workspace 输入模型。
%  **红线（E5，FR-107）**：oracle 未来需求非实时可实现——law=6 仅用于上界/增量验证，
%  代码与报告不得表述为可在线获得。
%
%  前置: 10_build_modelD.m 已重建模型（含 law=6 变体）；T-102 S20 剖面已产出
%        （results/ecms_phase1/profiles/S20_composite_voyage.mat）。
%  产物: results/ecms_phase1/kE_calibration.json（每档 K_E/SOC_end/fuel_raw/fuel_corr/
%        是否不劣于 law=5；含 law=5 S20 参照与最终选定 K_E 的 S20 × law=6 终值）。
%  用法: res = ecms2.calibrate_kE();   % ROOT 缺省 model/simulink_modelD
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    MDL = 'HybridShip_ModelD_DCBus';
    SOC_TARGET = 0.60;  BAND_LO = 0.595;  BAND_HI = 0.605;
    K_EQ = 0.4774;      % fuel_corr 折算系数 [kg/kWh]（design §3.1 基线 k_eq，T-104 口径）
    H_ORACLE = 300;     % 前瞻时域 [s]（design §3.2）
    KE_GRID = [0 0.02 0.05 0.1 0.2];

    fprintf('\n[ecms2.calibrate_kE] ===== T-106 K_E 标定（S20 × law=6，design §3.2）=====\n');
    slxPath = fullfile(ROOT, 'model', [MDL '.slx']);
    assert(exist(slxPath, 'file') == 2, 'ECMS2:CALKE', ...
        '%s 不存在（先运行 10_build_modelD.m）', slxPath);
    assert(exist(fullfile(ROOT, 'config', 'ecms_bsfc_override.mat'), 'file') == 2, ...
        'ECMS2:CALKE', '缺少 config/ecms_bsfc_override.mat（先运行 ecms2.make_bsfc_override）');
    profFile = fullfile(ROOT, 'results', 'ecms_phase1', 'profiles', 'S20_composite_voyage.mat');
    assert(exist(profFile, 'file') == 2, 'ECMS2:CALKE', ...
        '缺少 S20 剖面 %s（先运行 40_build_composite_voyage.m，T-102）', profFile);

    % ---- Oracle 前瞻信号（离线预计算，含交叉验证 <0.1%）----
    prof = load(profFile);
    [~, ~, ehO, infoO] = ecms2.oracle_future_energy(prof, H_ORACLE, true);
    ECMS_EHat_Data = infoO.data;              % N×3：[t, E_future, e_hat]
    fprintf('[calibrate_kE] E_future: Ē_H=%.2f kWh E_scale=%.2f kWh ê∈[%.3f, %.3f]\n', ...
        infoO.E_bar_H_kWh, infoO.E_scale_kWh, min(ehO), max(ehO));

    % ---- law=5 × S20 参照（λ₀/K_soc 已标定入 config；T-104/105）----
    fprintf('\n[calibrate_kE] ===== law=5 × S20 参照（K_E 标定基准）=====\n');
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    BatCap = evalin('base', 'Bat_Cap_kWh');
    lam0 = evalin('base', 'ECMS_Lambda');
    Ksoc = evalin('base', 'ECMS_K_soc');
    tic
    r5 = runS20Sim(ROOT, MDL, 5, lam0, Ksoc, NaN, ECMS_EHat_Data);
    wall5 = toc;
    fuelCorr5 = r5.fuel_total_kg - (r5.SOC_end - SOC_TARGET) * BatCap * K_EQ;
    fprintf('[calibrate_kE] law=5 × S20: fuel_raw=%.3f kg | fuel_corr=%.3f kg | SOC_end=%.4f | Vdc_min=%.1f V (%.0f s)\n', ...
        r5.fuel_total_kg, fuelCorr5, r5.SOC_end, r5.Vdc_min, wall5);
    law5ref = struct('SOC_end', r5.SOC_end, 'fuel_raw_kg', r5.fuel_total_kg, ...
        'fuel_corr_kg', fuelCorr5, 'Vdc_min_V', r5.Vdc_min, 'wall_time_s', wall5);

    % ---- K_E 网格搜索（S20 × law=6）----
    fprintf('\n[calibrate_kE] ===== K_E 网格搜索（S20 × law=6）=====\n');
    grid = struct([]);
    best = NaN; bestFuelCorr = inf; bestSOC = NaN;
    for i = 1:numel(KE_GRID)
        K = KE_GRID(i);
        tic
        r6 = runS20Sim(ROOT, MDL, 6, lam0, Ksoc, K, ECMS_EHat_Data);
        wall = toc;
        fc = r6.fuel_total_kg - (r6.SOC_end - SOC_TARGET) * BatCap * K_EQ;
        inBand = (r6.SOC_end >= BAND_LO && r6.SOC_end <= BAND_HI);
        notWorse = fc <= fuelCorr5 + 1e-9;
        qualified = inBand && notWorse;
        grid(end+1).K_E = K;                      %#ok<AGROW>
        grid(end).SOC_end = r6.SOC_end;
        grid(end).fuel_raw_kg = r6.fuel_total_kg;
        grid(end).fuel_corr_kg = fc;
        grid(end).Vdc_min_V = r6.Vdc_min;
        grid(end).soc_in_band = double(inBand);
        grid(end).not_worse_than_law5 = double(notWorse);
        grid(end).qualified = double(qualified);
        grid(end).wall_time_s = wall;
        fprintf('[calibrate_kE] K_E=%.2f -> SOC_end=%.4f fuel_raw=%.3f kg fuel_corr=%.3f kg Vdc_min=%.1f V -> %s\n', ...
            K, r6.SOC_end, r6.fuel_total_kg, fc, r6.Vdc_min, string(qualified));
        if qualified && (fc < bestFuelCorr - 1e-9 || ...
                (abs(fc - bestFuelCorr) < 1e-9 && (isnan(best) || K < best)))
            best = K; bestSOC = r6.SOC_end; bestFuelCorr = fc;
        end
    end

    % ---- 结果文件（每档记录 + law=5 参照 + 选定）----
    outDir = fullfile(ROOT, 'results', 'ecms_phase1');
    if ~exist(outDir, 'dir'), mkdir(outDir); end
    ovFile = fullfile(ROOT, 'config', 'ecms_bsfc_override.mat');
    ovTag = 'default (ASSUMED_BSFC_*)';
    if exist(ovFile, 'file')
        ovinfo = load(ovFile, 'override_tag');
        ovTag = ovinfo.override_tag;
    end
    res = struct();
    res.task = 'T-106 K_E calibration (S20 × law=6, tier2 mid override)';
    res.oracle = struct('source', profFile, 'H_s', H_ORACLE, ...
        'E_bar_H_kWh', infoO.E_bar_H_kWh, 'E_scale_kWh', infoO.E_scale_kWh, ...
        'n_samples', infoO.n, 'crossval_max_rel_err', infoO.check.max_rel_err);
    res.lambda_0 = lam0;
    res.K_soc = Ksoc;
    res.soc_target = SOC_TARGET;
    res.band = [BAND_LO, BAND_HI];
    res.k_eq_kg_kWh = K_EQ;
    res.bat_cap_kWh = BatCap;
    res.bsfc_override_tag = ovTag;
    res.law5_reference = law5ref;
    res.grid = grid;
    if isnan(best)
        res.chosen_K_E = [];
        res.qualified = 0;
        res.escalation = 'K_E 网格无合格档（SOC_end 未入带或全部劣于 law=5）——不写回 config，Escalate 由主 Agent 定夺（不得强行调参）';
    else
        res.chosen_K_E = best;
        res.chosen_SOC_end = bestSOC;
        res.chosen_fuel_corr_kg = bestFuelCorr;
        res.qualified = 1;
        % ---- 写回 config ECMS_K_E + canonical 镜像同步（md5 一致）----
        setConfigValue(ROOT, 'ECMS_K_E', best);
        assignin('base', 'ECMS_K_E', best);
        fprintf('[calibrate_kE] 选定 K_E=%.2f（合格集内 fuel_corr 最小 %.3f kg）| SOC_end=%.4f\n', ...
            best, bestFuelCorr, bestSOC);
        fprintf('[calibrate_kE] ECMS_K_E 已写回 config 并同步 canonical_data\n');
        % ---- 终值：S20 × law=6（选定 K_E）----
        tic
        rF = runS20Sim(ROOT, MDL, 6, lam0, Ksoc, best, ECMS_EHat_Data);
        wallF = toc;
        fcF = rF.fuel_total_kg - (rF.SOC_end - SOC_TARGET) * BatCap * K_EQ;
        res.final_run = struct('K_E', best, 'SOC_end', rF.SOC_end, ...
            'fuel_raw_kg', rF.fuel_total_kg, 'fuel_corr_kg', fcF, ...
            'Vdc_min_V', rF.Vdc_min, 'wall_time_s', wallF);
        fprintf('\n[calibrate_kE] ===== S20 × law=6 终值（K_E=%.2f）=====\n', best);
        fprintf('[calibrate_kE] fuel_raw=%.3f kg | fuel_corr=%.3f kg | SOC_end=%.4f | Vdc_min=%.1f V (%.0f s)\n', ...
            rF.fuel_total_kg, fcF, rF.SOC_end, rF.Vdc_min, wallF);
        fprintf('[calibrate_kE] vs law=5: fuel_corr %+.3f kg（%.3f%%）| SOC_end %+.4f\n', ...
            fcF - fuelCorr5, 100 * (fcF - fuelCorr5) / fuelCorr5, rF.SOC_end - r5.SOC_end);
    end
    res.non_realtime_note = 'law=6 使用真实未来需求剖面（oracle）离线预计算 ê_future——非实时可实现，仅用于上界/增量验证（E5，FR-107）';

    jsonTxt = jsonencode(res, 'PrettyPrint', true);
    jsonFile = fullfile(outDir, 'kE_calibration.json');
    fid = fopen(jsonFile, 'w', 'n', 'UTF-8');
    fprintf(fid, '%s\n', jsonTxt);
    fclose(fid);
    fprintf('[calibrate_kE] 已写入 %s\n', jsonFile);

    if isnan(best)
        error('ECMS2:CALKE:NOQUALIFIED', ...
            'K_E 网格无合格档（SOC_end 未入带或全部劣于 law=5）——Escalate，不得强行调参');
    end
end

%% ================= 本地仿真封装（S20 时间终止，law=5/6） =================
function r = runS20Sim(ROOT, MDL, law, lam, Ksoc, KE, EHatData)
% law=5（KE 传 NaN）或 law=6（KE 传数值）：S20 完整航次（stop_time_s 时间终止）
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);
    assignin('base', 'ECMS_Lambda', lam);
    assignin('base', 'ECMS_K_soc', Ksoc);
    assignin('base', 'ECMS_K_E', KE);
    assignin('base', 'ECMS_EHat_Data', EHatData);
    if ~bdIsLoaded(MDL), load_system(fullfile(ROOT, 'model', [MDL '.slx'])); end
    SCN = evalin('base', 'SCENARIOS(20)');
    out = model.run_scenario_sim(MDL, SCN, law, SCN.stop_time_s);
    ds = model.extract_logsout(out);
    r.SOC_end = ds.SOC(end);
    r.fuel_total_kg = ds.fuel_total_kg(end);
    r.Vdc_min = min(ds.V_dc_V);
end

function setConfigValue(ROOT, name, val)
% 改写 config/Model_Parameters.m 中 'name = ...;' 赋值行并同步 canonical 镜像（md5 一致）
    tgt = fullfile(ROOT, 'config', 'Model_Parameters.m');
    txt = fileread(tgt);
    pat = sprintf('(?m)^(%s\\s*=\\s*)[^;]*;', name);
    assert(~isempty(regexp(txt, pat, 'once')), 'ECMS2:CALKE', 'config 未找到 %s 赋值行', name);
    newVal = sprintf('%s = %.6g;', name, val);
    txt2 = regexprep(txt, pat, newVal);
    if ~strcmp(txt, txt2)
        fid = fopen(tgt, 'w', 'n', 'UTF-8');
        fwrite(fid, txt2); fclose(fid);
    else
        fprintf('[calibrate_kE] %s 已是 %.6g（config 无需改写，幂等重跑）\n', name, val);
    end
    canon = fullfile(ROOT, 'scripts', '+model', 'canonical_data', 'Model_Parameters.m');
    assert(exist(canon, 'file') == 2, 'ECMS2:CALKE', 'canonical 镜像缺失 %s', canon);
    copyfile(tgt, canon, 'f');
    if ~isequal(model.md5file(tgt), model.md5file(canon))
        error('ECMS2:CALKE', '%s 与 canonical 镜像 md5 不一致', name);
    end
    fprintf('[calibrate_kE] %s = %.6g（config + canonical_data 已同步，md5 一致）\n', name, val);
end

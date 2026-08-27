function res = calibrate_lambda(ROOT)
% ecms2.calibrate_lambda — λ / K_soc 标定（T-104/105，FR-105/106，design §3.4）
%
%  1) T-104：S01 × law=4，二分搜索 λ₀ 使 SOC_end ∈ [0.595, 0.605]
%     - 初值区间 [0.2, 0.8]，上限 40 次迭代；每轮复用 T-103 运行封装
%       （Model_Parameters + Model_Scenarios + ensure_bsfc_params + SCN(1)
%        law=4 仿真 + extract_logsout，同 ecms2.run_s01_law4 管线）。
%     - 标定成功后把 λ₀ 写回 config/Model_Parameters.m（ECMS_Lambda）并同步
%       canonical_data 镜像（md5 一致）；不收敛 -> error（不得放宽终端带）。
%  2) T-105（law=5 变体已重建入模型时自动执行）：S01 × law=5，一维网格搜索
%     K_soc ∈ [0, 0.25, 0.5, 1, 1.5, 2, 3, 4, 6, 8, 10, 12, 14, 16, 20, 25, 30, 40]
%     kg/kWh/SOC，合格 = SOC_end ∈ [0.595, 0.605] 且 fuel ≤ fuel_law4×1.003
%     （不劣于 0.3 pp）；合格集内取燃油最低者（并列取更大 K_soc），仅记录不过门；
%     写回 config ECMS_K_soc 并同步 canonical。
%  产物：results/ecms_phase1/lambda_calibration.json
%  用法: res = ecms2.calibrate_lambda();   % ROOT 缺省 model/simulink_modelD
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    MDL = 'HybridShip_ModelD_DCBus';
    SOC_TARGET = 0.60;  BAND_LO = 0.595;  BAND_HI = 0.605;
    K_EQ = 0.4774;      % fuel_corr 折算系数 [kg/kWh]（design §3.1 基线 k_eq，T-104 口径）
    LAM_LO0 = 0.2;  LAM_HI0 = 0.8;  MAX_ITER = 40;

    % ================= T-104：λ 二分标定 =================
    fprintf('\n[41] ===== T-104 λ 标定（S01 × law=4，SOC_end∈[0.595,0.605]）=====\n');
    slxPath = fullfile(ROOT, 'model', [MDL '.slx']);
    assert(exist(slxPath, 'file') == 2, 'ECMS2:CAL', ...
        '%s 不存在（先运行 10_build_modelD.m）', slxPath);
    assert(exist(fullfile(ROOT, 'config', 'ecms_bsfc_override.mat'), 'file') == 2, ...
        'ECMS2:CAL', '缺少 config/ecms_bsfc_override.mat（先运行 ecms2.make_bsfc_override）');

    trace = struct([]);
    nIter = 0;  conv = false;  lam = NaN;
    rlast = struct('SOC_end', NaN, 'fuel_total_kg', NaN, 'Vdc_min', NaN);

    % ---- bracketing 检查 ----
    rLo = runLaw4Sim(ROOT, MDL, LAM_LO0);
    rHi = runLaw4Sim(ROOT, MDL, LAM_HI0);
    fprintf('[41] 端点: λ=%.4f -> SOC_end=%.4f | λ=%.4f -> SOC_end=%.4f\n', ...
        LAM_LO0, rLo.SOC_end, LAM_HI0, rHi.SOC_end);
    if rLo.SOC_end > BAND_HI || rHi.SOC_end < BAND_LO
        error('ECMS2:CAL:NOBRACKET', ...
            'λ 区间 [%.2f, %.2f] 未覆盖目标带 [%.3f, %.3f]（端点 SOC_end=%.4f/%.4f）——不得放宽终端带，Escalate', ...
            LAM_LO0, LAM_HI0, BAND_LO, BAND_HI, rLo.SOC_end, rHi.SOC_end);
    end

    lo = LAM_LO0; hi = LAM_HI0;
    for k = 1:MAX_ITER
        lamMid = 0.5 * (lo + hi);
        rMid = runLaw4Sim(ROOT, MDL, lamMid);
        nIter = k;
        trace(end+1).iter   = k;   %#ok<AGROW>
        trace(end).lambda   = lamMid;
        trace(end).SOC_end  = rMid.SOC_end;
        trace(end).fuel_kg  = rMid.fuel_total_kg;
        fprintf('[41] iter %2d: λ=%.6f -> SOC_end=%.4f fuel=%.2f kg\n', ...
            k, lamMid, rMid.SOC_end, rMid.fuel_total_kg);
        if rMid.SOC_end < BAND_LO
            lo = lamMid;          % SOC 偏低 -> λ 上调（电池变贵）
        elseif rMid.SOC_end > BAND_HI
            hi = lamMid;          % SOC 偏高 -> λ 下调
        else
            conv = true;  lam = lamMid;  rlast = rMid;
            break;
        end
    end
    if ~conv
        error('ECMS2:CAL:NOCONV', ...
            'λ 二分 %d 次迭代未使 SOC_end 入带（末轮 λ=%.6f SOC_end=%.4f）——Escalate', ...
            nIter, lamMid, rMid.SOC_end);
    end

    % ---- fuel_corr（k_eq=0.4774 折算，SOC 盈余折算回燃油）----
    BatCap = evalin('base', 'Bat_Cap_kWh');
    fuelCorr = rlast.fuel_total_kg - (rlast.SOC_end - SOC_TARGET) * BatCap * K_EQ;
    fprintf('[41] λ₀=%.6f | SOC_end=%.4f | fuel_raw=%.3f kg | fuel_corr=%.3f kg (k_eq=%.4f)\n', ...
        lam, rlast.SOC_end, rlast.fuel_total_kg, fuelCorr, K_EQ);

    % ---- 写回 config ECMS_Lambda + canonical 镜像同步 ----
    setConfigValue(ROOT, 'ECMS_Lambda', lam);
    assignin('base', 'ECMS_Lambda', lam);
    fprintf('[41] ECMS_Lambda 已写回 config 并同步 canonical_data\n');

    % ================= T-105：K_soc 一维网格标定（law=5） =================
    ksoc = struct('done', false, 'chosen_K_soc', NaN, 'grid', struct([]), ...
                  'final_SOC_end', NaN, 'final_fuel_kg', NaN, 'qualified', false);
    if bdIsLoaded(MDL), close_system(MDL, 0); end
    load_system(slxPath);
    v5 = find_system([MDL '/05_EMS_Controller'], 'SearchDepth', 1, ...
        'MatchFilter', @Simulink.match.allVariants, 'Name', 'EMS_SOC_Adaptive_ECMS');
    if isempty(v5)
        fprintf('[41] 模型不含 law=5 变体（EMS_SOC_Adaptive_ECMS）——K_soc 标定跳过（先重建模型）\n');
    else
        fprintf('\n[41] ===== T-105 K_soc 标定（S01 × law=5，SOC_ref=0.60）=====\n');
        fuelLaw4 = rlast.fuel_total_kg;
        kgrid = [0 0.25 0.5 1 1.5 2 3 4 6 8 10 12 14 16 20 25 30 40];
        % 选择准则：合格集（SOC_end 入带 且 fuel ≤ law4×1.003）内取燃油最低者
        % （ECMS 目标本身是节油；并列时取 K_soc 更大——SOC 调节更强）
        best = NaN; bestFuel = inf; bestSOC = NaN;
        for i = 1:numel(kgrid)
            K = kgrid(i);
            rK = runLaw5Sim(ROOT, MDL, lam, K);
            qualified = (rK.SOC_end >= BAND_LO && rK.SOC_end <= BAND_HI) && ...
                        rK.fuel_total_kg <= fuelLaw4 * 1.003;
            ksoc.grid(end+1).K_soc = K;      %#ok<AGROW>
            ksoc.grid(end).SOC_end = rK.SOC_end;
            ksoc.grid(end).fuel_kg = rK.fuel_total_kg;
            ksoc.grid(end).qualified = double(qualified);
            fprintf('[41] K_soc=%.2f -> SOC_end=%.4f fuel=%.2f kg (law4=%.2f) -> %s\n', ...
                K, rK.SOC_end, rK.fuel_total_kg, fuelLaw4, string(qualified));
            if qualified && (rK.fuel_total_kg < bestFuel - 1e-9 || ...
                    (abs(rK.fuel_total_kg - bestFuel) < 1e-9 && (isnan(best) || K > best)))
                best = K; bestSOC = rK.SOC_end; bestFuel = rK.fuel_total_kg;
            end
        end
        if isnan(best)
            fprintf('[41] K_soc 网格无合格点（SOC_end 入带且燃油 ≤ law4×1.003）——仅记录，Escalate 由主 Agent 定夺\n');
        else
            ksoc.done = true;
            ksoc.chosen_K_soc = best;
            ksoc.final_SOC_end = bestSOC;
            ksoc.final_fuel_kg = bestFuel;
            ksoc.qualified = true;
            fprintf('[41] 选定 K_soc=%.2f（合格集内燃油最低 %.2f kg）| SOC_end=%.4f\n', ...
                best, bestFuel, bestSOC);
            setConfigValue(ROOT, 'ECMS_K_soc', best);
            assignin('base', 'ECMS_K_soc', best);
            fprintf('[41] ECMS_K_soc 已写回 config 并同步 canonical_data\n');
        end
    end

    % ================= 结果文件 =================
    outDir = fullfile(ROOT, 'results', 'ecms_phase1');
    if ~exist(outDir, 'dir'), mkdir(outDir); end
    res = struct();
    res.task = 'T-104/T-105 lambda & K_soc calibration (S01, tier2 mid override)';
    ovTag = 'default (ASSUMED_BSFC_*)';
    ovFile = fullfile(ROOT, 'config', 'ecms_bsfc_override.mat');
    if exist(ovFile, 'file')
        ovinfo = load(ovFile, 'override_tag');
        ovTag = ovinfo.override_tag;
    end
    res.bsfc_override_tag = ovTag;
    res.lambda_0 = lam;
    res.lambda_interval = [LAM_LO0, LAM_HI0];
    res.n_iterations = nIter;
    res.converged = double(conv);
    res.soc_target = SOC_TARGET;
    res.band = [BAND_LO, BAND_HI];
    res.soc_end_final = rlast.SOC_end;
    res.fuel_raw_kg = rlast.fuel_total_kg;
    res.fuel_corr_kg = fuelCorr;
    res.k_eq_kg_kWh = K_EQ;
    res.bat_cap_kWh = BatCap;
    res.Vdc_min_V = rlast.Vdc_min;
    res.trace = trace;
    res.ksoc_calibration = ksoc;
    jsonTxt = jsonencode(res, 'PrettyPrint', true);
    jsonFile = fullfile(outDir, 'lambda_calibration.json');
    fid = fopen(jsonFile, 'w', 'n', 'UTF-8');
    fprintf(fid, '%s\n', jsonTxt);
    fclose(fid);
    fprintf('[41] 已写入 %s\n', jsonFile);
end

%% ================= 本地仿真封装（复用 T-103 运行管线） =================
function r = runLaw4Sim(ROOT, MDL, lam)
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);
    assignin('base', 'ECMS_Lambda', lam);
    if ~bdIsLoaded(MDL), load_system(fullfile(ROOT, 'model', [MDL '.slx'])); end
    SCN = evalin('base', 'SCENARIOS(1)');
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    out = model.run_scenario_sim(MDL, SCN, 4, stopTimeLimit);
    ds = model.extract_logsout(out);
    r.SOC_end = ds.SOC(end);
    r.fuel_total_kg = ds.fuel_total_kg(end);
    r.Vdc_min = min(ds.V_dc_V);
end

function r = runLaw5Sim(ROOT, MDL, lam, K)
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);
    assignin('base', 'ECMS_Lambda', lam);
    assignin('base', 'ECMS_K_soc', K);
    if ~bdIsLoaded(MDL), load_system(fullfile(ROOT, 'model', [MDL '.slx'])); end
    SCN = evalin('base', 'SCENARIOS(1)');
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    out = model.run_scenario_sim(MDL, SCN, 5, stopTimeLimit);
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
    assert(~isempty(regexp(txt, pat, 'once')), 'ECMS2:CAL', 'config 未找到 %s 赋值行', name);
    newVal = sprintf('%s = %.6g;', name, val);
    txt2 = regexprep(txt, pat, newVal);
    if ~strcmp(txt, txt2)
        fid = fopen(tgt, 'w', 'n', 'UTF-8');
        fwrite(fid, txt2); fclose(fid);
    else
        fprintf('[41] %s 已是 %.6g（config 无需改写，幂等重跑）\n', name, val);
    end
    canon = fullfile(ROOT, 'scripts', '+model', 'canonical_data', 'Model_Parameters.m');
    assert(exist(canon, 'file') == 2, 'ECMS2:CAL', 'canonical 镜像缺失 %s', canon);
    copyfile(tgt, canon, 'f');
    if ~isequal(model.md5file(tgt), model.md5file(canon))
        error('ECMS2:CAL', '%s 与 canonical 镜像 md5 不一致', name);
    end
    fprintf('[41] %s = %.6g（config + canonical_data 已同步，md5 一致）\n', name, val);
end

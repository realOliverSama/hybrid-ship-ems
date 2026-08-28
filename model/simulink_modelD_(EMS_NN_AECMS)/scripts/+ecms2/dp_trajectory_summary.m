function res = dp_trajectory_summary(ROOT)
% ecms2.dp_trajectory_summary — DP vs C2（law=5）S20 轨迹诊断摘要（T-110 报告素材，主 Agent 指定）
%
%  目的：回答"DP 到底怎么用时序搬移赚到 9.51%"——比较 DP 最优解（P_min=0 连续口径，
%  tier2）与实时 C2（law=5 SOC-Adaptive ECMS）在 S20 上的 SOC(t) / P_g(t) 轨迹差异。
%
%  输入（复用既有产物，不新增仿真语义）：
%    results/ecms_phase1/dp_S20_continuous_pmin000.mat   DP 解（sol.t/sol.SOC/sol.P_g_kW/sol.P_bat_kW）
%    S20 law=5 仿真轨迹（本函数内部重跑一次，复用 ecms2.calibrate_kS 的 runS20Sim 同管线）
%  输出：results/ecms_phase1/dp_s20_trajectory_summary.json
%    - dp：SOC 峰谷/均值/终点、P_g 统计（min/max/mean/怠速占比/爬坡区段数）、电池吞吐量
%    - c2：同口径 law=5 轨迹统计
%    - deviation：|ΔSOC(t)| 的 MAE/max/RMS（1 s 网格逐点），SOC(t) 关键时刻采样（每 120 s）
%    - energy：DP 与 C2 的净电池能量/吞吐量/燃油
%  用法: res = ecms2.dp_trajectory_summary();   % ROOT 缺省 model/simulink_modelD
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    MDL = 'HybridShip_ModelD_DCBus';
    dpFile = fullfile(ROOT, 'results', 'ecms_phase1', 'dp_S20_continuous_pmin000.mat');
    assert(exist(dpFile, 'file') == 2, 'ECMS2:DPTRJ', ...
        '缺少 DP 解 %s（先运行 40_build_composite_voyage.m）', dpFile);

    fprintf('\n[ecms2.dp_trajectory_summary] ===== DP vs C2 S20 轨迹诊断（T-110 素材） =====\n');
    D = load(dpFile);
    sol = D.sol;
    nT = numel(sol.t);
    socDp = sol.SOC(1:nT);                 % sol.SOC 比 sol.t 多 1（终端带网格点），对齐 t
    PgDp  = sol.P_g_kW(:);
    PbatDp = sol.P_bat_kW(:);

    % ---- C2（law=5）S20 轨迹：复用标定管线跑一次 ----
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);
    lam0 = evalin('base', 'ECMS_Lambda');
    Ksoc = evalin('base', 'ECMS_K_soc');
    prof = load(fullfile(ROOT, 'results', 'ecms_phase1', 'profiles', 'S20_composite_voyage.mat'));
    [~, ~, ~, infoO] = ecms2.oracle_future_energy(prof, 300, true);
    EHat = infoO.data;
    tic
    r5 = runS20Sim(ROOT, MDL, lam0, Ksoc, EHat);
    wall = toc;
    ds1 = model.resample_1s(r5.ds);          % 1 s 均匀网格（与 sol.t 对齐：0..5520，n=5521）
    socC2 = ds1.SOC;
    PgC2  = ds1.P_gen_actual_kW;
    PbatC2 = ds1.P_bat_actual_kW;
    fprintf('[dp_trajectory_summary] C2 law=5 重跑完成（%.0f s）：SOC_end=%.4f fuel=%.2f kg\n', ...
        wall, r5.SOC_end, r5.fuel_total_kg);

    % ---- 对齐（1 s 网格，长度须一致）----
    n = min(numel(sol.t), numel(socC2));
    t = sol.t(1:n);
    socDp = socDp(1:n); socC2 = socC2(1:n);
    PgDp = PgDp(1:n);  PgC2 = PgC2(1:n);
    PbatDp = PbatDp(1:n); PbatC2 = PbatC2(1:n);

    % ---- 统计 ----
    dSoc = socDp - socC2;
    stats = @(x) struct('min', min(x), 'max', max(x), 'mean', mean(x), ...
        'start', x(1), 'end', x(end));
    idleFrac = @(x) mean(x < 1e-6);
    thr = @(x) sum(x / 3600);              % kW·1s -> kWh

    dp = struct( ...
        'SOC', stats(socDp), ...
        'P_g_kW', stats(PgDp), 'P_g_idle_frac', idleFrac(PgDp), ...
        'P_g_high_frac_gt800', mean(PgDp > 800), ...
        'E_bat_net_kWh', thr(PbatDp), 'E_bat_throughput_kWh', thr(abs(PbatDp)), ...
        'fuel_kg', sol.fuel_total_kg);
    c2 = struct( ...
        'SOC', stats(socC2), ...
        'P_g_kW', stats(PgC2), 'P_g_idle_frac', idleFrac(PgC2), ...
        'P_g_high_frac_gt800', mean(PgC2 > 800), ...
        'E_bat_net_kWh', thr(PbatC2), 'E_bat_throughput_kWh', thr(abs(PbatC2)), ...
        'fuel_kg', r5.fuel_total_kg);
    dev = struct( ...
        'n_points', n, ...
        'dSOC_mae', mean(abs(dSoc)), 'dSOC_max', max(abs(dSoc)), ...
        'dSOC_rms', sqrt(mean(dSoc .^ 2)), ...
        'dSOC_end', dSoc(end), ...
        'sampled_every_120s', [t(1:120:n).', socDp(1:120:n).', socC2(1:120:n).', dSoc(1:120:n).']);
    en = struct( ...
        'dp_fuel_kg', sol.fuel_total_kg, 'c2_fuel_kg', r5.fuel_total_kg, ...
        'saving_kg', sol.fuel_total_kg - r5.fuel_total_kg, ...
        'saving_pct', 100 * (sol.fuel_total_kg - r5.fuel_total_kg) / r5.fuel_total_kg);

    res = struct();
    res.task = 'DP vs C2 (law=5) S20 trajectory diagnostic (T-110 报告素材)';
    res.dp_source = dpFile;
    res.dp = dp;
    res.c2 = c2;
    res.deviation = dev;
    res.energy = en;
    res.note = 'DP 为 P_min=0 连续口径 tier2（T-102）；C2 为 law=5 SOC-Adaptive ECMS（λ₀=0.425, K_soc=30）；1 s 网格逐点比对';

    outDir = fullfile(ROOT, 'results', 'ecms_phase1');
    if ~exist(outDir, 'dir'), mkdir(outDir); end
    jsonTxt = jsonencode(res, 'PrettyPrint', true);
    jsonFile = fullfile(outDir, 'dp_s20_trajectory_summary.json');
    fid = fopen(jsonFile, 'w', 'n', 'UTF-8');
    fprintf(fid, '%s\n', jsonTxt);
    fclose(fid);

    fprintf('\n[dp_trajectory_summary] ---- 摘要 ----\n');
    fprintf('  DP : SOC [%.4f, %.4f] end=%.4f | P_g [%.0f, %.0f] kW idle=%.1f%% | E_bat_net=%.1f kWh 吞吐=%.1f kWh | fuel=%.2f kg\n', ...
        dp.SOC.min, dp.SOC.max, dp.SOC.end, dp.P_g_kW.min, dp.P_g_kW.max, ...
        100 * dp.P_g_idle_frac, dp.E_bat_net_kWh, dp.E_bat_throughput_kWh, dp.fuel_kg);
    fprintf('  C2 : SOC [%.4f, %.4f] end=%.4f | P_g [%.0f, %.0f] kW idle=%.1f%% | E_bat_net=%.1f kWh 吞吐=%.1f kWh | fuel=%.2f kg\n', ...
        c2.SOC.min, c2.SOC.max, c2.SOC.end, c2.P_g_kW.min, c2.P_g_kW.max, ...
        100 * c2.P_g_idle_frac, c2.E_bat_net_kWh, c2.E_bat_throughput_kWh, c2.fuel_kg);
    fprintf('  |ΔSOC| : MAE=%.4f max=%.4f RMS=%.4f | fuel saving=%.2f kg (%.2f%%)\n', ...
        dev.dSOC_mae, dev.dSOC_max, dev.dSOC_rms, en.saving_kg, en.saving_pct);
    fprintf('[dp_trajectory_summary] 已写入 %s\n', jsonFile);
end

function r = runS20Sim(ROOT, MDL, lam, Ksoc, EHatData)
% law=5 × S20 完整航次（复用 calibrate_kS 同管线；返回完整 ds 供轨迹比对）
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);
    assignin('base', 'ECMS_Lambda', lam);
    assignin('base', 'ECMS_K_soc', Ksoc);
    assignin('base', 'ECMS_K_S', 0);
    assignin('base', 'ECMS_EHat_Data', EHatData);
    if ~bdIsLoaded(MDL), load_system(fullfile(ROOT, 'model', [MDL '.slx'])); end
    SCN = evalin('base', 'SCENARIOS(20)');
    out = model.run_scenario_sim(MDL, SCN, 5, SCN.stop_time_s);
    ds = model.extract_logsout(out);
    r.SOC_end = ds.SOC(end);
    r.fuel_total_kg = ds.fuel_total_kg(end);
    r.Vdc_min = min(ds.V_dc_V);
    r.ds = ds;
end

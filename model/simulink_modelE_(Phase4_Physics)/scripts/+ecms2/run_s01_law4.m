function r = run_s01_law4(ROOT)
% ecms2.run_s01_law4 — S01 × Fixed ECMS（EMS_CTRL_LAW=4）验收运行（T-103）
%
%  完整 S01（voyage_distance_m=5000，静水，initial_SOC=0.60）以 law=4 仿真，
%  输出 fuel_total_kg / SOC_end / Vdc_min 与 P_gen_ref（ECMS 设定）行为摘要，
%  写入 results/ecms_phase1/s01_law4_fixed_ecms.json（机器可读）并打印。
%
%  前置: 10_build_modelD.m 已重建模型（含 law=4 变体）。
%  用法: r = ecms2.run_s01_law4();   % ROOT 缺省 model/simulink_modelD
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    fprintf('\n[ecms2.run_s01_law4] ===== S01 × Fixed ECMS（law=4）验收运行（T-103） =====\n');
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);

    MDL = 'HybridShip_ModelD_DCBus';
    slxPath = fullfile(ROOT, 'model', [MDL '.slx']);
    assert(exist(slxPath, 'file') == 2, 'ECMS2:RUN', ...
        '%s 不存在（先运行 10_build_modelD.m）', slxPath);
    if ~bdIsLoaded(MDL)
        load_system(slxPath);
    end

    SCN = evalin('base', 'SCENARIOS(1)');          % 完整 S01
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    tic
    out = model.run_scenario_sim(MDL, SCN, 4, stopTimeLimit);
    wall = toc;
    ds = model.extract_logsout(out);

    r = struct();
    r.scenario  = SCN.name;
    r.law       = 4;
    r.lambda    = evalin('base', 'ECMS_Lambda');
    r.bsfc_source = ecms2.ensure_bsfc_params(ROOT).source;
    r.duration_s  = ds.t(end);
    r.fuel_total_kg = ds.fuel_total_kg(end);
    r.SOC_end      = ds.SOC(end);
    r.SOC_min      = min(ds.SOC);
    r.SOC_max      = max(ds.SOC);
    r.Vdc_min_V    = min(ds.V_dc_V);
    r.P_gen_ref_max_kW = max(ds.P_gen_ref_kW);
    r.P_gen_actual_max_kW = max(ds.P_gen_actual_kW);
    r.P_bat_min_kW  = min(ds.P_bat_actual_kW);     % 负=充电
    r.P_bat_max_kW  = max(ds.P_bat_actual_kW);     % 正=放电
    r.wall_time_s   = wall;

    fprintf('\n[ecms2.run_s01_law4] ---- 结果 ----\n');
    fprintf('  duration        = %.0f s\n', r.duration_s);
    fprintf('  fuel_total_kg   = %.2f kg（验收参考 ~80 kg ±20%%）\n', r.fuel_total_kg);
    fprintf('  SOC_end         = %.4f (SOC_min=%.4f, SOC_max=%.4f)\n', ...
        r.SOC_end, r.SOC_min, r.SOC_max);
    fprintf('  Vdc_min         = %.1f V\n', r.Vdc_min_V);
    fprintf('  P_gen_ref max   = %.0f kW | P_gen_actual max = %.0f kW\n', ...
        r.P_gen_ref_max_kW, r.P_gen_actual_max_kW);
    fprintf('  P_bat range     = [%.0f, %.0f] kW（负=充电，正=放电）\n', ...
        r.P_bat_min_kW, r.P_bat_max_kW);
    fprintf('  wall_time       = %.1f s\n', wall);

    % ---- SOC 轨迹走向（λ 默认值行为描述）----
    % 以 20% 分位粗描：SOC 起点 0.60；Fixed ECMS（λ≈k_eq）倾向 SOC 平衡，
    % 起航低负荷段（P_load≈100）充电、高负荷段放电。
    q = prctile(ds.SOC, [0 25 50 75 100]);
    fprintf('  SOC 分位 [min p25 p50 p75 max] = %s\n', mat2str(q, 4));
    r.SOC_quartiles = q;

    outDir = fullfile(ROOT, 'results', 'ecms_phase1');
    if ~exist(outDir, 'dir'), mkdir(outDir); end
    model.write_json(fullfile(outDir, 's01_law4_fixed_ecms.json'), ...
        struct('scenario', r.scenario, 'law', r.law, 'lambda', r.lambda, ...
               'bsfc_source', r.bsfc_source, 'duration_s', r.duration_s, ...
               'fuel_total_kg', r.fuel_total_kg, 'SOC_end', r.SOC_end, ...
               'SOC_min', r.SOC_min, 'SOC_max', r.SOC_max, ...
               'Vdc_min_V', r.Vdc_min_V, ...
               'P_gen_ref_max_kW', r.P_gen_ref_max_kW, ...
               'P_bat_max_kW', r.P_bat_max_kW, 'wall_time_s', wall));
    fprintf('[ecms2.run_s01_law4] 已写入 %s\n', ...
        fullfile(outDir, 's01_law4_fixed_ecms.json'));
end

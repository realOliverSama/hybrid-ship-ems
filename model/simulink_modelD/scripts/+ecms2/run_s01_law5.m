function r = run_s01_law5(ROOT)
% ecms2.run_s01_law5 — S01 × SOC-Adaptive ECMS（EMS_CTRL_LAW=5）验收运行（T-105）
%
%  完整 S01（voyage_distance_m=5000，静水，initial_SOC=0.60）以 law=5 仿真，
%  λ(t) = λ₀ + K_soc·(SOC_ref − SOC)，输出 fuel_total_kg / SOC_end / Vdc_min
%  与 P_gen_ref（ECMS 设定）行为摘要，写入
%  results/ecms_phase1/s01_law5_adaptive_ecms.json（机器可读）并打印。
%
%  前置: 10_build_modelD.m 已重建模型（含 law=5 变体）。
%  用法: r = ecms2.run_s01_law5();   % ROOT 缺省 model/simulink_modelD
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    fprintf('\n[ecms2.run_s01_law5] ===== S01 × SOC-Adaptive ECMS（law=5）验收运行（T-105） =====\n');
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
    out = model.run_scenario_sim(MDL, SCN, 5, stopTimeLimit);
    wall = toc;
    ds = model.extract_logsout(out);

    r = struct();
    r.scenario  = SCN.name;
    r.law       = 5;
    r.lambda_0  = evalin('base', 'ECMS_Lambda');
    r.K_soc     = evalin('base', 'ECMS_K_soc');
    r.SOC_ref   = evalin('base', 'ECMS_SOC_Ref');
    r.bsfc_source = ecms2.ensure_bsfc_params(ROOT).source;
    r.duration_s  = ds.t(end);
    r.fuel_total_kg = ds.fuel_total_kg(end);
    r.SOC_end      = ds.SOC(end);
    r.SOC_min      = min(ds.SOC);
    r.SOC_max      = max(ds.SOC);
    r.Vdc_min_V    = min(ds.V_dc_V);
    r.P_gen_ref_max_kW = max(ds.P_gen_ref_kW);
    r.P_bat_min_kW  = min(ds.P_bat_actual_kW);     % 负=充电
    r.P_bat_max_kW  = max(ds.P_bat_actual_kW);     % 正=放电
    r.wall_time_s   = wall;

    fprintf('\n[ecms2.run_s01_law5] ---- 结果 ----\n');
    fprintf('  duration        = %.0f s\n', r.duration_s);
    fprintf('  lambda_0        = %.4f kg/kWh | K_soc = %.2f kg/kWh/SOC | SOC_ref = %.2f\n', ...
        r.lambda_0, r.K_soc, r.SOC_ref);
    fprintf('  fuel_total_kg   = %.2f kg\n', r.fuel_total_kg);
    fprintf('  SOC_end         = %.4f (SOC_min=%.4f, SOC_max=%.4f)\n', ...
        r.SOC_end, r.SOC_min, r.SOC_max);
    fprintf('  Vdc_min         = %.1f V\n', r.Vdc_min_V);
    fprintf('  P_gen_ref max   = %.0f kW | P_bat range = [%.0f, %.0f] kW\n', ...
        r.P_gen_ref_max_kW, r.P_bat_min_kW, r.P_bat_max_kW);
    fprintf('  wall_time       = %.1f s\n', wall);

    outDir = fullfile(ROOT, 'results', 'ecms_phase1');
    if ~exist(outDir, 'dir'), mkdir(outDir); end
    model.write_json(fullfile(outDir, 's01_law5_adaptive_ecms.json'), ...
        struct('scenario', r.scenario, 'law', r.law, 'lambda_0', r.lambda_0, ...
               'K_soc', r.K_soc, 'SOC_ref', r.SOC_ref, ...
               'bsfc_source', r.bsfc_source, 'duration_s', r.duration_s, ...
               'fuel_total_kg', r.fuel_total_kg, 'SOC_end', r.SOC_end, ...
               'SOC_min', r.SOC_min, 'SOC_max', r.SOC_max, ...
               'Vdc_min_V', r.Vdc_min_V, ...
               'P_gen_ref_max_kW', r.P_gen_ref_max_kW, ...
               'P_bat_max_kW', r.P_bat_max_kW, 'wall_time_s', wall));
    fprintf('[ecms2.run_s01_law5] 已写入 %s\n', ...
        fullfile(outDir, 's01_law5_adaptive_ecms.json'));
end

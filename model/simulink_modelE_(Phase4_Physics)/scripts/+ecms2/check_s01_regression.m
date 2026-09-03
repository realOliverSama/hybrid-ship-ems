function r = check_s01_regression(ROOT)
% ecms2.check_s01_regression — S01 回归检查（FR-101，T-101 验收项）
% 模型重建（10_build_modelD.m）后，重跑 S01 Baseline 并导出 P_demand，
% 与 Phase 0 剖面 results/ecms_phase0/profiles/S01_still_water.mat 逐位一致（容差 0）。
%
% 前置：模型已重建；本函数不写任何结果文件（纯校验）。
% 用法：r = ecms2.check_s01_regression();   % ROOT 缺省为 model/simulink_modelD
% 输出 r：字段级 max|diff| 与 pass 标志（P_demand 容差 0 为通过判据）。
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
    % 模型编译参数须在 base workspace（run 在函数内执行于局部工作区，故用 evalin）
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    MDL = 'HybridShip_ModelD_DCBus';
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));

    ref = ecms.load_profile(ROOT, 'S01_still_water');
    SCN = evalin('base', 'SCENARIOS(1)');
    % 与 30_export_load_profiles.m 完全一致的 StopTime 安全上限
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    prof = ecms.run_profile_sim(ROOT, SCN, 0, stopTimeLimit);

    r = struct();
    r.duration_s     = prof.duration_s;
    r.duration_ref_s = ref.duration_s;
    r.n_samples      = numel(prof.t);
    r.n_samples_ref  = numel(ref.t);
    cmpFlds = {'P_demand_kW','P_prop_demand_kW','P_hotel_kW','P_gen_kW','P_bat_kW', ...
        'SOC','V_dc_V','v_ship_mps','x_ship_m','P_load_total_kW','P_bus_residual_kW'};
    r.max_diff = struct();
    for i = 1:numel(cmpFlds)
        f = cmpFlds{i};
        if numel(prof.(f)) == numel(ref.(f))
            r.max_diff.(f) = max(abs(prof.(f) - ref.(f)));
        else
            r.max_diff.(f) = NaN;   % 长度不一致 -> 逐位比较不成立
        end
    end
    r.max_diff_P_demand = r.max_diff.P_demand_kW;
    r.pass = (r.n_samples == r.n_samples_ref) && (r.max_diff_P_demand == 0);

    fprintf('[ecms2.check_s01_regression] duration new=%.0f s ref=%.0f s | samples %d/%d\n', ...
        prof.duration_s, ref.duration_s, numel(prof.t), numel(ref.t));
    for i = 1:numel(cmpFlds)
        f = cmpFlds{i};
        fprintf('  max|d%s| = %.3g\n', f, r.max_diff.(f));
    end
    fprintf('[ecms2.check_s01_regression] PASS=%d (判据：P_demand 逐位一致，容差 0)\n', r.pass);
end

function report = verify_demand_invariance(ROOT)
% ecms.verify_demand_invariance — T-007：P_demand 双控制律因果隔离不变性验证（FR-006）
%
% 同一提取逻辑（ecms.run_profile_sim，P_demand = P_prop_demand_kW + P_hotel_kW）分别以：
%   EMS_CTRL_LAW=0（复用 30_export_load_profiles.m 导出的剖面 .mat）与
%   EMS_CTRL_LAW=1（AI_Enhanced，AI_Pred_Source=0 persistence，重新仿真）
% 对 8 个入选场景 {S01,S02,S03,S04,S07,S08,S09,S10} 各提取一条 P_demand(t)，逐点比较：
%   max_dev = max|P0 - P1| / max(peak(P0), peak(P1))；< 0.1% 判 PASS。
%
% 输出:
%   results/ecms_phase0/profiles/invariance_report.json
%     { overall, tolerance, config_hash, generated, per_scenario: {...} }
%   stdout 逐场景表。任何场景超差 -> overall=false，调用方必须 Escalate（不得放行），
%   本函数不静默放宽判据。
%
% 前置: 已运行 config/Model_Parameters.m、config/Model_Scenarios.m，且
%       results/ecms_phase0/profiles/<SCN>.mat 已由 30 导出；load_system 在函数内完成。
    if nargin < 1
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end

    % ---- 环境（参数必须在 base 工作区：模型掩码表达式在 base 求值）----
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
    model.ensure_canonical(ROOT);
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    MDL = 'HybridShip_ModelD_DCBus';
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));

    scenIDs = [1 2 3 4 7 8 9 10];
    stopTimeLimit = 5000 / 5.66 + 1800;
    TOL = 1e-3;   % 0.1%
    SCENARIOS = evalin('base', 'SCENARIOS');   % 工况定义在 base 工作区

    profilesDir = fullfile(ROOT, 'results', 'ecms_phase0', 'profiles');
    if ~exist(profilesDir, 'dir')
        error('ECMS:INVARIANCE', '缺少剖面目录 %s（先运行 30_export_load_profiles.m）', profilesDir);
    end

    paramHash = model.md5file(fullfile(ROOT, 'config', 'Model_Parameters.m'));
    scenHash  = model.md5file(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

    fprintf('\n[ECMS-INV] ===== T-007 P_demand 双控制律不变性验证（FR-006） =====\n');
    fprintf('[ECMS-INV] 场景=%d，判据 max|P0-P1|/peak < %.1f%%\n', numel(scenIDs), 100 * TOL);

    per = struct();
    overall = true;
    for k = 1:numel(scenIDs)
        SCN = SCENARIOS(scenIDs(k));
        fprintf('\n[ECMS-INV] (%d/%d) %s\n', k, numel(scenIDs), SCN.name);

        % ---- law=0：复用 30 导出的剖面（同一提取逻辑产出）----
        p0 = ecms.load_profile(ROOT, SCN.name);
        if ~isfield(p0, 'P_demand_kW')
            error('ECMS:INVARIANCE', '%s 剖面缺少 P_demand_kW（版本不匹配，请重跑 30）', SCN.name);
        end

        % ---- law=1：同一提取逻辑重新仿真（AI_Pred_Source=0 persistence）----
        tic;
        p1 = ecms.run_profile_sim(ROOT, SCN, 1, stopTimeLimit);
        wall = toc;

        % ---- 逐点比较（1 s 网格；长度不一致则取公共前缀并记录）----
        t0 = p0.t; t1 = p1.t;
        n = min(numel(t0), numel(t1));
        P0 = p0.P_demand_kW(1:n);
        P1 = p1.P_demand_kW(1:n);
        peak = max(max(P0), max(P1));
        max_dev = max(abs(P0 - P1)) / peak;
        pass = max_dev < TOL;

        per.(SCN.name) = struct( ...
            'max_dev', max_dev, ...
            'tolerance', TOL, ...
            'pass', pass, ...
            'n_samples_compared', n, ...
            'duration_law0_s', p0.duration_s, ...
            'duration_law1_s', p1.duration_s, ...
            'duration_mismatch_s', numel(t0) - numel(t1), ...
            'peak_law0_kW', max(P0), ...
            'peak_law1_kW', max(P1), ...
            'derate_event_law0', p0.derate_event, ...
            'derate_event_law1', p1.derate_event, ...
            'law1_wall_time_s', wall);

        if pass
            fprintf('[ECMS-INV][PASS] max_dev=%.4e (<%.1f%%) n=%d dur0=%.0f dur1=%.0f%s\n', ...
                max_dev, 100 * TOL, n, p0.duration_s, p1.duration_s, ...
                ternary(p1.derate_event, ' [WARN law1 derate]', ''));
        else
            fprintf('[ECMS-INV][FAIL] max_dev=%.4e (>=%.1f%%) n=%d dur0=%.0f dur1=%.0f%s\n', ...
                max_dev, 100 * TOL, n, p0.duration_s, p1.duration_s, ...
                ternary(p1.derate_event, ' [law1 derate]', ''));
            overall = false;
        end
    end

    % ---- 报告 ----
    report = struct();
    report.overall    = overall;
    report.tolerance  = TOL;
    report.extract_point = 'P_prop_demand_kW + P_hotel_kW (upstream of derate & EMS)';
    report.config_hash = [paramHash(1:8) scenHash(1:8)];
    report.generated  = datestr(now, 'yyyy-mm-dd HH:MM:SS');
    report.per_scenario = per;

    jsonPath = fullfile(profilesDir, 'invariance_report.json');
    ecms.write_json_deep(jsonPath, report);
    fprintf('\n[ECMS-INV] 报告已写入 %s\n', jsonPath);
    fprintf('[ECMS-INV] overall = %s\n', string(overall));
    if ~overall
        fprintf('[ECMS-INV][!!] 存在超差场景：P_demand 控制不变性不成立，禁止放行（须 Escalate）。\n');
    end
end

function s = ternary(cond, a, b)
    if cond, s = a; else, s = b; end
end

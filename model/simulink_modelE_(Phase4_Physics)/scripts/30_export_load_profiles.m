%% ========================================================================
%  30_export_load_profiles.m — ECMS Phase 0：8 场景负荷剖面导出（T-006，FR-006/FR-007）
%  产出:
%    results/ecms_phase0/profiles/<SCN_name>.mat     P_demand(t) + Baseline 参照轨迹
%    results/ecms_phase0/profiles/profiles_summary.json  每场景摘要（机器可读）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）:
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/30_export_load_profiles.m'))"
%  说明:
%    - 8 场景 {S01,S02,S03,S04,S07,S08,S09,S10}（FR-007），Baseline EMS
%      （EMS_CTRL_LAW=0，AI_Pred_Source=0 无预测），航程按既有场景定义 5000 m 终止；
%    - P_demand = P_prop_demand_kW + P_hotel_kW（速度 PI 输出上游，因果隔离；
%      证据见 +ecms/run_profile_sim.m 头注释）；两信号已由模型日志配置写入 logsout，
%      不改 .slx；
%    - 三档燃油 = 对同一条 P_gen(t) 用 T-004 bsfc_family.mat 三档聚合曲线离线积分
%      （不重跑仿真；BSFC 表输入为聚合发电机功率）；
%    - 欠压降额事件（Vdc<600 V 或降额激活或 P_unserved>0）-> 该场景剖面作废
%      （derate_event=true）并在摘要中注明（FR-006）；
%    - 能量闭合：η_rect·P_gen + P_bat(母线侧) ≈ P_demand，残差 <0.1% 判通过
%      （design §4.1 效率链）。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[30] ===== ECMS Phase 0 — 8 场景负荷剖面导出（T-006，FR-006/007） =====\n');
fprintf('[30] ROOT = %s\n', ROOT);

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

MDL = 'HybridShip_ModelD_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

% ---- T-004 BSFC 曲线族（三档聚合曲线）----
bsfcFile = fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc', 'bsfc_family.mat');
if ~exist(bsfcFile, 'file')
    error('ECMS:PROFILES', '缺少 T-004 产物 %s（先运行 31_build_bsfc_family.m）', bsfcFile);
end
F = load(bsfcFile);
fprintf('[30] BSFC 曲线族已加载（version=%s，三档 d25=%.2f/%.2f/%.2f）\n', ...
    F.version, F.tiers(1).d25, F.tiers(2).d25, F.tiers(3).d25);

% ---- 入选场景集（FR-007）：S01,S02,S03,S04,S07,S08,S09,S10 ----
scenIDs = [1 2 3 4 7 8 9 10];

% ---- 参数摘要哈希 ----
paramHash = model.md5file(fullfile(ROOT, 'config', 'Model_Parameters.m'));
scenHash  = model.md5file(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

outDir = fullfile(ROOT, 'results', 'ecms_phase0', 'profiles');
if ~exist(outDir, 'dir'), mkdir(outDir); end

stopTimeLimit = 5000 / 5.66 + 1800;   % 与 phase1 脚本一致的 StopTime 安全上限
LAW = 0;                              % Baseline EMS（无预测）

fprintf('[30] 入选场景=%d  EMS_CTRL_LAW=%d  StopTime上限=%.0f s\n', numel(scenIDs), LAW, stopTimeLimit);

%% ======================= 逐场景仿真与导出 =======================
summary = struct();
for k = 1:numel(scenIDs)
    SCN = SCENARIOS(scenIDs(k));
    fprintf('\n[30] (%d/%d) %s ... ', k, numel(scenIDs), SCN.name);
    tic;
    try
        prof = ecms.run_profile_sim(ROOT, SCN, LAW, stopTimeLimit);
    catch err
        fprintf('仿真失败: %s\n', err.message);
        for q = 1:numel(err.stack)
            fprintf('    at %s line %d\n', err.stack(q).name, err.stack(q).line);
        end
        error('ECMS:PROFILES', '%s 仿真失败，中止（保留已产出剖面）', SCN.name);
    end
    wall = toc;

    % ---- 三档燃油（离线积分，不重跑仿真；BSFC 表输入为聚合发电机功率）----
    for t = 1:3
        bp = F.tiers(t).agg_BP_kW;
        tb = F.tiers(t).agg_table_gkWh;
        % fuel [kg] = Σ P[kW] * BSFC[g/kWh] / 3.6e6 * dt[s]，dt = 1 s
        prof.(sprintf('fuel_base_tier%d_kg', t)) = sum(prof.P_gen_kW .* ...
            interp1(bp, tb, prof.P_gen_kW, 'linear', 'extrap')) / 3.6e6;
    end

    % ---- 能量闭合（在 run_profile_sim 内以原始 0.01s 网格计算，design §4.1）----
    closure_residual_frac = prof.energy_closure.residual_frac;
    closure_pass = prof.energy_closure.pass;

    % ---- 元数据（场景元数据 + 参数摘要哈希）----
    prof.meta = struct( ...
        'model', MDL, 'law', LAW, 'AI_Pred_Source', 0, ...
        'param_hash', paramHash, 'scenario_hash', scenHash, ...
        'bsfc_family_version', F.version, ...
        'generated', datestr(now, 'yyyy-mm-dd HH:MM:SS'), ...
        'extract_point', 'P_prop_demand_kW + P_hotel_kW (speed PI output, upstream of derate & EMS)');

    % ---- 保存 .mat（顶层字段展开）----
    matPath = fullfile(outDir, [SCN.name '.mat']);
    save(matPath, '-struct', 'prof');
    fprintf('[30] 已写入 %s\n', matPath);

    % ---- 摘要记录 ----
    summary.(SCN.name) = struct( ...
        'duration_s', prof.duration_s, ...
        'P_demand_peak_kW', max(prof.P_demand_kW), ...
        'P_demand_mean_kW', mean(prof.P_demand_kW), ...
        'SOC_end', prof.SOC(end), ...
        'Vdc_min_V', prof.Vdc_min_V, ...
        'derate_event', prof.derate_event, ...
        'n_derate_samples', prof.n_derate_samples, ...
        'n_undervoltage_samples', prof.n_undervoltage_samples, ...
        'E_unserved_kWh', prof.E_unserved_kWh, ...
        'closure_residual_frac', closure_residual_frac, ...
        'closure_pass', closure_pass, ...
        'closure_residual_1s_frac', prof.energy_closure.residual_1s_frac, ...
        'fuel_total_kg_ref', prof.fuel_total_kg, ...
        'fuel_tier1_kg', prof.fuel_base_tier1_kg, ...
        'fuel_tier2_kg', prof.fuel_base_tier2_kg, ...
        'fuel_tier3_kg', prof.fuel_base_tier3_kg, ...
        'wall_time_s', wall);

    tag = '';
    if prof.derate_event
        tag = '  [DERATE EVENT -> 剖面作废]';
    elseif ~closure_pass
        tag = '  [WARN closure>0.1%%]';
    end
    fprintf('[30] 完成 t_end=%.0f s | P_demand [%.0f, %.0f] kW (mean %.0f) | SOC_end=%.3f | Vdc_min=%.0f V | 闭合残差=%.4f%% | 燃油T1/T2/T3=%.2f/%.2f/%.2f kg | %.0f s wall%s\n', ...
        prof.duration_s, min(prof.P_demand_kW), max(prof.P_demand_kW), mean(prof.P_demand_kW), ...
        prof.SOC(end), prof.Vdc_min_V, 100 * closure_residual_frac, ...
        prof.fuel_base_tier1_kg, prof.fuel_base_tier2_kg, prof.fuel_base_tier3_kg, wall, tag);
end

%% ======================= 摘要 JSON =======================
ecms.write_json_deep(fullfile(outDir, 'profiles_summary.json'), summary);

%% ======================= 汇总表 =======================
fprintf('\n[30] ==================== 每场景摘要 ====================\n');
fprintf('%-22s %6s %8s %8s %7s %8s %8s %6s %10s %8s %8s %8s\n', ...
    'SCN', 'dur[s]', 'Pd_peak', 'Pd_mean', 'SOC_end', 'Vdc_min', 'closure%', 'derate', 'fuel_t1', 'fuel_t2', 'fuel_t3', 'fuel_ref');
names = fieldnames(summary);
for k = 1:numel(names)
    s = summary.(names{k});
    fprintf('%-22s %6.0f %8.0f %8.0f %7.3f %8.0f %8.4f %6d %10.2f %8.2f %8.2f %8.2f\n', ...
        names{k}, s.duration_s, s.P_demand_peak_kW, s.P_demand_mean_kW, s.SOC_end, ...
        s.Vdc_min_V, 100 * s.closure_residual_frac, double(s.derate_event), ...
        s.fuel_tier1_kg, s.fuel_tier2_kg, s.fuel_tier3_kg, s.fuel_total_kg_ref);
end
fprintf('\n[30] 完成：%d 个剖面 -> %s\n', numel(scenIDs), outDir);
fprintf('[30] 注：fuel_t1/t2/t3 = 三档 BSFC 离线积分（供 DP 基线对照）；fuel_ref = 模型内默认表积分（仅供参考）。\n');

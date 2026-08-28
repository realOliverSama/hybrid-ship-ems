%% ========================================================================
%  40_build_composite_voyage.m — ECMS Phase 1：S20 复合航次剖面导出 + DP 上限（T-102，FR-103）
%  产出:
%    results/ecms_phase1/profiles/S20_composite_voyage.mat   S20 Baseline 剖面
%        （字段结构与 Phase 0 剖面一致 + 三档 Baseline 燃油离线积分）
%    results/ecms_phase1/profiles/S20_summary.json           摘要（机器可读）
%    results/ecms_phase1/dp_S20_continuous_pmin000.mat        DP 解（tier2，连续口径，P_min=0）
%    results/ecms_phase1/dp_S20_continuous_pmin400.mat        DP 解（tier2，连续口径，P_min=400）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）:
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/40_build_composite_voyage.m'))"
%  前置: 10_build_modelD.m 已重建模型；31_build_bsfc_family.m 已产出 bsfc_family.mat；
%        T-101 S01 回归已通过（ecms2.check_s01_regression）。
%  语义要点:
%    - S20 时间终止（stop_time_s=5520，主）；里程作完整性核对（voyage_distance_m 放宽）；
%    - 降额事件出现即 error Escalate（FR-006 因果隔离）；能量闭合 <0.1% 判过；
%    - 三档燃油 = 对同一条 P_gen(t) 用 T-004 bsfc_family.mat 三档聚合曲线离线积分；
%    - DP：tier2 中档，连续口径，P_min ∈ {0,400}；dp_checks 内置校验不可绕过。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[40] ===== ECMS Phase 1 — S20 复合航次剖面 + DP 上限（T-102，FR-103） =====\n');
fprintf('[40] ROOT = %s\n', ROOT);

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

MDL = 'HybridShip_ModelD_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

% ---- T-004 BSFC 曲线族（三档聚合曲线）----
bsfcFile = fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc', 'bsfc_family.mat');
if ~exist(bsfcFile, 'file')
    error('ECMS:S20', '缺少 T-004 产物 %s（先运行 31_build_bsfc_family.m）', bsfcFile);
end
F = load(bsfcFile);

% ---- S20 场景（参数唯一源 +ecms2/voyage_s20.m；单一源一致性核对）----
S20DEF = ecms2.voyage_s20();
assert(isfield(SCENARIOS, 'id') && numel(SCENARIOS) >= 20 && SCENARIOS(20).id == 20, ...
    'ECMS:S20', 'config/Model_Scenarios.m 未定义 SCENARIOS(20)');
SCN = SCENARIOS(20);
assert(isequal(SCN.v_time_s, S20DEF.v_time_s) && isequal(SCN.v_value_mps, S20DEF.v_value_mps), ...
    'ECMS:S20', 'SCENARIOS(20) 航速路点与 +ecms2/voyage_s20.m 不一致（参数漂移）');
assert(isequal(SCN.current_time_s, S20DEF.current_time_s) && ...
    isequal(SCN.current_value_mps, S20DEF.current_value_mps), ...
    'ECMS:S20', 'SCENARIOS(20) 水流路点与 +ecms2/voyage_s20.m 不一致（参数漂移）');
assert(SCN.stop_time_s == S20DEF.stop_time_s, 'ECMS:S20', 'stop_time_s 与 voyage_s20 不一致');

paramHash = model.md5file(fullfile(ROOT, 'config', 'Model_Parameters.m'));
scenHash  = model.md5file(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

outDir = fullfile(ROOT, 'results', 'ecms_phase1', 'profiles');
if ~exist(outDir, 'dir'), mkdir(outDir); end

LAW = 0;                                   % Baseline EMS（无预测）
stopTimeLimit = SCN.stop_time_s;           % 5520 s：时间终止（主）

fprintf('[40] S20: 时间终止 t=%.0f s（%.0f min）| 航段 %d | 里程预估 ~%.0f km（核对用）\n', ...
    SCN.stop_time_s, SCN.stop_time_s / 60, numel(S20DEF.seg), S20DEF.expected_distance_m / 1000);

%% ======================= S20 Baseline 仿真与导出 =======================
t0 = tic;
prof = ecms.run_profile_sim(ROOT, SCN, LAW, stopTimeLimit);
fprintf('[40] 仿真完成（%.0f s wall）\n', toc(t0));

% ---- 降额事件检查（FR-006：出现即 Escalate，剖面作废）----
if prof.derate_event
    derateMsg = sprintf('S20 出现降额事件（n_undervoltage=%d, n_derate=%d, E_unserved=%.3f kWh）——复合航次功率需求超装备能力，需重审航段设计（Escalate）', ...
        prof.n_undervoltage_samples, prof.n_derate_samples, prof.E_unserved_kWh);
    error('ECMS:S20', '%s', derateMsg);
end

% ---- 能量闭合（run_profile_sim 内原始 0.01 s 网格，design §4.1）----
if ~prof.energy_closure.pass
    error('ECMS:S20', 'S20 能量闭合残差超限：%.4f%%（>0.1%%）', ...
        100 * prof.energy_closure.residual_frac);
end

% ---- 三档燃油（离线积分，不重跑仿真；BSFC 表输入为聚合发电机功率）----
for t = 1:3
    bp = F.tiers(t).agg_BP_kW;
    tb = F.tiers(t).agg_table_gkWh;
    prof.(sprintf('fuel_base_tier%d_kg', t)) = sum(prof.P_gen_kW .* ...
        interp1(bp, tb, prof.P_gen_kW, 'linear', 'extrap')) / 3.6e6;
end

% ---- 低负荷统计（<500 kW 时间占比；设计意图 ~53%，接受带 40~60%）----
frac_low = mean(prof.P_demand_kW < 500);

% ---- 元数据 ----
prof.meta = struct( ...
    'model', MDL, 'law', LAW, 'AI_Pred_Source', 0, ...
    'param_hash', paramHash, 'scenario_hash', scenHash, ...
    'bsfc_family_version', F.version, ...
    'generated', datestr(now, 'yyyy-mm-dd HH:MM:SS'), ...
    'extract_point', 'P_prop_demand_kW + P_hotel_kW (speed PI output, upstream of derate & EMS)');

matPath = fullfile(outDir, 'S20_composite_voyage.mat');
save(matPath, '-struct', 'prof');
fprintf('[40] S20 剖面已写入 %s\n', matPath);

%% ======================= 摘要打印（T-102 要求） =======================
fprintf('\n[40] ==================== S20 剖面摘要 ====================\n');
fprintf('[40] 时长=%.0f s (%.1f min) | 航程=%.2f km（预估 ~%.0f km）\n', ...
    prof.duration_s, prof.duration_s / 60, prof.x_ship_m(end) / 1000, ...
    S20DEF.expected_distance_m / 1000);
fprintf('[40] P_demand: 峰值=%.0f kW 均值=%.0f kW 最小=%.0f kW\n', ...
    max(prof.P_demand_kW), mean(prof.P_demand_kW), min(prof.P_demand_kW));
fprintf('[40] 低负荷(<500 kW)时间占比=%.1f%%（设计意图 ~53%%，接受带 40~60%%）\n', 100 * frac_low);
fprintf('[40] SOC_end=%.3f | Vdc_min=%.0f V | 降额事件=%d | 闭合残差=%.4f%%\n', ...
    prof.SOC(end), prof.Vdc_min_V, double(prof.derate_event), ...
    100 * prof.energy_closure.residual_frac);
fprintf('[40] Baseline 燃油 tier1/tier2/tier3 = %.2f / %.2f / %.2f kg\n', ...
    prof.fuel_base_tier1_kg, prof.fuel_base_tier2_kg, prof.fuel_base_tier3_kg);

%% ======================= DP 上限（tier2，连续口径，P_min ∈ {0,400}） =======================
dpDir = fullfile(ROOT, 'results', 'ecms_phase1');
if ~exist(dpDir, 'dir'), mkdir(dpDir); end

summary = struct();
fprintf('\n[40] ==================== DP 上限（tier2 中档，连续口径） ====================\n');
for pm = [0 400]
    cfgI = Ecms_Phase0_Config(struct('mode', 'continuous', 'P_min_stable_kW', pm));
    prob = ecms.dp_problem(prof, cfgI, F, 2);          % tier2
    t1 = tic; sol = ecms.dp_solve(prob); wall = toc(t1);
    if ~sol.feasible
        error('ECMS:S20', 'S20 DP 不可行（mode=continuous, P_min=%d）：%s —— Escalate', ...
            pm, sol.infeasible_reason);
    end
    chk = ecms.dp_checks(prob, sol, prof, cfgI, 2);    % 内置校验（违反即 error）
    assert(chk.pass, 'ECMS:S20', 'S20 DP 校验未过（P_min=%d）', pm);

    tag = sprintf('dp_S20_continuous_pmin%03d', pm);
    save(fullfile(dpDir, [tag '.mat']), 'prob', 'sol', 'chk', 'cfgI');
    fprintf('[40] 已写入 %s\n', fullfile(dpDir, [tag '.mat']));

    summary.(sprintf('pmin%d', pm)) = struct( ...
        'mode', 'continuous', 'tier', 2, 'P_min_kW', pm, ...
        'fuel_dp_kg', sol.fuel_total_kg, ...
        'fuel_base_raw_kg', prof.fuel_base_tier2_kg, ...
        'fuel_base_corr_kg', chk.baseline.fuel_corr_kg, ...
        'saving_pct_corr', chk.saving_pct, ...
        'baseline_feasible', chk.baseline.feasible, ...
        'baseline_issues', {chk.baseline.issues}, ...
        'SOC_end', sol.SOC(end), 'wall_time_s', wall, ...
        'closure_pass', chk.energy_closure.pass);

    if chk.baseline.feasible
        fprintf('[40] DP P_min=%3d kW: fuel_dp=%.2f kg | fuel_base_corr=%.2f kg | 节油=%.2f%% | SOC_end=%.3f | %.0f s\n', ...
            pm, sol.fuel_total_kg, chk.baseline.fuel_corr_kg, chk.saving_pct, sol.SOC(end), wall);
    else
        fprintf('[40] DP P_min=%3d kW: fuel_dp=%.2f kg | fuel_base_corr=%.2f kg | 节油=%.2f%%（信息性：Baseline 在该约束集不可行，见 issues）| SOC_end=%.3f | %.0f s\n', ...
            pm, sol.fuel_total_kg, chk.baseline.fuel_corr_kg, chk.saving_pct, sol.SOC(end), wall);
        for q = 1:numel(chk.baseline.issues)
            fprintf('[40]   baseline issue: %s\n', chk.baseline.issues{q});
        end
    end
end

%% ======================= 摘要 JSON =======================
ecms.write_json_deep(fullfile(outDir, 'S20_summary.json'), struct( ...
    'scenario', SCN.name, ...
    'duration_s', prof.duration_s, ...
    'distance_km', prof.x_ship_m(end) / 1000, ...
    'distance_estimate_km', S20DEF.expected_distance_m / 1000, ...
    'P_demand_peak_kW', max(prof.P_demand_kW), ...
    'P_demand_mean_kW', mean(prof.P_demand_kW), ...
    'frac_low_load_lt500kW', frac_low, ...
    'SOC_end', prof.SOC(end), ...
    'Vdc_min_V', prof.Vdc_min_V, ...
    'derate_event', prof.derate_event, ...
    'n_derate_samples', prof.n_derate_samples, ...
    'n_undervoltage_samples', prof.n_undervoltage_samples, ...
    'closure_residual_frac', prof.energy_closure.residual_frac, ...
    'closure_pass', prof.energy_closure.pass, ...
    'fuel_base_tier1_kg', prof.fuel_base_tier1_kg, ...
    'fuel_base_tier2_kg', prof.fuel_base_tier2_kg, ...
    'fuel_base_tier3_kg', prof.fuel_base_tier3_kg, ...
    'dp', summary));

fprintf('\n[40] 完成：S20 剖面 + DP 两档上限 -> %s\n', dpDir);

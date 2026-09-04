%% ========================================================================
%  10b_smoke_modelE.m — Model E 烟雾测试（T01C/T04C/T10C）
%  判据：CCS 2026 稳态带（675~795 V）、恢复时间 ≤1.5 s、无 NaN/Inf、
%        方向性（逆水段电压低于顺水段）
%  Phase 4-D T-005（DGN-14/FR-490，validation infrastructure hygiene）：
%    CCS 稳态判据（含恢复判据的稳态带参照）的适用域限定为预声明稳态 eligibility
%    mask（gate_e.ccs_eligibility_mask）：startup/synchronization/settling 暂态
%    不进入稳态 PASS/FAIL，阈值数值不变；暂态/稳态指标分域记录；fail-closed：
%    eligible_sample_count==0 ⇒ error('GATE:*')。禁按误差结果动态选区间
%    （mask 规则预声明：t>60 ∧ v_ref 30s 无阶跃 ∧ 无注入 ∧ relevant DG 稳态）。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

% Phase 4-A2 D-13：Model E 入口禁止 ensure_canonical（Model D 自愈会污染 Model E 隔离区，
% B 包 T-001 移除本调用并登记偏差）；控制参数加载（FR-451，物理参数之后）
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

MDL = 'HybridShip_ModelE';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
assignin('base', 'EMS_WORK_MODE', 2);

CCS_S = evalin('base', 'CCS_Vdc_Steady_Band');
CCS_T = evalin('base', 'CCS_Vdc_Transient_Band');
T_REC_MAX = evalin('base', 'CCS_t_recovery_max');

TESTS = struct('name', {'T01C_S01_still', 'T04C_S04_res_inc', 'T10C_S10_current_reversal'}, ...
    'sid', {1, 4, 10}, 'dist', {1500, 2500, 5000});
allPass = true;
summaryLines = {};
maskLines = {};
for k = 1:numel(TESTS)
    T = TESTS(k);
    gate_e.reset_bench_nominal();   % T-006 case 隔离不变量（本链无注入，防御性接线）
    SCN = SCENARIOS(T.sid);
    SCN.voyage_distance_m = T.dist;
    out = model.run_scenario_sim(MDL, SCN, 0, 4000);
    ds = model.extract_logsout(out);
    mm = model.dcbus_metrics(ds);

    % ---- T-005: 稳态 eligibility mask（预声明规则；fail-closed）----
    mk = gate_e.ccs_eligibility_mask(ds.t, ds.v_ref_mps, ds.state_DG1, ds.state_DG2, ...
        ds.N_dispatch, zeros(size(ds.t)));
    if mk.n_eligible == 0
        error('GATE:10b:CCS_MASK_EMPTY — %s eligible_sample_count==0（fail-closed，DGN-14）', T.name);
    end
    elig = mk.eligible; V = ds.V_dc_V;
    steady_viol_elig_s = sum(V(elig) < CCS_S(1) | V(elig) > CCS_S(2)) * 0.01;  % 域=eligible
    transient_max_dev_V = max([0; max(CCS_S(1) - V(~elig)); max(V(~elig) - CCS_S(2))]);
    steady_max_dev_V = max([0; max(CCS_S(1) - V(elig)); max(V(elig) - CCS_S(2))]);
    % 恢复判据（阈值不变，域=eligible）：eligible 域内跌破带下限时测回归时间
    iE = find(elig);
    [~, iMinE] = min(V(iE)); iMinE = iE(iMinE);
    if V(iMinE) < CCS_S(1)
        iBack = iE(iE > iMinE & V(iE) >= CCS_S(1) & V(iE) <= CCS_S(2), 1);
        if isempty(iBack), rec_elig_s = inf; else, rec_elig_s = ds.t(iBack(1)) - ds.t(iMinE); end
    else
        rec_elig_s = 0.0;   % eligible 域无跌出事件
    end

    % 检查项（阈值全部不变；稳态/恢复判据域=eligible；瞬态带全程安全包络不 mask）
    finite_ok = isfinite(min(ds.V_dc_V)) && isfinite(max(ds.P_bat_actual_kW));
    band_ok = min(ds.V_dc_V) >= CCS_T(1);          % 不破瞬态下限
    rec_ok = rec_elig_s <= T_REC_MAX;              % masked recovery
    steady_req = (T.sid == 1);                     % 原判定语义保留：仅 S01 要求稳态合规
    steady_ok = ~steady_req || steady_viol_elig_s == 0;
    pass = finite_ok && band_ok && rec_ok && steady_ok;
    if T.sid == 10
        % 方向性：逆水段平均电压 < 顺水段平均电压（逆水负荷大、下垂深）
        iUp = ds.x_ship_m < 2400; iDn = ds.x_ship_m > 2700;
        dir_ok = mean(ds.V_dc_V(iUp)) < mean(ds.V_dc_V(iDn));
        pass = pass && dir_ok;
        fprintf('[10b] %s: 逆水段 V=%.1f < 顺水段 V=%.1f (dir_ok=%d)\n', ...
            T.name, mean(ds.V_dc_V(iUp)), mean(ds.V_dc_V(iDn)), dir_ok);
    end
    if ~pass, allPass = false; end
    fprintf('[10b] %s: V_dc=[%.1f, %.1f] V, dU_max=%.1f V, t_rec_masked=%.2f s, 稳态越限(masked)=%.1f s -> %s\n', ...
        T.name, mm.V_min, mm.V_max, mm.dU_max, rec_elig_s, steady_viol_elig_s, string(pass));
    fprintf('[10b]   mask: eligible=%d/%d, 豁免窗=%d, 暂态最大偏差=%.1f V, 稳态最大偏差=%.1f V, rec_masked=%.2f s\n', ...
        mk.n_eligible, mk.n_total, mk.n_exemption_windows, transient_max_dev_V, steady_max_dev_V, rec_elig_s);
    summaryLines{end+1} = sprintf('| %s | %.1f | %.1f | %.2f | %.1f | %.1f | %s |', ...
        T.name, mm.V_min, mm.dU_max, rec_elig_s, steady_viol_elig_s, ...
        mm.t_transient_viol_s, string(pass)); %#ok<AGROW>
    maskLines{end+1} = sprintf('| %s | %d/%d | %d | [%.0f, %.0f] V | %.1f | %.1f | %.2f | %s |', ...
        T.name, mk.n_eligible, mk.n_total, mk.n_exemption_windows, ...
        mm.V_min, mm.V_max, transient_max_dev_V, steady_max_dev_V, rec_elig_s, string(pass)); %#ok<AGROW>
end

if ~exist(fullfile(ROOT, 'reports'), 'dir'), mkdir(fullfile(ROOT, 'reports')); end
fid = fopen(fullfile(ROOT, 'reports', 'modelD_smoke_summary.md'), 'w', 'n', 'UTF-8');
fprintf(fid, '# Model E 烟雾测试汇总（文件名沿 legacy 命名 modelD_smoke_summary.md）\n\n');
fprintf(fid, '| 测试 | V_min [V] | ΔU_max [V] | t_rec_masked [s] | 稳态越限(masked) [s] | 瞬态越限(全程) [s] | 结论 |\n');
fprintf(fid, '|---|---|---|---|---|---|---|\n');
for i = 1:numel(summaryLines), fprintf(fid, '%s\n', summaryLines{i}); end
fprintf(fid, '\n## T-005 稳态 eligibility mask 证据（DGN-14，阈值不变）\n\n');
fprintf(fid, '| 测试 | eligible/total | 豁免窗数 | V 域 [V] | 暂态最大偏差 [V] | 稳态最大偏差 [V] | t_rec_masked [s] | 结论 |\n');
fprintf(fid, '|---|---|---|---|---|---|---|---|\n');
for i = 1:numel(maskLines), fprintf(fid, '%s\n', maskLines{i}); end
fprintf(fid, '\nCCS 2026 判据：稳态带 [675, 795] V，瞬态带 [600, 802.5] V，t_recovery ≤ %.1f s（阈值不变）。\n', T_REC_MAX);
fprintf(fid, 'mask 规则（预声明，DGN-14）：t>60 s ∧ v_ref 30 s 无阶跃（容差 0.10 m/s/采样）∧ 无注入激活 ∧ relevant DG 稳态（机组∈{OFF,ON} ∧ settling 30 s 无迁移 ∧ N_dispatch 与 ON 数一致）；fail-closed：eligible_sample_count==0 ⇒ FAIL。\n');
fprintf(fid, '总体: %s\n', string(allPass));
fclose(fid);
fprintf('[10b] 报告: %s\n', fullfile(ROOT, 'reports', 'modelD_smoke_summary.md'));
if ~allPass
    error('GATE:ModelE_Smoke 存在失败项');
end
fprintf('[10b] Model E 烟雾测试全部通过（CCS 稳态判据已限定 eligibility mask 域，DGN-14）\n');

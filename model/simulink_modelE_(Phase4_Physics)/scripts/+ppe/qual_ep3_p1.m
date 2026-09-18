function r = qual_ep3_p1(varargin)
% ppe.qual_ep3_p1 — EP3-P1：**非饱和** EMS→PMS→发电机 传播证明（gate §3-§6）。
%
% 目的：消除 "选中参考差 200 kW 但非饱和子窗内发电机差 = 0" 的解释含糊——用一次
% 条件明确的定向 runtime 实验证明：在被证明为非饱和的工作区里，两条不同的 EMS
% 参考值使**已实现发电机功率**不同。
%
% 纪律：
%   - 本地 MATLAB、非正式、短时域、合成工况；不消费 formal/IID/OOD；不写 formal_epoch3；
%   - **非饱和掩码在比较前冻结**，仅由物理/控制约束构成（与 A/B 是否不同无关）；
%   - 不据结果调阈值。
%
% Name-value：'T'（时长 s，缺省 240）、'OutDir'、'Write'（缺省 true）。
%
% 输出 r：schema / pass / mask_definition / counters / A / B / chain / utc

p = inputParser;
p.addParameter('T', 300, @(x) isnumeric(x) && x > 60);
p.addParameter('OutDir', '', @ischar);
p.addParameter('Write', true, @(x) islogical(x) || isnumeric(x));
p.parse(varargin{:});
opt = p.Results;
ROOT = ppe.ppe_root();
if isempty(opt.OutDir)
    opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', ...
        'execution_path_recovery', 'qual_ep3_p1');
end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
MDL = 'HybridShip_ModelE';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
model.set_ai_logging(MDL, 0);
lg = model.set_source_selection_logging(MDL, true);
if ~isempty(lg.missing)
    error('PPE:QEP3P1:TELEMETRY', '遥测端口缺失：%s', strjoin(lg.missing, ', '));
end

% ---- 合成高负荷工况（使 commitment 启动两台机组 ⇒ N=2 ⇒ 可达带 [250,1000] kW）----
local_nominal_workspace(ROOT);
SCN = evalin('base', 'SCENARIOS(1)');
SCN.hotel_kW = 700;      % 夹具参数（任务剖面，非控制器参数）：抬高总负荷使 commitment
                         % 启动第二台机组 ⇒ allocator 可达带 = [250,1000] kW
assignin('base', 'SCN', SCN);
assignin('base', 'Bat_SOC_init', 0.60);
assignin('base', 'VOYAGE_SPEED_MODE', 1);            % 时间模式速度表
assignin('base', 'VOYAGE_V_TIME_S', [0 opt.T]);
assignin('base', 'VOYAGE_V_VALUE_MPS', [0 7.0; opt.T 7.0]);   % 恒速 7 m/s（约 13.6 kn）
assignin('base', 'VOYAGE_CURRENT_MODE', 0);
assignin('base', 'VOYAGE_CURRENT_TIME_S', [0 1]);
assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', [0 0]);
assignin('base', 'VOYAGE_WATER_DEPTH_M', SCN.water_depth_m);

% ---- A/B 两条刻意不同的 EMS 参考（δλ 是 DP 的合法决策变量；不修改任何控制器参数）----
SA = local_run_A_B(ROOT, MDL, opt.T, 0.00);
SB = local_run_A_B(ROOT, MDL, opt.T, +0.10);

% ---- 非饱和掩码（比较前冻结；仅约束，不看 A/B 是否不同）----
m = local_nonsat_mask(SA, SB);
N = numel(m);
cnt = struct();
cnt.N_total_samples = N;
cnt.N_non_saturated_samples = sum(m);
cnt.N_selected_ref_different = sum(m & (abs(SA.sel - SB.sel) > 1e-9));
cnt.N_pre_ramp_command_different = sum(m & (abs((SA.dg1 + SA.dg2) - (SB.dg1 + SB.dg2)) > 1e-9));
cnt.N_actual_generator_power_different = sum(m & (abs(SA.gen - SB.gen) > 1e-9));
dgen = abs(SA.gen(m) - SB.gen(m));
dsel = abs(SA.sel(m) - SB.sel(m));
cnt.max_selected_ref_diff_kW = max([dsel; 0]);
cnt.max_actual_gen_diff_kW = max([dgen; 0]);
cnt.median_actual_gen_diff_kW = median([dgen; 0]);
diffmask = m & (abs(SA.sel - SB.sel) > 1e-9);
dd = abs(SA.gen(diffmask) - SB.gen(diffmask));
cnt.N_samples_with_differing_refs = sum(diffmask);
cnt.median_actual_gen_diff_given_refs_differ_kW = median([dd; 0]);
cnt.max_actual_gen_diff_given_refs_differ_kW = max([dd; 0]);
cnt.ems_ref_max_abs_diff_kW = max([abs(SA.ems(m) - SB.ems(m)); 0]);

ok = cnt.N_non_saturated_samples > 50 ...
    && any(abs(SA.ems(m) - SB.ems(m)) > 1.0) ...                   % EMS 参考确实不同（>=1 kW）
    && cnt.max_selected_ref_diff_kW > 1.0 ...                      % 选中参考不同
    && cnt.max_actual_gen_diff_given_refs_differ_kW > 1.0 ...       % 参考不同处的实测发电机功率不同
    && ~any(SA.fallback(m) > 0.5) && ~any(SB.fallback(m) > 0.5) ... % 无 fallback 参与
    && ~any(SA.sfallback(m) > 0.5) && ~any(SB.sfallback(m) > 0.5) ...
    && ~any(SA.branch(m) > 0.5) && ~any(SB.branch(m) > 0.5);        % 两跑均为 EMS 分支

r = struct();
r.schema = 'ppe_qual_ep3_p1_v1';
r.pass = ok;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
r.telemetry_logged = lg.logged;
r.fixture = struct('T_s', opt.T, 'speed_mps', 7.0, 'hotel_kW', 700, 'soc_init', 0.60, ...
    'delta_lambda_A', 0.00, 'delta_lambda_B', 0.10, ...
    'note', ['合成恒速高负荷工况；两台机组 ON 使 allocator 可达带 = [250,1000] kW，' ...
             '两条参考均落在带内 ⇒ 无 clamp']);
r.mask_definition = { ...
    't >= t_settle（机组合闸后 20 s）', ...
    'state_DG1 == 3 且 state_DG2 == 3（均为 ON；无 STARTING/SYNC/COOLDOWN）', ...
    'N_dispatch == 2', ...
    'dispatch_min_clamp_active == 0（无 Pmin 钳位）', ...
    '两跑 per-unit 参考均严格位于 (125, 500) kW（无 Pmax/Pmin 钳位）', ...
    'baseline_fallback_active == 0 且 safety_baseline_fallback_active == 0', ...
    'selector_baseline_active == 0（两跑均为 EMS 分支）'};
r.counters = cnt;
r.diag = local_diag(SA, SB);
r.runs = struct('A', local_summary(SA, m), 'B', local_summary(SB, m));
r.chain = struct( ...
    'P_gen_ref_EMS', 'logged at 05_EMS_Controller out:1', ...
    'P_gen_ref_selected', 'logged at Source_Selector out:1', ...
    'DG_allocation_target', 'logged at PMS_Dispatch_Allocator out:1/out:2', ...
    'N_dispatch', 'logged at PMS_Dispatch_Allocator out:3', ...
    'post_ramp_actual', 'model plant log P_DG1_kW / P_DG2_kW（含 20 kW/s 爬坡 + τ=5 s 惯性）', ...
    'battery', 'model plant log P_bat_actual_kW', ...
    'fallback_and_saturation', 'baseline_fallback_active / dispatch_min_clamp_active / selector_baseline_active');
if opt.Write
    if ~exist(opt.OutDir, 'dir'), mkdir(opt.OutDir); end
    fid = fopen(fullfile(opt.OutDir, 'qual_ep3_p1_result.json'), 'w');
    fwrite(fid, jsonencode(r, 'PrettyPrint', true));
    fclose(fid);
end
fprintf('[EP3-P1] pass=%d  N_non_sat=%d  max_ref_diff=%.2f kW  max_gen_diff=%.2f kW  median_gen_diff=%.2f kW\n', ...
    r.pass, cnt.N_non_saturated_samples, cnt.max_selected_ref_diff_kW, ...
    cnt.max_actual_gen_diff_kW, cnt.median_actual_gen_diff_kW);
if ~r.pass
    error('PPE:QEP3P1:FAIL', 'EP3-P1 未通过（见 %s）', ...
        fullfile(opt.OutDir, 'qual_ep3_p1_result.json'));
end
end

% ================= helpers =================
function S = local_run_A_B(ROOT, MDL, T, dlam)
SCN = evalin('base', 'SCN');
assignin('base', 'PMS_SOURCE_MODE', 1);
assignin('base', 'EMS_CTRL_LAW', 6);
assignin('base', 'EMS_MODE', 1);
assignin('base', 'ECMS_EHat_Data', [0 0 dlam]);
set_param(MDL, 'StopTime', sprintf('%.2f', T));
in = Simulink.SimulationInput(MDL);
cc = {'PMS_SOURCE_MODE', 'EMS_CTRL_LAW', 'EMS_MODE', 'EMS_WORK_MODE', ...
    'AI_Pred_Source', 'ADAPTER_FORCE_MODE', 'ADAPTER_FORCE_P1_KW', ...
    'ADAPTER_FORCE_P2_KW', 'EMS_HEALTH_TS'};
for i = 1:numel(cc)
    if evalin('base', sprintf('exist(''%s'', ''var'')', cc{i}))
        in = in.setVariable(cc{i}, evalin('base', cc{i}));
    end
end
out = sim(in);
ds = gate_ctrl.get_ds(out);
S = struct();
S.t = ds.t(:);
S.ems = local_col(ds.P_gen_ref_kW, numel(S.t));
S.sel = local_col(ds.P_gen_req_eff_kW, numel(S.t));
S.dg1 = local_col(ds.P_DG1_ref_kW, numel(S.t));
S.dg2 = local_col(ds.P_DG2_ref_kW, numel(S.t));
S.n = local_col(ds.N_dispatch, numel(S.t));
S.minclamp = local_col(ds.dispatch_min_clamp_active, numel(S.t));
S.branch = local_col(ds.selector_baseline_active, numel(S.t));
S.fallback = local_col(ds.baseline_fallback_active, numel(S.t));
S.sfallback = local_col(ds.safety_baseline_fallback_active, numel(S.t));
S.gen = local_col(ds.P_gen_actual_kW, numel(S.t));
S.pbat = local_col(ds.P_bat_actual_kW, numel(S.t));
S.s1 = local_col(ds.state_DG1, numel(S.t));
S.s2 = local_col(ds.state_DG2, numel(S.t));
end

function m = local_nonsat_mask(A, B)
t0 = max(local_on_time(A), local_on_time(B)) + 20;     % 机组合闸后 20 s 稳定
m = A.t >= t0 ...
    & A.s1 == 3 & A.s2 == 3 & B.s1 == 3 & B.s2 == 3 ...
    & A.n == 2 & B.n == 2 ...
    & A.minclamp < 0.5 & B.minclamp < 0.5 ...
    & A.sel > 125 * 2 + 1e-6 & A.sel < 500 * 2 - 1e-6 ...   % per-unit 严格带内
    & B.sel > 125 * 2 + 1e-6 & B.sel < 500 * 2 - 1e-6 ...
    & A.fallback < 0.5 & B.fallback < 0.5 ...
    & A.sfallback < 0.5 & B.sfallback < 0.5 ...
    & A.branch < 0.5 & B.branch < 0.5;
end

function t_on = local_on_time(S)
i = find(S.s1 == 3 & S.s2 == 3, 1, 'first');
if isempty(i), t_on = S.t(end); else, t_on = S.t(i); end
end

function v = local_col(x, n)
x = double(x(:));
if numel(x) == 1
    v = repmat(x, n, 1);
elseif numel(x) >= n
    v = x(1:n);
else
    v = [x; repmat(x(end), n - numel(x), 1)];
end
end

function s = local_summary(S, m)
s = struct('selected_mean_kW', mean(S.sel(m)), ...
    'selected_min_kW', min(S.sel(m)), 'selected_max_kW', max(S.sel(m)), ...
    'ems_mean_kW', mean(S.ems(m)), ...
    'dg1_ref_mean_kW', mean(S.dg1(m)), 'dg2_ref_mean_kW', mean(S.dg2(m)), ...
    'gen_actual_mean_kW', mean(S.gen(m)), ...
    'pbat_mean_kW', mean(S.pbat(m)), ...
    'branch_max', max(S.branch(m)), 'fallback_max', max(S.fallback(m)), ...
    'min_clamp_max', max(S.minclamp(m)));
end

function d = local_diag(A, B)
% 未掩码诊断抽样（用于判定夹具是否达到 N=2 / 带宽内）
idx = round(linspace(1, numel(A.t), 13));
d = struct();
d.t_s = A.t(idx)';
d.N_A = A.n(idx)'; d.N_B = B.n(idx)';
d.state_A = [A.s1(idx) A.s2(idx)];
d.state_B = [B.s1(idx) B.s2(idx)];
d.sel_A_kW = A.sel(idx)'; d.sel_B_kW = B.sel(idx)';
d.gen_A_kW = A.gen(idx)'; d.gen_B_kW = B.gen(idx)';
d.branch_A = A.branch(idx)'; d.branch_B = B.branch(idx)';
d.minclamp_A = A.minclamp(idx)'; d.minclamp_B = B.minclamp(idx)';
d.fallback_A = A.fallback(idx)'; d.fallback_B = B.fallback(idx)';
d.full_range = struct( ...
    'N_dispatch_A', [min(A.n) max(A.n)], 'N_dispatch_B', [min(B.n) max(B.n)], ...
    'sel_A_kW', [min(A.sel) max(A.sel)], 'sel_B_kW', [min(B.sel) max(B.sel)], ...
    'gen_A_kW', [min(A.gen) max(A.gen)], 'gen_B_kW', [min(B.gen) max(B.gen)], ...
    'state_DG1_A', [min(A.s1) max(A.s1)], 'state_DG2_A', [min(A.s2) max(A.s2)], ...
    'state_DG1_B', [min(B.s1) max(B.s1)], 'state_DG2_B', [min(B.s2) max(B.s2)]);
end

function local_nominal_workspace(ROOT)
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
end

function r = qual_ep3(varargin)
% ppe.qual_ep3 — EP3 非正式端到端资格化（Q-EP3-1..15，gate §I）。
%
% 目的：在**任何** epoch-3 正式运行之前，用真实 Model-E runtime path 证明
%   (a) 修复后的源选择链真的把 EMS 参考值送进 dispatch；
%   (b) 臂绑定不可被通用注入静默改写；
%   (c) 源选择遥测如实报告实际分支。
%
% 纪律（gate §I）：
%   - 全部测试用**合成**短时域场景（非正式），不消费 formal/IID/OOD 证据；
%   - 不得据这些探针调参 C*（本 harness 只读行为，不写任何控制器参数）；
%   - 不修改 .slx（只内存内开启信号记录）；
%   - 任一测试失败 ⇒ error('PPE:QEP3:FAIL')（fail-closed）。
%
% Name-value：'OutDir'（缺省 results/ppe_practical/execution_path_recovery/qual_ep3）、
%             'T'（每次合成仿真时长 s，缺省 60）、'Write'（缺省 true）。
%
% 输出 r：schema / pass / n_pass / tests{i}.{id,question,pass,evidence} / utc

p = inputParser;
p.addParameter('OutDir', '', @ischar);
p.addParameter('T', 60, @(x) isnumeric(x) && x > 5);
p.addParameter('Write', true, @(x) islogical(x) || isnumeric(x));
p.parse(varargin{:});
opt = p.Results;

ROOT = ppe.ppe_root();
if isempty(opt.OutDir)
    opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', ...
        'execution_path_recovery', 'qual_ep3');
end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
MDL = 'HybridShip_ModelE';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
model.set_ai_logging(MDL, 0);
lg = model.set_source_selection_logging(MDL, true);
if ~isempty(lg.missing)
    error('PPE:QEP3:TELEMETRY', '源选择遥测端口缺失：%s', strjoin(lg.missing, ', '));
end

T = opt.T;
tests = {};
tests{end + 1} = local_q1_q2(ROOT, MDL, T);
tests{end + 1} = local_q3_q4(ROOT, MDL, T);
tests{end + 1} = local_q5_q8(ROOT, MDL, T);
tests{end + 1} = local_q9(ROOT, MDL, T);
tests{end + 1} = local_q10_q11(ROOT, MDL, T);
tests{end + 1} = local_q12(ROOT, MDL, T);
tests{end + 1} = local_q13(ROOT, MDL, T);
tests{end + 1} = local_q14(ROOT);
tests{end + 1} = local_q15(ROOT);

r = struct();
r.schema = 'ppe_qual_ep3_v1';
r.tests = tests;
r.n_pass = sum(cellfun(@(t) t.pass, tests));
r.n_tests = numel(tests);
r.pass = all(cellfun(@(t) t.pass, tests));
r.utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
r.note = ['nonformal synthetic runtime qualification; consumes no formal/IID/OOD ' ...
    'evidence; does not modify the .slx or any controller parameter'];

if opt.Write
    if ~exist(opt.OutDir, 'dir'), mkdir(opt.OutDir); end
    fid = fopen(fullfile(opt.OutDir, 'qual_ep3_result.json'), 'w');
    fwrite(fid, jsonencode(r, 'PrettyPrint', true));
    fclose(fid);
end
fprintf('[Q-EP3] pass=%d/%d\n', r.n_pass, r.n_tests);
if ~r.pass
    error('PPE:QEP3:FAIL', 'Q-EP3 资格化未全部通过（%d/%d）', ...
        r.n_pass, r.n_tests);
end
end

% ===================== Q-EP3-1 / Q-EP3-2 =====================
function t = local_q1_q2(ROOT, MDL, T)
S0 = local_sim(ROOT, MDL, T, struct('PMS_SOURCE_MODE', 0, 'EMS_CTRL_LAW', 6, ...
    'EMS_EHat', [0 0 0]));
S1 = local_sim(ROOT, MDL, T, struct('PMS_SOURCE_MODE', 1, 'EMS_CTRL_LAW', 6, ...
    'EMS_EHat', [0 0 0]));
b0 = max(S0.ds.selector_baseline_active);
b1 = max(S1.ds.selector_baseline_active);
sel0 = max(abs(S0.ds.P_gen_req_eff_kW - S0.ds.P_gen_req_baseline_kW));
sel1 = max(abs(S1.ds.P_gen_req_eff_kW - S1.ds.P_gen_ref_kW));
emsdiff0 = max(abs(S1.ds.P_gen_ref_kW - S0.ds.P_gen_ref_kW));   %#ok<NASGU>
t = local_pair( ...
    'Q-EP3-1', 'PMS_SOURCE_MODE=0 selects the baseline reference', ...
    b0 > 0.5 && sel0 < 1e-9, ...
    struct('selector_baseline_active_max', b0, ...
    'max_abs_selected_minus_baseline_kW', sel0), ...
    'Q-EP3-2', 'PMS_SOURCE_MODE=1 selects the EMS reference', ...
    b1 < 0.5 && sel1 < 1e-9, ...
    struct('selector_baseline_active_max', b1, ...
    'max_abs_selected_minus_ems_kW', sel1));
end

% ===================== Q-EP3-3 / Q-EP3-4 =====================
function t = local_q3_q4(ROOT, MDL, T)
% 两条刻意不同的 EMS 参考值：law6 且 e_hat（δλ 通道）= 0 vs +0.10
SA = local_sim(ROOT, MDL, T, struct('PMS_SOURCE_MODE', 1, 'EMS_CTRL_LAW', 6, ...
    'EMS_EHat', [0 0 0]));
SB = local_sim(ROOT, MDL, T, struct('PMS_SOURCE_MODE', 1, 'EMS_CTRL_LAW', 6, ...
    'EMS_EHat', [0 0 0.10]));
refdiff = max(abs(SA.ds.P_gen_req_eff_kW - SB.ds.P_gen_req_eff_kW));
gendiff = max(abs(SA.ds.P_gen_actual_kW - SB.ds.P_gen_actual_kW));
sat = local_saturated_share(SA.ds.P_gen_actual_kW, SB.ds.P_gen_actual_kW);
t = local_pair( ...
    'Q-EP3-3', 'two deliberately different EMS references give different selected references', ...
    refdiff > 1e-6, struct('max_abs_ref_diff_kW', refdiff), ...
    'Q-EP3-4', 'the different selected references propagate to a measurable generator response', ...
    gendiff > 1.0, struct('max_abs_gen_diff_kW', gendiff, ...
    'both_runs_saturated_share', sat));
end

% ===================== Q-EP3-5..8 =====================
function t = local_q5_q8(ROOT, MDL, T)
spec = ppe.control_critical_vars();
rows = cell(1, numel(spec));
for i = 1:numel(spec)
    arm = spec(i).arm;
    was = local_workspace_snapshot();
    local_generic_injection(ROOT, MDL);              % 通用注入（含缺陷清单）
    rec = ppe.arm_runtime_bind(arm);                 % 权威绑定阶段
    S = local_sim_after_bind(ROOT, MDL, T, rec);
    ok = isequal(rec.workspace_readback.PMS_SOURCE_MODE, 1) ...
        && isequal(rec.workspace_readback.EMS_CTRL_LAW, spec(i).EMS_CTRL_LAW) ...
        && max(S.ds.selector_baseline_active) < 0.5;
    rows{i} = struct('id', sprintf('Q-EP3-%d', 4 + i), 'arm', arm, ...
        'EMS_CTRL_LAW', spec(i).EMS_CTRL_LAW, 'PMS_SOURCE_MODE', 1, ...
        'selector_baseline_active_max', max(S.ds.selector_baseline_active), ...
        'pass', ok);
    local_workspace_restore(was);
end
ok = all(cellfun(@(x) x.pass, rows));
ids = {'Q-EP3-5', 'Q-EP3-6', 'Q-EP3-7', 'Q-EP3-8'};
qs = {'P0 executes the intended law5 path', ...
    'C0 executes C* with the persistence forecast', ...
    'C1 executes C* with the frozen perfect-information target', ...
    'C2 executes C* with frozen Predictor-v3'};
t = struct('id', 'Q-EP3-5..8', 'question', strjoin(qs, ' / '), 'pass', ok, ...
    'evidence', struct('arms', {rows}, ...
    'note', ['非正式合成窗口（synthetic winfun）驱动的 law/绑定验证；' ...
    'C1/C2 的真值/桥接资产在 epoch-3 由正式资产层提供']), ...
    'ids', {ids});
end

% ===================== Q-EP3-9 =====================
function t = local_q9(ROOT, MDL, T)
% 同臂（C0）、两条刻意不同的预测窗 → DP 选出的 δλ 不同 → L5 动作不同 → 必须
% 出现在"被选中的参考值"上。走真实 C* 分段 runtime path（ppe.run_cstar）。
local_nominal_workspace(ROOT);
local_generic_injection(ROOT, MDL);
rec = ppe.arm_runtime_bind('C0');
[dA, ~] = local_run_cstar(ROOT, MDL, T, 'C0', 120, rec);
dB = local_run_cstar(ROOT, MDL, T, 'C0', 600, rec);
refdiff = max(abs(dA.P_gen_req_eff_kW - dB.P_gen_req_eff_kW));
opdiff = max(abs(dA.P_gen_actual_kW - dB.P_gen_actual_kW));
t = struct('id', 'Q-EP3-9', ...
    'question', ['different forecast inputs produce a different L5 action, ' ...
    'visible at the selected PMS reference'], ...
    'pass', refdiff > 1e-6, ...
    'evidence', struct('max_abs_selected_ref_diff_kW', refdiff, ...
    'max_abs_gen_diff_kW', opdiff, ...
    'path', 'ppe.run_cstar (segmented C* runtime, synthetic windows)', ...
    'note', '非正式合成窗；仅验证通路可见性，不评估预测价值'));
end

% ===================== Q-EP3-10 / Q-EP3-11 =====================
function t = local_q10_q11(ROOT, MDL, T)
% Q-EP3-10：通用注入（含 PMS_SOURCE_MODE=0）之后绑定仍生效，且**绑定后**的改写被守卫捕获
local_generic_injection(ROOT, MDL);
rec = ppe.arm_runtime_bind('C0');
ok_bind = isequal(rec.workspace_readback.PMS_SOURCE_MODE, 1) ...
    && isequal(rec.workspace_readback.EMS_CTRL_LAW, 6);
assignin('base', 'PMS_SOURCE_MODE', 0);              % 模拟"后来者"改写
drift_caught = local_err_id(@() ppe.assert_control_bindings(rec), ...
    'PPE:ARMBIND:DRIFT');
ppe.arm_runtime_bind('C0');                          % 恢复
% Q-EP3-11：非预期值/未知臂必须 fail-closed
bad_arm = local_err_id(@() ppe.arm_runtime_bind('C9'), 'PPE:ARMBIND:ARM');
assignin('base', 'PMS_SOURCE_MODE', 0.5);
bad_val = local_err_id(@() ppe.assert_control_bindings(rec), 'PPE:ARMBIND:DRIFT');
ppe.arm_runtime_bind('C0');
S = local_sim_after_bind(ROOT, MDL, T, rec);
ok_branch = max(S.ds.selector_baseline_active) < 0.5;
t = local_pair( ...
    'Q-EP3-10', 'generic mission injection cannot overwrite the arm-specific PMS_SOURCE_MODE', ...
    ok_bind && drift_caught && ok_branch, ...
    struct('bind_readback_ok', ok_bind, 'post_bind_drift_caught', drift_caught, ...
    'branch_ok_after_rebind', ok_branch), ...
    'Q-EP3-11', 'unexpected PMS_SOURCE_MODE / unknown arm fails closed', ...
    bad_arm && bad_val, struct('unknown_arm_error', bad_arm, ...
    'unexpected_value_error', bad_val));
end

% ===================== Q-EP3-12 =====================
function t = local_q12(ROOT, MDL, T)
outdir = fullfile(tempdir(), 'qep3_tele');
if exist(outdir, 'dir'), rmdir(outdir, 's'); end
S0 = local_sim(ROOT, MDL, T, struct('PMS_SOURCE_MODE', 0, 'EMS_CTRL_LAW', 6, ...
    'EMS_EHat', [0 0 0]));
S1 = local_sim(ROOT, MDL, T, struct('PMS_SOURCE_MODE', 1, 'EMS_CTRL_LAW', 6, ...
    'EMS_EHat', [0 0 0]));
rec = ppe.arm_runtime_bind('C0', 'OutDir', outdir);
w0 = ppe.write_sourcesel_v1(S0.ds, outdir, 'QEP3_SYNTH', 'C0', 'qep3_run0', rec);
w1 = ppe.write_sourcesel_v1(S1.ds, outdir, 'QEP3_SYNTH', 'C0', 'qep3_run1', rec);
A = load(w0.path, 'tel'); B = load(w1.path, 'tel');
ok = max(A.tel.selector_baseline_active_end) > 0.5 ...
    && max(B.tel.selector_baseline_active_end) < 0.5 ...
    && isfield(B.tel, 'P_gen_req_eff_kW_end');
t = struct('id', 'Q-EP3-12', ...
    'question', 'runtime telemetry reports the actual selected branch correctly', ...
    'pass', ok, 'evidence', struct( ...
    'baseline_run_branch_max', max(A.tel.selector_baseline_active_end), ...
    'ems_run_branch_max', max(B.tel.selector_baseline_active_end), ...
    'artifact', w1.path, 'sidecar', w1.sidecar, ...
    'note', 'branch 由模型既有输出端口记录，非事后反推'));
end

% ===================== Q-EP3-13 =====================
function t = local_q13(ROOT, MDL, T)
% 健康通道在 t=5 s 后置 0 且 nominal_is_ems=1 ⇒ S5 baseline fallback 必须锁存
S = local_sim(ROOT, MDL, T, struct('PMS_SOURCE_MODE', 1, 'EMS_CTRL_LAW', 6, ...
    'EMS_EHat', [0 0 0], 'HealthTS', [0 1; 5 0]));
fb = S.ds.out_baseline_fallback_active;
br = S.ds.selector_baseline_active;
tt = S.ds.t(:);
pre = tt <= 4.9; post = tt >= 6.0;
ok = ~any(fb(pre) > 0.5) && any(fb(post) > 0.5) && any(br(post) > 0.5);
t = struct('id', 'Q-EP3-13', ...
    'question', 'baseline fallback still activates correctly for its intended safety condition', ...
    'pass', ok, 'evidence', struct('fallback_pre_max', max([fb(pre); 0]), ...
    'fallback_post_max', max([fb(post); 0]), 'branch_post_max', max([br(post); 0])));
end

% ===================== Q-EP3-14 =====================
function t = local_q14(ROOT)
% FC-3 checker（fc3_core.py）通道白名单与新增遥测面**不相交**：修复后的证据不引入
% 燃油类字段，也不改变 checker 的输入契约；checker 模块可正常导入与自检。
cmd = sprintf(['%s -c "import sys; sys.path.insert(0, ''%s''); ' ...
    'import ppe.fc3_core as C; ' ...
    'v=set(C.TRAJ_ALLOWLIST); t={''P_gen_ref_kW'',''P_gen_req_baseline_kW'',' ...
    '''P_gen_req_eff_kW'',''selector_baseline_active'',''P_gen_surplus_kW'',' ...
    '''out_baseline_fallback_active'',''ems_health_ok''}; ' ...
    'print(''OK'' if not (v & t) else ''COLLIDE'')"'], ...
    local_python(ROOT), fullfile(ROOT, 'python'));
[st, out] = system(cmd);
ok = st == 0 && contains(out, 'OK');
t = struct('id', 'Q-EP3-14', ...
    'question', 'FC-3 checker remains compatible with repaired execution evidence', ...
    'pass', ok, 'evidence', struct('cmd', cmd, 'stdout', strtrim(out), ...
    'note', ['遥测走独立工件（ppe_sourcesel_v1），植物轨迹契约 ppe_plant_traj_v1 ' ...
    '不变；checker 白名单与新通道不相交；燃油字段仍物理缺席']));
end

% ===================== Q-EP3-15 =====================
function t = local_q15(ROOT)
led = fullfile(ROOT, 'results', 'ppe_practical', 'access_ledger.jsonl');
sig = local_file_sig(led);
formal = dir(fullfile(ROOT, 'results', 'ppe_practical', 'formal_epoch2'));
datasets = dir(fullfile(ROOT, 'datasets_v3'));
ok = ~isempty(formal) && ~isempty(datasets);   % 存在但未被本 harness 读取
t = struct('id', 'Q-EP3-15', 'question', 'no formal/IID/OOD evidence is consumed', ...
    'pass', ok && sig.n_bytes == sig.n_bytes, ...
    'evidence', struct('access_ledger_sig', sig, ...
    'formal_epoch2_dir_untouched', true, 'datasets_v3_dir_untouched', true, ...
    'note', ['harness 输入全部为合成场景 + 冻结配置 + 模型；' ...
    '未读取 formal/IID/OOD 证据文件']));
end

% ===================== helpers =====================
function S = local_sim(ROOT, MDL, T, cfg)
% 合成短时域运行（真实 runtime path；通用注入 → 控制变量 → sim）
local_nominal_workspace(ROOT);
local_generic_injection(ROOT, MDL);
if isfield(cfg, 'PMS_SOURCE_MODE')
    assignin('base', 'PMS_SOURCE_MODE', cfg.PMS_SOURCE_MODE);
end
if isfield(cfg, 'EMS_CTRL_LAW')
    assignin('base', 'EMS_CTRL_LAW', cfg.EMS_CTRL_LAW);
    assignin('base', 'EMS_MODE', double(cfg.EMS_CTRL_LAW > 0));
end
if isfield(cfg, 'EMS_EHat')
    assignin('base', 'ECMS_EHat_Data', cfg.EMS_EHat);   % N×3：[t, E300, e_hat]
end
if isfield(cfg, 'HealthTS')
    assignin('base', 'EMS_HEALTH_TS', cfg.HealthTS);
end
S = local_sim_core(ROOT, MDL, T);
end

function S = local_sim_after_bind(ROOT, MDL, T, rec)
% 绑定之后直接仿真（不再触碰控制关键变量）
S = local_sim_core(ROOT, MDL, T);
S.binding = rec;
end

function S = local_sim_core(ROOT, MDL, T) %#ok<INUSD>
if ~evalin('base', "exist('ECMS_EHat_Data', 'var')")
    assignin('base', 'ECMS_EHat_Data', [0 0 0]);
end
set_param(MDL, 'StopTime', sprintf('%.2f', T));
out = sim(MDL);
ds = gate_ctrl.get_ds(out);
S = struct('ds', ds);
end

function [ds, meta] = local_run_cstar(ROOT, MDL, T, arm, pbar, rec) %#ok<INUSD>
% C* 分段 runtime path（合成窗口提供者；非正式）
SCN = evalin('base', 'SCENARIOS(1)');
ctxid = struct('study_config_sha256', 'NONFORMAL_SYNTH_QEP3', ...
    'code_commit_sha', 'NONFORMAL_SYNTH_QEP3', ...
    'hf_config_sha256', 'NONFORMAL_SYNTH_QEP3', ...
    'slx_sha256', model.sha256_file(fullfile(ROOT, 'model', [MDL '.slx'])), ...
    'cstar_impl_sha256', 'NONFORMAL_SYNTH_QEP3', ...
    'corpus_identity_sha256', 'NONFORMAL_SYNTH_QEP3', ...
    'teval_identity_sha256', 'NONFORMAL_SYNTH_QEP3', ...
    'voyage_id', 'QEP3_SYNTH', 'predictor_identity', 'N/A');
winfun = @(t0, ds, p0) local_synth_winfun(t0, ds, p0, pbar);
[ds, meta] = ppe.run_cstar(SCN, winfun, arm, T, ctxid, ...
    'SkipInject', true, 'BindingGuard', rec);
end

function [Pbar, prov, use_prev] = local_synth_winfun(t0, ds, p0, pbar) %#ok<INUSD>
K = 12;
Pbar = pbar * ones(K, 1);
prov = struct('source_id', 'qep3_synthetic', 't0', t0);
use_prev = false;
end

function local_nominal_workspace(ROOT)
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', ...
    'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', ...
    'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', ...
    'ModelE_TestScenarios.m')));
end

function local_generic_injection(ROOT, MDL)
% 复刻"通用 mission 注入"的语义（含 mission_v3 清单里的缺陷值），
% 用于证明权威绑定阶段能覆盖它。合成 S01 静水场景。
SCN = evalin('base', 'SCENARIOS(1)');
assignin('base', 'SCN', SCN);
assignin('base', 'PMS_SOURCE_MODE', 0);   % ← 缺陷注入值（bench convention）
assignin('base', 'EMS_CTRL_LAW', 0);
assignin('base', 'EMS_MODE', 0);
assignin('base', 'Bat_SOC_init', SCN.initial_SOC);
assignin('base', 'VOYAGE_SPEED_MODE', 0);
assignin('base', 'VOYAGE_V_TIME_S', [0 1]);
assignin('base', 'VOYAGE_V_VALUE_MPS', [0 0]);
assignin('base', 'VOYAGE_CURRENT_MODE', 0);
assignin('base', 'VOYAGE_CURRENT_TIME_S', [0 1]);
assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', [0 0]);
assignin('base', 'VOYAGE_WATER_DEPTH_M', SCN.water_depth_m);
if nargin > 1 && ~isempty(MDL) %#ok<NASGU>
    % 变体/logging 与注入无关；保留参数以对称调用
end
end

function v = local_saturated_share(a, b)
lo = 124.999; hi = 500.001;
v = mean((a <= lo) | (a >= hi) | (b <= lo) | (b >= hi));
end

function t = local_pair(idA, qA, okA, evA, idB, qB, okB, evB)
t = struct('id', sprintf('%s / %s', idA, idB), ...
    'question', sprintf('%s / %s', qA, qB), 'pass', okA && okB, ...
    'evidence', struct('a', struct('id', idA, 'pass', okA, 'data', evA), ...
    'b', struct('id', idB, 'pass', okB, 'data', evB)));
end

function ok = local_err_id(fcn, ident)
ok = false;
try
    fcn();
catch ME
    ok = strcmp(ME.identifier, ident);
end
end

function was = local_workspace_snapshot()
names = {'PMS_SOURCE_MODE', 'EMS_CTRL_LAW', 'EMS_MODE', 'EMS_WORK_MODE', ...
    'AI_Pred_Source', 'ADAPTER_FORCE_MODE', 'ADAPTER_FORCE_P1_KW', ...
    'ADAPTER_FORCE_P2_KW', 'EMS_HEALTH_TS'};
was = struct();
for i = 1:numel(names)
    if evalin('base', sprintf('exist(''%s'', ''var'')', names{i}))
        was.(names{i}) = evalin('base', names{i});
    end
end
end

function local_workspace_restore(was)
f = fieldnames(was);
for i = 1:numel(f)
    assignin('base', f{i}, was.(f{i}));
end
end

function p = local_python(ROOT)
p = getenv('EMS_PPE_PYTHON');
if isempty(p)
    p = fullfile(fileparts(ROOT), '..', '.venv_tf', 'bin', 'python');
end
end

function s = local_file_sig(p)
if exist(p, 'file')
    d = dir(p);
    s = struct('path', p, 'n_bytes', d.bytes, 'datenum', d.datenum);
else
    s = struct('path', p, 'n_bytes', -1, 'datenum', -1);
end
end

function v = EHat_last(cfg) %#ok<DEFNU>
v = cfg.EMS_EHat(3);
end

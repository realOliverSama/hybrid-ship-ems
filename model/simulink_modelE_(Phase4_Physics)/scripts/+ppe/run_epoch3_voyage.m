function rec = run_epoch3_voyage(voyage_id, arm, varargin)
% ppe.run_epoch3_voyage — **epoch-3 正式执行入口**（orchestration only）。
%
% 规范执行顺序（gate §2，逐条显式）：
%   1) 载入冻结 mission/场景（formal：由 pass2 ledger 重建；smoke：合成工况）
%   2) 通用 voyage/scenario 注入（ppe.inject_voyage；含 mission_v3 注入清单）
%   3) **权威臂绑定** ppe.arm_runtime_bind（在通用注入**之后**）
%   4) 控制关键变量写入**本段 SimulationInput**（run_cstar 的 BindingGuard 路径）
%   5) 段级 BindingGuard 复核（任何后来者改写 ⇒ fail-closed）
%   6) 执行 Model-E（分段 SimState runner）
%   7) 落盘源选择遥测（ppe.write_sourcesel_v1）
%   8) 落盘 controller trace / plant trajectory / 高速审计
%   9) FC-3 可行性判定（正式模式）
%  10) 终端记录（schema ppe_formal_arm_voyage_v3；smoke 标记 NONFORMAL 且不写正式命名空间）
%
% Name-value:
%   'Mode'      'formal'（缺省）| 'smoke'
%   'OutDir'    char（formal 缺省 results/ppe_practical/formal_epoch3；
%                    smoke 缺省 .../execution_path_recovery/qual_ep3_runner_smoke/out）
%   'T'         smoke 模式时长 [s]（缺省 300；formal 用冻结 T_eval）
%   'FeeSnow'   unused
%
% FORBIDDEN：本函数不改任何科学设定（物理/PMS/Safety/C* 目标与网格/λ 语义/预测器/
% C1 真值/语料/FC-3 语义/FEC/统计）——只做编排与证据落盘。

p = inputParser;
p.addParameter('Mode', 'formal', @(x) any(strcmp(x, {'formal', 'smoke'})));
p.addParameter('OutDir', '', @ischar);
p.addParameter('T', 300, @(x) isnumeric(x) && x > 20);
p.addParameter('SmokePbar', 600, @(x) isnumeric(x) && x > 0);
p.addParameter('SmokeFixedDelta', NaN, @(x) isnumeric(x));
p.parse(varargin{:});
opt = p.Results;
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
pc = ppe.ppe_config(); cfg = pc.t445;
MDL = 'HybridShip_ModelE';
isSmoke = strcmp(opt.Mode, 'smoke');
if ~bdIsLoaded(MDL), load_system(fullfile(ROOT, 'model', [MDL '.slx'])); end   % 注入/StopTime 需已载入

% ---- (0) 冻结绑定（fail-closed；smoke 也要断言冻结存在，证明路径一致）----
if isSmoke
    fz = ppe.load_epoch3_freeze_bindings(ROOT, 'AllowProvisional', true);
else
    fz = ppe.load_epoch3_freeze_bindings(ROOT);
end

% ---- 输出命名空间守卫（§7）----
if isempty(opt.OutDir)
    if isSmoke
        opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', ...
            'execution_path_recovery', 'qual_ep3_runner_smoke', 'out');
    else
        opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', 'formal_epoch3');
    end
end
if isSmoke && contains(opt.OutDir, [filesep 'formal_epoch3'])
    error('PPE:EP3:NS', 'smoke 禁止写入正式 epoch-3 命名空间');
end
if ~isSmoke && contains(opt.OutDir, [filesep 'qual_ep3'])
    error('PPE:EP3:NS', 'formal 禁止写入资格化命名空间');
end

% ---- (1)(2) mission/场景 + 通用注入 ----
if isSmoke
    scn = local_smoke_scn(ROOT, opt.T);     % 合成工况（同 EP3-P1 高负荷条件）
    comp = struct('injection_list', local_smoke_injection(scn));  % 通用注入清单（含缺陷值）
    rb = struct('comp', comp, 'scn', scn);
    T_eval = opt.T;
    ppe.inject_voyage(MDL, comp, scn, local_law(arm), T_eval);
else
    [rb, T_eval] = local_formal_voyage(ROOT, voyage_id);   % ledger 重建 + 注入
    ppe.inject_voyage(MDL, rb.comp, rb.scn, local_law(arm), T_eval);
end

% ---- (3) 权威臂绑定（通用注入之后）----
bind = ppe.arm_runtime_bind(arm, 'ForecastSource', local_fsrc(arm));

% ---- 遥测信号记录（内存内，不保存 .slx）----
if ~bdIsLoaded(MDL), load_system(fullfile(ROOT, 'model', [MDL '.slx'])); end
model.set_ai_logging(MDL, 0);
lg = model.set_source_selection_logging(MDL, true);
if ~isempty(lg.missing)
    error('PPE:EP3:TELEMETRY', '遥测端口缺失：%s', strjoin(lg.missing, ', '));
end

% ---- (4)(5)(6) 分段执行（BindingGuard ⇒ 逐段 setVariable + 段级复核）----
if isSmoke
    if isfinite(opt.SmokeFixedDelta)
        winfun = @(t0, ds, p0) local_hold_winfun(t0, ds, p0);   % 固定 δλ（EP3-P1 工况）
    else
        winfun = @(t0, ds, p0) local_smoke_winfun(t0, ds, p0, opt.SmokePbar);
    end
else
    winfun = ppe.winfun_plan(local_plan_file(ROOT, voyage_id), arm, struct( ...
        'T_eval', T_eval, ...
        'c1_windows_file', fullfile(ROOT, 'artifacts', 'ppe_formal', 'c1_windows', ...
            [char(voyage_id) '.mat']), ...
        'voyage_id', char(voyage_id), ...
        'water_depth_m', double(rb.scn.water_depth_m), 'ctrl_mode', 0));
end
ctxid = local_ctxid(ROOT, fz, pc, voyage_id, arm, isSmoke);
cargs = {'SkipInject', true, 'BindingGuard', bind, 'Seed', 'phase4f-ppe-formal-epoch1'};
if isSmoke && isfinite(opt.SmokeFixedDelta)
    cargs = [cargs, {'InitialDLam', opt.SmokeFixedDelta}];
end
[ds, meta] = ppe.run_cstar(rb.scn, winfun, arm, T_eval, ctxid, cargs{:});

% ---- (7)(8) 证据落盘 ----
vdir = fullfile(opt.OutDir, char(voyage_id));
if ~exist(vdir, 'dir'), mkdir(vdir); end
run_id = local_run_id(arm, voyage_id, isSmoke);
srcsel = ppe.write_sourcesel_v1(ds, opt.OutDir, voyage_id, arm, run_id, bind);
traj = ppe.write_traj_v1(ds, local_deriv(ROOT), opt.OutDir, voyage_id, arm, run_id);

% ---- (9) FC-3（正式模式；smoke 跳过且明确标记）----
if isSmoke
    feas = struct('applies', false, 'reason', 'NONFORMAL_SMOKE');
    fec = struct('fuel_raw_kg', NaN, 'SOC0', ds.SOC(1), 'SOC_end', ds.SOC(end), ...
        'J_FEC_kg', NaN);
else
    feas = ppe.feasibility_contract_v2(ds, cfg, 'T_frozen_s', T_eval, 'C_bus', local_cbus(ROOT));
    fec = t445.metrics_fec(ds, cfg);
end

% ---- (10) 终端记录 ----
rec = struct();
rec.schema = 'ppe_formal_arm_voyage_v3';
rec.formal_epoch = 3;
rec.mode = opt.Mode;
rec.is_formal_result = ~isSmoke;
rec.run_id = run_id; rec.voyage_id = char(voyage_id); rec.arm = char(arm);
rec.freeze_root_sha256 = fz.freeze_root_sha256;
rec.repaired_execution_commit = fz.repaired_execution_commit;
rec.slx_sha256 = model.sha256_file(fullfile(ROOT, 'model', [MDL '.slx']));
rec.T_eval = T_eval;
rec.arm_binding = bind;
rec.arm_binding_sha256 = local_json_sha(bind);
rec.pms_source_mode = bind.workspace_readback.PMS_SOURCE_MODE;
rec.ems_ctrl_law = bind.workspace_readback.EMS_CTRL_LAW;
rec.forecast_source = bind.forecast_source;
rec.controller_identity = bind.controller_identity;
rec.source_selection_telemetry = srcsel;
rec.trajectory = traj;
rec.controller_trace_sha256 = local_trace_sha(meta);
rec.n_segments = meta.n_segments;
if isfield(meta, 'binding_guard_applied')
    rec.simulationinput_binding = meta.binding_guard_applied;   % 逐段下发到 SimulationInput 的变量名
else
    rec.simulationinput_binding = {};
end
rec.binding_guard_applied_n_segments = meta.n_segments;
rec.delta_lambda = meta.dlambda_1;
rec.warmup_state = meta.warmup_state;
rec.hard_feasibility = feas;
rec.fuel_raw_kg = fec.fuel_raw_kg;
rec.SOC0 = fec.SOC0; rec.SOC_end = fec.SOC_end; rec.J_FEC_kg = fec.J_FEC_kg;
rec.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
fid = fopen(fullfile(vdir, sprintf('%s.json', char(arm))), 'w');
fwrite(fid, jsonencode(rec, 'PrettyPrint', true));
fclose(fid);
fprintf('[EP3] %s %s mode=%s -> %s (T=%.0f s, %d seg)\n', ...
    voyage_id, arm, opt.Mode, vdir, T_eval, meta.n_segments);
end

% =============== helpers ===============
function law = local_law(arm)
laws = struct('P0', 5, 'C0', 6, 'C1', 6, 'C2', 6);
law = laws.(arm);
end

function f = local_fsrc(arm)
fs = struct('P0', 'N/A_NO_FORECAST', 'C0', 'oracle', 'C1', 'c1_stored', 'C2', 'live_bridge');
f = fs.(arm);
end

function scn = local_smoke_scn(ROOT, T) %#ok<INUSD>
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
scn = evalin('base', 'SCENARIOS(1)');
scn.hotel_kW = 700;                 % 合成高负荷条件（同 EP3-P1 非饱和夹具）
scn.speed_profile_mode = 'time';
scn.v_time_s = [0 T]; scn.v_value_mps = [7.0 7.0];
scn.current_time_s = [0 1]; scn.current_value_mps = [0 0];
scn.initial_SOC = 0.60; scn.stop_time_s = T;
end

function il = local_smoke_injection(scn)
% 通用注入清单（刻意包含 mission_v3 的缺陷值：PMS_SOURCE_MODE=0 / EMS_CTRL_LAW=0）
names = {'VOYAGE_SPEED_MODE', 'VOYAGE_V_TIME_S', 'VOYAGE_V_VALUE_MPS', ...
    'VOYAGE_CURRENT_MODE', 'VOYAGE_CURRENT_TIME_S', 'VOYAGE_CURRENT_VALUE_MPS', ...
    'VOYAGE_WATER_DEPTH_M', 'PMS_SOURCE_MODE', 'EMS_CTRL_LAW'};
vals = {1.0, scn.v_time_s(:), scn.v_value_mps(:), 0.0, [0 1], [0 0], ...
    scn.water_depth_m, 0.0, 0.0};   % 与 names 同为行 cell（struct 尺寸需一致）
il = struct('var_name', names, 'value', vals);
end

function [Pbar, prov, use_prev] = local_hold_winfun(t0, ds, p0) %#ok<INUSD>
% 固定 δλ 夹具：ZOH 保持首段设定值（EP3-P1 同工况，经生产入口）
Pbar = 600 * ones(12, 1);
prov = struct('source_id', 'ep3_smoke_fixed_delta', 't0', t0);
use_prev = true;
end

function [Pbar, prov, use_prev] = local_smoke_winfun(t0, ds, p0, pbar) %#ok<INUSD>
Pbar = pbar * ones(12, 1);
prov = struct('source_id', 'ep3_smoke_synthetic', 't0', t0);
use_prev = false;
end

function [rb, T_eval] = local_formal_voyage(ROOT, voyage_id) %#ok<INUSD>
teval = jsondecode(fileread(fullfile(ROOT, 'config', 'phase4f_ppe_teval_table.json')));
T_eval = [];
for i = 1:numel(teval.voyages)
    if strcmp(teval.voyages(i).voyage_id, voyage_id), T_eval = teval.voyages(i).T_eval; break; end
end
if isempty(T_eval), error('PPE:EP3:VOYAGE', 'voyage %s 不在 T_eval 表', voyage_id); end
REPO = fileparts(fileparts(ROOT));
ACT = fullfile(REPO, 'docs', 'phase4', 'd_dataset_v3', 't432_activation_record.json');
led = jsondecode(fileread(fullfile(ROOT, 'results', 'dataset_v3', 'formal', 'pass2_iid_ledger.json')));
vrow = [];
for i = 1:numel(led.voyages)
    if strcmp(led.voyages(i).voyage_id, voyage_id), vrow = led.voyages(i); break; end
end
plan = fleet_v3.plan_load(ACT, 'stage_b_iid');
ctx = fleet_v3.ctx_build(plan);
rb = ppe.rebuild_voyage(plan, ctx, vrow);
end

function p = local_plan_file(ROOT, voyage_id)
p = fullfile(ROOT, 'artifacts', 'ppe_formal', 'plans', [char(voyage_id) '.json']);
end

function c = local_ctxid(ROOT, fz, pc, voyage_id, arm, isSmoke)
c = struct();
c.study_config_sha256 = pc.study_config_sha256;
c.code_commit_sha = fz.repaired_execution_commit;
c.hf_config_sha256 = 'EP3_HF_FROZEN';
c.slx_sha256 = model.sha256_file(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));
c.cstar_impl_sha256 = 'EP3_COMPOSITE';
c.corpus_identity_sha256 = fz.injection_chain.science(1).sha256;
c.teval_identity_sha256 = 'EP3_TEVAL';
c.voyage_id = char(voyage_id);
c.arm = char(arm);
if isSmoke
    c.predictor_identity = 'N/A';
else
    c.predictor_identity = struct('keras_sha256', 'EP3_FROZEN_PREDICTOR', ...
        'selected_config_sha256', 'EP3_FROZEN_PREDICTOR', ...
        'formal_norm_v3_sha256', 'EP3_FROZEN_PREDICTOR', ...
        'predictor_norm_ext_v1_sha256', 'EP3_FROZEN_PREDICTOR');
end
end

function d = local_deriv(ROOT)
d = struct('eta_rect', evalin('base', 'ASSUMED_eta_rect'), ...
    'ccs_v_hi', evalin('base', 'CCS_Vdc_Steady_Band(1)'), ...
    'ccs_v_lo', evalin('base', 'CCS_Vdc_Transient_Band(1)'));
end

function cb = local_cbus(ROOT)
cb = evalin('base', 'ASSUMED_C_bus');
end

function h = local_json_sha(s)
h = model.sha256_file(local_tmp_json(s));
end

function p = local_tmp_json(s)
p = [tempname '.json'];
fid = fopen(p, 'w'); fwrite(fid, jsonencode(s)); fclose(fid);
end

function h = local_trace_sha(meta)
% controller trace 身份：对逐段窗哈希序列取 SHA-256（字符串序列 → 拼接后哈希）
if isfield(meta, 'window_sha256') && ~isempty(meta.window_sha256)
    w = meta.window_sha256(:);
    parts = cell(numel(w), 1);
    for i = 1:numel(w)
        if ischar(w{i}), parts{i} = w{i}; else, parts{i} = 'ZOH_HOLD_PREVIOUS'; end
    end
    h = local_sha_str(strjoin(parts(:)', '|'));
else
    h = 'NA';
end
end

function h = local_sha_str(str)
md = java.security.MessageDigest.getInstance('SHA-256');
d = typecast(md.digest(typecast(uint8(str), 'int8')), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
end

function rid = local_run_id(arm, vid, isSmoke)
md = java.security.MessageDigest.getInstance('SHA-256');
tag = 'smoke'; if ~isSmoke, tag = 'formal'; end
raw = uint8(sprintf('ppe|ep3|%s|%s|%s', tag, char(arm), char(vid)));
d = typecast(md.digest(typecast(raw, 'int8')), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
rid = h(1:16);
end

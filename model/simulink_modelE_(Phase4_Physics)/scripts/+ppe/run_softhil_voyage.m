function rec = run_softhil_voyage(voyage_id, arm, varargin)
% ppe.run_softhil_voyage — **soft-HIL 板上后端单航次执行器**（phase4-g，spec
% .specs/phase4-g-reatt-board-deployment；非正式：is_formal_result=false）。
%
% 与冻结件 ppe.run_epoch3_voyage.m（genB 正式入口）的**全部科学机械一致**：
% 冻结绑定校验（epoch3b/genB freeze + 路径审计）、场景重建、臂绑定
% （arm_runtime_bind + BindingGuard）、分段执行（run_cstar）、遥测/轨迹/审计
% 落盘、FC-3 可行性、FEC——逐字节复用同一路径（见下差异清单）。
%
% 仅有的差异（3 项）：
%   ① C2 推理后端 = 板上 int8（ppe.c2_bridge_board / ppe.c2_server_board，
%      CMD_REATT 串口服务），非本机 keras fp32；
%   ② 输出命名空间 = results/ppe_practical/soft_hil/<voyage>/<arm>.json
%      （禁止写入任何 formal*/qual* 命名空间——fail-closed 守卫）；
%   ③ 终端记录 schema = ppe_softhil_arm_voyage_v1，is_formal_result=false，
%      标注 SOFT_HIL_NONFORMAL + 板后端 provenance。
%
% 配对基线：冻结 genB 语料同航次 C2（host keras fp32）记录
% （results/ppe_practical/formal_epoch3_genB/），逐航次配对比较。
%
% Name-value:
%   'C2BridgeToken' REQUIRED（C2 only）char
%   'C2BridgePort'  char 板串口（缺省 getenv('EMS_REATT_PORT')）
%   'OutDir'        char（缺省 results/ppe_practical/soft_hil）
%
% FORBIDDEN：不改任何科学设定（同 run_epoch3_voyage 头注）；不写正式命名空间。
p = inputParser;
p.addParameter('C2BridgeToken', '', @ischar);
p.addParameter('C2BridgePort', '', @ischar);
p.addParameter('C2Mock', false, @(x) islogical(x) && isscalar(x));
p.addParameter('OutDir', '', @ischar);
p.parse(varargin{:});
opt = p.Results;
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
pc = ppe.ppe_config(); cfg = pc.t445;
MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL), load_system(fullfile(ROOT, 'model', [MDL '.slx'])); end

% ---- (0) 冻结绑定（fail-closed；与 genB 正式路径同一条链）----
fz = ppe.load_epoch3b_freeze_bindings(ROOT);
ppe.assert_genb_path(ROOT, fz.path_audit_names, fz.path_audit_first);

% ---- 输出命名空间（soft_hil 专用；formal*/qual* 一律拒绝）----
if isempty(opt.OutDir)
    opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', 'soft_hil');
end
if opt.C2Mock
    % mock 联调产物与板级证据隔离（不作板级证据）
    opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', 'soft_hil', '_mock');
end
% 子串匹配即足以覆盖 formal/、formal_epoch2、formal_epoch3(_genB)、
% qual_ep3*、qualification*（soft_hil 命名空间不含上述任一子串）
forbidden = {'formal', 'qual'};
for k_forbidden = 1:numel(forbidden)
    if contains(opt.OutDir, forbidden{k_forbidden})
        error('PPE:SOFTHIL:NS', ...
            'soft-HIL 输出禁止指向正式/资格化命名空间：%s', opt.OutDir);
    end
end

% ---- (1)(2) mission/场景重建 + 通用注入（formal 路径逐字复用）----
[rb, T_eval] = local_formal_voyage(ROOT, voyage_id);
ppe.inject_voyage(MDL, rb.comp, rb.scn, local_law(arm), T_eval);

% ---- (3) 权威臂绑定（通用注入之后；同正式路径）----
bind = ppe.arm_runtime_bind(arm, 'ForecastSource', local_fsrc(arm));

% ---- 遥测（内存内；同正式路径）----
model.set_ai_logging(MDL, 0);
lg = model.set_source_selection_logging(MDL, true);
if ~isempty(lg.missing)
    error('PPE:SOFTHIL:TELEMETRY', '遥测端口缺失：%s', strjoin(lg.missing, ', '));
end

% ---- (4)(5)(6) 分段执行：差异① C2 = 板后端 ----
wopts = struct( ...
    'T_eval', T_eval, ...
    'c1_windows_file', fullfile(ROOT, 'artifacts', 'ppe_formal', 'c1_windows', ...
        [char(voyage_id) '.mat']), ...
    'voyage_id', char(voyage_id), ...
    'water_depth_m', double(rb.scn.water_depth_m), 'ctrl_mode', 0);
if strcmp(arm, 'C2')
    if isempty(opt.C2BridgeToken)
        error('PPE:SOFTHIL:BRIDGE', 'C2 需要 C2BridgeToken');
    end
    bdir = fullfile(ROOT, 'artifacts', 'ppe_softhil', opt.C2BridgeToken);
    for sub = {'req', 'resp'}
        d = fullfile(bdir, sub{1});
        if exist(d, 'dir')
            fl = dir(fullfile(d, [sub{1} '_*.mat']));
            for j = 1:numel(fl)
                delete(fullfile(fl(j).folder, fl(j).name));
            end
        end
    end
    br = ppe.c2_bridge_board(opt.C2BridgeToken, opt.C2BridgePort, opt.C2Mock);
    wopts.c2_bridge = br;
    cleanup_br = onCleanup(@() br.stop());   %#ok<NASGU> 无 server 泄漏
end
winfun = ppe.winfun_plan(local_plan_file(ROOT, voyage_id), arm, wopts);
ctxid = local_ctxid(ROOT, fz, pc, voyage_id, arm);
cargs = {'SkipInject', true, 'BindingGuard', bind, 'Seed', 'phase4f-ppe-formal-epoch1'};
[ds, meta] = ppe.run_cstar(rb.scn, winfun, arm, T_eval, ctxid, cargs{:});

% ---- (7)(8) 证据落盘（同正式路径）----
vdir = fullfile(opt.OutDir, char(voyage_id));
if ~exist(vdir, 'dir'), mkdir(vdir); end
run_id = local_run_id(arm, voyage_id);
srcsel = ppe.write_sourcesel_v1(ds, opt.OutDir, voyage_id, arm, run_id, bind);
traj = ppe.write_traj_v1(ds, local_deriv(ROOT), vdir, voyage_id, arm, run_id);

% ---- (9) FC-3（同正式路径）----
ta0 = jsondecode(fileread(fullfile(ROOT, '..', '..', '.specs', ...
    'phase4-f-practical-predictive-ems', 'reserve_derivation_TA0.json')));
dr = ta0.derived_reserves;
PR = struct('dis_kW', dr.P_reserve_dis_kW, 'chg_kW', dr.P_reserve_chg_kW, ...
    'dis_kWh', dr.E_reserve_dis_kWh, 'chg_kWh', dr.E_reserve_chg_kWh);
feas = ppe.feasibility_contract_v2(ds, cfg, PR, T_eval, local_cbus(ROOT));
fec = t445.metrics_fec(ds, cfg);

% ---- (9b) 高速率审计 + 控制器迹线（同正式路径）----
aud = ppe.write_audit_v1(ds, cfg, vdir, voyage_id, arm, run_id);
ctr = ppe.write_ctrace_v1(local_ctrace(meta, arm), vdir, voyage_id, arm, run_id);

% ---- (10) 终端记录：差异②③ schema/标记 + 板后端 provenance ----
rec = struct();
rec.schema = 'ppe_softhil_arm_voyage_v1';
rec.SOFT_HIL_NONFORMAL = true;
if opt.C2Mock, rec.MOCK_BACKEND = true; end
rec.is_formal_result = false;
rec.backend = ['c2_board_int8（差异①：推理经 ppe.c2_bridge_board → ' ...
    'EMS02_CHIL_REATT8_CPU CMD_REATT，int8 全 CPU 形态；对照基线 = 冻结 genB ' ...
    'C2 host keras fp32）'];
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
rec.audit_artifact = aud;
rec.ctrace_artifact = ctr;
rec.controller_trace_sha256 = local_trace_sha(meta);
rec.n_segments = meta.n_segments;
if isfield(meta, 'binding_guard_applied')
    rec.simulationinput_binding = meta.binding_guard_applied;
else
    rec.simulationinput_binding = {};
end
rec.binding_guard_applied_n_segments = meta.n_segments;
rec.delta_lambda = meta.dlambda_1;
rec.warmup_state = meta.warmup_state;
rec.hard_feasibility = feas;
rec.fuel_raw_kg = fec.fuel_raw_kg;
rec.SOC0 = fec.SOC0; rec.SOC_end = fec.SOC_end; rec.J_FEC_kg = fec.J_FEC_kg;
rec.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z'));
fid = fopen(fullfile(vdir, sprintf('%s.json', char(arm))), 'w');
fwrite(fid, jsonencode(rec, 'PrettyPrint', true));
fclose(fid);
fprintf('[SOFTHIL] %s %s -> %s (T=%.0f s, %d seg, fuel=%.2f kg)\n', ...
    voyage_id, arm, vdir, T_eval, meta.n_segments, fec.fuel_raw_kg);
end

% =============== helpers（与 run_epoch3_voyage.m 同源复用语义） ===============
function law = local_law(arm)
laws = struct('P0', 5, 'C0', 6, 'C1', 6, 'C2', 6);
law = laws.(arm);
end

function f = local_fsrc(arm)
fs = struct('P0', 'N/A_NO_FORECAST', 'C0', 'oracle', 'C1', 'c1_stored', 'C2', 'live_bridge');
f = fs.(arm);
end

function ct = local_ctrace(meta, arm)
if strcmp(arm, 'P0')
    ct = struct('applies', false, 'forecast_source_id', 'N/A');
    return;
end
switch char(arm)
    case 'C0', srcid = 'oracle';
    case 'C1', srcid = 'c1_stored';
    case 'C2', srcid = 'live_bridge';
    otherwise, error('PPE:SOFTHIL:CTRACE', 'arm %s 非法', arm);
end
dlam = meta.dlambda_1;
ct = struct('applies', true, 'decision_period_s', 10, ...
    'time', (0:10:(numel(dlam) - 1) * 10)', ...
    'delta_lambda', dlam(:), 'forecast_source_id', srcid);
ct.forecast_window_sha256 = meta.window_sha256;   % 构造后赋值（Q-OBS-21）
end

function [rb, T_eval] = local_formal_voyage(ROOT, voyage_id)
teval = jsondecode(fileread(fullfile(ROOT, 'config', 'phase4f_ppe_teval_table.json')));
T_eval = [];
for i = 1:numel(teval.voyages)
    if strcmp(teval.voyages(i).voyage_id, voyage_id), T_eval = teval.voyages(i).T_eval; break; end
end
if isempty(T_eval), error('PPE:SOFTHIL:VOYAGE', 'voyage %s 不在 T_eval 表', voyage_id); end
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

function c = local_ctxid(ROOT, fz, pc, voyage_id, arm)
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
c.predictor_identity = struct('keras_sha256', 'EP3_FROZEN_PREDICTOR', ...
    'selected_config_sha256', 'EP3_FROZEN_PREDICTOR', ...
    'formal_norm_v3_sha256', 'EP3_FROZEN_PREDICTOR', ...
    'predictor_norm_ext_v1_sha256', 'EP3_FROZEN_PREDICTOR');
end

function d = local_deriv(ROOT) %#ok<INUSD>
d = struct('eta_rect', evalin('base', 'ASSUMED_eta_rect'), ...
    'ccs_v_hi', evalin('base', 'CCS_Vdc_Steady_Band(1)'), ...
    'ccs_v_lo', evalin('base', 'CCS_Vdc_Transient_Band(1)'));
end

function cb = local_cbus(ROOT) %#ok<INUSD>
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

function rid = local_run_id(arm, vid)
md = java.security.MessageDigest.getInstance('SHA-256');
raw = uint8(sprintf('ppe|softhil|%s|%s', char(arm), char(vid)));
d = typecast(md.digest(typecast(raw, 'int8')), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
rid = h(1:16);
end

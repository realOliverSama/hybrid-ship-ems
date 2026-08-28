%% ========================================================================
%  51_generate_fleet_profiles.m — ECMS Phase 2：批量 Baseline 闭环仿真（T-201，FR-202）
%  产出（不入库，产物策略见 AGENTS.md §13）:
%    results/ecms_phase2/fleet_profiles/voyage_XXX.mat   每航次最小字段集
%    results/ecms_phase2/fleet_profiles/summary_seed<S>.json  逐航次状态汇总
%  执行:
%    cd model/simulink_modelD && nohup /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/51_generate_fleet_profiles.m'))" > results/ecms_phase2/fleet_gen.log 2>&1 &
%  参数（base 覆盖，可选）:
%    FLEET_N    仿真航次数（缺省：parsim 可用=200，串行降级=80，NFR-202）
%    FLEET_START 起始航次序号（缺省 1）
%  设计语义（design §2 / FR-202 / NFR-202）:
%    - 每航次 law=0 Baseline 闭环仿真；剖面经 Simulink.SimulationInput.setVariable
%      注入（VOYAGE_* + SCN），**不重建模型**（快路径，与 run_profile_sim 逐位一致，
%      证据：experiments/t201_inject_verify.m，maxdiff=0）；
%    - parsim 并行（SetupFcn 在 worker 上跑 config）；不可用时串行 sim 降级；
%    - 最小字段集：t_s / P_demand_kW / v_ship_mps（+ 元数据与降额/闭合校验记录）；
%    - 降额事件 >0 -> 该航次 rejected（不中断整批，原因记录）；
%    - 抽样深度验证：每 20 航次抽 1 检查 Vdc_min>=675 V 且能量闭合 <0.1%；
%    - 断点续跑：voyage_XXX.mat 已存在则跳过。
%  完成判定：日志末尾 "[51] FLEET COMPLETE n_ok=.. n_rejected=.."。
%  修订：2026-08-25 主 Agent 重写执行核心（worker 版慢路径 assignin+run_profile_sim
%        ~197 s/航次且 parsim 未真正使用；改为 setVariable 快路径 ~70 s + parsim）。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[51] ===== ECMS Phase 2 — 批量 Baseline 闭环仿真（T-201，FR-202） =====\n');
fprintf('[51] ROOT = %s\n', ROOT);
fprintf('[51] 开始: %s\n', datestr(now, 'yyyy-mm-dd HH:MM:SS'));

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

seed = ECMS_FLEET_SEED;
useParsim = exist('parsim', 'file') > 0;   % parsim 为 p 码时 exist 返回 6，勿用 ==2
if exist('FLEET_N', 'var') && ~isempty(FLEET_N)
    N = FLEET_N;
elseif useParsim
    N = 200;                     % design §2 默认 N
else
    N = 80;                      % NFR-202 串行降级（记录原因）
    fprintf('[51] 降级：Parallel Computing Toolbox 不可用 -> 串行 N=%d（NFR-202 下限 80）\n', N);
end
if exist('FLEET_START', 'var') && ~isempty(FLEET_START)
    START = FLEET_START;
else
    START = 1;
end

% ---- 航次定义（50 生成入库的 JSON）----
fleetDir = fullfile(ROOT, 'results', 'ecms_phase2', 'fleet');
jsonPath = fullfile(fleetDir, sprintf('voyages_seed%d.json', seed));
if ~exist(jsonPath, 'file')
    error('ECMS:T201', '缺少航次定义 %s（先运行 50_voyage_generator.m）', jsonPath);
end
vjson = jsondecode(fileread(jsonPath));
V = vjson.voyages;
assert(numel(V) >= 200, 'ECMS:T201', '航次定义数量异常：%d', numel(V));
assert(N <= numel(V) && N >= 1, 'ECMS:T201', 'FLEET_N=%d 超出航次集 [1,%d]', N, numel(V));
fprintf('[51] 航次集：seed=%d 共 %d 个，本批 %d~%d（%d 个），parsim=%d\n', ...
    seed, numel(V), START, START + N - 1, N, useParsim);

profDir = fullfile(ROOT, 'results', 'ecms_phase2', 'fleet_profiles');
if ~exist(profDir, 'dir'), mkdir(profDir); end

MDL = 'HybridShip_ModelD_DCBus';
mdlPath = fullfile(ROOT, 'model', [MDL '.slx']);
load_system(mdlPath);
fprintf('[51] 模型加载（单次，不随航次重建）: %s\n', MDL);

%% ======================= 构建 SimulationInput 集（跳过已完成） =======================
todo = [];
for i = START:(START + N - 1)
    if ~exist(fullfile(profDir, sprintf('voyage_%03d.mat', V(i).id)), 'file')
        todo(end + 1) = i; %#ok<AGROW>
    end
end
n_skip = N - numel(todo);
fprintf('[51] 待仿真 %d 个（跳过已完成 %d 个）\n', numel(todo), n_skip);

in = Simulink.SimulationInput.empty(0, 1);
for k = 1:numel(todo)
    v = V(todo(k));
    scn = ecms2.voyage_scn(v);
    inK = Simulink.SimulationInput(MDL);
    inK = inK.setVariable('SCN', scn);
    inK = inK.setVariable('EMS_CTRL_LAW', 0);
    inK = inK.setVariable('EMS_MODE', 0);
    inK = inK.setVariable('Bat_SOC_init', scn.initial_SOC);
    inK = inK.setVariable('AI_Pred_Source', 0);
    inK = inK.setVariable('AI_TPeak_Source', 0);
    inK = inK.setVariable('VOYAGE_SPEED_MODE', 1);
    inK = inK.setVariable('VOYAGE_V_TIME_S', scn.v_time_s);
    inK = inK.setVariable('VOYAGE_V_VALUE_MPS', scn.v_value_mps);
    inK = inK.setVariable('VOYAGE_CURRENT_MODE', 1);
    inK = inK.setVariable('VOYAGE_CURRENT_TIME_S', scn.current_time_s);
    inK = inK.setVariable('VOYAGE_CURRENT_VALUE_MPS', scn.current_value_mps);
    inK = inK.setModelParameter('StopTime', sprintf('%.2f', floor(scn.stop_time_s * 100) / 100));
    inK = inK.setUserString(sprintf('voyage_%03d', v.id));
    in(k) = inK; %#ok<AGROW>
end

%% ======================= 执行（parsim 优先，串行降级） =======================
% SetupFcn 用显式函数 model.parsim_setup（2026-08-26 Mac 端修复）：
% 原 '@() eval(setupStr)' 在 worker 上失败——eval 在匿名函数静态 workspace
% 执行，run() 创建的新变量（如 Model_SystemScope）报「静态工作区」错误，且
% 变量进不了 base，模型参数解析失败。见 handoff_mac_issues.md 问题 3。

tBatch = tic;
outs = cell(numel(todo), 1);
if useParsim && ~isempty(in)
    fprintf('[51] parsim 启动（SetupFcn 注入 config）: %s\n', datestr(now, 'HH:MM:SS'));
    try
        simOuts = parsim(in, 'ShowProgress', 'on', 'SetupFcn', @() model.parsim_setup(ROOT), ...
            'TransferBaseWorkspaceVariables', 'off');
        for k = 1:numel(todo), outs{k} = simOuts(k); end
        fprintf('[51] parsim 完成: %.1f s（均值 %.1f s/航次）\n', toc(tBatch), toc(tBatch) / numel(todo));
    catch errP
        fprintf('[51] parsim 失败（%s）-> 串行降级\n', errP.message);
        useParsim = false;
    end
end
if ~useParsim
    for k = 1:numel(todo)
        t0 = tic;
        try
            outs{k} = sim(in(k));
        catch errS
            outs{k} = errS;
        end
        if mod(k, 5) == 0 || k == 1
            fprintf('[51] 串行进度 %d/%d（最近 %.1f s/航次 | %s）\n', k, numel(todo), toc(t0), datestr(now, 'HH:MM:SS'));
        end
    end
    fprintf('[51] 串行完成: %.1f s\n', toc(tBatch));
end

%% ======================= 逐航次提取 + 校验 + 落盘（client 侧） =======================
Vdc_min_ok = 675;      % 抽样深度验证：CCS 稳态下限（design §5）
n_ok = 0; n_rejected = 0;
summary = struct('voyage', {}, 'name', {}, 'rejected', {}, 'reason', {}, ...
    'duration_s', {}, 'Vdc_min_V', {}, 'closure_frac', {}, ...
    'n_derate_samples', {}, 'wall_s', {});

for k = 1:numel(todo)
    i = todo(k);
    v = V(i);
    rejected = false; reason = '';
    t0 = tic;
    if isa(outs{k}, 'MException')
        rejected = true; reason = sprintf('sim_error:%s', outs{k}.message);
        prof = struct('t', [], 'P_demand_kW', [], 'v_ship_mps', [], 'duration_s', NaN, ...
            'Vdc_min_V', NaN, 'energy_closure', struct('residual_frac', NaN, 'pass', false), ...
            'n_derate_samples', NaN, 'n_undervoltage_samples', NaN, 'derate_event', true);
    else
        ds0 = model.extract_logsout(outs{k});
        ds = model.resample_1s(ds0);
        prof = struct();
        prof.t = ds.t;
        prof.P_demand_kW = ds.P_prop_demand_kW + ds.P_hotel_kW;   % 因果隔离提取点（FR-006）
        prof.v_ship_mps = ds.v_ship_mps;
        prof.duration_s = prof.t(end) - prof.t(1);
        prof.Vdc_min_V = min(ds.V_dc_V);
        prof.n_undervoltage_samples = sum(ds.V_dc_V < 600);
        prof.n_derate_samples = sum(ds.P_prop_limit_kW < Mot_P_nom - 1e-6);
        prof.derate_event = (prof.n_undervoltage_samples > 0) || (prof.n_derate_samples > 0) ...
            || (max(ds.P_unserved_kW) > 1e-6);
        % 能量闭合在原始 0.01 s 网格（run_profile_sim 同口径）
        tr = ds0.t; dtr = [diff(tr); tr(2) - tr(1)];
        Ein = sum((ASSUMED_eta_rect .* ds0.P_gen_actual_kW + ds0.P_bat_actual_kW) .* dtr) / 3600;
        Edem = sum((ds0.P_prop_demand_kW + ds0.P_hotel_kW) .* dtr) / 3600;
        resid = (Ein - Edem) / Edem;
        prof.energy_closure = struct('residual_frac', resid, 'pass', abs(resid) < 1e-3);
    end
    wall = toc(t0);

    if ~rejected && prof.derate_event
        rejected = true;
        reason = sprintf('derate(n_uv=%d,n_der=%d)', prof.n_undervoltage_samples, prof.n_derate_samples);
    end
    if ~rejected && mod(i, 20) == 0
        if prof.Vdc_min_V < Vdc_min_ok
            rejected = true; reason = [reason, '|deep_check:Vdc_min<675V'];
        end
        if ~prof.energy_closure.pass
            rejected = true; reason = [reason, sprintf('|deep_check:closure=%.4f%%>=0.1%%', 100 * prof.energy_closure.residual_frac)];
        end
    end
    if rejected, n_rejected = n_rejected + 1; else, n_ok = n_ok + 1; end

    % ---- 最小字段集 .mat（不入库）----
    m = struct();
    m.t_s = prof.t;                 % 1 s 均匀网格
    m.P_demand_kW = prof.P_demand_kW;
    m.v_ship_mps = prof.v_ship_mps;
    m.voyage_id = v.id;
    m.name = v.name;
    m.seed = seed;
    m.law = 0;
    m.simulation_generated = true;
    m.rejected = rejected;
    if ~isempty(reason), m.reject_reason = reason; end
    m.duration_s = prof.duration_s;
    m.Vdc_min_V = prof.Vdc_min_V;
    m.closure_residual_frac = prof.energy_closure.residual_frac;
    m.n_derate_samples = prof.n_derate_samples;
    m.n_undervoltage_samples = prof.n_undervoltage_samples;
    save(fullfile(profDir, sprintf('voyage_%03d.mat', v.id)), '-struct', 'm');

    summary(end + 1) = struct('voyage', v.id, 'name', v.name, ...
        'rejected', rejected, 'reason', reason, ...
        'duration_s', prof.duration_s, 'Vdc_min_V', prof.Vdc_min_V, ...
        'closure_frac', prof.energy_closure.residual_frac, ...
        'n_derate_samples', prof.n_derate_samples, 'wall_s', wall); %#ok<AGROW>

    rejStr = '';
    if rejected, rejStr = sprintf('  REJECTED:%s', reason); end
    pmax = NaN; if ~isempty(prof.P_demand_kW), pmax = max(prof.P_demand_kW); end
    fprintf('[51] voyage_%03d/%03d dur=%.0fs Pmax=%.0fkW Vdc_min=%.1fV closure=%.4f%% derate=%d%s\n', ...
        v.id, START + N - 1, prof.duration_s, pmax, prof.Vdc_min_V, ...
        100 * prof.energy_closure.residual_frac, prof.n_derate_samples, rejStr);
end

%% ======================= 汇总 + 完成判定 =======================
sumPath = fullfile(profDir, sprintf('summary_seed%d.json', seed));
sout = struct();
sout.meta = struct('seed', seed, 'requested_N', N, 'start', START, ...
    'law', 0, 'parsim', useParsim, 'injection', 'SimulationInput.setVariable (fast path)', ...
    'n_ok', n_ok, 'n_rejected', n_rejected, 'n_skip', n_skip, ...
    'deep_check_rule', 'every 20th voyage: Vdc_min>=675V and closure<0.1%', ...
    'generated', datestr(now, 'yyyy-mm-dd HH:MM:SS'));
sout.voyages = num2cell(summary);
ecms.write_json_deep(sumPath, sout);
fprintf('[51] 汇总已写入 %s\n', sumPath);

fprintf('\n[51] FLEET COMPLETE n_ok=%d n_rejected=%d n_skip=%d (seed=%d, %s)\n', ...
    n_ok, n_rejected, n_skip, seed, datestr(now, 'yyyy-mm-dd HH:MM:SS'));

function h = build_commitment_harness()
%BUILD_COMMITMENT_HARNESS 脚本化 Commitment chart 测试台（Model E T-005，禁 GUI）
%   h = tests.build_commitment_harness()
%   在 tempdir 创建一次性最小模型：PMS_Commitment_Supervisor chart + 8 输入(From Workspace)
%   + 15 输出(Outport, 信号记录)。与正式 builder 共用 model.build_commitment_chart_e()，
%   保证 harness 内 chart 与 07_PMS_v2 集成 chart 逐位一致。
%   用法（test_commitment_chart_e.m）：
%     h = build_commitment_harness();
%     每用例向 base 写 8 张注入表（IN_*_TS，[t value] 矩阵）→ set_param StopTime → sim。
%   依据：design §5.4 + FR-436~440；注入机制 = assignin/From Workspace+Interpolate off
%   （NFR-415 工程坑清单口径）。

RT = fileparts(fileparts(fileparts(mfilename('fullpath'))));   % Model E 工作区根
if isempty(RT) || ~exist(fullfile(RT, 'config'), 'dir'), RT = pwd; end
addpath(fullfile(RT, 'config'), fullfile(RT, 'scripts'));
% 本函数是函数作用域——chart Parameter 与 From Workspace 注入表都在 base 解析，
% 必须经 evalin('base',...) 加载（与 build_model_e 同模式，FR-451 顺序：物理→控制）
evalin('base', sprintf("run('%s')", fullfile(RT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(RT, 'config', 'ModelE_Control_Parameters.m')));

MDL = 'tmp_pms_commitment_harness';
if bdIsLoaded(MDL), close_system(MDL, 0); end
old = tempdir;
mdlPath = fullfile(old, [MDL '.slx']);
if exist(mdlPath, 'file'), delete(mdlPath); end
new_system(MDL);
load_system(MDL);
set_param(MDL, 'SolverType', 'Fixed-step', 'Solver', 'ode3', 'FixedStep', '0.01', ...
    'StopTime', '10', 'SignalLogging', 'on', 'SignalLoggingName', 'logsout', ...
    'ReturnWorkspaceOutputs', 'on', 'AlgebraicLoopMsg', 'error');

% chart（与 builder 同一构建函数）
chartBlk = [MDL '/PMS_Commitment_Supervisor'];
model.build_commitment_chart_e(chartBlk);

% 输入侧：8 张 From Workspace 表 → Inport → chart
inNames = {'P_commit_kW', 'state_DG2', 'fault_latch_DG1', 'fault_latch_DG2', ...
    'force_start_dg2_req', 'force_start_reason', 'force_stop_dg2_req', 'force_stop_reason'};
inVar = cellfun(@(s) sprintf('HC_%s_TS', upper(s)), inNames, 'UniformOutput', false);
for k = 1:numel(inNames)
    fw = sprintf('fw_%s', inNames{k});
    add_block('simulink/Sources/From Workspace', [MDL '/' fw], 'Position', [30 30+45*(k-1) 110 55+45*(k-1)], ...
        'VariableName', inVar{k}, 'SampleTime', '0.01', 'Interpolate', 'off', ...
        'OutputAfterFinalValue', 'HoldingFinalValue');
    add_line(MDL, sprintf('%s/1', fw), sprintf('PMS_Commitment_Supervisor/%d', k), 'autorouting', 'on');
    % 默认注入表（用例覆盖前提供安全默认：无故障/无 force/P_commit=0/state=0）
    assignin('base', inVar{k}, [0 0; 1e9 zeros(1, 1)]);
end
% 输出侧：15 Outport + 信号记录
outNames = {'cmd_onoff_DG1', 'cmd_onoff_DG2', 'commitment_state', 'reason_code', ...
    'start_hold_timer', 'stop_hold_timer', 'online_timer', 'audit_valid', ...
    'audit_old_state', 'audit_new_state', 'audit_P_commit_kW', ...
    'audit_t_start_hold', 'audit_t_stop_hold', 'audit_t_online', 'audit_safety_override'};
for k = 1:numel(outNames)
    op = sprintf('out_%s', outNames{k});
    add_block('simulink/Sinks/Out1', [MDL '/' op], 'Position', [560 30+40*(k-1) 590 55+40*(k-1)]);
    add_line(MDL, sprintf('PMS_Commitment_Supervisor/%d', k), sprintf('%s/1', op), 'autorouting', 'on');
    % 信号记录打在源端口（chart 输出端口，A2 configureLogging 同约定）
    chp = get_param(chartBlk, 'PortHandles');
    set_param(chp.Outport(k), 'DataLogging', 'on');
    ln = get_param(chp.Outport(k), 'Line');
    if ln > 0, set_param(ln, 'Name', outNames{k}); end
end
save_system(MDL, mdlPath);
h = struct('mdl', MDL, 'mdlPath', mdlPath, 'chartBlk', chartBlk, ...
    'inNames', {inNames}, 'inVar', {inVar}, 'outNames', {outNames});
end
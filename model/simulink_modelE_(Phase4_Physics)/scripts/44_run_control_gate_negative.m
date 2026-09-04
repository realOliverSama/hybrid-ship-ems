%% ========================================================================
%  44_run_control_gate_negative.m — T-011 控制层 Gate 负向验证（FR-455 判据 6 负向）
%  构造缺陷注入（42 号模式：仅 base workspace 注入，结束恢复 nominal，绝不修改文件），
%  确认 43 判据函数（C1/C2/C6/C7）对每类缺陷正确 FAIL——即真实 43 运行中此类缺陷
%  将导致 overall=false + error('GATE:*') 非零退出（Gate FAIL 禁止提交）。
%  缺陷清单：
%    D1 FORCED reason 注入（force 通道绕过阈值语义，design §5.4 枚举 7）→ C2 拒收
%    D2 正常 start timer 不足（t_start<3000）→ C2 拒收
%    D3 正常 stop min_up 不足（t_online<30000）→ C2 拒收
%    D4 应触发未触发 override（屏蔽 S2+S4 供给不足双判据，hold_ticks=1e9，Parameter 通道）
%       → 真实 S4 场景重跑 → C6 报缺失触发
%    D5 确定性破坏（单采样点改写）→ C1 拒收
%    D6 episode 一对一破坏（两次正常 start 仅一次饱和事件）→ C7 拒收
%  结果附加到 results/gate_control_result.json 的 negative_control 字段（42 号模式）。
%  用法：cd model/simulink_modelE_(Phase4_Physics); eval(fileread('scripts/44_run_control_gate_negative.m'))
%  编排主体在 +gate_ctrl/run_negative.m（eval 上下文不支持脚本内函数定义）。
%% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

neg = gate_ctrl.run_negative(ROOT);

resDir = fullfile(ROOT, 'results');
if ~exist(resDir, 'dir'), mkdir(resDir); end
gjPath = fullfile(resDir, 'gate_control_result.json');
if exist(gjPath, 'file')
    gj = jsondecode(fileread(gjPath));
    gj.negative_control = neg;
else
    gj = struct('negative_control', neg);
end
gate_ctrl.write_result(gjPath, gj);
if ~neg.all_detected
    error('GATE:negative_validation_failed — 44 号负向验证存在未被检出的缺陷（判据防线失效）');
end
fprintf('[44] NEGATIVE VALIDATION PASS：全部 %d 类构造缺陷被 43 判据正确检出\n', numel(neg.defects));
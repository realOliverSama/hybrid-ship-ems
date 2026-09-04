%% ========================================================================
%  43_run_modelE_control_gate.m — T-009/T-012 Model E 控制层 Gate（FR-452~456）
%  三臂对比（R0=law0+AGGREGATE / R5=law5+AGGREGATE / P2=BASELINE_POLICY）+
%  MANUAL M1/M2/M3 × P2 + Safety 注入 S1~S5 + 确定性两跑 + 判据 C1~C7。
%  输出 results/gate_control_result.json；FAIL 时 error('GATE:*') 非零退出
%  （Gate FAIL 禁止提交，AGENTS.md §12）。
%  可选 base 变量（T-012 注入）：
%    GATE43_BASELINE_PATH —— candidate 基线路径（Phase 1 引用 T-012 机器检查结论）
%    GATE43_QUICK         —— true 时骨架自检（S01×P2 两跑 + M1，T-009 验证级；默认全矩阵）
%  用法：cd model/simulink_modelE_(Phase4_Physics); eval(fileread('scripts/43_run_modelE_control_gate.m'))
%  编排主体在 +gate_ctrl/run_gate.m（数字开头文件无法作为函数调用——项目惯例）。
%% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

R = gate_ctrl.run_gate(ROOT, evalin('base', 'exist(''GATE43_QUICK'', ''var'') && GATE43_QUICK'));

resDir = fullfile(ROOT, 'results');
if ~exist(resDir, 'dir'), mkdir(resDir); end
gate_ctrl.write_result(fullfile(resDir, 'gate_control_result.json'), R);
fprintf('[43] overall=%s wall=%.0f s\n', string(R.overall), R.wall_time_s);
if ~R.overall
    error('GATE:control_gate_failed — 43 号控制 Gate 判据存在 FAIL（Gate FAIL 禁止提交）');
end
fprintf('[43] CONTROL GATE PASS\n');
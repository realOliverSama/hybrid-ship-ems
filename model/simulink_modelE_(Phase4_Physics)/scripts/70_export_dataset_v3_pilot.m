%% ========================================================================
%  70_export_dataset_v3_pilot.m — Phase 4-D T-007：Dataset v3 pilot producer（薄入口）
%  编排主体在 +dataset_v3/export_pilot.m（eval(fileread) 上下文不支持脚本内函数定义
%  ——项目惯例，44 号同款）。职责：环境准备（config 三件载入 base）→ 调用导出主体
%  → FAILED 航次即 error('GATE:*')。
%  导出内容与纪律（registry 驱动、causal fixtures、PILOT_ONLY、Model E 零改动）：
%  见 +dataset_v3/export_pilot.m 文档头。
%  用法：cd model/simulink_modelE_(Phase4_Physics); eval(fileread('scripts/70_export_dataset_v3_pilot.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

res = dataset_v3.export_pilot(ROOT);
if res.n_failed > 0
    error('GATE:70:pilot_export_failed — %d/%d 航次 FAILED（D0 pilot 要求全导出成功；详见 manifest）', ...
        res.n_failed, res.n_voyages);
end
fprintf('[70] pilot 导出全部完成：%d voyages\n', res.n_voyages);

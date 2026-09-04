%% ========================================================================
%  10_build_modelE.m — 从零重建 HybridShip_ModelE.slx（可重复执行）
%  Model D = Model A 功率流 + 直流母线电气层（聚合单母线 750 V，下垂稳压）
%  说明：所有构建逻辑位于 +model/build_model_e.m（本脚本仅做环境准备）。
%  用法：在 model/simulink_modelD 目录下运行  eval(fileread('scripts/10_build_modelE.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % 兼容非文件方式执行（如 eval 场景）
end
addpath(fullfile(ROOT, 'scripts'));
% Phase 4-A2 D-13：Model E 全部入口禁止调用 model.ensure_canonical（Model D 自愈机制
% 会把 Model D canonical config/01-06/README_Model.md 复活进 Model E，破坏 _legacy_reference
% 隔离；2026-09-04 B 包 T-001 入口接线时移除本调用，偏差随 D-B 登记簿落账）

model.build_model_e(ROOT);

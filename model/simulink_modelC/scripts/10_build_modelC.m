%% ========================================================================
%  10_build_modelC.m — 从零重建 HybridShip_ModelC_DCBus.slx（可重复执行）
%  Model C = Model A 功率流 + 直流母线电气层（聚合单母线 750 V，下垂稳压）
%  说明：所有构建逻辑位于 +model/build_model_c.m（本脚本仅做环境准备）。
%  用法：在 model/simulink_modelC 目录下运行  eval(fileread('scripts/10_build_modelC.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % 兼容非文件方式执行（如 eval 场景）
end
addpath(fullfile(ROOT, 'scripts'));
model.ensure_canonical(ROOT);

model.build_model_c(ROOT);

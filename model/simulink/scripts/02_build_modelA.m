%% ========================================================================
%  02_build_modelA.m — 从零重建 HybridShip_ModelA_MVP_K3.slx（可重复执行）
%  说明：所有构建逻辑位于 +modela_k3/build_model.m（本脚本仅做环境准备）。
%  用法：在 ModelA_MVP_K3 目录下运行  run('scripts/02_build_modelA.m')
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % 兼容非文件方式执行（如 eval 场景）
end
addpath(fullfile(ROOT, 'scripts'));
modela_k3.ensure_canonical(ROOT);

modela_k3.build_model(ROOT);

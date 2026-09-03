%% ========================================================================
%  02_build_model.m — 从零重建 HybridShip_Model_MVP.slx（可重复执行）
%  说明：所有构建逻辑位于 +model/build_model.m（本脚本仅做环境准备）。
%  用法：在 Model_MVP 目录下运行  run('scripts/02_build_model.m')
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % 兼容非文件方式执行（如 eval 场景）
end
addpath(fullfile(ROOT, 'scripts'));
model.ensure_canonical(ROOT);

model.build_model(ROOT);

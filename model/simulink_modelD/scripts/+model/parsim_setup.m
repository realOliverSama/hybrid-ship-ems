function parsim_setup(ROOT)
% model.parsim_setup — parsim SetupFcn：在 worker 上准备仿真环境（T-201，FR-202）
%
%   与客户端 51/53 的预处理完全一致：
%     - cd 到工程根；addpath config/scripts（worker 继承客户端 path，此处兜底）；
%     - 把 Model_Parameters.m / Model_Scenarios.m 注入 **base workspace**
%       （模型块参数从 base 解析，与客户端 run 行为一致）；
%     - ensure_bsfc_params / ensure_nn_params：BSFC 覆盖表与 NN 权重（base 注入，
%       law=7 变体需要）；
%     - load_system 模型。
%
%   2026-08-26 Mac 端修复（handoff_mac_issues.md 问题 3）：
%     原实现 parsim(..., 'SetupFcn', @() eval(setupStr)) 在 worker 上不可靠——
%     eval 在匿名函数静态 workspace 执行，run() 创建的新变量（如
%     Model_SystemScope）报「尝试将 X 添加到静态工作区」；且变量进不了 base，
%     模型参数解析失败。改为显式函数 + evalin('base') 注入，与客户端语义一致。
%
%   输入: ROOT 工程根目录（model/simulink_modelD）
arguments
    ROOT (1,1) string
end
cd(ROOT);
addpath(fullfile(pwd, 'config'), fullfile(pwd, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(pwd, 'config', 'Model_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(pwd, 'config', 'Model_Scenarios.m')));
ecms2.ensure_bsfc_params(pwd);
ecms2.ensure_nn_params(pwd);
load_system(fullfile(pwd, 'model', 'HybridShip_ModelD_DCBus.slx'));
end

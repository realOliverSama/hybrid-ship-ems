function parsim_setup_v2(ROOT)
% ecms25.parsim_setup_v2 — T-310：parsim SetupFcn（v2 门控版）
% 与 model.parsim_setup 同构，NN 注入改为 ecms25.ensure_nn_params_v2
% （config/ecms_nn_weights_v2.mat，W3 [32,3] 分位数头）。
arguments
    ROOT (1,1) string
end
cd(ROOT);
addpath(fullfile(pwd, 'config'), fullfile(pwd, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(pwd, 'config', 'Model_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(pwd, 'config', 'Model_Scenarios.m')));
ecms2.ensure_bsfc_params(pwd);
ecms25.ensure_nn_params_v2(pwd);
load_system(fullfile(pwd, 'model', 'HybridShip_ModelD_DCBus.slx'));
end

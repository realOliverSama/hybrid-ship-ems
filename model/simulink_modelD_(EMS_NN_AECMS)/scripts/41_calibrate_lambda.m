%% ========================================================================
%  41_calibrate_lambda.m — λ / K_soc 标定入口（T-104/105，FR-105/106，design §3.4）
%  用法：在 model/simulink_modelD 目录下运行  eval(fileread('scripts/41_calibrate_lambda.m'))
%  全部逻辑在 +ecms2/calibrate_lambda.m（本脚本仅一行转发，保持数字脚本入口约定）。
%  产物：results/ecms_phase1/lambda_calibration.json（λ₀/K_soc/迭代轨迹/收敛标志）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));
ecms2.calibrate_lambda(ROOT);

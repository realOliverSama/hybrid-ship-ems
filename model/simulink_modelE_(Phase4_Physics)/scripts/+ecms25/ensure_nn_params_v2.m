function s = ensure_nn_params_v2(ROOT)
% ecms25.ensure_nn_params_v2 — T-310：law7-v2（三分位数 + 门控）权重注入
% 与 ecms2.ensure_nn_params 同构，但加载 config/ecms_nn_weights_v2.mat（63 产物，
% W3 [32,3] 分位数头），并校验形状。base 注入 ECMS_NN_*（供 chart 端口解析）。
if nargin < 1 || isempty(ROOT)
    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
end
matFile = fullfile(ROOT, 'config', 'ecms_nn_weights_v2.mat');
assert(exist(matFile, 'file') == 2, ...
    'ECMS25:NNV2', '缺少 %s（先运行 63_integrate_gate_v2.m）', matFile);
W = load(matFile);
fns = fieldnames(W);
for i = 1:numel(fns)
    if startsWith(fns{i}, 'ECMS_NN_') && isnumeric(W.(fns{i}))
        assignin('base', fns{i}, W.(fns{i}));
    end
end
assignin('base', 'ECMS_NN_WeightsOK', 1);
assert(isequal(size(W.ECMS_NN_W3), [32 3]), 'ECMS25:NNV2', 'W3 形状异常（须 32x3）');
s = struct('ok', 1, 'source', 'config/ecms_nn_weights_v2.mat', ...
    'y_norm_scale', W.ECMS_NN_YStd);
fprintf('[ecms25.ensure_nn_params_v2] law7-v2 权重已载入: W1 %dx%d | W2 %dx%d | W3 %dx%d | YStd=%.4f\n', ...
    size(W.ECMS_NN_W1,1), size(W.ECMS_NN_W1,2), size(W.ECMS_NN_W2,1), ...
    size(W.ECMS_NN_W2,2), size(W.ECMS_NN_W3,1), size(W.ECMS_NN_W3,2), W.ECMS_NN_YStd);
end

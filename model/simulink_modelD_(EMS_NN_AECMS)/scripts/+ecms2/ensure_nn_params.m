function s = ensure_nn_params(ROOT)
% ecms2.ensure_nn_params — law=7 NN-A-ECMS 权重注入（T-204，FR-208，design §5）
%
%  保证 base workspace 存在 ECMS_NN_*（MLP 权重 W1..b3、归一化常数、S20 标幺常数、
%  WeightsOK 标志），供 law=7 变体（EMS_NN_AECMS）MATLAB Function 块参数解析。
%  注入链：
%     1) config/ecms_nn_weights.mat 存在（52_integrate_law7.m 由 T-203 入选模型
%        selected_model_weights.json + datasets/fleet_norm.json 生成；mat 不入库）
%        -> 载入并覆盖 base（ECMS_NN_WeightsOK=1）；
%     2) 缺失 -> 保持 Model_Parameters.m 占位默认（全零权重 + WeightsOK=0），
%        打印告警：块内回退 ê=0（≡law=5 行为，FR-208 回退路径；模型编译不失败）。
%  输入: ROOT 工程根目录（model/simulink_modelD；缺省自动探测）
%  输出: s = struct(ok, source, y_norm_scale)——ok=1 权重就绪；ok=0 回退。
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    matFile = fullfile(ROOT, 'config', 'ecms_nn_weights.mat');
    if exist(matFile, 'file') == 2
        W = load(matFile);
        fns = fieldnames(W);
        for i = 1:numel(fns)
            if startsWith(fns{i}, 'ECMS_NN_') && isnumeric(W.(fns{i}))
                assignin('base', fns{i}, W.(fns{i}));
            end
        end
        assignin('base', 'ECMS_NN_WeightsOK', 1);
        s = struct('ok', 1, 'source', 'config/ecms_nn_weights.mat', ...
            'y_norm_scale', W.ECMS_NN_YStd);
        fprintf('[ecms2.ensure_nn_params] law=7 权重已载入 (source=%s): W1 %dx%d | W2 %dx%d | W3 %dx%d | YStd=%.4f | Ebar=%.4f kWh | Escale=%.4f kWh\n', ...
            s.source, size(W.ECMS_NN_W1,1), size(W.ECMS_NN_W1,2), ...
            size(W.ECMS_NN_W2,1), size(W.ECMS_NN_W2,2), ...
            size(W.ECMS_NN_W3,1), size(W.ECMS_NN_W3,2), ...
            W.ECMS_NN_YStd, W.ECMS_NN_Ebar_H_kWh, W.ECMS_NN_Escale_kWh);
    else
        % 占位默认（Model_Parameters.m 已定义）：强制 WeightsOK=0
        % -> 块内 ê=0（≡law=5 行为，FR-208 回退），模型编译不失败。
        assignin('base', 'ECMS_NN_WeightsOK', 0);
        s = struct('ok', 0, 'source', 'default (config/ecms_nn_weights.mat 缺失)', ...
            'y_norm_scale', 1);
        fprintf('[ecms2.ensure_nn_params] 警告：%s 缺失 -> law=7 以零权重回退（ê=0 ≡ law=5 行为，FR-208）；先运行 52_integrate_law7.m 生成权重。\n', ...
            matFile);
    end
end

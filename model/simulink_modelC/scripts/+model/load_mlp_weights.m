function loaded = load_mlp_weights(ROOT)
% model.load_mlp_weights — 将训练导出的 MLP 权重载入 base workspace
% 供 build_model / 06 / 07 共用：mlp_weights.mat 存在时以其 MLP_Window 与权重为准，
% 覆盖 Model_Parameters.m 中的占位零矩阵（尺寸必须与模型构建时一致）。
% 返回：是否成功加载。
    if nargin < 1
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    wf = fullfile(ROOT, 'python', 'export', 'mlp_weights.mat');
    loaded = false;
    if ~exist(wf, 'file')
        return;
    end
    Wm = load(wf);
    fns = fieldnames(Wm);
    for i = 1:numel(fns)
        if startsWith(fns{i}, 'MLP_') && isnumeric(Wm.(fns{i}))
            assignin('base', fns{i}, double(Wm.(fns{i})));
        end
    end
    assignin('base', 'MLP_WeightsLoaded', 1);
    loaded = true;
end

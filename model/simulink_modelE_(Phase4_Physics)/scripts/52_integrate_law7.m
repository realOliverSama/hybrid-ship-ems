%% ========================================================================
%  52_integrate_law7.m — law=7 NN-A-ECMS 权重集成（T-204，FR-208，design §5）
%  从 T-203 入选模型（results/ecms_phase2/models/selected_model_weights.json，
%  MLP 302→128→64→1） + 数据集归一化（datasets/fleet_norm.json，fit-on-train）
%  生成 config/ecms_nn_weights.mat（ECMS_NN_* 字段，供 law=7 变体参数解析）。
%  Ē_H / E_scale 取与 law=6 相同的 S20 常数（results/ecms_phase1/kS_calibration.json
%  oracle 字段——与 law=6 oracle_future_energy 产物一致；保证 C3/C4 消融唯一差异是
%  ê 来源，design §5）。mat 不入库（.gitignore），生成脚本入库。
%  用法：在 model/simulink_modelD 下运行  eval(fileread('scripts/52_integrate_law7.m'))
%  产物：config/ecms_nn_weights.mat + base workspace ECMS_NN_*（WeightsOK=1）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % 兼容非文件方式执行（如 eval 场景）
end
addpath(fullfile(ROOT, 'scripts'));

wJson  = fullfile(ROOT, 'results', 'ecms_phase2', 'models', 'selected_model_weights.json');
nJson  = fullfile(ROOT, 'datasets', 'fleet_norm.json');
kJson  = fullfile(ROOT, 'results', 'ecms_phase1', 'kS_calibration.json');
matOut = fullfile(ROOT, 'config', 'ecms_nn_weights.mat');

assert(exist(wJson, 'file') == 2, 'ECMS2:LAW7', '入选模型权重缺失: %s（先跑 T-203）', wJson);
assert(exist(nJson, 'file') == 2, 'ECMS2:LAW7', '归一化参数缺失: %s', nJson);
assert(exist(kJson, 'file') == 2, 'ECMS2:LAW7', 'S20 标幺常数缺失: %s（需 Phase 1 kS 标定产物）', kJson);

WJ = jsondecode(fileread(wJson));
NJ = jsondecode(fileread(nJson));
KJ = jsondecode(fileread(kJson));

%% ---- 权重解析（R2026a jsondecode：嵌套数组直接返回数值矩阵；与 train_predictor._np_mlp_forward 一致）----
assert(strcmp(WJ.model, 'MLP'), 'ECMS2:LAW7', '入选模型非 MLP（%s）——law=7 内联仅支持 MLP', WJ.model);
W1 = WJ.weights.W1;                  % [302,128]
b1 = WJ.weights.b1(:).';             % [1,128]
W2 = WJ.weights.W2;                  % [128,64]
b2 = WJ.weights.b2(:).';             % [1,64]
W3 = WJ.weights.W3;                  % [64,1]
b3 = WJ.weights.b3;                  % 标量
assert(isequal(size(W1), [302 128]) && numel(b1) == 128, 'ECMS2:LAW7', ...
    'W1/b1 尺寸不符: %s/%s（预期 302x128 / 128）', mat2str(size(W1)), mat2str(size(b1)));
assert(isequal(size(W2), [128 64]) && numel(b2) == 64, 'ECMS2:LAW7', ...
    'W2/b2 尺寸不符: %s/%s（预期 128x64 / 64）', mat2str(size(W2)), mat2str(size(b2)));
assert(isequal(size(W3), [64 1]) && numel(b3) == 1, 'ECMS2:LAW7', ...
    'W3/b3 尺寸不符: %s/%s（预期 64x1 / 1）', mat2str(size(W3)), mat2str(size(b3)));

%% ---- 归一化（302 维 per-feature z-score；与 fleet_norm.json 交叉核对）----
XMean = WJ.norm.X_mean(:);   % [302,1]
XStd  = WJ.norm.X_std(:);    % [302,1]
YMean = WJ.norm.y_mean;
YStd  = WJ.norm.y_std;
assert(numel(XMean) == 302 && numel(XStd) == 302, 'ECMS2:LAW7', ...
    'X_mean/X_std 长度 %d/%d（预期 302）', numel(XMean), numel(XStd));
assert(all(XStd > 0), 'ECMS2:LAW7', 'X_std 含非正元素（归一化失效）');
% 与 fleet_norm.json 一致性（同一生成链，应逐位一致）
dx = max(abs(XMean - NJ.X_mean(:)));
ds = max(abs(XStd - NJ.X_std(:)));
if dx > 1e-9 || ds > 1e-9
    fprintf('[52] 警告：weights JSON 与 fleet_norm.json 归一化常数偏差 max|ΔXmean|=%.3g max|ΔXstd|=%.3g（应一致，沿用 weights JSON 值）\n', dx, ds);
else
    fprintf('[52] 归一化常数与 fleet_norm.json 逐位一致（max|Δ|=0）\n');
end
dy = max(abs([YMean YStd] - [NJ.y_mean NJ.y_std]));
assert(dy < 1e-9, 'ECMS2:LAW7', 'y_mean/y_std 与 fleet_norm.json 不一致（max|Δ|=%.3g）', dy);

%% ---- S20 标幺常数（与 law=6 相同：kS_calibration.json oracle 字段）----
assert(isfield(KJ, 'oracle') && isfield(KJ.oracle, 'E_bar_H_kWh') && isfield(KJ.oracle, 'E_scale_kWh'), ...
    'ECMS2:LAW7', 'kS_calibration.json 缺少 oracle.E_bar_H_kWh/E_scale_kWh');
Ebar   = KJ.oracle.E_bar_H_kWh;
Escale = KJ.oracle.E_scale_kWh;
assert(Escale > 1e-9, 'ECMS2:LAW7', 'E_scale=%.6g 非法（S20 标幺常数）', Escale);
% 与 config 默认值核对（Model_Parameters.m 已内联同常数）
if exist(fullfile(ROOT, 'config', 'Model_Parameters.m'), 'file')
    run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
    if abs(evalin('base', 'ECMS_NN_Ebar_H_kWh') - Ebar) > 1e-9 || ...
       abs(evalin('base', 'ECMS_NN_Escale_kWh') - Escale) > 1e-9
        fprintf('[52] 警告：config 默认 Ebar/Escale 与 kS_calibration.json 不一致（已以 JSON 为准写入 mat）\n');
    end
end

%% ---- 写出 config/ecms_nn_weights.mat（ECMS_NN_* 字段；mat 不入库）----
S = struct( ...
    'ECMS_NN_W1', W1, 'ECMS_NN_b1', b1, ...
    'ECMS_NN_W2', W2, 'ECMS_NN_b2', b2, ...
    'ECMS_NN_W3', W3, 'ECMS_NN_b3', b3, ...
    'ECMS_NN_XMean', XMean, 'ECMS_NN_XStd', XStd, ...
    'ECMS_NN_YMean', YMean, 'ECMS_NN_YStd', YStd, ...
    'ECMS_NN_Ebar_H_kWh', Ebar, 'ECMS_NN_Escale_kWh', Escale, ...
    'ECMS_NN_WeightsOK', 1);
save(matOut, '-struct', 'S', '-v7');

%% ---- 注入 base workspace（供构建/运行）----
s = ecms2.ensure_nn_params(ROOT);
assert(s.ok == 1, 'ECMS2:LAW7', '权重注入失败');

fprintf('\n[52] law=7 权重集成完成：\n');
fprintf('  model      : %s（%s，hidden=%s，lr=%s）\n', WJ.model, WJ.run_id, ...
    mat2str(WJ.config.hidden), num2str(WJ.config.lr));
fprintf('  W1 %dx%d | W2 %dx%d | W3 %dx%d | 参数量=%d\n', ...
    size(W1,1), size(W1,2), size(W2,1), size(W2,2), size(W3,1), size(W3,2), ...
    numel(W1) + numel(b1) + numel(W2) + numel(b2) + numel(W3) + numel(b3));
fprintf('  YMean=%.6f YStd=%.6f | Ebar=%.6f kWh Escale=%.6f kWh\n', YMean, YStd, Ebar, Escale);
fprintf('  mat -> %s（ECMS_NN_WeightsOK=1）\n', matOut);

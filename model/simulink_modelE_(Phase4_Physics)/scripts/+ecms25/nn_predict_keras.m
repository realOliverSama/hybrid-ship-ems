function ehat = nn_predict_keras(x, model_id, XMean, XStd, YMean, YStd, Ebar, Escale)
% nn_predict_keras — T-310 扩展：外部 Keras 模型闭环预测桥（law7 chart 调用）
% x: [302,1] 归一化前的原始窗口（与内联 MLP 同口径）
% model_id: 'mlp_h64_32_lr3e-4_q' | 'tcn_f64' | 'nbeats_f256_lr1e-3' | 'ensemble'
% 返回 ehat（标幺，与内联一致：e_hat=(E_q50-Ebar)/Escale）
% 首调初始化 py 桥；之后缓存。单步开销 ~10-50ms。
persistent inited
if isempty(inited)
    % 相对路径推导：本文件 -> +ecms25 -> scripts -> 工作区根（禁绝对路径，AGENTS 纪律）
    thisDir = fileparts(mfilename('fullpath'));
    rootDir = fileparts(fileparts(thisDir));   % .../simulink_modelD_(EMS_NN_AECMS)
    pyExe = fullfile(rootDir, '..', 'simulink', 'python', '.venv_tf', 'Scripts', 'python.exe');
    pyenv('Version', pyExe);
    py.sys.path().insert(int32(0), fullfile(rootDir, 'python'));
    LM = py.importlib.import_module('load_models_mcp');
    inited = true;
end
x = double(x(:));
Xm = double(XMean(:)); Xs = double(XStd(:));
xn = (x - Xm) ./ Xs;                      % 与训练归一化同口径（fit-on-train v2）
xn = xn(:);                               % 列向量 302x1
persistent tmpdir
if isempty(tmpdir), tmpdir = tempname; mkdir(tmpdir); end
inM = fullfile(tmpdir, 'in.mat'); outM = fullfile(tmpdir, 'out.mat');
save(inM, 'xn');
LM = py.importlib.import_module('load_models_mcp');
if strcmp(model_id, 'ensemble')
    outA = fullfile(tmpdir, 'out_a.mat'); outB = fullfile(tmpdir, 'out_b.mat');
    LM.predict_file('mlp_h64_32_lr3e-4_q', inM, outA);
    LM.predict_file('tcn_f64', inM, outB);
    qA = double(load(outA, 'q50_norm').q50_norm);
    qB = double(load(outB, 'q50_norm').q50_norm);
    q50 = 0.7 * qA + 0.3 * qB;             % MLP×0.7 + TCN×0.3（T-307）
else
    LM.predict_file(model_id, inM, outM);
    q50 = double(load(outM, 'q50_norm').q50_norm);
end
E_q50 = q50 * YStd + YMean;               % kWh
ehat = (E_q50 - Ebar) / Escale;           % 标幺（与内联同语义）
end

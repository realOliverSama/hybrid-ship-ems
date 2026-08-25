function stmts = nn_forward_code()
% ecms2.nn_forward_code — law=7 内联 NN 前向语句生成器（T-204，FR-208，design §5）
%
%  返回一组 MATLAB 语句文本（不含 function 头尾），计算归一化输出 y_norm：
%      xnorm = (x − XMean) ./ XStd          （x: 302×1 原始特征列向量）
%      h1    = relu(xnorm.'·W1 + b1)        （W1: 302×128，b1: 1×128）
%      h2    = relu(h1·W2 + b2)             （W2: 128×64， b2: 1×64）
%      y_norm = h2·W3 + b3                  （W3: 64×1，   b3: 标量）
%  与 Python 推理（train_predictor.py `_np_mlp_forward`：x@W1+b1，行优先
%  Keras 权重 W1[302,128]）数学一致；位一致性由 +ecms2/test_law7.m T1
%  （Simulink 块 vs .venv_tf Python，100 组随机输入，归一化空间 |Δ|<1e-5）锁定。
%
%  列向量形式（2026-08-25 攻坚结论）：`W1.'*xnorm` 而非 `xnorm.'*W1`——
%  Stateflow EML 解析器对"列向量的 .' 转置参与矩阵乘"有缺陷（二分实证：
%  含 xnorm.' 的变体全部解析失败，W1.' 矩阵转置形式通过）；数学完全等价。
%
%  共享原因（2026-08 调试结论：EMChart 局部函数不可见图级参数，MLP 推理必须
%  内联于主函数）：
%    - build_model_c.m 的 CODE_ECMS_NN() 以 %s 内联嵌入本语句（产品块）；
%    - test_law7.m 的独立前向 harness 块嵌入同一文本（测试块）。
%  两处文本由同一生成器产出，保证逐位一致（无漂移风险）。
    stmts = sprintf([ ...
        'xnorm = (x - XMean(:)) ./ XStd(:);\n' ...
        'h1 = max(W1.'' * xnorm + b1(:), 0);\n' ...
        'h2 = max(W2.'' * h1 + b2(:), 0);\n' ...
        'y_norm = W3.'' * h2 + b3;\n']);
end

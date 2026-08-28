function e_hat = nn_predict_ehat(t, P_load, v_ship)
% ecms2.nn_predict_ehat — law=7 NN 前向（extrinsic 兜底，T-204，design §5 / Amendment v1.2）
%
%  普通 MATLAB 函数（非 Stateflow EML），由 law=7 MATLAB Function 块以
%  coder.extrinsic 调用（1 Hz 控制门控处）。函数内部持有 300 s P_load 环形缓冲
%  与归一化/前向所需持久状态；Stateflow EML 解析器对内联 MLP 前向失败
%  （2026-08-25 攻坚证据链，见 项目开发进展.md），extrinsic 下本函数在 MATLAB
%  解释器执行，不参与图内代码生成（Amendment v1.2 兜底裁决）。
%
%  persistent: W（权重结构体，load 后缓存）、buf(300×1)、k、vprev、lastT
%   - t < lastT（新仿真开始）-> 重置 buf/k/vprev（buf 填当前 P_load，k=1，vprev=v_ship）；
%   - W 为空 -> 惰性 load config/ecms_nn_weights.mat（相对工程根
%     model/simulink_modelD；文件缺失 -> 返回 0，下次调用仍尝试）；
%   - 每调用：buf(k)=P_load（isfinite 才写）；k=mod(k,300)+1；accel=v_ship−vprev；
%     vprev=v_ship；
%   - x = [buf 按时间序重组 300 维; v_ship; accel]（时间升序：xP(1)=P(t−299) 最旧 ..
%     xP(300)=P(t) 最新——与 datasets/fleet_norm.json feature_layout
%     '[P(t-299)..P(t)] 时间升序 + [v_ship(t)] + [accel(t)]' 一致）；
%   - 归一化（mat 内 XMean/XStd）；前向 W1..b3（列向量形式 W1.'*xnorm + b1(:)，
%     与 +ecms2/nn_forward_code.m 同式）；Ê = y_norm*YStd + YMean；
%     e_hat = (Ê − Ebar)/Escale（S20 标幺，与 law=6 同常数）；
%   - 任何输入非有限 -> e_hat=0（FR-208 回退，≡ law=5 行为）。
%
%  输入: t 仿真时间 [s]；P_load 负荷 [kW]；v_ship 船速 [m/s]
%  输出: e_hat 无量纲预测偏差（SOC_ref 通道标幺，design §5）
    persistent W buf k vprev lastT
    if isempty(lastT)
        lastT = -1e9;
        k = 1;
        buf = zeros(300, 1);
        buf(:) = P_load;
        vprev = v_ship;
    end
    if t < lastT
        % 新仿真开始：重置缓冲/写指针/速度差基线（首次预测为常数历史，与内联版 init 同语义）；
        % 同时清空 W 缓存 -> 下次调用重新惰性加载（支持测试中 mat 缺失/恢复场景：T2 rename 后
        % 重试 load 失败 -> ê=0，T3 恢复后重载真实权重）
        k = 1;
        buf(:) = P_load;
        vprev = v_ship;
        W = [];
    end
    lastT = t;

    e_hat = 0;
    % 非有限输入 -> 回退 ê=0（FR-208）
    if ~(isfinite(t) && isfinite(P_load) && isfinite(v_ship))
        return;
    end
    % 惰性加载权重（相对工程根 model/simulink_modelD；缺失 -> ê=0 回退）
    if isempty(W)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
        matFile = fullfile(ROOT, 'config', 'ecms_nn_weights.mat');
        if exist(matFile, 'file') == 2
            W = load(matFile);
        else
            % 权重 mat 缺失：回退 ê=0（≡ law=5 行为，FR-208；构建期告警见
            % ecms2.ensure_nn_params）
            return;
        end
    end
    % 1 Hz 缓冲更新（NaN 不污染）
    if isfinite(P_load)
        buf(k) = P_load;
    end
    k = mod(k, 300) + 1;
    accel = v_ship - vprev;
    vprev = v_ship;
    if ~isfinite(accel)
        return;
    end
    % 时间升序重组：buf(k) 为最旧样本（300 步前写入），buf(k-1) 为最新
    xP = [buf(k:300); buf(1:k-1)];   % xP(1)=P(t-299) .. xP(300)=P(t)
    x = [xP; v_ship; accel];         % 302×1（fleet_norm.json feature_layout）
    if ~all(isfinite(x))
        return;
    end
    xnorm = (x - W.ECMS_NN_XMean(:)) ./ W.ECMS_NN_XStd(:);
    h1 = max(W.ECMS_NN_W1.' * xnorm + W.ECMS_NN_b1(:), 0);
    h2 = max(W.ECMS_NN_W2.' * h1 + W.ECMS_NN_b2(:), 0);
    y_norm = W.ECMS_NN_W3.' * h2 + W.ECMS_NN_b3;
    Ehat_kWh = y_norm * W.ECMS_NN_YStd + W.ECMS_NN_YMean;
    e_hat = (Ehat_kWh - W.ECMS_NN_Ebar_H_kWh) / W.ECMS_NN_Escale_kWh;
end

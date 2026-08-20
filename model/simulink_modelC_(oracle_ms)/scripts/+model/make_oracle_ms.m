function D_ms = make_oracle_ms(MDL, SCN, outMatPath, dsIn)
% model.make_oracle_ms — 生成多步负荷 oracle 预测表（Phase 1，FR-003/004）
% 输入:
%   MDL        模型名（已加载，如 'HybridShip_ModelCms_DCBus'）
%   SCN        工况结构体（config/Model_Scenarios.m）
%   outMatPath 输出 .mat 路径（可选；省略则仅返回）
%   dsIn       可选：直接提供 C0 的 extract_logsout 结果（复用 E0 仿真，
%              避免重复跑 C0；省略则内部自跑一次 C0）
% 输出:
%   D_ms       N×13 = [t, P_5, P_10, ..., P_60]
%              t      航次内绝对时间 [s]（1 Hz 网格，0,1,2,...）
%              P_h    未来 h 秒的外生需求负荷 [kW]，h ∈ {5,10,...,60}（12 节点）
% 因果性硬约束（FR-003/T-003，最高优先级）:
%   oracle 目标 = P_prop_demand_kW + P_hotel_kW（外生需求负荷）
%   —— 即 08_Undervolt_Derate 上游、EMS 与 DC-bus 动态无法影响的负荷请求。
%   **禁止**使用 P_load_total_kW（= P_prop_actual + hotel，降额后实际负荷，
%   受 Vdc 影响，会在弱机/慢机区域产生闭环因果污染）。
%   证据：results/ms_phase1/oracle_causality/causality_derate_evidence.png
%         （弱机 560kW/20kW/s/300kW 电池下降额窗口 [4.7, 90.4] s 内
%          demand+hotel 恒为 800 kW 不受 Vdc 影响，load_total 跌至 425 kW）
% 方法: C0 (EMS_CTRL_LAW=0) 全航程仿真 → extract_logsout → 1 Hz 重采样
%       （分段常数：取每 1 s bin 内最后值，与 resample_1s 语义一致）
%       → 前向节点线性插值（t+h 落在重采样网格上，直接取网格值）
% 节点: 固定 5 s 分辨率 × 12 节点 = [5,10,...,60] s（固定最大宽度，
%       模型内经 valid_node_count = MSFF_Horizon_s/5 消费前 H/5 个，FR-004）
% 末端处理: t+h 超出航程末端时取末端值（端点保持），全程有值不填 NaN，
%           与闭环"每个时刻都有预测"需求一致（沿袭 make_oracle_pred 惯例）。
    if nargin >= 4 && ~isempty(dsIn)
        ds = dsIn;
        fprintf('[make_oracle_ms] %s (复用 C0 ds) ...\n', SCN.name);
    else
        stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1200;
        fprintf('[make_oracle_ms] %s (C0, stop=%.0f s) ...\n', SCN.name, stopTimeLimit);
        out = model.run_scenario_sim(MDL, SCN, 0, stopTimeLimit);
        ds = model.extract_logsout(out);
    end

    % 必须存在的信号（否则说明取点链断裂）
    assert(isfield(ds, 'P_prop_demand_kW'), 'logsout 缺 P_prop_demand_kW');
    assert(isfield(ds, 'P_hotel_kW'), 'logsout 缺 P_hotel_kW');

    t0 = ds.t;
    t = (0:1:floor(t0(end)))';
    % 1 Hz 重采样（分段常数：每 bin 内最后值，与 resample_1s 语义一致）
    dem = ds.P_prop_demand_kW;
    hot = ds.P_hotel_kW;
    idx = zeros(numel(t), 1);
    for k = 1:numel(t)
        m = find(t0 <= t(k) + 1e-9, 1, 'last');
        if isempty(m), m = 1; end
        idx(k) = m;
    end
    dem1 = dem(idx);
    hot1 = hot(idx);
    P_exo = dem1 + hot1;   % 外生需求负荷（oracle 目标）

    nodes = 5:5:60;                       % 12 节点 [5..60] s
    n = numel(t);
    P = zeros(n, numel(nodes));
    t_end = t(end);
    for i = 1:numel(nodes)
        h = nodes(i);
        th = t + h;
        % 端点保持：t+h 超出末端 → 取末端值
        tt = min(th, t_end);
        % 网格对齐：t 与 tt 均为整数秒（1 Hz 网格），直接索引
        P(:, i) = P_exo(min(round(tt) + 1, n));
    end
    D_ms = [t, P];

    if nargin >= 3 && ~isempty(outMatPath)
        p = fileparts(outMatPath);
        if ~isempty(p) && ~exist(p, 'dir'), mkdir(p); end
        save(outMatPath, 'D_ms');
    end
    fprintf('[make_oracle_ms] %s: N=%d, t_end=%.0f s, 12 节点全生成\n', ...
        SCN.name, n, t_end);
    fprintf('  P_exo 范围: [%.1f, %.1f] kW\n', min(P_exo), max(P_exo));
end

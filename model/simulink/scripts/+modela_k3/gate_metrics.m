function m = gate_metrics(ds)
% modela_k3.gate_metrics — 计算单次烟雾测试的门禁指标
% 输入: ds = extract_logsout 输出（1 s 采样 struct）
% 输出: m  = 指标 struct（供 gate_compare 与基线持久化使用）
% 说明: v_ripple 为巡航段速度波动（后半程去均值 RMSE），不是对 v_cmd 的跟踪误差
    dt = [diff(ds.t); 0.01];
    m.fuel_kg = ds.fuel_total_kg(end);
    m.E_gen_kWh = sum(ds.P_gen_actual_kW .* dt) / 3600;
    m.mean_load_kW = mean(ds.P_load_total_kW);
    nlast = max(round(numel(ds.t) * 0.1), 1);
    m.v_ss = mean(ds.v_ship_mps(end - nlast + 1:end));
    m.soc_end = ds.SOC(end);
    m.soc_min = min(ds.SOC);
    % 巡航段速度波动（取航程后半段，避开起步暂态与工况切换）：
    % 用于检测振荡/失稳回归；起步与功率受限下垂由 cruise_reached 与 v_ss 回归负责
    nhalf = max(ceil(numel(ds.t) / 2), 1);
    vc = ds.v_ship_mps(nhalf:end);
    m.v_ripple = sqrt(mean((vc - mean(vc)) .^ 2));
    m.t_end = ds.t(end);
end

function m = dcbus_metrics(ds)
% model.dcbus_metrics — Model D 直流母线指标卡（FR-007）
% 输入: ds（extract_logsout 输出，含 V_dc_V/P_gen_actual_kW/P_bat_actual_kW/...）
% 输出: 指标 struct（外部安全 / 内部安全 / 节能 三组，CCS 2026 判据对照）
    V_dc_nom = evalin('base', 'V_dc_nom');
    CCS_S = evalin('base', 'CCS_Vdc_Steady_Band');
    CCS_T = evalin('base', 'CCS_Vdc_Transient_Band');
    t_rec_max = evalin('base', 'CCS_t_recovery_max');
    BatCap = evalin('base', 'Bat_Cap_kWh');
    k_eq = evalin('base', 'ASSUMED_k_eq_FEC');
    GenPmax = evalin('base', 'Gen_P_max');
    SOCmin = evalin('base', 'ASSUMED_SOC_min');
    SOCderate = evalin('base', 'ASSUMED_SOC_DisDerate_Start');

    t = ds.t; V = ds.V_dc_V;
    dt = [diff(t); 0.01];

    % ---- 内部安全：电压 ----
    m.V_min = min(V); m.V_max = max(V);
    m.dU_max = V_dc_nom - m.V_min;                    % 最大跌落深度 [V]
    m.dU_max_rel = m.dU_max / V_dc_nom;               % 相对额定
    m.t_steady_viol_s = sum(dt(V < CCS_S(1) | V > CCS_S(2)));   % 稳态带越限时长
    m.t_transient_viol_s = sum(dt(V < CCS_T(1) | V > CCS_T(2))); % 瞬态带越限时长
    % 恢复时间：全局最低电压点之后首次回到稳态带内的时间差
    [~, imin] = min(V);
    irec = find(V(imin:end) >= CCS_S(1) & V(imin:end) <= CCS_S(2), 1, 'first');
    if isempty(irec)
        m.t_recovery_s = inf;
    else
        m.t_recovery_s = t(imin + irec - 1) - t(imin);
    end
    m.t_recovery_ok = m.t_recovery_s <= t_rec_max;

    % ---- 内部安全：储备 ----
    m.P_margin_min_kW = min(GenPmax - ds.P_gen_actual_kW + ds.P_bat_dis_avail_kW - ds.P_bat_actual_kW);
    margin = GenPmax - ds.P_gen_actual_kW + max(ds.P_bat_dis_avail_kW - ds.P_bat_actual_kW, 0);
    m.T_reserve_low_pct = 100 * mean(margin < 100);
    m.SOC_min = min(ds.SOC); m.SOC_end = ds.SOC(end);
    m.T_soc_low_pct = 100 * mean(ds.SOC < SOCderate);
    m.P_bat_max_kW = max(ds.P_bat_actual_kW);

    % ---- 外部安全 ----
    m.v_rmse = sqrt(mean((ds.v_ship_mps - ds.v_cmd_mps).^2));
    m.v_drop_max = max(ds.v_cmd_mps - ds.v_ship_mps);
    if isfield(ds, 'P_unserved_kW')
        m.E_unserved_kWh = sum(ds.P_unserved_kW .* dt) / 3600;
        m.P_unserved_max_kW = max(ds.P_unserved_kW);
    else
        m.E_unserved_kWh = 0; m.P_unserved_max_kW = 0;
    end

    % ---- 节能 ----
    m.fuel_kg = ds.fuel_total_kg(end);
    m.E_gen_kWh = sum(ds.P_gen_actual_kW .* dt) / 3600;
    m.E_bat_dis_kWh = sum(max(ds.P_bat_actual_kW, 0) .* dt) / 3600;
    dSOC = ds.SOC(end) - 0.60;   % 相对 SOC 参考值
    m.FEC_kg = m.fuel_kg - dSOC * BatCap * k_eq;  % SOC 盈余折算扣除（多存的电折算回燃油）
    m.SOCmin_hard = SOCmin;
end

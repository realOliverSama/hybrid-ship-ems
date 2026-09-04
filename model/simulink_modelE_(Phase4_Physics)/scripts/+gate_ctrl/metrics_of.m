function m = metrics_of(ds, PMS_stop_threshold_kW)
% gate_ctrl.metrics_of — FR-454 指标体系后处理（T-009）
%   输入 get_ds 数据集 + 停机阈值（滞回下界，双机低负荷 dwell 判据）。
%   输出机器可读指标卡（fuel/starts/online/dwell/SOC/Vdc/battery/unserved/override）。
    dt = [diff(ds.t); 0];
    m = struct();
    m.duration_s = ds.t(end);

    % 燃油（10 号 per-genset + total）
    if isfield(ds, 'fuel_total_kg'), m.fuel_total_kg = ds.fuel_total_kg(end); end
    if isfield(ds, 'fuel_1_kg'), m.fuel_1_kg = ds.fuel_1_kg(end); end
    if isfield(ds, 'fuel_2_kg'), m.fuel_2_kg = ds.fuel_2_kg(end); end

    % 在线时长（design §6.4：用 Plant 实际 state_DGi==ON，非 commitment_state）
    m.DG1_online_time_s = sum(dt .* (ds.state_DG1 == 3));
    m.DG2_online_time_s = sum(dt .* (ds.state_DG2 == 3));
    m.twoDG_low_load_dwell_s = sum(dt .* ((ds.state_DG1 == 3) & (ds.state_DG2 == 3) & ...
        (ds.P_commit_kW < PMS_stop_threshold_kW)));

    % SOC / Vdc
    m.SOC_min = min(ds.SOC); m.SOC_end = ds.SOC(end);
    m.Vdc_min = min(ds.V_dc_V);
    m.Vdc_viol_600_s = sum(dt .* (ds.V_dc_V < 600));

    % battery throughput（P_bat_bus>0=放电，<0=充电；kWh 充/放分列）
    if isfield(ds, 'P_bat_actual_kW')
        P = ds.P_bat_actual_kW;
        m.batt_discharge_kWh = sum(dt .* max(P, 0)) / 1000;
        m.batt_charge_kWh    = sum(dt .* max(-P, 0)) / 1000;
    end

    % unserved / surplus 能量（指令错配诊断积分，kWh）
    if isfield(ds, 'P_gen_unserved_kW')
        m.unserved_energy_kWh = sum(dt .* max(ds.P_gen_unserved_kW, 0)) / 1000;
    end
    if isfield(ds, 'P_gen_surplus_kW')
        m.surplus_energy_kWh = sum(dt .* max(ds.P_gen_surplus_kW, 0)) / 1000;
    end

    % override 事件计数（16 号请求 + fault 锁存）
    m.safety_force_start_events = events_of(max(ds.safety_force_start_req, [], 2));
    m.safety_force_stop_events  = events_of(max(ds.safety_force_stop_req, [], 2));
    m.fault_latch_final = ds.fault_latch(end, :);
    if isfield(ds, 'baseline_fallback_active')
        m.baseline_fallback_active_end = ds.baseline_fallback_active(end);
    end

    % 启动计数交叉证据（Plant start_count）
    if isfield(ds, 'start_count_DG2'), m.DG2_plant_starts = ds.start_count_DG2(end); end
    if isfield(ds, 'start_count_DG1'), m.DG1_plant_starts = ds.start_count_DG1(end); end
end

function cnt = events_of(levelSig)
% 电平信号上升沿事件数
    s = levelSig(:) > 0.5;
    cnt = sum([false; diff(s) == 1]);
end
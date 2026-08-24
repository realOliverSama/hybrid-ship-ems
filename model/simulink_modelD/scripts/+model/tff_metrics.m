function m = tff_metrics(ds)
% model.tff_metrics — 定时前馈（TFF）航次指标卡（T-005）
% 输入: ds（model.extract_logsout 输出 struct）
% 输出: m 指标 struct，字段：
%   fuel_kg             累计燃油 [kg]
%   Vdc_min / Vdc_max   母线电压极值 [V]
%   SOC_end / SOC_min   SOC 末值 / 最小值
%   E_bat_dis_kWh       电池放电能量 [kWh]
%   P_bat_rms_kW        电池功率 RMS [kW]
%   P_bat_peak_abs_kW   电池功率峰值绝对值 [kW]
%   t_sat_s             发电机爬坡饱和时间 [s]（|限幅后参考 − 爬坡输出| > 1 kW 累计）
%   E_unserved_kWh      未服务能量 [kWh]
%   v_rmse_mps          船速跟随 RMSE [m/s]
%   P_ff_activated_s    预测前馈激活时长 [s]（P_ff_kW > 0.5 kW；字段缺失则 NaN）
% 缺失信号约定: P_gen_rampout_kW / P_ff_kW 不存在时对应字段为 NaN 并 warning
% （P_ff_kW 仅在 EMS_ScheduledFF 活动时记录；C0/C1 下为 NaN）。
    dt = [diff(ds.t); 0.01];

    m.fuel_kg = ds.fuel_total_kg(end);
    m.Vdc_min = min(ds.V_dc_V);
    m.Vdc_max = max(ds.V_dc_V);
    m.SOC_end = ds.SOC(end);
    m.SOC_min = min(ds.SOC);
    m.E_bat_dis_kWh = sum(max(ds.P_bat_actual_kW, 0) .* dt) / 3600;
    m.P_bat_rms_kW = sqrt(mean(ds.P_bat_actual_kW .^ 2));
    m.P_bat_peak_abs_kW = max(abs(ds.P_bat_actual_kW));

    % ---- 爬坡饱和时间：|限幅后参考 − 爬坡输出| > 1 kW ----
    if isfield(ds, 'P_gen_rampout_kW')
        GenPmax = evalin('base', 'Gen_P_max');
        MinStable = evalin('base', 'ASSUMED_Gen_MinStablePower');
        ref = min(max(ds.P_gen_ref_kW, MinStable), GenPmax);   % 机组入口限幅后参考
        m.t_sat_s = sum(dt(abs(ref - ds.P_gen_rampout_kW) > 1));
    else
        m.t_sat_s = NaN;
        warning('tff_metrics: ds 缺少 P_gen_rampout_kW，t_sat_s=NaN');
    end

    % ---- 未服务能量 / 船速 ----
    if isfield(ds, 'P_unserved_kW')
        m.E_unserved_kWh = sum(ds.P_unserved_kW .* dt) / 3600;
    else
        m.E_unserved_kWh = 0;
    end
    m.v_rmse_mps = sqrt(mean((ds.v_ship_mps - ds.v_cmd_mps) .^ 2));

    % ---- 前馈激活时长（仅 ScheduledFF 变体记录）----
    if isfield(ds, 'P_ff_kW')
        m.P_ff_activated_s = sum(dt(ds.P_ff_kW > 0.5));
    else
        m.P_ff_activated_s = NaN;
        warning('tff_metrics: ds 缺少 P_ff_kW，P_ff_activated_s=NaN');
    end
end

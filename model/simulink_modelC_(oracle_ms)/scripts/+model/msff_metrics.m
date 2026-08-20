function m = msff_metrics(ds, varargin)
% model.msff_metrics — Phase 1 多步前馈统一航次指标卡（T-006, FR-012）
% 输入: ds（model.extract_logsout 输出 struct，含 t/V_dc_V/P_bat_actual_kW/...）
%   可选名值对:
%     'UV_thresh'      真实欠压降额线 [V]，默认 675（FR-014 非劣化约束⑤判定线）
%     'Stress_thresh'  母线压力区阈值 [V]，默认 700（动态裕度分析，与欠压严格分开）
%     'FF_thresh'      前馈有效阈值 [kW]，默认 0.5（P_ff_applied 有效判定）
% 输出: m 指标 struct，字段：
%   --- 一级：动态安全与电池负担 ---
%   Vdc_min / Vdc_max         母线电压极值 [V]
%   N_UV / T_UV               欠压事件次数 / 累计时长 [s]（V < UV_thresh，真实降额线）
%   N_stress / T_stress       母线压力区事件次数 / 累计时长 [s]（V < Stress_thresh）
%   P_bat_peak_abs_kW         电池功率峰值绝对值 [kW]
%   P_bat_rms_kW              电池功率 RMS [kW]
%   E_bat_dis_kWh             电池放电能量 [kWh]
%   E_bat_throughput_kWh      电池吞吐量 = ∫|P_bat|dt/3600 [kWh]
%   --- 二级：系统运行效果 ---
%   SOC_end / SOC_min         SOC 末值 / 最小值
%   t_sat_s                   发电机爬坡饱和时间 [s]（|限幅后参考−爬坡输出|>1 kW 累计，同 tff_metrics）
%   E_unserved_kWh            未服务能量 [kWh]（ds 无 P_unserved_kW 则为 0）
%   v_rmse_mps                船速跟随 RMSE [m/s]
%   --- 约束：经济性 ---
%   fuel_kg                   航次燃油 [kg]
%   --- MinFF 诊断 ---
%   ff_active_s               前馈施加累计时间 [s]（P_ff_applied > FF_thresh）
%   ff_energy_kJ              前馈能量 = ∫P_ff_applied dt [kJ]
%   R_max_kW                  功率缺口序列（ds 有则整列保留，否则空）
% 缺失信号约定: P_ff_applied_kW / R_max_kW 不存在时对应字段 NaN/空并 warning
%   （旧 phase1 数据集为 law=0/1/2 运行，无 MinFF 信号，属预期）。
% 兼容性: 与 tff_metrics.m 同名的字段采用完全相同公式，保证 1e-6 级回放一致。

    % ---- 参数解析 ----
    p = inputParser;
    addParameter(p, 'UV_thresh', 675, @(x) isnumeric(x) && isscalar(x));
    addParameter(p, 'Stress_thresh', 700, @(x) isnumeric(x) && isscalar(x));
    addParameter(p, 'FF_thresh', 0.5, @(x) isnumeric(x) && isscalar(x));
    parse(p, varargin{:});
    UVth = p.Results.UV_thresh;
    STth = p.Results.Stress_thresh;
    FFth = p.Results.FF_thresh;

    dt = [diff(ds.t); 0.01];
    V = ds.V_dc_V;

    % ---- 一级：动态安全与电池负担 ----
    m.Vdc_min = min(V);
    m.Vdc_max = max(V);
    [m.N_UV, m.T_UV] = thresholdEvents(V, dt, UVth);
    [m.N_stress, m.T_stress] = thresholdEvents(V, dt, STth);
    m.P_bat_peak_abs_kW = max(abs(ds.P_bat_actual_kW));
    m.P_bat_rms_kW = sqrt(mean(ds.P_bat_actual_kW .^ 2));
    m.E_bat_dis_kWh = sum(max(ds.P_bat_actual_kW, 0) .* dt) / 3600;
    m.E_bat_throughput_kWh = sum(abs(ds.P_bat_actual_kW) .* dt) / 3600;

    % ---- 二级：系统运行效果 ----
    m.SOC_end = ds.SOC(end);
    m.SOC_min = min(ds.SOC);
    if isfield(ds, 'P_gen_rampout_kW')
        GenPmax = evalin('base', 'Gen_P_max');
        MinStable = evalin('base', 'ASSUMED_Gen_MinStablePower');
        ref = min(max(ds.P_gen_ref_kW, MinStable), GenPmax);   % 机组入口限幅后参考
        m.t_sat_s = sum(dt(abs(ref - ds.P_gen_rampout_kW) > 1));
    else
        m.t_sat_s = NaN;
        warning('msff_metrics: ds 缺少 P_gen_rampout_kW，t_sat_s=NaN');
    end
    if isfield(ds, 'P_unserved_kW')
        m.E_unserved_kWh = sum(ds.P_unserved_kW .* dt) / 3600;
    else
        m.E_unserved_kWh = 0;
    end
    m.v_rmse_mps = sqrt(mean((ds.v_ship_mps - ds.v_cmd_mps) .^ 2));

    % ---- 约束：经济性 ----
    m.fuel_kg = ds.fuel_total_kg(end);

    % ---- MinFF 诊断 ----
    if isfield(ds, 'P_ff_applied_kW')
        m.ff_active_s = sum(dt(ds.P_ff_applied_kW > FFth));
        m.ff_energy_kJ = sum(ds.P_ff_applied_kW .* dt);   % kW·s = kJ
    else
        m.ff_active_s = NaN;
        m.ff_energy_kJ = NaN;
        warning('msff_metrics: ds 缺少 P_ff_applied_kW（非 MinFF 运行），ff 字段=NaN');
    end
    if isfield(ds, 'R_max_kW')
        m.R_max_kW = ds.R_max_kW;   % 整列保留（不写入 CSV 行）
    else
        m.R_max_kW = [];
    end
end

function [N, T] = thresholdEvents(V, dt, thr)
% 阈值穿越事件计数与累计时长
% N: 由阈值以下→以上（或起点即在阈值以下）进入的段数
% T: V < thr 的累计时长 [s]
    below = V < thr;
    T = sum(dt .* below);
    N = sum(diff([0; below]) == 1);
end

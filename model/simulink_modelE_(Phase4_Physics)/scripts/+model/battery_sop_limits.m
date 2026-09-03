function [P_dis_bus_kW, P_chg_bus_kW, flags] = battery_sop_limits(...
    SOC, OCV_V, V1_V, R0_dis_ohm, R0_chg_ohm, ...
    Bat_I_limit_surrogate_A, Bat_V_min_V, Bat_V_max_V, eta_dcdc, ...
    Bat_P_conv_bus_max_kW, SOC_min, SOC_max, SOC_DisDerate_Start, SOC_ChgDerate_Start)
% BATTERY_SOP_LIMITS 电池 SOP 四约束可用功率（bus-side 口径，FR-424 + OQ-9；链 C T-009）
%   P_SOP = min(P_current, P_voltage, P_SOC, P_converter)——四约束取最小；
%   P_converter（1050 kW）为 DC/DC 母线侧图纸约束，是四约束之一的普通上界，
%   不凌驾电压/电流安全边界之上（OQ-9 冻结：1050 kW = bus-side）。
%
%   单位：V [V]、R [ohm]、I [A]；功率入参/出参均为 [kW]（bus-side）。
%   符号约定：放电为正方向——P_dis_bus_kW >= 0 表示可向母线放电的最大功率，
%   P_chg_bus_kW >= 0 表示可从母线吸收的最大功率。
%
%   【V_oc_est 开路估计语义（OQ-9）】SOP 是限制值而非工作点解：由状态量
%   (SOC, V1) 用开路估计 V_oc_est = OCV_V - V1_V 计算可功极限，无代数依赖、
%   无迭代（NFR-409 HIL-friendly）。known-limitation：未用工作点端电压 V_t，
%   偏差由 |V1| 决定（1RC 稳态 |V1| = |I|*R1 < ~15 V，对可功影响 < 3%），
%   见 chain_c_integration_notes.md。
%
%   【P_SOC 降额族】满刻度 = Bat_P_conv_bus_max_kW（沿用现有降额族口径）；
%   降额带外置为 Bat_P_conv_bus_max_kW（与约束 4 同值非绑定，min 结果与
%   Inf 等价，避免 Inf 进入 block 代码与测试 isfinite 扫描）。
%
%   【内联纪律】本函数体（V_oc_est 至 flags 赋值）被逐字内联进 BatteryECM
%   block 源码（+model/code_battery_ecm_e.m，注释"内联自 +model/battery_sop_limits.m，
%   保持一致"）；修改此处必须同步内联副本；test_battery_ecm.m 含一致性对照断言。
%
%   参数 provenance（ModelE_Physics_Parameters.m §6，冻结事实源，禁止重议）：
%   Bat_I_limit_surrogate_A = 1823 A（DERIVED surrogate = 1050 kW / 576 V，
%   非独立 BMS 电流额定）；eta_dcdc = 0.97；V_min/V_max = 360/657 V；
%   SOC_min/max = 0.10/0.95；DisDerate_Start/ChgDerate_Start = 0.25/0.90。

% ---- V_oc_est：状态量开路估计（OCV 与 V1 均为状态量，无代数依赖；OQ-9 语义）----
V_oc_est = OCV_V - V1_V;

% ---- 约束 1：P_current（电流 surrogate 上限，bus-side，design §4.6 原式）----
P_current_dis = Bat_I_limit_surrogate_A * V_oc_est / eta_dcdc / 1000;   % kW
P_current_chg = Bat_I_limit_surrogate_A * V_oc_est * eta_dcdc / 1000;   % kW

% ---- 约束 2：P_voltage（端子电压边界反解电流上限，bus-side）----
% 放电：V_t >= V_min 反解 I 上限；端子电压取 max(V_t, V_min) 保证不跌破 V_min
I_vmax = max(V_oc_est - Bat_V_min_V, 0) / max(R0_dis_ohm, 1e-12);
V_t_dis_lim = max(V_oc_est - I_vmax * R0_dis_ohm, Bat_V_min_V);
P_voltage_dis = I_vmax * V_t_dis_lim / eta_dcdc / 1000;                 % kW
% 充电：V_t <= V_max 反解电流上限（I_vmin <= 0）；V_oc_est >= V_max 时 I_vmin=0
% （P_chg=0：开路估计已到上限，不再允许充电）
I_vmin = -max(Bat_V_max_V - V_oc_est, 0) / max(R0_chg_ohm, 1e-12);      % <= 0
V_t_chg_lim = min(V_oc_est - I_vmin * R0_chg_ohm, Bat_V_max_V);
P_voltage_chg = max(abs(I_vmin) * V_t_chg_lim * eta_dcdc / 1000, 0);    % kW，clamp >= 0

% ---- 约束 3：P_SOC（沿用现有降额族；满刻度 = Bat_P_conv_bus_max_kW）----
% 放电：SOC <= DisDerate_Start(0.25) 线性降额 -> SOC_min(0.10) 归零
if SOC <= SOC_min
    P_soc_dis = 0;
    soc_derate_dis = 1;
elseif SOC <= SOC_DisDerate_Start
    P_soc_dis = Bat_P_conv_bus_max_kW * (SOC - SOC_min) / ...
        (SOC_DisDerate_Start - SOC_min);
    soc_derate_dis = 1;
else
    P_soc_dis = Bat_P_conv_bus_max_kW;    % 降额带外：非绑定（与约束 4 同值）
    soc_derate_dis = 0;
end
% 充电：SOC >= ChgDerate_Start(0.90) 线性降额 -> SOC_max(0.95) 归零
if SOC >= SOC_max
    P_soc_chg = 0;
    soc_derate_chg = 1;
elseif SOC >= SOC_ChgDerate_Start
    P_soc_chg = Bat_P_conv_bus_max_kW * (SOC_max - SOC) / ...
        (SOC_max - SOC_ChgDerate_Start);
    soc_derate_chg = 1;
else
    P_soc_chg = Bat_P_conv_bus_max_kW;    % 降额带外：非绑定（与约束 4 同值）
    soc_derate_chg = 0;
end

% ---- 约束 4：P_converter（DC/DC 母线侧图纸额定，bus-side；OQ-9 冻结）----
P_converter = Bat_P_conv_bus_max_kW;

% ---- 四约束取最小（可用功率非负：防退化状态产生负可用功率）----
P_dis_bus_kW = max(min(min(P_current_dis, P_voltage_dis), ...
    min(P_soc_dis, P_converter)), 0);
P_chg_bus_kW = max(min(min(P_current_chg, P_voltage_chg), ...
    min(P_soc_chg, P_converter)), 0);

flags = struct('soc_derate_dis', soc_derate_dis, 'soc_derate_chg', soc_derate_chg);
end
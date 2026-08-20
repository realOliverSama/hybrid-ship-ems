%% ========================================================================
%  Model_Parameters.m — HybridShip_Model_MVP 统一参数表
%  说明：本脚本只定义参数，无任何副作用；模型编译前必须先运行本脚本。
%  参数分类：
%    [DRAWING] 来自系统图/硬件选型（图纸额定值）
%    [LEGACY]  来自原模型 HybridShip0730 及其参数脚本 ship_parameters0616.m
%    [ASSUMED] 暂时缺少可靠来源的假设值（ASSUMED_ 前缀）
%  来源与置信度明细见 reports/parameter_audit.csv（由 02_build_model.m 生成）。
%
%  v2.0 变更（双推进等效 + 水动力重新标定）：实船左右舷双推进，图纸“单支路额定”
%    为每支路值（原模型 DC_Grid_Node_Model 双支路接口为佐证）；对称工况聚合等效。
%    修复原标定不自洽（8 kn 需 ~380 kW > 单侧 350 kW）及派生遗漏 Elec_Eff 问题。
%  v2.1 变更（船型定位：长江危化品应急处置船/公务船）：
%    服务航速 8 kn -> 11 kn。8 kn 对公务船偏慢；按功率预算与文献 R/W 区间交叉校核
%    后取 11 kn（Fn∇≈0.57，R/W≈0.0058 处于瘦线型内河公务船合理带）。
%    除发动机(500 kW/支路)、电池(600 kWh/支路)、推进电机(350 kW/支路)为图纸确定值外，
%    其余水动力/质量参数均为估计值（ASSUMED/低置信度，需船模试验或 CFD 确认）。
% ========================================================================

%% ------------------------------------------------------------------------
%  1. 模型范围 [Model 配置]
% -------------------------------------------------------------------------
Model_SystemScope = "twin_string_equivalent"; % 双支路等效聚合模型（v2.0）
EMS_MODE      = 0;    % 兼容列（= double(EMS_CTRL_LAW>0)），日志 ems_mode 用
EMS_CTRL_LAW  = 0;    % EMS 变体选择：0=EMS_Baseline(无预测) 1=EMS_AI_Enhanced(即时前馈) 2=EMS_ScheduledFF(定时前馈, T-004)
AI_Pred_Source = 0;   % 预测源：0=Disabled/Persistence，1=FromWorkspace，2=External(STM32N6 预留)，3=EmbeddedML(环内推理)
AI_TPeak_Source = 0;  % t_peak 来源：0=按 AI_Pred_Source 语义，1=强制取 AI_Oracle_Data 第3列(真值, E3 用)

%% ------------------------------------------------------------------------
%  2. 电气额定值 [DRAWING]  —— 来源：系统图/单线图、直流配电板系统图、硬件选型表
%  图纸额定均为“单支路”值；实船左右舷各一套（双支路），对称工况下聚合等效。
% -------------------------------------------------------------------------
DRAWING_N_PropStrings = 2;      % 推进支路数 [-]（左右舷双推进，用户确认；图纸单支路额定×2）
V_dc_nom    = 750;    % 直流母线额定电压 [V]（DC/DC 接入母线侧；与电池电压分离）
V_bat_nom   = 576;    % 动力电池簇标称电压 [V]（两支路并联，电压不变）
DRAWING_Bat_Cap_kWh_perString = 600;    % 单支路电池簇标称容量 [kWh]（图纸额定）
DRAWING_Gen_P_max_perString   = 500;    % 单支路发电机额定电功率 [kW]（图纸额定）
DRAWING_Mot_P_nom_perString   = 350;    % 单支路推进电机额定电功率 [kW]（图纸额定）
Bat_Cap_kWh = DRAWING_Bat_Cap_kWh_perString * DRAWING_N_PropStrings; % 1200 kWh（聚合）
Gen_P_max   = DRAWING_Gen_P_max_perString   * DRAWING_N_PropStrings; % 1000 kW（聚合）
Mot_P_nom   = DRAWING_Mot_P_nom_perString   * DRAWING_N_PropStrings; % 700  kW（聚合）
P_hotel_nom = 100;    % 全船日用/辅机负荷 [kW]（全船级负荷，不随支路翻倍；沿用原模型 EMS_P_Hotel=100）
% 多步预测 EMS (MSFF) 统一设计参考最大负荷：推进电功率物理天花板(Mot_P_nom) + 日用负荷(P_hotel_nom)。
% λ_P = Gen_P_max / P_load_design_max 的统一分母（跨场景固定，禁止逐场景自定义；spec FR-002/FR-011）
P_load_design_max = Mot_P_nom + P_hotel_nom; % 800 kW（DRAWING 派生）

%% ------------------------------------------------------------------------
%  1b. MSFF（最小必要前馈 / 多步预测）配置 [ASSUMED/Model C 配置]
%  对应 spec multistep-predictive-ems（FR-002/003/004/005/006）；T-002 登记，T-004 消费。
% -------------------------------------------------------------------------
MSFF_Horizon_s = 30;     % 预测 horizon [s]（实验变量 {10,15,20,30,45,60}；valid_node_count = H/5）
MSFF_dt_reach  = 0.1;    % 发电机可达性前向递推步长 [s]（≤ tau_gen/10 保证一阶离散稳定）
MSFF_FF_Hyst_kW = 5;     % 前馈防抖阈值 [kW]（新 P_ff* 与保持值差 < 此值时维持原值）
MSFF_InfoMode  = 'traj'; % C_P 信息降级开关：'traj'=完整轨迹 C_MS，'peak'=全窗口恒值 P_peak (C_P, FR-013A)
MSFF_SOC_Knee  = [0.30 0.45];  % f_SOC 折点 [低, 高]：SOC≥高→1.0，SOC≤低→0.0，其间线性（ASSUMED(low)）
MSFF_Vdc_Knee  = [690 720];    % f_V 折点 [低, 高]：Vdc≥高→1.0，Vdc≤低→0.0，其间线性（ASSUMED(low)）

%% ------------------------------------------------------------------------
%  3. 初始状态 [LEGACY/DRAWING]
% -------------------------------------------------------------------------
Ship_V_init = 0;      % 初始对地船速 [m/s]
Ship_X_init = 0;      % 初始航程 [m]
Bat_SOC_init = 0.60;  % 初始 SOC [-]
Pgen_init   = 0;      % 发电机初始功率 [kW]

%% ------------------------------------------------------------------------
%  4. 动力学参数 [LEGACY]  —— 来源：ship_parameters0616.m（聚合量×2）
% -------------------------------------------------------------------------
Ship_Mass   = 1.0e6;  % 船舶等效质量 [kg]（约 1000 t 排水量，船体固有属性，不翻倍）
Gear_Ratio  = 6.08;   % 齿轮箱减速比 [-]（单侧）
Gear_Eff    = 0.96;   % 齿轮箱机械效率 [-]（单级船用齿轮箱典型 0.96~0.98，取保守值）
Elec_Eff    = 0.92;   % 变频器+电机综合电气效率 [-]（典型 0.90~0.94）
Gen_Ramp_Up   = 40;   % 发电机聚合升功率爬坡率 [kW/s]（20 kW/s × 2 台并联）
Gen_Ramp_Down = 40;   % 发电机聚合降功率爬坡率 [kW/s]

%% ------------------------------------------------------------------------
%  5. 推进与水动力参数 [重新标定 v2.0]
%  标定链路（全部可复查）：
%    P_shaft_max = Mot_P_nom · Elec_Eff · Gear_Eff      （电功率→轴功率全链路）
%    P_eff_design = eta_prop · P_shaft_max / (1+SeaMargin)（设计点有效功率）
%    Hull_K_R = P_eff_design / V_design^3               （船体阻力系数）
%  物理含义：8 kn 设计航速时，扣除海况裕量后推进功率恰好用上全部轴功率。
% -------------------------------------------------------------------------
Mot_RPM_nom  = 1500;  % 推进电机额定转速 [rpm]（单侧）
Mot_T_max    = (Mot_P_nom*1000) / (2*pi*Mot_RPM_nom/60); % 聚合电机最大转矩 [Nm]（×2 自动）
Prop_J       = 1600;  % 双轴系+双螺旋桨聚合等效转动惯量（含附水） [kg*m^2]（800×2）
Shaft_B      = 0;     % 轴系线性阻尼 [Nm/(rad/s)]
wake_fraction    = 0.15;  % 伴流分数 [-]（双桨船典型 0.10~0.20，取中值）
ASSUMED_Prop_Efficiency = 0.60; % 准推进效率 [-]（双桨机动船典型 0.55~0.65，取代原 0.55 低估值）
ASSUMED_Sea_Margin      = 0.15; % 海况裕量 [-]（船舶设计常用 10~20%，静水顶速因此略高于设计航速）
V_design_kn      = 11;    % 设计/服务航速 [kn]（公务船定位，交叉校核见下）
V_design_mps     = V_design_kn / 1.944;   % [m/s] ≈ 5.66
% 交叉校核（v2.1）：Fn∇ = V/√(g·∇^(1/3))，∇≈1000 m³ -> Fn∇≈0.57（半排水型）；
% 该区间瘦线型内河公务船 R/W ≈ 0.004~0.006；本标定 R = P_eff/V ≈ 57 kN
% -> R/W ≈ 0.0058，落在区间内；功率受限静水极速 ≈ 11.5 kn。
P_shaft_nom_W    = Mot_P_nom * Elec_Eff * Gear_Eff * 1000; % 聚合轴功率上限 [W] ≈ 618.2 kW
P_shaft_design_W = P_shaft_nom_W / (1 + ASSUMED_Sea_Margin); % 设计点轴功率 [W] ≈ 537.6 kW（裕量保留给风浪）
P_effective_design_W = ASSUMED_Prop_Efficiency * P_shaft_design_W; % ≈ 322.5 kW
Hull_K_R    = P_effective_design_W / V_design_mps^3;  % 船体阻力系数 [N/(m/s)^2] ≈ 1780
% 螺旋桨二次模型系数（聚合等效桨；派生公式与原模型一致，设计点已更新）
% 注意：Q_prop_nom 必须取设计点轴功率（含海况裕量），否则 8 kn 巡航即占满电机额定、
% 裕量被隐含效率吞掉（v2.0 首轮自校发现的问题）
n_prop_nom_rpm = Mot_RPM_nom / Gear_Ratio;            % [rpm]
omega_prop_nom = n_prop_nom_rpm * pi / 30;            % [rad/s]
Q_prop_nom     = P_shaft_design_W / omega_prop_nom;   % [Nm]（设计点轴扭矩）
V_A_design     = (1 - wake_fraction) * V_design_mps;  % [m/s]
F_eff_design   = P_effective_design_W / V_design_mps; % [N]
F_bollard_factor = 1.5; F_bollard = F_bollard_factor * F_eff_design;
K_Fw = F_bollard / omega_prop_nom^2;
K_Fv = (F_bollard - F_eff_design) / V_A_design^2;
Q_bollard_factor = 1.2; Q_bollard = Q_bollard_factor * Q_prop_nom;
K_Qw = Q_bollard / omega_prop_nom^2;
K_Qv = (Q_bollard - Q_prop_nom) / V_A_design^2;
F_env_const = 0;      % 额外环境纵向力 [N]（正=助推）
omega_min_radps = 0.01; % 转矩换算的最小角速度保护 [rad/s]（避免除零）

%% ------------------------------------------------------------------------
%  6. 假设参数 [ASSUMED]  —— 缺少可靠来源，需后续确认
% -------------------------------------------------------------------------
ASSUMED_Gen_TimeConstant    = 5;    % 发电机平均模型一阶时间常数 [s]（原模型约1~3s，取保守值）
ASSUMED_Gen_MinStablePower  = 0;    % 发电机最低稳定功率 [kW]（0=可怠速空载；真实机组通常为额定30~50%）
ASSUMED_Bat_DischargeLimit  = 1000; % 电池聚合最大放电功率 [kW]（500×2；原 EMS 硬编码 500/支路）
ASSUMED_Bat_ChargeLimit     = 1000; % 电池聚合最大充电功率 [kW]（500×2）
ASSUMED_Bat_Eff_Discharge   = 0.95; % 放电效率 [-]
ASSUMED_Bat_Eff_Charge      = 0.95; % 充电效率 [-]
ASSUMED_SOC_min             = 0.10; % SOC 下限 [-]
ASSUMED_SOC_max             = 0.95; % SOC 上限 [-]
ASSUMED_SOC_DisDerate_Start = 0.25; % 低SOC放电降额起始点 [-]（SOC<=0.10 时降额到0）
ASSUMED_SOC_ChgDerate_Start = 0.90; % 高SOC充电降额起始点 [-]（SOC>=0.95 时降额到0）
ASSUMED_SpeedPI_Kp          = 600;  % 船速PI比例增益 [kW/(m/s)]（随功率规模×2）
ASSUMED_SpeedPI_Ki          = 20;   % 船速PI积分增益 [kW/(m/s)/s]（随功率规模×2）
ASSUMED_Speed_Ramp_Up       = 0.05; % 目标航速斜坡率 [m/s^2]（常规巡航加减速；机动工况由 SCN 覆盖）
ASSUMED_Drive_TimeConstant  = 0.5;  % 推进驱动功率滞后时间常数 [s]
ASSUMED_EMS_SOC_Ref         = 0.60; % EMS 电池SOC参考值 [-]
ASSUMED_EMS_SOC_Gain        = 200;  % EMS 充电功率修正增益 [kW/SOC]（随功率规模×2）
ASSUMED_EMS_FF_Gain         = 1.0;  % 预测前馈增益 K_ff [-]
ASSUMED_EMS_FF_Max_kW       = 400;  % 预测前馈最大附加功率 [kW]（随功率规模×2）
ASSUMED_Pred_Bound_kW       = 2000; % 预测值有效性上界 [kW]（随功率规模×2）
ASSUMED_TFF_TauMargin       = 10;   % 定时前馈到位裕量 [s]（覆盖机组一阶惯性 τ=5s，T-004）
ASSUMED_TFF_SOC_Thresh      = 0.40; % 定时前馈低SOC保守提前阈值 [-]（T-004）
ASSUMED_TFF_SOC_AdvancePct  = 0.25; % 定时前馈低SOC爬坡时间放宽比例 [-]（T-004）
ASSUMED_BSFC_Breakpoints    = [0 100 200 400 600 800 1000];      % BSFC 断点 [kW]（聚合功率，原断点×2）
ASSUMED_BSFC_Table          = [1200 700 550 480 450 430 460];    % BSFC [g/kWh]（比油耗与单机相同）
ASSUMED_TargetSpeed         = 5.66;  % 默认目标航速 [m/s]（11 kn 服务航速；功率受限极速 ≈11.5 kn）
ASSUMED_VoyageDistance      = 5000; % 默认航程 [m]
ASSUMED_RiverCurrentSpeed   = 1.0;  % 河流流速幅值 [m/s]（S02 顺水 +、S03 逆水 -）
ASSUMED_Resistance_Change_RampTime = 15; % 阻力渐变区过渡时间 [s]
ASSUMED_Resistance_Change_Amplitude = 0.4; % 阻力变化幅值默认值（S04 增 0.4）
ASSUMED_AI_DataValid        = false; % 预测数据有效性开关（AI_Pred_Source>=1 时生效）
AI_Oracle_Data = [0 0 0; 100000 0 0]; % oracle 外部预测信号 [t(s) P_pred(kW) t_peak_rel(s)]（From Workspace 块读取，ai_predict 端口 7/8）
AI_Pred_Data   = [0 0];     % 兼容保留（T-002 后 ai_predict 不再引用；FromWorkspace 通道改用 AI_Oracle_Data）
STM32_Pred_Data = [0 0];    % 外部/STM32N6 预测数据 [t(s) P_pred(kW)] 或 N×3（预留）
AI_Pred_Timeout_s = 2;      % 外部通信超时阈值 [s]，预留

%% 电气层（Model C，直流母线）—— 全部为待设计方核定的预置值
EMS_WORK_MODE = 2;          % EMS 工作模式：0=手动(只监测) 1=半自动(规则分配) 2=全自动(含预测增强)
ASSUMED_C_bus = 40;         % 母线等效电容 [F]（沿用 0730 每段 20 F × 2 段并联聚合）
ASSUMED_K_droop = 10;       % 电池 DC/DC 下垂系数 [kW/V]（初值；经灵敏度扫描固化）
ASSUMED_eta_rect = 0.97;    % 发电机整流效率 [-]
ASSUMED_eta_dcdc = 0.97;    % 电池 DC/DC 变换效率 [-]
ASSUMED_EMS_Manual_Pgen_kW = 400; % 手动模式发电机功率设定 [kW]
ASSUMED_V_floor = 50;       % 功率-电流换算电压下限保护 [V]
% FEC 等效燃油换算系数（ECMS 思想）：1 kWh 电池储能 ≈ k_eq kg 燃油
ASSUMED_BSFC_Avg_gkWh = mean(ASSUMED_BSFC_Table(3:end)); % BSFC 表工作区均值 ≈474 [g/kWh]
ASSUMED_k_eq_FEC = ASSUMED_BSFC_Avg_gkWh / (1000 * ASSUMED_Bat_Eff_Charge); % ≈0.499 [kg/kWh]

%% CCS 2026《船舶应用直流综合电力系统指南》电能质量判据（来源：CCS 指南，置信度 high）
CCS_Vdc_Steady_Band    = V_dc_nom * [0.90 1.06];   % 稳态 [675 795] V
CCS_Vdc_Transient_Band = V_dc_nom * [0.80 1.07];   % 瞬态 [600 802.5] V
CCS_t_recovery_max     = 1.5;                      % 恢复时间上限 [s]

%% 嵌入式 ML 预测器占位参数（AI_Pred_Source=3 环内推理；由 07_run_ai_comparison.m 加载真实权重）
MLP_WeightsLoaded = 0;     % 0/1 数值（勿用 logical，EMChart 参数类型推断问题）   % 权重是否已加载（false 时 source=3 自动回退 Baseline）
MLP_W1 = zeros(300, 64); MLP_b1 = zeros(1, 64);
MLP_W2 = zeros(64, 32);  MLP_b2 = zeros(1, 32);
MLP_W3 = zeros(32, 1);   MLP_b3 = 0;
MLP_FeatMean = zeros(1, 5); MLP_FeatStd = ones(1, 5);
MLP_YMean = 0; MLP_YStd = 1;

%% ------------------------------------------------------------------------
%  7. 参数元数据（用于生成 reports/parameter_audit.csv）
%  列：名称 | 值 | 单位 | 来源 | 置信度 | 使用位置 | 是否需要确认
% -------------------------------------------------------------------------
ParamMeta = {
%  name                             unit       source                          confidence  used_by                    need_confirm
  'Model_SystemScope'              '-'        'Model 配置 (v2.0 双支路等效)' 'high'      '全部'                     false
  'EMS_MODE'                        '-'        'Model 配置(兼容列)'          'high'      '05_EMS_Controller'          false
  'EMS_CTRL_LAW'                     '-'        'Model C 配置 (T-003)'        'high'      '05_EMS_Controller'          false
  'AI_Pred_Source'                  '-'        'Model 配置'                  'high'      '05_EMS_Controller'          false
  'AI_TPeak_Source'                 '-'        'Model C 配置 (T-002)'        'high'      '05_EMS_AI'                  false
  'DRAWING_N_PropStrings'           '-'        'DRAWING: 双支路(用户确认)'     'high'      '派生全部聚合额定'           false
  'V_dc_nom'                        'V'        'DRAWING: 系统图'               'high'      '08_Power_Availability'      false
  'V_bat_nom'                       'V'        'DRAWING: 系统图'               'high'      '07_Battery_Energy'          false
  'DRAWING_Bat_Cap_kWh_perString'   'kWh'      'DRAWING: 图纸额定/支路'        'high'      '派生 Bat_Cap_kWh'           false
  'DRAWING_Gen_P_max_perString'     'kW'       'DRAWING: 图纸额定/支路'        'high'      '派生 Gen_P_max'             false
  'DRAWING_Mot_P_nom_perString'     'kW'       'DRAWING: 图纸额定/支路'        'high'      '派生 Mot_P_nom'             false
  'Bat_Cap_kWh'                     'kWh'      'DRAWING: 600×2 聚合'           'high'      '07_Battery_Energy'          false
  'Gen_P_max'                       'kW'       'DRAWING/LEGACY: 500×2 聚合'    'high'      '05/06/08'                   false
  'Mot_P_nom'                       'kW'       'DRAWING/LEGACY: 350×2 聚合'    'high'      '02/03/08'                   false
  'P_hotel_nom'                     'kW'       'LEGACY: EMS_P_Hotel'           'medium'    '01/04'                      true
  'P_load_design_max'               'kW'       'DRAWING: Mot_P_nom+P_hotel 派生' 'high'    'MSFF λ_P 统一分母(FR-002)'  false
  'MSFF_Horizon_s'                  's'        'MSFF 配置(T-002)'             'high'      '05_EMS_MinFF'               false
  'MSFF_dt_reach'                   's'        'MSFF 配置(ASSUMED)'           'low'       '05_EMS_MinFF'               true
  'MSFF_FF_Hyst_kW'                 'kW'       'MSFF 配置(ASSUMED)'           'low'       '05_EMS_MinFF'               true
  'MSFF_InfoMode'                   '-'        'MSFF 配置(FR-013A)'           'high'      '05_EMS_MinFF'               false
  'MSFF_SOC_Knee'                   '-'        'MSFF 配置(ASSUMED)'           'low'       '05_EMS_MinFF'               true
  'MSFF_Vdc_Knee'                   'V'        'MSFF 配置(ASSUMED)'           'low'       '05_EMS_MinFF'               true
  'Ship_V_init'                     'm/s'      'LEGACY: 原脚本'                'medium'    '03_Propulsion'              false
  'Ship_X_init'                     'm'        'Model 规格'                  'high'      '03_Propulsion'              false
  'Bat_SOC_init'                    '-'        'LEGACY: 原脚本'                'medium'    '07_Battery_Energy'          false
  'Pgen_init'                       'kW'       'Model 规格'                  'high'      '06_Genset'                  false
  'Ship_Mass'                       'kg'       'LEGACY: 原脚本'                'medium'    '03_Propulsion'              true
  'Gear_Ratio'                      '-'        'LEGACY: 原脚本'                'medium'    '03_Propulsion'              true
  'Gear_Eff'                        '-'        'LEGACY: 原脚本(行业0.96~0.98)' 'medium'    '03_Propulsion'              true
  'Elec_Eff'                        '-'        'LEGACY: 原脚本(行业0.90~0.94)' 'medium'    '03_Propulsion'              true
  'Gen_Ramp_Up'                     'kW/s'     'LEGACY: 20×2 聚合'             'medium'    '06_Genset'                  true
  'Gen_Ramp_Down'                   'kW/s'     'LEGACY: 20×2 聚合'             'medium'    '06_Genset'                  true
  'Mot_RPM_nom'                     'rpm'      'LEGACY: 原脚本'                'medium'    '03_Propulsion'              true
  'Mot_T_max'                       'Nm'       'LEGACY: 派生(×2)'              'medium'    '03_Propulsion'              true
  'Prop_J'                          'kg*m^2'   'LEGACY: 800×2 聚合'            'low'       '03_Propulsion'              true
  'Shaft_B'                         'Nm/(rad/s)', 'LEGACY: 原脚本'              'low'       '03_Propulsion'              true
  'wake_fraction'                   '-'        'LEGACY: 原脚本(双桨0.10~0.20)' 'low'       '03_Propulsion'              true
  'ASSUMED_Prop_Efficiency'         '-'        'ASSUMED(行业0.55~0.65)'        'low'       '派生 Hull_K_R'              true
  'ASSUMED_Sea_Margin'              '-'        'ASSUMED(设计惯例10~20%)'       'low'       '派生 Hull_K_R'              true
  'V_design_kn'                     'kn'       '公务船定位(11kn,R/W校核)'  'medium'    '03_Propulsion'              true
  'Hull_K_R'                        'N/(m/s)^2', '重新标定: η·P_shaft/(1+m)/V^3' 'medium'  '03_Propulsion'              true
  'K_Fw'                            'N/(rad/s)^2', 'LEGACY: 派生(新设计点)'    'medium'    '03_Propulsion'              true
  'K_Fv'                            'N/(m/s)^2', 'LEGACY: 派生(新设计点)'      'medium'    '03_Propulsion'              true
  'K_Qw'                            'Nm/(rad/s)^2', 'LEGACY: 派生(新设计点)'   'medium'    '03_Propulsion'              true
  'K_Qv'                            'Nm/(m/s)^2', 'LEGACY: 派生(新设计点)'     'medium'    '03_Propulsion'              true
  'F_env_const'                     'N'        'LEGACY: 原脚本'                'medium'    '03_Propulsion'              false
  'omega_min_radps'                 'rad/s'    'Model 实现细节'              'high'      '03_Propulsion'              false
  'ASSUMED_Gen_TimeConstant'        's'        'ASSUMED'                       'low'       '06_Genset'                  true
  'ASSUMED_Gen_MinStablePower'      'kW'       'ASSUMED'                       'low'       '06_Genset'                  true
  'ASSUMED_Bat_DischargeLimit'      'kW'       'ASSUMED(500×2 聚合)'           'low'       '07/08'                      true
  'ASSUMED_Bat_ChargeLimit'         'kW'       'ASSUMED(500×2 聚合)'           'low'       '07/08'                      true
  'ASSUMED_Bat_Eff_Discharge'       '-'        'ASSUMED'                       'low'       '07_Battery_Energy'          true
  'ASSUMED_Bat_Eff_Charge'          '-'        'ASSUMED'                       'low'       '07_Battery_Energy'          true
  'ASSUMED_SOC_min'                 '-'        'ASSUMED'                       'low'       '07_Battery_Energy'          true
  'ASSUMED_SOC_max'                 '-'        'ASSUMED'                       'low'       '07_Battery_Energy'          true
  'ASSUMED_SOC_DisDerate_Start'     '-'        'ASSUMED'                       'low'       '07_Battery_Energy'          true
  'ASSUMED_SOC_ChgDerate_Start'     '-'        'ASSUMED'                       'low'       '07_Battery_Energy'          true
  'ASSUMED_SpeedPI_Kp'              'kW/(m/s)' 'ASSUMED(×2)'                   'low'       '02_Ship_Speed'              true
  'ASSUMED_SpeedPI_Ki'              'kW/(m/s)/s', 'ASSUMED(×2)'                'low'       '02_Ship_Speed'              true
  'ASSUMED_Speed_Ramp_Up'           'm/s^2'    'ASSUMED'                       'low'       '01_Scenario'                true
  'ASSUMED_Drive_TimeConstant'      's'        'ASSUMED'                       'low'       '03_Propulsion'              true
  'ASSUMED_EMS_SOC_Ref'             '-'        'ASSUMED'                       'low'       '05_EMS'                     true
  'ASSUMED_EMS_SOC_Gain'            'kW/SOC'   'ASSUMED(×2)'                   'low'       '05_EMS'                     true
  'ASSUMED_EMS_FF_Gain'             '-'        'ASSUMED'                       'low'       '05_EMS_AI'                  true
  'ASSUMED_EMS_FF_Max_kW'           'kW'       'ASSUMED(×2)'                   'low'       '05_EMS_AI'                  true
  'ASSUMED_Pred_Bound_kW'           'kW'       'ASSUMED(×2)'                   'low'       '05_EMS_AI'                  true
  'ASSUMED_TFF_TauMargin'           's'        'ASSUMED(T-004, 覆盖惯性τ=5s)'  'low'       '05_EMS_ScheduledFF(C)'      true
  'ASSUMED_TFF_SOC_Thresh'          '-'        'ASSUMED(T-004)'                'low'       '05_EMS_ScheduledFF(C)'      true
  'ASSUMED_TFF_SOC_AdvancePct'      '-'        'ASSUMED(T-004)'                'low'       '05_EMS_ScheduledFF(C)'      true
  'ASSUMED_BSFC_Breakpoints'        'kW'       'ASSUMED(原表断点×2)'           'low'       '10_Fuel'                    true
  'ASSUMED_BSFC_Table'              'g/kWh'    'ASSUMED(原模型查表)'           'low'       '10_Fuel'                    true
  'ASSUMED_TargetSpeed'             'm/s'      'ASSUMED(11 kn 服务航速)'     'low'       '01_Scenario'                true
  'ASSUMED_VoyageDistance'          'm'        'ASSUMED'                       'low'       '11_Voyage_Stop'             true
  'ASSUMED_RiverCurrentSpeed'       'm/s'      'ASSUMED'                       'low'       '01_Scenario'                true
  'ASSUMED_Resistance_Change_RampTime' 's'     'ASSUMED'                       'low'       '01_Scenario'                true
  'ASSUMED_Resistance_Change_Amplitude' '-'    'ASSUMED'                       'low'       '01_Scenario'                true
  'ASSUMED_AI_DataValid'            '-'        'ASSUMED'                       'low'       '05_EMS_AI'                  true
  'AI_Pred_Data'                    '[t,P,tp]'  '预留接口(T-002 扩展 N×3)'      'na'        '05_EMS_AI'                  true
  'AI_Oracle_Data'                  '[t,P,tp]'  'FromWorkspace oracle 通道(T-002)' 'na'  '05_EMS_AI'                  true
  'AI_Pred_Data'                    '[t,P,tp]'  '兼容保留(T-002 后不再引用)'      'na'        '05_EMS_AI'                  true
  'STM32_Pred_Data'                 '[t,P]'    '预留接口'                      'na'        '05_EMS_AI'                  true
  'AI_Pred_Timeout_s'               's'        '预留接口'                      'na'        '05_EMS_AI'                  true
  'MLP_WeightsLoaded'               '-'        'python/export/mlp_weights.mat' 'na'        '05_EMS_AI(source=3)'        false
  'EMS_WORK_MODE'                   '-'        'Model C 配置'                  'high'      '05_EMS_Controller(C)'       false
  'ASSUMED_C_bus'                   'F'        'ASSUMED(0730 每段20F×2)'       'low'       '13_DC_Bus(C)'               true
  'ASSUMED_K_droop'                 'kW/V'     'ASSUMED(灵敏度扫描固化)'       'low'       '14_Battery_Converter(C)'    true
  'ASSUMED_eta_rect'                '-'        'ASSUMED'                       'low'       '06_Genset(C)'               true
  'ASSUMED_eta_dcdc'                '-'        'ASSUMED'                       'low'       '14_Battery_Converter(C)'    true
  'ASSUMED_EMS_Manual_Pgen_kW'      'kW'       'ASSUMED'                       'low'       '05_EMS(C,手动模式)'         true
  'ASSUMED_V_floor'                 'V'        'ASSUMED(除法保护)'             'high'      '13/14(C)'                   false
  'ASSUMED_BSFC_Avg_gkWh'           'g/kWh'    'ASSUMED(表工作区均值)'         'low'       'FEC 指标'                   true
  'ASSUMED_k_eq_FEC'                'kg/kWh'   'ASSUMED(ECMS 折算)'            'low'       'FEC 指标'                   true
  'CCS_Vdc_Steady_Band'             'V'        'CCS 2026 指南'                'high'      '15_Bus_Voltage_Monitor(C)'  false
  'CCS_Vdc_Transient_Band'          'V'        'CCS 2026 指南'                'high'      '15_Bus_Voltage_Monitor(C)'  false
  'CCS_t_recovery_max'              's'        'CCS 2026 指南'                'high'      '15_Bus_Voltage_Monitor(C)'  false
};
fprintf('[Model_Parameters] v2.0 双支路等效：电机 %.0f kW / 发电机 %.0f kW / 电池 %.0f kWh，Hull_K_R=%.0f N/(m/s)^2\n', ...
    Mot_P_nom, Gen_P_max, Bat_Cap_kWh, Hull_K_R);
fprintf('[Model_Parameters] 参数已加载：%d 项（含 %d 项 ASSUMED_）\n', ...
    size(ParamMeta,1), sum(startsWith(ParamMeta(:,1), 'ASSUMED_')));

%% ========================================================================
%  ModelE_Physics_Parameters.m — Model E (Phase 4 Physics) 参数事实源（完整独立）
%  ========================================================================
%  唯一事实源：.specs/phase4-a2-model-e-construction/（brainstorm/requirements/design）
%  + 开发日志 2026-09-04 条目（17 项参考文献证据链 [1]~[17]）
%  + 顾问 Model E v1 Literature-Surrogate 参数集（2026-09-04 session 裁决冻结，
%    九节点 LUT 原始记录见 A2 证据链文档 modelE_parameter_evidence.md）。
%
%  【完整性纪律（OQ-8，用户裁决）】本文件**完整独立**：单独 run 即定义 Model E
%  全部参数（含 EMS 七变体内部所需继承项），禁止先 run legacy 参数文件再 override，
%  禁止引用 config/_legacy_reference/（该目录不入路径、不参与运行，仅证据对照）。
%
%  【诚实标记纪律（NFR-410）】500 t representative hull 声明：
%    target_vessel_confirmed = false。本文件全部几何/水动力参数为文献代理或推导值，
%    绝不表述为目标船实测/实船几何；500 t 排水量本身是未确认工作假定
%    （docs/data_request_list.md 挂账，真实资料到位后走 spec 修订）。
%    推导值 ≠ 文献原值：引用文献时必须写"根据文献[n]按公式××推导"。
%
%  【冻结敏感性档位（brainstorm Q1：登记不扫掠，扫掠属 B/F 包）】
%    DG_P_min_kW     ∈ {125, 200, 250} kW
%    DG_ramp_kW_per_s ∈ {10, 20, 50} kW/s/unit
%    DG_tau_s        ∈ {2, 5, 10} s
%    DG_C_start_kg   ∈ {0.5, 1, 2} kg/start
%  唯一在 A2 扫掠的敏感性：螺旋桨参数族 ±20% OAT（T-011，FR-417）。
%
%  【与 Model D 的关键差异】（override 依据 = A2 spec/design 裁决）：
%    Ship_Mass 1.0e6 → 5.0e5 kg（Q3 裁决 500 t nominal，放弃绝对量连续性）
%    Hull_K_R 1780 → 890 N/(m/s)^2（Q4 裁决线性 Δ 缩放）
%    新增 hull 几何/浅水修正链/w(H/T) 信号化/双机组/BSFC PCHIP/电池 1RC ECM+SOP
%    wake_fraction 0.15 const → w_deep=0.10（Molland 回归）+ Δw_Zhang(H/T) 信号
%    K_Fw/K_Fv/K_Qw/K_Qv 按 w_deep=0.10 重标定（design §4.2，D-8）
%    ASSUMED_Gen_MinStablePower 0 → 125 kW/unit（design §2 override 表）
%  ========================================================================

%% ------------------------------------------------------------------------
%  0. 模型范围与变体选择 [INHERITED_LEGACY，接口不变]
%  ------------------------------------------------------------------------
Model_SystemScope = "twin_string_equivalent"; % 双支路等效聚合模型（不变）
EMS_MODE      = 0;    % 兼容列（= double(EMS_CTRL_LAW>0)）
EMS_CTRL_LAW  = 0;    % 0=Baseline 1=AI_Enhanced 2=ScheduledFF 4=Fixed_ECMS 5=SOC_Adaptive 6=Oracle 7=NN_AECMS
AI_Pred_Source = 0;   % 0=Disabled/Persistence 1=FromWorkspace 2=External(STM32N6) 3=EmbeddedML
AI_TPeak_Source = 0;  % 0=按 AI_Pred_Source 语义 1=强制 AI_Oracle_Data 第3列
EMS_WORK_MODE = 2;    % 0=手动 1=半自动 2=全自动

%% ------------------------------------------------------------------------
%  1. 船体主尺度与质量 [Model E 新事实源，FR-412]
%  ------------------------------------------------------------------------
%  几何 = 按 Δ=ρ·L·B·T·C_B=500 t 守恒 + Rhine86 [2] L/B、B/T 比例相似缩尺的
%  **推导值**（λ=0.6250），非文献原值、非实船几何（target_vessel_confirmed=false）。
Hull_Lpp_m   = 53.45;   % 垂线间长 [m]   DERIVED（Rhine86 [2] 相似缩尺）
Hull_B_m     = 7.13;    % 型宽     [m]   DERIVED（同上）
Hull_T_m     = 2.188;   % 设计吃水 [m]   DERIVED（同上）
Hull_Sw_m2   = 554.2;   % 湿表面积 [m^2] DERIVED（缩尺换算，非文献原值）
Hull_CB      = 0.60;    % 方形系数 [-]   LITERATURE_NOMINAL（三腿佐证见开发日志 2026-09-04；公务船丰满度上界点，[CB-1][CB-2]）
target_vessel_confirmed = false;  % NFR-410 诚实标记：500 t/几何均未获实船确认

Ship_Mass    = 5.0e5;   % 排水量质量 [kg] DERIVED（500 t 工作假定，Q3 用户裁决 2026-09-04）
rho_water    = 1000;    % 水密度 [kg/m^3] INHERITED_LEGACY（内河淡水口径，与 Model D 一致）
nu_water     = 1.19e-6; % 运动粘度 [m^2/s] LITERATURE_NOMINAL（淡水 15°C 近似，design §4.1）

%% ------------------------------------------------------------------------
%  2. 船体阻力与浅水修正 [Model E 新事实源，FR-412/413/415]
%  ------------------------------------------------------------------------
%  K_R 释义：R_deep = res_mult · K_R · V_rel·|V_rel| 的集总标定系数
%  （设计点反演量，非 build-up 物理量）。890 = 1780 × (500/1000) 线性 Δ 缩放
%  （Q4 用户裁决 2026-09-04；拒绝 Δ^(2/3)≈1121：Fn≈0.33 时纯摩擦相似低估阻力）。
%  K_R 是整条速度-功率曲线的形状手柄；执行中禁止重新论证（tasks T-004 冻结）。
Hull_K_R     = 890;     % [N/(m/s)^2] DERIVED（线性 Δ 缩放，Q4 裁决）

%  浅水修正链（design §4.1 相对增量法：文献公式只算 Δr，绝对水平由 K_R 锚定）
Hull_k_form  = 0.2;     % 形状因子 k [-]，1+k=1.2；ENGINEERING_SURROGATE（T-013 裁决：1+k∈1.15~1.30 行业带内的工程取值，无单篇文献直接锚定本船型；待 Rhine86 报告/ITTC 惯例类资料确认）
Shallow_deltak_coef = 0.644;    % Δk = 0.644·(H/T)^(-1.72)，Millward [4]
Shallow_deltak_exp  = -1.72;    %   （Zhang 2023 [1] 亦采用）
Shallow_c1   = 1.1680;  % 摩擦修正 C_F* 系数 [1][3]（Rhine86 标定族；C_B=0.86 型差外推局限见证据链文档）
Shallow_c2   = -0.5238;
Shallow_c3   = -1.472;
Shallow_HT_deep_limit = 3.0;    % H/T>3 → Δr=0（深水渐近，FR-415）
Shallow_HT_min_valid  = 1.2;    % H/T<1.2 → clamp=1.2 + MODEL_DOMAIN_WARNING（禁止静默外推）
WaterDepth_default_m  = 20.0;   % 默认水深 [m]（H/T≈9.1 → 深水渐近，行为与 Model D 一致）

%% ------------------------------------------------------------------------
%  3. 伴流分数 w(H/T) [OQ-4 裁决：混合锚定，design D-3]
%  ------------------------------------------------------------------------
%  w_deep = Harvald/Molland 双桨回归（Molland/Turnock/Hudson）：
%    w_T = (0.71 − 2.39·C_B + 2.33·C_B^2) + 0.12·C_B^4·(6.5 − L/B)
%  代入 C_B=0.60、L/B=7.4965 → w_T ≈ 0.099 → 冻结 0.10（LITERATURE_NOMINAL）。
%  深度修正 = Zhang 2023 双桨表的**相对变化量**（标定族 C_B=0.86 ≠ 本船 0.60，
%  只借形状不借绝对值；transferred literature surrogate，非本船验证伴流场）。
Wake_CB    = Hull_CB;
Wake_LB    = Hull_Lpp_m / Hull_B_m;
Wake_w_deep = 0.71 - 2.39*Wake_CB + 2.33*Wake_CB^2 + 0.12*Wake_CB^4*(6.5 - Wake_LB);  % ≈0.0993
Wake_w_deep = 0.10;     % 冻结值 [OQ-4/D-3]（回归计算值 0.0993 舍入，见上行注释）
%  Zhang 2023 双桨 w 表（H/T 升序，供 1-D linear 插值；域 clamp [1.2, 2.7]）
Wake_HT_nodes = [1.2  1.4  1.6  1.8  2.0  2.7];
Wake_w_Zhang  = [0.32 0.26 0.20 0.23 0.27 0.22];
Wake_HT_ref   = 2.7;    % Δw 参照锚：Δw(H/T) = w_Zhang(H/T) − w_Zhang(2.7)
Wake_w_ref    = 0.22;   %  = w_Zhang(2.7)
%  w 由参数变为信号（输入 Propeller_Hydrodynamics，T-005 接线；H/T 单一事实源 = 船体模块 HT_ratio 输出）
wake_fraction = Wake_w_deep;  % 兼容别名 [DEPRECATED→signal]：T-005 后 builder 不再作常量引用

%% ------------------------------------------------------------------------
%  4. 推进链与螺旋桨（重标定 D-8；效率链边界 FR-416）
%  ------------------------------------------------------------------------
Mot_P_nom   = 700;      % 聚合推进电机额定 [kW]（350×2）[DRAWING]
Mot_RPM_nom = 1500;     % 推进电机额定转速 [rpm] [DRAWING]
Gear_Ratio  = 6.08;     % 齿轮箱减速比 [-] INHERITED_LEGACY
Gear_Eff    = 0.96;     % 齿轮箱效率 [-] INHERITED_LEGACY（D-1 裁决保持 0.96；文献带 0.96~0.98，顾问 0.97 入证据链备查）
ASSUMED_Sea_Margin = 0.15;      % 海况裕量 [-] INHERITED_LEGACY（设计惯例 10~20%）
ASSUMED_Prop_Efficiency = 0.60; % 准推进效率 [-] INHERITED_LEGACY（双桨机动船 0.55~0.65）

%  效率链物理边界（FR-416，[5] LI 2026）：
%    Elec_Eff = η_inv × η_motor × η_shaft = 0.98 × 0.96 × 0.98 ≈ 0.9219 → 冻结 0.92
%    **不含齿轮箱**（Gear_Eff 独立存在于转矩链）；每因子在推进能量路径上恰好出现一次。
Eff_inv   = 0.98;       % 逆变器效率 LITERATURE_NOMINAL [5]
Eff_motor = 0.96;       % 电机效率   LITERATURE_NOMINAL [5]
Eff_shaft = 0.98;       % 轴系效率   LITERATURE_NOMINAL [5]
Elec_Eff  = 0.92;       % 综合电气效率 LITERATURE_NOMINAL（Model D 口径不变，provenance 升级）

%  ---- 螺旋桨四 K 重标定（w_deep=0.10 → V_A_design 变更，禁止沿用 Model D 值，D-8）----
%  设计点：11 kn 巡航桨设计点（保留）；推导链与 Model D 相同，仅 V_A 变更：
V_design_kn  = 11;
V_design_mps = V_design_kn / 1.944;                          % ≈5.6584 m/s
P_shaft_nom_W    = Mot_P_nom * Elec_Eff * Gear_Eff * 1000;   % 618,240 W
P_shaft_design_W = P_shaft_nom_W / (1 + ASSUMED_Sea_Margin); % 537,600 W（海况裕量保留）
P_effective_design_W = ASSUMED_Prop_Efficiency * P_shaft_design_W;  % 322,560 W
F_eff_design_N   = P_effective_design_W / V_design_mps;      % ≈56,999 N
Prop_J      = 1600;  % 双轴系+双桨聚合转动惯量（含附水）[kg·m^2] INHERITED_LEGACY（800×2）
Shaft_B     = 0;     % 轴系线性阻尼 INHERITED_LEGACY
n_prop_nom_rpm = Mot_RPM_nom / Gear_Ratio;                   % ≈246.71 rpm
omega_prop_nom = n_prop_nom_rpm * pi / 30;                   % ≈25.829 rad/s
Q_prop_nom  = P_shaft_design_W / omega_prop_nom;             % ≈20,810 N·m（设计点轴扭矩）
V_A_design  = (1 - Wake_w_deep) * V_design_mps;              % ≈5.0926 m/s（w=0.10 → 重标定根）
F_bollard   = 1.5 * F_eff_design_N;                          % ≈85,500 N
K_Fw = F_bollard / omega_prop_nom^2;                         % ≈128.2 N/(rad/s)^2
K_Fv = (F_bollard - F_eff_design_N) / V_A_design^2;          % ≈1098.9 N/(m/s)^2
Q_bollard = 1.2 * Q_prop_nom;                                % ≈24,972 N·m
K_Qw = Q_bollard / omega_prop_nom^2;                         % ≈37.43 N·m/(rad/s)^2
K_Qv = (Q_bollard - Q_prop_nom) / V_A_design^2;              % ≈160.5 N·m/(m/s)
F_env_const = 0;      % 额外环境纵向力 [N]（正=助推）INHERITED_LEGACY（bank/wind 接口预留）
omega_min_radps = 0.01;  % 转矩换算最小角速度保护 INHERITED_LEGACY

%% ------------------------------------------------------------------------
%  5. 双发电机组（FR-418/419/420；WH17ME 6WH17C680ME5 [16][17] VENDOR）
%  ------------------------------------------------------------------------
DG_P_rated_kW   = 500;   % 单机额定电功率 [kW] VENDOR [16]（500 kW @1500 rpm）
DG_n_rated_rpm  = 1500;  % 额定转速 [rpm] VENDOR [16]
DG_n_idle_rpm   = 600;   % 怠速转速 [rpm] VENDOR [16]
DG_P_min_kW     = 125;   % 最低稳定负荷 25% [kW/unit] LITERATURE_NOMINAL [6]（冻结敏感性 {125,200,250}）
DG_P_opt_kW     = 375;   % 最佳效率点 75% [kW/unit] LITERATURE_NOMINAL [6]
DG_ramp_kW_per_s = 20;   % 爬坡率 [kW/s/unit] engineering-calibrated（[9] 1.67%~4% rated/s 带内；冻结 {10,20,50}）
DG_tau_s        = 5;     % 一阶时间常数 [s] engineering-calibrated（无机型精确数据；冻结 {2,5,10}）
DG_t_starting_s = 25;    % STARTING 段 [s] DERIVED + LITERATURE_SURROGATE：由总 start-to-online ≈28~30 s [7] 与 SYNC ≈3~5 s [8] 组合推得（30−5=25），非文献直接给出 25 s
DG_t_sync_s     = 5;     % SYNCHRONIZING 段 [s] LITERATURE_NOMINAL [8]（同步 3~5 s）
DG_t_cooldown_s = 30;    % COOLDOWN 段 [s] surrogate（[7] 量级；怠速油耗未计，登记局限）
N_ticks_starting = 2500; % 状态机 tick 数（0.01 s 采样 → 25 s；整数 tick 判据，OQ-6）
N_ticks_sync     = 500;  % 5 s
N_ticks_cooldown = 3000; % 30 s
%  启动燃油（方案 A event-level surrogate，OQ-6：STARTING+SYNC 30 s 均摊，只算一次）：
%  C_start = 125 kW × 0.6 kg/kWh × (30/3600) h = 0.625 kg → 冻结 0.6 kg/start（DERIVED surrogate，
%  非实测；冻结敏感性 {0.5, 1, 2} kg）。禁止与 STARTING 段 idle fuel 双算。
DG_C_start_kg = 0.6;     % [kg/start] DERIVED
%  per-genset BSFC 五点 PCHIP（[11] Fridell 2021 形状 × [16] WH17ME 427 锚定归一；
%  375 kW 点 427/441 归一 ≈447；100% 点 453 沿用项目 rated anchor；
%  全部 LITERATURE_NOMINAL/DERIVED，不得标实测。LUT 只在 ON 且 P≥P_min 有效，禁止向 0 kW 外推）
DG_BSFC_P_kW  = [125  250  375  400  500];
DG_BSFC_gkWh  = [600  547  447  427  453];
%  聚合兼容量（legacy 变量名，T-006 改造前过渡期仍被 builder 引用；语义=2×单机）
Gen_P_max   = DG_P_rated_kW * 2;             % 1000 kW 聚合 [DRAWING]
Gen_Ramp_Up   = DG_ramp_kW_per_s * 2;        % 40 kW/s 聚合（=20×2，口径一致）
Gen_Ramp_Down = DG_ramp_kW_per_s * 2;
ASSUMED_Gen_MinStablePower = DG_P_min_kW;    % 125 kW/unit（design §2 override：0→125，物理可信化）
ASSUMED_Gen_TimeConstant   = DG_tau_s;       % 5 s（与单机一致）
ASSUMED_eta_rect = 0.97;   % 发电机整流效率 INHERITED_LEGACY

%% ------------------------------------------------------------------------
%  6. 电池 LFP 1RC ECM + SOP（FR-422/423/424；顾问参数集 v1，2026-09-04 冻结）
%  ------------------------------------------------------------------------
%  【chemistry 声明】LFP 为 literature surrogate（项目资料未确认实船化学体系，
%  任何文档不得表述为"目标船采用 LFP"）。cell→pack 缩放：K2 LFP26650P（2.6 Ah/3.2 V，
%  Tran 2021 [12] HPPC 拟合 + Pérez Vidal 2011 [13] OCV）按 N_s=576/3.2=180、
%  N_p=1860/2.6=715.38 连续等效缩放：R_pack=R_cell·N_s/N_p，C_pack=C_cell·N_p/N_s。
Bat_Q_nom_Ah  = 1860;    % pack 标称容量 [Ah]（576 V × 1860 Ah ≈ 1071.36 kWh，与 DRAWING 一致）
Bat_eta_coul  = 0.98;    % 库仑效率 [-]（仅进 SOC 计量；LITERATURE_NOMINAL [14]）
Bat_V_min_V   = 360;     % 绝对电压下限 [V]（cell 2.00 V × 180s）LITERATURE_NOMINAL [12]
Bat_V_max_V   = 657;     % 绝对电压上限 [V]（cell 3.65 V × 180s）
%  九节点 SOC-LUT（pack 级 DERIVED；升序；域 [0.1, 0.9]，域外 clamp + ecm_lut_boundary_flag）
Bat_SOC_LUT_nodes = 0.1:0.1:0.9;
Bat_OCV_LUT_V  = [541.08 578.68 585.99 591.12 592.87 593.42 594.97 600.12 600.55];
Bat_R0_LUT_ohm = [0.00866 0.00815 0.00770 0.00745 0.00715 0.00702 0.00682 0.00662 0.00632];
Bat_R1_LUT_ohm = [0.01887 0.01225 0.00986 0.00923 0.00798 0.00785 0.00722 0.00883 0.00705];
Bat_C1_LUT_F   = [1697 2070 2254 2458 2579 2733 2862 2969 3058];
Bat_R0_chg_mult = 1.0;   % 充放电共用 R_0(SOC)（2026-09-04 T-013 裁决：charging gain
                         % ×1.15 无具体文献/实验支撑——顾问参数表 v1 仅表注一行，Tran 2021
                         % HPPC [12] 未单列充电方向拟合；暂不人为引入，待真实 BMS 数据）
%  功率/电流边界（OQ-9 定稿：1050 kW = bus-side 图纸约束，作为 SOP 上界之一）
ASSUMED_eta_dcdc = 0.97;             % DC/DC 效率 INHERITED_LEGACY（保持）
Bat_P_conv_bus_max_kW  = 1050;       % DC/DC 母线侧额定 [kW] DRAWING（20260526设备参数）
Bat_P_term_dis_max_kW  = 1050 / 0.97; % 放电端子侧换算上限 ≈1082.5 kW DERIVED（方向感知）
Bat_P_term_chg_max_kW  = 1050 * 0.97; % 充电端子侧换算上限 ≈1018.5 kW DERIVED
Bat_I_limit_surrogate_A = 1823;      % 电流约束 surrogate [A] DERIVED（=1050 kW/576 V，
                                     %  非独立 BMS 电流额定，论文不得表述为实测电流上限）
ASSUMED_Bat_DischargeLimit = 1050;   % 兼容别名（旧端口约束引用点，Plant 侧由 SOP 接管）
ASSUMED_Bat_ChargeLimit    = 1050;
%  SOC 域与降额族（INHERITED_LEGACY；SOP 的 P_SOC 约束沿用）
ASSUMED_SOC_min             = 0.10;
ASSUMED_SOC_max             = 0.95;
ASSUMED_SOC_DisDerate_Start = 0.25;  % SOC≤0.25 线性降额 → SOC_min 归零
ASSUMED_SOC_ChgDerate_Start = 0.90;  % SOC≥0.90 线性降额 → SOC_max(0.95) 归零
% [T-013 裁决 2026-09-04] SOC 降额族 provenance 升级：ENGINEERING_SURROGATE
% （保守 BMS/SOP surrogate safety envelope，沿用 Model D 降额族数值；Model E
%  独立理由=电池绝对边界+DC/DC 约束的保守运行包络；待真实 BMS 策略数据重评）
Bat_SOC_init = 0.60;                 % 初始 SOC INHERITED_LEGACY
%  【Q5 裁决】旧端口效率 0.95 不再乘到电池端口功率（ECM 内阻显式损耗已含）。
%  ASSUMED_Bat_Eff_* 定义保留（ECMS k_eq 派生引用，INHERITED_LEGACY），Plant 侧停用：
ASSUMED_Bat_Eff_Discharge = 0.95;    % INHERITED_LEGACY（仅 ECMS k_eq 引用，待 B/F 包重评）
ASSUMED_Bat_Eff_Charge    = 0.95;    % INHERITED_LEGACY（同上）

%% ------------------------------------------------------------------------
%  7. 直流母线电气层（INHERITED_LEGACY，Model D 口径不变）
%  ------------------------------------------------------------------------
V_dc_nom = 750;              % 母线额定 [V] DRAWING
V_bat_nom = 576;             % 电池簇标称电压 [V] DRAWING（与 ECM LUT 的 576 V 缩放基准一致）
DRAWING_N_PropStrings = 2;   % 推进支路数 DRAWING
DRAWING_Bat_Cap_kWh_perString = 535.68;  % [kWh/支路] DRAWING
DRAWING_Gen_P_max_perString = 500;       % [kW/支路] DRAWING
DRAWING_Mot_P_nom_perString = 350;       % [kW/支路] DRAWING
Bat_Cap_kWh = DRAWING_Bat_Cap_kWh_perString * DRAWING_N_PropStrings; % 1071.36 kWh
Mot_T_max = (Mot_P_nom*1000) / (2*pi*Mot_RPM_nom/60);  % 聚合电机最大转矩 [Nm] 派生
ASSUMED_C_bus   = 40;        % 母线等效电容 [F] INHERITED_LEGACY
ASSUMED_K_droop = 10;        % 电池 DC/DC 下垂系数 [kW/V] INHERITED_LEGACY
ASSUMED_V_floor = 50;        % 功率-电流换算电压下限保护 [V] INHERITED_LEGACY
CCS_Vdc_Steady_Band    = V_dc_nom * [0.90 1.06];  % [675 795] V（CCS 2026）
CCS_Vdc_Transient_Band = V_dc_nom * [0.80 1.07];  % [600 802.5] V
CCS_t_recovery_max     = 1.5;   % [s]

%% ------------------------------------------------------------------------
%  8. EMS 七变体内部所需参数（INHERITED_LEGACY，数值不变，待 B/F 包重评）
%  ------------------------------------------------------------------------
ASSUMED_SpeedPI_Kp     = 600;   % 船速 PI [kW/(m/s)]
ASSUMED_SpeedPI_Ki     = 20;    % 船速 PI [kW/(m/s)/s]
ASSUMED_Speed_Ramp_Up  = 0.05;  % 目标航速斜坡 [m/s^2]
ASSUMED_Drive_TimeConstant = 0.5;  % 推进驱动滞后 [s]
ASSUMED_EMS_SOC_Ref    = 0.60;
ASSUMED_EMS_SOC_Gain   = 200;   % [kW/SOC]
ASSUMED_EMS_FF_Gain    = 1.0;
ASSUMED_EMS_FF_Max_kW  = 400;
ASSUMED_Pred_Bound_kW  = 2000;
ASSUMED_TFF_TauMargin      = 10;
ASSUMED_TFF_SOC_Thresh     = 0.40;
ASSUMED_TFF_SOC_AdvancePct = 0.25;
ASSUMED_EMS_Manual_Pgen_kW = 400;
%  ECMS 聚合 BSFC 曲线（law=4/5/6 变体内部常数源，ensure_bsfc_params 注入链第 1 级；
%  与 Plant 侧 per-genset PCHIP（§5）并存——EMS 内部口径不动（FR-426/D-9），待 B 包 PMS v2 重评）
ASSUMED_BSFC_Breakpoints = [0 100 200 400 600 800 1000];
ASSUMED_BSFC_Table       = [1200 700 550 480 450 430 453];
BSFC_Breakpoints_kW = ASSUMED_BSFC_Breakpoints;   % ensure_bsfc_params 注入链第 1 级直接命中
BSFC_Table_gkWh     = ASSUMED_BSFC_Table;
ASSUMED_BSFC_Avg_gkWh = mean(ASSUMED_BSFC_Table(3:end));      % ≈474 g/kWh
ASSUMED_k_eq_FEC = ASSUMED_BSFC_Avg_gkWh / (1000 * ASSUMED_Bat_Eff_Charge);  % ≈0.499 kg/kWh
%  ECMS Phase 1（T-103~T-106 标定值，law=4/5/6）
ECMS_Lambda        = 0.425;   % [kg/kWh] Fixed ECMS（T-104 标定）
ECMS_K_soc         = 30;      % [kg/kWh/SOC]（T-105 标定）
ECMS_K_S           = 0.01;    % [-] law=6 v2
ECMS_Lambda_Floor  = 0.43;    % [kg/kWh] law=6 v2 下限钳位
ECMS_SOC_Ref           = 0.60;
ECMS_SOC_Discharge_Max = 0.105;
ECMS_SOC_Charge_Min    = 0.945;
ECMS_Idle_Fuel_kgph    = 6.795;   % 怠速油耗 [kg/h]（assumed，Plant 侧 COOLDOWN fuel=0，仅 EMS 内部口径）
ECMS_Grid_dP_kW        = 20;
ECMS_dt_ctrl_s         = 1;
ECMS_EHat_Data    = [0 0 0; 100000 0 0];  % law=6 oracle 前瞻信号占位
ECMS_FLEET_SEED   = 20260825;
%  Phase 2 NN-A-ECMS 占位（law=7；extrinsic 路径运行期自 load config/ecms_nn_weights.mat，
%  mat 缺失 → ê=0 ≡ law=5 回退（FR-208），模型编译不失败。占位形状与 legacy 完全一致）
ECMS_NN_WeightsOK = 0;
ECMS_NN_W1 = zeros(302, 128); ECMS_NN_b1 = zeros(1, 128);
ECMS_NN_W2 = zeros(128, 64);  ECMS_NN_b2 = zeros(1, 64);
ECMS_NN_W3 = zeros(64, 1);    ECMS_NN_b3 = 0;
ECMS_NN_XMean = zeros(302, 1); ECMS_NN_XStd = ones(302, 1);
ECMS_NN_YMean = 0; ECMS_NN_YStd = 1;
ECMS_NN_Ebar_H_kWh  = 36.893333753896165;  % S20 Ē_H（kS_calibration.json，INHERITED_LEGACY）
ECMS_NN_Escale_kWh  = 20.506967855122674;  % S20 E_scale
%  AI/预测接口数据（law=1/2 FromWorkspace 通道占位，INHERITED_LEGACY）
ASSUMED_AI_DataValid = false;
AI_Oracle_Data  = [0 0 0; 100000 0 0];
AI_Pred_Data    = [0 0];
STM32_Pred_Data = [0 0];
AI_Pred_Timeout_s = 2;
%  嵌入式 ML 预测器占位（AI_Pred_Source=3；07 号脚本加载真实权重，INHERITED_LEGACY）
MLP_WeightsLoaded = 0;
MLP_W1 = zeros(300, 64); MLP_b1 = zeros(1, 64);
MLP_W2 = zeros(64, 32);  MLP_b2 = zeros(1, 32);
MLP_W3 = zeros(32, 1);   MLP_b3 = 0;
MLP_FeatMean = zeros(1, 5); MLP_FeatStd = ones(1, 5);
MLP_YMean = 0; MLP_YStd = 1;

%% ------------------------------------------------------------------------
%  9. 初始状态与工况默认（INHERITED_LEGACY）
%  ------------------------------------------------------------------------
Ship_V_init = 0;
Ship_X_init = 0;
Pgen_init   = 0;
ASSUMED_TargetSpeed    = 5.66;   % [m/s]（11 kn 服务航速；K_R=890 下巡航平衡点仍 ≈11 kn，
                                 %  功率受限极速 ≈14.5 kn——Q4 新口径）
ASSUMED_VoyageDistance = 5000;
ASSUMED_RiverCurrentSpeed = 1.0;
ASSUMED_Resistance_Change_RampTime   = 15;
ASSUMED_Resistance_Change_Amplitude  = 0.4;

%% ------------------------------------------------------------------------
%  10. 参数元数据（reports/parameter_audit.csv 生成源，Model E 口径）
%  ------------------------------------------------------------------------
ParamMeta = {
  'Model_SystemScope'            '-'   'Model 配置(INHERITED)'            'high' '全部' false
  'Hull_Lpp_m'                   'm'   'DERIVED: Rhine86[2]相似缩尺 λ=0.6250' 'medium' '03_Hull/浅水' true
  'Hull_B_m'                     'm'   'DERIVED: 同上'                     'medium' '浅水/阻力' true
  'Hull_T_m'                     'm'   'DERIVED: 同上'                     'medium' 'UKC/H/T' true
  'Hull_Sw_m2'                   'm^2' 'DERIVED: 缩尺换算'                 'medium' '浅水 C_F*' true
  'Hull_CB'                      '-'   'LITERATURE_NOMINAL: 公务船上界[CB-1/2]' 'medium' 'w 回归/Δk' true
  'Ship_Mass'                    'kg'  'DERIVED: 500t 工作假定(Q3 裁决)'    'medium' '03_Hull' true
  'Hull_K_R'                     'N/(m/s)^2' 'DERIVED: 线性Δ缩放(Q4 裁决)'  'medium' '03_Hull' true
  'Wake_w_deep'                  '-'   'LITERATURE_NOMINAL: Molland 回归(OQ-4)' 'medium' '03_Propeller' false
  'Hull_k_form'                  '-'   'LITERATURE_RANGE 1.15~1.30 取中'   'medium' '浅水 Δr' false
  'Shallow_c1'                   '-'   'LITERATURE_NOMINAL: Zhang[1]/Zeng[3]' 'medium' '浅水 C_F*' false
  'Shallow_deltak_coef'          '-'   'LITERATURE_NOMINAL: Millward[4]'   'medium' '浅水 Δk' false
  'Elec_Eff'                     '-'   'LITERATURE_NOMINAL: η_inv×η_mot×η_shaft[5]' 'high' '03_Propulsion' false
  'Gear_Eff'                     '-'   'INHERITED_LEGACY(D-1 保持)'         'medium' '03_转矩链' true
  'K_Fw'                         'N/(rad/s)^2' 'DERIVED: w=0.10 重标定(D-8)' 'medium' '03_Propeller' false
  'K_Fv'                         'N/(m/s)^2'   'DERIVED: 同上'             'medium' '03_Propeller' false
  'K_Qw'                         'Nm/(rad/s)^2' 'DERIVED: 同上'             'medium' '03_Propeller' false
  'K_Qv'                         'Nm/(m/s)'     'DERIVED: 同上'             'medium' '03_Propeller' false
  'DG_P_rated_kW'                'kW'  'VENDOR: WH17ME[16]'                'high' '06_Genset_System' false
  'DG_P_min_kW'                  'kW'  'LITERATURE_NOMINAL: 25%[6]'         'medium' '06 状态机/SOP' true
  'DG_P_opt_kW'                  'kW'  'LITERATURE_NOMINAL: 75%[6]'         'medium' '06' true
  'DG_ramp_kW_per_s'             'kW/s/unit' 'engineering-calibrated[9]'    'low'   '06' true
  'DG_tau_s'                     's'   'engineering-calibrated'             'low'  '06' true
  'DG_t_starting_s'              's'   'LITERATURE_NOMINAL: Perabo[7]'      'medium' '06 状态机' false
  'DG_t_sync_s'                  's'   'LITERATURE_NOMINAL: Jeon[8]'        'medium' '06 状态机' false
  'DG_t_cooldown_s'              's'   'surrogate（怠速油耗未计）'           'low'  '06 状态机' true
  'DG_C_start_kg'                'kg/start' 'DERIVED: 125kW×0.6×30/3600'    'medium' '10_Fuel' true
  'DG_BSFC_gkWh'                 'g/kWh' 'LITERATURE_NOMINAL/DERIVED: [11]形状×[16]锚定' 'medium' '10_Fuel PCHIP' true
  'Bat_Q_nom_Ah'                 'Ah'  'DERIVED: 930Ah×2 pack 等效'         'medium' '14_Battery' true
  'Bat_eta_coul'                 '-'   'LITERATURE_NOMINAL: [14]'           'medium' '14 SOC 积分' false
  'Bat_OCV_LUT_V'                'V'   'DERIVED: K2 LFP26650P[13] 缩放'     'medium' '14 ECM' false
  'Bat_R0_LUT_ohm'               'Ohm' 'DERIVED: HPPC[12] pack 缩放'        'medium' '14 ECM' true
  'Bat_R1_LUT_ohm'               'Ohm' 'DERIVED: 同上'                      'medium' '14 ECM' true
  'Bat_C1_LUT_F'                 'F'   'DERIVED: 同上'                      'medium' '14 ECM' true
  'Bat_I_limit_surrogate_A'      'A'   'DERIVED: 1050kW/576V(非 BMS 实测)'  'medium' 'SOP' true
  'Bat_P_conv_bus_max_kW'        'kW'  'DRAWING: DC/DC 750V×700A×2'         'high' 'SOP' false
  'ASSUMED_Gen_MinStablePower'   'kW'  'LITERATURE_NOMINAL: 25%(A2 裁决)'   'medium' '06' true
  'ASSUMED_Bat_Eff_Discharge'    '-'   'INHERITED_LEGACY(Plant 侧停用,Q5)'  'low'  'ECMS k_eq' true
  'ASSUMED_Bat_Eff_Charge'       '-'   'INHERITED_LEGACY(Plant 侧停用,Q5)'  'low'  'ECMS k_eq' true
  'WaterDepth_default_m'         'm'   'Model E 工况默认(深水渐近)'          'high' '01_Scenario' false
};

fprintf('[ModelE_Physics_Parameters] Model E 参数事实源已载入（独立成套，无 legacy 依赖）\n');
fprintf('  500 t hull: Lpp=%.2f B=%.2f T=%.3f C_B=%.2f | K_R=%.0f | Ship_Mass=%.1e kg\n', ...
    Hull_Lpp_m, Hull_B_m, Hull_T_m, Hull_CB, Hull_K_R, Ship_Mass);
fprintf('  螺旋桨重标定(w_deep=%.2f): K_Fw=%.2f K_Fv=%.1f K_Qw=%.2f K_Qv=%.1f\n', ...
    Wake_w_deep, K_Fw, K_Fv, K_Qw, K_Qv);
fprintf('  双机组 2×%.0f kW (P_min=%.0f P_opt=%.0f ramp=%.0f τ=%.0f C_start=%.2f kg) | BSFC PCHIP 5 节点\n', ...
    DG_P_rated_kW, DG_P_min_kW, DG_P_opt_kW, DG_ramp_kW_per_s, DG_tau_s, DG_C_start_kg);
fprintf('  电池 1RC: Q_nom=%.0f Ah, OCV[0.1~0.9]=[%.1f~%.1f] V, R_0=[%.2f~%.2f] mΩ, η_coul=%.2f\n', ...
    Bat_Q_nom_Ah, min(Bat_OCV_LUT_V), max(Bat_OCV_LUT_V), min(Bat_R0_LUT_ohm)*1e3, max(Bat_R0_LUT_ohm)*1e3, Bat_eta_coul);
fprintf('  参数已加载：%d 项 ParamMeta（含 target_vessel_confirmed=false 声明）\n', size(ParamMeta, 1));
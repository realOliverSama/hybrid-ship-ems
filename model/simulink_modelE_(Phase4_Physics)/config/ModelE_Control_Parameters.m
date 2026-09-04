%% ========================================================================
%  ModelE_Control_Parameters.m — Model E 控制侧参数事实源（Phase 4-B, FR-451）
%  spec: .specs/phase4-b-pms-safety（brainstorm D-B1~D-B9 + requirements FR-451/451A
%        + design §3/§5/§8，全部参数值以 spec 原文为准，本文件不得重议数值）
%  纪律：
%    - 本文件只承载**控制/安全侧**参数；Plant 物理事实源是 config/ModelE_Physics_Parameters.m
%      （NFR-412：物理参数文件零改动，本文件与其互不覆盖）；
%    - 加载顺序：必须在 ModelE_Physics_Parameters.m **之后**运行（builder/40/41/42/
%      43/44/run_scenario_sim 各入口保证此顺序；本文件头部 assert 兜底）；
%    - canonical 功率域 = genset-side（generator-equivalent，DD-1）；域换算常数
%      直接引用物理事实源变量（ASSUMED_eta_rect / DG_P_rated_kW / DG_P_min_kW /
%      Mot_P_nom），禁止在本文件复制数值；
%    - provenance：全参数 ENGINEERING_SURROGATE（除注明 LITERATURE-ANCHORED），
%      措辞纪律按 NFR-418（禁虚构 OEM 惯例、禁普适化表述）；
%    - 敏感性档位一律**冻结登记不扫**（FR-451；Q1 裁决口径延续）。
%% ========================================================================

% ---- 加载顺序兜底：物理事实源必须已加载（DERIVED 阈值引用其变量） ----
if ~exist('DG_P_rated_kW', 'var') || ~exist('ASSUMED_eta_rect', 'var')
    error('ModelE_Control_Parameters: 必须在 ModelE_Physics_Parameters.m 之后运行（引用 DG_P_rated_kW/ASSUMED_eta_rect）');
end

%% ---- PMS Baseline Demand Policy（FR-432~435, brainstorm D-B3）----
PMS_LoadLPF_tau_s     = 60;    % [s] 负荷一阶低通时间常数 ENGINEERING_SURROGATE（D-B3）；敏感性注册 {30,120} 不扫
PMS_SOC_lo            = 0.55;  % [-] recovery 进入阈值，比较符号 SOC<=PMS_SOC_lo（含等号，FR-433）
PMS_SOC_recovered     = 0.60;  % [-] recovery 退出阈值（=SOC_init，charge-sustaining 口径），SOC>=PMS_SOC_recovered 出
PMS_P_chg_recovery_kW = 100;   % [kW] generator-equivalent recovery bias（≈0.09 C；bus 侧新增发电约 97 kW，
                               %      措辞纪律：battery genset-equivalent ≠ 物理过 rectifier，DD-1）

%% ---- DG Commitment（FR-436~440, brainstorm D-B4/D-B5）----
PMS_alpha_start = 0.85;         % [-] 启动门槛系数（×DG_P_rated_kW=425 kW）
                                %     LITERATURE-ANCHORED ENGINEERING_SURROGATE：
                                %     Siemens 默认 85% 门槛 × Perabo ITEC 2020 HIL 30/120 s 判定延迟的
                                %     **组合 surrogate**（Perabo 原表 60%/30 s 启、25%/120 s 停；
                                %     Siemens 延迟 5 s 不适用混动船——禁写成任一单一直引，NFR-418）
PMS_alpha_stop  = 0.70;         % [-] 停机门槛系数（remaining-unit 口径，×DG_P_rated_kW=350 kW；非 0.70×1000）
T_start_hold_s  = 30;           % [s] 启动判定持续时间（D-B4）
T_stop_hold_s   = 120;          % [s] 停机判定持续时间（D-B4）
PMS_min_up_s    = 300;          % [s] anti-short-cycle backstop ENGINEERING_SURROGATE（D-B5；
                                %     禁写"OEM 普遍 15–30 min"——无可靠证据）；与 stop-hold 并行计时
                                %     （AND 汇合，最短 ON=恰 300 s）；敏感性注册 {0,300,600} 不扫
PMS_min_down_s  = 0;            % [s] D-B5：无最小停机（425+30 s hold+75 kW 滞回已防 restart 抖动）；不注册敏感性

%% ---- Safety / Interlock（FR-444~448, brainstorm D-B7/D-B9）----
PMS_firm_margin_kW      = 50;   % [kW] S4 prospective firm-capacity 余度（genset 域字面，DD-1）；注册 {0,100} 不扫
PMS_S2_hold_s           = 2;    % [s] S2 realized supply deficit 持续判据（D-B14 评审口径）
PMS_S4_hold_s           = 2;    % [s] S4 持续判据（D-B9）
PMS_start_fail_window_s = 40;   % [s] S1b 启动失败窗（>25+5 s 物理时序 + 裕量，D-B7）

%% ---- Manual 推进指令（FR-429, design DD-3/§5.2）----
Manual_Cmd_RateLimit_pct_s = 20;  % [%/s] command-side rate limiter（满行程 5 s）ENGINEERING_SURROGATE；
                                  %       折算 kW/s = 0.2×Mot_P_nom/s，与 DG 20 kW/s 物理 ramp 完全独立（FR-429）
Manual_Cmd_RateLimit_kW_per_s = Manual_Cmd_RateLimit_pct_s / 100 * Mot_P_nom;  % 140 kW/s（DERIVED，引用 Mot_P_nom 不复制）

%% ---- 编译期选择器（FR-450 source selector / FR-428 ctrl_mode）----
PMS_SOURCE_MODE = 1;            % 0=BASELINE_POLICY 1=EMS_AGGREGATE（nominal source；
                                %     S5 fallback 可 runtime override：ems_health_ok==0 → BASELINE_POLICY）
VOYAGE_CTRL_MODE = 0;           % 0=AUTO（既有速度 PI 原样）1=MANUAL（归一化功率指令）；
                                %     单个仿真工况内模式固定（FR-428，编译期）

%% ---- 离散 tick 换算（DERIVED；模型固定步 0.01 s——与 GensetUnitE dt 硬编码同一口径，
%        若模型固定步长变更，本节与 N_ticks_* 须一并重推，A2 chain_b_integration_notes）----
PMS_TS_S             = 0.01;                                     % [s] 决策/逻辑离散步
T_start_hold_ticks   = round(T_start_hold_s         / PMS_TS_S); % 3000 tick
T_stop_hold_ticks    = round(T_stop_hold_s          / PMS_TS_S); % 12000 tick
PMS_min_up_ticks     = round(PMS_min_up_s           / PMS_TS_S); % 30000 tick
PMS_S2_hold_ticks    = round(PMS_S2_hold_s          / PMS_TS_S); % 200 tick
PMS_S4_hold_ticks    = round(PMS_S4_hold_s          / PMS_TS_S); % 200 tick
PMS_start_fail_ticks = round(PMS_start_fail_window_s / PMS_TS_S); % 4000 tick

%% ---- 阈值 DERIVED（genset 域字面，引用物理事实源；禁止在此改写额定值）----
PMS_start_threshold_kW = PMS_alpha_start * DG_P_rated_kW;   % 425 kW（N_dispatch 单机/滞回上界）
PMS_stop_threshold_kW  = PMS_alpha_stop  * DG_P_rated_kW;   % 350 kW（remaining-unit 口径滞回下界）

%% ---- 敏感性注册表（FR-451：冻结登记，本 spec 不扫掠）----
%   PMS_LoadLPF_tau_s  ∈ {30, 120}
%   PMS_alpha_start    ∈ {0.80, 0.85, 0.90}
%   PMS_alpha_stop     ∈ {0.60, 0.70, 0.80}
%   PMS_min_up_s       ∈ {0, 300, 600}
%   PMS_firm_margin_kW ∈ {0, 100}
%   （PMS_min_down_s 不注册——D-B5；DG 侧 {125,200,250} 等物理敏感性见物理参数文件）

%% ---- 注入通道表（场景数据，From Workspace + 'Interpolate','off'）----
%   GEN_CMD_STOP_TS   —— 既有（ModelE_TestScenarios.m 定义，S1 注入复用，FR-448）
%   EMS_HEALTH_TS     —— 新增 S5 健康通道（列 [t, health]；默认全 1 表；T-010 随 16 号块接线落地）
%   VOYAGE_MANUAL_CMD_TS —— 新增 MANUAL 指令序列表（列 [t, manual_power_cmd_pct]；T-006 随 01/02 改组落地）
fprintf('[ModelE_Control_Parameters] PMS v2 控制参数已加载（FR-451；canonical 域=genset-side DD-1；阈值 %.0f/%.0f kW）\n', ...
    PMS_start_threshold_kW, PMS_stop_threshold_kW);
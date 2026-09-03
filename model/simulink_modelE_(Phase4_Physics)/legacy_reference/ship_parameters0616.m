%% 甲醇混合动力内河船舶 HIL 仿真参数表
% 文件名: Ship_Parameters0616.m
% 说明: 在 Simulink 模型运行前加载至 Workspace

%% 1. 直流电网与母线参数 (DC Grid Parameters)
V_bus_nom = 576;        % 直流母排额定电压 [V]
C_bus     = 20;          % 变流柜直流侧等效总支撑电容 [F] 
R_tie     = 0.002;      % 母联接触器与铜排等效接触电阻 [Ohm]

%% 2. 动力电池簇参数 (Battery Pack Parameters)
Bat_Cap_kWh = 535.68;                      % 电池总能量 [kWh]
Bat_Cap_Ah  = Bat_Cap_kWh*1000/V_bus_nom;  % 电池等效安时容量 [Ah] (535680Wh / 576V)
Bat_R_int   = 0.02;                        % 电池簇等效内阻 [Ohm]
Bat_SOC_init= 0.6;                         % 仿真初始 SOC [0~1]
%电池放电曲线一维查表需根据实际电池设置

%% 3. 甲醇发电机组参数 (Methanol Genset Parameters)
Gen_P_max = 500;        % 发电机最大电功率 [kW]
Gen_RPM_nom = 1500;     % 发电机额定转速 [rpm]
Gen_RPM_max = 1700;     % 发电机最大转速 [rpm]
Gen_J = 50;             % 发电机组转子转动惯量 [kg*m^2]
Gen_Fric_B = 0.1;       % 发电机内部机械摩擦系数 [Nm/(rad/s)]
%发电机BSFC曲线一维查表需根据实际发动机设置

% 发电机调速器 (Governor) PI 参数
Gen_PI_P = 50;          
Gen_PI_I = 20;

%% 4. 推进电机与齿轮箱参数 (Propulsion & Gearbox Parameters)
Mot_RPM_nom = 1500;     % 推进电机额定转速 [rpm]
Mot_P_nom = 350;        % 推进电机额定功率 [kW]
Mot_T_max = (Mot_P_nom*1000)/(2*pi*Mot_RPM_nom/60);
                        % 推进电机最大扭矩 [Nm]
Gear_Ratio = 6.08;      % 齿轮箱减速比
Gear_Eff   = 0.96;      % 齿轮箱机械传动效率 (96%)
Elec_Eff   = 0.92;      % 变频器与电机综合电气效率 (92%)

% 推进变频器 (Inverter) PI 参数
Mot_PI_P = 20;
Mot_PI_I = 5;

%% 5. 船舶水动力参数 (Ship Dynamics Parameters)
Prop_k = (Mot_P_nom * Gear_Eff * 1000)/((Mot_RPM_nom/Gear_Ratio)*(2*pi/60))^3;
                    % P_prop = T_prop * ω_prop
                    % P_prop = Mot_P_nom * Gear_Eff
                    % T_prop = k * ω_prop 
                    % -> k = (Mot_P_nom * Gear_Eff) / ω_prop 螺旋桨水阻力系数 [Nm/(rad/s)^2]
Prop_J = 800;       % 轴系与螺旋桨等效总转动惯量 (含附水惯量) [kg*m^2]

%% 5A. 船舶参数

Ship_Mass = 1.0e6;          % 船舶等效质量 [kg]
Ship_V_init = 0;            % 初始对地船速 [m/s]

River_Current_mps = -1.0;   % 河流流速 [m/s]，负数表示逆流
F_env_const = 0;            % 额外环境纵向力 [N]，正向助推，负向阻碍

rho_water = 1000;           % 淡水密度 [kg/m^3]
wake_fraction = 0.15;       % 伴流分数 w，第一版先固定 0.10~0.20
thrust_deduction = 0.10;    % 推力减额 t，第一版先固定 0.05~0.15

V_design_kn = 8;            % 设计航速 [kn]，后面换成真实数据
V_design_mps = V_design_kn / 1.944;

P_shaft_nom_W = Mot_P_nom * Gear_Eff * 1000;
eta_prop_guess = 0.55;      % 推进效率估计值，第一版先取 0.5~0.6
P_effective_design_W = eta_prop_guess * P_shaft_nom_W;

Hull_K_R = P_effective_design_W / V_design_mps^3;

%% 5B. 简单螺旋桨水动力参数
n_prop_nom_rpm = Mot_RPM_nom / Gear_Ratio;
omega_prop_nom = n_prop_nom_rpm * pi / 30;

Q_prop_nom = P_shaft_nom_W / omega_prop_nom;

V_A_design = (1 - wake_fraction) * V_design_mps;

% 设计工况下的有效推进力
F_eff_design = P_effective_design_W / V_design_mps;

% 螺旋桨静推力估计，第一版取设计推力的 1.3~1.6 倍
F_bollard_factor = 1.5;
F_bollard = F_bollard_factor * F_eff_design;

% 推力模型：F_prop_eff = K_Fw*w*abs(w) - K_Fv*Va*abs(Va)
K_Fw = F_bollard / omega_prop_nom^2;
K_Fv = (F_bollard - F_eff_design) / V_A_design^2;

% 扭矩模型：Q_prop = K_Qw*w*abs(w) - K_Qv*Va*abs(Va)
Q_bollard_factor = 1.2;
Q_bollard = Q_bollard_factor * Q_prop_nom;

K_Qw = Q_bollard / omega_prop_nom^2;
K_Qv = (Q_bollard - Q_prop_nom) / V_A_design^2;

Shaft_B = 0;    % 轴承/密封等线性阻尼 [Nm/(rad/s)]，第一版可先设 0

%% 6. EMS 控制策略参数 (EMS Policy Parameters)
EMS_K_droop_bat = 2;    % 电池稳压下垂系数 [kW/V]
EMS_Ramp_Rate   = 20;   % 发电机功率指令最大爬坡率 [kW/s]
EMS_P_Hotel     = 100;  % 全船日用/辅机负载恒定消耗 [kW]

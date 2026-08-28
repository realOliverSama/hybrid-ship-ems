function cfg = Ecms_Phase0_Config(ov)
% Ecms_Phase0_Config — ECMS Phase 0 单一配置入口（T-008/T-009，design §1/§4/§6）
%
%  集中登记全部 Phase 0 配置：场景集、SOC/P 网格、爬坡率、最低稳定负荷档、
%  运行口径、启动代价、效率链、电池容量/功率限制、初始 SOC、终端对称带、
%  怠速燃油、SOC 修正系数（design §5）、收敛检查参数、输出路径。
%
%  用法：
%    cfg = Ecms_Phase0_Config();                       % 全部缺省值
%    cfg = Ecms_Phase0_Config(struct('mode','startstop', ...)) % 逐实例覆盖
%
%  设计红线（design §4，施工前写死，不得在求解器内再次放行）：
%    - SOC 网格 [0.10:0.005:0.95]（171 点）；P 网格 {0（仅启停口径）} ∪
%      [P_min_stable:20:1000] kW；Δt = 1 s
%    - 符号约定：P_bat_dc > 0 = 放电（E −= P_bat/(η_dcdc·η_bat)·Δt/3600），
%      < 0 = 充电（E += (−P_bat)·η_dcdc·η_bat·Δt/3600）
%    - 功率平衡 P_d = η_rect·P_g + P_bat_dc；|P_bat_dc| ≤ 1050 kW
%    - 终端对称带 |SOC_T − SOC₀| ≤ 0.005（SOC₀ = 0.60）
%    - 单步燃油：P_g × bsfc_eff(P_g)/3.6e6（连续口径 P_g=0 用怠速流量折算；
%      启停口径停机 fuel=0，0→1 加 C_start）
%
%  注：怠速燃油缺省 6.795 kg/h 与 T-004 bsfc_family.mat 的
%  fuel_flow_idle_kgph 一致（= 0.03 × 226.5 kg/h 单机额定流量）；
%  dp_problem 构建时以 fam.fuel_flow_idle_kgph 为准并校验一致性。

    % ==================== 缺省配置 ====================
    cfg.scenarios = {'S01_still_water', 'S02_downstream', 'S03_upstream', ...
        'S04_resistance_inc', 'S07_bridge_maneuver', 'S08_shoal_series', ...
        'S09_emergency_sprint', 'S10_current_reversal'};  % FR-007 入选 8 场景

    % ---- 网格（design §4.1）----
    cfg.SOC_min        = 0.10;   % 状态 SOC 下界 [-]
    cfg.SOC_max        = 0.95;   % 状态 SOC 上界 [-]
    cfg.dSOC           = 0.005;  % SOC 网格步长 [-]
    cfg.dP_kW          = 20;     % P 控制网格步长 [kW]
    cfg.P_max_kW       = 1000;   % 聚合机组最大功率 [kW]
    cfg.P_min_stable_set_kW = [0 300 400 500];  % 最低稳定负荷档位（聚合）[kW]
    cfg.dt_s           = 1;      % 决策步长 [s]（剖面已 1 s 重采样）

    % ---- 机组 / 电池 / 效率（design §4.1/§4.3）----
    cfg.R_gen_kWps     = 40;     % 聚合爬坡率 [kW/s]（主矩阵缺省；单因素扫 {20,40,80}）
    cfg.eta_rect       = 0.97;   % 整流器效率
    cfg.eta_dcdc       = 0.97;   % DC/DC 效率
    cfg.eta_bat        = 0.95;   % 电池充放效率
    cfg.Bat_Cap_kWh    = 1071.36;% 聚合电池容量 [kWh]（535.68×2，FR-002）
    cfg.Bat_P_limit_kW = 1050;   % 电池功率限制 [kW]（双向 DC/DC 瓶颈，FR-002）
    cfg.SOC0           = 0.60;   % 初始 SOC（全剖面一致）
    cfg.SOC_terminal_band = 0.005; % 终端对称带 |SOC_T − SOC₀| ≤ band
    cfg.C_start_kg     = 2;      % 启停口径启动代价 [kg/次]（OQ-4 初值，敏感性 {1,2,5}）
    cfg.fuel_flow_idle_kgph = 6.795; % 怠速燃油 [kg/h]（连续口径 P_g=0 折算，design §3.1）

    % ---- 运行口径与 BSFC ----
    cfg.modes          = {'continuous', 'startstop'};
    cfg.bsfc_tiers_d25 = [0.30 0.50 0.70];  % 低负荷恶化档位（与 bsfc_family 一致）
    cfg.tier_default   = 2;        % 缺省中档 d25=+50%
    cfg.use_2d         = false;    % 主链路一维定速曲线（二维 map 仅扩展分析）
    cfg.k_eq_bsfc_gkWh = 427;      % SOC 修正等效系数基准（design §5，bsfc_min）

    % ---- 内置校验阈值（dp_checks）----
    cfg.energy_closure_tol = 1e-3;    % 能量闭合残差 < 0.1%
    cfg.trivial_soc_eps    = 0.005;   % 非平凡性：SOC ≤ 0.10+eps 记为贴下限
    cfg.trivial_frac_thr   = 0.50;    % 贴下限时间占比 > 50% -> trivial_suspect
    cfg.base_check_tol_abs = 1e-4;    % DP ≤ Baseline 比较绝对容差 [kg]
    cfg.base_check_tol_rel = 1e-6;    % DP ≤ Baseline 比较相对容差 [-]
    cfg.base_feas_tol_kW   = 1e-3;    % Baseline 可行性数值容差 [kW]

    % ---- 收敛检查（T-012，S07 网格加密）----
    cfg.convergence = struct( ...
        'dSOC', 0.0025, 'dP_kW', 10, 'fuel_diff_tol', 0.005);

    % ---- 输出路径（FR-017：runs/*.mat 不入库，runs.csv 入库）----
    cfg.output_root = 'results/ecms_phase0';
    cfg.runs_dir    = fullfile(cfg.output_root, 'runs');

    % ---- 版本 ----
    cfg.version = '0.1.0';

    % ==================== 覆盖 ====================
    if nargin >= 1 && ~isempty(ov) && isstruct(ov)
        flds = fieldnames(ov);
        for i = 1:numel(flds)
            cfg.(flds{i}) = ov.(flds{i});
        end
    end

    % ==================== 一致性自检 ====================
    assert(isnumeric(cfg.dSOC) && cfg.dSOC > 0, 'ECMS:CFG', 'dSOC 非法');
    assert(mod(cfg.SOC0 - cfg.SOC_min, cfg.dSOC) < 1e-12, 'ECMS:CFG', ...
        'SOC0 必须落在 SOC 网格上（(SOC0-SOC_min)/dSOC 为整数）');
    assert(mod(cfg.SOC_terminal_band, cfg.dSOC) < 1e-12 + cfg.dSOC*1e-6, ...
        'ECMS:CFG', '终端带必须是 dSOC 的整数倍（保证带内存在网格点）');
    assert(all(cfg.P_min_stable_set_kW >= 0) && all(mod(cfg.P_min_stable_set_kW, cfg.dP_kW) == 0), ...
        'ECMS:CFG', 'P_min_stable 档位必须是 dP_kW 整数倍且 ≥ 0');
    assert(cfg.Bat_P_limit_kW > 0 && cfg.Bat_Cap_kWh > 0, 'ECMS:CFG', '电池参数非法');
    assert(any(abs(cfg.bsfc_tiers_d25 - 0.50) < 1e-9), 'ECMS:CFG', ...
        'bsfc_tiers_d25 必须含中档 0.50（tier_default=2 语义）');
    assert(islogical(cfg.use_2d) || cfg.use_2d == 0 || cfg.use_2d == 1, ...
        'ECMS:CFG', 'use_2d 须为 logical');
end

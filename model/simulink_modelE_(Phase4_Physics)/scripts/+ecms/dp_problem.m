function prob = dp_problem(prof, cfg, fam, tierIdx)
% ecms.dp_problem — DP 问题结构体构建（design §4.1/§4.2，T-008）
%
%  由场景剖面 × 配置 × BSFC 曲线族组合构建单实例 problem struct。
%  所有与时间/控制相关的预计算（单步燃油、电池能量变化、爬坡窗口）在此完成，
%  dp_solve 只做后向归纳与轨迹重建，不含任何配置语义。
%
%  输入:
%    prof     剖面 struct（ecms.load_profile 输出；仅使用 t 与 P_demand_kW，
%              toy 单测可直接构造最小 struct）
%    cfg      实例级配置 struct（Ecms_Phase0_Config 返回，mode 与
%              P_min_stable_kW 为已解析单实例值）
%    fam      T-004 bsfc_family.mat 结构体（tiers + fuel_flow_idle_kgph）
%    tierIdx  BSFC 档位 1..3
%  输出:
%    prob     问题结构体（字段见代码尾部清单）
%
%  设计约束（写死，不在 dp_solve 内再放行）：
%    - 状态 x = (SOC, P_prev)；SOC 网格 cfg.SOC_min:cfg.dSOC:cfg.SOC_max
%    - 控制网格：连续口径 [P_min_stable:20:1000]；
%      启停口径 {0} ∪ [P_min_stable:20:1000]（P=0 ⟺ 停机，on ⟺ P>0）
%    - 爬坡 |P_g − P_prev| ≤ R_gen·Δt 仅约束 on→on（启停口径的 0→1 启动为离散
%      事件 +C_start，1→0 停机为离散事件，均不施加爬坡——设计 §4.2 语义；
%      实现选择已注释登记，见 dp_solve 头注释）
%    - 单步燃油：P_g × bsfc_eff(P_g)/3.6e6 [kg]；连续口径 P_g=0 用怠速流量折算；
%      启停口径 P_g=0（停机）fuel=0
    if nargin < 4 || isempty(tierIdx)
        tierIdx = cfg.tier_default;
    end
    assert(isfield(prof, 't') && isfield(prof, 'P_demand_kW'), ...
        'ECMS:PROB', 'prof 缺少 t/P_demand_kW');
    assert(isfield(cfg, 'mode') && ismember(cfg.mode, cfg.modes), ...
        'ECMS:PROB', 'cfg.mode 必须为 continuous|startstop');
    assert(isscalar(cfg.P_min_stable_kW) && cfg.P_min_stable_kW >= 0, ...
        'ECMS:PROB', 'cfg.P_min_stable_kW 须为标量 ≥ 0');
    assert(isfield(fam, 'tiers') && tierIdx >= 1 && tierIdx <= numel(fam.tiers), ...
        'ECMS:PROB', 'fam/tierIdx 非法');

    prob = struct();
    if isfield(prof, 'scenario_name')
        prob.scenario = prof.scenario_name;
    else
        prob.scenario = 'toy';
    end
    prob.t        = prof.t(:);
    prob.N        = numel(prof.t);
    prob.dt_s     = cfg.dt_s;
    prob.P_demand_kW = double(prof.P_demand_kW(:));

    prob.mode          = cfg.mode;
    prob.P_min_stable  = cfg.P_min_stable_kW;
    prob.P_max         = cfg.P_max_kW;
    prob.R_gen         = cfg.R_gen_kWps;
    prob.eta_rect      = cfg.eta_rect;
    prob.eta_dcdc      = cfg.eta_dcdc;
    prob.eta_bat       = cfg.eta_bat;
    prob.Cap_kWh       = cfg.Bat_Cap_kWh;
    prob.Bat_P_limit   = cfg.Bat_P_limit_kW;
    prob.SOC_min       = cfg.SOC_min;
    prob.SOC_max       = cfg.SOC_max;
    prob.SOC0          = cfg.SOC0;
    prob.SOC_terminal_band = cfg.SOC_terminal_band;
    prob.C_start_kg    = cfg.C_start_kg;
    prob.tierIdx       = tierIdx;

    % ---- 状态网格 ----
    prob.SOC_grid = (cfg.SOC_min : cfg.dSOC : cfg.SOC_max).';
    prob.E_grid_kWh = prob.SOC_grid * cfg.Bat_Cap_kWh;
    prob.Nsoc = numel(prob.SOC_grid);

    % ---- 控制网格（含停机点）----
    if strcmp(cfg.mode, 'continuous')
        Pg = (cfg.P_min_stable_kW : cfg.dP_kW : cfg.P_max_kW).';
    else
        Pg = unique([0; (cfg.P_min_stable_kW : cfg.dP_kW : cfg.P_max_kW).']);
    end
    prob.P_grid   = Pg;
    prob.on_grid  = Pg > 1e-9;   % 启停口径：P=0 ⟺ 停机；连续口径恒 true（P_min=0 时 P=0 为怠速）
    prob.Np       = numel(Pg);

    % ---- 等效一维 BSFC 表（主链路恒等；use2d 扩展预留）----
    use2d = isfield(cfg, 'use_2d') && cfg.use_2d;
    [bpEff, tbEff, metaEff] = ecms.speed_cost_min(fam, tierIdx, use2d);
    % 控制网格点必须落在 BSFC 断点上（P 步长 20 与断点 0:2:1000 对齐，精确查表）
    bsfc_eff = interp1(bpEff, tbEff, Pg, 'linear', 'extrap');
    prob.bsfc_eff_gkWh = bsfc_eff;
    prob.bsfc_method   = metaEff.method;

    % ---- 单步燃油 [kg]（不含启动代价）----
    fuel_step = Pg .* bsfc_eff / 3.6e6;          % P[kW]×BSFC[g/kWh]/3.6e6 = kg/s×dt(1 s)
    if strcmp(cfg.mode, 'continuous')
        % 连续口径 P=0 = 怠速（机组开机）：怠速流量折算；与 T-004 一致校验
        assert(abs(cfg.fuel_flow_idle_kgph - fam.fuel_flow_idle_kgph) < 1e-6, ...
            'ECMS:PROB', 'cfg 怠速燃油与 bsfc_family 不一致');
        fuel_step(Pg == 0) = cfg.fuel_flow_idle_kgph / 3600 * cfg.dt_s;
    else
        fuel_step(Pg == 0) = 0;                  % 停机 fuel = 0
    end
    prob.fuel_step_kg = fuel_step;

    % ---- 预计算电池侧功率 / 存储能量变化 / 可行性（N × Np）----
    Pbat    = zeros(prob.N, prob.Np);
    dE      = zeros(prob.N, prob.Np);
    Pbat_ok = false(prob.N, prob.Np);
    for g = 1:prob.Np
        Pbat(:, g) = prob.P_demand_kW - prob.eta_rect * Pg(g);   % 功率平衡
        dE(:, g)   = ecms.bat_energy_delta_kWh(Pbat(:, g), cfg.eta_dcdc, cfg.eta_bat, cfg.dt_s);
        Pbat_ok(:, g) = abs(Pbat(:, g)) <= cfg.Bat_P_limit_kW + 1e-9;
    end
    prob.Pbat_kW = Pbat;
    prob.dE_kWh  = dE;
    prob.Pbat_ok = Pbat_ok;

    % ---- 爬坡窗口（on→on 允许的控制索引半径）----
    prob.w = floor(cfg.R_gen_kWps * cfg.dt_s / cfg.dP_kW);   % |ΔP| ≤ R·Δt 的网格索引半径

    % ---- 终端带内 SOC 网格索引 ----
    prob.term_in_band = abs(prob.SOC_grid - cfg.SOC0) <= cfg.SOC_terminal_band + 1e-12;
    prob.iSOC0 = find(abs(prob.SOC_grid - cfg.SOC0) < 1e-12, 1);
    assert(~isempty(prob.iSOC0), 'ECMS:PROB', 'SOC0 不在网格上');
end

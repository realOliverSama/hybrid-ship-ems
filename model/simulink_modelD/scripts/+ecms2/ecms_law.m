function Pg_star = ecms_law(P_demand, SOC, lambda, params, law, e_hat)
% ecms2.ecms_law — ECMS 单步决策核心（design §3.1/§3.2，T-103~T-106，FR-104~107）
%
%   P_g* = argmin_{P_g ∈ 可行集} [ mf_f(P_g) + lam(t) . mbe_bat,eq(P_g) ]
%   可行集 = {0（怠速 mf_f = 6.795/3600 kg/s）} ∪ [0 : 20 : 1000] kW（P_min=0 口径）
%   SOC 硬约束（优先于 λ，design §3.1）：
%     SOC ≤ 0.105 → 禁放电：P_g 取满足需求（P_bat ≤ 0）的最小可行网格点；
%                  需求超过 η_rect·P_max（无网格点可满足）时取 P_max 尽力供载。
%     SOC ≥ 0.945 → 禁充电：从 argmin 候选集排除充电（P_bat < 0）网格点。
%   λ 单位 kg/kWh；P_bat = P_demand − η_rect·P_g，符号与 Phase 0 DP 一致。
%   law=4（Fixed ECMS，T-103）：λ(t) = λ（常数输入）。
%   law=5（SOC-Adaptive ECMS，T-105，design §3.2）：λ(t) = λ + K_soc·(SOC_ref − SOC)，
%     其中 λ 为 λ₀（ECMS_Lambda），K_soc=params.k_soc，SOC_ref=params.soc_ref=0.60。
%   law=6（Oracle Predictive A-ECMS，T-106，design §3.2 + Amendment Log v1.1）：
%     v2（定案）：预测改走 SOC_ref 通道，不再直接加在 λ 上——
%       SOC_ref_eff(t) = SOC_ref + K_S·ê_future(t)（K_S=params.k_s，ECMS_K_S）
%       λ(t) = λ₀ + K_soc·(SOC_ref_eff(t) − SOC(t))
%       λ_eff 下限钳位：λ_eff = max(λ_eff, params.lambda_floor)（0.43，悬崖 0.41+0.02 裕量，
%       防发电塌缩支路；机理：预测经已标定 K_soc 通路作用，避免直接戳 λ 悬崖）
%     ê_future 由 +ecms2/oracle_future_energy.m 对真实未来需求剖面离线预计算
%     （(E_future_H(t)−Ē_H)/E_scale，H=300 s）。
%     **红线（E5，FR-107）**：oracle 前瞻信息非实时可实现，law=6 仅为上界/增量验证工具。
%     （v1 直接加法 K_E·ê 已于 S20 标定证伪——透支电池致 fuel_corr 劣化，见 kE_calibration.json）
%
%   Simulink law=4 变体（EMS_Fixed_ECMS）、law=5 变体（EMS_SOC_Adaptive_ECMS）
%   与 law=6 变体（EMS_Oracle_ECMS）的 MATLAB Function 块内联同一算法
%   （脚本 CODE_ECMS_FIXED/CODE_ECMS_ADAPTIVE/CODE_ECMS_ORACLE），
%   逐位一致性由 +ecms2/test_ecms_law.m 单测 5/7/9（容差 0）锁定。
%
%  输入:
%    P_demand  需求功率 [kW]（模型内取 P_load_total，与 Baseline 同一信号）
%    SOC       电池荷电状态 [-]（law=5 时用于 λ 自适应）
%    lambda    λ₀ 常数 [kg/kWh]（ECMS_Lambda；缺省用 params.lambda）
%    params    可选 ecms2.ecms_params() 输出（缺省自动解析）
%    law       变体序号：4=Fixed ECMS（缺省），5=SOC-Adaptive ECMS，6=Oracle Predictive A-ECMS
%    e_hat     ê_future(t) 无量纲前瞻信号（law=6 专用；缺省 0，law=4/5 忽略）
%  输出:
%    Pg_star   最优发电机功率设定 [kW]（供 06_Genset 爬坡/惯性环节）
%
%  用法: Pg = ecms2.ecms_law(700, 0.60, 0.425);           % law=4
%        Pg = ecms2.ecms_law(700, 0.70, 0.425, p, 5);     % law=5（SOC 高于 ref）
%        Pg = ecms2.ecms_law(700, 0.60, 0.425, p, 6, 1.5); % law=6 v2（E_future 高于均值 -> SOC_ref 升）
    if nargin < 4 || isempty(params)
        params = ecms2.ecms_params();
    end
    if nargin < 3 || isempty(lambda)
        lambda = params.lambda;
    end
    if nargin < 5 || isempty(law)
        law = 4;
    end
    if nargin < 6 || isempty(e_hat)
        e_hat = 0;
    end
    if law == 5 || law == 6
        % design §3.2：SOC 高于 SOC_ref -> λ 下降（电池更便宜）；低于 -> λ 上升
        soc_ref_eff = params.soc_ref;
        if law == 6
            % design v1.1（T-106 v2）：预测经 SOC_ref 通道作用——ê>0（未来高需求）->
            % SOC_ref 升 -> λ 升 -> 提前保留/回充电量（保电方向）
            soc_ref_eff = params.soc_ref + params.k_s * e_hat;
        end
        lambda = lambda + params.k_soc * (soc_ref_eff - SOC);
        if law == 6
            % design v1.1：λ_eff 下限钳位（悬崖 0.41 + 0.02 裕量，防发电塌缩支路）。
            % 仅当 oracle 通道激活（K_S≠0）时生效——K_S=0 时 law=6 ≡ law=5（方案 A 收尾语义）
            if params.k_s ~= 0
                lambda = max(lambda, params.lambda_floor);
            end
        end
    end
    grid = params.grid_kW;
    N = numel(grid);
    Pbest = 0;
    Jbest = inf;
    for i = 1:N
        Pg = grid(i);
        Pbat = P_demand - params.eta_rect * Pg;
        % SOC 硬约束：禁放电（P_g 取满足需求的最小可行值）
        if SOC <= params.SOC_dis_max
            if Pbat > 1e-9
                continue;
            end
            Pbest = Pg;
            break;
        end
        % SOC 硬约束：禁充电（排除充电网格点）
        if SOC >= params.SOC_chg_min && Pbat < 0
            continue;
        end
        [mf, mbe] = ecms2.ecms_cost_parts(Pg, P_demand, params);
        J = mf + lambda * mbe;
        if J < Jbest
            Jbest = J;
            Pbest = Pg;
        end
    end
    % 禁放电且需求 > η_rect·P_max（无网格点可满足）-> 尽力最大
    if SOC <= params.SOC_dis_max && Pbest < 1e-9 && ...
            P_demand > params.eta_rect * grid(end)
        Pbest = grid(end);
    end
    Pg_star = Pbest;
end

function Pg_star = ecms_law(P_demand, SOC, lambda, params)
% ecms2.ecms_law — Fixed ECMS 单步决策核心（design §3.1，T-103，FR-104）
%
%   P_g* = argmin_{P_g ∈ 可行集} [ ṁ_f(P_g) + λ · ṁ_bat,eq(P_g) ]
%   可行集 = {0（怠速 ṁ_f = 6.795/3600 kg/s）} ∪ [0 : 20 : 1000] kW（P_min=0 口径）
%   SOC 硬约束（优先于 λ，design §3.1）：
%     SOC ≤ 0.105 → 禁放电：P_g 取满足需求（P_bat ≤ 0）的最小可行网格点；
%                  需求超过 η_rect·P_max（无网格点可满足）时取 P_max 尽力供载。
%     SOC ≥ 0.945 → 禁充电：从 argmin 候选集排除充电（P_bat < 0）网格点。
%   λ 单位 kg/kWh；P_bat = P_demand − η_rect·P_g，符号与 Phase 0 DP 一致。
%
%   Simulink law=4 变体（EMS_Fixed_ECMS）的 MATLAB Function 块内联同一算法
%   （脚本 CODE_ECMS_FIXED），逐位一致性由 +ecms2/test_ecms_law.m 单测 5
%   （容差 0）锁定。离线实现与块实现互相独立可交叉验证。
%
%  输入:
%    P_demand  需求功率 [kW]（模型内取 P_load_total，与 Baseline 同一信号）
%    SOC       电池荷电状态 [-]
%    lambda    λ 常数 [kg/kWh]（ECMS_Lambda；缺省用 params.lambda）
%    params    可选 ecms2.ecms_params() 输出（缺省自动解析）
%  输出:
%    Pg_star   最优发电机功率设定 [kW]（供 06_Genset 爬坡/惯性环节）
%
%  用法: Pg = ecms2.ecms_law(700, 0.60, 0.4774);
    if nargin < 4 || isempty(params)
        params = ecms2.ecms_params();
    end
    if nargin < 3 || isempty(lambda)
        lambda = params.lambda;
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

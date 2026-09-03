function [mf_kgps, mbe_kgps] = ecms_cost_parts(Pg, P_demand, p)
% ecms2.ecms_cost_parts — ECMS 单步成本分量（design §3.1，T-103，FR-104/FR-108）
%
%   ṁ_f(P_g)      = P_g · bsfc(P_g) / 3600 / 1000      [kg/s]
%                    （P_g=0 怠速：ṁ_f = idle_fuel_kgph/3600）
%   ṁ_bat,eq(P_g) = P_bat / (η_dcdc·η_bat) / 3600       P_bat > 0（放电，正代价）
%                  = P_bat · η_dcdc · η_bat / 3600      P_bat < 0（充电，负代价=收益）
%   P_bat         = P_demand − η_rect · P_g             [kW]
%
%  符号约定与 ecms.bat_energy_delta_kWh（Phase 0 DP）一致：P_bat>0 放电。
%  效率链与 dp_problem 的 cfg.eta_rect/eta_dcdc/eta_bat 一致。
%  BSFC 线性插值为手工实现（clamp + 线性），与 Simulink law=4 块内联代码
%  逐位一致（+ecms2/test_ecms_law.m 单测 4/5 锁定）。
%
%  输入: Pg 发电机功率网格点 [kW]；P_demand 需求 [kW]；p ecms2.ecms_params()
%  输出: mf_kgps 燃油流量 [kg/s]；mbe_kgps 电池等效流量 [kg/s]（放电正、充电负）。
    if nargin < 3 || isempty(p)
        p = ecms2.ecms_params();
    end
    Pbat = P_demand - p.eta_rect * Pg;

    % ---- ṁ_f ----
    if Pg < 1e-9
        mf_kgps = p.idle_fuel_kgph / 3600;
    else
        bsfc = p.bsfc_tbl_gkWh(1);
        nb = numel(p.bsfc_bp_kW);
        if Pg >= p.bsfc_bp_kW(nb)
            bsfc = p.bsfc_tbl_gkWh(nb);
        elseif Pg > p.bsfc_bp_kW(1)
            ii = 1;
            while ii < nb - 1 && p.bsfc_bp_kW(ii + 1) < Pg
                ii = ii + 1;
            end
            w = (Pg - p.bsfc_bp_kW(ii)) / (p.bsfc_bp_kW(ii + 1) - p.bsfc_bp_kW(ii));
            bsfc = p.bsfc_tbl_gkWh(ii) + w * (p.bsfc_tbl_gkWh(ii + 1) - p.bsfc_tbl_gkWh(ii));
        end
        mf_kgps = Pg * bsfc / 3.6e6;
    end

    % ---- ṁ_bat,eq（符号：放电正代价，充电负收益）----
    if Pbat > 0
        mbe_kgps = Pbat / (p.eta_dcdc * p.eta_dis) / 3600;
    else
        mbe_kgps = Pbat * (p.eta_dcdc * p.eta_chg) / 3600;
    end
end

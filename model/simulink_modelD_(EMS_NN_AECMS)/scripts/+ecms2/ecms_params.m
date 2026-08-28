function p = ecms_params()
% ecms2.ecms_params — ECMS 参数解析（design §3.1/§3.2，T-103~T-105，FR-104~106）
%
%  优先取 base workspace（config/Model_Parameters.m 已运行）数值，
%  否则用 design 定案缺省值。BSFC 表经 ecms2.ensure_bsfc_params 注入链解析
%  （override 文件 → Model_Parameters 默认表，design §3.3 / FR-113）。
%
%  输出 p（struct）：
%    eta_rect       整流器效率 [-]（0.97）
%    eta_dcdc       DC/DC 效率 [-]（0.97）
%    eta_dis/eta_chg 电池放电/充电效率 [-]（0.95/0.95）
%    lambda         λ 常数 [kg/kWh]（ECMS_Lambda，T-104 标定注入）
%    k_soc          SOC-Adaptive ECMS K_soc [kg/kWh/SOC]（ECMS_K_soc，T-105）
%    k_s            Oracle Predictive A-ECMS K_S [-]（ECMS_K_S，T-106；law=6 v2
%                   SOC_ref 通道增益，oracle 上界验证，非实时可实现 E5）
%    lambda_floor   law=6 v2 λ_eff 下限钳位 [kg/kWh]（ECMS_Lambda_Floor=0.43，design v1.1）
%    soc_ref        SOC-Adaptive ECMS SOC 参考值 [-]（ECMS_SOC_Ref=0.60，design §3.2）
%    idle_fuel_kgph 怠速燃油 [kg/h]（P_g=0，6.795，与 T-004 fam 一致）
%    dP_kW/P_max_kW 控制网格步长/上限 [kW]（20 / 1000）
%    dt_ctrl_s      控制周期 [s]（1）
%    SOC_dis_max    SOC ≤ 此值禁放电（0.105，硬约束）
%    SOC_chg_min    SOC ≥ 此值禁充电（0.945，硬约束）
%    grid_kW        可行网格 [kW] = 0:dP:P_max（P_min=0 口径）
%    bsfc_bp_kW / bsfc_tbl_gkWh  BSFC 断点/表（1×N）
%
%  用法: p = ecms2.ecms_params();
    function v = bval(name, defval)
        if evalin('base', sprintf('exist(''%s'', ''var'') && isnumeric(%s)', name, name))
            v = evalin('base', name);
        else
            v = defval;
        end
    end

    p = struct();
    p.eta_rect  = bval('ASSUMED_eta_rect', 0.97);
    p.eta_dcdc  = bval('ASSUMED_eta_dcdc', 0.97);
    p.eta_dis   = bval('ASSUMED_Bat_Eff_Discharge', 0.95);
    p.eta_chg   = bval('ASSUMED_Bat_Eff_Charge', 0.95);
    p.lambda    = bval('ECMS_Lambda', 0.4774);
    p.k_soc     = bval('ECMS_K_soc', 1.0);
    p.k_s       = bval('ECMS_K_S', 0);
    p.lambda_floor = bval('ECMS_Lambda_Floor', 0.43);
    p.soc_ref   = bval('ECMS_SOC_Ref', 0.60);
    p.idle_fuel_kgph = bval('ECMS_Idle_Fuel_kgph', 6.795);
    p.dP_kW     = bval('ECMS_Grid_dP_kW', 20);
    p.P_max_kW  = bval('Gen_P_max', 1000);
    p.dt_ctrl_s = bval('ECMS_dt_ctrl_s', 1);
    p.SOC_dis_max = bval('ECMS_SOC_Discharge_Max', 0.105);
    p.SOC_chg_min = bval('ECMS_SOC_Charge_Min', 0.945);
    p.grid_kW   = (0 : p.dP_kW : p.P_max_kW);
    bs = ecms2.ensure_bsfc_params();
    p.bsfc_bp_kW   = bs.bp;
    p.bsfc_tbl_gkWh = bs.tbl;
end

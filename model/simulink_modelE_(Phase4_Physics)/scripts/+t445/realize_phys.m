function phys = realize_phys(cfg)
% t445.realize_phys — Amendment E 冻结机组/电池积分参数（无拟合系数）
if nargin < 1 || isempty(cfg)
    cfg = phase4f_t445_config();
end
phys = struct();
phys.dt = 1;
phys.dt_bin = cfg.b2_dp.dt_s;
phys.cap = cfg.k_eq.bat_cap_kWh;
phys.eta_dcdc = cfg.b2_dp.eta_dcdc;
phys.eta_chg = cfg.b2_dp.eta_chg;
phys.eta_dis = cfg.b2_dp.eta_dis;
phys.eta_rect = 0.97;
phys.ramp = 20;
phys.tau = 5;
phys.Pmin = 125;
phys.Prated = 500;
phys.P_start = 425;
phys.t_start = 25;
phys.t_sync = 5;
phys.t_cool = 30;
phys.C_start = 0.6;
phys.P_bp = [125 250 375 400 500];
phys.g_kWh = [600 547 447 427 453];
if evalin('base', "exist('ASSUMED_eta_rect','var')")
    phys.eta_rect = evalin('base', 'ASSUMED_eta_rect');
end
if evalin('base', "exist('DG_ramp_kW_per_s','var')")
    phys.ramp = evalin('base', 'DG_ramp_kW_per_s');
    phys.tau = evalin('base', 'DG_tau_s');
    phys.Pmin = evalin('base', 'DG_P_min_kW');
    phys.Prated = evalin('base', 'DG_P_rated_kW');
    phys.t_start = evalin('base', 'DG_t_starting_s');
    phys.t_sync = evalin('base', 'DG_t_sync_s');
    phys.t_cool = evalin('base', 'DG_t_cooldown_s');
    phys.C_start = evalin('base', 'DG_C_start_kg');
    phys.P_bp = evalin('base', 'DG_BSFC_P_kW');
    phys.g_kWh = evalin('base', 'DG_BSFC_gkWh');
end
if evalin('base', "exist('PMS_start_threshold_kW','var')")
    phys.P_start = evalin('base', 'PMS_start_threshold_kW');
end
pg = phys.Pmin:1:phys.Prated;
phys.bsfc_lut = interp1(phys.P_bp, phys.g_kWh, pg, 'pchip');
phys.bsfc_p0 = phys.Pmin;
end

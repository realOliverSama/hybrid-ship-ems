function v = b2_vterm(SOC_H, cfg, SOC_target)
% t445.b2_vterm — 线性终端价值 + 带外二次罚（DGN-F-04）
% V = -k_eq*Cap*(SOC_H-SOC_target) + w_term*max(0,|dSOC|-delta_term)^2
if nargin < 3 || isempty(SOC_target)
    SOC_target = cfg.v_term.corpus_soc0_machine_assert;
end
keq = cfg.k_eq.value_kg_per_kWh;
cap = cfg.k_eq.bat_cap_kWh;
dlt = cfg.v_term.delta_term;
w = cfg.v_term.w_term;
lin = -keq * cap * (SOC_H - SOC_target);
exc = max(0, abs(SOC_H - SOC_target) - dlt);
v = lin + w * exc.^2;
end

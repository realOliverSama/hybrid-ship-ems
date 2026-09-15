function v = b2_objective(fuel_kg, SOC_H, cfg, SOC_target)
% t445.b2_objective — 窗成本 = 代理燃油 + V_term（DGN-F-04/F-09）
if nargin < 4 || isempty(SOC_target)
    SOC_target = cfg.v_term.corpus_soc0_machine_assert;
end
v = fuel_kg + t445.b2_vterm(SOC_H, cfg, SOC_target);
end

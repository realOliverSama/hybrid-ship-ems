% t002b_transition_smoke.m — 过渡态物理验证：law=0 静水巡航（新参数+旧 Plant 代码）
% 目的：在链集成前验证 K 重标定链端到端自洽——11 kn 巡航电功率应落在
% design §5.3 冻结判据 [274, 335] kW（锚定链 304.5 kW ±10%）。
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
MDL = 'HybridShip_ModelE';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
SCN = SCENARIOS(1);
SCN.voyage_distance_m = 1500;
out = model.run_scenario_sim(MDL, SCN, 0, 2500);
ds = model.extract_logsout(out);
% 稳态段（最后 30% 航程）平均电功率
iSteady = ds.x_ship_m > 1000;
P_steady = mean(ds.P_prop_actual_kW(iSteady));
v_steady = mean(ds.v_ship_mps(iSteady));
fprintf('STEADY: v=%.3f m/s (%.2f kn) | P_prop_elec=%.1f kW | 判据带 [274,335] kW -> %s\n', ...
    v_steady, v_steady*1.944, P_steady, string(P_steady >= 274 && P_steady <= 335));
fprintf('TRANSITION-SMOKE: V_dc=[%.1f, %.1f] V | SOC_end=%.4f | fuel=%.3f kg | NaN=%d\n', ...
    min(ds.V_dc_V), max(ds.V_dc_V), ds.SOC(end), ds.fuel_total_kg(end), ...
    ~all(isfinite(ds.P_prop_actual_kW)));
close_system(MDL, 0);
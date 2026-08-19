function modelb_plot(RB, RA, sdir)
% model.modelb_plot — Model B Baseline vs AI 对比出图（每工况 3 张）
dB = RB.ds; dA = RA.ds;

% --- 1. 母线电压对比（Model B 核心图） ---
f = figure('Visible', 'off', 'Position', [100 100 1000 420]);
plot(dB.t, dB.V_dc_V, 'b-', 'LineWidth', 1.0); hold on;
plot(dA.t, dA.V_dc_V, 'r--', 'LineWidth', 1.0);
yline(795, 'k:', 'LineWidth', 0.8); yline(675, 'k:', 'LineWidth', 0.8);
yline(802.5, 'k--', 'LineWidth', 0.8); yline(600, 'k--', 'LineWidth', 0.8);
grid on; xlabel('时间 [s]'); ylabel('V_{dc} [V]');
legend({'Baseline', 'AI', 'CCS 稳态带', '', 'CCS 瞬态带', ''}, 'Location', 'best');
title(sprintf('%s：母线电压对比', RB.scen), 'Interpreter', 'none');
exportgraphics(f, fullfile(sdir, 'vdc_compare.png'), 'Resolution', 140);
close(f);

% --- 2. 功率对比（发电机+电池） ---
f = figure('Visible', 'off', 'Position', [100 100 1000 640]);
subplot(2,1,1);
plot(dB.t, dB.P_load_total_kW, 'k-', 'LineWidth', 1.0); hold on;
plot(dB.t, dB.P_gen_actual_kW, 'b-', 'LineWidth', 1.0);
plot(dA.t, dA.P_gen_actual_kW, 'r--', 'LineWidth', 1.0);
grid on; ylabel('功率 [kW]');
legend({'P_{load}', 'P_{gen} Baseline', 'P_{gen} AI'}, 'Location', 'best');
title(sprintf('%s：功率对比', RB.scen), 'Interpreter', 'none');
subplot(2,1,2);
plot(dB.t, dB.P_bat_actual_kW, 'b-', 'LineWidth', 1.0); hold on;
plot(dA.t, dA.P_bat_actual_kW, 'r--', 'LineWidth', 1.0);
grid on; xlabel('时间 [s]'); ylabel('电池功率 [kW]');
legend({'P_{bat} Baseline', 'P_{bat} AI'}, 'Location', 'best');
exportgraphics(f, fullfile(sdir, 'power_compare.png'), 'Resolution', 140);
close(f);

% --- 3. SOC 与船速 ---
f = figure('Visible', 'off', 'Position', [100 100 1000 640]);
subplot(2,1,1);
plot(dB.t, dB.SOC, 'b-', 'LineWidth', 1.0); hold on;
plot(dA.t, dA.SOC, 'r--', 'LineWidth', 1.0);
grid on; ylabel('SOC [-]');
legend({'SOC Baseline', 'SOC AI'}, 'Location', 'best');
subplot(2,1,2);
plot(dB.t, dB.v_cmd_mps, 'k--', 'LineWidth', 1.0); hold on;
plot(dB.t, dB.v_ship_mps, 'b-', 'LineWidth', 1.0);
plot(dA.t, dA.v_ship_mps, 'r--', 'LineWidth', 1.0);
grid on; xlabel('时间 [s]'); ylabel('船速 [m/s]');
legend({'v_{cmd}', 'v Baseline', 'v AI'}, 'Location', 'best');
exportgraphics(f, fullfile(sdir, 'soc_speed_compare.png'), 'Resolution', 140);
close(f);
end

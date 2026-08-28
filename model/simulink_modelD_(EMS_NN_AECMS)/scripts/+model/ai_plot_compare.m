% model.ai_plot_compare — AI 对比出图
function local_plot_compare(RB, RA, sdir)
dB = RB.ds; dA = RA.ds;
% --- 功率对比 ---
f = figure('Visible', 'off', 'Position', [100 100 1000 640]);
subplot(2,1,1);
plot(dB.t, dB.P_load_total_kW, 'k-', 'LineWidth', 1.2); hold on;
plot(dB.t, dB.P_gen_actual_kW, 'b-', 'LineWidth', 1.0);
plot(dA.t, dA.P_gen_actual_kW, 'r--', 'LineWidth', 1.0);
grid on; ylabel('功率 [kW]');
legend({'P_{load}', 'P_{gen} Baseline', 'P_{gen} AI'}, 'Location', 'best');
title(sprintf('%s：发电机功率对比', RB.scen), 'Interpreter', 'none');
subplot(2,1,2);
plot(dB.t, dB.P_bat_actual_kW, 'b-', 'LineWidth', 1.0); hold on;
plot(dA.t, dA.P_bat_actual_kW, 'r--', 'LineWidth', 1.0);
grid on; xlabel('时间 [s]'); ylabel('电池功率 [kW]');
legend({'P_{bat} Baseline (放电+)', 'P_{bat} AI'}, 'Location', 'best');
exportgraphics(f, fullfile(sdir, 'power_compare.png'), 'Resolution', 140);
close(f);
% --- SOC 与船速 ---
f = figure('Visible', 'off', 'Position', [100 100 1000 640]);
subplot(2,1,1);
plot(dB.t, dB.SOC, 'b-', 'LineWidth', 1.0); hold on;
plot(dA.t, dA.SOC, 'r--', 'LineWidth', 1.0);
grid on; ylabel('SOC [-]');
legend({'SOC Baseline', 'SOC AI'}, 'Location', 'best');
title(sprintf('%s：SOC 与船速对比', RB.scen), 'Interpreter', 'none');
subplot(2,1,2);
plot(dB.t, dB.v_cmd_mps, 'k--', 'LineWidth', 1.0); hold on;
plot(dB.t, dB.v_ship_mps, 'b-', 'LineWidth', 1.0);
plot(dA.t, dA.v_ship_mps, 'r--', 'LineWidth', 1.0);
grid on; xlabel('时间 [s]'); ylabel('船速 [m/s]');
legend({'v_{cmd}', 'v Baseline', 'v AI'}, 'Location', 'best');
exportgraphics(f, fullfile(sdir, 'soc_speed_compare.png'), 'Resolution', 140);
close(f);
% --- 预测质量（AI 航次） ---
f = figure('Visible', 'off', 'Position', [100 100 1000 400]);
plot(dA.t, dA.P_load_total_kW, 'k-', 'LineWidth', 1.0); hold on;
if isfield(dA, 'P_pred_max_kW')
    plot(dA.t, dA.P_pred_max_kW, 'g-', 'LineWidth', 1.0);
    leg = {'P_{load} 实际', 'P_{pred,max} 预测'};
else
    leg = {'P_{load} 实际'};
end
if isfield(dA, 'resistance_multiplier')
    yyaxis right
    plot(dA.t, dA.resistance_multiplier, ':', 'Color', [0.5 0.5 0.5]);
    ylabel('阻力倍率 [-]');
    leg{end+1} = 'resistance\_multiplier';
    yyaxis left
end
grid on; xlabel('时间 [s]'); ylabel('功率 [kW]');
legend(leg, 'Location', 'best');
title(sprintf('%s：AI 预测质量', RA.scen), 'Interpreter', 'none');
exportgraphics(f, fullfile(sdir, 'pred_quality.png'), 'Resolution', 140);
close(f);
end

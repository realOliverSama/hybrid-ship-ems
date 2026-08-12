function plot_smoke(ds, SCN, outdir)
% modela_k3.plot_smoke — 生成每工况 5 张结果图
%   speed_tracking.png / power_split.png / soc.png / fuel.png / power_balance.png
    if ~exist(outdir, 'dir'), mkdir(outdir); end
    t = ds.t;
    % 1. 船速跟踪
    f = figure('Visible', 'off', 'Position', [100 100 900 500]);
    plot(t, ds.v_cmd_mps, 'b--', 'LineWidth', 1.2); hold on;
    plot(t, ds.v_ship_mps, 'r-', 'LineWidth', 1.2);
    xlabel('时间 t [s]'); ylabel('船速 [m/s]');
    title(sprintf('%s — 目标/实际船速 (target=%.2f m/s)', SCN.name, SCN.target_speed_mps));
    legend('v\_cmd\_mps', 'v\_ship\_mps', 'Location', 'southeast');
    grid on; saveas(f, fullfile(outdir, 'speed_tracking.png')); close(f);
    % 2. 功率分配
    f = figure('Visible', 'off', 'Position', [100 100 900 500]);
    plot(t, ds.P_load_total_kW, 'k-', 'LineWidth', 1.2); hold on;
    plot(t, ds.P_gen_actual_kW, 'b-', 'LineWidth', 1.2);
    plot(t, ds.P_bat_actual_kW, 'g-', 'LineWidth', 1.2);
    plot(t, ds.P_prop_actual_kW, 'm--', 'LineWidth', 1.0);
    plot(t, ds.P_unserved_kW, 'r:', 'LineWidth', 1.5);
    xlabel('时间 t [s]'); ylabel('功率 [kW]');
    title(sprintf('%s — 功率分配 (总负荷/发电机/电池/推进/缺电)', SCN.name));
    legend('P\_load\_total', 'P\_gen\_actual', 'P\_bat\_actual', 'P\_prop\_actual', 'P\_unserved', 'Location', 'best');
    grid on; saveas(f, fullfile(outdir, 'power_split.png')); close(f);
    % 3. SOC
    f = figure('Visible', 'off', 'Position', [100 100 900 400]);
    plot(t, ds.SOC, 'g-', 'LineWidth', 1.2);
    xlabel('时间 t [s]'); ylabel('SOC [-]');
    title(sprintf('%s — 电池 SOC', SCN.name));
    grid on; ylim([0 1]); saveas(f, fullfile(outdir, 'soc.png')); close(f);
    % 4. 燃油
    f = figure('Visible', 'off', 'Position', [100 100 900 400]);
    plot(t, ds.fuel_total_kg, 'b-', 'LineWidth', 1.2);
    xlabel('时间 t [s]'); ylabel('累计燃油 [kg]');
    title(sprintf('%s — 累计燃油消耗 (%.1f kg)', SCN.name, ds.fuel_total_kg(end)));
    grid on; saveas(f, fullfile(outdir, 'fuel.png')); close(f);
    % 5. 功率平衡
    f = figure('Visible', 'off', 'Position', [100 100 900 400]);
    plot(t, ds.e_balance_kW, 'r-', 'LineWidth', 1.0); hold on;
    plot(t, ds.e_balance_kW ./ max(ds.P_load_total_kW, 1) * 100, 'b--', 'LineWidth', 1.0);
    xlabel('时间 t [s]'); ylabel('平衡误差 [kW] / [%]');
    title(sprintf('%s — 功率平衡误差 e_balance = P_gen+P_bat-P_load+P_unserved', SCN.name));
    legend('e\_balance [kW]', '相对误差 [%]', 'Location', 'best');
    grid on; saveas(f, fullfile(outdir, 'power_balance.png')); close(f);
end

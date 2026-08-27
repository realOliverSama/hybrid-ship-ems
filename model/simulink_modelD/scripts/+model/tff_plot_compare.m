function tff_plot_compare(sdir, scenName, R)
% model.tff_plot_compare — Phase 1 四实验对比出图（E0/E1/E2/E3 + tau 扫描）
% 输入:
%   sdir     输出目录（results/phase1/Sxx）
%   scenName 工况名（标题用）
%   R        单元数组 {expName, tau, ds, m}，按绘制顺序
% 输出: sdir/compare.png（6 面板：P_load / P_gen_ref / P_gen_actual / P_ff / Vdc / SOC）
    styles = {'k-', 'b-', 'r-', 'm-', 'r--', 'r:', 'b--', 'm--'};
    f = figure('Visible', 'off', 'Position', [80 80 1100 860]);
    sigs = {'P_load_total_kW', 'P_gen_ref_kW', 'P_gen_actual_kW', 'P_ff_kW', 'V_dc_V', 'SOC'};
    ylabs = {'P_{load} [kW]', 'P_{gen,ref} [kW]', 'P_{gen,act} [kW]', 'P_{ff} [kW]', 'V_{dc} [V]', 'SOC [-]'};
    for p = 1:6
        subplot(3, 2, p);
        hold on;
        nplot = 0;
        for k = 1:numel(R)
            ds = R{k}{3};
            nm = sigs{p};
            if ~isfield(ds, nm), continue; end
            lab = sprintf('%s', R{k}{1});
            if ~isempty(R{k}{2}), lab = sprintf('%s\\tau=%d', lab, R{k}{2}); end
            plot(ds.t, ds.(nm), styles{mod(k - 1, numel(styles)) + 1}, 'LineWidth', 1.0, ...
                'DisplayName', lab);
            nplot = nplot + 1;
        end
        grid on; xlabel('时间 [s]'); ylabel(ylabs{p});
        if p == 1
            legend('Location', 'best', 'FontSize', 7);
            title(sprintf('%s：E0–E3 对比', scenName), 'Interpreter', 'none');
        end
        hold off;
    end
    sgtitle(sprintf('%s — Phase 1 控制律对比', scenName), 'Interpreter', 'none');
    exportgraphics(f, fullfile(sdir, 'compare.png'), 'Resolution', 140);
    close(f);
end

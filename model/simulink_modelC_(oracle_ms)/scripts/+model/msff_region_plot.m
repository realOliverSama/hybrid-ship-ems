function msff_region_plot(csvPath, outDir, varargin)
% model.msff_region_plot — 预测增强 EMS 有效工作区域图（T-006, FR-013）
% 输入:
%   csvPath  runs.csv 路径（write_msff_csv 产出）
%   outDir   输出目录（PNG 写入处）
%   可选名值对:
%     'lambda_grid'   λ_P 网格（默认 [0.8 0.9 1.0 1.1 1.25]，升序）
%     'Rgen_grid'     R_gen 网格（默认 [20 30 40 50 60] kW/s）
%     'Peak_drop_pct' 收益判据：P_bat_peak 下降阈值 %（默认 15）
%     'Rms_drop_pct'  RMS/吞吐量下降阈值 %（默认 10）
% 输出: 每个 (scenario × horizon) 一个 PNG：
%   子图 1 ΔP_bat_peak%   2 ΔVdc_min [V]   3 Δfuel%   4 收益掩码
% 归一化: 相对同 (scenario, lambda_P, R_gen, tau_gen, SOC0) 的 C0 基线。
% 收益单元格 = ΔP_bat_peak% ≤ -Peak_drop_pct 且 (ΔRMS% ≤ -Rms_drop_pct 或 Δ吞吐量% ≤ -Rms_drop_pct)
% 失败行（status != ok）不参与聚合，在对应单元格标注 'X'。

    p = inputParser;
    addParameter(p, 'lambda_grid', [0.8 0.9 1.0 1.1 1.25], @(x) isnumeric(x) && numel(x) >= 2);
    addParameter(p, 'Rgen_grid', [20 30 40 50 60], @(x) isnumeric(x) && numel(x) >= 2);
    addParameter(p, 'Peak_drop_pct', 15, @isscalar);
    addParameter(p, 'Rms_drop_pct', 10, @isscalar);
    parse(p, varargin{:});
    LG = p.Results.lambda_grid; RG = p.Results.Rgen_grid;
    PK = p.Results.Peak_drop_pct; RM = p.Results.Rms_drop_pct;

    if ~exist(outDir, 'dir'), mkdir(outDir); end
    T = readtable(csvPath, 'TextType', 'string');
    % 数值列解析（NaN 字符串自动→NaN）
    numCols = {'lambda_P','R_gen','tau_gen','SOC0','horizon_s', ...
               'fuel_kg','Vdc_min','Vdc_max','SOC_end','SOC_min', ...
               'E_bat_dis_kWh','E_bat_throughput_kWh','P_bat_rms_kW','P_bat_peak_abs_kW', ...
               't_sat_s','E_unserved_kWh','v_rmse_mps','N_UV','T_UV','N_stress','T_stress', ...
               'ff_active_s','ff_energy_kJ'};
    for i = 1:numel(numCols)
        if ismember(numCols{i}, T.Properties.VariableNames) && isstring(T.(numCols{i}))
            T.(numCols{i}) = str2double(T.(numCols{i}));
        end
    end

    okRows = T.status == "ok";
    T0 = T(okRows, :);   % 聚合只用成功行

    % ---- C0 基线（controller=='C0'，无 horizon 依赖）----
    c0 = T0(T0.controller == "C0", :);
    [baselines, key2row] = baselineMap(c0);

    % ---- CMS/CP 行 ----
    ff = T0(T0.controller ~= "C0", :);
    if isempty(ff)
        warning('msff_region_plot: 无非 C0 行，区域图无内容');
        return;
    end

    facets = unique(ff.scenario + "_" + string(ff.horizon_s), 'rows');
    for f = 1:numel(facets)
        parts = split(facets(f), "_");
        scen = parts(1); H = str2double(parts(2));
        sub = ff(ff.scenario == scen & ff.horizon_s == H, :);
        mkRegionFig(sub, baselines, key2row, LG, RG, PK, RM, scen, H, outDir);
    end
    fprintf('msff_region_plot: %d 个 facet 图已输出到 %s\n', numel(facets), outDir);
end

function [baselines, key2row] = baselineMap(c0)
% baselines: containers.Map key=join(scen,lP,Rgen,tau,SOC0,'|') -> struct of C0 指标
% key2row: 同 key 到 c0 行号映射（供失败标注）
    baselines = containers.Map();
    key2row = containers.Map();
    for i = 1:height(c0)
        k = join([c0.scenario(i), string(c0.lambda_P(i)), string(c0.R_gen(i)), ...
                  string(c0.tau_gen(i)), string(c0.SOC0(i))], '|');
        b = struct();
        b.fuel_kg = c0.fuel_kg(i);
        b.Vdc_min = c0.Vdc_min(i);
        b.P_bat_peak_abs_kW = c0.P_bat_peak_abs_kW(i);
        b.P_bat_rms_kW = c0.P_bat_rms_kW(i);
        b.E_bat_throughput_kWh = c0.E_bat_throughput_kWh(i);
        baselines(k) = b;
        key2row(k) = i;
    end
end

function mkRegionFig(sub, baselines, key2row, LG, RG, PK, RM, scen, H, outDir)
    nL = numel(LG); nR = numel(RG);
    dPeak = NaN(nL, nR); dVdc = NaN(nL, nR); dFuel = NaN(nL, nR); benefit = zeros(nL, nR);
    for i = 1:height(sub)
        li = find(LG == sub.lambda_P(i), 1);
        ri = find(RG == sub.R_gen(i), 1);
        if isempty(li) || isempty(ri), continue; end
        k = join([sub.scenario(i), string(sub.lambda_P(i)), string(sub.R_gen(i)), ...
                  string(sub.tau_gen(i)), string(sub.SOC0(i))], '|');
        if ~baselines.isKey(k), continue; end
        b = baselines(k);
        cmsPeak = sub.P_bat_peak_abs_kW(i);
        cmsRms = sub.P_bat_rms_kW(i);
        cmsThr = sub.E_bat_throughput_kWh(i);
        dPeak(li, ri) = 100 * (cmsPeak - b.P_bat_peak_abs_kW) / b.P_bat_peak_abs_kW;
        dVdc(li, ri) = sub.Vdc_min(i) - b.Vdc_min;   % 绝对值 [V]，正=母线最低电压更高
        dFuel(li, ri) = 100 * (sub.fuel_kg(i) - b.fuel_kg) / b.fuel_kg;
        dRmsPct = 100 * (cmsRms - b.P_bat_rms_kW) / b.P_bat_rms_kW;
        dThrPct = 100 * (cmsThr - b.E_bat_throughput_kWh) / b.E_bat_throughput_kWh;
        benefit(li, ri) = (dPeak(li, ri) <= -PK) && (dRmsPct <= -RM || dThrPct <= -RM);
    end

    fig = figure('Visible', 'off', 'Position', [100 100 1150 850]);
    tiledlayout(2, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
    plotHeat(fig, dPeak, 'ΔP_{bat,peak} [%]（负=改善）', LG, RG, 'div');
    plotHeat(fig, dVdc, 'ΔV_{dc,min} [V]（正=提升）', LG, RG, 'div');
    plotHeat(fig, dFuel, 'ΔFuel [%]（正=代价）', LG, RG, 'div');
    % 收益掩码
    nexttile(4);
    imagesc(1:nR, 1:nL, benefit);
    set(gca, 'YDir', 'normal', 'XTick', 1:nR, 'XTickLabel', string(RG), ...
        'YTick', 1:nL, 'YTickLabel', string(LG));
    colormap(gca, gray(2));
    xlabel('R_{gen} [kW/s]'); ylabel('λ_P');
    title(sprintf('收益判据掩码（ΔP_{peak}≤%g%% 且 RMS/吞吐量≤-%g%%）', PK, RM));
    for li = 1:nL
        for ri = 1:nR
            if benefit(li, ri)
                text(ri, li, '✓', 'HorizontalAlignment', 'center', 'Color', 'g', 'FontSize', 12, 'FontWeight', 'bold');
            elseif isnan(dPeak(li, ri))
                text(ri, li, 'X', 'HorizontalAlignment', 'center', 'Color', 'r');
            end
        end
    end
    sgtitle(sprintf('Predictive EMS Effective Region — %s, H=%d s', scen, H));
    outFile = fullfile(outDir, sprintf('msff_region_%s_H%d.png', scen, H));
    exportgraphics(fig, outFile, 'Resolution', 150);
    close(fig);
    fprintf('  -> %s\n', outFile);
end

function plotHeat(fig, M, ttl, LG, RG, cmap)
    nexttile;
    imagesc(1:numel(RG), 1:numel(LG), M);
    set(gca, 'YDir', 'normal', 'XTick', 1:numel(RG), 'XTickLabel', string(RG), ...
        'YTick', 1:numel(LG), 'YTickLabel', string(LG));
    if strcmp(cmap, 'div')
        % 蓝-白-红发散色图（MATLAB 内置无 RdYlBu，手工构造）
        c = [0.19 0.32 0.68; 0.90 0.93 0.98; 0.70 0.16 0.15];
        colormap(gca, c);
    else
        colormap(gca, cmap);
    end
    colorbar;
    xlabel('R_{gen} [kW/s]'); ylabel('λ_P');
    title(ttl);
    % 数值标注
    for li = 1:numel(LG)
        for ri = 1:numel(RG)
            v = M(li, ri);
            if isnan(v)
                text(ri, li, 'X', 'HorizontalAlignment', 'center', 'Color', [0.6 0.6 0.6]);
            else
                text(ri, li, sprintf('%.0f', v), 'HorizontalAlignment', 'center', ...
                    'FontSize', 8, 'Color', [0.1 0.1 0.1]);
            end
        end
    end
end

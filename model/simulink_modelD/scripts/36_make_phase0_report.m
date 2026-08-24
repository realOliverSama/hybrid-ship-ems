%% ========================================================================
%  36_make_phase0_report.m — Phase 0 汇总图表生成（T-016，design §1）
%  读取 results/ecms_phase0/runs.csv + ext_2d/runs_ext.csv，生成报告用 PNG。
%  用法：cd modelD; eval(fileread('scripts/36_make_phase0_report.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
OUT  = fullfile(ROOT, 'results', 'ecms_phase0');
if ~exist(OUT, 'dir'); mkdir(OUT); end

opts = detectImportOptions(fullfile(OUT, 'runs.csv'), 'TextType', 'string');
T = readtable(fullfile(OUT, 'runs.csv'), opts);
mat = T(T.scan == "matrix", :);

tiers = [1 2 3];
scenList = unique(mat.scenario, 'stable');

%% 图1：分场景节油上限（连续口径，三档 BSFC，P_min=400 kW 主判定配置）
sel = mat(mat.mode == "continuous" & mat.P_min_kW == 400, :);
Y = zeros(numel(scenList), 3);
for i = 1:numel(scenList)
    for t = 1:3
        r = sel(sel.scenario == scenList(i) & sel.tier == tiers(t), :);
        Y(i, t) = r.saving_pct_corr(1);
    end
end
fig = figure('Position', [100 100 980 420], 'Visible', 'off');
b = bar(Y); grid on;
xticks(1:numel(scenList)); xticklabels(scenList); xtickangle(20);
set(gca, 'TickLabelInterpreter', 'none');
ylabel('fuel saving vs corrected baseline [%]');
title('Phase 0 DP upper bound by scenario (continuous, P_{min}=400 kW)');
legend({'tier1 (+30%)', 'tier2 (+50%)', 'tier3 (+70%)'}, 'Location', 'best');
yline(5, '--r', 'GO threshold 5%', 'LineWidth', 1.2, 'HandleVisibility', 'off');
yline(2, '--', 'NO-GO threshold 2%', 'LineWidth', 1.2, 'Color', [0.5 0.5 0.5], 'HandleVisibility', 'off', 'LabelHorizontalAlignment', 'left');
saveas(fig, fullfile(OUT, 'fig_scenario_savings.png')); close(fig);

%% 图2：P_min 档位 × tier（连续口径，8 场景均值）
pmSet = [0 300 400 500];
Y2 = zeros(numel(pmSet), 3);
for i = 1:numel(pmSet)
    for t = 1:3
        r = mat(mat.mode == "continuous" & mat.P_min_kW == pmSet(i) & mat.tier == tiers(t), :);
        Y2(i, t) = mean(r.saving_pct_corr);
    end
end
fig = figure('Position', [100 100 640 400], 'Visible', 'off');
plot(pmSet, Y2, '-o', 'LineWidth', 1.5); grid on;
xlabel('P_{min,stable} (aggregated) [kW]'); ylabel('mean saving [%]');
title('Sensitivity to minimum stable load (continuous, mean of 8 scenarios)');
legend({'tier1', 'tier2', 'tier3'}, 'Location', 'best');
saveas(fig, fullfile(OUT, 'fig_pmin_sensitivity.png')); close(fig);

%% 图3：单因素扫描（tier2, P_min=400, 8 场景均值）
fig = figure('Position', [100 100 980 360], 'Visible', 'off');
subplot(1, 3, 1);
r = T(T.scan == "ramp", :);
gv = zeros(0); sv = zeros(0);
for g = [20 40 80]
    rr = r(r.R_gen_kWps == g, :); gv(end+1) = g; sv(end+1) = mean(rr.saving_pct_corr); %#ok<AGROW>
end
plot(gv, sv, '-o', 'LineWidth', 1.5); grid on;
xlabel('R_{gen} [kW/s]'); ylabel('mean saving [%]'); title('Ramp rate (continuous)');
subplot(1, 3, 2);
r = T(T.scan == "batcap", :);
gv = [643 1071.36 1500]; sv = zeros(size(gv));
for i = 1:numel(gv)
    rr = r(abs(r.Bat_Cap_kWh - gv(i)) < 1, :); sv(i) = mean(rr.saving_pct_corr);
end
plot(gv, sv, '-o', 'LineWidth', 1.5); grid on;
xlabel('Battery capacity [kWh]'); ylabel('mean saving [%]'); title('Battery capacity (continuous)');
subplot(1, 3, 3);
r = T(T.scan == "cstart", :);
gv = [1 2 5]; sv = zeros(size(gv));
for i = 1:numel(gv)
    rr = r(r.C_start_kg == gv(i), :); sv(i) = mean(rr.saving_pct_corr);
end
plot(gv, sv, '-o', 'LineWidth', 1.5); grid on;
xlabel('C_{start} [kg]'); ylabel('mean saving [%]'); title('Startup cost (startstop)');
saveas(fig, fullfile(OUT, 'fig_singlefactor.png')); close(fig);

%% 图4：二维扩展 vs 一维主链路（连续口径，P_min=400）
fext = fullfile(OUT, 'ext_2d', 'runs_ext.csv');
if exist(fext, 'file')
    TE = readtable(fext, opts);
    se = TE(TE.mode == "continuous" & TE.P_min_kW == 400, :);
    s1 = mat(mat.mode == "continuous" & mat.P_min_kW == 400, :);
    D = zeros(1, 3);
    for t = 1:3
        D(t) = mean(se.saving_pct_corr(se.tier == tiers(t))) - mean(s1.saving_pct_corr(s1.tier == tiers(t)));
    end
    fig = figure('Position', [100 100 520 360], 'Visible', 'off');
    bar(1:3, D); grid on;
    xticks(1:3); xticklabels({'tier1', 'tier2', 'tier3'});
    ylabel('\Delta saving (2D variable-speed minus 1D) [pp]');
    title('2-D extension vs 1-D main chain (hypothetical VSG)');
    saveas(fig, fullfile(OUT, 'fig_ext2d_delta.png')); close(fig);
end

fprintf('[36] Phase 0 report figures written to %s\n', OUT);

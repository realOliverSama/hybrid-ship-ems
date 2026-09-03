%% ========================================================================
%  31_build_bsfc_family.m — ECMS Phase 0：BSFC 曲线族生成器入口（T-004，FR-004/FR-005）
%  产出：
%    results/ecms_phase0/bsfc/bsfc_family.mat      曲线族 .mat（字段见下方清单）
%    results/ecms_phase0/bsfc/bsfc_1d_family.png   一维三档叠加 + 双锚点标注
%    results/ecms_phase0/bsfc/bsfc_2d_map.png      二维等高线 + 最优转速线
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/31_build_bsfc_family.m'))"
%  自检任一不过：+ecms/bsfc_family.m 内部 error('ECMS:BSFC', ...) 非零退出。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));     % 提供 +ecms 包

fprintf('\n[31] ===== ECMS Phase 0 — BSFC 曲线族生成（T-004） =====\n');
fprintf('[31] ROOT = %s\n', ROOT);

% ---- 生成曲线族（含自检，FR-005；不通过则在此 error 退出） ----
fam = ecms.bsfc_family();

% ---- 输出目录 ----
outDir = fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc');
if ~exist(outDir, 'dir'), mkdir(outDir); end
matPath = fullfile(outDir, 'bsfc_family.mat');
png1    = fullfile(outDir, 'bsfc_1d_family.png');
png2    = fullfile(outDir, 'bsfc_2d_map.png');

% ---- 保存 .mat（顶层字段展开，兼容 Model_Parameters.m 覆盖 hook 读取
%      BSFC_Breakpoints_kW / BSFC_Table_gkWh / override_tag） ----
F = fam;
save(matPath, '-struct', 'F');
fprintf('[31] 已写入 %s\n', matPath);

% ========================================================================
%  一维图：三档叠加 + 双锚点标注（英文标注，避免 batch 字体乱码）
% ========================================================================
f1 = figure('Visible', 'off', 'Position', [80 80 900 600]);
colors = lines(3);
leg = cell(1, 3);
for t = 1:3
    plot(fam.tiers(t).unit_BP_kW, fam.tiers(t).unit_table_gkWh, '-', ...
        'Color', colors(t, :), 'LineWidth', 1.6);
    hold on;
    leg{t} = sprintf('d25=+%.0f%%', 100 * fam.tiers(t).d25);
end
% 双锚点标注
plot(400, 427, 'ko', 'MarkerFaceColor', 'k', 'MarkerSize', 7);
text(400, 427 - 14, 'opt anchor: 427 g/kWh @ L=0.80', ...
    'HorizontalAlignment', 'center', 'FontSize', 9);
plot(500, 453, 'ks', 'MarkerFaceColor', 'k', 'MarkerSize', 7);
text(500, 453 + 14, 'rated anchor: 453 g/kWh @ 500 kW', ...
    'HorizontalAlignment', 'center', 'FontSize', 9);
xline(400, '--', 'Color', [0.6 0.6 0.6]);
xlabel('P [kW]  (single unit, 1500 rpm)');
ylabel('BSFC [g/kWh]');
title('BSFC 1-D family: 3 low-load degradation tiers (anchors 427/453 g/kWh)');
legend(leg, 'Location', 'northwest', 'Interpreter', 'none');
grid on; axis([0 500 400 760]);
exportgraphics(f1, png1, 'Resolution', 150);
close(f1);
fprintf('[31] 已写入 %s\n', png1);

% ========================================================================
%  二维图：等高线 + 最优转速线（中档 d25=+50% 作示意，扩展分析专用）
% ========================================================================
tMap = find(abs([fam.map2d.tiers.d25] - 0.50) < 1e-9, 1);
if isempty(tMap), tMap = 2; end
Z = fam.map2d.tiers(tMap).table_gkWh.';      % n×P，供 meshgrid 匹配
[Xp, Yn] = meshgrid(fam.map2d.P_kW, fam.map2d.n_rpm);
f2 = figure('Visible', 'off', 'Position', [80 80 900 620]);
contourf(Xp, Yn, Z, 16);
colormap(parula);
colorbar; clim([min(Z(:)) max(Z(:))]);
hold on;
plot(fam.map2d.P_kW, fam.map2d.n_opt_rpm, 'r-', 'LineWidth', 2, ...
    'DisplayName', 'n*(P) optimal speed line');
plot(fam.map2d.P_kW([1 end]), [1500 1500], 'k--', 'LineWidth', 1.2, ...
    'DisplayName', 'rated speed 1500 rpm (1-D curve)');
xlabel('P [kW]  (single unit)');
ylabel('n [rpm]');
title(sprintf('BSFC 2-D map (expansion analysis, mid tier d25=+%.0f%%): k_n=0.10, \\Deltan_band=300', ...
    100 * fam.map2d.tiers(tMap).d25));
legend('Location', 'northwest', 'Interpreter', 'none');
exportgraphics(f2, png2, 'Resolution', 150);
close(f2);
fprintf('[31] 已写入 %s\n', png2);

% ========================================================================
%  stdout：自检通过项清单 + 关键数值证据
% ========================================================================
fprintf('\n[31] ===== 自检通过项清单（FR-005）=====\n');
for i = 1:numel(fam.selfcheck.checks)
    c = fam.selfcheck.checks(i);
    fprintf('  [PASS] %-42s %s\n', c.name, c.detail);
end

fprintf('\n[31] ===== 关键数值 =====\n');
fprintf('  怠速燃油流量 fuel_flow_idle = %.4f kg/h (=0.03 x %.1f x %d / 1000)\n', ...
    fam.fuel_flow_idle_kgph, fam.params.P_unit_rated, fam.params.bsfc_rated);
fprintf('  额定燃油流量 = %.4f kg/h\n', fam.fuel_flow_rated_kgph);
for t = 1:3
    v = interp1(fam.tiers(t).unit_BP_kW, fam.tiers(t).unit_table_gkWh, 0.25 * fam.params.P_unit_rated);
    fprintf('  L=0.25 表值 (d25=%.2f): %.4f g/kWh (期望 427x%.1f=%.1f)\n', ...
        fam.tiers(t).d25, v, 1 + fam.tiers(t).d25, 427 * (1 + fam.tiers(t).d25));
end

fprintf('\n[31] ===== 输出文件 =====\n');
fprintf('  %s\n  %s\n  %s\n', matPath, png1, png2);
fprintf('\n[31] BSFC 曲线族生成完成（T-004）。\n');

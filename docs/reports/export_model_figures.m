%% ========================================================================
%  export_model_figures.m — 导出 Model D（HybridShip_ModelD_DCBus.slx）顶层与关键
%  子系统结构截图，供《EMS01 研究总结报告》建模章节使用。
%
%  输出：docs/reports/figures/model/*.png（模型只读，排列后不落盘，零改动）
%  用法：matlab -batch "run('docs/reports/export_model_figures.m')"   （仓库根目录下）
%  幂等可重跑；相对路径（仓库根由脚本位置推导）。
% ========================================================================
HERE = fileparts(mfilename('fullpath'));
if isempty(HERE) || ~exist(fullfile(HERE, 'build_docx.py'), 'file')
    HERE = pwd;  % 兼容 eval(fileread(...)) / -batch 顶层执行
end
REPO = fileparts(fileparts(HERE));                 % docs/reports -> 仓库根
WS   = fullfile(REPO, 'model', 'simulink_modelD_(EMS_NN_AECMS)');
FIGD = fullfile(HERE, 'figures', 'model');
if ~exist(FIGD, 'dir'), mkdir(FIGD); end

mdl = 'HybridShip_ModelD_DCBus';
load_system(fullfile(WS, 'model', [mdl '.slx']));
cleanup = onCleanup(@() close_system(mdl, 0));     % 不保存，模型零改动

% 导出目标：{模型内路径, 输出文件名}
targets = { ...
    mdl,                                                  'modeld_top'; ...
    [mdl '/01_Scenario_Manager'],                         'modeld_01_scenario'; ...
    [mdl '/02_Ship_Speed_Controller'],                    'modeld_02_speed_pi'; ...
    [mdl '/03_Propulsion_Plant'],                         'modeld_03_propulsion'; ...
    [mdl '/05_EMS_Controller/EMS_SOC_Adaptive_ECMS'],     'modeld_05_ems_law5'; ...
    [mdl '/05_EMS_Controller/EMS_NN_AECMS'],              'modeld_05_ems_law7'; ...
    [mdl '/06_Genset_Average_Plant'],                     'modeld_06_genset'; ...
    [mdl '/10_Fuel_Consumption'],                         'modeld_10_fuel'; ...
    [mdl '/13_DC_Bus'],                                   'modeld_13_dcbus'; ...
    [mdl '/14_Battery_Converter'],                        'modeld_14_battery'; ...
    };

for k = 1:size(targets, 1)
    sys = targets{k, 1};
    out = fullfile(FIGD, [targets{k, 2} '.png']);
    try
        Simulink.BlockDiagram.arrangeSystem(sys);
    catch ME
        fprintf('[export] arrangeSystem(%s) 失败（继续导出）: %s\n', sys, ME.message);
    end
    % 右下角加临时垫块扩展自动打印范围：Gain 三角形等图标超出 Position 矩形，
    % 子系统打印按图元包围盒自动裁剪，PaperPosition/PaperType 对子系统打印均不生效
    % （R2026a 实测）。垫块打印后删除，模型不落盘。
    blks = find_system(sys, 'SearchDepth', 1, 'Type', 'Block');
    x0 = inf; y0 = inf; x1 = -inf; y1 = -inf;
    for i = 1:numel(blks)
        p = get_param(blks{i}, 'Position');
        x0 = min(x0, p(1)); y0 = min(y0, p(2));
        x1 = max(x1, p(3)); y1 = max(y1, p(4));
    end
    pad = add_block('simulink/Sinks/Terminator', [sys '/__print_pad__'], ...
        'Position', [x1 + 100, y1 + 40, x1 + 120, y1 + 60], 'ShowName', 'off');
    print(['-s' sys], '-dpng', '-r150', out);
    delete_block(pad);
    fprintf('[export] %s\n', out);
end
fprintf('[export] 完成，共 %d 张 -> %s\n', size(targets, 1), FIGD);
close_system(mdl, 0);   % 显式放弃内存改动（run() 场景下 onCleanup 不触发）

function s = ensure_bsfc_params(ROOT)
% ecms2.ensure_bsfc_params — ECMS BSFC 表注入（T-103，FR-113 同曲线要求）
%
%  保证 base workspace 存在 BSFC_Breakpoints_kW / BSFC_Table_gkWh
%  （law=4 变体 MATLAB Function 块的常数输入源，design §3.3）。注入链：
%     1) base 已存在 BSFC_Breakpoints_kW 与 BSFC_Table_gkWh -> 直接采用；
%     2) 否则若 config/ecms_bsfc_override.mat 存在（T-004 bsfc_family 生成，
%        tier2 中档聚合曲线，字段 BSFC_Breakpoints_kW/BSFC_Table_gkWh）
%        -> 载入并写入 base；
%     3) 否则用 Model_Parameters 默认表（ASSUMED_BSFC_Breakpoints/Table），
%        并在日志注明（当前仓库无 override 文件时走此路径）。
%  输入: ROOT 工程根目录（model/simulink_modelD；缺省自动探测）
%  输出: s = struct(bp, tbl, source)，bp/tbl 为 1×N 行向量。
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    have1 = evalin('base', ...
        'exist(''BSFC_Breakpoints_kW'', ''var'') && exist(''BSFC_Table_gkWh'', ''var'')');
    if have1
        bp  = evalin('base', 'BSFC_Breakpoints_kW');
        tbl = evalin('base', 'BSFC_Table_gkWh');
        src = 'workspace';
    else
        ovFile = fullfile(ROOT, 'config', 'ecms_bsfc_override.mat');
        if exist(ovFile, 'file')
            ov = load(ovFile, 'BSFC_Breakpoints_kW', 'BSFC_Table_gkWh');
            bp = ov.BSFC_Breakpoints_kW;
            tbl = ov.BSFC_Table_gkWh;
            src = 'config/ecms_bsfc_override.mat';
        elseif evalin('base', 'exist(''ASSUMED_BSFC_Breakpoints'', ''var'')')
            bp  = evalin('base', 'ASSUMED_BSFC_Breakpoints');
            tbl = evalin('base', 'ASSUMED_BSFC_Table');
            src = 'default (ASSUMED_BSFC_*)';
        else
            bp  = [0 100 200 400 600 800 1000];
            tbl = [1200 700 550 480 450 430 453];
            src = 'default (builtin)';
        end
        assignin('base', 'BSFC_Breakpoints_kW', bp(:).');
        assignin('base', 'BSFC_Table_gkWh', tbl(:).');
    end
    if ~strcmp(src, 'workspace')
        fprintf('[ecms2] BSFC 表注入（source=%s）: %d 断点, 首末 BSFC=[%.1f, %.1f] g/kWh\n', ...
            src, numel(bp), tbl(1), tbl(end));
    end
    s = struct('bp', bp(:).', 'tbl', tbl(:).', 'source', src);
end

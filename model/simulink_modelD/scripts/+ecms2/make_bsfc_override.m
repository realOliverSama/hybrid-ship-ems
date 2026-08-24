function ov = make_bsfc_override(ROOT)
% ecms2.make_bsfc_override — 生成 config/ecms_bsfc_override.mat（T-104/105 前置，FR-113 同曲线口径）
%
%  从 results/ecms_phase0/bsfc/bsfc_family.mat 顶层 mid tier2 聚合曲线
%  （BSFC_Breakpoints_kW / BSFC_Table_gkWh / override_tag）复制到
%  config/ecms_bsfc_override.mat，字段名保持一致。
%
%  生效链（两条路径同曲线，Phase 1 既定口径）：
%    1) Model_Parameters.m：override 文件覆盖 ASSUMED_BSFC_*（10_Fuel_Consumption
%       计量 + ASSUMED_k_eq_FEC 派生，design §3.3）；
%    2) ecms2.ensure_bsfc_params：注入 BSFC_Breakpoints_kW/BSFC_Table_gkWh
%       （law=4/5 变体 MATLAB Function 块常数端口输入）。
%
%  override 文件存在期间所有仿真均使用 tier2 曲线（Phase 1 既定口径，不得删除）。
%
%  用法: ov = ecms2.make_bsfc_override();   % ROOT 缺省 model/simulink_modelD
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    srcFile = fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc', 'bsfc_family.mat');
    assert(exist(srcFile, 'file') == 2, 'ECMS2:BSFC', '缺少 %s（先运行 31_build_bsfc_family.m）', srcFile);
    d = load(srcFile, 'BSFC_Breakpoints_kW', 'BSFC_Table_gkWh', 'override_tag');
    need = {'BSFC_Breakpoints_kW', 'BSFC_Table_gkWh', 'override_tag'};
    assert(all(isfield(d, need)), 'ECMS2:BSFC', 'bsfc_family.mat 缺少顶层字段（需 mid tier2 聚合曲线）');
    assert(isequal(size(d.BSFC_Breakpoints_kW), size(d.BSFC_Table_gkWh)), ...
        'ECMS2:BSFC', 'BSFC 断点/表尺寸不一致');
    assert(all(isfinite(d.BSFC_Table_gkWh)) && min(d.BSFC_Table_gkWh) > 0, ...
        'ECMS2:BSFC', 'BSFC 表含非正值');

    ov = struct();
    ov.BSFC_Breakpoints_kW = d.BSFC_Breakpoints_kW;
    ov.BSFC_Table_gkWh     = d.BSFC_Table_gkWh;
    ov.override_tag        = d.override_tag;

    outFile = fullfile(ROOT, 'config', 'ecms_bsfc_override.mat');
    save(outFile, '-struct', 'ov');
    fprintf('[ecms2.make_bsfc_override] 已生成 %s\n', outFile);
    fprintf('  override_tag = %s | %d 断点 (0..%.0f kW) | BSFC 首末=[%.1f, %.1f] g/kWh\n', ...
        ov.override_tag, numel(ov.BSFC_Breakpoints_kW), ov.BSFC_Breakpoints_kW(end), ...
        ov.BSFC_Table_gkWh(1), ov.BSFC_Table_gkWh(end));
end

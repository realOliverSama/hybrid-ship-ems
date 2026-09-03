function fam = bsfc_family(p)
%ecms.bsfc_family — BSFC 曲线族生成器（ECMS Phase 0，T-004，FR-004/FR-005）
%
%  生成内容：
%    1) 一维定速曲线（1500 rpm，单机 P ∈ [0,500] kW）：双锚点（最优 427 /
%       额定 453）+ 低负荷恶化三档 d25 ∈ {0.30, 0.50, 0.70}，pchip 保形插值，
%       采样步长 1 kW；含无过冲处理（全域下限 427、[ξ_opt,1] 单调不降、
%       [0,ξ_opt) 单调不升）。
%    2) 聚合曲线（等分假设）：bsfc_agg(P) = bsfc_unit(P/2)，P ∈ [0,1000] kW，
%       输出断点/表两个向量，供 config/Model_Parameters.m 覆盖 hook 与 DP 使用。
%    3) 二维 map（转速 × 功率，扩展分析专用，不进主 Gate）：
%       bsfc(n,P) = bsfc_1d(P) × (1 + k_n × ((n − n*(P)) / Δn_band)^2)，
%       n ∈ [900,1500] rpm 步长 50；n*(P) = n_lo + (n_hi−n_lo)·(P/500)^α。
%    4) 怠速燃油参数：fuel_flow_idle = P_idle_frac × P_rated × bsfc_rated / 1000
%       ≈ 6.8 kg/h（连续口径 P=0 时使用）。
%
%  自检（FR-005）：锚点偏差 <0.1%；三档满足 d10 > d25 > d50 ≥ 0；全域最小值
%  = 427 且出现在 ξ_opt；[0.8,1.0] 单调不降；[0.1,0.8] 单调不升；pchip 无过冲；
%  聚合等分精确性；二维 map n=1500 切片在 P ≥ ξ_opt×500 段与一维曲线 max 偏差
%  <1%。任一不过即 error('ECMS:BSFC', ...)（非零退出）。
%
%  关于二维切片检查的说明：一维曲线锚定 1500 rpm，而最优转速线 n*(500)=1500、
%  n*(小负荷) < 1500。因此 n=1500 切片仅要求 P ≥ ξ_opt×500 段与一维一致（额定端
%  严格相等）；低功率段 1500 rpm 切片高于一维（偏离最优转速带）属预期，不进判据。
%  二维扩展的真正收益（低负荷降速运行）体现在 min_n bsfc(n,P)，其与一维偏差
%  <0.1%（转速网格 50 rpm 量化），故二维扩展不改变一维主判定结论。
%
%  输入：可选 struct p，字段覆盖下述缺省参数（供后续 config/Ecms_Phase0_Config.m
%  接入；缺省值 = design §3 初值）。
%  输出：struct fam，见代码尾部字段清单（tiers/map2d/怠速燃油/参数/时间戳/版本/
%        override-hook 兼容字段 BSFC_Breakpoints_kW、BSFC_Table_gkWh、override_tag）。

% ==================== 参数（design §3 初值，集中登记） ====================
D.P_unit_rated = 500;    % 单机额定电功率 [kW]（DRAWING_Gen_P_max_perString）
D.n_rated      = 1500;   % 额定转速 [rpm]（一维曲线锚定转速）
D.bsfc_opt     = 427;    % 最优点 BSFC [g/kWh]（潍柴 WH17ME 官方产品页，2026-08-24）
D.bsfc_rated   = 453;    % 额定点 BSFC [g/kWh]（20260526设备参数.docx，453(+5%)）
D.xi_opt       = 0.80;   % 最优负荷率 ξ_opt（最优锚点所在负荷率）
D.bsfc_mid     = 438;    % L=0.90 过渡控制点 [g/kWh]（两锚点线性近似，design §3.1）
D.d25_set      = [0.30 0.50 0.70]; % 低负荷恶化档位 d25（L=0.25 处相对最优点恶化幅度）
D.k_d50        = 0.35;   % d50 = k_d50×d25（L=0.50 恶化系数，design 假定值）
D.k_d10        = 1.60;   % d10 = k_d10×d25（L=0.10 恶化系数，design 假定值）
D.dP_sample    = 1;      % 一维采样步长 [kW]（≤1 kW，requirement）
D.P_idle_frac  = 0.03;   % 怠速燃油 = P_idle_frac × 额定燃油流量（design §3.1）
% ---- 二维 map（扩展分析专用，全部参数化，design §3.2） ----
D.n_lo    = 900;    % 转速下界 [rpm]
D.n_hi    = 1500;   % 转速上界 [rpm]
D.n_step  = 50;     % 转速网格步长 [rpm]
D.alpha   = 0.70;   % 最优转速线指数 n*(P)=n_lo+(n_hi-n_lo)*(P/P_rated)^alpha
D.dN_band = 300;    % 转速偏差特征带宽 Δn_band [rpm]
D.k_n     = 0.10;   % 转速偏离惩罚系数 k_n [-]
D.version = '0.1.0'; % 版本字符串（写入 .mat）

if nargin < 1, p = struct(); end
flds = fieldnames(D);
for i = 1:numel(flds)
    if isfield(p, flds{i}), D.(flds{i}) = p.(flds{i}); end
end
assert(isrow(D.d25_set) && numel(D.d25_set) == 3 && all(diff(D.d25_set) > 0), ...
    'ECMS:BSFC', 'd25_set 必须为 3 个升序档位');

% ==================== 一维曲线（每档） ====================
P_unit = (0 : D.dP_sample : D.P_unit_rated).';     % 单机功率列向量 [0..500] kW
L      = P_unit / D.P_unit_rated;                  % 负荷率
ctrl_L = [0.00 0.10 0.25 0.50 D.xi_opt 0.90 1.00]; % 控制点负荷率（design §3.1）
ctrl_P = ctrl_L * D.P_unit_rated;
% 注：P=0 端点取与 L=0.10 相同的值（平段延伸）。P=0 的实际燃油由
% fuel_flow_idle 单独描述；此处仅保证查表插值在 [0,50] 定义完整且
% [0,ξ_opt] 区间单调不升（design §3.1 的 L<0.8 单调不升保证）。

checks = struct('name', {}, 'ok', {}, 'detail', {});
addChk = @(nm, ok, dt) struct('name', nm, 'ok', ok, 'detail', dt);
tiers = struct([]);

for t = 1:numel(D.d25_set)
    d25 = D.d25_set(t);
    d10 = D.k_d10 * d25;   % 恶化(0.10)
    d50 = D.k_d50 * d25;   % 恶化(0.50)
    y_ctrl = [D.bsfc_opt*(1+d10), D.bsfc_opt*(1+d10), D.bsfc_opt*(1+d25), ...
              D.bsfc_opt*(1+d50), D.bsfc_opt, D.bsfc_mid, D.bsfc_rated];

    % ---- pchip 保形插值（P_unit 为列向量，ppval 返回同形状列向量） ----
    y = ppval(pchip(ctrl_P, y_ctrl), P_unit);

    % ---- 无过冲处理 ----
    % (a) 最优点为全域最小值：逐点下限 427
    y = max(y, D.bsfc_opt);
    % (b) [ξ_opt, 1] 单调不降；违反则对该段改用线性插值（两锚点连线）
    iHi = L >= D.xi_opt;
    if any(diff(y(iHi)) < -1e-9)
        y(iHi) = interp1([D.xi_opt, 1], [D.bsfc_opt, D.bsfc_rated], L(iHi), 'linear');
        fprintf('[ECMS-BSFC][WARN] 档 d25=%.2f：[%.2f,1] 单调性违反，已改用线性插值\n', d25, D.xi_opt);
    end
    % (c) [0, ξ_opt) 单调不升：从左到右运行最小钳制（pchip 保形下应为 no-op）
    if any(diff(y(L < D.xi_opt)) > 1e-9)
        for k = 2:numel(P_unit)
            if L(k) < D.xi_opt && y(k) > y(k-1), y(k) = y(k-1); end
        end
        fprintf('[ECMS-BSFC][WARN] 档 d25=%.2f：[0,%.2f) 单调性违反，已运行最小钳制\n', d25, D.xi_opt);
    end

    % ---- 聚合曲线（等分假设）：bsfc_agg(P)=bsfc_unit(P/2), P∈[0,1000] kW ----
    % 断点取 0:2:1000，每点恰为单机 1 kW 网格的 2 倍，无插值误差。
    agg_P = (0 : 2*D.dP_sample : 2*D.P_unit_rated).';   % [0..1000] kW
    agg_y = interp1(P_unit, y, agg_P/2, 'linear');

    tiers(t).d25             = d25;
    tiers(t).d10             = d10;
    tiers(t).d50             = d50;
    tiers(t).y_ctrl_max      = max(y_ctrl);   % 控制点包络最大值（无过冲判据用）
    tiers(t).bsfc_opt        = D.bsfc_opt;
    tiers(t).bsfc_rated      = D.bsfc_rated;
    tiers(t).unit_BP_kW      = P_unit;        % 单机断点 [0..500] kW
    tiers(t).unit_table_gkWh = y;             % 单机表 [g/kWh]
    tiers(t).agg_BP_kW       = agg_P;         % 聚合断点 [0..1000] kW
    tiers(t).agg_table_gkWh  = agg_y;         % 聚合表 [g/kWh]
end

% ==================== 二维 map（扩展分析专用，design §3.2） ====================
n_grid = (D.n_lo : D.n_step : D.n_hi).';      % 转速网格 [900:50:1500] rpm
n_opt  = D.n_lo + (D.n_hi - D.n_lo) * (P_unit / D.P_unit_rated).^D.alpha; % 最优转速线
map2d.n_rpm     = n_grid;
map2d.P_kW      = P_unit;
map2d.n_opt_rpm = n_opt;
for t = 1:numel(D.d25_set)
    y1d = tiers(t).unit_table_gkWh;                      % 列向量 (P×1)
    dN  = n_grid.' - n_opt;                              % 隐式扩展 (P×n)
    map2d.tiers(t).d25        = D.d25_set(t);
    map2d.tiers(t).table_gkWh = y1d .* (1 + D.k_n * (dN / D.dN_band).^2); % P×n
end

% ==================== 怠速燃油参数（design §3.1） ====================
fuel_rated_kgph = D.P_unit_rated * D.bsfc_rated / 1000;  % 226.5 kg/h
fuel_flow_idle_kgph = D.P_idle_frac * fuel_rated_kgph;   % ≈ 6.795 ≈ 6.8 kg/h

% ==================== 自检（FR-005） ====================
for t = 1:numel(D.d25_set)
    ti = tiers(t);
    y  = ti.unit_table_gkWh;
    iOpt = find(abs(P_unit - D.xi_opt*D.P_unit_rated) < 1e-9, 1);
    iRtd = numel(P_unit);
    % 1) 锚点偏差 < 0.1%
    devOpt = abs(y(iOpt) - D.bsfc_opt) / D.bsfc_opt;
    devRtd = abs(y(iRtd) - D.bsfc_rated) / D.bsfc_rated;
    checks(end+1) = addChk(sprintf('anchor_tier%d_d25%.2f', t, D.d25_set(t)), ...
        devOpt < 1e-3 && devRtd < 1e-3, ...
        sprintf('dev(opt)=%.4f%%, dev(rated)=%.4f%% (<0.1%%)', 100*devOpt, 100*devRtd)); %#ok<AGROW>
    % 2) 三档恶化顺序：d10 > d25 > d50 >= 0
    d10c = interp1(P_unit, y, 0.10*D.P_unit_rated) / D.bsfc_opt - 1;
    d25c = interp1(P_unit, y, 0.25*D.P_unit_rated) / D.bsfc_opt - 1;
    d50c = interp1(P_unit, y, 0.50*D.P_unit_rated) / D.bsfc_opt - 1;
    checks(end+1) = addChk(sprintf('dev_order_tier%d_d25%.2f', t, D.d25_set(t)), ...
        d10c > d25c && d25c > d50c && d50c >= -1e-12, ...
        sprintf('d10=%.4f > d25=%.4f > d50=%.4f >= 0', d10c, d25c, d50c)); %#ok<AGROW>
    % 3) 全域最小值 = 427 且出现在 ξ_opt
    [mn, imn] = min(y);
    checks(end+1) = addChk(sprintf('global_min_tier%d_d25%.2f', t, D.d25_set(t)), ...
        abs(mn - D.bsfc_opt) < 1e-6 && abs(P_unit(imn) - D.xi_opt*D.P_unit_rated) < 1e-6, ...
        sprintf('min=%.6f at P=%.1f kW (expect 427 @ %.0f kW)', mn, P_unit(imn), D.xi_opt*D.P_unit_rated)); %#ok<AGROW>
    % 4) [ξ_opt, 1] 单调不降
    iHi = L >= D.xi_opt;
    checks(end+1) = addChk(sprintf('mono_nondecr_tier%d_d25%.2f', t, D.d25_set(t)), ...
        all(diff(y(iHi)) >= -1e-9), 'BSFC non-decreasing on [0.8,1.0]'); %#ok<AGROW>
    % 5) [0.1, 0.8] 单调不升
    iLo = L >= 0.1 & L <= D.xi_opt;
    checks(end+1) = addChk(sprintf('mono_nonincr_tier%d_d25%.2f', t, D.d25_set(t)), ...
        all(diff(y(iLo)) <= 1e-9), 'BSFC non-increasing on [0.1,0.8]'); %#ok<AGROW>
    % 6) pchip 无过冲：不超控制点包络
    checks(end+1) = addChk(sprintf('no_overshoot_tier%d_d25%.2f', t, D.d25_set(t)), ...
        max(y) <= ti.y_ctrl_max*(1+1e-9), ...
        sprintf('max=%.4f <= ctrl_max=%.4f', max(y), ti.y_ctrl_max)); %#ok<AGROW>
    % 7) 聚合等分精确性：bsfc_agg(2P) == bsfc_unit(P)
    aggCk = interp1(ti.agg_BP_kW, ti.agg_table_gkWh, P_unit*2, 'linear');
    checks(end+1) = addChk(sprintf('agg_exact_tier%d_d25%.2f', t, D.d25_set(t)), ...
        max(abs(aggCk - y)) < 1e-9, ...
        sprintf('bsfc_agg(2P) == bsfc_unit(P) (max dev %.2e)', max(abs(aggCk - y)))); %#ok<AGROW>
    % 8) 二维 map n=1500 切片 vs 一维：P >= ξ_opt×500 段 max 偏差 < 1%
    T   = map2d.tiers(t).table_gkWh;                    % P×n，末列 = n_hi = 1500
    iP  = P_unit >= D.xi_opt * D.P_unit_rated;
    dev = max(abs(T(iP, end) - y(iP)) ./ y(iP));
    checks(end+1) = addChk(sprintf('map_slice1500_tier%d_d25%.2f', t, D.d25_set(t)), ...
        dev < 0.01, ...
        sprintf('max dev vs 1D on P>=%.0f kW: %.4f%% (<1%%)', D.xi_opt*D.P_unit_rated, 100*dev)); %#ok<AGROW>
    % 9) 二维有效成本（对 n 取最小）≈ 一维（转速网格量化偏差，信息性判据）
    eff  = min(T, [], 2);
    dev2 = max(abs(eff - y) ./ y);
    checks(end+1) = addChk(sprintf('map_eff_tier%d_d25%.2f', t, D.d25_set(t)), ...
        dev2 < 0.01, ...
        sprintf('max |min_n bsfc(n,P) - bsfc_1d(P)|/bsfc_1d: %.4f%% (<1%%)', 100*dev2)); %#ok<AGROW>
end
% 10) 怠速燃油 ≈ 6.8 kg/h
checks(end+1) = addChk('fuel_flow_idle', ...
    abs(fuel_flow_idle_kgph - 6.795) / 6.795 < 0.01, ...
    sprintf('idle=%.4f kg/h (expect ~6.8)', fuel_flow_idle_kgph)); %#ok<AGROW>

% ---- 打印自检逐项结果 ----
fprintf('[ECMS-BSFC] 自检（FR-005，共 %d 项）：\n', numel(checks));
for i = 1:numel(checks)
    fprintf('[ECMS-BSFC][CHECK %02d] %-42s -> %s  (%s)\n', i, checks(i).name, ...
        string(checks(i).ok), checks(i).detail);
end
if ~all([checks.ok])
    error('ECMS:BSFC', 'BSFC 自检失败：%d/%d 项未通过（见上方 [CHECK] 行）', ...
        sum(~[checks.ok]), numel(checks));
end
fprintf('[ECMS-BSFC] 自检全部通过（%d 项）。\n', numel(checks));

% ==================== 组装输出 struct ====================
fam.params               = D;                     % 全部参数（含版本字符串）
fam.version              = D.version;             % 版本字符串
fam.generated            = datestr(now, 'yyyy-mm-dd HH:MM:SS');  % 生成时间戳
fam.tiers                = tiers;                 % 三档一维曲线（单机+聚合）
fam.map2d                = map2d;                 % 二维 map（P×n 表 + 最优转速线）
fam.fuel_flow_idle_kgph  = fuel_flow_idle_kgph;   % 怠速燃油流量 [kg/h]
fam.fuel_flow_rated_kgph = fuel_rated_kgph;       % 额定燃油流量 [kg/h]
% ---- override-hook 兼容字段（聚合中档 d25=0.50，供 config/Model_Parameters.m 读取；
%      后续 T-006 据此写 config/ecms_bsfc_override.mat，三档可经 fam.tiers 选取） ----
iMid = find(abs(D.d25_set - 0.50) < 1e-9, 1);
if isempty(iMid), iMid = 2; end
fam.BSFC_Breakpoints_kW = tiers(iMid).agg_BP_kW;       % 聚合断点 [0..1000] kW
fam.BSFC_Table_gkWh     = tiers(iMid).agg_table_gkWh;  % 聚合表 [g/kWh]
fam.override_tag = sprintf('ecms-bsfc-family-mid-d25=%.2f-agg-v%s', ...
    D.d25_set(iMid), D.version);
% ---- 自检结果（供入口脚本打印通过项清单） ----
fam.selfcheck = struct('passed', all([checks.ok]), 'count', numel(checks), ...
    'checks', checks);
end

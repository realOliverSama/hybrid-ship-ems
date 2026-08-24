function [BP_kW, table_gkWh, meta] = speed_cost_min(fam, tierIdx, use2d)
% ecms.speed_cost_min — 二维转速解耦预处理（design §4.4，扩展分析预留接口）
%
%  转速不进状态维（无动态、无约束），对给定 P_g 的最优转速选择只影响当步成本：
%    bsfc_eff(P) = min over n grid of bsfc(n, P)
%  预处理为等效一维成本函数，DP 控制维保持一维。
%
%  主判定链路暂用一维定速曲线（1500 rpm，design §2/§3.2 外部评审修正），
%  此时本函数退化为恒等：直接返回该档聚合一维曲线（bsfc_agg(P)=bsfc_unit(P/2)）。
%  二维 map 仅作扩展分析（T-013），不进入主 Gate。
%
%  输入:
%    fam       T-004 bsfc_family.mat 结构体（tiers(1..3).agg_BP_kW/agg_table_gkWh、
%              map2d.P_kW/n_rpm/tiers(t).table_gkWh(P×n)）
%    tierIdx   档位索引 1..3（d25 = 0.30/0.50/0.70）
%    use2d     false（缺省）= 恒等返回一维聚合曲线；true = min_n 二维有效曲线
%  输出:
%    BP_kW      聚合断点列向量 [kW]（= fam.tiers(tierIdx).agg_BP_kW，0:2:1000）
%    table_gkWh 有效 BSFC 表列向量 [g/kWh]（与 BP_kW 同长度）
%    meta       struct：method（'identity_1d'|'min_over_n_2d'）、tierIdx、
%               n_opt_eff_rpm（仅 use2d 时，聚合 P 网格上的最优转速列向量）
%
%  说明：use2d 时 map2d.P_kW = 0:1:500（单机），聚合 P/2 恰落其网格上，无需插值。
    if nargin < 3 || isempty(use2d)
        use2d = false;
    end
    assert(tierIdx >= 1 && tierIdx <= numel(fam.tiers), 'ECMS:SPEED', 'tierIdx 越界');
    assert(isfield(fam.tiers, 'agg_BP_kW') && isfield(fam.tiers, 'agg_table_gkWh'), ...
        'ECMS:SPEED', 'fam 缺少聚合一维曲线字段（T-004 产物？）');

    BP_kW      = fam.tiers(tierIdx).agg_BP_kW;
    table_gkWh = fam.tiers(tierIdx).agg_table_gkWh;

    if use2d
        assert(isfield(fam, 'map2d') && isfield(fam.map2d, 'tiers'), ...
            'ECMS:SPEED', 'use2d=true 但 fam 缺少 map2d');
        T = fam.map2d.tiers(tierIdx).table_gkWh;     % P_unit × n_rpm
        iP = fam.map2d.P_kW;                          % 0:1:500
        idxUnit = BP_kW / 2 + 1;                      % 聚合断点/2 -> 单机网格索引
        assert(all(idxUnit == round(idxUnit)) && all(idxUnit >= 1) && ...
               all(idxUnit <= numel(iP)), 'ECMS:SPEED', '聚合断点/2 未落单机网格');
        eff_unit = min(T(idxUnit, :), [], 2);         % min over n
        table_gkWh = eff_unit;
        meta = struct('method', 'min_over_n_2d', 'tierIdx', tierIdx, ...
            'n_opt_eff_rpm', fam.map2d.n_rpm(argmin(T(idxUnit, :), [], 2)));
    else
        meta = struct('method', 'identity_1d', 'tierIdx', tierIdx, ...
            'n_opt_eff_rpm', []);
    end
end

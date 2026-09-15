function r = b2_roll_fixed(Pbar, SOC0, dlam_seq, cfg, tbl)
% t445.b2_roll_fixed — 固定 Δλ 序列的代理前向（资格化/反套利夹具，非正式）
% Amendment E：与窗内 DP 同一条实现功率轨迹（燃油+SOC）。
Pbar = Pbar(:);
dlam_seq = dlam_seq(:);
assert(numel(Pbar) == numel(dlam_seq), 'T445:B2:ROLL');
phys = t445.realize_phys(cfg);
soc = SOC0;
fuel = 0;
oob = 0;
K = numel(Pbar);
soc_path = zeros(K + 1, 1);
soc_path(1) = soc;
N = 1;
gen = [];
for k = 1:K
    [fk, soc, N, gen, o1] = t445.realize_bin(Pbar(k), soc, N, gen, dlam_seq(k), ...
        cfg, tbl, phys);
    oob = oob + o1;
    fuel = fuel + fk;
    soc_path(k + 1) = soc;
end
r = struct('fuel_kg', fuel, 'SOC_H', soc, 'soc_path', soc_path, ...
    'oob_query_count', oob);
end

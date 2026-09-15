function r = b2_roll_fixed(Pbar, SOC0, dlam_seq, cfg, tbl)
% t445.b2_roll_fixed — 固定 Δλ 序列的代理前向（资格化/反套利夹具，非正式）
Pbar = Pbar(:);
dlam_seq = dlam_seq(:);
assert(numel(Pbar) == numel(dlam_seq), 'T445:B2:ROLL');
lam0 = cfg.lambda_law.lambda0;
Ksoc = cfg.lambda_law.K_soc;
SOCref = cfg.lambda_law.SOC_ref;
floorL = cfg.lambda_law.floor;
cap = cfg.k_eq.bat_cap_kWh;
soc = SOC0;
fuel = 0;
oob = 0;
K = numel(Pbar);
soc_path = zeros(K + 1, 1);
soc_path(1) = soc;
for k = 1:K
    lam_eff = max(lam0 + Ksoc * (SOCref - soc) + dlam_seq(k), floorL);
    [il, ip, ir, o1] = t445.policy_lookup(tbl, lam_eff, Pbar(k), soc);
    oob = oob + o1;
    fuel = fuel + tbl.fuel_kg(il, ip, ir);
    soc = soc + tbl.dE_kWh(il, ip, ir) / cap;
    soc_path(k + 1) = soc;
end
r = struct('fuel_kg', fuel, 'SOC_H', soc, 'soc_path', soc_path, ...
    'oob_query_count', oob);
end

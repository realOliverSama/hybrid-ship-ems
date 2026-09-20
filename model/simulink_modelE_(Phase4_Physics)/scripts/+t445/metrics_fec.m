function r = metrics_fec(ds, cfg)
% t445.metrics_fec — J_FEC raw 六联（FR-740/743，k_eq^E 路径）
if nargin < 2 || isempty(cfg)
    cfg = phase4f_t445_config();
end
keq = cfg.k_eq.value_kg_per_kWh;
cap = cfg.k_eq.bat_cap_kWh;
fuel = ds.fuel_total_kg(end);
SOC0 = ds.SOC(1);
SOCend = ds.SOC(end);
dSOC = SOCend - SOC0;
corr = (SOC0 - SOCend) * cap * keq;
r = struct();
r.fuel_raw_kg = fuel;
r.SOC0 = SOC0;
r.SOC_end = SOCend;
r.dSOC = dSOC;
r.battery_energy_correction_kg = corr;
r.J_FEC_kg = fuel + corr;
r.k_eq_kg_per_kWh = keq;
r.bat_cap_kWh = cap;
r.k_eq_status = cfg.k_eq.status;
end

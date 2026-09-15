function r = run_t004_tests()
% t445.run_t004_tests — 合成 logsout 已知答案（T-004）
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
cfg = phase4f_t445_config();
ds = local_synth(cfg);

fec = t445.metrics_fec(ds, cfg);
keq = cfg.k_eq.value_kg_per_kWh;
assert(abs(keq - 0.5457) < 1e-12, 'keq_amend_c');
expect_corr = (0.60 - 0.598) * 1071.36 * keq;
expect_J = 10 + expect_corr;
assert(abs(fec.fuel_raw_kg - 10) < 1e-12, 'fuel');
assert(abs(fec.SOC0 - 0.60) < 1e-12, 'soc0');
assert(abs(fec.SOC_end - 0.598) < 1e-12, 'socend');
assert(abs(fec.battery_energy_correction_kg - expect_corr) < 1e-9, 'corr');
assert(abs(fec.J_FEC_kg - expect_J) < 1e-9, 'J');

M = t445.audit_matrix(ds, cfg);
assert(M.closed, 'matrix not closed');
assert(M.n == 12, 'n!=12');
assert(isempty(M.unplanned_not_instrumented));

hf = t445.hard_feasibility(ds, cfg, 'T_frozen_s', ds.t(end));
assert(hf.pass, 'hard feas fail on clean fixture');

ds_bad = ds;
ds_bad.SOC(end) = 0.50;  % |dSOC|=0.10 > 0.005
hf2 = t445.hard_feasibility(ds_bad, cfg, 'T_frozen_s', ds.t(end));
assert(~hf2.pass && ~hf2.items.terminal_soc, 'terminal fail not detected');

r = struct();
r.schema = 't445_t004_evidence_v1';
r.task = 'T-004';
r.purpose = 'SYNTHETIC_SMOKE';
r.config_sha256 = cfg.config_sha256;
r.overall = true;
r.fec = fec;
r.audit_closed = M.closed;
r.hard_pass_clean = hf.pass;
r.hard_fail_terminal = ~hf2.pass;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
outDir = fullfile(ROOT, 'results', 't445_oracle_gate');
fid = fopen(fullfile(outDir, 't004_unit_test_result.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[T-004] overall=true J_FEC=%.6f kg audit_closed=%d\n', fec.J_FEC_kg, M.closed);
end

function ds = local_synth(cfg)
n = 1001;
t = linspace(0, 10, n).';
ds = struct();
ds.t = t;
ds.SOC = linspace(0.60, 0.598, n).';
ds.fuel_total_kg = linspace(0, 10, n).';
ds.P_load_total_kW = 400 * ones(n, 1);
ds.P_gen_actual_kW = 400 * ones(n, 1);
ds.P_gen1_kW = 200 * ones(n, 1);
ds.P_gen2_kW = 200 * ones(n, 1);
ds.P_bat_actual_kW = zeros(n, 1);
ds.P_bat_dis_avail_kW = 500 * ones(n, 1);
ds.P_bat_chg_avail_kW = 500 * ones(n, 1);
ds.V_dc_V = 750 * ones(n, 1);
ds.V_bat_V = 576 * ones(n, 1);
ds.I_bat_A = zeros(n, 1);
ds.state_DG1 = 3 * ones(n, 1);
ds.state_DG2 = 3 * ones(n, 1);
ds.v_ref_mps = 4 * ones(n, 1);
ds.v_ship_mps = 4 * ones(n, 1);
ds.P_bus_residual_kW = zeros(n, 1);
ds.derate08_active = zeros(n, 1);
ds.safety_s1a = zeros(n, 1);
ds.safety_s1b = zeros(n, 1);
ds.safety_s2 = zeros(n, 1);
end

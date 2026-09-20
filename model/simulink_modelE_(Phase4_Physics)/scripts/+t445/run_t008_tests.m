function r = run_t008_tests()
% t445.run_t008_tests — T-008 窗内 DP / 策略表 / V_term / AC-611/612/613
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
cfg = phase4f_t445_config();
assert(strcmp(cfg.k_eq.status, 'CANONICAL'), 'T445:T008:KEQ');
assert(abs(cfg.k_eq.value_kg_per_kWh - 0.5457) < 1e-12, 'T445:T008:KEQV');
assert(abs(cfg.v_term.w_term - 1.17e5) < 1e-6, 'T445:T008:WTERM');
params = ecms2.ecms_params();
tbl = t445.build_policy_table(cfg, params);

cases = struct('name', {}, 'pass', {}, 'detail', {});
c = local_domain(tbl, cfg);
cases(end+1) = c; %#ok<AGROW>
c = local_parity_sample(tbl, params);
cases(end+1) = c; %#ok<AGROW>
c = local_ac611(cfg, tbl);
cases(end+1) = c; %#ok<AGROW>
c = local_ac612(cfg, tbl);
cases(end+1) = c; %#ok<AGROW>
c = local_ac613(cfg, tbl);
cases(end+1) = c; %#ok<AGROW>
c = local_toy_dp(cfg, tbl);
cases(end+1) = c; %#ok<AGROW>
c = local_no_cap_scan(ROOT);
cases(end+1) = c; %#ok<AGROW>

overall = all([cases.pass]);
r = struct();
r.schema = 't445_t008_evidence_v1';
r.task = 'T-008';
r.purpose = 'SYNTHETIC_SMOKE';
r.config_sha256 = cfg.config_sha256;
r.k_eq_status = cfg.k_eq.status;
r.overall = overall;
r.cases = cases;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
outDir = fullfile(ROOT, 'results', 't445_oracle_gate');
fid = fopen(fullfile(outDir, 't008_unit_test_result.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fid = fopen(fullfile(outDir, 'access_ledger.jsonl'), 'a');
fprintf(fid, '%s\n', jsonencode(struct('purpose','SYNTHETIC_SMOKE','task','T-008', ...
    'artifact','t008_unit_test_result.json','utc',r.utc)));
fclose(fid);
fprintf('[T-008] overall=%d n_cases=%d\n', overall, numel(cases));
if ~overall
    error('T445:T008:FAIL', 'T-008 unit tests failed');
end
end

function c = local_domain(tbl, cfg)
ok = tbl.n_lambda == 3020 ...
    && abs(tbl.lambda_table_max - 15.525) < 1e-12 ...
    && abs(tbl.lambda_grid(1) - 0.43) < 1e-12 ...
    && abs(tbl.lambda_grid(end) - 15.525) < 1e-12 ...
    && tbl.n_pbar == 66 ...
    && tbl.derivation_ok ...
    && cfg.b2_policy_table.oob_query_count_required == 0 ...
    && isempty(cfg.lambda_law.upper_cap);
c = struct('name','policy_table_domain','pass',ok, ...
    'detail',sprintf('nL=%d max=%.4f', tbl.n_lambda, tbl.lambda_table_max));
end

function c = local_parity_sample(tbl, params)
% 节点子集 vs ecms_law 容差 0（全表 Q1 归 T-010）
lam = [0.43, 0.80, 2.00, 5.00, 15.525];
pbar = [0, 200, 400, 800, 1300];
socs = [0.60, 0.10, 0.95];
n_ok = 0; n = 0; maxAbs = 0;
for i = 1:numel(lam)
    for j = 1:numel(pbar)
        for k = 1:numel(socs)
            n = n + 1;
            Pg_law = ecms2.ecms_law(pbar(j), socs(k), lam(i), params, 4);
            [il, ip, ir] = t445.policy_lookup(tbl, lam(i), pbar(j), socs(k));
            Pg_t = tbl.Pg(il, ip, ir);
            d = abs(Pg_t - Pg_law);
            maxAbs = max(maxAbs, d);
            if d < 1e-12
                n_ok = n_ok + 1;
            end
        end
    end
end
c = struct('name','policy_table_parity_sample','pass', n_ok == n, ...
    'detail', sprintf('ok=%d/%d maxAbs=%g', n_ok, n, maxAbs));
end

function c = local_ac611(cfg, tbl)
Pbar = 400 * ones(cfg.oracle_window.K, 1);
sol = t445.b2_window_dp(Pbar, 0.60, cfg, tbl, 't0', 0);
fn = fieldnames(sol);
banned = {'P_gen','P_bat','P_g','commitment','P_gen_kW','P_bat_kW'};
hit = intersect(fn, banned);
ok = isempty(hit) && isfield(sol, 'dlambda') && numel(sol.dlambda) == 30;
% 源码动作出口只有 dlambda
src = fileread(fullfile(t445.modele_root(), 'scripts', '+t445', 'b2_window_dp.m'));
ok = ok && contains(src, 'sol.dlambda = dlambda') ...
    && ~contains(src, 'sol.P_gen') && ~contains(src, 'sol.P_bat') ...
    && ~contains(src, 'sol.commitment');
c = struct('name','AC611_action_boundary','pass',ok, ...
    'detail', sprintf('fields=%s', strjoin(fn.', ',')));
end

function c = local_ac612(cfg, tbl)
t0 = 40;
K = 30;
P_full = 300 + 50 * sin((0:99).'/10);
P_win = P_full(1:K);
P_leak = [P_win; 900 * ones(20, 1)];
sol_a = t445.b2_window_dp(P_win, 0.60, cfg, tbl, 't0', t0, 'horizon_s', 300);
sol_b = t445.b2_window_dp(P_leak(1:K), 0.60, cfg, tbl, 't0', t0, 'horizon_s', 300);
ok = isequal(sol_a.dlambda, sol_b.dlambda) ...
    && abs(sol_a.info_window(1) - t0) < 1e-12 ...
    && abs(sol_a.info_window(2) - (t0 + 300)) < 1e-12 ...
    && numel(P_win) * 10 == 300;
c = struct('name','AC612_info_window','pass',ok, ...
    'detail', sprintf('win=[%.0f,%.0f] dlam_equal=%d', ...
    sol_a.info_window(1), sol_a.info_window(2), isequal(sol_a.dlambda, sol_b.dlambda)));
end

function c = local_ac613(cfg, tbl)
K = 30;
Pbar = 400 * ones(K, 1);
SOC0 = 0.58;
tgt = 0.60;
dkeep = 0.10 * ones(K, 1);
dem = -0.10 * ones(K, 1);
r0 = t445.b2_roll_fixed(Pbar, SOC0, dkeep, cfg, tbl);
rem = t445.b2_roll_fixed(Pbar, SOC0, dem, cfg, tbl);
G = r0.fuel_kg - rem.fuel_kg;
a_ok = G > 0;
J0 = t445.b2_objective(r0.fuel_kg, r0.SOC_H, cfg, tgt);
Jem = t445.b2_objective(rem.fuel_kg, rem.SOC_H, cfg, tgt);
b_ok = Jem >= J0 - 1e-6;
sol = t445.b2_window_dp(Pbar, SOC0, cfg, tbl, 'SOC_target', tgt);
exc = max(0, abs(sol.SOC_H - tgt) - cfg.v_term.delta_term);
lin_credit = cfg.k_eq.value_kg_per_kWh * cfg.k_eq.bat_cap_kWh * exc;
pen = cfg.v_term.w_term * exc^2;
if exc > 0
    c_ok = pen + 1e-12 >= 1.9 * lin_credit;
    c_note = 'exceeded';
else
    c_ok = true;
    c_note = 'in_deadzone';
end
ok = a_ok && b_ok && c_ok && sol.oob_query_count == 0;
c = struct('name','AC613_antiarbitrage','pass',ok, ...
    'detail', sprintf('G=%.6f J0=%.6f Jem=%.6f exc=%.4f %s oob=%d', ...
    G, J0, Jem, exc, c_note, sol.oob_query_count));
end

function c = local_toy_dp(cfg, tbl)
Pbar = [400*ones(10,1); 200*ones(10,1); 600*ones(10,1)];
s1 = t445.b2_window_dp(Pbar, 0.60, cfg, tbl, 't0', 0);
s2 = t445.b2_window_dp(Pbar, 0.60, cfg, tbl, 't0', 0);
ok = isequal(s1.dlambda, s2.dlambda) ...
    && s1.oob_query_count == 0 && s2.oob_query_count == 0 ...
    && all(s1.dlambda >= -0.10-1e-12) && all(s1.dlambda <= 0.10+1e-12) ...
    && s1.SOC_H >= 0.10-1e-9 && s1.SOC_H <= 0.95+1e-9 ...
    && numel(s1.dlambda) == 30;
% 无 0.80 上限：λ_raw 可大于 0.80
lam_raw = cfg.lambda_law.lambda0 + cfg.lambda_law.K_soc * ...
    (cfg.lambda_law.SOC_ref - 0.10) + 0.10;
ok = ok && lam_raw > 0.80 && abs(lam_raw - 15.525) < 1e-12;
c = struct('name','toy_dp_determinism','pass',ok, ...
    'detail', sprintf('dlam1=%.4f SOC_H=%.4f J=%.4f lam_raw_max=%.4f', ...
    s1.dlambda(1), s1.SOC_H, s1.J_window_kg, lam_raw));
end

function c = local_no_cap_scan(ROOT)
files = {
    fullfile(ROOT, 'scripts', '+t445', 'b2_window_dp.m')
    fullfile(ROOT, 'scripts', '+t445', 'build_policy_table.m')
    fullfile(ROOT, 'scripts', '+t445', 'law6_lambda_eff.m')
    };
ok = true;
for i = 1:numel(files)
    t = fileread(files{i});
    if contains(t, 'lambda_eff in [0.43, 0.80]') || contains(t, 'min(lambda') ...
            || contains(t, '0.80 cap')
        ok = false;
    end
end
c = struct('name','no_artificial_cap','pass',ok,'detail','scan_ok');
end

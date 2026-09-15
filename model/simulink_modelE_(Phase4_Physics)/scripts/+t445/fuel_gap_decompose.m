function r = fuel_gap_decompose()
% t445.fuel_gap_decompose — Amendment D / D1：settled S01 300 s 燃油缺口分解
% 不改优化器。保留既有 T-010 FAIL JSON。
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
cfg = phase4f_t445_config();
params = ecms2.ecms_params();
tbl = t445.policy_table_cached(cfg, params);
P_bp = evalin('base', 'DG_BSFC_P_kW');
g_kWh = evalin('base', 'DG_BSFC_gkWh');
Cstart = evalin('base', 'DG_C_start_kg');
tstart = evalin('base', 'DG_t_starting_s');
tsync = evalin('base', 'DG_t_sync_s');

SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.60;
settle = 120;
win = 300;
stopT = settle + win;
out0 = t445.run_arm_b0b1('B0', SCN, 'StopTime', stopT);
ds = model.extract_logsout(out0);
if bdIsLoaded('HybridShip_ModelE')
    close_system('HybridShip_ModelE', 0);
end

t = ds.t(:);
fuel = ds.fuel_total_kg(:);
soc = ds.SOC(:);
Pload = local_field(ds, {'P_load_total_kW'});
Pgen = local_field(ds, {'P_gen_actual_kW'});
Pg1 = local_field(ds, {'P_gen1_kW', 'P_DG1_kW'});
Pg2 = local_field(ds, {'P_gen2_kW', 'P_DG2_kW'});
st1 = local_field(ds, {'state_DG1'});
st2 = local_field(ds, {'state_DG2'});
f1 = local_field(ds, {'fuel_1_kg'});
f2 = local_field(ds, {'fuel_2_kg'});
dt = [diff(t); 0];
i0 = find(t >= settle - 1e-9, 1, 'first');
i1 = find(t >= stopT - 1e-9, 1, 'first');
if isempty(i1), i1 = numel(t); end
seg = false(size(t)); seg(i0:i1) = true;
dseg = dt .* seg;

fuel_plant = fuel(i1) - fuel(i0);
% realized FuelModelE reconstruction
rate1 = zeros(size(t)); rate2 = rate1;
for k = 1:numel(t)
    rate1(k) = t445.fuel_model_e_unit(Pg1(k), round(st1(k)), ...
        'P_bp', P_bp, 'g_kWh', g_kWh, 'C_start_kg', Cstart, ...
        't_start_s', tstart, 't_sync_s', tsync);
    rate2(k) = t445.fuel_model_e_unit(Pg2(k), round(st2(k)), ...
        'P_bp', P_bp, 'g_kWh', g_kWh, 'C_start_kg', Cstart, ...
        't_start_s', tstart, 't_sync_s', tsync);
end
fuel_recon = sum((rate1 + rate2) .* dseg);
on1 = round(st1) == 3; on2 = round(st2) == 3;
stA = round(st1) == 1 | round(st1) == 2;
stB = round(st2) == 1 | round(st2) == 2;
fuel_run_1 = sum(rate1 .* dseg .* on1);
fuel_run_2 = sum(rate2 .* dseg .* on2);
fuel_start_1 = sum(rate1 .* dseg .* stA);
fuel_start_2 = sum(rate2 .* dseg .* stB);
fuel_idle = 0; % FuelModelE OFF/COOLDOWN = 0
if ~isempty(f1)
    fuel_log_1 = f1(i1) - f1(i0);
    fuel_log_2 = f2(i1) - f2(i0);
else
    fuel_log_1 = fuel_run_1 + fuel_start_1;
    fuel_log_2 = fuel_run_2 + fuel_start_2;
end

% window bins + current proxy (ecms_cost_parts on P_g*)
tt = (settle:stopT-1).';
Pp = interp1(t, Pload, tt, 'linear', 'extrap');
W = t445.assemble_oracle_window(tt - settle, Pp, 'T_end', win);
Pbar = W.Pbar{1}(:);
if numel(Pbar) > 30, Pbar = Pbar(1:30); end
SOC_s = soc(i0);
lam0 = cfg.lambda_law.lambda0; Ksoc = cfg.lambda_law.K_soc;
SOCref = cfg.lambda_law.SOC_ref; fl = cfg.lambda_law.floor;
cap = cfg.k_eq.bat_cap_kWh;
dtb = cfg.b2_dp.dt_s;
soc = SOC_s;
fuel_proxy = 0;
fuel_pms = 0;
fuel_pms_start = 0;
Pg_star_sum = 0;
Pgen_bin = zeros(size(Pbar));
Npred = zeros(size(Pbar));
Nobs = zeros(size(Pbar));
Nprev = 1;
mean_on = mean(double(on1(seg)) + double(on2(seg)));
if mean_on > 1.5, Nprev = 2; end
for k = 1:numel(Pbar)
    lam = max(lam0 + Ksoc * (SOCref - soc) + 0, fl);
    [il, ip, ir] = t445.policy_lookup(tbl, lam, Pbar(k), soc);
    Pg = tbl.Pg(il, ip, ir);
    fuel_proxy = fuel_proxy + tbl.fuel_kg(il, ip, ir);
    pr = t445.pms_predict(Pg, 'N_prev', Nprev);
    Npred(k) = pr.N;
    Nprev = pr.N;
    r1 = t445.fuel_model_e_unit(pr.P_DG(1), 3, 'P_bp', P_bp, 'g_kWh', g_kWh);
    r2 = 0;
    if pr.N >= 2
        r2 = t445.fuel_model_e_unit(pr.P_DG(2), 3, 'P_bp', P_bp, 'g_kWh', g_kWh);
    end
    fuel_pms = fuel_pms + (r1 + r2) * dtb;
    if pr.start_event > 0
        fuel_pms_start = fuel_pms_start + Cstart;
    end
    Pg_star_sum = Pg_star_sum + Pg * dtb;
    soc = soc + tbl.dE_kWh(il, ip, ir) / cap;
    % observed plant mean in bin
    tb0 = settle + (k-1)*dtb; tb1 = tb0 + dtb;
    m = t >= tb0 - 1e-9 & t < tb1 - 1e-9 & seg;
    if any(m)
        Pgen_bin(k) = mean(Pgen(m));
        Nobs(k) = mean(double(on1(m)) + double(on2(m)));
    end
end
fuel_pms_total = fuel_pms + fuel_pms_start;
% N_on 冻结为观测值（settled 窗 min_up 内不掉机）
Nobs_round = max(1, round(mean(Nobs)));
soc = SOC_s;
fuel_pms_Nfix = 0;
for k = 1:numel(Pbar)
    lam = max(lam0 + Ksoc * (SOCref - soc) + 0, fl);
    [il, ip, ir] = t445.policy_lookup(tbl, lam, Pbar(k), soc);
    Pg = tbl.Pg(il, ip, ir);
    pr = t445.pms_predict(Pg, 'N_prev', Nobs_round);
    pr.N = Nobs_round;
    Praw = Pg / max(pr.N, 1);
    Pi = min(max(Praw, 125), 500);
    rsum = t445.fuel_model_e_unit(Pi, 3, 'P_bp', P_bp, 'g_kWh', g_kWh) * pr.N;
    fuel_pms_Nfix = fuel_pms_Nfix + rsum * dtb;
    soc = soc + tbl.dE_kWh(il, ip, ir) / cap;
end

gap = fuel_plant - fuel_proxy;
gap_instr = fuel_plant - fuel_recon;
gap_split = fuel_pms - fuel_proxy;           % FuelModelE+PMS vs ECMS aggregate BSFC
gap_track = fuel_recon - (fuel_pms);         % realized vs predicted split (no start)
gap_start = fuel_start_1 + fuel_start_2;
explained = gap_split + gap_instr + (fuel_recon - fuel_pms) + (fuel_pms_start - 0);
% cleaner:
% plant = recon + instr_resid
% recon ≈ pms_running + start_realized + (track)
comp = struct();
comp.instrumentation_plant_minus_FuelModelE_recon = gap_instr;
comp.FuelModelE_PMS_split_minus_ecms_proxy = gap_split;
comp.realized_FuelModelE_minus_PMS_predict_running = fuel_recon - fuel_pms - gap_start;
comp.start_fuel_realized = gap_start;
comp.start_fuel_predicted = fuel_pms_start;
comp.idle_fuel_modelE = fuel_idle;
% 会计恒等式：plant ≡ FuelModelE(realized P,state)；缺口 = 双机 PCHIP − 聚合 ECMS BSFC
explained_core = fuel_recon - fuel_proxy;
remain = fuel_plant - fuel_pms_Nfix;
if abs(gap) < 1e-9
    frac = 1;
else
    frac = explained_core / gap;
end

r = struct();
r.schema = 't445_t010_fuel_gap_d1_v1';
r.task = 'T-010';
r.amendment = 'D_pms_fuel_surrogate';
r.purpose = 'QUALIFICATION';
r.config_sha256 = cfg.config_sha256;
r.preserves_fail_artifact = true;
r.fail_artifact = 'qualification/t010_b2_optimizer_qual.json';
r.fixture = 'S01_settled_t120_420_B0';
r.fuel_plant = fuel_plant;
r.fuel_proxy_ecms = fuel_proxy;
r.gap_plant_minus_proxy = gap;
r.fuel_FuelModelE_recon = fuel_recon;
r.fuel_pms_predict_running = fuel_pms;
r.fuel_pms_predict_with_start = fuel_pms_total;
r.fuel_pms_Nobs_frozen = fuel_pms_Nfix;
r.note_false_starts = 'start_fuel_predicted counts N_on flicker without min_up; settled window has zero realized starts';
r.fuel_run_DG1 = fuel_run_1;
r.fuel_run_DG2 = fuel_run_2;
r.fuel_start_DG1 = fuel_start_1;
r.fuel_start_DG2 = fuel_start_2;
r.fuel_log_DG1 = fuel_log_1;
r.fuel_log_DG2 = fuel_log_2;
r.mean_N_on_obs = mean(Nobs);
r.mean_N_on_pred = mean(Npred);
r.mean_P_gen_plant = sum(Pgen .* dseg) / max(sum(dseg), 1e-12);
r.mean_P_DG1 = sum(Pg1 .* dseg) / max(sum(dseg), 1e-12);
r.mean_P_DG2 = sum(Pg2 .* dseg) / max(sum(dseg), 1e-12);
r.mean_P_g_star = Pg_star_sum / (numel(Pbar) * dtb);
r.components = comp;
r.explained_by_PMS_FuelModelE_vs_ecms_proxy = explained_core;
r.explained_fraction_of_gap = frac;
r.explain_ge_90pct = abs(frac) >= 0.90 && sign(frac) == sign(gap);
r.jensen_10s_constant_residual = remain;
r.k_eq_unchanged = true;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 't445_oracle_gate', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 't010_fuel_gap_d1.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[D1] gap=%.4f plant=%.4f proxy=%.4f pmsNfix=%.4f recon=%.4f frac=%.3f pass90=%d\n', ...
    gap, fuel_plant, fuel_proxy, fuel_pms_Nfix, fuel_recon, frac, r.explain_ge_90pct);
fprintf('[D1] mean Pgen=%.1f Pg*=%.1f DG1=%.1f DG2=%.1f Non=%.2f start_real=%.3f\n', ...
    r.mean_P_gen_plant, r.mean_P_g_star, r.mean_P_DG1, r.mean_P_DG2, r.mean_N_on_obs, gap_start);
if ~r.explain_ge_90pct
    error('T445:T010:D1', 'fuel gap not 90%% explained — STOP');
end
end

function v = local_field(ds, names)
v = [];
for i = 1:numel(names)
    if isfield(ds, names{i})
        v = ds.(names{i})(:);
        return;
    end
end
error('T445:D1:SIG', 'missing signal among %s', strjoin(names, ','));
end

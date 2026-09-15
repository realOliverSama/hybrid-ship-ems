function r = qual_b2_optimizer()
% t445.qual_b2_optimizer — T-010 B2_OPTIMIZER_QUALIFICATION Q1~Q6
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
cfg = phase4f_t445_config();
params = ecms2.ecms_params();
tbl = t445.policy_table_cached(cfg, params);
Q = struct('Q1', false, 'Q2', false, 'Q3', false, 'Q4', false, 'Q5', false, 'Q6', false);
detail = struct();

detail.Q1 = local_q1(tbl, params, cfg);
Q.Q1 = detail.Q1.pass;
detail.Q2 = local_q23(cfg, tbl, 'soc');
Q.Q2 = detail.Q2.pass;
detail.Q3 = local_q23(cfg, tbl, 'dlam');
Q.Q3 = detail.Q3.pass;
detail.Q5 = local_q5(cfg, tbl);
Q.Q5 = detail.Q5.pass;
[detail.Q4, detail.Q6] = local_q46(cfg, tbl, ROOT);
Q.Q4 = detail.Q4.pass;
Q.Q6 = detail.Q6.pass;

r = struct();
r.schema = 't445_t010_b2_optimizer_qual_v1';
r.task = 'T-010';
r.purpose = 'QUALIFICATION';
r.config_sha256 = cfg.config_sha256;
r.Q = Q;
r.detail = detail;
r.pass = Q.Q1 && Q.Q2 && Q.Q3 && Q.Q4 && Q.Q5 && Q.Q6;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 't445_oracle_gate', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
% 保留既有 FAIL / E0 / Amendment D：不覆盖旧 JSON
r.amendment = 'E_generator_realization';
fid = fopen(fullfile(qdir, 't010_b2_optimizer_qual_amd_e.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[T-010 Q1-Q6] pass=%d Q1=%d Q2=%d Q3=%d Q4=%d Q5=%d Q6=%d\n', ...
    r.pass, Q.Q1, Q.Q2, Q.Q3, Q.Q4, Q.Q5, Q.Q6);
if ~r.pass
    error('T445:T010:B2QUAL', 'B2 optimizer qualification FAIL');
end
end

function d = local_q1(tbl, params, cfg)
% 全节点容差 0 + 1000 随机点 ≤20 kW + 域覆盖 + OOB=0
n_ok = 0; n = 0; maxAbs = 0;
for il = 1:tbl.n_lambda
    for ip = 1:tbl.n_pbar
        for ir = 1:3
            n = n + 1;
            Pg_t = tbl.Pg(il, ip, ir);
            Pg_l = ecms2.ecms_law(tbl.pbar_grid(ip), tbl.soc_rep(ir), ...
                tbl.lambda_grid(il), params, 4);
            ad = abs(Pg_t - Pg_l);
            maxAbs = max(maxAbs, ad);
            if ad < 1e-12
                n_ok = n_ok + 1;
            end
        end
    end
end
seed = 'phase4f-t445-q1-policy-table-v1';
rng(uint32(mod(sum(double(seed)), 2^31-1)), 'twister');
nR = 1000; nRok = 0; maxR = 0;
for k = 1:nR
    lam = tbl.lambda_grid(randi(tbl.n_lambda));
    Pd = tbl.pbar_grid(randi(tbl.n_pbar));
    ir = randi(3);
    SOC = tbl.soc_rep(ir);
    Pg_l = ecms2.ecms_law(Pd, SOC, lam, params, 4);
    [il, ip, irr] = t445.policy_lookup(tbl, lam, Pd, SOC);
    Pg_t = tbl.Pg(il, ip, irr);
    ad = abs(Pg_t - Pg_l);
    maxR = max(maxR, ad);
    if ad <= 20 + 1e-9
        nRok = nRok + 1;
    end
end
% domain coverage
oob = 0; ncov = 0; nin = 0;
socg = 0.10:0.01:0.95;
dlam = -0.10:0.005:0.10;
lam0 = cfg.lambda_law.lambda0; Ksoc = cfg.lambda_law.K_soc;
SOCref = cfg.lambda_law.SOC_ref; fl = cfg.lambda_law.floor;
for i = 1:numel(socg)
    for j = 1:numel(dlam)
        ncov = ncov + 1;
        le = max(lam0 + Ksoc * (SOCref - socg(i)) + dlam(j), fl);
        [~,~,~,o1] = t445.policy_lookup(tbl, le, 400, socg(i));
        oob = oob + o1;
        if le >= 0.43 - 1e-12 && le <= 15.525 + 1e-12
            nin = nin + 1;
        end
    end
end
d = struct();
d.pass = (n_ok == n) && (nRok == nR) && (oob == 0) && (nin == ncov);
d.nodes_ok = n_ok; d.nodes_n = n; d.nodes_maxAbs = maxAbs;
d.rand_ok = nRok; d.rand_n = nR; d.rand_maxAbs = maxR;
d.oob = oob; d.domain_in = nin; d.domain_n = ncov;
d.LAMBDA_DOMAIN_COVERAGE = (nin == ncov) && (oob == 0);
end

function d = local_q23(cfg, tbl, which)
Pbar = 400 * ones(30, 1);
s0 = t445.b2_window_dp(Pbar, 0.60, cfg, tbl);
if strcmp(which, 'soc')
    s1 = t445.b2_window_dp(Pbar, 0.60, cfg, tbl, 'soc_step', 0.005);
    tol = 0.005;
    name = 'Q2';
else
    s1 = t445.b2_window_dp(Pbar, 0.60, cfg, tbl, 'dlam_step', 0.0025);
    tol = 0.005;
    name = 'Q3'; %#ok<NASGU>
end
den = max(abs(s0.J_window_kg), 1e-12);
rel = abs(s0.J_window_kg - s1.J_window_kg) / den;
d = struct('pass', rel <= tol, 'rel', rel, 'J0', s0.J_window_kg, 'J1', s1.J_window_kg);
end

function d = local_q5(cfg, tbl)
Pbar = [300*ones(10,1); 500*ones(10,1); 200*ones(10,1)];
s1 = t445.b2_window_dp(Pbar, 0.60, cfg, tbl);
s2 = t445.b2_window_dp(Pbar, 0.60, cfg, tbl);
d = struct('pass', isequal(s1.dlambda(1), s2.dlambda(1)) && isequal(s1.dlambda, s2.dlambda), ...
    'dlam1', s1.dlambda(1));
end

function [d4, d6] = local_q46(cfg, tbl, ROOT) %#ok<INUSD>
SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.60;
settle = 120;
win = 300;
stopT = settle + win;
dt = 10;
t0s = (0:dt:(stopT-dt)).';
e0 = [t0s, zeros(size(t0s)), zeros(size(t0s))];
KS = cfg.lambda_law.K_S_B2_num / cfg.lambda_law.K_S_B2_den;

% Q6: B2_ZERO_CORRECTION_SANITY — law=6 K_S=1/30 ê=0，非 law=5
out6 = t445.run_arm_b0b1('B1', SCN, 'StopTime', stopT, 'EHatData', e0, 'K_S', KS);
ds6 = model.extract_logsout(out6);
[inc6, Pbar, SOC_s] = local_window_inc(ds6, settle, stopT);
[gen6, N6] = t445.gen0_from_ds(ds6, settle);
s6 = t445.b2_window_dp(Pbar, SOC_s, cfg, tbl, 'freeze_dlambda', 0, ...
    't0', settle, 'SOC_target', SCN.initial_SOC, 'N0', N6, 'gen0', gen6);
rel6 = abs(s6.fuel_proxy_kg - inc6.fuel) / max(abs(inc6.fuel), 1e-12);
dsoc6 = abs((s6.SOC_H - s6.SOC0) - inc6.dSOC);
d6 = struct('pass', rel6 <= 0.01 && dsoc6 <= 0.002, 'rel', rel6, ...
    'dsoc_err', dsoc6, 'fuel_proxy', s6.fuel_proxy_kg, 'fuel_plant', inc6.fuel, ...
    'name', 'B2_ZERO_CORRECTION_SANITY');

% 冷启动 0–300 s 覆盖（不替代 settled，不决定 PASS）
[incC, PbarC, SOC_c] = local_window_inc(ds6, 0, 300);
[genC, NC] = t445.gen0_from_ds(ds6, 0);
sC = t445.b2_window_dp(PbarC, SOC_c, cfg, tbl, 'freeze_dlambda', 0, ...
    't0', 0, 'SOC_target', SCN.initial_SOC, 'N0', NC, 'gen0', genC);
relC = abs(sC.fuel_proxy_kg - incC.fuel) / max(abs(incC.fuel), 1e-12);
d6.cold_start_diagnostic = struct('fuel_rel', relC, 'fuel_proxy', sC.fuel_proxy_kg, ...
    'fuel_plant', incC.fuel, 'not_used_for_pass', true);

% Q4：同一 settled P̄ 优化后 law=6 回放
s4 = t445.b2_window_dp(Pbar, SOC_s, cfg, tbl, 't0', settle, ...
    'SOC_target', SCN.initial_SOC, 'N0', N6, 'gen0', gen6);
t0w = (settle:dt:(stopT-dt)).';
nd = numel(s4.dlambda);
ehat = [t0w(1:nd), zeros(nd,1), s4.dlambda];
if numel(t0w) > nd
    extra = t0w(nd+1:end);
    ehat = [ehat; [extra, zeros(size(extra)), s4.dlambda(end)*ones(size(extra))]];
end
ehat = [e0(e0(:,1) < settle, :); ehat];
outP = t445.run_arm_b0b1('B1', SCN, 'StopTime', stopT, 'EHatData', ehat, 'K_S', KS);
dsP = model.extract_logsout(outP);
incP = local_window_inc(dsP, settle, stopT);
relf = abs(s4.fuel_proxy_kg - incP.fuel) / max(abs(incP.fuel), 1e-12);
dsoc_err = abs((s4.SOC_H - s4.SOC0) - incP.dSOC);
d4 = struct('pass', relf <= 0.01 && dsoc_err <= 0.002, 'fuel_rel', relf, ...
    'dsoc_err', dsoc_err, 'fuel_proxy', s4.fuel_proxy_kg, 'fuel_plant', incP.fuel);
if bdIsLoaded('HybridShip_ModelE')
    close_system('HybridShip_ModelE', 0);
end
end

function [inc, Pbar, SOC_s] = local_window_inc(ds, t0, t1)
t = ds.t(:);
fuel = ds.fuel_total_kg(:);
soc = ds.SOC(:);
P = ds.P_load_total_kW(:);
i0 = find(t >= t0 - 1e-9, 1, 'first');
i1 = find(t >= t1 - 1e-9, 1, 'first');
if isempty(i1), i1 = numel(t); end
inc = struct('fuel', fuel(i1) - fuel(i0), 'dSOC', soc(i1) - soc(i0));
SOC_s = soc(i0);
tt = (t0:t1-1).';
Pp = interp1(t, P, tt, 'linear', 'extrap');
W = t445.assemble_oracle_window(tt - t0, Pp, 'T_end', t1 - t0);
pb = W.Pbar{1};
Pbar = pb(:);
if numel(Pbar) > 30, Pbar = Pbar(1:30); end
end


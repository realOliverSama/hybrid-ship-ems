function sol = b2_window_dp(Pbar, SOC0, cfg, tbl, varargin)
% t445.b2_window_dp — 窗内 DP。动作仅为 Δλ（AC-611）。无 .slx / law=6 核心改动。
% Amendment E：P_g* 仍来自策略表；成本/SOC 由冻结机组实现积分（爬坡/惯性/PMS）。
% λ_eff = max(λ0+K_soc*(SOC_ref-SOC)+Δλ, 0.43)，无上限。
p = inputParser;
p.addParameter('t0', 0, @isnumeric);
p.addParameter('horizon_s', 300, @isnumeric);
p.addParameter('SOC_target', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('use_vterm', true, @islogical);
p.addParameter('soc_step', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('dlam_step', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('freeze_dlambda', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('N0', 1, @isnumeric);
p.addParameter('gen0', [], @(x) true);
p.parse(varargin{:});
opt = p.Results;
if nargin < 3 || isempty(cfg)
    cfg = phase4f_t445_config();
end
if nargin < 4 || isempty(tbl)
    tbl = t445.build_policy_table(cfg);
end
Pbar = Pbar(:);
K = numel(Pbar);
assert(K >= 1 && K <= cfg.b2_dp.K, 'T445:B2:K', 'K=%d', K);
soc_col = cfg.b2_dp.soc_colon;
sst = soc_col(2);
if ~isempty(opt.soc_step), sst = opt.soc_step; end
soc_grid = soc_col(1):sst:soc_col(3);
if isempty(opt.soc_step)
    assert(numel(soc_grid) == cfg.b2_dp.n_soc, 'T445:B2:NSOC');
end
dst = cfg.delta_lambda.step;
if ~isempty(opt.dlam_step), dst = opt.dlam_step; end
dlam = cfg.delta_lambda.min_kg_per_kWh:dst:cfg.delta_lambda.max_kg_per_kWh;
if isempty(opt.dlam_step)
    assert(numel(dlam) == cfg.delta_lambda.n_levels, 'T445:B2:NDLAM');
end
if ~isempty(opt.freeze_dlambda)
    dlam = opt.freeze_dlambda;
end
nS = numel(soc_grid);
nD = numel(dlam);
BIG = cfg.b2_dp.big_M;
lam0 = cfg.lambda_law.lambda0;
Ksoc = cfg.lambda_law.K_soc;
SOCref = cfg.lambda_law.SOC_ref;
floorL = cfg.lambda_law.floor;
if isempty(opt.SOC_target)
    SOC_target = cfg.v_term.corpus_soc0_machine_assert;
else
    SOC_target = opt.SOC_target;
end
oob = 0;
izero = find(abs(dlam) < 1e-12, 1, 'first');
assert(~isempty(izero), 'T445:B2:DL0');
phys = t445.realize_phys(cfg);
N0 = max(1, min(2, round(opt.N0)));

V_all = zeros(K + 1, nS, 2);
vt = zeros(nS, 1);
if opt.use_vterm
    vt = t445.b2_vterm(soc_grid(:), cfg, SOC_target);
end
V_all(K + 1, :, 1) = vt;
V_all(K + 1, :, 2) = vt;
infeas_step = false(K, 1);

for k = K:-1:1
    Pd = Pbar(k);
    Vnext = squeeze(V_all(k + 1, :, :));
    Vk = BIG * ones(nS, 2);
    step_infeas = true(nS, 2);
    for iN = 1:2
        for i = 1:nS
            SOC = soc_grid(i);
            [bestJ, ~, valid, o1] = local_best(SOC, iN, Pd, dlam, Vnext, soc_grid, ...
                tbl, cfg, phys, BIG, izero);
            oob = oob + o1;
            Vk(i, iN) = bestJ;
            step_infeas(i, iN) = ~valid;
            if ~valid
                [j0, o2] = local_cost(SOC, iN, Pd, 0, Vnext, soc_grid, tbl, cfg, phys, BIG);
                oob = oob + o2;
                Vk(i, iN) = j0;
            end
        end
    end
    V_all(k, :, :) = Vk;
    infeas_step(k) = all(step_infeas(:));
end

soc = SOC0;
N = N0;
gen = opt.gen0;
dlambda = zeros(K, 1);
soc_path = zeros(K + 1, 1);
soc_path(1) = soc;
N_path = zeros(K + 1, 1);
N_path(1) = N;
fuel_proxy = 0;
fallback_n = 0;
for k = 1:K
    Pd = Pbar(k);
    Vnext = squeeze(V_all(k + 1, :, :));
    [bestJ, bestU, valid, o1] = local_best(soc, N, Pd, dlam, Vnext, soc_grid, ...
        tbl, cfg, phys, BIG, izero); %#ok<ASGLU>
    oob = oob + o1;
    if ~valid || infeas_step(k)
        bestU = izero;
        fallback_n = fallback_n + 1;
    end
    dlambda(k) = dlam(bestU);
    [fuel_k, soc, N, gen, o2] = local_realize(Pd, soc, N, gen, dlambda(k), cfg, tbl, phys);
    oob = oob + o2;
    fuel_proxy = fuel_proxy + fuel_k;
    soc = min(max(soc, soc_grid(1)), soc_grid(end));
    soc_path(k + 1) = soc;
    N_path(k + 1) = N;
end
J = t445.b2_objective(fuel_proxy, soc_path(end), cfg, SOC_target);
if ~opt.use_vterm
    J = fuel_proxy;
end

sol = struct();
sol.schema = 't445_b2_window_dp_v1';
sol.dlambda = dlambda;          % 唯一动作（AC-611）
sol.info_window = [opt.t0, opt.t0 + K * cfg.b2_dp.dt_s];  % AC-612: 实际用到的窗
sol.K = K;
sol.SOC0 = SOC0;
sol.SOC_H = soc_path(end);
sol.SOC_target = SOC_target;
sol.fuel_proxy_kg = fuel_proxy;
sol.J_window_kg = J;
sol.oob_query_count = oob;
sol.infeasible_fallback_n = fallback_n;
sol.infeasible_share = fallback_n / K;
sol.config_sha256 = cfg.config_sha256;
sol.diagnostics = struct( ...
    'soc_path', soc_path, ...
    'N_path', N_path, ...
    'N0', N0, ...
    'use_vterm', opt.use_vterm, ...
    'lambda_floor', floorL, ...
    'no_upper_cap', true, ...
    'pms_fuel', true, ...
    'realize_dyn', true, ...
    'dt_internal_s', phys.dt);
end

function [bestJ, bestU, valid, oob] = local_best(SOC, N, Pd, dlam, Vnext, soc_grid, ...
        tbl, cfg, phys, BIG, izero)
nD = numel(dlam);
bestJ = BIG;
bestU = izero;
valid = false;
oob = 0;
for u = 1:nD
    [J, o1] = local_cost(SOC, N, Pd, dlam(u), Vnext, soc_grid, tbl, cfg, phys, BIG);
    oob = oob + o1;
    if J < bestJ - 1e-15
        bestJ = J;
        bestU = u;
        valid = isfinite(J) && J < BIG * 0.5;
    end
end
end

function [J, oob] = local_cost(SOC, N, Pd, dlam_u, Vnext, soc_grid, tbl, cfg, phys, BIG)
[fuel, socn, n2, ~, oob] = local_realize(Pd, SOC, N, [], dlam_u, cfg, tbl, phys);
n2 = max(1, min(2, n2));
if socn < soc_grid(1) - 1e-12 || socn > soc_grid(end) + 1e-12
    J = BIG;
else
    vn = interp1(soc_grid, Vnext(:, n2), socn, 'linear', BIG);
    J = fuel + vn;
end
end

function [fuel, soc1, N1, gen1, oob] = local_realize(Pd, SOC, N, gen, dlam, cfg, tbl, phys)
lam0 = cfg.lambda_law.lambda0;
Ksoc = cfg.lambda_law.K_soc;
SOCref = cfg.lambda_law.SOC_ref;
floorL = cfg.lambda_law.floor;
dt = phys.dt;
nI = max(1, round(phys.dt_bin / dt));
soc = SOC;
Ncur = max(1, min(2, round(N)));
oob = 0;
fuel = 0;
need_eq = isempty(gen);
if need_eq
    gen = struct('st', [0, 0], 'P_ramp', [0, 0], 'P_lag', [0, 0], 't_dwell', [0, 0]);
end
lam_min = tbl.lambda_table_min;
lam_step = tbl.lambda_step;
nL = tbl.n_lambda;
nP = tbl.n_pbar;
pstep = tbl.pbar_step;
Pg_tbl = tbl.Pg;
for i = 1:nI
    lam_eff = max(lam0 + Ksoc * (SOCref - soc) + dlam, floorL);
    o1 = 0;
    if lam_eff < lam_min - 1e-12 || lam_eff > tbl.lambda_table_max + 1e-12
        o1 = 1;
    end
    if Pd < -1e-12 || Pd > tbl.pbar_grid(end) + 1e-12
        o1 = 1;
    end
    oob = oob + o1;
    il = min(max(round((lam_eff - lam_min) / lam_step) + 1, 1), nL);
    ip = min(max(round(Pd / pstep) + 1, 1), nP);
    if soc <= 0.105
        ir = 2;
    elseif soc >= 0.945
        ir = 3;
    else
        ir = 1;
    end
    Pg_star = Pg_tbl(il, ip, ir);
    if Ncur >= 2
        Nn = 2;
    elseif Pg_star > phys.Prated + 1e-9 || Pg_star > phys.P_start + 1e-9
        Nn = 2;
    else
        Nn = 1;
    end
    Pref = zeros(1, 2);
    Praw = Pg_star / Nn;
    Pi = min(max(Praw, phys.Pmin), phys.Prated);
    Pref(1:Nn) = Pi;
    if need_eq
        for u = 1:Nn
            gen.st(u) = 3;
            gen.P_ramp(u) = Pref(u);
            gen.P_lag(u) = Pref(u);
        end
        need_eq = false;
    end
    Ncur = Nn;
    P_act = [0, 0];
    for u = 1:2
        cmd_start = double(u <= Nn);
        cmd_stop = double(u > Nn);
        [gen, P_act(u)] = local_genset(gen, u, Pref(u), cmd_start, cmd_stop, phys, dt);
        fuel = fuel + local_fuel(P_act(u), gen.st(u), phys) * dt;
    end
    P_gen = P_act(1) + P_act(2);
    Pbat = Pd - phys.eta_rect * P_gen;
    if Pbat > 0
        eta_rt = phys.eta_dcdc * phys.eta_dis;
        dE = -Pbat / eta_rt * (dt / 3600);
    elseif Pbat < 0
        eta_rt = phys.eta_dcdc * phys.eta_chg;
        dE = (-Pbat) * eta_rt * (dt / 3600);
    else
        dE = 0;
    end
    soc = soc + dE / phys.cap;
end
soc1 = soc;
N1 = Ncur;
gen1 = gen;
end

function r = local_fuel(P_kW, st, phys)
if st == 3
    Pc = min(max(P_kW, phys.Pmin), phys.Prated);
    idx = min(max(round(Pc) - phys.bsfc_p0 + 1, 1), numel(phys.bsfc_lut));
    r = P_kW * phys.bsfc_lut(idx) / 3.6e6;
elseif st == 1 || st == 2
    r = phys.C_start / (phys.t_start + phys.t_sync);
else
    r = 0;
end
end

function [gen, P_act] = local_genset(gen, u, P_ref, cmd_start, cmd_stop, phys, dt)
st = gen.st(u);
P_ramp = gen.P_ramp(u);
P_lag = gen.P_lag(u);
t_dwell = gen.t_dwell(u);
P_act = 0;
switch st
    case 0
        if cmd_start > 0.5 && cmd_stop < 0.5
            st = 1; t_dwell = 0; P_ramp = 0; P_lag = 0;
        end
    case 1
        t_dwell = t_dwell + dt;
        if t_dwell >= phys.t_start - 1e-12
            st = 2; t_dwell = 0;
        end
    case 2
        t_dwell = t_dwell + dt;
        if t_dwell >= phys.t_sync - 1e-12
            st = 3; t_dwell = 0;
        end
    case 3
        if cmd_stop > 0.5
            st = 4; t_dwell = 0; P_act = 0;
        else
            P_ref_c = min(max(P_ref, phys.Pmin), phys.Prated);
            if P_ramp < P_ref_c
                P_ramp = min(P_ramp + phys.ramp * dt, P_ref_c);
            elseif P_ramp > P_ref_c
                P_ramp = max(P_ramp - phys.ramp * dt, P_ref_c);
            end
            P_lag = P_lag + (P_ramp - P_lag) * dt / phys.tau;
            P_act = min(max(P_lag, 0), phys.Prated);
        end
    case 4
        t_dwell = t_dwell + dt;
        if t_dwell >= phys.t_cool - 1e-12
            st = 0; t_dwell = 0;
        end
    otherwise
        st = 0; t_dwell = 0;
end
gen.st(u) = st;
gen.P_ramp(u) = P_ramp;
gen.P_lag(u) = P_lag;
gen.t_dwell(u) = t_dwell;
if st ~= 3
    P_act = 0;
end
end



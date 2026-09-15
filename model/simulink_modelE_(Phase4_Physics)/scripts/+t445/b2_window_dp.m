function sol = b2_window_dp(Pbar, SOC0, cfg, tbl, varargin)
% t445.b2_window_dp — 窗内 DP。动作仅为 Δλ（AC-611）。无 .slx / law=6 核心改动。
% 状态：SOC（爬坡/惯性按 DGN-F-09 代理简化忽略，故 P_g_prev 不构成转移约束）。
% λ_eff = max(λ0+K_soc*(SOC_ref-SOC)+Δλ, 0.43)，无上限。
p = inputParser;
p.addParameter('t0', 0, @isnumeric);
p.addParameter('horizon_s', 300, @isnumeric);
p.addParameter('SOC_target', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('use_vterm', true, @islogical);
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
soc_grid = soc_col(1):soc_col(2):soc_col(3);
assert(numel(soc_grid) == cfg.b2_dp.n_soc, 'T445:B2:NSOC');
dlam = cfg.delta_lambda.min_kg_per_kWh:cfg.delta_lambda.step:cfg.delta_lambda.max_kg_per_kWh;
assert(numel(dlam) == cfg.delta_lambda.n_levels, 'T445:B2:NDLAM');
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
cap = cfg.k_eq.bat_cap_kWh;
oob = 0;
izero = find(abs(dlam) < 1e-12, 1, 'first');
assert(~isempty(izero), 'T445:B2:DL0');

% terminal
V_all = zeros(K + 1, nS);
if opt.use_vterm
    V_all(K + 1, :) = t445.b2_vterm(soc_grid(:), cfg, SOC_target).';
else
    V_all(K + 1, :) = 0;
end
infeas_step = false(K, 1);

for k = K:-1:1
    Pd = Pbar(k);
    Vnext = V_all(k + 1, :).';
    Vk = BIG * ones(nS, 1);
    step_infeas = true(nS, 1);
    for i = 1:nS
        SOC = soc_grid(i);
        [bestJ, ~, valid, o1] = local_best(SOC, Pd, dlam, Vnext, soc_grid, ...
            tbl, lam0, Ksoc, SOCref, floorL, cap, BIG, izero);
        oob = oob + o1;
        Vk(i) = bestJ;
        step_infeas(i) = ~valid;
        if ~valid
            [j0, o2] = local_cost(SOC, Pd, 0, Vnext, soc_grid, tbl, ...
                lam0, Ksoc, SOCref, floorL, cap, BIG);
            oob = oob + o2;
            Vk(i) = j0;
        end
    end
    V_all(k, :) = Vk.';
    infeas_step(k) = all(step_infeas);
end

% forward: 在真实 SOC 上对 Δλ 再 argmin（用已存 V_{k+1}）
soc = SOC0;
dlambda = zeros(K, 1);
soc_path = zeros(K + 1, 1);
soc_path(1) = soc;
fuel_proxy = 0;
fallback_n = 0;
for k = 1:K
    Pd = Pbar(k);
    Vnext = V_all(k + 1, :).';
    [bestJ, bestU, valid, o1] = local_best(soc, Pd, dlam, Vnext, soc_grid, ...
        tbl, lam0, Ksoc, SOCref, floorL, cap, BIG, izero); %#ok<ASGLU>
    oob = oob + o1;
    if ~valid || infeas_step(k)
        bestU = izero;
        fallback_n = fallback_n + 1;
    end
    dlambda(k) = dlam(bestU);
    lam_eff = max(lam0 + Ksoc * (SOCref - soc) + dlambda(k), floorL);
    [il, ip, ir, o2] = t445.policy_lookup(tbl, lam_eff, Pd, soc);
    oob = oob + o2;
    fuel_proxy = fuel_proxy + tbl.fuel_kg(il, ip, ir);
    soc = soc + tbl.dE_kWh(il, ip, ir) / cap;
    soc = min(max(soc, soc_grid(1)), soc_grid(end));
    soc_path(k + 1) = soc;
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
    'use_vterm', opt.use_vterm, ...
    'lambda_floor', floorL, ...
    'no_upper_cap', true);
end

function [bestJ, bestU, valid, oob] = local_best(SOC, Pd, dlam, Vnext, soc_grid, ...
        tbl, lam0, Ksoc, SOCref, floorL, cap, BIG, izero)
nD = numel(dlam);
bestJ = BIG;
bestU = izero;
valid = false;
oob = 0;
for u = 1:nD
    [J, o1] = local_cost(SOC, Pd, dlam(u), Vnext, soc_grid, tbl, ...
        lam0, Ksoc, SOCref, floorL, cap, BIG);
    oob = oob + o1;
    if J < bestJ - 1e-15
        bestJ = J;
        bestU = u;
        valid = isfinite(J) && J < BIG * 0.5;
    end
end
end

function [J, oob] = local_cost(SOC, Pd, dlam_u, Vnext, soc_grid, tbl, ...
        lam0, Ksoc, SOCref, floorL, cap, BIG)
lam_eff = max(lam0 + Ksoc * (SOCref - SOC) + dlam_u, floorL);
[il, ip, ir, oob] = t445.policy_lookup(tbl, lam_eff, Pd, SOC);
fuel = tbl.fuel_kg(il, ip, ir);
dE = tbl.dE_kWh(il, ip, ir);
socn = SOC + dE / cap;
if socn < soc_grid(1) - 1e-12 || socn > soc_grid(end) + 1e-12
    J = BIG;
else
    vn = interp1(soc_grid, Vnext, socn, 'linear', BIG);
    J = fuel + vn;
end
end

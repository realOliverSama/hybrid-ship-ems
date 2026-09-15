function sol = b2_window_dp(Pbar, SOC0, cfg, tbl, varargin)
% t445.b2_window_dp — 窗内 DP。动作仅为 Δλ（AC-611）。无 .slx / law=6 核心改动。
% 状态：SOC（爬坡/惯性按 DGN-F-09 代理简化忽略，故 P_g_prev 不构成转移约束）。
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
cap = cfg.k_eq.bat_cap_kWh;
oob = 0;
izero = find(abs(dlam) < 1e-12, 1, 'first');
assert(~isempty(izero), 'T445:B2:DL0');
fm = struct('dt', cfg.b2_dp.dt_s, 'C_start', 0.6, ...
    'P_bp', [125 250 375 400 500], 'g_kWh', [600 547 447 427 453]);
if evalin('base', "exist('DG_C_start_kg','var')")
    fm.C_start = evalin('base', 'DG_C_start_kg');
    fm.P_bp = evalin('base', 'DG_BSFC_P_kW');
    fm.g_kWh = evalin('base', 'DG_BSFC_gkWh');
end
N0 = max(1, min(2, round(opt.N0)));

% terminal: V 与 N 无关
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
    Vnext = squeeze(V_all(k + 1, :, :));  % nS x 2
    Vk = BIG * ones(nS, 2);
    step_infeas = true(nS, 2);
    for iN = 1:2
        for i = 1:nS
            SOC = soc_grid(i);
            [bestJ, ~, valid, o1] = local_best(SOC, iN, Pd, dlam, Vnext, soc_grid, ...
                tbl, lam0, Ksoc, SOCref, floorL, cap, BIG, izero, fm);
            oob = oob + o1;
            Vk(i, iN) = bestJ;
            step_infeas(i, iN) = ~valid;
            if ~valid
                [j0, o2] = local_cost(SOC, iN, Pd, 0, Vnext, soc_grid, tbl, ...
                    lam0, Ksoc, SOCref, floorL, cap, BIG, fm);
                oob = oob + o2;
                Vk(i, iN) = j0;
            end
        end
    end
    V_all(k, :, :) = Vk;
    infeas_step(k) = all(step_infeas(:));
end

% forward
soc = SOC0;
N = N0;
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
        tbl, lam0, Ksoc, SOCref, floorL, cap, BIG, izero, fm); %#ok<ASGLU>
    oob = oob + o1;
    if ~valid || infeas_step(k)
        bestU = izero;
        fallback_n = fallback_n + 1;
    end
    dlambda(k) = dlam(bestU);
    lam_eff = max(lam0 + Ksoc * (SOCref - soc) + dlambda(k), floorL);
    [il, ip, ir, o2] = t445.policy_lookup(tbl, lam_eff, Pd, soc);
    oob = oob + o2;
    Pg = tbl.Pg(il, ip, ir);
    pr = t445.pms_predict(Pg, 'N_prev', N);
    fuel_proxy = fuel_proxy + local_pms_fuel(pr, fm);
    N = pr.N;
    soc = soc + tbl.dE_kWh(il, ip, ir) / cap;
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
    'pms_fuel', true);
end

function [bestJ, bestU, valid, oob] = local_best(SOC, N, Pd, dlam, Vnext, soc_grid, ...
        tbl, lam0, Ksoc, SOCref, floorL, cap, BIG, izero, fm)
nD = numel(dlam);
bestJ = BIG;
bestU = izero;
valid = false;
oob = 0;
for u = 1:nD
    [J, o1] = local_cost(SOC, N, Pd, dlam(u), Vnext, soc_grid, tbl, ...
        lam0, Ksoc, SOCref, floorL, cap, BIG, fm);
    oob = oob + o1;
    if J < bestJ - 1e-15
        bestJ = J;
        bestU = u;
        valid = isfinite(J) && J < BIG * 0.5;
    end
end
end

function [J, oob] = local_cost(SOC, N, Pd, dlam_u, Vnext, soc_grid, tbl, ...
        lam0, Ksoc, SOCref, floorL, cap, BIG, fm)
lam_eff = max(lam0 + Ksoc * (SOCref - SOC) + dlam_u, floorL);
[il, ip, ir, oob] = t445.policy_lookup(tbl, lam_eff, Pd, SOC);
Pg = tbl.Pg(il, ip, ir);
pr = t445.pms_predict(Pg, 'N_prev', N);
fuel = local_pms_fuel(pr, fm);
dE = tbl.dE_kWh(il, ip, ir);
socn = SOC + dE / cap;
n2 = max(1, min(2, pr.N));
if socn < soc_grid(1) - 1e-12 || socn > soc_grid(end) + 1e-12
    J = BIG;
else
    vn = interp1(soc_grid, Vnext(:, n2), socn, 'linear', BIG);
    J = fuel + vn;
end
end

function fuel = local_pms_fuel(pr, fm)
fuel = 0;
for i = 1:pr.N
    fuel = fuel + t445.fuel_model_e_unit(pr.P_DG(i), 3, ...
        'P_bp', fm.P_bp, 'g_kWh', fm.g_kWh) * fm.dt;
end
if pr.start_event > 0
    fuel = fuel + fm.C_start;
end
end

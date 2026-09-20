function tbl = build_policy_table(cfg, params)
% t445.build_policy_table — λ×P̄×SOC-region 穷举，ecms_law 逐位复制品（DGN-F-09）
% λ 轴 0.43:0.005:15.525（n=3020）由冻结原语导出，禁止只硬编码派生值。
if nargin < 1 || isempty(cfg)
    cfg = phase4f_t445_config();
end
if nargin < 2 || isempty(params)
    params = ecms2.ecms_params();
end
pt = cfg.b2_policy_table;
remax = pt.lambda0 + pt.K_soc * (pt.SOC_ref - pt.SOC_DP_min) + pt.delta_lambda_max;
assert(abs(remax - pt.lambda_table_max) < 1e-12, 'T445:B2:LAMAX');
assert(abs(pt.lambda_table_max - 15.525) < 1e-12, 'T445:B2:15p525');
lam = pt.lambda_colon(1):pt.lambda_colon(2):pt.lambda_colon(3);
assert(numel(lam) == pt.n_lambda && numel(lam) == 3020, 'T445:B2:NLAM');
pbar = pt.pbar_colon_kW(1):pt.pbar_colon_kW(2):pt.pbar_colon_kW(3);
assert(numel(pbar) == pt.n_pbar && numel(pbar) == 66, 'T445:B2:NPBAR');
soc_rep = [0.60; 0.10; 0.95];   % normal / no_discharge / no_charge
nL = numel(lam); nP = numel(pbar); nR = 3;
Pg = zeros(nL, nP, nR);
fuel_kg = zeros(nL, nP, nR);
dE_kWh = zeros(nL, nP, nR);
dt = cfg.b2_dp.dt_s;
eta_dcdc = cfg.b2_dp.eta_dcdc;
eta_chg = cfg.b2_dp.eta_chg;
eta_dis = cfg.b2_dp.eta_dis;
grid = params.grid_kW(:);
nG = numel(grid);
for ip = 1:nP
    Pd = pbar(ip);
    for ir = 1:nR
        SOC = soc_rep(ir);
        mf = zeros(nG, 1);
        mbe = zeros(nG, 1);
        Pbat = zeros(nG, 1);
        feas = true(nG, 1);
        for g = 1:nG
            Pbat(g) = Pd - params.eta_rect * grid(g);
            if SOC <= params.SOC_dis_max && Pbat(g) > 1e-9
                feas(g) = false;
                continue;
            end
            if SOC >= params.SOC_chg_min && Pbat(g) < 0
                feas(g) = false;
                continue;
            end
            [mf(g), mbe(g)] = ecms2.ecms_cost_parts(grid(g), Pd, params);
        end
        if SOC <= params.SOC_dis_max
            % ecms_law: 禁放电取满足需求的最小网格点，与 λ 无关
            gBest = find(feas, 1, 'first');
            if isempty(gBest)
                if Pd > params.eta_rect * grid(end)
                    gBest = nG;
                    [mf(gBest), mbe(gBest)] = ecms2.ecms_cost_parts(grid(gBest), Pd, params);
                    Pbat(gBest) = Pd - params.eta_rect * grid(gBest);
                else
                    gBest = 1;
                end
            end
            Pg(:, ip, ir) = grid(gBest);
            fuel_kg(:, ip, ir) = mf(gBest) * dt;
            if Pbat(gBest) > 0
                eta_b = eta_dis;
            else
                eta_b = eta_chg;
            end
            dE_kWh(:, ip, ir) = ecms.bat_energy_delta_kWh(Pbat(gBest), eta_dcdc, eta_b, dt);
            continue;
        end
        for il = 1:nL
            J = inf(nG, 1);
            J(feas) = mf(feas) + lam(il) * mbe(feas);
            [~, gBest] = min(J);
            if ~isfinite(J(gBest))
                gBest = 1;
            end
            Pg(il, ip, ir) = grid(gBest);
            fuel_kg(il, ip, ir) = mf(gBest) * dt;
            if Pbat(gBest) > 0
                eta_b = eta_dis;
            else
                eta_b = eta_chg;
            end
            dE_kWh(il, ip, ir) = ecms.bat_energy_delta_kWh(Pbat(gBest), eta_dcdc, eta_b, dt);
        end
    end
end
tbl = struct();
tbl.schema = 't445_b2_policy_table_v1';
tbl.lambda_grid = lam(:);
tbl.pbar_grid = pbar(:);
tbl.soc_rep = soc_rep;
tbl.soc_regions = {'normal'; 'no_discharge_le_0.105'; 'no_charge_ge_0.945'};
tbl.Pg = Pg;
tbl.fuel_kg = fuel_kg;
tbl.dE_kWh = dE_kWh;
tbl.n_lambda = nL;
tbl.n_pbar = nP;
tbl.lambda_table_min = pt.lambda_table_min;
tbl.lambda_table_max = pt.lambda_table_max;
tbl.lambda_step = pt.lambda_table_step;
tbl.pbar_step = pt.pbar_colon_kW(2);
tbl.oob_query_count = 0;
tbl.config_sha256 = cfg.config_sha256;
tbl.derivation_ok = abs(remax - pt.lambda_table_max) < 1e-12;
end

function [fuel, soc1, N1, gen1, oob] = realize_bin(Pd, SOC, N, gen, dlam, cfg, tbl, phys)
% t445.realize_bin — 一个 10 s 决策箱的冻结机组实现积分（Amendment E）
%
% 决策间隔仍为 10 s：Pbar、Δλ 箱内恒定。内部 dt=phys.dt（默认 1 s）只传播
% 冻结 Model-E 爬坡/惯性/PMS 均分/FuelModelE，以及由实现功率得到的电池 SOC。
% 策略表语义不变：仍只提供聚合 P_g*。
% gen 空：以该箱首拍 P_g* 的 PMS 均分作为 ON 机组平衡初态（无植物初值时）。
lam0 = cfg.lambda_law.lambda0;
Ksoc = cfg.lambda_law.K_soc;
SOCref = cfg.lambda_law.SOC_ref;
floorL = cfg.lambda_law.floor;
cap = phys.cap;
dt = phys.dt;
nI = max(1, round(phys.dt_bin / dt));
soc = SOC;
Ncur = max(1, min(2, round(N)));
oob = 0;
fuel = 0;
if isempty(gen)
    gen = local_empty_gen();
    need_eq = true;
else
    need_eq = false;
end
for i = 1:nI
    lam_eff = max(lam0 + Ksoc * (SOCref - soc) + dlam, floorL);
    [il, ip, ir, o1] = t445.policy_lookup(tbl, lam_eff, Pd, soc);
    oob = oob + o1;
    Pg_star = tbl.Pg(il, ip, ir);
    pr = t445.pms_predict(Pg_star, 'N_prev', Ncur, ...
        'P_min', phys.Pmin, 'P_rated', phys.Prated, 'P_start_th', phys.P_start);
    if need_eq
        gen = local_eq_gen(pr.N, pr.P_DG);
        need_eq = false;
    end
    Ncur = pr.N;
    P_act = [0, 0];
    for u = 1:2
        cmd_start = double(u <= pr.N);
        cmd_stop = double(u > pr.N);
        Pref = 0;
        if u <= pr.N
            Pref = pr.P_DG(u);
        end
        [gen, P_act(u)] = local_genset(gen, u, Pref, cmd_start, cmd_stop, phys, dt);
        fuel = fuel + local_fuel(P_act(u), gen.st(u), phys) * dt;
    end
    P_gen = P_act(1) + P_act(2);
    Pbat = Pd - phys.eta_rect * P_gen;
    if Pbat > 0
        eta_b = phys.eta_dis;
    else
        eta_b = phys.eta_chg;
    end
    dE = ecms.bat_energy_delta_kWh(Pbat, phys.eta_dcdc, eta_b, dt);
    soc = soc + dE / cap;
end
soc1 = soc;
N1 = Ncur;
gen1 = gen;
end

function gen = local_empty_gen()
gen = struct('st', [0, 0], 'P_ramp', [0, 0], 'P_lag', [0, 0], 't_dwell', [0, 0]);
end

function gen = local_eq_gen(N, Pref)
gen = local_empty_gen();
N = max(0, min(2, round(N)));
for u = 1:N
    gen.st(u) = 3;
    gen.P_ramp(u) = Pref(u);
    gen.P_lag(u) = Pref(u);
    gen.t_dwell(u) = 0;
end
end

function [gen, P_act] = local_genset(gen, u, P_ref, cmd_start, cmd_stop, phys, dt)
% GensetUnitE 功率链/状态机，dt 替换硬编码 0.01 s（E3 授权内部步）
st = gen.st(u);
P_ramp = gen.P_ramp(u);
P_lag = gen.P_lag(u);
t_dwell = gen.t_dwell(u);
P_act = 0;
switch st
    case 0
        if cmd_start > 0.5 && cmd_stop < 0.5
            st = 1;
            t_dwell = 0;
            P_ramp = 0;
            P_lag = 0;
        end
    case 1
        t_dwell = t_dwell + dt;
        if t_dwell >= phys.t_start - 1e-12
            st = 2;
            t_dwell = 0;
        end
    case 2
        t_dwell = t_dwell + dt;
        if t_dwell >= phys.t_sync - 1e-12
            st = 3;
            t_dwell = 0;
        end
    case 3
        if cmd_stop > 0.5
            st = 4;
            t_dwell = 0;
            P_act = 0;
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
            st = 0;
            t_dwell = 0;
        end
    otherwise
        st = 0;
        t_dwell = 0;
end
gen.st(u) = st;
gen.P_ramp(u) = P_ramp;
gen.P_lag(u) = P_lag;
gen.t_dwell(u) = t_dwell;
if st ~= 3
    P_act = 0;
end
end

function r = local_fuel(P_kW, st, phys)
if st == 3
    Pc = min(max(P_kW, phys.Pmin), phys.Prated);
    if isfield(phys, 'bsfc_lut')
        idx = min(max(round(Pc) - phys.bsfc_p0 + 1, 1), numel(phys.bsfc_lut));
        bsfc = phys.bsfc_lut(idx);
    else
        bsfc = interp1(phys.P_bp, phys.g_kWh, Pc, 'pchip');
    end
    r = P_kW * bsfc / 3.6e6;
elseif st == 1 || st == 2
    r = phys.C_start / (phys.t_start + phys.t_sync);
else
    r = 0;
end
end

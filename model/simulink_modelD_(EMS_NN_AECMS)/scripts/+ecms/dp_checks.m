function chk = dp_checks(prob, sol, prof, cfg, tierIdx)
% ecms.dp_checks — DP 内置正确性校验（FR-010，T-009，不可绕过）
%
%  校验项：
%    1) 能量闭合 < 0.1%（沿 DP 轨迹重算电量平衡，design §4.1 效率链）：
%       - 母线平衡：Σ(η_rect·P_g + P_bat − P_demand)·dt/3600，相对需求能量 < 0.1%
%       - 电池记账：ΔE_store(由 P_bat 重算) 与 SOC 轨迹隐含 ΔE 一致 < 0.1%
%    2) Baseline 可行性预检 + 条件 DP ≤ Baseline（design §5 / FR-010）：
%       先验证 Baseline P_gen 轨迹在当前 DP 约束集（P_min_stable/爬坡/电池限值/
%       SOC 界）下可行；可行则强制 fuel_dp ≤ fuel_base_corr（违反即 error）；
%       不可行则跳过并标记 baseline_infeasible_conservative=true（保守比较方向）。
%       主比较口径 = SOC 修正燃油（design §5）：raw 与 corr 同时输出。
%    3) 非平凡性：SOC 轨迹贴下限（≤ SOC_min+ε）时间占比 > 50% -> trivial_suspect
%       （供 gate 判据使用，不 error）。
%
%  输入: prob/sol dp_problem/dp_solve 输出；prof 剖面（Baseline 参照）；
%        cfg 配置；tierIdx BSFC 档位（选 fuel_base_tier<tierIdx>_kg）。
%  输出: chk 结构体（字段见代码尾部清单）。
%  失败语义：硬校验违反 -> error('ECMS:DPCHECK', ...)（非零退出，不静默放行）。
    if nargin < 5 || isempty(tierIdx)
        tierIdx = prob.tierIdx;
    end
    assert(isfield(sol, 'feasible') && sol.feasible, 'ECMS:DPCHECK', ...
        'dp_checks 仅对可行解执行');
    assert(~isempty(sol.P_g_kW), 'ECMS:DPCHECK', 'sol 轨迹为空');

    dt = prob.dt_s;
    N  = prob.N;
    chk = struct();

    % ==================== 1) 能量闭合（沿 DP 轨迹重算） ====================
    Edem_kWh = sum(prob.P_demand_kW) * dt / 3600;                  % 需求总能量
    Ein_kWh  = sum(prob.eta_rect .* sol.P_g_kW) * dt / 3600;       % 机组送入母排
    Ebat_bus_kWh = sum(sol.P_bat_kW) * dt / 3600;                  % 电池母线侧净输出
    bus_resid = (Ein_kWh + Ebat_bus_kWh - Edem_kWh) / Edem_kWh;    % 恒等式（防实现错误）
    % 电池记账重算：由 P_bat 轨迹独立重算 ΔE_store，与 SOC 轨迹隐含 ΔE 对比
    E_store = zeros(N + 1, 1);
    E_store(1) = prob.SOC0 * prob.Cap_kWh;
    for k = 1:N
        E_store(k + 1) = E_store(k) + ...
            ecms.bat_energy_delta_kWh(sol.P_bat_kW(k), prob.eta_dcdc, prob.eta_bat, dt);
    end
    dE_soc = (sol.SOC(N + 1) - sol.SOC(1)) * prob.Cap_kWh;         % SOC 轨迹隐含 ΔE
    book_resid = (E_store(N + 1) - E_store(1) - dE_soc) / Edem_kWh;
    closure_pass = abs(bus_resid) < cfg.energy_closure_tol && ...
                   abs(book_resid) < cfg.energy_closure_tol;
    chk.energy_closure = struct('Edem_kWh', Edem_kWh, 'Ein_kWh', Ein_kWh, ...
        'Ebat_bus_kWh', Ebat_bus_kWh, 'bus_residual_frac', bus_resid, ...
        'bookkeeping_residual_frac', book_resid, 'pass', closure_pass);

    % ==================== 2) Baseline 可行性预检 + 条件 DP ≤ Baseline ====================
    fld = sprintf('fuel_base_tier%d_kg', tierIdx);
    assert(isfield(prof, fld), 'ECMS:DPCHECK', 'prof 缺少 %s', fld);
    fuel_base_raw = prof.(fld);
    SOC_end_base  = prof.SOC(end);
    k_eq = cfg.k_eq_bsfc_gkWh / (cfg.eta_rect * cfg.eta_dcdc * cfg.eta_bat) / 1000; % [kg/kWh]
    fuel_base_corr = fuel_base_raw + (cfg.SOC0 - SOC_end_base) * cfg.Bat_Cap_kWh * k_eq;

    Pg_base = prof.P_gen_kW;
    tolK = cfg.base_feas_tol_kW;
    feas_issues = {};
    if any(Pg_base > prob.P_max + tolK)
        feas_issues{end+1} = sprintf('P_gen>P_max(%.1f) %d 点', prob.P_max, sum(Pg_base > prob.P_max + tolK)); %#ok<AGROW>
    end
    if strcmp(prob.mode, 'continuous')
        if any(Pg_base < prob.P_min_stable - tolK)
            feas_issues{end+1} = sprintf('连续口径 P_gen<P_min_stable(%.0f) %d 点', ...
                prob.P_min_stable, sum(Pg_base < prob.P_min_stable - tolK)); %#ok<AGROW>
        end
    else
        loBad = Pg_base > tolK & Pg_base < prob.P_min_stable - tolK;
        if any(loBad)
            feas_issues{end+1} = sprintf('启停口径 P_gen∈(0,P_min_stable) %d 点', sum(loBad)); %#ok<AGROW>
        end
    end
    if any(abs(diff(Pg_base)) > prob.R_gen * dt + tolK)
        feas_issues{end+1} = sprintf('|ΔP_gen|>R·Δt(%.0f) %d 点', ...
            prob.R_gen * dt, sum(abs(diff(Pg_base)) > prob.R_gen * dt + tolK)); %#ok<AGROW>
    end
    if any(abs(prof.P_bat_kW) > prob.Bat_P_limit + tolK)
        feas_issues{end+1} = sprintf('|P_bat|>限值(%.0f) %d 点', ...
            prob.Bat_P_limit, sum(abs(prof.P_bat_kW) > prob.Bat_P_limit + tolK)); %#ok<AGROW>
    end
    if any(prof.SOC < prob.SOC_min - tolK) || any(prof.SOC > prob.SOC_max + tolK)
        feas_issues{end+1} = 'Baseline SOC 越界'; %#ok<AGROW>
    end

    chk.baseline = struct('fuel_raw_kg', fuel_base_raw, 'fuel_corr_kg', fuel_base_corr, ...
        'SOC_end', SOC_end_base, 'k_eq_kg_kWh', k_eq, ...
        'feasible', isempty(feas_issues), 'issues', {feas_issues}, ...
        'infeasible_conservative', ~isempty(feas_issues));

    if chk.baseline.feasible
        tolChk = cfg.base_check_tol_abs + cfg.base_check_tol_rel * fuel_base_corr;
        chk.dp_le_baseline = (sol.fuel_total_kg <= fuel_base_corr + tolChk);
        if ~chk.dp_le_baseline
            error('ECMS:DPCHECK', ...
                'DP 燃油 %.4f kg > Baseline 修正燃油 %.4f kg（同物理可行域下违反 FR-010）', ...
                sol.fuel_total_kg, fuel_base_corr);
        end
    else
        chk.dp_le_baseline = NaN;    % 跳过（保守方向：DP 约束强于 Baseline）
    end

    % ==================== 3) 非平凡性（SOC 贴下限滑行） ====================
    frac_low = mean(sol.SOC(1:N) <= prob.SOC_min + cfg.trivial_soc_eps);
    chk.trivial = struct('frac_soc_at_floor', frac_low, ...
        'threshold', cfg.trivial_frac_thr, ...
        'trivial_suspect', frac_low > cfg.trivial_frac_thr);

    % ==================== 汇总（硬校验违反均已 error，到达此处即全过） ====================
    if ~closure_pass
        error('ECMS:DPCHECK', '能量闭合残差超限：bus=%.2e book=%.2e（<%.1f%%）', ...
            bus_resid, book_resid, 100 * cfg.energy_closure_tol);
    end
    chk.pass = true;
    chk.saving_pct = (fuel_base_corr - sol.fuel_total_kg) / fuel_base_corr * 100;
end

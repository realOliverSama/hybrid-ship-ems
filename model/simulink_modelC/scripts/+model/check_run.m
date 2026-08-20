function out = check_run(ds, SCN, mode, stopTimeLimit)
% model.check_run — 单次航次完整性检查（烟雾测试共用）
% 返回 struct: ok(逻辑), results(逐项 cell), details(字符串)
    out = struct('ok', false, 'details', '');
    R = {}; ok = true;
    function push(nm, pass, det)
        R{end+1, 1} = nm; %#ok<AGROW>
        R{end, 2} = pass; %#ok<AGROW>
        R{end, 3} = det;  %#ok<AGROW>
        if ~pass, ok = false; end %#ok<NASGU>
    end
    % 需要 evalin base 中的参数
    Gen_P_max = evalin('base', 'Gen_P_max');
    Mot_P_nom = evalin('base', 'Mot_P_nom');
    ASSUMED_Bat_DischargeLimit = evalin('base', 'ASSUMED_Bat_DischargeLimit');
    ASSUMED_Bat_ChargeLimit = evalin('base', 'ASSUMED_Bat_ChargeLimit');
    ASSUMED_SOC_min = evalin('base', 'ASSUMED_SOC_min');
    ASSUMED_SOC_max = evalin('base', 'ASSUMED_SOC_max');

    t = ds.t; dt = t(2) - t(1);
    names = fieldnames(ds);
    % --- 1. 无 NaN/Inf ---
    bad = {};
    for i = 1:numel(names)
        if strcmp(names{i}, 't'), continue; end
        if any(~isfinite(ds.(names{i})))
            bad{end+1} = names{i}; %#ok<AGROW>
        end
    end
    push('no_nan_inf', isempty(bad), sprintf('非有限信号: %s', strjoin(bad, ',')));
    % --- 2. 船速有界不发散 ---
    vmax = max(abs(ds.v_ship_mps));
    vbound = max(SCN.target_speed_mps, 1) * 1.5 + 2;
    push('speed_bounded', vmax < vbound, sprintf('max|v|=%.2f < %.2f', vmax, vbound));
    % --- 3. SOC 有效范围 ---
    socmin = min(ds.SOC); socmax = max(ds.SOC);
    push('soc_range', socmin >= ASSUMED_SOC_min - 0.01 && socmax <= ASSUMED_SOC_max + 0.01, ...
        sprintf('SOC=[%.3f, %.3f]', socmin, socmax));
    % --- 4. 发电机/电池功率不越界 ---
    gmax = max(ds.P_gen_actual_kW); gmin = min(ds.P_gen_actual_kW);
    bmax = max(ds.P_bat_actual_kW); bmin = min(ds.P_bat_actual_kW);
    push('gen_bounds', gmax <= Gen_P_max + 1 && gmin >= -1, ...
        sprintf('P_gen=[%.1f, %.1f]', gmin, gmax));
    push('bat_bounds', bmax <= ASSUMED_Bat_DischargeLimit + 1 && bmin >= -ASSUMED_Bat_ChargeLimit - 1, ...
        sprintf('P_bat=[%.1f, %.1f]', bmin, bmax));
    % --- 5. 达到固定航程并停止 ---
    xend = ds.x_ship_m(end);
    push('voyage_reached', xend >= 0.98 * SCN.voyage_distance_m, ...
        sprintf('x_end=%.1f / %.0f m', xend, SCN.voyage_distance_m));
    push('stopped_early', t(end) < stopTimeLimit - 1, ...
        sprintf('t_end=%.1f s < %.0f s', t(end), stopTimeLimit));
    % --- 6. 功率平衡误差（稳态 < 1%） ---
    nlast = max(round(numel(t) * 0.1), 1);
    ebal = abs(ds.e_balance_kW(end-nlast+1:end));
    pload = mean(ds.P_load_total_kW(end-nlast+1:end));
    erel = max(ebal) / max(pload, 1);
    push('power_balance', erel < 0.01, sprintf('max|e_balance|/P_load=%.4f (<1%%)', erel));
    % --- 7. 可用功率限幅不变式 ---
    inv = max(ds.P_prop_actual_kW - ds.P_prop_limit_kW);
    push('limit_invariant', inv <= 1, sprintf('max(P_prop_actual-P_prop_limit)=%.2f kW', inv));
    % --- 8. 从静止起步 ---
    push('start_from_rest', abs(ds.v_ship_mps(1)) < 1e-6 && abs(ds.x_ship_m(1)) < 1e-6, ...
        sprintf('v(0)=%.4f, x(0)=%.4f', ds.v_ship_mps(1), ds.x_ship_m(1)));
    % --- 9. 达到巡航速度（考虑推进电机额定功率限制与阻力倍率） ---
    % v2.0 标定：Hull_K_R 已含海况裕量，静水可达相对水速 = V_design·(1+SeaMargin)^(1/3)
    V_design_mps = evalin('base', 'V_design_mps');
    ASSUMED_Sea_Margin = evalin('base', 'ASSUMED_Sea_Margin');
    multEnd = max(ds.resistance_multiplier(end), 0.05);
    v_pl = V_design_mps * (1+ASSUMED_Sea_Margin)^(1/3) / multEnd^(1/3) + SCN.river_current_mps; % 功率受限对地速度
    targetEff = min(SCN.target_speed_mps, v_pl);
    vss = mean(ds.v_ship_mps(end-nlast+1:end));
    push('cruise_reached', vss > 0.9 * targetEff, ...
        sprintf('v_ss=%.3f / 目标%.3f m/s (功率受限可达 %.2f m/s)', vss, SCN.target_speed_mps, v_pl));
    out.ok = ok;
    out.results = R;
end

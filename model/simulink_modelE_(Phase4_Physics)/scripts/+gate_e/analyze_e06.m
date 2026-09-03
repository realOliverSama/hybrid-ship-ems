function E = analyze_e06(ds)
    save(fullfile(tempdir, "e06_latest.mat"), "ds");   % Gate 环境数据留证（调试后移除）
% gate_e.analyze_e06 — E06 时序/C_start/缺口分析（tick 判据换算秒，0.01 s 采样）
    st2 = ds.state_DG2; t = ds.t;
    t_on_idx = find(st2 == 1, 1);
    t_sync_idx = find(st2 == 2, 1);
    t_run_idx = find(st2 == 3, 1);
    t_cool_idx = find(st2 == 4, 1);
    t_off_idx = find(st2 == 0 & t > t(t_run_idx(1)), 1);   % 时间值比较（非索引）
    if isempty(t_off_idx) || isempty(t_cool_idx)
        seq = sprintf('t=%.1f:st=%d; ', t(1:10:end), st2(1:10:end));
        save(fullfile(tempdir, 'e06_debug.mat'), 'ds');
        error('GATE:E06_sequence_incomplete：state 序列不完整（state4 或 OFF 未出现，t_end=%.2f s）。ds 已存 tempdir/e06_debug.mat', t(end));
    end
    E.t_starting_s = t(t_sync_idx(1)) - t(t_on_idx(1));
    E.t_sync_s = t(t_run_idx(1)) - t(t_sync_idx(1));
    E.t_cooldown_s = t(t_off_idx(1)) - t(t_cool_idx(1));
    E.state2_unique = mat2str(unique(st2));
    E.t_state4_first = t(t_cool_idx(1));
    E.P_during_startup_kW = ds.P_DG2_kW(t_on_idx(1):t_run_idx(1)-1);
    E.C_start_measured_kg = ds.fuel_2_kg(t_run_idx(1)) - ds.fuel_2_kg(t_on_idx(1));
    iOn = find(st2 == 3);
    P_on_vals = ds.P_DG2_kW(iOn);                 % ON 拍值序列（不假设连续——cmd_start 持续时存在多段 ON）
    below_on_vals = ds.below_min_DG2(iOn);
    E.P_on_kW = P_on_vals;
    E.below_min_on = below_on_vals;
    % ON 态稳段 = ON 拍值序列的末 20%（爬坡过渡段 P<P_min 为 registered 过渡，不入判据）
    nOn = numel(iOn);
    iSteady = max(1, round(0.8 * nOn)):nOn;
    E.P_on_steady_kW = P_on_vals(iSteady);
    E.below_min_steady = below_on_vals(iSteady);
end

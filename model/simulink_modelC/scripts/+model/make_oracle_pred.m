function D = make_oracle_pred(MDL, SCN, outMatPath, horizonS, dtS, dsIn)
% model.make_oracle_pred — 生成未来窗口负荷峰值 oracle 预测表（Phase 1）
% 输入:
%   MDL        模型名（已加载，如 'HybridShip_ModelC_DCBus'）
%   SCN        工况结构体（config/Model_Scenarios.m）
%   outMatPath 输出 .mat 路径（可选；省略则仅返回 D）
%   horizonS   预测窗口 [s]（默认 60）
%   dtS        重采样周期 [s]（默认 1）
%   dsIn       可选：直接提供 C0 的 extract_logsout 结果（复用 E0 仿真，
%              避免重复跑 C0；省略则内部自跑一次 C0）
% 输出:
%   D          N×3 = [t, P_peak, t_peak_rel]
%              t          航次内绝对时间 [s]（dtS 网格，默认 1 Hz）
%              P_peak     (t, t+horizonS] 窗口内负荷峰值 [kW]
%              t_peak_rel 峰值相对偏移 [s]（窗口内首个最大值位置，0~horizonS）
% 方法: C0 (EMS_CTRL_LAW=0) 全航程仿真 → extract_logsout → resample_1s → 滑窗
% 约定: 与 05_export_dataset/write_split 的 P_load_future_max_60s 同源（1 Hz
%       resample_1s），但窗口为开区间 (t, t+60] 且区分"首个最大值位置"（t_peak_rel），
%       供 sched_ff 定时前馈使用。
% 末端处理: 航程末尾不足 horizonS 时窗口截断到末端，t_peak_rel ≤ 剩余时长，
%           全程有值不填 NaN（闭环需要每个时刻都有预测）。
    if nargin < 4, horizonS = 60; end
    if nargin < 5, dtS = 1; end

    if nargin >= 6 && ~isempty(dsIn)
        ds = dsIn;
        fprintf('[make_oracle_pred] %s (复用 C0 ds, horizon=%d s) ...\n', ...
            SCN.name, horizonS);
    else
        stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1200;
        fprintf('[make_oracle_pred] %s (C0, stop=%.0f s, horizon=%d s) ...\n', ...
            SCN.name, stopTimeLimit, horizonS);
        out = model.run_scenario_sim(MDL, SCN, 0, stopTimeLimit);
        ds = model.extract_logsout(out);
    end
    % 重采样到 dtS 网格（分段常数：取每 bin 内最后值，与 resample_1s 语义一致）
    t0 = ds.t;
    t = (0:dtS:floor(t0(end)))';
    P_raw = ds.P_load_total_kW;
    P = zeros(numel(t), 1);
    for k = 1:numel(t)
        m = find(t0 <= t(k) + 1e-9, 1, 'last');
        if isempty(m), m = 1; end
        P(k) = P_raw(m);
    end
    n = numel(t);

    P_peak = zeros(n, 1);
    t_peak_rel = zeros(n, 1);
    for i = 1:n
        win_idx = find(t > t(i) & t <= t(i) + horizonS);
        if isempty(win_idx)
            P_peak(i) = P(i);
            t_peak_rel(i) = 0;
            continue;
        end
        win = P(win_idx);
        pmax = max(win);
        kfirst = find(win == pmax, 1, 'first');
        P_peak(i) = pmax;
        t_peak_rel(i) = t(win_idx(kfirst)) - t(i);
    end
    D = [t, P_peak, t_peak_rel];

    if nargin >= 3 && ~isempty(outMatPath)
        p = fileparts(outMatPath);
        if ~isempty(p) && ~exist(p, 'dir'), mkdir(p); end
        save(outMatPath, 'D');
    end

    fprintf('[make_oracle_pred] %s: N=%d 行, t_end=%.0f s\n', SCN.name, n, t(end));
    fprintf('  P_peak: [%.1f, %.1f] kW\n', min(P_peak), max(P_peak));
    fprintf('  t_peak_rel: mean=%.1f s, median=%.1f s, <5s 占比=%.1f%%\n', ...
        mean(t_peak_rel), median(t_peak_rel), 100 * mean(t_peak_rel < 5));
end

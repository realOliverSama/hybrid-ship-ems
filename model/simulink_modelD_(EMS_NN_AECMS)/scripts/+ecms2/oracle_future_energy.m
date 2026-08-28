function [t, E_future, e_hat, info] = oracle_future_energy(profile, H, doCheck)
% ecms2.oracle_future_energy — Oracle 前瞻信号预计算（T-106，FR-107，design §3.2）
%
%   E_future_H(t) = ∫_t^{t+H} P_demand(τ)dτ   （1 s 网格，尾端零填充）
%   Ē_H           = mean_t E_future_H(t)         （全航程均值，标幺基数）
%   E_scale       = std_t  E_future_H(t)         （全航程标准差，标幺基数）
%   ê_future(t)   = (E_future_H(t) − Ē_H) / E_scale   （无量纲，前瞻归一化信号）
%
%  语义（design §3.2）：未来高能量需求 -> ê_future 高 -> λ 升高 -> 保留电池。
%  E_future 单位为 kWh（P_demand [kW] × dt [s] / 3600）；ê_future 无量纲。
%
%  **红线（E5，FR-107）**：本信号由真实未来需求剖面（oracle）离线预计算，
%  仅用于 law=6 上界/增量验证；**非实时可实现**，报告与代码注释中不得表述为可在线获得。
%
%  输入:
%    profile  剖面数据（三选一）：
%             - struct：含 t / P_demand_kW 字段（1 s 均匀网格，如
%               results/ecms_phase1/profiles/S20_composite_voyage.mat）
%             - char/string：上述 .mat 路径
%             - 数值矩阵 N×2：[t, P_demand_kW]
%    H        前瞻时域 [s]（缺省 300，design §3.2）
%    doCheck  逻辑：执行独立手算积分交叉验证（缺省 true；10 个随机时刻，
%             相对误差 <0.1% 才过，否则 error）
%  输出:
%    t        1 s 网格列向量 [s]（= profile.t）
%    E_future E_future_H(t) 列向量 [kWh]
%    e_hat    ê_future(t) 列向量 [-]
%    info     结构：E_bar_H_kWh / E_scale_kWh / H_s / n / data（[t E_future e_hat]
%             N×3，可直接赋给 ECMS_EHat_Data 供 From Workspace）/ check 明细
%
%  用法:
%    S = load('results/ecms_phase1/profiles/S20_composite_voyage.mat');
%    [t, Ef, eh, info] = ecms2.oracle_future_energy(S);      % 默认 H=300 + 交叉验证
%    assignin('base', 'ECMS_EHat_Data', info.data);
    if nargin < 2 || isempty(H), H = 300; end
    if nargin < 3 || isempty(doCheck), doCheck = true; end

    % ---- 输入解析：struct / 路径 / N×2 矩阵 ----
    if ischar(profile) || isstring(profile)
        profile = load(profile);
    end
    if isstruct(profile)
        assert(isfield(profile, 't') && isfield(profile, 'P_demand_kW'), ...
            'ECMS2:ORACLE', 'profile struct 需含 t / P_demand_kW 字段');
        t = profile.t(:);
        Pd = profile.P_demand_kW(:);
    else
        assert(size(profile, 2) >= 2, 'ECMS2:ORACLE', ...
            '数值输入须为 N×2 矩阵 [t, P_demand_kW]');
        t = profile(:, 1);
        Pd = profile(:, 2);
    end
    n = numel(t);
    assert(n >= H + 2, 'ECMS2:ORACLE', ...
        '剖面长度 %d < H+2=%d（至少一个完整前瞻窗 + 尾端零填充演示点）', n, H + 2);
    % 网格校验（1 s 均匀网格，design §3.2）
    dt = diff(t);
    assert(all(abs(dt - 1) < 1e-9), 'ECMS2:ORACLE', ...
        'E_future 预计算要求 1 s 均匀时间网格（当前 dt 非均匀）');

    % ---- E_future_H(t)：滑窗 H 样本积分（尾端零填充，P_demand 超界视为 0）----
    %   c = cumsum([0; Pd])（长度 n+1，c(n+1)=Σ全剖面）；E(k)=c(min(k+H,n+1))−c(k)
    c = cumsum([0; Pd]);
    j2 = min((1:n).' + H, n + 1);
    E_future = (c(j2) - c(1:n)) / 3600;       % [kWh]（P_demand kW × 1 s / 3600）

    % ---- 标幺基数与 ê_future ----
    E_bar = mean(E_future);
    E_scale = std(E_future);
    assert(E_scale > 1e-9, 'ECMS2:ORACLE', ...
        'E_future 全航程标准差为 0（剖面常数？）——无法标幺，Escalate');
    e_hat = (E_future - E_bar) / E_scale;

    info = struct();
    info.H_s = H;
    info.E_bar_H_kWh = E_bar;
    info.E_scale_kWh = E_scale;
    info.n = n;
    info.source = 'offline oracle (真实未来需求剖面，非实时可实现，E5)';
    info.data = [t, E_future, e_hat];

    % ---- 交叉验证：随机 10 时刻独立手算积分比对（<0.1%）----
    if doCheck
        info.check = crossValidate(t, Pd, E_future, H, n);
        worst = info.check.max_rel_err;
        fprintf('[ecms2.oracle_future_energy] 交叉验证：%d 个随机时刻，最大相对误差=%.6g%%（阈值 0.1%%）\n', ...
            10, 100 * worst);
        if worst >= 1e-3
            error('ECMS2:ORACLE:CHECK', ...
                'E_future 交叉验证最大相对误差 %.6g%% ≥ 0.1%%（实现自检失败）', 100 * worst);
        end
    end
end

function chk = crossValidate(t, Pd, E_future, H, n)
% 独立参考实现（不复用滑窗 cumsum 路径）：显式循环逐样本求和，
% 用于交叉验证 E_future 数值正确性。取 10 个完整前瞻窗内随机时刻。
    rng(42, 'twister');          % 固定种子，可复现
    iMax = n - H;                % k+H-1 ≤ n 的完整窗时刻数（t 从 0 起）
    idx = sort(randi(iMax, 10, 1));
    worst = 0;
    times = zeros(10, 1);
    refs = zeros(10, 1);
    vals = zeros(10, 1);
    rels = zeros(10, 1);
    for q = 1:10
        k = idx(q);
        s = 0;
        for j = 0:H - 1
            s = s + Pd(k + j);          % 独立积分：显式循环累加
        end
        ref = s / 3600;                 % [kWh]
        val = E_future(k);
        rel = abs(val - ref) / max(abs(ref), 1e-9);
        times(q) = t(k); refs(q) = ref; vals(q) = val; rels(q) = rel;
        if rel > worst, worst = rel; end
    end
    fprintf('[ecms2.oracle_future_energy] 交叉验证样本（t, E_future, 手算, rel%%）：\n');
    for q = 1:10
        fprintf('    t=%6.0f s  E_future=%.4f kWh  手算=%.4f kWh  rel=%.6g%%\n', ...
            times(q), vals(q), refs(q), 100 * rels(q));
    end
    chk = struct('n_samples', 10, 'indices', idx, 'times_s', times, ...
        'E_future_kWh', vals, 'ref_kWh', refs, 'rel_err', rels, ...
        'max_rel_err', worst);
end

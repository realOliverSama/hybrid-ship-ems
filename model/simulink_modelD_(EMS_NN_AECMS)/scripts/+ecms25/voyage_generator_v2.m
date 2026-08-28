function voyages = voyage_generator_v2(seed, N)
% ecms25.voyage_generator_v2 — 数据集 v2 参数化随机航次生成器（Phase 2.5 T-302，FR-301）
%
% 依据：.specs/nn-predictive-ecms-phase25/t301_dataset_v2_perturbation.md
%   M1 段型速度随机化：各段型速度由 v1 固定值改为范围随机（范围见 BLK2）
%   M2 水流幅值/方向扩展：upstream −[0.6,1.2] m/s，其余段 ±[0.3] m/s（段级恒定）
%   M3 航次级阻力乘子 k_res ∈ [0.95,1.15]（航次内恒定）
%   M0 拒绝硬规则（逐段判定）：
%     V_rel = v_mps − c_mps（upstream 段 c<0 ⇒ V_rel = v + |c|）
%     P_calc_kW = k_res · Hull_K_R · V_rel³ / eta_chain / 1000
%     非 upstream 段 P_calc > 700 kW ⇒ 整航次拒绝重抽
%     upstream 段 V_rel > 7.06 m/s ⇒ 整航次拒绝重抽（逆水段允许饱和，v1 语义）
%
% 段型积木（type, v_kn 范围[kn], 时长[min], 水流[m/s]）：
%   depart(1)  [2.5 3.5] [3 6]     [-0.3 0.3]
%   harbor(2)  [4.5 5.5] [5 12]    [-0.3 0.3]
%   eco(3)     [7 9]     [10 25]   [-0.3 0.3]
%   rush(4)    [10 11.5] [8 18]    [-0.3 0.3]
%   bridge(5)  [4.5 5.5] [6 12]    [-0.3 0.3]
%   upstream(6)[10 11.5] [6 12]    [-1.2 -0.6]
%   berth(7)   [2.5 3.5] [3 6]     [-0.3 0.3]
% 组合规则与 v1 一致：离泊开头、靠泊结尾，中间随机 3~6 段，upstream ≤1 段/航次，
% 总时长 60~120 min（越界整航次拒绝重抽）。
%
% 抽取顺序（单随机流 rng(seed,'twister')，顺序固定保证可复现）：
%   ① nMid = randi([3 6])
%   ② midTypes = MID_TYPES(randi(5,1,nMid))；upstream 计数>1 ⇒ 整航次重抽
%   ③ 各段时长 U[min max]（按段序循环）
%   ④ 总时长越界 ⇒ 整航次重抽
%   ⑤ 各段 v_kn U[min max]（按段序循环）；v_mps = round(v_kn*0.5144*1e4)/1e4
%   ⑥ 各段 c_mps U[min max]，round(c*1e4)/1e4
%   ⑦ k_res = round((0.95 + rand()*0.20)*1e4)/1e4
%   ⑧ M0 预检（V_rel / P_calc 逐段）⇒ 不合格整航次重抽
% 任何拒绝均回到①重新抽取（随机流继续前进），与 v1 行为一致。
%
% 输出：voyages 1×N struct 数组，字段与 v1 完全同名，另增：
%   voyage 级：k_res
%   seg 级：v_kn（实际抽取值，round 3 位）、P_calc_kW、P_deliv_est_kW
%           （= min(P_calc, Mot_P_nom) + hotel_kW，送达功率口径 v1.1）
% 时间路点约定（与 +ecms2/voyage_s20.m / v1 一致）：
%   v_time_s = [b0 b1 ... bN]，v_value_mps = [v1 ... vN vN]。
    if nargin < 1 || isempty(seed), seed = 20260829; end
    if nargin < 2 || isempty(N), N = 400; end

    % ---- 航段积木库 v2（速度/水流为范围；struct 构造：cell 值分发）----
    BLK2 = struct( ...
        'type',    {'depart','harbor','eco','rush','bridge','upstream','berth'}, ...
        'label',   {'离泊机动','港区限速','经济巡航','赶班巡航','桥区限速','逆水段','靠泊机动'}, ...
        'v_kn',    {[2.5 3.5], [4.5 5.5], [7 9], [10 11.5], [4.5 5.5], [10 11.5], [2.5 3.5]}, ...
        'dur_min', {[3 6], [5 12], [10 25], [8 18], [6 12], [6 12], [3 6]}, ...
        'current_mps', {[-0.3 0.3], [-0.3 0.3], [-0.3 0.3], [-0.3 0.3], [-0.3 0.3], [-1.2 -0.6], [-0.3 0.3]});
    MID_TYPES = 2:6;                 % 中间段可抽取类型（港区/经济/赶班/桥区/逆水）

    kn2mps = 0.5144;
    eta_chain = 0.97 * 0.95 * 0.60 * 0.97;   % 效率链（与 v1 相同表达式）
    P_limit_kW = 850;                        % 记录字段（v1 约定，仅记录用）
    P_sat_kW   = 700;                        % M0 硬规则：非 upstream 段拒绝阈值
    V_rel_up_max = 7.06;                     % M0 硬规则：upstream 段 V_rel 包络上限
    hotel_kW = 100;                          % 日用负荷（S20 hotel_kW）
    ACCEL = 0.15;                            % 快加速（与 v1 相同）
    DECEL = 0.05;                            % 标准减速率（与 v1 相同）
    T_MIN_MIN = 60; T_MAX_MIN = 120;         % 总时长约束 [min]

    % Hull_K_R / Mot_P_nom 取自 config/Model_Parameters.m（base）；未加载时回退默认
    if evalin('base', 'exist(''Hull_K_R'', ''var'')')
        Hull_K_R = evalin('base', 'Hull_K_R');
    else
        Hull_K_R = 1780;
    end
    if evalin('base', 'exist(''Mot_P_nom'', ''var'')')
        Mot_P_nom = evalin('base', 'Mot_P_nom');
    else
        Mot_P_nom = 700;
    end

    % ---- 单随机序列（种子固定 -> 可复现）----
    rng(seed, 'twister');

    voyages = struct('id', {}, 'name', {}, 'seed', {}, 'n_seg', {}, ...
        'duration_s', {}, 'duration_min', {}, 'seg', {}, ...
        'v_time_s', {}, 'v_value_mps', {}, 'current_time_s', {}, 'current_value_mps', {}, ...
        'accel_ramp_mps2', {}, 'decel_ramp_mps2', {}, ...
        'P_max_est_kW', {}, 'P_limit_kW', {}, 'draw_attempts', {}, 'k_res', {});
    for i = 1:N
        attempts = 0;
        while true
            attempts = attempts + 1;
            % ① 中间段数
            nMid = randi([3 6]);
            % ② 中间段类型（有放回）；upstream 最多 1 段
            midTypes = MID_TYPES(randi(numel(MID_TYPES), 1, nMid));
            if sum(midTypes == 6) > 1
                continue;
            end
            types = [1, midTypes, 7];                        % 离泊 ... 靠泊
            % ③ 各段时长 U[min max]
            dursMin = zeros(1, numel(types));
            for j = 1:numel(types)
                dm = BLK2(types(j)).dur_min;
                dursMin(j) = dm(1) + rand() * (dm(2) - dm(1));
            end
            % ④ 总时长越界 -> 整航次拒绝重抽
            if sum(dursMin) < T_MIN_MIN || sum(dursMin) > T_MAX_MIN
                continue;
            end
            % ⑤ 各段 v_kn U[min max]（M1）；v_mps 4 位小数（v1 同精度约定）
            vKn = zeros(1, numel(types));
            vMps = zeros(1, numel(types));
            for j = 1:numel(types)
                vr = BLK2(types(j)).v_kn;
                vk = vr(1) + rand() * (vr(2) - vr(1));
                vKn(j) = round(vk * 1e3) / 1e3;               % 实际抽取值，round 3 位
                vMps(j) = round(vKn(j) * kn2mps * 1e4) / 1e4;  % round 4 位
            end
            % ⑥ 各段 c_mps U[min max]（M2）；round 4 位
            cMps = zeros(1, numel(types));
            for j = 1:numel(types)
                cr = BLK2(types(j)).current_mps;
                cMps(j) = round((cr(1) + rand() * (cr(2) - cr(1))) * 1e4) / 1e4;
            end
            % ⑦ k_res 航次级（M3）
            k_res = round((0.95 + rand() * 0.20) * 1e4) / 1e4;
            % ⑧ M0 预检（逐段；对舍入后 vMps/cMps 判定，保证 manifest 可复现）
            ok = true;
            for j = 1:numel(types)
                V_rel = vMps(j) - cMps(j);
                P_calc = k_res * Hull_K_R * V_rel^3 / eta_chain / 1000;
                if types(j) == 6
                    if V_rel > V_rel_up_max
                        ok = false; break;
                    end
                else
                    if P_calc > P_sat_kW
                        ok = false; break;
                    end
                end
            end
            if ~ok
                continue;
            end
            break;
        end
        durS = round(dursMin * 60);                            % [s]，整数秒
        tt = cumsum([0, durS]);                                % N+1 个时间边界
        seg = struct('type', {}, 'label', {}, 'v_kn', {}, 'v_mps', {}, ...
            'dur_s', {}, 'current_mps', {}, 'P_calc_kW', {}, 'P_deliv_est_kW', {});
        P_deliv = zeros(1, numel(types));
        for j = 1:numel(types)
            V_rel = vMps(j) - cMps(j);
            P_calc = k_res * Hull_K_R * V_rel^3 / eta_chain / 1000;
            P_deliv(j) = min(P_calc, Mot_P_nom) + hotel_kW;
            seg(j) = struct('type', BLK2(types(j)).type, 'label', BLK2(types(j)).label, ...
                'v_kn', vKn(j), 'v_mps', vMps(j), 'dur_s', durS(j), ...
                'current_mps', cMps(j), 'P_calc_kW', P_calc, 'P_deliv_est_kW', P_deliv(j));
        end
        v = struct('id', i, 'name', sprintf('voyage_%03d', i), ...
            'seed', seed, 'n_seg', numel(types), ...
            'duration_s', tt(end), 'duration_min', tt(end) / 60, ...
            'accel_ramp_mps2', ACCEL, 'decel_ramp_mps2', DECEL, ...
            'P_max_est_kW', max(P_deliv), 'P_limit_kW', P_limit_kW, ...
            'draw_attempts', attempts, 'k_res', k_res);
        v.seg = seg;                                          % 1×n_seg struct 数组
        v.v_time_s          = tt;                             % N+1 边界（S20 约定）
        v.v_value_mps       = [vMps, vMps(end)];              % N 段值 + 尾保持
        v.current_time_s    = tt;
        v.current_value_mps = [cMps, cMps(end)];
        voyages(i) = v;
    end
end

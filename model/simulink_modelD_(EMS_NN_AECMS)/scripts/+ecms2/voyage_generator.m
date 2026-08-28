function voyages = voyage_generator(seed, N)
% ecms2.voyage_generator — 参数化随机航次生成器（T-201，design §2，FR-201）
%
% 航段积木（类型, v[kn], 时长[min], 水流[m/s]）——与 S20 工况类型一致：
%   离泊(3, 3~6)、港区限速(5, 5~12)、经济巡航(8, 10~25)、赶班巡航(11, 8~18)、
%   桥区限速(5, 6~12)、逆水(11 + 水流 -1 m/s, 6~12)、靠泊(3, 3~6)
% 组合规则：
%   - 离泊开头、靠泊结尾；中间随机 3~6 段（从 港区/经济/赶班/桥区/逆水 抽取，可重复）；
%   - 总时长 60~120 min（越界 -> 整航次拒绝重抽）；
%   - kn→m/s 换算与 S20 一致（1 kn = 0.5144 m/s）。
% 解析预检（design §2 / FR-201 饱和预检；v1.1 修订：送达功率口径）:
%   P_delivered = min(Hull_K_R·v_eff³/(0.97·0.95·0.60·0.97)/1000, Mot_P_nom) + 100，
%   v_eff = v（逆水段 = v + 1，|水流|=1 m/s）；任一段 >850 kW -> 整航次拒绝重抽。
%   口径依据（design Amendment Log v1.1）：速度饱和 ≠ 降额——速度 PI 在 Mot_P_nom
%   饱和后实际送达 P_demand ≤ Mot_P_nom + hotel（S20 段 9 已验证：11kn 逆水
%   800 kW 持续饱和、零降额、Vdc 健康）；预检目的排除“需求超装备能力致降额”航次。
% 附加约束：每个航次逆水段最多 1 段（避免多次饱和的极端航次污染分布）。
% 种子：rng(seed,'twister') 全程单随机序列 -> 同种子两次调用结果逐位一致
%   （50_voyage_generator.m 断言；design §2 种子复现验证）。
% 输出：voyages 1×N struct 数组，字段：
%   id/name/seed/n_seg/duration_s/duration_min/seg(cell of struct)/
%   v_time_s/v_value_mps/current_time_s/current_value_mps/
%   accel_ramp_mps2/decel_ramp_mps2/P_max_est_kW/P_limit_kW/draw_attempts
% 时间路点约定（与 +ecms2/voyage_s20.m 一致）：N 段边界 b0=0<b1<...<bN，
%   v_time_s = [b0 b1 ... bN]，v_value_mps = [v1 ... vN vN]（尾值保持 t>=bN）。
    if nargin < 1 || isempty(seed), seed = 20260825; end
    if nargin < 2 || isempty(N), N = 200; end

    % ---- 航段积木库（T-201 / design §2 固定；struct 构造：cell 值分发 -> 1×7 struct 数组）----
    BLK = struct( ...
        'type',    {'depart','harbor','eco','rush','bridge','upstream','berth'}, ...
        'label',   {'离泊机动','港区限速','经济巡航','赶班巡航','桥区限速','逆水段','靠泊机动'}, ...
        'v_kn',    {3, 5, 8, 11, 5, 11, 3}, ...
        'dur_min', {[3 6], [5 12], [10 25], [8 18], [6 12], [6 12], [3 6]}, ...
        'current_mps', {0, 0, 0, 0, 0, -1, 0});
    MID_TYPES = 2:6;                 % 中间段可抽取类型（港区/经济/赶班/桥区/逆水）

    kn2mps = 0.5144;
    eta_chain = 0.97 * 0.95 * 0.60 * 0.97;   % design §2 解析预检效率链
    P_limit_kW = 850;                        % 预检饱和上限（design §2 / FR-201）
    hotel_kW = 100;                          % 日用负荷（S20 hotel_kW）
    ACCEL = 0.15;                            % 快加速（S20 design 注：赶班/出桥段 0.15）
    DECEL = 0.05;                            % 标准减速率（RAMP_NORM）
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
        Mot_P_nom = 700;      % 速度 PI 输出饱和上限（送达功率口径 v1.1）
    end

    % ---- 单随机序列（种子固定 -> 可复现）----
    rng(seed, 'twister');

    voyages = struct('id', {}, 'name', {}, 'seed', {}, 'n_seg', {}, ...
        'duration_s', {}, 'duration_min', {}, 'seg', {}, ...
        'v_time_s', {}, 'v_value_mps', {}, 'current_time_s', {}, 'current_value_mps', {}, ...
        'accel_ramp_mps2', {}, 'decel_ramp_mps2', {}, ...
        'P_max_est_kW', {}, 'P_limit_kW', {}, 'draw_attempts', {});
    for i = 1:N
        attempts = 0;
        while true
            attempts = attempts + 1;
            nMid = randi([3 6]);
            midTypes = MID_TYPES(randi(numel(MID_TYPES), 1, nMid));
            if sum(midTypes == 6) > 1
                continue;    % 逆水段最多 1 段/航次（design Amendment Log v1.1）
            end
            types = [1, midTypes, 7];                        % 离泊 ... 靠泊
            dursMin = zeros(1, numel(types));
            for j = 1:numel(types)
                dm = BLK(types(j)).dur_min;
                dursMin(j) = dm(1) + rand() * (dm(2) - dm(1));   % 连续均匀 [min]
            end
            if sum(dursMin) < T_MIN_MIN || sum(dursMin) > T_MAX_MIN
                continue;    % 总时长越界 -> 整航次拒绝重抽
            end
            % 解析预检（送达功率口径 v1.1：速度饱和不判拒；逆水 v_eff = v + 1）
            v_eff = round([BLK(types).v_kn] * kn2mps * 1e4) / 1e4;   % 与 vMps 同精度
            upMask = types == 6;
            v_eff(upMask) = v_eff(upMask) + 1;
            P_prop = min(Hull_K_R * v_eff.^3 / eta_chain / 1000, Mot_P_nom);
            P_est = P_prop + hotel_kW;
            if any(P_est > P_limit_kW)
                continue;    % 需求超装备能力 -> 整航次拒绝重抽
            end
            break;
        end
        durS = round(dursMin * 60);                            % [s]，整数秒
        tt = cumsum([0, durS]);                                % N+1 个时间边界
        % 航速取 4 位小数（0.0001 m/s ≈ 0.0002 kn）：JSON %.10g 往返逐位一致
        % （design §2 航次定义入库可复现；与预检 v_eff 同精度）
        vMps = round([BLK(types).v_kn] * kn2mps * 1e4) / 1e4;
        cMps = [BLK(types).current_mps];
        seg = struct('type', {}, 'label', {}, 'v_kn', {}, 'v_mps', {}, ...
            'dur_s', {}, 'current_mps', {}, 'P_est_kW', {});
        for j = 1:numel(types)
            seg(j) = struct('type', BLK(types(j)).type, 'label', BLK(types(j)).label, ...
                'v_kn', BLK(types(j)).v_kn, 'v_mps', vMps(j), ...
                'dur_s', durS(j), 'current_mps', BLK(types(j)).current_mps, ...
                'P_est_kW', P_est(j));
        end
        v = struct('id', i, 'name', sprintf('voyage_%03d', i), ...
            'seed', seed, 'n_seg', numel(types), ...
            'duration_s', tt(end), 'duration_min', tt(end) / 60, ...
            'accel_ramp_mps2', ACCEL, 'decel_ramp_mps2', DECEL, ...
            'P_max_est_kW', max(P_est), 'P_limit_kW', P_limit_kW, ...
            'draw_attempts', attempts);
        v.seg = seg;                                          % 1×n_seg struct 数组
        v.v_time_s          = tt;                             % N+1 边界（S20 约定）
        v.v_value_mps       = [vMps, vMps(end)];              % N 段值 + 尾保持
        v.current_time_s    = tt;
        v.current_value_mps = [cMps, cMps(end)];
        voyages(i) = v;
    end
end

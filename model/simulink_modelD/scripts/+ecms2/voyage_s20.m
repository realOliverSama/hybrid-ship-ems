function s20 = voyage_s20()
% ecms2.voyage_s20 — S20 复合航次剖面定义（ECMS Phase 1 T-101/T-102，design §2.1 参数唯一源）
%
% 消费链：
%   config/Model_Scenarios.m      -> SCENARIOS(20)（本函数输出直接填充 SCN 字段）
%   +model/build_model_c.m        -> 01_Scenario_Manager 时间序列路径
%                                    （Constant 块编译期读 SCN.v_time_s 等）
%   scripts/40_build_composite_voyage.m -> 仿真/导出/DP（本函数用于报告与单一源一致性核对）
%
% 设计语义（design §2.1）：
%   - v_value_mps 为时间路点分段恒定目标航速；节点间过渡由 01_Scenario_Manager
%     的 Rate Limiter 按 accel/decel_ramp_mps2 平滑（"以限幅器为准"）；
%   - 水流仅段 9（4440–5040 s）为逆水 −1 m/s，其余 0；
%   - 航次以时间终止 t = stop_time_s（主），里程作完整性核对（voyage_distance_m
%     在 config 中放宽，避免 11_Voyage_Stop_Logic 提前停机）。
    s20 = struct();
    s20.id   = 20;
    s20.name = 'S20_composite_voyage';

    % ---- kn -> m/s（design §2.1：3/5/8/11 kn）----
    s20.kn2mps = [1.543, 2.572, 4.115, 5.658];

    % ---- 时间航速路点（分段恒定；过渡由 Rate Limiter 平滑）----
    v = s20.kn2mps;
    s20.v_time_s    = [0 300 780 900 2100 2220 3120 3720 4440 5040 5220 5520];
    s20.v_value_mps = [v(1) v(2) v(3) v(3) v(4) v(4) v(2) v(4) v(4) v(1) v(1) v(1)];

    % ---- 时间水流路点（段 9 逆水 −1 m/s，其余 0）----
    s20.current_time_s    = [0 4440 5040 5520];
    s20.current_value_mps = [0 -1 0 0];

    % ---- 终止与斜坡 ----
    s20.stop_time_s     = 5520;   % 时间终止（主条件）
    s20.accel_ramp_mps2 = 0.15;   % 赶班/出桥快加速（design 注：出桥/赶班段可用 0.15）
    s20.decel_ramp_mps2 = 0.05;   % 巡航段标准减速率（RAMP_NORM）

    % ---- 航段表（打印/报告用；t0 为段起点，段内 v 为目标航速）----
    s20.seg = struct( ...
        't0',    {0 300 780 900 2100 2220 3120 3720 4440 5040 5220}, ...
        't1',    {300 780 900 2100 2220 3120 3720 4440 5040 5220 5520}, ...
        'v',     {v(1) v(2) v(3) v(3) v(4) v(4) v(2) v(4) v(4) v(1) v(1)}, ...
        'label', {'离泊机动','港区限速','加速','经济巡航','加速赶班','服务巡航', ...
                  '桥区限速','出桥巡航','逆水段','减速进港','靠泊机动'}, ...
        'current', {0 0 0 0 0 0 0 0 -1 0 0});

    % ---- 里程预估（design §2.1 预计 ~21 km；实际以仿真 x_ship_m(end) 为准）----
    s20.expected_distance_m = 21000;

    s20.hotel_kW = 100;
end

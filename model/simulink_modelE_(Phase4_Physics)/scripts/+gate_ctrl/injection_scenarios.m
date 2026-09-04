function spec = injection_scenarios()
% gate_ctrl.injection_scenarios — Safety 注入场景定义（T-010，FR-448/453）
%   五场景各一（确定性 assignin 通道，禁 setVariable——A2 T-412 教训）；
%   每场景 prep_fn 先恢复全 nominal 再施加本场景注入（OAT 纪律：跨 case 不泄漏）；
%   expected = C6 断言通道（见 check_c6_safety_assertions）；
%   law/src/use_manual/manual_ts 为该场景臂配置（S5 需 EMS_AGGREGATE nominal 才构成事件）。
%
%   S1  DG fault（非预期丢失）：M3 剖面使 DG2 确定性并网（≈215 s），t=400 s 经
%       GEN_CMD_STOP_TS 注入非指令停机 → S1a 锁存 + chart FORCE_STOP 语义回 ONE（reason 3）；
%       另一机（DG1）在线 → 无 force_start 目标（flag-only 路径同证）。
%   S2  realized supply deficit：SOC_init=0.12 → SOP_dis 崩缩 → DG1 启动 30 s 内
%       实现缺口 > SOP 连续 2 s → force_start DG2（reason 4，design §6.2 同拍优先级）。
%   S3  min generation surplus：SOC_init=0.90 → 充电 SOP 收缩；MANUAL 低档（u=0.10）
%       使 N_dispatch·125 − P_load_gen > SOP_chg 且双健康机在线 → FORCE_STOP DG2（reason 6）。
%   S4  prospective firm-capacity deficit：SOC_init=0.15 → SOP_dis 中度收缩，使
%       D_firm（commanded 口径）先于 realized S2 触发 → force_start DG2（reason 5）。
%   S5  EMS health fallback：PMS_SOURCE_MODE=1（EMS_AGGREGATE）下 EMS_HEALTH_TS t=100 s
%       拉低 → fallback 锁存 + selector 切 BASELINE（reason 1）+ 无 commitment 迁移。

    spec = struct( ...
        'name', {}, 'prep_fn', {}, 'stop_time_s', {}, 'expected', {}, ...
        'law', {}, 'src', {}, 'use_manual', {}, 'manual_ts', {}, 'voyage_distance_m', {}, ...
        'soc_init', {}, 'force_mode', {}, 'hotel_kW', {});

    % ---- S1 ----
    s = blank();
    s.name = 'S1_dg_fault_unexpected_loss';
    s.prep_fn = @prep_s1;
    s.stop_time_s = 900;
    s.expected = {'fault_latch_dg2'};
    s.law = 0; s.src = 0;                       % P2 baseline（确定性需求源）
    s.soc_init = 0.60;
    s.use_manual = true;
    s.manual_ts = [0 0.30; 120 0.30; 120.01 0.70; 360 0.70; 360.01 0.30; 1e6 0.30];  % = MANUAL_CMD_M3_TS（design §6.5，内联自包含）
    s.voyage_distance_m = 1e6;
    spec(1) = s;

    % ---- S2 ----
    s = blank();
    s.name = 'S2_realized_supply_deficit';
    s.prep_fn = @prep_s2;
    s.stop_time_s = 90;
    s.expected = {'force_start_dg2'};
    s.law = 0; s.src = 0;
    s.use_manual = false; s.manual_ts = [];
    s.voyage_distance_m = 1e6;
    s.soc_init = 0.12;                          % SOP_dis 崩缩（realized 缺口路径）
    spec(2) = s;

    % ---- S3 ----
    s = blank();
    s.name = 'S3_min_gen_surplus';
    s.prep_fn = @prep_s3;
    s.stop_time_s = 820;   % 实测火点 ≈757 s（充电斜率受 η_coul/LUT 影响；裕量 +60 s）
    s.expected = {'force_stop_dg2'};
    s.law = 0; s.src = 0;
    s.use_manual = true;                        % MANUAL u=0 锁定低负荷（AUTO 速度 PI 会拉高负荷毁掉盈余）
    s.manual_ts = [0 0; 1e6 0];
    s.voyage_distance_m = 1e6;
    s.soc_init = 0.90;                          % 充电爬入死区：SOC 0.90→0.9389 → margin>SOP_chg
    s.force_mode = 1;                           % bench 直接指令双机 125 kW（DD-8/D-B10 通道）
    s.hotel_kW = 10;                            % margin = 250−13.4 ≈ 237 kW > SOP_chg（→0）
    spec(3) = s;

    % ---- S4 ----
    s = blank();
    s.name = 'S4_prospective_firm_deficit';
    s.prep_fn = @prep_s4;
    s.stop_time_s = 90;
    s.expected = {'force_start_dg2'};
    s.law = 0; s.src = 0;
    s.use_manual = false; s.manual_ts = [];
    s.voyage_distance_m = 1e6;
    s.soc_init = 0.15;                          % SOP_dis 中度收缩（commanded 先于 realized 触发）
    spec(4) = s;

    % ---- S5 ----
    s = blank();
    s.name = 'S5_ems_health_fallback';
    s.prep_fn = @prep_s5;
    s.stop_time_s = 300;
    s.expected = {'baseline_fallback'};
    s.law = 0; s.src = 1;                        % nominal = EMS_AGGREGATE（S5 才构成事件，D-B15）
    s.use_manual = false; s.manual_ts = [];
    s.voyage_distance_m = 1e6;
    s.soc_init = 0.60;
    spec(5) = s;
end

function s = blank()
    s = struct('name', '', 'prep_fn', [], 'stop_time_s', 0, 'expected', {{}}, ...
        'law', 0, 'src', 0, 'use_manual', false, 'manual_ts', [], 'voyage_distance_m', 1e6, ...
        'soc_init', 0.60, 'force_mode', 0, 'hotel_kW', 100);
end

function prep_nominal()
% 全 nominal 恢复（OAT 纪律：跨 case 泄漏防线）——必须在 base workspace 恢复
% （模型 Parameter/From Workspace 都在 base 解析；本函数为函数作用域 → evalin）
    E = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    evalin('base', sprintf("run('%s')", fullfile(E, 'config', 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(E, 'config', 'ModelE_Control_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(E, 'config', 'ModelE_TestScenarios.m')));
    assignin('base', 'ADAPTER_FORCE_MODE', 0);
    assignin('base', 'ADAPTER_FORCE_P1_KW', 0);
    assignin('base', 'ADAPTER_FORCE_P2_KW', 0);
end

function prep_s1()
    prep_nominal();
    % GEN_CMD_STOP_TS：t=400 s 注入 DG2 非指令停机 0.5 s（M3 剖面 DG2 已并网 ≈215 s，
    % chart 仍 cmd_onoff_2=1 且无本方 force_stop → S1a）
    assignin('base', 'GEN_CMD_STOP_TS', [0 0 0; 400 0 1; 400.5 0 0; 1e6 0 0]);
end

function prep_s2()
    prep_nominal();   % SOC_init=0.12 经 spec.soc_init → scn.initial_SOC（runOne 统一注入）
end

function prep_s3()
    prep_nominal();
    % bench force 通道：双机 125 kW（min）直接指令——16 号 Safety 层照常运行
    % （输出级被 force 旁路 → force_stop 请求为探测断言对象，非执动）；
    % 电池以 ~237 kW 充电 → SOC 0.90→0.95（~51 s）→ P_soc_chg=0 → S3 即时触发
    assignin('base', 'ADAPTER_FORCE_MODE', 1);
    assignin('base', 'ADAPTER_FORCE_P1_KW', 125);
    assignin('base', 'ADAPTER_FORCE_P2_KW', 125);
    % S3 专用 MANUAL 剖面已内联于 spec.manual_ts（低档 u=0.10 → P_load_gen≈175 kW；
    % 低档进入拍 N_dispatch=2 ∧ 250−175=75 kW 盈余 > SOP_chg → S3 即时 FORCE_STOP DG2）
end

function prep_s4()
    prep_nominal();   % SOC_init=0.15 经 spec.soc_init（D_firm 先于 realized S2 触发）
end

function prep_s5()
    prep_nominal();
    % EMS_HEALTH_TS：t=100 s 拉低（列 [t, health]；From Workspace + Interpolate off）
    assignin('base', 'EMS_HEALTH_TS', [0 1; 100 1; 100.01 0; 1e6 0]);
end
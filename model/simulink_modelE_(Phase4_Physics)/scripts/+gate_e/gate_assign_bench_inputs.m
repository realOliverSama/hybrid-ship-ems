function gate_assign_bench_inputs(force_mode, force_p1, force_p2, stop_ts, SCN)
% gate_e.assign_bench_inputs — Plant 测试台确定性指令注入（assignin 通道，FR-425）
%   与 run_scenario_sim 同机制（base workspace 注入）；设计 §5.2 确定性测试台。
%   注入项：EMS 变体选择（law=0 Baseline）、adapter force 组合、GEN_CMD_STOP_TS、
%   VOYAGE_*（编译期常量族）、SOC 初值。
    assignin('base', 'ADAPTER_FORCE_MODE', force_mode);
    assignin('base', 'ADAPTER_FORCE_P1_KW', force_p1);
    assignin('base', 'ADAPTER_FORCE_P2_KW', force_p2);
    assignin('base', 'GEN_CMD_STOP_TS', stop_ts);
    assignin('base', 'EMS_CTRL_LAW', 0);
    assignin('base', 'EMS_MODE', 0);
    assignin('base', 'SCN', SCN);
    isTime = strcmp(ecms2.scn_field(SCN, 'speed_profile_mode', 'distance'), 'time');
    assignin('base', 'VOYAGE_SPEED_MODE', double(isTime));
    assignin('base', 'VOYAGE_V_TIME_S', ecms2.scn_field(SCN, 'v_time_s', [0 1]));
    assignin('base', 'VOYAGE_V_VALUE_MPS', ecms2.scn_field(SCN, 'v_value_mps', [0 0]));
    assignin('base', 'VOYAGE_CURRENT_MODE', double(isTime));
    assignin('base', 'VOYAGE_CURRENT_TIME_S', ecms2.scn_field(SCN, 'current_time_s', [0 1]));
    assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(SCN, 'current_value_mps', [0 0]));
    assignin('base', 'VOYAGE_WATER_DEPTH_M', ecms2.scn_field(SCN, 'water_depth_m', 20.0));
    assignin('base', 'Bat_SOC_init', ecms2.scn_field(SCN, 'initial_SOC', 0.60));
end
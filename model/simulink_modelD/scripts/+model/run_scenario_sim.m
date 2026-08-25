function out = run_scenario_sim(mdl, SCN, mode, stopTimeLimit)
% model.run_scenario_sim — 按工况运行 Model 仿真
% mdl: 模型名；SCN: 工况结构体；mode: 0=Baseline 1=AI_Enhanced 2=ScheduledFF；stopTimeLimit: 安全上限 [s]
% 副作用: 向 base workspace 写入 SCN / EMS_CTRL_LAW / EMS_MODE(兼容) / Bat_SOC_init（运行参数）
% 返回: SimulationOutput；若仿真被 Voyage_Stop_Logic 提前终止则正常返回。
    assignin('base', 'SCN', SCN);
    assignin('base', 'EMS_CTRL_LAW', double(mode));
    assignin('base', 'EMS_MODE', double(mode > 0));   % 兼容列（日志 ems_mode）
    assignin('base', 'Bat_SOC_init', SCN.initial_SOC);
    % T-201 注入机制配套同步（主 Agent 修复 2026-08-25）：build_model_c 将 01_Scenario
    % 时间模式表改读 base 变量 VOYAGE_*（供 SimulationInput.setVariable 逐航次注入），
    % 因此每个 SCN 切换点必须同步 VOYAGE_*，否则距离模式场景会误用 Model_Scenarios.m
    % 中的 S20 默认时间剖面（行为污染）。51 批量路径不经本函数（自行 setVariable）。
    isTimeMode = strcmp(ecms2.scn_field(SCN, 'speed_profile_mode', 'distance'), 'time');
    assignin('base', 'VOYAGE_SPEED_MODE', double(isTimeMode));
    assignin('base', 'VOYAGE_V_TIME_S', ecms2.scn_field(SCN, 'v_time_s', [0 1]));
    assignin('base', 'VOYAGE_V_VALUE_MPS', ecms2.scn_field(SCN, 'v_value_mps', [0 0]));
    assignin('base', 'VOYAGE_CURRENT_MODE', double(isTimeMode));
    assignin('base', 'VOYAGE_CURRENT_TIME_S', ecms2.scn_field(SCN, 'current_time_s', [0 1]));
    assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(SCN, 'current_value_mps', [0 0]));
    % 固定步长 0.01 s：StopTime 需为步长整数倍
    stopTimeLimit = floor(stopTimeLimit * 100) / 100;
    set_param(mdl, 'StopTime', sprintf('%.2f', stopTimeLimit));
    % 按模式启用/停用 AI 内部信号记录（避免对非活动变体记录）
    model.set_ai_logging(mdl, mode);
    out = sim(mdl);
end

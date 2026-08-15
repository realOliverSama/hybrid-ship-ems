function out = run_scenario_sim(mdl, SCN, mode, stopTimeLimit)
% model.run_scenario_sim — 按工况运行 Model 仿真
% mdl: 模型名；SCN: 工况结构体；mode: 0=Baseline 1=AI；stopTimeLimit: 安全上限 [s]
% 副作用: 向 base workspace 写入 SCN / EMS_MODE / Bat_SOC_init（运行参数）
% 返回: SimulationOutput；若仿真被 Voyage_Stop_Logic 提前终止则正常返回。
    assignin('base', 'SCN', SCN);
    assignin('base', 'EMS_MODE', double(mode));
    assignin('base', 'Bat_SOC_init', SCN.initial_SOC);
    % 固定步长 0.01 s：StopTime 需为步长整数倍
    stopTimeLimit = floor(stopTimeLimit * 100) / 100;
    set_param(mdl, 'StopTime', sprintf('%.2f', stopTimeLimit));
    % 按模式启用/停用 AI 内部信号记录（避免对非活动变体记录）
    model.set_ai_logging(mdl, mode);
    out = sim(mdl);
end

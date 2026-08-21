function out = run_ms_sim(mdl, SCN, law, H, infoMode, oracle, stopTimeLimit, genEff)
% model.run_ms_sim — EMS_MinFF 场景仿真（T-004R 复现性修复）
% 与 run_scenario_sim 的区别：所有 per-run 变量一律经 Simulink.SimulationInput.setVariable
% 注入（EMS_CTRL_LAW / EMS_MODE / Bat_SOC_init / MSFF_Horizon_s / MSFF_InfoMode /
% MSFF_Oracle_Data / Gen_P_max / Gen_Ramp_Up / Gen_Ramp_Down / ASSUMED_Gen_TimeConstant），
% 每个 sim(in) 都是独立编译单元，不依赖 base workspace 变更检测（T-007 根因修复）。
% 输入:
%   mdl          模型名
%   SCN          工况结构体（含 initial_SOC）
%   law          EMS_CTRL_LAW：0=Baseline 3=EMS_MinFF
%   H            MSFF_Horizon_s [s]（law=0 时任意）
%   infoMode     MSFF_InfoMode（0=traj, 1=peak；law=0 时任意）
%   oracle       MSFF_Oracle_Data 矩阵（N×13；law=0 时传 [] 亦可）
%   stopTimeLimit 仿真停止时间上限 [s]
%   genEff       结构体（Gen_P_max/Gen_Ramp_Up/Gen_Ramp_Down/tau_gen）
% 返回: SimulationOutput
    in = Simulink.SimulationInput(mdl);
    in = in.setVariable('SCN', SCN);
    in = in.setVariable('EMS_CTRL_LAW', double(law));
    in = in.setVariable('EMS_MODE', double(law > 0));
    in = in.setVariable('Bat_SOC_init', SCN.initial_SOC);
    in = in.setVariable('MSFF_Horizon_s', H);
    in = in.setVariable('MSFF_InfoMode', infoMode);
    in = in.setVariable('MSFF_Oracle_Data', oracle);
    in = in.setVariable('Gen_P_max', genEff.Gen_P_max);
    in = in.setVariable('Gen_Ramp_Up', genEff.Gen_Ramp_Up);
    in = in.setVariable('Gen_Ramp_Down', genEff.Gen_Ramp_Down);
    in = in.setVariable('ASSUMED_Gen_TimeConstant', genEff.tau_gen);
    if law == 0
        % C0 显式关闭预测通道（与 21 旧运行器一致）
        in = in.setVariable('AI_Pred_Source', 0);
        in = in.setVariable('AI_TPeak_Source', 0);
        in = in.setVariable('ASSUMED_AI_DataValid', false);
    end
    stopTimeLimit = floor(stopTimeLimit * 100) / 100;
    in = in.setModelParameter('StopTime', sprintf('%.2f', stopTimeLimit));
    if getenv('DBG_RUNMS') == "1"
        fprintf('[run_ms_sim] law=%d H=%g info=%g SOC0=%g GP=%g Rup=%g Rdn=%g tau=%g oracle_rows=%d\n', ...
            law, H, infoMode, SCN.initial_SOC, genEff.Gen_P_max, genEff.Gen_Ramp_Up, ...
            genEff.Gen_Ramp_Down, genEff.tau_gen, size(oracle,1));
    end
    model.set_ai_logging(mdl, law);
    out = sim(in);
end

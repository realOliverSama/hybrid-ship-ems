function inject_scenario(MDL, SCN, mode, stopTimeLimit)
% t445.inject_scenario — run_scenario_sim 的注入段（不 sim），供 B1 ZOH SimulationInput
ROOT = t445.modele_root();
if ~evalin('base', "exist('PMS_LoadLPF_tau_s', 'var')")
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
end
assignin('base', 'SCN', SCN);
assignin('base', 'EMS_CTRL_LAW', double(mode));
assignin('base', 'EMS_MODE', double(mode > 0));
assignin('base', 'Bat_SOC_init', SCN.initial_SOC);
isTimeMode = strcmp(ecms2.scn_field(SCN, 'speed_profile_mode', 'distance'), 'time');
assignin('base', 'VOYAGE_SPEED_MODE', double(isTimeMode));
assignin('base', 'VOYAGE_V_TIME_S', ecms2.scn_field(SCN, 'v_time_s', [0 1]));
assignin('base', 'VOYAGE_V_VALUE_MPS', ecms2.scn_field(SCN, 'v_value_mps', [0 0]));
assignin('base', 'VOYAGE_CURRENT_MODE', double(isTimeMode));
assignin('base', 'VOYAGE_CURRENT_TIME_S', ecms2.scn_field(SCN, 'current_time_s', [0 1]));
assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(SCN, 'current_value_mps', [0 0]));
assignin('base', 'VOYAGE_WATER_DEPTH_M', ecms2.scn_field(SCN, 'water_depth_m', 20.0));
stopTimeLimit = floor(stopTimeLimit * 100) / 100;
set_param(MDL, 'StopTime', sprintf('%.2f', stopTimeLimit));
model.set_ai_logging(MDL, mode);
end

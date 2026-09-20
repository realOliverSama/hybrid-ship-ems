function inject_voyage(MDL, comp, scn, law, T_eval)
% ppe.inject_voyage — mission_v3 voyage injection for PPE arms (no sim).
%
% Mirrors model.run_scenario_sim's assignment half + member_exec's
% injection_list, then sets the PPE law and StopTime=T_eval (D4). Does not
% modify or save the .slx.
if ~evalin('base', "exist('PMS_LoadLPF_tau_s', 'var')")
    ROOT = ppe.ppe_root();
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
end
il = comp.injection_list;
for j = 1:numel(il)
    e = il(j);
    assignin('base', e.var_name, e.value);
end
assignin('base', 'SCN', scn);
assignin('base', 'EMS_CTRL_LAW', double(law));
assignin('base', 'EMS_MODE', double(law > 0));
assignin('base', 'Bat_SOC_init', scn.initial_SOC);
isTimeMode = strcmp(ecms2.scn_field(scn, 'speed_profile_mode', 'distance'), 'time');
assignin('base', 'VOYAGE_SPEED_MODE', double(isTimeMode));
assignin('base', 'VOYAGE_V_TIME_S', ecms2.scn_field(scn, 'v_time_s', [0 1]));
assignin('base', 'VOYAGE_V_VALUE_MPS', ecms2.scn_field(scn, 'v_value_mps', [0 0]));
assignin('base', 'VOYAGE_CURRENT_MODE', double(isTimeMode));
assignin('base', 'VOYAGE_CURRENT_TIME_S', ecms2.scn_field(scn, 'current_time_s', [0 1]));
assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(scn, 'current_value_mps', [0 0]));
assignin('base', 'VOYAGE_WATER_DEPTH_M', ecms2.scn_field(scn, 'water_depth_m', 20.0));
set_param(MDL, 'StopTime', sprintf('%.2f', floor(T_eval * 100) / 100));
model.set_ai_logging(MDL, 0);
end

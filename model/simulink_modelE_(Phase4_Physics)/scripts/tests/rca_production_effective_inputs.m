cd('/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)');
addpath('scripts'); addpath('config');
run('config/ModelE_Physics_Parameters.m'); run('config/ModelE_Control_Parameters.m'); run('config/ModelE_TestScenarios.m');
MDL='HybridShip_ModelE'; load_system(fullfile('model',[MDL '.slx']));
T=300;
readback = @() local_rb();
% ---- C：生产入口 smoke 路径的注入（仅注入，不仿真）----
scnC = evalin('base','SCENARIOS(1)'); scnC.hotel_kW=700;
scnC.speed_profile_mode='time'; scnC.v_time_s=[0 T]; scnC.v_value_mps=[7.0 7.0];
scnC.current_time_s=[0 1]; scnC.current_value_mps=[0 0]; scnC.initial_SOC=0.60; scnC.stop_time_s=T;
names={'VOYAGE_SPEED_MODE','VOYAGE_V_TIME_S','VOYAGE_V_VALUE_MPS','VOYAGE_CURRENT_MODE','VOYAGE_CURRENT_TIME_S','VOYAGE_CURRENT_VALUE_MPS','VOYAGE_WATER_DEPTH_M','PMS_SOURCE_MODE','EMS_CTRL_LAW'};
vals={1.0, scnC.v_time_s(:), scnC.v_value_mps(:), 0.0, [0 1], [0 0], scnC.water_depth_m, 0.0, 0.0};
comp=struct('injection_list',struct('var_name',names,'value',vals));
ppe.inject_voyage(MDL,comp,scnC,6,T);
ppe.arm_runtime_bind('C0');
rbC = readback();
% ---- A/B：RCA/EP3-P1 直接注入 ----
SCN=evalin('base','SCENARIOS(1)'); SCN.hotel_kW=700;
assignin('base','SCN',SCN); assignin('base','Bat_SOC_init',0.60);
assignin('base','VOYAGE_SPEED_MODE',1); assignin('base','VOYAGE_V_TIME_S',[0 T]);
assignin('base','VOYAGE_V_VALUE_MPS',[7 7]);
assignin('base','VOYAGE_CURRENT_MODE',0); assignin('base','VOYAGE_CURRENT_TIME_S',[0 1]);
assignin('base','VOYAGE_CURRENT_VALUE_MPS',[0 0]); assignin('base','VOYAGE_WATER_DEPTH_M',SCN.water_depth_m);
rbA = readback();
f=fieldnames(rbA);
for i=1:numel(f)
  a=rbA.(f{i}); c=rbC.(f{i});
  same = isequal(a,c);
  if ~same, fprintf('DIFF %-24s A=%s  C=%s\n', f{i}, local_s(a), local_s(c)); end
end
fprintf('EFFIN_DONE (差异见上；无 DIFF 行 = 有效输入一致)\n');
function rb=local_rb()
v={'VOYAGE_SPEED_MODE','VOYAGE_V_TIME_S','VOYAGE_V_VALUE_MPS','VOYAGE_CURRENT_MODE','VOYAGE_CURRENT_TIME_S','VOYAGE_CURRENT_VALUE_MPS','VOYAGE_WATER_DEPTH_M','PMS_SOURCE_MODE','EMS_CTRL_LAW','EMS_MODE','EMS_WORK_MODE','AI_Pred_Source'};
rb=struct();
for i=1:numel(v), rb.(v{i})=evalin('base',v{i}); end
s=evalin('base','SCN'); rb.SCN_hotel=s.hotel_kW; rb.SCN_speed_mode=s.speed_profile_mode;
rb.SCN_v_time=s.v_time_s(:)'; rb.SCN_v_val=s.v_value_mps(:)';
rb.SCN_v_layout_missing=[];
end
function s=local_s(x)
if ischar(x), s=x; elseif isnumeric(x) && numel(x)<8, s=mat2str(x); else, s=sprintf('<%s %s>', class(x), mat2str(size(x))); end
end

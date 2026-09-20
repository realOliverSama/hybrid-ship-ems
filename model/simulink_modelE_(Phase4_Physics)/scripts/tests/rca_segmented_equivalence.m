cd('/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)');
addpath('scripts'); addpath('config');
run('config/ModelE_Physics_Parameters.m'); run('config/ModelE_Control_Parameters.m'); run('config/ModelE_TestScenarios.m');
MDL='HybridShip_ModelE'; load_system(fullfile('model',[MDL '.slx']));
model.set_ai_logging(MDL,0); lg=model.set_source_selection_logging(MDL,true);
T=300; DLAM=0.00;
SCN=evalin('base','SCENARIOS(1)'); SCN.hotel_kW=700;
assignin('base','SCN',SCN); assignin('base','Bat_SOC_init',0.60);
assignin('base','VOYAGE_SPEED_MODE',1); assignin('base','VOYAGE_V_TIME_S',[0 T]);
assignin('base','VOYAGE_V_VALUE_MPS',[7 7]);
assignin('base','VOYAGE_CURRENT_MODE',0); assignin('base','VOYAGE_CURRENT_TIME_S',[0 1]);
assignin('base','VOYAGE_CURRENT_VALUE_MPS',[0 0]); assignin('base','VOYAGE_WATER_DEPTH_M',SCN.water_depth_m);
assignin('base','ECMS_EHat_Data',[0 0 DLAM]);
assignin('base','PMS_SOURCE_MODE',1); assignin('base','EMS_CTRL_LAW',6); assignin('base','EMS_MODE',1);
cc={'PMS_SOURCE_MODE','EMS_CTRL_LAW','EMS_MODE','EMS_WORK_MODE','AI_Pred_Source','ADAPTER_FORCE_MODE','ADAPTER_FORCE_P1_KW','ADAPTER_FORCE_P2_KW','EMS_HEALTH_TS'};
mk=@(mdl) local_in(mdl,cc);
% A: continuous
set_param(MDL,'StopTime',sprintf('%.2f',T)); outA=sim(mk(MDL)); dA=gate_ctrl.get_ds(outA);
% B: segmented production path (run_cstar, SkipInject since injected above)
SCNw=evalin('base','SCN');
ctxid=struct('study_config_sha256','RCA','code_commit_sha','RCA','hf_config_sha256','RCA', ...
 'slx_sha256',model.sha256_file(fullfile('model',[MDL '.slx'])),'cstar_impl_sha256','RCA', ...
 'corpus_identity_sha256','RCA','teval_identity_sha256','RCA','voyage_id','RCA_SYNTH','predictor_identity','N/A');
bind=ppe.arm_runtime_bind('C0');
winfun=@(t0,ds,p0) local_hold(t0,ds,p0);
[dB,~]=ppe.run_cstar(SCNw,winfun,'C0',T,ctxid,'SkipInject',true,'BindingGuard',bind,'InitialDLam',DLAM);
% first divergence
ch={'P_load_total_kW','N_dispatch','state_DG1','state_DG2','P_gen_actual_kW','SOC'};
ta=dA.t(:); tb=dB.t(:); n=min(numel(ta),numel(tb)); first=[];
for c=1:numel(ch)
  a=local_col(dA.(ch{c}),numel(ta)); b=local_col(dB.(ch{c}),numel(tb));
  d=abs(a(1:n)-b(1:n)); i=find(d>1e-9,1,'first');
  fprintf('CH %-18s maxdiff=%.4g  first_diff_t=%s\n', ch{c}, max(d), mat2str(ta(min(i,n))));
  if ~isempty(i) && (isempty(first) || ta(i)<first(1)), first=[ta(i),c]; end
end
if ~isempty(first), fprintf('FIRST_DIVERGENCE t=%.2f signal=%s\n', first(1), ch{first(2)}); else, fprintf('FIRST_DIVERGENCE none (identical)\n'); end
fprintf('A: N max=%g  load_end=%.1f  gen_end=%.1f | B: N max=%g  load_end=%.1f  gen_end=%.1f\n', ...
  max(dA.N_dispatch), dA.P_load_total_kW(end), dA.P_gen_actual_kW(end), max(dB.N_dispatch), dB.P_load_total_kW(end), dB.P_gen_actual_kW(end));
fprintf('A n=%d B n=%d | A t_end=%.2f B t_end=%.2f\n', numel(ta), numel(tb), ta(end), tb(end));
disp('RCA_OK');
function in=local_in(mdl,cc)
in=Simulink.SimulationInput(mdl);
for i=1:numel(cc), if evalin('base',sprintf('exist(''%s'',''var'')',cc{i})), in=in.setVariable(cc{i},evalin('base',cc{i})); end, end
end
function v=local_col(x,n)
x=double(x(:)); if numel(x)==1, v=repmat(x,n,1); elseif numel(x)>=n, v=x(1:n); else, v=[x;repmat(x(end),n-numel(x),1)]; end
end
function [Pbar,prov,use_prev]=local_hold(t0,ds,p0)
Pbar=600*ones(12,1); prov=struct('source_id','rca_hold','t0',t0); use_prev=true;
end

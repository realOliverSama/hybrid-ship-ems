% H2_INTERFACE_PROPAGATION_QUALIFICATION（非正式；生产入口；接口钩子）
cd(fileparts(fileparts(fileparts(mfilename('fullpath')))));
addpath('scripts'); addpath('config');
ROOT=ppe.ppe_root(); OUTDIR=fullfile(ROOT,'results','ppe_practical','execution_path_recovery','h2_interface');
n0=local_count(fullfile(ROOT,'results','ppe_practical','formal_epoch3'));
hA=ppe.run_epoch3_voyage('H2_SYNTH','C0','Mode','smoke','T',300,'QHookManualRef',700,'OutDir',fullfile(OUTDIR,'outA'));
hB=ppe.run_epoch3_voyage('H2_SYNTH','C0','Mode','smoke','T',300,'QHookManualRef',900,'OutDir',fullfile(OUTDIR,'outB'));
n1=local_count(fullfile(ROOT,'results','ppe_practical','formal_epoch3'));
% formal 模式拒绝钩子（fail-closed 测试）
refused=false;
try, ppe.run_epoch3_voyage('H2_SYNTH','C0','Mode','formal','T',300,'QHookManualRef',700); catch ME, refused=strcmp(ME.identifier,'PPE:EP3:QHOOK'); end
[A,At]=local_tel(hA); [B,Bt]=local_tel(hB);
t0=max([max(local_on(A),local_on(B))+20,150]);
m = A.t>=t0 & B.t>=t0 & A.s1==3 & A.s2==3 & B.s1==3 & B.s2==3 & A.n==2 & B.n==2 ...
  & A.br<0.5 & B.br<0.5 & A.fb<0.5 & B.fb<0.5 & A.sfb<0.5 & B.sfb<0.5 ...
  & A.sel>251 & A.sel<999 & B.sel>251 & B.sel<999 & A.mc<0.5 & B.mc<0.5;
dsel=max([abs(A.sel(m)-B.sel(m));0]); dtar=max([abs((A.g1(m)+A.g2(m))-(B.g1(m)+B.g2(m)));0]);
dgen=max([abs(A.gen(m)-B.gen(m));0]); dgenmed=median([abs(A.gen(m)-B.gen(m));0]);
r=struct('schema','ppe_h2_interface_result_v1','utc',char(datetime('now','TimeZone','UTC','Format','yyyy-MM-dd''T''HH:mm:ss''Z')), ...
 'NONFORMAL',true,'FORMAL_EPOCH3_RESULT',false,'mask_n',sum(m), ...
 'prescribed_ref_diff_kW',abs(700-900),'selected_ref_diff_kW',dsel,'allocator_target_diff_kW',dtar, ...
 'gen_diff_max_kW',dgen,'gen_diff_median_kW',dgenmed, ...
 'ems_ref_A_mean_kW',mean(A.ems(m)),'ems_ref_B_mean_kW',mean(B.ems(m)), ...
 'sel_A_mean_kW',mean(A.sel(m)),'sel_B_mean_kW',mean(B.sel(m)), ...
 'branch_max_A',max(A.br(m)),'branch_max_B',max(B.br(m)),'fallback_max',max([A.fb(m);B.fb(m)]), ...
 'minclamp_max',max([A.mc(m);B.mc(m)]),'N_dispatch_min',min([A.n(m);B.n(m)]), ...
 'formal_ns_before',n0,'formal_ns_after',n1,'hook_formal_refused',refused);
r.pass = sum(m)>50 && dsel>1 && dtar>1 && dgen>1 && max(A.br(m))<0.5 && max(B.br(m))<0.5 ...
  && n0==0 && n1==0 && refused;
if ~exist(OUTDIR,'dir'), mkdir(OUTDIR); end
fid=fopen(fullfile(OUTDIR,'h2_interface_result.json'),'w'); fwrite(fid,jsonencode(r,'PrettyPrint',true)); fclose(fid);
fprintf('[H2] pass=%d mask=%d dsel=%.1f dtar=%.1f dgen=%.1f/med %.1f formal=%d/%d hook_refused=%d\n', ...
  r.pass,r.mask_n,dsel,dtar,dgen,dgenmed,n0,n1,refused);
function n=local_count(d)
n=0; if ~exist(d,'dir'), return; end
f=dir(fullfile(d,'*','*.json'));
for i=1:numel(f), try, j=jsondecode(fileread(fullfile(f(i).folder,f(i).name))); if isfield(j,'is_formal_result') && j.is_formal_result, n=n+1; end, catch, end, end
end
function v=local_on(S)
i=find(S.s1==3 & S.s2==3,1,'first'); if isempty(i), v=S.t(end); else, v=S.t(i); end
end
function [T,Tt]=local_tel(rec)
S=load(rec.trajectory.path,'traj'); tr=S.traj; Q=load(rec.source_selection_telemetry.path,'tel'); tel=Q.tel;
T=struct(); T.t=tr.time(:); T.gen=tr.P_gen_actual_kW(:); T.n=tr.N_dispatch(:); T.s1=tr.state_DG1(:); T.s2=tr.state_DG2(:);
T.sfb=tr.safety_baseline_fallback_active(:);
T.sel=interp1(tel.time_s(:),tel.P_gen_req_eff_kW_end(:),T.t,'previous','extrap');
T.ems=interp1(tel.time_s(:),tel.P_gen_ref_kW_end(:),T.t,'previous','extrap');
T.br =interp1(tel.time_s(:),tel.selector_baseline_active_end(:),T.t,'previous','extrap');
T.fb =interp1(tel.time_s(:),tel.baseline_fallback_active_end(:),T.t,'previous','extrap');
T.mc =interp1(tel.time_s(:),tel.dispatch_min_clamp_active_end(:),T.t,'previous','extrap');
T.g1 =interp1(tel.time_s(:),tel.P_DG1_ref_kW_end(:),T.t,'previous','extrap');
T.g2 =interp1(tel.time_s(:),tel.P_DG2_ref_kW_end(:),T.t,'previous','extrap');
Tt=tel.time_s(:);
end

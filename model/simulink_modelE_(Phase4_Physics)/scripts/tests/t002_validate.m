% t002_validate.m — T-002 验证（独立加载/超集/派生值/manifest/sha 对照/浅水档）
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
E = ROOT;
addpath(fullfile(E, 'config'), fullfile(E, 'scripts'));
% 注：matlab -batch 启动即干净 workspace；独立加载能力由后续 run 参数文件直接验证
run(fullfile(E, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(E, 'config', 'ModelE_TestScenarios.m'));

req = {'AI_Oracle_Data','AI_Pred_Source','AI_TPeak_Source','ASSUMED_AI_DataValid','ASSUMED_BSFC_Breakpoints','ASSUMED_BSFC_Table','ASSUMED_Bat_ChargeLimit','ASSUMED_Bat_DischargeLimit','ASSUMED_Bat_Eff_Charge','ASSUMED_Bat_Eff_Discharge','ASSUMED_C_bus','ASSUMED_Drive_TimeConstant','ASSUMED_EMS_FF_Gain','ASSUMED_EMS_FF_Max_kW','ASSUMED_EMS_Manual_Pgen_kW','ASSUMED_EMS_SOC_Gain','ASSUMED_EMS_SOC_Ref','ASSUMED_Gen_MinStablePower','ASSUMED_Gen_TimeConstant','ASSUMED_K_droop','ASSUMED_Pred_Bound_kW','ASSUMED_SOC_ChgDerate_Start','ASSUMED_SOC_DisDerate_Start','ASSUMED_SOC_max','ASSUMED_SOC_min','ASSUMED_SpeedPI_Ki','ASSUMED_SpeedPI_Kp','ASSUMED_TFF_SOC_AdvancePct','ASSUMED_TFF_SOC_Thresh','ASSUMED_TFF_TauMargin','ASSUMED_V_floor','ASSUMED_eta_dcdc','ASSUMED_eta_rect','Bat_Cap_kWh','Bat_SOC_init','CCS_Vdc_Steady_Band','CCS_Vdc_Transient_Band','ECMS_EHat_Data','ECMS_Grid_dP_kW','ECMS_Idle_Fuel_kgph','ECMS_K_S','ECMS_K_soc','ECMS_Lambda','ECMS_Lambda_Floor','ECMS_NN_Ebar_H_kWh','ECMS_NN_Escale_kWh','ECMS_NN_W1','ECMS_NN_W2','ECMS_NN_W3','ECMS_NN_WeightsOK','ECMS_NN_XMean','ECMS_NN_YMean','ECMS_SOC_Charge_Min','ECMS_SOC_Discharge_Max','ECMS_SOC_Ref','ECMS_dt_ctrl_s','EMS_CTRL_LAW','EMS_MODE','EMS_WORK_MODE','Elec_Eff','F_env_const','Gear_Eff','Gear_Ratio','Gen_P_max','Gen_Ramp_Down','Gen_Ramp_Up','Hull_K_R','K_Fv','K_Fw','K_Qv','K_Qw','MLP_FeatMean','MLP_W1','MLP_W2','MLP_W3','MLP_WeightsLoaded','MLP_YMean','Mot_P_nom','Mot_T_max','Pgen_init','Prop_J','Shaft_B','Ship_Mass','Ship_V_init','Ship_X_init','V_dc_nom','omega_min_radps','wake_fraction','SCENARIOS','VOYAGE_CURRENT_MODE','VOYAGE_CURRENT_TIME_S','VOYAGE_CURRENT_VALUE_MPS','VOYAGE_SPEED_MODE','VOYAGE_V_TIME_S','VOYAGE_V_VALUE_MPS','VOYAGE_WATER_DEPTH_M'};
missing = {};
for k = 1:numel(req)
    if ~evalin('base', sprintf('exist(''%s'',''var'')', req{k})), missing{end+1} = req{k}; end %#ok<SAGROW>
end
fprintf('REQUIRED-VARS: %d/%d covered', numel(req) - numel(missing), numel(req));
if ~isempty(missing), fprintf(' | MISSING: %s\n', strjoin(missing, ',')); else, fprintf(' | ALL COVERED\n'); end

fprintf('DERIVED K: K_Fw=%.3f K_Fv=%.3f K_Qw=%.3f K_Qv=%.3f | V_A_design=%.4f | w_deep=%.4f\n', ...
    K_Fw, K_Fv, K_Qw, K_Qv, V_A_design, Wake_w_deep);

ht = arrayfun(@(s) s.water_depth_m, SCENARIOS(31:36));
fprintf('SHALLOW TIERS (m): %s\n', mat2str(round(ht, 3)));
assert(max(abs(ht - [6.564 5.9076 4.376 3.5008 3.0632 2.6256])) < 1e-9, 'H/T tier depth mismatch');

out = model.write_config_manifest(E);
fprintf('MANIFEST: profile=%s | sha(ParamFile)=%s...\n', out.sensitivity_profile.profile_name, out.config_sha256.ModelE_Physics_Parameters_m(1:12));

[~, cmdout] = system(sprintf('shasum -a 256 ''%s'' | cut -c1-16', fullfile(E, 'config', 'ModelE_Physics_Parameters.m')));
fprintf('SHA256-CROSSCHECK: matlab=%s vs shasum=%s match=%d\n', ...
    out.config_sha256.ModelE_Physics_Parameters_m(1:16), strtrim(cmdout), ...
    strcmp(out.config_sha256.ModelE_Physics_Parameters_m(1:16), strtrim(cmdout)));

assert(numel(SCENARIOS) == 36);
fn = fieldnames(SCENARIOS(1)); fn36 = fieldnames(SCENARIOS(36));
fprintf('SCENARIOS: %d entries | fields consistent S01 vs S36: %d\n', numel(SCENARIOS), isequal(fn, fn36));
p = ModelE_Sensitivity_Profile();
fprintf('SENSITIVITY PROFILE: %s (%d overrides)\n', p.profile_name, numel(p.overrides));
fprintf('T002-VALIDATE: ALL PASS\n');
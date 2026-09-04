function out = write_config_manifest(ROOT, profile)
% model.write_config_manifest — Model E config manifest（design §2：有效配置可追溯）
%
%  每次 build（10_build_modelE）与 Gate（40_run_modelE_gate）调用：
%  写 results/config_manifest.json，含：
%    - Model E 三个 config 文件 sha256（本文件为唯一事实源证据）；
%    - 关键有效值快照（物理参数 + 参数来源分类快照）；
%    - historical_reference_hashes：Model D 四关键文件 sha256（NFR-407 零改动证据）；
%    - sensitivity profile 记录（nominal 档 = 零 override）。
%
%  输入：ROOT 工程根（缺省自动探测）；profile 敏感性档位（缺省 nominal）。
%  输出：manifest struct（并落盘 results/config_manifest.json）。

if nargin < 1 || isempty(ROOT)
    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
end
if nargin < 2 || isempty(profile)
    profile = ModelE_Sensitivity_Profile();  % nominal
end

cfgDir = fullfile(ROOT, 'config');
files = {'ModelE_Physics_Parameters.m', 'ModelE_Control_Parameters.m', 'ModelE_TestScenarios.m', 'ModelE_Sensitivity_Profile.m'};
sha = struct();
for k = 1:numel(files)
    p = fullfile(cfgDir, files{k});
    sha.(matlab.lang.makeValidName(files{k}, 'ReplacementStyle', 'underscore')) = model.sha256_file(p);
end

% ---- 关键有效值快照（base workspace 读取；调用前须已 run 参数文件）----
snap_names = {'Ship_Mass','Hull_K_R','Hull_Lpp_m','Hull_B_m','Hull_T_m','Hull_Sw_m2','Hull_CB', ...
    'Wake_w_deep','Hull_k_form','Shallow_c1','Shallow_c2','Shallow_c3','Shallow_HT_deep_limit', ...
    'Shallow_HT_min_valid','WaterDepth_default_m','Elec_Eff','Gear_Eff','K_Fw','K_Fv','K_Qw','K_Qv', ...
    'DG_P_rated_kW','DG_P_min_kW','DG_P_opt_kW','DG_ramp_kW_per_s','DG_tau_s', ...
    'DG_t_starting_s','DG_t_sync_s','DG_t_cooldown_s','DG_C_start_kg', ...
    'DG_BSFC_P_kW','DG_BSFC_gkWh','Bat_Q_nom_Ah','Bat_eta_coul','Bat_V_min_V','Bat_V_max_V', ...
    'Bat_OCV_LUT_V','Bat_R0_LUT_ohm','Bat_R1_LUT_ohm','Bat_C1_LUT_F', ...
    'Bat_I_limit_surrogate_A','Bat_P_conv_bus_max_kW','Bat_R0_chg_mult', ...
    'ASSUMED_Gen_MinStablePower','ASSUMED_eta_dcdc','WaterDepth_default_m', ...
    'PMS_LoadLPF_tau_s','PMS_SOC_lo','PMS_SOC_recovered','PMS_P_chg_recovery_kW', ...
    'PMS_alpha_start','PMS_alpha_stop','T_start_hold_s','T_stop_hold_s', ...
    'PMS_min_up_s','PMS_min_down_s','PMS_firm_margin_kW','PMS_S2_hold_s','PMS_S4_hold_s', ...
    'PMS_start_fail_window_s','Manual_Cmd_RateLimit_pct_s','PMS_SOURCE_MODE','VOYAGE_CTRL_MODE', ...
    'PMS_start_threshold_kW','PMS_stop_threshold_kW'};
snap = struct();
for k = 1:numel(snap_names)
    nm = snap_names{k};
    if evalin('base', sprintf('exist(''%s'',''var'')', nm))
        snap.(nm) = evalin('base', nm);
    else
        snap.(nm) = '<undefined>';
    end
end

% ---- Model D 历史资产零改动参照（NFR-407 / FR-411 证据链）----
legacyDir = fullfile(ROOT, '..', 'simulink_modelD_(EMS_NN_AECMS)');
legacy_files = {'config/Model_Parameters.m', 'config/Model_Scenarios.m', ...
    'scripts/+model/build_model_c.m', 'model/HybridShip_ModelD_DCBus.slx'};
hist = struct();
for k = 1:numel(legacy_files)
    p = fullfile(legacyDir, legacy_files{k});
    fn = matlab.lang.makeValidName(legacy_files{k}, 'ReplacementStyle', 'underscore');
    if exist(p, 'file')
        hist.(fn) = model.sha256_file(p);
    else
        hist.(fn) = '<missing>';
    end
end

out = struct();
out.schema = 'modelE_config_manifest_v1';
out.generated_at = char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ'));
out.config_sha256 = sha;
out.effective_values = snap;
out.sensitivity_profile = struct( ...
    'profile_name', profile.profile_name, ...
    'description', profile.description, ...
    'n_overrides', numel(profile.overrides), ...
    'overrides', profile.overrides);
out.historical_reference_hashes = hist;

resDir = fullfile(ROOT, 'results');
if ~exist(resDir, 'dir'), mkdir(resDir); end
outPath = fullfile(resDir, 'config_manifest.json');
fid = fopen(outPath, 'w');
fwrite(fid, jsonencode(out, 'PrettyPrint', true));
fclose(fid);
fprintf('[config_manifest] 已写入 %s（profile=%s, overrides=%d）\n', ...
    outPath, profile.profile_name, numel(profile.overrides));
end
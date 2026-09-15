function cfg = phase4f_t445_config()
% phase4f_t445_config — T-445 冻结参数 MATLAB 入口（DGN-F-01~F-15 🔒）
%
% 机器可读完整表 = 同目录 phase4f_t445_config.json（T-001 导出）。
% 本函数 jsondecode 该表，并对 design 锁定标量做 fail-closed 断言。
% k_eq^E 在 T-007 identity PASS 前为
% PROVISIONAL_CANDIDATE_PENDING_T007_REQUALIFICATION（Amendment C）。
%
% Frozen scalars (design.md; must match JSON):
%   k_eq_E = 0.5457 kg/kWh (nominal-SOC FEC; not OCV chemical kWh)
%   E_ref = 27.781415 kWh ; E_scale = 13.671476 kWh
%   provenance_sha256 = b9dcf268f533a9a93c73bbaeeed3a6dca6b15227cfd868fb459c46fcc81cf045
%   Delta-lambda in [-0.10, +0.10] step 0.005 (41 levels); no rate limit
%   lambda_eff = max(lambda_raw, 0.43); no artificial upper cap (Amendment a')
%   lambda_table = 0.43:0.005:15.525 (n=3020; max derived from primitives)
%   V_term: delta_term=0.01, w_term=1.17e5 kg/SOC^2, SOC_target=SOC0_voyage
%   TERMINAL_SOC_ADMISSIBILITY |dSOC_end|<=0.005
%   X=2.0%% / Y=0%%
%   seeds: phase4f-t445-corpus-v1 / phase4f-t445-bootstrap-v1
%   bootstrap B=10000 ; eps_num = 0.005 * J_FEC,B2
%   Bat_Cap = 1071.36 kWh
%
% Path rule: mfilename('fullpath') + pwd fallback; no absolute literals.

HERE = fileparts(mfilename('fullpath'));
if isempty(HERE)
    cand = fullfile(pwd, 'config');
    if exist(fullfile(cand, 'phase4f_t445_config.json'), 'file')
        HERE = cand;
    else
        HERE = pwd;
    end
end
jsonPath = fullfile(HERE, 'phase4f_t445_config.json');
if ~exist(jsonPath, 'file')
    error('T445:ConfigMissing', ...
        'phase4f_t445_config.json not found next to phase4f_t445_config.m');
end
raw = fileread(jsonPath);
cfg = jsondecode(raw);

assert(strcmp(cfg.schema, 'phase4f_t445_config_v1'), 'T445:Schema');
assert(abs(cfg.k_eq.value_kg_per_kWh - 0.5457) < 1e-12, 'T445:k_eq');
assert(strcmp(cfg.k_eq.status, 'PROVISIONAL_CANDIDATE_PENDING_T007_REQUALIFICATION') || ...
       strcmp(cfg.k_eq.status, 'CANONICAL'), 'T445:k_eq_status');
assert(abs(cfg.k_eq.eta_chg_FEC_ref - 0.9403) < 1e-12, 'T445:eta_fec');
assert(abs(cfg.k_eq.eta_chg_ocv_diagnostic - 0.9687) < 1e-12, 'T445:eta_ocv');
assert(cfg.k_eq.do_not_use_eta_chg_measured_0p7201 == true, 'T445:no072');
assert(abs(cfg.k_eq.bat_cap_kWh - 1071.36) < 1e-12, 'T445:BatCap');
assert(abs(cfg.b1_norm.E_ref_kWh - 27.781415) < 1e-12, 'T445:E_ref');
assert(abs(cfg.b1_norm.E_scale_kWh - 13.671476) < 1e-12, 'T445:E_scale');
assert(strcmp(cfg.b1_norm.provenance_sha256, ...
    'b9dcf268f533a9a93c73bbaeeed3a6dca6b15227cfd868fb459c46fcc81cf045'), ...
    'T445:provenance');
assert(abs(cfg.delta_lambda.min_kg_per_kWh + 0.10) < 1e-12, 'T445:dlam_min');
assert(abs(cfg.delta_lambda.max_kg_per_kWh - 0.10) < 1e-12, 'T445:dlam_max');
assert(abs(cfg.delta_lambda.step - 0.005) < 1e-12, 'T445:dlam_step');
assert(cfg.delta_lambda.n_levels == 41, 'T445:dlam_n');
assert(cfg.delta_lambda.rate_limit == false, 'T445:dlam_rate');
assert(abs(cfg.lambda_law.floor - 0.43) < 1e-12, 'T445:floor');
assert(isempty(cfg.lambda_law.upper_cap), 'T445:no_cap');
assert(contains(string(cfg.lambda_law.envelope), "max(lambda_raw, 0.43)"), 'T445:env');
remax = cfg.lambda_law.lambda0 + cfg.lambda_law.K_soc * ...
    (cfg.lambda_law.SOC_ref - cfg.b2_policy_table.SOC_DP_min) + ...
    cfg.delta_lambda.max_kg_per_kWh;
assert(abs(remax - cfg.b2_policy_table.lambda_table_max) < 1e-12, 'T445:lamax');
assert(abs(cfg.b2_policy_table.lambda_table_max - 15.525) < 1e-12, 'T445:15p525');
assert(cfg.b2_policy_table.n_lambda == 3020, 'T445:nlam');
assert(cfg.b2_policy_table.oob_query_count_required == 0, 'T445:oob');
assert(abs(cfg.v_term.delta_term - 0.01) < 1e-12, 'T445:dterm');
assert(abs(cfg.v_term.w_term - 1.17e5) < 1e-6, 'T445:wterm');
assert(abs(cfg.terminal_soc_admissibility.abs_dsoc_max - 0.005) < 1e-12, 'T445:termSOC');
assert(abs(cfg.gate_thresholds.X_pct - 2.0) < 1e-12, 'T445:X');
assert(abs(cfg.gate_thresholds.Y_pct - 0.0) < 1e-12, 'T445:Y');
assert(strcmp(cfg.seeds.corpus, 'phase4f-t445-corpus-v1'), 'T445:seed_corpus');
assert(strcmp(cfg.seeds.bootstrap, 'phase4f-t445-bootstrap-v1'), 'T445:seed_boot');
assert(cfg.bootstrap.B == 10000, 'T445:B');
assert(abs(cfg.epsilon_num.rel_of_j_fec_b2 - 0.005) < 1e-12, 'T445:eps');
assert(cfg.oracle_window.refresh_s == 10 && cfg.oracle_window.K == 30, 'T445:window');
end

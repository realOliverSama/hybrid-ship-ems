function out = ppe_config()
% ppe.ppe_config — load PPE study config + frozen T-445 config, assert
% frozen C* numerics (FR-811 / DGN-PPE-03). Fail-closed on any drift.
ROOT = ppe.ppe_root();
sc = jsondecode(fileread(fullfile(ROOT, 'config', 'phase4f_ppe_study_config.json')));
cs = sc.cstar;
assert(cs.law == 6, 'PPE:CFG:LAW');
assert(abs(cs.delta_lambda_min + 0.10) < 1e-12 && ...
       abs(cs.delta_lambda_max - 0.10) < 1e-12 && ...
       abs(cs.delta_lambda_step - 0.005) < 1e-12 && cs.n_delta_lambda == 41, ...
       'PPE:CFG:DLAM');
assert(abs(cs.soc_grid_step - 0.005) < 1e-15 && cs.n_soc == 171, 'PPE:CFG:SOC');
sg = cs.soc_grid_min:cs.soc_grid_step:cs.soc_grid_max;
assert(numel(sg) == 171, 'PPE:CFG:NSOC171');
assert(abs(cs.k_eq_E_kg_per_kwh - 0.5457) < 1e-12, 'PPE:CFG:KEQ');
assert(abs(cs.w_term_kg - 1.17e5) < 1e-3, 'PPE:CFG:WTERM');
assert(abs(cs.lambda_eff_floor - 0.43) < 1e-12 && isempty(cs.lambda_eff_cap), ...
    'PPE:CFG:FLOOR');
assert(abs(cs.K_S - 1/30) < 1e-12, 'PPE:CFG:KS');
assert(abs(cs.realize_bin_dt_internal_s - 1.0) < 1e-12, 'PPE:CFG:DTINT');
assert(abs(cs.bat_cap_kwh - 1071.36) < 1e-9, 'PPE:CFG:CAP');
assert(abs(cs.dg_c_start_kg - 0.6) < 1e-12, 'PPE:CFG:START');
assert(abs(cs.terminal_soc_abs_dsoc_max - 0.005) < 1e-15, 'PPE:CFG:TSOC');

addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
cfg = phase4f_t445_config();
assert(strcmp(cfg.k_eq.status, 'CANONICAL'), 'PPE:CFG:KEQSTATUS');
assert(abs(cfg.k_eq.value_kg_per_kWh - 0.5457) < 1e-12, 'PPE:CFG:KEQ2');
assert(abs(cfg.lambda_law.K_S_B2_num / cfg.lambda_law.K_S_B2_den - 1/30) < 1e-12, ...
    'PPE:CFG:KS2');
assert(abs(cfg.v_term.w_term - 1.17e5) < 1e-3, 'PPE:CFG:WTERM2');
assert(abs(cfg.lambda_law.floor - 0.43) < 1e-12 && ...
       isempty(cfg.lambda_law.upper_cap), 'PPE:CFG:FLOOR2');

out = struct('study', sc, 't445', cfg, 'study_config_sha256', ...
    sc.config_sha256);
end

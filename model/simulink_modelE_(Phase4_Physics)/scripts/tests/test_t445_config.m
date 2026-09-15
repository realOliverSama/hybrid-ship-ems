function test_t445_config()
% test_t445_config — T-001 MATLAB 侧冻结 config 断言（AC 字段存在且=design）
HERE = fileparts(mfilename('fullpath'));
ROOT = fileparts(fileparts(HERE));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'));
cfg = phase4f_t445_config();
assert(strcmp(cfg.k_eq.status, 'PROVISIONAL_PENDING_QUALIFICATION'));
assert(abs(cfg.k_eq.value_kg_per_kWh - 0.5296) < 1e-12);
fprintf('[T-001] phase4f_t445_config MATLAB load PASS, sha=%s\n', cfg.config_sha256);
end

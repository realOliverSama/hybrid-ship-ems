function r = check_g1_structure(ROOT, sims)
% gate_e.check_g1_structure — G1 结构判据（design §5.3，physics_invariant）
%   fixed-step ode3 0.01 s；零代数环（update 已在 builder AlgebraicLoopMsg=error 下通过）；
%   零 NaN/Inf（全部测试台仿真）；Model D 目录零改动（sha256 对照 manifest）。
r.name = 'G1_structure';
r.gate_class = 'physics_invariant';
r.checks = {};

% 1) solver 配置
load_system(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));
st = get_param('HybridShip_ModelE', 'SolverType');
sv = get_param('HybridShip_ModelE', 'Solver');
fs = get_param('HybridShip_ModelE', 'FixedStep');
ok_solver = strcmp(st, 'Fixed-step') && strcmp(sv, 'ode3') && abs(str2double(fs) - 0.01) < 1e-12;
r.checks{end+1} = struct('item', 'solver_fixed_step_ode3_0.01s', 'pass', ok_solver); %#ok<SAGROW>

% 2) 零代数环（构建期 AlgebraicLoopMsg=error 已强制；此处复核模型设置）
al = get_param('HybridShip_ModelE', 'AlgebraicLoopMsg');
r.checks{end+1} = struct('item', 'algebraic_loop_error_mode', 'pass', strcmp(al, 'error')); %#ok<SAGROW>
close_system('HybridShip_ModelE', 0);

% 3) 零 NaN/Inf（全部测试台仿真关键信号）
nan_ok = true; nan_details = {};
for k = 1:numel(sims)
    s = sims{k};
    fields = fieldnames(s.ds);
    for f = 1:numel(fields)
        v = s.ds.(fields{f});
        if isnumeric(v) && ~all(isfinite(v(:)))
            nan_ok = false;
            nan_details{end+1} = sprintf('%s.%s', s.name, fields{f}); %#ok<SAGROW>
        end
    end
end
r.checks{end+1} = struct('item', 'zero_NaN_Inf_all_sims', 'pass', nan_ok, 'offenders', {nan_details}); %#ok<SAGROW>

% 4) Model D 零改动（历史资产 sha256 与 2026-09-04 冻结值对照）
expected = { ...
    'config/Model_Parameters.m', '830eec47'; ...
    'config/Model_Scenarios.m', 'cafd670f'; ...
    'scripts/+model/build_model_c.m', '90b7c9c9'; ...
    'model/HybridShip_ModelD_DCBus.slx', '50359e65'};
legacyDir = fullfile(ROOT, '..', 'simulink_modelD_(EMS_NN_AECMS)');
zc_ok = true; zc_detail = {};
for k = 1:size(expected, 1)
    p = fullfile(legacyDir, expected{k, 1});
    if exist(p, 'file')
        h = model.sha256_file(p);
        okc = strncmp(h, expected{k, 2}, 8);
    else
        okc = false;
    end
    zc_ok = zc_ok && okc;
    zc_detail{end+1} = sprintf('%s=%s', expected{k, 1}, string(okc)); %#ok<SAGROW>
end
r.checks{end+1} = struct('item', 'modelD_zero_change_sha256', 'pass', zc_ok, 'detail', {zc_detail}); %#ok<SAGROW>

r.pass = all(cellfun(@(c) c.pass, r.checks));
end
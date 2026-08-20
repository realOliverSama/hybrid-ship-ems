%% ========================================================================
%  08c_droop_sweep.m — K_droop 灵敏度扫描（T-008，固化下垂系数依据）
%  对 S04（阻力渐增）分别取 K_droop = 5/10/20 kW/V，比较稳态偏差与跌落深度
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

MDL = 'HybridShip_ModelC_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
assignin('base', 'EMS_WORK_MODE', 2);

SCN = SCENARIOS(4);  % S04 阻力渐增（负荷扰动代表工况）
fprintf('[08c] K_droop 灵敏度扫描（S04, %d m）\n', SCN.voyage_distance_m);
fprintf('%-10s %-12s %-12s %-12s %-12s %-12s\n', 'K_droop', 'V_min[V]', 'V_ss_end[V]', 'dU_max[V]', 't_rec[s]', '稳态越限[s]');
sweepRes = {};
for Kd = [5, 10, 20]
    assignin('base', 'ASSUMED_K_droop', Kd);
    out = model.run_scenario_sim(MDL, SCN, 0, 4000);
    ds = model.extract_logsout(out);
    mm = model.dcbus_metrics(ds);
    vss = mean(ds.V_dc_V(end-100:end));
    fprintf('%-10.0f %-12.1f %-12.1f %-12.1f %-12.2f %-12.1f\n', ...
        Kd, mm.V_min, vss, mm.dU_max, mm.t_recovery_s, mm.t_steady_viol_s);
    sweepRes{end+1} = struct('Kd', Kd, 'V_min', mm.V_min, 'V_ss_end', vss, ...
        'dU_max', mm.dU_max, 't_rec', mm.t_recovery_s, 'viol', mm.t_steady_viol_s); %#ok<AGROW>
end
assignin('base', 'ASSUMED_K_droop', 10);  % 恢复默认
save(fullfile(ROOT, 'results', 'droop_sweep.mat'), 'sweepRes');
fprintf('[08c] 扫描结果已存 results/droop_sweep.mat\n');

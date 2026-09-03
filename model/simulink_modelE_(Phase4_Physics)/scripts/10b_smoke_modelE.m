%% ========================================================================
%  10b_smoke_modelD.m — Model D 烟雾测试（T01C/T04C/T10C）
%  判据：CCS 2026 稳态带（675~795 V）、恢复时间 ≤1.5 s（参照）、无 NaN/Inf、
%        方向性（逆水段电压低于顺水段）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

MDL = 'HybridShip_ModelE';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
assignin('base', 'EMS_WORK_MODE', 2);

TESTS = struct('name', {'T01C_S01_still', 'T04C_S04_res_inc', 'T10C_S10_current_reversal'}, ...
    'sid', {1, 4, 10}, 'dist', {1500, 2500, 5000});
allPass = true;
summaryLines = {};
for k = 1:numel(TESTS)
    T = TESTS(k);
    SCN = SCENARIOS(T.sid);
    SCN.voyage_distance_m = T.dist;
    out = model.run_scenario_sim(MDL, SCN, 0, 4000);
    ds = model.extract_logsout(out);
    mm = model.dcbus_metrics(ds);
    % 检查项
    finite_ok = isfinite(min(ds.V_dc_V)) && isfinite(max(ds.P_bat_actual_kW));
    steady_ok = mm.t_steady_viol_s == 0 || T.sid ~= 1;   % 仅 S01 要求全程在稳态带
    CCS_T = evalin('base','CCS_Vdc_Transient_Band');
    band_ok = min(ds.V_dc_V) >= CCS_T(1);   % 不破瞬态下限
    rec_ok = mm.t_recovery_s <= evalin('base','CCS_t_recovery_max') || mm.t_recovery_s == 0;
    pass = finite_ok && band_ok && rec_ok && (T.sid ~= 1 || mm.t_steady_viol_s == 0);
    if T.sid == 10
        % 方向性：逆水段平均电压 < 顺水段平均电压（逆水负荷大、下垂深）
        iUp = ds.x_ship_m < 2400; iDn = ds.x_ship_m > 2700;
        dir_ok = mean(ds.V_dc_V(iUp)) < mean(ds.V_dc_V(iDn));
        pass = pass && dir_ok;
        fprintf('[10b] %s: 逆水段 V=%.1f < 顺水段 V=%.1f (dir_ok=%d)\n', ...
            T.name, mean(ds.V_dc_V(iUp)), mean(ds.V_dc_V(iDn)), dir_ok);
    end
    if ~pass, allPass = false; end
    fprintf('[10b] %s: V_dc=[%.1f, %.1f] V, dU_max=%.1f V, t_rec=%.2f s, 稳态越限=%.1f s, 瞬态越限=%.1f s -> %s\n', ...
        T.name, mm.V_min, mm.V_max, mm.dU_max, mm.t_recovery_s, ...
        mm.t_steady_viol_s, mm.t_transient_viol_s, string(pass));
    summaryLines{end+1} = sprintf('| %s | %.1f | %.1f | %.2f | %.1f | %.1f | %s |', ...
        T.name, mm.V_min, mm.dU_max, mm.t_recovery_s, mm.t_steady_viol_s, ...
        mm.t_transient_viol_s, string(pass)); %#ok<AGROW>
end

fid = fopen(fullfile(ROOT, 'reports', 'modelD_smoke_summary.md'), 'w', 'n', 'UTF-8');
fprintf(fid, '# Model D 烟雾测试汇总\n\n| 测试 | V_min [V] | ΔU_max [V] | t_rec [s] | 稳态越限 [s] | 瞬态越限 [s] | 结论 |\n');
fprintf(fid, '|---|---|---|---|---|---|---|\n');
for i = 1:numel(summaryLines), fprintf(fid, '%s\n', summaryLines{i}); end
fprintf(fid, '\nCCS 2026 判据：稳态带 [675, 795] V，瞬态带 [600, 802.5] V，t_recovery ≤ 1.5 s。\n');
fprintf(fid, '总体: %s\n', string(allPass));
fclose(fid);
fprintf('[10b] 报告: %s\n', fullfile(ROOT, 'reports', 'modelD_smoke_summary.md'));
if ~allPass
    error('GATE:ModelD_Smoke 存在失败项');
end
fprintf('[10b] Model D 烟雾测试全部通过\n');

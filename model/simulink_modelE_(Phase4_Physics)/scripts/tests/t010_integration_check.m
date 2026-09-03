% t010_integration_check.m — T-010 集成验证：结构指纹导出 + 幂等连建 + law=0/law=5 兼容 smoke
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));
MDL = 'HybridShip_ModelE';
mdlPath = fullfile(ROOT, 'model', [MDL '.slx']);

%% 1) 结构指纹导出（find_system → JSON，NFR-405 幂等证据基础）
model.export_structure_fingerprint(mdlPath, fullfile(ROOT, 'results', 'structure_fingerprint_build1.json'));
f1 = readstruct(fullfile(ROOT, 'results', 'structure_fingerprint_build1.json'));

%% 2) 幂等：连建第二次，块/线集合逐位一致（排除 generated_at 时间戳）
model.build_model_e(ROOT);
model.export_structure_fingerprint(mdlPath, fullfile(ROOT, 'results', 'structure_fingerprint_build2.json'));
f2 = readstruct(fullfile(ROOT, 'results', 'structure_fingerprint_build2.json'));
assert(f1.n_blocks == f2.n_blocks && f1.n_lines == f2.n_lines, '块/线数不一致');
p1 = string({f1.blocks.path}); p2 = string({f2.blocks.path});
same = isequal(sort(p1), sort(p2));
fprintf('IDEMPOTENT (blocks=%d lines=%d): %s\n', f1.n_blocks, f1.n_lines, string(same));
assert(same, '结构指纹不一致——builder 非幂等');

%% 3) law=0/law=5 兼容 smoke（E10 口径：无错误即可，不作物理判据，D-9）
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
load_system(mdlPath);
SCN = SCENARIOS(1); SCN.voyage_distance_m = 800;
for law = [0 5]
    out = model.run_scenario_sim(MDL, SCN, law, 1200);
    ds = model.extract_logsout(out);
    assert(all(isfinite(ds.V_dc_V)), 'law=%d V_dc 出现 NaN', law);
    assert(all(isfinite(ds.P_gen_actual_kW)), 'law=%d P_gen 出现 NaN', law);
    fprintf('COMPAT-SMOKE law=%d: OK (V_dc=[%.1f,%.1f], P_gen_max=%.1f kW, SOC_end=%.4f)\n', ...
        law, min(ds.V_dc_V), max(ds.V_dc_V), max(ds.P_gen_actual_kW), ds.SOC(end));
end
close_system(MDL, 0);
fprintf('T010-INTEGRATION-CHECK: ALL PASS\n');
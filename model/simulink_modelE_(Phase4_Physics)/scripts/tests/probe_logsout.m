% probe_logsout.m — 列出过渡态模型 logsout 字段名（临时探针）
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
load_system(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));
out = model.run_scenario_sim('HybridShip_ModelE', SCENARIOS(1), 0, 600);
ds = model.extract_logsout(out);
disp(fieldnames(ds));
close_system('HybridShip_ModelE', 0);
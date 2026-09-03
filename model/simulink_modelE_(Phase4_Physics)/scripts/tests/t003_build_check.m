% t003_build_check.m — T-003 验证：全量构建 Model E（新参数+旧物理过渡态）
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));
model.build_model_e(ROOT);
% 验证 water_depth_m 输出端口存在且接线
load_system(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));
sys = 'HybridShip_ModelE/01_Scenario_Manager';
assert(~isempty(find_system(sys, 'SearchDepth', 1, 'Name', 'water_depth_m')), 'water_depth_m 输出端口缺失');
assert(~isempty(find_system('HybridShip_ModelE/03_Propulsion_Plant', 'SearchDepth', 1, 'Name', 'water_depth_m')), '03_Plant water_depth_m 输入端口缺失');
ph = get_param('HybridShip_ModelE/01_Scenario_Manager', 'PortHandles');
assert(numel(ph.Outport) >= 6, '01_Scenario_Manager 输出端口不足 6');
lh = get_param(ph.Outport(6), 'Line');
assert(lh ~= -1, 'water_depth_m（Out6）顶层接线缺失');
dph = get_param(lh, 'DstPortHandle');
assert(numel(dph) == 1 && dph ~= -1, 'water_depth_m 顶层线无有效目标');
db = get_param(dph, 'Parent');
assert(strcmp(db, 'HybridShip_ModelE/03_Propulsion_Plant'), 'water_depth_m 未接入 03_Propulsion_Plant（目标=%s）', db);
pph = get_param('HybridShip_ModelE/03_Propulsion_Plant', 'PortHandles');
assert(isequal(dph, pph.Inport(5)), 'water_depth_m 未接至 03_Plant In5');
fprintf('T003-BUILD-CHECK: water_depth_m 端口与顶层接线就绪\n');
close_system('HybridShip_ModelE', 0);
fprintf('T003-BUILD-CHECK: ALL PASS\n');
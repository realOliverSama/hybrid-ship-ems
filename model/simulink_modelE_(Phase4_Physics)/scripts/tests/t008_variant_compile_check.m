% t008_variant_compile_check.m — T-008：EMS 七变体编译/构建矩阵 + law=0/5 兼容 smoke
%   验证（FR-450 / A2 FR-426 契约延续）：全部七变体（law=0/1/2/4/5/6/7）update 编译成功；
%   law=0/law=5 经 EMS_AGGREGATE 通道 + PMS v2 共享 Commitment/Dispatch/Safety 短程运行无错误
%   （不作物理判据，A2 既定口径；PMS_SOURCE_MODE=1=EMS_AGGREGATE）。
%   运行（Model E 工作区根）：
%   /Applications/MATLAB_R2026a.app/bin/matlab -batch "run('scripts/tests/t008_variant_compile_check.m')"
RT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(RT) || ~exist(fullfile(RT, 'config'), 'dir'), RT = pwd; end
addpath(fullfile(RT, 'config'), fullfile(RT, 'scripts'));
run(fullfile(RT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(RT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(RT, 'config', 'ModelE_TestScenarios.m'));
MDL = 'HybridShip_ModelE';
load_system(fullfile(RT, 'model', [MDL '.slx']));

%% Phase 1：七变体编译矩阵（update = 编译级验证）
laws = [0 1 2 4 5 6 7];
for k = 1:numel(laws)
    assignin('base', 'EMS_CTRL_LAW', laws(k));
    assignin('base', 'EMS_MODE', double(laws(k) > 0));
    set_param(MDL, 'SimulationCommand', 'update');
    fprintf('T008 COMPILE PASS: law=%d update 成功\n', laws(k));
end

%% Phase 2：law=0/5 短程 smoke（EMS_AGGREGATE + PMS v2 共享链）
smokeLaws = [0 5];
for k = 1:numel(smokeLaws)
    assignin('base', 'EMS_CTRL_LAW', smokeLaws(k));
    assignin('base', 'EMS_MODE', double(smokeLaws(k) > 0));
    assignin('base', 'PMS_SOURCE_MODE', 1);            % EMS_AGGREGATE（共享链，FR-450）
    SCN = SCENARIOS(1);                                 % S01 静水，短程
    assignin('base', 'SCN', SCN);
    assignin('base', 'Bat_SOC_init', SCN.initial_SOC);
    assignin('base', 'VOYAGE_SPEED_MODE', 0);
    assignin('base', 'VOYAGE_V_TIME_S', [0 1]);
    assignin('base', 'VOYAGE_V_VALUE_MPS', [0 0]);
    assignin('base', 'VOYAGE_CURRENT_MODE', 0);
    assignin('base', 'VOYAGE_CURRENT_TIME_S', [0 1]);
    assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', [0 0]);
    assignin('base', 'VOYAGE_WATER_DEPTH_M', 20);
    set_param(MDL, 'StopTime', '600');
    out = sim(MDL);
    ls = out.logsout;
    soc = ls.get('SOC').Values.Data;
    assert(all(isfinite(soc)), 'T008: law=%d 短程出现非有限 SOC', smokeLaws(k));
    v = ls.get('V_dc_V').Values.Data;
    assert(all(isfinite(v)), 'T008: law=%d 短程出现非有限 Vdc', smokeLaws(k));
    st2 = ls.get('state_DG2').Values;
    st2d = st2.Data; if size(st2.Data,1) == numel(st2.Time), st2v = st2.Data; else, st2v = squeeze(st2.Data); end
    fprintf('T008 SMOKE PASS: law=%d 600 s 无错误（SOC end=%.3f, Vdc in [%.1f, %.1f], state_DG2 max=%g）\n', ...
        smokeLaws(k), soc(end), min(v), max(v), max(st2v));
end
fprintf('T008 ALL PASS: 七变体编译矩阵 + law=0/5 兼容 smoke\n');
close_system(MDL, 0);
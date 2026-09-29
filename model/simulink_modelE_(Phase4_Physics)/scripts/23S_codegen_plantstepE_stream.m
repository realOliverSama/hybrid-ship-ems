%% ========================================================================
%  23S_codegen_plantstepE_stream.m — S4-P3 B2 步骤③（S4_P3_STREAM_PLANT_SPEC.md §2）
%  用途: Simulink Coder（GRT，同 23E 口径）将 HybridShip_ModelE_PlantStepE_Stream.slx
%        （20S 产物，stream_build_audit.json 前置）生成 C 代码到
%        EMS04_CHIL_FULLCTRL/Host/plantstep_e_stream/codegen/，并写 build_manifest.json。
%  单次构建（航次无关）: 流回放数据在运行期由 plantstep_e_stream_shim.c 从
%        <vid>/*.stream.f64.bin 读入（dladdr 定位同目录），不参与 codegen；
%        装饰分支（01/02/03/08）沿用 SCN(S20) 内联常量 → 4 航次共用一份生成码。
%  注入（base，编译期解析; 同 23E 口径）: SCN / EMS_CTRL_LAW=6 / EMS_MODE=1 /
%    PMS_SOURCE_MODE=1 / Bat_SOC_init / VOYAGE_* / VOYAGE_MANUAL_CMD_TS。
%  codegen 配置: 'SystemTargetFile','grt.tlc'（ERT 拒连续状态，23E 定案）/
%    'GenCodeOnly','on'（编译由 build_plantstep_e_stream.sh 承担）/
%    'DefaultParameterBehavior','Inlined' / 'SupportNonFinite','on' /
%    'MatFileLogging','off' / 'GenerateReport','off' / 'ExternalMode','off'。
%  红线: Stream 模型 / PlantStepE / 主模型三者内容哈希 slbuild 前后复核
%        （不等则 GATE:P3CODEGEN:*）；不改既有编号脚本。
%  失败语义: 失败即 error('GATE:P3CODEGEN:FAILED')（spec §6：禁硬绕）。
%  运行: cd <模型工作区根>; eval(fileread('scripts/23S_codegen_plantstepE_stream.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
REPO = fileparts(fileparts(ROOT));
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
if ~exist(fullfile(REPO, 'AGENTS.md'), 'file')
    error('GATE:P3CODEGEN:REPO — 仓库根定位失败: %s', REPO);
end
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

MDL_ST  = 'HybridShip_ModelE_PlantStepE_Stream';
MDL_PS  = 'HybridShip_ModelE_PlantStepE';
stPath   = fullfile(ROOT, 'model', [MDL_ST '.slx']);
psPath   = fullfile(ROOT, 'model', [MDL_PS '.slx']);
mainPath = fullfile(ROOT, 'model', 'HybridShip_ModelE.slx');
hostRoot = fullfile(REPO, 'firmware', 'stm32n6', '04_SourceCode', ...
    'SoftwarePackage', 'EMS_Projects', 'EMS04_CHIL_FULLCTRL', 'Host');
outDir = fullfile(hostRoot, 'plantstep_e_stream', 'codegen');
if ~exist(outDir, 'dir'), mkdir(outDir); end

if ~exist(stPath, 'file')
    error('GATE:P3CODEGEN:NO_ST — 先运行 20S_build_plantstepE_stream.m');
end

% ---- 哈希基线（三模型） ----
paths = {stPath, psPath, mainPath};
names = {'stream', 'plantstepe', 'main'};
sha0 = cell(1, 3);
for i = 1:3
    [~, o] = system(['shasum -a 256 ''' paths{i} '''']);
    tk = regexp(o, '([0-9a-f]{64})', 'tokens', 'once');
    sha0{i} = tk{1};
end

% ---- base 注入（同 23E 口径；SCN 取 S20 复合航次） ----
SC = evalin('base', 'SCENARIOS');
sid = 0;
for k = 1:numel(SC)
    if strcmpi(SC(k).name, 'S20_composite_voyage'), sid = k; break; end
end
if sid == 0, error('GATE:P3CODEGEN:SCN — S20_composite_voyage 未找到'); end
SCN = SC(sid);
assignin('base', 'SCN', SCN);
assignin('base', 'EMS_CTRL_LAW', 6);
assignin('base', 'EMS_MODE', 1);
assignin('base', 'PMS_SOURCE_MODE', 1);
assignin('base', 'Bat_SOC_init', SCN.initial_SOC);
isTimeMode = strcmp(ecms2.scn_field(SCN, 'speed_profile_mode', 'distance'), 'time');
assignin('base', 'VOYAGE_SPEED_MODE', double(isTimeMode));
assignin('base', 'VOYAGE_V_TIME_S', ecms2.scn_field(SCN, 'v_time_s', [0 1]));
assignin('base', 'VOYAGE_V_VALUE_MPS', ecms2.scn_field(SCN, 'v_value_mps', [0 0]));
assignin('base', 'VOYAGE_CURRENT_MODE', double(isTimeMode));
assignin('base', 'VOYAGE_CURRENT_TIME_S', ecms2.scn_field(SCN, 'current_time_s', [0 1]));
assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(SCN, 'current_value_mps', [0 0]));
assignin('base', 'VOYAGE_WATER_DEPTH_M', ecms2.scn_field(SCN, 'water_depth_m', 20.0));
if ~evalin('base', 'exist(''VOYAGE_MANUAL_CMD_TS'',''var'')')
    assignin('base', 'VOYAGE_MANUAL_CMD_TS', [0 0; 1e6 0]);
end

if ~bdIsLoaded(MDL_ST)
    load_system(stPath);
end

% ---- codegen 配置（运行期 set_param，不 save_system） ----
cfgApplied = {}; cfgFailed = {};
cfgs = {'GenCodeOnly', 'on'; 'DefaultParameterBehavior', 'Inlined'; ...
        'SupportNonFinite', 'on'; 'MatFileLogging', 'off'; ...
        'GenerateReport', 'off'; 'ExternalMode', 'off'; ...
        'SolverMode', 'SingleTasking'};
for c = 1:size(cfgs, 1)
    try
        set_param(MDL_ST, cfgs{c, 1}, cfgs{c, 2});
        cfgApplied{end+1} = cfgs{c, 1}; %#ok<AGROW>
    catch
        cfgFailed{end+1} = cfgs{c, 1}; %#ok<AGROW>
    end
end
tlc = 'grt.tlc'; usedGRT = true;   % 23E 定案（连续状态模型 ERT 拒绝）
set_param(MDL_ST, 'SystemTargetFile', tlc);

% ---- 幂等：删除旧 rtw 目录 ----
old = dir(fullfile(outDir, [MDL_ST '_*']));
priorRtw = ~isempty(old);
for k = 1:numel(old)
    rmdir(fullfile(old(k).folder, old(k).name), 's');
end

% ---- slbuild ----
prevDir = pwd;
cd(outDir);
genErr = '';
try
    slbuild(MDL_ST);
    genOK = ~isempty(dir(fullfile(outDir, [MDL_ST '_*'])));
catch ge
    genOK = false;
    genErr = ge.message;
end
cd(prevDir);
if exist(fullfile(ROOT, 'slprj'), 'dir'), rmdir(fullfile(ROOT, 'slprj'), 's'); end
if exist(fullfile(REPO, 'slprj'), 'dir'), rmdir(fullfile(REPO, 'slprj'), 's'); end
if ~genOK
    fprintf('[23S] codegen FAILED: %s\n', string(genErr));
    error('GATE:P3CODEGEN:FAILED — %s', genErr);
end

% ---- 产物逐文件 sha256 ----
rtwDir = fullfile(outDir, [MDL_ST '_grt_rtw']);
if ~exist(rtwDir, 'dir')
    d = dir(fullfile(outDir, [MDL_ST '_*_rtw']));
    rtwDir = fullfile(d(1).folder, d(1).name);
end
files = [dir(fullfile(rtwDir, '*.c')); dir(fullfile(rtwDir, '*.h')); ...
         dir(fullfile(rtwDir, '*.cnf')); dir(fullfile(rtwDir, '*.xml'))];
fileNames = cell(numel(files), 1);
fileSha = cell(numel(files), 1);
for k = 1:numel(files)
    [~, ho] = system(['shasum -a 256 ''' fullfile(rtwDir, files(k).name) '''']);
    tk = regexp(ho, '([0-9a-f]{64})', 'tokens', 'once');
    fileNames{k} = files(k).name;
    fileSha{k} = tk{1};
end

% ---- manifest ----
man = struct();
man.schema = 's4_p3_stream_codegen_manifest_v1';
man.created_utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'''));
man.matlab = version;
man.model = MDL_ST;
man.model_sha256 = sha0{1};
man.plantstepe_sha256 = sha0{2};
man.main_model_sha256 = sha0{3};
man.tlc = tlc;
man.used_grt_fallback = usedGRT;
man.cfg_applied = {cfgApplied};
man.cfg_failed = {cfgFailed};
man.prior_rtw_existed = priorRtw;
man.rtw_dir = strrep(rtwDir, [hostRoot '/'], 'Host/');
man.n_files = numel(files);
man.note = ['dylib 由 Host/tools/build_plantstep_e_stream.sh 编译 ' ...
    '(plantstep_e_stream_shim.c + 生成码)；单份生成码 4 航次共用 ' ...
    '（stream bin 运行期由 shim 经 dladdr 同目录读取）。'];
manPath = fullfile(outDir, 'build_manifest.json');
fid = fopen(manPath, 'w');
fwrite(fid, jsonencode(man, 'PrettyPrint', true));
fclose(fid);
fprintf('[23S] %d files -> %s\n', numel(files), rtwDir);

% ---- 红线复核：三模型零改动 ----
for i = 1:3
    [~, o] = system(['shasum -a 256 ''' paths{i} '''']);
    tk = regexp(o, '([0-9a-f]{64})', 'tokens', 'once');
    if ~strcmp(tk{1}, sha0{i})
        error('GATE:P3CODEGEN:TOUCHED — %s.slx 内容哈希变化（%s）', names{i}, paths{i});
    end
end
fprintf('[23S] 哈希门禁 PASS（stream/plantstepe/main 三模型零改动）\n');
fprintf('[23S] ALL DONE\n');

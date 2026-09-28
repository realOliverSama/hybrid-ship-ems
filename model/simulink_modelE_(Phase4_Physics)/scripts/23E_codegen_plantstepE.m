%% ========================================================================
%  23E_codegen_plantstepE.m — S4-G2（spec: S4_FULLCTRL_CONSTRUCTION_SPEC.md）
%  用途: 按场景烘焙（base 注入 + DefaultParameterBehavior=Inlined），Simulink
%        Coder 将 HybridShip_ModelE_PlantStepE.slx（20E 产物，审计 overall 前置）
%        生成 C 代码到 EMS04_CHIL_FULLCTRL/Host/plantstep_e/<scenario>/，并写
%        build_manifest.json 指纹。
%  场景: base 变量 CODEGEN_SCENARIOS_E（缺省 {'S20'}；S20=SCENARIOS(20) 复合航次，
%        与 Model D PlantStep 先例同口径）。
%  注入（base，slbuild 编译期解析）: SCN / EMS_CTRL_LAW=6 / EMS_MODE=1 /
%    PMS_SOURCE_MODE=1 / Bat_SOC_init / VOYAGE_SPEED_MODE / VOYAGE_V_TIME_S /
%    VOYAGE_V_VALUE_MPS / VOYAGE_CURRENT_MODE / VOYAGE_CURRENT_TIME_S /
%    VOYAGE_CURRENT_VALUE_MPS / VOYAGE_WATER_DEPTH_M / VOYAGE_MANUAL_CMD_TS。
%    （05 已删，ECMS_*/AI_* 不再被引用，无需注入。）
%  codegen 配置（运行期 set_param，绝不 save_system 主模型/PlantStepE——模型只装载）:
%    'SystemTargetFile','grt.tlc'（主案，Model D 先例：ERT 拒连续状态，
%      grt 生产产物即 Model D libplantstep dylib 同路径）
%    'GenCodeOnly','on'（编译由 Host/tools/build_plantstep_e.sh 承担）
%    'DefaultParameterBehavior','Inlined' / 'SupportNonFinite','on' /
%    'MatFileLogging','off' / 'GenerateReport','off' / 'ExternalMode','off'
%  红线: PlantStepE.slx 内容哈希 slbuild 前后复核（不等则 GATE:CODEGENE:TOUCHED）；
%        主模型 HybridShip_ModelE.slx 零改动（哈希前后复核）。
%  失败语义: 单场景失败记录后继续；结束时任一场景失败即 error('GATE:CODEGENE:FAILED')。
%  运行: cd <模型工作区根>; eval(fileread('scripts/23E_codegen_plantstepE.m'))
%        可选: CODEGEN_SCENARIOS_E={'S20'}; 后再 eval
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

MDL_PS = 'HybridShip_ModelE_PlantStepE';
psPath = fullfile(ROOT, 'model', [MDL_PS '.slx']);
mainPath = fullfile(ROOT, 'model', 'HybridShip_ModelE.slx');
hostRoot = fullfile('/Users/oliverlyu/Projects/hybrid-ship-ems', 'firmware', ...
    'stm32n6', '04_SourceCode', 'SoftwarePackage', 'EMS_Projects', 'EMS04_CHIL_FULLCTRL', 'Host');
if ~exist(fullfile(ROOT, 'experiments'), 'dir') && exist(fullfile(ROOT, '..', 'EMS04_CHIL_FULLCTRL'), 'dir')
    % cwd 兜底（非仓库布局时不猜）
end
% EMS Host 目标目录存在性校验（路径纪律：本脚本默认布局为仓库标准布局）
if ~exist(hostRoot, 'dir')
    error('GATE:CODEGENE:HOSTDIR — EMS Host 目录不存在: %s', hostRoot);
end

if ~exist(psPath, 'file')
    error('GATE:CODEGENE:NO_PS — 先运行 20E_build_plantstepE_model.m');
end
if ~evalin('base', "exist('CODEGEN_SCENARIOS_E','var')") && ~exist('CODEGEN_SCENARIOS_E','var')
    CODEGEN_SCENARIOS_E = {'S20'};
end

% ---- 哈希基线 ----
[~, h1] = system(['shasum -a 256 ''' psPath '''']);
tk = regexp(h1, '([0-9a-f]{64})', 'tokens', 'once'); psSha = tk{1};
[~, h2] = system(['shasum -a 256 ''' mainPath '''']);
tk = regexp(h2, '([0-9a-f]{64})', 'tokens', 'once'); mainSha = tk{1};

if ~bdIsLoaded(MDL_PS)
    load_system(psPath);
end

anyFail = false;
for s = 1:numel(CODEGEN_SCENARIOS_E)
    scnName = CODEGEN_SCENARIOS_E{s};
    sid = 0;
    SC = evalin('base', 'SCENARIOS');
    for k = 1:numel(SC)
        if any(strcmpi(SC(k).name, scnName)) || strcmpi(SC(k).name, [scnName '_composite_voyage'])
            sid = k; scnName = SC(k).name; break;
        end
    end
    if sid == 0
        error('GATE:CODEGENE:SCN — 场景 %s 未在 SCENARIOS 中找到', scnName);
    end
    SCN = SC(sid);
    outDir = fullfile(hostRoot, 'plantstep_e', scnName);
    if ~exist(outDir, 'dir'), mkdir(outDir); end

    % ---- base 注入（编译期解析） ----
    % 注入集合与 +model/run_scenario_sim 同口径（但不调 sim）：
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
    % VOYAGE_MANUAL_CMD_TS 已由 ModelE_TestScenarios 定义（base）；显式兜底
    if ~evalin('base', 'exist(''VOYAGE_MANUAL_CMD_TS'',''var'')')
        assignin('base', 'VOYAGE_MANUAL_CMD_TS', [0 0; 1e6 0]);
    end

    % ---- codegen 配置（运行期 set_param，不 save_system） ----
    cfgApplied = {}; cfgFailed = {};
    cfgs = {'GenCodeOnly','on'; 'DefaultParameterBehavior','Inlined'; ...
            'SupportNonFinite','on'; 'MatFileLogging','off'; ...
            'GenerateReport','off'; 'ExternalMode','off'; ...
            'SolverMode','SingleTasking'};
    for c = 1:size(cfgs, 1)
        try
            set_param(MDL_PS, cfgs{c,1}, cfgs{c,2});
            cfgApplied{end+1} = cfgs{c,1};
        catch
            cfgFailed{end+1} = cfgs{c,1};
        end
    end
    % TLC 主案 = grt.tlc（Model D 先例定案：本类含连续状态模型 ERT 报
    %   ContinuousTimeNotSupported ×10 拒绝（2026-09-28 实测，同 Model D 2026-09-02
    %   fallback 记录），GRT 产物为 Model D 生产 dylib 同路径）。
    tlc = 'grt.tlc'; usedGRT = true;
    set_param(MDL_PS, 'SystemTargetFile', tlc);
    tkT = regexp(tlc, '(\w+)\.tlc', 'tokens', 'once'); tlcName = tkT{1};

    % ---- 幂等：删除本场景旧 rtw 目录 ----
    old = dir(fullfile(outDir, [MDL_PS '_*']));
    priorRtw = ~isempty(old);
    for k = 1:numel(old)
        rmdir(fullfile(old(k).folder, old(k).name), 's');
    end

    % ---- slbuild（pwd 即生成目录） ----
    prevDir = pwd;
    cd(outDir);
    genErr = '';
    try
        slbuild(MDL_PS);
        genOK = ~isempty(dir(fullfile(outDir, [MDL_PS '_*'])));
    catch ge
        genOK = false; genErr = ge.message;
    end
    cd(prevDir);
    % slprj 缓存立即清除（AGENTS §13 永不入库）
    if exist(fullfile(ROOT, 'slprj'), 'dir'), rmdir(fullfile(ROOT, 'slprj'), 's'); end

    if ~genOK
        fprintf('[23E] %s codegen FAILED: %s\n', scnName, string(genErr));
        anyFail = true;
        continue;
    end

    % ---- 产物逐文件 sha256 ----
    rtwDir = fullfile(outDir, [MDL_PS '_' tlcName '_rtw']);
    if ~exist(rtwDir, 'dir')
        % 命名兜底：任取唯一 *_rtw
        d = dir(fullfile(outDir, [MDL_PS '_*_rtw']));
        rtwDir = fullfile(d(1).folder, d(1).name);
    end
    files = [dir(fullfile(rtwDir, '*.c')); dir(fullfile(rtwDir, '*.h')); ...
             dir(fullfile(rtwDir, '*.cnf')); dir(fullfile(rtwDir, '*.xml'))];
    fileSha = struct('name', {}, 'sha256', {});
    for k = 1:numel(files)
        [~, ho] = system(['shasum -a 256 ''' fullfile(rtwDir, files(k).name) '''']);
        tkF = regexp(ho, '([0-9a-f]{64})', 'tokens', 'once');
        fileSha(end+1) = struct('name', files(k).name, 'sha256', tkF{1});
    end

    % ---- manifest ----
    man = struct();
    man.schema = 's4_plantstepE_codegen_manifest_v1';
    man.created_utc = char(datetime('now','TimeZone','UTC','Format','yyyy-MM-dd''T''HH:mm:ss''Z'''));
    man.matlab = version;
    man.scenario = scnName;
    man.model = MDL_PS;
    man.model_sha256 = psSha;
    man.main_model_sha256 = mainSha;
    man.tlc = tlc; man.used_grt_fallback = usedGRT;
    man.cfg_applied = {cfgApplied}; man.cfg_failed = {cfgFailed};
    man.prior_rtw_existed = priorRtw;
    man.rtw_dir = strrep(rtwDir, [hostRoot '/'], 'Host/');
    man.files = fileSha;
    man.note = 'dylib 由 Host/tools/build_plantstep_e.sh 编译（plantstep_e_shim.c + 生成码）；本脚本不编译。';
    fid = fopen(fullfile(outDir, 'build_manifest.json'), 'w');
    fwrite(fid, jsonencode(man, 'PrettyPrint', true)); fclose(fid);
    fprintf('[23E] %s -> %s（%d files）\n', scnName, rtwDir, numel(files));
end

% ---- 红线复核：PlantStepE / 主模型零改动 ----
[~, h1b] = system(['shasum -a 256 ''' psPath '''']);
tk = regexp(h1b, '([0-9a-f]{64})', 'tokens', 'once'); psSha2 = tk{1};
[~, h2b] = system(['shasum -a 256 ''' mainPath '''']);
tk = regexp(h2b, '([0-9a-f]{64})', 'tokens', 'once'); mainSha2 = tk{1};
if ~strcmp(psSha, psSha2)
    error('GATE:CODEGENE:TOUCHED — PlantStepE.slx 内容哈希变化');
end
if ~strcmp(mainSha, mainSha2)
    error('GATE:CODEGENE:MAIN_TOUCHED — 主模型内容哈希变化');
end

if anyFail
    error('GATE:CODEGENE:FAILED — 存在失败场景（manifest 已落盘可查）');
end
fprintf('[23E] ALL DONE\n');

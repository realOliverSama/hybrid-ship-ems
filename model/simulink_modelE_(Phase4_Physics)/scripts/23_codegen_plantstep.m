%% ========================================================================
%  23_codegen_plantstep.m — T-604（spec: ems01-chil-v3-closedloop,
%  FR-602 / NFR-601 / AC-602）
%  用途: 按场景烘焙 VOYAGE_*/SCN（base 注入后 codegen 期解析为常量内联），
%        Simulink Coder 将被控对象子集模型 HybridShip_ModelD_PlantStep.slx
%        （T-601 产物，结构审计 overall=true 为前置）生成 C 代码到 EMS 工程
%        EMS01_CHIL_ECMS/Host/plantstep/<scenario>/HybridShip_ModelD_PlantStep_ert_rtw/，
%        并逐场景写 build_manifest.json 指纹（design §4.1）。
%  场景: base 变量 CODEGEN_SCENARIOS（缺省 {'S20','voyage002'} 顺序双场景，
%        FD1）；场景构造与 22_run_gp1_gate.m §4 完全同构（S20=SCENARIOS(20)；
%        voyage002=fleet json id==2 -> ecms25.voyage_scn_v2，66:96-107 同构）。
%  注入: 与 22 §3/§9 同集合（base 工作区，slbuild 编译期解析）:
%    SCN / EMS_CTRL_LAW=7 / EMS_MODE=1 / AI_Pred_Source=0 / Bat_SOC_init /
%    VOYAGE_SPEED_MODE=1 / VOYAGE_V_TIME_S / VOYAGE_V_VALUE_MPS /
%    VOYAGE_CURRENT_MODE=1 / VOYAGE_CURRENT_TIME_S / VOYAGE_CURRENT_VALUE_MPS
%    （VOYAGE_* 经 ecms2.scn_field 缺省 [0 1]/[0 0]，同 22:194-198）。
%    PlantStep 无 05_EMS_Controller，EMS_* 三变量为与参考运行（64/66 law=7
%    环境）一致的兜底注入，模型不引用（22 §3 同口径）。
%  codegen 配置（运行期 set_param，绝不 save_system——模型只装载）:
%    'SystemTargetFile','ert.tlc'   主案 ERT（design §4.1）；slbuild 失败
%                                   （许可/工具链/块不支持）时 try/catch 回退
%                                   'grt.tlc' 并在 manifest 记录（双路径均失败
%                                   则 GATE:CODEGEN:SLBUILD 终止）；
%    'GenCodeOnly','on'             不自动编译——编译由 EMS01 Host/tools/
%                                   build_plantstep.sh 承担（FD2）；
%    'DefaultParameterBehavior','Inlined'   按场景烘焙关键（base 参数编译期
%                                   全部内联为常量，Constant 块引用的 SCN.*/
%                                   VOYAGE_*/BSFC 等随之固化）；
%    'ExternalMode','off' / 'GenerateReport','off' / 'SupportNonFinite','on' /
%    'InlineInvariantSignals','on' / 'DynamicMemoryAllocation','Never'（禁动态
%    内存；本模型无可变尺寸信号本就不触发——参数名以 R2026a 实测为准，若
%    set_param 失败仅记入 manifest config_failed 不阻断）/
%    'MatFileLogging','off' / 'SolverMode','SingleTasking'（连续状态模型必须
%    单任务；若该参数名非法则失败记录，模型默认即为单任务）。
%  生成目录: cd <scenario>/ 后 slbuild（pwd 即 codegen 目录，实现最简——
%    不用 Simulink.fileGenControl），产物落在
%    plantstep/<scenario>/HybridShip_ModelD_PlantStep_ert_rtw/；slprj 缓存
%    （若产生）生成后立即删除（AGENTS §13 永不入库）。
%  幂等（FD1）: 每场景重跑先删除本脚本自身产物（rtw 子目录），与
%    20_build_plantstep_model.m 对旧 PlantStep.slx 的 delete 前例同类；
%    prior_rtw_existed 记入 manifest。
%  manifest 分工（与 EMS01/Host/tools/build_plantstep.sh 头注释互为镜像）:
%    本脚本（MATLAB）全责写 <scenario>/build_manifest.json——生成时间、
%    MATLAB 版本（regexp version 取 R20xx）、场景名/run_id、PlantStep.slx 与
%    config/Model_Parameters.m sha256（system shasum -a 256，T-602 诊断定案：
%    java fis.read 传参不回填缺陷禁用）、场景变量指纹（SCN 关键字段 + 注入
%    VOYAGE_* 全量 jsonencode -> 临时文件 shasum）、代码生成配置 applied/
%    failed 清单、ert/grt 标志、生成产物逐文件 sha256。
%    build_plantstep.sh 不写 manifest（只编译并打印 dylib sha256 供核对；
%    dylib 可由 manifest 记录的输入复现）。
%  红线: PlantStep.slx 内容哈希 slbuild 前后复核，不等则 GATE:CODEGEN:TOUCHED
%    （主模型/20/21/22 脚本/config/+model/冻结资产零改动；不写 Host/reference/）。
%    前置: T-601 审计 overall=true 且 G-P1 门禁（22 号脚本）该场景 overall=true
%    （design Testing Strategy：前一层全绿才进入后一层）。
%  失败语义: 单场景失败记录后继续下一场景；结束时任一场景失败即
%    error('GATE:CODEGEN:FAILED')（成功场景 manifest 已落盘可查，机器可读优先）。
%  运行（本文件名数字开头，同 20/21/22 惯例只能 eval 执行；脚本不得含局部函数）:
%    cd <模型工作区根>   % 即 model/simulink_modelD_(EMS_NN_AECMS)/
%    eval(fileread('scripts/23_codegen_plantstep.m'))
%    % 可选: CODEGEN_SCENARIOS={'S20'}; 后再 eval（缺省双场景）
%  ========================================================================

% ---- §20.2 路径纪律: 本脚本位于模型工作区根下 2 层（scripts/），禁绝对路径 ----
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % eval(fileread) 顶层 mfilename 为空 -> 以 cwd 兜底（须位于根）
end
cd(ROOT);  % 63 教训: eval 下相对路径脚本必须位于工作区根
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'), ...
    fullfile(ROOT, 'scripts', '+ecms2'), fullfile(ROOT, 'scripts', '+ecms25'));
fprintf('\n[23] ===== T-604 plantstep codegen per-scenario bake (FR-602/AC-602) =====\n');
fprintf('[23] ROOT = %s | start: %s\n', ROOT, datestr(now, 'yyyy-mm-dd HH:MM:SS'));
fprintf('[23] MATLAB %s\n', version);

MDL_PS = 'HybridShip_ModelD_PlantStep';
psPath = fullfile(ROOT, 'model', [MDL_PS '.slx']);

%% ---- 1. 场景清单（FD1: base 变量 CODEGEN_SCENARIOS，缺省双场景顺序生成） ----
if evalin('base', 'exist(''CODEGEN_SCENARIOS'', ''var'')') == 1
    scnList = evalin('base', 'CODEGEN_SCENARIOS');
    if isstring(scnList), scnList = cellstr(scnList); end
    if ~iscell(scnList), scnList = {char(scnList)}; end
else
    scnList = {'S20', 'voyage002'};
end
for si = 1:numel(scnList)
    if ~(ischar(scnList{si}) && any(strcmp(scnList{si}, {'S20', 'voyage002'})))
        error('GATE:CODEGEN:SCENARIO', ...
            '[23] CODEGEN_SCENARIOS 元素非法（仅 ''S20'' | ''voyage002''）: ''%s''', ...
            char(scnList{si}));
    end
end
fprintf('[23] 场景: %s\n', strjoin(scnList, ', '));

%% ---- 2. 预检: 模型 + T-601 审计 overall=true（只读） ----
auditPath = fullfile(ROOT, 'results', 'ecms_phase25', 'plantstep', ...
    'plantstep_build_audit.json');
if ~(exist(psPath, 'file') == 2 || exist(psPath, 'file') == 4)
    error('GATE:CODEGEN:PRE', '[23] 缺少 %s（先执行 20_build_plantstep_model.m）', ...
        fullfile('model', [MDL_PS '.slx']));
end
if ~(exist(auditPath, 'file') == 2)
    error('GATE:CODEGEN:PRE', '[23] 缺少 %s（T-601 结构审计产物）', ...
        fullfile('results', 'ecms_phase25', 'plantstep', 'plantstep_build_audit.json'));
end
auditJ = jsondecode(fileread(auditPath));
if ~(isfield(auditJ, 'overall') && auditJ.overall)
    error('GATE:CODEGEN:PRE', '[23] T-601 结构审计 overall~=true，禁止进入 codegen');
end

%% ---- 3. base 注入两份 config（22 §3 同口径；slbuild 编译期自 base 解析） ----
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));

%% ---- 4. EMS 工程目标目录（相对 ROOT 上推两级，禁绝对路径写死） ----
emsRoot = fullfile(ROOT, '..', '..', 'firmware', 'stm32n6', '04_SourceCode', ...
    'SoftwarePackage', 'EMS_Projects', 'EMS01_CHIL_ECMS');
if ~(exist(emsRoot, 'dir') == 7)
    error('GATE:CODEGEN:PRE', '[23] EMS 工程目录不存在: %s', ...
        fullfile('firmware', 'stm32n6', '04_SourceCode', 'SoftwarePackage', ...
        'EMS_Projects', 'EMS01_CHIL_ECMS'));
end

% ---- 模型/参数基准哈希（参数文件全场景共用，模型逐场景复核） ----
[stH, outH] = system(sprintf('shasum -a 256 "%s"', ...
    fullfile(ROOT, 'config', 'Model_Parameters.m')));
if stH ~= 0
    error('GATE:CODEGEN:HASH', '[23] shasum(Model_Parameters.m) 失败: %s', outH);
end
paramSha = regexp(outH, '^[0-9a-f]{64}', 'match', 'once');

% ---- codegen 配置清单（FD2；SystemTargetFile 在场景内走 ert->grt 双路径） ----
CFG = { ...
    'GenCodeOnly',              'on'; ...
    'DefaultParameterBehavior', 'Inlined'; ...
    'GenerateReport',           'off'; ...
    'ExternalMode',             'off'; ...
    'SupportNonFinite',         'on'; ...
    'InlineInvariantSignals',   'on'; ...
    'DynamicMemoryAllocation',  'Never'; ...
    'MatFileLogging',           'off'; ...
    'SolverMode',               'SingleTasking'};

scnStatus = cell(1, numel(scnList));
for si = 1:numel(scnList)
    scnTag = scnList{si};
    st = struct('scenario', scnTag, 'ok', false, 'target', '', 'manifest', '', ...
        'error_id', '', 'error_msg', '');
    try
        % ================= 场景构造（22 §4 同构） =================
        switch scnTag
            case 'S20'
                scn = evalin('base', 'SCENARIOS(20)');
                if ~(isfield(scn, 'name') && strcmp(scn.name, 'S20_composite_voyage'))
                    error('GATE:CODEGEN:SCENARIO', ...
                        '[23] SCENARIOS(20).name=''%s'' 非预期 S20_composite_voyage', ...
                        scn.name);
                end
                runId = 'G2_S20_composite_voyage_matrix';
            case 'voyage002'
                vid = 2;
                fleetJson = fullfile(ROOT, 'results', 'ecms_phase25', 'fleet', ...
                    'voyages_seed20260829.json');
                if ~(exist(fleetJson, 'file') == 2)
                    error('GATE:CODEGEN:SCENARIO', '[23] 缺少 fleet json: %s', ...
                        fullfile('results', 'ecms_phase25', 'fleet', ...
                        'voyages_seed20260829.json'));
                end
                VJ = jsondecode(fileread(fleetJson));
                vSel = [];
                for j = 1:numel(VJ.voyages)
                    if VJ.voyages(j).id == vid, vSel = VJ.voyages(j); break; end
                end
                if isempty(vSel)
                    error('GATE:CODEGEN:SCENARIO', '[23] fleet json 中无 id==%d 航次', vid);
                end
                scn = ecms25.voyage_scn_v2(vSel);
                runId = sprintf('G2_test_voyage_%03d_v2', vid);
        end
        fprintf('[23] ---- 场景 %s | run_id=%s | stop_time=%.6g s | SOC_init=%.6g ----\n', ...
            scnTag, runId, scn.stop_time_s, scn.initial_SOC);

        % ---- 前置: G-P1 门禁该场景 overall=true（design Testing Strategy 分层） ----
        gp1Json = fullfile(emsRoot, 'Host', 'reference', ['v3_gp1_' scnTag '_gate.json']);
        if ~(exist(gp1Json, 'file') == 2)
            error('GATE:CODEGEN:PRE', ...
                '[23] 缺少 G-P1 门禁结果 %s（先执行 22_run_gp1_gate.m，GP1_SCENARIO=''%s''）', ...
                fullfile('Host', 'reference', ['v3_gp1_' scnTag '_gate.json']), scnTag);
        end
        gj = jsondecode(fileread(gp1Json));
        if ~(isfield(gj, 'overall') && gj.overall)
            error('GATE:CODEGEN:PRE', ...
                '[23] G-P1 门禁（%s）overall~=true——前层未全绿，禁止进入 codegen', scnTag);
        end

        % ================= base 注入（22 §9 同集合，编译期烘焙源） =================
        vT = ecms2.scn_field(scn, 'v_time_s', [0 1]);
        vV = ecms2.scn_field(scn, 'v_value_mps', [0 0]);
        cT = ecms2.scn_field(scn, 'current_time_s', [0 1]);
        cV = ecms2.scn_field(scn, 'current_value_mps', [0 0]);
        assignin('base', 'SCN', scn);
        assignin('base', 'EMS_CTRL_LAW', 7);
        assignin('base', 'EMS_MODE', 1);
        assignin('base', 'AI_Pred_Source', 0);
        assignin('base', 'Bat_SOC_init', scn.initial_SOC);
        assignin('base', 'VOYAGE_SPEED_MODE', 1);
        assignin('base', 'VOYAGE_V_TIME_S', vT);
        assignin('base', 'VOYAGE_V_VALUE_MPS', vV);
        assignin('base', 'VOYAGE_CURRENT_MODE', 1);
        assignin('base', 'VOYAGE_CURRENT_TIME_S', cT);
        assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', cV);

        % ================= 目标目录（幂等清理自身产物） =================
        genDir = fullfile(emsRoot, 'Host', 'plantstep', scnTag);
        rtwDir = fullfile(genDir, [MDL_PS '_ert_rtw']);   % 实际目录随 targetUsed 更新（ert/grt 回退，T-604 执行修）
        mfPath = fullfile(genDir, 'build_manifest.json');
        if ~exist(genDir, 'dir'), mkdir(genDir); end
        for pd = {[MDL_PS '_ert_rtw'], [MDL_PS '_grt_rtw']}
            if exist(fullfile(genDir, pd{1}), 'dir') == 7
                rmdir(fullfile(genDir, pd{1}), 's');      % 重跑覆盖（FD1，自产目录；双目标前例均清）
            end
        end
        if exist(fullfile(genDir, 'slprj'), 'dir') == 7
            rmdir(fullfile(genDir, 'slprj'), 's');        % 缓存不入库（AGENTS §13）
        end

        % ================= 模型装载 + 基准哈希（只装载，不 save_system） =================
        if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end
        load_system(psPath);
        if ~strcmp(get_param(MDL_PS, 'Name'), MDL_PS)
            error('GATE:CODEGEN:PRE', '[23] 装载根名与 %s 不一致', MDL_PS);
        end
        [stH, outH] = system(sprintf('shasum -a 256 "%s"', psPath));
        if stH ~= 0
            error('GATE:CODEGEN:HASH', '[23] shasum(PlantStep.slx) 失败: %s', outH);
        end
        shaBefore = regexp(outH, '^[0-9a-f]{64}', 'match', 'once');

        % ================= codegen 配置（运行期 set_param，soft-set 记录） =================
        cfgApplied = {}; cfgFailed = {};
        haveGenCodeOnly = false; haveInlined = false;
        for ci = 1:size(CFG, 1)
            try
                set_param(MDL_PS, CFG{ci, 1}, CFG{ci, 2});
                cfgApplied{end+1} = sprintf('%s=%s', CFG{ci, 1}, CFG{ci, 2}); %#ok<SAGROW>
                if strcmp(CFG{ci, 1}, 'GenCodeOnly'), haveGenCodeOnly = true; end
                if strcmp(CFG{ci, 1}, 'DefaultParameterBehavior'), haveInlined = true; end
            catch errP
                cfgFailed{end+1} = sprintf('%s=%s (%s)', ... %#ok<SAGROW>
                    CFG{ci, 1}, CFG{ci, 2}, errP.message);
            end
        end
        if ~(haveGenCodeOnly && haveInlined)
            error('GATE:CODEGEN:PARAM', ...
                ['[23] 关键 codegen 参数设置失败（GenCodeOnly/DefaultParameterBehavior）——' ...
                 'applied: %s | failed: %s'], ...
                strjoin(cfgApplied, '; '), strjoin(cfgFailed, '; '));
        end

        % ================= slbuild（ert 主案 -> grt 回退，try/catch 双路径） =================
        tlcList = {'ert.tlc', 'grt.tlc'};
        targetUsed = ''; buildMsgs = {};
        for ti = 1:numel(tlcList)
            tlc = tlcList{ti};
            for pd = {[MDL_PS '_ert_rtw'], [MDL_PS '_grt_rtw']}
                if exist(fullfile(genDir, pd{1}), 'dir') == 7
                    rmdir(fullfile(genDir, pd{1}), 's');   % 上次尝试残留清理（双目标）
                end
            end
            try
                set_param(MDL_PS, 'SystemTargetFile', tlc);
                cd(genDir);   % pwd 即 codegen 目录：slbuild 产物落 <pwd>/<model>_ert_rtw
                fprintf('[23] slbuild(%s) ...\n', tlc);
                t0 = tic;
                slbuild(MDL_PS);   % GenCodeOnly=on：仅生成代码，不编译
                cd(ROOT);
                fprintf('[23] slbuild(%s) done in %.1f s\n', tlc, toc(t0));
                targetUsed = tlc;
                tgt3 = tlc(1:3);   % 'ert'/'grt'
                rtwDir = fullfile(genDir, sprintf('%s_%s_rtw', MDL_PS, tgt3));   % 产物目录跟随实际目标
                break;
            catch errB
                if ~strcmp(pwd, ROOT), cd(ROOT); end   % 失败亦恢复当前目录
                buildMsgs{end+1} = sprintf('%s: %s', tlc, errB.message); %#ok<SAGROW>
                fprintf('[23] WARN: %s 生成失败\n', tlc);
                if ti == numel(tlcList)
                    error('GATE:CODEGEN:SLBUILD', ...
                        '[23] %s 生成失败（ert/grt 双路径均试）:\n  %s', ...
                        MDL_PS, strjoin(buildMsgs, '\n  '));
                end
                fprintf('[23] 回退 %s ...\n', tlcList{ti + 1});
            end
        end

        % ---- .slxc 缓存清理（slbuild 落在场景目录；§13 缓存不入库/不污染产物区） ----
        slxcFile = fullfile(genDir, [MDL_PS '.slxc']);
        if exist(slxcFile, 'file') == 2, delete(slxcFile); end

        % ---- MATLAB 外部头复制进 rtw 目录（tmwtypes.h 等——build_plantstep.sh
        %      自包含编译所需；MATLABROOT 为仓库外依赖，头文件随产物复制即
        %      自包含可复现，逐文件 sha256 入 manifest） ----
        extInc = fullfile(matlabroot, 'extern', 'include');
        hdrList = {'tmwtypes.h'};
        for hi = 1:numel(hdrList)
            hSrc = fullfile(extInc, hdrList{hi});
            hDst = fullfile(rtwDir, hdrList{hi});
            if exist(hSrc, 'file') == 2 && exist(hDst, 'file') ~= 2
                copyfile(hSrc, hDst);
                fprintf('[23] 外部头 %s -> rtw 目录\n', hdrList{hi});
            end
        end

        % ---- 产物断言 + 模型零改动复核 ----
        if ~(exist(fullfile(rtwDir, [MDL_PS '.c']), 'file') == 2 && ...
             exist(fullfile(rtwDir, [MDL_PS '.h']), 'file') == 2)
            error('GATE:CODEGEN:ARTIFACT', ...
                '[23] rtw 产物缺少 %s.c/.h: %s', MDL_PS, rtwDir);
        end
        [stH, outH] = system(sprintf('shasum -a 256 "%s"', psPath));
        if stH ~= 0
            error('GATE:CODEGEN:HASH', '[23] shasum(PlantStep.slx) 失败: %s', outH);
        end
        shaAfter = regexp(outH, '^[0-9a-f]{64}', 'match', 'once');
        if ~strcmp(shaBefore, shaAfter)
            error('GATE:CODEGEN:TOUCHED', ...
                ['[23] PlantStep.slx 内容被 codegen 触碰（before=%s after=%s）——' ...
                 '运行期 set_param 不应写回模型，须排查'], shaBefore, shaAfter);
        end
        if exist(fullfile(genDir, 'slprj'), 'dir') == 7
            rmdir(fullfile(genDir, 'slprj'), 's');   % 生成后清理缓存（AGENTS §13）
        end
        if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end   % 丢弃运行期配置，不保存

        % ================= 场景变量指纹（SCN 关键字段 + 注入 VOYAGE_* 全量） =================
        keyNames = {'name', 'id', 'stop_time_s', 'initial_SOC', 'hotel_kW', ...
            'target_speed_mps', 'voyage_distance_m', 'profile_code', ...
            'resistance_profile', 'speed_profile_mode', ...
            'accel_ramp_mps2', 'decel_ramp_mps2'};
        scnKey = struct();
        for ki = 1:numel(keyNames)
            if isfield(scn, keyNames{ki})
                scnKey.(keyNames{ki}) = scn.(keyNames{ki});
            else
                scnKey.(keyNames{ki}) = 'ABSENT';
            end
        end
        scnFp = struct('scn_key', scnKey, ...
            'inject', struct('VOYAGE_SPEED_MODE', 1, ...
                             'VOYAGE_V_TIME_S', {vT}, 'VOYAGE_V_VALUE_MPS', {vV}, ...
                             'VOYAGE_CURRENT_MODE', 1, ...
                             'VOYAGE_CURRENT_TIME_S', {cT}, ...
                             'VOYAGE_CURRENT_VALUE_MPS', {cV}, ...
                             'Bat_SOC_init', scn.initial_SOC, ...
                             'EMS_CTRL_LAW', 7, 'EMS_MODE', 1, 'AI_Pred_Source', 0));
        scnJson = jsonencode(scnFp);
        tmpFp = [tempname '.json'];
        fid = fopen(tmpFp, 'w');
        if ~(fid > 0)
            error('GATE:CODEGEN:HASH', '[23] 无法写指纹临时文件 %s', tmpFp);
        end
        fwrite(fid, scnJson, 'char');
        fclose(fid);
        [stH, outH] = system(sprintf('shasum -a 256 "%s"', tmpFp));
        delete(tmpFp);
        if stH ~= 0
            error('GATE:CODEGEN:HASH', '[23] shasum(场景指纹) 失败: %s', outH);
        end
        scnSha = regexp(outH, '^[0-9a-f]{64}', 'match', 'once');

        % ================= 生成产物逐文件 sha256 =================
        listing = dir(fullfile(rtwDir, '**', '*'));
        artFiles = struct('file', {}, 'sha256', {}, 'bytes', {});
        for ii = 1:numel(listing)
            if listing(ii).isdir, continue; end
            pFull = fullfile(listing(ii).folder, listing(ii).name);
            [stH, outH] = system(sprintf('shasum -a 256 "%s"', pFull));
            if stH ~= 0
                error('GATE:CODEGEN:HASH', '[23] shasum(%s) 失败: %s', ...
                    listing(ii).name, outH);
            end
            rtwDirName = strrep(rtwDir, [genDir filesep], '');
            rel = [rtwDirName strrep(strrep(pFull, [rtwDir filesep], ''), filesep, '/')];
            artFiles(end+1) = struct('file', rel, ...
                'sha256', regexp(outH, '^[0-9a-f]{64}', 'match', 'once'), ...
                'bytes', listing(ii).bytes); %#ok<SAGROW>
        end
        fprintf('[23] 产物 %d 个文件 -> %s\n', numel(artFiles), ...
            rtwDir);

        % ================= build_manifest.json（本脚本全责，design §4.1） =================
        mf = struct();
        mf.task = 'T-604';
        mf.spec = 'ems01-chil-v3-closedloop (FR-602/NFR-601/AC-602)';
        mf.generated = char(datetime('now', 'Format', 'yyyy-MM-dd HH:mm:ss'));
        matlabVer = regexp(version, 'R\d{4}[ab]', 'match', 'once');
        if isempty(matlabVer), matlabVer = version; end
        mf.matlab_version = matlabVer;
        mf.matlab_version_string = version;
        mf.scenario = scnTag;
        mf.run_id = runId;
        mf.plantstep_model = 'model/HybridShip_ModelD_PlantStep.slx';
        mf.model_sha256 = shaBefore;
        mf.model_sha_unchanged_after_codegen = strcmp(shaBefore, shaAfter);
        mf.parameters_file = 'config/Model_Parameters.m';
        mf.parameters_sha256 = paramSha;
        mf.scenario_variables = scnFp;
        mf.scenario_variables_sha256 = scnSha;
        mf.scenario_variables_note = ...
            'jsonencode(scn_key + inject VOYAGE_*/SCN兜底) 的 sha256（临时文件 shasum -a 256）';
        mf.codegen = struct('target_file', targetUsed, ...
            'coder_type', 'ERT', 'gen_code_only', true, ...
            'config_applied', {cfgApplied}, 'config_failed', {cfgFailed}, ...
            'fallback_note', '');
        if strcmp(targetUsed, 'grt.tlc')
            mf.codegen.coder_type = 'GRT';
            mf.codegen.fallback_note = ...
                ['ert.tlc 生成失败回退 grt.tlc（design §4.1 显式 fallback；' ...
                 '失败消息: ' strjoin(buildMsgs, ' | ') '）'];
        end
        mf.artifacts = struct('dir', ...
            sprintf('Host/plantstep/%s/%s_%s_rtw', scnTag, MDL_PS, tgt3), ...
            'n_files', numel(artFiles), 'files', {artFiles});
        mf.idempotency = struct('prior_rtw_existed', priorRtw, ...
            'note', '每场景重跑先删除自身 rtw 产物目录（20_build_plantstep_model.m delete 前例同类）');
        mf.out_dir = sprintf('Host/plantstep/%s/', scnTag);
        mf.overall = true;
        try
            js = jsonencode(mf, 'PrettyPrint', true);
        catch
            js = jsonencode(mf);   % 旧版本无 PrettyPrint 时退化紧凑输出
        end
        fid = fopen(mfPath, 'w', 'n', 'UTF-8');
        if ~(fid > 0)
            error('GATE:CODEGEN:OUTDIR', '[23] 无法写 %s', ...
                fullfile('Host', 'plantstep', scnTag, 'build_manifest.json'));
        end
        fwrite(fid, js, 'char');
        fclose(fid);
        fprintf('[23] build_manifest.json -> %s | target=%s\n', ...
            fullfile('Host', 'plantstep', scnTag, 'build_manifest.json'), targetUsed);

        st.ok = true;
        st.target = targetUsed;
        st.manifest = fullfile('Host', 'plantstep', scnTag, 'build_manifest.json');
    catch errS
        st.error_id = errS.identifier;
        st.error_msg = errS.message;
        fprintf('[23] FAIL 场景 %s: [%s] %s\n', scnTag, errS.identifier, errS.message);
        if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end   % 丢弃运行期改动
    end
    scnStatus{si} = st;
end

%% ---- 5. 汇总出口（任一场景失败即 GATE 终止；成功场景 manifest 已落盘） ----
oks = cellfun(@(s) s.ok, scnStatus);
for si = 1:numel(scnList)
    st = scnStatus{si};
    if st.ok
        fprintf('[23] OK   %-9s target=%-8s manifest=%s\n', st.scenario, st.target, st.manifest);
    else
        fprintf('[23] FAIL %-9s [%s]\n', st.scenario, st.error_id);
    end
end
if ~all(oks)
    failedIds = scnStatus(~oks);
    failedNames = cell(1, numel(failedIds));
    for fi = 1:numel(failedIds), failedNames{fi} = failedIds{fi}.scenario; end
    failed = strjoin(failedNames, ', ');
    error('GATE:CODEGEN:FAILED', ...
        ['[23] T-604 codegen 失败场景: %s——排查后重跑（幂等，成功场景可保留）；' ...
         '设计层冲突（ERT 不兼容块/符号结构）按 spec Escalate-If 上报'], failed);
end
fprintf('[23] T-604 codegen 完成（%d 场景）：后续由 EMS01 Host/tools/build_plantstep.sh 编译 dylib\n', ...
    numel(scnList));
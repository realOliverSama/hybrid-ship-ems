%% ========================================================================
%  20_build_plantstep_model.m — T-601（spec: ems01-chil-v3-closedloop,
%  FR-601 / NFR-604 / AC-601）
%  用途: 装载主模型 -> SaveAs 生成被控对象子集变体 HybridShip_ModelD_PlantStep.slx
%        （plant-only: 删除 05_EMS_Controller 与 12_Data_Logging，根级 Inport
%        P_gen_ref_kW 替代 05 输出，11 项根级 Outport 暴露闭环接口），
%        并写出机器可读结构审计 JSON。
%  红线: 主模型 HybridShip_ModelD_DCBus.slx 零改动（脚本内内容哈希前后复核，
%        不等则 GATE:PLANTSTEP_MAIN_TOUCHED）；不修改 build_model_c.m、+model/、
%        config/、既有编号脚本；本脚本仅生成 PlantStep 模型与审计 JSON。
%  产物: model/HybridShip_ModelD_PlantStep.slx
%        results/ecms_phase25/plantstep/plantstep_build_audit.json
%  运行: eval(fileread('scripts/20_build_plantstep_model.m'))
%        （run() 不接受数字开头脚本名，惯例同 10_build_modelD.m）
%  方法: 装载主模型 -> SaveAs -> 原位变换（主 Agent 定案，T-601 FD1）。
%        - 05 的 7 输入源端口动态解析（Line/SrcPortHandle），Outport 1-7 与其
%          同源同端口（CMD_TICK 载荷 ≡ 05 输入，接口零新增）；
%        - 删 05 的 8 条线后不加新 Terminator（主 Agent 纠偏定案）：
%          01/1、03/2、03/3、04/1 的兄弟分支保留；真正失去唯一消费者的
%          Clock t/1、14_Battery_Converter/2(SOC)、03_Propulsion_Plant/4(accel)
%          分别直连 Outport 1(t_s)/3(SOC)/6(acceleration_mps2)；
%        - Outport 8-11 源动态解析并在审计 JSON 记录实测接线分类；
%        - 11 个既有 T_log_* Terminator 保持原样不动。
% ========================================================================

% ---- §20.2 路径纪律: 本脚本位于模型工作区根下 2 层（scripts/） ----
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % 兼容非文件方式执行（如 eval 场景）
end
addpath(fullfile(ROOT, 'scripts'));   % +model 包（model.md5file 等）

MDL_MAIN = 'HybridShip_ModelD_DCBus';
MDL_PS   = 'HybridShip_ModelD_PlantStep';
mainPath  = fullfile(ROOT, 'model', [MDL_MAIN '.slx']);
psPath    = fullfile(ROOT, 'model', [MDL_PS '.slx']);
auditDir  = fullfile(ROOT, 'results', 'ecms_phase25', 'plantstep');
auditPath = fullfile(auditDir, 'plantstep_build_audit.json');
if ~exist(fullfile(ROOT, 'model'), 'dir'), mkdir(fullfile(ROOT, 'model')); end
if ~exist(auditDir, 'dir'), mkdir(auditDir); end

% ---- 预期清单（源码事实: +model/build_model_c.m L54-71 构建调用清单；
%      L865-871 buildBatteryConverter 实际创建 14_Battery_Converter，
%      模型中不存在 07_* 子系统；主模型 blockdiagram 实测一致） ----
expectedSubs = reshape(sort({'01_Scenario_Manager', '02_Ship_Speed_Controller', ...
    '03_Propulsion_Plant', '04_Load_Measurement', '05_EMS_Controller', ...
    '06_Genset_Average_Plant', '08_Undervolt_Derate', '09_Bus_Residual_Monitor', ...
    '10_Fuel_Consumption', '11_Voyage_Stop_Logic', '12_Data_Logging', ...
    '13_DC_Bus', '14_Battery_Converter', '15_Bus_Voltage_Monitor'}), 1, []);
removedSubs = {'05_EMS_Controller', '12_Data_Logging'};
expectedRemain = reshape(sort(setdiff(expectedSubs, removedSubs)), 1, []);   % 应剩 12 个子系统
% 11 项根级 Outport（顺序即端口号 1..11；前 7 项 = CMD_TICK 载荷 ≡ 05 输入）
outNames = {'t_s', 'P_load_total_kW', 'SOC', 'v_cmd_mps', 'v_ship_mps', ...
    'acceleration_mps2', 'x_ship_m', 'fuel_total_kg', 'P_bat_kW', ...
    'P_prop_kW', 'voyage_stop'};
% Outport 8-10 的子系统内部 Outport 块名（端口号动态解析；语义唯一对应）
extSrc = { ...
    'fuel_total_kg', '10_Fuel_Consumption',  'fuel_total_kg'; ...
    'P_bat_kW',      '14_Battery_Converter', 'P_bat_bus_kW'; ...
    'P_prop_kW',     '03_Propulsion_Plant',  'P_prop_actual_kW'};

% ---- 状态跟踪（失败路径也落账审计 JSON） ----
st = struct('stage', 'init', 'inventory_ok', false, 'wiring_ok', false, ...
    'solver_ok', false, 'logging_off', false, 'compile_ok', false, ...
    'compile_msg', '', 'main_unchanged', false, 'error_id', '', 'error_msg', '');
found0 = {}; remaining0 = {}; wiringBad = {};
emsIn = struct('port', {}, 'src', {}, 'srcPort', {}, 'srcName', {}, 'wiring', {});
emsPorts = struct('name', {}, 'port', {}, 'dir', {});
emsOutPortNum = 0;
dstRec = struct('dst', '', 'dstPort', 0);
tlogMap = struct('name', {}, 'src', {}, 'srcName', {});
solverCfg = struct(); logBefore = ''; logAfter = '';
shaBefore = ''; shaAfter = '';
hadOldPS = false;
rootIn = struct('name', 'P_gen_ref_kW', 'dst', '');
outSrc = repmat({''}, 1, 11);
outWire = repmat({''}, 1, 11);
vsNote = '';

% ---- 主模型内容哈希（before）: 系统 shasum -a 256 ----
% （2026-09-02 诊断: java fis.read(buf) 经 MATLAB 传参不回填 MATLAB 侧 buf，
%   内联哈希恒为全零缓冲哈希——绝对值无效；改用系统 shasum，git status 兜底）
[stS, outS] = system(sprintf('shasum -a 256 "%s"', mainPath));
assert(stS == 0, 'GATE:PLANTSTEP_HASH', 'shasum 失败: %s', outS);
shaBefore = regexp(outS, '^[0-9a-f]{64}', 'match', 'once');

err0 = [];
try
    % ================= 阶段 A: 幂等清理 + 装载主模型 + SaveAs =================
    st.stage = 'load_and_saveas';
    if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end
    hadOldPS = exist(psPath, 'file') > 0;
    if hadOldPS, delete(psPath); end
    if bdIsLoaded(MDL_MAIN), close_system(MDL_MAIN, 0); end
    load_system(mainPath);                 % 主模型只读装载（此后不写回主模型）
    save_system(MDL_MAIN, psPath);         % SaveAs: 写新文件；内存改名语义不做假设
    if bdIsLoaded(MDL_MAIN), close_system(MDL_MAIN, 0); end
    if bdIsLoaded(MDL_PS),   close_system(MDL_PS, 0); end
    load_system(psPath);                   % 以新文件干净装载
    if ~bdIsLoaded(MDL_PS)
        error('GATE:PLANTSTEP_SAVEAS', '%s 装载失败', psPath);
    end
    PS = get_param(MDL_PS, 'Name');        % 实际根名（禁止假设 SaveAs 改名语义）
    if ~strcmp(PS, MDL_PS)
        error('GATE:PLANTSTEP_SAVEAS', '实际根名 %s 与 %s 不一致', PS, MDL_PS);
    end

    % ================= 阶段 B: 结构审计基线（变换前实测） =================
    st.stage = 'inventory_baseline';
    subsAll = find_system(PS, 'SearchDepth', 1, 'BlockType', 'SubSystem');
    found0 = reshape(sort(cellfun(@(s) s(length(PS)+2:end), subsAll, 'UniformOutput', false)), 1, []);
    st.inventory_ok = isequal(found0, expectedSubs);
    if ~st.inventory_ok
        error('GATE:PLANTSTEP_INVENTORY', '子系统清单不符, 实测: %s', strjoin(found0, ', '));
    end

    % 11 个既有记录 Terminator（T_log_1..11）接线实测（变换不触碰它们，仅记录）
    for k = 1:11
        tb = [PS '/T_log_' num2str(k)];
        phT = get_param(tb, 'PortHandles');
        ln = get_param(phT.Inport(1), 'Line');
        if ~(isnumeric(ln) && ln > 0)
            error('GATE:PLANTSTEP_INVENTORY', 'T_log_%d 无连线', k);
        end
        srcPH = get_param(ln, 'SrcPortHandle');
        srcBlk = get_param(srcPH, 'Parent');
        phSrc = get_param(srcBlk, 'PortHandles');
        srcPort = find(phSrc.Outport == srcPH, 1);
        if isempty(srcPort)
            error('GATE:PLANTSTEP_INVENTORY', 'T_log_%d 源端口解析失败', k);
        end
        if strcmp(get_param(srcBlk, 'BlockType'), 'SubSystem')
            ops = find_system(srcBlk, 'SearchDepth', 1, 'BlockType', 'Outport');
            srcNm = '';
            for q = 1:numel(ops)
                if str2double(get_param(ops{q}, 'Port')) == srcPort
                    srcNm = get_param(ops{q}, 'Name'); break;
                end
            end
        else
            [~, srcNm] = fileparts(srcBlk);
        end
        tlogMap(k).name = sprintf('T_log_%d', k);
        tlogMap(k).src = sprintf('%s/%d', srcBlk(length(PS)+2:end), srcPort);
        tlogMap(k).srcName = srcNm;
    end

    % ================= 阶段 C: 05_EMS_Controller 边界解析（删线前） =================
    st.stage = 'ems_boundary_resolve';
    emsSys = [PS '/05_EMS_Controller'];
    phEms = get_param(emsSys, 'PortHandles');
    nIn = numel(phEms.Inport); nOut = numel(phEms.Outport);
    % 05 接口端口名 -> 端口号（动态解析，同 wireTopLevel/emsPortMap L1102-1118 方式）
    ins05 = find_system(emsSys, 'SearchDepth', 1, 'BlockType', 'Inport');
    ops05 = find_system(emsSys, 'SearchDepth', 1, 'BlockType', 'Outport');
    for q = 1:numel(ins05)
        emsPorts(end+1) = struct('name', get_param(ins05{q}, 'Name'), ...
            'port', str2double(get_param(ins05{q}, 'Port')), 'dir', 'in'); %#ok<AGROW>
    end
    for q = 1:numel(ops05)
        emsPorts(end+1) = struct('name', get_param(ops05{q}, 'Name'), ...
            'port', str2double(get_param(ops05{q}, 'Port')), 'dir', 'out'); %#ok<AGROW>
        if strcmp(get_param(ops05{q}, 'Name'), 'P_gen_ref_kW')
            emsOutPortNum = str2double(get_param(ops05{q}, 'Port'));
        end
    end
    % 7 条输入线源端口逐线解析（SrcPortHandle -> 源块/端口号/信号名）
    emsIn = struct('port', {}, 'src', {}, 'srcPort', {}, 'srcName', {}, 'wiring', {});
    for k = 1:nIn
        ln = get_param(phEms.Inport(k), 'Line');
        if ~(isnumeric(ln) && ln > 0)
            error('GATE:PLANTSTEP_INVENTORY', '05 输入端口 %d 无连线', k);
        end
        srcPH = get_param(ln, 'SrcPortHandle');
        srcBlk = get_param(srcPH, 'Parent');
        phSrc = get_param(srcBlk, 'PortHandles');
        srcPort = find(phSrc.Outport == srcPH, 1);
        if isempty(srcPort)
            error('GATE:PLANTSTEP_INVENTORY', '05 输入端口 %d 源解析失败', k);
        end
        if strcmp(get_param(srcBlk, 'BlockType'), 'SubSystem')
            ops = find_system(srcBlk, 'SearchDepth', 1, 'BlockType', 'Outport');
            srcNm = '';
            for q = 1:numel(ops)
                if str2double(get_param(ops{q}, 'Port')) == srcPort
                    srcNm = get_param(ops{q}, 'Name'); break;
                end
            end
        else
            [~, srcNm] = fileparts(srcBlk);
        end
        emsIn(k).port = k;
        emsIn(k).src = srcBlk(length(PS)+2:end);
        emsIn(k).srcPort = srcPort;
        emsIn(k).srcName = srcNm;
    end
    % 05 唯一输出 P_gen_ref_kW 的目的端口（从被删线记录，禁止假设）
    if nOut ~= 1
        error('GATE:PLANTSTEP_INVENTORY', '05 输出端口数 %d != 1', nOut);
    end
    lnOut = get_param(phEms.Outport(1), 'Line');
    if ~(isnumeric(lnOut) && lnOut > 0)
        error('GATE:PLANTSTEP_INVENTORY', '05 输出无连线');
    end
    dstPH = get_param(lnOut, 'DstPortHandle');
    if ~isscalar(dstPH)
        error('GATE:PLANTSTEP_WIRING', '05 输出线存在多个目的端口, 数量=%d', numel(dstPH));
    end
    dstBlk = get_param(dstPH, 'Parent');
    phDst = get_param(dstBlk, 'PortHandles');
    dstPort = find(phDst.Inport == dstPH, 1);
    dstRec.dst = dstBlk(length(PS)+2:end);
    dstRec.dstPort = dstPort;
    if ~strcmp(dstRec.dst, '06_Genset_Average_Plant') || dstPort ~= 1
        error('GATE:PLANTSTEP_WIRING', ...
            '05 输出目的端口 %s/%d 非预期 06_Genset_Average_Plant/1', dstRec.dst, dstPort);
    end
    % 输入源名字集合校验（与 CMD_TICK 载荷逐一对应，顺序无关）
    expIn = {'P_load_total_kW', 'SOC', 'v_cmd_mps', 'v_ship_mps', ...
        'acceleration_mps2', 'x_ship_m', 't'};
    gotIn = {emsIn.srcName};
    if nIn ~= 7 || ~isempty(setdiff(expIn, gotIn)) || ~isempty(setdiff(gotIn, expIn))
        error('GATE:PLANTSTEP_INVENTORY', '05 输入源解析不符, 实测: %s', strjoin(gotIn, ', '));
    end

    % ================= 阶段 D: 原位变换 =================
    st.stage = 'transform';
    % D1. 删 05 的 7 条输入线与 1 条输出线（删分支段，不误伤共享源其他分支）
    for k = 1:nIn
        delete_line(get_param(phEms.Inport(k), 'Line'));
    end
    delete_line(lnOut);
    delete_block(emsSys);
    % D2. 删除 12_Data_Logging（确认无端口连线后删；实测为无端口说明子系统）
    sys12 = [PS '/12_Data_Logging'];
    ph12 = get_param(sys12, 'PortHandles');
    if ~isempty(ph12.Inport) || ~isempty(ph12.Outport)
        error('GATE:PLANTSTEP_DATALOGGING', '12_Data_Logging 存在连线, 拒绝删除');
    end
    delete_block(sys12);
    % D3. 源端口孤儿状态实测（决定 Outport 直连或分支; 不加任何新 Terminator）
    for k = 1:nIn
        phS = get_param([PS '/' emsIn(k).src], 'PortHandles');
        lnSrc = get_param(phS.Outport(emsIn(k).srcPort), 'Line');
        if isnumeric(lnSrc) && lnSrc > 0
            emsIn(k).wiring = 'branch_existing_line';      % 兄弟分支仍在
        else
            emsIn(k).wiring = 'sole_consumer_direct';      % 05 曾是唯一消费者
        end
    end
    % D4. 根级 Inport P_gen_ref_kW 接到记录的 06 目的端口
    % Inport 显式离散 0.01s——必须复现 05 chart 输出的采样语义（T-602 诊断:
    % 缺省连续采样使 Ramp Limiter 连续化直通、IC 失效，植物动力学漂移）
    add_block('simulink/Sources/In1', [PS '/P_gen_ref_kW'], 'Position', [40 250 70 280], ...
        'SampleTime', '0.01');
    rootIn.name = 'P_gen_ref_kW';
    rootIn.dst = sprintf('%s/%d', dstRec.dst, dstRec.dstPort);
    add_line(PS, 'P_gen_ref_kW/1', rootIn.dst, 'autorouting', 'on');
    % D5. Outport 1-7 源映射（= 05 被删输入线同源同端口, 接口零新增）
    for k = 1:nIn
        nm = emsIn(k).srcName;
        if strcmp(nm, 't'), nm = 't_s'; end   % Clock 块名 't' -> Outport 1 't_s'
        oi = find(strcmp(outNames(1:7), nm), 1);
        if isempty(oi)
            error('GATE:PLANTSTEP_WIRING', '输入源 %s 无对应 Outport', nm);
        end
        outSrc{oi} = sprintf('%s/%d', emsIn(k).src, emsIn(k).srcPort);
        outWire{oi} = emsIn(k).wiring;
    end
    % t_s 源断言: 必须是顶层 Clock 块 t
    idxT = find(strcmp({emsIn.srcName}, 't'), 1);
    if isempty(idxT) || ~strcmp(get_param([PS '/' emsIn(idxT).src], 'BlockType'), 'Clock')
        error('GATE:PLANTSTEP_WIRING', 't_s 源块非顶层 Clock');
    end
    % D6. Outport 1-7 建块并接线（孤儿源直连 / 非孤儿源分支, 由 Simulink 自动处理）
    for k = 1:7
        add_block('simulink/Sinks/Out1', [PS '/' outNames{k}], ...
            'Position', [1020 60+40*(k-1) 1050 90+40*(k-1)]);
        add_line(PS, outSrc{k}, [outNames{k} '/1'], 'autorouting', 'on');
    end
    % D7. Outport 8-10: 子系统内部 Outport 块名动态解析端口号后加分支
    for e = 1:size(extSrc, 1)
        oi = find(strcmp(outNames, extSrc{e, 1}), 1);
        sub = [PS '/' extSrc{e, 2}];
        opsE = find_system(sub, 'SearchDepth', 1, 'BlockType', 'Outport');
        pNum = -1;
        for q = 1:numel(opsE)
            if strcmp(get_param(opsE{q}, 'Name'), extSrc{e, 3})
                pNum = str2double(get_param(opsE{q}, 'Port')); break;
            end
        end
        if pNum < 1
            error('GATE:PLANTSTEP_WIRING', '%s 内未找到 Outport 块 %s', extSrc{e, 2}, extSrc{e, 3});
        end
        outSrc{oi} = sprintf('%s/%d', extSrc{e, 2}, pNum);
        outWire{oi} = 'branch_subsystem_output_line';
        add_block('simulink/Sinks/Out1', [PS '/' extSrc{e, 1}], ...
            'Position', [1020 60+40*(oi-1) 1050 90+40*(oi-1)]);
        add_line(PS, outSrc{oi}, [extSrc{e, 1} '/1'], 'autorouting', 'on');
    end
    % D8. Outport 11 (voyage_stop): 主模型中不存在该信号 —— 11_Voyage_Stop_Logic
    %     内部为 x_ship_m -> reach(Compare To Constant) -> stop(Stop Simulation)，
    %     PortCounts 仅 in=1、无 Outport（buildVoyageStop L988-999 与 .slx 实测一致）。
    %     design.md §3.3 规定 voyage_stop(bool) 为 Outport 11 —— 在 PlantStep 副本
    %     的 11 内部新增 Outport，源 = 驱动 Stop Simulation 的同一比较信号（reach
    %     输出），主模型零改动，控制语义不变。审计 JSON 记录此来源。
    vsSys = [PS '/11_Voyage_Stop_Logic'];
    phVs = get_param(vsSys, 'PortHandles');
    if ~isempty(phVs.Outport)
        error('GATE:PLANTSTEP_WIRING', '11_Voyage_Stop_Logic 已存在输出端口, 结构漂移');
    end
    phStop = get_param([vsSys '/stop'], 'PortHandles');
    lnStop = get_param(phStop.Inport(1), 'Line');
    if ~(isnumeric(lnStop) && lnStop > 0)
        error('GATE:PLANTSTEP_WIRING', '11_Voyage_Stop_Logic/stop 无连线');
    end
    srcStopPH = get_param(lnStop, 'SrcPortHandle');
    srcStopBlk = get_param(srcStopPH, 'Parent');
    [~, srcStopNm] = fileparts(srcStopBlk);
    if ~strcmp(srcStopNm, 'reach')
        error('GATE:PLANTSTEP_WIRING', '11 内 stop 源块 %s 非预期 reach', srcStopNm);
    end
    vsInRel = srcStopBlk(length(vsSys)+2:end);
    vsInPort = find(get_param(srcStopBlk, 'PortHandles').Outport == srcStopPH, 1);
    add_block('simulink/Sinks/Out1', [vsSys '/voyage_stop'], 'Position', [340 80 370 110]);
    add_line(vsSys, sprintf('%s/%d', vsInRel, vsInPort), 'voyage_stop/1');
    vsOutPort = str2double(get_param([vsSys '/voyage_stop'], 'Port'));
    outSrc{11} = sprintf('11_Voyage_Stop_Logic/%d', vsOutPort);
    outWire{11} = 'new_internal_outport_in_11_Voyage_Stop_Logic';
    vsNote = '11_Voyage_Stop_Logic 内新增 Outport voyage_stop, 源=驱动 Stop Simulation 的 reach 比较输出; 主模型不存在现成 voyage_stop 信号(design §3.3 要求 Outport 11)';
    add_block('simulink/Sinks/Out1', [PS '/voyage_stop'], ...
        'Position', [1020 60+40*10 1050 90+40*10]);
    add_line(PS, outSrc{11}, 'voyage_stop/1', 'autorouting', 'on');
    % D9. 根级 Outport 端口号强制 1..11 顺序
    for k = 1:11
        blk = [PS '/' outNames{k}];
        if str2double(get_param(blk, 'Port')) ~= k
            set_param(blk, 'Port', num2str(k));
        end
    end

    % ================= 阶段 E: 连线完备性检查（防静默断线） =================
    st.stage = 'wiring_check';
    subsRem = find_system(PS, 'SearchDepth', 1, 'BlockType', 'SubSystem');
    remaining0 = reshape(sort(cellfun(@(s) s(length(PS)+2:end), subsRem, 'UniformOutput', false)), 1, []);
    if ~isequal(remaining0, expectedRemain)
        error('GATE:PLANTSTEP_INVENTORY', ...
            '删除 05/12 后剩余子系统 %d 个非预期 12 个: %s', ...
            numel(remaining0), strjoin(remaining0, ', '));
    end
    wiringBad = {};
    rbs = find_system(PS, 'SearchDepth', 1, 'Type', 'block');
    for i = 1:numel(rbs)
        b = rbs{i}; bt = get_param(b, 'BlockType');
        ph = get_param(b, 'PortHandles');
        switch bt
            case 'SubSystem'
                for p = 1:numel(ph.Inport)
                    if ~(get_param(ph.Inport(p), 'Line') > 0)
                        wiringBad{end+1} = sprintf('%s/in%d', b, p); %#ok<AGROW>
                    end
                end
                for p = 1:numel(ph.Outport)
                    if ~(get_param(ph.Outport(p), 'Line') > 0)
                        wiringBad{end+1} = sprintf('%s/out%d', b, p); %#ok<AGROW>
                    end
                end
            case 'Inport'
                if ~(get_param(ph.Outport(1), 'Line') > 0)
                    wiringBad{end+1} = sprintf('%s/1', b); %#ok<AGROW>
                end
            case {'Outport', 'Terminator'}
                if ~(get_param(ph.Inport(1), 'Line') > 0)
                    wiringBad{end+1} = sprintf('%s/1', b); %#ok<AGROW>
                end
            case 'Clock'
                if ~(get_param(ph.Outport(1), 'Line') > 0)
                    wiringBad{end+1} = sprintf('%s/1', b); %#ok<AGROW>
                end
            otherwise
        end
    end
    wiringBad = cellfun(@(s) s(length(PS)+2:end), wiringBad, 'UniformOutput', false);
    st.wiring_ok = isempty(wiringBad);
    if ~st.wiring_ok
        error('GATE:PLANTSTEP_WIRING', '连线完备性检查失败: %s', strjoin(wiringBad, ', '));
    end

    % ================= 阶段 F: 求解器断言 + SignalLogging off =================
    st.stage = 'solver_and_logging';
    solverCfg = struct('SolverType', get_param(PS, 'SolverType'), ...
        'Solver', get_param(PS, 'Solver'), 'FixedStep', get_param(PS, 'FixedStep'), ...
        'StopTime', get_param(PS, 'StopTime'));
    st.solver_ok = strcmp(solverCfg.SolverType, 'Fixed-step') && ...
        strcmp(solverCfg.Solver, 'ode3') && strcmp(solverCfg.FixedStep, '0.01');
    if ~st.solver_ok
        error('GATE:PLANTSTEP_SOLVER', '求解器非 Fixed-step ode3 @0.01s: %s/%s/%s', ...
            solverCfg.SolverType, solverCfg.Solver, solverCfg.FixedStep);
    end
    logBefore = get_param(PS, 'SignalLogging');
    set_param(PS, 'SignalLogging', 'off');   % 记录由闭环 harness 承担（design §3.1）
    logAfter = get_param(PS, 'SignalLogging');
    st.logging_off = strcmp(logAfter, 'off');

    % ================= 阶段 G: 保存 PlantStep =================
    st.stage = 'save_plantstep';
    save_system(PS);

    % ================= 阶段 H: 编译冒烟（base 注入两份 config，同 L16-19） =================
    st.stage = 'compile_smoke';
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    % EMS 变量兜底（05 已删除, PlantStep 不读取; 保持 base 与 build_model_c L35-37 一致）
    assignin('base', 'EMS_CTRL_LAW', 7);
    assignin('base', 'EMS_MODE', 1);
    try
        so = sim(PS, 'ReturnWorkspaceOutputs', 'on', 'StopTime', '0.01'); %#ok<NASGU>
        st.compile_ok = true;
        st.compile_msg = 'sim(StopTime=0.01, ReturnWorkspaceOutputs=on) 通过';
        clear so;
    catch errC
        st.compile_msg = errC.message;
        error('GATE:PLANTSTEP_COMPILE', '%s', errC.message);
    end

    % ================= 阶段 I: 主模型哈希复核（after） =================
    st.stage = 'main_hash_verify';
    [stS2, outS2] = system(sprintf('shasum -a 256 "%s"', mainPath));
    assert(stS2 == 0, 'GATE:PLANTSTEP_HASH', 'shasum 失败: %s', outS2);
    shaAfter = regexp(outS2, '^[0-9a-f]{64}', 'match', 'once');
    st.main_unchanged = strcmp(shaAfter, shaBefore);
    if ~st.main_unchanged
        error('GATE:PLANTSTEP_MAIN_TOUCHED', '主模型哈希前后不一致, 拒绝通过');
    end
    st.stage = 'done';
catch err0
    st.error_id = err0.identifier;
    st.error_msg = err0.message;
end

% ================= 阶段 J: 审计 JSON（成败均落账, 机器可读） =================
if bdIsLoaded(MDL_PS)
    subsRem = find_system(MDL_PS, 'SearchDepth', 1, 'BlockType', 'SubSystem');
    remaining0 = reshape(sort(cellfun(@(s) s(length(MDL_PS)+2:end), subsRem, 'UniformOutput', false)), 1, []);
end
rootOut = struct('index', {}, 'name', {}, 'src', {}, 'wiring', {});
for k = 1:11
    rootOut(k).index = k;
    rootOut(k).name = outNames{k};
    rootOut(k).src = outSrc{k};
    rootOut(k).wiring = outWire{k};
end
audit = struct();
audit.task = 'T-601';
audit.spec = 'ems01-chil-v3-closedloop (FR-601/NFR-604/AC-601)';
audit.generated = char(datetime('now', 'Format', 'yyyy-MM-dd HH:mm:ss'));
audit.matlab_version = regexp(version, 'R\d{4}[ab]', 'match', 'once');   % R2026a（version('-release') 返回 '2026a'，此处取标准记法）
audit.matlab_version_string = version;   % R2026a 已移除 version('matlab') 选项
audit.build_method = 'load main model -> SaveAs -> in-place transform (main agent decision, T-601 FD1)';
audit.main_model = 'HybridShip_ModelD_DCBus.slx';
audit.main_sha256_before = shaBefore;
audit.main_sha256_after = shaAfter;
audit.main_sha256_unchanged = st.main_unchanged;
audit.main_hash_note = 'sha256 via system shasum -a 256 (java fis.read pass-by-value bug fixed 2026-09-02, see T-602 diagnosis)';
audit.plantstep_model = 'model/HybridShip_ModelD_PlantStep.slx';
audit.idempotency = sprintf('old plantstep existed=%d, deleted before rebuild', hadOldPS);
audit.subsystem_inventory = struct('expected', {expectedSubs}, 'found', {found0}, ...
    'removed', {removedSubs}, 'remaining', {remaining0}, ...
    'disambiguation', 'buildBatteryConverter creates 14_Battery_Converter (build_model_c L865-871); no 07_* subsystem exists in HybridShip_ModelD_DCBus.slx; design.md conceptual names 07_Battery_Converter/12_Data_Logging_Doc map to actual 14_Battery_Converter/12_Data_Logging');
audit.ems_interface_ports = emsPorts;
audit.ems_out_port_number = emsOutPortNum;
audit.root_inports = rootIn;
audit.root_outports = rootOut;
audit.root_outport_11_note = vsNote;
audit.terminators_T_log_inherited = tlogMap;
audit.terminators_added_note = 'no new Terminator added (main agent correction): delete_line removes only the 05 branch, sibling consumers keep their lines; orphaned sources Clock t/1, 14_Battery_Converter/2 (SOC), 03_Propulsion_Plant/4 (accel) feed root Outport 1/3/6 directly';
audit.solver = solverCfg;
audit.solver_assert_passed = st.solver_ok;
audit.signal_logging = struct('before', logBefore, 'after', logAfter, ...
    'note', 'SignalLogging off; recording delegated to closed-loop harness (design §3.1); logging objects inside 05 removed with the subsystem');
audit.base_injection = {'config/Model_Parameters.m', 'config/Model_Scenarios.m'};
audit.compile_check = struct('passed', st.compile_ok, 'stop_time', '0.01', ...
    'method', 'sim with base config injection', 'message', st.compile_msg);
audit.stage_reached = st.stage;
audit.error_id = st.error_id;
audit.error_message = st.error_msg;
audit.wiring_violations = wiringBad;
audit.overall = st.inventory_ok && st.wiring_ok && st.solver_ok && st.logging_off && ...
    st.compile_ok && st.main_unchanged && strcmp(st.stage, 'done');

try
    try
        js = jsonencode(audit, 'PrettyPrint', true);
    catch
        js = jsonencode(audit);   % 旧版本无 PrettyPrint 时退化为紧凑输出
    end
    fid = fopen(auditPath, 'w', 'n', 'UTF-8');
    fwrite(fid, js, 'char');
    fclose(fid);
catch errJ
    error('GATE:PLANTSTEP_AUDIT', '审计 JSON 写出失败: %s', errJ.message);
end

if ~audit.overall
    % 失败已落账; 重抛原始 GATE 错误（保留原始错误标识）
    if isempty(err0)
        error('GATE:PLANTSTEP_AUDIT', 'overall=false 且无原始错误记录');
    end
    rethrow(err0);
end

fprintf('[20] PlantStep 构建完成: %s (overall=%d, 子系统 %d 个, Outport 11 项)\n', ...
    psPath, audit.overall, numel(expectedRemain));

fprintf('[20] T-601 PlantStep 构建完成: %s | overall=%s\n', psPath, string(audit.overall));
%% ========================================================================
%  20E_build_plantstepE_model.m — S4-G2（spec: S4_FULLCTRL_CONSTRUCTION_SPEC.md）
%  用途: 装载 HybridShip_ModelE -> SaveAs 生成被控对象子集变体
%        HybridShip_ModelE_PlantStepE.slx（plant-only）：
%          - 删除 05_EMS_Controller 与 12_Data_Logging；
%          - 根 Inport P_gen_ref_kW 原位替代 05 输出（07_PMS_v2/16_Safety_Interlock
%            的 P_gen_ref 消费端口同源续接）；
%          - 根 Outport 16 项暴露闭环接口（前 7 项 = 05 的 7 输入同源同端口，
%            承接 Model D CMD_TICK 载荷契约；8-16 项为 fuel/P_bat/P_prop/
%            voyage_stop/state_DG1/state_DG2/P_DG1/P_DG2/V_dc_V）；
%          - 11_Voyage_Stop_Logic 内部 reach/1 分支出 voyage_stop Outport
%            （原仅驱动 Stop 块，分支不改语义）。
%  ★ 规格修正 SA-1（主 Agent 2026-09-28，接线事实驱动）：
%    S4 spec §2-G2 原文"删除 05/07/16/12，根 Inport=P_gen_ref"按字面执行不可行——
%    根级接线实测 P_gen_ref 仅被 07_PMS_v2 与 16_Safety_Interlock 消费，植物物理
%    （06_Genset_System）消费的是 07 的 5 个输出（DG1/2_enable、P_DG1/2_ref_kW、
%    DG_cmd_stop）。故本阶段（C1）保留 07/16 于植物子集内（根 Inport=P_gen_ref 与
%    原文一致）；"07/16 删除 + 根面=5×DG 指令"变体留待 C3（板载 PMS/Safety 上线、
%    S4-G1 host 位级对拍之后）。07/16 的 Simulink 行为同时作为 S4-G1 C 移植的
%    位级对拍参考（S1.5 同法）。
%  红线: 主模型 HybridShip_ModelE.slx 零改动（内容哈希前后复核，不等则
%        GATE:PLANTSTEPE_MAIN_TOUCHED）；不修改 +model/、config/、既有编号脚本；
%        本脚本仅生成 PlantStepE 模型与审计 JSON。
%  产物: model/HybridShip_ModelE_PlantStepE.slx
%        results/s4/plantstepE/plantstepE_build_audit.json
%  运行: eval(fileread('scripts/20E_build_plantstepE_model.m'))
% ========================================================================

% ---- §20.2 路径纪律: 本脚本位于模型工作区根下 2 层（scripts/） ----
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % 兼容 eval 场景
end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

% 控制参数防御性加载（FR-451，同 run_scenario_sim；编译期 07/16 Parameter 作用域块需要）
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
assignin('base', 'EMS_WORK_MODE', 2);
assignin('base', 'EMS_CTRL_LAW', 6);
assignin('base', 'EMS_MODE', 1);
assignin('base', 'PMS_SOURCE_MODE', 1);

MDL_MAIN = 'HybridShip_ModelE';
MDL_PS   = 'HybridShip_ModelE_PlantStepE';
mainPath  = fullfile(ROOT, 'model', [MDL_MAIN '.slx']);
psPath    = fullfile(ROOT, 'model', [MDL_PS '.slx']);
auditDir  = fullfile(ROOT, 'results', 's4', 'plantstepE');
auditPath = fullfile(auditDir, 'plantstepE_build_audit.json');
if ~exist(auditDir, 'dir'), mkdir(auditDir); end

st = struct('stage', 'init', 'inventory_ok', false, 'wiring_ok', false, ...
    'compile_ok', false, 'compile_msg', '', 'main_unchanged', false, ...
    'main_sha_before', '', 'main_sha_after', '', 'prior_ps_existed', false, ...
    'error_id', '', 'error_msg', '');

% ---- 主模型内容哈希（before） ----
[~, hOut] = system(['shasum -a 256 ''' mainPath '''']);
tok = regexp(hOut, '([0-9a-f]{64})', 'tokens', 'once');
if isempty(tok)
    error('GATE:PLANTSTEPE:HASH_BEFORE — 主模型哈希失败');
end
st.main_sha_before = tok{1};

% ---- 期望清单（源码事实: 根级 16 子系统，本日只读查询实测） ----
expectedSubs = reshape(sort({'01_Operator_Interface', '02_Propulsion_Controller', ...
    '03_Propulsion_Plant', '04_Load_Measurement', '05_EMS_Controller', ...
    '06_Genset_System', '07_PMS_v2', '08_Undervolt_Derate', ...
    '09_Bus_Residual_Monitor', '10_Fuel_Consumption', '11_Voyage_Stop_Logic', ...
    '12_Data_Logging', '13_DC_Bus', '14_Battery_Converter', ...
    '15_Bus_Voltage_Monitor', '16_Safety_Interlock'}), 1, []);
removedSubs = {'05_EMS_Controller', '12_Data_Logging'};
% 注意: 与 S4 spec §2-G2 原文的差异见头注 SA-1（07/16 保留）
expectedRemain = reshape(sort(setdiff(expectedSubs, removedSubs)), 1, []);  % 应剩 14 个

% 前 7 项 Outport = 05 的 7 输入同源同端口（顺序 = Model D CMD_TICK 载荷契约）
emsInNames = {'P_load_total_kW', 'SOC', 'acceleration_mps2', 't', ...
    'v_cmd_mps', 'v_ship_mps', 'x_ship_m'};
outNames = {'t_s', 'P_load_total_kW', 'SOC', 'v_cmd_mps', 'v_ship_mps', ...
    'acceleration_mps2', 'x_ship_m', ...                      % 1..7（≡ CMD_TICK）
    'fuel_total_kg', 'P_bat_kW', 'P_prop_kW', 'voyage_stop', ... % 8..11（Model D 契约）
    'state_DG1', 'state_DG2', 'P_DG1_kW', 'P_DG2_kW', 'V_dc_V'}; % 12..16（S4 扩展）
% Outport 8-11 的源（子系统内部 Outport 块名；端口号动态解析）
extSrc = { ...
    'fuel_total_kg', '10_Fuel_Consumption',  'fuel_total_kg'; ...
    'P_bat_kW',      '14_Battery_Converter', 'P_bat_bus_kW'; ...
    'P_prop_kW',     '03_Propulsion_Plant',  'P_prop_actual_kW'};
% Outport 12-16 同法
extSrc2 = { ...
    'state_DG1', '06_Genset_System', 'state_DG1'; ...
    'state_DG2', '06_Genset_System', 'state_DG2'; ...
    'P_DG1_kW',  '06_Genset_System', 'P_DG1_kW'; ...
    'P_DG2_kW',  '06_Genset_System', 'P_DG2_kW'; ...
    'V_dc_V',    '13_DC_Bus',        'V_dc_V'};

try
    % ---- 装载主模型 ----
    if ~bdIsLoaded(MDL_MAIN)
        load_system(mainPath);
    end

    % ---- 结构盘点 ----
    subs = find_system(MDL_MAIN, 'SearchDepth', 1, 'BlockType', 'SubSystem');
    subsName = sort(strrep(subs, [MDL_MAIN '/'], ''));
    foundOK = isequal(numel(subsName), numel(expectedSubs)) && ...
              all(ismember(expectedSubs, subsName));
    st.inventory_ok = foundOK;
    if ~foundOK
        error('GATE:PLANTSTEPE:INVENTORY — 根级子系统清单不符（实测 %d 个）', numel(subsName));
    end

    % ---- 05 输入源端口记录（删除前；同源同端口原则） ----
    b5 = [MDL_MAIN '/05_EMS_Controller'];
    in5 = find_system(b5, 'SearchDepth', 1, 'BlockType', 'Inport');
    in5Name = cell(1, numel(in5));
    for i = 1:numel(in5), in5Name{i} = get_param(in5{i}, 'Name'); end
    if ~isequal(sort(in5Name), sort(emsInNames))
        error('GATE:PLANTSTEPE:EMS_IN — 05 输入清单不符: %s', strjoin(in5Name, ','));
    end
    % ---- 05 接线画像（主模型上记录"拓扑文本"；句柄在副本内重解析——
    %      SaveAs 后旧句柄全部失效，禁止跨副本携带） ----
    % 输入侧：以 05 子系统端口序号（=内部 Inport 块 Port 值）为键记录源口文本；
    % 输出侧：以"消费者子系统 + 其 Inport 序号"为键记录。
    srcDesc = cell(1, 7);
    srcPortNo = zeros(1, 7);
    ph5 = get_param(b5, 'PortHandles');
    for i = 1:7
        portNo = str2double(get_param([b5 '/' emsInNames{i}], 'Port'));
        srcPortNo(i) = portNo;
        lh = get_param(ph5.Inport(portNo), 'Line');
        if lh == -1
            error('GATE:PLANTSTEPE:EMS_IN_UNCONNECTED — 05/%s 根级未连线', emsInNames{i});
        end
        sp = get_param(lh, 'SrcPortHandle');
        srcDesc{i} = sprintf('%s/%s', strrep(get_param(sp,'Parent'), [MDL_MAIN '/'], ''), ...
                             get_param(sp, 'Name'));
    end
    lhOut = get_param(ph5.Outport(1), 'Line');
    if lhOut == -1
        error('GATE:PLANTSTEPE:EMS_OUT — 05 输出未连线');
    end
    dstsMain = get_param(lhOut, 'DstPortHandle');
    dstSub = cell(1, numel(dstsMain));
    dstIdx = zeros(1, numel(dstsMain));
    dstDesc = cell(1, numel(dstsMain));
    for j = 1:numel(dstsMain)
        dstBlk = get_param(dstsMain(j), 'Parent');
        dstSub{j} = strrep(dstBlk, [MDL_MAIN '/'], '');
        phD = get_param(dstBlk, 'PortHandles');
        idx = find(phD.Inport == dstsMain(j), 1);
        if isempty(idx)
            error('GATE:PLANTSTEPE:DST_PORT — 消费者 %s 输入口未定位', dstSub{j});
        end
        dstIdx(j) = idx;
        dstDesc{j} = sprintf('%s/in%d', dstSub{j}, idx);
    end
    fprintf('[20E] 05/P_gen_ref 消费者: %s\n', strjoin(dstDesc, ' | '));

    % ---- SaveAs 生成 PlantStepE（先删旧产物） ----
    st.prior_ps_existed = exist(psPath, 'file') ~= 0;
    if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end
    if st.prior_ps_existed, delete(psPath); end
    save_system(MDL_MAIN, psPath);   % 主模型不落盘（仅另存副本）
    ps = MDL_PS;

    % ---- 副本内句柄重解析（SaveAs 后主模型句柄失效） ----
    ps5 = [ps '/05_EMS_Controller'];
    ph5ps = get_param(ps5, 'PortHandles');
    srcHandles = cell(1, 7);
    for i = 1:7
        lh = get_param(ph5ps.Inport(srcPortNo(i)), 'Line');
        srcHandles{i} = get_param(lh, 'SrcPortHandle');
    end
    dsts = zeros(1, numel(dstSub));
    for j = 1:numel(dstSub)
        phD = get_param([ps '/' dstSub{j}], 'PortHandles');
        dsts(j) = phD.Inport(dstIdx(j));
    end

    % ---- 原位变换 ①: 删除 05 / 12 ----
    delete_block(ps5);
    % 12_Data_Logging：根级 T_* Terminator 不经 12（本日接线实测），直删安全
    delete_block([ps '/12_Data_Logging']);

    % ---- 原位变换 ②: 根 Inport P_gen_ref_kW 续接 05 原消费者 ----
    % 先清 05 删除后残留的悬空线（src=-1 的线仍占据目的端口）
    for j = 1:numel(dsts)
        lhD = get_param(dsts(j), 'Line');
        if lhD ~= -1
            delete_line(lhD);
        end
    end
    add_block('simulink/Sources/In1', [ps '/P_gen_ref_kW'], 'Position', [50 40 90 68]);
    iph = get_param([ps '/P_gen_ref_kW'], 'PortHandles');
    for j = 1:numel(dsts)
        add_line(ps, iph.Outport(1), dsts(j), 'autorouting', 'on');
    end

    % ---- 原位变换 ③: Outport 1..7（05 输入同源同端口；按【名字】对应——
    %      05 端口序（P_load,SOC,accel,t,v_cmd,v_ship,x）与 Model D CMD_TICK
    %      载荷序（t_s,P_load,SOC,v_cmd,v_ship,accel,x）不同，按名解析禁按位） ----
    for i = 1:7
        nm = outNames{i};
        % outNames[i] 的语义名 → 05 对应输入名（七名一一对应，仅 't_s'↔'t' 专名）
        switch nm
            case 't_s',             srcKey = 't';
            case 'P_load_total_kW', srcKey = 'P_load_total_kW';
            case 'SOC',             srcKey = 'SOC';
            case 'v_cmd_mps',       srcKey = 'v_cmd_mps';
            case 'v_ship_mps',      srcKey = 'v_ship_mps';
            case 'acceleration_mps2', srcKey = 'acceleration_mps2';
            case 'x_ship_m',        srcKey = 'x_ship_m';
            otherwise, error('GATE:PLANTSTEPE:NAME — 未登记的 outport 名 %s', nm);
        end
        j = find(strcmp(emsInNames, srcKey), 1);
        if isempty(j)
            error('GATE:PLANTSTEPE:NAME — 05 无输入 %s', srcKey);
        end
        add_block('simulink/Sinks/Out1', [ps '/' nm], 'Position', [900 40+50*i 940 68+50*i]);
        oph = get_param([ps '/' nm], 'PortHandles');
        add_line(ps, srcHandles{j}, oph.Inport(1), 'autorouting', 'on');
    end

    % ---- 原位变换 ④: Outport 8-11 / 12-16（子系统内部 Outport 块名动态解析） ----
    allExt = [extSrc; extSrc2];
    for r = 1:size(allExt, 1)
        nm = allExt{r,1}; sub = allExt{r,2}; inner = allExt{r,3};
        ib = find_system([ps '/' sub], 'SearchDepth', 1, 'BlockType', 'Outport');
        ibn = strrep(ib, [ps '/' sub '/'], '');
        idx = find(strcmp(ibn, inner), 1);
        if isempty(idx)
            error('GATE:PLANTSTEPE:EXT_SRC — %s 内未找到 Outport %s', sub, inner);
        end
        % 先验证该端口根级确有连线（有分支依据）；无也允许直连
        subPH = get_param([ps '/' sub], 'PortHandles');
        srcPH = subPH.Outport(idx);
        add_block('simulink/Sinks/Out1', [ps '/' nm], 'Position', [900 420+50*r 940 448+50*r]);
        oph = get_param([ps '/' nm], 'PortHandles');
        add_line(ps, srcPH, oph.Inport(1), 'autorouting', 'on');
    end

    % ---- 原位变换 ⑤: voyage_stop（11 内 reach/1 分支） ----
    b11 = [ps '/11_Voyage_Stop_Logic'];
    rch = [b11 '/reach'];
    rph = get_param(rch, 'PortHandles');
    if numel(rph.Outport) ~= 1
        error('GATE:PLANTSTEPE:REACH — 11/reach 输出口数异常');
    end
    add_block('simulink/Sinks/Out1', [b11 '/voyage_stop'], 'Position', [400 40 440 68]);
    rvh = get_param([b11 '/voyage_stop'], 'PortHandles');
    add_line(b11, rph.Outport(1), rvh.Inport(1), 'autorouting', 'on');
    % 删除 Stop Simulation 块（ERT 不支持；停车语义由闭环 harness 经 voyage_stop
    % 输出承担——Model D G-P2 先例同口径，仅 PlantStepE 副本内，主模型零改动）
    delete_block([b11 '/stop']);
    add_block('simulink/Sinks/Out1', [ps '/voyage_stop'], 'Position', [900 700 940 728]);
    vph = get_param([ps '/voyage_stop'], 'PortHandles');
    p11 = get_param(b11, 'PortHandles');
    add_line(ps, p11.Outport(end), vph.Inport(1), 'autorouting', 'on');

    st.wiring_ok = true;

    % ---- 根面盘点 ----
    rootIn = find_system(ps, 'SearchDepth', 1, 'BlockType', 'Inport');
    rootOut = find_system(ps, 'SearchDepth', 1, 'BlockType', 'Outport');
    fprintf('[20E] PlantStepE 根面: In=%d（P_gen_ref_kW）, Out=%d\n', numel(rootIn), numel(rootOut));
    if numel(rootIn) ~= 1 || numel(rootOut) ~= numel(outNames)
        error('GATE:PLANTSTEPE:ROOTFACE — 根面数量不符 In=%d Out=%d', numel(rootIn), numel(rootOut));
    end

    % ---- 编译检查（update diagram；07/16 Parameter 需已注入） ----
    try
        set_param(ps, 'SimulationCommand', 'update');
        st.compile_ok = true;
        st.compile_msg = 'update OK';
    catch ce
        st.compile_ok = false;
        st.compile_msg = ce.message;
        error('GATE:PLANTSTEPE:COMPILE — %s', ce.message);
    end
    save_system(ps);

    % ---- 主模型哈希（after；零改动红线） ----
    [~, h2] = system(['shasum -a 256 ''' mainPath '''']);
    tok2 = regexp(h2, '([0-9a-f]{64})', 'tokens', 'once');
    if isempty(tok2)
        error('GATE:PLANTSTEPE:HASH_AFTER — 主模型哈希失败');
    end
    st.main_sha_after = tok2{1};
    st.main_unchanged = strcmp(st.main_sha_before, st.main_sha_after);
    if ~st.main_unchanged
        error('GATE:PLANTSTEPE_MAIN_TOUCHED — 主模型内容哈希变化');
    end

    st.stage = 'done';
    fprintf('[20E] DONE: %s\n', psPath);
catch e
    st.error_id = e.identifier; st.error_msg = e.message;
    if exist(psPath, 'file') && strcmp(st.stage, 'init')
        % 半成品不留档（fail 路径清理自身产物）
    end
    jsonDump = jsonencode(st, 'PrettyPrint', true);
    fid = fopen(auditPath, 'w'); fwrite(fid, jsonDump); fclose(fid);
    rethrow(e);
end

% ---- 审计 JSON ----
audit = struct();
audit.schema = 's4_plantstepE_build_audit_v1';
audit.created_utc = char(datetime('now','TimeZone','UTC','Format','yyyy-MM-dd''T''HH:mm:ss''Z'''));
audit.main_model = MDL_MAIN;
audit.plantstep_model = MDL_PS;
audit.main_sha256_before = st.main_sha_before;
audit.main_sha256_after = st.main_sha_after;
audit.main_unchanged = st.main_unchanged;
audit.prior_ps_existed = st.prior_ps_existed;
audit.removed_subsystems = {removedSubs};
audit.retained_pms_safety = true;
audit.spec_amendment_SA1 = '07_PMS_v2/16_Safety_Interlock retained in plant subset: P_gen_ref has no direct plant consumer; plant physics consumes 07 outputs (DG enables/refs). 07/16-deletion variant deferred to C3 after S4-G1 board port. See header SA-1.';
audit.ems_input_sources = srcDesc;
audit.pgen_ref_consumers = dstDesc;
audit.root_inports = {'P_gen_ref_kW'};
audit.root_outports = {outNames};
audit.compile_ok = st.compile_ok;
audit.compile_msg = st.compile_msg;
audit.solver = 'ode3 @ 0.01 s (inherited, unchanged)';
audit.stop_block_removed = '11_Voyage_Stop_Logic/stop removed in PS copy only; ERT unsupported; harness stops via voyage_stop outport (Model D G-P2 precedent)';
jsonDump = jsonencode(audit, 'PrettyPrint', true);
fid = fopen(auditPath, 'w'); fwrite(fid, jsonDump); fclose(fid);
fprintf('[20E] audit -> %s\n', auditPath);

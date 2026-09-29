%% ========================================================================
%  20S_build_plantstepE_stream.m — S4-P3 B2 步骤②（S4_P3_STREAM_PLANT_SPEC.md §1/§2）
%  用途: 装载 HybridShip_ModelE_PlantStepE.slx（20E 产物）→ SaveAs 流驱动变体
%        HybridShip_ModelE_PlantStepE_Stream.slx（Path S″，2026-09-29 冻结件裁决）。
%  接线实测依据（20S_stream_wiring_audit.m，2026-09-29）:
%    - 04_Load_Measurement = 双输入 Sum'++'（in1←03/out1 P_prop_actual，in2←01/out4
%      P_hotel）→ 注入点选 04 双输入（回放后 P_load_total ≡ P_prop_realized+P_hotel）；
%    - 03↔02 双向反馈环 + 08 降额→03 回授确认 → 03/02/08 分支整体保持原样
%      （装饰分支，能量域保护 07/16 不受影响）；回放语义下需求侧为处方量，
%      欠压降额不再作用于处方负载 —— Path S″ 固有语义，登记 REGISTERED_DEVIATION；
%    - 能量域（06/07/09/10/13/14/15/16）不直接消费 v_ship/n_prop/accel（实测）；
%    - P_hotel 的 07/in2 与 G1_hotel 消费者一并改接回放列（母线口径一致性）。
%  手术清单（SaveAs 副本内，全部脚本化）:
%    R1 04/in1:  03/out1              -> 根 Inport P_prop_realized_kW
%    R2 04/in2:  01/out4              -> 根 Inport P_hotel_kW
%    R3 07/in2:  01/out4              -> 根 Inport P_hotel_kW
%    R4 G1_hotel/in1: 01/out4         -> 根 Inport P_hotel_kW
%    R5 Outport P_prop_kW/in1: 03/out1 -> 根 Inport P_prop_realized_kW（诚实遥测）
%    R6 Outport v_ship_mps/in1: 03/out2 -> 根 Inport v_ship_mps_replay（命名偏差：
%       Simulink 同层 In/Out 禁同名，规格名 v_ship_mps 让位给既有 Outport 契约）
%    R7 Outport v_cmd_mps/in1: 01/out1 -> 根 Inport v_ship_mps_replay（规格 §1.2：
%       v_cmd_mps 输出回放值）
%    R8 新增 Discrete-Time Integrator（Ts=0.01 ForwardEuler，IC=0）:
%       v_ship_mps_replay -> x_ship_replay
%    R9 Outport x_ship_m/in1:  03/out3 -> x_ship_replay 积分器输出
%    R10 11_Voyage_Stop_Logic/in1: 03/out3 -> x_ship_replay（11 内部 reach 语义保持）
%  根面契约: Inport 4 项顺序冻结 P_gen_ref_kW(1)/P_prop_realized_kW(2)/
%            P_hotel_kW(3)/v_ship_mps_replay(4)；根 Outport 名称/顺序零改动。
%  红线: 主模型 HybridShip_ModelE.slx 与 HybridShip_ModelE_PlantStepE.slx 内容哈希
%        前后复核（不等则 GATE:P3STREAM_*）；不修改 +model/、config/、既有编号脚本。
%  产物: model/HybridShip_ModelE_PlantStepE_Stream.slx
%        results/s4/plantstepE/stream_build_audit.json
%  运行: cd <模型工作区根>; eval(fileread('scripts/20S_build_plantstepE_stream.m'))
%        （eval 兼容：本脚本不含局部函数）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
assignin('base', 'EMS_WORK_MODE', 2);
assignin('base', 'EMS_CTRL_LAW', 6);
assignin('base', 'EMS_MODE', 1);
assignin('base', 'PMS_SOURCE_MODE', 1);

MDL_PS  = 'HybridShip_ModelE_PlantStepE';
MDL_ST  = 'HybridShip_ModelE_PlantStepE_Stream';
psPath    = fullfile(ROOT, 'model', [MDL_PS '.slx']);
mainPath  = fullfile(ROOT, 'model', 'HybridShip_ModelE.slx');
stPath    = fullfile(ROOT, 'model', [MDL_ST '.slx']);
auditDir  = fullfile(ROOT, 'results', 's4', 'plantstepE');
auditPath = fullfile(auditDir, 'stream_build_audit.json');
if ~exist(auditDir, 'dir'), mkdir(auditDir); end

st = struct('stage', 'init', 'rewire_ok', false, 'compile_ok', false, ...
    'compile_msg', '', 'ps_unchanged', false, 'main_unchanged', false, ...
    'ps_sha_before', '', 'ps_sha_after', '', 'main_sha_before', '', ...
    'main_sha_after', '', 'stream_sha', '', 'error_id', '', 'error_msg', '');
getSha = @(p) regexp([system('placeholder')], '', 'tokens', 'once'); %#ok<NASGU>

% ---- 双哈希（before） ----
[~, o1] = system(['shasum -a 256 ''' psPath '''']);
tk = regexp(o1, '([0-9a-f]{64})', 'tokens', 'once');
if isempty(tk), error('GATE:P3STREAM:HASH — PS 哈希失败'); end
st.ps_sha_before = tk{1};
[~, o2] = system(['shasum -a 256 ''' mainPath '''']);
tk = regexp(o2, '([0-9a-f]{64})', 'tokens', 'once');
if isempty(tk), error('GATE:P3STREAM:HASH — 主模型哈希失败'); end
st.main_sha_before = tk{1};

try
    % ---- 装载 PlantStepE（不落盘）→ SaveAs 流变体 ----
    if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end
    if bdIsLoaded(MDL_ST), close_system(MDL_ST, 0); end
    if exist(stPath, 'file'), delete(stPath); end
    load_system(psPath);
    save_system(MDL_PS, stPath);          % PS 本体不落盘（20E 同法）
    m = MDL_ST;                           % SaveAs 后加载名 = 新文件名

    % ---- R1/R2 前置断言: 04 双输入现源与审计一致 ----
    b04 = [m '/04_Load_Measurement'];
    ph04 = get_param(b04, 'PortHandles');
    wantSrc = {'03_Propulsion_Plant', '01_Operator_Interface'};
    for p = 1:2
        lh = get_param(ph04.Inport(p), 'Line');
        if lh == -1, error('GATE:P3STREAM:WIRE — 04/in%d 未连线', p); end
        sp = get_param(lh, 'SrcPortHandle');
        if ~strcmp(get_param(sp, 'Parent'), [m '/' wantSrc{p}])
            error('GATE:P3STREAM:WIRE — 04/in%d 现源 %s 与审计不符（期望 %s）', ...
                p, get_param(sp, 'Parent'), wantSrc{p});
        end
    end
    % ---- 根 Inport ×3（端口顺序冻结: 1=P_gen_ref_kW 既有） ----
    add_block('simulink/Sources/In1', [m '/P_prop_realized_kW'], ...
        'Position', [50 120 90 148]);
    add_block('simulink/Sources/In1', [m '/P_hotel_kW'], ...
        'Position', [50 200 90 228]);
    add_block('simulink/Sources/In1', [m '/v_ship_mps_replay'], ...
        'Position', [50 280 90 308]);
    set_param([m '/P_gen_ref_kW'], 'Port', '1');
    set_param([m '/P_prop_realized_kW'], 'Port', '2');
    set_param([m '/P_hotel_kW'], 'Port', '3');
    set_param([m '/v_ship_mps_replay'], 'Port', '4');
    phPR = get_param([m '/P_prop_realized_kW'], 'PortHandles');
    phHT = get_param([m '/P_hotel_kW'], 'PortHandles');
    phVS = get_param([m '/v_ship_mps_replay'], 'PortHandles');

    % ---- R8: 离散积分器（v_ship_replay -> x_ship_replay；先建后用） ----
    add_block('simulink/Discrete/Discrete-Time Integrator', ...
        [m '/x_ship_replay_int'], ...
        'IntegratorMethod', 'ForwardEuler', 'SampleTime', '0.01', ...
        'InitialCondition', '0', 'Position', [200 280 260 320]);
    phIX = get_param([m '/x_ship_replay_int'], 'PortHandles');
    add_line(m, phVS.Outport(1), phIX.Inport(1), 'autorouting', 'on');

    % ---- R1..R10 改接（表驱动；dst 子系统路径相对 m，in 端口号，新源句柄） ----
    rwDst = {b04, b04, [m '/07_PMS_v2'], [m '/G1_hotel'], [m '/P_prop_kW'], ...
             [m '/v_ship_mps'], [m '/v_cmd_mps'], [m '/x_ship_m'], ...
             [m '/11_Voyage_Stop_Logic']};
    rwIn  = [1, 2, 2, 1, 1, 1, 1, 1, 1];
    rwSrc = {phPR.Outport(1), phHT.Outport(1), phHT.Outport(1), ...
             phHT.Outport(1), phPR.Outport(1), phVS.Outport(1), ...
             phVS.Outport(1), phIX.Outport(1), phIX.Outport(1)};
    rwTag = {'R1', 'R2', 'R3', 'R4', 'R5', 'R6', 'R7', 'R9', 'R10'};
    for r = 1:numel(rwDst)
        phD = get_param(rwDst{r}, 'PortHandles');
        lh = get_param(phD.Inport(rwIn(r)), 'Line');
        if lh == -1
            error('GATE:P3STREAM:WIRE — %s/in%d 未连线（与审计不符，%s）', ...
                rwDst{r}, rwIn(r), rwTag{r});
        end
        delete_line(lh);
        add_line(m, rwSrc{r}, phD.Inport(rwIn(r)), 'autorouting', 'on');
        fprintf('[20S] %s: %s/in%d 改接 OK\n', rwTag{r}, ...
            strrep(rwDst{r}, [m '/'], ''), rwIn(r));
    end
    st.rewire_ok = true;

    % ---- 根面盘点 ----
    rootIn = find_system(m, 'SearchDepth', 1, 'BlockType', 'Inport');
    rootInName = cell(1, numel(rootIn));
    rootInPort = zeros(1, numel(rootIn));
    for i = 1:numel(rootIn)
        rootInName{i} = strrep(rootIn{i}, [m '/'], '');
        rootInPort(i) = str2double(get_param(rootIn{i}, 'Port'));
    end
    wantIn = {'P_gen_ref_kW', 'P_prop_realized_kW', 'P_hotel_kW', ...
        'v_ship_mps_replay'};
    [~, ord] = sort(rootInPort);
    fprintf('[20S] 根 Inport（按端口序）: %s\n', strjoin(rootInName(ord), ', '));
    if ~isequal(rootInName(ord), wantIn)
        error('GATE:P3STREAM:ROOTIN — 根 Inport 面不符: %s', ...
            strjoin(rootInName(ord), ','));
    end

    % ---- 编译检查 ----
    try
        set_param(m, 'SimulationCommand', 'update');
        st.compile_ok = true;
        st.compile_msg = 'update OK';
    catch ce
        st.compile_msg = ce.message;
        error('GATE:P3STREAM:COMPILE — %s', ce.message);
    end
    save_system(m);

    % ---- 双哈希（after; 零改动红线） ----
    [~, o1b] = system(['shasum -a 256 ''' psPath '''']);
    tk = regexp(o1b, '([0-9a-f]{64})', 'tokens', 'once');
    st.ps_sha_after = tk{1};
    [~, o2b] = system(['shasum -a 256 ''' mainPath '''']);
    tk = regexp(o2b, '([0-9a-f]{64})', 'tokens', 'once');
    st.main_sha_after = tk{1};
    st.ps_unchanged = strcmp(st.ps_sha_before, st.ps_sha_after);
    st.main_unchanged = strcmp(st.main_sha_before, st.main_sha_after);
    if ~st.ps_unchanged
        error('GATE:P3STREAM_PSTEPS_TOUCHED — PlantStepE.slx 哈希变化');
    end
    if ~st.main_unchanged
        error('GATE:P3STREAM_MAIN_TOUCHED — 主模型哈希变化');
    end
    [~, o3] = system(['shasum -a 256 ''' stPath '''']);
    tk = regexp(o3, '([0-9a-f]{64})', 'tokens', 'once');
    st.stream_sha = tk{1};

    st.stage = 'done';
    fprintf('[20S] DONE: %s\n', stPath);
catch e
    st.error_id = e.identifier;
    st.error_msg = e.message;
    fid = fopen(auditPath, 'w');
    fwrite(fid, jsonencode(st, 'PrettyPrint', true));
    fclose(fid);
    rethrow(e);
end

% ---- 审计 JSON（jsonencode 安全：一律 1-D/标量字段） ----
audit = struct();
audit.schema = 's4_p3_stream_build_audit_v1';
audit.created_utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'''));
audit.source_model = MDL_PS;
audit.stream_model = MDL_ST;
audit.main_model = 'HybridShip_ModelE';
audit.main_sha256_before = st.main_sha_before;
audit.main_sha256_after = st.main_sha_after;
audit.main_unchanged = st.main_unchanged;
audit.plantstepe_sha256_before = st.ps_sha_before;
audit.plantstepe_sha256_after = st.ps_sha_after;
audit.plantstepe_unchanged = st.ps_unchanged;
audit.stream_model_sha256 = st.stream_sha;
audit.root_inports_portorder = rootInName(ord);
audit.rewire_tags = rwTag;
audit.rewire_targets = cellfun(@(s) strrep(s, [m '/'], ''), rwDst, ...
    'UniformOutput', false);
audit.rewire_inports = num2cell(rwIn);
audit.retained_decorative_branch = ['02-03 demand/feedback loop + 08 derate->03 ' ...
    'retained intact (03 outputs no longer feed 04 / root outports except ' ...
    'acceleration_mps2 and T_log taps); replay prescribes the load — undervolt ' ...
    'derating cannot act on a prescribed load (Path S" inherent, ' ...
    'REGISTERED_DEVIATION)'];
audit.naming_deviation = ['root Inport named v_ship_mps_replay (spec name ' ...
    'v_ship_mps collides with the existing Outport contract name; Simulink ' ...
    'forbids same-level duplicate names)'];
audit.accel_outport_note = ['Outport acceleration_mps2 kept sourced from ' ...
    '03/out4 (decorative; not consumed by gates / downlink 8-state)'];
audit.outport_contract = ['16 Outport names/order unchanged (t_s..voyage_stop); ' ...
    'G1_* extra outports untouched'];
audit.compile_ok = st.compile_ok;
audit.compile_msg = st.compile_msg;
audit.solver = 'ode3 @ 0.01 s (inherited, unchanged)';
fid = fopen(auditPath, 'w');
fwrite(fid, jsonencode(audit, 'PrettyPrint', true));
fclose(fid);
fprintf('[20S] audit -> %s\n', auditPath);

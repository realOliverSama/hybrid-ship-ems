%% ========================================================================
%  22_run_gp1_gate.m — T-602（spec: ems01-chil-v3-closedloop, FR-604 / AC-601）
%  用途: G-P1 被控对象子集保真门禁（design §5 G-P1；阈值 design §5.2 先验设定，
%        FAIL 排查不放宽）。以 ref_G2 参考轨迹的 P_gen_ref_kW 逐拍序列构造
%        0.01 s ZOH 回放网格，经 SimulationInput.setExternalInput 驱动被控对象
%        子集模型 HybridShip_ModelD_PlantStep.slx（解释执行 sim，T-601 产物，
%        结构审计 overall=true 为前置条件），逐拍（k=0..N-1，取输出网格 t=k
%        处的采样）对照 SOC / v_ship_mps / x_ship_m / P_load_total_kW，并对照
%        fuel_total_kg 终值 vs export_summary.json 的 fuel_raw_kg（fuel_corr
%        含 SOC 折算、子集不适用，design §3.6 定案）。
%  对照口径: ref CSV 第 k 行（tick=k）的状态 = 全模型 t=k 时刻状态；子集输出在
%        t=k 处采样同口径比对（k=0 行为初始状态）；ref t 列与子集 t_s 同时断言。
%  阈值（design §5.2 G-P1，先验设定）: SOC / v_ship / x_ship / P_load 逐拍
%        max|Δ| ≤ 1e-6；fuel 终值相对偏差 ≤ 1e-6；数值健康（全量无 NaN/Inf）。
%        v_cmd_mps 逐拍仅作报告信息列（design 对照集不含）；ref 的 accel_mps2
%        列是事后差分口径，不参与对照（replay CSV 的 accel 列取子集 Outport 6）。
%  场景: base 变量 GP1_SCENARIO（或同名环境变量）选择：'S20'（默认）|
%        'voyage002'。T-603 以 GP1_SCENARIO='voyage002' 注入后 eval 本脚本复用。
%  产物（EMS 工程 Host/reference/，路径相对模型工作区根推导，禁止写死）:
%    ../../firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/
%      EMS01_CHIL_ECMS/Host/reference/v3_gp1_<场景>_gate.json    机器可读门禁
%      EMS01_CHIL_ECMS/Host/reference/v3_gp1_<场景>_replay.csv   回放轨迹（%.17g）
%  冻结资产零改动零写入: ref_G2_*.csv / export_summary.json / meta_G2_*.json /
%        baseline_manifest.json 只读；PlantStep.slx 与主模型 .slx 零改动
%        （本脚本仅运行期参数级设置，不 save_system）。
%  驱动方式依据（FD3 选择，写明）:
%    - in = Simulink.SimulationInput(PS); in = in.setExternalInput(ts)：依据
%      MATLAB 文档，setExternalInput 将外部输入数据与 SimulationInput 对象关联，
%      仿真时映射到模型根 Inport（默认按端口序；PlantStep 唯一根 Inport
%      P_gen_ref_kW，端口序/名称映射无歧义）；不手动改模型 InputOption 配置
%      （SimulationInput 自管，模型零持久化改动）。
%    - 根 Inport 施加语义按 ZOH 处理（fixed-step 输入数据惯例）；装载后若 Inport
%      存在 Interpolate 参数则置 off（运行期参数级，不回写），保证与主模型 05 的
%      逐拍保持（held）P_gen 序列逐点一致；无该参数则记录并依赖 fixed-step 默认。
%    - 外部输入生效判别探针: 先以 StopTime=30 短仿真，检查 fuel_total_kg(30)。
%      06_Genset_Average_Plant→10_Fuel_Consumption 为纯前馈链（P_gen 经限幅/
%      爬坡/惯性），fuel(30) 只由输入序列决定、不受被控对象保真度影响：
%      输入生效 ≥ ~1.2 kg（20→1000 kW 序列，40 kW/s 爬坡 + 5 s 惯性），
%      输入未生效（Inport 接地 P_gen=0）→ 恰为 0。阈值 0.3 kg（双侧裕度 >4 倍）。
%      探针失败时以 in.setModelParameter('InputOption','IndividualExternalInputs')
%      显式启用输入通道重试一次（仍失败则 GATE:GP1:INPUT 终止并带双份诊断）。
%  结构性错误（文件缺失/ref 结构不符/模型装载失败/输入未生效/yout 命名不符）:
%        error('GATE:GP1:*') 立即终止（不写 gate JSON）；门禁对照 FAIL: 先写
%        JSON+CSV 再 error('GATE:GP1Failed')（03/06 惯例，机器可读结果优先）。
%  运行（本文件名数字开头，同 63/64/66 惯例只能 eval 执行；脚本不得含局部函数）:
%    cd <模型工作区根>   % 即 model/simulink_modelD_(EMS_NN_AECMS)/
%    GP1_SCENARIO='S20'; eval(fileread('scripts/22_run_gp1_gate.m'))
%  ========================================================================

% ---- §20.2 路径纪律: 本脚本位于模型工作区根下 2 层（scripts/），禁绝对路径 ----
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;  % eval(fileread) 顶层 mfilename 为空 -> 以 cwd 兜底（须位于根）
end
cd(ROOT);  % 63 教训: eval 下相对路径脚本必须位于工作区根
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'), ...
    fullfile(ROOT, 'scripts', '+ecms2'), fullfile(ROOT, 'scripts', '+ecms25'));
fprintf('\n[22] ===== T-602 G-P1 subset fidelity gate (FR-604/AC-601) =====\n');
fprintf('[22] ROOT = %s | start: %s\n', ROOT, datestr(now, 'yyyy-mm-dd HH:MM:SS'));
fprintf('[22] MATLAB %s\n', version);

%% ---- 1. 场景选择（FD1: base 变量 GP1_SCENARIO / 环境变量双通道，缺省 S20） ----
if evalin('base', 'exist(''GP1_SCENARIO'', ''var'')') == 1
    scenarioSel = evalin('base', 'GP1_SCENARIO');
elseif ~isempty(getenv('GP1_SCENARIO'))
    scenarioSel = getenv('GP1_SCENARIO');
else
    scenarioSel = 'S20';
end
if isstring(scenarioSel), scenarioSel = char(scenarioSel); end
if ~(ischar(scenarioSel) && any(strcmp(scenarioSel, {'S20', 'voyage002'})))
    error('GATE:GP1:SCENARIO', ...
        '[22] GP1_SCENARIO 非法（仅 ''S20'' | ''voyage002''）；实际类型 %s', ...
        class(scenarioSel));
end
fprintf('[22] 场景: %s\n', scenarioSel);

%% ---- 2. 预检: T-601 结构审计 overall=true + 模型/冻结资产就绪（只读） ----
MDL_PS  = 'HybridShip_ModelD_PlantStep';
psPath    = fullfile(ROOT, 'model', [MDL_PS '.slx']);
auditPath = fullfile(ROOT, 'results', 'ecms_phase25', 'plantstep', ...
    'plantstep_build_audit.json');
if ~(exist(psPath, 'file') == 2 || exist(psPath, 'file') == 4)   % 4=已在内存的模型
    error('GATE:GP1:PRE', '[22] 缺少 %s（先执行 20_build_plantstep_model.m）', ...
        fullfile('model', [MDL_PS '.slx']));
end
if ~(exist(auditPath, 'file') == 2)
    error('GATE:GP1:PRE', '[22] 缺少 %s（T-601 结构审计产物）', ...
        fullfile('results', 'ecms_phase25', 'plantstep', 'plantstep_build_audit.json'));
end
auditJ = jsondecode(fileread(auditPath));
if ~(isfield(auditJ, 'overall') && auditJ.overall)
    error('GATE:GP1:PRE', '[22] T-601 结构审计 overall~=true，禁止进入 G-P1');
end

%% ---- 3. base 注入两份 config（照 20_build_plantstep_model.m 阶段 H 惯例） ----
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
% PlantStep 不读取 EMS 变量（05 已删除），但按 T-601 编译冒烟口径注入兜底，
% 与参考运行（64/66: law=7/mode=1/AI_Pred_Source=0）环境保持一致：
% 经 SimulationInput.setVariable 注入（64:194-233 同构），不污染 base 参数名。

%% ---- 4. 场景构造（S20: SCENARIOS(20)；voyage002: fleet json id==2） ----
switch scenarioSel
    case 'S20'
        scn = SCENARIOS(20);   % Model_Scenarios.m 已注入 base（本脚本同工作区）
        if ~(isfield(scn, 'name') && strcmp(scn.name, 'S20_composite_voyage'))
            error('GATE:GP1:SCENARIO', ...
                '[22] SCENARIOS(20).name=''%s'' 非预期 S20_composite_voyage', ...
                scn.name);
        end
        stopTime = scn.stop_time_s;
        runId  = 'G2_S20_composite_voyage_matrix';
        scnTag = 'S20';
    case 'voyage002'
        vid = 2;
        fleetJson = fullfile(ROOT, 'results', 'ecms_phase25', 'fleet', ...
            'voyages_seed20260829.json');
        if ~(exist(fleetJson, 'file') == 2)
            error('GATE:GP1:SCENARIO', '[22] 缺少 fleet json: %s', ...
                fullfile('results', 'ecms_phase25', 'fleet', ...
                'voyages_seed20260829.json'));
        end
        VJ = jsondecode(fileread(fleetJson));
        vSel = [];
        for j = 1:numel(VJ.voyages)
            if VJ.voyages(j).id == vid, vSel = VJ.voyages(j); break; end
        end
        if isempty(vSel)
            error('GATE:GP1:SCENARIO', '[22] fleet json 中无 id==%d 航次', vid);
        end
        scn = ecms25.voyage_scn_v2(vSel);   % 66 同构（66:130-140）
        stopTime = scn.stop_time_s;
        runId  = sprintf('G2_test_voyage_%03d_v2', vid);   % 66 命名惯例
        scnTag = 'voyage002';
end
fprintf('[22] run_id=%s | stop_time=%.6g s | SOC_init=%.6g\n', ...
    runId, stopTime, scn.initial_SOC);

%% ---- 5. 冻结参考数据读取 + 结构断言（只读，零写入） ----
refDir  = fullfile(ROOT, 'results', 'ecms_phase25', 'fw_reference');
refFile = ['ref_' runId '.csv'];
refPath = fullfile(refDir, refFile);
metaPath = fullfile(refDir, ['meta_' runId '.json']);
sumPath  = fullfile(refDir, 'export_summary.json');
for p = {refPath, metaPath, sumPath}
    if ~(exist(p{1}, 'file') == 2)
        error('GATE:GP1:PRE', '[22] 缺少冻结参考文件: %s', p{1});
    end
end

refOpt = detectImportOptions(refPath);
refNames = refOpt.VariableNames;
EXP_REF_NAMES = {'tick', 't', 'P_load_kW', 'SOC', 'v_cmd_mps', 'v_ship_mps', ...
    'accel_mps2', 'x_ship_m', 'P_gen_ref_kW', 'e_hat', 'e_hat_raw', 'K', 'K_q', ...
    'K_w', 'cls_eff', 'cls_raw', 'q10_kwh', 'q50_kwh', 'q90_kwh', 's_q_kwh', ...
    'P_replay_kW'};
if ~isequal(refNames, EXP_REF_NAMES)
    error('GATE:GP1:REF', ...
        '[22] ref CSV 表头与预期 21 列不符；实测: %s', strjoin(refNames, ','));
end
refM = readmatrix(refPath, refOpt);
[nsRef, ncRef] = size(refM);
iTick = find(strcmp(refNames, 'tick'));
iTref = find(strcmp(refNames, 't'));
iPL   = find(strcmp(refNames, 'P_load_kW'));
iSOC  = find(strcmp(refNames, 'SOC'));
iVcmd = find(strcmp(refNames, 'v_cmd_mps'));
iVsh  = find(strcmp(refNames, 'v_ship_mps'));
iXsh  = find(strcmp(refNames, 'x_ship_m'));
iPgen = find(strcmp(refNames, 'P_gen_ref_kW'));
nRef = nsRef;
if nRef < 2 || ncRef ~= 21
    error('GATE:GP1:REF', '[22] ref CSV 尺寸 %dx%d 非预期（>=2 行 x 21 列）', ...
        nRef, ncRef);
end
refTick = refM(:, iTick);
refT    = refM(:, iTref);
if ~all(refTick == (0:nRef-1).')       % FD5: tick 列 == 0..N-1（精确整数）
    error('GATE:GP1:REF', '[22] ref tick 列非 0..%d 连续整数', nRef-1);
end
if ~all(abs(refT - (0:nRef-1).') < 1e-9)  % FD5: t 列同断言
    error('GATE:GP1:REF', '[22] ref t 列与 0..%d 不一致', nRef-1);
end
if ~all(isfinite(refM(:, [iPL iSOC iVcmd iVsh iXsh iPgen])))
    error('GATE:GP1:REF', '[22] ref 对照列存在 NaN/Inf（冻结资产异常）');
end

%% ---- 6. fuel 参考值: export_summary.json 按 run_id 确定性定位 + meta 交叉断言 ----
sumJ = jsondecode(fileread(sumPath));
if ~(isfield(sumJ, 'runs') && numel(sumJ.runs) >= 1)
    error('GATE:GP1:REF', '[22] export_summary.json 无 runs[]');
end
if iscell(sumJ.runs)   % jsondecode 对异构元素（如 S20 的 source 字段）返回 cell 数组
    runIds = cellfun(@(r) char(r.run_id), sumJ.runs, 'UniformOutput', false);
else
    runIds = {sumJ.runs.run_id};
end
selIdx = find(strcmp(runIds, runId));
if numel(selIdx) ~= 1
    error('GATE:GP1:REF', ...
        '[22] export_summary.json runs[] 按 run_id=''%s'' 定位到 %d 条（须恰 1 条）', ...
        runId, numel(selIdx));
end
if iscell(sumJ.runs)
    runJ = sumJ.runs{selIdx};
else
    runJ = sumJ.runs(selIdx);
end
if ~(isfield(runJ, 'fuel_raw_kg') && isfield(runJ, 'stop_time_s'))
    error('GATE:GP1:REF', ...
        '[22] summary run ''%s'' 缺少 fuel_raw_kg/stop_time_s 字段', runId);
end
metaJ = jsondecode(fileread(metaPath));
fuelRef = runJ.fuel_raw_kg;   % design §3.6: fuel_raw 口径（fuel_corr 含 SOC 折算不适用）
% 冻结资产一致性（meta 是逐 run 权威，summary 是汇总——不一致即资产异常）：
if ~(abs(metaJ.fuel_raw_kg - fuelRef) < 1e-9 && strcmp(metaJ.run_id, runId))
    error('GATE:GP1:REF', '[22] meta 与 summary 的 fuel_raw_kg/run_id 不一致');
end
if ~(abs(metaJ.stop_time_s - stopTime) < 1e-9 && metaJ.n_ticks == nRef)
    error('GATE:GP1:REF', ...
        '[22] meta stop_time_s/n_ticks(%g/%d) 与场景/CSV(%g/%d) 不一致', ...
        metaJ.stop_time_s, metaJ.n_ticks, stopTime, nRef);
end
if ~(abs(runJ.stop_time_s - stopTime) < 1e-9)
    error('GATE:GP1:REF', '[22] summary stop_time_s 与场景不一致');
end
if ~(abs(stopTime - (nRef - 1)) < 1e-9)
    error('GATE:GP1:REF', ...
        '[22] 场景 stop_time=%g 与 ref 行数-1=%d 不一致（回放网格无覆盖）', ...
        stopTime, nRef - 1);
end
fprintf('[22] fuel_ref(fuel_raw_kg)=%.12g | ref 行数=%d\n', fuelRef, nRef);

%% ---- 7. 回放网格构造（FD2: 0.01 s ZOH，t_grid=0:0.01:stop_time） ----
refP = refM(:, iPgen);
tGrid = 0:0.01:stopTime;                    % N*100+1 点（含端点 t=stop_time）
pGrid = [repelem(refP(1:end-1), 100); refP(end)];   % P(t)=ref 行 floor(t)+1（1-based）
nGrid = round(stopTime * 100) + 1;
assert(numel(tGrid) == nGrid, 'GATE:GP1:REF', ...
    '[22] t_grid 点数 %d ~= stop_time*100+1=%d（colon 端点异常）', ...
    numel(tGrid), nGrid);
assert(numel(pGrid) == nGrid && numel(pGrid) == (nRef-1)*100 + 1, ...
    'GATE:GP1:REF', '[22] P 网格点数与 ref 行数构造不一致');
tGrid = tGrid(:);
pGrid = pGrid(:);
tsP = timeseries(pGrid, tGrid, 'Name', 'P_gen_ref_kW');  % 单根 Inport，端口序映射无歧义
fprintf('[22] 回放网格: %d 点 @0.01 s（t=0..%.2f），P_gen min/max=%.4g/%.4g kW\n', ...
    nGrid, tGrid(end), min(pGrid), max(pGrid));

%% ---- 8. 模型装载（全新）+ 根 Inport ZOH 施加保障（运行期参数级，不回写） ----
if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end
load_system(psPath);
if ~strcmp(get_param(MDL_PS, 'Name'), MDL_PS)
    error('GATE:GP1:MODEL', '[22] 装载根名与 %s 不一致', MDL_PS);
end
ipBlk = [MDL_PS '/P_gen_ref_kW'];
dpar  = get_param(ipBlk, 'DialogParameters');
if isfield(dpar, 'Interpolate')
    set_param(ipBlk, 'Interpolate', 'off');   % ZOH（与主模型 05 held 输出一致）；不 save_system
    fprintf('[22] Inport Interpolate 参数存在 -> 运行期置 off（ZOH，不回写 .slx）\n');
else
    fprintf('[22] Inport 无 Interpolate 参数 -> 按 fixed-step 默认施加输入数据\n');
end

%% ---- 9. 仿真（探针 -> [fallback 探针] -> 全程；注入与 64:194-233 同构） ----
TH_FUEL_PROBE = 0.3;   % kg；输入生效 >=~1.2 kg，接地=0（见头部探针依据）
useIO = false;         % 当前/后续 sim 是否显式设 InputOption（探针通过后全程沿用）
plans = { struct('label', 'probe(StopTime=30)', 'stopTime', 30, 'isProbe', true), ...
          struct('label', sprintf('full(StopTime=%.2f)', stopTime), ...
                 'stopTime', stopTime, 'isProbe', false) };
dsFull = []; tvFull = []; ioNote = 'setExternalInput（SimulationInput 托管，未手动设 InputOption）';
pl = 0;
while pl < numel(plans)
    pl = pl + 1;
    plan = plans{pl};
    nG = round(plan.stopTime * 100) + 1;
    inC = Simulink.SimulationInput(MDL_PS);
    inC = inC.setVariable('SCN', scn);
    inC = inC.setVariable('EMS_CTRL_LAW', 7);
    inC = inC.setVariable('EMS_MODE', 1);
    inC = inC.setVariable('AI_Pred_Source', 0);
    inC = inC.setVariable('Bat_SOC_init', scn.initial_SOC);
    inC = inC.setVariable('VOYAGE_SPEED_MODE', 1);
    inC = inC.setVariable('VOYAGE_V_TIME_S', ecms2.scn_field(scn, 'v_time_s', [0 1]));
    inC = inC.setVariable('VOYAGE_V_VALUE_MPS', ecms2.scn_field(scn, 'v_value_mps', [0 0]));
    inC = inC.setVariable('VOYAGE_CURRENT_MODE', 1);
    inC = inC.setVariable('VOYAGE_CURRENT_TIME_S', ecms2.scn_field(scn, 'current_time_s', [0 1]));
    inC = inC.setVariable('VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(scn, 'current_value_mps', [0 0]));
    inC = inC.setModelParameter('StopTime', sprintf('%.2f', floor(plan.stopTime * 100) / 100));
    inC = inC.setModelParameter('SaveOutput', 'on');
    inC = inC.setModelParameter('SaveFormat', 'Dataset');
    inC = inC.setModelParameter('ReturnWorkspaceOutputs', 'on');
    inC = inC.setExternalInput(tsP);
    if useIO
        inC = inC.setModelParameter('InputOption', 'IndividualExternalInputs');
    end
    fprintf('[22] sim %s ...\n', plan.label);
    t0 = tic;
    outC = sim(inC);
    fprintf('[22] sim done in %.1f s\n', toc(t0));

    % ---- yout 提取（SaveFormat=Dataset；元素按根 Outport 端口序，名字取线名） ----
    yout = outC.yout;
    if ~isa(yout, 'Simulink.SimulationData.Dataset')
        error('GATE:GP1:YOUT', ...
            '[22] out.yout 非 Dataset（SaveFormat 未生效?）——实际类 %s', class(yout));
    end
    nEl = yout.numElements;
    % R2026a 实证（T-602 执行诊断）: yout 元素按根 Outport 端口序 1..11 排列，
    % 元素名 = 源信号线名（未命名线为空）而非 Outport 块名 —— 故按端口序映射，
    % 非空元素名与块名的差异（P_bat/P_prop 源线有历史线名）仅 NOTE 不判错。
    EXP_OUT = {'t_s', 'P_load_total_kW', 'SOC', 'v_cmd_mps', 'v_ship_mps', ...
        'acceleration_mps2', 'x_ship_m', 'fuel_total_kg', 'P_bat_kW', ...
        'P_prop_kW', 'voyage_stop'};
    if nEl ~= numel(EXP_OUT)
        error('GATE:GP1:YOUT', ...
            '[22] yout 元素数 %d 非预期 %d（根 Outport 端口序 1..11）', nEl, numel(EXP_OUT));
    end
    dsC = struct();
    for e = 1:nEl
        el = yout.getElement(e);
        % 元素形态归一化: yout Dataset 元素可能是 bare timeseries（无 .Values）
        % 或 SimulationData.Signal（.Values 为 timeseries）——两种都支持
        if isa(el, 'timeseries')
            vv = el;
        else
            vv = el.Values;
        end
        nm = el.Name;
        tvE = double(vv.Time(:));
        dvE = squeeze(double(vv.Data));
        if size(dvE, 2) > 1, dvE = dvE(:, 1); end
        dvE = dvE(:);
        key = EXP_OUT{e};   % 端口序映射（R2026a yout 元素名取线名，不可靠）
        if ~isempty(nm) && ~strcmp(nm, key)
            fprintf('[22] NOTE: yout el%d 信号名 [%s] 与 Outport 块名 [%s] 不同（按端口序映射）\n', ...
                e, nm, key);
        end
        dsC.(key) = dvE;
        dsC.([key '__t']) = tvE;
    end
    % 时间轴断言（全信号同轴 0.01 s 网格；不一致含提前停机/voyage_stop 情形）
    axExpect = (0:nG-1).' * 0.01;
    % 身份断言: Outport 1 (t_s) 即仿真时间轴本身（端口序映射的正确性锚）
    if max(abs(dsC.t_s - axExpect)) > 1e-9
        error('GATE:GP1:YOUT', '[22] Outport 1 (t_s) 与仿真时间轴不一致——端口序映射异常');
    end
    for s = 1:numel(EXP_OUT)
        key = EXP_OUT{s};
        if numel(dsC.([key '__t'])) ~= nG || ...
           max(abs(dsC.([key '__t']) - axExpect)) > 1e-6
            error('GATE:GP1:YOUT', ...
                '[22] 信号 %s 时间轴与 0.01s 网格不符（%d 点 vs %d；疑似提前停机/降采样）', ...
                key, numel(dsC.([key '__t'])), nG);
        end
    end

    if plan.isProbe
        % 探针: fuel(30) 判外部输入是否生效（06→10 纯前馈，见头部依据）
        fuelProbe = dsC.fuel_total_kg(nG);
        fprintf('[22] 探针 fuel(%.2f)=%.6g kg（阈值 %.2g；接地=0）\n', ...
            plan.stopTime, fuelProbe, TH_FUEL_PROBE);
        if fuelProbe < TH_FUEL_PROBE && ~useIO
            fprintf('[22] WARN: 外部输入疑似未生效——以 InputOption 显式启用重试\n');
            useIO = true;
            % 在全程计划之前插入 fallback 探针（顺序保证: 探针验证先于全程 sim）
            plans = {plans{1}, struct('label', 'probe-fallback(StopTime=30)', ...
                'stopTime', 30, 'isProbe', true), plans{2}};
            continue;
        elseif fuelProbe < TH_FUEL_PROBE
            error('GATE:GP1:INPUT', ...
                ['[22] 外部输入未生效（fuel(30)=%.6g kg < %.2g，接地特征）。' ...
                 'setExternalInput 直连与 InputOption=''IndividualExternalInputs'' ' ...
                 '两路径均失败——请核查 SimulationInput 外部输入机制'], ...
                fuelProbe, TH_FUEL_PROBE);
        else
            if useIO
                ioNote = 'setExternalInput + InputOption=''IndividualExternalInputs''（fallback 生效）';
            end
            fprintf('[22] 探针 PASS: 外部输入已生效（%s）\n', ioNote);
        end
    else
        dsFull = dsC;   % 保留全程结果供对照
        tvFull = dsC.t_s__t;
    end
end
if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end   % 丢弃运行期参数改动，不保存

%% ---- 10. 逐拍对照（FD4/FD5；k=0..N-1 取输出网格 t=k 采样） ----
idxK  = (0:nRef-1).' * 100 + 1;      % t=k 处采样索引（0.01 s 网格）
refSOC = refM(:, iSOC);  refVsh = refM(:, iVsh);
refXsh = refM(:, iXsh);  refPL  = refM(:, iPL);
refPgen = refM(:, iPgen); refVcmd = refM(:, iVcmd);
mSoc = max(abs(dsFull.SOC(idxK) - refSOC));
mVsh = max(abs(dsFull.v_ship_mps(idxK) - refVsh));
mXsh = max(abs(dsFull.x_ship_m(idxK) - refXsh));
mPL  = max(abs(dsFull.P_load_total_kW(idxK) - refPL));
fuelSim = dsFull.fuel_total_kg(nGrid);
if ~(abs(tvFull(nGrid) - stopTime) < 1e-6)
    error('GATE:GP1:YOUT', '[22] 终点采样 t=%.9g 与 stop_time=%.9g 不符', ...
        tvFull(nGrid), stopTime);
end
fuelRel = abs(fuelSim - fuelRef) / fuelRef;
% t 对齐断言（FD5: 子集 t_s == k）
if max(abs(dsFull.t_s(idxK) - (0:nRef-1).')) > 1e-6
    error('GATE:GP1:YOUT', '[22] 子集 t_s 逐拍与 k 不对齐');
end
% 数值健康（FD4 前置条件）: SOC/v_ship/x_ship/P_load/fuel 全程无 NaN/Inf
health = all(isfinite(dsFull.SOC(idxK))) && all(isfinite(dsFull.v_ship_mps(idxK))) && ...
    all(isfinite(dsFull.x_ship_m(idxK))) && all(isfinite(dsFull.P_load_total_kW(idxK))) && ...
    all(isfinite(dsFull.fuel_total_kg(idxK))) && isfinite(fuelSim);
fprintf(['[22] 对照结果: SOC max|d|=%.3g | v_ship=%.3g | x_ship=%.3g | ' ...
    'P_load=%.3g | fuel_rel=%.3g (sim=%.9g ref=%.9g) | health=%d\n'], ...
    mSoc, mVsh, mXsh, mPL, fuelRel, fuelSim, fuelRef, health);

%% ---- 11. 产物写出（先写机器可读结果，FAIL 也在证据在案后终止） ----
emsRoot = fullfile(ROOT, '..', '..', 'firmware', 'stm32n6', '04_SourceCode', ...
    'SoftwarePackage', 'EMS_Projects', 'EMS01_CHIL_ECMS');
if ~(exist(emsRoot, 'dir') == 7)
    error('GATE:GP1:OUTDIR', '[22] EMS 工程目录不存在: %s', ...
        fullfile('firmware', 'stm32n6', '04_SourceCode', 'SoftwarePackage', ...
        'EMS_Projects', 'EMS01_CHIL_ECMS'));
end
refOutDir = fullfile(emsRoot, 'Host', 'reference');
if ~(exist(refOutDir, 'dir') == 7), mkdir(refOutDir); end
jsonPath = fullfile(refOutDir, ['v3_gp1_' scnTag '_gate.json']);
csvPath  = fullfile(refOutDir, ['v3_gp1_' scnTag '_replay.csv']);

% ---- 11.1 回放轨迹 CSV（FD7: 子集采样值 + 回放 P_gen；%.17g） ----
fid = fopen(csvPath, 'w');
if ~(fid > 0)
    error('GATE:GP1:OUTDIR', '[22] 无法写 %s', csvPath);
end
fprintf(fid, '%s\n', ['tick,t,P_load_kW,SOC,v_cmd_mps,v_ship_mps,accel_mps2,' ...
    'x_ship_m,fuel_total_kg,P_bat_kW,P_prop_kW,voyage_stop,P_gen_ref_kW']);
for k = 1:nRef
    fprintf(fid, ['%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,' ...
        '%.17g,%d,%.17g\n'], ...
        k - 1, dsFull.t_s(idxK(k)), dsFull.P_load_total_kW(idxK(k)), ...
        dsFull.SOC(idxK(k)), dsFull.v_cmd_mps(idxK(k)), ...
        dsFull.v_ship_mps(idxK(k)), dsFull.acceleration_mps2(idxK(k)), ...
        dsFull.x_ship_m(idxK(k)), dsFull.fuel_total_kg(idxK(k)), ...
        dsFull.P_bat_kW(idxK(k)), dsFull.P_prop_kW(idxK(k)), ...
        double(dsFull.voyage_stop(idxK(k))), refPgen(k));
end
fclose(fid);
fprintf('[22] replay CSV -> %s（%d 行）\n', ...
    fullfile('Host', 'reference', ['v3_gp1_' scnTag '_replay.csv']), nRef);

% ---- 11.2 gate JSON（FD6 结构，沿袭 gate_result.json 风格） ----
THR = 1e-6;
items = struct('metric', {}, 'value', {}, 'threshold', {}, 'pass', {});
items(end+1) = struct('metric', 'SOC_max_abs', 'value', mSoc, ...
    'threshold', THR, 'pass', mSoc <= THR);
items(end+1) = struct('metric', 'v_ship_max_abs', 'value', mVsh, ...
    'threshold', THR, 'pass', mVsh <= THR);
items(end+1) = struct('metric', 'x_ship_max_abs', 'value', mXsh, ...
    'threshold', THR, 'pass', mXsh <= THR);
items(end+1) = struct('metric', 'P_load_max_abs', 'value', mPL, ...
    'threshold', THR, 'pass', mPL <= THR);
items(end+1) = struct('metric', 'fuel_rel', 'value', fuelRel, ...
    'threshold', THR, 'pass', fuelRel <= THR);
items(end+1) = struct('metric', 'numerical_health', 'value', health, ...
    'threshold', true, 'pass', health);
overall = all([items.pass]);
gate = struct();
gate.task = 'T-602';
gate.spec = 'ems01-chil-v3-closedloop (FR-604/AC-601)';
gate.generated = char(datetime('now', 'Format', 'yyyy-MM-dd HH:mm:ss'));
matlabVer = regexp(version, 'R\d{4}[ab]', 'match', 'once');
if isempty(matlabVer), matlabVer = version; end
gate.matlab_version = matlabVer;
gate.scenario = scnTag;
gate.plantstep_model = 'model/HybridShip_ModelD_PlantStep.slx';
gate.ref_csv = sprintf('results/ecms_phase25/fw_reference/%s', refFile);
gate.thresholds = struct('SOC_max_abs', THR, 'v_ship_max_abs', THR, ...
    'x_ship_max_abs', THR, 'P_load_max_abs', THR, 'fuel_rel', THR, ...
    'numerical_health', true);
gate.metrics = struct('SOC_max_abs', mSoc, 'v_ship_max_abs', mVsh, ...
    'x_ship_max_abs', mXsh, 'P_load_max_abs', mPL, 'fuel_rel', fuelRel, ...
    'fuel_ref', fuelRef, 'fuel_sim', fuelSim, 'rows_cmp', nRef, ...
    'numerical_health', health);
gate.items = items;
gate.overall = overall;
try
    js = jsonencode(gate, 'PrettyPrint', true);
catch
    js = jsonencode(gate);   % 旧版本无 PrettyPrint 时退化紧凑输出
end
fid = fopen(jsonPath, 'w', 'n', 'UTF-8');
if ~(fid > 0)
    error('GATE:GP1:OUTDIR', '[22] 无法写 %s', jsonPath);
end
fwrite(fid, js, 'char');
fclose(fid);
fprintf('[22] gate JSON -> %s | overall=%s\n', ...
    fullfile('Host', 'reference', ['v3_gp1_' scnTag '_gate.json']), string(overall));

%% ---- 12. 门禁出口（03/06 惯例: FAIL 以 error 终止，Agent 以工具失败为准） ----
if ~overall
    error('GATE:GP1Failed', ...
        ['[22] G-P1 门禁 FAIL（%s）: 逐拍/fuel 对照超 design §5.2 阈值，' ...
         '排查不放宽。详见 Host/reference/v3_gp1_%s_gate.json'], scnTag, scnTag);
end
fprintf('[22] G-P1 门禁 PASS（%s）: %d 拍逐点对照 + fuel 终值均在阈内\n', scnTag, nRef);
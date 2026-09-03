%% ========================================================================
%  24_export_gp2_simref.m — T-605 配套（spec: ems01-chil-v3-closedloop,
%  FR-605/AC-602；design §4.4a + Host/tools/run_gp2_gate.py 的 sim-ref 契约）
%
%  用途: 解释执行运行被控对象子集模型（与 22_run_gp1_gate.m 完全同构的回放
%        注入与确定性运行——G-P1 已证两遍确定性，本运行轨迹与 gp1 replay
%        逐位一致），在 tick 时刻（t=1..N-1 拍边界）采样 logsout 信号，导出
%        G-P2 门禁的 sim 参考侧:
%        EMS01_CHIL_ECMS/Host/reference/v3_gp2_<scenario>_simref.csv
%        （16 列，%.17g，列契约 = run_gp2_gate.py 头注释 sim-ref 契约）。
%
%  列源（16 列）:
%    tick,t                  —— 拍序 1..N-1 与 t=k
%    x_ship_m/v_ship_mps/SOC —— 既有 logsout 信号（= 03/14 的 CSTATE 输出）
%    fuel_total_kg           —— 既有 logsout 'fuel_total_kg'（kg 口径）
%    omega_prop_radps        —— 03_Propulsion_Plant/omega_prop 输出线（运行期增配 logging）
%    drive_lag_s             —— 03_Propulsion_Plant/DriveLag 输出线（运行期增配 logging）
%    ki_integrator           —— 02_Ship_Speed_Controller/Ki_int 输出线（运行期增配 logging）
%    V_dc_V                  —— 既有 logsout 'V_dc_V'（13/1 = V_int CSTATE 输出）
%    p_gen_inertia_kW        —— 既有 logsout 'P_gen_actual_kW'（06/1 = inertia CSTATE 输出）
%    P_load_kW/v_cmd_mps/accel_mps2/P_bat_kW/P_prop_kW
%                            —— 既有 logsout 同名信号（与 gp1 replay 交叉核对列）
%
%  内部状态增配: omega_prop/DriveLag/Ki_int 不在 configureLogging 清单内——
%        本脚本运行期对这三条输出线启用 DataLogging（自定义名），
%        close_system(.,0) 丢弃、不 save_system——模型文件零改动（22 号纪律）。
%
%  场景: base 变量 GP2_SCENARIOS（cell of 'S20'|'voyage002'，缺省双场景）。
%  运行: GP2_SCENARIOS={'S20'}; eval(fileread('scripts/24_export_gp2_simref.m'))
%        （数字开头只能 eval；无局部函数——线 logging 启用为内联代码段）
%  红线: 主模型与 PlantStep.slx 零改动（运行期 logging 配置不回写，哈希复核）；
%        冻结资产只读。失败 error('GATE:GP2SIMREF:*')。
%  ========================================================================

ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
cd(ROOT);
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'), ...
    fullfile(ROOT, 'scripts', '+ecms2'), fullfile(ROOT, 'scripts', '+ecms25'));
fprintf('\n[24] ===== T-605 gp2 simref export (FR-605/AC-602) =====\n');
fprintf('[24] ROOT = %s | start: %s\n', ROOT, datestr(now, 'yyyy-mm-dd HH:MM:SS'));
fprintf('[24] MATLAB %s\n', version);

%% ---- 0. 前置与场景清单 ----
MDL_PS = 'HybridShip_ModelD_PlantStep';
psPath = fullfile(ROOT, 'model', [MDL_PS '.slx']);
auditPath = fullfile(ROOT, 'results', 'ecms_phase25', 'plantstep', ...
    'plantstep_build_audit.json');
if exist(psPath, 'file') <= 0
    error('GATE:GP2SIMREF:PRE', '[24] 缺少 %s（先执行 20 号）', ...
        fullfile('model', [MDL_PS '.slx']));
end
if exist(auditPath, 'file') ~= 2
    error('GATE:GP2SIMREF:PRE', '[24] 缺少 T-601 结构审计产物');
end
if evalin('base', 'exist(''GP2_SCENARIOS'', ''var'')') == 1
    scnList = evalin('base', 'GP2_SCENARIOS');
else
    scnList = {'S20', 'voyage002'};
end
refRunIdOf = containers.Map({'S20', 'voyage002'}, ...
    {'G2_S20_composite_voyage_matrix', 'G2_test_voyage_002_v2'});

emsRoot = fullfile(ROOT, '..', '..', 'firmware', 'stm32n6', '04_SourceCode', ...
    'SoftwarePackage', 'EMS_Projects', 'EMS01_CHIL_ECMS');
if ~(exist(emsRoot, 'dir') == 7)
    error('GATE:GP2SIMREF:OUTDIR', '[24] EMS 工程目录不存在');
end
outDir = fullfile(emsRoot, 'Host', 'reference');
if ~exist(outDir, 'dir'), mkdir(outDir); end

%% ---- 1. base 注入两份 config（22 §3 同口径） ----
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));

%% ---- 2. 主模型 PlantStep 内容哈希（before，shasum——T-602 定案禁 java fis.read） ----
[stS, outS] = system(sprintf('shasum -a 256 "%s"', psPath));
if stS ~= 0
    error('GATE:GP2SIMREF:HASH', '[24] shasum(PlantStep) 失败: %s', outS);
end
shaBefore = regexp(outS, '^[0-9a-f]{64}', 'match', 'once');

failCount = 0;
for si = 1:numel(scnList)
    scnTag = scnList{si};
    if ~any(strcmp(scnTag, {'S20', 'voyage002'}))
        error('GATE:GP2SIMREF:SCENARIO', '[24] 非法场景 %s', scnTag);
    end
    runId = refRunIdOf(scnTag);
    refFile = fullfile(ROOT, 'results', 'ecms_phase25', 'fw_reference', ...
        ['ref_' runId '.csv']);
    if exist(refFile, 'file') ~= 2
        error('GATE:GP2SIMREF:PRE', '[24] 缺少冻结回放输入 %s', refFile);
    end
    refM = readmatrix(refFile);
    nRef = size(refM, 1);              % tick 0..N-1（G-P1 replay 同行数）
    stopTime = nRef - 1;
    fprintf('\n[24] ---- %s | run_id=%s | stop_time=%.6g | rows=%d ----\n', ...
        scnTag, runId, stopTime, nRef);

    % ---- 场景构造（22 §4 同构） ----
    if strcmp(scnTag, 'S20')
        scn = evalin('base', 'SCENARIOS(20);');
        assert(strcmp(scn.name, 'S20_composite_voyage'), 'GATE:GP2SIMREF:SCENARIO', ...
            'SCENARIOS(20).name 漂移');
    else
        vid = 2;
        fleetJson = fullfile(ROOT, 'results', 'ecms_phase25', 'fleet', ...
            'voyages_seed20260829.json');
        VJ = jsondecode(fileread(fleetJson));
        vSel = [];
        for j = 1:numel(VJ.voyages)
            if VJ.voyages(j).id == vid, vSel = VJ.voyages(j); break; end
        end
        assert(~isempty(vSel), 'GATE:GP2SIMREF:SCENARIO', 'fleet json 缺 id=%d', vid);
        scn = ecms25.voyage_scn_v2(vSel);
    end

    % ---- 回放网格（22 §7 同构造: P(t)=ref 行 floor(t)+1，1s ZOH） ----
    pCol = refM(:, 9);                 % P_gen_ref_kW（21 列固定序）
    tGrid = (0:0.01:stopTime).';
    pGrid = pCol(min(floor(tGrid) + 1, numel(pCol)));
    assert(numel(tGrid) == round(stopTime * 100) + 1, 'GATE:GP2SIMREF:GRID', ...
        '[24] 回放网格点数不符');
    tsP = timeseries(pGrid(:), tGrid(:), 'Name', 'P_gen_ref_kW');

    % ---- 装载 + 运行期 logging 增配（不 save_system） ----
    if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end
    load_system(psPath);
    if ~strcmp(get_param(MDL_PS, 'Name'), MDL_PS)
        error('GATE:GP2SIMREF:MODEL', '[24] 装载根名不符');
    end
    % 三个内部连续状态信号: 找其输出线 → 运行期启用 DataLogging（自定义名）
    intSigBlk = containers.Map( ...
        {'omega_prop_radps', 'drive_lag_s', 'ki_integrator'}, ...
        {'03_Propulsion_Plant/omega_prop', '03_Propulsion_Plant/DriveLag', ...
         '02_Ship_Speed_Controller/Ki_int'});
    keySet = keys(intSigBlk);
    for k = 1:numel(keySet)
        nm = keySet{k};
        blk = [MDL_PS '/' intSigBlk(nm)];
        ph = get_param(blk, 'PortHandles');
        ln = get_param(ph.Outport(1), 'Line');
        if ~(isnumeric(ln) && ln > 0)
            error('GATE:GP2SIMREF:LOGCFG', '[24] %s 输出线不存在（无连线）', blk);
        end
        % DataLogging 参数在【源端口句柄】上（line 无该参数，T-605 执行期实测）
        set_param(ph.Outport(1), 'DataLogging', 'on', ...
            'DataLoggingNameMode', 'Custom', 'DataLoggingName', nm, ...
            'DataLoggingDecimateData', 'off');
        fprintf('[24] 运行期 logging 增配: %s <- %s\n', nm, intSigBlk(nm));
    end

    % ---- 仿真（22 §9 同注入；SaveOutput 仅用于取 yout——实际用 logsout dense） ----
    inK = Simulink.SimulationInput(MDL_PS);
    inK = inK.setVariable('SCN', scn);
    inK = inK.setVariable('EMS_CTRL_LAW', 7);
    inK = inK.setVariable('EMS_MODE', 1);
    inK = inK.setVariable('AI_Pred_Source', 0);
    inK = inK.setVariable('Bat_SOC_init', scn.initial_SOC);
    inK = inK.setVariable('VOYAGE_SPEED_MODE', 1);
    inK = inK.setVariable('VOYAGE_V_TIME_S', ecms2.scn_field(scn, 'v_time_s', [0 1]));
    inK = inK.setVariable('VOYAGE_V_VALUE_MPS', ecms2.scn_field(scn, 'v_value_mps', [0 0]));
    inK = inK.setVariable('VOYAGE_CURRENT_MODE', 1);
    inK = inK.setVariable('VOYAGE_CURRENT_TIME_S', ecms2.scn_field(scn, 'current_time_s', [0 1]));
    inK = inK.setVariable('VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(scn, 'current_value_mps', [0 0]));
    inK = inK.setModelParameter('StopTime', sprintf('%.2f', floor(stopTime * 100) / 100));
    inK = inK.setModelParameter('ReturnWorkspaceOutputs', 'on');
    inK = inK.setModelParameter('SignalLogging', 'on');
    inK = inK.setModelParameter('SignalLoggingName', 'logsout');
    inK = inK.setExternalInput(tsP);
    fprintf('[24] sim %s ...\n', scnTag);
    t0 = tic;
    out = sim(inK);
    fprintf('[24] sim done in %.1f s\n', toc(t0));

    % ---- logsout 提取（0.01 s dense；命名严格匹配） ----
    ls = out.logsout;
    want = {'P_load_total_kW', 'SOC', 'v_cmd_mps', 'v_ship_mps', ...   % t_s=Clock 根 Outport 不在 logsout（CSV t 列用时间轴）
        'acceleration_mps2', 'x_ship_m', 'fuel_total_kg', 'P_bat_actual_kW', ...
        'P_prop_actual_kW', 'V_dc_V', 'P_gen_actual_kW', ...
        'omega_prop_radps', 'drive_lag_s', 'ki_integrator'};
    got = struct();
    gotNames = {};
    for e = 1:ls.numElements
        el = ls.getElement(e);
        if isa(el, 'timeseries'), vv = el; else, vv = el.Values; end
        nm = el.Name;
        if isempty(nm), continue; end
        tt = double(vv.Time(:));
        dd = squeeze(double(vv.Data));
        if size(dd, 2) > 1, dd = dd(:, 1); end
        dd = dd(:);
        gotNames{end + 1} = nm; %#ok<SAGROW>
        got.(matlab.lang.makeValidName(nm)) = dd;
        got.(matlab.lang.makeValidName([nm '__t'])) = tt;
    end
    miss = want(cellfun(@(s) ~isfield(got, matlab.lang.makeValidName(s)), want));
    if ~isempty(miss)
        error('GATE:GP2SIMREF:LOGCFG', ...
            '[24] logsout 缺少信号: %s | 实际: %s', ...
            strjoin(miss, ','), strjoin(gotNames, ','));
    end

    % ---- tick 采样（t=1..N-1 拍边界；t 轴 0.01 s 网格断言） ----
    axT = got.SOC__t;
    nGrid = numel(axT);
    assert(abs(axT(2) - 0.01) < 1e-12 && abs(axT(end) - stopTime) < 1e-9, ...
        'GATE:GP2SIMREF:AXIS', '[24] logsout 时间轴非 0.01 s 网格');
    idxK = (1:(nRef - 1)).' * 100 + 1;      % t=k（k=1..N-1）
    assert(idxK(end) <= nGrid, 'GATE:GP2SIMREF:AXIS', '[24] 采样越界');
    tk = axT(idxK);

    % ---- 写 simref CSV（16 列 %.17g，行=tick 1..N-1） ----
    csvPath = fullfile(outDir, ['v3_gp2_' scnTag '_simref.csv']);
    fid = fopen(csvPath, 'w', 'n', 'UTF-8');
    assert(fid > 0, 'GATE:GP2SIMREF:OUTDIR', '[24] 无法写 %s', csvPath);
    fprintf(fid, ['tick,t,x_ship_m,v_ship_mps,SOC,fuel_total_kg,' ...
        'omega_prop_radps,drive_lag_s,V_dc_V,p_gen_inertia_kW,' ...
        'ki_integrator,P_load_kW,v_cmd_mps,accel_mps2,P_bat_kW,P_prop_kW\n']);
    for i = 1:numel(idxK)
        kk = idxK(i);
        fprintf(fid, ['%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,' ...
            '%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n'], ...
            i, tk(i), got.x_ship_m(kk), got.v_ship_mps(kk), got.SOC(kk), ...
            got.fuel_total_kg(kk), got.omega_prop_radps(kk), ...
            got.drive_lag_s(kk), got.V_dc_V(kk), got.P_gen_actual_kW(kk), ...
            got.ki_integrator(kk), got.P_load_total_kW(kk), ...
            got.v_cmd_mps(kk), got.acceleration_mps2(kk), ...
            got.P_bat_actual_kW(kk), got.P_prop_actual_kW(kk));
    end
    fclose(fid);
    fprintf('[24] simref CSV -> %s（%d 行 × 16 列）\n', ...
        fullfile('Host', 'reference', ['v3_gp2_' scnTag '_simref.csv']), ...
        numel(idxK));

    % ---- 模型零改动复核 ----
    [stS2, outS2] = system(sprintf('shasum -a 256 "%s"', psPath));
    assert(stS2 == 0, 'GATE:GP2SIMREF:HASH', '[24] shasum 失败: %s', outS2);
    shaAfter = regexp(outS2, '^[0-9a-f]{64}', 'match', 'once');
    if ~strcmp(shaBefore, shaAfter)
        error('GATE:GP2SIMREF:TOUCHED', ...
            '[24] PlantStep.slx 内容被触碰（%s -> %s）——拒绝通过', ...
            shaBefore, shaAfter);
    end
    if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end   % 丢弃运行期配置
    fprintf('[24] OK %s（模型哈希未变，运行期配置已丢弃）\n', scnTag);
end

fprintf('\n[24] T-605 simref 导出完成（%d 场景）\n', numel(scnList));


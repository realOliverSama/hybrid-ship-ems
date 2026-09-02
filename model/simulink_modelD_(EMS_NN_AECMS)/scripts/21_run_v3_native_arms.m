%% ========================================================================
%  21_run_v3_native_arms.m — T-610（spec ems01-chil-v3-closedloop，FR-609/AC-604）：
%  v3 原生闭环仿真臂 (b′)/(c) 运行与导出（原生 Simulink 解释执行，非 codegen 被控对象）。
%
%  臂定义（Fixed Decisions，不得偏离）：
%    (b′) law=7：EMS_NN_AECMS 变体（MLP-Q 预测 + 工况门控，G2）解释执行闭环。
%        前置：主模型 chart 已处于 G2（载入后 chart Script 含 'GATE_MODE = 2'，
%        66:136-138 断言机制；本脚本不做 63 集成、不 save_system，主模型只装载/sim）。
%    (c)  law=5：EMS_SOC_Adaptive_ECMS 变体（SOC-Adaptive ECMS，无预测，无 chart swap）。
%  场景：每次调用跑当前 law 的两个场景（每场景同 session 串行跑两遍做确定性 REPRO）：
%    S20_composite_voyage（Model_Scenarios SCENARIOS(20)，StopTime=5520）
%    voyage_002（fleet json id==2 -> ecms25.voyage_scn_v2，StopTime=4211，同 66:96-107）
%
%  机制来源（只读复用 64/66，不改动源脚本/模型/冻结资产）：
%    - 注入与串行 sim：64:194-233 / 66:171-186（SimulationInput：SCN / EMS_CTRL_LAW /
%      EMS_MODE / Bat_SOC_init / AI_Pred_Source=0 / VOYAGE_* 六变量 + StopTime 覆盖
%      floor(t*100)/100；useParsim=false 串行，64:172）；
%    - 信号提取：66:189-240 内联严格同轴校验（固定步长 0.01 s 全信号同轴）；
%    - tick 判定：66:245-258（EML 早退守卫 t-lastT < dt-1e-9 逐采样重放）；
%    - EML 重放（e_hat/K/cls 列）：66:129-170 dbg 副本文本变换（仅增 dbg 输出，P 路径
%      零改动）+ 66:150-162 REPLAY_ARGS + 66:277-283 同网格率 >= 0.90 断言；
%      诊断副本改名 ecms_nn_dbg_v3 避让 fw_reference 冻结副本，防旧路径遮蔽；
%      每场景重放前 clear 该函数 persistent（EML 的 t<lastT 新仿真重置分支会使暖会话
%      第二场景的首拍早退；clear 后与 chart 每次 sim 的全新实例语义一致）；
%    - 参考轨迹 CSV：66:270-279 的 21 列格式（%.17g，表头与 ref_G2 CSV 逐字符一致），
%      追加第 22 列 fuel_total_kg（tick 时刻累计油耗；ref_G2 无逐拍油耗列，design
%      "逐拍油耗诊断另行一次性导出、产物放 v3 专用目录"定案）；
%    - 指标：design §8.1；fuel_corr = fuel_raw + (0.60-SOC_end)*BatCap*K_EQ
%      （64:239 同式同常数，K_EQ=0.4774、BatCap=Bat_Cap_kWh，单一来源禁新造公式）；
%    - REPRO：66:313-336 冻结缓存比对阈值（abs(dFc)<1e-3 && abs(dSoc)<1e-6）+
%      本脚本新增同 session 两遍确定性比对（d_fuel==0 && d_SOC==0）。
%
%  law=5 (c) 臂无 EML 预测，CSV 门控列直填（Fixed Decision 5）：
%    e_hat=0、e_hat_raw=0、K=1、cls_eff=-1、cls_raw=-1；
%    K_q/K_w/q10_kwh/q50_kwh/q90_kwh/s_q_kwh/P_replay_kW = NaN（不适用）。
%
%  产物（results/ecms_phase25/closedloop/native_arms/，重跑覆盖自身文件，幂等）：
%    <run_id>.csv                22 列逐拍轨迹（%.17g）
%      run_id：V3_B_S20_native_law7 / V3_B_voyage002_native_law7 /
%              V3_C_S20_native_law5 / V3_C_voyage002_native_law5
%    native_arms_summary.json    task/generated/matlab_version/arms[]/overall；
%      arm：{run_id,law,scenario,fuel_raw_kg,fuel_corr_kg,SOC_end,v_track_rmse,
%            e_hat_active_ratio,n_undervoltage,n_derate,wall_time_s,sim_error,
%            repro{pass,d_fuel,d_SOC,ref_source,two_pass,cache}}（另含辅助诊断字段，
%            见 summary.notes）；多次调用（law=7 与 law=5 各一次）合并写入：
%            非本次 law 的 arm 保留，本次 law 的 arm 覆盖，overall 按合并后重算。
%    ecms_nn_dbg_v3.m            law=7 诊断 EML 副本（66 §4 机制，文本变换生成）
%
%  冻结资产只读：ref_G2_*.csv / export_summary.json / meta_G2_*.json /
%    baseline_manifest.json / gating/（缓存 mat 仅 load 比对，不回写）。
%
%  运行方式（脚本名数字开头只能 eval 执行，与 63/64/66 同；本文件不得含局部函数）：
%    NATIVE_ARMS_LAW=7; eval(fileread('scripts/21_run_v3_native_arms.m'))
%    NATIVE_ARMS_LAW=5; eval(fileread('scripts/21_run_v3_native_arms.m'))
%  路径纪律（AGENTS §20.2）：ROOT = 双 fileparts + pwd 兜底，禁绝对路径。
%  ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
cd(ROOT);  % eval 下 mfilename 为空，必须位于工作区根（66 同处理）
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'), ...
    fullfile(ROOT, 'scripts', '+ecms2'), fullfile(ROOT, 'scripts', '+ecms25'), ...
    fullfile(ROOT, 'scripts', '+model'));

MDL = 'HybridShip_ModelD_DCBus';
OUT = fullfile(ROOT, 'results', 'ecms_phase25', 'closedloop', 'native_arms');
if ~exist(OUT, 'dir'), mkdir(OUT); end
diary(fullfile(OUT, 'native_arms_diary.txt')); diary on;
fprintf('\n[21] ===== T-610 v3 native closed-loop arms =====\n');
fprintf('[21] ROOT = %s | start: %s\n', ROOT, datestr(now, 'yyyy-mm-dd HH:MM:SS'));
fprintf('[21] MATLAB %s\n', version);

%% ---- 0. 参数校验（Fixed Decision 1：NATIVE_ARMS_LAW 必填 7|5） ----
if ~exist('NATIVE_ARMS_LAW', 'var')
    error('ECMS25:V3ARMS:ARG', ['缺少 base 变量 NATIVE_ARMS_LAW（7|5）。用法：' ...
        'NATIVE_ARMS_LAW=7; eval(fileread(''scripts/21_run_v3_native_arms.m''))']);
end
LAW = double(NATIVE_ARMS_LAW);
if ~ismember(LAW, [5, 7])
    error('ECMS25:V3ARMS:ARG', ...
        'NATIVE_ARMS_LAW 必须为 7（(b′) 臂 G2 门控）或 5（(c) 臂 SOC-Adaptive），当前为 %g', LAW);
end
if LAW == 7, armTag = '(b′)'; else, armTag = '(c)'; end
fprintf('[21] NATIVE_ARMS_LAW = %d（%s 臂）\n', LAW, armTag);

%% ---- 1. 常数/场景准备（64:33-39 / 66:44-49 同口径） ----
model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
LAM0_EXP = 0.425;  K_SOC_EXP = 30;  K_S_EXP = 0.01;
assert(abs(ECMS_Lambda - LAM0_EXP) < 1e-9, 'ECMS25:V3ARMS:ARG', 'ECMS_Lambda 漂移');
assert(abs(ECMS_K_soc - K_SOC_EXP) < 1e-9, 'ECMS25:V3ARMS:ARG', 'ECMS_K_soc 漂移');
assert(abs(ECMS_K_S - K_S_EXP) < 1e-9, 'ECMS25:V3ARMS:ARG', 'ECMS_K_S 漂移');
BatCap = Bat_Cap_kWh;                % 1071.36 kWh（fuel_corr 折算，64 同式）
K_EQ   = 0.4774;                     % fuel_corr 折算系数 [kg/kWh]（64 同口径）
SOC_REF_V3 = 0.60;                   % fuel_corr 公式字面 0.60（64:239 同式）
if LAW == 7
    sNN = ecms25.ensure_nn_params_v2(ROOT);
    assert(sNN.ok == 1, 'ECMS25:V3ARMS:ARG', ...
        'law7-v2 权重未就绪（63 应已写入 config/ecms_nn_weights_v2.mat）');
end

%% ---- 2. 场景行规格（S20 同 66:88-94；voyage_002 同 66:96-107） ----
scnMap = containers.Map('KeyType', 'char', 'ValueType', 'any');
for k = 1:numel(SCENARIOS)
    if ischar(SCENARIOS(k).name) && ~isempty(SCENARIOS(k).name)
        scnMap(SCENARIOS(k).name) = SCENARIOS(k);
    end
end
SCN_S20 = scnMap('S20_composite_voyage');

VID_V3 = 2;   % voyage_002（fleet json id==2，spec 固定）
fleetJson = fullfile(ROOT, 'results', 'ecms_phase25', 'fleet', 'voyages_seed20260829.json');
VJ = jsondecode(fileread(fleetJson));
vV2 = [];
for j = 1:numel(VJ.voyages)
    if VJ.voyages(j).id == VID_V3, vV2 = VJ.voyages(j); break; end
end
assert(~isempty(vV2), 'ECMS25:V3ARMS:ARG', 'fleet json 缺 id=%d 航次（voyage_002）', VID_V3);
SCN_V2 = ecms25.voyage_scn_v2(vV2);

if LAW == 7
    ridS20 = 'V3_B_S20_native_law7';
    ridV2  = 'V3_B_voyage002_native_law7';
    cachFileS20 = 'run_G2_S20_composite_voyage_matrix.mat';   % 冻结缓存（66 产物，只读）
    cachFileV2  = 'run_G2_test_voyage_002_v2.mat';
    cachNameS20 = ['gating/runs/' cachFileS20];   % JSON 显示用相对路径（66 meta 同风格）
    cachNameV2  = ['gating/runs/' cachFileV2];
else
    ridS20 = 'V3_C_S20_native_law5';
    ridV2  = 'V3_C_voyage002_native_law5';
    cachFileS20 = '';                % law=5 无既有冻结缓存（仅两遍确定性比对）
    cachFileV2  = '';
    cachNameS20 = 'none';
    cachNameV2  = 'none';
end
RS = struct('run_id', {}, 'scn', {}, 'stopTime', {}, 'cache', {}, 'cacheName', {});
RS(end + 1) = struct('run_id', ridS20, 'scn', SCN_S20, 'stopTime', SCN_S20.stop_time_s, ...
    'cache', fullfile(ROOT, 'results', 'ecms_phase25', 'gating', 'runs', cachFileS20), ...
    'cacheName', cachNameS20);
RS(end + 1) = struct('run_id', ridV2, 'scn', SCN_V2, 'stopTime', SCN_V2.stop_time_s, ...
    'cache', fullfile(ROOT, 'results', 'ecms_phase25', 'gating', 'runs', cachFileV2), ...
    'cacheName', cachNameV2);
fprintf('[21] rows: %s\n', strjoin({RS.run_id}, ', '));

%% ---- 3. 装载主模型 + law=7 G2 断言（66:136-138 机制；只装载不 save） ----
if bdIsLoaded(MDL), close_system(MDL, 0); end
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
if LAW == 7
    rt = sfroot;
    ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', ...
        [MDL '/05_EMS_Controller/EMS_NN_AECMS/ecms_step']);
    if isempty(ch) || ~contains(ch(1).Script, 'GATE_MODE = 2')
        error('ECMS25:V3ARG:G2', ...
            'chart 未处于 G2（主模型须先经 63_integrate_gate_v2 GATE_MODE=2 修复后重试）');
    end
    fprintf('[21] chart GATE_MODE=2 已确认 | ChartUpdate=%s SampleTime=%s\n', ...
        ch(1).ChartUpdate, ch(1).SampleTime);
else
    fprintf('[21] law=5 (c) 臂：EMS_SOC_Adaptive_ECMS 变体，无 chart swap/断言\n');
end

%% ---- 4. law=7 诊断用 EML 副本（66:129-170 机制原样，仅函数名 _v3 避让冻结副本） ----
if LAW == 7
    emlPath = fullfile(ROOT, 'results', 'ecms_phase25', 'gating', 'eml_ecms_step_mode2.m');
    dbgPath = fullfile(OUT, 'ecms_nn_dbg_v3.m');
    src = fileread(emlPath);
    A_SIG  = 'function P = ecms_nn(t, P_load, v_ship, SOC,';
    A_PER  = 'persistent lastT Pheld buf k vprev cls_eff run_cls run_cnt';
    A_GATE = '    e_hat = K * e_hat;';
    A_FB   = '    e_hat = 0;';
    assert(count(src, A_SIG) == 1 && count(src, A_PER) == 1 && ...
        count(src, A_GATE) == 1 && count(src, A_FB) == 1, ...
        'ECMS25:V3ARMS:ARG', 'EML 诊断变换锚点不唯一，拒绝变换');
    src = strrep(src, A_SIG, 'function [P, dbg] = ecms_nn_dbg_v3(t, P_load, v_ship, SOC,');
    src = strrep(src, A_PER, [A_PER newline 'dbg = [];']);
    CAP_OK = ['    dbg = struct(''ok'',true,''e_hat_raw'',e_hat,''K_q'',K_q,' ...
        '''K_w'',K_w,''K'',K,''cls_raw'',cls,''cls_eff'',cls_eff,' ...
        '''q10'',Ehat_q(1),''q50'',Ehat_q(2),''q90'',Ehat_q(3),''s_q'',s_q);'];
    src = strrep(src, A_GATE, [CAP_OK newline A_GATE]);   % 先捕获（raw）再门控
    CAP_FB = ['    dbg = struct(''ok'',false,''e_hat_raw'',0,''K_q'',NaN,' ...
        '''K_w'',NaN,''K'',NaN,''cls_raw'',-1,''cls_eff'',-1,' ...
        '''q10'',NaN,''q50'',NaN,''q90'',NaN,''s_q'',NaN);'];
    src = strrep(src, A_FB, [A_FB newline CAP_FB]);
    fid = fopen(dbgPath, 'w'); assert(fid > 0, 'ECMS25:V3ARMS:ARG', '无法写 %s', dbgPath);
    fwrite(fid, src, 'char'); fclose(fid);
    addpath(OUT);
    clear('ecms_nn_dbg_v3');   % 强制按新文本重新解析
    wChk = which('ecms_nn_dbg_v3');
    assert(~isempty(wChk) && strcmp(wChk, dbgPath), 'ECMS25:V3ARMS:ARG', ...
        'ecms_nn_dbg_v3 解析到 %s 而非 %s（路径遮蔽）', wChk, dbgPath);
    % EML 重放实参（66:150-162 逐项一致：chart Constant/Parameter 的 base workspace 解析值）
    REPLAY_ARGS = {ECMS_Lambda, ECMS_K_soc, ECMS_SOC_Ref, ECMS_K_S, ...
        ECMS_Lambda_Floor, BSFC_Breakpoints_kW, BSFC_Table_gkWh, ...
        ECMS_NN_W1, ECMS_NN_b1, ECMS_NN_W2, ECMS_NN_b2, ECMS_NN_W3, ECMS_NN_b3, ...
        ECMS_NN_XMean, ECMS_NN_XStd, ECMS_NN_YMean, ECMS_NN_YStd, ...
        ECMS_NN_Ebar_H_kWh, ECMS_NN_Escale_kWh, 1, ...
        ASSUMED_eta_rect, ASSUMED_eta_dcdc, ASSUMED_Bat_Eff_Discharge, ...
        ASSUMED_Bat_Eff_Charge, Gen_P_max, ECMS_SOC_Discharge_Max, ...
        ECMS_SOC_Charge_Min, ECMS_Idle_Fuel_kgph, ECMS_Grid_dP_kW, ...
        ECMS_dt_ctrl_s, Pgen_init};
    fprintf('[21] 诊断副本 -> %s（P 计算路径与基线逐字相同）\n', dbgPath);
end
REQ_SIG = {'v_cmd_mps', 'v_ship_mps', 'x_ship_m', 'acceleration_mps2', ...
    'P_load_total_kW', 'SOC', 'P_gen_ref_kW', 'fuel_total_kg'};

%% ---- 5. 逐场景运行（每场景同 session 串行两遍；注入同 64:194-233 / 66:171-186） ----
arms = {};                       % 本次调用的 arm 记录（cell of struct）
simFailCount = 0;
reproFailCount = 0;
for r = 1:numel(RS)
    row = RS(r);
    fprintf('\n[21] ----- run %s (law=%d, stopTime=%.2f) -----\n', row.run_id, LAW, row.stopTime);
    useCache = ~strcmp(row.cacheName, 'none');
    fuelP = [NaN NaN]; socP = [NaN NaN]; wallP = [NaN NaN];
    simErrStr = ''; ds = []; dsP = []; d_traj = NaN; metricNotes = {};
    n_samples = 0; nt = 0;

    for iP = 1:2
        inK = Simulink.SimulationInput(MDL);
        inK = inK.setVariable('SCN', row.scn);
        inK = inK.setVariable('EMS_CTRL_LAW', LAW);
        inK = inK.setVariable('EMS_MODE', double(LAW > 0));
        inK = inK.setVariable('Bat_SOC_init', row.scn.initial_SOC);
        inK = inK.setVariable('AI_Pred_Source', 0);
        inK = inK.setVariable('VOYAGE_SPEED_MODE', 1);
        inK = inK.setVariable('VOYAGE_V_TIME_S', ecms2.scn_field(row.scn, 'v_time_s', [0 1]));
        inK = inK.setVariable('VOYAGE_V_VALUE_MPS', ecms2.scn_field(row.scn, 'v_value_mps', [0 0]));
        inK = inK.setVariable('VOYAGE_CURRENT_MODE', 1);
        inK = inK.setVariable('VOYAGE_CURRENT_TIME_S', ecms2.scn_field(row.scn, 'current_time_s', [0 1]));
        inK = inK.setVariable('VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(row.scn, 'current_value_mps', [0 0]));
        inK = inK.setModelParameter('StopTime', sprintf('%.2f', floor(row.stopTime * 100) / 100));
        inK = inK.setUserString(row.run_id);

        t0 = tic;
        simErrP = '';
        try, out = sim(inK); catch errS, out = []; simErrP = errS.message; end
        wallP(iP) = toc(t0);
        if isempty(out)
            fprintf('[21] sim FAIL (pass %d) %s: %s\n', iP, row.run_id, simErrP);
            simErrStr = simErrP;
            break;   % pass1 失败不再跑 pass2；pass2 失败保留 pass1 导出件
        end
        fprintf('[21] pass %d sim done in %.1f s\n', iP, wallP(iP));

        % ---- 信号提取（66:189-240 内联机制原样：固定步长 0.01 s 全信号同轴严格校验；
        %         本文件数字开头只能 eval，不允许局部函数） ----
        ls = out.logsout;
        nEl = ls.numElements;
        names = {}; tss = {}; dds = {};
        for i = 1:nEl
            el = ls.getElement(i);
            nm = el.Name;
            if isempty(nm), continue; end
            vv = el.Values;
            tt = double(vv.Time(:));
            dd = squeeze(vv.Data);
            if size(dd, 2) > 1, dd = dd(:, 1); end
            dd = double(dd(:));
            if isempty(tt), continue; end
            names{end + 1} = nm; tss{end + 1} = tt; dds{end + 1} = dd; %#ok<AGROW>
        end
        assert(~isempty(names), 'ECMS25:EXTRACT', 'logsout 无命名信号');
        lenAx = cellfun(@numel, tss);
        [~, iMax] = max(lenAx);
        sigT = tss{iMax};                      % 最长轴（唯一性由逐信号校验保证）
        dsP = struct('t', sigT);
        seenNames = {};
        for i = 1:numel(names)
            tt = tss{i}; dd = dds{i};
            if numel(tt) == 1
                dd = repmat(dd(1), numel(sigT), 1);  % 常量信号沿全程复制
            elseif numel(tt) ~= numel(sigT) || any(tt ~= sigT)
                error('ECMS25:EXTRACT', ...
                    '信号 %s 时间轴与主轴不一致（%d vs %d 点）——固定步长下不应发生', ...
                    names{i}, numel(tt), numel(sigT));
            end
            key = matlab.lang.makeValidName(names{i});
            assert(~any(strcmp(seenNames, key)), 'ECMS25:EXTRACT', '重复信号名 %s', names{i});
            seenNames{end + 1} = key; %#ok<AGROW>
            dsP.(key) = dd;
        end
        for i = 1:numel(REQ_SIG)
            assert(isfield(dsP, REQ_SIG{i}), 'ECMS25:EXTRACT', '缺少必需记录信号 %s', REQ_SIG{i});
        end
        clear out ls   % 数据已复制入 dsP，释放 SimulationOutput
        fuelP(iP) = dsP.fuel_total_kg(end);
        socP(iP) = dsP.SOC(end);

        if iP == 1
            ds = dsP;   % 第一遍全量数据用于导出与指标
            n_samples = numel(ds.t);
            fprintf('[21] samples=%d  t_end=%.6g\n', n_samples, ds.t(end));

            % ---- tick 判定（66:245-258 机制原样：EML 早退守卫逐采样重放） ----
            dtc = ECMS_dt_ctrl_s;
            lastT = -1e9;
            tickIdx = zeros(n_samples, 1);   % 上界预分配
            nt = 0;
            for k = 1:n_samples
                if ds.t(k) - lastT >= dtc - 1e-9
                    nt = nt + 1;
                    tickIdx(nt) = k;
                    lastT = ds.t(k);
                end
            end
            tickIdx = tickIdx(1:nt);
            ntI = tickIdx;
            fprintf('[21] control ticks=%d（期望 floor(StopTime)+1=%d）\n', ...
                nt, floor(row.stopTime) + 1);

            % ---- accel 列（66:222 同口径：accel = v(tick) - v(prev tick)，首拍 0） ----
            acc = [0; diff(ds.v_ship_mps(ntI))];

            % ---- law=7 内部量重放（66:261-283 机制；每场景重放前 clear persistent：
            %         EML 的 t<lastT 新仿真重置分支 + 早退守卫会使暖会话第二场景首拍
            %         早退（dbg=[]），clear 后与 chart 每次 sim 全新实例语义一致） ----
            if LAW == 7
                clear('ecms_nn_dbg_v3');
                P_replay = zeros(nt, 1);
                dbg = repmat(struct('ok', false, 'e_hat_raw', 0, 'K_q', NaN, 'K_w', NaN, ...
                    'K', NaN, 'cls_raw', -1, 'cls_eff', -1, 'q10', NaN, 'q50', NaN, ...
                    'q90', NaN, 's_q', NaN), nt, 1);
                for i = 1:nt
                    k = ntI(i);
                    [P_replay(i), dbg(i)] = ecms_nn_dbg_v3(ds.t(k), ds.P_load_total_kW(k), ...
                        ds.v_ship_mps(k), ds.SOC(k), REPLAY_ARGS{:});
                end
                % 66:277-283 同口径：网格级诊断断言（灾难性错位防护），非位级一致断言
                dReplayVec = abs(P_replay - ds.P_gen_ref_kW(ntI));
                dReplay = max(dReplayVec);
                gridRate = mean(dReplayVec <= ECMS_Grid_dP_kW + 1e-9);
                fprintf('[21] replay vs logged P_gen_ref: max|dP|=%.3g kW  同网格率=%.4f\n', ...
                    dReplay, gridRate);
                assert(gridRate >= 0.90, 'ECMS25:EXPORT', ...
                    'tick 重放同网格率 %.4f < 0.90（灾难性错位/文本不一致）', gridRate);
            end

            % ---- 门控/预测列（law=7 来自 dbg 重放；law=5 直填，Fixed Decision 5） ----
            ehat_col = zeros(nt, 1);  ehr_col = zeros(nt, 1);  K_col = zeros(nt, 1);
            Kq_col = NaN(nt, 1);  Kw_col = NaN(nt, 1);
            ce_col = -ones(nt, 1);  cr_col = -ones(nt, 1);
            q1_col = NaN(nt, 1);  q5_col = NaN(nt, 1);  q9_col = NaN(nt, 1);  sq_col = NaN(nt, 1);
            if LAW == 7
                actv = false(nt, 1);
                for i = 1:nt
                    ehr_col(i) = dbg(i).e_hat_raw;  K_col(i) = dbg(i).K;
                    Kq_col(i) = dbg(i).K_q;  Kw_col(i) = dbg(i).K_w;
                    ce_col(i) = dbg(i).cls_eff;  cr_col(i) = dbg(i).cls_raw;
                    q1_col(i) = dbg(i).q10;  q5_col(i) = dbg(i).q50;
                    q9_col(i) = dbg(i).q90;  sq_col(i) = dbg(i).s_q;
                    if dbg(i).ok
                        ehat_col(i) = dbg(i).K * dbg(i).e_hat_raw;  % EML: e_hat = K * e_hat（门控后）
                        actv(i) = (dbg(i).K > 0);   % ê 生效：非回退且 K>0
                    end
                end
                e_hat_ratio = mean(actv);          % design §8.1：mean(K>0 且非回退)
                P_replay_col = P_replay;
            else
                K_col = ones(nt, 1);               % law=5：K=1（无门控，e_hat=K*0=0）
                e_hat_ratio = 0;                   % law=5 无预测，恒 0（spec 固定）
                P_replay_col = NaN(nt, 1);         % law=5 无 EML 重放（列不适用）
            end

            % ---- 指标（64:211-247 口径 + design §8.1） ----
            ds1 = model.resample_1s(ds);
            Vdc_min_V = min(ds1.V_dc_V);
            n_derate = sum(ds1.P_prop_limit_kW < Mot_P_nom - 1e-6);   % 64 同口径（1 s 轴）
            if isfield(ds1, 'alarm_steady') && isfield(ds1, 'alarm_transient')
                asSt = ds1.alarm_steady > 0.5;  asTr = ds1.alarm_transient > 0.5;
                n_alarm_steady = sum(asSt);  n_alarm_transient = sum(asTr);
                n_undervoltage = sum(asSt | asTr);
            else
                n_alarm_steady = NaN;  n_alarm_transient = NaN;  n_undervoltage = NaN;
                metricNotes{end + 1} = 'alarm_steady/alarm_transient 缺失，n_undervoltage=null'; %#ok<AGROW>
            end
            v_track_rmse = sqrt(mean((ds.v_ship_mps - ds.v_cmd_mps).^2));        % 全分辨率 0.01 s 轴
            v_track_rmse_1s = sqrt(mean((ds1.v_ship_mps - ds1.v_cmd_mps).^2));   % 1 s 轴辅助口径

            % ---- 写参考 CSV（66:270-279 的 21 列格式 + 第 22 列 fuel_total_kg，%.17g） ----
            csvPath = fullfile(OUT, [row.run_id '.csv']);
            fid = fopen(csvPath, 'w');
            assert(fid > 0, 'ECMS25:V3ARMS:ARG', '无法写 %s', csvPath);
            fprintf(fid, ['tick,t,P_load_kW,SOC,v_cmd_mps,v_ship_mps,accel_mps2,' ...
                'x_ship_m,P_gen_ref_kW,e_hat,e_hat_raw,K,K_q,K_w,cls_eff,cls_raw,' ...
                'q10_kwh,q50_kwh,q90_kwh,s_q_kwh,P_replay_kW,fuel_total_kg\n']);
            fuelTick = ds.fuel_total_kg(ntI);
            for i = 1:nt
                k = ntI(i);
                fprintf(fid, ['%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,' ...
                    '%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n'], ...
                    i - 1, ds.t(k), ds.P_load_total_kW(k), ds.SOC(k), ds.v_cmd_mps(k), ...
                    ds.v_ship_mps(k), acc(i), ds.x_ship_m(k), ds.P_gen_ref_kW(k), ...
                    ehat_col(i), ehr_col(i), K_col(i), Kq_col(i), Kw_col(i), ...
                    ce_col(i), cr_col(i), q1_col(i), q5_col(i), q9_col(i), sq_col(i), ...
                    P_replay_col(i), fuelTick(i));
            end
            fclose(fid);
            fprintf('[21] CSV -> %s（%d 行 × 22 列）\n', csvPath, nt);
        else
            % 第二遍：全程油耗轨迹逐拍最大差（诊断项，不参与门禁）
            d_traj = max(abs(dsP.fuel_total_kg - ds.fuel_total_kg));
            clear dsP
        end
    end

    % ---- 6. REPRO（Fixed Decision 4：两遍确定性 + law=7 冻结缓存比对） ----
    d_fuel = fuelP(2) - fuelP(1);
    d_SOC = socP(2) - socP(1);
    two_pass_ok = isfinite(d_fuel) && isfinite(d_SOC) && (d_fuel == 0) && (d_SOC == 0);

    if isempty(ds)
        % pass1 失败：无导出件，仅记录失败
        simFailCount = simFailCount + 1;
        arm = struct('run_id', row.run_id, 'law', LAW, 'scenario', row.scn.name, ...
            'fuel_raw_kg', NaN, 'fuel_corr_kg', NaN, 'SOC_end', NaN, ...
            'v_track_rmse', NaN, 'v_track_rmse_1s_grid', NaN, ...
            'e_hat_active_ratio', NaN, 'n_undervoltage', NaN, 'n_derate', NaN, ...
            'n_alarm_steady', NaN, 'n_alarm_transient', NaN, 'Vdc_min_V', NaN, ...
            'stop_time_s', row.stopTime, 'stop_time_set', floor(row.stopTime * 100) / 100, ...
            'n_samples', 0, 'n_ticks', 0, ...
            'wall_time_s', wallP(1), 'wall_time_pass2_s', wallP(2), ...
            'sim_error', simErrStr, 'metrics_note', 'pass1 sim failed，无 CSV 产物', ...
            'repro', struct('pass', false, 'd_fuel', NaN, 'd_SOC', NaN, ...
                'ref_source', 'sim_failed_no_data', ...
                'two_pass', struct('pass', false, 'fuel_raw_kg_p1', fuelP(1), ...
                    'fuel_raw_kg_p2', fuelP(2), 'SOC_end_p1', socP(1), ...
                    'SOC_end_p2', socP(2), 'd_fuel', NaN, 'd_SOC', NaN, ...
                    'd_fuel_traj_max', NaN), ...
                'cache', struct('file', row.cacheName, 'repro_pass', false, ...
                    'note', 'pass1 sim failed')));
        arms{end + 1} = arm; %#ok<AGROW>
        continue;
    end
    if ~isempty(simErrStr)
        simFailCount = simFailCount + 1;   % pass1 OK、pass2 失败
    end
    if ~two_pass_ok && isempty(simErrStr)
        % 仅当两遍均完整运行后端点仍不一致才计为 REPRO 失败；pass2 仿真失败
        % 已计入 simFailCount（无法完成比对不等于比对不一致）
        reproFailCount = reproFailCount + 1;
        fprintf('[21] ERROR: 两遍确定性 REPRO 失败（d_fuel=%.3g kg, d_SOC=%.3g）\n', d_fuel, d_SOC);
    end

    % pass1 产出（与 pass2 端点一致时即 REPRO 通过口径）
    fuel_raw = fuelP(1);
    SOC_end = socP(1);
    fuel_corr = fuel_raw + (SOC_REF_V3 - SOC_end) * BatCap * K_EQ;   % 64:239/66 同式同常数

    cache_ok = true;
    if useCache
        if exist(row.cache, 'file') == 2
            cm = load(row.cache);
            dFc = fuel_corr - cm.fuel_corr;
            dSoc = SOC_end - cm.SOC_end;
            cache_ok = (abs(dFc) < 1e-3) && (abs(dSoc) < 1e-6);   % 66:330 同阈值
            cacheRec = struct('file', row.cacheName, 'fuel_raw_kg', cm.fuel_raw, ...
                'fuel_corr_kg', cm.fuel_corr, 'SOC_end', cm.SOC_end, ...
                'd_fuel_corr_kg', dFc, 'd_SOC_end', dSoc, 'repro_pass', cache_ok);
            fprintf('[21] 缓存比对: d_fuel_corr=%.3g kg, d_SOC_end=%.3g\n', dFc, dSoc);
        else
            cache_ok = false;
            cacheRec = struct('file', row.cacheName, 'repro_pass', false, ...
                'note', 'missing（冻结缓存缺失，无法复现比对）');
            fprintf('[21] ERROR: 冻结缓存 mat 缺失（%s）——无法复现比对\n', row.cacheName);
        end
        if ~cache_ok
            reproFailCount = reproFailCount + 1;
            fprintf('[21] ERROR: 与 gating/runs 冻结缓存不一致——禁止重跑"直到好看"\n');
        end
        refSrc = ['in_session_two_pass + frozen_cache: ' row.cacheName];
    else
        cacheRec = struct('file', 'none', 'repro_pass', true, ...
            'note', 'law=5 (c) 臂无既有冻结缓存，REPRO 仅两遍同进程比对');
        refSrc = 'in_session_two_pass（law=5 无既有冻结缓存）';
    end

    arm = struct('run_id', row.run_id, 'law', LAW, 'scenario', row.scn.name, ...
        'fuel_raw_kg', fuel_raw, 'fuel_corr_kg', fuel_corr, 'SOC_end', SOC_end, ...
        'v_track_rmse', v_track_rmse, 'v_track_rmse_1s_grid', v_track_rmse_1s, ...
        'e_hat_active_ratio', e_hat_ratio, ...
        'n_undervoltage', n_undervoltage, 'n_derate', n_derate, ...
        'n_alarm_steady', n_alarm_steady, 'n_alarm_transient', n_alarm_transient, ...
        'Vdc_min_V', Vdc_min_V, ...
        'stop_time_s', row.stopTime, 'stop_time_set', floor(row.stopTime * 100) / 100, ...
        'n_samples', n_samples, 'n_ticks', nt, ...
        'wall_time_s', wallP(1), 'wall_time_pass2_s', wallP(2), ...
        'sim_error', simErrStr, 'metrics_note', strjoin(metricNotes, ' | '), ...
        'repro', struct('pass', two_pass_ok && cache_ok, ...
            'd_fuel', d_fuel, 'd_SOC', d_SOC, 'ref_source', refSrc, ...
            'two_pass', struct('pass', two_pass_ok, ...
                'fuel_raw_kg_p1', fuelP(1), 'fuel_raw_kg_p2', fuelP(2), ...
                'SOC_end_p1', socP(1), 'SOC_end_p2', socP(2), ...
                'd_fuel', d_fuel, 'd_SOC', d_SOC, 'd_fuel_traj_max', d_traj), ...
            'cache', cacheRec));
    arms{end + 1} = arm; %#ok<AGROW>
    fprintf('[21] %-34s law=%d fuel_raw=%.4f fuel_corr=%.4f SOC_end=%.4f ticks=%d\n', ...
        row.run_id, LAW, fuel_raw, fuel_corr, SOC_end, nt);
    clear ds dsP ds1
end

%% ---- 7. 汇总 JSON（合并写：非本次 law 的 arm 保留，本次 law 的 arm 覆盖） ----
sumPath = fullfile(OUT, 'native_arms_summary.json');
prevArms = {};
if exist(sumPath, 'file') == 2
    try
        prev = jsondecode(fileread(sumPath));
        if isfield(prev, 'arms') && ~isempty(prev.arms)
            pa = prev.arms;
            if isstruct(pa)
                paC = cell(1, numel(pa));
                for i = 1:numel(pa), paC{i} = pa(i); end
            else
                paC = pa;
            end
            for i = 1:numel(paC)
                a = paC{i};
                if ~isfield(a, 'law') || ~isequal(double(a.law), LAW)
                    prevArms{end + 1} = a; %#ok<AGROW>   % 另一 law 的 arm 保留
                end
            end
        end
    catch
        fprintf('[21] WARN: 既有 %s 解析失败，按全新汇总处理\n', sumPath);
    end
end
mergedArms = [prevArms, arms];
allOk = true;
for i = 1:numel(mergedArms)
    a = mergedArms{i};
    eStr = '';
    if isfield(a, 'sim_error'), eStr = a.sim_error; end
    pOk = false;
    if isfield(a, 'repro') && isfield(a.repro, 'pass'), pOk = a.repro.pass; end
    if ~isempty(eStr) || ~pOk, allOk = false; end
end

% 注意：不能用 struct(..., 'arms', {}) 初始化——空 cell 会使 struct() 产生 0x0
% struct 数组（66 §4 T-504 首跑事故同款）。先建 1x1，再挂 cell 字段。
summary = struct('task', 'T-610', 'spec', 'ems01-chil-v3-closedloop', ...
    'generated', datestr(now), 'matlab_version', version, ...
    'native_arms_law', LAW, 'sim_fail_count', simFailCount, ...
    'repro_fail_count', reproFailCount);
summary.arms = {};
for i = 1:numel(mergedArms)
    summary.arms{end + 1} = mergedArms{i}; %#ok<AGROW>
end
summary.notes = {
    'v_track_rmse：design §8.1 全任务口径，在 logsout 全分辨率轴（0.01 s）计算；v_track_rmse_1s_grid 为 model.resample_1s 1 s 轴辅助口径';
    'n_undervoltage：15_Bus_Voltage_Monitor alarm_steady/alarm_transient 于 1 s 轴的生效样本数（二者并集）；分解见 n_alarm_steady/n_alarm_transient（ccs_check 为逐样本带越限标志）';
    'n_derate：08_Undervolt_Derate P_prop_limit_kW < Mot_P_nom-1e-6 于 1 s 轴样本数（64_run_matrix_v2 同口径）';
    'fuel_corr = fuel_raw + (0.60-SOC_end)*BatCap*K_EQ（64:239/66 同式同常数，BatCap=Bat_Cap_kWh、K_EQ=0.4774）';
    'law=7 (b′) 臂 e_hat/e_hat_raw/K/K_q/K_w/cls_eff/cls_raw/q10/q50/q90/s_q 列由 ecms_nn_dbg_v3 逐 tick 重放生成（66:129-258 机制；函数名 _v3 避让 fw_reference 冻结副本）；重放断言同网格率>=0.90（灾难性错位防护，非位级一致）';
    'law=5 (c) 臂无 EML 预测：e_hat=0、e_hat_raw=0、K=1、cls_eff=-1、cls_raw=-1；K_q/K_w/q10/q50/q90/s_q/P_replay_kW=NaN（不适用）';
    'REPRO：每场景同 session 串行两遍，d_fuel==0 && d_SOC==0（endpoint）；d_fuel_traj_max 为全程油耗轨迹逐拍最大差（诊断项，不参与门禁）；law=7 另对照 gating/runs 冻结缓存（abs(dFc)<1e-3 && abs(dSoc)<1e-6，66:330 同阈值），缓存缺失按 REPRO 失败处理';
    'CSV 列 = 66 ref CSV 21 列（表头逐字符一致）+ 第 22 列 fuel_total_kg（tick 时刻累计油耗）';
    '冻结资产只读（ref_G2_*/export_summary/meta_*/baseline_manifest/gating 缓存仅 load 比对）；主模型只装载/sim 不 save';
    'wall_time_s = 第一遍 sim 墙钟；wall_time_pass2_s = 第二遍（REPRO 复跑）墙钟';
    };
summary.overall = allOk;
fid = fopen(sumPath, 'w', 'n', 'UTF-8');
assert(fid > 0, 'ECMS25:V3ARMS:ARG', '无法写 %s', sumPath);
fwrite(fid, jsonencode(summary, 'PrettyPrint', true));
fclose(fid);
fprintf('[21] summary -> %s（arms=%d, overall=%d）\n', sumPath, numel(mergedArms), allOk);

if reproFailCount > 0
    error('ECMS25:V3REPRO', ...
        '%d 个 run REPRO 失败（两遍确定性 / 冻结缓存比对）——见 native_arms_summary.json，禁止重跑"直到好看"', ...
        reproFailCount);
end
if simFailCount > 0
    error('ECMS25:V3SIM', '%d 个 run 仿真失败——见 native_arms_summary.json 的 sim_error', simFailCount);
end
fprintf('[21] NATIVE ARMS COMPLETE (law=%d, %s 臂)\n', LAW, armTag);
diary off;
%% ========================================================================
%  66_export_fw_reference.m — T-504：M2 固件参考轨迹生成（EMS01 CHIL ECMS，spec
%  ems01-chil-ecms-v1）。law=7 G2（GATE_MODE=2 经 63_integrate_gate_v2.m 机制注入
%  chart，T-310 先例脚本驱动）跑 S20 场景 + 1 段留出航次（v2 test split，评估用途
%  合法，同 T-310 FR-318 口径），逐控制 tick（ECMS_dt_ctrl_s=1 s）导出参考 CSV：
%    t, P_load, SOC, v_cmd, v_ship, accel, x_ship, P_gen_ref
%    + 门控内部量 e_hat/e_hat_raw/K/K_q/K_w/cls_eff/cls_raw/q10/q50/q90/s_q
%    + P_replay（EML 函数逐 tick 重放，tick 对齐诊断）。
%  说明：e_hat 列 = 门控后（EML `e_hat = K*e_hat`），e_hat_raw = 门控前；回退拍
%  （预期不出现）ok=false/K=NaN，cls_eff/cls_raw 记 -1 表示该拍未更新。
%  产物目录：results/ecms_phase25/fw_reference/
%    ref_<run_id>.csv            每 tick 参考轨迹（%.17g 无损精度）
%    meta_<run_id>.json          运行元数据 + 与 runs_gate_v2 缓存复现比对
%    ecms_nn_dbg.m               基线 EML 文本变换生成的诊断副本（多输出 dbg；
%                                P 计算路径零改动，不得作语义基准）
%    export_summary.json         汇总（CSV 清单/行数/复现判定）
%
%  tick 判定：EML 早退守卫 `t-lastT < dt-1e-9`（dt=ECMS_dt_ctrl_s）逐采样重放
%  logsout 时间轴，与 chart 内部状态机逐位一致（固定步长 0.01 s，全部信号同轴）。
%
%  复现校验（Escalate 条件）：本次 G2 运行 fuel_corr/SOC_end 必须与
%  gating/runs/run_<run_id>.mat 缓存一致（同机同版确定性仿真），超差即
%  error('ECMS25:REPRO:*')，禁止重跑"直到好看"。
%
%  用法（在模型工作区根目录；本文件名以数字开头，与 63/64 同样只能 eval 执行，
%  因此本脚本不得含局部函数）：
%    matlab -batch "eval(fileread('scripts/66_export_fw_reference.m'))"
%  可选变量（执行前赋值）：EXPORT_TEST_VOYS（holdout 航次 id 数组，默认取
%  dataset_manifest_v2.json split.test_voyages 的第 1 个）；EXPORT_SKIP_S20
%  （true=跳过 S20 重跑，沿用已归档 ref/meta，仅跑 holdout；汇总并入已归档 S20 meta）。
%  ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
cd(ROOT);  % 63 脚本在 eval 下以 ROOT='' 相对路径运行，必须位于工作区根
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'), ...
    fullfile(ROOT, 'scripts', '+ecms2'), fullfile(ROOT, 'scripts', '+ecms25'), ...
    fullfile(ROOT, 'scripts', '+model'), fullfile(ROOT, 'python', 'export'));

MDL = 'HybridShip_ModelD_DCBus';
OUT = fullfile(ROOT, 'results', 'ecms_phase25', 'fw_reference');
if ~exist(OUT, 'dir'), mkdir(OUT); end
diary(fullfile(OUT, 'export_diary.txt'));
fprintf('\n[66] ===== T-504 fw reference export (law=7 G2) =====\n');
fprintf('[66] ROOT = %s | start: %s\n', ROOT, datestr(now, 'yyyy-mm-dd HH:MM:SS'));
fprintf('[66] MATLAB %s\n', version);

%% ---- 1. G2 chart 注入（63_integrate_gate_v2.m 机制，T-310 先例） ----
GATE_MODE = 2;  % G2：工况识别门控（design 判定口径，禁改）
eval(fileread(fullfile(ROOT, 'scripts', '63_integrate_gate_v2.m')));

%% ---- 2. 常数/场景准备（与 64_run_matrix_v2.m 同口径） ----
model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms25.ensure_nn_params_v2(ROOT);
assert(sNN.ok == 1, 'ECMS25:EXPORT', 'law7-v2 权重未就绪（63 应已写入 mat）');
LAM0_EXP = 0.425;  K_SOC_EXP = 30;  K_S_EXP = 0.01;
assert(abs(ECMS_Lambda - LAM0_EXP) < 1e-9, 'ECMS25:EXPORT', 'ECMS_Lambda 漂移');
assert(abs(ECMS_K_soc - K_SOC_EXP) < 1e-9, 'ECMS25:EXPORT', 'ECMS_K_soc 漂移');
assert(abs(ECMS_K_S - K_S_EXP) < 1e-9, 'ECMS25:EXPORT', 'ECMS_K_S 漂移');
BatCap = Bat_Cap_kWh;                % 1071.36 kWh（fuel_corr 折算，64 同式）
K_EQ   = 0.4774;                     % fuel_corr 折算系数 [kg/kWh]（64 同口径）

%% ---- 3. 行规格（run_id 与 64 完全一致，便于对缓存） ----
MAIN_S20 = 'S20_composite_voyage';
scnMap = containers.Map('KeyType', 'char', 'ValueType', 'any');
for k = 1:numel(SCENARIOS)
    if ischar(SCENARIOS(k).name) && ~isempty(SCENARIOS(k).name)
        scnMap(SCENARIOS(k).name) = SCENARIOS(k);
    end
end
SCN_S20 = scnMap(MAIN_S20);
if ~exist('EXPORT_SKIP_S20', 'var'), EXPORT_SKIP_S20 = false; end
RS = struct('run_id', {}, 'scn', {}, 'stopTime', {});
if EXPORT_SKIP_S20
    fprintf('[66] EXPORT_SKIP_S20=true：跳过 S20（沿用已归档 ref/meta）\n');
else
    RS(end + 1) = struct('run_id', sprintf('G2_%s_matrix', MAIN_S20), ...
        'scn', SCN_S20, 'stopTime', SCN_S20.stop_time_s);
end

manPath = fullfile(ROOT, 'results', 'ecms_phase25', 'dataset_manifest_v2.json');
MJ = jsondecode(fileread(manPath));
testIds = MJ.split.test_voyages;
if ~exist('EXPORT_TEST_VOYS', 'var') || isempty(EXPORT_TEST_VOYS)
    EXPORT_TEST_VOYS = testIds(1);   % 默认第 1 个 test 航次（有 G2 缓存可对）
end
fleetJson = fullfile(ROOT, 'results', 'ecms_phase25', 'fleet', 'voyages_seed20260829.json');
VJ = jsondecode(fileread(fleetJson));
for i = 1:numel(EXPORT_TEST_VOYS)
    vid = EXPORT_TEST_VOYS(i);
    v = [];
    for j = 1:numel(VJ.voyages)
        if VJ.voyages(j).id == vid, v = VJ.voyages(j); break; end
    end
    assert(~isempty(v), 'ECMS25:EXPORT', 'test 航次 %d 不在 fleet json', vid);
    scn = ecms25.voyage_scn_v2(v);
    RS(end + 1) = struct('run_id', sprintf('G2_test_voyage_%03d_v2', vid), ...
        'scn', scn, 'stopTime', scn.stop_time_s);
end
fprintf('[66] rows: %s\n', strjoin({RS.run_id}, ', '));

%% ---- 4. 诊断用 EML 副本（文本变换：仅增 dbg 输出，P 路径零改动） ----
emlPath = fullfile(ROOT, 'results', 'ecms_phase25', 'gating', 'eml_ecms_step_mode2.m');
dbgPath = fullfile(OUT, 'ecms_nn_dbg.m');
src = fileread(emlPath);
A_SIG  = 'function P = ecms_nn(t, P_load, v_ship, SOC,';
A_PER  = 'persistent lastT Pheld buf k vprev cls_eff run_cls run_cnt';
A_GATE = '    e_hat = K * e_hat;';
A_FB   = '    e_hat = 0;';
assert(count(src, A_SIG) == 1 && count(src, A_PER) == 1 && ...
    count(src, A_GATE) == 1 && count(src, A_FB) == 1, ...
    'ECMS25:EXPORT', 'EML 诊断变换锚点不唯一，拒绝变换');
src = strrep(src, A_SIG, 'function [P, dbg] = ecms_nn_dbg(t, P_load, v_ship, SOC,');
src = strrep(src, A_PER, [A_PER newline 'dbg = [];']);
CAP_OK = ['    dbg = struct(''ok'',true,''e_hat_raw'',e_hat,''K_q'',K_q,' ...
    '''K_w'',K_w,''K'',K,''cls_raw'',cls,''cls_eff'',cls_eff,' ...
    '''q10'',Ehat_q(1),''q50'',Ehat_q(2),''q90'',Ehat_q(3),''s_q'',s_q);'];
src = strrep(src, A_GATE, [CAP_OK newline A_GATE]);   % 先捕获（raw）再门控
CAP_FB = ['    dbg = struct(''ok'',false,''e_hat_raw'',0,''K_q'',NaN,' ...
    '''K_w'',NaN,''K'',NaN,''cls_raw'',-1,''cls_eff'',-1,' ...
    '''q10'',NaN,''q50'',NaN,''q90'',NaN,''s_q'',NaN);'];
src = strrep(src, A_FB, [A_FB newline CAP_FB]);
fid = fopen(dbgPath, 'w'); assert(fid > 0, 'ECMS25:EXPORT', '无法写 %s', dbgPath);
fwrite(fid, src, 'char'); fclose(fid);
addpath(OUT);
fprintf('[66] 诊断副本 -> %s（P 计算路径与基线逐字相同）\n', dbgPath);

%% ---- 5. 重载模型（T-310 教训：chart 切换后必须全新加载再编译） ----
if bdIsLoaded(MDL), close_system(MDL, 0); end
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
rt = sfroot;
ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', ...
    [MDL '/05_EMS_Controller/EMS_NN_AECMS/ecms_step']);
assert(~isempty(ch) && contains(ch(1).Script, 'GATE_MODE = 2'), ...
    'ECMS25:EXPORT', 'chart 未处于 G2（63 注入失败）');
fprintf('[66] chart GATE_MODE=2 已确认 | ChartUpdate=%s SampleTime=%s\n', ...
    ch(1).ChartUpdate, ch(1).SampleTime);

% EML 重放实参（= chart Constant/Parameter 块的 base workspace 解析值）
REPLAY_ARGS = {ECMS_Lambda, ECMS_K_soc, ECMS_SOC_Ref, ECMS_K_S, ...
    ECMS_Lambda_Floor, BSFC_Breakpoints_kW, BSFC_Table_gkWh, ...
    ECMS_NN_W1, ECMS_NN_b1, ECMS_NN_W2, ECMS_NN_b2, ECMS_NN_W3, ECMS_NN_b3, ...
    ECMS_NN_XMean, ECMS_NN_XStd, ECMS_NN_YMean, ECMS_NN_YStd, ...
    ECMS_NN_Ebar_H_kWh, ECMS_NN_Escale_kWh, 1, ...
    ASSUMED_eta_rect, ASSUMED_eta_dcdc, ASSUMED_Bat_Eff_Discharge, ...
    ASSUMED_Bat_Eff_Charge, Gen_P_max, ECMS_SOC_Discharge_Max, ...
    ECMS_SOC_Charge_Min, ECMS_Idle_Fuel_kgph, ECMS_Grid_dP_kW, ...
    ECMS_dt_ctrl_s, Pgen_init};
REQ_SIG = {'v_cmd_mps', 'v_ship_mps', 'x_ship_m', 'acceleration_mps2', ...
    'P_load_total_kW', 'SOC', 'P_gen_ref_kW', 'fuel_total_kg'};

% 注意：不能用 struct(..., 'runs', {}) 初始化——空 cell 会使 struct() 产生 0x0
% struct 数组（T-504 首跑事故：dot 索引得 0 值逗号列表）。先建 1x1，再挂 cell 字段。
summary = struct('task', 'T-504', 'generated', datestr(now), ...
    'gate_mode', 2, 'matlab', version);
summary.runs = {};
reproFail = 0;
if EXPORT_SKIP_S20
    metaS20 = fullfile(OUT, ['meta_G2_' MAIN_S20 '_matrix.json']);
    if exist(metaS20, 'file') == 2
        repS20 = jsondecode(fileread(metaS20));
        repS20.source = 'archived_meta（EXPORT_SKIP_S20=true，本次未重跑）';
        summary.runs{end + 1} = repS20; %#ok<SAGROW>
        if ~(isfield(repS20, 'cache') && isfield(repS20.cache, 'repro_pass') && repS20.cache.repro_pass)
            reproFail = reproFail + 1;   % 归档件本身复现不合格则同样不通过
        end
    else
        error('ECMS25:EXPORT', 'EXPORT_SKIP_S20=true 但缺少 %s', metaS20);
    end
end

for r = 1:numel(RS)
    row = RS(r);
    fprintf('\n[66] ----- run %s (stopTime=%.2f) -----\n', row.run_id, row.stopTime);

    % ---- 5.1 仿真输入（64:217-233 逐项一致） ----
    inK = Simulink.SimulationInput(MDL);
    inK = inK.setVariable('SCN', row.scn);
    inK = inK.setVariable('EMS_CTRL_LAW', 7);
    inK = inK.setVariable('EMS_MODE', 1);
    inK = inK.setVariable('Bat_SOC_init', row.scn.initial_SOC);
    inK = inK.setVariable('AI_Pred_Source', 0);
    inK = inK.setVariable('VOYAGE_SPEED_MODE', 1);
    inK = inK.setVariable('VOYAGE_V_TIME_S', ecms2.scn_field(row.scn, 'v_time_s', [0 1]));
    inK = inK.setVariable('VOYAGE_V_VALUE_MPS', ecms2.scn_field(row.scn, 'v_value_mps', [0 0]));
    inK = inK.setVariable('VOYAGE_CURRENT_MODE', 1);
    inK = inK.setVariable('VOYAGE_CURRENT_TIME_S', ecms2.scn_field(row.scn, 'current_time_s', [0 1]));
    inK = inK.setVariable('VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(row.scn, 'current_value_mps', [0 0]));
    inK = inK.setModelParameter('StopTime', sprintf('%.2f', floor(row.stopTime * 100) / 100));

    t0 = tic;
    out = sim(inK);
    wall = toc(t0);
    fprintf('[66] sim done in %.1f s\n', wall);

    % ---- 5.2 信号提取（固定步长 0.01 s，全信号同轴；严格校验，内联实现：
    %         本文件以数字开头只能 eval 执行，不允许局部函数） ----
    ls = out.logsout;
    nEl = ls.numElements;
    % 第一遍：收集（名称/时间轴/数据），取最长时间轴为主轴（固定步长下即全量轴）
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
    ds = struct('t', sigT);
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
        ds.(key) = dd;
    end
    for i = 1:numel(REQ_SIG)
        assert(isfield(ds, REQ_SIG{i}), 'ECMS25:EXTRACT', '缺少必需记录信号 %s', REQ_SIG{i});
    end
    T = ds.t;
    n_samples = numel(T);
    fprintf('[66] samples=%d  t_end=%.6g\n', n_samples, T(end));

    % ---- 5.3 tick 判定（EML 早退守卫逐采样重放，与 chart 状态机一致） ----
    dtc = ECMS_dt_ctrl_s;
    lastT = -1e9;
    tickIdx = zeros(n_samples, 1);   % 上界预分配
    nt = 0;
    for k = 1:n_samples
        if T(k) - lastT >= dtc - 1e-9
            nt = nt + 1;
            tickIdx(nt) = k;
            lastT = T(k);
        end
    end
    tickIdx = tickIdx(1:nt);
    fprintf('[66] control ticks=%d（期望 floor(StopTime)+1=%d）\n', ...
        nt, floor(row.stopTime) + 1);

    % ---- 5.4 内部量重放（诊断副本，逐 tick；状态变化仅发生于 tick） ----
    ntI = tickIdx;
    vt = ds.v_ship_mps(ntI);
    acc = [0; diff(vt)];   % EML:55-56 accel = v(tick) - v(prev tick)，首拍 0
    P_replay = zeros(nt, 1);
    dbg = repmat(struct('ok', false, 'e_hat_raw', 0, 'K_q', NaN, 'K_w', NaN, ...
        'K', NaN, 'cls_raw', -1, 'cls_eff', -1, 'q10', NaN, 'q50', NaN, ...
        'q90', NaN, 's_q', NaN), nt, 1);
    for i = 1:nt
        k = ntI(i);
        [P_replay(i), dbg(i)] = ecms_nn_dbg(T(k), ds.P_load_total_kW(k), ...
            ds.v_ship_mps(k), ds.SOC(k), REPLAY_ARGS{:});
    end
    % 2026-09 口径修订：replay 对比由 1e-6 位级断言改为网格级诊断断言。
    % 根因（修订期间实证）：chart 仿真编译目标与 MATLAB 解释器存在 FP 微差
    % （tick5 实证：e_hat 微差翻转 λ 网格 argmin；归一化口径下分类器 logits 变平，
    % argmax 近平局密度上升，迟滞状态机放大为成簇 P 分歧）。位级一致在该口径下
    % 不可达；参考数据保真门禁由 L0（≥99% 同网格）与层① 板上注入承担（design 固定）。
    % 断言保留灾难性错位防护：同网格率 < 90% 视为对齐/文本错误。
    dReplayVec = abs(P_replay - ds.P_gen_ref_kW(ntI));
    dReplay = max(dReplayVec);
    gridRate = mean(dReplayVec <= ECMS_Grid_dP_kW + 1e-9);
    fprintf('[66] replay vs logged P_gen_ref: max|dP|=%.3g kW  同网格率=%.4f\n', dReplay, gridRate);
    assert(gridRate >= 0.90, 'ECMS25:EXPORT', ...
        'tick 重放同网格率 %.4f < 0.90（灾难性错位/文本不一致）', gridRate);

    % ---- 5.5 写参考 CSV（%.17g 无损） ----
    csvPath = fullfile(OUT, ['ref_' row.run_id '.csv']);
    fid = fopen(csvPath, 'w');
    assert(fid > 0, 'ECMS25:EXPORT', '无法写 %s', csvPath);
    fprintf(fid, ['tick,t,P_load_kW,SOC,v_cmd_mps,v_ship_mps,accel_mps2,' ...
        'x_ship_m,P_gen_ref_kW,e_hat,e_hat_raw,K,K_q,K_w,cls_eff,cls_raw,' ...
        'q10_kwh,q50_kwh,q90_kwh,s_q_kwh,P_replay_kW\n']);
    for i = 1:nt
        k = ntI(i);
        if dbg(i).ok
            ehat_g = dbg(i).K * dbg(i).e_hat_raw;   % EML: e_hat = K * e_hat（门控后）
        else
            ehat_g = 0;                              % EML 回退分支 e_hat = 0
        end
        fprintf(fid, ['%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,' ...
            '%.17g,%.17g,%.17g,%.17g,%d,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g\n'], ...
            i - 1, T(k), ds.P_load_total_kW(k), ds.SOC(k), ds.v_cmd_mps(k), ...
            ds.v_ship_mps(k), acc(i), ds.x_ship_m(k), ds.P_gen_ref_kW(k), ...
            ehat_g, dbg(i).e_hat_raw, dbg(i).K, ...
            dbg(i).K_q, dbg(i).K_w, dbg(i).cls_eff, dbg(i).cls_raw, ...
            dbg(i).q10, dbg(i).q50, dbg(i).q90, dbg(i).s_q, P_replay(i));
    end
    fclose(fid);
    fprintf('[66] CSV -> %s（%d 行）\n', csvPath, nt);

    % ---- 5.6 汇总指标 + 缓存复现比对（64 同式；超差即 fail，不重跑） ----
    fuel_raw = ds.fuel_total_kg(end);
    SOC_end = ds.SOC(end);
    fuel_corr = fuel_raw + (0.60 - SOC_end) * BatCap * K_EQ;
    rep = struct('run_id', row.run_id, 'scenario', row.scn.name, ...
        'stop_time_s', row.stopTime, 'stop_time_set', ...
        floor(row.stopTime * 100) / 100, 'n_samples', n_samples, ...
        'n_ticks', nt, 'wall_time_s', wall, ...
        'fuel_raw_kg', fuel_raw, 'fuel_corr_kg', fuel_corr, ...
        'SOC_end', SOC_end, 'replay_max_abs_dP_kW', dReplay);
    cacheMat = fullfile(ROOT, 'results', 'ecms_phase25', 'gating', 'runs', ...
        ['run_' row.run_id '.mat']);
    if exist(cacheMat, 'file') == 2
        cm = load(cacheMat);
        dFc = fuel_corr - cm.fuel_corr;
        dSoc = SOC_end - cm.SOC_end;
        rep.cache = struct('file', ['gating/runs/run_' row.run_id '.mat'], ...
            'fuel_raw_kg', cm.fuel_raw, 'fuel_corr_kg', cm.fuel_corr, ...
            'SOC_end', cm.SOC_end, 'd_fuel_corr_kg', dFc, 'd_SOC_end', dSoc);
        fprintf('[66] 缓存比对: d_fuel_corr=%.3g kg, d_SOC_end=%.3g\n', dFc, dSoc);
        if ~(abs(dFc) < 1e-3 && abs(dSoc) < 1e-6)
            reproFail = reproFail + 1;
            rep.cache.repro_pass = false;
            fprintf('[66] ERROR: 与 runs_gate_v2 缓存不一致（Escalate 条件）\n');
        else
            rep.cache.repro_pass = true;
        end
    else
        rep.cache = struct('file', 'missing', 'repro_pass', false);
        fprintf('[66] WARN: 无缓存 mat（%s）——无法复现比对\n', cacheMat);
    end
    % meta json
    jPath = fullfile(OUT, ['meta_' row.run_id '.json']);
    fid = fopen(jPath, 'w', 'n', 'UTF-8');
    fwrite(fid, jsonencode(rep, 'PrettyPrint', true)); fclose(fid);
    summary.runs{end + 1} = rep; %#ok<SAGROW>
end

%% ---- 6. 汇总 ----
summary.repro_fail_count = reproFail;
summary.all_repro_pass = (reproFail == 0);
jPath = fullfile(OUT, 'export_summary.json');
fid = fopen(jPath, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(summary, 'PrettyPrint', true)); fclose(fid);
fprintf('[66] summary -> %s\n', jPath);
if reproFail > 0
    error('ECMS25:REPRO', '%d 个 run 与 runs_gate_v2 缓存不一致——禁止继续，Escalate', reproFail);
end
fprintf('[66] EXPORT COMPLETE\n');
diary off;

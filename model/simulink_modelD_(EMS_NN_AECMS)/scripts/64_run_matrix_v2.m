%% ========================================================================
%  64_run_matrix_v2.m — T-310：门控闭环矩阵 v2（G0~G3 消融 + FR-318 holdout门禁 + capture）
%
%  矩阵：{G0,G1,G2,G3} × {S20,S01,S02,S07}（law=7 v2 门控变体，chart Script 按MODE切换）
%        + holdout {law=5 基线, law=7 v2} × test 航次前 10（FR-318 non-degradation）
%  前置：63_integrate_gate_v2.m（GATE_MODE=0 已应用；ECMS_NN_* v2 mat 就绪）
%  产出：results/ecms_phase25/gating/runs_gate_v2.csv + gate_result_v2.json
%
%  判定（T-309 §8）：capture = (fuel_C2 − fuel_C4_mode)/(fuel_C2 − fuel_C3)（S20）；
%   FR-318：节油差 median ≥ −0.5%、min ≥ −1.5%、零新增降额、零 SOC 越限。
%   G2/G3 相对 G1 增量规则：(a) S07 回退消除 (b) S20 capture 不显著损失 (c) holdout门禁通过。
%  执行：matlab -batch "eval(fileread('scripts/64_run_matrix_v2.m'))"
%  ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'), ...
    fullfile(ROOT, 'scripts', '+ecms25'), fullfile(ROOT, 'scripts', '+model'));

fprintf('\n[64] ===== T-310 gate closed-loop matrix v2 (G0-G3) =====\n');
diary(fullfile(ROOT, 'results', 'ecms_phase25', 'gating', 'matrix_diary.txt')); diary on;
fprintf('[64] ROOT = %s | start: %s\n', ROOT, datestr(now, 'yyyy-mm-dd HH:MM:SS'));

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms25.ensure_nn_params_v2(ROOT);
assert(sNN.ok == 1, 'ECMS25:MATRIX', 'law7-v2 权重未就绪（先运行 63）');

LAM0_EXP = 0.425;  K_SOC_EXP = 30;  K_S_EXP = 0.01;
assert(abs(ECMS_Lambda - LAM0_EXP) < 1e-9, 'ECMS25:MATRIX', 'ECMS_Lambda 漂移');
assert(abs(ECMS_K_soc - K_SOC_EXP) < 1e-9, 'ECMS25:MATRIX', 'ECMS_K_soc 漂移');
assert(abs(ECMS_K_S - K_S_EXP) < 1e-9, 'ECMS25:MATRIX', 'ECMS_K_S 漂移');
% 派生常数（与 53_run_phase2_matrix 同口径）
V_UV    = 600;                       % 欠压判定阈值 [V]
MotPnom = Mot_P_nom;                 % 700 kW
K_EQ    = 0.4774;                    % fuel_corr 折算系数 [kg/kWh]
SOC_REF = ECMS_SOC_Ref;              % 0.60
BatCap  = Bat_Cap_kWh;               % 1071.36 kWh


MAIN_SCNS = {'S20_composite_voyage', 'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'};
MODES = [0 1 2 3];   % G0..G3
MODE_NAME = {'G0', 'G1', 'G2', 'G3'};

outDir = fullfile(ROOT, 'results', 'ecms_phase25', 'gating');
if ~exist(outDir, 'dir'), mkdir(outDir); end
runsDir = fullfile(outDir, 'runs');
if ~exist(runsDir, 'dir'), mkdir(runsDir); end
csvPath = fullfile(outDir, 'runs_gate_v2.csv');

%% ---- test 航次（holdout ≥10）----
manPath = fullfile(ROOT, 'results', 'ecms_phase25', 'dataset_manifest_v2.json');
MJ = jsondecode(fileread(manPath));
testIds = MJ.split.test_voyages;
if numel(testIds) > 10, testIds = testIds(1:10); end
fprintf('[64] holdout voyages (first %d) : %s\n', numel(testIds), mat2str(testIds));

%% ---- 航次集 ----
fleetJson = fullfile(ROOT, 'results', 'ecms_phase25', 'fleet', 'voyages_seed20260829.json');
VJ = jsondecode(fileread(fleetJson));
idToV = containers.Map('KeyType', 'double', 'ValueType', 'any');
for i = 1:numel(VJ.voyages)
    idToV(VJ.voyages(i).id) = VJ.voyages(i);
end

%% ---- Phase 1 复用（C2 law5 / C3 law6 main fuel + fuel_dp）----
p1 = fullfile(ROOT, 'results', 'ecms_phase1', 'runs_phase1.csv');
assert(exist(p1, 'file') == 2, 'ECMS25:MATRIX', '缺少 Phase 1 runs_phase1.csv');
T1 = readtable(p1, 'VariableNamingRule', 'preserve');
fuelDp = containers.Map('KeyType', 'char', 'ValueType', 'double');
fuelC2 = containers.Map('KeyType', 'char', 'ValueType', 'double');
fuelC3 = containers.Map('KeyType', 'char', 'ValueType', 'double');
for i = 1:height(T1)
    sc = char(T1.scenario{i});
    if isKey(fuelDp, sc), fuelDp(sc) = min(fuelDp(sc), T1.fuel_dp_kg(i)); else, fuelDp(sc) = T1.fuel_dp_kg(i); end
    if strcmpi(char(T1.controller{i}), 'C2'), fuelC2(sc) = T1.fuel_corr_kg(i); end
    if strcmpi(char(T1.controller{i}), 'C3'), fuelC3(sc) = T1.fuel_corr_kg(i); end
end
for s = 1:numel(MAIN_SCNS)
    assert(isKey(fuelC2, MAIN_SCNS{s}) && isKey(fuelC3, MAIN_SCNS{s}), ...
        'ECMS25:MATRIX', 'Phase 1 缺 %s 的 C2/C3 行', MAIN_SCNS{s});
end

%% ---- 行规格 ----
RS = struct('run_id', {}, 'mode', {}, 'ctrl', {}, 'scn', {}, 'law', {}, ...
    'stopTime', {}, 'scnVal', {}, 'cacheMat', {}, 'cached', {});
mkRow = @(rid, mode, ctrl, scnName, law, stop, scnv) struct( ...
    'run_id', rid, 'mode', mode, 'ctrl', ctrl, 'scn', scnName, 'law', law, ...
    'stopTime', stop, 'scnVal', scnv, ...
    'cacheMat', fullfile(runsDir, ['run_' rid '.mat']), 'cached', false);

% law=5 基线（holdout，跨MODE复用，仅一次）
for i = 1:numel(testIds)
    vid = testIds(i);
    v = idToV(vid);
    scn = ecms25.voyage_scn_v2(v);
    RS(end + 1) = mkRow(sprintf('C2_test_voyage_%03d_v2', vid), -1, 'C2', v.name, ...
        5, scn.stop_time_s, scn); %#ok<SAGROW>
end
% 各MODE：main 4 行 + holdout law7 10 行
scnMap = containers.Map('KeyType', 'char', 'ValueType', 'any');
for k = 1:numel(SCENARIOS)
    if ischar(SCENARIOS(k).name) && ~isempty(SCENARIOS(k).name)
        scnMap(SCENARIOS(k).name) = SCENARIOS(k);
    end
end
for m = 1:numel(MODES)
    for s = 1:numel(MAIN_SCNS)
        scnName = MAIN_SCNS{s};
        SCN = scnMap(scnName);
        if strcmp(scnName, 'S20_composite_voyage')
            stopTimeLimit = SCN.stop_time_s;
        else
            stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
        end
        rid = sprintf('G%d_%s_matrix', MODES(m), scnName);
        RS(end + 1) = mkRow(rid, MODES(m), 'C4v2', scnName, 7, stopTimeLimit, SCN); %#ok<SAGROW>
    end
    for i = 1:numel(testIds)
        vid = testIds(i);
        v = idToV(vid);
        scn = ecms25.voyage_scn_v2(v);
        rid = sprintf('G%d_test_voyage_%03d_v2', MODES(m), vid);
        RS(end + 1) = mkRow(rid, MODES(m), 'C4v2', v.name, 7, scn.stop_time_s, scn); %#ok<SAGROW>
    end
end

%% ---- 缓存检查 ----
nSim = 0;
for k = 1:numel(RS)
    if exist(RS(k).cacheMat, 'file') == 2, RS(k).cached = true; else, nSim = nSim + 1; end
end
fprintf('[64] rows total %d | new %d | cached %d\n', numel(RS), nSim, numel(RS) - nSim);

%% ======================= 执行（按MODE分组；组间切换 chart Script） =======================
addpath('model');
load_system(fullfile(ROOT, 'model', 'HybridShip_ModelD_DCBus.slx'));
emlDir = fullfile(ROOT, 'results', 'ecms_phase25', 'gating');
metMap = containers.Map('KeyType', 'char', 'ValueType', 'any');
useParsim = false;   % 串行（eval 环境下 evalin 引号脆弱；parsim 留待优化）
if ~useParsim, fprintf('[64] MODEUSE_PARSIM  parsim \n'); end
for m = 1:numel(MODES)
    gm = MODES(m);
    fprintf('\n[64] ===== MODE %s (GATE_MODE=%d)=====\n', MODE_NAME{m}, gm);
    % 每MODE重新获取 chart 句柄（save_system 后旧句柄失效，T-310 事故教训）
    rt = sfroot;
    chAll = rt.find('-isa', 'Stateflow.EMChart');
    lawChart = [];
    for i = 1:numel(chAll)
        try
            if contains(chAll(i).Script, 'ECMS_NN') && contains(chAll(i).Path, 'EMS_NN_AECMS')
                lawChart = chAll(i); break;
            end
        catch
        end
    end
    assert(~isempty(lawChart), 'ECMS25:MATRIX', '未找到 law7 EML chart');
    emlFile = fullfile(emlDir, sprintf('eml_ecms_step_mode%d.m', gm));
    lawChart.Script = fileread(emlFile);
    save_system('HybridShip_ModelD_DCBus');
    % 原地切换后强制重新加载模型（chart 重编译需全新加载，T-310 教训）
    close_system('HybridShip_ModelD_DCBus', 0);
    load_system(fullfile(ROOT, 'model', 'HybridShip_ModelD_DCBus.slx'));
    fprintf('>>MARK chart-switch mode %d done (reloaded)\n', gm);
    fprintf('[64] chart Script  mode%d\n', gm);

    % 本MODE行
    idxs = [];
    for k = 1:numel(RS)
        if RS(k).mode == gm && ~RS(k).cached, idxs(end + 1) = k; end %#ok
    end
    % law=5 基线行（mode=-1）在首个MODE批次一并跑
    if m == 1
        for k = 1:numel(RS)
            if RS(k).mode == -1 && ~RS(k).cached, idxs(end + 1) = k; end %#ok
        end
    end
    if isempty(idxs)
        fprintf('[64] MODE\n'); continue;
    end
    fprintf('>>MARK sims to run in mode %d: %d\n', gm, numel(idxs));

    in = cell(numel(idxs), 1);
    for kk = 1:numel(idxs)
        r = RS(idxs(kk));
        inK = Simulink.SimulationInput('HybridShip_ModelD_DCBus');
        inK = inK.setVariable('SCN', r.scnVal);
        inK = inK.setVariable('EMS_CTRL_LAW', r.law);
        inK = inK.setVariable('EMS_MODE', double(r.law > 0));
        inK = inK.setVariable('Bat_SOC_init', r.scnVal.initial_SOC);
        inK = inK.setVariable('AI_Pred_Source', 0);
        inK = inK.setVariable('VOYAGE_SPEED_MODE', 1);
        inK = inK.setVariable('VOYAGE_V_TIME_S', ecms2.scn_field(r.scnVal, 'v_time_s', [0 1]));
        inK = inK.setVariable('VOYAGE_V_VALUE_MPS', ecms2.scn_field(r.scnVal, 'v_value_mps', [0 0]));
        inK = inK.setVariable('VOYAGE_CURRENT_MODE', 1);
        inK = inK.setVariable('VOYAGE_CURRENT_TIME_S', ecms2.scn_field(r.scnVal, 'current_time_s', [0 1]));
        inK = inK.setVariable('VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(r.scnVal, 'current_value_mps', [0 0]));
        inK = inK.setModelParameter('StopTime', sprintf('%.2f', floor(r.stopTime * 100) / 100));
        inK = inK.setUserString(r.run_id);
        in{kk} = inK; %#ok
    end

    outs = cell(numel(idxs), 1);
    tBatch = tic;
    if useParsim
        try
            simOuts = parsim(in, 'ShowProgress', 'on', ...
                'SetupFcn', @() ecms25.parsim_setup_v2(ROOT), ...
                'TransferBaseWorkspaceVariables', 'off');
            for kk = 1:numel(idxs), outs{kk} = simOuts(kk); end
            fprintf('[64] parsim done: %.1f s\n', toc(tBatch));
        catch errP
            fprintf('[64] parsim failed (%s-> serial fallback\n', errP.message);
            useParsim = false;
        end
    end
    if ~useParsim
        for kk = 1:numel(idxs)
            t0 = tic;
            try, outs{kk} = sim(in{kk}); catch errS, outs{kk} = errS; end
            fprintf('>>MARK serial %d/%d done\n', kk, numel(idxs));
        end
    end

    for kk = 1:numel(idxs)
        r = RS(idxs(kk));
        if isa(outs{kk}, 'MException')
            met = struct('run_id', r.run_id, 'mode', r.mode, 'controller', r.ctrl, ...
                'scenario', r.scn, 'law', r.law, 'fuel_raw', NaN, 'fuel_corr', NaN, ...
                'SOC_end', NaN, 'Vdc_min', NaN, 'n_uv', NaN, 'n_der', NaN, ...
                'bt', NaN, 'wall', NaN, 'sim_error', outs{kk}.message);
            fprintf('[64] sim fail %s: %s\n', r.run_id, outs{kk}.message);
        else
            ds0 = model.extract_logsout(outs{kk});
            ds1 = model.resample_1s(ds0);
            fuel_raw = ds1.fuel_total_kg(end);
            SOC_end = ds1.SOC(end);
            fuel_corr = fuel_raw + (0.60 - SOC_end) * BatCap * K_EQ;
            met = struct('run_id', r.run_id, 'mode', r.mode, 'controller', r.ctrl, ...
                'scenario', r.scn, 'law', r.law, 'fuel_raw', fuel_raw, ...
                'fuel_corr', fuel_corr, 'SOC_end', SOC_end, ...
                'Vdc_min', min(ds1.V_dc_V), 'n_uv', sum(ds1.V_dc_V < V_UV), ...
                'n_der', sum(ds1.P_prop_limit_kW < MotPnom - 1e-6), ...
                'bt', sum(abs(ds1.P_bat_actual_kW)) / 3600, 'wall', toc(t0), 'sim_error', '');
            save(r.cacheMat, '-struct', 'met');
            fprintf('[64] %-40s law=%d fuel_corr=%.4f SOC_end=%.4f n_der=%d\n', ...
                r.run_id, r.law, fuel_corr, SOC_end, met.n_der);
        end
        metMap(r.run_id) = met;
    end
end
% 缓存行补 metMap
for k = 1:numel(RS)
    if RS(k).cached && ~isKey(metMap, RS(k).run_id)
        metMap(RS(k).run_id) = load(RS(k).cacheMat);
    end
end

%% ======================= CSV =======================
HEADER = ['run_id,mode,controller,scenario,law,fuel_raw_kg,fuel_corr_kg,SOC_end,' ...
    'Vdc_min_V,n_undervoltage,n_derate,bat_throughput_kWh,wall_time_s,sim_error'];
csvPath = fullfile(outDir, 'runs_gate_v2.csv');
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
assert(fid > 0, 'ECMS25:MATRIX', '无法写 %s', csvPath);
fprintf(fid, '%s\n', HEADER);
for k = 1:numel(RS)
    m = metMap(RS(k).run_id);
    fprintf(fid, '%s,%d,%s,%s,%d,%.6g,%.6g,%.6g,%.6g,%d,%d,%.6g,%.6g,%s\n', ...
        m.run_id, m.mode, m.controller, m.scenario, m.law, m.fuel_raw, m.fuel_corr, ...
        m.SOC_end, m.Vdc_min, m.n_uv, m.n_der, m.bt, m.wall, m.sim_error);
end
fclose(fid);
fprintf('[64] CSV -> %s\n', csvPath);

%% ======================= 判定（capture + FR-318 + G2/G3 增量规则） =======================
res = struct('modes', {}, 'fr318', {});
modeSum = struct('mode', {}, 'name', {}, 'capture_S20', {}, 'fuel_S20_kg', {}, 'sim_errors', {});
fr318 = struct();
% FR-318（各MODE vs law=5 基线）
sav_med = []; sav_min = []; der_ok = []; uv_ok = []; soc_ok = [];
for m = 1:numel(MODES)
    gm = MODES(m);
    % capture（S20）
    f4 = metMap(sprintf('G%d_S20_composite_voyage_matrix', gm));
    f2 = fuelC2('S20_composite_voyage'); f3 = fuelC3('S20_composite_voyage');
    den = f2 - f3;
    if den > 0, cap = (f2 - f4.fuel_corr) / den; else, cap = NaN; end
    % FR-318
    diffs = []; d_ok = true; u_ok = true; s_ok = true;
    for i = 1:numel(testIds)
        vid = testIds(i);
        m5 = metMap(sprintf('C2_test_voyage_%03d_v2', vid));
        m7 = metMap(sprintf('G%d_test_voyage_%03d_v2', gm, vid));
        d_pct = (m5.fuel_corr - m7.fuel_corr) / m5.fuel_corr * 100;
        diffs(end + 1) = d_pct; %#ok
        if m7.n_der > m5.n_der, d_ok = false; end
        if m7.n_uv > m5.n_uv, u_ok = false; end
        if m7.SOC_end < 0 || m7.SOC_end > 1, s_ok = false; end
    end
    fr318.(MODE_NAME{m}) = struct('saving_pct_median', round(median(diffs), 4), ...
        'saving_pct_min', round(min(diffs), 4), ...
        'zero_new_derate', d_ok, 'zero_new_undervolt', u_ok, 'soc_ok', s_ok, ...
        'pass', median(diffs) >= -0.5 && min(diffs) >= -1.5 && d_ok && u_ok && s_ok);
    modeSum(end + 1) = struct('mode', gm, 'name', MODE_NAME{m}, ...
        'capture_S20', round(cap, 4), ...
        'fuel_S20_kg', round(f4.fuel_corr, 4), ...
        'sim_errors', 0); %#ok
end
% S07 回退消除检查（G MODE n_der vs G0）
s07_g0 = metMap('G0_S07_bridge_maneuver_matrix');
for m = 1:numel(MODES)
    m7 = metMap(sprintf('G%d_S07_bridge_maneuver_matrix', MODES(m)));
    modeSum(m).s07_derate = m7.n_der;
end
g0_der = s07_g0.n_der;
% G2/G3 增量规则（相对 G1）
g1 = fr318.G1;
for m = 1:numel(MODES)
    nm = MODE_NAME{m};
    modeSum(m).fr318_pass = fr318.(nm).pass;
end
g1_cap = -inf;
for m = 1:numel(MODES)
    if MODES(m) == 1, g1_cap = modeSum(m).capture_S20; end
end
rec_g2 = false; rec_g3 = false;
for m = 1:numel(MODES)
    if MODES(m) == 2
        rec_g2 = modeSum(m).s07_derate < g0_der && ...
            abs(modeSum(m).capture_S20 - g1_cap) <= 0.05 && fr318.G2.pass;
    end
    if MODES(m) == 3
        rec_g3 = modeSum(m).s07_derate < g0_der && ...
            abs(modeSum(m).capture_S20 - g1_cap) <= 0.05 && fr318.G3.pass;
    end
end

gate = struct();
gate.task = 'T-310';
gate.generated = datestr(now, 'yyyy-mm-dd HH:MM:SS');
gate.capture_formula = '(fuel_C2 - fuel_C4)/(fuel_C2 - fuel_C3) on S20';
gate.capture_thresholds = [0.50, 0.30];
gate.fr318_thresholds = 'saving median >= -0.5%, min >= -1.5%, zero new derate/undervolt';
gate.modes = modeSum;
gate.fr318 = fr318;
gate.g2_increment_rule = rec_g2;
gate.g3_increment_rule = rec_g3;
gate.winner_recommendation = 'PENDING_T310_REVIEW';
if rec_g3
    gate.winner_recommendation = 'G3 (quantile + condition gating)';
elseif rec_g2
    gate.winner_recommendation = 'G2 (condition gating, upstream only)';
else
    gate.winner_recommendation = 'G1 (quantile gating)';
end
jsonPath = fullfile(outDir, 'gate_result_v2.json');
fid = fopen(fullfile(outDir, 'gate_result_v2.json'), 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(gate, 'PrettyPrint', true));
fclose(fid);
fprintf('[64] gate_result_v2 -> %s\n', fullfile(outDir, 'gate_result_v2.json'));
fprintf('[64] : %s\n', gate.winner_recommendation);
fprintf('[64] MATRIX COMPLETE\n');
%% ========================================================================
%  53_run_phase2_matrix.m — ECMS Phase 2：C4 实验矩阵 + 泛化（T-205，FR-209，design §6）
%
%  矩阵结构（design §6 / FR-209）：
%    主矩阵 {C2,C3,C4} × {S20,S01,S02,S07}：
%      C2(law=5)/C3(law=6)/C0 复用 Phase 1 数值（results/ecms_phase1/runs_phase1.csv，
%      注明 source=phase1_reuse）；C4(law=7) 本脚本新跑闭环（source=phase2_sim）。
%    泛化 {C2,C4} × test 航次前 10 个（dataset_manifest.json split.test_voyages，
%      闭环新跑 20 行；FR-204：test 航次只跑一次，本脚本不支持对已完成航次重跑——
%      按 run_id 断点缓存，已完成行直接复用缓存 metrics，不重新仿真）。
%
%  每行指标：controller / scenario(voyage) / fuel_raw / fuel_corr（k_eq=0.4774，
%  SOC₀=0.60，fuel_corr = fuel_raw + (0.60−SOC_end)×BatCap×k_eq）/ SOC_end /
%  Vdc_min / 欠压+降额事件 / wall_time。
%
%  执行核心：SimulationInput.setVariable 快路径（与 51 逐位一致，law=0 已验证；
%  T-204 后 law=5/7 变体同机制：setVariable EMS_CTRL_LAW 驱动变体选择）。
%  law=7 的 NN 前向经 coder.extrinsic 调 ecms2.nn_predict_ehat（Amendment v1.2），
%  权重 mat（config/ecms_nn_weights.mat，52 产物）惰性加载，无需额外注入。
%  parsim 优先（SetupFcn 注入 config+BSFC+NN 参数），不可用串行降级。
%
%  产出（results/ecms_phase2/）：
%    runs_phase2.csv     汇总表（36 行 = 16 主矩阵 + 20 泛化；入库）
%    runs/run_<id>.mat   每新跑行 metrics 缓存（断点续跑；不入库）
%  完成判定：日志末尾 "[53] MATRIX COMPLETE n_ok=.. n_sim=.. n_reuse=.."。
%
%  执行（数字开头脚本须 eval(fileread) 方式运行；nohup 后台）：
%    cd model/simulink_modelD && nohup /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/53_run_phase2_matrix.m'))" \
%      > results/ecms_phase2/matrix_gen.log 2>&1 &
%  注：脚本无本地函数（eval(fileread) 不支持函数定义），全部逻辑内联。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[53] ===== ECMS Phase 2 — C4 实验矩阵 + 泛化（T-205，FR-209，design §6） =====\n');
fprintf('[53] ROOT = %s | 开始: %s\n', ROOT, datestr(now, 'yyyy-mm-dd HH:MM:SS'));

%% ======================= 环境与冻结参数校验（config 为唯一真源；不改任何参数） =======================
model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);
sNN = ecms2.ensure_nn_params(ROOT);
assert(sNN.ok == 1, 'ECMS2:MATRIX', 'law=7 权重未就绪（先运行 52_integrate_law7.m）');

LAM0_EXP = 0.425;  K_SOC_EXP = 30;  K_S_EXP = 0.01;   % 标定终值（T-104/105/106，不得重标定）
assert(abs(ECMS_Lambda - LAM0_EXP) < 1e-9, 'ECMS:MATRIX', ...
    'ECMS_Lambda=%.4f ≠ 冻结值 %.4f（config 漂移，Escalate）', ECMS_Lambda, LAM0_EXP);
assert(abs(ECMS_K_soc - K_SOC_EXP) < 1e-9, 'ECMS:MATRIX', ...
    'ECMS_K_soc=%.4f ≠ 冻结值 %.4f（config 漂移，Escalate）', ECMS_K_soc, K_SOC_EXP);
assert(abs(ECMS_K_S - K_S_EXP) < 1e-9, 'ECMS:MATRIX', ...
    'ECMS_K_S=%.4f ≠ 冻结值 %.4f（config 漂移，Escalate）', ECMS_K_S, K_S_EXP);

SOC_REF = ECMS_SOC_Ref;              % 0.60
K_EQ    = 0.4774;                    % fuel_corr 折算系数 [kg/kWh]（design §3.1 / kS_calibration.json）
BatCap  = Bat_Cap_kWh;               % 1071.36 kWh
MotPnom = Mot_P_nom;                 % 700 kW（降额判定基准）
V_UV    = 600;                       % 欠压判定阈值 [V]（design §5 / run_profile_sim 同口径）

MDL = 'HybridShip_ModelD_DCBus';
slxPath = fullfile(ROOT, 'model', [MDL '.slx']);
assert(exist(slxPath, 'file') == 2, 'ECMS:MATRIX', '缺少 %s（先运行 10_build_modelD.m）', slxPath);
if ~bdIsLoaded(MDL), load_system(slxPath); end

outDir = fullfile(ROOT, 'results', 'ecms_phase2');
runsDir = fullfile(outDir, 'runs');
if ~exist(runsDir, 'dir'), mkdir(runsDir); end
csvPath = fullfile(outDir, 'runs_phase2.csv');

%% ======================= config_hash（本批运行参数指纹） =======================
md5str  = @(s) sprintf('%02x', typecast(...
    java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
ovFile = fullfile(ROOT, 'config', 'ecms_bsfc_override.mat');
ovTag = 'default (ASSUMED_BSFC_*)';
if exist(ovFile, 'file')
    ovinfo = load(ovFile, 'override_tag');
    ovTag = ovinfo.override_tag;
end
cfgHash = md5str(sprintf(...
    'phase2_matrix_v1;lam=%.6g;ksoc=%.6g;ks=%.6g;floor=%.6g;ref=%.6g;keq=%.6g;cap=%.6g;Pmax=%.6g;dt=%.6g;Vuv=%.6g;bsfc=%s;nn=ecms_nn_weights.mat', ...
    ECMS_Lambda, ECMS_K_soc, ECMS_K_S, ECMS_Lambda_Floor, SOC_REF, K_EQ, ...
    BatCap, MotPnom, ECMS_dt_ctrl_s, V_UV, ovTag));

%% ======================= Phase 1 复用行（C0/C2/C3 × 主矩阵场景，source=phase1_reuse） =======================
p1csv = fullfile(ROOT, 'results', 'ecms_phase1', 'runs_phase1.csv');
assert(exist(p1csv, 'file') == 2, 'ECMS:MATRIX', '缺少 %s（先运行 42）', p1csv);
fid1 = fopen(p1csv, 'r', 'n', 'UTF-8');
assert(fid1 > 0, 'ECMS:MATRIX', '无法读取 %s', p1csv);
hdr1 = fgetl(fid1);   % 表头不复用
MAIN_SCNS = {'S20_composite_voyage', 'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'};
reuse = containers.Map();    % key: ctrl_scn
fuelDp = containers.Map();   % scn -> fuel_dp_kg（取各场景 C0 行）
l1 = fgetl(fid1);
while ischar(l1)
    f1 = strsplit(l1, ',', 'CollapseDelimiters', false);
    if numel(f1) == 16 && strcmp(f1{13}, 'matrix') && ...
            any(strcmp(f1{2}, {'C0', 'C2', 'C3'})) && any(strcmp(f1{3}, MAIN_SCNS))
        key = [f1{2} '_' f1{3}];
        reuse(key) = struct( ...
            'fuel_raw', str2double(f1{5}), 'fuel_corr', str2double(f1{6}), ...
            'SOC_end', str2double(f1{7}), 'Vdc_min', str2double(f1{8}), ...
            'n_uv', str2double(f1{9}), 'n_der', str2double(f1{10}), ...
            'bt', str2double(f1{11}), 'wall', str2double(f1{12}), ...
            'fuel_dp', str2double(f1{14}), 'cfg', f1{16});
        if strcmp(f1{2}, 'C0')
            fuelDp(f1{3}) = str2double(f1{14});
        end
    end
    l1 = fgetl(fid1);
end
fclose(fid1);
for s = 1:numel(MAIN_SCNS)
    for c = {'C0', 'C2', 'C3'}
        key = [c{1} '_' MAIN_SCNS{s}];
        assert(isKey(reuse, key), 'ECMS:MATRIX', 'Phase 1 复用行缺失: %s', key);
    end
    assert(isKey(fuelDp, MAIN_SCNS{s}), 'ECMS:MATRIX', 'Phase 1 缺 %s 的 fuel_dp（C0 行）', MAIN_SCNS{s});
end
fprintf('[53] Phase 1 复用：C0/C2/C3 × %d 场景（12 行）；S20 C2=%.4f kg / C3=%.4f kg（runs_phase1.csv）\n', ...
    numel(MAIN_SCNS), reuse('C2_S20_composite_voyage').fuel_corr, ...
    reuse('C3_S20_composite_voyage').fuel_corr);

%% ======================= test 航次名单（manifest split.test_voyages 前 10，FR-204） =======================
manPath = fullfile(outDir, 'dataset_manifest.json');
assert(exist(manPath, 'file') == 2, 'ECMS:MATRIX', '缺少 %s（先跑 python/build_dataset.py）', manPath);
man = jsondecode(fileread(manPath));
testIdsAll = man.split.test_voyages(:).';
assert(numel(testIdsAll) >= 10, 'ECMS:MATRIX', 'test_voyages 仅 %d 个（<10）', numel(testIdsAll));
testIds = testIdsAll(1:10);
fprintf('[53] test 航次（前 10）：%s\n', strjoin(arrayfun(@(x) sprintf('%d', x), testIds, 'UniformOutput', false), ', '));

seed = ECMS_FLEET_SEED;
fleetDir = fullfile(outDir, 'fleet');
jsonPath = fullfile(fleetDir, sprintf('voyages_seed%d.json', seed));
assert(exist(jsonPath, 'file') == 2, 'ECMS:MATRIX', '缺少航次定义 %s（先运行 50）', jsonPath);
vjson = jsondecode(fileread(jsonPath));
V = vjson.voyages;
idToV = containers.Map('KeyType', 'double', 'ValueType', 'any');
for k = 1:numel(V)
    idToV(V(k).id) = V(k);
end

%% ======================= 场景名 -> SCN（主矩阵） =======================
scnMap = containers.Map();
for k = 1:numel(SCENARIOS)
    if ischar(SCENARIOS(k).name) && ~isempty(SCENARIOS(k).name)
        scnMap(SCENARIOS(k).name) = SCENARIOS(k);
    end
end
LAW_OF = containers.Map({'C2', 'C4'}, {5, 7});   % C2=law5 SOC-Adaptive，C4=law7 NN-A-ECMS（design §6）

%% ======================= 行规格（新跑 24 行；已缓存行跳过不重跑） =======================
% 字段：run_id/ctrl/scn/law/scan/stopTime/scnVal/cacheMat/cached
RS = struct('run_id', {}, 'ctrl', {}, 'scn', {}, 'law', {}, 'scan', {}, ...
    'stopTime', {}, 'scnVal', {}, 'cacheMat', {}, 'cached', {});
mkRow = @(rid, ctrl, scnName, law, scan, stop, scnv) struct( ...
    'run_id', rid, 'ctrl', ctrl, 'scn', scnName, 'law', law, 'scan', scan, ...
    'stopTime', stop, 'scnVal', scnv, ...
    'cacheMat', fullfile(runsDir, ['run_' rid '.mat']), 'cached', false);

% ---- 主矩阵 C4（law=7）4 行 ----
for s = 1:numel(MAIN_SCNS)
    scnName = MAIN_SCNS{s};
    SCN = scnMap(scnName);
    if strcmp(scnName, 'S20_composite_voyage')
        stopTimeLimit = SCN.stop_time_s;
    else
        stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    end
    rid = sprintf('C4_%s_matrix', scnName);
    RS(end + 1) = mkRow(rid, 'C4', scnName, 7, 'matrix', stopTimeLimit, SCN); %#ok<SAGROW>
end

% ---- 泛化 {C2,C4} × test 前 10（20 行）----
for i = 1:numel(testIds)
    vid = testIds(i);
    assert(isKey(idToV, vid), 'ECMS:MATRIX', 'test 航次 %d 不在航次集', vid);
    v = idToV(vid);
    scn = ecms2.voyage_scn(v);
    for c = {'C2', 'C4'}
        ctrl = c{1};
        rid = sprintf('%s_test_voyage_%03d_gen', ctrl, vid);
        RS(end + 1) = mkRow(rid, ctrl, v.name, LAW_OF(ctrl), 'generalization', ...
            scn.stop_time_s, scn); %#ok<SAGROW>
    end
end

% ---- 缓存检查（FR-204：已完成 test 航次绝不重跑）----
nSim = 0;
for k = 1:numel(RS)
    if exist(RS(k).cacheMat, 'file') == 2
        RS(k).cached = true;
    else
        nSim = nSim + 1;
    end
end
fprintf('[53] 新跑 %d 行（主矩阵 C4×4 + 泛化 %d），缓存跳过 %d 行\n', nSim, numel(RS) - 4, numel(RS) - nSim);

%% ======================= 构建 SimulationInput（仅未缓存行；VOYAGE_* 注入，51 快路径同式） =======================
todoIdx = find(~[RS.cached]);
in = Simulink.SimulationInput.empty(0, 1);
for k = 1:numel(todoIdx)
    r = RS(todoIdx(k));
    scn = r.scnVal;
    isTimeMode = strcmp(ecms2.scn_field(scn, 'speed_profile_mode', 'distance'), 'time');
    inK = Simulink.SimulationInput(MDL);
    inK = inK.setVariable('SCN', scn);
    inK = inK.setVariable('EMS_CTRL_LAW', r.law);
    inK = inK.setVariable('EMS_MODE', double(r.law > 0));
    inK = inK.setVariable('Bat_SOC_init', scn.initial_SOC);
    inK = inK.setVariable('AI_Pred_Source', 0);
    inK = inK.setVariable('AI_TPeak_Source', 0);
    inK = inK.setVariable('VOYAGE_SPEED_MODE', double(isTimeMode));
    inK = inK.setVariable('VOYAGE_V_TIME_S', ecms2.scn_field(scn, 'v_time_s', [0 1]));
    inK = inK.setVariable('VOYAGE_V_VALUE_MPS', ecms2.scn_field(scn, 'v_value_mps', [0 0]));
    inK = inK.setVariable('VOYAGE_CURRENT_MODE', double(isTimeMode));
    inK = inK.setVariable('VOYAGE_CURRENT_TIME_S', ecms2.scn_field(scn, 'current_time_s', [0 1]));
    inK = inK.setVariable('VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(scn, 'current_value_mps', [0 0]));
    inK = inK.setModelParameter('StopTime', sprintf('%.2f', floor(r.stopTime * 100) / 100));
    inK = inK.setUserString(r.run_id);
    in(k) = inK; %#ok<AGROW>
end

%% ======================= 执行（parsim 优先，串行降级） =======================
% SetupFcn 用显式函数 model.parsim_setup（2026-08-26 Mac 端修复）：
% 原 '@() eval(setupStr)' 在 worker 上失败——eval 在匿名函数静态 workspace
% 执行，run() 创建的新变量（如 Model_SystemScope）报「静态工作区」错误，且
% 变量进不了 base，模型参数解析失败。见 handoff_mac_issues.md 问题 3。

outs = cell(numel(todoIdx), 1);
tBatch = tic;
if useParsim && ~isempty(in)
    fprintf('[53] parsim 启动（%d 行）: %s\n', numel(todoIdx), datestr(now, 'HH:MM:SS'));
    try
        simOuts = parsim(in, 'ShowProgress', 'on', 'SetupFcn', @() model.parsim_setup(ROOT), ...
            'TransferBaseWorkspaceVariables', 'off');
        for k = 1:numel(todoIdx), outs{k} = simOuts(k); end
        fprintf('[53] parsim 完成: %.1f s（均值 %.1f s/行）\n', toc(tBatch), toc(tBatch) / max(numel(todoIdx), 1));
    catch errP
        fprintf('[53] parsim 失败（%s）-> 串行降级\n', errP.message);
        useParsim = false;
    end
end
if ~useParsim
    for k = 1:numel(todoIdx)
        t0 = tic;
        try
            outs{k} = sim(in(k));
        catch errS
            outs{k} = errS;
        end
        if mod(k, 5) == 0 || k == 1
            fprintf('[53] 串行进度 %d/%d（最近 %.1f s/行 | %s）\n', k, numel(todoIdx), toc(t0), datestr(now, 'HH:MM:SS'));
        end
    end
    fprintf('[53] 串行完成: %.1f s\n', toc(tBatch));
end

%% ======================= 逐行提取 + 缓存落盘（仅未缓存行） =======================
metMap = containers.Map();   % run_id -> metrics struct
for k = 1:numel(todoIdx)
    r = RS(todoIdx(k));
    t0 = tic;
    if isa(outs{k}, 'MException')
        % 仿真失败：如实记录（不抛错中断整批；gate 侧会看到 NaN）
        met = struct('run_id', r.run_id, 'controller', r.ctrl, 'scenario', r.scn, ...
            'law', r.law, 'fuel_raw', NaN, 'fuel_corr', NaN, 'SOC_end', NaN, ...
            'Vdc_min', NaN, 'n_uv', NaN, 'n_der', NaN, 'bt', NaN, ...
            'wall', NaN, 'sim_error', outs{k}.message);
        fprintf('[53] 仿真失败 %-44s %s\n', r.run_id, outs{k}.message);
    else
        ds0 = model.extract_logsout(outs{k});
        ds1 = model.resample_1s(ds0);
        fuel_raw = ds1.fuel_total_kg(end);
        SOC_end  = ds1.SOC(end);
        fuel_corr = fuel_raw + (SOC_REF - SOC_end) * BatCap * K_EQ;
        Vdc_min   = min(ds1.V_dc_V);
        n_uv      = sum(ds1.V_dc_V < V_UV);
        n_der     = sum(ds1.P_prop_limit_kW < MotPnom - 1e-6);
        bt        = sum(abs(ds1.P_bat_actual_kW)) / 3600;
        met = struct('run_id', r.run_id, 'controller', r.ctrl, 'scenario', r.scn, ...
            'law', r.law, 'fuel_raw', fuel_raw, 'fuel_corr', fuel_corr, ...
            'SOC_end', SOC_end, 'Vdc_min', Vdc_min, 'n_uv', n_uv, 'n_der', n_der, ...
            'bt', bt, 'wall', toc(t0), 'sim_error', '');
        save(r.cacheMat, '-struct', 'met');   % 断点缓存（含 wall_time 原值）
        fprintf('[53] %-44s law=%d fuel_corr=%.4f kg SOC_end=%.4f Vdc_min=%.1f V n_der=%d wall=%.1f s\n', ...
            r.run_id, r.law, fuel_corr, SOC_end, Vdc_min, n_der, met.wall);
    end
    metMap(r.run_id) = met;
end

% ---- 加载缓存行 metrics（断点续跑场景）----
for k = 1:numel(RS)
    if RS(k).cached
        metMap(RS(k).run_id) = load(RS(k).cacheMat);
    end
end

%% ======================= 组装 CSV（36 行 = 16 主矩阵 + 20 泛化；Phase 1 行注明来源） =======================
HEADER = ['run_id,controller,scenario,law,fuel_raw_kg,fuel_corr_kg,SOC_end,Vdc_min_V,' ...
    'n_undervoltage,n_derate,bat_throughput_kWh,wall_time_s,scan,source,fuel_dp_kg,config_hash'];
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:MATRIX', '无法写入 %s', csvPath);
fprintf(fid, '%s\n', HEADER);

nReuse = 0; nSimDone = 0;
writeRow = @(rid, ctrl, scn, law, fraw, fcorr, soc, vdc, nuv, nder, bt, wall, scan, src, fdp, ch) ...
    fprintf(fid, '%s,%s,%s,%d,%.6g,%.6g,%.6g,%.6g,%d,%d,%.6g,%.6g,%s,%s,%.6g,%s\n', ...
        rid, ctrl, scn, law, fraw, fcorr, soc, vdc, nuv, nder, bt, wall, scan, src, fdp, ch);

% ---- 主矩阵：S20 优先，C0/C2/C3 复用 + C4 新跑 ----
for s = 1:numel(MAIN_SCNS)
    scnName = MAIN_SCNS{s};
    fdp = fuelDp(scnName);
    for c = {'C0', 'C2', 'C3'}
        ctrl = c{1};
        r1 = reuse([ctrl '_' scnName]);
        writeRow(sprintf('%s_%s_matrix', ctrl, scnName), ctrl, scnName, ...
            struct('C0', 0, 'C2', 5, 'C3', 6).(ctrl), r1.fuel_raw, r1.fuel_corr, ...
            r1.SOC_end, r1.Vdc_min, r1.n_uv, r1.n_der, r1.bt, r1.wall, ...
            'matrix', 'phase1_reuse', fdp, r1.cfg);
        nReuse = nReuse + 1;
    end
    rid = sprintf('C4_%s_matrix', scnName);
    m4 = metMap(rid);
    writeRow(rid, 'C4', scnName, 7, m4.fuel_raw, m4.fuel_corr, m4.SOC_end, ...
        m4.Vdc_min, m4.n_uv, m4.n_der, m4.bt, m4.wall, 'matrix', 'phase2_sim', fdp, cfgHash);
    nSimDone = nSimDone + 1;
end

% ---- 泛化 20 行 ----
for i = 1:numel(testIds)
    vid = testIds(i);
    v = idToV(vid);
    for c = {'C2', 'C4'}
        ctrl = c{1};
        rid = sprintf('%s_test_voyage_%03d_gen', ctrl, vid);
        m = metMap(rid);
        writeRow(rid, ctrl, v.name, LAW_OF(ctrl), m.fuel_raw, m.fuel_corr, m.SOC_end, ...
            m.Vdc_min, m.n_uv, m.n_der, m.bt, m.wall, 'generalization', 'phase2_sim', NaN, cfgHash);
        nSimDone = nSimDone + 1;
    end
end
fclose(fid);
fprintf('[53] 已写入 %s（%d 复用 + %d 新跑 = %d 行 + 表头）\n', csvPath, nReuse, nSimDone, nReuse + nSimDone);

%% ======================= 验收级断言 =======================
assert(nReuse == 12 && nSimDone == 24, 'ECMS:MATRIX', ...
    '行数异常：reuse=%d（期望 12），sim=%d（期望 24）', nReuse, nSimDone);
% 新跑行指标必须有限（除仿真失败行：如实保留 NaN 并在 gate 侧暴露）
nNaN = 0;
for k = 1:numel(RS)
    m = metMap(RS(k).run_id);
    if ~isfinite(m.fuel_corr)
        nNaN = nNaN + 1;
        fprintf('[53] 警告：%s fuel_corr=NaN（sim_error=%s）——gate 判定须处理 NaN\n', ...
            RS(k).run_id, m.sim_error);
    end
end
% S20 C4 行存在且非 NaN（主判定必需；NaN -> 矩阵未完成，不产完成标记）
mS20 = metMap('C4_S20_composite_voyage_matrix');
assert(isfinite(mS20.fuel_corr), 'ECMS:MATRIX', ...
    'S20 C4 fuel_corr=NaN（主判定行失败）——matrix 未完成，Escalate');
fprintf('[53] S20 C4（law=7）：fuel_corr=%.4f kg | SOC_end=%.4f | Vdc_min=%.1f V | n_der=%d\n', ...
    mS20.fuel_corr, mS20.SOC_end, mS20.Vdc_min, mS20.n_der);

%% ======================= 摘要打印 =======================
fprintf('\n[53] ===== 主矩阵摘要（fuel_corr [kg]，C2/C3 Phase 1 复用，C4 新跑） =====\n');
fprintf('%-22s %-14s %-14s %-14s\n', 'scenario', 'C2(phase1)', 'C3(phase1)', 'C4(phase2)');
for s = 1:numel(MAIN_SCNS)
    scnName = MAIN_SCNS{s};
    fc2 = reuse(['C2_' scnName]).fuel_corr;
    fc3 = reuse(['C3_' scnName]).fuel_corr;
    fc4 = metMap(['C4_' scnName '_matrix']).fuel_corr;
    fprintf('%-22s %-14.4f %-14.4f %-14.4f\n', scnName, fc2, fc3, fc4);
end
fprintf('\n[53] MATRIX COMPLETE n_ok=%d n_sim=%d n_reuse=%d（seed=%d, %s）\n', ...
    nSimDone, nSimDone, nReuse, seed, datestr(now, 'yyyy-mm-dd HH:MM:SS'));

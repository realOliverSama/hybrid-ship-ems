%% ========================================================================
%  42_run_phase1_matrix.m — ECMS Phase 1：实验矩阵（T-107，FR-109/114，design §4）
%  矩阵 = 主矩阵 {C0,C1,C2,C3} × {S20,S01,S02,S07}（16 行）
%       + 回归 {C0,C1,C2} × 全 8 场景（24 行）= 40 行
%  C0 = Baseline 剖面复用（S01/S02/S07 用 Phase 0 profiles，S20 用 Phase 1 profile）；
%      fuel_raw = fuel_base_tier2_kg（tier2 中档曲线离线积分，与 DP 严格同曲线 FR-113）；
%      fuel_corr = raw + (0.60 − SOC_end) × 1071.36 × 0.4774（k_eq 口径，同 calibrate_kS）；
%  C1 = law=4 Fixed ECMS（λ = λ₀ = 0.425，T-104 标定冻结）
%  C2 = law=5 SOC-Adaptive ECMS（K_soc = 30，T-105 标定冻结）
%  C3 = law=6 Oracle Predictive A-ECMS（K_S = 0.01，T-106 标定冻结；E_future 由
%      +ecms2/oracle_future_energy 对对应场景剖面预计算，H=300 s；oracle 非实时
%      可实现 E5，仅用于上界/增量验证——报告中不得表述为可在线获得）
%  C1/C2/C3 为闭环仿真（复用 model.run_scenario_sim 入口；BSFC 经
%  config/ecms_bsfc_override.mat 注入 tier2 中档，与 DP 同曲线）。
%  指标（每行）：fuel_raw / fuel_corr / SOC_end / Vdc_min / 欠压事件数 / 降额事件数 /
%  电池吞吐 ∫|P_bat|dt/3600 / wall_time；主矩阵行附 fuel_dp_kg（DP 参考）与
%  capture_rate_pct = (C0_corr − Cx_corr)/(C0_corr − DP) × 100。
%  DP 参考：S01/S02/S07 从 Phase 0 runs.csv（tier=2, mode=continuous, P_min=0）取；
%           S20 从 dp_S20_continuous_pmin000.mat 取（校验 ≈ 315.83）。
%  C1/C2 负节油或 SOC 出带：如实记录，不得调参掩盖。
%  产出：
%    results/ecms_phase1/runs_phase1.csv     汇总表（40 行，入库）
%    results/ecms_phase1/runs/<run_id>.mat   轨迹（1 s 重采样；不入库，主 Agent 把关）
%  复现（T-109，FR-114）：ECMS_PHASE1_OUTPUT_ROOT 环境变量覆盖输出目录（唯一路径开关）；
%    以覆盖目录重跑完成后，脚本末尾自动与默认 runs_phase1.csv 逐位对比（除 wall_time_s
%    列），写入 results/ecms_phase1/repro_check.json。
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/42_run_phase1_matrix.m'))"
%    ECMS_PHASE1_OUTPUT_ROOT=results/ecms_phase1/repro_tmp 同上（复现重跑）
%  注：脚本无本地函数（eval(fileread) 不支持函数定义），全部逻辑内联。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[42] ===== ECMS Phase 1 — 实验矩阵（T-107，FR-109/114，40 行） =====\n');
fprintf('[42] ROOT = %s\n', ROOT);

cfg = Ecms_Phase0_Config();

%% ======================= 输出目录（T-109 复现开关；唯一路径改动点） =======================
defaultOut = fullfile(ROOT, 'results', 'ecms_phase1');
outRoot = defaultOut;
envOut = getenv('ECMS_PHASE1_OUTPUT_ROOT');
if ~isempty(envOut)
    if startsWith(envOut, '/')
        outRoot = envOut;
    else
        outRoot = fullfile(ROOT, envOut);
    end
    fprintf('[42] 输出目录覆盖（ECMS_PHASE1_OUTPUT_ROOT）→ %s\n', outRoot);
end
runsDir = fullfile(outRoot, 'runs');
if ~exist(runsDir, 'dir'), mkdir(runsDir); end
csvPath = fullfile(outRoot, 'runs_phase1.csv');

%% ======================= 环境与冻结参数校验（config 为唯一真源，T-104~106 冻结） =======================
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
ecms2.ensure_bsfc_params(ROOT);

LAM0_EXP = 0.425;  K_SOC_EXP = 30;  K_S_EXP = 0.01;   % 标定终值（T-104/105/106，不得重标定）
assert(abs(ECMS_Lambda - LAM0_EXP) < 1e-9, 'ECMS:MATRIX', ...
    'ECMS_Lambda=%.4f ≠ 标定冻结值 %.4f（config 漂移，Escalate）', ECMS_Lambda, LAM0_EXP);
assert(abs(ECMS_K_soc - K_SOC_EXP) < 1e-9, 'ECMS:MATRIX', ...
    'ECMS_K_soc=%.4f ≠ 标定冻结值 %.4f（config 漂移，Escalate）', ECMS_K_soc, K_SOC_EXP);
assert(abs(ECMS_K_S - K_S_EXP) < 1e-9, 'ECMS:MATRIX', ...
    'ECMS_K_S=%.4f ≠ 标定冻结值 %.4f（config 漂移，Escalate）', ECMS_K_S, K_S_EXP);

SOC_REF  = ECMS_SOC_Ref;              % 0.60
K_EQ     = 0.4774;                    % fuel_corr 折算系数 [kg/kWh]（design §3.1 基线 k_eq，任务定口径）
BatCap   = Bat_Cap_kWh;               % 1071.36 kWh
MotPnom  = Mot_P_nom;                 % 700 kW（降额判定基准）
V_UV     = 600;                       % 欠压判定阈值 [V]（design §5 / run_profile_sim 同口径）
H_ORACLE = 300;                       % 前瞻时域 [s]（design §3.2）

MDL = 'HybridShip_ModelD_DCBus';
slxPath = fullfile(ROOT, 'model', [MDL '.slx']);
assert(exist(slxPath, 'file') == 2, 'ECMS:MATRIX', ...
    '缺少 %s（先运行 10_build_modelD.m）', slxPath);
if ~bdIsLoaded(MDL), load_system(slxPath); end

ovFile = fullfile(ROOT, 'config', 'ecms_bsfc_override.mat');
ovTag = 'default (ASSUMED_BSFC_*)';
if exist(ovFile, 'file')
    ovinfo = load(ovFile, 'override_tag');
    ovTag = ovinfo.override_tag;
end

md5str  = @(s) sprintf('%02x', typecast(...
    java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
cfgHash = md5str(sprintf(...
    'lam=%.6g;ksoc=%.6g;ks=%.6g;floor=%.6g;ref=%.6g;keq=%.6g;cap=%.6g;dP=%.6g;Pmax=%.6g;dt_ctrl=%.6g;idle=%.6g;H=%.6g;Vuv=%.6g;bsfc=%s', ...
    ECMS_Lambda, ECMS_K_soc, ECMS_K_S, ECMS_Lambda_Floor, ECMS_SOC_Ref, K_EQ, ...
    BatCap, ECMS_Grid_dP_kW, Gen_P_max, ECMS_dt_ctrl_s, ECMS_Idle_Fuel_kgph, ...
    H_ORACLE, V_UV, ovTag));

%% ======================= 剖面加载（Phase 0 全 8 + S20；因果隔离前置校验不可绕过） =======================
profDir0 = fullfile(ROOT, 'results', 'ecms_phase0', 'profiles');
profDir1 = fullfile(ROOT, 'results', 'ecms_phase1', 'profiles');
phase0Scns = cfg.scenarios;      % 8 场景（FR-007 入选）
matrixScns = {'S20_composite_voyage', 'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'};
profs = struct();
for k = 1:numel(phase0Scns)
    p = load(fullfile(profDir0, [phase0Scns{k} '.mat']));
    assert(~p.derate_event, 'ECMS:MATRIX', 'Phase 0 剖面 %s 含 derate_event，已作废（FR-006）', phase0Scns{k});
    assert(p.energy_closure.pass, 'ECMS:MATRIX', 'Phase 0 剖面 %s 能量闭合未过', phase0Scns{k});
    assert(isfield(p, 'fuel_base_tier2_kg'), 'ECMS:MATRIX', 'Phase 0 剖面 %s 缺 fuel_base_tier2_kg', phase0Scns{k});
    profs.(phase0Scns{k}) = p;
end
p20 = load(fullfile(profDir1, 'S20_composite_voyage.mat'));
assert(~p20.derate_event, 'ECMS:MATRIX', 'S20 剖面含 derate_event，已作废（FR-006，Escalate）');
assert(p20.energy_closure.pass, 'ECMS:MATRIX', 'S20 剖面能量闭合未过');
assert(isfield(p20, 'fuel_base_tier2_kg'), 'ECMS:MATRIX', 'S20 剖面缺 fuel_base_tier2_kg');
profs.S20_composite_voyage = p20;
fprintf('[42] 剖面加载完成：Phase 0 × %d + S20\n', numel(phase0Scns));

%% ======================= DP 参考（S20: dp 文件；S01/S02/S07: Phase 0 runs.csv tier2 cont pmin0） =======================
dpRef = containers.Map();
d20 = load(fullfile(ROOT, 'results', 'ecms_phase1', 'dp_S20_continuous_pmin000.mat'));
fuelDp20 = d20.sol.fuel_total_kg;
assert(abs(fuelDp20 - 315.83) < 0.05, 'ECMS:MATRIX', ...
    'S20 DP fuel=%.3f ≠ 315.83（T-102 产物被改动？）', fuelDp20);
dpRef('S20_composite_voyage') = fuelDp20;
fprintf('[42] DP 参考 S20 = %.3f kg（T-102）\n', fuelDp20);

p0csv = fullfile(ROOT, 'results', 'ecms_phase0', 'runs.csv');
fid0 = fopen(p0csv, 'r', 'n', 'UTF-8');
assert(fid0 > 0, 'ECMS:MATRIX', '无法读取 %s（先运行 32）', p0csv);
hdr0 = fgetl(fid0);   % 表头，不复用内容
l0 = fgetl(fid0);
while ischar(l0)
    f0 = strsplit(l0, ',', 'CollapseDelimiters', false);
    if numel(f0) == 23 && strcmp(f0{3}, '2') && strcmp(f0{4}, 'continuous') && ...
            abs(str2double(f0{5})) < 1e-9 && ...
            any(strcmp(f0{2}, {'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'}))
        if isKey(dpRef, f0{2})
            error('ECMS:MATRIX', 'Phase 0 runs.csv 中 %s tier2 continuous P_min=0 多行', f0{2});
        end
        dpRef(f0{2}) = str2double(f0{9});   % fuel_dp_kg
    end
    l0 = fgetl(fid0);
end
fclose(fid0);
for s = {'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'}
    sn = s{1};
    assert(isKey(dpRef, sn), 'ECMS:MATRIX', ...
        'Phase 0 runs.csv 缺 %s tier2 continuous P_min=0 的 fuel_dp', sn);
    fprintf('[42] DP 参考 %-20s = %.4f kg（Phase 0 runs.csv, tier2 continuous P_min=0）\n', sn, dpRef(sn));
end

%% ======================= 场景名 → SCN 映射（内联；S20 时间终止，其余距离终止） =======================
scnMap = containers.Map();
for k = 1:numel(SCENARIOS)
    if ischar(SCENARIOS(k).name) && ~isempty(SCENARIOS(k).name)
        scnMap(SCENARIOS(k).name) = SCENARIOS(k);
    end
end

%% ======================= Oracle 前瞻信号（C3 用；每场景一次，design §3.2 / FR-107） =======================
ehat = containers.Map();
allScns = [matrixScns, setdiff(phase0Scns, matrixScns)];
for k = 1:numel(allScns)
    sn = allScns{k};
    [~, ~, ~, info] = ecms2.oracle_future_energy(profs.(sn), H_ORACLE, true);
    ehat(sn) = info.data;   % N×3：[t, E_future, e_hat]
end
fprintf('[42] Oracle 前瞻信号预计算完成（%d 场景，H=%d s）\n', numel(allScns), H_ORACLE);

%% ======================= CSV 头 =======================
HEADER = ['run_id,controller,scenario,law,fuel_raw_kg,fuel_corr_kg,SOC_end,Vdc_min_V,' ...
    'n_undervoltage,n_derate,bat_throughput_kWh,wall_time_s,scan,fuel_dp_kg,capture_rate_pct,config_hash'];
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:MATRIX', '无法写入 %s', csvPath);
fprintf(fid, '%s\n', HEADER);

allRows = cell(1, 40);
nDone = 0;
tAll = tic;
c0corr = containers.Map();   % scnName -> C0 fuel_corr（capture_rate 分母）

MAIN_CTRL = {'C0', 'C1', 'C2', 'C3'};
REG_CTRL  = {'C0', 'C1', 'C2'};
LAW_OF    = containers.Map({'C0', 'C1', 'C2', 'C3'}, {0, 4, 5, 6});

%% ======================= 主矩阵 {C0,C1,C2,C3} × {S20,S01,S02,S07} =======================
for s = 1:numel(matrixScns)
    scnName = matrixScns{s};
    prof = profs.(scnName);
    SCN = scnMap(scnName);
    if strcmp(scnName, 'S20_composite_voyage')
        stopTimeLimit = SCN.stop_time_s;
    else
        stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    end
    for c = 1:numel(MAIN_CTRL)
        ctrl = MAIN_CTRL{c};
        law = LAW_OF(ctrl);
        run_id = sprintf('%s_%s_matrix', ctrl, scnName);
        nDone = nDone + 1;

        if law == 0
            % ---- C0：剖面复用（无仿真；fuel_base_tier2_kg 即 tier2 口径燃油）----
            fuel_raw  = prof.fuel_base_tier2_kg;
            SOC_end   = prof.SOC(end);
            fuel_corr = fuel_raw + (SOC_REF - SOC_end) * BatCap * K_EQ;
            Vdc_min   = prof.Vdc_min_V;
            n_uv      = prof.n_undervoltage_samples;
            n_der     = prof.n_derate_samples;
            bt        = sum(abs(prof.P_bat_kW)) / 3600;
            wall      = 0;
        else
            % ---- C1/C2/C3：闭环仿真（tier2 BSFC override 生效）----
            assignin('base', 'ECMS_EHat_Data', ehat(scnName));   % law=6 用（law4/5 变体不读）
            t0 = tic;
            out = model.run_scenario_sim(MDL, SCN, law, stopTimeLimit);
            wall = toc(t0);
            ds  = model.extract_logsout(out);
            ds1 = model.resample_1s(ds);
            fuel_raw  = ds.fuel_total_kg(end);
            SOC_end   = ds.SOC(end);
            fuel_corr = fuel_raw + (SOC_REF - SOC_end) * BatCap * K_EQ;
            Vdc_min   = min(ds.V_dc_V);
            n_uv      = sum(ds.V_dc_V < V_UV);
            n_der     = sum(ds.P_prop_limit_kW < MotPnom - 1e-6);
            bt        = sum(abs(ds1.P_bat_actual_kW)) / 3600;
        end
        if law == 0, c0corr(scnName) = fuel_corr; end
        fuelDp = dpRef(scnName);
        if law == 0
            crPct = NaN;
        else
            crPct = (c0corr(scnName) - fuel_corr) / (c0corr(scnName) - fuelDp) * 100;
        end

        metrics = struct('run_id', run_id, 'controller', ctrl, 'scenario', scnName, ...
            'law', law, 'fuel_raw_kg', fuel_raw, 'fuel_corr_kg', fuel_corr, ...
            'SOC_end', SOC_end, 'Vdc_min_V', Vdc_min, 'n_undervoltage', n_uv, ...
            'n_derate', n_der, 'bat_throughput_kWh', bt, 'wall_time_s', wall, ...
            'scan', 'matrix', 'fuel_dp_kg', fuelDp, 'capture_rate_pct', crPct, ...
            'config_hash', cfgHash);
        if law == 0
            save(fullfile(runsDir, [run_id '.mat']), 'metrics', 'run_id');
        else
            save(fullfile(runsDir, [run_id '.mat']), 'metrics', 'ds1', 'run_id');
        end

        row = {run_id, ctrl, scnName, law, fuel_raw, fuel_corr, SOC_end, Vdc_min, ...
            n_uv, n_der, bt, wall, 'matrix', fuelDp, crPct, cfgHash};
        allRows{nDone} = row;   % T-107 修复：行内容须入 allRows（原遗漏导致摘要/校验块崩溃）
        rowStr = cell(1, numel(row));
        for q = 1:numel(row)
            v = row{q};
            if ischar(v) || isstring(v)
                rowStr{q} = char(v);
            elseif isnumeric(v)
                if isscalar(v) && isnan(v)
                    rowStr{q} = 'NaN';
                else
                    rowStr{q} = sprintf('%.6g', v);
                end
            else
                rowStr{q} = '';
            end
        end
        fprintf(fid, '%s\n', strjoin(rowStr, ','));
        fprintf('[42] (%2d/40) %-42s fuel_corr=%.4f kg SOC_end=%.4f Vdc_min=%.1f V bt=%.1f kWh wall=%.1f s\n', ...
            nDone, run_id, fuel_corr, SOC_end, Vdc_min, bt, wall);
    end
end

%% ======================= 回归 {C0,C1,C2} × 全 8 场景 =======================
for s = 1:numel(phase0Scns)
    scnName = phase0Scns{s};
    prof = profs.(scnName);
    SCN = scnMap(scnName);
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    for c = 1:numel(REG_CTRL)
        ctrl = REG_CTRL{c};
        law = LAW_OF(ctrl);
        run_id = sprintf('%s_%s_regression', ctrl, scnName);
        nDone = nDone + 1;

        if law == 0
            fuel_raw  = prof.fuel_base_tier2_kg;
            SOC_end   = prof.SOC(end);
            fuel_corr = fuel_raw + (SOC_REF - SOC_end) * BatCap * K_EQ;
            Vdc_min   = prof.Vdc_min_V;
            n_uv      = prof.n_undervoltage_samples;
            n_der     = prof.n_derate_samples;
            bt        = sum(abs(prof.P_bat_kW)) / 3600;
            wall      = 0;
        else
            assignin('base', 'ECMS_EHat_Data', ehat(scnName));
            t0 = tic;
            out = model.run_scenario_sim(MDL, SCN, law, stopTimeLimit);
            wall = toc(t0);
            ds  = model.extract_logsout(out);
            ds1 = model.resample_1s(ds);
            fuel_raw  = ds.fuel_total_kg(end);
            SOC_end   = ds.SOC(end);
            fuel_corr = fuel_raw + (SOC_REF - SOC_end) * BatCap * K_EQ;
            Vdc_min   = min(ds.V_dc_V);
            n_uv      = sum(ds.V_dc_V < V_UV);
            n_der     = sum(ds.P_prop_limit_kW < MotPnom - 1e-6);
            bt        = sum(abs(ds1.P_bat_actual_kW)) / 3600;
        end

        metrics = struct('run_id', run_id, 'controller', ctrl, 'scenario', scnName, ...
            'law', law, 'fuel_raw_kg', fuel_raw, 'fuel_corr_kg', fuel_corr, ...
            'SOC_end', SOC_end, 'Vdc_min_V', Vdc_min, 'n_undervoltage', n_uv, ...
            'n_derate', n_der, 'bat_throughput_kWh', bt, 'wall_time_s', wall, ...
            'scan', 'regression', 'fuel_dp_kg', NaN, 'capture_rate_pct', NaN, ...
            'config_hash', cfgHash);
        if law == 0
            save(fullfile(runsDir, [run_id '.mat']), 'metrics', 'run_id');
        else
            save(fullfile(runsDir, [run_id '.mat']), 'metrics', 'ds1', 'run_id');
        end

        row = {run_id, ctrl, scnName, law, fuel_raw, fuel_corr, SOC_end, Vdc_min, ...
            n_uv, n_der, bt, wall, 'regression', NaN, NaN, cfgHash};
        allRows{nDone} = row;   % T-107 修复：行内容须入 allRows
        rowStr = cell(1, numel(row));
        for q = 1:numel(row)
            v = row{q};
            if ischar(v) || isstring(v)
                rowStr{q} = char(v);
            elseif isnumeric(v)
                if isscalar(v) && isnan(v)
                    rowStr{q} = 'NaN';
                else
                    rowStr{q} = sprintf('%.6g', v);
                end
            else
                rowStr{q} = '';
            end
        end
        fprintf(fid, '%s\n', strjoin(rowStr, ','));
        fprintf('[42] (%2d/40) %-42s fuel_corr=%.4f kg SOC_end=%.4f Vdc_min=%.1f V bt=%.1f kWh wall=%.1f s\n', ...
            nDone, run_id, fuel_corr, SOC_end, Vdc_min, bt, wall);
    end
end
fclose(fid);
fprintf('\n[42] 已写入 %s（%d 行 + 表头）\n', csvPath, nDone);

%% ======================= 验收级断言（T-107 / FR-109 / design §4） =======================
assert(nDone == 40, 'ECMS:MATRIX', '矩阵行数 %d ≠ 40', nDone);
for i = 1:40
    r = allRows{i};
    assert(isfinite(r{5}) && isfinite(r{6}) && isfinite(r{7}) && isfinite(r{8}), ...
        'ECMS:MATRIX', '行 %d（%s）存在 NaN/缺失指标', i, r{1});
end
nMain = sum(cellfun(@(r) strcmp(r{13}, 'matrix'), allRows));
nReg  = sum(cellfun(@(r) strcmp(r{13}, 'regression'), allRows));
assert(nMain == 16 && nReg == 24, 'ECMS:MATRIX', '主矩阵 %d ≠ 16 或回归 %d ≠ 24', nMain, nReg);

% ---- C3 × S20 终值与 T-106 标定终值一致（容差 0.5%；不一致 = 矩阵接线有误，Escalate）----
idxC3S20 = find(cellfun(@(r) strcmp(r{2}, 'C3') && strcmp(r{3}, 'S20_composite_voyage'), allRows));
assert(numel(idxC3S20) == 1, 'ECMS:MATRIX', 'C3×S20 行缺失');
fc3s20 = allRows{idxC3S20}{6};
C3_REF = 324.95727255293008;   % kS_calibration.json final_run.fuel_corr_kg（T-106 终值）
relErr = abs(fc3s20 - C3_REF) / C3_REF;
assert(relErr < 0.005, 'ECMS:MATRIX:C3', ...
    'C3×S20 fuel_corr=%.4f kg vs 标定终值 %.4f kg，相对偏差 %.4f%% ≥ 0.5%% ——矩阵接线有误，Escalate', ...
    fc3s20, C3_REF, 100 * relErr);
fprintf('[42] 验收：C3×S20 fuel_corr=%.4f kg（T-106 终值 %.4f kg，偏差 %.4f%% < 0.5%%）\n', ...
    fc3s20, C3_REF, 100 * relErr);

% ---- C2 × S20 与 law=5 参照比对（信息性，不一致亦属接线异常，打印警告）----
idxC2S20 = find(cellfun(@(r) strcmp(r{2}, 'C2') && strcmp(r{3}, 'S20_composite_voyage'), allRows));
fc2s20 = allRows{idxC2S20}{6};
C2_REF = 330.66187376504934;   % kS_calibration.json law5_reference.fuel_corr_kg
if abs(fc2s20 - C2_REF) / C2_REF > 0.005
    fprintf('[42] 警告：C2×S20 fuel_corr=%.4f kg vs law=5 参照 %.4f kg（偏差 %.4f%% > 0.5%%），请复核接线\n', ...
        fc2s20, C2_REF, 100 * abs(fc2s20 - C2_REF) / C2_REF);
else
    fprintf('[42] C2×S20 fuel_corr=%.4f kg（law=5 参照 %.4f kg，一致）\n', fc2s20, C2_REF);
end

%% ======================= 摘要打印（主矩阵 16 行：fuel_corr 与 saving%） =======================
fprintf('\n[42] ===== 主矩阵摘要（fuel_corr [kg]，saving% vs C0 corr） =====\n');
fprintf('%-22s %-14s %-16s %-16s %-16s\n', 'scenario', 'C0', 'C1', 'C2', 'C3');
for s = 1:numel(matrixScns)
    scnName = matrixScns{s};
    line = sprintf('%-22s', scnName);
    c0 = NaN;
    for c = 1:numel(MAIN_CTRL)
        ctrl = MAIN_CTRL{c};
        hit = find(cellfun(@(r) strcmp(r{2}, ctrl) && strcmp(r{3}, scnName), allRows), 1);
        fc = allRows{hit}{6};
        if c == 1
            c0 = fc;
            line = [line sprintf(' %-13.4f', fc)]; %#ok<AGROW>
        else
            sav = (c0 - fc) / c0 * 100;
            line = [line sprintf(' %-9.4f(%+.2f%%)', fc, sav)]; %#ok<AGROW>
        end
    end
    fprintf('%s\n', line);
end
fprintf('\n[42] 主矩阵完成：40 行（16 主 + 24 回归），总耗时 %.1f s。runs_phase1.csv 与 runs/*.mat 已写入。\n', toc(tAll));

%% ======================= T-109 复现双跑对比（仅覆盖目录运行时执行） =======================
% 覆盖目录运行（ECMS_PHASE1_OUTPUT_ROOT 非空且 ≠ 默认）时，与默认 runs_phase1.csv
% 逐位对比（除 wall_time_s 列），写入默认目录 repro_check.json。
if ~isempty(envOut)
    outAbs = outRoot;
    if ~startsWith(outRoot, '/'), outAbs = fullfile(pwd, outRoot); end
    if ~strcmp(outAbs, defaultOut)
        defCsv = fullfile(defaultOut, 'runs_phase1.csv');
        if exist(defCsv, 'file') == 2
            % ---- 读取两个 CSV（字符串保真）----
            % 主 Agent 修复：rds 须初始化为带字段的 0x0 struct——`struct()` 是 1x1，
            % 会导致首个文件写入 rds(2)、比较错位（repro_dbg 实证）
            rds = struct('header', {}, 'headerFields', {}, 'rows', {});
            for rp = {defCsv, csvPath}
                fh = fopen(rp{1}, 'r', 'n', 'UTF-8');
                assert(fh > 0, 'ECMS:REPRO', '无法读取 %s', rp{1});
                hd = fgetl(fh);
                rr = {}; ll = fgetl(fh);
                while ischar(ll)
                    rr{end+1} = strsplit(ll, ',', 'CollapseDelimiters', false); %#ok<AGROW>
                    ll = fgetl(fh);
                end
                fclose(fh);
                rds(end+1).header = hd; %#ok<AGROW>
                rds(end).headerFields = strsplit(hd, ',', 'CollapseDelimiters', false);
                rds(end).rows = rr;
            end
            % ---- 逐位对比（除 wall_time_s 字段 12）----
            chkPass = true; ndiff = 0; diffs = {};
            if ~strcmp(rds(1).header, rds(2).header)
                chkPass = false; diffs{end+1} = 'header'; %#ok<AGROW>
            end
            if numel(rds(1).rows) ~= numel(rds(2).rows)
                chkPass = false;
                diffs{end+1} = sprintf('row count %d vs %d', numel(rds(1).rows), numel(rds(2).rows)); %#ok<AGROW>
            end
            nMin = min(numel(rds(1).rows), numel(rds(2).rows));
            for i = 1:nMin
                fa = rds(1).rows{i}; fb = rds(2).rows{i};
                if numel(fa) ~= numel(fb)
                    chkPass = false; ndiff = ndiff + 1;
                    diffs{end+1} = sprintf('row %d 字段数 %d vs %d', i, numel(fa), numel(fb)); %#ok<AGROW>
                    continue; %#ok<UNRCH>
                end
                for k = 1:numel(fa)
                    if k == 12, continue; end   % wall_time_s 允许不同
                    if ~strcmp(fa{k}, fb{k})
                        chkPass = false; ndiff = ndiff + 1;
                        diffs{end+1} = sprintf('row %d field %d (%s): "%s" vs "%s"', ...
                            i, k, rds(1).headerFields{k}, fa{k}, fb{k}); %#ok<AGROW>
                    end
                end
            end
            chk = struct();
            chk.pass = chkPass;
            chk.n_rows = nDone;
            chk.compare = 'runs_phase1.csv 双跑逐位一致（除 wall_time_s 列）';
            chk.n_diff_fields = ndiff;
            chk.diffs = diffs;
            chk.default_csv = defCsv;
            chk.repro_csv = csvPath;
            chk.generated = datestr(now, 'yyyy-mm-dd HH:MM:SS');
            chk.notes = 'T-109（FR-114）：主矩阵双跑逐位一致；wall_time_s 除外';
            ecms.write_json_deep(fullfile(defaultOut, 'repro_check.json'), chk);
            fprintf('\n[42] T-109 复现对比：pass=%d（差异字段 %d）→ %s\n', ...
                double(chkPass), ndiff, fullfile(defaultOut, 'repro_check.json'));
            if ~chkPass
                error('ECMS:MATRIX:REPRO', '复现对比不一致：%d 处差异（见 repro_check.json diffs）', ndiff);
            end
        else
            fprintf('[42] 警告：默认 %s 不存在，跳过 T-109 双跑对比（首次覆盖运行？）\n', defCsv);
        end
    end
end

fprintf('[42] 完成。\n');

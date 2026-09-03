%% ========================================================================
%  43_phase1_gate_judge_unittest.m — Gate 判定逻辑单测（T-108，FR-110）
%  构造 3 个迷你 runs_phase1.csv（40 行 = 16 主矩阵 + 24 回归），触达全部判定分支：
%    case A: PROCEED_TO_PHASE2_NN   —— S20 oracle_increment_pp = 0.80 pp（≥0.5）
%    case B: DECISION_REQUIRED      —— S20 oracle_increment_pp = 0.35 pp（0.2~0.5）
%    case C: NN_VALUE_LIMITED       —— S20 oracle_increment_pp = 0.05 pp（<0.2）
%  通过环境变量 ECMS_PHASE1_GATE_RUNS_CSV / ECMS_PHASE1_GATE_OUT_JSON 驱动
%  43_phase1_gate_judge.m（eval(fileread) 执行生产脚本本体），再 jsondecode
%  输出产物断言 recommendation 与数值。任一用例失败 → error（非零退出）。
%  注：脚本无本地函数（eval(fileread) 不支持函数定义），全部逻辑内联。
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && matlab -batch \
%      "eval(fileread('scripts/43_phase1_gate_judge_unittest.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[43-UT] ===== Gate 判定逻辑单测（T-108） =====\n');
utDir = fullfile(ROOT, 'results', 'ecms_phase1', '_gate_ut');
if ~exist(utDir, 'dir'), mkdir(utDir); end

HEADER = ['run_id,controller,scenario,law,fuel_raw_kg,fuel_corr_kg,SOC_end,Vdc_min_V,' ...
    'n_undervoltage,n_derate,bat_throughput_kWh,wall_time_s,scan,fuel_dp_kg,capture_rate_pct,config_hash'];

MAIN_SCNS = {'S20_composite_voyage', 'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'};
REG_SCNS  = {'S01_still_water', 'S02_downstream', 'S03_upstream', 'S04_resistance_inc', ...
    'S07_bridge_maneuver', 'S08_shoal_series', 'S09_emergency_sprint', 'S10_current_reversal'};
CTRLS = {'C0', 'C1', 'C2', 'C3'};
LAW_LIST_M = [0 4 5 6];   % C0/C1/C2/C3
LAW_LIST_R = [0 4 5];    % C0/C1/C2（回归）

% ---- 行构造：fc(ctrl, scn) 由 saving 反推；fuel_dp=80 -> 分母 20 ----
mkRow = @(runId, ctrl, scn, law, fc, soc, vdc, bt, wall, scan, fdp, crPct, h) { ...
    runId, ctrl, scn, num2str(law), num2str(fc - 1, '%.6g'), num2str(fc, '%.6g'), ...
    num2str(soc, '%.6g'), num2str(vdc, '%.6g'), '0', '0', num2str(bt, '%.6g'), ...
    num2str(wall, '%.6g'), scan, num2str(fdp, '%.6g'), num2str(crPct, '%.6g'), h};
rowToStr = @(row) strjoin(row, ',');

cases = {
    struct('name', 'A_PROCEED_TO_PHASE2_NN', 'incS20', 0.80, 'expRec', 'PROCEED_TO_PHASE2_NN');
    struct('name', 'B_DECISION_REQUIRED',    'incS20', 0.35, 'expRec', 'DECISION_REQUIRED');
    struct('name', 'C_NN_VALUE_LIMITED',     'incS20', 0.05, 'expRec', 'NN_VALUE_LIMITED')
    };

for ci = 1:numel(cases)
    cs = cases{ci};
    R = {};
    k = 0;
    h = sprintf('ut_hash_%d', ci);

    % ---- 主矩阵 16 行（saving: C1=2%, C2=4%, C3=4+inc）----
    for s = 1:numel(MAIN_SCNS)
        scn = MAIN_SCNS{s};
        inc = cs.incS20;
        if ~strcmp(scn, 'S20_composite_voyage'), inc = 0.30; end   % 附列场景固定值
        fc0 = 100; fc1 = 98; fc2 = 96; fc3 = 100 - (4 + inc);
        fdp = 80;
        fcs = containers.Map({'C0', 'C1', 'C2', 'C3'}, {fc0, fc1, fc2, fc3});
        for c = 1:numel(CTRLS)
            ctrl = CTRLS{c};
            k = k + 1;
            runId = sprintf('%s_%s_matrix', ctrl, scn);
            law = LAW_LIST_M(c);
            fcv = fcs(ctrl);
            if strcmp(ctrl, 'C0')
                crPct = NaN;
            else
                crPct = (fc0 - fcv) / (fc0 - fdp) * 100;
            end
            R{end+1} = mkRow(runId, ctrl, scn, law, fcv, 0.60, 700, 5.0, 10.0, 'matrix', fdp, crPct, h); %#ok<SAGROW>
        end
    end
    % ---- 回归 24 行（C0/C1/C2 × 8 场景；无 DP/无 capture）----
    for s = 1:numel(REG_SCNS)
        scn = REG_SCNS{s};
        fcs = containers.Map({'C0', 'C1', 'C2'}, {100, 98, 96});
        for c = 1:3
            ctrl = CTRLS{c};
            k = k + 1;
            runId = sprintf('%s_%s_regression', ctrl, scn);
            law = LAW_LIST_R(c);
            R{end+1} = mkRow(runId, ctrl, scn, law, fcs(ctrl), 0.60, 700, 5.0, 10.0, 'regression', NaN, NaN, h); %#ok<SAGROW>
        end
    end
    assert(numel(R) == 40, 'ECMS:UT', '构造行数 %d ≠ 40', numel(R));

    csvPath = fullfile(utDir, ['ut_' cs.name '.csv']);
    jsonPath = fullfile(utDir, ['ut_' cs.name '.json']);
    fid = fopen(csvPath, 'w', 'n', 'UTF-8');
    assert(fid > 0, 'ECMS:UT', '无法写入 %s', csvPath);
    fprintf(fid, '%s\n', HEADER);
    for i = 1:numel(R)
        fprintf(fid, '%s\n', rowToStr(R{i}));
    end
    fclose(fid);

    % ---- 环境变量驱动生产脚本（判定逻辑本体）----
    setenv('ECMS_PHASE1_GATE_RUNS_CSV', csvPath);
    setenv('ECMS_PHASE1_GATE_OUT_JSON', jsonPath);
    eval(fileread(fullfile(ROOT, 'scripts', '43_phase1_gate_judge.m')));
    assert(exist(jsonPath, 'file') == 2, 'ECMS:UT', 'case %s：gate 未产出 %s', cs.name, jsonPath);
    res = jsondecode(fileread(jsonPath));

    % ---- 断言：recommendation 与数值一致性 ----
    expC1 = (100 - 98) / (100 - 80);          % 0.10
    expC2 = (100 - 96) / (100 - 80);          % 0.20
    expC3 = (100 - (100 - (4 + cs.incS20))) / (100 - 80);   % (4+inc)/20
    assert(strcmp(res.recommendation, cs.expRec), 'ECMS:UT', ...
        'case %s：recommendation=%s ≠ %s', cs.name, res.recommendation, cs.expRec);
    assert(abs(res.oracle_increment_pp - cs.incS20) < 1e-9, 'ECMS:UT', ...
        'case %s：oracle_increment_pp=%.6f ≠ %.6f', cs.name, res.oracle_increment_pp, cs.incS20);
    assert(abs(res.capture_rate_c1 - expC1) < 1e-9, 'ECMS:UT', ...
        'case %s：capture_rate_c1=%.6f ≠ %.6f', cs.name, res.capture_rate_c1, expC1);
    assert(abs(res.capture_rate_c2 - expC2) < 1e-9, 'ECMS:UT', ...
        'case %s：capture_rate_c2=%.6f ≠ %.6f', cs.name, res.capture_rate_c2, expC2);
    assert(abs(res.capture_rate_c3 - expC3) < 1e-9, 'ECMS:UT', ...
        'case %s：capture_rate_c3=%.6f ≠ %.6f', cs.name, res.capture_rate_c3, expC3);
    assert(abs(res.ecms_capture_rate - expC1) < 1e-9, 'ECMS:UT', ...
        'case %s：ecms_capture_rate=%.6f ≠ %.6f', cs.name, res.ecms_capture_rate, expC1);
    % per_scenario 自洽（S20）
    ps20 = res.per_scenario.S20_composite_voyage;
    assert(abs(ps20.oracle_increment_pp - cs.incS20) < 1e-9, 'ECMS:UT', ...
        'case %s：per_scenario.S20.oracle_increment_pp=%.6f ≠ %.6f', cs.name, ...
        ps20.oracle_increment_pp, cs.incS20);
    assert(abs(ps20.capture_rate.C3 - expC3) < 1e-9, 'ECMS:UT', ...
        'case %s：per_scenario.S20.capture_rate.C3=%.6f ≠ %.6f', cs.name, ...
        ps20.capture_rate.C3, expC3);
    assert(~isempty(res.config_hash) && ~isempty(res.generated), 'ECMS:UT', ...
        'case %s：config_hash/generated 缺失', cs.name);

    fprintf('[43-UT][PASS] case %-24s rec=%-24s inc=%.2f pp  capture=[%.4f %.4f %.4f]\n', ...
        cs.name, res.recommendation, res.oracle_increment_pp, ...
        res.capture_rate_c1, res.capture_rate_c2, res.capture_rate_c3);
end

fprintf('\n[43-UT] Gate 判定逻辑单测全部通过（3 用例：PROCEED / DECISION / NN_VALUE_LIMITED）。\n');

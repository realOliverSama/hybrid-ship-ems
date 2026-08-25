%% ========================================================================
%  54_phase2_gate_judge_unittest.m — Gate 判定逻辑单测（T-205，FR-210）
%  构造 3 个迷你 runs_phase2.csv（36 行 = 16 主矩阵 + 20 泛化），触达全部判定分支：
%    case A: PROCEED_TO_PHASE3  —— S20 nn_oracle_capture = 0.60（≥0.5）
%    case B: PARTIAL            —— S20 nn_oracle_capture = 0.40（0.3~0.5）
%    case C: LIMITED            —— S20 nn_oracle_capture = 0.20（<0.3）
%  S20 燃料：C2=330.662 / C3=324.957（Phase 1 真值，分母 = 5.705）；C4 按
%    capture 目标反推（fc4 = fc2 − capture×(fc2−fc3)），保证判定纯由 capture 驱动。
%  泛化：10 个 test 航次，saving_diff_pct_i = (1+0.1·i)/(40+i)×100，
%    测试侧手算 median/min 并与 54 输出断言。
%  通过环境变量 ECMS_PHASE2_GATE_RUNS_CSV / ECMS_PHASE2_GATE_OUT_JSON 驱动
%  54_phase2_gate_judge.m（eval(fileread) 执行生产脚本本体），再 jsondecode
%  输出产物断言 recommendation 与数值。任一用例失败 → error（非零退出）。
%  注：脚本无本地函数（eval(fileread) 不支持函数定义），全部逻辑内联。
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && matlab -batch \
%      "eval(fileread('scripts/54_phase2_gate_judge_unittest.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[54-UT] ===== Gate 判定逻辑单测（T-205，FR-210） =====\n');
utDir = fullfile(ROOT, 'results', 'ecms_phase2', '_gate_ut');
if ~exist(utDir, 'dir'), mkdir(utDir); end

HEADER = ['run_id,controller,scenario,law,fuel_raw_kg,fuel_corr_kg,SOC_end,Vdc_min_V,' ...
    'n_undervoltage,n_derate,bat_throughput_kWh,wall_time_s,scan,source,fuel_dp_kg,config_hash'];

MAIN_SCNS = {'S20_composite_voyage', 'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'};
CTRLS = {'C0', 'C2', 'C3', 'C4'};
LAW_OF = containers.Map({'C0', 'C2', 'C3', 'C4'}, {0, 5, 6, 7});
% Phase 1 真值（附列场景），仅 S20 由 capture 目标反推
P1_C2 = containers.Map(MAIN_SCNS, {330.662, 83.6048, 60.9674, 80.7452});
P1_C3 = containers.Map(MAIN_SCNS, {324.957, 83.191, 54.9743, 80.6989});
P1_C0 = containers.Map(MAIN_SCNS, {349.002, 84.3198, 64.3204, 82.8431});
DP    = containers.Map(MAIN_SCNS, {315.829, 81.3239, 59.0083, 78.9195});

% ---- 行构造：fuel_raw=fc−1（任意），fuel_corr=fc，其余固定 ----
mkRow = @(runId, ctrl, scn, law, fc, scan, src, fdp, h) { ...
    runId, ctrl, scn, num2str(law), num2str(fc - 1, '%.6g'), num2str(fc, '%.6g'), ...
    '0.60', '683.0', '0', '0', '5.0', '10.0', scan, src, num2str(fdp, '%.6g'), h};
rowToStr = @(row) strjoin(row, ',');

cases = {
    struct('name', 'A_PROCEED_TO_PHASE3', 'capS20', 0.60, 'expRec', 'PROCEED_TO_PHASE3');
    struct('name', 'B_PARTIAL',            'capS20', 0.40, 'expRec', 'PARTIAL');
    struct('name', 'C_LIMITED',            'capS20', 0.20, 'expRec', 'LIMITED')
    };

for ci = 1:numel(cases)
    cs = cases{ci};
    R = {};
    h = sprintf('ut_hash_%d', ci);

    % ---- 主矩阵 16 行 ----
    for s = 1:numel(MAIN_SCNS)
        scn = MAIN_SCNS{s};
        fc2 = P1_C2(scn); fc3 = P1_C3(scn);
        if strcmp(scn, 'S20_composite_voyage')
            fc4 = fc2 - cs.capS20 * (fc2 - fc3);   % capture 目标反推
        else
            fc4 = fc3 + 0.5 * (fc2 - fc3);         % 附列场景固定 capture=0.5
        end
        fcs = containers.Map({'C0', 'C2', 'C3', 'C4'}, {P1_C0(scn), fc2, fc3, fc4});
        for c = 1:numel(CTRLS)
            ctrl = CTRLS{c};
            src = 'phase1_reuse';
            if strcmp(ctrl, 'C4'), src = 'phase2_sim'; end
            R{end + 1} = mkRow(sprintf('%s_%s_matrix', ctrl, scn), ctrl, scn, ...
                LAW_OF(ctrl), fcs(ctrl), 'matrix', src, DP(scn), h); %#ok<SAGROW>
        end
    end
    % ---- 泛化 20 行（C2/C4 × 10 航次）----
    sdExp = zeros(1, 10);
    for i = 1:10
        f2 = 40 + i;
        f4 = f2 - (1 + 0.1 * i);
        sdExp(i) = (f2 - f4) / f2 * 100;   % (1+0.1i)/(40+i)×100
        for c = {'C2', 'C4'}
            ctrl = c{1};
            if strcmp(ctrl, 'C2')
                fcv = f2; law = 5;
            else
                fcv = f4; law = 7;
            end
            R{end + 1} = mkRow(sprintf('%s_test_voyage_%03d_gen', ctrl, 100 + i), ...
                ctrl, sprintf('voyage_%03d', 100 + i), law, fcv, ...
                'generalization', 'phase2_sim', NaN, h); %#ok<SAGROW>
        end
    end
    assert(numel(R) == 36, 'ECMS:UT', '构造行数 %d ≠ 36', numel(R));

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
    setenv('ECMS_PHASE2_GATE_RUNS_CSV', csvPath);
    setenv('ECMS_PHASE2_GATE_OUT_JSON', jsonPath);
    eval(fileread(fullfile(ROOT, 'scripts', '54_phase2_gate_judge.m')));
    assert(exist(jsonPath, 'file') == 2, 'ECMS:UT', 'case %s：gate 未产出 %s', cs.name, jsonPath);
    res = jsondecode(fileread(jsonPath));

    % ---- 断言：recommendation / nn_oracle_capture / 泛化分布 ----
    expMed = median(sdExp); expMin = min(sdExp);
    assert(strcmp(res.recommendation, cs.expRec), 'ECMS:UT', ...
        'case %s：recommendation=%s ≠ %s', cs.name, res.recommendation, cs.expRec);
    assert(abs(res.nn_oracle_capture - cs.capS20) < 1e-9, 'ECMS:UT', ...
        'case %s：nn_oracle_capture=%.6f ≠ %.6f', cs.name, res.nn_oracle_capture, cs.capS20);
    assert(abs(res.fuel_c2_S20_kg - P1_C2('S20_composite_voyage')) < 1e-6, 'ECMS:UT', ...
        'case %s：fuel_c2_S20 异常', cs.name);
    assert(abs(res.fuel_c3_S20_kg - P1_C3('S20_composite_voyage')) < 1e-6, 'ECMS:UT', ...
        'case %s：fuel_c3_S20 异常', cs.name);
    assert(res.generalization.n_test_voyages == 10, 'ECMS:UT', ...
        'case %s：泛化航次数 %d ≠ 10', cs.name, res.generalization.n_test_voyages);
    assert(abs(res.generalization.saving_diff_pct_median - expMed) < 1e-6, 'ECMS:UT', ...
        'case %s：泛化 median=%.6f ≠ 手算 %.6f', cs.name, ...
        res.generalization.saving_diff_pct_median, expMed);
    assert(abs(res.generalization.saving_diff_pct_min - expMin) < 1e-6, 'ECMS:UT', ...
        'case %s：泛化 min=%.6f ≠ 手算 %.6f', cs.name, ...
        res.generalization.saving_diff_pct_min, expMin);
    assert(abs(res.per_scenario.S20_composite_voyage.capture - cs.capS20) < 1e-9, 'ECMS:UT', ...
        'case %s：per_scenario.S20.capture=%.6f ≠ %.6f', cs.name, ...
        res.per_scenario.S20_composite_voyage.capture, cs.capS20);
    assert(~isempty(res.config_hash) && ~isempty(res.generated), 'ECMS:UT', ...
        'case %s：config_hash/generated 缺失', cs.name);

    fprintf('[54-UT][PASS] case %-22s rec=%-18s capture=%.2f  gen_med=%.4f%% gen_min=%.4f%%\n', ...
        cs.name, res.recommendation, res.nn_oracle_capture, ...
        res.generalization.saving_diff_pct_median, res.generalization.saving_diff_pct_min);
end

fprintf('\n[54-UT] Gate 判定逻辑单测全部通过（3 用例：PROCEED / PARTIAL / LIMITED）。\n');

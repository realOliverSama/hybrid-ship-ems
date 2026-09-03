%% ========================================================================
%  43_phase1_gate_judge.m — ECMS Phase 1：Gate 机器判定（T-108，FR-110，design §5）
%  读取 runs_phase1.csv（仅 scan='matrix' 行：{C0,C1,C2,C3} × {S20,S01,S02,S07}）。
%  指标（design §5 定案公式，S20 主判定，其余场景附列）：
%    capture_rate(Cx) = (fuel_C0_corr − fuel_Cx_corr) / (fuel_C0_corr − fuel_DP)
%    saving(Cx)       = (fuel_C0_corr − fuel_Cx_corr) / fuel_C0_corr × 100   [%]
%    oracle_increment_pp = saving(C3) − saving(C2)                          [pp]
%  recommendation（design §5 / FR-110 精确阈值，不得“调整”改变结论）：
%    oracle_increment_pp ≥ 0.5 pp  → PROCEED_TO_PHASE2_NN
%    0.2 ≤ oracle_increment_pp < 0.5 → DECISION_REQUIRED
%    oracle_increment_pp < 0.2 pp  → NN_VALUE_LIMITED
%  语义：capture_rate 是诊断指标不是过门条件；Phase 1 的 GO 语义 = 完成 RQ1~3
%  测量（机器可读），判读归用户（design §5 注）。
%  产出：results/ecms_phase1/gate_result.json
%    （字段：capture_rate_c1/c2/c3、ecms_capture_rate（FR-110）、oracle_increment_pp、
%      recommendation、per_scenario、config_hash、generated、notes）
%  测试钩子（供 43_phase1_gate_judge_unittest.m 构造数据单测判定逻辑）：
%    ECMS_PHASE1_GATE_RUNS_CSV — 输入 runs_phase1.csv 路径（缺省 .../runs_phase1.csv）
%    ECMS_PHASE1_GATE_OUT_JSON — 输出 gate_result.json 路径（缺省 .../gate_result.json）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/43_phase1_gate_judge.m'))"
%  注：脚本无本地函数（eval(fileread) 不支持函数定义），全部逻辑内联。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[43] ===== ECMS Phase 1 — Gate 机器判定（T-108，FR-110，design §5） =====\n');
fprintf('[43] ROOT = %s\n', ROOT);

%% ======================= 判定参数（design §5 定案阈值，冻结） =======================
INC_PROCEED = 0.5;    % oracle_increment ≥ 0.5 pp → PROCEED_TO_PHASE2_NN
INC_LIMITED = 0.2;    % oracle_increment < 0.2 pp → NN_VALUE_LIMITED；其间 → DECISION_REQUIRED

%% ======================= 输入 / 输出（环境变量为单测钩子；缺省 = 提交态路径） =======================
inCsv = getenv('ECMS_PHASE1_GATE_RUNS_CSV');
if isempty(inCsv)
    inCsv = fullfile(ROOT, 'results', 'ecms_phase1', 'runs_phase1.csv');
end
outJson = getenv('ECMS_PHASE1_GATE_OUT_JSON');
if isempty(outJson)
    outJson = fullfile(ROOT, 'results', 'ecms_phase1', 'gate_result.json');
end
assert(exist(inCsv, 'file') == 2, 'ECMS:GATE', '未找到 %s（先运行 42）', inCsv);
fprintf('[43] 输入 runs_phase1.csv = %s\n', inCsv);
fprintf('[43] 输出 gate JSON       = %s\n', outJson);

%% ======================= 解析 runs_phase1.csv（字符串保真，同 35 风格） =======================
HEADER = ['run_id,controller,scenario,law,fuel_raw_kg,fuel_corr_kg,SOC_end,Vdc_min_V,' ...
    'n_undervoltage,n_derate,bat_throughput_kWh,wall_time_s,scan,fuel_dp_kg,capture_rate_pct,config_hash'];
fid = fopen(inCsv, 'r', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:GATE', '无法读取 %s', inCsv);
hdrLine = fgetl(fid);
assert(strcmp(hdrLine, HEADER), 'ECMS:GATE', 'runs_phase1.csv 表头与 42 不匹配（请先重跑 42）');
rows = {};
l = fgetl(fid);
while ischar(l)
    f = strsplit(l, ',', 'CollapseDelimiters', false);
    assert(numel(f) == 16, 'ECMS:GATE', 'runs_phase1.csv 行字段数异常: %s', l);
    rows{end+1} = f; %#ok<AGROW>
    l = fgetl(fid);
end
fclose(fid);
fprintf('[43] 读取总行数：%d\n', numel(rows));

numv = @(s) str2double(s);
isMatrix = cellfun(@(r) strcmp(r{13}, 'matrix'), rows);
isReg    = cellfun(@(r) strcmp(r{13}, 'regression'), rows);
assert(sum(isMatrix) == 16, 'ECMS:GATE', 'scan=matrix 行数 %d ≠ 16（矩阵不完整，先重跑 42）', sum(isMatrix));
assert(sum(isReg) == 24, 'ECMS:GATE', 'scan=regression 行数 %d ≠ 24（回归不完整，先重跑 42）', sum(isReg));

MAIN_SCNS = {'S20_composite_voyage', 'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'};
CTRLS = {'C0', 'C1', 'C2', 'C3'};

%% ======================= per_scenario：燃料 / saving / capture_rate / oracle 增量 =======================
ps = struct();
for s = 1:numel(MAIN_SCNS)
    scn = MAIN_SCNS{s};
    ent = struct();
    fc = containers.Map();
    fuelDp = NaN;
    for c = 1:numel(CTRLS)
        ctrl = CTRLS{c};
        hit = find(isMatrix & cellfun(@(r) strcmp(r{2}, ctrl) && strcmp(r{3}, scn), rows), 1);
        assert(~isempty(hit), 'ECMS:GATE', '矩阵缺行：%s × %s', ctrl, scn);
        r = rows{hit};
        ent.(ctrl) = struct( ...
            'fuel_raw_kg', numv(r{5}), 'fuel_corr_kg', numv(r{6}), ...
            'SOC_end', numv(r{7}), 'Vdc_min_V', numv(r{8}), ...
            'n_undervoltage', numv(r{9}), 'n_derate', numv(r{10}), ...
            'bat_throughput_kWh', numv(r{11}));
        fc(ctrl) = numv(r{6});
        if strcmp(ctrl, 'C0')
            fuelDp = numv(r{14});
            assert(isfinite(fuelDp) && fuelDp > 0, 'ECMS:GATE', ...
                '场景 %s 的 C0 行缺 fuel_dp_kg（DP 参考）', scn);
        end
    end
    ent.fuel_dp_kg = fuelDp;
    ent.saving_pct = struct();
    ent.capture_rate = struct();
    assert(fc('C0') > fuelDp, 'ECMS:GATE', ...
        '场景 %s：C0 fuel_corr=%.3f ≤ fuel_dp=%.3f，capture_rate 分母非正（度量域外）', ...
        scn, fc('C0'), fuelDp);
    for c = 2:4
        ctrl = CTRLS{c};
        sav = (fc('C0') - fc(ctrl)) / fc('C0') * 100;
        ent.saving_pct.(ctrl) = sav;
        ent.capture_rate.(ctrl) = (fc('C0') - fc(ctrl)) / (fc('C0') - fuelDp);
        % ---- CSV capture_rate_pct 列自洽校验（防篡改/防接线错位）----
        r = rows{find(isMatrix & cellfun(@(r) strcmp(r{2}, ctrl) && strcmp(r{3}, scn), rows), 1)};
        csvCr = numv(r{15});
        recCr = ent.capture_rate.(ctrl) * 100;
        % 容差 0.01 pp：CSV 写出精度（%.4f）截断所致，远低于判定阈值（0.2/0.5 pp）——主 Agent 修订
        if ~(isfinite(csvCr) && abs(csvCr - recCr) <= 0.01)
            error('ECMS:GATE', '%s × %s：CSV capture_rate_pct=%.6f 与燃料值重算 %.6f 不一致', ...
                scn, ctrl, csvCr, recCr);
        end
    end
    ent.oracle_increment_pp = ent.saving_pct.C3 - ent.saving_pct.C2;
    ps.(scn) = ent;
end

%% ======================= S20 主判定指标 =======================
s20 = ps.S20_composite_voyage;
capture_rate_c1 = s20.capture_rate.C1;
capture_rate_c2 = s20.capture_rate.C2;
capture_rate_c3 = s20.capture_rate.C3;
oracle_increment_pp = s20.oracle_increment_pp;

fprintf('\n[43] ===== S20 主判定（design §5） =====\n');
fprintf('[43] capture_rate C1/C2/C3 = %.4f / %.4f / %.4f（C0_corr=%.2f kg，DP=%.2f kg）\n', ...
    capture_rate_c1, capture_rate_c2, capture_rate_c3, s20.C0.fuel_corr_kg, s20.fuel_dp_kg);
fprintf('[43] saving%% C1/C2/C3 = %.3f / %.3f / %.3f\n', ...
    s20.saving_pct.C1, s20.saving_pct.C2, s20.saving_pct.C3);
fprintf('[43] oracle_increment_pp = saving(C3)−saving(C2) = %.4f pp\n', oracle_increment_pp);

%% ======================= recommendation（design §5 精确阈值） =======================
if oracle_increment_pp >= INC_PROCEED
    recommendation = 'PROCEED_TO_PHASE2_NN';
elseif oracle_increment_pp >= INC_LIMITED
    recommendation = 'DECISION_REQUIRED';
else
    recommendation = 'NN_VALUE_LIMITED';
end
fprintf('[43] recommendation = %s（oracle_increment=%.4f pp；阈值 ≥%.1f→PROCEED，≥%.1f→DECISION）\n', ...
    recommendation, oracle_increment_pp, INC_PROCEED, INC_LIMITED);

%% ======================= config_hash（行 config_hash 唯一集 + 判定规则） =======================
md5str = @(s) sprintf('%02x', typecast(...
    java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
uniqH = unique(sort(cellfun(@(r) r{16}, rows, 'UniformOutput', false)));
ruleStr = sprintf('gate_judge_v1;inc_proceed=%.6g;inc_limited=%.6g', INC_PROCEED, INC_LIMITED);
config_hash = md5str([strjoin(uniqH, ';') '|' ruleStr]);

%% ======================= 组装输出 =======================
out = struct();
out.capture_rate_c1 = capture_rate_c1;
out.capture_rate_c2 = capture_rate_c2;
out.capture_rate_c3 = capture_rate_c3;
out.ecms_capture_rate = capture_rate_c1;   % FR-110 判定字段（S20 C1 捕获率）
out.oracle_increment_pp = oracle_increment_pp;
out.recommendation = recommendation;
out.per_scenario = ps;
out.judge_params = struct('main_scenario', 'S20_composite_voyage', ...
    'oracle_proceed_threshold_pp', INC_PROCEED, ...
    'oracle_limited_threshold_pp', INC_LIMITED, ...
    'n_rows_total', numel(rows), 'n_matrix_rows', sum(isMatrix), ...
    'n_regression_rows', sum(isReg));
out.config_hash = config_hash;
out.generated = datestr(now, 'yyyy-mm-dd HH:MM:SS');
out.notes = 'RQ1~3 机器可读测量：capture_rate 为诊断指标非过门条件；判读归用户（design §5）。C3 使用离线 oracle 前瞻（E5：非实时可实现，仅上界/增量验证）';

ecms.write_json_deep(outJson, out);
fprintf('\n[43] 已写入 %s\n', outJson);

%% ======================= 摘要打印（per_scenario 附列） =======================
fprintf('\n[43] ===== per_scenario（fuel_corr / saving%% / capture_rate / oracle 增量 pp） =====\n');
for s = 1:numel(MAIN_SCNS)
    scn = MAIN_SCNS{s};
    e = ps.(scn);
    fprintf('  %-22s C0=%.2f  DP=%.2f kg\n', scn, e.C0.fuel_corr_kg, e.fuel_dp_kg);
    fprintf('      C1: fuel=%.3f kg  saving=%+6.3f%%  capture=%.4f\n', ...
        e.C1.fuel_corr_kg, e.saving_pct.C1, e.capture_rate.C1);
    fprintf('      C2: fuel=%.3f kg  saving=%+6.3f%%  capture=%.4f\n', ...
        e.C2.fuel_corr_kg, e.saving_pct.C2, e.capture_rate.C2);
    fprintf('      C3: fuel=%.3f kg  saving=%+6.3f%%  capture=%.4f\n', ...
        e.C3.fuel_corr_kg, e.saving_pct.C3, e.capture_rate.C3);
    fprintf('      oracle_increment_pp = %+.4f\n', e.oracle_increment_pp);
end
fprintf('\n[43] Gate 判定完成：recommendation=%s。只产出证据，不触发后续施工。\n', recommendation);

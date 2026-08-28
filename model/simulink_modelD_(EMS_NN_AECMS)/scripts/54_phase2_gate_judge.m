%% ========================================================================
%  54_phase2_gate_judge.m — ECMS Phase 2：Gate 机器判定（T-205，FR-210，design §6）
%  读取 results/ecms_phase2/runs_phase2.csv（53 产物）：
%    主矩阵行（scan='matrix'，controller ∈ {C2,C3,C4}，S20 主判定）；
%    泛化行（scan='generalization'，{C2,C4} × test 航次前 10）。
%  指标（design §6 / FR-210 定案公式）：
%    nn_oracle_capture(S20) = (fuel_C2 − fuel_C4) / (fuel_C2 − fuel_C3)
%      ——NN 对 oracle 增量的捕获率；C2/C3 为 Phase 1 复用值（53 注明 source）。
%    泛化摘要：test 航次上 C4 vs C2 的 saving 差分布
%      saving_diff_pct_i = (fuel_C2_i − fuel_C4_i) / fuel_C2_i × 100
%      报告分布的中位数 / 最小值。
%  recommendation（design §6 精确阈值，不得“调整”改变结论）：
%    nn_oracle_capture ≥ 0.50  → PROCEED_TO_PHASE3
%    0.30 ≤ nn_oracle_capture < 0.50 → PARTIAL
%    nn_oracle_capture < 0.30  → LIMITED（含 NaN：分母非正或数据缺失 → 不裁决）
%  语义：C4（law=7 NN-A-ECMS）为实时可实现预测；C3（law=6 oracle）为离线上界
%  （E5：非实时可实现）。捕获率接近 1 = NN 达到 oracle 的预测价值；≤0 = NN 无增量。
%
%  产出：results/ecms_phase2/gate_result.json
%    （字段：nn_oracle_capture、fuel_c2/c3/c4_S20、generalization{...}、
%      per_scenario、recommendation、judge_params、config_hash、generated、notes）
%  测试钩子（供 54_phase2_gate_judge_unittest.m 构造数据单测判定逻辑）：
%    ECMS_PHASE2_GATE_RUNS_CSV  — 输入 runs_phase2.csv 路径（缺省 .../runs_phase2.csv）
%    ECMS_PHASE2_GATE_OUT_JSON  — 输出 gate_result.json 路径（缺省 .../gate_result.json）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/54_phase2_gate_judge.m'))"
%  注：脚本无本地函数（eval(fileread) 不支持函数定义），全部逻辑内联。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[54] ===== ECMS Phase 2 — Gate 机器判定（T-205，FR-210，design §6） =====\n');
fprintf('[54] ROOT = %s\n', ROOT);

%% ======================= 判定参数（design §6 定案阈值，冻结） =======================
CAP_PROCEED = 0.5;    % nn_oracle_capture ≥ 0.50 → PROCEED_TO_PHASE3
CAP_PARTIAL = 0.3;    % 0.30 ≤ capture < 0.50 → PARTIAL；< 0.30 → LIMITED

%% ======================= 输入 / 输出（环境变量为单测钩子；缺省 = 提交态路径） =======================
inCsv = getenv('ECMS_PHASE2_GATE_RUNS_CSV');
if isempty(inCsv)
    inCsv = fullfile(ROOT, 'results', 'ecms_phase2', 'runs_phase2.csv');
end
outJson = getenv('ECMS_PHASE2_GATE_OUT_JSON');
if isempty(outJson)
    outJson = fullfile(ROOT, 'results', 'ecms_phase2', 'gate_result.json');
end
assert(exist(inCsv, 'file') == 2, 'ECMS:GATE2', '未找到 %s（先运行 53）', inCsv);
fprintf('[54] 输入 runs_phase2.csv = %s\n', inCsv);
fprintf('[54] 输出 gate JSON       = %s\n', outJson);

%% ======================= 解析 runs_phase2.csv（字符串保真，同 43 风格） =======================
HEADER = ['run_id,controller,scenario,law,fuel_raw_kg,fuel_corr_kg,SOC_end,Vdc_min_V,' ...
    'n_undervoltage,n_derate,bat_throughput_kWh,wall_time_s,scan,source,fuel_dp_kg,config_hash'];
fid = fopen(inCsv, 'r', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:GATE2', '无法读取 %s', inCsv);
hdrLine = fgetl(fid);
assert(strcmp(hdrLine, HEADER), 'ECMS:GATE2', ...
    'runs_phase2.csv 表头与 53 不匹配（请先重跑 53；实际: %s）', hdrLine);
rows = {};
l = fgetl(fid);
while ischar(l)
    f = strsplit(l, ',', 'CollapseDelimiters', false);
    assert(numel(f) == 16, 'ECMS:GATE2', 'runs_phase2.csv 行字段数异常: %s', l);
    rows{end + 1} = f; %#ok<SAGROW>
    l = fgetl(fid);
end
fclose(fid);
fprintf('[54] 读取总行数：%d\n', numel(rows));

numv = @(s) str2double(s);
isMatrix = cellfun(@(r) strcmp(r{13}, 'matrix'), rows);
isGen    = cellfun(@(r) strcmp(r{13}, 'generalization'), rows);

%% ======================= 主矩阵：per_scenario（C2/C3/C4；附列 C0） =======================
MAIN_SCNS = {'S20_composite_voyage', 'S01_still_water', 'S02_downstream', 'S07_bridge_maneuver'};
CTRLS = {'C0', 'C2', 'C3', 'C4'};
ps = struct();
for s = 1:numel(MAIN_SCNS)
    scn = MAIN_SCNS{s};
    ent = struct();
    fc = containers.Map();
    for c = 1:numel(CTRLS)
        ctrl = CTRLS{c};
        hit = find(isMatrix & cellfun(@(r) strcmp(r{2}, ctrl) && strcmp(r{3}, scn), rows), 1);
        assert(~isempty(hit), 'ECMS:GATE2', '主矩阵缺行：%s × %s', ctrl, scn);
        r = rows{hit};
        ent.(ctrl) = struct( ...
            'fuel_raw_kg', numv(r{5}), 'fuel_corr_kg', numv(r{6}), ...
            'SOC_end', numv(r{7}), 'Vdc_min_V', numv(r{8}), ...
            'n_undervoltage', numv(r{9}), 'n_derate', numv(r{10}), ...
            'source', r{14});
        fc(ctrl) = numv(r{6});
    end
    fc2 = fc('C2'); fc3 = fc('C3'); fc4 = fc('C4');
    if isfinite(fc2) && isfinite(fc3) && isfinite(fc4) && (fc2 - fc3) > 1e-9
        ent.capture = (fc2 - fc4) / (fc2 - fc3);
    else
        ent.capture = NaN;   % 数据缺失或分母非正（C3 未优于 C2）→ 该场景不裁决
    end
    ps.(scn) = ent;
end

%% ======================= S20 主判定指标 =======================
s20 = ps.S20_composite_voyage;
fc2 = s20.C2.fuel_corr_kg; fc3 = s20.C3.fuel_corr_kg; fc4 = s20.C4.fuel_corr_kg;
if isfinite(s20.capture)
    nn_oracle_capture = s20.capture;
else
    nn_oracle_capture = NaN;
end
fprintf('\n[54] ===== S20 主判定（design §6） =====\n');
fprintf('[54] fuel_corr C2/C3/C4 = %.4f / %.4f / %.4f kg（C2/C3 为 Phase 1 复用，C4 为 law=7 新跑）\n', ...
    fc2, fc3, fc4);
fprintf('[54] nn_oracle_capture = (%.4f − %.4f)/(%.4f − %.4f) = %.4f\n', ...
    fc2, fc4, fc2, fc3, nn_oracle_capture);

%% ======================= 泛化摘要：test 航次 C4 vs C2 saving 差分布 =======================
gen = struct();
genRows = find(isGen);
if numel(genRows) < 2
    fprintf('[54] 警告：泛化行不足（%d 行），泛化摘要为空\n', numel(genRows));
    gen.n_test_voyages = 0;
    gen.saving_diff_pct_median = NaN;
    gen.saving_diff_pct_min = NaN;
    gen.saving_diff_pct_mean = NaN;
    gen.saving_diff_pct_max = NaN;
    gen.per_voyage = {};
    gen.notes = 'generalization 行不足，未形成分布';
else
    voyNames = {};
    for i = 1:numel(genRows)
        if ~any(strcmp(voyNames, rows{genRows(i)}{3}))
            voyNames{end + 1} = rows{genRows(i)}{3}; %#ok<SAGROW>
        end
    end
    savDiff = []; perV = {};   % cell of scalar structs（write_json_deep 不支持 struct 数组）
    for i = 1:numel(voyNames)
        vn = voyNames{i};
        hit2 = find(isGen & cellfun(@(r) strcmp(r{2}, 'C2') && strcmp(r{3}, vn), rows), 1);
        hit4 = find(isGen & cellfun(@(r) strcmp(r{2}, 'C4') && strcmp(r{3}, vn), rows), 1);
        if isempty(hit2) || isempty(hit4)
            fprintf('[54] 警告：%s 缺 C2/C4 配对行（C2=%d C4=%d），剔除出分布\n', vn, ~isempty(hit2), ~isempty(hit4));
            continue;
        end
        f2 = numv(rows{hit2}{6}); f4 = numv(rows{hit4}{6});
        if ~(isfinite(f2) && isfinite(f4) && f2 > 1e-9)
            fprintf('[54] 警告：%s 燃料值无效（C2=%.4g C4=%.4g），剔除出分布\n', vn, f2, f4);
            continue;
        end
        sd = (f2 - f4) / f2 * 100;   % C4 vs C2 相对节油 [%]
        savDiff(end + 1) = sd; %#ok<SAGROW>
        perV{end + 1} = struct('voyage', vn, 'fuel_c2_kg', f2, 'fuel_c4_kg', f4, ...
            'saving_diff_pct', sd); %#ok<SAGROW>
    end
    gen.n_test_voyages = numel(savDiff);
    gen.saving_diff_pct_median = NaN; gen.saving_diff_pct_min = NaN;
    gen.saving_diff_pct_mean = NaN; gen.saving_diff_pct_max = NaN;
    if ~isempty(savDiff)
        gen.saving_diff_pct_median = median(savDiff);
        gen.saving_diff_pct_min = min(savDiff);
        gen.saving_diff_pct_mean = mean(savDiff);
        gen.saving_diff_pct_max = max(savDiff);
    end
    gen.per_voyage = perV;   % cell of scalar structs（每航次一条）
    gen.notes = sprintf('saving_diff_pct_i = (fuel_C2_i − fuel_C4_i)/fuel_C2_i × 100；%d 个 test 航次', numel(savDiff));
end
fprintf('\n[54] ===== 泛化摘要（test 航次 C4 vs C2 saving 差 %） =====\n');
fprintf('[54] n=%d | median=%.3f | min=%.3f | mean=%.3f | max=%.3f\n', ...
    gen.n_test_voyages, gen.saving_diff_pct_median, gen.saving_diff_pct_min, ...
    gen.saving_diff_pct_mean, gen.saving_diff_pct_max);

%% ======================= recommendation（design §6 精确阈值） =======================
if ~isfinite(nn_oracle_capture)
    recommendation = 'LIMITED';
    recNote = 'nn_oracle_capture=NaN（数据缺失或分母非正）→ 不裁决，如实上报';
elseif nn_oracle_capture >= CAP_PROCEED
    recommendation = 'PROCEED_TO_PHASE3';
    recNote = sprintf('capture=%.4f ≥ %.2f', nn_oracle_capture, CAP_PROCEED);
elseif nn_oracle_capture >= CAP_PARTIAL
    recommendation = 'PARTIAL';
    recNote = sprintf('capture=%.4f ∈ [%.2f, %.2f)', nn_oracle_capture, CAP_PARTIAL, CAP_PROCEED);
else
    recommendation = 'LIMITED';
    recNote = sprintf('capture=%.4f < %.2f', nn_oracle_capture, CAP_PARTIAL);
end
fprintf('[54] recommendation = %s（%s；阈值 ≥%.2f→PROCEED，≥%.2f→PARTIAL）\n', ...
    recommendation, recNote, CAP_PROCEED, CAP_PARTIAL);

%% ======================= config_hash（行 config_hash 唯一集 + 判定规则） =======================
md5str = @(s) sprintf('%02x', typecast(...
    java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
uniqH = unique(sort(cellfun(@(r) r{16}, rows, 'UniformOutput', false)));
ruleStr = sprintf('gate_judge_v1;capture_proceed=%.6g;capture_partial=%.6g', CAP_PROCEED, CAP_PARTIAL);
config_hash = md5str([strjoin(uniqH, ';') '|' ruleStr]);

%% ======================= 组装输出 =======================
out = struct();
out.nn_oracle_capture = nn_oracle_capture;
out.fuel_c2_S20_kg = fc2;
out.fuel_c3_S20_kg = fc3;
out.fuel_c4_S20_kg = fc4;
out.generalization = gen;
out.per_scenario = ps;
out.recommendation = recommendation;
out.judge_params = struct('main_scenario', 'S20_composite_voyage', ...
    'formula', 'nn_oracle_capture = (fuel_C2 − fuel_C4)/(fuel_C2 − fuel_C3)（S20）', ...
    'capture_proceed_threshold', CAP_PROCEED, ...
    'capture_partial_threshold', CAP_PARTIAL, ...
    'k_eq_kg_kWh', 0.4774, 'SOC_target', 0.60, ...
    'n_rows_total', numel(rows), 'n_matrix_rows', sum(isMatrix), ...
    'n_generalization_rows', sum(isGen), ...
    'generalization_metric', 'C4 vs C2 saving diff: (fuel_C2−fuel_C4)/fuel_C2×100 per test voyage');
out.config_hash = config_hash;
out.generated = datestr(now, 'yyyy-mm-dd HH:MM:SS');
out.notes = ['FR-210 机器可读判定：nn_oracle_capture（S20）为主判定指标，泛化摘要为附列。' ...
    'C3（law=6 oracle）为离线上界（E5：非实时可实现）；C4（law=7 NN-A-ECMS）为实时可实现预测。' ...
    'recommendation 仅依据 nn_oracle_capture；判读归用户（design §6 注）。' recNote];

ecms.write_json_deep(outJson, out);
fprintf('\n[54] 已写入 %s\n', outJson);

%% ======================= 摘要打印（per_scenario 附列） =======================
fprintf('\n[54] ===== per_scenario（fuel_corr kg / capture） =====\n');
for s = 1:numel(MAIN_SCNS)
    scn = MAIN_SCNS{s};
    e = ps.(scn);
    fprintf('  %-22s C2=%.4f C3=%.4f C4=%.4f | capture=%.4f\n', scn, ...
        e.C2.fuel_corr_kg, e.C3.fuel_corr_kg, e.C4.fuel_corr_kg, e.capture);
end
fprintf('\n[54] Gate 判定完成：recommendation=%s。只产出证据，不触发后续施工。\n', recommendation);

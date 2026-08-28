%% ========================================================================
%  35_phase0_gate_judge.m — ECMS Phase 0：Gate 机器判定（T-014，FR-013）
%  读取 runs.csv，只用 scan='matrix' 且 mode='continuous' 的行；
%  P_min = 400 kW 档为主判定配置，其余 P_min 档在 reference_by_pmin 中给出参考值。
%  判定映射（design §7 / FR-013，D7 + D10 三档稳健性）：
%    GO_ROBUST      : 保守档 tier1（d25=+30%）≥4/8 场景 saving_pct_corr ≥5%
%                     且主判定配置无非平凡性标记（SOC 贴下限滑行）
%    NO-GO          : 三档 × 全部 P_min 档连续口径平均 saving 全部 <2%
%    DECISION_REQUIRED : 其余（tier 按最高达标档标记 ROBUST/CONDITIONAL/NONE）
%  gate 只产出证据（gate_result.json），不触发任何后续施工。
%  聚合单机口径声明写入 notes：双机独立调度（N_on∈{0,1,2}）为未探索自由度。
%
%  测试钩子（供 35_phase0_gate_judge_unittest.m 构造数据单测判定逻辑）：
%    ECMS_GATE_RUNS_CSV — 输入 runs.csv 路径（缺省 results/ecms_phase0/runs.csv）
%    ECMS_GATE_OUT_JSON — 输出 gate_result.json 路径（缺省 .../gate_result.json）
%
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/35_phase0_gate_judge.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[35] ===== ECMS Phase 0 — Gate 机器判定（T-014，FR-013） =====\n');
fprintf('[35] ROOT = %s\n', ROOT);

cfg = Ecms_Phase0_Config();

% ---- 判定参数（design §7 / FR-013；不得“调整”阈值改变结论）----
MAIN_PMIN  = 400;   % 主判定最低稳定负荷档 [kW]（任务指定）
GE5_THRESH = 5;     % 场景节油率达标阈值 [%]
GE5_COUNT  = 4;     % 达标场景数阈值 [/8]
LT2_THRESH = 2;     % NO-GO 平均节油率阈值 [%]

% ---- 输入 / 输出（环境变量为单测钩子；缺省 = 提交态路径）----
inCsv = getenv('ECMS_GATE_RUNS_CSV');
if isempty(inCsv)
    inCsv = fullfile(ROOT, cfg.output_root, 'runs.csv');
end
outJson = getenv('ECMS_GATE_OUT_JSON');
if isempty(outJson)
    outJson = fullfile(ROOT, cfg.output_root, 'gate_result.json');
end
assert(exist(inCsv, 'file') == 2, 'ECMS:GATE', '未找到 %s', inCsv);
fprintf('[35] 输入 runs.csv   = %s\n', inCsv);
fprintf('[35] 输出 gate JSON  = %s\n', outJson);

%% ======================= 解析 runs.csv（字符串保真，同 33 风格） =======================
HEADER = ['run_id,scenario,tier,mode,P_min_kW,R_gen_kWps,Bat_Cap_kWh,C_start_kg,' ...
    'fuel_dp_kg,fuel_base_raw_kg,fuel_base_corr_kg,saving_pct_raw,saving_pct_corr,' ...
    'feasible,E_unserved_kWh,E_unserved_gt0,baseline_feasible,nontrivial_suspect,' ...
    'wall_time_s,checks_pass,infeasible_reason,config_hash,scan'];
fid = fopen(inCsv, 'r', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:GATE', '无法读取 %s', inCsv);
hdrLine = fgetl(fid);
assert(strcmp(hdrLine, HEADER), 'ECMS:GATE', 'runs.csv 表头与 32 不匹配（请先重跑 32）');
rows = {};
l = fgetl(fid);
while ischar(l)
    f = strsplit(l, ',', 'CollapseDelimiters', false);
    assert(numel(f) == 23, 'ECMS:GATE', 'runs.csv 行字段数异常: %s', l);
    rows{end+1} = f; %#ok<AGROW>
    l = fgetl(fid);
end
fclose(fid);
fprintf('[35] 读取总行数：%d\n', numel(rows));

numv    = @(s) str2double(s);
feasOf  = @(r) strcmp(r{14}, '1');
savOf   = @(r) numv(r{13});
mainAt  = @(r) abs(numv(r{5}) - MAIN_PMIN) < 1e-9;

%% ======================= 过滤：scan=matrix & mode=continuous =======================
isGate = cellfun(@(r) strcmp(r{23}, 'matrix') && strcmp(r{4}, 'continuous'), rows);
gate = rows(isGate);
nGate = numel(gate);
assert(nGate > 0, 'ECMS:GATE', 'gate 子集为空：scan=matrix & mode=continuous 无行');
assert(any(cellfun(@(r) mainAt(r), gate)), 'ECMS:GATE', ...
    'gate 子集缺主判定 P_min=%d kW 档', MAIN_PMIN);
fprintf('[35] gate 子集：%d 行（scan=matrix, mode=continuous）\n', nGate);

%% ======================= criteria =======================
% 1) 三档 ≥5% 场景数（P_min=400，去重场景）
ge5count = zeros(1, 3);
for tier = 1:3
    scnHit = {};
    for i = 1:nGate
        r = gate{i};
        if numv(r{3}) == tier && mainAt(r) && feasOf(r) && ~isnan(savOf(r)) && ...
                savOf(r) >= GE5_THRESH
            scnHit{end+1} = r{2}; %#ok<AGROW>
        end
    end
    ge5count(tier) = numel(unique(scnHit));
end
fprintf('[35] ≥%.0f%% 场景数（P_min=%d）：tier1=%d  tier2=%d  tier3=%d（/8）\n', ...
    GE5_THRESH, MAIN_PMIN, ge5count(1), ge5count(2), ge5count(3));

% 2) 三档 × 全 P_min 档连续口径平均 saving 是否全 <2%（NO-GO 判据）
pminVals = sort(unique(cellfun(@(r) numv(r{5}), gate)));
allLT2 = true;
for tier = 1:3
    for pm = pminVals(:).'
        idx = cellfun(@(r) numv(r{3}) == tier && abs(numv(r{5}) - pm) < 1e-9 && ...
            feasOf(r) && ~isnan(savOf(r)), gate);
        if any(idx)
            m = mean(cellfun(@(r) savOf(r), gate(idx)));
            if ~(m < LT2_THRESH), allLT2 = false; end
        else
            allLT2 = false;    % 无可行实例 → 视为不满足“全 <2%”
        end
    end
end
fprintf('[35] 三档×全 P_min 档连续口径平均 saving 全 <%.0f%%：%d\n', LT2_THRESH, allLT2);

% 3) 非平凡性：主判定配置（P_min=400）可行实例均无非平凡性标记
idxM = cellfun(@(r) mainAt(r) && feasOf(r), gate);
nontrivPass = true;
if any(idxM)
    nontrivPass = all(cellfun(@(r) strcmp(r{18}, '0') || strcmp(r{18}, ''), gate(idxM)));
end
fprintf('[35] 主判定配置无非平凡性标记：%d\n', nontrivPass);

%% ======================= overall / tier 判定 =======================
if ge5count(1) >= GE5_COUNT && nontrivPass
    overall = 'GO_ROBUST';
elseif allLT2
    overall = 'NO-GO';
else
    overall = 'DECISION_REQUIRED';
end
if ge5count(1) >= GE5_COUNT
    tierLvl = 'ROBUST';
elseif max(ge5count(2:3)) >= GE5_COUNT
    tierLvl = 'CONDITIONAL';
else
    tierLvl = 'NONE';
end
fprintf('\n[35] ===== 判定结果 =====\n');
fprintf('[35] overall = %s   |   criteria.tier = %s\n', overall, tierLvl);

%% ======================= per_scenario（主判定配置 P_min=400） =======================
ps = struct();
for s = 1:numel(cfg.scenarios)
    scn = cfg.scenarios{s};
    ent = struct();
    for tier = 1:3
        hit = find(cellfun(@(r) strcmp(r{2}, scn) && numv(r{3}) == tier && mainAt(r), gate), 1);
        if ~isempty(hit) && feasOf(gate{hit})
            ent.(sprintf('tier%d', tier)) = savOf(gate{hit});
        else
            ent.(sprintf('tier%d', tier)) = NaN;
        end
    end
    ps.(scn) = ent;
end

%% ======================= reference_by_pmin（全部 P_min 档参考值） =======================
ref = struct();
for pm = pminVals(:).'
    ent = struct();
    for tier = 1:3
        idx = cellfun(@(r) numv(r{3}) == tier && abs(numv(r{5}) - pm) < 1e-9 && ...
            feasOf(r) && ~isnan(savOf(r)), gate);
        if any(idx)
            ent.(sprintf('tier%d_mean_pct', tier)) = mean(cellfun(@(r) savOf(r), gate(idx)));
        else
            ent.(sprintf('tier%d_mean_pct', tier)) = NaN;
        end
    end
    ref.(sprintf('pmin%d', pm)) = ent;
end

%% ======================= config_hash =======================
md5str = @(s) sprintf('%02x', typecast(...
    java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
uniqH = unique(sort(cellfun(@(r) r{22}, gate, 'UniformOutput', false)));
ruleStr = sprintf('gate_judge_v1;mainPmin=%.6g;ge5=%.6g;ge5count=%d;lt2=%.6g', ...
    MAIN_PMIN, GE5_THRESH, GE5_COUNT, LT2_THRESH);
config_hash = md5str([strjoin(uniqH, ';') '|' ruleStr]);

%% ======================= 组装输出 =======================
out = struct();
out.overall = overall;
out.criteria = struct('tier', tierLvl, ...
    'robust_ge5pct_scenarios', ge5count(1), ...
    'mid_ge5pct_scenarios', ge5count(2), ...
    'severe_ge5pct_scenarios', ge5count(3), ...
    'all_configs_lt_2pct', allLT2, ...
    'nontrivial_check_pass', nontrivPass);
out.per_scenario = ps;
out.reference_by_pmin = ref;
out.judge_params = struct('main_pmin_kW', MAIN_PMIN, 'ge5_threshold_pct', GE5_THRESH, ...
    'ge5_count_threshold', GE5_COUNT, 'lt2_threshold_pct', LT2_THRESH, ...
    'n_rows_used', nGate, 'n_rows_total', numel(rows));
out.config_hash = config_hash;
out.generated = datestr(now, 'yyyy-mm-dd HH:MM:SS');
out.notes = '聚合单机口径；双机独立调度为未探索自由度（brainstorm D8）';

ecms.write_json_deep(outJson, out);
fprintf('[35] 已写入 %s\n', outJson);

%% ======================= 摘要打印 =======================
fprintf('\n[35] ===== 主判定配置（P_min=%d kW，连续口径）场景节油率 per_scenario =====\n', MAIN_PMIN);
for s = 1:numel(cfg.scenarios)
    scn = cfg.scenarios{s};
    e = ps.(scn);
    fprintf('  %-24s tier1=%6.2f%%  tier2=%6.2f%%  tier3=%6.2f%%\n', scn, ...
        e.tier1, e.tier2, e.tier3);
end
fprintf('\n[35] Gate 判定完成：overall=%s，tier=%s。只产出证据，不触发后续施工。\n', overall, tierLvl);

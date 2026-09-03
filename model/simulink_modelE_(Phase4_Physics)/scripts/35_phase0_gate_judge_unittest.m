%% ========================================================================
%  35_phase0_gate_judge_unittest.m — Gate 判定逻辑单测（T-014，FR-013）
%  构造 4 个迷你 runs.csv（仅 continuous/matrix 行，触达全部判定分支）：
%    case A: GO_ROBUST                 —— tier1 ≥4/8 场景 ≥5% 且非平凡
%    case B: NO-GO                     —— 三档 × 全 P_min 档平均 saving 全 <2%
%    case C: DECISION_REQUIRED (NONE)  —— 无任何档 ≥4/8，且存在 ≥2% 配置
%    case D: DECISION_REQUIRED (CONDITIONAL) —— 仅中/高档 ≥4/8
%  通过环境变量 ECMS_GATE_RUNS_CSV / ECMS_GATE_OUT_JSON 驱动
%  35_phase0_gate_judge.m（eval(fileread) 执行生产脚本本体），
%  再 jsondecode 输出产物断言。任一用例失败 → error（非零退出）。
%  注：脚本无本地函数（eval(fileread) 不支持函数定义），全部逻辑内联。
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && matlab -batch \
%      "eval(fileread('scripts/35_phase0_gate_judge_unittest.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[35-UT] ===== Gate 判定逻辑单测（T-014） =====\n');
cfg   = Ecms_Phase0_Config();
utDir = fullfile(ROOT, cfg.output_root, 'ext_2d', '_gate_ut');
if ~exist(utDir, 'dir'), mkdir(utDir); end

HEADER = ['run_id,scenario,tier,mode,P_min_kW,R_gen_kWps,Bat_Cap_kWh,C_start_kg,' ...
    'fuel_dp_kg,fuel_base_raw_kg,fuel_base_corr_kg,saving_pct_raw,saving_pct_corr,' ...
    'feasible,E_unserved_kWh,E_unserved_gt0,baseline_feasible,nontrivial_suspect,' ...
    'wall_time_s,checks_pass,infeasible_reason,config_hash,scan'];

% ---- 行构造匿名函数：savFcn(tier, scnIdx) → saving_pct_corr ----
mkRow = @(scn, tier, pm, savC, k) { ...
    sprintf('%s_tier%d_continuous_pmin%03d_ut%d', scn, tier, pm, k), scn, ...
    num2str(tier), 'continuous', num2str(pm), '40', '1071.36', 'NaN', ...
    '80', '79', '83', num2str(savC), num2str(savC), '1', '0', '0', '1', '0', ...
    '0.5', '1', '', sprintf('ut_hash_%d', k), 'matrix'};
rowToStr = @(row) strjoin(row, ',');

% ---- 4 个用例定义（pmins/tiers/sav 函数/期望值）----
cases = {
    struct('name', 'A_GO_ROBUST', 'pmins', [400], 'tiers', [1 2 3], ...
        'sav', @(t, s) 6.0 + 0.1 * t + 0.05 * mod(s, 3), ...
        'expOverall', 'GO_ROBUST', 'expTier', 'ROBUST', ...
        'expGe5', [8 8 8], 'expLT2', false, 'expNT', true);
    struct('name', 'B_NO_GO', 'pmins', [0 400], 'tiers', [1 2 3], ...
        'sav', @(t, s) 1.0, ...
        'expOverall', 'NO-GO', 'expTier', 'NONE', ...
        'expGe5', [0 0 0], 'expLT2', true, 'expNT', true);
    struct('name', 'C_DECISION_NONE', 'pmins', [400], 'tiers', [1 2 3], ...
        'sav', @(t, s) 3.0, ...
        'expOverall', 'DECISION_REQUIRED', 'expTier', 'NONE', ...
        'expGe5', [0 0 0], 'expLT2', false, 'expNT', true);
    struct('name', 'D_DECISION_CONDITIONAL', 'pmins', [400], 'tiers', [1 2 3], ...
        'sav', @(t, s) (t >= 2 && s <= 4) * 6.0 + 3.0, ...
        'expOverall', 'DECISION_REQUIRED', 'expTier', 'CONDITIONAL', ...
        'expGe5', [0 4 4], 'expLT2', false, 'expNT', true)
    };

for ci = 1:numel(cases)
    cs = cases{ci};

    % ---- 构造迷你 runs.csv ----
    R = {};
    k = 0;
    for tier = cs.tiers
        for pm = cs.pmins
            for s = 1:numel(cfg.scenarios)
                k = k + 1;
                R{end+1} = mkRow(cfg.scenarios{s}, tier, pm, cs.sav(tier, s), k); %#ok<SAGROW>
            end
        end
    end
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
    setenv('ECMS_GATE_RUNS_CSV', csvPath);
    setenv('ECMS_GATE_OUT_JSON', jsonPath);
    eval(fileread(fullfile(ROOT, 'scripts', '35_phase0_gate_judge.m')));
    assert(exist(jsonPath, 'file') == 2, 'ECMS:UT', 'case %s：gate 未产出 %s', cs.name, jsonPath);
    res = jsondecode(fileread(jsonPath));

    % ---- 断言 ----
    ge5 = [res.criteria.robust_ge5pct_scenarios, res.criteria.mid_ge5pct_scenarios, ...
        res.criteria.severe_ge5pct_scenarios];
    assert(strcmp(res.overall, cs.expOverall), 'ECMS:UT', ...
        'case %s：overall=%s ≠ %s', cs.name, res.overall, cs.expOverall);
    assert(strcmp(res.criteria.tier, cs.expTier), 'ECMS:UT', ...
        'case %s：tier=%s ≠ %s', cs.name, res.criteria.tier, cs.expTier);
    assert(isequal(ge5, cs.expGe5), 'ECMS:UT', ...
        'case %s：ge5=[%d %d %d] ≠ [%d %d %d]', cs.name, ge5, cs.expGe5);
    assert(res.criteria.all_configs_lt_2pct == cs.expLT2, 'ECMS:UT', ...
        'case %s：all_configs_lt_2pct=%d ≠ %d', cs.name, ...
        res.criteria.all_configs_lt_2pct, cs.expLT2);
    assert(res.criteria.nontrivial_check_pass == cs.expNT, 'ECMS:UT', ...
        'case %s：nontrivial_check_pass=%d ≠ %d', cs.name, ...
        res.criteria.nontrivial_check_pass, cs.expNT);
    fprintf('[35-UT][PASS] case %-22s overall=%-18s tier=%s ge5=[%d %d %d]\n', ...
        cs.name, res.overall, res.criteria.tier, ge5(1), ge5(2), ge5(3));
end

fprintf('\n[35-UT] Gate 判定逻辑单测全部通过（4 用例：GO_ROBUST / NO-GO / DECISION_REQUIRED×2）。\n');

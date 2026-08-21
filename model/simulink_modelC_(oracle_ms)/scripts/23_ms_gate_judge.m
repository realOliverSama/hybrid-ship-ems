%% ========================================================================
%  23_ms_gate_judge.m — Phase 1 GO/NO-GO 机器判定（FR-014，NO-GO 路径）
%  Spec: multistep-predictive-ems
%  T-011: 基于 T-008B 机理门禁 + T-009-lite 稀疏域确认 + Phase 1A 全量，
%         用户裁决直接形成 Phase 1 NO-GO 判定。
%  ----------------------------------------------------------------
%  【关键语义】NO-GO 是合法判定结果，脚本正常退出（exit 0）；
%  仅当数据缺失/不完整/自相矛盾时 error('GATE:MSFF:*') 终止（exit != 0）。
%  ----------------------------------------------------------------
%  判据（FR-014）：
%    非劣化约束（逐 C0/CMS 配对，全部配对必须 PASS）：
%      ① v_rmse 相对退化 ≤ 1%
%      ② |ΔSOC_end| ≤ 0.005
%      ③ fuel ≤ C0×1.015
%      ④ E_unserved = 0
%      ⑤ 不得产生 Baseline 没有的欠压事件（N_UV@675：CMS ≤ C0）
%    收益判据：λ_P×R_gen 网格 ≥4 个相邻组合 × ≥2 场景同时满足
%      ⑥ P_bat,peak↓ ≥15% 且 ⑦ (P_bat,RMS↓≥10% 或 吞吐量↓≥10%)
%      ⑧ 母线压力区工况（C0 的 Vdc_min<700）附加 Vdc_min 改善检查
%  GO = 非劣化全 PASS 且收益成立；否则 NO-GO。
%
%  输出:
%    results/ms_phase1/gate_result.json  机器可读判定
%    results/ms_phase1/gate_judge_<ts>.log 执行日志
%  执行: cd model/simulink_modelC_(oracle_ms) && matlab -batch "eval(fileread('scripts/23_ms_gate_judge.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'results'),'dir'), ROOT = pwd; end

OUTDIR = fullfile(ROOT,'results','ms_phase1');
if ~exist(OUTDIR,'dir'), error('GATE:MSFF:OUTDIR 缺失: %s', OUTDIR); end
TS = datestr(now,'yyyymmdd_HHMMSS');
LOGF = fullfile(OUTDIR, sprintf('gate_judge_%s.log', TS));
fid = fopen(LOGF,'w');
logf = @(fmt,varargin) fprintf(fid, fmt, varargin{:});
logp = @(s) fprintf('[GATE] %s\n', s);

% ---------------- 数据完整性 ----------------
CSVS = {
    fullfile(OUTDIR,'phase1A','runs.csv')            'Phase 1A 全量(78)'
    fullfile(OUTDIR,'phase1B_lite','runs.csv')       'Phase 1B-lite 稀疏域(64)'
    fullfile(OUTDIR,'t008b','runs.csv')              'T-008B 弱机点(8)'
};
dataOk = true;
for i = 1:size(CSVS,1)
    if ~exist(CSVS{i,1},'file')
        logp(sprintf('数据缺失: %s (%s)', CSVS{i,1}, CSVS{i,2}));
        dataOk = false;
    end
end
if ~dataOk
    error('GATE:MSFF:DATA 数据不完整，无法判定（详见 %s）', LOGF);
end

% ---------------- 读取 ----------------
T1  = readtable(CSVS{1,1}, 'TextType','string');   % Phase 1A
T2  = readtable(CSVS{2,1}, 'TextType','string');   % lite
T3  = readtable(CSVS{3,1}, 'TextType','string');   % t008b
if height(T1) ~= 78, error('GATE:MSFF:DATA Phase 1A 行数=%d ≠ 78', height(T1)); end
if height(T2) ~= 64, error('GATE:MSFF:DATA lite 行数=%d ≠ 64', height(T2)); end
if height(T3) ~= 8,  error('GATE:MSFF:DATA t008b 行数=%d ≠ 8',  height(T3)); end

% ---------------- 非劣化约束（逐 C0/CMS 配对） ----------------
% 配对构造：
%  Phase 1A: key=(anchor, scenario, horizon)，C0 按 (anchor, scenario) 基线
%  lite:     key=(lambda_P, R_gen, scenario)，C0 基线，CMS H45
%  t008b:    key=(scenario, horizon)，C0 基线
violations = {};  % {exp_id, criterion, detail}
pairs = 0; pairsPass = 0;

pairDatasets = {'phase1A', T1; 'lite', T2; 't008b', T3};
for di = 1:size(pairDatasets,1)
    dsName = pairDatasets{di,1};
    T = pairDatasets{di,2};
    % C0 行
    c0rows = T(T.controller == "C0", :);
    cmsrows = T(T.controller == "CMS", :);
    for k = 1:height(cmsrows)
        r = cmsrows(k,:);
        if dsName == "phase1A"
            m = c0rows(c0rows.anchor == r.anchor & c0rows.scenario == r.scenario, :);
        elseif dsName == "lite"
            m = c0rows(c0rows.lambda_P == r.lambda_P & c0rows.R_gen == r.R_gen & c0rows.scenario == r.scenario, :);
        else % t008b
            m = c0rows(c0rows.scenario == r.scenario, :);
        end
        if isempty(m)
            error('GATE:MSFF:DATA %s 行 %s 无对应 C0', dsName, r.exp_id);
        end
        c0 = m(1,:);
        pairs = pairs + 1;
        ok = true; vlist = {};
        % ① v_rmse 相对退化 ≤1%
        if isfinite(r.v_rmse_mps) && isfinite(c0.v_rmse_mps) && c0.v_rmse_mps > 1e-9
            rel = (r.v_rmse_mps - c0.v_rmse_mps) / c0.v_rmse_mps;
            if rel > 0.01
                ok = false; vlist{end+1} = sprintf('v_rmse rel=%.4f', rel); %#ok<AGROW>
            end
        end
        % ② |ΔSOC_end| ≤ 0.005
        if isfinite(r.SOC_end) && isfinite(c0.SOC_end)
            if abs(r.SOC_end - c0.SOC_end) > 0.005
                ok = false; vlist{end+1} = sprintf('|dSOC_end|=%.4f', abs(r.SOC_end-c0.SOC_end)); %#ok<AGROW>
            end
        end
        % ③ fuel ≤ C0×1.015
        if isfinite(r.fuel_kg) && isfinite(c0.fuel_kg)
            if r.fuel_kg > c0.fuel_kg * 1.015
                ok = false; vlist{end+1} = sprintf('fuel %.3f vs C0 %.3f', r.fuel_kg, c0.fuel_kg); %#ok<AGROW>
            end
        end
        % ④ E_unserved = 0
        if isfinite(r.E_unserved_kWh) && r.E_unserved_kWh > 1e-9
            ok = false; vlist{end+1} = sprintf('E_unserved=%.4f', r.E_unserved_kWh); %#ok<AGROW>
        end
        % ⑤ 无新增欠压（N_UV@675）
        if isfinite(r.N_UV) && isfinite(c0.N_UV)
            if r.N_UV > c0.N_UV
                ok = false; vlist{end+1} = sprintf('N_UV %d > C0 %d', r.N_UV, c0.N_UV); %#ok<AGROW>
            end
        end
        if ok
            pairsPass = pairsPass + 1;
        else
            violations{end+1} = sprintf('%s/%s: %s', dsName, r.exp_id, strjoin(vlist,'; ')); %#ok<AGROW>
        end
    end
end
logp(sprintf('非劣化: pairs_total=%d pairs_pass=%d violations=%d', pairs, pairsPass, numel(violations)));

% ---------------- 收益判据（如实统计，预期 0） ----------------
% 在 λ_P×R_gen 网格上：对每个 (point, scenario)，CMS(H=45, traj) vs C0
%   ⑥ ΔP_bat_peak = (r.P_bat_peak_abs_kW - c0.P_bat_peak_abs_kW)/c0.P_bat_peak_abs_kW ≤ -0.15
%   ⑦ ΔP_bat_rms ≤ -0.10 或 Δthroughput ≤ -0.10
% 统计满足的 (组合×场景)；再统计相邻（四连通）组合数 ≥4 且场景数 ≥2
T = T2;  % lite 网格
c0rows = T(T.controller == "C0", :);
cmsrows = T(T.controller == "CMS", :);
lamU = sort(unique(T.lambda_P)); RgU = sort(unique(T.R_gen));
benefitCells = {};  % {scenario, lambda_P, R_gen}
scenU = unique(T.scenario);
for si = 1:numel(scenU)
    sc = scenU(si);
    for li = 1:numel(lamU)
        for ri = 1:numel(RgU)
            c0 = c0rows(c0rows.lambda_P==lamU(li) & c0rows.R_gen==RgU(ri) & c0rows.scenario==sc, :);
            cm = cmsrows(cmsrows.lambda_P==lamU(li) & cmsrows.R_gen==RgU(ri) & cmsrows.scenario==sc, :);
            if isempty(c0) || isempty(cm), continue; end
            c0 = c0(1,:); cm = cm(1,:);
            if ~isfinite(c0.P_bat_peak_abs_kW) || ~isfinite(cm.P_bat_peak_abs_kW), continue; end
            dPk = (cm.P_bat_peak_abs_kW - c0.P_bat_peak_abs_kW) / c0.P_bat_peak_abs_kW;
            dRms = (cm.P_bat_rms_kW - c0.P_bat_rms_kW) / c0.P_bat_rms_kW;
            dThr = (cm.E_bat_throughput_kWh - c0.E_bat_throughput_kWh) / c0.E_bat_throughput_kWh;
            c6 = dPk <= -0.15;
            c7 = (dRms <= -0.10) || (dThr <= -0.10);
            if c6 && c7
                benefitCells{end+1} = struct('scenario', sc, 'lambda_P', lamU(li), 'R_gen', RgU(ri), ...
                    'dPk', dPk, 'dRms', dRms, 'dThr', dThr); %#ok<AGROW>
            end
        end
    end
end
% 相邻（四连通）组合计数：对每个场景，在网格上找满足条件的最大的四连通分量
% 简化：count adjacent pairs of qualifying cells per scenario
scenQualify = 0;  % 满足 ≥4 相邻组合的场景数
totalAdjCombos = 0;
for si = 1:numel(scenU)
    sc = scenU(si);
    if isempty(benefitCells)
        continue;
    end
    cells = benefitCells(strcmp({benefitCells.scenario}, sc));
    if isempty(cells), continue; end
    % 构造网格 mask
    [lamG, RgG] = meshgrid(lamU, RgU);
    mask = zeros(size(lamG));
    for c = 1:numel(cells)
        mask(RgU==cells(c).R_gen, lamU==cells(c).lambda_P) = 1;
    end
    % 四连通分量（简单 flood-fill）
    visited = zeros(size(mask));
    maxComp = 0;
    for ii = 1:numel(mask)
        if mask(ii) && ~visited(ii)
            % BFS
            q = ii; visited(ii) = 1; comp = 0;
            [rr,cc] = ind2sub(size(mask), ii);
            while ~isempty(q)
                cur = q(1); q(1) = [];
                comp = comp + 1;
                [r0,c0_] = ind2sub(size(mask), cur);
                for d = [-1 1]
                    if r0+d >= 1 && r0+d <= size(mask,1) && mask(r0+d,c0_) && ~visited(r0+d,c0_)
                        visited(r0+d,c0_) = 1; q(end+1) = sub2ind(size(mask), r0+d, c0_); %#ok<AGROW>
                    end
                    if c0_+d >= 1 && c0_+d <= size(mask,2) && mask(r0,c0_+d) && ~visited(r0,c0_+d)
                        visited(r0,c0_+d) = 1; q(end+1) = sub2ind(size(mask), r0, c0_+d); %#ok<AGROW>
                    end
                end
            end
            maxComp = max(maxComp, comp);
        end
    end
    if maxComp >= 4
        scenQualify = scenQualify + 1;
    end
    totalAdjCombos = totalAdjCombos + maxComp;
    logf('收益判据: 场景 %s 最大四连通分量=%d\n', sc, maxComp);
end
benefitPass = (scenQualify >= 2);
logp(sprintf('收益判据: 满足组合数=%d, 满足场景数=%d (要求 ≥4 组合 × ≥2 场景) -> %s', ...
    numel(benefitCells), scenQualify, string(benefitPass)));

% ---------------- 机理证据（记录项，非判据） ----------------
mech = struct();
mech.positive_control = struct( ...
    'desc', '合成 1900 kW 未来阶跃（超出 gen 1000 + 电池 1000 包络）@Nominal CMS H45', ...
    'max_dP_gen_actual_kW', 190.9, ...
    'first_divergence_t_s', 255.4, ...
    'ff_lead_s', 45.0, ...
    'ff_star_kW', 192, ...
    'pre_ramp_kW', [709 900]);
mech.lite_grid = struct( ...
    'points_total', 16, 'points_strict_zero', 14, ...
    'corner_transient_kW', 9.212807, ...
    'corner_transient_mechanism', 'ff 释放时 τ=10 惯性回溢 ~15 s 噪声级瞬态（Δfuel=+0.013% 负收益）');
mech.envelope_margin = struct( ...
    'margin_min_range_kW', [-160 -80 0 200], ...
    'margin_mean_range_kW', [892 1353], ...
    'negative_margin_overlap_ff', '负裕度仅出现在冷启动 f_V 塌缩段且与 ff 激活窗口 100% 重叠', ...
    'contradictions', 0);

% ---------------- 判定 ----------------
% 数据完整性已检查；若 violations 非空 -> NO-GO（标注）；否则进入收益判定
noninferPass = (numel(violations) == 0);
if ~noninferPass
    verdict = 'NO-GO';
    verdictReason = '非劣化约束存在 violation（详见 violations 列表）';
elseif ~benefitPass
    verdict = 'NO-GO';
    verdictReason = '收益判据不成立（有效工作区域为空）';
else
    verdict = 'GO';
    verdictReason = '非劣化全 PASS 且收益判据成立';
end

result = struct();
result.verdict = verdict;
result.verdict_reason = verdictReason;
result.noninferiority = struct( ...
    'pairs_total', pairs, 'pairs_pass', pairsPass, ...
    'violations', {violations});
result.benefit = struct( ...
    'combos_qualifying', numel(benefitCells), ...
    'scenarios_qualifying', scenQualify, ...
    'threshold', '>=4 combos x >=2 scenarios', ...
    'pass', benefitPass, ...
    'qualifying_cells', {benefitCells});
result.mechanism_evidence = mech;
result.data_sources = struct( ...
    'phase1A', sprintf('%s (%d 行)', CSVS{1,1}, height(T1)), ...
    'lite', sprintf('%s (%d 行)', CSVS{2,1}, height(T2)), ...
    't008b', sprintf('%s (%d 行)', CSVS{3,1}, height(T3)), ...
    'repro_gate', 'results/ms_phase1/repro/v5_compare_20260821_082030.log (R1-R4 全 PASS)', ...
    'positive_control', 'results/ms_phase1/t008b/positive_control_S07_H45.mat');
[~, sha] = system('git rev-parse --short HEAD');
result.generated_at = datestr(now,'yyyy-mm-dd HH:MM:SS');
result.git_sha = strtrim(sha);

jsonPath = fullfile(OUTDIR,'gate_result.json');
json = jsonencode(result);
fid2 = fopen(jsonPath,'w');
fprintf(fid2, '%s\n', json);
fclose(fid2);

% ---------------- stdout + log ----------------
logp(sprintf('判定: %s', verdict));
logp(sprintf('原因: %s', verdictReason));
logp(sprintf('gate_result.json: %s', jsonPath));
fprintf('\n=== GATE RESULT ===\n');
fprintf('verdict: %s\n', verdict);
fprintf('noninferiority: pairs_total=%d pairs_pass=%d\n', pairs, pairsPass);
fprintf('benefit: combos=%d scenarios=%d pass=%s\n', numel(benefitCells), scenQualify, string(benefitPass));
fprintf('mechanism: positive_control_max_dP=%.1f kW, lite_strict_zero=%d/16\n', mech.positive_control.max_dP_gen_actual_kW, mech.lite_grid.points_strict_zero);
fprintf('json: %s\n', jsonPath);
fclose(fid);

% 输出到 stdout 的同时写入日志文件（含完整 violations 明细）
fid3 = fopen(LOGF,'a');
fprintf(fid3, '\n=== GATE RESULT ===\n');
fprintf(fid3, 'verdict: %s\n', verdict);
fprintf(fid3, 'reason: %s\n', verdictReason);
for v = 1:numel(violations)
    fprintf(fid3, 'violation: %s\n', violations{v});
end
fclose(fid3);
fprintf('[GATE] 判定完成，日志: %s\n', LOGF);

%% ========================================================================
%  33_run_singlefactor_sweeps.m — ECMS Phase 0：单因素敏感性扫描（T-011，FR-012）
%  固定：tier2（中档 d25=+50%）+ P_min=400 kW + 8 场景
%    ramp  : R_gen ∈ {20, 80} kW/s（基准 40 已在主矩阵）——连续口径
%    batcap: Bat_Cap ∈ {643, 1500} kWh（基准 1071.36 已在主矩阵）——连续口径
%            （SOC 网格能量边界随容量同步缩放，1050 kW 功率限值不变）
%    cstart: C_start ∈ {1, 5} kg（基准 2 已在主矩阵）——启停口径
%  结果追加到 runs.csv（scan 列 = matrix/ramp/batcap/cstart；基准点不重复计入）
%  重入：重跑 33 会重建全部扫描行（matrix 行保持原样）；重跑 32 后需重跑 33 恢复。
%  产出：
%    results/ecms_phase0/runs/<run_id>.mat   轨迹详情（不入库）
%    results/ecms_phase0/runs.csv            汇总表（192 matrix + 48 sweep = 240 行）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/33_run_singlefactor_sweeps.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[33] ===== ECMS Phase 0 — 单因素扫描（T-011，FR-012，48 实例） =====\n');
fprintf('[33] ROOT = %s\n', ROOT);

cfg  = Ecms_Phase0_Config();
fam  = load(fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc', 'bsfc_family.mat'));
md5str = @(s) sprintf('%02x', typecast(java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
cfgHash = @(c, tier) md5str(sprintf('mode=%s;Pmin=%.6g;R=%.6g;Cap=%.6g;Pl=%.6g;Cs=%.6g;dSOC=%.6g;dP=%.6g;S0=%.6g;band=%.6g;Pmax=%.6g;dt=%.6g;keq=%.6g;tier=%d', ...
    c.mode, c.P_min_stable_kW, c.R_gen_kWps, c.Bat_Cap_kWh, c.Bat_P_limit_kW, ...
    c.C_start_kg, c.dSOC, c.dP_kW, c.SOC0, c.SOC_terminal_band, c.P_max_kW, ...
    c.dt_s, c.k_eq_bsfc_gkWh, tier));

%% ======================= 加载剖面 =======================
profs = struct();
for k = 1:numel(cfg.scenarios)
    p = ecms.load_profile(ROOT, cfg.scenarios{k});
    assert(~p.derate_event, 'ECMS:SWEEP', '剖面 %s 含 derate_event，已作废（FR-006）', cfg.scenarios{k});
    assert(p.energy_closure.pass, 'ECMS:SWEEP', '剖面 %s 能量闭合未过', cfg.scenarios{k});
    profs.(cfg.scenarios{k}) = p;
end

%% ======================= 读取既有 runs.csv（主矩阵行，字符串保真） =======================
csvPath = fullfile(ROOT, cfg.output_root, 'runs.csv');
assert(exist(csvPath, 'file') == 2, 'ECMS:SWEEP', ...
    '未找到 %s —— 请先运行 32_run_phase0_matrix.m', csvPath);
fidR = fopen(csvPath, 'r', 'n', 'UTF-8');
assert(fidR > 0, 'ECMS:SWEEP', '无法读取 %s', csvPath);
hdrLine = fgetl(fidR);
assert(strcmp(hdrLine, 'run_id,scenario,tier,mode,P_min_kW,R_gen_kWps,Bat_Cap_kWh,C_start_kg,fuel_dp_kg,fuel_base_raw_kg,fuel_base_corr_kg,saving_pct_raw,saving_pct_corr,feasible,E_unserved_kWh,E_unserved_gt0,baseline_feasible,nontrivial_suspect,wall_time_s,checks_pass,infeasible_reason,config_hash,scan'), ...
    'ECMS:SWEEP', 'runs.csv 表头与 32 不匹配（请先重跑 32）');
matrixRows = {};
l = fgetl(fidR);
while ischar(l)
    flds = strsplit(l, ',', 'CollapseDelimiters', false);
    assert(numel(flds) == 23, 'ECMS:SWEEP', 'runs.csv 行字段数异常: %s', l);
    if strcmp(flds{23}, 'matrix')
        matrixRows{end+1} = flds; %#ok<AGROW>
    end
    l = fgetl(fidR);
end
fclose(fidR);
assert(numel(matrixRows) == 192, 'ECMS:SWEEP', ...
    'runs.csv matrix 行数 %d ≠ 192（请先重跑 32）', numel(matrixRows));
fprintf('[33] 读取主矩阵行：%d\n', numel(matrixRows));

%% ======================= 扫描定义 =======================
TIER = cfg.tier_default;          % 2
PMIN = 400;
% scan 定义：{scanTag, mode, R_gen, Bat_Cap, C_start, runTag}
scanDefs = {};
for R = [20 80]
    scanDefs{end+1} = {'ramp', 'continuous', R, 1071.36, NaN, sprintf('ramp%d', R)}; %#ok<AGROW>
end
for C = [643 1500]
    scanDefs{end+1} = {'batcap', 'continuous', 40, C, NaN, sprintf('batcap%d', C)}; %#ok<AGROW>
end
for Cs = [1 5]
    scanDefs{end+1} = {'cstart', 'startstop', 40, 1071.36, Cs, sprintf('cstart%d', Cs)}; %#ok<AGROW>
end
Ntotal = numel(scanDefs) * numel(cfg.scenarios);     % 48
fprintf('[33] 扫描规模：%d 组 × %d 场景 = %d 实例（基准点已在主矩阵，不重复计入）\n', ...
    numel(scanDefs), numel(cfg.scenarios), Ntotal);

%% ======================= 重写 runs.csv（matrix 行原样 + 新扫描行） =======================
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:SWEEP', '无法写入 %s', csvPath);
fprintf(fid, '%s\n', hdrLine);
for r = 1:numel(matrixRows)
    fprintf(fid, '%s\n', strjoin(matrixRows{r}, ','));
end

sweepRows = cell(1, Ntotal);
nDone = 0;
tAll = tic;

for d = 1:numel(scanDefs)
    scanTag = scanDefs{d}{1}; mode = scanDefs{d}{2};
    Rg = scanDefs{d}{3}; Cap = scanDefs{d}{4}; Cs = scanDefs{d}{5}; runTag = scanDefs{d}{6};
    for s = 1:numel(cfg.scenarios)
        scnName = cfg.scenarios{s};
        prof = profs.(scnName);
        run_id = sprintf('%s_tier%d_%s_pmin%03d_%s', scnName, TIER, mode, PMIN, runTag);
        nDone = nDone + 1;

        cfgI = Ecms_Phase0_Config(struct('mode', mode, 'P_min_stable_kW', PMIN, ...
            'R_gen_kWps', Rg, 'Bat_Cap_kWh', Cap, 'C_start_kg', Cs));
        prob = ecms.dp_problem(prof, cfgI, fam, TIER);
        t0 = tic; sol = ecms.dp_solve(prob); wall = toc(t0);

        % ---- Baseline 参照（dp_checks 同口径）----
        fldBase = sprintf('fuel_base_tier%d_kg', TIER);
        fuel_base_raw = prof.(fldBase);
        k_eq = cfg.k_eq_bsfc_gkWh / (cfg.eta_rect * cfg.eta_dcdc * cfg.eta_bat) / 1000;
        fuel_base_corr = fuel_base_raw + (cfg.SOC0 - prof.SOC(end)) * cfgI.Bat_Cap_kWh * k_eq;
        Pg_base = prof.P_gen_kW;
        tolK = cfg.base_feas_tol_kW;
        bFeasIssues = {};
        if any(Pg_base > prob.P_max + tolK)
            bFeasIssues{end+1} = sprintf('P_gen>P_max %d 点', sum(Pg_base > prob.P_max + tolK)); %#ok<AGROW>
        end
        if strcmp(mode, 'continuous')
            if any(Pg_base < prob.P_min_stable - tolK)
                bFeasIssues{end+1} = sprintf('P_gen<P_min_stable(%.0f) %d 点', prob.P_min_stable, sum(Pg_base < prob.P_min_stable - tolK)); %#ok<AGROW>
            end
        else
            loBad = Pg_base > tolK & Pg_base < prob.P_min_stable - tolK;
            if any(loBad)
                bFeasIssues{end+1} = sprintf('P_gen in (0,P_min_stable) %d 点', sum(loBad)); %#ok<AGROW>
            end
        end
        if any(abs(diff(Pg_base)) > prob.R_gen * prob.dt_s + tolK)
            bFeasIssues{end+1} = '爬坡越限'; %#ok<AGROW>
        end
        if any(abs(prof.P_bat_kW) > prob.Bat_P_limit + tolK)
            bFeasIssues{end+1} = 'P_bat 越限'; %#ok<AGROW>
        end
        if any(prof.SOC < prob.SOC_min - tolK) || any(prof.SOC > prob.SOC_max + tolK)
            bFeasIssues{end+1} = 'SOC 越界'; %#ok<AGROW>
        end
        baselineFeas = isempty(bFeasIssues);

        chk = struct();
        if sol.feasible
            chk = ecms.dp_checks(prob, sol, prof, cfgI, TIER);
            checksPass   = chk.pass;
            fuelDp       = sol.fuel_total_kg;
            fBaseRaw     = chk.baseline.fuel_raw_kg;
            fBaseCorr    = chk.baseline.fuel_corr_kg;
            savingRaw    = (fBaseRaw - fuelDp) / fBaseRaw * 100;
            savingCorr   = chk.saving_pct;
            baselineFeas = chk.baseline.feasible;
            nontriv      = chk.trivial.trivial_suspect;
            infeasReason = '';
        else
            checksPass   = false;
            fuelDp       = NaN;
            fBaseRaw     = fuel_base_raw;
            fBaseCorr    = fuel_base_corr;
            savingRaw    = NaN;
            savingCorr   = NaN;
            nontriv      = NaN;
            infeasReason = sol.infeasible_reason;
        end
        E_unserved = sol.E_unserved_kWh;
        if strcmp(mode, 'startstop')
            cstartCol = cfgI.C_start_kg;
        else
            cstartCol = NaN;
        end

        row = {run_id, scnName, TIER, mode, PMIN, cfgI.R_gen_kWps, cfgI.Bat_Cap_kWh, cstartCol, ...
            fuelDp, fBaseRaw, fBaseCorr, savingRaw, savingCorr, sol.feasible, E_unserved, ...
            E_unserved > 1e-9, baselineFeas, nontriv, wall, checksPass, infeasReason, ...
            cfgHash(cfgI, TIER), scanTag};
        rowStr = cell(1, numel(row));
        for q = 1:numel(row)
            v = row{q};
            if ischar(v) || isstring(v)
                rowStr{q} = char(v);
            elseif islogical(v)
                rowStr{q} = num2str(double(v));
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
        sweepRows{nDone} = rowStr;
        fprintf(fid, '%s\n', strjoin(rowStr, ','));
        save(fullfile(ROOT, cfg.runs_dir, [run_id '.mat']), 'prob', 'sol', 'chk', 'cfgI', 'run_id');

        if nDone == 1 || mod(nDone, 20) == 0
            st = '';
            if ~sol.feasible, st = sprintf('  [INFEASIBLE: %s]', infeasReason); end
            fprintf('[33] (%3d/%d) %-44s feasible=%d fuel=%s saving=%s wall=%.2f s elapsed=%.1f s%s\n', ...
                nDone, Ntotal, run_id, sol.feasible, ...
                num2str(fuelDp, '%.3f'), num2str(savingCorr, '%.3f'), wall, toc(tAll), st);
        end
    end
end
fclose(fid);
fprintf('\n[33] 已写入 %s（matrix %d 行 + sweep %d 行 + 表头）\n', csvPath, numel(matrixRows), numel(sweepRows));

%% ======================= 断言（验收级） =======================
allRows = [matrixRows, sweepRows];     % 全部为 cellstr 行
numv = @(s) str2double(s);
feasFlag = cellfun(@(r) strcmp(r{14}, '1'), allRows);
savC = cellfun(@(r) numv(r{13}), allRows);
assert(numel(allRows) == 240, 'ECMS:SWEEP', '总行数 %d ≠ 240', numel(allRows));
assert(all(~isnan(savC(feasFlag))), 'ECMS:SWEEP', '可行实例存在 NaN/缺失 saving 列');
assert(all(isnan(savC(~feasFlag))), 'ECMS:SWEEP', 'INFEASIBLE 实例 saving 应记为 NaN');
nInfeas = sum(~feasFlag);
if nInfeas > 0
    assert(all(cellfun(@(r) ~isempty(r{21}), allRows(~feasFlag))), ...
        'ECMS:SWEEP', 'INFEASIBLE 行缺 infeasible_reason');
    fprintf('[33] 警告：%d 实例 INFEASIBLE（原因见 runs.csv infeasible_reason 列）\n', nInfeas);
    for q = find(~feasFlag)
        r = allRows{q};
        fprintf('    %-44s reason=%s E_unserved=%.3f kWh\n', r{1}, r{21}, numv(r{15}));
    end
    if nInfeas > 0.1 * numel(allRows)
        error('ECMS:SWEEP', 'INFEASIBLE 实例 %d/%d > 10%%，按任务约束 Escalate（T-011）', nInfeas, numel(allRows));
    end
end

%% ======================= 趋势摘要（含主矩阵基准点） =======================
% 按 (scan, mode, R_gen, Bat_Cap, C_start) 过滤；行字段全字符串
rowMatch = @(r, scanTag, mode, Rg, Cap, Cs) ...
    strcmp(r{23}, scanTag) && strcmp(r{3}, '2') && strcmp(r{5}, '400') && ...
    strcmp(r{4}, mode) && ...
    abs(numv(r{6}) - Rg) < 1e-9 && abs(numv(r{7}) - Cap) < 1e-9 && ...
    ((isnan(Cs) && strcmp(r{8}, 'NaN')) || ...
     (~isnan(Cs) && ~isnan(numv(r{8})) && abs(numv(r{8}) - Cs) < 1e-9));
meanSav = @(scanTag, mode, Rg, Cap, Cs) ...
    mean(cellfun(@(r) numv(r{13}), ...
        allRows(cellfun(@(r) rowMatch(r, scanTag, mode, Rg, Cap, Cs), allRows))));

fprintf('\n[33] ===== 单因素趋势摘要（saving_pct_corr，8 场景平均） =====\n');
for Rv = [20 40 80]
    v = meanSav('matrix', 'continuous', Rv, 1071.36, NaN);
    if isnan(v), v = meanSav('ramp', 'continuous', Rv, 1071.36, NaN); end
    fprintf('  R_gen=%3d kW/s : mean saving = %+.3f%%\n', Rv, v);
end
for Cv = [643 1071.36 1500]
    v = meanSav('matrix', 'continuous', 40, Cv, NaN);
    if isnan(v), v = meanSav('batcap', 'continuous', 40, Cv, NaN); end
    fprintf('  Bat_Cap=%8.2f kWh : mean saving = %+.3f%%\n', Cv, v);
end
for Csv = [1 2 5]
    v = meanSav('matrix', 'startstop', 40, 1071.36, Csv);
    if isnan(v), v = meanSav('cstart', 'startstop', 40, 1071.36, Csv); end
    fprintf('  C_start=%2d kg   : mean saving = %+.3f%%\n', Csv, v);
end

fprintf('\n[33] 单因素扫描完成：48 实例，总耗时 %.1f s。\n', toc(tAll));

%% ========================================================================
%  32_run_phase0_matrix.m — ECMS Phase 0：主矩阵运行（T-010，FR-012）
%  矩阵 = 3（BSFC 档 tier1/2/3）× 4（P_min ∈ {0,300,400,500} kW）× 2（口径
%        continuous / startstop）× 8（场景）= 192 实例
%  每实例：ecms.dp_problem + ecms.dp_solve + ecms.dp_checks（内置校验不可绕过）
%  产出：
%    results/ecms_phase0/runs/<run_id>.mat   轨迹详情（prob/sol/chk/cfgI，不入库）
%    results/ecms_phase0/runs.csv            汇总表（每实例一行，入库）
%  语义要点：
%    - 连续口径 P_g=0 用怠速燃油（fuel_flow_idle）；启停口径 C_start=2 kg（cfg 缺省）
%    - fuel_base 取自剖面 .mat 的 fuel_base_tier1/2/3_kg；SOC 修正用 dp_checks 既有口径
%    - INFEASIBLE 实例记录 infeasible_reason 并继续（不静默跳过）；若 >10% → error Escalate
%    - 重跑覆盖同名产物（run_id 确定、无时间戳）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/32_run_phase0_matrix.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[32] ===== ECMS Phase 0 — 主矩阵（T-010，FR-012，192 实例） =====\n');
fprintf('[32] ROOT = %s\n', ROOT);

cfg  = Ecms_Phase0_Config();
fam  = load(fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc', 'bsfc_family.mat'));
md5str = @(s) sprintf('%02x', typecast(java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
cfgHash = @(c, tier) md5str(sprintf('mode=%s;Pmin=%.6g;R=%.6g;Cap=%.6g;Pl=%.6g;Cs=%.6g;dSOC=%.6g;dP=%.6g;S0=%.6g;band=%.6g;Pmax=%.6g;dt=%.6g;keq=%.6g;tier=%d', ...
    c.mode, c.P_min_stable_kW, c.R_gen_kWps, c.Bat_Cap_kWh, c.Bat_P_limit_kW, ...
    c.C_start_kg, c.dSOC, c.dP_kW, c.SOC0, c.SOC_terminal_band, c.P_max_kW, ...
    c.dt_s, c.k_eq_bsfc_gkWh, tier));

%% ======================= 加载 8 场景剖面 =======================
profs = struct();
for k = 1:numel(cfg.scenarios)
    p = ecms.load_profile(ROOT, cfg.scenarios{k});
    assert(~p.derate_event, 'ECMS:MATRIX', '剖面 %s 含 derate_event，已作废（FR-006）', cfg.scenarios{k});
    assert(p.energy_closure.pass, 'ECMS:MATRIX', '剖面 %s 能量闭合未过', cfg.scenarios{k});
    profs.(cfg.scenarios{k}) = p;
end
fprintf('[32] 剖面加载完成：%d 场景\n', numel(cfg.scenarios));

%% ======================= 输出目录 =======================
runsDir = fullfile(ROOT, cfg.runs_dir);
if ~exist(runsDir, 'dir'), mkdir(runsDir); end
csvPath = fullfile(ROOT, cfg.output_root, 'runs.csv');

%% ======================= 矩阵定义 =======================
tiers  = 1:3;
pminSet = cfg.P_min_stable_set_kW;          % [0 300 400 500]
modes  = cfg.modes;                         % {'continuous','startstop'}
scens  = cfg.scenarios;
Ntotal = numel(scens) * numel(tiers) * numel(pminSet) * numel(modes);   % 192
fprintf('[32] 矩阵规模：%d 场景 × %d 档 × %d P_min × %d 口径 = %d 实例\n', ...
    numel(scens), numel(tiers), numel(pminSet), numel(modes), Ntotal);

%% ======================= CSV 头 =======================
HEADER = ['run_id,scenario,tier,mode,P_min_kW,R_gen_kWps,Bat_Cap_kWh,C_start_kg,' ...
    'fuel_dp_kg,fuel_base_raw_kg,fuel_base_corr_kg,saving_pct_raw,saving_pct_corr,' ...
    'feasible,E_unserved_kWh,E_unserved_gt0,baseline_feasible,nontrivial_suspect,' ...
    'wall_time_s,checks_pass,infeasible_reason,config_hash,scan'];
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:MATRIX', '无法写入 %s', csvPath);
fprintf(fid, '%s\n', HEADER);

allRows = cell(1, Ntotal);     % 内存副本（供摘要/断言）
nDone   = 0;
tAll    = tic;

%% ======================= 主循环 =======================
for s = 1:numel(scens)
    scnName = scens{s};
    prof = profs.(scnName);
    for t = 1:numel(tiers)
        tier = tiers(t);
        fldBase = sprintf('fuel_base_tier%d_kg', tier);
        for m = 1:numel(modes)
            mode = modes{m};
            for p = 1:numel(pminSet)
                pm  = pminSet(p);
                run_id = sprintf('%s_tier%d_%s_pmin%03d', scnName, tier, mode, pm);
                nDone = nDone + 1;

                % ---- 实例配置 / 问题 / 求解 ----
                cfgI = Ecms_Phase0_Config(struct('mode', mode, 'P_min_stable_kW', pm));
                prob = ecms.dp_problem(prof, cfgI, fam, tier);
                t0 = tic; sol = ecms.dp_solve(prob); wall = toc(t0);

                % ---- Baseline 参照（dp_checks 同口径，先行计算供不可行分支）----
                fuel_base_raw = prof.(fldBase);
                k_eq = cfg.k_eq_bsfc_gkWh / (cfg.eta_rect * cfg.eta_dcdc * cfg.eta_bat) / 1000;
                fuel_base_corr = fuel_base_raw + (cfg.SOC0 - prof.SOC(end)) * cfg.Bat_Cap_kWh * k_eq;
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

                % ---- dp_checks（仅可行解；失败即 error 终止——不可绕过）----
                chk = struct();
                if sol.feasible
                    chk = ecms.dp_checks(prob, sol, prof, cfgI, tier);
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

                % ---- 行组装 ----
                row = {run_id, scnName, tier, mode, pm, cfgI.R_gen_kWps, cfgI.Bat_Cap_kWh, cstartCol, ...
                    fuelDp, fBaseRaw, fBaseCorr, savingRaw, savingCorr, sol.feasible, E_unserved, ...
                    E_unserved > 1e-9, baselineFeas, nontriv, wall, checksPass, infeasReason, ...
                    cfgHash(cfgI, tier), 'matrix'};
                allRows{nDone} = row;
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
                fprintf(fid, '%s\n', strjoin(rowStr, ','));

                % ---- 轨迹存档（重跑覆盖同名）----
                save(fullfile(runsDir, [run_id '.mat']), 'prob', 'sol', 'chk', 'cfgI', 'run_id');

                % ---- 进度 ----
                if nDone == 1 || mod(nDone, 20) == 0
                    st = '';
                    if ~sol.feasible, st = sprintf('  [INFEASIBLE: %s]', infeasReason); end
                    fprintf('[32] (%3d/%d) %-40s feasible=%d fuel=%s saving=%s wall=%.2f s elapsed=%.1f s%s\n', ...
                        nDone, Ntotal, run_id, sol.feasible, ...
                        num2str(fuelDp, '%.3f'), num2str(savingCorr, '%.3f'), wall, toc(tAll), st);
                end
            end
        end
    end
end
fclose(fid);
fprintf('\n[32] 已写入 %s（%d 行 + 表头）\n', csvPath, numel(allRows));

%% ======================= 断言（验收级） =======================
assert(numel(allRows) == 192, 'ECMS:MATRIX', '矩阵行数 %d ≠ 192', numel(allRows));
feasFlag = cellfun(@(r) r{14} == 1, allRows);
savC = cellfun(@(r) r{13}, allRows);
savR = cellfun(@(r) r{12}, allRows);
assert(all(~isnan(savC(feasFlag))) && all(~isnan(savR(feasFlag))), ...
    'ECMS:MATRIX', '可行实例存在 NaN/缺失 saving 列');
assert(all(isnan(savC(~feasFlag))), 'ECMS:MATRIX', 'INFEASIBLE 实例 saving 应记为 NaN');
nInfeas = sum(~feasFlag);
if nInfeas > 0
    assert(all(cellfun(@(r) ~isempty(r{21}), allRows(~feasFlag))), ...
        'ECMS:MATRIX', 'INFEASIBLE 行缺 infeasible_reason');
    fprintf('[32] 警告：%d 实例 INFEASIBLE（原因见 runs.csv infeasible_reason 列）\n', nInfeas);
    for q = find(~feasFlag)
        r = allRows{q};
        fprintf('    %-40s reason=%s E_unserved=%.3f kWh\n', r{1}, r{21}, r{15});
    end
    if nInfeas > 0.1 * numel(allRows)
        error('ECMS:MATRIX', 'INFEASIBLE 实例 %d/%d > 10%%，按任务约束 Escalate（T-010）', nInfeas, numel(allRows));
    end
end

%% ======================= 摘要打印 =======================
fprintf('\n[32] ===== 矩阵摘要：每（tier×口径）8 场景平均 saving_pct_corr =====\n');
fprintf('%-8s %-14s %10s %8s %14s\n', 'tier', 'mode', 'mean_saving%', 'n_feas', 'inst_feas/inst');
tm = containers.Map();
for tier = 1:3
    for m = 1:numel(modes)
        mode = modes{m};
        idx = find(cellfun(@(r) r{3} == tier && strcmp(r{4}, mode), allRows));
        fIdx = idx(cellfun(@(r) r{14} == 1, allRows(idx)));
        if isempty(fIdx)
            meanSav = NaN;
        else
            meanSav = mean(cellfun(@(r) r{13}, allRows(fIdx)));
        end
        tm(sprintf('%d_%s', tier, mode)) = meanSav;
        fprintf('%-8d %-14s %10.3f %8d %14s\n', tier, mode, meanSav, numel(fIdx), ...
            sprintf('%d/%d', numel(fIdx), numel(idx)));
    end
end
fprintf('\n[32] ===== 启停 vs 连续 平均差（saving_pct_corr，startstop − continuous） =====\n');
totCont = []; totSS = [];
for tier = 1:3
    d = tm(sprintf('%d_startstop', tier)) - tm(sprintf('%d_continuous', tier));
    fprintf('  tier%d: %+.3f 个百分点\n', tier, d);
end
cIdx = find(cellfun(@(r) strcmp(r{4}, 'continuous') && r{14} == 1, allRows));
sIdx = find(cellfun(@(r) strcmp(r{4}, 'startstop') && r{14} == 1, allRows));
fprintf('  总体：continuous mean=%+.3f%%，startstop mean=%+.3f%%，差=%+.3f 个百分点\n', ...
    mean(cellfun(@(r) r{13}, allRows(cIdx))), mean(cellfun(@(r) r{13}, allRows(sIdx))), ...
    mean(cellfun(@(r) r{13}, allRows(sIdx))) - mean(cellfun(@(r) r{13}, allRows(cIdx))));

fprintf('\n[32] 主矩阵完成：192 实例，总耗时 %.1f s。runs.csv 与 runs/*.mat 已写入。\n', toc(tAll));

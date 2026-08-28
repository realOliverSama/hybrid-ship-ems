%% ========================================================================
%  37_run_ext_2d_analysis.m — ECMS Phase 0：二维 map 扩展分析（T-013，EXTENSION）
%  扩展情景（design §3.2）：假设机组可变速（有源前端/变频耐受），二维 BSFC map
%  可用；不进主 Gate，独立输出。
%  用 +ecms/speed_cost_min.m（use2d=true，design §4.4 转速解耦）把二维 map
%  （bsfc_family.mat map2d，三档）预处理为等效一维 bsfc_eff(P) = min over n，
%  DP 控制维保持一维。
%  缩减矩阵：3（tier）× 2（mode）× 8（场景）× P_min ∈ {0, 400} = 96 实例。
%  产出：
%    results/ecms_phase0/ext_2d/runs_ext.csv   汇总表（列结构同 runs.csv，scan=ext2d）
%    results/ecms_phase0/ext_2d/runs_ext/*.mat 轨迹详情（含 n_opt_rpm 最优转速轨迹，
%                                               design §4.4 论文素材）
%  对比摘要：同配置（scenario/tier/mode/P_min）下 1D 定速（runs.csv matrix 行）vs
%  2D 可变速的 saving_pct_corr 差（pp）。
%  语义要点：
%    - Baseline 与 1D 主矩阵同口径（fuel_base_tier*_kg，1500 rpm 定速）；2D DP 的
%      额外自由度 = 低负荷降速，其收益体现为 2D 有效曲线 ≤ 1D 曲线（网格量化内）
%    - 二维 map 构造于 1D 曲线之上（bsfc(n,P)=bsfc_1d(P)×(1+k_n·((n−n*(P))/Δn)²)），
%      min over n 恢复 ≈1D（偏差 <0.1%，n 网格 50 rpm 量化，bsfc_family 自检 9），
%      故 2D 与 1D saving 差预期 ≈0 pp——若出现微小负差属网格量化，design 已解释
%    - INFEASIBLE 实例记录 infeasible_reason 并继续；若 >10% → error Escalate
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/37_run_ext_2d_analysis.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[37] ===== ECMS Phase 0 — 二维 map 扩展分析（T-013，EXTENSION，96 实例） =====\n');
fprintf('[37] ROOT = %s\n', ROOT);

cfg = Ecms_Phase0_Config();
fam = load(fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc', 'bsfc_family.mat'));
md5str = @(s) sprintf('%02x', typecast(java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
cfgHash = @(c, tier) md5str(sprintf('mode=%s;Pmin=%.6g;R=%.6g;Cap=%.6g;Pl=%.6g;Cs=%.6g;dSOC=%.6g;dP=%.6g;S0=%.6g;band=%.6g;Pmax=%.6g;dt=%.6g;keq=%.6g;tier=%d;use2d=1', ...
    c.mode, c.P_min_stable_kW, c.R_gen_kWps, c.Bat_Cap_kWh, c.Bat_P_limit_kW, ...
    c.C_start_kg, c.dSOC, c.dP_kW, c.SOC0, c.SOC_terminal_band, c.P_max_kW, ...
    c.dt_s, c.k_eq_bsfc_gkWh, tier));

%% ======================= 二维有效曲线预处理（design §4.4） =======================
fprintf('\n[37] ===== 二维有效曲线（min over n，经 +ecms/speed_cost_min use2d=true） =====\n');
for t = 1:3
    [bpEff, tbEff, meta] = ecms.speed_cost_min(fam, t, true);
    assert(strcmp(meta.method, 'min_over_n_2d'), 'ECMS:EXT2D', 'speed_cost_min 未返回 2D 路径');
    dev = max(abs(tbEff - fam.tiers(t).agg_table_gkWh) ./ fam.tiers(t).agg_table_gkWh) * 100;
    fprintf('[37] tier%d：method=%s | 2D eff vs 1D agg max 偏差=%.4f%% | n_opt@0kW=%d rpm, @1000kW=%d rpm\n', ...
        t, meta.method, dev, meta.n_opt_eff_rpm(1), meta.n_opt_eff_rpm(end));
end
fprintf('[37] 注：二维 map 构造于 1D 曲线之上，min over n 恢复 ≈1D（n 网格 50 rpm 量化），\n');
fprintf('[37]     故 2D 相对 1D 的 saving 差预期 ≈0 pp——扩展情景仅验证可变速假设下的收益面。\n');

%% ======================= 加载 8 场景剖面 =======================
profs = struct();
for k = 1:numel(cfg.scenarios)
    p = ecms.load_profile(ROOT, cfg.scenarios{k});
    assert(~p.derate_event, 'ECMS:EXT2D', '剖面 %s 含 derate_event，已作废（FR-006）', cfg.scenarios{k});
    assert(p.energy_closure.pass, 'ECMS:EXT2D', '剖面 %s 能量闭合未过', cfg.scenarios{k});
    profs.(cfg.scenarios{k}) = p;
end

%% ======================= 读取 1D 主矩阵参考（runs.csv matrix 行） =======================
refCsv = fullfile(ROOT, cfg.output_root, 'runs.csv');
assert(exist(refCsv, 'file') == 2, 'ECMS:EXT2D', '未找到 %s —— 请先运行 32', refCsv);
fidR = fopen(refCsv, 'r', 'n', 'UTF-8');
assert(fidR > 0, 'ECMS:EXT2D', '无法读取 %s', refCsv);
fgetl(fidR);   % 表头（列位置与 32 固定一致）
ref1d = containers.Map();   % key = 'scn|tier|mode|pmin' → saving_pct_corr
l = fgetl(fidR);
while ischar(l)
    f = strsplit(l, ',', 'CollapseDelimiters', false);
    if numel(f) == 23 && strcmp(f{23}, 'matrix')
        key = sprintf('%s|%s|%s|%s', f{2}, f{3}, f{4}, f{5});
        if isKey(ref1d, key)
            error('ECMS:EXT2D', '1D 参考重复 key=%s', key);
        end
        ref1d(key) = str2double(f{13});   % saving_pct_corr
    end
    l = fgetl(fidR);
end
fclose(fidR);
fprintf('[37] 1D 主矩阵参考（runs.csv matrix 行）已载入：%d 条\n', ref1d.Count);

%% ======================= 输出目录 =======================
extRoot = fullfile(ROOT, cfg.output_root, 'ext_2d');
runsDir = fullfile(extRoot, 'runs_ext');
if ~exist(runsDir, 'dir'), mkdir(runsDir); end
csvPath = fullfile(extRoot, 'runs_ext.csv');

%% ======================= 缩减矩阵定义 =======================
tiers   = 1:3;
pminSet = [0 400];          % 缩减：P_min ∈ {0, 400}
modes   = cfg.modes;
scens   = cfg.scenarios;
Ntotal  = numel(tiers) * numel(pminSet) * numel(modes) * numel(scens);   % 96
fprintf('\n[37] 缩减矩阵：%d 档 × %d P_min × %d 口径 × %d 场景 = %d 实例\n', ...
    numel(tiers), numel(pminSet), numel(modes), numel(scens), Ntotal);

%% ======================= CSV 头（列结构同 runs.csv） =======================
HEADER = ['run_id,scenario,tier,mode,P_min_kW,R_gen_kWps,Bat_Cap_kWh,C_start_kg,' ...
    'fuel_dp_kg,fuel_base_raw_kg,fuel_base_corr_kg,saving_pct_raw,saving_pct_corr,' ...
    'feasible,E_unserved_kWh,E_unserved_gt0,baseline_feasible,nontrivial_suspect,' ...
    'wall_time_s,checks_pass,infeasible_reason,config_hash,scan'];
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
assert(fid > 0, 'ECMS:EXT2D', '无法写入 %s', csvPath);
fprintf(fid, '%s\n', HEADER);

allRows = cell(1, Ntotal);
nDone   = 0;
tAll    = tic;

%% ======================= 主循环（96 实例，2D 可变速） =======================
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
                run_id = sprintf('%s_tier%d_%s_pmin%03d_2d', scnName, tier, mode, pm);
                nDone = nDone + 1;

                cfgI = Ecms_Phase0_Config(struct('mode', mode, 'P_min_stable_kW', pm, ...
                    'use_2d', true));
                prob = ecms.dp_problem(prof, cfgI, fam, tier);
                assert(strcmp(prob.bsfc_method, 'min_over_n_2d'), 'ECMS:EXT2D', ...
                    '%s：prob 未使用 2D 有效曲线', run_id);
                t0 = tic; sol = ecms.dp_solve(prob); wall = toc(t0);

                fuel_base_raw = prof.(fldBase);
                k_eq = cfg.k_eq_bsfc_gkWh / (cfg.eta_rect * cfg.eta_dcdc * cfg.eta_bat) / 1000;
                fuel_base_corr = fuel_base_raw + (cfg.SOC0 - prof.SOC(end)) * cfg.Bat_Cap_kWh * k_eq;

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
                    % 最优转速轨迹（design §4.4 论文素材）：对每步 P_g 取 map 内 argmin n
                    T2 = fam.map2d.tiers(tier).table_gkWh;   % P_unit × n_rpm
                    PgU = sol.P_g_kW / 2;                    % 聚合 / 2 → 单机功率
                    nOpt = zeros(prob.N, 1);
                    for tt = 1:prob.N
                        iU = round(PgU(tt)) + 1;             % map2d.P_kW = 0:1:500
                        iU = max(1, min(numel(fam.map2d.P_kW), iU));
                        [~, ni] = min(T2(iU, :));
                        nOpt(tt) = fam.map2d.n_rpm(ni);
                    end
                else
                    checksPass   = false;
                    fuelDp       = NaN;
                    fBaseRaw     = fuel_base_raw;
                    fBaseCorr    = fuel_base_corr;
                    savingRaw    = NaN;
                    savingCorr   = NaN;
                    baselineFeas = false;
                    nontriv      = NaN;
                    infeasReason = sol.infeasible_reason;
                    nOpt         = [];
                end
                E_unserved = sol.E_unserved_kWh;
                if strcmp(mode, 'startstop')
                    cstartCol = cfgI.C_start_kg;
                else
                    cstartCol = NaN;
                end

                % ---- 1D 同配置参考与 saving 差（pp）----
                key1d = sprintf('%s|%d|%s|%d', scnName, tier, mode, pm);
                assert(isKey(ref1d, key1d), 'ECMS:EXT2D', '1D 参考缺失 key=%s', key1d);
                sav1d = ref1d(key1d);
                if isfinite(savingCorr) && isfinite(sav1d)
                    deltaPp = savingCorr - sav1d;
                else
                    deltaPp = NaN;
                end

                row = {run_id, scnName, tier, mode, pm, cfgI.R_gen_kWps, cfgI.Bat_Cap_kWh, ...
                    cstartCol, fuelDp, fBaseRaw, fBaseCorr, savingRaw, savingCorr, ...
                    sol.feasible, E_unserved, E_unserved > 1e-9, baselineFeas, nontriv, ...
                    wall, checksPass, infeasReason, cfgHash(cfgI, tier), 'ext2d'};
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

                save(fullfile(runsDir, [run_id '.mat']), 'prob', 'sol', 'chk', 'cfgI', ...
                    'run_id', 'nOpt', 'sav1d', 'deltaPp');

                if nDone == 1 || mod(nDone, 20) == 0
                    st = '';
                    if ~sol.feasible, st = sprintf('  [INFEASIBLE: %s]', infeasReason); end
                    fprintf('[37] (%3d/%d) %-42s feasible=%d fuel=%s saving2d=%s sav1d=%s dPp=%s wall=%.2f s elapsed=%.1f s%s\n', ...
                        nDone, Ntotal, run_id, sol.feasible, num2str(fuelDp, '%.3f'), ...
                        num2str(savingCorr, '%.3f'), num2str(sav1d, '%.3f'), ...
                        num2str(deltaPp, '%+.3f'), wall, toc(tAll), st);
                end
            end
        end
    end
end
fclose(fid);
fprintf('\n[37] 已写入 %s（%d 行 + 表头）\n', csvPath, numel(allRows));

%% ======================= 断言（验收级） =======================
assert(numel(allRows) == 96, 'ECMS:EXT2D', '扩展矩阵行数 %d ≠ 96', numel(allRows));
feasFlag = cellfun(@(r) r{14} == 1, allRows);
savC = cellfun(@(r) r{13}, allRows);
assert(all(~isnan(savC(feasFlag))) && all(isnan(savC(~feasFlag))), ...
    'ECMS:EXT2D', 'saving_pct_corr 一致性断言失败');
nInfeas = sum(~feasFlag);
if nInfeas > 0
    assert(all(cellfun(@(r) ~isempty(r{21}), allRows(~feasFlag))), ...
        'ECMS:EXT2D', 'INFEASIBLE 行缺 infeasible_reason');
    fprintf('[37] 警告：%d 实例 INFEASIBLE（原因见 runs_ext.csv）\n', nInfeas);
    for q = find(~feasFlag)
        r = allRows{q};
        fprintf('    %-42s reason=%s\n', r{1}, r{21});
    end
    if nInfeas > 0.1 * numel(allRows)
        error('ECMS:EXT2D', 'INFEASIBLE 实例 %d/%d > 10%%，按任务约束 Escalate（T-013）', ...
            nInfeas, numel(allRows));
    end
end

%% ======================= 二维 vs 一维 对比摘要 =======================
fprintf('\n[37] ===== 二维可变速 vs 一维定速 对比摘要（saving_pct_corr，Δpp = 2D − 1D） =====\n');
fprintf('%-8s %-12s %6s %12s %12s %12s\n', 'tier', 'mode', 'P_min', 'mean_2D%', 'mean_1D%', 'mean_dPp');
tot2 = []; tot1 = []; totD = [];
for tier = 1:3
    for m = 1:numel(modes)
        mode = modes{m};
        for pm = pminSet
            idx = find(cellfun(@(r) r{3} == tier && strcmp(r{4}, mode) && r{5} == pm, allRows));
            fIdx = idx(cellfun(@(r) r{14} == 1, allRows(idx)));
            if isempty(fIdx)
                fprintf('%-8d %-12s %6d   （无可行实例）\n', tier, mode, pm);
                continue;
            end
            v2 = mean(cellfun(@(r) r{13}, allRows(fIdx)));
            v1 = mean(cellfun(@(r) ref1d(sprintf('%s|%d|%s|%d', r{2}, r{3}, r{4}, r{5})), allRows(fIdx)));
            dP = mean(cellfun(@(r) r{13} - ref1d(sprintf('%s|%d|%s|%d', r{2}, r{3}, r{4}, r{5})), allRows(fIdx)));
            fprintf('%-8d %-12s %6d %12.3f %12.3f %+12.3f\n', tier, mode, pm, v2, v1, dP);
            tot2 = [tot2 v2]; tot1 = [tot1 v1]; totD = [totD dP]; %#ok<AGROW>
        end
    end
end
fprintf('  总体（全部可行实例）：mean_2D=%+.3f%%，mean_1D=%+.3f%%，Δ=%+.3f pp\n', ...
    mean(tot2), mean(tot1), mean(totD));
fprintf('\n[37] 声明：本分析为扩展情景——假设机组可变速（二维 BSFC map 可用）。\n');
fprintf('[37]     主判定链路仍以 32/35 的一维定速（1500 rpm）为准；二维不进主 Gate（design §3.2）。\n');
fprintf('\n[37] 二维扩展分析完成：96 实例，总耗时 %.1f s。\n', toc(tAll));

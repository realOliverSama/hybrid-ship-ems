%% ========================================================================
%  22b_analyze_phase1B_lite.m — T-009-lite 稀疏域确认：分析 + 双图 + 条件补测
%  输入: results/ms_phase1/phase1B_lite/ds_lite_*.mat（22_run_phase1B_lite 产物）
%  输出:
%    results/ms_phase1/phase1B_lite/summary_lite.csv   每点×场景一行（机器可读）
%    results/ms_phase1/phase1B_lite/lite_facts.md      事实记录（两汇总表）
%    results/ms_phase1/phase1B_lite/lite_maps_<S07|S09>.png 双面板热力图
%    results/ms_phase1/phase1B_lite/h60_<expID>.mat   条件补测产物（若触发）
%  判据（主 Agent 定）:
%    control-authority 判据: max|ΔP_gen_actual|（CMS vs C0，0.1 s 网格）
%    envelope-margin 判据:   margin(t)=Gen_P_max + P_bat_dis_avail×f_SOC×f_V
%                            − P_load_total；记录 min/mean 与 min 时刻
%    矛盾信号（escalate）:   min margin < 0 但 ff 未激活（ff_active_s=0），
%                            或任一点 max|ΔP_gen_actual| > 1 kW（完成 H60 补测）
%  本脚本只陈述事实与数值；"区域为空"结论由主 Agent 判定。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
model.load_mlp_weights(ROOT);
run(fullfile(ROOT,'config','Model_Scenarios.m'));

MDL = 'HybridShip_ModelCms_DCBus';
outDir = fullfile(ROOT,'results','ms_phase1','phase1B_lite');
if ~exist(outDir,'dir'), error('缺 22 产物: %s', outDir); end

lambdaP = [0.8 0.9 1.0 1.25];
Rgens   = [20 30 40 60];
scens   = {'S07','S09'};
DTG  = 0.1;      % 公共网格 [s]
divTol = 0.5;    % 首个有意义分歧阈值 [kW]（同 21c）
ctrlTol = 1.0;   % 条件补测触发阈值 [kW]（>1 kW 补 H60）
FFth = 0.5;      % ff 有效阈值 [kW]
KS = [0.30 0.45]; % f_SOC 折点
KV = [690 720];   % f_V 折点

loadD = @(id) load(fullfile(outDir,['ds_' id '.mat']));

% ---------------- 主流程 ----------------
load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);
stopTimeLimit = 5000/5.66 + 1800;

rows = cell(0,23);   % summary_lite 行（23 列，与 hdr 一致）
md = {};
md{end+1} = '# T-009-lite 稀疏域确认：事实记录（无结论）';
md{end+1} = '';
md{end+1} = '固定矩阵: λ_P∈{0.8,0.9,1.0,1.25} × R_gen∈{20,30,40,60}（16 点）, τ_gen=10 s, SOC0=0.60, H=45 s (CMS traj), 场景 S07/S09, C0+CMS 成对 = 64 次';
md{end+1} = sprintf('公共网格 %.1f s；分歧阈值 %.1f kW；补测触发阈值 %.1f kW', DTG, divTol, ctrlTol);
md{end+1} = '';

triggerList = {};       % max|Δ|>1 kW 的点
contradictList = {};    % min margin<0 且 ff 未激活的点
coldAbsorbList = {};    % min margin<0 且 ff 激活（冷启动吸收段）

for scn = scens
    scn = scn{1};
    md{end+1} = sprintf('## %s', scn); %#ok<AGROW>
    md{end+1} = '| λ_P | R_gen | maxΔP_act [kW] | 首分歧 t [s] | ff_active [s] | ff_energy [kJ] | margin_min [kW] | margin_mean [kW] | margin_min t [s] |'; %#ok<AGROW>
    for lp = lambdaP
        for rg = Rgens
            id0 = sprintf('lite_lP%.2f_Rg%02d_%s_C0', lp, rg, scn);
            id3 = sprintf('lite_lP%.2f_Rg%02d_%s_CMS_H45', lp, rg, scn);
            d0 = loadD(id0); d3 = loadD(id3);
            ds0 = d0.ds; ds3 = d3.ds;
            m0 = model.msff_metrics(ds0); m3 = model.msff_metrics(ds3);
            GPM = d3.genEff.Gen_P_max;

            % ---- ΔP_gen_actual ----
            tEnd = min(ds0.t(end), ds3.t(end));
            gridT = (0:DTG:tEnd)';
            a0 = interp1(ds0.t, ds0.P_gen_actual_kW, gridT, 'linear');
            a3 = interp1(ds3.t, ds3.P_gen_actual_kW, gridT, 'linear');
            dA = a3 - a0;
            [mx, imx] = max(abs(dA));
            iDiv = find(abs(dA) > divTol, 1, 'first');
            tDiv = NaN; if ~isempty(iDiv), tDiv = gridT(iDiv); end

            % ---- envelope margin（CMS 轨迹，最严口径）----
            S = ds3.SOC; V = ds3.V_dc_V;
            fS = zeros(size(S)); fS(S>=KS(2)) = 1;
            msk = S>KS(1) & S<KS(2); fS(msk) = (S(msk)-KS(1))/(KS(2)-KS(1));
            fV = zeros(size(V)); fV(V>=KV(2)) = 1;
            msk = V>KV(1) & V<KV(2); fV(msk) = (V(msk)-KV(1))/(KV(2)-KV(1));
            marg = GPM + ds3.P_bat_dis_avail_kW .* fS .* fV - ds3.P_load_total_kW;
            [mmn, imn] = min(marg);
            mmn_t = ds3.t(imn);
            mmean = mean(marg);

            % ---- ff 窗口与负裕度窗口重叠检查 ----
            iFF = ds3.P_ff_applied_kW > FFth;
            negWin = marg < 0;
            overlap = any(iFF & negWin);

            % ---- 触发/矛盾判定 ----
            if mx > ctrlTol
                triggerList{end+1} = sprintf('%s lP%.2f Rg%02d maxΔ=%.3f kW @t=%.1f s', scn, lp, rg, mx, gridT(imx)); %#ok<AGROW>
            end
            if mmn < 0 && m3.ff_active_s <= 0.5
                contradictList{end+1} = sprintf('%s lP%.2f Rg%02d margin_min=%.1f kW ff_active=%.2f s', scn, lp, rg, mmn, m3.ff_active_s); %#ok<AGROW>
            elseif mmn < 0 && m3.ff_active_s > 0.5
                coldAbsorbList{end+1} = sprintf('%s lP%.2f Rg%02d margin_min=%.1f kW @t=%.1fs ff_active=%.2fs overlap=%d', scn, lp, rg, mmn, mmn_t, m3.ff_active_s, overlap); %#ok<AGROW>
            end

            % ---- 汇总行 ----
            dF = (m3.fuel_kg - m0.fuel_kg)/m0.fuel_kg*100;
            rows(end+1,:) = {scn, lp, rg, mx, tDiv, m3.ff_active_s, m3.ff_energy_kJ, ...
                mmn, mmean, mmn_t, ...
                m0.fuel_kg, m3.fuel_kg, dF, ...
                m0.Vdc_min, m3.Vdc_min, m0.SOC_end, m3.SOC_end, ...
                m0.P_bat_rms_kW, m3.P_bat_rms_kW, ...
                m3.N_UV, m3.N_stress, m3.E_unserved_kWh, ...
                overlap}; %#ok<AGROW>
            md{end+1} = sprintf('| %.2f | %d | %.6f | %.1f | %.1f | %.0f | %.1f | %.1f | %.1f |', ...
                lp, rg, mx, tDiv, m3.ff_active_s, m3.ff_energy_kJ, mmn, mmean, mmn_t); %#ok<AGROW>
        end
    end
    md{end+1} = ''; %#ok<AGROW>
end

% ---------------- summary_lite.csv ----------------
hdr = {'scenario','lambda_P','R_gen','max_dP_actual_kW','first_div_t_s', ...
       'ff_active_s','ff_energy_kJ', ...
       'margin_min_kW','margin_mean_kW','margin_min_t_s', ...
       'fuel_C0_kg','fuel_CMS_kg','dFuel_pct', ...
       'Vdc_min_C0','Vdc_min_CMS','SOC_end_C0','SOC_end_CMS', ...
       'P_bat_rms_C0_kW','P_bat_rms_CMS_kW', ...
       'N_UV_675','N_stress_700','E_unserved_kWh','neg_margin_overlaps_ff'};
T = cell2table(rows, 'VariableNames', hdr);
writetable(T, fullfile(outDir,'summary_lite.csv'));
fprintf('[C] summary_lite.csv: %d 行\n', height(T));

% ---------------- 条件补测 H60 ----------------
if ~isempty(triggerList)
    fprintf('[C] **** 检测到 %d 个 max|ΔP|>1 kW 的点，执行 H60 补测 ****\n', numel(triggerList));
    for k = 1:numel(triggerList)
        tok = regexp(triggerList{k}, '^(\w+) lP([\d.]+) Rg(\d+)', 'tokens');
        if ~isempty(tok)
            scn = tok{1}{1}; lp = str2double(tok{1}{2}); rg = str2double(tok{1}{3});
            sname = scn;
            orc = load(fullfile(ROOT,'results','ms_phase1',sprintf('oracle_%s.mat',sname)));
            D = orc.D_ms;
            SCN = SCENARIOS(str2double(sname(2:end)));
            genEff = struct('Gen_P_max', lp*800, 'Gen_Ramp_Up', rg, ...
                            'Gen_Ramp_Down', rg, 'tau_gen', 10);
            expID = sprintf('lite_lP%.2f_Rg%02d_%s_CMS_H60', lp, rg, sname);
            out = model.run_ms_sim(MDL, SCN, 3, 60, 0, D, stopTimeLimit, genEff);
            ds60 = model.extract_logsout(out);
            m60 = model.msff_metrics(ds60);
            save(fullfile(outDir,['ds_' expID '.mat']), 'ds60', 'm60', 'genEff');
            % 记录到 facts（自包含：重取该点 C0 对照）
            d0b = loadD(sprintf('lite_lP%.2f_Rg%02d_%s_C0', lp, rg, sname));
            tEb = min(d0b.ds.t(end), ds60.t(end));
            gridB = (0:DTG:tEb)';
            a0b = interp1(d0b.ds.t, d0b.ds.P_gen_actual_kW, gridB, 'linear');
            a6b = interp1(ds60.t, ds60.P_gen_actual_kW, gridB, 'linear');
            mxd = max(abs(a6b-a0b));
            md{end+1} = sprintf('H60: max|ΔP_actual|=%.6f kW, ff_active=%.2f s, fuel=%.2f kg', mxd, m60.ff_active_s, m60.fuel_kg); %#ok<AGROW>
            md{end+1} = ''; %#ok<AGROW>
        end
    end
end

% ---------------- 双图（每场景 1 张，双面板热力图） ----------------
for scn = scens
    scn = scn{1};
    subT = T(strcmp(T.scenario, scn), :);
    Zc = nan(4,4); Zm = nan(4,4);
    for i = 1:height(subT)
        ri = find(Rgens == subT.R_gen(i), 1);
        ci = find(lambdaP == subT.lambda_P(i), 1);
        Zc(ri,ci) = subT.max_dP_actual_kW(i);
        Zm(ri,ci) = subT.margin_min_kW(i);
    end
    f = figure('Visible','off','Position',[100 100 1200 500]);
    % 左: control-authority map
    subplot(1,2,1);
    imagesc(1:4, 1:4, Zc); colormap(flipud(hot)); colorbar;
    set(gca,'XTick',1:4,'XTickLabel',arrayfun(@(x)sprintf('%.2f',x),lambdaP,'UniformOutput',false), ...
             'YTick',1:4,'YTickLabel',arrayfun(@(x)sprintf('%d',x),Rgens,'UniformOutput',false));
    xlabel('λ_P'); ylabel('R_gen [kW/s]'); grid on;
    title(sprintf('%s control-authority map\nmax|ΔP_gen_actual| [kW] (CMS H45 vs C0)', scn));
    for i = 1:4
        for j = 1:4
            cc = 'k';
            if Zc(i,j) > 0.01 && Zc(i,j) < (max(max(Zc))/2*0.5 + 0.05)
                cc = 'w';
            end
            text(j,i, sprintf('%.4g', Zc(i,j)), 'HorizontalAlignment','center', 'Color', cc);
        end
    end
    % 右: envelope-margin map
    subplot(1,2,2);
    imagesc(1:4, 1:4, Zm); colormap(flipud(hot)); colorbar;
    set(gca,'XTick',1:4,'XTickLabel',arrayfun(@(x)sprintf('%.2f',x),lambdaP,'UniformOutput',false), ...
             'YTick',1:4,'YTickLabel',arrayfun(@(x)sprintf('%d',x),Rgens,'UniformOutput',false));
    xlabel('λ_P'); ylabel('R_gen [kW/s]'); grid on;
    title(sprintf('%s envelope-margin map\nmin margin [kW] (Gen_P_max + P_bat_avail·fS·fV − P_load)', scn));
    for i = 1:4
        for j = 1:4
            text(j,i, sprintf('%.0f', Zm(i,j)), 'HorizontalAlignment','center', 'Color','k');
        end
    end
    saveas(f, fullfile(outDir, sprintf('lite_maps_%s.png', scn)));
    close(f);
end
fprintf('[C] 双图已写出: lite_maps_S07/S09.png\n');

% ---------------- facts MD 落盘 ----------------
md{end+1} = '## 触发/矛盾清单';
md{end+1} = sprintf('- 条件补测触发（max|ΔP|>%.1f kW）: %d 个 -> %s', ctrlTol, numel(triggerList), strjoin(triggerList,'; '));
md{end+1} = sprintf('- 矛盾信号（min margin<0 且 ff 未激活）: %d 个 -> %s', numel(contradictList), strjoin(contradictList,'; '));
md{end+1} = sprintf('- 负裕度但 ff 激活（冷启动吸收段，T-008B 已记录）: %d 个 -> %s', numel(coldAbsorbList), strjoin(coldAbsorbList,'; '));
mdPath = fullfile(outDir,'lite_facts.md');
fid = fopen(mdPath,'w'); fprintf(fid,'%s\n', md{:}); fclose(fid);
fprintf('[C] lite_facts.md 已写出\n');

% ---------------- 机器可读终态 ----------------
fprintf('[C] 汇总: 点=%d 场景=%d 行=%d | 触发=%d 矛盾=%d 冷启动吸收=%d\n', ...
    16, numel(scens), height(T), numel(triggerList), numel(contradictList), numel(coldAbsorbList));
fprintf('[C] 最大 max|ΔP_gen_actual| 全网格: %.6f kW\n', max(T.max_dP_actual_kW));
fprintf('[C] 最小 margin_min 全网格: %.1f kW\n', min(T.margin_min_kW));

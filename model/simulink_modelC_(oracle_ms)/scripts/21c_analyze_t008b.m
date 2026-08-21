%% ========================================================================
%  21c_analyze_t008b.m — T-008B 机理数据包（只陈述事实与数值，不下结论）
%  输入: results/ms_phase1/t008b/ds_t008b_W1_*.mat（21b 产物）
%  输出:
%    results/ms_phase1/t008b/mechanism_metrics.csv  机器可读（kind: summary|saturation|event）
%    results/ms_phase1/t008b/mechanism_analysis.md  事实记录
%    results/ms_phase1/t008b/t008b_<S07|S09>_H45.png  时序图（4 子图）
%  判据相关数值全部如实输出；机理成立与否由主 Agent 判定。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));

outDir = fullfile(ROOT,'results','ms_phase1','t008b');
Rgen = 20;              % W1 爬坡率 [kW/s]
FFth = 0.5;             % P_ff_applied 有效阈值 [kW]（同 msff_metrics 默认）
DTG = 0.1;              % 公共时间网格 [s]
divTol = 0.5;           % 首个"有意义分歧"阈值 [kW]
riseTol = 20;           % 发电机上升判定阈值 [kW]（相对平台基线）

scens = {'S07','S09'};
Hs = [20 45 60];
loadDs = @(scn,ctrl) load(fullfile(outDir,sprintf('ds_t008b_W1_%s_%s.mat',scn,ctrl)));

% ---------------- 收集 ----
D = struct();   % D.(scn).(ctrl).ds
for s = 1:numel(scens)
    scn = scens{s};
    D.(scn).C0 = loadDs(scn,'C0').ds;
    for h = Hs
        D.(scn).(sprintf('CMS_H%d',h)) = loadDs(scn,sprintf('CMS_H%d',h)).ds;
    end
end

% ---------------- CSV 准备 ----------------
csvPath = fullfile(outDir,'mechanism_metrics.csv');
fid = fopen(csvPath,'w');
fprintf(fid,'kind,scenario,H,t,value,unit,note\n');

mdLines = {};
mdLines{end+1} = sprintf('# T-008B 机理数据包（事实记录，无结论）');
mdLines{end+1} = sprintf('');
mdLines{end+1} = sprintf('弱机点 W1: Gen_P_max=640 kW (λ_P=0.8), R_gen=20 kW/s, tau_gen=10 s; SOC0=0.60');
mdLines{end+1} = sprintf('公共网格: %.1f s；分歧阈值: %.1f kW；发电机上升阈值: %.1f kW', DTG, divTol, riseTol);
mdLines{end+1} = sprintf('');
mdLines{end+1} = sprintf('## 1. 概要');
mdLines{end+1} = sprintf('| scenario | run | t_end [s] | fuel [kg] | Vdc_min [V] | SOC_end | ff_active [s] |');

% ---------------- 概要表 ----------------
for s = 1:numel(scens)
    scn = scens{s};
    ctrls = ['C0', arrayfun(@(h)sprintf('CMS_H%d',h), Hs, 'UniformOutput',false)];
    for c = 1:numel(ctrls)
        ctrl = ctrls{c};
        ds = D.(scn).(ctrl);
        m = model.msff_metrics(ds);
        mdLines{end+1} = sprintf('| %s | %s | %.0f | %.2f | %.1f | %.4f | %.1f |', ...
            scn, ctrl, ds.t(end), m.fuel_kg, m.Vdc_min, m.SOC_end, m.ff_active_s);
        fprintf(fid,'summary,%s,%s,NaN,%.4f,kg,id=%s t_end=%.0f\n', scn, ctrl, m.fuel_kg, ctrl, ds.t(end));
    end
end

% ---------------- 逐 H 的 ΔP_gen_actual ----------------
mdLines{end+1} = sprintf('');
mdLines{end+1} = sprintf('## 2. max|ΔP_gen_actual|（CMS vs C0，0.1 s 网格）');
mdLines{end+1} = sprintf('| scenario | H [s] | max|ΔP_actual| [kW] | 首个分歧 t [s] (%.1f kW) | 首个大分歧 t [s] (>10 kW) |', divTol);

summaryMaxDiff = struct();   % 供 MD 汇总
for s = 1:numel(scens)
    scn = scens{s};
    ds0 = D.(scn).C0;
    t0 = ds0.t;
    for h = Hs
        ctrl = sprintf('CMS_H%d',h);
        ds3 = D.(scn).(ctrl);
        tEnd = min(t0(end), ds3.t(end));
        gridT = (0:DTG:tEnd)';
        a0 = interp1(t0, ds0.P_gen_actual_kW, gridT, 'linear');
        a3 = interp1(ds3.t, ds3.P_gen_actual_kW, gridT, 'linear');
        dA = a3 - a0;
        [mx, imx] = max(abs(dA));
        iDiv = find(abs(dA) > divTol, 1, 'first');
        iDiv10 = find(abs(dA) > 10, 1, 'first');
        tDiv = gridT(iDiv);   if isempty(tDiv), tDiv = NaN; end
        tDiv10 = gridT(iDiv10); if isempty(tDiv10), tDiv10 = NaN; end
        fprintf(fid,'summary,%s,%d,%d,%.4f,kW,max|dA|\n', scn, h, find(dA==mx,1), mx);
        fprintf(fid,'summary,%s,%d,%d,%.4f,s,first_div_%.1fkW\n', scn, h, find(abs(dA)>divTol,1), tDiv);
        fprintf(fid,'summary,%s,%d,%d,%.4f,s,first_div_10kW\n', scn, h, find(abs(dA)>10,1), tDiv10);
        mdLines{end+1} = sprintf('| %s | %d | %.4f | %.1f | %.1f |', scn, h, mx, tDiv, tDiv10);
        summaryMaxDiff.(scn).(ctrl) = mx;
    end
end

% ---------------- 阶跃事件分析 ----------------
mdLines{end+1} = sprintf('');
mdLines{end+1} = sprintf('## 3. 需求阶跃事件（|ΔP_prop_demand|>100 kW/5s，每场景最多前 3 个）');
mdLines{end+1} = sprintf('| scenario | H [s] | # | t_step [s] | step_mag [kW] | gen_rise_C0 [s] | gen_rise_CMS [s] | Δlead [s] | ff_win [s] | ff_raw_peak [kW] | pre_ramp |');

for s = 1:numel(scens)
    scn = scens{s};
    ds0 = D.(scn).C0;
    t0 = ds0.t;
    pd = ds0.P_prop_demand_kW;
    % 统一 0.1s 网格上找阶跃
    tEnd = t0(end);
    gridT = (0:DTG:tEnd)';
    pdG = interp1(t0, pd, gridT, 'linear');
    N = numel(gridT);
    rise = false(N,1);
    win = round(5/DTG);          % 5 s 窗口
    for i = 1:N-win
        if pdG(i+win) - pdG(i) > 100
            rise(i) = true;
        end
    end
    % 聚类连续段
    events = [];
    inEv = false; evStart = 0;
    for i = 1:N
        if rise(i) && ~inEv, inEv = true; evStart = i; end
        if ~rise(i) && inEv
            % 事件 [evStart, i]
            j0 = max(1, evStart-20); j1 = min(N, i+20);
            % 阶跃到达时刻 = 上升起点（网格）
            tStep = gridT(evStart);
            base = mean(pdG(max(1,evStart-400):max(1,evStart-50)));  % 平台基线（上升前 5~40 s 均值）
            if isnan(base) || ~isfinite(base), base = pdG(max(1,evStart-50)); end
            pk = max(pdG(evStart:min(N,evStart+round(30/DTG))));
            events(end+1,:) = [tStep, pk-base, evStart]; %#ok<AGROW>
            inEv = false;
        end
    end
    if inEv
        tStep = gridT(evStart);
        base = mean(pdG(max(1,evStart-400):max(1,evStart-50)));
        pk = max(pdG(evStart:min(N,evStart+round(30/DTG))));
        events(end+1,:) = [tStep, pk-base, evStart]; %#ok<AGROW>
    end
    % 合并重叠事件（间隔 <10 s 视为同一事件，取首个）
    if size(events,1) > 1
        keep = true(size(events,1),1);
        for i = 2:size(events,1)
            if events(i,1) - events(i-1,1) < 10, keep(i) = false; end
        end
        events = events(keep,:);
    end
    nEv = min(3, size(events,1));
    for e = 1:nEv
        tStep = events(e,1); mag = events(e,2);
        % 发电机上升起点（C0 与各 CMS）
        for h = [0 Hs]
            if h == 0
                dsx = ds0; ctrlx = 'C0'; hhx = 'C0';
            else
                ctrlx = sprintf('CMS_H%d',h); dsx = D.(scn).(ctrlx); hhx = ctrlx;
            end
            % 平台基线（tStep-40 ~ tStep-10）
            iB = find(dsx.t >= tStep-40 & dsx.t <= tStep-10);
            if isempty(iB), iB = find(dsx.t >= 0 & dsx.t <= tStep-5); end
            if isempty(iB), gBase = NaN; riseT = NaN;
            else
                gBase = mean(dsx.P_gen_actual_kW(iB));
                % 上升起点：tStep-5 之后首个持续超过基线的时刻
                iS = find(dsx.t >= tStep-5, 1, 'first');
                riseT = NaN;
                for i = iS:numel(dsx.t)-10
                    if dsx.P_gen_actual_kW(i) >= gBase + riseTol && ...
                       mean(dsx.P_gen_actual_kW(i:i+10)) >= gBase + riseTol
                        riseT = dsx.t(i); break;
                    end
                end
            end
            fprintf(fid,'event,%s,%d,%d,%.2f,s,t_step\n', scn, h, e, tStep);
            fprintf(fid,'event,%s,%d,%d,%.2f,kW,step_mag\n', scn, h, e, mag);
            fprintf(fid,'event,%s,%d,%d,%.2f,s,gen_rise\n', scn, h, e, riseT);
            if h == 0
                riseC0 = riseT;
            else
                riseCMS.(ctrlx) = riseT;
            end
        end
        % ff 窗口（CMS H45 为代表记录；其余 H 同样记录窗口）
        for h = Hs
            ctrlx = sprintf('CMS_H%d',h); dsx = D.(scn).(ctrlx);
            iF = find(dsx.t >= tStep-60 & dsx.t <= tStep+60 & dsx.P_ff_applied_kW > FFth);
            if isempty(iF)
                ffs = NaN; ffe = NaN; ffpk = NaN; pre = 0;
            else
                ffs = dsx.t(iF(1)); ffe = dsx.t(iF(end));
                ffpk = max(dsx.P_ff_raw_kW(iF));
                pre = double(ffs < tStep - 1);
            end
            fprintf(fid,'event,%s,%d,%d,%.2f,s,ff_start\n', scn, h, e, ffs);
            fprintf(fid,'event,%s,%d,%d,%.2f,s,ff_end\n', scn, h, e, ffe);
            fprintf(fid,'event,%s,%d,%d,%.2f,kW,ff_raw_peak\n', scn, h, e, ffpk);
            fprintf(fid,'event,%s,%d,%d,%d,-,pre_ramp\n', scn, h, e, pre);
            if h == 45
                ffWin = sprintf('%.0f~%.0f', ffs, ffe);
                ffPk45 = ffpk; pre45 = pre;
            end
        end
        mdLines{end+1} = sprintf('| %s | 45 | %d | %.0f | %.0f | %.1f | %.1f | %.1f | %s | %.0f | %d |', ...
            scn, e, tStep, mag, riseC0, riseCMS.CMS_H45, riseC0-riseCMS.CMS_H45, ffWin, ffPk45, pre45);
    end
    fprintf(fid,'summary,%s,NaN,%d,%d,-,n_events\n', scn, nEv);
end

% ---------------- 爬坡饱和度 ----------------
mdLines{end+1} = sprintf('');
mdLines{end+1} = sprintf('## 4. 爬坡饱和度（ff 激活窗口内，0.1 s 网格）');
mdLines{end+1} = sprintf('| scenario | H [s] | ff_active [s] | \\|d(rampout)/dt\\|≥0.95·R_gen 占比 | \\|ref−rampout\\|>50 kW 占比 |');

for s = 1:numel(scens)
    scn = scens{s};
    for h = Hs
        ctrlx = sprintf('CMS_H%d',h); dsx = D.(scn).(ctrlx);
        tEnd = dsx.t(end);
        gridT = (0:DTG:tEnd)';
        rp = interp1(dsx.t, dsx.P_gen_rampout_kW, gridT, 'linear');
        rf = interp1(dsx.t, dsx.P_gen_ref_kW, gridT, 'linear');
        ff = interp1(dsx.t, dsx.P_ff_applied_kW, gridT, 'linear');
        iW = ff > FFth;
        if any(iW)
            dr = abs(diff(rp(iW)))/DTG;
            fr1 = mean(dr >= 0.95*Rgen);
            fr2 = mean(abs(rf(iW(1:end-1)) - rp(iW(1:end-1))) > 50);
            fprintf(fid,'saturation,%s,%d,%d,%.4f,-,ramp_rate_sat_frac\n', scn, h, 1, fr1);
            fprintf(fid,'saturation,%s,%d,%d,%.4f,-,ref_gap_frac\n', scn, h, 2, fr2);
            mdLines{end+1} = sprintf('| %s | %d | %.0f | %.3f | %.3f |', scn, h, sum(ff>FFth)*DTG, fr1, fr2);
        else
            mdLines{end+1} = sprintf('| %s | %d | 0 | - | - |', scn, h);
        end
    end
end

fclose(fid);

% ---------------- 图 ----------------
for s = 1:numel(scens)
    scn = scens{s};
    ds0 = D.(scn).C0;
    ds3 = D.(scn).CMS_H45;
    f = figure('Visible','off','Position',[100 100 1100 800]);
    % ① 需求
    subplot(4,1,1);
    plot(ds0.t, ds0.P_prop_demand_kW, 'b', 'LineWidth', 1); hold on;
    plot(ds0.t, ds0.P_load_total_kW, 'Color',[0.6 0.6 0.6]);
    legend('P_prop_demand','P_load_total','Location','best'); grid on;
    title(sprintf('%s (W1) C0 vs CMS H45', scn)); ylabel('kW');
    % ② gen actual
    subplot(4,1,2);
    plot(ds0.t, ds0.P_gen_actual_kW, 'k', 'LineWidth', 1.2); hold on;
    plot(ds3.t, ds3.P_gen_actual_kW, 'r--', 'LineWidth', 1.2);
    legend('C0','CMS H45','Location','best'); grid on; ylabel('P_gen_actual kW');
    % ③ ff
    subplot(4,1,3);
    plot(ds3.t, ds3.P_ff_raw_kW, 'b', 'LineWidth', 0.8); hold on;
    plot(ds3.t, ds3.P_ff_applied_kW, 'r', 'LineWidth', 1.2);
    legend('P_ff_raw','P_ff_applied','Location','best'); grid on; ylabel('ff kW');
    % ④ ref vs rampout（标饱和段）
    subplot(4,1,4);
    tE = min(ds3.t(end), ds0.t(end));
    gridT = (0:DTG:tE)';
    rf = interp1(ds3.t, ds3.P_gen_ref_kW, gridT, 'linear');
    rp = interp1(ds3.t, ds3.P_gen_rampout_kW, gridT, 'linear');
    ff = interp1(ds3.t, ds3.P_ff_applied_kW, gridT, 'linear');
    plot(gridT, rf, 'b', 'LineWidth', 0.8); hold on;
    plot(gridT, rp, 'r', 'LineWidth', 1.2);
    sat = abs(rf - rp) > 50;
    if any(sat)
        % 找连续饱和段并画背景
        d = diff([0; sat; 0]);
        st = find(d == 1); en = find(d == -1) - 1;
        for k = 1:min(numel(st),20)
            xp = gridT(st(k):en(k));
            ytop = max([max(rf),max(rp)])*ones(size(xp));
            patch([xp(1); xp; xp(end)], [0; ytop; 0], [1 0.9 0.3], 'EdgeColor','none', 'FaceAlpha', 0.25);
        end
    end
    legend('P_gen_ref','P_gen_rampout','饱和段(|ref-rampout|>50kW)','Location','best');
    grid on; ylabel('kW'); xlabel('t [s]');
    saveas(f, fullfile(outDir, sprintf('t008b_%s_H45.png', scn)));
    close(f);
end

% ---------------- MD 落盘 ----------------
mdPath = fullfile(outDir,'mechanism_analysis.md');
fid = fopen(mdPath,'w');
fprintf(fid, '%s\n', mdLines{:});
fclose(fid);

fprintf('[C] mechanism_metrics.csv + mechanism_analysis.md + 2 PNG 已写出: %s\n', outDir);

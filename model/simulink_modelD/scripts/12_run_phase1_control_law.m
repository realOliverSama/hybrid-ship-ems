%% ========================================================================
%  12_run_phase1_control_law.m — Phase 1 主实验：E0–E3 控制律对照 + τ_margin 扫描
%  Model D 定时前馈 EMS 研究（spec: timed-feedforward-ems, T-007）
%  场景: S04 阻力渐增 / S06 阻力阶跃(未见) / S07 桥区机动 / S09 应急出动(未见)
%  实验: E0(C0 Baseline) / E1(C1 即时+oracle) / E2(C2 定时+oracle) / E3(C2+MLP幅值+oracle t_peak)
%  扫描: ASSUMED_TFF_TauMargin ∈ {5,15,20}（仅 S04/S06，τ=10 复用主实验 E2）
%  可断点续跑: 每次仿真结果存 results/phase1/Sxx/<exp>.mat，已存在则跳过；
%             CSV 在脚本末尾从全部 .mat 重新生成（幂等，resume 不产生重复行）。
%  输出: results/phase1/phase1_metrics.csv + results/phase1/Sxx/compare.png
%  执行: cd model/simulink_modelD && matlab -batch "eval(fileread('scripts/12_run_phase1_control_law.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

% ---- 加载 MLP 权重（E3 环内推理用；构建脚本已自动载入，此处双保险） ----
wfile = fullfile(ROOT, 'python', 'export', 'mlp_weights.mat');
if ~exist(wfile, 'file')
    error('未找到 %s', wfile);
end
W = load(wfile);
fns = fieldnames(W);
for i = 1:numel(fns)
    if startsWith(fns{i}, 'MLP_') && isnumeric(W.(fns{i}))
        assignin('base', fns{i}, double(W.(fns{i})));
    end
end
assignin('base', 'MLP_WeightsLoaded', 1);
fprintf('[12] MLP 权重已加载 (window=%d, ch=%d)\n', W.MLP_Window, W.MLP_NCh);

MDL = 'HybridShip_ModelD_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
assignin('base', 'EMS_WORK_MODE', 2);

outRoot = fullfile(ROOT, 'results', 'phase1');
if ~exist(outRoot, 'dir'), mkdir(outRoot); end
csvPath = fullfile(outRoot, 'phase1_metrics.csv');

testScen = [4, 6, 7, 9];
stopTimeLimit = 5000 / 5.66 + 1800;

Kff = evalin('base', 'ASSUMED_EMS_FF_Gain');
FFmax = evalin('base', 'ASSUMED_EMS_FF_Max_kW');

for s = testScen
    SCN = SCENARIOS(s);
    sdir = fullfile(outRoot, sprintf('S%02d', s));
    if ~exist(sdir, 'dir'), mkdir(sdir); end
    fprintf('\n[12] ========== %s (S%02d) ==========\n', SCN.name, s);

    R = {};   % {expName, tau, ds, m}

    % ---------------------------------------------------------------- E0
    fE0 = fullfile(sdir, 'E0.mat');
    if exist(fE0, 'file')
        load(fE0, 'ds', 'm');
        fprintf('[12] E0 复用已存结果\n');
    else
        assignin('base', 'AI_Pred_Source', 0);
        assignin('base', 'AI_TPeak_Source', 0);
        assignin('base', 'ASSUMED_AI_DataValid', false);
        try
            tic;
            out = model.run_scenario_sim(MDL, SCN, 0, stopTimeLimit);
            ds = model.extract_logsout(out);
            m = model.tff_metrics(ds);
            save(fE0, 'ds', 'm');
            fprintf('[12] E0 完成 (%.0f s, %d 点): fuel=%.2f kg Vdc_min=%.1f V SOC_end=%.3f (%.0f s 仿真)\n', ...
                ds.t(end), numel(ds.t), m.fuel_kg, m.Vdc_min, m.SOC_end, toc);
        catch err
            fprintf('[12] E0 失败: %s\n', err.message);
            continue;
        end
    end
    R{end + 1} = {'E0', [], ds, m};

    % ------------------------------------------------------- oracle 表
    oracleFile = fullfile(outRoot, sprintf('oracle_S%02d.mat', s));
    D = model.make_oracle_pred(MDL, SCN, oracleFile, 60, 1, ds);
    assignin('base', 'AI_Oracle_Data', D);
    assignin('base', 'ASSUMED_AI_DataValid', true);
    fprintf('[12] oracle: N=%d, P_peak∈[%.0f,%.0f] kW\n', size(D, 1), min(D(:, 2)), max(D(:, 2)));

    % ---------------------------------------------------------------- E1
    fE1 = fullfile(sdir, 'E1.mat');
    if exist(fE1, 'file')
        load(fE1, 'ds', 'm');
        fprintf('[12] E1 复用已存结果\n');
    else
        assignin('base', 'AI_Pred_Source', 1);
        assignin('base', 'AI_TPeak_Source', 0);
        try
            tic;
            out = model.run_scenario_sim(MDL, SCN, 1, stopTimeLimit);
            ds = model.extract_logsout(out);
            m = model.tff_metrics(ds);
            save(fE1, 'ds', 'm');
            fprintf('[12] E1 完成 (%.0f s): fuel=%.2f kg Vdc_min=%.1f V (%.0f s 仿真)\n', ...
                ds.t(end), m.fuel_kg, m.Vdc_min, toc);
        catch err
            fprintf('[12] E1 失败: %s\n', err.message);
            continue;
        end
    end
    R{end + 1} = {'E1', [], ds, m};

    % ---------------------------------------------------------------- E2
    fE2 = fullfile(sdir, 'E2.mat');
    if exist(fE2, 'file')
        load(fE2, 'ds', 'm');
        fprintf('[12] E2 复用已存结果\n');
    else
        assignin('base', 'AI_Pred_Source', 1);
        assignin('base', 'AI_TPeak_Source', 0);
        assignin('base', 'ASSUMED_TFF_TauMargin', 10);
        try
            tic;
            out = model.run_scenario_sim(MDL, SCN, 2, stopTimeLimit);
            ds = model.extract_logsout(out);
            m = model.tff_metrics(ds);
            save(fE2, 'ds', 'm');
            fprintf('[12] E2 完成 (%.0f s): fuel=%.2f kg Vdc_min=%.1f V P_ff_active=%.0f s (%.0f s 仿真)\n', ...
                ds.t(end), m.fuel_kg, m.Vdc_min, m.P_ff_activated_s, toc);
        catch err
            fprintf('[12] E2 失败: %s\n', err.message);
            continue;
        end
    end
    R{end + 1} = {'E2', [], ds, m};

    % ---------------------------------------------------------------- E3
    fE3 = fullfile(sdir, 'E3.mat');
    if exist(fE3, 'file')
        load(fE3, 'ds', 'm');
        fprintf('[12] E3 复用已存结果\n');
    else
        assignin('base', 'AI_Pred_Source', 3);
        assignin('base', 'AI_TPeak_Source', 1);
        assignin('base', 'ASSUMED_TFF_TauMargin', 10);
        try
            tic;
            out = model.run_scenario_sim(MDL, SCN, 2, stopTimeLimit);
            ds = model.extract_logsout(out);
            m = model.tff_metrics(ds);
            save(fE3, 'ds', 'm');
            fprintf('[12] E3 完成 (%.0f s): fuel=%.2f kg Vdc_min=%.1f V P_ff_active=%.0f s (%.0f s 仿真)\n', ...
                ds.t(end), m.fuel_kg, m.Vdc_min, m.P_ff_activated_s, toc);
        catch err
            fprintf('[12] E3 失败: %s\n', err.message);
            continue;
        end
    end
    R{end + 1} = {'E3', [], ds, m};

    % ------------------------------------------- τ_margin 扫描（S04/S06）
    if ismember(s, [4, 6])
        for tau = [5, 15, 20]
            fTau = fullfile(sdir, sprintf('E2_tau%d.mat', tau));
            if exist(fTau, 'file')
                load(fTau, 'ds', 'm');
                fprintf('[12] E2 τ=%d 复用已存结果\n', tau);
            else
                assignin('base', 'AI_Pred_Source', 1);
                assignin('base', 'AI_TPeak_Source', 0);
                assignin('base', 'ASSUMED_TFF_TauMargin', tau);
                try
                    tic;
                    out = model.run_scenario_sim(MDL, SCN, 2, stopTimeLimit);
                    ds = model.extract_logsout(out);
                    m = model.tff_metrics(ds);
                    save(fTau, 'ds', 'm');
                    fprintf('[12] E2 τ=%d 完成: fuel=%.2f kg Vdc_min=%.1f V P_ff_active=%.0f s (%.0f s 仿真)\n', ...
                        tau, m.fuel_kg, m.Vdc_min, m.P_ff_activated_s, toc);
                catch err
                    fprintf('[12] E2 τ=%d 失败: %s\n', tau, err.message);
                    continue;
                end
            end
            R{end + 1} = {'E2', tau, ds, m};
        end
        assignin('base', 'ASSUMED_TFF_TauMargin', 10);   % 恢复默认
    end

    % ------------------------------------------------------- sanity check
    mE0 = []; mE1 = []; mE2 = [];
    for k = 1:numel(R)
        if strcmp(R{k}{1}, 'E0'), mE0 = R{k}{4}; end
        if strcmp(R{k}{1}, 'E1'), mE1 = R{k}{4}; end
        if strcmp(R{k}{1}, 'E2') && isempty(R{k}{2}), mE2 = R{k}{4}; end
    end
    if ~isempty(mE1) && ~isempty(mE0)
        if mE1.Vdc_min < mE0.Vdc_min - 0.5
            fprintf('[12][WARN] %s: E1 Vdc_min=%.1f < E0 Vdc_min=%.1f - 0.5（完美预测即时前馈未改善母线）\n', ...
                SCN.name, mE1.Vdc_min, mE0.Vdc_min);
        else
            fprintf('[12][OK]   %s: E1 Vdc_min=%.1f >= E0 %.1f - 0.5\n', SCN.name, mE1.Vdc_min, mE0.Vdc_min);
        end
    end
    if ~isempty(mE2) && ~isempty(mE1)
        % 重构 C1 前馈活动时长（law=1 不记录 P_ff_kW，由 P_pred/P_load 重构）
        dtv = [diff(R{2}{3}.t); 0.01];
        ff1 = min(max(R{2}{3}.P_pred_max_kW - R{2}{3}.P_load_total_kW, 0) * Kff, FFmax);
        ff1_active = sum(dtv(ff1 > 0.5));
        fprintf('[12][FF]   %s: E1 FF 活动=%.0f s, E2 FF 活动=%.0f s（定时应显著小于即时）\n', ...
            SCN.name, ff1_active, mE2.P_ff_activated_s);
    end

    % ------------------------------------------------------- 出图
    if numel(R) >= 4
        model.tff_plot_compare(sdir, SCN.name, R);
        fprintf('[12] 图: %s\n', fullfile(sdir, 'compare.png'));
    end
end

% ============================================================== CSV 汇总
% 幂等重建：从所有 .mat 重新生成完整 CSV（resume 安全，无重复行）
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
fprintf(fid, '%s\n', 'scenario,exp,tau_margin,fuel_kg,Vdc_min,Vdc_max,SOC_end,SOC_min,E_bat_dis_kWh,P_bat_rms_kW,P_bat_peak_abs_kW,t_sat_s,E_unserved_kWh,v_rmse_mps,P_ff_activated_s,ff_active_s,t_first_ff_s');
fclose(fid);
for s = testScen
    sdir = fullfile(outRoot, sprintf('S%02d', s));
    if ~exist(sdir, 'dir'), continue; end
    files = dir(fullfile(sdir, '*.mat'));
    for k = 1:numel(files)
        [~, expn] = fileparts(files(k).name);
        if ~startsWith(expn, 'E'), continue; end
        tmp = load(fullfile(sdir, files(k).name));
        ds = tmp.ds; m = tmp.m;
        tau = 10;
        if startsWith(expn, 'E2_tau'), tau = str2double(expn(strfind(expn, '_tau') + 4:end)); end
        ffActive = NaN; ffFirst = NaN;
        if strcmp(expn, 'E1')
            dtv = [diff(ds.t); 0.01];
            ff1 = min(max(ds.P_pred_max_kW - ds.P_load_total_kW, 0) * Kff, FFmax);
            ffActive = sum(dtv(ff1 > 0.5));
            ix = find(ff1 > 0.5, 1, 'first');
            if ~isempty(ix), ffFirst = ds.t(ix); end
        elseif startsWith(expn, 'E2') || strcmp(expn, 'E3')
            ffActive = m.P_ff_activated_s;
        end
        model.write_phase1_csv(csvPath, sprintf('S%02d', s), expn, tau, m, ffActive, ffFirst);
    end
end
fprintf('\n[12] Phase 1 全部完成。CSV: %s\n', csvPath);

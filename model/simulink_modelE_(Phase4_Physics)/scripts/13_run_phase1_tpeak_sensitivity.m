%% ========================================================================
%  13_run_phase1_tpeak_sensitivity.m — Phase 1 t_peak 误差敏感性分析（T-008）
%  Model D 定时前馈 EMS 研究（spec: timed-feedforward-ems, FR-009, design §4.3）
%  对象: 场景 {S04 阻力渐增 / S06 阻力阶跃 / S09 应急出动}，控制律固定 C2
%        (EMS_CTRL_LAW=2, AI_Pred_Source=1, AI_TPeak_Source=0, EMS_WORK_MODE=2,
%         ASSUMED_TFF_TauMargin=10)
%  注入: oracle 表第 3 列 t_peak_rel 系统偏移 δ ∈ {−20, −10, −5, +5, +10, +20} s
%        D_shift(:,3) = max(0, min(60, D(:,3)+δ))
%        （负偏移 = 预测峰值比实际更晚 = 启动偏晚 = 最危险方向）
%  断点续跑: 每次仿真存 results/phase1/sensitivity/Sxx_delta±N.mat，已存在则跳过；
%            CSV 末尾从全部 .mat 幂等重建（resume 无重复行）。
%  输出: results/phase1/sensitivity/Sxx_delta±N.mat
%        results/phase1/tpeak_sensitivity.csv
%        results/phase1/sensitivity/sensitivity.png
%  附加诊断（S04）: Vdc_min 事件时刻定位 + E2 P_ff 激活状态 + E3 幅值偏差量化
%  执行: cd model/simulink_modelD && matlab -batch "eval(fileread('scripts/13_run_phase1_tpeak_sensitivity.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

MDL = 'HybridShip_ModelD_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
assignin('base', 'EMS_WORK_MODE', 2);

% ---- 加载 MLP 权重（ScheduledFF 变体 ai_predict 图需要 MLP_W1 等参数与声明尺寸一致；
%       T-007 脚本同样在开头加载；Source=1 虽不走 MLP 分支，图参数仍须匹配） ----
wfile = fullfile(ROOT, 'python', 'export', 'mlp_weights.mat');
if ~exist(wfile, 'file')
    error('未找到 %s（需先运行 python/train_load_predictor.py 导出权重）', wfile);
end
W = load(wfile);
fns = fieldnames(W);
for i = 1:numel(fns)
    if startsWith(fns{i}, 'MLP_') && isnumeric(W.(fns{i}))
        assignin('base', fns{i}, double(W.(fns{i})));
    end
end
assignin('base', 'MLP_WeightsLoaded', 1);
fprintf('[13] MLP 权重已加载 (window=%d, ch=%d)\n', W.MLP_Window, W.MLP_NCh);

outRoot  = fullfile(ROOT, 'results', 'phase1');
sensRoot = fullfile(outRoot, 'sensitivity');
if ~exist(sensRoot, 'dir'), mkdir(sensRoot); end
csvPath  = fullfile(outRoot, 'tpeak_sensitivity.csv');

testScen = [4, 6, 9];
deltas   = [-20, -10, -5, 5, 10, 20];
stopTimeLimit = 5000 / 5.66 + 1800;

% =================================================================
% 第一段：18 次仿真（断点续跑）
% =================================================================
for s = testScen
    SCN = SCENARIOS(s);
    fprintf('\n[13] ========== %s (S%02d) ==========\n', SCN.name, s);

    oracleFile = fullfile(outRoot, sprintf('oracle_S%02d.mat', s));
    if ~exist(oracleFile, 'file')
        fprintf('[13][WARN] 缺少 oracle: %s（跳过场景）\n', oracleFile);
        continue;
    end
    O = load(oracleFile);
    D = O.D;   % N×3 = [t, P_peak, t_peak_rel]

    for d = deltas
        fMat = fullfile(sensRoot, sprintf('S%02d_delta%+d.mat', s, d));
        if exist(fMat, 'file')
            fprintf('[13] S%02d δ=%+d 复用已存结果\n', s, d);
            continue;
        end

        % 注入 t_peak 系统偏移（裁剪到 [0,60]）
        D_shift = D;
        D_shift(:, 3) = max(0, min(60, D(:, 3) + d));

        assignin('base', 'AI_Oracle_Data', D_shift);
        assignin('base', 'AI_Pred_Source', 1);
        assignin('base', 'AI_TPeak_Source', 0);
        assignin('base', 'ASSUMED_TFF_TauMargin', 10);
        assignin('base', 'ASSUMED_AI_DataValid', true);

        try
            tic;
            out = model.run_scenario_sim(MDL, SCN, 2, stopTimeLimit);
            ds = model.extract_logsout(out);
            m  = model.tff_metrics(ds);
            save(fMat, 'ds', 'm', 's', 'd');
            fprintf([ ...
                '[13] S%02d δ=%+3d 完成 (%.0f s, %d 点): fuel=%.2f kg Vdc_min=%.1f V ', ...
                'P_ff_active=%.0f s (仿真 %.0f s)\n'], ...
                s, d, ds.t(end), numel(ds.t), m.fuel_kg, m.Vdc_min, m.P_ff_activated_s, toc);
        catch err
            fprintf('[13][FAIL] S%02d δ=%+d: %s\n', s, d, err.message);
        end
    end
end
assignin('base', 'ASSUMED_TFF_TauMargin', 10);   % 恢复默认

% =================================================================
% 第二段：CSV 汇总（幂等重建，δ=0 基线 = T-007 E2 结果）
% =================================================================
fprintf('\n[13] 汇总 CSV ...\n');
fid = fopen(csvPath, 'w', 'n', 'UTF-8');
fprintf(fid, '%s\n', 'scenario,delta_s,fuel_kg,Vdc_min,SOC_end,P_ff_activated_s,t_sat_s,dfuel_kg,dVdc_V');
fclose(fid);

refM = containers.Map();   % 场景 -> E2(δ=0) 指标
for s = testScen
    fE2 = fullfile(outRoot, sprintf('S%02d', s), 'E2.mat');
    if exist(fE2, 'file')
        E2 = load(fE2);
        refM(num2str(s)) = model.tff_metrics(E2.ds);
    end
end

for s = testScen
    for d = deltas
        fMat = fullfile(sensRoot, sprintf('S%02d_delta%+d.mat', s, d));
        if ~exist(fMat, 'file'), continue; end
        T = load(fMat);
        m = T.m;
        dfuel = NaN; dVdc = NaN;
        if refM.isKey(num2str(s))
            m0 = refM(num2str(s));
            dfuel = m.fuel_kg - m0.fuel_kg;
            dVdc  = m.Vdc_min - m0.Vdc_min;
        end
        fid = fopen(csvPath, 'a', 'n', 'UTF-8');
        fprintf(fid, 'S%02d,%+d,%.4f,%.2f,%.4f,%.2f,%.2f,%.4f,%.2f\n', ...
            s, d, m.fuel_kg, m.Vdc_min, m.SOC_end, m.P_ff_activated_s, m.t_sat_s, dfuel, dVdc);
        fclose(fid);
    end
end
fprintf('[13] CSV: %s\n', csvPath);

% =================================================================
% 第三段：出图 sensitivity.png（2 行 × 3 列：Vdc_min / fuel，按场景）
% =================================================================
fprintf('[13] 出图 ...\n');
f = figure('Visible', 'off', 'Position', [80 80 1200 700]);
for k = 1:numel(testScen)
    s = testScen(k);
    Ds = []; fuel = []; vdc = [];
    for d = deltas
        fMat = fullfile(sensRoot, sprintf('S%02d_delta%+d.mat', s, d));
        if ~exist(fMat, 'file'), continue; end
        T = load(fMat);
        Ds(end + 1) = d;   %#ok<AGROW>
        fuel(end + 1) = T.m.fuel_kg;   %#ok<AGROW>
        vdc(end + 1)  = T.m.Vdc_min;   %#ok<AGROW>
    end
    if isempty(Ds), continue; end
    [Ds, ix] = sort(Ds); fuel = fuel(ix); vdc = vdc(ix);

    subplot(2, numel(testScen), k);
    plot(Ds, vdc, 'r-o', 'LineWidth', 1.5, 'MarkerSize', 6);
    grid on; xlabel('\delta_{t\_peak} [s]'); ylabel('V_{dc,min} [V]');
    title(sprintf('S%02d：V_{dc,min}', s), 'Interpreter', 'none');
    xlim([-25, 25]);

    subplot(2, numel(testScen), k + numel(testScen));
    plot(Ds, fuel, 'b-o', 'LineWidth', 1.5, 'MarkerSize', 6);
    grid on; xlabel('\delta_{t\_peak} [s]'); ylabel('fuel [kg]');
    title(sprintf('S%02d：fuel', s), 'Interpreter', 'none');
    xlim([-25, 25]);
end
sgtitle('Phase 1 t_{peak} 误差敏感性（C2 定时前馈, τ=10 s）');
exportgraphics(f, fullfile(sensRoot, 'sensitivity.png'), 'Resolution', 140);
close(f);
fprintf('[13] 图: %s\n', fullfile(sensRoot, 'sensitivity.png'));

% =================================================================
% 第四段：S04 附加诊断（T-009 报告证据）
% =================================================================
fprintf('\n[13] ========== S04 附加诊断 ==========\n');
try
    E0 = load(fullfile(outRoot, 'S04', 'E0.mat'));
    E2 = load(fullfile(outRoot, 'S04', 'E2.mat'));
    E3 = load(fullfile(outRoot, 'S04', 'E3.mat'));
    O4 = load(fullfile(outRoot, 'oracle_S04.mat'));

    % --- 1) Vdc_min 事件定位（E0 基准轨迹） ---
    [vmin, i0] = min(E0.ds.V_dc_V);
    t0 = E0.ds.t(i0);
    fprintf('[D1] E0 Vdc_min=%.1f V 出现在 t=%.1f s\n', vmin, t0);
    % 1 s 邻域工况上下文
    iw = max(1, i0 - 50):min(numel(E0.ds.t), i0 + 50);
    fprintf('[D1] 邻域 P_load=%.0f..%.0f kW, v_ship=%.2f..%.2f m/s, x_ship=%.0f..%.0f m\n', ...
        min(E0.ds.P_load_total_kW(iw)), max(E0.ds.P_load_total_kW(iw)), ...
        min(E0.ds.v_ship_mps(iw)), max(E0.ds.v_ship_mps(iw)), ...
        min(E0.ds.x_ship_m(iw)), max(E0.ds.x_ship_m(iw)));
    fprintf('[D1] 初始加速段? x_ship=%.0f m (航程 5000 m)\n', E0.ds.x_ship_m(i0));

    % --- 2) E2(oracle C2) 在该时刻前后 P_ff 激活状态 ---
    j = find(E2.ds.t >= t0 - 10 & E2.ds.t <= t0 + 10);
    fprintf('[D2] E2 在 t0±10 s 内 P_ff: 最大=%.1f kW, 激活采样数=%d/%d\n', ...
        max(E2.ds.P_ff_kW(j)), sum(E2.ds.P_ff_kW(j) > 0.5), numel(j));
    if E2.ds.t(end) >= t0
        [~, j2] = min(abs(E2.ds.t - t0));
        fprintf('[D2] E2 在 t0 时刻 P_ff=%.1f kW, P_gen_ref=%.0f kW, Vdc=%.1f V\n', ...
            E2.ds.P_ff_kW(j2), E2.ds.P_gen_ref_kW(j2), E2.ds.V_dc_V(j2));
    end

    % --- 3) E3(MLP 幅值) 在该时刻的幅值偏差 ---
    if E3.ds.t(end) >= t0 && isfield(E3.ds, 'P_pred_max_kW')
        [~, j3] = min(abs(E3.ds.t - t0));
        p_mlp = E3.ds.P_pred_max_kW(j3);
        p_ora = interp1(O4.D(:, 1), O4.D(:, 2), t0, 'linear', 'extrap');
        fprintf('[D3] t0 时刻: MLP 预测幅值=%.1f kW vs oracle P_peak=%.1f kW, 偏差=%+.1f kW (%+.1f%%)\n', ...
            p_mlp, p_ora, p_mlp - p_ora, 100 * (p_mlp - p_ora) / max(p_ora, 1));
        % 航次平均偏差
        p_mlp_v = interp1(E3.ds.t, E3.ds.P_pred_max_kW, O4.D(:, 1), 'linear', 'extrap');
        valid = isfinite(p_mlp_v);
        fprintf('[D3] 航次 MLP vs oracle P_peak: MAE=%.1f kW, 均值偏差=%+.1f kW, 低估样本占比=%.1f%%\n', ...
            mean(abs(p_mlp_v(valid) - O4.D(valid, 2))), ...
            mean(p_mlp_v(valid) - O4.D(valid, 2)), ...
            100 * mean(p_mlp_v(valid) < O4.D(valid, 2) - 0.5));
    else
        fprintf('[D3] E3 数据不可用（t0=%.1f s 超出航次或缺少 P_pred 字段）\n', t0);
    end
catch err
    fprintf('[D][FAIL] S04 附加诊断失败: %s\n', err.message);
end

fprintf('\n[13] T-008 敏感性分析全部结束。\n');

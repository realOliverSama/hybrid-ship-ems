function results = test_law7(ROOT)
% ecms2.test_law7 — T-204 law=7 NN-A-ECMS 单元测试（design §5，FR-208；Amendment v1.2 extrinsic 兜底）
%
%  T1 ecms2.nn_predict_ehat vs Python 推理：直接调用函数（含 300 s P_load 环形缓冲/
%     归一化/MLP 前向持久状态），逐测试点喂入 300 个时间升序样本后捕获输出，与
%     .venv_tf/bin/python（train_predictor.py 同款 _np_mlp_forward）对比，
%     100 组随机输入，归一化空间 max|Δ| < 1e-5。函数特征布局与
%     datasets/fleet_norm.json feature_layout 一致（302 维时间升序 + v_ship + accel）。
%  T2 回退路径（权重 mat 缺失 -> ê=0）：临时 rename config/ecms_nn_weights.mat，
%     短 S01 × law=7 闭环试跑，P_gen_ref ≡ 离线 ecms2.ecms_law(law=6, e_hat=0)
%     （容差 0）。ê=0 时 SOC_ref_eff=SOC_ref，λ 下限钳位按 law=6 v2 语义仍生效
%     （K_S=0.01≠0）——与 law=5 仅差 λ floor（0.43 vs 0.425，design v1.1），
%     即“预测通道惰性”意义上的 law=5 行为。测试后恢复 mat。
%  T3 law=7 全闭环 S01 试跑（真实权重）：fuel/SOC_end/Vdc_min 合理，
%     SOC_end ∈ [0.55, 0.65]（验收判据），Vdc_min ≥ 600 V（CCS 瞬态带下限）。
%
%  新仿真自动重置：ecms2.nn_predict_ehat 以 t<lastT 识别新仿真并重置缓冲/权重缓存，
%  故 T1→T2（mat 缺失回退）→T3（mat 恢复重载）无需 clear 函数（R2026a 下
%  clear <pkg>.<func> 不清除包函数 persistent，已实证）。
%
%  前置: 10_build_modelD.m 已重建模型（含 law=7 extrinsic 变体）；
%        52_integrate_law7.m 已生成 config/ecms_nn_weights.mat（真实权重）。
%  用法: r = ecms2.test_law7();   % ROOT 缺省 model/simulink_modelD
%  失败: error('ECMS2:TEST', ...) 非零退出；成功时写入
%        results/ecms_phase2/test_law7.json 并返回 r（r.pass=true）。
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    fprintf('\n[ecms2.test_law7] ===== T-204 law=7 NN-A-ECMS 单元测试（Amendment v1.2 extrinsic 兜底） =====\n');

    r = struct('t1_max_delta_y_norm', NaN, 't2_max_diff_kW', NaN, ...
        't3_fuel_kg', NaN, 't3_SOC_end', NaN, 't3_Vdc_min_V', NaN, ...
        't3_duration_s', NaN, 'pass', false);
    t = struct('t1', 'nn_predict_ehat vs Python（100 组随机输入，归一化空间 |Δ|<1e-5）', ...
               't2', '回退路径：权重 mat 缺失 -> ê=0（块 ≡ 离线 law=6-ehat=0，容差 0）', ...
               't3', 'law=7 全闭环 S01 试跑（SOC_end∈[0.55,0.65]，Vdc_min≥600V）');

    %% ---- 公共准备：参数 + 权重 ----
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);
    sNN = ecms2.ensure_nn_params(ROOT);

    matFile = fullfile(ROOT, 'config', 'ecms_nn_weights.mat');
    assert(exist(matFile, 'file') == 2, 'ECMS2:TEST', ...
        'T1 SKIP->FAIL: %s 不存在（先运行 52_integrate_law7.m）', matFile);
    Wm = load(matFile);

    MDL = 'HybridShip_ModelD_DCBus';
    slxPath = fullfile(ROOT, 'model', [MDL '.slx']);
    assert(exist(slxPath, 'file') == 2, 'ECMS2:TEST', ...
        'T1 SKIP->FAIL: %s 不存在（先运行 10_build_modelD.m）', slxPath);
    if ~bdIsLoaded(MDL)
        load_system(slxPath);
    end
    % 确认 law=7 变体已物化
    v7 = find_system([MDL '/05_EMS_Controller'], 'SearchDepth', 1, ...
        'MatchFilter', @Simulink.match.allVariants, 'Name', 'EMS_NN_AECMS');
    assert(~isempty(v7), 'ECMS2:TEST', ...
        'T1 FAIL: 模型缺少 EMS_NN_AECMS 变体（模型未含 law=7，请重建）');

    %% ================= T1: nn_predict_ehat vs Python =================
    fprintf('\n[ecms2.test_law7] ---- T1: ecms2.nn_predict_ehat vs Python（100 组随机输入） ----\n');
    N = 100;
    rng(204, 'twister');
    P_hist = 100 + 800 * rand(300, N);   % 每列一个测试点的 300 s P 历史（时间升序喂入）
    v_test = 0.1 + 5.9 * rand(1, N);     % 测试点船速 v(t) [m/s]
    a_test = -0.3 + 0.6 * rand(1, N);    % 测试点 accel(t) = v(t)-v(t-1) [m/s^2]
    x_raw = [P_hist; v_test; a_test];    % 302×N（fleet_norm.json feature_layout 同序）

    % -- 直接驱动函数（含持久环形缓冲；t 单调递增，捕获每测试点第 300 样本输出）--
    e_mat = zeros(1, N);
    tt = 0;
    for j = 1:N
        for s = 1:300
            tt = tt + 1;
            if s < 300
                vv = v_test(j) - a_test(j);   % 使第 300 样本处 accel = v(t)-v(t-1) = a_test(j)
            else
                vv = v_test(j);
            end
            eh = ecms2.nn_predict_ehat(tt, P_hist(s, j), vv);
            if s == 300
                e_mat(j) = eh;
            end
        end
    end

    % -- Python 参考（.venv_tf，同款 _np_mlp_forward 数学）--
    tmpDir = tempname; mkdir(tmpDir);
    try
        xbin = fullfile(tmpDir, 'x_raw.bin');
        fid = fopen(xbin, 'w'); fwrite(fid, x_raw, 'double'); fclose(fid);
        pyScript = fullfile(tmpDir, 'law7_infer.py');
        fid = fopen(pyScript, 'w', 'n', 'UTF-8');
        fprintf(fid, '%s\n', ...
            'import json, sys, numpy as np', ...
            'wj = json.load(open(sys.argv[1], encoding="utf-8"))', ...
            'x = np.fromfile(sys.argv[2], dtype=np.float64).reshape(302, -1, order="F")', ...
            'W1 = np.asarray(wj["weights"]["W1"], dtype=np.float64)', ...
            'b1 = np.asarray(wj["weights"]["b1"], dtype=np.float64)', ...
            'W2 = np.asarray(wj["weights"]["W2"], dtype=np.float64)', ...
            'b2 = np.asarray(wj["weights"]["b2"], dtype=np.float64)', ...
            'W3 = np.asarray(wj["weights"]["W3"], dtype=np.float64)', ...
            'b3 = np.asarray(wj["weights"]["b3"], dtype=np.float64)', ...
            'Xm = np.asarray(wj["norm"]["X_mean"], dtype=np.float64).reshape(-1, 1)', ...
            'Xs = np.asarray(wj["norm"]["X_std"], dtype=np.float64).reshape(-1, 1)', ...
            'xn = (x - Xm) / Xs', ...
            'h1 = np.maximum(xn.T @ W1 + b1, 0.0)', ...
            'h2 = np.maximum(h1 @ W2 + b2, 0.0)', ...
            'y = (h2 @ W3 + b3).ravel()', ...
            'y.tofile(sys.argv[3])', ...
            'print("n=%d max|y|=%g" % (y.size, float(np.max(np.abs(y)))), flush=True)');
        fclose(fid);
        venvBase = fullfile(fileparts(ROOT), 'simulink', '.venv_tf');
        venvPy = '';
        candW = fullfile(venvBase, 'Scripts', 'python');   % Windows layout
        candU = fullfile(venvBase, 'bin', 'python');       % Unix layout
        if exist(candW, 'file') == 2
            venvPy = candW;
        elseif exist(candU, 'file') == 2
            venvPy = candU;
        end
        assert(~isempty(venvPy), 'ECMS2:TEST', 'T1 FAIL: .venv_tf python not found under %s', venvBase);
        ybin = fullfile(tmpDir, 'y_py.bin');
        wJson = fullfile(ROOT, 'results', 'ecms_phase2', 'models', 'selected_model_weights.json');
        cmd = sprintf('"%s" "%s" "%s" "%s" "%s"', venvPy, pyScript, wJson, xbin, ybin);
        [st, so] = system(cmd);
        if st ~= 0
            error('ECMS2:TEST', 'T1 FAIL: Python 推理退出码 %d\n%s', st, so);
        end
        fprintf('[T1] %s', strtrim(so));
        fid = fopen(ybin, 'r'); y_py = fread(fid, 'double'); fclose(fid);
        assert(numel(y_py) == N, 'ECMS2:TEST', 'T1 FAIL: Python 输出 %d 点（预期 %d）', numel(y_py), N);
    catch err
        rmdir(tmpDir, 's');
        rethrow(err);
    end
    rmdir(tmpDir, 's');
    % -- 比对（归一化空间）：函数输出 e_hat -> 反推 y_norm，与 Python y_norm 对比 --
    y_blk = (e_mat .* Wm.ECMS_NN_Escale_kWh + Wm.ECMS_NN_Ebar_H_kWh - Wm.ECMS_NN_YMean) ./ Wm.ECMS_NN_YStd;
    maxDelta = max(abs(y_blk(:) - y_py(:)));
    fprintf('[T1] 比对 %d 组随机输入：函数 vs Python max|Δ| = %.6g（阈值 1e-5，归一化空间）\n', ...
        N, maxDelta);
    assert(maxDelta < 1e-5, 'ECMS2:TEST', ...
        'T1 FAIL: 函数 vs Python 最大偏差 %.6g ≥ 1e-5', maxDelta);
    r.t1_max_delta_y_norm = maxDelta;

    %% ================= T2: 回退路径（权重 mat 缺失 -> ê=0） =================
    fprintf('\n[ecms2.test_law7] ---- T2: 回退路径（mat 缺失 -> ê=0） ----\n');
    matBak = [matFile '.bak_t204'];
    if exist(matBak, 'file'), delete(matBak); end
    movefile(matFile, matBak);
    % 内联版（2026-08-26）：权重/归一化常数经变体内 Constant 引用 base 的 ECMS_NN_*
    % 显式信号传入，回退门控为 ECMS_NN_WeightsOK；仅 rename mat 不会撤回已注入的
    % 权重信号。必须同步重跑 ensure_nn_params（此时 mat 缺失 -> WeightsOK=0），
    % 回退路径（FR-208， ê=0 ≡law5）才真正生效。
    sFb = ecms2.ensure_nn_params(ROOT);
    assert(sFb.ok == 0, 'ECMS2:TEST', 'T2 FAIL: 权重 mat 已移除但 WeightsOK 未归零');
    try
        % 新仿真 t=0 < lastT（T1 结束于 t=30000）-> 函数重置并重试 load（缺失 -> ê=0）
        set_param(MDL, 'SimulationCommand', 'update');
        SCN = evalin('base', 'SCENARIOS(1)');
        SCN.voyage_distance_m = 2000;
        stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
        out = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
        ds = model.extract_logsout(out);
        assert(isfield(ds, 'P_gen_ref_kW'), 'ECMS2:TEST', ...
            'T2 FAIL: logsout 缺少 P_gen_ref_kW（law=7 记录未生效）');
        ti = ds.t;
        idx = find(abs(ti - round(ti)) < 1e-6);
        P_load_s = ds.P_load_total_kW(idx);
        SOC_s    = ds.SOC(idx);
        P_ref_blk = ds.P_gen_ref_kW(idx);
        lamT2 = evalin('base', 'ECMS_Lambda');
        pT2 = ecms2.ecms_params();
        maxDiff2 = 0;
        nCmp2 = numel(idx);
        for k = 1:nCmp2
            Pg_off = ecms2.ecms_law(P_load_s(k), SOC_s(k), lamT2, pT2, 6, 0.0); % ê=0
            d = abs(Pg_off - P_ref_blk(k));
            if d > maxDiff2, maxDiff2 = d; end
        end
        fprintf('[T2] 比对 %d 个整数秒采样点：块(law=7, ê=0) vs 离线 law=6-ehat=0 max|Δ| = %.6g kW（容差 0）\n', ...
            nCmp2, maxDiff2);
        fprintf('[T2] 语义说明：ê=0 -> SOC_ref_eff=SOC_ref（预测通道惰性 ≡ law=5）；λ floor 按 law=6 v2 仍生效\n');
        assert(maxDiff2 == 0, 'ECMS2:TEST', ...
            'T2 FAIL: 回退路径块与离线实现最大偏差 %.6g kW ≠ 0', maxDiff2);
        r.t2_max_diff_kW = maxDiff2;
    catch err
        movefile(matBak, matFile);   % 异常路径也恢复 mat
        rethrow(err);
    end
    movefile(matBak, matFile);
    fprintf('[T2] 已恢复 %s\n', matFile);

    %% ================= T3: law=7 全闭环 S01 试跑（真实权重） =================
    fprintf('\n[ecms2.test_law7] ---- T3: law=7 全闭环 S01 试跑（真实权重） ----\n');
    sNN = ecms2.ensure_nn_params(ROOT);   % 重新装载真实权重（WeightsOK=1；extrinsic 经 t<lastT 重置后重载 mat）
    assert(sNN.ok == 1, 'ECMS2:TEST', 'T3 FAIL: 权重重新装载失败');
    set_param(MDL, 'SimulationCommand', 'update');
    SCN = evalin('base', 'SCENARIOS(1)');          % 完整 S01（5000 m，initial_SOC=0.60）
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    tic
    out = model.run_scenario_sim(MDL, SCN, 7, stopTimeLimit);
    wall = toc;
    ds = model.extract_logsout(out);
    fuel = ds.fuel_total_kg(end);
    socEnd = ds.SOC(end);
    vdcMin = min(ds.V_dc_V);
    fprintf('[T3] law=7 S01: fuel=%.2f kg | SOC_end=%.4f | Vdc_min=%.1f V | duration=%.0f s | wall=%.1f s\n', ...
        fuel, socEnd, vdcMin, ds.t(end), wall);
    assert(isfinite(fuel) && fuel > 0, 'ECMS2:TEST', 'T3 FAIL: fuel 不合理（%.3g）', fuel);
    assert(isfinite(socEnd) && socEnd >= 0.55 && socEnd <= 0.65, 'ECMS2:TEST', ...
        'T3 FAIL: SOC_end=%.4f 不在 [0.55, 0.65] 带内', socEnd);
    assert(isfinite(vdcMin) && vdcMin >= 600, 'ECMS2:TEST', ...
        'T3 FAIL: Vdc_min=%.1f V < 600 V（CCS 瞬态带下限）', vdcMin);
    r.t3_fuel_kg = fuel;
    r.t3_SOC_end = socEnd;
    r.t3_Vdc_min_V = vdcMin;
    r.t3_duration_s = ds.t(end);

    %% ---- 汇总 ----
    r.pass = true;
    results = r;
    fprintf('\n[ecms2.test_law7] 全部通过：\n');
    fprintf('  [PASS] %-52s -> max|Δ| = %.6g\n', t.t1, r.t1_max_delta_y_norm);
    fprintf('  [PASS] %-52s -> max|Δ| = %.6g kW\n', t.t2, r.t2_max_diff_kW);
    fprintf('  [PASS] %-52s -> fuel=%.2f kg SOC_end=%.4f Vdc_min=%.1f V\n', t.t3, ...
        r.t3_fuel_kg, r.t3_SOC_end, r.t3_Vdc_min_V);
    outDir = fullfile(ROOT, 'results', 'ecms_phase2');
    if ~exist(outDir, 'dir'), mkdir(outDir); end
    model.write_json(fullfile(outDir, 'test_law7.json'), ...
        struct('t1_max_delta_y_norm', r.t1_max_delta_y_norm, ...
               't2_max_diff_kW_law7_fallback', r.t2_max_diff_kW, ...
               't3_fuel_total_kg', r.t3_fuel_kg, 't3_SOC_end', r.t3_SOC_end, ...
               't3_Vdc_min_V', r.t3_Vdc_min_V, 't3_duration_s', r.t3_duration_s, ...
               'pass', 1));
    fprintf('[ecms2.test_law7] 结果已写入 %s\n', fullfile(outDir, 'test_law7.json'));
end

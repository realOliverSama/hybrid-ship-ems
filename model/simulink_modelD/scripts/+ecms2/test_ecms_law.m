function results = test_ecms_law(ROOT)
% ecms2.test_ecms_law — T-103/T-105 单元测试（design §3.5，FR-108）
%
%  七项：
%   1. 恒定需求 700 kW 稳态：P_g* 与离线独立穷举最优点一致（<2%）
%   2. λ→大（5.0）：电池保护，P_g* = 满功率上限（1000 kW）
%   3. λ=0：纯瞬时燃油最优（P_g*=0 怠速）；SOC 硬约束下禁放电取最小满足点
%   4. 符号约定：放电正代价 / 充电负收益（ecms_cost_parts 单测 + 独立公式核对）
%   5. Simulink 块（EMS_CTRL_LAW=4）与离线 ecms_law 同输入同输出（容差 0，
%      固定整数秒采样点全比对）
%   6. law=5 SOC-Adaptive ECMS（离线）：SOC>ref 时 λ 下降（电池更便宜）、
%      SOC<ref 时 λ 上升；与独立 λ_eff 穷举一致；K_soc=0 退化回 law=4
%   7. Simulink 块（EMS_CTRL_LAW=5）与离线 ecms_law law=5 同输入同输出（容差 0）
%
%  前置: 10_build_modelD.m 已重建模型（含 law=4/5 变体）。
%  用法: r = ecms2.test_ecms_law();          % ROOT 缺省 model/simulink_modelD
%  失败: error('ECMS2:TEST', ...) 非零退出；成功时写入
%        results/ecms_phase1/test_ecms_law.json 并返回 r（r.pass=true）。
    if nargin < 1 || isempty(ROOT)
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    fprintf('\n[ecms2.test_ecms_law] ===== T-103/105 ECMS 单元测试（design §3.5） =====\n');

    p = ecms2.ecms_params();
    r = struct('t1', NaN, 't2', NaN, 't3a', NaN, 't3b', NaN, 't4', NaN, 't5', NaN, 't6', NaN, 't7', NaN);
    t = struct('t1', '恒定700kW稳态 vs 独立穷举(<2%)', ...
               't2', 'λ=5.0 -> 满功率上限(电池保护)', ...
               't3a', 'λ=0 -> 纯瞬时燃油最优(怠速0kW)', ...
               't3b', 'SOC≤0.105 禁放电 -> 最小满足点', ...
               't4', '符号约定：放电正代价/充电负收益', ...
               't5', 'Simulink块(law=4) vs 离线实现（容差0）', ...
               't6', 'law=5 自适应：SOC>ref λ降 / SOC<ref λ升 + 穷举一致', ...
               't7', 'Simulink块(law=5) vs 离线实现（容差0）');

    %% ---- Test 1: 恒定需求稳态，与独立穷举一致（<2%）----
    P_load = 700; SOC = 0.60; lam = p.lambda;
    Pg1 = ecms2.ecms_law(P_load, SOC, lam, p);
    [Pg_ref, Jref] = refArgmin(P_load, SOC, lam, p);
    rel = abs(Pg1 - Pg_ref) / max(Pg_ref, 1);
    fprintf('[T1] P_load=700: ecms_law Pg*=%g kW | 独立穷举 Pg*=%g kW (J=%.6g) | rel=%.4g\n', ...
        Pg1, Pg_ref, Jref, rel);
    assert(rel < 0.02, 'ECMS2:TEST', ...
        'T1 FAIL: ecms_law Pg*=%.0f 与独立穷举 %.0f 偏差 %.3g%% > 2%%', Pg1, Pg_ref, 100 * rel);
    r.t1 = Pg1;

    %% ---- Test 2: λ→大 -> 满功率上限（电池保护）----
    Pg2 = ecms2.ecms_law(700, 0.60, 5.0, p);
    fprintf('[T2] λ=5.0: Pg*=%g kW（上限 %g）\n', Pg2, p.P_max_kW);
    assert(Pg2 == p.P_max_kW, 'ECMS2:TEST', ...
        'T2 FAIL: λ=5.0 应取满功率 %g，实际 %.0f', p.P_max_kW, Pg2);
    r.t2 = Pg2;

    %% ---- Test 3: λ=0 纯瞬时燃油最优 + SOC 硬约束禁放电 ----
    Pg3a = ecms2.ecms_law(700, 0.60, 0.0, p);
    fprintf('[T3a] λ=0: Pg*=%g kW（预期 0 怠速）\n', Pg3a);
    assert(Pg3a == 0, 'ECMS2:TEST', 'T3a FAIL: λ=0 应取怠速 0 kW，实际 %.0f', Pg3a);
    r.t3a = Pg3a;
    % SOC 硬约束：SOC=0.10 <= 0.105 禁放电 -> 最小满足 P_g·η_rect ≥ P_load 的网格点
    exp3b = p.grid_kW(find(p.grid_kW * p.eta_rect >= P_load - 1e-9, 1, 'first'));
    Pg3b = ecms2.ecms_law(700, 0.10, 0.0, p);
    fprintf('[T3b] SOC=0.10（≤%.3g）: Pg*=%g kW（预期最小满足点 %g）\n', ...
        p.SOC_dis_max, Pg3b, exp3b);
    assert(Pg3b == exp3b, 'ECMS2:TEST', ...
        'T3b FAIL: 禁放电应取 %g，实际 %.0f', exp3b, Pg3b);
    r.t3b = Pg3b;

    %% ---- Test 4: 符号约定（放电正代价 / 充电负收益）----
    [mf4, mbe4] = ecms2.ecms_cost_parts(0, 700, p);        % P_bat=700>0 放电
    exp4dis = 700 / (p.eta_dcdc * p.eta_dis) / 3600;
    assert(mbe4 > 0 && abs(mbe4 - exp4dis) < 1e-12, 'ECMS2:TEST', ...
        'T4 FAIL: 放电 mbe=%.9f 应为正且 = %.9f', mbe4, exp4dis);
    [~, mbe4b] = ecms2.ecms_cost_parts(1000, 700, p);      % P_bat=-270<0 充电
    exp4chg = (700 - p.eta_rect * 1000) * (p.eta_dcdc * p.eta_chg) / 3600;
    assert(mbe4b < 0 && abs(mbe4b - exp4chg) < 1e-12, 'ECMS2:TEST', ...
        'T4 FAIL: 充电 mbe=%.9f 应为负且 = %.9f', mbe4b, exp4chg);
    % 方向性：放电代价随 λ 增大；充电收益（负项）随 λ 增大而更负
    assert((mf4 + 5.0 * mbe4) > (mf4 + p.lambda * mbe4), 'ECMS2:TEST', ...
        'T4 FAIL: 放电代价未随 λ 增大');
    assert((mf4 + 5.0 * mbe4b) < (mf4 + p.lambda * mbe4b), 'ECMS2:TEST', ...
        'T4 FAIL: 充电收益未随 λ 增大');
    fprintf('[T4] 放电 mbe=%+.6g kg/s（正代价）| 充电 mbe=%+.6g kg/s（负收益）OK\n', mbe4, mbe4b);
    r.t4 = mbe4;

    %% ---- Test 6: law=5 SOC-Adaptive ECMS（离线，design §3.2）----
    lam6 = p.lambda;   % λ₀（当前 ECMS_Lambda，T-104 标定值）
    % 6a: 自适应方向——SOC 高于 ref 时 λ_eff 下降（电池更便宜），低于 ref 时上升
    le_hi = lam6 + p.k_soc * (p.soc_ref - 0.70);
    le_lo = lam6 + p.k_soc * (p.soc_ref - 0.50);
    fprintf('[T6a] λ₀=%.4f K_soc=%.2f SOC_ref=%.2f: λ_eff(SOC=0.70)=%.4f λ_eff(SOC=0.50)=%.4f\n', ...
        lam6, p.k_soc, p.soc_ref, le_hi, le_lo);
    assert(le_hi < lam6 - 1e-12 && le_lo > lam6 + 1e-12, 'ECMS2:TEST', ...
        'T6a FAIL: 自适应方向错误（SOC>ref 应 λ 降、SOC<ref 应 λ 升）');
    % 6b: 与独立 λ_eff 穷举一致（<2%，同 T1 容差口径）
    Pg6b = ecms2.ecms_law(700, 0.70, lam6, p, 5);
    [Pg6r, J6r] = refArgmin(700, 0.70, le_hi, p);
    rel6 = abs(Pg6b - Pg6r) / max(Pg6r, 1);
    fprintf('[T6b] SOC=0.70: law5 Pg*=%g | 独立 λ_eff 穷举 Pg*=%g (J=%.6g) | rel=%.4g\n', ...
        Pg6b, Pg6r, J6r, rel6);
    assert(rel6 < 0.02, 'ECMS2:TEST', ...
        'T6b FAIL: law=5 Pg*=%.0f 与独立 λ_eff 穷举 %.0f 偏差 %.3g%% > 2%%', ...
        Pg6b, Pg6r, 100 * rel6);
    % 6c: SOC 高于 ref（电池更便宜）-> 同工况下 law5 Pg* ≤ law4 Pg*；K_soc=0 退化回 law4
    Pg6c4 = ecms2.ecms_law(700, 0.70, lam6, p, 4);
    Pg6c5 = ecms2.ecms_law(700, 0.70, lam6, p, 5);
    fprintf('[T6c] SOC=0.70: law4 Pg*=%g law5 Pg*=%g（低λ→更倾向放电→Pg 不升）\n', Pg6c4, Pg6c5);
    assert(Pg6c5 <= Pg6c4 + 1e-9, 'ECMS2:TEST', ...
        'T6c FAIL: 电池更便宜时 Pg* 不应上升（law5=%g > law4=%g）', Pg6c5, Pg6c4);
    p0 = p; p0.k_soc = 0;
    Pg6d = ecms2.ecms_law(700, 0.70, lam6, p0, 5);
    assert(Pg6d == Pg6c4, 'ECMS2:TEST', 'T6c FAIL: K_soc=0 应退化回 law=4（%g≠%g）', Pg6d, Pg6c4);
    r.t6 = Pg6b;

    %% ---- Test 5: Simulink 块（law=4） vs 离线实现（容差 0）----
    MDL = 'HybridShip_ModelD_DCBus';
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
    evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));
    ecms2.ensure_bsfc_params(ROOT);
    slxPath = fullfile(ROOT, 'model', [MDL '.slx']);
    assert(exist(slxPath, 'file') == 2, 'ECMS2:TEST', ...
        'T5 SKIP->FAIL: %s 不存在（先运行 10_build_modelD.m）', slxPath);
    if ~bdIsLoaded(MDL)
        load_system(slxPath);
    end
    % 确认 law=4 变体已物化
    v4 = find_system([MDL '/05_EMS_Controller'], 'SearchDepth', 1, ...
        'MatchFilter', @Simulink.match.allVariants, 'Name', 'EMS_Fixed_ECMS');
    assert(~isempty(v4), 'ECMS2:TEST', ...
        'T5 FAIL: 模型缺少 EMS_Fixed_ECMS 变体（模型未含 law=4，请重建）');
    % 短 S01 运行（航程 2000 m，约 400 s，够多控制周期）
    SCN = evalin('base', 'SCENARIOS(1)');
    SCN.voyage_distance_m = 2000;
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    out = model.run_scenario_sim(MDL, SCN, 4, stopTimeLimit);
    ds = model.extract_logsout(out);
    assert(isfield(ds, 'P_gen_ref_kW'), 'ECMS2:TEST', ...
        'T5 FAIL: logsout 缺少 P_gen_ref_kW（law=4 记录未生效）');
    % 整数秒采样点（块每 ECMS_dt_ctrl_s=1 s 更新一次，输出保持）
    ti = ds.t;
    idx = find(abs(ti - round(ti)) < 1e-6);
    P_load_s = ds.P_load_total_kW(idx);
    SOC_s    = ds.SOC(idx);
    P_ref_blk = ds.P_gen_ref_kW(idx);
    lam5 = evalin('base', 'ECMS_Lambda');
    p5 = ecms2.ecms_params();
    maxDiff = 0;
    nCmp = numel(idx);
    for k = 1:nCmp
        Pg_off = ecms2.ecms_law(P_load_s(k), SOC_s(k), lam5, p5);
        d = abs(Pg_off - P_ref_blk(k));
        if d > maxDiff
            maxDiff = d;
        end
    end
    fprintf('[T5] 比对 %d 个整数秒采样点：块 vs 离线 max|Δ| = %.6g kW（容差 0）\n', ...
        nCmp, maxDiff);
    assert(maxDiff == 0, 'ECMS2:TEST', ...
        'T5 FAIL: 块与离线实现最大偏差 %.6g kW ≠ 0', maxDiff);
    r.t5 = maxDiff;

    %% ---- Test 7: Simulink 块（law=5） vs 离线实现（容差 0）----
    v5 = find_system([MDL '/05_EMS_Controller'], 'SearchDepth', 1, ...
        'MatchFilter', @Simulink.match.allVariants, 'Name', 'EMS_SOC_Adaptive_ECMS');
    assert(~isempty(v5), 'ECMS2:TEST', ...
        'T7 FAIL: 模型缺少 EMS_SOC_Adaptive_ECMS 变体（模型未含 law=5，请重建）');
    SCN.voyage_distance_m = 2000;
    stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1800;
    out = model.run_scenario_sim(MDL, SCN, 5, stopTimeLimit);
    ds = model.extract_logsout(out);
    assert(isfield(ds, 'P_gen_ref_kW'), 'ECMS2:TEST', ...
        'T7 FAIL: logsout 缺少 P_gen_ref_kW（law=5 记录未生效）');
    ti = ds.t;
    idx = find(abs(ti - round(ti)) < 1e-6);
    P_load_s = ds.P_load_total_kW(idx);
    SOC_s    = ds.SOC(idx);
    P_ref_blk = ds.P_gen_ref_kW(idx);
    lam7 = evalin('base', 'ECMS_Lambda');
    p7 = ecms2.ecms_params();
    maxDiff7 = 0;
    nCmp7 = numel(idx);
    for k = 1:nCmp7
        Pg_off = ecms2.ecms_law(P_load_s(k), SOC_s(k), lam7, p7, 5);
        d = abs(Pg_off - P_ref_blk(k));
        if d > maxDiff7
            maxDiff7 = d;
        end
    end
    fprintf('[T7] 比对 %d 个整数秒采样点：块(law=5) vs 离线 max|Δ| = %.6g kW（容差 0）\n', ...
        nCmp7, maxDiff7);
    assert(maxDiff7 == 0, 'ECMS2:TEST', ...
        'T7 FAIL: law=5 块与离线实现最大偏差 %.6g kW ≠ 0', maxDiff7);
    r.t7 = maxDiff7;

    %% ---- 汇总 ----
    r.pass = true;
    results = r;
    fprintf('\n[ecms2.test_ecms_law] 全部通过：\n');
    for fn = {'t1', 't2', 't3a', 't3b', 't4', 't5', 't6', 't7'}
        fprintf('  [PASS] %-40s -> %s\n', t.(fn{1}), mat2str(r.(fn{1})));
    end
    % 机器可读结果
    outDir = fullfile(ROOT, 'results', 'ecms_phase1');
    if ~exist(outDir, 'dir'), mkdir(outDir); end
    model.write_json(fullfile(outDir, 'test_ecms_law.json'), ...
        struct('t1_Pg_star_kW', r.t1, 't2_Pg_star_kW', r.t2, ...
               't3a_Pg_star_kW', r.t3a, 't3b_Pg_star_kW', r.t3b, ...
               't4_mbe_dis_kgps', r.t4, 't5_max_diff_kW', r.t5, ...
               't6_Pg_star_law5_kW', r.t6, 't7_max_diff_kW_law5', r.t7, 'pass', 1));
    fprintf('[ecms2.test_ecms_law] 结果已写入 %s\n', ...
        fullfile(outDir, 'test_ecms_law.json'));
end

%% ==================== 独立参考实现（不复用 ecms_law）====================
function [Pg_ref, Jref] = refArgmin(P_load, SOC, lam, p)
% 独立穷举：公式按 design §3.1 独立书写（interp1 路径），用于交叉验证。
    grid = p.grid_kW;
    J = nan(size(grid));
    for i = 1:numel(grid)
        Pg = grid(i);
        Pbat = P_load - p.eta_rect * Pg;
        if SOC <= p.SOC_dis_max && Pbat > 1e-9, continue; end
        if SOC >= p.SOC_chg_min && Pbat < 0, continue; end
        if Pg < 1e-9
            mf = p.idle_fuel_kgph / 3600;
        else
            mf = Pg * interp1(p.bsfc_bp_kW, p.bsfc_tbl_gkWh, Pg, 'linear') / 3.6e6;
        end
        if Pbat > 0
            mbe = Pbat / (p.eta_dcdc * p.eta_dis) / 3600;
        else
            mbe = Pbat * (p.eta_dcdc * p.eta_chg) / 3600;
        end
        J(i) = mf + lam * mbe;
    end
    [Jref, i] = min(J);
    Pg_ref = grid(i);
end

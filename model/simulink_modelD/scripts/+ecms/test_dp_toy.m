function res = test_dp_toy()
% ecms.test_dp_toy — DP 玩具问题单测（T-008，FR-008/FR-009，design §4）
%
%  三项单测：
%    T1 符号约定（design §4.1，施工前写死）：P_bat=0 -> E 不变；放电必减、
%       充电必增（含数值精确公式校验）。
%    T2 手工可解问题（连续口径）：恒定需求 776 kW × 10 s、电池功率限 ~0、
%       无爬坡约束、P_min=0。功率平衡强制 P_g ≡ 776/η_rect = 800 kW（恰为
%       聚合一维 BSFC 最优点 L=0.8 锚点，在 20 kW 网格上），理论燃油 =
%       10 × 800 × 427/3.6e6。DP 解与理论值偏差 < 1%，且 P_bat ≈ 0、SOC 恒定。
%       说明：需求取 776 kW 而非 400 kW 是为使 P_d/η_rect = 800 恰落在控制网格
%       与 BSFC 锚点（若取 400 kW，P_g=412.37 不在网格上，且 ±0.005 终端带允许
%       电池借能 5.36 kWh，理论最优将退化为“怠速+放电”，不再手算可解——
%       故以电池功率限 ~0 锁定“唯一可行=理论最优”）。
%    T3 启停玩具：需求 776 kW×5 s 后 0×5 s、电池限 ~0、P_min=0、C_start=2 kg。
%       连续口径 = 5×f(800)+5×怠速；启停口径 = C_start+5×f(800)（首次启动付
%       启动代价、零需求段停机而非怠速）。验证 DP 正确选择停机 vs 怠速
%       （若怠速将多付 5×怠速≈0.0094 kg），on 轨迹 [1,1,1,1,1,0,0,0,0,0]。
%
%  失败语义：任一单测不满足 -> error('ECMS:TOY', ...)（非零退出）。
%  输出: res struct（tests 逐项 + passed）。
%
%  执行：cd model/simulink_modelD && matlab -batch "addpath('scripts','config'); r=ecms.test_dp_toy()"
    fprintf('\n[ECMS-TOY] ===== DP 玩具问题单测（T-008） =====\n');
    res = struct('tests', struct([]), 'passed', false);

    % ---- 通用：配置缺省 + 覆盖 ----
    cfgBase = Ecms_Phase0_Config();
    fam = ecms.bsfc_family();          % T-004 曲线族（含自检）
    assert(abs(fam.fuel_flow_idle_kgph - cfgBase.fuel_flow_idle_kgph) < 1e-6, ...
        'ECMS:TOY', '配置怠速燃油与曲线族不一致');

    tests = struct('id', {}, 'name', {}, 'ok', {}, 'detail', {});
    addT = @(id, nm, ok, dt) struct('id', id, 'name', nm, 'ok', ok, 'detail', dt);

    %% ============ T1 符号约定 ============
    dE0  = ecms.bat_energy_delta_kWh(0, 0.97, 0.95, 1);
    dEd  = ecms.bat_energy_delta_kWh(500, 0.97, 0.95, 1);
    dEc  = ecms.bat_energy_delta_kWh(-500, 0.97, 0.95, 1);
    expD = -500 / (0.97 * 0.95) / 3600;      % 放电：E −= P/(η_dcdc·η_bat)·Δt/3600
    expC = 500 * 0.97 * 0.95 / 3600;         % 充电：E += (−P)·η_dcdc·η_bat·Δt/3600
    ok1 = abs(dE0) < 1e-15 && dEd < 0 && dEc > 0 && ...
          abs(dEd - expD) < 1e-12 && abs(dEc - expC) < 1e-12;
    tests(end + 1) = addT('T1', 'sign_convention', ok1, ...
        sprintf('Pbat=0 -> dE=%.2e; discharge dE=%.6f (expect %.6f); charge dE=%.6f (expect %.6f)', ...
        dE0, dEd, expD, dEc, expC)); %#ok<AGROW>

    %% ============ T2 恒定需求 776 kW × 10 s（连续，理论最优 = 唯一可行） ============
    prof2 = struct('scenario_name', 'toy2_constant', 't', (0:9).', ...
        'P_demand_kW', 776 * ones(10, 1));
    cfg2 = Ecms_Phase0_Config(struct('mode', 'continuous', 'P_min_stable_kW', 0, ...
        'Bat_P_limit_kW', 0.5, 'R_gen_kWps', 1e6, 'C_start_kg', 2));
    prob2 = ecms.dp_problem(prof2, cfg2, fam, 2);
    sol2  = ecms.dp_solve(prob2);
    iP800 = find(abs(prob2.P_grid - 800) < 1e-9, 1);
    f800  = prob2.fuel_step_kg(iP800);
    expFuel2 = 10 * f800;                     % = 10×800×427/3.6e6（L=0.8 锚点）
    ok2 = sol2.feasible && ...
          abs(sol2.fuel_total_kg - expFuel2) / expFuel2 < 0.01 && ...
          all(abs(sol2.P_g_kW - 800) < 1e-6) && ...
          all(abs(sol2.P_bat_kW) < 1e-6) && ...
          abs(sol2.SOC(end) - 0.60) < 1e-9;
    tests(end + 1) = addT('T2', 'hand_solvable_continuous', ok2, ...
        sprintf('fuel=%.5f kg (theory %.5f, dev %.3f%%); P_g 全=800; P_bat≈0; SOC_end=%.4f', ...
        sol2.fuel_total_kg, expFuel2, 100 * abs(sol2.fuel_total_kg - expFuel2) / expFuel2, ...
        sol2.SOC(end))); %#ok<AGROW>

    %% ============ T3 启停：776×5s + 0×5s ============
    prof3 = struct('scenario_name', 'toy3_startstop', 't', (0:9).', ...
        'P_demand_kW', [776 * ones(5, 1); zeros(5, 1)]);
    cfg3c = Ecms_Phase0_Config(struct('mode', 'continuous', 'P_min_stable_kW', 0, ...
        'Bat_P_limit_kW', 0.5, 'R_gen_kWps', 1e6, 'C_start_kg', 2));
    cfg3s = Ecms_Phase0_Config(struct('mode', 'startstop', 'P_min_stable_kW', 0, ...
        'Bat_P_limit_kW', 0.5, 'R_gen_kWps', 1e6, 'C_start_kg', 2));
    prob3c = ecms.dp_problem(prof3, cfg3c, fam, 2);
    sol3c  = ecms.dp_solve(prob3c);
    prob3s = ecms.dp_problem(prof3, cfg3s, fam, 2);
    sol3s  = ecms.dp_solve(prob3s);
    idleSec = cfg3c.fuel_flow_idle_kgph / 3600;     % 怠速每秒燃油
    expCont = 5 * f800 + 5 * idleSec;               % 连续：5 s 满负荷 + 5 s 怠速
    expSs   = cfg3s.C_start_kg + 5 * f800;          % 启停：首次启动 + 5 s 满负荷 + 停机
    ok3 = sol3c.feasible && sol3s.feasible && ...
          abs(sol3c.fuel_total_kg - expCont) / expCont < 0.01 && ...
          abs(sol3s.fuel_total_kg - expSs) < 1e-6 && ...
          isequal(sol3s.on(:).', [true true true true true false false false false false]);
    % 停机 vs 怠速：on 轨迹末 5 s = 0（停机）；若怠速则 fuel = expSs + 5×怠速（差 0.0094 kg，
    % 超出 1e-6 容差），故上述断言已排除怠速选择。
    tests(end + 1) = addT('T3', 'startstop_shutdown_vs_idle', ok3, ...
        sprintf('cont=%.5f (theory %.5f); ss=%.5f (theory %.5f=C_start+5f); on=%s', ...
        sol3c.fuel_total_kg, expCont, sol3s.fuel_total_kg, expSs, ...
        mat2str(double(sol3s.on(:).')))); %#ok<AGROW>

    %% ============ 汇总 ============
    res.tests = tests;
    res.passed = all([tests.ok]);
    fprintf('[ECMS-TOY] 单测结果（%d 项）：\n', numel(tests));
    for i = 1:numel(tests)
        fprintf('  [%s] %-30s %s\n', string(tests(i).ok), tests(i).name, tests(i).detail);
    end
    if ~res.passed
        error('ECMS:TOY', '玩具单测失败：%d/%d 未通过', sum(~[tests.ok]), numel(tests));
    end
    fprintf('[ECMS-TOY] 全部通过（%d 项）。\n', numel(tests));
end

%% test_hull_shallow.m —— T-004 船体浅水单元测试（纯 MATLAB，无 Simulink 依赖，FR-412/413/415/417，NFR-411）
% 测的代码 = 块的代码：把 model.code_hull_e() 返回的字符串写入 tempdir 后直接调用（同一字符串源）。
% 运行（从工作区根）：matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_hull_shallow.m')"
% 锚点基准（V_ship=5.6584 m/s、V_current=0、Lpp=53.45、nu=1.19e-6、k=0.2，参数取自参数事实源）：
%   Δr(H/T=2.0) ≈ 0.2364 ±2%   【主 Agent 裁决精化；任务书原 0.2375 亦在容差内，勘误登记 C1】
%   Δr(H/T=1.2) ≈ 0.5646 ±2%   【主 Agent 裁决勘误：任务书原 0.5005 为手算滑差，登记 C1】
%   Δr(H/T=3.0) ≈ 0.123378 ±2% 【主 Agent 裁决 C3：严格 > 判深水，3.0 落在公式域内】
%   H/T=3.5 / 5.0 → R_shallow=0（深水渐近）；H/T=1.0 → clamp 1.2 + domain_warning=1（NFR-411）
% 勘误与裁决登记：chain_a_integration_notes.md §6。

SCR = '';
mp = mfilename('fullpath');
if ~isempty(mp)
    SCR = fileparts(fileparts(mp));   % 脚本在 scripts/tests/ 下 → 2 层 fileparts 到 scripts/
end
if isempty(SCR) || ~exist(fullfile(SCR, '+model'), 'dir')
    c1 = fullfile(pwd, 'model', 'simulink_modelE_(Phase4_Physics)', 'scripts');
    c2 = fullfile(pwd, 'scripts');
    if exist(fullfile(c1, '+model'), 'dir')
        SCR = c1;                      % pwd 兜底：从工作区根执行（NFR-406，禁绝对路径）
    elseif exist(fullfile(c2, '+model'), 'dir')
        SCR = c2;
    else
        error('TEST_HULL:PATH', '无法定位 scripts/（mfilename 与 pwd 兜底均失败）');
    end
end
MW = fileparts(SCR);                   % Model E 工作区根（config/ 的父目录）
addpath(SCR);
run(fullfile(MW, 'config', 'ModelE_Physics_Parameters.m'));   % 参数唯一事实源（OQ-8）

%% 把块源码写入 tempdir 并调用（保证测的代码 = block 的代码）
td = tempdir;
fHull = fullfile(td, 'ShipHullDynamicsE.m');
fid = fopen(fHull, 'w');
fprintf(fid, '%s', model.code_hull_e());
fclose(fid);
addpath(td);

failMsg = '';
try
    V  = 5.6584;  Vc = 0;  Fp = 0;  Fe = 0;  rm = 1.0;
    args = {Hull_K_R, Ship_Mass, Hull_Lpp_m, Hull_T_m, nu_water, Hull_k_form, ...
            Shallow_c1, Shallow_c2, Shallow_c3, Shallow_HT_deep_limit, Shallow_HT_min_valid};
    Rdeep_ref = rm * Hull_K_R * V^2;   % 深水阻力参照（V_rel=V、Vc=0）

    %% ---- 锚点 1：H/T=2.0（water_depth=4.376 m）→ Δr ≈ 0.2364 ±2%（勘误 C1）----
    wd = 2.0 * Hull_T_m;
    [dVdt, Rtot, Rsh, HTo, warn] = ShipHullDynamicsE(V, Fp, Vc, Fe, rm, wd, args{:});
    assert(abs(HTo - 2.0) < 1e-12 && warn == 0, 'H/T=2.0: HT_ratio/warn 异常');
    assert(abs((Rtot - Rsh) - Rdeep_ref) <= 1e-9 * Rdeep_ref, 'H/T=2.0: R_deep 偏离 K_R*V^2');
    ratio20 = Rsh / (Rtot - Rsh);
    assert(abs(ratio20 - 0.2364) <= 0.02 * 0.2364, ...
        'H/T=2.0 锚点失败: Rsh/Rdeep=%.6f, 期望 0.2364 ±2%%', ratio20);
    assert(all(isfinite([dVdt, Rtot, Rsh])), 'H/T=2.0 出现非有限值（NFR-411）');
    fprintf('PASS  H/T=2.0  Rsh/Rdeep=%.6f (锚 0.2364 ±2%%)  Rtot=%.3f N\n', ratio20, Rtot);

    %% ---- 锚点 2：H/T=1.2（water_depth=2.6256 m）→ Δr ≈ 0.5646 ±2%（勘误 C1）----
    wd = 1.2 * Hull_T_m;
    [dVdt, Rtot12, Rsh12, HTo, warn] = ShipHullDynamicsE(V, Fp, Vc, Fe, rm, wd, args{:});
    assert(abs(HTo - 1.2) < 1e-12 && warn == 0, 'H/T=1.2: HT_ratio/warn 异常（域内应无报警）');
    ratio12 = Rsh12 / (Rtot12 - Rsh12);
    assert(abs(ratio12 - 0.5646) <= 0.02 * 0.5646, ...
        'H/T=1.2 锚点失败: Rsh/Rdeep=%.6f, 期望 0.5646 ±2%%（裁决 C1 勘误，任务书原 0.5005 为手算滑差）', ratio12);
    assert(all(isfinite([dVdt, Rtot12, Rsh12])), 'H/T=1.2 出现非有限值（NFR-411）');
    fprintf('PASS  H/T=1.2  Rsh/Rdeep=%.6f (锚 0.5646 ±2%%)  Rtot=%.3f N\n', ratio12, Rtot12);

    %% ---- clamp 语义：H/T=1.0（water_depth=2.188 m）→ warn=1 且结果与 H/T=1.2 档一致 ----
    wd = 1.0 * Hull_T_m;
    [dVdt, Rtot10, Rsh10, HTo, warn] = ShipHullDynamicsE(V, Fp, Vc, Fe, rm, wd, args{:});
    assert(warn == 1, 'H/T=1.0: domain_warning 应为 1（NFR-411 禁静默外推）');
    assert(HTo == 1.0, 'H/T=1.0: HT_ratio 输出应为原始比值 1.0（clamp 只作用于 Δr 计算）');
    assert(abs(Rtot10 - Rtot12) == 0 && abs(Rsh10 - Rsh12) == 0, ...
        'H/T=1.0 clamp 语义失败: R_total 应与 H/T=1.2 档逐位一致');
    assert(all(isfinite([dVdt, Rtot10, Rsh10])), 'H/T=1.0 出现非有限值（NFR-411）');
    fprintf('PASS  H/T=1.0  warn=1  Rtot=%.3f N（= H/T=1.2 档，clamp 生效）\n', Rtot10);

    %% ---- 裁决 C3：H/T=3.0 在公式域内 → Δr ≈ 0.123378 ±2%；H/T=3.5/5.0 → 深水渐近 R_shallow=0 ----
    wd = 3.0 * Hull_T_m;
    [dVdt, Rtot, Rsh, HTo, warn] = ShipHullDynamicsE(V, Fp, Vc, Fe, rm, wd, args{:});
    ratio30 = Rsh / (Rtot - Rsh);
    assert(abs(ratio30 - 0.123378) <= 0.02 * 0.123378, ...
        'H/T=3.0 锚点失败: Rsh/Rdeep=%.6f, 期望 0.123378 ±2%%（裁决 C3：严格 > 判深水）', ratio30);
    assert(warn == 0, 'H/T=3.0: 域内不应报警');
    fprintf('PASS  H/T=3.0  Rsh/Rdeep=%.6f (锚 0.123378 ±2%%，公式域内)  \n', ratio30);

    wd = 3.5 * Hull_T_m;
    [dVdt, Rtot, Rsh, ~, warn] = ShipHullDynamicsE(V, Fp, Vc, Fe, rm, wd, args{:});
    assert(Rsh == 0 && warn == 0, 'H/T=3.5: 应走深水渐近分支 R_shallow=0');
    fprintf('PASS  H/T=3.5  Rsh=0（深水渐近，裁决 C3）\n');

    wd = 5.0 * Hull_T_m;
    [dVdt, Rtot, Rsh, ~, warn] = ShipHullDynamicsE(V, Fp, Vc, Fe, rm, wd, args{:});
    assert(Rsh == 0 && warn == 0, 'H/T=5.0: 应走深水渐近分支 R_shallow=0');
    assert(abs(Rtot - Rdeep_ref) <= 1e-9 * Rdeep_ref, 'H/T=5.0: R_total 应等于 R_deep');
    fprintf('PASS  H/T=5.0  Rsh=0  Rtot=R_deep\n');

    %% ---- 零速保护（NFR-411）：V_ship=0, V_current=0 → R_total=0、R_shallow=0、无 NaN ----
    [dVdt0, Rtot0, Rsh0, HTo0, warn0] = ShipHullDynamicsE(0, 100, 0, 0, 1.0, 10.0, args{:});
    assert(Rtot0 == 0 && Rsh0 == 0, '零速: R_total/R_shallow 应为 0');
    assert(abs(dVdt0 - 100 / Ship_Mass) <= 1e-12 * max(1, abs(100 / Ship_Mass)), ...
        '零速: dVdt 应为 (F_prop_eff + F_env)/Ship_Mass');
    assert(warn0 == 0 && all(isfinite([dVdt0, Rtot0, Rsh0, HTo0])), '零速: 出现报警或非有限值');
    fprintf('PASS  零速  Rtot=0  Rsh=0  dVdt=%.3e（=(F_prop+F_env)/M）全有限\n', dVdt0);

    %% ---- 浅水单调性：固定 V=5.6584，water_depth 6.564→…→2.626 m，R_total 严格递增（rel tol 1e-6，零反例）----
    wds = [6.564 5.908 4.376 3.501 3.063 2.626];
    rts = zeros(size(wds));
    for k = 1:numel(wds)
        [~, rts(k), ~, ~, ~] = ShipHullDynamicsE(V, Fp, Vc, Fe, rm, wds(k), args{:});
    end
    assert(all(isfinite(rts)), '单调性扫描出现非有限值（NFR-411）');
    nViol = sum(diff(rts) <= 1e-6 * rts(1:end-1));
    assert(nViol == 0, '浅水单调性失败: %d 个反例, diffs=%s', nViol, mat2str(diff(rts), 5));
    fprintf('PASS  单调性  Rtot(6.564→2.626 m) 严格递增（0 反例, rel tol 1e-6）\n');

    %% ---- 逆水方向性：V_ship=5.6584 固定，V_current +1 → -1 → R_total 增大（AC-409）----
    [~, RtotP, ~, ~, ~] = ShipHullDynamicsE(V, Fp,  1, Fe, rm, 2.0 * Hull_T_m, args{:});
    [~, RtotM, ~, ~, ~] = ShipHullDynamicsE(V, Fp, -1, Fe, rm, 2.0 * Hull_T_m, args{:});
    assert(RtotM > RtotP, '逆水方向性失败: V_current=-1 应使 R_total 增大');
    assert(all(isfinite([RtotP, RtotM])), '逆水工况出现非有限值（NFR-411）');
    fprintf('PASS  逆水  Vc=+1: Rtot=%.2f N < Vc=-1: Rtot=%.2f N\n', RtotP, RtotM);

catch err
    failMsg = err.message;
end

%% 清理 tempdir 临时文件并报告
delete(fHull);
try
    rmpath(td);
catch
end
if ~isempty(failMsg)
    error('TEST_HULL:FAIL', 'test_hull_shallow FAILED: %s', failMsg);
end
fprintf('ALL PASS: test_hull_shallow\n');
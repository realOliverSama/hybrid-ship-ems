%% test_propeller_e.m —— T-005 螺旋桨重标定 + w(H/T) 信号化单元测试（纯 MATLAB，无 Simulink 依赖）
% 测的代码 = 块的代码：把 model.code_propeller_e() / model.code_wake_ht_e() 字符串写入
% tempdir 后直接调用（同一字符串源）。
% 运行（从工作区根）：matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_propeller_e.m')"
% 锚点（参数取自参数事实源 config/ModelE_Physics_Parameters.m，w_deep=0.10 重标定，D-8）：
%   设计点 n=246.7105 rpm（=1500/6.08）、V_ship=5.6584、V_current=0、w=0.10
%     → F ≈ 57011 N（±0.5%）、Q ≈ 20798 N·m（±0.5%）（设计点 η_prop·P_shaft/V 平衡复原）
%   w 信号生效（主 Agent 裁决 C2，按公式实际方向）：w 0.10→0.15 ⇒ V_A 减小、F 增大
%     【任务书原方向句为文字错误，勘误登记见 chain_a_integration_notes.md §6】
%   w(H/T) LUT：1.2→0.20、2.0→0.15、2.7→0.10、3.5→0.10（深水渐近）、1.0→0.20（clamp）

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
        error('TEST_PROP:PATH', '无法定位 scripts/（mfilename 与 pwd 兜底均失败）');
    end
end
MW = fileparts(SCR);                   % Model E 工作区根（config/ 的父目录）
addpath(SCR);
run(fullfile(MW, 'config', 'ModelE_Physics_Parameters.m'));   % 参数唯一事实源（OQ-8）

%% 把块源码写入 tempdir 并调用（保证测的代码 = block 的代码）
td = tempdir;
fProp = fullfile(td, 'PropellerHydroSimpleE.m');
fWake = fullfile(td, 'WakeVsHT.m');
fid = fopen(fProp, 'w'); fprintf(fid, '%s', model.code_propeller_e()); fclose(fid);
fid = fopen(fWake, 'w'); fprintf(fid, '%s', model.code_wake_ht_e());   fclose(fid);
addpath(td);

failMsg = '';
try
    %% ---- 设计点推力平衡锚点（w=0.10 重标定链复原，D-8）----
    n = Mot_RPM_nom / Gear_Ratio;      % = 1500/6.08 = 246.7105 rpm
    assert(abs(n - 246.7105) < 1e-3, '设计点转速锚点失败: n=%.6f', n);
    Vt = 5.6584;  Vc = 0;  w = Wake_w_deep;   % 0.10
    [F, Q, Ph, VA] = PropellerHydroSimpleE(n, Vt, Vc, w, K_Fw, K_Fv, K_Qw, K_Qv);
    assert(abs(F - 57011) <= 0.005 * 57011, ...
        '推力设计点锚点失败: F=%.3f N, 期望 57011 ±0.5%%', F);
    assert(abs(Q - 20798) <= 0.005 * 20798, ...
        '扭矩设计点锚点失败: Q=%.3f N·m, 期望 20798 ±0.5%%', Q);
    assert(all(isfinite([F, Q, Ph])) && Ph > 0, '设计点出现非有限/非物理值（NFR-411）');
    assert(abs(VA - (1 - Wake_w_deep) * Vt) <= 1e-12 * VA, 'V_A=(1-w)*V_rel 同构性失败');
    fprintf('PASS  设计点  n=%.4f rpm  F=%.3f N (锚 57011 ±0.5%%)  Q=%.3f N·m (锚 20798 ±0.5%%)  P_hydro=%.3f kW\n', ...
        n, F, Q, Ph);

    %% ---- w 信号生效（裁决 C2：公式实际方向 w↑ ⇒ V_A↓ ⇒ F↑）----
    [F15, Q15, ~, VA15] = PropellerHydroSimpleE(n, Vt, Vc, 0.15, K_Fw, K_Fv, K_Qw, K_Qv);
    assert(VA15 < VA, 'w 信号生效断言失败: w 增大应使 V_A=(1-w)*V_rel 减小');
    assert(F15 > F, 'w 信号生效断言失败: V_A 减小应使 F=K_Fw*ω|ω|-K_Fv*V_A|V_A| 增大');
    assert(Q15 > Q, 'w 信号生效断言失败: 扭矩应随 V_A 减小同向增大');
    assert(all(isfinite([F15, Q15, VA15])), 'w=0.15 工况出现非有限值（NFR-411）');
    fprintf('PASS  w 信号生效  w: 0.10→0.15  VA %.6f→%.6f (减小)  F %.1f→%.1f (增大)\n', ...
        VA, VA15, F, F15);

    %% ---- w(H/T) LUT 锚点（OQ-4/D-3，参数取自参数事实源）----
    wFun = @(ht) WakeVsHT(ht, Wake_w_deep, Wake_HT_nodes, Wake_w_Zhang, Wake_HT_ref, Wake_w_ref);
    htsL = [1.2 2.0 2.7 3.5 1.0];        % 任务书五锚点（含 3.5 深水渐近、1.0 clamp）
    wExp = [0.20 0.15 0.10 0.10 0.20];
    for k = 1:numel(htsL)
        wGot = wFun(htsL(k));
        assert(abs(wGot - wExp(k)) <= 1e-9, 'w(H/T=%.1f) 锚点失败: w=%.6f, 期望 %.2f', ...
            htsL(k), wGot, wExp(k));
        fprintf('PASS  w(H/T=%.1f) = %.6f (锚 %.2f)\n', htsL(k), wGot, wExp(k));
    end
    %% 插值抽检（tasks.md T-005 验证项：端点/插值/域外 clamp）
    %  H/T=1.3 落在节点 1.2(0.32) 与 1.4(0.26) 之间 → w_Zhang=0.29 → w = 0.10+0.29-0.22 = 0.17
    assert(abs(wFun(1.3) - 0.17) <= 1e-9, 'w(H/T) linear 插值抽检失败: w(1.3)=%.6f', wFun(1.3));
    fprintf('PASS  w(H/T=1.3) = %.6f（linear 插值抽检 0.17）\n', wFun(1.3));

catch err
    failMsg = err.message;
end

%% 清理 tempdir 临时文件并报告
delete(fProp);
delete(fWake);
try
    rmpath(td);
catch
end
if ~isempty(failMsg)
    error('TEST_PROP:FAIL', 'test_propeller_e FAILED: %s', failMsg);
end
fprintf('ALL PASS: test_propeller_e\n');
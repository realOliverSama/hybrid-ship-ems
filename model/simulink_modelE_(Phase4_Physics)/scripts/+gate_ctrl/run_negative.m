function neg = run_negative(ROOT)
% gate_ctrl.run_negative — 44 号控制层 Gate 负向验证编排主体（T-011，FR-455 判据 6 负向）
%   构造缺陷注入（42 号模式：仅 base workspace 注入，结束恢复 nominal，绝不修改文件），
%   确认 43 判据函数（C1/C2/C6/C7）对每类缺陷正确 FAIL——即真实 43 运行中此类缺陷
%   将导致 overall=false + error('GATE:*') 非零退出（Gate FAIL 禁止提交）。
    % 配置由 44 号薄入口以脚本方式载入 base（本函数作用域内 run() 不落 base）
    MDL = 'HybridShip_ModelE';
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
    T_start = evalin('base', 'T_start_hold_ticks');
    T_stop  = evalin('base', 'T_stop_hold_ticks');
    T_up    = evalin('base', 'PMS_min_up_ticks');
    SCENARIOS = evalin('base', 'SCENARIOS');
    PMS_stop_thr = evalin('base', 'PMS_stop_threshold_kW');

    neg = struct();
    neg.schema = 'gate_control_negative_v1';
    neg.generated_at = char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ'));
    neg.defects = {};
    allDetected = true;

    %% ---- 锚点正常跑（P2/S01，取合法审计行供缺陷加工） ----
    fprintf('[44] 锚点正常跑 P2/S01 ...\n');
    [dsA, ~, rowsA] = runP2S01(300);
    assert(~isempty(rowsA), '44: 锚点跑无审计行，无法构造缺陷');

    %% ---- D1：FORCED(7) reason 注入 → C2 拒收 ----
    rowsD1 = rowsA;
    rowsD1(end+1) = rowsA(end);
    rowsD1(end).reason = 7; rowsD1(end).old = 1; rowsD1(end).new = 2;
    c2 = gate_ctrl.check_c2_transition_legality(rowsD1, T_start, T_stop, T_up, 'D1_forced');
    mark('D1', 'FORCED(7) reason（force 通道绕过阈值）', ~c2.pass, 'C2 reason domain + FORCED ban');

    %% ---- D2：正常 start timer 不足 → C2 拒收 ----
    rowsD2 = rowsA;
    iStart = find(arrayfun(@(x) x.reason == 1 && x.old == 1 && x.new == 2, rowsA), 1);
    assert(~isempty(iStart), '44: 锚点无正常 start 行');
    rowsD2(iStart).t_start = 1500;   % < 3000
    c2 = gate_ctrl.check_c2_transition_legality(rowsD2, T_start, T_stop, T_up, 'D2_start_timer');
    mark('D2', '正常 start timer 不足（t_start=1500<3000）', ~c2.pass, 'C2 normal_start_timer');

    %% ---- D3：正常 stop min_up 不足 → C2 拒收 ----
    rowsD3 = rowsA;
    iStop = find(arrayfun(@(x) x.reason == 2 && x.old == 3 && x.new == 1, rowsA), 1);
    if isempty(iStop)
        rowsD3(end+1) = struct('t', 299, 'old', 3, 'new', 1, 'reason', 2, ...
            'P_commit', 300, 't_start', 0, 't_stop', 12000, 't_online', 10000, 'safety', 0);
        iStop = numel(rowsD3);
    end
    rowsD3(iStop).t_online = 10000;   % < 30000
    c2 = gate_ctrl.check_c2_transition_legality(rowsD3, T_start, T_stop, T_up, 'D3_stop_minup');
    mark('D3', '正常 stop min_up 不足（t_online=10000<30000）', ~c2.pass, 'C2 normal_stop_timer');

    %% ---- D4：屏蔽供给不足双判据（S2+S4）→ S4 场景应触发未触发 → C6 报缺失 ----
    %   仅屏蔽 S4 会被 S2 的后备覆盖（realized 缺口由 S2 兜底——安全冗余按设计生效，
    %   此时"应触发"通道仍命中，不构成可检出缺陷）；须双判据同时屏蔽才构造出
    %   "应触发未触发"缺陷。42 号模式：Parameter 通道注入，不落盘。
    fprintf('[44] D4: 屏蔽 S2+S4（hold_ticks=1e9）+ 重跑 S4 场景 ...\n');
    inj = gate_ctrl.injection_scenarios();
    s4 = inj(4);
    s4.prep_fn();
    assignin('base', 'PMS_S2_hold_ticks', 1e9);   % 缺陷注入（Parameter 通道，不落盘）
    assignin('base', 'PMS_S4_hold_ticks', 1e9);
    scn = SCENARIOS(1); scn.voyage_distance_m = s4.voyage_distance_m;
    scn.initial_SOC = s4.soc_init; scn.hotel_kW = s4.hotel_kW;
    assignin('base', 'EMS_CTRL_LAW', s4.law);
    assignin('base', 'EMS_MODE', double(s4.law > 0));
    assignin('base', 'PMS_SOURCE_MODE', s4.src);
    assignin('base', 'VOYAGE_CTRL_MODE', 0);
    assignin('base', 'SCN', scn);
    assignin('base', 'Bat_SOC_init', scn.initial_SOC);
    assignin('base', 'VOYAGE_SPEED_MODE', 0);
    assignin('base', 'VOYAGE_V_TIME_S', [0 1]);
    assignin('base', 'VOYAGE_V_VALUE_MPS', [0 0]);
    assignin('base', 'VOYAGE_CURRENT_MODE', 0);
    assignin('base', 'VOYAGE_CURRENT_TIME_S', [0 1]);
    assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', [0 0]);
    assignin('base', 'VOYAGE_WATER_DEPTH_M', 20);
    set_param(MDL, 'StopTime', sprintf('%.2f', s4.stop_time_s));
    out = sim(MDL);
    ds4 = gate_ctrl.get_ds(out);
    mm4 = gate_ctrl.metrics_of(ds4, PMS_stop_thr);
    c6 = gate_ctrl.check_c6_safety_assertions(ds4, mm4, s4.expected, 'D4_s4_suppressed');
    mark('D4', '应触发未触发 override（屏蔽 S2+S4 → S4 场景缺 force_start）', ~c6.pass, 'C6 expect_trigger');
    % 恢复 nominal（42 号纪律）——base workspace 重跑三配置文件
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));

    %% ---- D5：确定性破坏 → C1 拒收 ----
    dsD5 = dsA;
    dsD5.SOC(1234) = dsD5.SOC(1234) + 1e-6;
    c1 = gate_ctrl.check_c1_determinism(dsA, dsD5, 'D5_determinism');
    mark('D5', '确定性破坏（单采样点改写）', ~c1.pass, 'C1 bitwise');

    %% ---- D6：episode 一对一破坏（两次正常 start 仅一次饱和） → C7 拒收 ----
    rowsD6 = rowsA;
    rowsD6(end+1) = rowsA(iStart);
    rowsD6(end).t = rowsD6(end).t + 50;
    c7 = gate_ctrl.check_c7_audit_completeness(rowsD6, dsA, 'D6_episode', T_start, false);
    mark('D6', 'episode 一对一破坏（双 start 单饱和事件）', ~c7.pass, 'C7 one_to_one');

    %% ---- D7：G6/C1 尺寸不匹配 → 必须 FAIL（fail-closed 结构关闭，T-014 amendment 2）----
    %   D-B19 的机制级关闭验证：构造尺寸不匹配（A2 期空洞 PASS 的确切形态——
    %   E01b 航次终止时刻差一采样），fail-closed G6/C1 必须拒绝并整体 FAIL。
    dsD7 = dsA;
    dsD7.t = dsD7.t(1:end-1);            % 截断时间轴（尺寸不匹配）
    dsD7.SOC = dsD7.SOC(1:end-1);
    g6 = gate_e.check_g6_runtime(dsA, dsD7, 0);
    c1 = gate_ctrl.check_c1_determinism(dsA, dsD7, 'D7_size_mismatch');
    det7 = ~g6.pass && ~c1.pass && g6.skipped_fields > 0 && ~g6.comparability_pass;
    mark('D7', '尺寸不匹配必须 FAIL（旧 G6 会跳过比较空洞 PASS）', det7, ...
        sprintf('G6 skipped=%d comparability=%d | C1 skipped=%d', g6.skipped_fields, g6.comparability_pass, c1.skipped_fields));

    %% ---- 汇总 ----
    for k = 1:numel(neg.defects)
        allDetected = allDetected && neg.defects{k}.correctly_detected;
    end
    neg.all_detected = allDetected;
    close_system(MDL, 0);

    %% ===== 嵌套函数 =====
        function mark(id, desc, detected, detail)
            neg.defects{end+1} = struct('id', id, 'desc', desc, ...
                'correctly_detected', detected, 'detail', detail); %#ok<AGROW>
            if detected
                fprintf('[44] %s %s → 判据正确 FAIL（缺陷被检出）\n', id, desc);
            else
                fprintf('[44] %s %s → 判据未检出（负向验证 FAIL）\n', id, desc);
            end
        end

        function [ds, mm, rows] = runP2S01(stopT)
            assignin('base', 'EMS_CTRL_LAW', 0);
            assignin('base', 'EMS_MODE', 0);
            assignin('base', 'PMS_SOURCE_MODE', 0);
            assignin('base', 'VOYAGE_CTRL_MODE', 0);
            scn = SCENARIOS(1);
            assignin('base', 'SCN', scn);
            assignin('base', 'Bat_SOC_init', scn.initial_SOC);
            assignin('base', 'VOYAGE_SPEED_MODE', 0);
            assignin('base', 'VOYAGE_V_TIME_S', [0 1]);
            assignin('base', 'VOYAGE_V_VALUE_MPS', [0 0]);
            assignin('base', 'VOYAGE_CURRENT_MODE', 0);
            assignin('base', 'VOYAGE_CURRENT_TIME_S', [0 1]);
            assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', [0 0]);
            assignin('base', 'VOYAGE_WATER_DEPTH_M', 20);
            set_param(MDL, 'StopTime', sprintf('%.2f', stopT));
            out = sim(MDL);
            ds = gate_ctrl.get_ds(out);
            mm = gate_ctrl.metrics_of(ds, PMS_stop_thr);
            rows = gate_ctrl.audit_rows_of(ds);
        end
end
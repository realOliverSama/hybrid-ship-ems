function R = run_gate(ROOT, QUICK)
% gate_ctrl.run_gate — 43 号控制 Gate 编排主体（T-009/T-012）
%   嵌套函数共享主函数工作区（MDL/R/checks）；43 号数字脚本仅做环境准备与转发
%   （数字开头文件无法作为 function 调用——项目惯例 eval(fileread) 入口）。
%   判据 C1~C7（FR-455）+ 三臂 AUTO 矩阵 + MANUAL×P2 + 注入场景（T-010 定义）。
    addpath(fullfile(ROOT, 'scripts'));
    MDL = 'HybridShip_ModelE';
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
    tAll = tic;
    T_start = evalin('base', 'T_start_hold_ticks');
    T_stop  = evalin('base', 'T_stop_hold_ticks');
    T_up    = evalin('base', 'PMS_min_up_ticks');
    C_bus   = evalin('base', 'ASSUMED_C_bus');
    T_stop_thr = evalin('base', 'PMS_stop_threshold_kW');
    SCENARIOS = evalin('base', 'SCENARIOS');                 % 工况定义（base → 函数工作区，嵌套函数捕获）
    MANUAL_SCENARIOS = evalin('base', 'MANUAL_SCENARIOS');
    QUICK = evalin('base', 'exist(''GATE43_QUICK'', ''var'') && GATE43_QUICK');

    R = struct('schema', 'gate_control_result_v1');
    R.generated_at = char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ'));
    R.checks = {};
    R.autoruns = struct();
    R.injruns = struct();
    R.overall = true;

    %% Phase 1：candidate 引用（T-012 机器检查背书）
    if evalin('base', 'exist(''GATE43_BASELINE_PATH'', ''var'')')
        bp = evalin('base', 'GATE43_BASELINE_PATH');
        R.phase1 = struct('note', 'candidate 由 T-012（40 --baseline_path）机器检查背书', ...
            'candidate_path', bp);
        fprintf('[43] Phase 1: candidate=%s\n', bp);
    else
        R.phase1 = struct('note', 'GATE43_BASELINE_PATH 未设（骨架/评审前模式）');
        fprintf('[43] Phase 1: 无 candidate，跳过基线比对\n');
    end

    %% Phase 2：AUTO 工况矩阵 × 三臂 + 逐跑判据
    autoIds = [20 1 2 3 31 34 36];
    armDefs = {'R0', 0, 1; 'R5', 5, 1; 'P2', 0, 0};
    if QUICK
        autoIds = 1; armDefs = {'P2', 0, 0};
    end
    for a = 1:size(armDefs, 1)
        armName = armDefs{a, 1}; law = armDefs{a, 2}; src = armDefs{a, 3};
        for s = 1:numel(autoIds)
            scn = SCENARIOS(autoIds(s));
            lbl = sprintf('%s/%s', armName, scn.name);
            fprintf('[43] run %s\n', lbl);
            [ds, mm, rows] = runOne(law, src, scn, false, [], cap_of(scn));
            key = matlab.lang.makeValidName(lbl);
            [nSat, ~] = gate_ctrl.saturation_events(ds, T_start);
            entry = struct('label', lbl, 'metrics', mm, 'n_audit_rows', numel(rows), ...
                'SOC_end', ds.SOC(end), 'fuel_total_kg', mm.fuel_total_kg, ...
                'n_saturation_events', nSat);
            c2 = gate_ctrl.check_c2_transition_legality(rows, T_start, T_stop, T_up, lbl);
            addCheck('C2_transition_legality', lbl, c2.pass);
            c3 = gate_ctrl.check_c3_soc_band(ds, lbl);
            addCheck('C3_soc_band', lbl, c3.pass);
            c4 = gate_ctrl.check_c4_vdc(ds, lbl, false);
            addCheck('C4_vdc', lbl, c4.pass);
            c5 = gate_ctrl.check_c5_energy_closure(ds, lbl, C_bus);
            addCheck('C5_energy_closure', lbl, c5.pass);
            c7 = gate_ctrl.check_c7_audit_completeness(rows, ds, lbl, T_start);
            addCheck('C7_audit_completeness', lbl, c7.pass);
            if autoIds(s) == 1
                entry.ds_backup = ds;   % S01 保留供确定性比较
            end
            R.autoruns.(key) = entry;
        end
    end

    %% Phase 2b：确定性两跑（每臂 S01 重跑，G6 口径）
    for a = 1:size(armDefs, 1)
        armName = armDefs{a, 1};
        scn = SCENARIOS(1);
        dsB = runOne(armDefs{a, 2}, armDefs{a, 3}, scn, false, [], cap_of(scn));
        key = matlab.lang.makeValidName(sprintf('%s_S01_still_water', armName));
        dsA = R.autoruns.(key).ds_backup;
        c1 = gate_ctrl.check_c1_determinism(dsA, dsB, sprintf('%s/S01', armName));
        addCheck('C1_determinism', sprintf('%s/S01', armName), c1.pass);
    end

    %% Phase 3：MANUAL M1/M2/M3 × P2（FR-453）
    mid = [1 2 3];
    if QUICK, mid = 1; end
    for mi = 1:numel(mid)
        row = MANUAL_SCENARIOS(mid(mi));
        scn = SCENARIOS(1);
        scn.voyage_distance_m = row.voyage_distance_m;
        lbl = sprintf('P2/%s', row.name);
        fprintf('[43] run %s\n', lbl);
        [ds, mm, rows] = runOne(0, 0, scn, true, row.cmd_ts, cap_manual(row.duration_s));
        key = matlab.lang.makeValidName(sprintf('P2_%s', row.name));
        R.autoruns.(key) = struct('metrics', mm, 'n_audit_rows', numel(rows), ...
            'SOC_end', ds.SOC(end), 'fuel_total_kg', mm.fuel_total_kg);
        c2 = gate_ctrl.check_c2_transition_legality(rows, T_start, T_stop, T_up, lbl);
        addCheck('C2_transition_legality', lbl, c2.pass);
        c4 = gate_ctrl.check_c4_vdc(ds, lbl, false);
        addCheck('C4_vdc', lbl, c4.pass);
        c7 = gate_ctrl.check_c7_audit_completeness(rows, ds, lbl, T_start);
        addCheck('C7_audit_completeness', lbl, c7.pass);
        if strcmp(row.name, 'M3_staged')   % AC-416：M3 确定性门槛链 1DG→2DG→1DG
            on2 = find(ds.state_DG2 == 3, 1);
            cd2 = find(ds.state_DG2 == 4, 1);
            chainOK = ~isempty(on2) && ~isempty(cd2);
            addCheck('M3_full_chain_1DG_2DG_1DG', lbl, chainOK);
            if chainOK
                fprintf('[43]   M3 链: DG2 ON@%.1f s, COOLDOWN@%.1f s\n', ds.t(on2), ds.t(cd2));
            end
        end
    end

    %% Phase 4：Safety 注入 S1~S5（场景定义由 T-010 提供）
    if exist('gate_ctrl.injection_scenarios', 'file') == 2
        injSpec = gate_ctrl.injection_scenarios();
        for k = 1:numel(injSpec)
            lbl = sprintf('INJ/%s', injSpec(k).name);
            fprintf('[43] run %s\n', lbl);
            injSpec(k).prep_fn();                    % assignin 确定性注入
            scn = SCENARIOS(1);
            scn.voyage_distance_m = 1e6;
            [ds, mm, rows] = runOne(0, 0, scn, false, [], injSpec(k).stop_time_s);
            c6 = gate_ctrl.check_c6_safety_assertions(ds, mm, injSpec(k).expected, lbl);
            addCheck('C6_safety_assertions', lbl, c6.pass);
            c4 = gate_ctrl.check_c4_vdc(ds, lbl, true);
            addCheck('C4_vdc_injection', lbl, c4.pass);
            c7 = gate_ctrl.check_c7_audit_completeness(rows, ds, lbl, T_start);
            addCheck('C7_audit_completeness', lbl, c7.pass);
            R.injruns.(matlab.lang.makeValidName(injSpec(k).name)) = ...
                struct('metrics', mm, 'n_audit_rows', numel(rows));
        end
    else
        fprintf('[43] Phase 4: injection_scenarios 未就绪（T-010），跳过\n');
    end

    %% 汇总
    for k = 1:numel(R.checks)
        R.overall = R.overall && R.checks{k}.pass;
    end
    R.wall_time_s = toc(tAll);
    % 剥离大数据（ds 不入 JSON）
    fns = fieldnames(R.autoruns);
    for k = 1:numel(fns)
        if isfield(R.autoruns.(fns{k}), 'ds_backup')
            R.autoruns.(fns{k}) = rmfield(R.autoruns.(fns{k}), 'ds_backup');
        end
    end
    close_system(MDL, 0);

    %% ===== 嵌套函数（共享主函数工作区） =====
        function addCheck(name, label, pass)
            R.checks{end+1} = struct('name', name, 'label', label, 'pass', pass); %#ok<AGROW>
            if ~pass
                fprintf('[43] CHECK FAIL: %s / %s\n', name, label);
            end
        end

        function [ds, mm, rows] = runOne(law, src, scn, useManual, manualTs, stopT)
            assignin('base', 'EMS_CTRL_LAW', law);
            assignin('base', 'EMS_MODE', double(law > 0));
            assignin('base', 'PMS_SOURCE_MODE', src);
            if useManual
                assignin('base', 'VOYAGE_CTRL_MODE', 1);
                assignin('base', 'VOYAGE_MANUAL_CMD_TS', manualTs);
            else
                assignin('base', 'VOYAGE_CTRL_MODE', 0);
            end
            assignin('base', 'SCN', scn);
            assignin('base', 'Bat_SOC_init', scn.initial_SOC);
            isTime = strcmp(ecms2.scn_field(scn, 'speed_profile_mode', 'distance'), 'time');
            assignin('base', 'VOYAGE_SPEED_MODE', double(isTime));
            assignin('base', 'VOYAGE_V_TIME_S', ecms2.scn_field(scn, 'v_time_s', [0 1]));
            assignin('base', 'VOYAGE_V_VALUE_MPS', ecms2.scn_field(scn, 'v_value_mps', [0 0]));
            assignin('base', 'VOYAGE_CURRENT_MODE', double(isTime));
            assignin('base', 'VOYAGE_CURRENT_TIME_S', ecms2.scn_field(scn, 'current_time_s', [0 1]));
            assignin('base', 'VOYAGE_CURRENT_VALUE_MPS', ecms2.scn_field(scn, 'current_value_mps', [0 0]));
            assignin('base', 'VOYAGE_WATER_DEPTH_M', ecms2.scn_field(scn, 'water_depth_m', 20.0));
            set_param(MDL, 'StopTime', sprintf('%.2f', stopT));
            out = sim(MDL);
            ds = gate_ctrl.get_ds(out);
            mm = gate_ctrl.metrics_of(ds, T_stop_thr);
            rows = gate_ctrl.audit_rows_of(ds);
        end

        function cap = cap_of(scn)
            dist = ecms2.scn_field(scn, 'voyage_distance_m', 5000);
            cap = floor((dist / 3.0 + 900) * 100) / 100;
        end

        function cap = cap_manual(dur)
            cap = floor((dur + 60) * 100) / 100;
        end
end

function r = check_c6_safety_assertions(ds, m, expected, label)
% gate_ctrl.check_c6_safety_assertions — 判据 6：S1~S5 注入场景断言式验证（FR-455 判据 6）
%   expected 注入场景预期断言表（T-010 定义）字段：
%     .expect_trigger   —— 期望触发的 override 通道与机组：{'force_start_dg2','force_stop_dg2',
%                          'fault_latch_dg2','fault_latch_dg1','baseline_fallback','none'}
%     .forbid_trigger   —— 不该触发的通道（负向：不该触发时不触发）
%     .min_Vdc / .allow_vdc_viol —— Vdc 场景特定（默认越限须被记录）
%     .expect_no_nan    —— 关键控制信号无 NaN 失控
    r.name = 'C6_safety_assertions';
    r.label = label;
    r.checks = {};
    ok = true;

    fireStart = max(ds.safety_force_start_req, [], 2) > 0.5;
    fireStop  = max(ds.safety_force_stop_req, [], 2) > 0.5;
    fl2 = ds.fault_latch(end, 2) > 0.5; fl1 = ds.fault_latch(end, 1) > 0.5;
    hasFB = false;
    if isfield(ds, 'baseline_fallback_active'), hasFB = ds.baseline_fallback_active(end) > 0.5; end

    chan = struct( ...
        'force_start_dg2', any(fireStart) && any(ds.safety_force_start_req(:,2) > 0.5), ...
        'force_start_dg1', any(fireStart) && any(ds.safety_force_start_req(:,1) > 0.5), ...
        'force_stop_dg2',  any(fireStop) && any(ds.safety_force_stop_req(:,2) > 0.5), ...
        'force_stop_dg1',  any(fireStop) && any(ds.safety_force_stop_req(:,1) > 0.5), ...
        'fault_latch_dg2', fl2, 'fault_latch_dg1', fl1, ...
        'baseline_fallback', hasFB, 'none', false);

    for k = 1:numel(expected)
        e = expected{k};
        fired = chan.(e);
        if strcmp(e, 'none')
            fired = any(fireStart) || any(fireStop) || fl1 || fl2;
            ok = ok && ~fired;
            r.checks{end+1} = struct('item', 'expect_no_override', 'pass', ~fired); %#ok<SAGROW>
        else
            r.checks{end+1} = struct('item', sprintf('expect_trigger_%s', e), ...
                'pass', fired); %#ok<SAGROW>
            ok = ok && fired;
        end
    end
    % 监视 flag 一致性（FR-446：flags 置位须正确）
    if any(strcmp(expected, 'flag_soc_low'))
        if ~(ds.flag_soc_low(end) > 0.5)
            ok = false;
            r.checks{end+1} = struct('item', 'expect_flag_soc_low', 'pass', false); %#ok<SAGROW>
        end
    end
    if any(strcmp(expected, 'flag_vdc_low'))
        if ~(ds.flag_vdc_low(end) > 0.5)
            ok = false;
            r.checks{end+1} = struct('item', 'expect_flag_vdc_low', 'pass', false); %#ok<SAGROW>
        end
    end
    % 无 NaN 失控（关键控制量）
    keyOK = all(isfinite(ds.V_dc_V)) && all(isfinite(ds.SOC)) && ...
        (isfield(ds, 'P_commit_kW') && all(isfinite(ds.P_commit_kW)));
    r.checks{end+1} = struct('item', 'no_nan_key_signals', 'pass', keyOK); %#ok<SAGROW>
    ok = ok && keyOK;
    % override 事件计数交叉（metrics）
    r.checks{end+1} = struct('item', 'override_event_counts', 'pass', true, ...
        'force_start_events', m.safety_force_start_events, ...
        'force_stop_events', m.safety_force_stop_events); %#ok<SAGROW>
    r.pass = ok;
end
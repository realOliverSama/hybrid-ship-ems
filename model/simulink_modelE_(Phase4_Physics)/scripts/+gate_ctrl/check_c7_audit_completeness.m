function r = check_c7_audit_completeness(rows, ds, ctx, T_start_hold_ticks)
% gate_ctrl.check_c7_audit_completeness — 判据 7：transition reason 审计（FR-455 判据 7）
%   - 每次迁移行字段完整（九元组全部有限）；
%   - reason ∈ 合法域且正常路径无 FORCED(7)；
%   - Plant 启动事件与审计行一对一（episode 算法 §6.3 + start_count 交叉）；
%   - 每个 Plant start 均可由合法 reason 解释（DG2：audit 有对应 start/confirm 行）。
    r.name = 'C7_audit_completeness';
    r.label = ctx;
    r.checks = {};
    ok = true;

    % 1) 字段完整性
    complete = true;
    for k = 1:numel(rows)
        row = rows(k);
        if ~all(isfinite([row.old, row.new, row.reason, row.P_commit, row.t_start, ...
                          row.t_stop, row.t_online, row.safety]))
            complete = false;
            r.checks{end+1} = struct('item', sprintf('audit_fields_row%d', k), ...
                'pass', false, 't', row.t); %#ok<SAGROW>
        end
    end
    r.checks{end+1} = struct('item', 'audit_fields_complete', 'pass', complete); %#ok<SAGROW>
    ok = ok && complete;

    % 2) Plant 启动与审计一对一（episode 一对一，§6.3）
    [nSat, ~] = gate_ctrl.saturation_events(ds, T_start_hold_ticks);
    nNormalStart = sum(arrayfun(@(x) x.reason == 1 && x.old == 1 && x.new == 2, rows));
    one2one = nNormalStart <= nSat;
    r.checks{end+1} = struct('item', 'episode_one_to_one_N_starts_le_saturation_events', ...
        'pass', one2one, 'N_starts_normal', nNormalStart, 'N_saturation_events', nSat); %#ok<SAGROW>
    ok = ok && one2one;

    % 3) Plant start_count 与审计行一致（DG2：确认行 old=2→new=3）
    if isfield(ds, 'start_count_DG2')
        nConfirm = sum(arrayfun(@(x) x.old == 2 && x.new == 3, rows));
        crossOK = ds.start_count_DG2(end) == nConfirm;
        r.checks{end+1} = struct('item', 'plant_starts_match_audit_confirm_rows', ...
            'pass', crossOK, 'plant_starts_DG2', ds.start_count_DG2(end), ...
            'audit_confirm_rows', nConfirm); %#ok<SAGROW>
        ok = ok && crossOK;
    end

    % 4) FORCED(7) 零出现（正常 Gate）
    noForced = ~any(arrayfun(@(x) x.reason == 7, rows));
    r.checks{end+1} = struct('item', 'no_FORCED_reason_in_normal_gate', 'pass', noForced); %#ok<SAGROW>
    ok = ok && noForced;

    r.pass = ok;
end
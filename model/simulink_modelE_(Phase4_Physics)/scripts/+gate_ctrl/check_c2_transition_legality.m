function r = check_c2_transition_legality(rows, T_start_hold_ticks, T_stop_hold_ticks, PMS_min_up_ticks, ctx)
% gate_ctrl.check_c2_transition_legality — 判据 2：规则合法性（FR-455 判据 2，替代 starts 硬阈值）
%   逐迁移断言（design §6.2）：
%   - reason 合法域 {1..7}；FORCED(7) 在正常 Gate 中出现即 FAIL（兼负向验证向量）；
%   - 正常 start（reason==1, old=1→new=2）：审计 t_start==T_start_hold_ticks（饱和）；
%   - 正常 stop（reason==2, old=3→new=1）：t_stop==T_stop_hold_ticks ∧ t_online>=min_up；
%   - SAFETY_*(3..6) 豁免 timer 检查；
%   - episode 一对一：N_starts(reason==1) <= N_saturation_events（§6.3，另见 episode 输出）；
%   - 正常 start 的审计记录须为饱和值（§6.3 第 4 步）。
    r.name = 'C2_transition_legality';
    r.label = ctx;
    r.checks = {};
    ok = true;

    for k = 1:numel(rows)
        row = rows(k);
        if ~ismember(row.reason, 1:7)
            ok = false;
            r.checks{end+1} = struct('item', sprintf('reason_domain_row%d', k), 'pass', false, ...
                'reason', row.reason, 't', row.t); %#ok<SAGROW>
            continue;
        end
        if row.reason == 7
            ok = false;
            r.checks{end+1} = struct('item', sprintf('FORCED_in_normal_gate_row%d', k), ...
                'pass', false, 't', row.t); %#ok<SAGROW>
            continue;
        end
        isNormalStart = (row.old == 1 && row.new == 2 && row.reason == 1);
        isNormalStop  = (row.old == 3 && row.new == 1 && row.reason == 2);
        if isNormalStart && row.t_start ~= T_start_hold_ticks
            ok = false;
            r.checks{end+1} = struct('item', sprintf('normal_start_timer_row%d', k), ...
                'pass', false, 't_start', row.t_start, 'expect', T_start_hold_ticks, 't', row.t); %#ok<SAGROW>
        end
        if isNormalStop && ~(row.t_stop == T_stop_hold_ticks && row.t_online >= PMS_min_up_ticks)
            ok = false;
            r.checks{end+1} = struct('item', sprintf('normal_stop_timer_row%d', k), ...
                'pass', false, 't_stop', row.t_stop, 't_online', row.t_online, 't', row.t); %#ok<SAGROW>
        end
    end
    r.pass = ok;
end
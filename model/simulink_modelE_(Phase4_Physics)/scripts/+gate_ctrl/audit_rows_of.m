function rows = audit_rows_of(ds)
% gate_ctrl.audit_rows_of — commitment 迁移审计表提取（FR-440，T-009）
%   迁移检测：audit_new_state 变化时刻 = 迁移拍（chart 审计值迁移后保持至下次迁移）。
%   行字段 = FR-440 九元组（timestamp 取 logsout 采样时刻，其余取迁移拍保持值）。
    rows = struct('t', {}, 'old', {}, 'new', {}, 'reason', {}, 'P_commit', {}, ...
        't_start', {}, 't_stop', {}, 't_online', {}, 'safety', {});
    nv = ds.audit_new_state(:);
    for k = 2:numel(nv)
        if nv(k) ~= nv(k-1)
            r = struct('t', ds.t(k), 'old', ds.audit_old_state(k), 'new', nv(k), ...
                'reason', pick(ds.reason_code, ds.t, ds.t(k)), ...
                'P_commit', ds.audit_P_commit_kW(k), ...
                't_start', ds.audit_t_start_hold(k), ...
                't_stop', ds.audit_t_stop_hold(k), ...
                't_online', ds.audit_t_online(k), ...
                'safety', ds.audit_safety_override(k));
            rows(end+1) = r; %#ok<AGROW>
        end
    end
end

function v = pick(sig, t, tq)
    [~, i] = min(abs(t - tq));
    v = sig_at(sig, i);
end

function v = sig_at(sig, i)
    v = sig(i);
end
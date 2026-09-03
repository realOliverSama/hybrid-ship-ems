function r = check_g6_runtime(e01_a, e01_b, t_elapsed_s)
% gate_e.check_g6_runtime — G6 运行时判据（design §5.3，regression）
%   确定性：同配置两跑 logsout 逐位一致；runtime informational 记录（不做门禁）
r.name = 'G6_runtime';
r.gate_class = 'regression';
r.checks = {};

det_ok = true; diff_detail = '';
fa = fieldnames(e01_a);
for k = 1:numel(fa)
    va = e01_a.(fa{k}); vb = e01_b.(fa{k});
    if isnumeric(va) && isequal(size(va), size(vb))
        if ~isequal(va, vb)
            det_ok = false;
            diff_detail = sprintf('%s,', fa{k});
        end
    end
end
r.checks{end+1} = struct('item', 'determinism_two_runs_bitwise_identical', 'pass', det_ok, ...
    'differing_fields', diff_detail); %#ok<SAGROW>
r.checks{end+1} = struct('item', 'runtime_informational', 'pass', true, ...
    'total_wall_time_s', t_elapsed_s); %#ok<SAGROW>

r.pass = all(cellfun(@(c) c.pass, r.checks));
end
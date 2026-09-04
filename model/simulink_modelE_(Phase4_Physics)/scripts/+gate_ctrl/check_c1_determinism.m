function r = check_c1_determinism(dsA, dsB, label)
% gate_ctrl.check_c1_determinism — 判据 1：确定性（FR-455 判据 1，G6 口径函数复用）
%   同工况同配置两跑逐位一致（logsout 数值逐位比较；时间轴等长前提）。
    r.name = 'C1_determinism';
    r.label = label;
    r.checks = {};
    ok = true; diff_fields = '';
    fa = fieldnames(dsA);
    for k = 1:numel(fa)
        va = dsA.(fa{k}); vb = [];
        if isfield(dsB, fa{k}), vb = dsB.(fa{k}); end
        if isnumeric(va) && isequal(size(va), size(vb))
            if ~isequal(va, vb)
                ok = false;
                diff_fields = [diff_fields, fa{k}, ',']; %#ok<AGROW>
            end
        elseif ~isfield(dsB, fa{k})
            ok = false;
            diff_fields = [diff_fields, fa{k}, '(missing),']; %#ok<AGROW>
        end
    end
    r.checks{end+1} = struct('item', 'two_runs_bitwise_identical', 'pass', ok, ...
        'differing_fields', diff_fields); %#ok<SAGROW>
    r.pass = ok;
end
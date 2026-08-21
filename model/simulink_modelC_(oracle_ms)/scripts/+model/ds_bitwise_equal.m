function [ok, detail] = ds_bitwise_equal(a, b)
% model.ds_bitwise_equal — 逐位一致比较两个 extract_logsout ds 结构
% 要求：同字段集合、同时间轴、逐字段逐元素完全相等（==0，不允许 1e-12 容差）。
% 输出 ok=false 时 detail 给出首个不一致字段与最大差。
    ok = true;
    detail = '';
    fa = fieldnames(a);
    fb = fieldnames(b);
    if numel(fa) ~= numel(fb) || ~all(strcmp(sort(fa), sort(fb)))
        ok = false;
        detail = sprintf('field set mismatch: %d vs %d', numel(fa), numel(fb));
        return;
    end
    for i = 1:numel(fa)
        f = fa{i};
        x = a.(f); y = b.(f);
        if ~isequal(size(x), size(y))
            ok = false;
            detail = sprintf('%s size %s vs %s', f, mat2str(size(x)), mat2str(size(y)));
            return;
        end
        d = max(abs(x(:) - y(:)));
        if d > 0 || any(isnan(x(:)) ~= isnan(y(:)))
            ok = false;
            detail = sprintf('%s max|diff|=%.3g', f, d);
            return;
        end
    end
end

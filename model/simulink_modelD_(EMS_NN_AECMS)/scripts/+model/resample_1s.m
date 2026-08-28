function res = resample_1s(ds)
% model.resample_1s — 将 logsout 数据重采样到 1 s（数据集导出周期）
    t = ds.t;
    t1 = (0:1:floor(t(end)))';
    res = struct('t', t1);
    names = fieldnames(ds);
    for i = 1:numel(names)
        nm = names{i};
        if strcmp(nm, 't'), continue; end
        v = ds.(nm);
        % 分段常数插值（0.01s 固定步长记录，取每 1s 内最后值）
        idx = zeros(numel(t1), 1);
        for k = 1:numel(t1)
            m = find(t <= t1(k) + 1e-9, 1, 'last');
            if isempty(m), m = 1; end
            idx(k) = m;
        end
        res.(nm) = v(idx);
    end
end

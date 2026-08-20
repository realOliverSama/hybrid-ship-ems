function ds = extract_logsout(out)
% model.extract_logsout — 将 SimulationOutput.logsout 提取为 struct
% 返回: ds.t (列向量), ds.<SignalName> (列向量)
% 恒定信号（1 个采样点）沿全程复制；缺失信号填 NaN。
    ls = out.logsout;
    n = ls.numElements;
    names = cell(n, 1);
    vals = cell(n, 1);
    tmax = [];
    for i = 1:n
        el = ls.getElement(i);
        nm = el.Name;
        if isempty(nm)
            continue; % 跳过未命名元素
        end
        try
            v = el.Values;
            tt = double(v.Time(:));
            dd = squeeze(v.Data);
            if size(dd, 2) > 1, dd = dd(:, 1); end
            dd = double(dd(:));
        catch
            tt = []; dd = [];
        end
        if isempty(tt), continue; end
        names{i} = nm;
        vals{i} = struct('t', tt, 'd', dd);
        if numel(tt) > numel(tmax), tmax = tt; end
    end
    t = tmax;
    if isempty(t)
        error('logsout 为空或所有元素未命名');
    end
    ds = struct('t', t);
    for i = 1:n
        if isempty(names{i}), continue; end
        nm = matlab.lang.makeValidName(names{i});
        tt = vals{i}.t; dd = vals{i}.d;
        if numel(tt) == numel(t)
            ds.(nm) = dd;
        elseif numel(tt) == 1
            % 恒定信号：全程复制
            ds.(nm) = repmat(dd(1), numel(t), 1);
        else
            % 时间轴不同：按最近邻对齐
            idx = zeros(numel(t), 1);
            for k = 1:numel(t)
                m = find(tt <= t(k) + 1e-9, 1, 'last');
                if isempty(m), m = 1; end
                idx(k) = m;
            end
            ds.(nm) = dd(idx);
        end
    end
end

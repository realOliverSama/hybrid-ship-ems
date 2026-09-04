function ds = get_ds(out)
% gate_ctrl.get_ds — logsout → 结构化数据集（T-009，43 号控制 Gate 数据基础）
%   返回 ds.t（全程统一时间轴）+ 标量信号为 N×1 列、向量信号为 N×k 矩阵（列=元素）。
%   恒定信号沿全程复制；缺失信号不建字段（调用方以 isfield 判定）。
%   R2026a 注意：logsout 元素 Values 可能为 timeseries（变化）或 double（恒值），
%   向量信号 Data 形如 [1×k×N] 需 squeeze 转置。
    ls = out.logsout;
    n = ls.numElements;
    t = [];
    raw = struct('nm', {}, 't', {}, 'd', {});
    for i = 1:n
        el = ls.getElement(i);
        nm = el.Name;
        if isempty(nm), continue; end
        try
            v = el.Values;
            if isa(v, 'timeseries')
                tt = double(v.Time(:));
                dd = v.Data;
                if ndims(dd) == 3
                    dd = permute(dd, [3 2 1]);          % [1×k×N] → [N×k×1]
                    dd = squeeze(dd);
                    if size(dd, 1) ~= numel(tt), dd = dd'; end
                else
                    dd = double(dd(:));
                end
            else
                tt = 0; dd = double(v(:))';              % 恒值信号：单点
            end
        catch
            continue;
        end
        if isempty(tt), continue; end
        raw(end+1) = struct('nm', nm, 't', tt, 'd', dd); %#ok<SAGROW>
        if numel(tt) > numel(t) || isempty(t), t = tt; end
    end
    if isempty(t), error('gate_ctrl.get_ds: logsout 为空'); end
    ds = struct('t', t);
    for i = 1:numel(raw)
        nm = matlab.lang.makeValidName(raw(i).nm);
        tt = raw(i).t; dd = raw(i).d;
        if size(dd, 2) > 1 && numel(tt) == numel(t)
            ds.(nm) = dd;                                 % 向量信号 [N×k]
        elseif numel(tt) == numel(t)
            ds.(nm) = dd(:);
        elseif numel(tt) == 1
            ds.(nm) = repmat(dd(1), numel(t), 1);
        else
            idx = interp1(tt, 1:numel(tt), t, 'nearest', 1);
            ds.(nm) = dd(idx);
        end
    end
end
function write_param_csv(outPath)
% model.write_param_csv — 由 base workspace 的 ParamMeta 生成参数审计 CSV
% 列: parameter, value, unit, source, confidence, used_by, needs_confirmation
    if ~evalin('base', 'exist(''ParamMeta'', ''var'')')
        error('ParamMeta 未定义，请先运行 config/Model_Parameters.m');
    end
    meta = evalin('base', 'ParamMeta');
    fid = fopen(outPath, 'w', 'n', 'UTF-8');
    fprintf(fid, 'parameter,value,unit,source,confidence,used_by,needs_confirmation\n');
    for i = 1:size(meta, 1)
        nm = meta{i, 1};
        v = evalin('base', sprintf('(%s)', nm));
        if isnumeric(v) && numel(v) <= 8
            vs = mat2str(v);
        elseif isnumeric(v)
            vs = sprintf('[%dx%d %s]', size(v, 1), size(v, 2), class(v));
        elseif islogical(v)
            vs = mat2str(v);
        else
            vs = string(v);
        end
        fprintf(fid, '%s,%s,%s,%s,%s,%s,%s\n', ...
            nm, vs, meta{i, 2}, meta{i, 3}, meta{i, 4}, meta{i, 5}, mat2str(meta{i, 6}));
    end
    fclose(fid);
    fprintf('[parameter_audit] 已生成 %s（%d 项）\n', outPath, size(meta, 1));
end

function write_json(path, md)
% model.write_json — 简易 JSON 写出（metadata）
    fid = fopen(path, 'w', 'n', 'UTF-8');
    f = fieldnames(md);
    fprintf(fid, '{\n');
    for i = 1:numel(f)
        v = md.(f{i});
        if isnumeric(v)
            s = sprintf('%.10g', v);
        else
            s = sprintf('%s', v);
        end
        if i < numel(f)
            fprintf(fid, '  "%s": "%s",\n', f{i}, s);
        else
            fprintf(fid, '  "%s": "%s"\n', f{i}, s);
        end
    end
    fprintf(fid, '}\n');
    fclose(fid);
end

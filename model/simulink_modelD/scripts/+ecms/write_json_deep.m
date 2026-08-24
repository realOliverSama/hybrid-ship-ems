function write_json_deep(path, md)
% ecms.write_json_deep — 嵌套 struct/数值/逻辑/字符/元胞数组 JSON 写出
% （Phase 0 机器可读产物；model.write_json 仅支持扁平字符串字段）
% 输入: path 输出路径；md 任意嵌套结构
    [d, ~, ~] = fileparts(path);
    if ~isempty(d) && ~exist(d, 'dir'), mkdir(d); end
    fid = fopen(path, 'w', 'n', 'UTF-8');
    if fid < 0
        error('ECMS:JSON', '无法写入 %s', path);
    end
    try
        writeVal(fid, md, 0);
        fprintf(fid, '\n');
        fclose(fid);
    catch err
        fclose(fid);
        rethrow(err);
    end
end

function writeVal(fid, v, ind)
    if isstruct(v)
        writeStruct(fid, v, ind);
    elseif ischar(v) || isstring(v)
        fprintf(fid, '"%s"', jsonEscape(char(v)));
    elseif isnumeric(v) && isscalar(v)
        if isfinite(v)
            fprintf(fid, '%.10g', v);
        else
            fprintf(fid, 'null');
        end
    elseif islogical(v) && isscalar(v)
        if v, fprintf(fid, 'true'); else, fprintf(fid, 'false'); end
    elseif isnumeric(v) || islogical(v)
        fprintf(fid, '[');
        for k = 1:numel(v)
            if k > 1, fprintf(fid, ','); end
            if islogical(v(k))
                if v(k), fprintf(fid, 'true'); else, fprintf(fid, 'false'); end
            else
                fprintf(fid, '%.10g', v(k));
            end
        end
        fprintf(fid, ']');
    elseif iscell(v)
        fprintf(fid, '[');
        for k = 1:numel(v)
            if k > 1, fprintf(fid, ','); end
            writeVal(fid, v{k}, ind + 1);
        end
        fprintf(fid, ']');
    else
        fprintf(fid, 'null');
    end
end

function writeStruct(fid, s, ind)
    fn = fieldnames(s);
    fprintf(fid, '{\n');
    for k = 1:numel(fn)
        fprintf(fid, repmat('  ', 1, ind + 1));
        fprintf(fid, '"%s": ', fn{k});
        writeVal(fid, s.(fn{k}), ind + 1);
        if k < numel(fn), fprintf(fid, ','); end
        fprintf(fid, '\n');
    end
    fprintf(fid, repmat('  ', 1, ind));
    fprintf(fid, '}');
end

function s = jsonEscape(s)
    s = strrep(s, '\', '\\');
    s = strrep(s, '"', '\"');
    s = strrep(s, sprintf('\n'), '\\n');
    s = strrep(s, sprintf('\t'), '\\t');
end

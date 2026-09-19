function assert_genb_path(ROOT, path_names, path_first)
% ppe.assert_genb_path — Generation-B 路径解析守卫（fail-closed）。
% path_names/path_first：freeze 快照中的关键函数名与**仓库相对**解析首路径。
% 运行时第一解析（转相对后）必须与快照逐条一致；任何 shadowing 即拒绝执行。
if numel(path_names) ~= numel(path_first)
    error('PPE:E3B:PATH', '快照列长度不一致');
end
bad = {};
FS = filesep;
for i = 1:numel(path_names)
    fn = path_names{i};
    w = which(fn);   % 运行时第一解析（绝对）
    if startsWith(w, [ROOT FS])
        wr = w(length(ROOT) + 2:end);   % 转仓库相对
    else
        wr = w;                          % 仓库外解析 => 必然不匹配
    end
    if isempty(wr) || ~strcmp(wr, path_first{i})
        bad{end + 1} = sprintf('%s: which=%s freeze=%s', ...
            fn, wr, path_first{i}); %#ok<AGROW>
    end
end
if ~isempty(bad)
    error('PPE:E3B:PATH', '关键函数路径解析与 freeze 快照不一致：\n  %s', ...
        strjoin(bad, '\n  '));
end
end

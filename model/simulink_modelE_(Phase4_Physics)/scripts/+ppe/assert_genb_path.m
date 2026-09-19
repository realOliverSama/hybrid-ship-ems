function assert_genb_path(path_names, path_first) %#ok<INUSD>
% ppe.assert_genb_path — Generation-B 路径解析守卫（fail-closed）。
% path_names/path_first：freeze 快照中的关键函数名与解析首路径（平行 cellstr）。
% 运行时第一解析必须与快照逐条一致；任何 shadowing 即拒绝执行。
if numel(path_names) ~= numel(path_first)
    error('PPE:E3B:PATH', '快照列长度不一致');
end
bad = {};
for i = 1:numel(path_names)
    fn = path_names{i};
    w = which(fn);   % 运行时第一解析
    if isempty(w) || ~strcmp(w, path_first{i})
        bad{end + 1} = sprintf('%s: which=%s freeze=%s', ...
            fn, local_show(w), local_show(path_first{i})); %#ok<AGROW>
    end
end
if ~isempty(bad)
    error('PPE:E3B:PATH', '关键函数路径解析与 freeze 快照不一致：\n  %s', ...
        strjoin(bad, '\n  '));
end
end

function s = local_show(x)
if isempty(x)
    s = '<empty>';
elseif ischar(x)
    s = x;
else
    s = mat2str(x);
end
end

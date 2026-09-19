function assert_genb_path(ROOT, which_all)
% ppe.assert_genb_path — Generation-B 路径解析守卫（fail-closed）。
% 关键函数的运行时解析（which 第一结果）必须与 freeze 快照逐条一致；
% 任何 shadowing（路径上的同名替代实现）即拒绝执行。
fn = fieldnames(which_all);
bad = {};
for i = 1:numel(fn)
    w = which(fn{i});   % 运行时第一解析
    want = which_all.(fn{i});
    if ischar(want), want = {want}; end
    want1 = want{1};
    if isempty(w) || ~strcmp(w, want1)
        bad{end + 1} = sprintf('%s: which=%s freeze=%s', ...
            fn{i}, local_show(w), local_show(want1)); %#ok<AGROW>
    end
end
if ~isempty(bad)
    error('PPE:E3B:PATH', '关键函数路径解析与 freeze 快照不一致：\n  %s', ...
        strjoin(bad, '\n  '));
end
end

function s = local_show(x)
if isnumeric(x) || isempty(x)
    s = mat2str(x);
else
    s = char(x);
end
end

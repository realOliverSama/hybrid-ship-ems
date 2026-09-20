function assert_control_bindings(rec)
% ppe.assert_control_bindings — 逐段控制关键绑定复核（fail-closed）。
%
% 用途（EP3 执行路径修复 §F）：run_cstar 在每个决策段开头调用本函数，重新从 base
% 工作区回读全部控制关键变量，与 ppe.arm_runtime_bind 产出的绑定记录逐位比对。
% 任何"后来者"（通用注入、桥接、诊断脚本、checkpoint 恢复等）改动关键绑定 ⇒ 立即
% 报错终止，而不是静默改变实验语义。
%
% 输入 rec：ppe.arm_runtime_bind 返回的绑定记录（含 bindings 全量期望值）。
% 错误 ID：PPE:ARMBIND:DRIFT（含逐项 diff）
if nargin < 1 || ~isstruct(rec) || ~isfield(rec, 'bindings')
    error('PPE:ARMBIND:DRIFT', 'assert_control_bindings 需要 arm_runtime_bind 记录');
end
b = rec.bindings;
names = fieldnames(b);
bad = {};
for k = 1:numel(names)
    nm = names{k};
    if ~evalin('base', sprintf('exist(''%s'', ''var'')', nm))
        bad{end + 1} = sprintf('%s 缺失（绑定后被移除）', nm); %#ok<AGROW>
        continue;
    end
    got = evalin('base', nm);
    exp = b.(nm);
    ok = isnumeric(got) && isequal(size(got), size(exp)) ...
        && all(isfinite(got(:))) && isequal(got, exp);
    if ~ok
        bad{end + 1} = sprintf('%s: 绑定 %s → 现值 %s', nm, ...
            local_show(exp), local_show(got)); %#ok<AGROW>
    end
end
if ~isempty(bad)
    error('PPE:ARMBIND:DRIFT', ...
        '臂 %s 控制关键绑定在运行中被改写（段级复核失败）：\n  %s', ...
        rec.arm, strjoin(bad, '\n  '));
end
end

function s = local_show(x)
if isnumeric(x) && isscalar(x)
    s = sprintf('%g', x);
else
    s = mat2str(x);
end
end

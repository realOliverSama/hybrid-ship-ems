function v = scn_field(SCN, fld, defval)
% ecms2.scn_field — 安全读取 SCN 场景字段（供 Simulink Constant 块 Value 表达式使用）
% 用法（Constant 块 Value，模型编译期在 base workspace 求值）：
%   ecms2.scn_field(SCN,'v_time_s',[0 1])
% 语义：字段存在且非空时返回其值，否则返回 defval。用于让既有场景（无时间序列
% 字段）的模型结构保持可编译，同时 S20 携带新字段时自动生效（T-101 框架扩展）。
    if isfield(SCN, fld) && ~isempty(SCN.(fld))
        v = SCN.(fld);
    else
        v = defval;
    end
end

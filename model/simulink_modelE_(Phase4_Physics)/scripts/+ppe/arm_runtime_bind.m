function rec = arm_runtime_bind(arm, varargin)
% ppe.arm_runtime_bind — 臂运行时**唯一权威**控制选择绑定阶段（EP3 执行路径修复）。
%
% 背景（2026-09-18 Execution-Path Defect，见 CONTROL_PATH_FUNNEL_CLOSURE_REPORT.md）：
% 通用 mission 注入（mission_v3.scenario_compile → ppe.inject_voyage）会把
% PMS_SOURCE_MODE 写成 0，而注入后的恢复步骤只恢复 EMS_CTRL_LAW ⇒ 正式 C* 实验里
% PMS 源选择器恒选 baseline 需求律，EMS 输出被整体丢弃。
%
% 本函数是该缺陷的**结构性修复**：在通用注入**之后**统一绑定全部控制关键变量
% （集合与期望值来自 ppe.control_critical_vars 单一事实源），随后逐项**回读断言**，
% 任一项不符即 fail-closed。绑定记录 rec 交给 run_cstar 逐段复核
% （ppe.assert_control_bindings），使"后来者覆盖关键绑定"必然报错而不是静默生效。
%
% 调用契约（顺序不可颠倒）：
%   1) 通用 mission/场景注入（含 VOYAGE_*、SCN、Bat_SOC_init、StopTime）
%   2) ppe.arm_runtime_bind(arm, ...)        ← 本函数（权威绑定）
%   3) 仿真（run_cstar 每段开头 ppe.assert_control_bindings(rec)）
%
% Name-value:
%   'ForecastSource' char  — 臂预测来源标识（写入记录；不改变工作区控制变量）。
%                            缺省取 ppe.control_critical_vars 表中的臂定义。
%   'OutDir'         char  — 若给出，则把绑定记录写成 JSON（迁移/审计用）。
%   'Strict'         logical — 缺省 true；false 时只记录不抛错（**仅**允许在
%                            明确标注的非正式诊断中使用，正式路径禁止）。
%
% 输出 rec（struct）：schema / arm / bound_at_utc / matlab_version /
%   bindings（name→value 的绑定值）/ workspace_readback（回读值）/
%   forecast_source / controller_identity / strict
%
% Fail-closed 错误 ID：PPE:ARMBIND:ARM / :READBACK / :NONFINITE / :STRICT

p = inputParser;
p.addParameter('ForecastSource', '', @ischar);
p.addParameter('OutDir', '', @ischar);
p.addParameter('Strict', true, @(x) islogical(x) || isnumeric(x));
p.parse(varargin{:});
opt = p.Results;

spec = ppe.control_critical_vars();
idx = find(strcmp({spec.arm}, arm));
if isempty(idx)
    error('PPE:ARMBIND:ARM', ...
        'arm "%s" 不在控制关键变量表内（允许：%s）', arm, ...
        strjoin({spec.arm}, ', '));
end
s = spec(idx);
if isempty(opt.ForecastSource)
    opt.ForecastSource = s.forecast_source;
end

% ---- 1) 权威绑定（在通用注入之后执行）----
names = {'PMS_SOURCE_MODE', 'EMS_CTRL_LAW', 'EMS_MODE', 'EMS_WORK_MODE', ...
    'AI_Pred_Source', 'ADAPTER_FORCE_MODE', 'ADAPTER_FORCE_P1_KW', ...
    'ADAPTER_FORCE_P2_KW'};
vals = {s.PMS_SOURCE_MODE, s.EMS_CTRL_LAW, s.EMS_MODE, s.EMS_WORK_MODE, ...
    s.AI_Pred_Source, s.ADAPTER_FORCE_MODE, s.ADAPTER_FORCE_P1_KW, ...
    s.ADAPTER_FORCE_P2_KW};
for k = 1:numel(names)
    assignin('base', names{k}, vals{k});
end
assignin('base', 'EMS_HEALTH_TS', s.EMS_HEALTH_TS);

% ---- 2) 回读断言（fail-closed）----
rb = struct();
bad = {};
for k = 1:numel(names)
    got = evalin('base', names{k});
    rb.(names{k}) = got;
    if ~(isnumeric(got) && isscalar(got) && isfinite(got) && got == vals{k})
        bad{end + 1} = sprintf('%s: 期望 %g, 回读 %s', names{k}, vals{k}, ...
            local_show(got)); %#ok<AGROW>
    end
end
gotH = evalin('base', 'EMS_HEALTH_TS');
rb.EMS_HEALTH_TS = gotH;
if ~(isnumeric(gotH) && isequal(size(gotH), size(s.EMS_HEALTH_TS)) ...
        && all(isfinite(gotH(:))) && isequal(gotH, s.EMS_HEALTH_TS))
    bad{end + 1} = sprintf('EMS_HEALTH_TS: 期望 %s, 回读 %s', ...
        mat2str(s.EMS_HEALTH_TS), local_show(gotH));
end
if ~isempty(bad)
    msg = sprintf('臂 %s 控制关键绑定回读失败：\n  %s', arm, strjoin(bad, '\n  '));
    if opt.Strict
        error('PPE:ARMBIND:READBACK', '%s', msg);
    else
        warning('PPE:ARMBIND:READBACK', '%s', msg);
    end
end

% ---- 3) 绑定记录（供逐段复核 + 遥测工件）----
rec = struct();
rec.schema = 'ppe_arm_runtime_binding_v1';
rec.arm = arm;
rec.bound_at_utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
v = ver('MATLAB');
rec.matlab_version = sprintf('%s %s', v.Name, v.Version);
rec.forecast_source = opt.ForecastSource;
rec.controller_identity = s.controller_identity;
rec.strict = logical(opt.Strict);
rec.bindings = struct('PMS_SOURCE_MODE', s.PMS_SOURCE_MODE, ...
    'EMS_CTRL_LAW', s.EMS_CTRL_LAW, 'EMS_MODE', s.EMS_MODE, ...
    'EMS_WORK_MODE', s.EMS_WORK_MODE, 'AI_Pred_Source', s.AI_Pred_Source, ...
    'ADAPTER_FORCE_MODE', s.ADAPTER_FORCE_MODE, ...
    'ADAPTER_FORCE_P1_KW', s.ADAPTER_FORCE_P1_KW, ...
    'ADAPTER_FORCE_P2_KW', s.ADAPTER_FORCE_P2_KW, ...
    'EMS_HEALTH_TS', s.EMS_HEALTH_TS);
rec.workspace_readback = rb;
rec.semantics = struct( ...
    'PMS_SOURCE_MODE', ['1=EMS_AGGREGATE(EMS 参考值进入 dispatch)；' ...
        '0=BASELINE_POLICY（EMS 输出被丢弃）— source: ' ...
        '07_PMS_v2/Source_Selector, cmp_mode_baseline=(PMS_SOURCE_MODE<0.5)'], ...
    'EMS_CTRL_LAW', 'Variant select: 5=SOC-Adaptive, 6=Oracle predictive');

if ~isempty(opt.OutDir)
    if ~exist(opt.OutDir, 'dir'), mkdir(opt.OutDir); end
    fid = fopen(fullfile(opt.OutDir, sprintf('arm_binding_%s.json', arm)), 'w');
    fwrite(fid, jsonencode(rec, 'PrettyPrint', true));
    fclose(fid);
end
end

function s = local_show(x)
if isnumeric(x) && isscalar(x)
    s = sprintf('%g', x);
else
    s = mat2str(x);
end
end

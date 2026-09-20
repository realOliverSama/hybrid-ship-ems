function man = arm_runtime_manifest(varargin)
% ppe.arm_runtime_manifest — Phase 4-F 臂运行时绑定清单（机器可读，可写盘）。
%
% 输出 man（struct）：
%   schema / generated_at_utc / source_of_truth / arms / semantics / assertions
%
%   arms(i) = {arm, EMS_CTRL_LAW, PMS_SOURCE_MODE, EMS_MODE, EMS_WORK_MODE,
%              AI_Pred_Source, ADAPTER_FORCE_MODE, ADAPTER_FORCE_P1_KW,
%              ADAPTER_FORCE_P2_KW, EMS_HEALTH_TS, forecast_source,
%              controller_identity}
%
%   semantics：PMS_SOURCE_MODE 的源语义（1=EMS_AGGREGATE / 0=BASELINE_POLICY）
%              与消费点（Source Selector 判据、Safety nominal_is_ems）逐条登记。
%
% 用途（EP3）：epoch-3 冻结包必须绑定本清单（§E/§M）；冻结生成器与 Q-EP3-10/11
% 都以本函数为唯一事实源，禁止在别处复制绑定表。
%
% Name-value: 'OutFile' char — 给出则写 JSON（目录不存在则创建）。

p = inputParser;
p.addParameter('OutFile', '', @ischar);
p.parse(varargin{:});
opt = p.Results;

spec = ppe.control_critical_vars();
v = ver('MATLAB');
man = struct();
man.schema = 'ppe_arm_runtime_manifest_v1';
man.generated_at_utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
man.matlab_version = sprintf('%s %s', v.Name, v.Version);
man.source_of_truth = 'scripts/+ppe/control_critical_vars.m';
man.arms = spec;
man.semantics = struct( ...
    'PMS_SOURCE_MODE', ['1 = EMS_AGGREGATE (EMS reference enters the dispatch ' ...
        'allocator); 0 = BASELINE_POLICY (EMS output discarded)'], ...
    'PMS_SOURCE_MODE_definition', 'config/ModelE_Control_Parameters.m:56', ...
    'PMS_SOURCE_MODE_consumers', ['07_PMS_v2/Source_Selector ' ...
        '(cmp_mode_baseline = PMS_SOURCE_MODE < 0.5 selects BASELINE input); ' ...
        '16_Safety_Interlock nominal_is_ems (fback latch enable)'], ...
    'EMS_CTRL_LAW_definition', 'config/ModelE_Physics_Parameters.m:40', ...
    'EMS_CTRL_LAW_consumer', '05_EMS_Controller Variant Subsystem', ...
    'phase4f_arm_intent', ['all four arms intend the EMS law to drive the ' ...
        'plant; the baseline branch is reserved for the S5 safety fallback']);
man.assertions = struct( ...
    'PMS_SOURCE_MODE_ARM_BOUND', true, ...
    'NO_RUNTIME_DEFAULT_FOR_PMS_SOURCE_MODE', true, ...
    'BINDING_STAGE', 'after generic mission injection, before any sim', ...
    'DRIFT_POLICY', 'PPE:ARMBIND:DRIFT (fail-closed, per segment)');

if ~isempty(opt.OutFile)
    d = fileparts(opt.OutFile);
    if ~isempty(d) && ~exist(d, 'dir'), mkdir(d); end
    fid = fopen(opt.OutFile, 'w');
    fwrite(fid, jsonencode(man, 'PrettyPrint', true));
    fclose(fid);
end
end

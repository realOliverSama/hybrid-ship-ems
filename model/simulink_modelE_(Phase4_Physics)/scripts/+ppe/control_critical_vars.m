function spec = control_critical_vars()
% ppe.control_critical_vars — Phase 4-F 臂运行时**控制关键变量**单一事实源。
%
% 设立依据（2026-09-18 Execution-Path Defect）：正式 C* 实验的 EMS 输出被
% 07_PMS_v2 源选择器整体丢弃，根因是 mission_v3.scenario_compile 的注入清单把
% PMS_SOURCE_MODE 硬编码为 0（baseline bench convention），而通用注入之后的恢复
% 步骤只恢复了 EMS_CTRL_LAW。本表把"哪些变量决定 EMS 是否真的驱动 plant"显式化、
% 机器可读化，供 arm_runtime_bind / assert_control_bindings / 冻结写入点审计共用。
%
% 源语义（逐条可查，不得猜测）：
%   PMS_SOURCE_MODE  1 = EMS_AGGREGATE（EMS 参考值进入 dispatch allocator）；
%                    0 = BASELINE_POLICY（PMS 基线需求律，EMS 输出被丢弃）。
%      定义：config/ModelE_Control_Parameters.m:56；
%      消费：07_PMS_v2 Source_Selector —— cmp_mode_baseline = (PMS_SOURCE_MODE < 0.5)，
%            该条件为真时选择 baseline 输入（frozen slx: system_317 SID 380/381/383/384）；
%            同一变量还作为 16_Safety_Interlock 的 nominal_is_ems，决定 baseline
%            fallback 锁存是否使能（frozen slx: system_409 SID 446 → C_source_mode）。
%   EMS_CTRL_LAW     变体选择（0=Baseline 1=AI 2=ScheduledFF 4=FixedECMS
%                    5=SOC-Adaptive 6=Oracle 7=NN-AECMS）；
%      定义：config/ModelE_Physics_Parameters.m:40（默认 0 是第二个静默陷阱：
%            忘写即跑 baseline 律）。
%   EMS_MODE         兼容列 = double(EMS_CTRL_LAW > 0)（Physics:39）。
%   EMS_WORK_MODE    0=手动 1=半自动 2=全自动（Physics:43；law∈{0,1,2,3} 消费）。
%   AI_Pred_Source   0=Disabled/Persistence 1=FromWorkspace 2=External 3=EmbeddedML
%                    （Physics:41；law∈{1,2,7} 消费）。
%   ADAPTER_FORCE_MODE / ADAPTER_FORCE_P1_KW / ADAPTER_FORCE_P2_KW
%                    台架强制通道（ModelE_TestScenarios.m:302-304）。
%   EMS_HEALTH_TS    EMS 健康通道（From Workspace → 16_Safety_Interlock
%                    ems_health_src）；标称 = 全程健康 [0 1; 1e6 1]
%                    （ModelE_TestScenarios.m:285）。
%
% 非控制关键（本表**不含**，binder 不得触碰）：SCN / VOYAGE_* / Bat_SOC_init /
% T_eval / StopTime —— 属 mission/场景输入，由通用注入层负责。
%
% Phase 4-F 臂语义（四臂的实验意图都是"EMS 律驱动 plant"）：
%   P0  = law 5（SOC-自适应 ECMS，无预测），无预测通道；
%   C0  = law 6 oracle 预测，forecast = 严格因果持续性（persistence）；
%   C1  = law 6 oracle 预测，forecast = 冻结完美信息真值（C1 stored windows）；
%   C2  = law 6 oracle 预测，forecast = 冻结 Predictor-v3 经活桥。
%   ⇒ 四臂 PMS_SOURCE_MODE 一律 = 1（EMS_AGGREGATE）。baseline 分支保留给
%     S5 安全 fallback，不是任何臂的运行模式。
%
% 输出：spec = 1×4 struct array，字段：
%   arm / EMS_CTRL_LAW / PMS_SOURCE_MODE / EMS_MODE / EMS_WORK_MODE /
%   AI_Pred_Source / ADAPTER_FORCE_MODE / ADAPTER_FORCE_P1_KW /
%   ADAPTER_FORCE_P2_KW / EMS_HEALTH_TS / forecast_source / controller_identity

arms = {'P0', 'C0', 'C1', 'C2'};
law = [5, 6, 6, 6];
fsrc = {'N/A_NO_FORECAST', 'oracle', 'c1_stored', 'live_bridge'};
cid = {'law5_soc_adaptive_ecms', 'law6_oracle_ecms', 'law6_oracle_ecms', ...
       'law6_oracle_ecms'};

spec = struct('arm', arms, 'EMS_CTRL_LAW', num2cell(law), ...
    'PMS_SOURCE_MODE', num2cell(ones(1, 4)), ...
    'EMS_MODE', num2cell(double(law > 0)), ...
    'EMS_WORK_MODE', num2cell(2 * ones(1, 4)), ...
    'AI_Pred_Source', num2cell(zeros(1, 4)), ...
    'ADAPTER_FORCE_MODE', num2cell(zeros(1, 4)), ...
    'ADAPTER_FORCE_P1_KW', num2cell(zeros(1, 4)), ...
    'ADAPTER_FORCE_P2_KW', num2cell(zeros(1, 4)), ...
    'forecast_source', fsrc, 'controller_identity', cid);
for i = 1:numel(spec)
    spec(i).EMS_HEALTH_TS = [0 1; 1e6 1];   % 标称：全程健康
end
end

# Model B：直流母线电气层 + 安全/节能指标卡 + S10 工况 Tasks

Spec: `model-b-dc-bus`  
Status: implementation-complete
Created: 2026-08-19  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`  
Design: `./design.md`

## Implementation Plan

- [x] 1. 开分支并勘察改名后的现状（Kimi 线重构后的文件结构、质量门机制）
  - ID: T-001
  - Requirement(s): NFR-004
  - Files/areas: 仓库根；`model/simulink/scripts/+model/`（注意已改名，原 +modela_k3）；`config/Model_GateThresholds.m`、`gate_baseline_smoke.json`
  - Validation: `git checkout -b feat/model-b-dc-bus`（基于 main d977c9f8）；读通 `+model/build_model.m`、`03_run_smoke_tests.m`、质量门脚本，确认调用约定（包名、函数签名、质量门跑法）

- [x] 2. 参数表扩展：新增电气层与指标参数（ASSUMED_ 标注 + ParamMeta 行）
  - ID: T-002
  - Requirement(s): NFR-003, NFR-008
  - Files/areas: `model/simulink/config/Model_Parameters.m`
  - Validation: MATLAB 中运行参数脚本无错误；`reports/parameter_audit.csv` 重建后含新增行（C_bus=40F、K_droop、η_rect、η_dcdc、EMS_Manual_Pgen、V_floor、EMS_WORK_MODE、k_eq 等）

- [x] 3. 工况扩展：水流剖面机制 + 新增 S10 水流反转（不动 S01–S09 语义）
  - ID: T-003
  - Requirement(s): FR-006
  - Files/areas: `config/Model_Scenarios.m`（新增 current_knots 字段+S10）；`+model/build_model.m` 的 01_Scenario_Manager 构建器（水流 Constant → 里程剖面函数）
  - Validation: 重建 Model A 后跑 S01–S09 中 3 个工况（S01/S03/S08）与改名前结果一致（电流恒定工况行为不变）；S10 单独仿真验证水流剖面 −1→+1 m/s 正确过渡

- [x] 4. Model A 回归验证（证明扩展未破坏 A）
  - ID: T-004
  - Requirement(s): NFR-004
  - Files/areas: `scripts/02_build_model.m`、`03_run_smoke_tests.m`、质量门脚本
  - Validation: Model A 重建 + 5 项烟雾测试全 PASS；质量门（gate_compare vs gate_baseline_smoke.json）通过

- [x] 5. 实现 Model B 构建函数（电气层闭环：下垂电池+母线电容+欠压降额+监视器）
  - ID: T-005
  - Requirement(s): FR-001, FR-002, FR-003, FR-004, FR-005, NFR-001, NFR-007
  - Files/areas: 新增 `+model/build_model_b.m`（自包含，复制必要构建器）；生成 `model/HybridShip_ModelB_DCBus.slx`
  - Validation: `scripts/08_build_modelB.m` 从零重建成功；EMS_MODE=0/1 Update Diagram 通过；删除 .slx 后可重建（NFR-002）

- [x] 6. EMS 三模式实现（手动/半自动/全自动，异常只报警不切模式）
  - ID: T-006
  - Requirement(s): FR-005b
  - Files/areas: `+model/build_model_b.m` 的 EMS 变体构建器
  - Validation: EMS_WORK_MODE=0/1/2 三模式 Update Diagram 均通过；手动模式 S01 短仿真 P_gen_ref 恒等于 ASSUMED_EMS_Manual_Pgen_kW

- [x] 7. Model B 烟雾测试（T01B/T04B/T10B）+ 数值稳定性确认
  - ID: T-007
  - Requirement(s): FR-008, NFR-001
  - Files/areas: 新增 `scripts/08b_smoke_modelB.m`（或并入 08）；`+model/dcbus_metrics.m` 初版
  - Validation: T01B：巡航 V_dc∈[675,795] V 且稳态偏差<5%；T04B：扰动段电压可观测跌落且最深点在负荷上升段、t_recovery 有输出；T10B：全程无 NaN/Inf、逆水段电压低于顺水段；无求解器刚性报错

- [x] 8. 下垂系数灵敏度扫描（K_droop ∈ {5,10,20} kW/V × S04）并固化选定值
  - ID: T-008
  - Requirement(s): FR-002, NFR-003
  - Files/areas: `+model/dcbus_metrics.m`；参数表；扫描结果写入报告
  - Validation: 三组扫描结果（稳态偏差/跌落深度/t_recovery）表格化；选定值满足稳态偏差<5% 且瞬态不越 CCS 瞬态带；参数表更新并标注明细

- [x] 9. 指标卡与四工况对比（S01/S04/S07/S10 × Baseline/AI）
  - ID: T-009
  - Requirement(s): FR-007, FR-010, NFR-005
  - Files/areas: 新增 `scripts/09_run_modelB_comparison.m`、`+model/dcbus_metrics.m`（完整版）、`+model/modelb_plot.m`
  - Validation: 生成 `reports/ModelB_comparison_report.md` 含完整指标卡（fuel_kg + FEC_kg 并列）；`results/modelB_compare/<scen>/*.png`（V_dc 曲线对比、功率对比、SOC 对比）；AI 无优势项如实呈现

- [x] 10. 回退验证与 Model A 零改动验证
  - ID: T-010
  - Requirement(s): FR-005, NFR-004
  - Files/areas: Model B（MLP_WeightsLoaded=0）；git
  - Validation: Model B 在 AI 模式但权重缺失时行为 == Baseline（max|ΔP_gen|<1e-6）；`git diff main -- model/simulink/model/HybridShip_Model_MVP.slx` 及 Model A 脚本为空（仅允许参数表/工况表的新增行）

- [x] 11. 交付两种 EMS 控制逻辑的纯文本流程图
  - ID: T-011
  - Requirement(s): FR-009
  - Files/areas: `reports/ems_control_flowcharts.md`（Baseline 规则门限流程 + AI 预测增强流程，ASCII）
  - Validation: 文件生成；流程图与代码实际逻辑逐步核对一致；同时在会话回复中贴出

- [x] 12. 提交、推送、收尾报告
  - ID: T-012
  - Requirement(s): NFR-004
  - Files/areas: 分支 feat/model-b-dc-bus；`.specs/model-b-dc-bus/`（spec 文件入库）
  - Validation: commit 信息清晰（参数/工况/模型/测试/报告分别说明）；`git push -u origin feat/model-b-dc-bus` 成功；spec 文件已纳入仓库；本地与远程一致

## Verification Checklist

- [x] Requirements IDs are referenced by tasks.
- [x] Design testing strategy is represented by tasks（T-004 回归、T-007 烟雾、T-008 扫描、T-009 对比、T-010 回退）。
- [x] Each task has a clear validation command or manual check.
- [x] Rollback/migration tasks are included when applicable（T-012 回滚=删分支；T-004/T-010 双重保护 Model A）。

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run implementation-readiness validation before coding.
- When coding starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering.
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.

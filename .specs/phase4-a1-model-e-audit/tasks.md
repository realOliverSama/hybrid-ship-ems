# Phase 4-A1 — Model E 建模与参数审计 Tasks

Spec: `phase4-a1-model-e-audit`  
Status: implementation-in-progress
Created: 2026-09-03  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`  
Design: `./design.md`

> 本 spec 为纯审计零施工段（对应路线文档 `docs/phase4_research_roadmap.md` 的
> T-401 + T-402）。任务编号 T-001~T-009 为本 spec 局部编号；
> 路线文档 T-4xx 号段在 A2 spec 中继续使用。

## Implementation Plan

- [x] 1. 建立 A1 资产目录与零改动基线
  - ID: T-001
  - Requirement(s): FR-408, FR-409, NFR-402
  - Files/areas: 新建 `docs/phase4/README.md`、`docs/phase4/a1_model_audit/README.md`、`docs/phase4/a1_model_audit/scripts/`；更新 `docs/README.md`（§20 登记）；基线记录（写入审计报告附录草案）：`git status --short model/` + `Model_Parameters.m` / `Model_Scenarios.m` / `scripts/+model/build_model_c.m` / 模型 `.slx` 的 sha256
  - Validation: 目录与 README 就位；基线 sha256 表落盘；`git status` 确认 Model D 零改动
  - Notes: Model D 只读纪律自此生效

- [x] 2. 参数提取脚本 build_parameter_table.py
  - ID: T-002
  - Requirement(s): FR-404, FR-405, NFR-401, NFR-402
  - Files/areas: 新建 `docs/phase4/a1_model_audit/scripts/build_parameter_table.py`；输入（只读）：`model/simulink_modelD_(EMS_NN_AECMS)/config/Model_Parameters.m`、`Model_Scenarios.m`、`reports/parameter_audit.csv`；输出：`docs/phase4/a1_model_audit/modelE_parameter_audit.csv`（十列，OQ-1 列集）
  - Validation: 手造 .m 片段小样本提取对照正确；全量运行行数与既有 parameter_audit.csv（112 行）数量级对照无异常；幂等重跑逐位一致；无法识别的行以 pending+notes 入表（不静默丢弃）
  - Notes: 纯文本解析，不需 MATLAB；幂等保留人工列（design §4.3）

- [x] 3. 参数五级分类人工复核与影响等级标注
  - ID: T-003
  - Requirement(s): FR-404, FR-405, FR-406
  - Files/areas: `modelE_parameter_audit.csv`（source_class5 override / impact / notes 三列人工复核）
  - Validation: 全表 source_class5 ∈ 六值枚举无空缺；pending 行 notes 全非空；override 行理由记录；重点十类假设（FR-406 清单）impact 均有标注
  - Notes: 主 Agent 分析工作；用户回填字段（user_value/user_reference）本任务留空

- [x] 4. 架构审计 architecture_audit.json 撰写
  - ID: T-004
  - Requirement(s): FR-401, FR-402, FR-403
  - Files/areas: 新建 `docs/phase4/a1_model_audit/architecture_audit.json`；清点输入（只读）：`scripts/+model/build_model_c.m` + 模型文件结构
  - Validation: `subsystem_inventory` 覆盖 builder 清点的全部顶层 subsystem（100%）；每项 disposition/fidelity/rtcost/rationale 无空缺；`additions` 覆盖五类新模块（浅水 UKC / DG1 / DG2 / 启停状态机 / Battery ECM+SOP）；`propeller_decision.choice` 二选一且 basis 非空
  - Notes: 主 Agent 核心分析工作；清点双源交叉（design §4.1）；螺旋桨分支依据 `docs/reference/` 与 `docs/data_request_list.md` 现状裁决

- [x] 5. 报告渲染脚本与两份 MD 报告
  - ID: T-005
  - Requirement(s): FR-401, FR-403, FR-406, NFR-401
  - Files/areas: 新建 `scripts/render_audit_reports.py`；产出 `modelE_architecture_audit.md`、`modelE_parameter_audit.md`（含手工撰写的"假设影响分析"保留块）
  - Validation: 渲染脚本连跑两遍输出逐位一致；保留块内容重跑不丢失；报告含 FR-406 十类假设影响分析与"最可能影响论文结论"清单
  - Notes: 保留块缺失时报错而非覆盖（design §5）

- [x] 6. 完整性 Gate 脚本 gate_a1_completeness.py
  - ID: T-006
  - Requirement(s): FR-407, NFR-401, NFR-402
  - Files/areas: 新建 `scripts/gate_a1_completeness.py`；输出 `gate_result_a1.json`
  - Validation: 完整链（T-002→T-005 产物）运行 Gate，G1~G5 全过、`overall: true`、退出码 0
  - Notes: FAIL 时非零退出 + stderr 缺项清单（design §4.4）

- [x] 7. Gate 负向验证
  - ID: T-007
  - Requirement(s): FR-407, AC-405
  - Files/areas: 临时构造缺项副本（不污染正式产物）：(a) 删一个 subsystem 条目；(b) 置空一格 source_class5；(c) 删 propeller_decision
  - Validation: 三种缺项均被 Gate 正确检出 FAIL 且退出码非零、缺项信息正确；正式产物复跑恢复 overall=true
  - Notes: 临时副本用 mktemp 目录，用后清理（不动仓库）

- [x] 8. 零改动证据收尾与 README 登记
  - ID: T-008
  - Requirement(s): FR-408, FR-409, AC-406
  - Files/areas: 审计报告附录（sha256 前后对照表 + git status 证据）；`docs/phase4/a1_model_audit/README.md` 全条目登记核对；`docs/README.md` 登记核对
  - Validation: sha256 前后逐位一致；`git status --short model/` 为空；README 条目与实际文件一致（§20）
  - Notes: 附 A1 会话的绝对路径扫描（NFR-402，`grep -rE` 排除 URL/env）

- [ ] 9. 用户评审门
  - ID: T-009
  - Requirement(s): AC-401~AC-407
  - Files/areas: 两份审计报告 + gate_result_a1.json + 本文件全部勾选
  - Validation: 用户对两份报告结论书面批准；A2 spec 方可立项
  - Notes: 评审意见如需修改结论，回到对应任务修订并重跑 Gate

## 任务依赖

```text
T-001 ─► T-002 ─► T-003 ─┐
              T-004 ─────┴─► T-005 ─► T-006 ─► T-007 ─► T-008 ─► T-009
```

T-003 与 T-004 无相互依赖，可交换顺序；其余严格按序。

## Verification Checklist

- [x] Requirements IDs are referenced by tasks（FR-401~409 / NFR-401~404 / AC-405/406 全覆盖）
- [x] Design testing strategy is represented by tasks（小样本=T-002；集成=T-006；负向=T-007；幂等=T-002/T-005；零改动=T-001/T-008；用户评审=T-009）
- [x] Each task has a clear validation command or manual check
- [x] Rollback/migration tasks are included when applicable（design §8：回退=删除 docs/phase4/，Model D 从未触碰，无需迁移任务）

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run implementation-readiness validation before coding.
- When coding starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering（T-003/T-004 可交换）。
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.
- 执行分工（brainstorm 已记）：主 Agent 承担分析（T-003/T-004/T-009 材料）；
  机械性脚本实现（T-002/T-005/T-006）如委派 subagent 须先经用户授权（AGENTS.md §3.3）。

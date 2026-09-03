# Phase4 A1 Model E Audit Brainstorm

Spec: `phase4-a1-model-e-audit`  
Status: brainstorm-approved  
Created: 2026-09-03

## User Intent

## User Intent

Phase 4（系统优化重构，`Phase4` 分支，路线文档 `docs/phase4_research_roadmap.md`）工作包 A「Model E 船舶物理模型升级」拆为两个 spec；本 spec 为 **A1：审计与设计决策段**（路线文档 T-401 + T-402，纯分析、零施工），产出作为 A2 施工段（T-403~T-412）的 Fixed Decisions 输入。

## 裁决记录（Q-A1 ~ Q-A5，用户逐项批准）

- **Q-A1 (b)**：A 包拆两个 spec——A1 = T-401 建模审计 + T-402 参数真实性审计（纯分析）；A2 = T-403~T-412 施工（另立 spec）。
- **Q-A2 (a)**：Model E 工作区 = Model D 整个复制分叉为 `model/simulink_modelE_(Phase4_Physics)/`，Model D 冻结不动；参数化共用方案否决（会破坏 Model D 复现性）。注：A1 为纯审计，不创建该工作区；工作区创建属 A2。
- **Q-A3（按主 Agent 建议）**：标注体系 = GPT 方案为主（每 subsystem 标 KEEP/REFINE/REPLACE/ADD/REJECT 处置决策 + Physical Fidelity / Real-Time Cost 两等级）；T-402 参数审计沿用五级来源分类（图纸/厂商/文献/估计/假设，映射现行 DRAWING/LEGACY/ASSUMED 体系）；Real-Time Cost 仅信息性标注（本 Phase 不以实时化为目标，供后续 HIL Phase 参考）；旧四色法弃用。**用户补充**：T-402 产出 = 全参数导出为一张总表，逐参数标注来源类别，留字段由用户逐个回填可用典型参数值与对应文献（表 = 审计 + 待补数据清单二合一）。
- **Q-A4 (a)**：参数总表以 CSV 为准（延续 reports/parameter_audit.csv 惯例，机器可读）；列：参数名/当前值/单位/所在模块/现行口径/五级来源分类/影响等级/用户回填典型值/用户回填文献来源/备注；MD 审计报告只放摘要与重点分析。
- **Q-A5（认可）**：A1 验收 = 机器可读完整性门（T-401 全顶层 subsystem 覆盖率 100% 无空缺；T-402 全参数入 CSV 且来源分类无空缺；T-406 螺旋桨分支在报告中有明确二选一结论）+ 用户评审门（两份审计报告结论经用户批准后方可立 A2）。

## Proposed Approach

A1 产出三件套：① `modelE_architecture_audit.md`（T-401：Model D 全部顶层 subsystem——Scenario_Manager / Ship_Speed_Controller / Propulsion_Plant / Load_Measurement / EMS_Controller / Genset / Battery / DC Bus / DC-DC / Fuel model / Protection——逐项处置决策 + 两等级 + T-406 螺旋桨分支结论）；② 参数总表 CSV（T-402，全参数 + 五级分类 + 用户回填字段）；③ `modelE_parameter_audit.md` 摘要报告（重点分析：哪些假设最可能影响论文结论——船体阻力/排水量/吃水/水深/螺旋桨/电机效率/发电机最低负荷与启停/电池内阻 OCV/辅机负荷）。

## Assumptions

- Phase 4 边界：暂不做 N6/HIL/NPU（路线文档 §1.2）；Model D 工作区冻结。
- 执行以主 Agent 分析为主；机械性提取（参数清单从 Model_Parameters.m 导出）可委派。
- 用户将以 CSV 回填字段的形式分批提供真实参数与文献。

## Remaining Doubts

- 无阻塞性疑问。T-406 螺旋桨资料可用性在 A1 审计中收敛（二选一结论即审计产出之一）。

## Decision

- [x] 待用户批准进入 requirements 起草。（2026-09-03 用户批准）

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [x] User approved creating the spec skeleton and drafting requirements.（2026-09-03）

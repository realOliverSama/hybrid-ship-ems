# Phase 4 — Model E：面向 HIL 的模型重构与实时化 Brainstorm

> **⚠️ SUPERSEDED（2026-09-03，用户 Q1 裁决）**：本 draft 已被 `docs/phase4_research_roadmap.md`
> 新 Phase 4 路线（A~F 工作包）取代；本目录保留存档，禁止实施。

Spec: `phase4-model-e`  
Status: brainstorm-complete  
Created: 2026-08-31

## User Intent

用户意图：依据外部顾问（GPT）五优先级建议，为"Model E 下一代平台"建立 Phase 4 spec 的 tasks.md，内部标注 draft，后期与本地模型讨论定稿；本轮不要求走完整提问式 brainstorm（用户指令即 gate 结论：直接落草稿）。

范围提案（草稿，源自 GPT 建议 + 仓库现状）：Model E = Model D 的下一代面向 HIL/实时的重构平台——①参数升级优先级由五级来源分类（Measured/Datasheet/Literature/Estimated/Assumed ↔ 现行 DRAWING/LEGACY/ASSUMED 审计体系映射）驱动；②结构演进以 build_model_c.m 逐 subsystem 评估（哪部分是假设/真实/可升级/升级后破坏实时性）；③sample time 与 solver 决策（10ms/5ms/1ms 候选）依赖 HIL 平台资料（经纬恒润设备型号、plant 宿主、N647 接口形式、I/O 周期、CPU 核数、目标 fixed-step——均在用户工程项目文件夹，未入库）；④HIL 接口映射设计。与 Phase 3（CHIL/轨迹级 EMS/生成器 v3）并行不阻塞，汇合点为 CHIL plant 切换 Model E-RT。

假设：Phase 4 编号顺延 Phase 2.5/3；任务编号沿用 T-4xx 段；工作区未来新建 model/simulink_modelE_(...)，Model D 冻结不动。

遗留疑问（已写入 tasks.md 开放问题节，讨论时必须回答）：①Model E 与 Phase 3 CHIL 平台的先后/并行关系；②HIL 资料何时可得；③是否要求 plant 代码生成（ERT）就绪；④真实船体主尺度/螺旋桨参数是否存在；⑤Phase 3 轨迹级 EMS 在哪个平台验证（Model D-CHIL vs Model E-RT）。

状态：本轮仅产出 tasks.md（tasks-draft）+ spec_init 骨架（requirements/design 亦为 draft 模板）；禁止在 requirements/design/tasks 任一门禁批准前实施。

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.

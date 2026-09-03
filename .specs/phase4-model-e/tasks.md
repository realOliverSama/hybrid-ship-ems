# Phase 4 — Model E：面向 HIL 的模型重构与实时化 Tasks

> **⚠️ SUPERSEDED（2026-09-03，用户 Q1 裁决）**：本 draft 已被 `docs/phase4_research_roadmap.md`
> 新 Phase 4 路线（A~F 工作包）取代；HIL 实时化推后为后续 Phase（待 HIL 资料到位再立项），
> 本文件 T-401~T-407 编号作废（新路线复用 T-4xx 号段）。本目录保留存档，禁止实施。

Spec: `phase4-model-e`  
Status: tasks-draft  
Created: 2026-08-31  
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

> ## ⚠️ DRAFT 声明（定稿前必读）
>
> 本文件为**初始草稿**，依据外部顾问（GPT）五优先级建议 + Phase 3 补充规划
> （`docs/research_roadmap.md` §8.6/§8.8）起草，**待用户与本地模型讨论定稿**。
>
> - 任务编号、范围、顺序、验收标准均可讨论修改，不构成实施授权；
> - 定稿流程：先讨论并批准 `requirements.md`（FR-4xx）→ `design.md` → 本文件
>   升为 `tasks-approved`，之后方可实施（SDD 门禁 + AGENTS.md §6）；
> - 当前 requirements/design 亦为骨架草稿，Requirement(s) 字段引用的 FR-4xx
>   编号是**占位映射**，待 requirements 定稿后回填核对。

## 开放问题（讨论定稿前必须回答）

1. **平台关系**：Model E 是 Phase 3 CHIL 平台的演进替代（plant 切换），还是与
   Model D-CHIL 长期并行？汇合点与切换判据是什么？
2. **HIL 资料时点**：经纬恒润设备型号、plant 宿主、N647 接口形式、I/O 周期、
   CPU 核数、目标 fixed-step 何时可提供？（T-401 的输入，卡住 T-404）
3. **代码生成边界**：Model E 是否要求 plant 可 ERT/Embedded Coder 代码生成，
   还是仅要求 fixed-step 可跑 + 实时性余量证明？
4. **真实参数可用性**：船体主尺度、排水量、螺旋桨图谱、实机油耗曲线在图纸/厂方
   资料中是否存在？（决定 T-402 是"接入真实参数"还是"维持假设并显式标注"）
5. **控制律承载**：Phase 3 的 trajectory-aware EMS 在哪个平台验证——Model D-CHIL
   先行、Model E-RT 复验，还是直接在 Model E 上开发？
6. **编号与命名**：工作区名 `model/simulink_modelE_(?)` 括号后缀、Phase 4 是否
   拆分为多个 spec（平台/控制/数据各一）。

## Implementation Plan（DRAFT）

> 通用约束（草稿，讨论可改）：
> - 前置输入：Phase 3 CHIL 大表结论；T-401 HIL 资料到位（用户侧）；
> - 延续全部工程纪律：脚本驱动可从零重建（禁止 GUI 手工状态）；参数来源五级
>   分级审计（Measured/Datasheet/Literature/Estimated/Assumed，映射现行
>   DRAWING/LEGACY/ASSUMED 体系并在 parameter_audit 中统一）；每阶段机器可读
>   Gate（`gate_result.json`，`error('GATE:*')` 终止）；Model D 工作区冻结不动；
> - 工作区（拟）：`model/simulink_modelE_(<后缀待定)>/`，config/scripts/model/
>   python/reports 目录结构与 Model D 一致（README 索引随建随登记）。

- [ ] 1. HIL 平台资料收集与固化
  - ID: T-401
  - Requirement(s): FR-401（占位）
  - Files/areas: `docs/reference/hil_platform.md`（新建，入库）；用户提供原始资料
  - Validation: 资料清单逐项有出处（型号/宿主/接口/周期/CPU/fixed-step 六项全非空或显式标注"暂缺"）；用户确认
  - Notes: 内容清单见 roadmap §8.6 表格；此任务是 T-404 的硬前置，缺失则 Phase 4 卡在决策门
  - DRAFT: 范围与产出路径待讨论

- [ ] 2. 参数升级优先级矩阵
  - ID: T-402
  - Requirement(s): FR-402（占位）
  - Files/areas: 基于 `reports/parameter_audit.csv` + `config/Model_Parameters.m`；产出 `reports/model_e_parameter_upgrade_plan.md`
  - Validation: 每个 Model D 参数标注五级来源分类 + Model E 升级动作（keep/upgrade/replace/add）+ 升级对实时性的影响预估；用户评审
  - Notes: 重点覆盖当前缺失项——船体主尺度、排水量、螺旋桨特性、实机 BSFC 形状、辅机负载谱；"不改假参数凑指标"红线（roadmap §9.3）继续生效
  - DRAFT: 五级分类与现有三级体系的映射规则待讨论

- [ ] 3. Model E 架构评审（builder 逐 subsystem 评估）
  - ID: T-403
  - Requirement(s): FR-403（占位）
  - Files/areas: 输入 `scripts/+model/build_model_c.m`（~2000 行，15 个顶层子系统）；产出 `reports/model_e_architecture_review.md`
  - Validation: 每个 subsystem 给出四色标注——假设值区 / 真实参数区 / 可升级区 / 升级后破坏实时性风险区；与 T-402 矩阵交叉核对一致
  - Notes: 明确 DC 母线电气层（0.01s 动态）在目标步长下的处理（保留/降阶/等效平均）
  - DRAFT: 四色分类法是否合适待讨论

- [ ] 4. Sample time 与 solver 决策
  - ID: T-404
  - Requirement(s): FR-404（占位）
  - Files/areas: 产出 `reports/model_e_rt_step_decision.md`；候选 Ts = 10ms / 5ms / 1ms / 更快
  - Validation: 决策有据——T-401 的 HIL 目标 fixed-step + T-403 的实时性风险标注 + 步长敏感性预实验（关键动态不失真判据，机器可读）
  - Notes: 决策只依赖 HIL 资料与实验证据，不预设结论；EMS 控制周期 1s 与 plant 步长解耦关系需显式论证
  - DRAFT: 候选步长集合待 T-401 后收敛

- [ ] 5. Model E 构建与 smoke
  - ID: T-405
  - Requirement(s): FR-405（占位）
  - Files/areas: 新工作区 `model/simulink_modelE_(...)/`：config（参数+场景）、+model/build_model_e.m（从 build_model_c.m 派生）、03 系 smoke、Gate 阈值
  - Validation: 删模可从零重建；smoke 全过（机器可读 gate_result.json overall=true）；Model D 基线场景（S01/S20）行为对照报告（预期差异显式解释）
  - Notes: 构建器派生策略（复制分叉 vs 参数化共用函数库）需在 design.md 定稿
  - DRAFT: 与 Model D 的回归对照口径待讨论

- [ ] 6. 实时性验证与代码生成就绪性
  - ID: T-406
  - Requirement(s): FR-406（占位）
  - Files/areas: 步长压力测试 + （若 T-404 决策要求）ERT/Embedded Coder 就绪性检查
  - Validation: 目标步长下单步计算耗时余量 ≥ 安全系数（阈值 design 定）；无代数环/不可生成块；结果机器可读
  - DRAFT: 是否包含代码生成分支取决于开放问题 3

- [ ] 7. HIL 接口映射设计
  - ID: T-407
  - Requirement(s): FR-407（占位）
  - Files/areas: 产出 `reports/model_e_hil_interface.md`（信号表：plant ↔ N647，接口形式/方向/精度/周期）
  - Validation: 接口表覆盖 T-401 清单全部物理接口；与 Phase 3 CHIL 通信方案（N6↔plant 步进同步）不冲突；用户评审
  - Notes: 与 Phase 3 固件侧（CAN/以太网驱动）共享信号定义，避免两套口径
  - DRAFT: 接口形式取决于 T-401 实际资料

## 任务依赖（DRAFT）

```text
T-401 (HIL 资料, 用户侧)
   └─► T-404 (步长决策) ─┐
T-402 (参数矩阵) ─┬──────┤
T-403 (架构评审) ─┴─► T-405 (构建) ─► T-406 (实时性) ─► T-407 (接口映射)
```

## Verification Checklist

- [ ] Requirements IDs are referenced by tasks.（FR-4xx 为占位，requirements 定稿后回填核对）
- [ ] Design testing strategy is represented by tasks.（design.md 待定稿）
- [ ] Each task has a clear validation command or manual check.（草稿级已给，定稿时细化为命令）
- [ ] Rollback/migration tasks are included when applicable.（Model D 冻结即回退保证；无需迁移任务）

## Execution Notes

- **本文件处于 `tasks-draft`：在用户与本地模型讨论定稿并明确批准之前，禁止实施任何任务。**
- 定稿路径：开放问题逐项回答 → requirements.md 定稿（用户批准）→ design.md 定稿
  （用户批准）→ 本文件修订并升 `tasks-approved` → `spec_validate(phase:"implementation")` 通过后才开工。
- 讨论期间的修改纪律：每次讨论后更新本文件并在文末追加变更记录（日期 + 讨论结论）。

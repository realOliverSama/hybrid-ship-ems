# Phase 4-A1 — Model E 建模与参数审计 Requirements

Spec: `phase4-a1-model-e-audit`  
Status: requirements-approved  
Created: 2026-09-03  
Brainstorm: `./brainstorm.md`

## Overview

Phase 4（系统优化重构，`Phase4` 分支）工作包 A「Model E 船舶物理模型升级」的第一段（审计与设计决策段）。
本 spec 覆盖路线文档 `docs/phase4_research_roadmap.md` §2 中的 **T-401（Model D → Model E 建模审计）**
与 **T-402（参数真实性重新审计）**，为**纯分析、零施工**任务：

- 不修改 Model D 任何内容；
- 不创建 Model E 工作区（属 A2）；
- 产出两份审计报告 + 一张全参数 CSV 总表，作为后续 A2 spec（T-403~T-412 施工）的
  Fixed Decisions 输入。

用户将以 CSV 回填字段的形式，分批提供真实参数典型值与对应文献；A1 的完成不要求回填完成，
只要求分类与字段体系完整、可用。

## Scope

### In Scope

- T-401：Model D 全部顶层 subsystem 的逐项审计（处置决策 + Physical Fidelity +
  Real-Time Cost 双等级 + 理由），含 Model E 需新增的 ADD 项清单；
- T-406 螺旋桨分支预收敛：资料可用性评估 → 明确二选一结论（维持现模型+敏感性验证 /
  升级 J-K_T/K_Q LUT），写入架构审计报告，作为 A2 的 Fixed Decision；
- T-402：全参数 CSV 总表（来源五级分类 + 影响等级 + 用户回填字段）+
  参数审计摘要报告（重点分析哪些假设最可能影响论文结论）；
- 机器可读完整性 Gate 脚本（校验覆盖率与字段完备性，输出 gate JSON）；
- 用户评审门（两份报告结论经用户批准）。

### Out of Scope

- 对 Model D 的任何修改（模型、脚本、参数、config 一律零改动）；
- Model E 工作区创建与任何施工（T-403~T-412 属后续 A2 spec）；
- 真实参数的获取与回填本身（用户分批进行，非 A1 完成条件）；
- N6 / HIL / NPU 相关任何工作（Phase 4 边界，见路线文档 §1.2）；
- 工况生成器、数据集、预测模型、EMS 控制（C/D/E/F 包）。

## User Stories

- 作为研究者，我需要一份 Model D 逐 subsystem 的处置决策清单，以便 A2 施工时
  每个模块"保留/细化/替换/新增/弃用"有据可依，不临场即兴决策。
- 作为研究者，我需要一张全参数来源分类表，以便明确哪些参数是假设值、
  哪些假设最可能影响论文结论，并能把后续查到的真实值与文献逐条回填。
- 作为论文作者，我需要螺旋桨模型路线在施工前定案，以便 A2 不会出现
  "做到一半发现资料不足"的返工。

## Functional Requirements

- **FR-401**: THE audit SHALL enumerate 100% of Model D 顶层 subsystem（以
  `model/simulink_modelD_(EMS_NN_AECMS)/` 实际模型与 builder 脚本为准清点，
  预期覆盖 Scenario_Manager / Ship_Speed_Controller / Propulsion_Plant /
  Load_Measurement / EMS_Controller / Genset / Battery / DC Bus / DC-DC /
  Fuel model / Protection 等全部顶层模块，实际清单以清点结果为准），
  并逐项给出处置决策（KEEP / REFINE / REPLACE / ADD / REJECT 五选一）、
  Physical Fidelity 等级、Real-Time Cost 等级与理由。
- **FR-402**: THE audit SHALL 包含 ADD 类清单——Model E 需要但 Model D 没有的模块
  （至少覆盖：水深/UKC 浅水效应、双发电机 DG1/DG2、发电机启停状态机、
  Battery 1RC ECM 与 SOP，与路线文档 §2 T-404/T-407/T-408/T-410/T-411 对应）。
- **FR-403**: THE audit SHALL 对螺旋桨模型给出明确二选一结论：
  (i) 资料不足 → 维持现有 F/Q 简化模型并在 A2 做参数敏感性验证；或
  (ii) 资料可用 → A2 升级为 J = V_A/(nD)、K_T(J)/K_Q(J) LUT。
  结论须写明依据（可用资料清单或缺失声明）。
- **FR-404**: THE parameter audit SHALL 导出 Model D 全部参数为一张 CSV 总表
  （来源以 `config/Model_Parameters.m` 为主，config 目录其他参数定义一并清点），
  列至少包含：参数名 / 当前值 / 单位 / 所在模块 / 现行口径
  （DRAWING/LEGACY/ASSUMED）/ 五级来源分类（图纸/厂商/文献/估计/假设）/
  影响等级 / 用户回填典型值 / 用户回填文献来源 / 备注。
- **FR-405**: 五级来源分类 SHALL 与现行 DRAWING/LEGACY/ASSUMED 口径建立显式映射规则，
  写入参数审计报告；映射不确定的参数标"待定"并在备注说明，不得猜测归类。
- **FR-406**: THE parameter audit report SHALL 包含"假设影响分析"——至少覆盖：
  船体阻力、排水量、吃水、水深、螺旋桨参数、电机效率、发电机最低稳定负荷、
  发电机启停过程、电池内阻/OCV、辅机负荷；并明确指出哪些假设最可能影响论文结论。
- **FR-407**: THE spec SHALL 提供机器可读完整性 Gate 脚本，校验：
  (a) T-401 顶层 subsystem 覆盖率 100%、每项处置决策与双等级无空缺；
  (b) T-402 CSV 全参数来源分类列无空缺（"待定"须备注非空）；
  (c) 螺旋桨分支结论存在且为 (i)/(ii) 之一；
  输出 gate JSON（`overall: true/false`），FAIL 时以非零退出码终止。
- **FR-408**: 全部产出物 SHALL 落在本工作区约定路径（建议
  `model/simulink_modelD_(EMS_NN_AECMS)/reports/` 或 spec design 阶段定稿的新路径），
  并按 AGENTS.md §20 登记到对应目录 README。
- **FR-409**: Model D 工作区 SHALL 保持零改动——审计结束后以
  `git status` + 关键文件 sha256 对照证明（审计允许新增 reports/ 下产出物，
  禁止修改任何既有文件）。

## Non-Functional Requirements

- **NFR-401**: 全部产出脚本驱动、可复现；禁止 GUI 手工状态；CSV 生成脚本可重跑幂等。
- **NFR-402**: 脚本禁止硬编码绝对路径（AGENTS.md §20.2），用 `mfilename`/`__file__`
  推导并加 pwd 兜底。
- **NFR-403**: Real-Time Cost 等级仅作信息性标注（供后续 HIL Phase 参考），
  不作为本 Phase 任何门禁或取舍依据。
- **NFR-404**: "不改假参数凑指标"红线（`docs/optimization_backlog.md` §1）继续生效：
  审计只登记与分类，不在本阶段替换任何参数值。

## Acceptance Criteria

- [ ] AC-401（对应 FR-401/402）：架构审计报告存在；完整性 Gate 对 subsystem 覆盖率
  校验 PASS（100%，无空缺）；ADD 清单覆盖 FR-402 列出的五类新模块。
- [ ] AC-402（对应 FR-403）：架构审计报告含螺旋桨分支结论，明确为 (i) 或 (ii)，
  附依据；Gate 校验该项 PASS。
- [ ] AC-403（对应 FR-404/405）：参数总表 CSV 存在；全参数来源分类无空缺；
  映射规则写入报告；Gate 校验该项 PASS。
- [ ] AC-404（对应 FR-406）：参数审计报告含十类重点假设的影响分析，并给出
  "最可能影响论文结论"的明确清单。
- [ ] AC-405（对应 FR-407）：Gate 脚本输出 `overall: true` 的 JSON；
  FAIL 路径经人为构造缺项验证过（能正确报错）。
- [ ] AC-406（对应 FR-409）：`git status` 与 sha256 对照证明 Model D 既有文件零改动。
- [ ] AC-407（用户评审门）：两份审计报告结论经用户书面批准（本文件 Status 升级即视为批准记录）。

## Edge Cases

- 参数在多处定义（如 `Model_Parameters.m` 与 `Model_Scenarios.m` 重复/派生）：
  CSV 逐行登记并在"所在模块"列标明全部出处，不得合并隐瞒；
- 运行时计算/派生参数（非常数）：纳入 CSV，当前值列标"派生"并注明计算式；
- 无法判定来源的参数：五级分类填"待定"，备注说明，Gate 视"待定+备注"为合规，
  视"待定+空备注"为 FAIL；
- 审计中发现 Model D 既有实现疑点：如实记录进报告"发现"节，**不修复**（零改动纪律）。

## Dependencies and Constraints

- 输入：`model/simulink_modelD_(EMS_NN_AECMS)/`（模型、builder、config、
  既有 `reports/parameter_audit.csv` 若存在则复用对照）、
  `docs/phase4_research_roadmap.md` §2、`docs/optimization_backlog.md`；
- 前置：brainstorm-approved（已完成，2026-09-03）；
- 后继：A2 spec（T-403~T-412 施工）以本 spec 产出为 Fixed Decisions 输入，
  本 spec 全部 AC 通过且用户批准后方可立项；
- 纪律：AGENTS.md 全部适用（脚本驱动、机器可读 Gate、§20 README 登记）。

## Open Questions

- [ ] OQ-1：参数总表 CSV 的最终列集与文件名（design 阶段定稿，FR-404 为下限要求）；
- [ ] OQ-2：产出物路径放 `reports/`（Model D 目录内新增文件）还是 Phase 4 独立目录
  （如 `docs/reports/` 或新目录）——倾向 design 阶段裁决，需权衡"Model D 目录只增不改"
  与"Phase 4 资产集中管理"；
- [ ] OQ-3：Physical Fidelity / Real-Time Cost 的分级刻度（如 高/中/低 三级还是 1~5 级），
  design 阶段定稿。

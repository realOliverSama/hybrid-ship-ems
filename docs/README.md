# docs/ — 项目文档

> **入库策略**: 入库。

存放项目设计文档、研究文档与 AI Agent 领域规则。与实现保持同步是硬要求（见 `AGENTS.md` §1）。

> 分工：`reference/` 存放可追溯的设备/厂商研究依据（入库）；`assets/` 存放文档配套网页存档；
> 体积不可控的本地原始大数据放 `data/`（不入库）——不要再把参考资料放回 `data/`。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `research_roadmap.md` | 文件 | 研究流程文档——边缘 AI 自适应 ECMS 甲醇混合动力船舶能量管理策略（研究路径、实验步骤与逻辑链条，面向导师/评审） |
| `optimization_backlog.md` | 文件 | 优化 Backlog——NN Predictive A-ECMS（Phase 2 PARTIAL 后续优化方向） |
| `data_request_list.md` | 文件 | 设备方/设计方数据索取清单（活文档，随项目演进持续更新） |
| `bsfc_anchors_sources.md` | 文件 | BSFC 双锚点来源登记（adaptive-ecms-methanol / FR-018） |
| `MathWorks_bug_report_EMChart_crossblock.md` | 文件 | MathWorks Bug Report 待提交包（EMChart 跨 block 问题，含复现代码） |
| `agent/` | 目录 | AI Agent 领域规则：`AI_MODELING.md`（MLP/LSTM、数据集、训练、模型导出）、`STM32N6_ARCHITECTURE.md`（STM32N6、FreeRTOS、固件、调试、硬件） |
| `reference/` | 目录 | 设备/厂商参考资料（入库保存的研究依据）：`20260526设备参数.md`（设备参数表，模型标定原始依据）、`bsfc_1d_concept.png`（BSFC 一维概念图）、`weichai_WH17ME_page.html`（潍柴 WH17ME 机组网页存档）；2026-08-27 自 `data/` 迁入 |
| `assets/` | 目录 | 文档配套资料存档（如 `weichai_WH17ME_20260824.html` 厂商网页存档） |

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）。

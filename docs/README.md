# docs/ — 项目文档

> **入库策略**: 入库。

存放项目设计文档、研究文档与 AI Agent 领域规则。与实现保持同步是硬要求（见 `AGENTS.md` §1）。

> 分工：`reference/` 存放可追溯的设备/厂商研究依据（入库）；`assets/` 存放文档配套网页存档；
> 体积不可控的本地原始大数据放 `data/`（不入库）——不要再把参考资料放回 `data/`。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `research_roadmap.md` | 文件 | 研究流程文档——边缘 AI 自适应 ECMS 甲醇混合动力船舶能量管理策略（研究路径、实验步骤与逻辑链条，面向导师/评审；2026-09 收官：Phase 2.5 + Phase 3 v1/v3 闭环完成） |
| `phase4_research_roadmap.md` | 文件 | Phase 4 研究路线——系统优化重构（A~F 六工作包：Model E 物理升级 / 真实控制逻辑 PMS / 工况生成器 v3 / 数据集 v3 / 新一代预测模型 / 预测式先进 EMS，T-401~T-455 backlog 级规划；含旧规划对照、分支策略与 Q1~Q5 裁决；`Phase4` 分支；新 session 启动工作包的研读入口） |
| `phase4/` | 目录 | Phase 4 各工作包文档与审计资产：`a1_model_audit/`（工作包 A 第一段审计产物：架构审计 JSON/报告、参数总表 CSV、Gate 结果与脚本） |
| `reports/` | 目录 | 导师研究总结报告工作区：`EMS01_研究总结报告_soft-HIL真闭环_20260902.docx`（Word 报告终稿，14 图 + 13 表插入正文；§2.1 为 Model D 分模块建模章节）、`generate_figures.py`（从闭环产物数据生成 8 张 300dpi 实验数据图，幂等可重跑）、`generate_model_schematic.py`（生成 Model D 顶层架构示意图，幂等可重跑）、`export_model_figures.m`（MATLAB 批量导出 Simulink 顶层/子系统结构截图，模型只读零改动，幂等可重跑）、`build_docx.py`（读 metrics JSON + 图表组装 Word 报告，幂等可重跑）、`figures/`（8 张实验数据 PNG：四臂 SOC×2 / 油耗柱状 / P_gen 对比 / 航速跟踪 / FR-508 特写 / G-P1 残差 / 阶段演进）、`figures/model/`（建模章节用图：架构示意图 + Simulink 截图 modeld_top/03/13/14/05-law5/law7/01/02/06/10） |
| `optimization_backlog.md` | 文件 | 优化 Backlog——NN Predictive A-ECMS（Phase 2 PARTIAL 后续优化方向） |
| `data_request_list.md` | 文件 | 设备方/设计方数据索取清单（活文档，随项目演进持续更新） |
| `bsfc_anchors_sources.md` | 文件 | BSFC 双锚点来源登记（adaptive-ecms-methanol / FR-018） |
| `MathWorks_bug_report_EMChart_crossblock.md` | 文件 | MathWorks Bug Report 待提交包（EMChart 跨 block 问题，含复现代码） |
| `t305_handoff_macbook.md` | 文件 | T-305 跨机交接文档——MacBook M1 接手 TCN 扫描的环境搭建/命令/硬门/STOP 条件（2026-08-29） |
| `agent/` | 目录 | AI Agent 领域规则：`AI_MODELING.md`（MLP/LSTM、数据集、训练、模型导出）、`STM32N6_ARCHITECTURE.md`（STM32N6、FreeRTOS、固件、调试、硬件） |
| `reference/` | 目录 | 设备/厂商参考资料（入库保存的研究依据）：`20260526设备参数.md`（设备参数表，模型标定原始依据）、`bsfc_1d_concept.png`（BSFC 一维概念图）、`weichai_WH17ME_page.html`（潍柴 WH17ME 机组网页存档）；2026-08-27 自 `data/` 迁入 |
| `assets/` | 目录 | 文档配套资料存档（如 `weichai_WH17ME_20260824.html` 厂商网页存档） |

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）。

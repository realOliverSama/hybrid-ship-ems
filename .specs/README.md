# .specs/ — Spec-Driven Development 工作目录

> **入库策略**: 入库（spec 是项目决策与需求的可追溯记录）。

存放项目层规格（spec）：每个子目录对应一个研究/功能特性，走 brainstorm → requirements → design → tasks
四阶段流程（见各阶段文件内的 `Status:` 行，`*-draft` → `*-approved` 逐门批准）。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `config.yaml` | 文件 | SDD 项目配置（project.goal / tech_stack / principles / hooks） |
| `adaptive-ecms-methanol/` | 目录 | 边缘 AI 自适应 ECMS 甲醇混合动力船舶 EMS（当前活跃研究主线，同名分支） |
| `adaptive-ecms-phase1/` | 目录 | Phase 1 实时 ECMS 控制器链与 DP gap 测量 |
| `nn-predictive-ecms-phase2/` | 目录 | Phase 2 NN 预测型 A-ECMS（SOC_ref 前瞻通道） |
| `nn-predictive-ecms-phase25/` | 目录 | Phase 2.5 NPU 导向模型族优化（TCN 主推）+ 数据集 v2 内河工况扩展 + 工况识别门控（models-research 分支，tasks-approved / Amendment v1.1） |
| `multistep-predictive-ems/` | 目录 | 面向参数不确定性的多步负荷预测增强型 EMS |
| `timed-feedforward-ems/` | 目录 | Model C 定时前馈 EMS（Peak + Time-to-Peak 预测调度） |
| `model-b-dc-bus/` | 目录 | Model B 直流母线电气层 + 安全/节能指标卡 + S10 工况 |

每个 spec 目录内典型文件：`brainstorm.md`（意图与方案门）、`requirements.md`（需求）、
`design.md`（设计）、`tasks.md`（任务清单与勾选），部分含 `reviews/`（评审记录）。

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）；
新增 spec 目录（即新特性立项）时必须同时在本 README 登记。

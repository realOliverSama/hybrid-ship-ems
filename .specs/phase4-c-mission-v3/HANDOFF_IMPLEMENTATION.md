# HANDOFF — Phase 4-C Mission v3 施工（新 session 开工提示词）

> 写给接手 Phase 4-C 施工的 session。本文件 + `.specs/phase4-c-mission-v3/` 四件 spec = 开工包。
> 创建：2026-09-05（四门禁全部批准，tasks-approved）。

## 0. 开工仪式（必须先做）

```bash
git fetch origin && git status -sb && git log --oneline -3
```

- 分支：`feat/phase4-c-mission-v3`（已推送 origin；本地直接 checkout 继续）；
- 工作区两个未跟踪目录（`firmware/.../Host/fault_out/`、`model/simulink_modelD_(EMS_NN_AECMS)/slprj_v1_stale/`）为既定不入库产物，**不清理、不提交**；
- 工作区若有其他未提交改动 → 停止报告用户；
- 开工先跑 `spec_validate phase: implementation` 确认 14/14 PASS。

## 1. 状态与授权

- spec 四门禁全部批准（brainstorm C-D1~C-D7+Closeout A/B/C；requirements FR-501~562 R1~R7 闭合；design DGN-01~16 R1~R9 闭合；tasks T-001~T-024 R1~R8 闭合）；
- **授权施工 T-001~T-024**；任务依赖图见 tasks.md 阶段总览与硬依赖屏障节；
- **EXT-CD-GATE**（D-owned formal C/D Interface Gate overall=true）是 spec implementation-complete 的外部前提；C 侧施工完成 = T-001~T-024 + PR，随后停。

## 2. 必读材料（按序，禁止凭记忆开工）

1. `.specs/phase4-c-mission-v3/tasks.md`（执行约束 + 硬依赖屏障 + 24 任务全文——**唯一施工事实源**）；
2. `.specs/phase4-c-mission-v3/requirements.md` + `design.md`（数值/语义裁决细节）；
3. `.specs/phase4-c-mission-v3/brainstorm.md`（C-D1~C-D7 + Closeout A/B/C 原文）；
4. D 侧机器权威：`model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_producer_schema.json`（self_hash=7993e141…）+ `dataset_v3_signal_registry.json`（9ab11dea…）；
5. 布局参考实现：`scripts/+dataset_v3/export_pilot.m`（issue-major 布局/bin 右端点语义）；
6. `model/simulink_modelE_(Phase4_Physics)/README_Model.md`（16 块结构/接口）；
7. `项目开发进展.md` 2026-09-04/05 条目（R2026a 坑清单 + D0 事故 + Phase 4-C spec 条目）。

## 3. 硬依赖屏障（违反=返工）

```text
PHASE2_ENTRY_GATE：T-008 起生成器任务必须待 T-003（架构文档用户批准）+ T-007（phase1_freeze.json）完成；
PREQUAL_PLAN_GATE：T-020 生成必须待 T-019（prequal_plan.json + qualification_stack_identity.json）；
T429_BLIND_GATE：T-022 度量计算必须待 T-021（t429_freeze.json 不可变 artifact）；
EXT-CD-GATE：spec implementation-complete 必须待 D 侧 formal C/D Gate overall=true。
```

## 4. 关键冻结点速查（细节以 requirements/design 为准）

- Model E 零改动；配置脚本只读；VOYAGE_* 注入走 assignin 白名单；PMS_SOURCE_MODE 显式设定；
- 河流坐标流语义：环境真值河流坐标生成，沿迹分量派生；往返段共享同一实现；
- 航次合格判据：min_duration_s=1200 ∧ min_canonical_windows=60 ∧ unexpected_invalid=0；
- 失败分类学六类 disposition；FAILED slot=记录→同 seed retry→继续→受控末汇 FAIL；
- seed 血统=完整 SHA256 branch-path digest（uint32 仅引擎输入，碰撞 fail-closed）；
- DEGRADED lag=1800 s 需 ≥1800 s 因果 prehistory（t 自 −1800 s 起演化）；
- hotel=航次级常量（Path B）；水深=h_chart+ΔWL（h_chart→K、ΔWL 预报→F、实际→ORACLE）；
- T-429：R_min/A_min/切片几何/探针编码在 T-021 盲冻结（冻结前禁算任何 SBD/BA 结果）；对比六项必需+混杂控制按 registry；
- strata=template×regime 9 格 ACTIVE；N_grp,min=14 unique split groups（最终义务在 T-432 链）；
- T-429 产物命名 identifiability_*，永不复用 leakage_sbd_*；
- artifact 格式消费 D 契约冻结值（.mat v7 per D-2），tasks 层不重述。

## 5. 纪律（与 D 包同款，违反=返工）

- 每任务完成即 `spec_task_check`；Gate FAIL（`error('GATE:*')`）即停升级；
- 禁绝对路径（§20.2 扫描零命中）；§20 README 两级同步；逐文件 add、禁 force-push；
- R2026a 坑清单生效（数字开头脚本 eval(fileread)、薄入口+包函数、禁 base 重跑含循环配置脚本、From Workspace Interpolate off、禁链式索引）；
- 委派授权已在 HANDOFF §5 给用户确认过：T-020 大规模生成可委派 subagent（写+独立审查 crosscheck）；其余实现任务按需委派，主 Agent 保留设计/决策/终验；
- 诚实记录：T-429 失败/NO-GO 同为正式记录内容。

## 6. 完成定义（C 侧 STOP 条件）

T-001~T-024 全勾 + AC-433/434/436~440 达成（AC-435 保持 open 待 EXT-CD-GATE）→ C→Phase4 PR 交用户审批 → **停**。D 包 formal 收尾（D-15 必改→正式 C/D Gate→T-432 activation→Stage A/B→T-435-formal→Freeze）属另一 session/立项。

## 7. 新 session 建议开局动作

1. 开工仪式（§0）；
2. 按 §2 顺序研读；
3. 从 T-001 开始施工（首批无依赖）；
4. T-003 架构文档完成后**主动提交用户批准**（AC-433 是 PHASE2_ENTRY_GATE 组成）。

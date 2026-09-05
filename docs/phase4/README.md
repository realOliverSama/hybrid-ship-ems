# docs/phase4/ — Phase 4 工作包文档与审计资产

> **入库策略**: 入库。

Phase 4（系统优化重构，`Phase4` 分支）各工作包的审计报告、分析资产与配套脚本。
总体路线见 `docs/phase4_research_roadmap.md`；各包 spec 见 `.specs/phase4-*`。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `a1_model_audit/` | 目录 | 工作包 A 第一段（spec `phase4-a1-model-e-audit`）：Model D → Model E 建模审计 + 参数真实性审计产物（架构审计 JSON/报告、参数总表 CSV、Gate 结果与脚本） |
| `a2_model_e/` | 目录 | 工作包 A 第二段（spec `phase4-a2-model-e-construction`）：Model E v1 参数证据链（值/provenance/文献 [1]~[17]/推导式 + 效率链审计 + 螺旋桨重标定推导）+ 螺旋桨 ±20% OAT 敏感性报告 |
| `b_pms_safety/` | 目录 | ★ 工作包 B（spec `phase4-b-pms-safety`，2026-09-04）：PMS v2 + Safety-Interlock 控制侧证据——三臂对比报告（R0/R5/P2）、裁决链索引、基线 v1.0→v1.1 candidate field-level diff（DD-9 Step D） |
| `d_dataset_v3/` | 目录 | ★ 工作包 D（spec `phase4-d-dataset-v3`，2026-09-04/05）：Dataset v3 契约文档——observation_contract / future_information_contract / c_producer_interface_contract / dataset_v3_schema（解释层，机器事实源为 Model E config 两 JSON）+ T-432 activation record 与 unblock checklist 模板；索引见该目录 README.md |
| `c_mission_v3/` | 目录 | ★ 工作包 C（spec `phase4-c-mission-v3`，分支 `feat/phase4-c-mission-v3`，2026-09-05）：Mission Generator v3 工程文档——T-003 架构文档（AC-433，模块边界/producer schema 十一项接口映射/无旁路边界；待用户批准）+ 后继预资格/T-429 报告归档；索引见该目录 README.md |

| `d_dataset_v3_HANDOFF.md` | 文件 | ★ 工作包 D（spec 未建 → 已建 `.specs/phase4-d-dataset-v3`，分支 `feat/phase4-d-dataset-v3`）开工提示词：Phase 4 现状 / D 包任务范围（T-430~T-435）/ 必读材料 / 挂账 / 纪律 / 顺序建议 |
| `c_mission_v3_HANDOFF.md` | 文件 | ★ 工作包 C（T-419~T-429，spec 未建）开工提示词（2026-09-05，D0 完成后）：D 冻结契约输入 / 任务范围 / 契约红线 / brainstorm 必裁清单 / 完成定义 |

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）。

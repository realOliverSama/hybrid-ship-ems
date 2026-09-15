# phase4-f-practical-predictive-ems

> **入库策略**: 入库（spec）。

Phase 4-F **修订路线**：在固定实用预测式监督控制器 C* 下，比较 C0 持续预测 /
C1 合法 30×10 s 真值 / C2 冻结 REAT-T q50 的 **实用 Model-E 控制收益**。

C1 仅称 `PERFECT_INFORMATION_CONTROLLER_REFERENCE`。禁止理论上限措辞。

| 条目 | 说明 |
|---|---|
| `brainstorm.md` | **brainstorm-approved**（Gate APPROVED WITH REQUIRED REVISIONS，R1–R10 已落地） |
| `requirements.md` | **requirements-draft**（Requirements Gate 待审） |
| `reuse_audit.md` | 复用矩阵（含 `st_edge_ai_upload/` OUT_OF_SCOPE_…_DO_NOT_TOUCH） |
| `architecture.md` | P0/C0/C1/C2 + ForecastWindow 草图 |
| `scientific_roadmap_amendment_v1.md` | 路线图修订（brainstorm-approved） |

前置：T-445 **CLOSED `GATE_VALIDITY_FAIL`**。

无 `design.md` / `tasks.md`。Requirements Gate 通过前不得施工、不得跑正式 C0/C1/C2。

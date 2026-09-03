# docs/phase4/a1_model_audit/ — Phase 4-A1 Model E 建模与参数审计

> **入库策略**: 入库。

Spec：`.specs/phase4-a1-model-e-audit/`（brainstorm ✅ / requirements ✅ / design ✅ / tasks ✅）。
纯审计零施工：Model D 工作区只读（含零新增），全部产物集中在本目录。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `architecture_audit.json` | 文件 | 架构审计单一事实源（手工撰写）：subsystem 清单 + 处置决策（KEEP/REFINE/REPLACE/ADD/REJECT）+ Physical Fidelity / Real-Time Cost 双等级 + ADD 清单 + 螺旋桨分支结论 |
| `modelE_parameter_audit.csv` | 文件 | 全参数总表（十列：parameter/current_value/unit/module/legacy_caliber/source_class5/impact/user_value/user_reference/notes）；脚本生成 + 人工复核 + 用户回填（user_value/user_reference 由用户分批填写） |
| `modelE_architecture_audit.md` | 文件 | 架构审计报告（render_audit_reports.py 渲染，幂等可重跑） |
| `modelE_parameter_audit.md` | 文件 | 参数审计报告（渲染摘要 + 手工"假设影响分析"保留块） |
| `gate_result_a1.json` | 文件 | 完整性 Gate 输出（gate_a1_completeness.py 生成，overall=true 为通过） |
| `scripts/` | 目录 | `build_parameter_table.py`（参数提取/合并→CSV）、`render_audit_reports.py`（JSON/CSV→MD）、`gate_a1_completeness.py`（完整性 Gate G1~G5） |

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）。

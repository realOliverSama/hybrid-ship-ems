# docs/phase4/d_dataset_v3/ — Phase 4-D Dataset v3 契约与激活文档

> **入库策略**: 入库。

## 用途

Phase 4-D（Dataset v3，spec `.specs/phase4-d-dataset-v3`）的解释层契约文档与
T-432 激活设施。**机器事实源**（schema/registry 两 JSON）在
`model/simulink_modelE_(Phase4_Physics)/config/`；本目录 Markdown 仅作解释层，
冲突时以机器契约为准（FR-465）。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `observation_contract.md` | 文件 | T-002 观测变量契约：H1/H2/H3 三层分类 + 部署可获得性论证 + aggregation 语义（time_mean trapezoidal 等）+ H3→storage role 映射 + ctrl_mode zero-fill（DGN-06） |
| `future_information_contract.md` | 文件 | T-004 未来信息契约：K/F/U 认识论分类 + issue-time causality x(t₀+k\|t₀) + 双索引存储 + forecast provenance 六字段 + pilot causal fixtures（DGN-13） |
| `c_producer_interface_contract.md` | 文件 | T-004 D→C producer 接口契约（C 包开工输入）：十一项能力 + formal 航次合格判据义务（数值 activation 预注册，不预填）+ C/D Gate 十项 + reopening 纪律 |
| `dataset_v3_schema.md` | 文件 | T-004/T-010 schema 文档：voyage artifact 双表 + 八 role + 窗口几何 + target/E300/predictive quantile 契约 + Dataset views + manifest 字段 |
| `t432_activation_record.md` | 文件 | T-432 激活记录**模板**（activation 时填值；不预填任何 activation 数值） |
| `t432_unblock_checklist.json` | 文件 | FR-498 八项 Unblock Checklist 机器化模板（D0 交付时全 RED） |

## 维护规则

新增/移动/重命名/删除本目录内任何文件，必须同步更新本 README（`AGENTS.md` §20）。

# docs/phase4/a2_model_e/ — Phase 4-A2 Model E 施工段归档（§20 索引）

本目录归档 Phase 4-A2（Model E 施工段，T-403~T-412）的参数证据链与验证报告。
对应 spec：`.specs/phase4-a2-model-e-construction/`；实施分支：`feat/phase4-a2-model-e`。

入库策略：**入库**（精选证据链与机器可读报告）。

## 文件清单

| 文件 | 内容 | 状态 |
|---|---|---|
| `README.md` | 本索引（§20） | ✅ |
| `modelE_parameter_evidence.md` | Model E v1 参数证据链：值/provenance/文献编号 [1]~[17]/推导公式；效率链审计；螺旋桨重标定推导；已知局限登记 | 🚧 施工中（骨架+参数表已落，待三链集成后补效率链审计终稿） |
| `prop_sensitivity_report.md` | 螺旋桨 ±20% OAT 敏感性报告（41 号脚本渲染 + 结论稳健域） | ⏳ 待 T-011 |

## 参数证据链纪律（引用规则，论文写作必须遵守）

1. **provenance 五级 + 继承**：RULED_USER（用户裁决）/ DRAWING / VENDOR / LITERATURE_NOMINAL / LITERATURE_RANGE / DERIVED / INHERITED_LEGACY / ASSUMED——全部参数在 `config/ModelE_Physics_Parameters.m` 逐条标记；
2. **推导值 ≠ 文献原值**：引用文献后必须给出自己的计算公式，写"根据文献[n]按公式××推导"，禁止写成"文献[n]给出该值"（适用于 53.45 m 几何、pack 级 LUT、1823 A、0.6 kg/start 等全部推导量）；
3. **500 t representative hull 声明**：`target_vessel_confirmed=false`，几何为按 Δ=500 t 守恒构造的相似缩尺推导值，任何文档不得表述为目标船实船几何；
4. **LFP 为 literature surrogate chemistry**：项目资料未确认实船化学体系，不得表述为"目标船采用 LFP"。

## 与开发日志的关系

参数集裁决链（Q1~Q5 + C_B）与 17 项参考文献（GB/T 7714 + 链接）以
`项目开发进展.md` 2026-09-04 条目为准；本目录的参数证据链文档是其工程化落地。
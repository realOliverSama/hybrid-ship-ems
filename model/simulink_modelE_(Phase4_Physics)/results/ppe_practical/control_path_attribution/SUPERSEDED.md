# SUPERSEDED NOTICE — CONTROL_PATH_ATTRIBUTION_REPORT.md

本目录 v1 报告（`CONTROL_PATH_ATTRIBUTION_REPORT.md` / `control_path_attribution.json`，
task `CONTROL_PATH_INVARIANCE_ATTRIBUTION`，2026-09-18T04:52Z）**保持原样不修改**（证据不可变），
但其两项结论已由 **`CONTROL_PATH_FUNNEL_CLOSURE_REPORT.md` + `control_path_funnel_closure.json`**
（task `CONTROL_PATH_FUNNEL_CLOSURE`）逐点重算后**撤回**：

| v1 结论 | 逐点重算结果 |
|---|---|
| "λ_eff = max(0.425+30(0.6−SOC)+K_S·e_hat+δλ, 0.43)；**28.0%** 的晚期决策点被 0.43 下限吸收" | **公式误读**：δλ 与 e_hat 被双重计入。实际 `λ_eff = max(0.425 + 30(0.6 + (1/30)·δλ − SOC), 0.43)`；SOC∈[0.593,0.601] ⇒ δλ→λ 的 floor 吸收点数 = **0**（0/948、0/1210、0/603） |
| "主机制 = `ACTION_QUANTIZATION_OR_POLICY_INSENSITIVITY`；正式零结果 = `VALID_NEGATIVE_ARCHITECTURAL_RESULT`" | 主机制 = **`DOWNSTREAM_OVERRIDE_OR_SATURATION`**（L6/L7 PMS 源选择器丢弃 EMS 输出）；正式零结果 **不能** 维持 `VALID_NEGATIVE_ARCHITECTURAL_RESULT`（逐点 L5 重建证明动作在 686 个 instant 上不同，而实际功率逐位相同） |
| "C1 vs C2 有 8.3% 决策点窗哈希偶合" | 该 1,436 点全部是 **双方同时处于 plan `tail` ZOH hold**，非真实窗偶合 |
| "L4→L5 为首个差异消失层" | 差异消失层在 **L6/L7**（源选择器），L5 的量化吸收（262/551/468 点）真实存在但不是零效应主因 |

定量判定与证据链见新报告的 §5（两个已落盘 PMS 内部通道：`P_gen_unserved_kW` 与
`P_gen_actual_kW` 对四个参考值假设的判别；基线需求律 98.88–99.02% 逐点命中，
替代假设误差量级 563–710 kW）。

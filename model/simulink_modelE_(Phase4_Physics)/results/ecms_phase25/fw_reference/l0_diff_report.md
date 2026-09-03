# T-504 L0 参考轨迹差分报告（host gcc ems_ctrl vs Simulink law=7 G2）

- 门禁（design 固定）：≥99% tick 同网格点（20 kW）且 max |Δ| ≤ 20 kW
- 编译：`gcc -std=c11 -Wall -Wextra -ffp-contract=off`（O2/O0 双构型，零警告）

## G2_S20_composite_voyage_matrix

| 指标 | 值 |
|---|---|
| tick 数 | 5521 |
| 逐位一致（bit-exact） | 5521（100.0000%） |
| 同网格点 | 5521（100.0000%） |
| max abs(dP) | 0 kW |
| held/not-ok tick | 0 / 0 |
| 诊断 e_hat 最大abs差 | 0.0131 |
| 诊断 K/cls_eff 不一致拍 | 0 / 0 |
| -O0 vs -O2 P 不一致拍 | 0 |
| 偏差 tick 清单 | model/simulink_modelD_(EMS_NN_AECMS)/results/ecms_phase25/fw_reference/l0_deviation_G2_S20_composite_voyage_matrix.csv（0 条） |
| **门禁** | **PASS** |

## G2_test_voyage_002_v2

| 指标 | 值 |
|---|---|
| tick 数 | 4212 |
| 逐位一致（bit-exact） | 4212（100.0000%） |
| 同网格点 | 4212（100.0000%） |
| max abs(dP) | 0 kW |
| held/not-ok tick | 0 / 0 |
| 诊断 e_hat 最大abs差 | 0.0131 |
| 诊断 K/cls_eff 不一致拍 | 0 / 0 |
| -O0 vs -O2 P 不一致拍 | 0 |
| 偏差 tick 清单 | model/simulink_modelD_(EMS_NN_AECMS)/results/ecms_phase25/fw_reference/l0_deviation_G2_test_voyage_002_v2.csv（0 条） |
| **门禁** | **PASS** |

诊断发现（e_hat/q50/q90 常量偏移）: 已确认：q50/q90/e_hat 遥测差异为常量偏移（ref−fw: q50=0.268961840, q90=0.272098251 kWh, q10=0，多 tick 仅浮点末位抖动），与 EML 行向量 b3 隐式扩张预测 (b3(1)-b3(j))*YStd 逐项吻合——chart 实际语义为 三分位数共用偏置 b3(1)（Ehat_q 线性索引取 [3x3] 第 1 列），而 T-503 C 为逐头偏置（设计意图）。P_gen_ref 不受影响（bit-exact）。

总体判定: **PASS**

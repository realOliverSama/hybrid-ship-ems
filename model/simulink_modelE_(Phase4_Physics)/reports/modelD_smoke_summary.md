# Model E 烟雾测试汇总（文件名沿 legacy 命名 modelD_smoke_summary.md）

| 测试 | V_min [V] | ΔU_max [V] | t_rec_masked [s] | 稳态越限(masked) [s] | 瞬态越限(全程) [s] | 结论 |
|---|---|---|---|---|---|---|
| T01C_S01_still | 672.4 | 77.6 | 0.00 | 0.0 | 0.0 | true |
| T04C_S04_res_inc | 672.4 | 77.6 | 0.00 | 0.0 | 0.0 | true |
| T10C_S10_current_reversal | 672.4 | 77.6 | 0.00 | 0.0 | 0.0 | true |

## T-005 稳态 eligibility mask 证据（DGN-14，阈值不变）

| 测试 | eligible/total | 豁免窗数 | 豁免窗起止 [s] | V 域 [V] | 暂态最大偏差 [V] | 稳态最大偏差 [V] | t_rec_masked [s] | 结论 |
|---|---|---|---|---|---|---|---|---|
| T01C_S01_still | 16246/28247 | 2 | [60.01, 67.20] [127.21, 282.46] | [672, 757] V | 2.6 | 0.0 | 0.00 | true |
| T04C_S04_res_inc | 34015/46016 | 2 | [60.01, 67.20] [127.21, 460.15] | [672, 757] V | 2.6 | 0.0 | 0.00 | true |
| T10C_S10_current_reversal | 72256/90257 | 3 | [60.01, 67.20] [127.21, 684.39] [744.40, 902.56] | [672, 776] V | 2.6 | 0.0 | 0.00 | true |

CCS 2026 判据：稳态带 [675, 795] V，瞬态带 [600, 802.5] V，t_recovery ≤ 1.5 s（阈值不变）。
mask 规则（预声明，DGN-14）：t>60 s ∧ v_ref 30 s 无阶跃（容差 0.10 m/s/采样）∧ 无注入激活 ∧ relevant DG 稳态（机组∈{OFF,ON} ∧ settling 30 s 无迁移 ∧ N_dispatch 与 ON 数一致）；fail-closed：eligible_sample_count==0 ⇒ FAIL。
总体: true

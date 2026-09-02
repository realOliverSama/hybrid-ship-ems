# EMS01 v3 闭环四臂两场景统一指标汇总（T-615）

- spec: ems01-chil-v3-closedloop (FR-610/FR-611/AC-605)
- generated_utc: 2026-09-02T06:23:37Z
- script: `Host/tools/closedloop_metrics.py`（纯 stdlib，相对路径，只读输入）
- 机器可读全文：`closedloop_metrics.json`（本文件为其人类可读摘要）

## §0 常量与来源（禁新造公式，design §8.1）

| 常量 | 值 | 来源 |
|---|---|---|
| BatCap (Bat_Cap_kWh) | 1071.36 kWh | Bat_Cap_kWh，model 仓库 scripts/21_run_v3_native_arms.m:94（64:239 同式同常数） |
| K_EQ | 0.4774 kg/kWh | model 仓库 scripts/21_run_v3_native_arms.m:95（64 同口径） |
| SOC 参考（fuel_corr） | 0.6 | design §8.1 fuel_corr 式（64/66 同式） |
| P_gen 网格步 | 20.0 kW | Host/ems_inject.py GRID_STEP_KW（design 固定 20 kW） |
| SOC 降额起始 | 0.25 | ASSUMED_SOC_DisDerate_Start，config/Model_Parameters.m:123 |
| SOC 公平带 | ±0.02 | design §8.2 |

## §1 四臂 × 两场景核心指标（fuel_corr = fuel_raw + (0.60−SOC_end)×BatCap×K_EQ）

### 场景 S20

| 臂 | fuel_raw [kg] | fuel_corr [kg] | SOC_end | v_track_rmse [m/s] | ê 生效占比 | SOC<0.25 事件/时长 | n_undervoltage |
|---|---|---|---|---|---|---|---|
| (a) NPU 板载闭环 | 328.488669 | 328.413570 | 0.60014683 | 0.351676 | 0.902554 | 0 / 0.0 s | 3 |
| (b) REF 板载闭环 | 328.114131 | 328.144147 | 0.59994131 | 0.351676 | 0.902011 | 0 / 0.0 s | 3 |
| (b′) 原生 law=7 | 328.094642 | 328.125012 | 0.59994062 | 0.351674 | 0.902011 | 0 / 0.0 s | null |
| (c) 原生 law=5 | 330.742384 | 330.661874 | 0.60015741 | 0.351674 | 0.000000 | 0 / 0.0 s | null |

### 场景 voyage002

| 臂 | fuel_raw [kg] | fuel_corr [kg] | SOC_end | v_track_rmse [m/s] | ê 生效占比 | SOC<0.25 事件/时长 | n_undervoltage |
|---|---|---|---|---|---|---|---|
| (a) NPU 板载闭环 | 264.239826 | 224.114829 | 0.67845076 | 0.291286 | 1.000000 | 0 / 0.0 s | 3 |
| (b) REF 板载闭环 | 264.225321 | 224.078252 | 0.67849392 | 0.291286 | 0.999288 | 0 / 0.0 s | 3 |
| (b′) 原生 law=7 | 264.216856 | 224.086249 | 0.67846173 | 0.291283 | 0.999288 | 0 / 0.0 s | null |
| (c) 原生 law=5 | 239.318043 | 239.821607 | 0.59901545 | 0.291283 | 0.000000 | 0 / 0.0 s | null |

> 注：n_undervoltage 口径见 JSON 各臂 `n_undervoltage_source`（板载=flags HELD 保持拍数；原生臂 CSV 无 alarm 列记 null，交叉核对值见 §6）。

## §2 SOC 公平性判定（±0.02 带，design §8.2）

### 场景 S20

| 对比 | 用途 | Δfuel_raw [kg] | Δfuel_corr [kg] | ΔSOC_end | 判定 |
|---|---|---|---|---|---|
| a_npu_onboard vs c_native_law5 | NN 增益：(a) NPU 板载 vs (c) law=5 基线 | -2.253714 | -2.248303 | -0.00001058 | **FAIR** |
| └ 结论 | | | | | 带内：fuel_raw 差 -2.253714 kg 即主结论（SOC 公平）；次级（fuel_corr 差）= -2.248303 kg |
| b_ref_onboard vs c_native_law5 | NN 增益：(b) REF 板载 vs (c) law=5 基线 | -2.628253 | -2.517727 | -0.00021610 | **FAIR** |
| └ 结论 | | | | | 带内：fuel_raw 差 -2.628253 kg 即主结论（SOC 公平）；次级（fuel_corr 差）= -2.517727 kg |
| bp_native_law7 vs c_native_law5 | NN 增益复验：(b′) 原生 law=7 vs (c) law=5 基线 | -2.647742 | -2.536862 | -0.00021679 | **FAIR** |
| └ 结论 | | | | | 带内：fuel_raw 差 -2.647742 kg 即主结论（SOC 公平）；次级（fuel_corr 差）= -2.536862 kg |
| a_npu_onboard vs b_ref_onboard | 零混杂 NN 移植代价：(a) NPU vs (b) REF | +0.374539 | +0.269423 | +0.00020552 | **FAIR** |
| └ 结论 | | | | | 带内：fuel_raw 差 +0.374539 kg 即主结论（SOC 公平）；次级（fuel_corr 差）= +0.269423 kg |

### 场景 voyage002

| 对比 | 用途 | Δfuel_raw [kg] | Δfuel_corr [kg] | ΔSOC_end | 判定 |
|---|---|---|---|---|---|
| a_npu_onboard vs c_native_law5 | NN 增益：(a) NPU 板载 vs (c) law=5 基线 | +24.921784 | -15.706778 | +0.07943531 | **SOC-UNFAIR** |
| └ 结论 | | | | | SOC-UNFAIR：|ΔSOC_end|=0.079435 超出 ±0.02 容差带，fuel_raw 差不作主结论；次级（fuel_corr 差）= -15.706778 kg |
| b_ref_onboard vs c_native_law5 | NN 增益：(b) REF 板载 vs (c) law=5 基线 | +24.907279 | -15.743355 | +0.07947847 | **SOC-UNFAIR** |
| └ 结论 | | | | | SOC-UNFAIR：|ΔSOC_end|=0.079478 超出 ±0.02 容差带，fuel_raw 差不作主结论；次级（fuel_corr 差）= -15.743355 kg |
| bp_native_law7 vs c_native_law5 | NN 增益复验：(b′) 原生 law=7 vs (c) law=5 基线 | +24.898814 | -15.735358 | +0.07944628 | **SOC-UNFAIR** |
| └ 结论 | | | | | SOC-UNFAIR：|ΔSOC_end|=0.079446 超出 ±0.02 容差带，fuel_raw 差不作主结论；次级（fuel_corr 差）= -15.735358 kg |
| a_npu_onboard vs b_ref_onboard | 零混杂 NN 移植代价：(a) NPU vs (b) REF | +0.014505 | +0.036577 | -0.00004315 | **FAIR** |
| └ 结论 | | | | | 带内：fuel_raw 差 +0.014505 kg 即主结论（SOC 公平）；次级（fuel_corr 差）= +0.036577 kg |

## §3 仪器级①②门禁复述（FR-610，design §5；T-613 现场已核对，此处正式落 JSON）

阈值：SOC_end |Δ| ≤ 0.005；fuel_raw 相对偏差 ≤ 1%；v_ship RMSE ≤ 0.01 m/s；P_gen 网格一致率（|ΔP| ≤ 20 kW）≥ 95%；数值健康。

### 仪器级①：闭环 (b) vs gp1 replay（开环参考）

| 场景 | 指标 | 值 | 阈值 | 判定 |
|---|---|---|---|---|
| S20 | soc_end_abs_delta | 0.00000069 | <= 0.005 | PASS |
| S20 | fuel_raw_rel_delta | 0.00005940 | <= 0.01 | PASS |
| S20 | v_ship_rmse_mps | 0.00002667 | <= 0.01 | PASS |
| S20 | p_gen_grid_match_rate | 0.99601521 | >= 0.95 | PASS |
| S20 | numeric_health | True | == True | PASS |
| S20 | **overall** | | | **PASS** |
| voyage002 | soc_end_abs_delta | 0.00003219 | <= 0.005 | PASS |
| voyage002 | fuel_raw_rel_delta | 0.00003204 | <= 0.01 | PASS |
| voyage002 | v_ship_rmse_mps | 0.00002833 | <= 0.01 | PASS |
| voyage002 | p_gen_grid_match_rate | 0.98575499 | >= 0.95 | PASS |
| voyage002 | numeric_health | True | == True | PASS |
| voyage002 | **overall** | | | **PASS** |

### 仪器级②：闭环 (b) vs 原生 (b′)

| 场景 | 指标 | 值 | 阈值 | 判定 |
|---|---|---|---|---|
| S20 | soc_end_abs_delta | 0.00000069 | <= 0.005 | PASS |
| S20 | fuel_raw_rel_delta | 0.00005940 | <= 0.01 | PASS |
| S20 | v_ship_rmse_mps | 0.00002667 | <= 0.01 | PASS |
| S20 | p_gen_grid_match_rate | 0.99601521 | >= 0.95 | PASS |
| S20 | numeric_health | True | == True | PASS |
| S20 | **overall** | | | **PASS** |
| voyage002 | soc_end_abs_delta | 0.00003219 | <= 0.005 | PASS |
| voyage002 | fuel_raw_rel_delta | 0.00003204 | <= 0.01 | PASS |
| voyage002 | v_ship_rmse_mps | 0.00002833 | <= 0.01 | PASS |
| voyage002 | p_gen_grid_match_rate | 0.98575499 | >= 0.95 | PASS |
| voyage002 | numeric_health | True | == True | PASS |
| voyage002 | **overall** | | | **PASS** |

## §4 层③报告性输出（FR-612/§8.3，不作门禁）：闭环 (a) vs ref_G2 同拍 P_gen 分布

| 场景 | n_ticks | exact 率 | ±1 网格率 | max\|ΔP\| [kW]（@tick） | 超 1 网格拍（起动瞬态 tick≤20） |
|---|---|---|---|---|---|
| S20 | 5521 | 0.900380 | 0.947836 | 980.000（@1） | 288（14） |
| voyage002 | 4212 | 0.980294 | 0.986705 | 980.000（@1） | 56（17） |

> 注：闭环 (a) 自 v_ship=0 初值起动（首拍 INIT fill/HELD），开环 ref 序列无此瞬态；超 1 网格拍集中于起动段，其余为零散 λ 网格边界翻转（design §4.4 机制）。层③报告性，不作门禁。

## §5 FR-508 闭环故障注入代价（critical_section，图像=NPU）

| 场景 | focus | FALLBACK 段/拍 | 段内 Δfuel [kg] | 段内 ΔSOC | STALE/HELD/FB 拍 | 会话存活 |
|---|---|---|---|---|---|---|
| F1_nan_ticks (nan) | F1 fault_nan_ticks | 1 / 2 | +0.026667 | +0.00005186 | 0/0/2 | True |
| F2_stall_ticks (stall) | F2 fault_stall_ticks | 0 / 0 | +0.000000 | +0.00000000 | 2/2/0 | True |
| F3_garbage_frames (garbage) | F3 fault_garbage_frames | 0 / 0 | +0.000000 | +0.00000000 | 2/2/0 | True |
| F4_reset_recovery (reset) | F4 fault_reset_recovery | 1 / 1 | +0.013333 | +0.00002593 | 0/0/1 | True |

FALLBACK 汇总：2 段 / 3 拍 / Δfuel=0.040000 kg / ΔSOC=0.00007778

> 注：注入运行用 stub advancer + pty+_MockFW（STALE 阈加速 0.05s）——验证协议/flags/回退语义；段内 Δfuel 为 mock 口径，不代表 plant 保真油耗。

## §6 交叉核对（锚点）

| 核对项 | 值组 | max\|Δ\| | 容差 | 判定 |
|---|---|---|---|---|
| gp1_fuel_raw_S20_consistency | [328.094641944, 328.094641944, 328.094641944, 328.094641944] | 0.000000000000 | 0.000001000000 | PASS |
| gp1_fuel_raw_voyage002_consistency | [264.216856500, 264.216856500, 264.216856500] | 0.000000000000 | 0.000001000000 | PASS |
| native_vs_summary.V3_B_S20_native_law7.fuel_raw_kg | [328.094641944, 328.094641944] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_S20_native_law7.fuel_corr_kg | [328.125012003, 328.125012003] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_S20_native_law7.SOC_end | [0.599940622, 0.599940622] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_S20_native_law7.e_hat_active_ratio | [0.902010505, 0.902010505] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_S20_native_law7.v_track_rmse_1s_axis | [0.351674358, 0.351674358] | 0.000000000000 | null | n/a（记录差异） |
| native_vs_summary.V3_C_S20_native_law5.fuel_raw_kg | [330.742383547, 330.742383547] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_S20_native_law5.fuel_corr_kg | [330.661873765, 330.661873765] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_S20_native_law5.SOC_end | [0.600157409, 0.600157409] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_S20_native_law5.e_hat_active_ratio | [0.000000000, 0] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_S20_native_law5.v_track_rmse_1s_axis | [0.351674358, 0.351674358] | 0.000000000000 | null | n/a（记录差异） |
| native_vs_summary.V3_B_voyage002_native_law7.fuel_raw_kg | [264.216856500, 264.216856500] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_voyage002_native_law7.fuel_corr_kg | [224.086249430, 224.086249430] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_voyage002_native_law7.SOC_end | [0.678461731, 0.678461731] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_voyage002_native_law7.e_hat_active_ratio | [0.999287749, 0.999287749] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_B_voyage002_native_law7.v_track_rmse_1s_axis | [0.291283448, 0.291283448] | 0.000000000000 | null | n/a（记录差异） |
| native_vs_summary.V3_C_voyage002_native_law5.fuel_raw_kg | [239.318042634, 239.318042634] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_voyage002_native_law5.fuel_corr_kg | [239.821607263, 239.821607263] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_voyage002_native_law5.SOC_end | [0.599015451, 0.599015451] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_voyage002_native_law5.e_hat_active_ratio | [0.000000000, 0] | 0.000000000000 | 0.000000001000 | PASS |
| native_vs_summary.V3_C_voyage002_native_law5.v_track_rmse_1s_axis | [0.291283448, 0.291283448] | 0.000000000000 | null | n/a（记录差异） |

## §7 口径备注

- 四臂口径：板载 CSV 19 列（P_gen 列名 P_gen_kW，flags 位镜像 ems_proto.h：STALE=0x01/FALLBACK=0x02/CLS_BAD=0x04/HELD=0x08）；原生臂 CSV 22 列（P_gen 列名 P_gen_ref_kW，无 flags 列）；gp1 参考 CSV 13 列。均按各自表头映射。
- ê 生效占比：板载臂 = mean(K>0 ∧ flags&0x02==0)；原生 law=7 = mean(K>0)（与 native_arms_summary.json 逐位一致）；law=5 恒 0（CSV K=1 为中性值，不据其计算）。
- v_track_rmse 为全任务 1 s 轴口径（各臂自体 v_ship vs v_cmd，design §8.1 定义式）；原生臂另附 native_arms_summary.json 的 0.01 s 全分辨率主口径与 1 s 网格辅助口径交叉核对。
- fuel_corr 电荷维持修正：64/66 同式同常数（design §8.1 禁新造公式）；每臂同时报告 fuel_raw 与 fuel_corr + SOC_end。
- 仪器级①参考 = gp1 replay（其 P_gen_ref_kW 为 ref_G2 驱动序列，G-P1 T-602 gate fuel_rel=0）；两场景全部算齐，pass/fail 如实记录，不作退出码门禁。
- 层③为报告性输出（FR-612/§8.3），不作门禁；层② (a) 裁决登记归 T-616 报告。
- tick 对齐：全部比较按 tick 号交集对齐（板载 0..N-1 与原生/gp1 同网格）。
- 退出码 0 = 完成；仪器级①②判定仅记录于 JSON，不改变退出码。

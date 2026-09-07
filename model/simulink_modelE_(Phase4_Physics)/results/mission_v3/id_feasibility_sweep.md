# T-005 只读联合可行性矩阵扫描报告（DGN-05 Phase-1 冻结前置证据）

- 任务：T-005 / Phase 4-C Mission v3（路线映射 T-428；FR-519/520；DGN-05 R2/R3）
- 执行：writer subagent（feat/phase4-c-mission-v3），2026-09-05
- 机器可读证据：`results/mission_v3/id_feasibility_sweep.json`（schema `mission_v3_id_feasibility_sweep_v1`）
- 纪律：绝对只读——Model E 的 `.slx`、`config/ModelE_*.m`、`+model/`、既有 scripts 01~72 零改动；场景全部运行时 SCN struct 构造；本任务不改任何支撑界数值、不生成正式航次

---

## 1. 执行摘要

| 项 | 值 |
|---|---|
| 实际执行网格 | **246 点全格**（未降格）|
| 可行 / 不可行 | **163 / 83**（可行性率 66.3%）|
| 仿真失败 | **0**（246/246 sim_completed，无 NaN）|
| safety trip / SOP 饱和 / domain warning | **全部 0 次**（246 点三项判据全过）|
| 不可行唯一原因 | `speed_achieved`（83/83 全部且仅此项失败）|
| 单点 wall time | 中位 **3.66 s**，最大 18.22 s（首点含 JIT）|
| 总 wall time | **986 s（约 16.4 min）**，单 MATLAB 进程内循环 + 逐点增量落盘 |
| 降格 | **未触发**（Fixed Decision 10 阈值 5 h，实测远低于）|

**核心结论**：在本网格内，联合可行性约束**完全由推进功率 vs 水动力需求（speed_achieved）主导**；执行器/电池 SOP 与 Safety/Interlock 在 246 点中从未成为约束。候选边际界内存在 **22 个联合不可行组合**（§5），集中在 planned SOG ≥ 6.0 m/s × 高 adverse 流——**联合可行域必须耦合速度轴与流向轴**，这正是 DGN-05 R2"边际界内不代表笛卡尔组合均可行"的实测证据。

---

## 2. Interface findings（勘察结论，Required Work A）

1. **draft 与 H/T 换算**：draft 唯一事实源 `config/ModelE_Physics_Parameters.m` `Hull_T_m = 2.188 m`（Rhine86 相似缩尺 DERIVED 值）；换算 `h = H/T × 2.188`，与 T-412 E05 S31~S36 家族（`ModelE_TestScenarios.m` L241-266）逐字一致。网格 H/T {1.2, 1.35, 1.5, 2.0, 3.0} → h = {2.6256, 2.9538, 3.2820, 4.3760, 6.5640} m。ΔWL=0（h_chart 即控制量）。
2. **VOYAGE_WATER_DEPTH_M 注入机制**：`01_Operator_Interface` 内 Constant 块 `'Value','VOYAGE_WATER_DEPTH_M'`（`build_model_e.m` L213，编译期在 base 求值）；运行时经 `gate_e.reset_bench_nominal` + `model.run_scenario_sim`（内部 `assignin('base','VOYAGE_WATER_DEPTH_M', SCN.water_depth_m)`）注入——与任务书指定的复用机制一致，未发明新通道。
3. **SCN 时间模字段与 `run_scenario_sim` 调用**：`speed_profile_mode='time'` + `v_time_s/v_value_mps`（分段恒定 ZOH 查表，`CODE_VPROFILE`）+ `current_time_s/current_value_mps`（`CODE_CURRENT`，时间模分段恒定）；过渡由下游 Rate Limiter 按 `SCN.accel/decel_ramp_mps2` 平滑（既有默认 0.05 m/s²，Fixed Decision 4 的"既有默认"）。`run_scenario_sim(MDL, SCN, 0, 600)` 第四参为 StopTime 安全上限（floor 至 0.01 s 倍数）。SCN 另需全字段集（v_knots_x/v_knots_v/resistance_bands/current_knots_* 等即使时间模不用也必须存在——Constant 块编译期求值），已按 S31~S36 字段形状构造。
4. **流符号约定**：`CODE_CURRENT`/S02/S03 语义——**负值=逆水（adverse，阻力方向），正值=顺水（following）**。adverse 注入 `-U`，following 注入 `+U`，1 s 网格（601 点列向量）ZOH 常值流。
5. **PMS_SOURCE_MODE**：`ModelE_Control_Parameters.m` nominal=1（EMS_AGGREGATE）；`gate_e.reset_bench_nominal()`（40 号物理 Gate 系 bench 惯例）显式恢复 0=BASELINE_POLICY。本扫描采用 **PMS_SOURCE_MODE=0 + EMS_CTRL_LAW=0（Baseline）**（确定性 bench，与 40 号物理 Gate 家族一致；43 号 Gate 的 runOne 亦为逐 case 显式 assignin 机制）。已登记 JSON `grid.pms_source_mode=0`。
6. **SOP 饱和指标来源**：电池 SOP 四约束内联于 `14_Battery_Converter`（`+model/battery_sop_limits.m`）；已 log 信号 `battery_power_limited`（power-limit flag）与 `P_bat_actual_kW`/`P_bat_dis_avail_kW`；40 号 Gate E08 的既有提取方式 = `P_bat_bus <= P_dis_avail + 1e-6` 包络检查 + flag 语义。⚠ 勘察澄清：`gate_ctrl.saturation_events` 是 DG `start_hold_timer` 饱和（机组启动 episode 一对一判据），**不是**电池 SOP 饱和，未误用。本扫描 SOP 饱和计数 = `battery_power_limited > 0.5` 步数（另记录包络超出峰值 `sop_envelope_max_exceed_kW`，全网格 ≤ 0，未触及 SOP）。
7. **domain warning 来源**：`03_Propulsion_Plant/domain_warning` 显式模型信号（`code_hull_e`：H/T<1.2 → clamp=1.2 + warning=1，NFR-411；G2 有 `domain_warning_not_triggered_in_valid_domain` 判据）。非"以验证域覆盖代替"——信号真实存在并已逐点统计。
8. **Safety trip 来源**：`16_Safety_Interlock` 输出 `fault_latch`（[1×2] DG1/DG2 锁存）与 `safety_force_stop_req`/`safety_force_start_req`；事件计数沿用 `gate_ctrl.metrics_of` 的电平上升沿法（`events_of`）。trip 判据 = fault_latch 步数=0 且 force_stop 事件=0（force_start 计数另记证据，不计入 trip）。
9. **信号提取**：用 `gate_ctrl.get_ds`（43 号 Gate 数据基础；保留向量信号 [N×k]，而 `model.extract_logsout` 只留第 1 列，不适合 fault_latch 双列）。
10. **G2 证据块位置**：`results/gate_result.json` → `checks.G2`（G2_hull_shallow），含 `U_against_up_P_prop_up`（P_down 166.5 < P_up 486.1 kW @11 kn ±1 m/s）、`shallow_mono R_by_tier`（11 kn 六档单调零违例）、`domain_warning_not_triggered_in_valid_domain`、`full_power_speed_band_12.5_15.5_kn` value_kn=**14.6202**（即 SOG=7.522 m/s 证据）、`cruise_power_band_274_335_kW` value=298.4 kW。
11. **工程注意（实现期发现）**：(a) 包函数内 `run(config)` 只落入函数工作区，模型 Parameter 作用域在 base 求值——必须 `evalin('base', run(...))` 载入三件 config（41/70 号为脚本层 run 故无此问题）；(b) MATLAB 包嵌套子包必须 `+audits/`（实现为 `scripts/+mission_v3/+audits/feasibility_sweep.m`，任务书路径中的 `audits/` 依 MATLAB 语义加 `+`，函数全名 `mission_v3.audits.feasibility_sweep` 不变）；(c) `jsondecode` 对异构字段数组返回 cell（G2 checks 兼容处理）。

---

## 3. 网格与判据（冻结值执行声明）

- **网格**：SOG {2.0, 3.0, 4.0, 5.0, 6.0, 7.522} m/s × adverse {0, 0.5, 1.0, 1.4, 1.8, 2.0, 2.4, 2.8} m/s × H/T {1.2, 1.35, 1.5, 2.0, 3.0} 全交叉 = 240 点；following 抽测 {1.5, 1.8, 2.0} × SOG {4.0, 6.0} = 6 点。**following 的 H/T 选择**（任务书未指定，执行者裁量）：固定 **H/T=1.5**（DGN-05 R3 边界点）——deep water 对联合包线证据价值低，且 HT=1.5 下与 adverse 同点成对（1.8/2.0 精确配对），支持方向性对照。已在 JSON `grid.following_ht_ratio=1.5` 登记。
- **leg 600 s**、hotel 100 kW、SOC_init 0.60、`voyage_distance_m=40000`（S20 时间模惯例，600 s 内最大里程 4.6 km 不触发距离停车）、`EMS_CTRL_LAW=0`、`PMS_SOURCE_MODE=0`。
- **判据**（Fixed Decision 7 冻结值，未放宽）：`sim_completed` ∧ `no_safety_trip` ∧ `no_sop_saturation` ∧ `no_domain_warning` ∧ `speed_achieved`（末段 120 s 均值 ≥ 计划 − 0.1 m/s）。
- pilot 3 点（任务书 C.1 指定角点）先行实测 wall time 后转入全格；pilot 3 点即全格成员，经断点续跑机制复用（脚本支持 crash-resumable，逐点 tmp+rename 原子落盘）。

---

## 4. 结果分布

### 4.1 按 planned SOG（adverse 主轴 + following 合计）

| SOG (m/s) | 点数 | 可行 | 不可行 |
|---|---|---|---|
| 2.0 | 40 | 40 | 0 |
| 3.0 | 40 | 40 | 0 |
| 4.0 | 43 | 42 | 1（仅 `adverse=2.8, H/T=1.2` 角点）|
| 5.0 | 40 | 26 | 14（adverse ≥ 1.8 起，HT 越浅越早）|
| 6.0 | 43 | 15 | 28（adverse ≥ 1.0~1.4 起）|
| 7.522 | 40 | **0** | 40（含零流点，见 §6 机制分析）|

### 4.2 按 adverse 流幅值（30 点/档）

| adverse (m/s) | 0 | 0.5 | 1.0 | 1.4 | 1.8 | 2.0 | 2.4 | 2.8 |
|---|---|---|---|---|---|---|---|---|
| 可行 | 25 | 25 | 22 | 20 | 19 | 17 | 15 | 14 |

### 4.3 按 H/T（adverse 主轴口径；following 6 点全部落在 HT=1.5）

| H/T | 1.2 | 1.35 | 1.5 | 2.0 | 3.0 |
|---|---|---|---|---|---|
| 可行（纯 adverse 240 点）| 29 | 31 | 31 | 33 | 33 |

H/T 效应方向单调（H/T↓ → R↑ → 可行点减少）但**量级温和**；速度×流耦合才是主导约束。following 6/6 全可行（P_prop 17.8~203.2 kW，远低于 adverse 同幅值）。

### 4.4 判据失败计数（246 点）

`sim_completed`: 0，`no_safety_trip`: 0，`no_sop_saturation`: 0，`no_domain_warning`: 0，`speed_achieved`: **83**。

---

## 5. 候选界内不可行组合清单（原样上报，22 点；证据非失败）

判读口径（任务书 Fixed Decision 9）：adverse ≤ 1.8 且 H/T ≥ 1.5（following: 幅值 ≤ 1.5 且 H/T ≥ 1.5，实际 0 点）。

| # | key | SOG (m/s) | adverse (m/s) | H/T | 失败判据 |
|---|---|---|---|---|---|
| 1 | sog6.000_adverse_1.00_ht1.50 | 6.0 | 1.0 | 1.50 | speed_achieved |
| 2 | sog6.000_adverse_1.40_ht1.50 | 6.0 | 1.4 | 1.50 | speed_achieved |
| 3 | sog6.000_adverse_1.40_ht2.00 | 6.0 | 1.4 | 2.00 | speed_achieved |
| 4 | sog6.000_adverse_1.40_ht3.00 | 6.0 | 1.4 | 3.00 | speed_achieved |
| 5 | sog6.000_adverse_1.80_ht1.50 | 6.0 | 1.8 | 1.50 | speed_achieved |
| 6 | sog6.000_adverse_1.80_ht2.00 | 6.0 | 1.8 | 2.00 | speed_achieved |
| 7 | sog6.000_adverse_1.80_ht3.00 | 6.0 | 1.8 | 3.00 | speed_achieved |
| 8 | sog7.522_adverse_0.00_ht1.50 | 7.522 | 0.0 | 1.50 | speed_achieved |
| 9 | sog7.522_adverse_0.00_ht2.00 | 7.522 | 0.0 | 2.00 | speed_achieved |
| 10 | sog7.522_adverse_0.00_ht3.00 | 7.522 | 0.0 | 3.00 | speed_achieved |
| 11 | sog7.522_adverse_0.50_ht1.50 | 7.522 | 0.5 | 1.50 | speed_achieved |
| 12 | sog7.522_adverse_0.50_ht2.00 | 7.522 | 0.5 | 2.00 | speed_achieved |
| 13 | sog7.522_adverse_0.50_ht3.00 | 7.522 | 0.5 | 3.00 | speed_achieved |
| 14 | sog7.522_adverse_1.00_ht1.50 | 7.522 | 1.0 | 1.50 | speed_achieved |
| 15 | sog7.522_adverse_1.00_ht2.00 | 7.522 | 1.0 | 2.00 | speed_achieved |
| 16 | sog7.522_adverse_1.00_ht3.00 | 7.522 | 1.0 | 3.00 | speed_achieved |
| 17 | sog7.522_adverse_1.40_ht1.50 | 7.522 | 1.4 | 1.50 | speed_achieved |
| 18 | sog7.522_adverse_1.40_ht2.00 | 7.522 | 1.4 | 2.00 | speed_achieved |
| 19 | sog7.522_adverse_1.40_ht3.00 | 7.522 | 1.4 | 3.00 | speed_achieved |
| 20 | sog7.522_adverse_1.80_ht1.50 | 7.522 | 1.8 | 1.50 | speed_achieved |
| 21 | sog7.522_adverse_1.80_ht2.00 | 7.522 | 1.8 | 2.00 | speed_achieved |
| 22 | sog7.522_adverse_1.80_ht3.00 | 7.522 | 1.8 | 3.00 | speed_achieved |

（界外不可行点另见 JSON `points` 数组逐点 `feasible=false` 记录，此处不重复。）

---

## 6. SOG=7.522 零流点的机制分析（升级主 Agent 裁决事项；**已被 §11 稳态精化部分修正，见该节**）

**现象**：`(7.522, adverse=0, H/T=3.0)` 判 infeasible，sog_mean_last120 = **7.249 m/s** < 7.422 阈值——与 G2 `full_power_speed_band` value_kn=14.6202（稳态全功率极速即 7.522 m/s 量级）方向表面相悖。

**机制证据（全部来自本扫描逐点记录）**：
- P_prop_mean_last120 = **700.0 kW**（Mot_P_nom 封顶，推进全程满功率）；
- `sog_mean_all(6.860) < sog_mean_last120(7.249)`——leg 末段**仍在加速**；
- 速度余量趋势：随 H/T 减小（阻力增大）sog120 单调下降（7.249 → 6.694 @H/T=1.2），行为一致。

**根因**：扫描 SCN 按 config 默认从**静止起步**（`Ship_V_init=0`，accel ramp 0.05 m/s²），高 SOG 点的加速暂态（准平衡渐近，残余加速度 ~0.01 m/s² 量级）在 600 s leg 内不能收敛到平衡态；G2 的 14.62 kn 是**稳态全功率平衡值**（40 号 Gate bench 的更长时距下测得）。**这不是推进能力不可达，而是"站立起步 + 600 s 腿"对高 SOG 点的暂态截断**。

**对 DGN-05 的含义（证据性观察，不裁决）**：
1. 满功率下 `sog120` 随 adverse 流 **~1:1 下降**（HT=3.0 行：7.249 → 6.749 → 6.249 → ... → 4.449，V_rel ≈ 7.25 m/s 恒定）——sprint 极速对 adverse 流极其敏感；即使按平衡态口径，planned SOG=7.522 也只能容忍 adverse ≲ 0.2 m/s 量级。
2. 因此 §5 中 SOG=7.522 的 13 个界内不可行点应分为两类：(a) **平衡态不可行**（adverse ≥ 0.5：即使收敛也远低于计划值，硬证据）；(b) **暂态截断**（adverse=0 三点：平衡态大概率可达，600 s 站立起步腿无法证明）。(b) 类是否计入包线证据，取决于 mission v3 段语义（段入口是否已达速度）——属主 Agent 裁决项（候选修订路径：段入口速度注入 / 更长验证腿 / 段配对语义），本任务按 Fixed Decision 9 原样记录不处理。

---

## 7. G2 交叉对照（Fixed Decision 8）

| 对照项 | G2 参照 | 本扫描结果 | 方向一致性 |
|---|---|---|---|
| 流向功率不对称 `U_against_up_P_prop_up` | P_downstream 166.5 < P_upstream 486.1 kW（11 kn ±1 m/s）| 4/4 配对 adverse_heavier=true：SOG4/6 × flow1.8/2.0 @HT1.5，如 SOG6 f1.8：adverse 700.0 vs following 165.2 kW | **一致** |
| 浅水阻力单调 `shallow_mono_R_by_tier` | 11 kn 六档 R 单调零违例 | 6/6 个 SOG 列（adverse=0，H/T 3.0→1.2）R_hull_mean 严格单调升，如 SOG6：36005→50133 N | **一致** |
| 域内无假警 `domain_warning_not_triggered_in_valid_domain` | PASS | 246 点（含 H/T=1.2 域下限点）domain_warning 全 0 | **一致** |
| 冲刺可达性 `full_power_speed_band`（14.6202 kn）| 全功率稳态极速 14.62 kn | `(7.522,0,3.0)` 600 s 站立起步腿 sog120=7.249 m/s，speed_achieved=false | **不一致 → §6 暂态截断根因**，升级主 Agent |

---

## 8. 边界点 sanity（DGN-05 R3：H/T=1.50 / 1.35）

| 关键点 | H/T=1.35 | H/T=1.50 |
|---|---|---|
| SOG=4.0, adverse=0 | feasible，sog120=4.000，P=144.2 kW，R_shallow=6527 N | feasible，P=142.7 kW，R_shallow=5438 N |
| SOG=6.0, adverse=0 | feasible，P=487.3 kW，R_shallow=14735 N | feasible，P=482.2 kW，R_shallow=12295 N |
| SOG=4.0, adverse=1.8 | feasible，P=440.1 kW | feasible，P=435.5 kW |
| SOG=6.0, adverse=1.8 | infeasible，sog120=4.969，P=700（封顶）| infeasible，sog120=4.993，P=700（封顶）|

两边界点物理行为明确、无异常（无 warning、无 SOP、无跳机），1.35 与 1.5 之间差异温和且方向单调。H/T=1.2 域下限确认点：48 点全部 domain_warning=0（1.2 恰在 clamp 边界上、未越域），其中 29 点可行、19 点因高速×高流不可行——"1.2 域下限确认"完成，其不可行来自推进功率而非越域。

---

## 9. 降格声明

**未降格**：pilot 实测单点 wall（稳态 ~3.6-5 s）→ 246 点预估 ~16 min ≪ 5 h 阈值，按 Fixed Decision 10 执行全格。
（备注：Fixed Decision 10 降格预案中"边界细扫 = 30 点"与其枚举式 `2 H/T × 6 SOG × 3 流 = 36 点` 存在算术不一致；本次未触发降格，不影响执行，仅登记备案。）

## 10. 复现方式

```text
cd model/simulink_modelE_(Phase4_Physics)
/Applications/MATLAB_R2026a.app/bin/matlab -batch "addpath('scripts'); res = mission_v3.audits.feasibility_sweep();"
% 薄入口（等价，支持 T005_MODE base 变量）:
%   eval(fileread('scripts/75_run_mission_v3_feasibility_sweep.m'))
% 断点续跑：同名 JSON 存在时自动跳过已完成点（key = sog_dir_flow_ht）
```

产出物：`results/mission_v3/id_feasibility_sweep.json`（246 点逐点证据 + summary + g2_cross_check + grid meta）、本报告。新脚本：`scripts/+mission_v3/+audits/feasibility_sweep.m`、`scripts/75_run_mission_v3_feasibility_sweep.m`（薄入口）。

---

## 11. 稳态精化（主 Agent 裁决后的 83 点 3000 s 补测，2026-09-05）

**裁决**：G2 的 14.6202 kn（E02：站立起步、指令打满 700 kW、2200 s、末段 15% 稳态窗、**20 m 深水**）是稳态口径；凡 `speed_achieved=false ∧ P_prop_mean_last120_kW≥699` 的点用 E02 同款长腿法（3000 s）重测。原 600 s JSON 未改动（审计血统），补测落盘 `results/mission_v3/id_feasibility_sweep_steady.json`（同 schema + `refinement` 块）。

**补测执行**：程序化筛选（判据核验恰 83 点，与不可行集完全一致）；判据不变（末 120 s 均值 ≥ 计划 − 0.1 m/s）；83 点全部完成，**总 wall 2064 s**（~24-25 s/点）。

### 11.1 重测结果：零翻案

| 项 | 结果 |
|---|---|
| 翻转可行点数 | **0 / 83** |
| `sog_mean_last120` 600 s vs 3000 s | 83/83 逐点**完全一致**（至 mG 量级，差 0.000）|
| 末 120 s 速度斜率 | 83/83 ≈ 0（|slope| 最大 < 1e-5 m/s²）|
| `needs_longer_leg`（\|slope\|>0.001）| **0 点** |
| 仿真失败 / NaN | 0 / 0 |

**结论（证据性，非裁决）**：功率受限点（P_prop 顶格 700 kW）在 600 s 腿内**早已到达稳态平衡**——600 s 的 `speed_achieved=false` 判定全部是**稳态真实判定**，不存在暂态截断。§6 中"leg 末段仍在加速"的初步解读被本次直接测量（斜率≈0 + 600/3000 s 逐点相等）证伪并修正；该解读依据的 `sog_mean_all < sog_mean_last120` 差值实为腿前段（斜坡+加速段）拉低全程均值所致。

### 11.2 E02 锚点核对：**未过门（−3.627% > 0.5% 容差），按裁决停下报告**

- 锚点 `sog7.522_adverse_0.00_ht3.00` 3000 s 稳态值 = **7.2492 m/s（14.096 kn）**，对 E02 参照 7.522 m/s（14.6202 kn）偏差 **−3.627%**。
- **根因（水深口径差，方法本身已被斜率证据验证）**：E02 基于 `SCENARIOS(1)`（S01）→ `water_depth_m = WaterDepth_default_m = 20.0 m`（H/T≈9.14，**真深水渐近，Δr=0**）；而网格最深档 H/T=3.0（h=6.564 m）在本 Plant 中 `Shallow_HT_deep_limit=3.0` 为**严格大于**才归零——H/T=3.0 恰好仍带 Millward Δk 浅水附加阻力。定量自洽：G2 自身 `R_by_tier[1]`（11 kn, H/T=3.0）= 32082 N vs 纯深水 K_R·V² = 28512 N（**+12.5%**）；固定功率下 V ∝ k^(−1/3) → 7.522 × (1/1.125)^(1/3) ≈ 7.24 m/s，与实测 7.2492 吻合。
- 即：锚点差异**不是**补测方法失效，而是"E02 深水参照"与"网格最深档 H/T=3.0（非渐近）"的水深口径差。原始 600 s 扫描的 H/T 轴为任务书冻结值，本补测未越界改水深。

### 11.3 修正后分类与候选界内不可行清单

- 修正后可行/不可行分类：**与原始 600 s 扫描完全一致**（83 点重测后仍全部不可行，其余 163 点不在功率受限判据内、不受裁决影响）。
- **修正后候选界内不可行清单 = 原 §5 清单（22 点），零增减**：SOG=6.0 × adverse≥1.0~1.4（HT≥1.5）7 点 + SOG=7.522 界内 13 点 + SOG=6.0/1.0/1.5 组合等，逐点见 §5 表。
- 即：**这 22 个界内组合的不可行是稳态意义上的真实不可行**（满功率推进平衡速度低于计划值 − 0.1 m/s），对 DGN-05 联合包线是硬约束证据：planned SOG ≥ 6 m/s 的段在候选 adverse 界（≤1.8 m/s）内大面积不可行；sprint 7.522 m/s 即使零流、在本网格最深档也只可达 7.249 m/s。

### 11.4 停机声明与待决事项

按裁决第 3 条（锚点偏差 >0.5% 即停），包线相关最终判读交还主 Agent。已核事实：(a) 83 点不可行为稳态真实（零翻案）；(b) 锚点 −3.63% 差异有明确物理根因（H/T=3.0 非 Δr=0 渐近）且与 G2 自身浅水证据定量自洽。**候选后续（需主 Agent 批准，本任务未执行）**：网格外单点对照——`sog7.522_adverse_0.00_ht: 水深 20 m`（E02 同水深）跑 3000 s，若 ≈7.52 则方法对 E02 精确闭环，并可考虑将"真深水锚点"作为包线证据的补充口径。

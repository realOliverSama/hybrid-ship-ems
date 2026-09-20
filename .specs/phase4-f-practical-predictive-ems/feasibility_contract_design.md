# Phase 4-F PPE — 可行性契约修正设计（Amendment Design）

状态：**DRAFT — Amendment Design Gate 待审**。上游：amendment.md §1 追踪表、requirements.md FR-C1–C7。
所有数值出处均标注冻结参数名；无 epoch-1 结局拟合。

## D-1 信号映射（genset，已核实的 Model-E 事实）

| 语义 | 信号 | 产生层 | build_model_e 行 |
|---|---|---|---|
| DG1/DG2 实际功率 | `P_DG1_kW` / `P_DG2_kW` | 06_Genset_System | L() 1406–1407 |
| DG1/DG2 状态 | `state_DG1` / `state_DG2`（0/1/2/3/4） | 06_Genset_System | L() 1408–1409 |
| min 钳位诊断 | `below_min_DG1/2`（机组层）、`dispatch_min_clamp_active`（PMS 层） | 06/07 | L() 1410, 1377 |
| 调度参考 | `P_DG1_ref_kW` / `P_DG2_ref_kW`、`N_dispatch` | 07_PMS_v2 | L() 1373–1375 |
| 总机组出力 | `P_gen_actual_kW` | 06_Genset_System | L() 1405 |

机组模型事实（code_genset_unit_e.m）：ON 态 `P_ref` 钳位至 `[DG_P_min_kW, DG_P_rated_kW]`；
`below_min_flag=1` 当且仅当 state==3 且 ref<P_min（含 ON 入口拍）→ 机组实际功率**不可能**低于
P_min 运行（钳位保证）。故“带外违规”在分机组真值上只能来自：非 ON 态 P≠0（状态机破坏）或
钳位/模型数值缺陷——两者均为**实现正确性**问题而非运行问题。

## D-2 状态 × 约束合法性表（冻结值）

| state | 名称 | P 约束 | ramp | 备注 |
|---|---|---|---|---|
| 0 | OFF | P==0 | — | 断路器分闸 |
| 1 | STARTING | P==0 | — | 不可中断；STARTING→SYNC 由状态机冻结计时 |
| 2 | SYNC | P==0 | — | 同步期；完成→ON |
| 3 | ON | 125 ≤ P ≤ 500（钳位后） | \|dP/dt\|≤20+1e-6 kW/s（ON↔ON） | ON 入口/出口拍豁免（断路器事件）；`below_min_*`/`min_clamp_*` 走诊断 |
| 4 | COOLDOWN | P==0 | — | 最小停机驻留；cmd_start 忽略 |

## D-3 缺供信号（权威选择）

- **权威**：`P_unserved_kW`（08_Undervolt_Derate）——物理服务缺口
  （推进需求被 V_dc 降额限制而未满足的部分，`max(min(demand,Mot_P_nom)−P_prop_limit,0)`）。
- 对偶诊断：`P_gen_unserved_kW`（07 Allocator command mismatch，代码红线 b：纯诊断）。
- 降级诊断：`P_bus_residual_kW`（= P_gen×ASSUMED_eta_rect + P_bat_bus − P_load ≈ C·V·dV/dt）
  —— 仅作电气一致性监视；其常值偏置（η_rect 假设 0.97 vs 真实整流效率）记录为参数校准线索。
- 服务完备性论证：hotel 为刚性负载（模型无 hotel 卸载机制）→ 唯一可缺的服务是推进
  → 推进缺供信号即服务缺口的完备度量 ✓。
- **对偶一致性诊断（Design Gate A5 强制）**：
  `P_service_gap(t) = max(P_prop_demand(t) − P_prop_actual(t), 0)`（两路已记录信号之差，
  覆盖任何原因的推进缺口）。资格化必须证明：零真实缺供 ⇒ P_unserved=0 且
  P_service_gap=0；注入缺供 ⇒ 两者同步响应；残差偏置 ⇒ 两者皆不受影响；
  正式运行若出现 P_service_gap>0 而 P_unserved=0 的持续分歧 → `P_unserved`
  服务语义不完备 → FAIL CLOSED 上报（不得以单一信号放行）。
- epoch-1 佐证：`derate08_s = 0`（192/192）→ 降额从未激活 → `P_unserved ≡ 0` 全程成立
  → **epoch-1 无真实服务缺供**（load_shortfall 判据为假阳性，T-011A §3）。

## D-4 母线残差诊断语义

`residual(t) = P_gen_actual×0.97 + P_bat_bus − P_load_total`。
诊断输出：残差均值/积分、`ASSUMED_eta_rect` 反推（若 P_bat_bus≈0 平稳段：
η_implied = (P_load − P_bat)/P_gen）、对照 C·V·dV/dt。
该通道仅产校准线索（如 η_rect 的真实值），不进任何硬判据。

## D-5 终端 SOC 带推导（全部冻结出处）

```
0.10  ── soc_hard_bounds.soc_min（硬安全，soc_hard_bounds 判据继续执行）
0.20  ── flag_soc_low（PMS 安全互锁监视阈值，FR-446，spec 字面冻结）
0.25  ── ASSUMED_SOC_DisDerate_Start（放电 SOP 满能力下限）→ SOC_terminal_min
0.60  ── Bat_SOC_init（航次起始约定，非判据）
0.90  ── ASSUMED_SOC_ChgDerate_Start（充电 SOP 满能力上限）→ SOC_terminal_max
0.95  ── soc_hard_bounds.soc_max（硬安全）
```

### D-5b 三层 SOC 区分（Design Gate A2 裁决，独立冻结）

| 层 | 区间 | 冻结出处 | 用途 |
|---|---|---|---|
| HARD_SAFETY_SOC | [0.10, 0.95] | soc_hard_bounds（冻结） | 电池硬安全（soc_hard_bounds 判据不变） |
| NORMAL_OPERATIONAL_SOC | **[0.25, 0.90]** | 电池 SOP 降额参数 | 正常运行窗口（本设计冻结） |
| TERMINAL_POWER_AND_ENERGY_RESERVE | 单独定义（D-6） | 设计基准事件集 | 终端功率/能量储备判据 |

SOC 落于 NORMAL_OPERATIONAL 窗口内**不构成**储备充分的证明（储备由 D-6 单独判定）。

推导逻辑：
- 下限取 0.25：保证航次结束时电池具备**满放电 SOP**（应急/续航能力不受降额），
  且高于 PMS `flag_soc_low`（0.20）——不会在航次结束后立即触发低 SOC 互锁监视。
- 上限取 0.90：保证航次结束时仍具备**满充电接受能力**（下一任务能量管理自由度）。
- [0.25, 0.90] 完全由冻结的电池 SOP 降额参数导出，无 epoch-1 拟合、无任意选取。

## D-6 终端 SOP 储备定义

`terminal_SOP_reserve = PASS ⟺ P_dis_avail_kW(SOC_end) ≥ P_reserve`

- `P_dis_avail_kW(SOC)`：电池 SOP 四约束的最小值（电流/电压/SOC 降额/变换器，
  code_battery_ecm_e 内联实现），SOC 依赖（0.25 以下线性降额）。
- **`P_reserve_dis = 544 kW`、`E_reserve_dis = 6.84 kWh`、`P_reserve_chg = 820 kW`
  —— Design Gate A3/A4 裁决：由设计基准事件集 `E_design` 从冻结参数 + 正式 48 航次
  输入流统计推导（无人工启发式系数、无 epoch-1 结局拟合）**：

  | 事件 | 冻结参数依据 | 量化 |
  |---|---|---|
  | EV-1 单→双机过渡 | STARTING 2500 tick(25s) + SYNC 500 tick(5s)（冻结）；窗口内仅 DG1 可达 ≤额定 500 kW | 负载取航次 P95（最坏 902 kW） |
  | EV-2 负载阶跃 | 语料 99.9 分位逐秒步幅 49–61 kW/s > 机组 ramp 20 kW/s | 阶跃 ~544 kW（P95−中位） |
  | EV-3 突然卸载 | 机组 P_min 钳位 125×2=250 kW + ramp-down 限制 | 负载谷 82–212 kW |
  | EV-4 低载持续盈余 | 18/48 航次均值 < 250 kW | 钳位盈余充电 |

  派生：`P_reserve_dis = max(P95) − 500 = 544 kW (dynamic)`（EV-1 窗口内 DG1 钳位额定的持续缺口）；
  `E_reserve_dis = 544 kW (dynamic) × 30 s / 3600 = 3.35 kWh`（EV-2 ramp 恢复能量 ~0.03 kWh 更小）；
  `P_reserve_chg = 2×125 − 82 = 820 kW (dynamic)`（钳位盈余充电）。

  **A4 派生结果（保留检查）**：SOC=0.25 处 `P_dis_avail ≈ min(1050, V/I) ≈ 900 kW ≫ 544 kW (dynamic)`、
  带内可用能量 696 kWh ≫ 3.35 kWh → 功率/能量储备在 [0.25,0.90] 带内**自动满足**
  （记录为派生结果，保留作参数变化鲁棒性闸门，不因冗余删除）。

## D-7 epoch-2 1 Hz 轨迹工件

- 通道（25）：见 amendment.md §5.1（覆盖 FR-C1/C2/C3 全部判据所需信号 +
  C* 诊断通道 delta_lambda/forecast_source_id）。
- cadence 1 Hz 右端点；采样自 logsout（对齐规则与 extract_logsout 最近邻一致）。
- schema：MAT v7.3 struct `traj`，字段=信号名，`traj.t` 1 Hz；
  sidecar JSON：`{schema:"ppe_traj_v1", voyage_id, arm, run_id, n_samples, t0, t1,
  sha256(traj.mat), channels[25], dt:1.0}`。
- 原子写：`tmp → movefile`；写失败 ⇒ 臂失败（原子重跑）。
- 容量：0.92 MB/臂 × 192 ≈ 180 MB（数据盘余 22 GB ✓）。
- 生成方式：sim 结束后从 logsout 一次性抽取（**不改 Model-E 物理**，仅日志提取），
  与 gate_ctrl.get_ds 同层。

### D-7b 高速率硬谓词审计工件（Design Gate A6）

快量谓词（battery SOP V/I `duration_tol_s=1e-6`、genset ramp 逐 tick）在 0.01 s
原生节拍由执行器就地评估（1 Hz 采样会漏检亚秒违规），并持久化紧凑审计工件
`<arm>.audit.v1.json`：`{predicate, violation_count, max_excursion, duration_s,
first_t, last_t, extremum_value, context_state}` per component。
1 Hz 轨迹 + 审计工件齐备才算完成（requirements FR-C8.2）。
审计工件纳入 sha256 链（FR-C8.3）。

## D-8 检查器修正设计（epoch-2 生产实现，Gate 后才实施）

`feasibility_contract_v2(ds, cfg)` 替代 `t445.hard_feasibility` 于正式评估路径：
- 必需信号硬校验：P_DG1_kW/P_DG2_kW/state_DG1/state_DG2/P_unserved_kW/SOC/… 缺任一 → error（FAIL CLOSED）
- genset_limits：按 D-2 状态×约束表逐通道判定（band 分量/ramp 分量分列）
- load_shortfall：`E_short = Σ max(P_unserved,0)Δt/3600`（1 Hz 轨迹采样）；
  `P_bus_residual` 移入诊断通道
- terminal：band [0.25,0.90] + SOP 储备（R-a/R-b/R-c 按 Gate 裁决）
- 输出 schema：`hard_feasibility_v2`（items 分量细化 + 诊断通道分离）
- 旧 `t445.hard_feasibility` 保留（epoch-1 证据复现用），正式评估不再调用

## D-9 资格化矩阵映射（I 项 Q1–Q20 → 实现载体）

| Q | 载体 | 说明 |
|---|---|---|
| Q1–Q8 | `feasibility_contract_v2` 合成 ds 单测（纯 MATLAB，scripts/tests/） | 合成信号构造各状态/违规 |
| Q9–Q12 | 合成 P_unserved/residual 通道单测 | 零缺供 PASS、注入 FAIL、残差偏置不串扰 |
| Q13–Q15 | 终端判据合成单测 | 带 PASS/带 FAIL/SOP 储备 FAIL |
| Q16 | FEC 重构合成单测 | 与冻结公式逐位一致 |
| Q17–Q18 | 轨迹工件读写单测 + 中断注入 | schema/hash/原子性 |
| Q19–Q20 | 流程审计（ledger/gate 检查） | 0 密封接触、0 调参事件 |

全部 fixture 使用合成/玩具信号；无正式语料消费；无 epoch-2 结局观察。

## D-10 不变量与合规自查（K 条款）

- 分机组信号已识别 ✓；P_unserved 语义已从源码核实 ✓；终端带 [0.25,0.90] 有独立冻结出处 ✓
- P_reserve 数值 = 唯一人工决策点 ✓；SOP 储备语义可定义 ✓
- 轨迹持久化仅日志提取，不改 Model-E 物理 ✓；不改 C*/预测器科学语义 ✓
- epoch-1 零改动 ✓；无 epoch-2 正式/结果性运行 ✓

## D-12 FEC 终端估值稳健性预注册（Design Gate A7）

- 主判据：k_eq_E = 0.5457 kg/kWh（冻结，不变）。
- 预注册敏感性：0.8×（0.4366）/ 1.2×（0.6549）两侧 J_FEC 重算 + break-even k_eq
  （使成对臂 J_FEC 相等的临界 k_eq；仅诊断呈现，不进主结果）。
- 可选次要诊断：SOC/效率感知终端能量估值（基于冻结 OCV LUT 积分）——**恒为次要**，
  主结论以线性 k_eq 为准。
- 全部在观察任何 epoch-2 结局**之前**实现并冻结于资格化套件；
  禁止观察结局后选择估值规则或敏感性侧（A7 红线）。

## D-13 epoch-1 terminal_soc 定性修正（Design Gate A1）

epoch-1 terminal_soc 违约（144/192）的正确定性 =
**LEGACY_TERMINAL_ENERGY_NEUTRALITY_CONTRACT_VIOLATION**：任务完成、SOC 硬边界、
电池 SOP/V/I、安全事件四判据全 PASS → 船舶**安全完成了任务**；违约仅说明旧
电荷维持契约（±0.005 能量中性）未满足。禁止 TRUE_PHYSICAL_INFEASIBILITY 分类语言，
禁止据此声称船舶物理上无法安全完成任务。T-011A 报告 JSON 的对应分类字段以本条为准
（修正案历史版本中的旧语言由本节显式取代）。

# Phase 4-F PPE — Feasibility Contract Amendment（可行性契约修正案）

状态：**DRAFT — 待 Amendment Design Gate 人工/ChatGPT 评审**
授权：T-011A 完成后的可行性契约修正工作（formal_epoch 1 证据不可变）
关联：T-011A `model/.../results/ppe_practical/feasibility_root_cause/T011A_REPORT.{json,md}`

## 0. 摘要与范围

Epoch 1 的 192/192 正式运行全部 `COMPLETE_HARD_INFEASIBLE`。T-011A 根因分析证明三个失败谓词中
两个是**检查器语义错配**（非物理不可行），一个是**真实的边缘性能量中性违约**。本修正案：

1. 重写 genset 合法性谓词（状态感知、分机组真值、禁止均分回退）；
2. 以真实缺供信号替代母线残差监视器作为 load_shortfall 判据；
3. 废弃 `|SOC_end − SOC0| ≤ 0.005` 终端契约，改为 **终端 SOC 运行带 + 终端 SOP 储备**；
4. 规定 epoch-2 **1 Hz 正式轨迹持久化**（可审计性前提）；
5. FEC 公式与冻结参数**不变**，附估值稳健性分析要求。

Epoch 1 保持 `FORMAL_EXECUTION_VALID / PRIMARY_EFFECT_ESTIMATION_WITHHELD_DUE_UNIVERSAL_HARD_INFEASIBILITY`
不动、不重分类、不重跑、不删除。

## 1. T-011A 发现 → 修正条目追踪

| T-011A 发现 | 修正条目 | 设计落点 | 资格化测试 | epoch-2 冻结字段 |
|---|---|---|---|---|
| F1: genset_limits 字段名错配（P_DG1_kW/P_DG2_kW vs P_gen1_kW）+ 均分回退 → 假阳性 | FC-1 状态感知分机组合法性谓词 | D-1 信号映射 + D-2 状态×约束表 | Q1–Q8 | `feas.genset.state_aware_v2` |
| F2: load_shortfall 误读电容功率残差监视器（零容忍 × ~1.75kW 常值偏置） | FC-2 真实缺供谓词 + FC-3 残差降级为诊断 | D-3 缺供信号定义 + D-4 残差诊断语义 | Q9–Q12 | `feas.shortfall.unserved_v2` |
| F3: terminal_soc ±0.005 全臂一致违约（航次能量结构决定） | FC-4 终端 SOC 运行带 + SOP 储备 | D-5 带推导 + D-6 储备定义 | Q13–Q15 | `feas.terminal.band_v2` |
| F4: 逐时刻轨迹未持久化（episode 取证不可行） | FC-5 1 Hz 轨迹强制持久化 | D-7 轨迹 schema/原子写 | Q17–Q18 | `traj.v1` |
| F5: 均分分配器为冻结设计（D-B6）——修正案不得改变分配律 | 非目标（显式排除） | — | — | — |

历史注记：epoch-1 的需求/设计文件**原样保留**（不回写）；本修正案以增量形式存在，
明确标注"supersedes for formal_epoch ≥ 2 only"。

## 2. FC-1 genset 合法性（状态感知）

### 2.1 信号映射（已核实，build_model_e.m L() 清单）

| 语义 | Model-E 信号 | 来源层 |
|---|---|---|
| DG1/DG2 实际功率 | `P_DG1_kW` / `P_DG2_kW` | 06_Genset_System |
| DG1/DG2 状态 | `state_DG1` / `state_DG2`（0=OFF 1=STARTING 2=SYNC 3=ON 4=COOLDOWN） | 06_Genset_System |
| DG1/DG2 调度参考 | `P_DG1_ref_kW` / `P_DG2_ref_kW` | 07_PMS_v2 |
| min 钳位激活 | `dispatch_min_clamp_active` / `below_min_DG1/2` | 07_PMS_v2 / 06 |
| 机组数/承诺 | `N_dispatch` / `commitment_state` | 07_PMS_v2 |

禁止：任何 `P_gen_actual_kW / 2` 形式的均分推断。缺失任一必需信号 → **FAIL CLOSED**
（该航次不可判定，按未完成处理）。

### 2.2 状态 × 约束合法性表（冻结 PMS-v2 语义）

| 状态 | P 约束 | ramp 约束 | 说明 |
|---|---|---|---|
| 0 OFF | P == 0 | 不适用 | 断路器分闸 |
| 1 STARTING | P == 0 | 不适用 | 不可中断；启动计时由状态机冻结参数决定 |
| 2 SYNC | P == 0 | 不适用 | 同步期；同期完成前无功率 |
| 3 ON | `P_min − tol ≤ P_DGi ≤ P_max + tol`（机组钳位后实际值） | `|dP/dt| ≤ ramp + tol`（仅 ON↔ON 拍间；ON 入口拍与出口拍豁免——断路器事件） | 钳位由机组模型保证；`below_min_flag`/`dispatch_min_clamp_active` 为**PMS 调度诊断**（单独计数，不入硬判据） |
| 4 COOLDOWN | P == 0 | 不适用 | 最小停机驻留；cmd_start 忽略 |

（数值：P_min=125 kW、P_max=500 kW、ramp=20 kW/s，均继承冻结值；tol 在 Q-fixture 中由
数值精度实验确定，不得由 epoch-1 观测幅度推导。）

### 2.3 FC-1 形式化

对每个 DG i ∈ {1,2}、每个 1 Hz 采样 t：
- state_i(t) ∈ {0,1,2,4} ⇒ |P_DGi(t)| == 0（容差由 Q-fixture 确定）
- state_i(t) == 3 ⇒ `P_min − tol ≤ P_DGi(t) ≤ P_max + tol`
- ramp：仅当 state_i(t) == state_i(t−1) == 3 时 `|P_DGi(t) − P_DGi(t−1)|/Δt ≤ ramp + tol`
- 任一违反 → `genset_limits = FAIL`（记录违反分量：Pmin/Pmax/ramp）
- `below_min_flag`、`dispatch_min_clamp_active` 计数为**独立诊断通道**（不进硬判据）

## 3. FC-2 真实缺供谓词 / FC-3 母线残差降级

### 3.1 信号语义（已核实）

| 信号 | 层 | 语义 |
|---|---|---|
| `P_unserved_kW` | 08_Undervolt_Derate | 推进缺供：`max(min(P_prop_demand, Mot_P_nom) − P_prop_limit, 0)`，其中 P_prop_limit = derate_fn(V_dc)×Mot_P_nom —— **物理服务缺口的权威信号** |
| `P_gen_unserved_kW` | 07_PMS_v2 Allocator | 发电侧 command mismatch：`max(0, P_gen_req_eff − ΣP_ref)`；**代码红线 b：纯诊断，禁用于 Safety 判据** |
| `P_bus_residual_kW` | 09_Bus_Residual_Monitor | `P_gen×ASSUMED_eta_rect + P_bat_bus − P_load_total`；模型注释：应等于 C·V·dV/dt —— **母线电容功率一致性诊断**。降级为诊断量（不再作为 shortfall 判据） |

### 3.2 FC-2 形式化

`P_service_shortfall(t) = P_unserved_kW(t)`（权威缺供信号；对偶诊断通道 `P_gen_unserved_kW` 单独记录）。
`shortfall_energy = ∫ P_service_shortfall dt`（1 Hz 右端点采样，kWh）。
硬判据：`shortfall_energy ≤ tol_short` 且 `derate08_s ≤ tol_derate`。
`tol_short`/`tol_derate` **只能**来自：合成零缺供 fixture 的数值精度实验 + 信号/采样语义论证。
**禁止**引用 epoch-1 观测幅度（~1.9 kWh）作为容差来源。

### 3.3 FC-3 母线残差诊断（保留但降级）

`P_bus_residual_kW` 及其积分作为**电气层一致性诊断**保留（对照 C·V·dV/dt），
持续偏置（如 ASSUMED_eta_rect=0.97 与真实整流效率之差）记录为模型参数校准线索，
**不构成任何硬可行性判据**。

## 4. FC-4 终端 SOC 运行带 + SOP 储备

### 4.1 独立推导（全部取自冻结仓库参数，非拟合 epoch-1 结局）

| 边界 | 数值 | 冻结出处 | 含义 |
|---|---|---|---|
| 硬安全下限 | 0.10 | soc_hard_bounds.soc_min | 电池硬安全 |
| **放电 SOP 满能力下限** | **0.25** | ASSUMED_SOC_DisDerate_Start | SOC<0.25 放电 SOP 线性降额 |
| PMS 低 SOC 旗标 | 0.20 | flag_soc_low（FR-446，spec 字面冻结） | PMS 安全互锁“低 SOC”监视 |
| **充电 SOP 满能力上限** | **0.90** | ASSUMED_SOC_ChgDerate_Start | SOC>0.90 充电接受线性降额 |
| 硬安全上限 | 0.95 | soc_hard_bounds.soc_max | 电池硬安全 |
| 任务 convention | 0.60 | Bat_SOC_init（INHERITED_LEGACY） | 航次起始约定 |

### 4.2 建议契约（主提案 = Option B 变体）

`SOC_terminal_min = 0.25`（独立冻结：放电 SOP 满能力下限）
`SOC_terminal_max = 0.90`（独立冻结：充电 SOP 满能力上限）
`terminal_SOP_reserve`：`P_dis_avail(SOC_end) ≥ P_reserve`，其中
`P_reserve = P_hotel_voyage_median`（保证航后全船酒店/应急电气负载可持续供电）。

推导：下限 0.25 保证 voyage 结束时电池**仍具备满放电 SOP**（应急能力不受降额）；
上限 0.90 保证仍具满充电接受能力（下一航次能量管理自由度不受降额）。
三者（0.25/0.90/P_reserve=酒店负载）全部取自冻结仓库证据，无 epoch-1 拟合成分。

### 4.3 政策选项（D 项要求的人工决策点）

| 选项 | 定义 | 后果 |
|---|---|---|
| A 固定运行带 | [0.25, 0.90]，无附加储备 | 最宽松；应急储备仅由 SOP 隐式保证 |
| **B（本修正案推荐）** | [0.25, 0.90] + `P_dis_avail(SOC_end) ≥ P_hotel_voyage_median` | 航后酒店负载可无限期供电；无额外能量裕度政策 |
| C 任务续接储备 | `E_reserve(SOC_end) ≥ E_next_mission_min`（下一任务最小需求能量） | 最保守；需引入“任务续接假设”这一新工程需求（仓库无冻结先例）→ 需人工确认 |

（epoch-1 观测：48 航次 SOC_end ∈ [0.587, 0.602]，全部落在 [0.25, 0.90] 带内——
该事实**不构成本提案的依据**；提案依据仅为上表冻结 SOP 边界。）

### 4.4 FEC（E 项）

`J_FEC = fuel_raw_kg + (SOC0 − SOC_end) × Bat_Cap_kWh × k_eq_E`（k_eq_E=0.5457 kg/kWh，冻结）**不变**。
FEC 天然惩罚“以低 SOC_end 换取表观节油”，与更宽终端带**兼容**。
设计新增（次要、非调参）：终端估值稳健性分析——
SOC_end ∈ [0.25, 0.90] 全范围上 `|∂J_FEC/∂SOC_end| = Bat_Cap×k_eq_E` 为常数（线性估值），
须报告 k_eq 的适用区间假设（SOC→OCV 非线性对能量等价的影响）；
**该分析不得反馈用于控制器调参**。

## 5. FC-5 epoch-2 1 Hz 正式轨迹持久化（F 项）

### 5.1 信号清单（25 通道）

time; P_load_total_kW; P_prop_demand_kW; P_prop_actual_kW; P_unserved_kW; P_gen_unserved_kW(诊断);
P_gen_actual_kW; P_DG1_kW; P_DG2_kW; state_DG1; state_DG2; commitment_state; N_dispatch;
P_bat_bus_kW; P_bat_actual_kW; P_bat_dis_avail_kW; P_bat_chg_avail_kW; SOC; V_bat_V; I_bat_A;
V_dc_V; derate08_active; safety_hard_count; delta_lambda; forecast_source_id
（ForecastWindow 内容以 hash 引用：`window_sha256` per update，与 epoch-1 checkpoint 语义一致）

### 5.2 技术规格

- cadence：**1 Hz**（右端点采样；冻结硬谓词全部可在 1 Hz 上判定——epoch-1 缺口/带外/SOC 均为慢量）
- 单位/命名：与 logsout 信号名一致（SI，kW/V/A/s/SOC 无量纲）
- 工件命名：`<formal_dir>/<voyage_id>/<arm>.traj.v1.mat`（v7.3 → 压缩可选；附带 sidecar JSON：sha256、行数、t 范围、schema 版本、关联终端记录的 run_id）
- 原子写：临时文件 + `movefile`；**无轨迹工件的完成记录不计数为完成**（pack 幂等检查加入此条件）
- 预期大小：25×4600×8 B ≈ 0.92 MB/臂 → 192 臂 ≈ **180 MB**（未压缩）/ ~45 MB（zstd）；数据盘容量充足
- checkpoint/resume：轨迹按臂整体原子写（臂原子策略不变，epoch-1 已验证）

### 5.3 完整性

`sha256(traj.mat)` 写入终端记录（`rec.trajectory_sha256`）与 pack 日志；
同步/封存链条按现有 30 分钟同步 + T-012 流程。

## 6. 资格化测试矩阵（I 项，Q1–Q20）

| # | 测试 | fixture | 期望 |
|---|---|---|---|
| Q1 | 信号名正确性 | 合成 ds 含 P_DG1_kW/P_DG2_kW | 判定使用分机组真值 |
| Q2 | 无均分回退 | 合成 ds **不含** P_gen1_kW/P_gen2_kW 且缺任一必需信号 | FAIL CLOSED |
| Q3 | OFF 态不误判 | 合成 state=0, P=0 | PASS |
| Q4 | 合法单机低载 | 合成 state_DG1=3, P_DG1=150 kW（<250 总出力） | PASS（epoch-1 会 FAIL 的反例） |
| Q5 | 合法双机高载 | state=3/3, P_DG=300/300 kW | PASS |
| Q6 | 真实 Pmax 违规 | P_DG1=510 kW | FAIL（Pmax 分量） |
| Q7 | 真实 ramp 违规 | ON↔ON 25 kW/s | FAIL（ramp 分量） |
| Q8 | STARTING/SYNC/COOLDOWN 语义 | state=1/2/4, P≠0 | FAIL（P==0 约束）；state=1/2/4, P=0 → PASS |
| Q9 | 零缺供 | P_unserved=0 全程 | load_shortfall PASS |
| Q10 | 注入缺供 | P_unserved=50 kW × 60 s | FAIL（能量=0.83 kWh） |
| Q11 | 残差偏置不触发 | P_bus_residual=−3 kW 持续 + P_unserved=0 | PASS（FC-3 降级生效） |
| Q12 | 残差仍被独立监视 | 同 Q11 | 诊断通道记录 bias 积分值 |
| Q13 | 终端带内 + SOP 储备 PASS | SOC_end=0.60, P_dis_avail≥P_reserve | PASS |
| Q14 | 终端带外 | SOC_end=0.15 | FAIL（band） |
| Q15 | 带内但 SOP 储备不足 | SOC_end=0.26 且 P_dis_avail<P_reserve | FAIL（reserve） |
| Q16 | FEC 重构不变 | 合成 fuel/SOC 序列 | 与冻结公式逐位一致 |
| Q17 | 1 Hz 轨迹完整性/schema/hash | 合成轨迹 | schema 校验 + sha256 一致 |
| Q18 | 轨迹原子写 + resume | 中断注入 | 原子性保持；resume 后轨迹续写正确 |
| Q19 | 无密封 IID/OOD 访问 | 全 fixture 审计 | 0 接触 |
| Q20 | 无结果性调参 | 流程审计 | 0 事件 |

Q9–Q12 使用合成信号（非正式语料）；全部 fixture 不含任何 epoch-2 正式结局。

## 7. epoch-2 冻结字段增量（K 项 traceability 终点）

`feas.genset.state_aware_v2`（状态×约束表 + 信号映射 + tol）
`feas.shortfall.unserved_v2`（P_unserved 语义 + tol_short/tol_derate 推导记录）
`feas.terminal.band_v2`（[0.25,0.90] + P_reserve 定义与数值——P_reserve 待人工选定）
`traj.v1`（schema/命名/哈希/原子写）
冻结方式与 epoch-1 相同：JSON + 自哈希 + Pre-Execution Gate 重审。

## 8. 与 epoch-1 的关系（G 项）

epoch-1 保持 `FORMAL_EXECUTION_VALID / PRIMARY_EFFECT_ESTIMATION_WITHHELD_DUE_UNIVERSAL_HARD_INFEASIBILITY`。
本修正案**不回溯重分类**任何 epoch-1 记录；epoch-1 的 192 条记录与全部隔离件原样保留。
epoch-2（若获授权）为**新正式纪元**：新冻结字段 + 轨迹持久化 + 修正检查器，
语料不变（同 48 航次/同 C*/同预测器/同 forecast-source 语义/同 bootstrap 方法）。

## 9. STOP 项自查（K 条款）

- 分机组信号已识别（P_DG1_kW/P_DG2_kW，build_model_e L() 1406–1407）✓ 不触发
- P_unserved_kW/P_gen_unserved_kW 语义已从源码核实 ✓ 不触发
- 终端带 [0.25,0.90] 有独立冻结出处 ✓ 不触发；**P_reserve 数值定义需人工选定** → Design Gate 议题
- SOP 储备语义可定义（P_dis_avail(SOC_end) ≥ P_reserve）✓ 不触发
- 轨迹持久化仅需 logsout 提取+落盘（不改 Model-E 物理）✓ 不触发
- 不改 C*/预测器科学语义 ✓ 不触发

## 10. STOP

本修正案止步于 **Amendment Design Gate**。未经人工/ChatGPT gate 授权：
不实现修正版生产检查器、不改动正式 runner、不启动 epoch 2、不重跑 192 正式运行、
不做 Phase-4F 效应估计、不修改 Predictor-v3、不调 C*。

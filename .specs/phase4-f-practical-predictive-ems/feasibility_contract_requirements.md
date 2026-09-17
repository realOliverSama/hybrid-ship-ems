# Phase 4-F PPE — 可行性契约修正需求（Amendment Requirements）

状态：**DRAFT — Amendment Design Gate 待审**。上游：`feasibility_contract_amendment.md`。
范围：仅增量需求（FR-C 系列）；epoch-1 历史 FR 原样保留（本文件不回写历史语义）。
验证状态标记：[V] = 可机器测试（Q 矩阵映射见 design §6）。

## FR-C1 genset 合法性谓词（状态感知、分机组真值）

- **FR-C1.1** [V] 正式可行性评估**必须**使用分机组实际功率信号
  `P_DG1_kW`、`P_DG2_kW` 与状态信号 `state_DG1`、`state_DG2`。
  禁止以 `P_gen_actual_kW/2` 或任何均分方式推断分机组功率。
  （来源：T-011A F1——字段名错配 P_DG1_kW vs P_gen1_kW 导致均分回退假阳性。）
- **FR-C1.2** [V] 任一必需信号缺失/采样不足 → 该航次可行性判定 **FAIL CLOSED**
  （标记 `INDETERMINATE_MISSING_SIGNALS`，不产生可行/不可行结论）。
- **FR-C1.3** [V] 状态×约束表（冻结值 P_min=125 kW、P_max=500 kW、ramp=20 kW/s）：
  state ∈ {0 OFF, 1 STARTING, 2 SYNC, 4 COOLDOWN} ⇒ |P_DGi| == 0（含容差）；
  state == 3 ON ⇒ `P_min − tol ≤ P_DGi ≤ P_max + tol`；
  ramp 约束仅在相邻两拍均为 ON 时适用（断路器事件豁免）。
- **FR-C1.4** [V] `below_min_DG1/2`、`dispatch_min_clamp_active`、`below_min_flag`
  为 **PMS 调度诊断通道**：单独记录、单独统计，**不构成机组硬违规**。
- **FR-C1.5** [V] 失败记录必须标注违规分量（Pmin/Pmax/ramp-up/ramp-down）、
  违反时长、最大越界、发生时的 state/commitment。

## FR-C2 服务缺供谓词（真实缺供信号）

- **FR-C2.1** [V] 正式 load_shortfall 判据**必须**使用权威缺供信号
  `P_unserved_kW`（08_Undervolt_Derate：推进服务缺口，
  `max(min(P_prop_demand, Mot_P_nom) − P_prop_limit, 0)`）。
  **禁止**使用 `P_bus_residual_kW`（母线电容功率残差）作为缺供判据。
  （来源：T-011A F2——残差监视器语义错配 + η_rect 偏置 → 1.9 kWh 假阳性。）
- **FR-C2.2** [V] `P_bus_residual_kW` 降级为**DC 母线功率一致性诊断**：
  独立记录残差积分与 C·V·dV/dt 对照，用于电气层数值一致性检查；
  偏置（含 ASSUMED_eta_rect 误差）仅作为模型校准线索记录。
- **FR-C2.3** [V] `P_gen_unserved_kW`（PMS Allocator command mismatch）作为
  **第二诊断通道**记录；遵守代码红线 b（禁用于 Safety/硬判据）。
- **FR-C2.4** 缺供能量积分约定：1 Hz 右端点采样、
  `E_short = Σ max(P_unserved,0)·Δt/3600 (kWh)`，积分区间 `[0, T_eval]`。
- **FR-C2.5** 硬判据容差 `tol_short`、derate 容差 `tol_derate` 的推导**必须**来自：
  合成零缺供 fixture 数值精度实验 + 采样/积分语义论证。
  **禁止**引用 epoch-1 观测幅度（~1.9 kWh）作为容差或阈值依据。
- **FR-C2.6** 失败记录必须标注：总缺供电能、缺供功率峰值、缺供总时长。

## FR-C3 终端 SOC 运行带 + SOP 储备

- **FR-C3.1** 废弃 `|SOC_end − SOC0| ≤ 0.005` 作为 epoch ≥ 2 的终端契约
  （epoch-1 记录按原契约保留，不重分类）。
- **FR-C3.2** [V] 新终端契约：`SOC_terminal_min ≤ SOC_end ≤ SOC_terminal_max`，
  默认提案 **[0.25, 0.90]**（独立冻结依据：0.25 = 放电 SOP 满能力下限
  `ASSUMED_SOC_DisDerate_Start`；0.90 = 充电 SOP 满能力上限
  `ASSUMED_SOC_ChgDerate_Start`；上下限均不依赖 epoch-1 结局）。
- **FR-C3.3** [V] 附加终端储备判据：`P_dis_avail(SOC_end) ≥ P_reserve`
  （默认提案 `P_reserve = P_hotel_voyage_median`：航后酒店/应急负载可持续供电）。
  `P_reserve` 数值为 **DECISION_REQUIRED**（Design Gate 人工选定）。
- **FR-C3.4** 电池硬安全边界 [0.10, 0.95]（soc_hard_bounds）**不变**、独立于
  运行带判据继续执行。
- **FR-C3.5** 失败记录必须标注：SOC0/SOC_end/ΔSOC、带余量（signed）、
  `P_dis_avail(SOC_end)` 与 P_reserve 之差、违反分量（band/reserve）。
- **FR-C3.6** [V] `soc_hard_bounds`、电池 SOP 四约束、Safety 互锁逻辑**不变**。

## FR-C4 FEC 与终端估值稳健性

- **FR-C4.1** `J_FEC = fuel_raw_kg + (SOC0 − SOC_end) × Bat_Cap_kWh × k_eq_E`
  公式与冻结参数（k_eq_E = 0.5457 kg/kWh）**不变**（公平性机制保留）。
- **FR-C4.2** Design 必须包含终端估值稳健性分析：SOC_end ∈ [0.25, 0.90] 上
  线性估值的适用性（SOC→OCV 非线性影响）、k_eq 敏感性；
  该分析**仅作报告**，禁止用于结果观察后的控制器调参。

## FR-C5 epoch-2 1 Hz 轨迹持久化

- **FR-C5.1** [V] 每条正式臂**必须**产出 1 Hz 轨迹工件（25 通道清单见 design）；
  缺失/校验失败的轨迹 ⇒ 该臂**不计数为完成**（pack 幂等检查加入此条件）。
- **FR-C5.2** [V] 轨迹写入原子（临时文件 + rename）；附带 sidecar JSON
  （sha256、行数、t 范围、schema 版本、run_id 关联）。
- **FR-C5.3** [V] 终端记录新增 `trajectory_sha256` 字段（绑定轨迹工件）。
- **FR-C5.4** 通道清单、单位、命名、大小预算（~0.92 MB/臂，全量 ~180 MB）见 design。
- **FR-C5.5** epoch-1 无轨迹属**历史事实**：不回溯、不伪造；T-011A 报告已如实记录该限制。

## FR-C6 epoch-1 不可变性与 epoch-2 边界

- **FR-C6.1** epoch-1 全部证据、状态标签
  （FORMAL_EXECUTION_VALID / PRIMARY_EFFECT_ESTIMATION_WITHHELD_…）、隔离件
  **原样保留**；禁止按新契约重分类。
- **FR-C6.2** epoch-2（若获授权）必须使用新冻结字段
  （`feas.genset.state_aware_v2` / `feas.shortfall.unserved_v2` /
  `feas.terminal.band_v2` / `traj.v1`）并通过重审 gate；语料/语料划分/bootstrap 方法不变。
- **FR-C6.3** 科学意图不变：同一 C*，P0/C0/C1/C2 四臂对比；
  修正案只涉及可行性契约正确性与可观测性，不涉及预测价值假设。

## FR-C7 禁止事项（延续）

禁止：改 C* 以改善结局；改预测器身份；因 epoch-1 结局更换语料或 bootstrap 方法；
访问密封 IID/OOD；retune REAT-T；在资格化期间查看 epoch-2 正式结局。

# FC-3 可行性契约修正案 — 设计稿（DRAFT，待 FC-3 Design Gate 评审）

状态：**DRAFT——本文件即 FC-3 Design Gate 评审对象**。未实现检查器、未重放、未重跑。
上游：T-011B-F 归因（`results/ppe_practical/infeasibility_attribution/`，C7/C8 修正后）
+ Post-T-011B-F Interpretation Gate 裁决（C7–C9）。
证据基线：epoch-2 冻结证据包（root `8d6106ff`，192 臂-航次）。

---

## 0. 科学现状声明（不可违反）

- `PREDICTOR_V3_FORECAST_CAPABILITY = ESTABLISHED`
- `PRACTICAL_PREDICTIVE_FUEL_BENEFIT = NOT_YET_ESTIMATED`
- 禁止表述"预测不省油 / 预测 EMS 失败"；四臂不可行全部发生在 300s 分化边界之前且跨臂共同。
- 本修正案不使用任何控制器燃油结果（epoch-2 的 fuel/J_FEC 仅以
  INFEASIBLE_DIAGNOSTIC_ONLY 形式存在于原始记录，FC-3 与重放均不读取）。

## 1. FC-3-1 硬服务削减（HARD SERVICE CURTAILMENT）定义

**权威判据**：`P_unserved_kW`（08_Undervolt_Derate 路径，源码定义的"需求服务被
电气/电力系统实际削减"信号）。

- 硬失败当且仅当：`E_unserved = ∫P_unserved dt / 3600 > tol_hard`（tol_hard 沿用
  冻结值 1e-6 kWh）或 `derate08_active` 置位时长 > 0（现有语义不变）。
- **tol_hard 不从 epoch-2 观测分布（0.107%/0.378kWh/≤14kW）推导**——这些数值
  仅作为归因诊断证据保留。

## 2. FC-3-2 动态跟踪诊断（DYNAMIC TRACKING DIAGNOSTIC）定义

`tracking_gap = max(P_prop_demand − P_prop_actual, 0)`：

- **回归诊断通道**（FC-2 设计意图的恢复），单独不构成 HARD_INFEASIBLE；
- 仅当伴随**源码可证的削减/电气受限语义**时方可升级参与硬判据：
  升级条件（合取）：`tracking_gap > 0` **且** `P_unserved > 0`（或
  `derate08_active=1`，或 `P_prop_limit_kW` 类源码定义的服务限制信号激活）——
  即跟踪缺口与权威削减共现（divergence-fail-closed 语义：
  P_unserved=0 而 gap>0 ⇒ 纯跟踪动态，不硬判）。
- 诊断输出保留在记录的 diagnostics 字段（能量/时长/极值/episode 数）。

## 3. FC-3-3 PMIN 判定（派生加载态）设计

依据 C8 源码裁定（`code_genset_unit_e.m`：ON=已并网允许加载；功率链在 ON 内，
指令钳位[125,500]→20kW/s 爬坡→τ=5s 一阶滞后；爬坡期 P<P_min 为模型自身
registered 过渡）：

**派生状态（不改 Model-E，全部由不可变证据派生）**：

```
ON_LOADING  := state==3 且 (t − t_ON_entry) ≤ T_load_bound
ON_STEADY   := state==3 且 (t − t_ON_entry) >  T_load_bound
```

其中 `t_ON_entry` 由轨迹中 state 非 3→3 的跳变沿取（0.01s 原生审计 / 1Hz 轨迹
均可取沿，重放用 1Hz）。

**T_load_bound 的源码推导（非任意宽限）**：最坏情形为指令恰为 P_min 的一阶链：

```
P_lag(t) = P_cmd·(t/τ_r·u − ...)  的闭合解下界；
T_load_bound = P_min/ramp + 4τ = 125/20 + 4×5 = 26.25 s
```

（精确最坏值 ≈13.3s，取解析上界 26.25s＝爬坡满程 + 4 倍时间常数的一阶整定
惯例；**与任何观测分布无关**，仅由冻结参数 P_min/ramp/τ 决定。）

**PMIN 硬判据**：仅 `ON_STEADY ∧ P_actual < P_min − tol` 构成违规；
ON_LOADING 期间 P<P_min 记为 registered 过渡诊断（对齐模型 below_min_flag
语义）。停机方向源码已自洽（ON→COOLDOWN 当拍 P=0 且 state 同拍离 3，
不触发任何谓词）。PMAX/ramp/非ON功率合法性/电池安全/储备谓词**全部不变**。

## 4. 离线性判定与锁定重放资格

**FC-3 是否仅离线/事后裁决？——是。** 逐项核对：

| 项 | 是否改变 | 
|---|---|
| Model-E 物理 / PMS 行为 / C* 决策 / Predictor-v3 / ForecastWindow / 正式任务输入 / 物理轨迹 / FEC 定义 | **全部不变**（FC-3 只改 checker/证据逻辑） |
| 所需新信号 | 无（P_unserved、state、P_DG*、derate 均已在冻结证据与终端记录中） |

**LOCKED_REPLAY_ELIGIBLE = TRUE**

重放所需的原生分辨率量：`E_unserved`（0.01s 积分）直接取自终端记录内嵌的
checker_v2.violations（不可变原始证据）；时间/状态上下文取自 1Hz 轨迹。
稳态 PMIN 违规按性质是长时程事件（>T_load_bound 后仍持续），1Hz 足以裁定；
加载豁免为时间窗派生，1Hz 足以取沿。

## 5. FEASIBILITY_RE-ADJUDICATION_EPOCH_1（重放身份模型）

- 输入：仅 epoch-2 冻结证据包（轨迹/审计/终端记录）；**禁止读取** comparative
  fuel/J_FEC/arm 性能；
- 裁决器：新冻结的 FC-3 checker（独立 SHA）；
- 输出命名空间：`results/ppe_practical/readjudication_epoch1/`（新层，
  **不触碰、不改写 epoch-2 终端记录**——原始 HARD_INFEASIBLE 分类原样保留）；
- 每条重放输出绑定：原 run_id + 原冻结根 `8d6106ff` + 原 trajectory_sha256
  （复验）+ FC-3 checker SHA + 重放工具 SHA；
- 输出内容：仅可行性分类 + 逐谓词证据（含 ON_LOADING 派生窗、registered
  过渡诊断、跟踪诊断统计）。

## 6. FC-3 资格化矩阵（合成/非正式 fixture，Q-FC3-1..12）

| # | 测试 | 期望 |
|---|---|---|
| Q-FC3-1 | 正常推进加速、P_unserved=0 | 服务谓词**不**硬失败 |
| Q-FC3-2 | 小幅常规跟踪滞后、无电气削减 | **不**硬失败（诊断记录） |
| Q-FC3-3 | 注入真实服务削减（P_unserved>0） | 硬失败 |
| Q-FC3-4 | 欠压降载（derate08_active） | 硬失败 |
| Q-FC3-5 | 母线残差偏置 | 不影响服务可行性（FC-2 语义保持） |
| Q-FC3-6 | 并网后合法加载（ON_LOADING 窗内 P<P_min） | PMIN**不**硬失败（registered 诊断） |
| Q-FC3-7 | ON_STEADY 稳态 P<P_min | **硬失败** |
| Q-FC3-8 | PMAX/ramp/状态合法性谓词 | 与现行逐位等价 |
| Q-FC3-9 | 电池硬安全/储备谓词 | 与现行逐位等价 |
| Q-FC3-10 | FC-3 checker 不读取 fuel/J_FEC/臂间比较 | 静态+运行时断言 |
| Q-FC3-11 | 锁定重放不可改写 epoch-2 原始证据 | 哈希前后复验 |
| Q-FC3-12 | 重放输出绑定 原 run SHA + FC-3 checker SHA | 逐条校验 |

补充案例（PMIN 专项）：合法并网一阶加载 / 稳态 ON≥Pmin / 真稳态低于 Pmin /
停机过渡 / 重启动 / 双机转换——全部含于 Q-FC3-6/7/8 的参数化 fixture 族；
**不设任何源自观测结果的宽限窗**（T_load_bound 纯解析推导，§3）。

## 7. 未决事项（需 FC-3 Design Gate 裁决）

1. `T_load_bound` 取解析上界 26.25s（P_min/ramp+4τ）还是精确最坏 13.3s？
   （本稿推荐上界——更保守、与整定惯例一致、不含逐案拟合）；
2. 跟踪缺口升级硬判据的第三通道（`P_prop_limit_kW` 激活）是否纳入 v1，
   还是先仅 P_unserved/derate 双通道；
3. 重放输出是否包含"若按 FC-3 分类则为 COMPLETE_FEASIBLE/HARD_INFEASIBLE"
   的**新分类字段**（建议：是，字段名 `readjudicated_status`，原始
   `terminal_status` 并列保留）；
4. 重放是否同时输出修订版 warmup-parity 核验（不需要——parity 与可行性正交）。

## 8. 纪律

本稿为止：不实现最终 checker、不重放 epoch-2、不重跑 Simulink、不做效应分析。

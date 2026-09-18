# FC-3 可行性契约修正案 — 设计（D1–D6 已并入，**DESIGN APPROVED**）

状态：**FC-3 Design Gate：APPROVED WITH REQUIRED CORRECTIONS D1–D6（已全部并入本稿）**。
C7/C8/C9 裁定接受（episode 计数 96/192；ON=已并网允许加载；PMIN 改类
POST_SYNC_LOADING_TRANSIENT、撤销 PMS_TRANSITION_LIMITATION）。
未实现检查器、未重放、未重跑。
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

## 1. FC-3-1 硬服务削减（D2：仅代表实际削减）

**硬失败当且仅当**：`E_unserved = ∫P_unserved_kW dt / 3600 > tol_hard`
（tol_hard 沿用冻结数值语义 1e-6 kWh）。

- `P_unserved_kW` 为权威**仅因源码审查确立其含义** = 相关电气限制路径对
  推进服务的**实际削减**；
- **derate08_active 单独不构成硬服务失败**——降载状态可在需求仍被全额
  服务时存在；其角色 = **原因/上下文诊断**（除非另有已冻结的安全谓词独立
  定义该电气状态为不安全——电池 V/I 硬谓词即属后者，独立存在）；
- 不因"降载逻辑激活但服务足额"而双重计罪。

## 2. FC-3-2 动态跟踪诊断（DYNAMIC TRACKING DIAGNOSTIC）定义

`tracking_gap = max(P_prop_demand − P_prop_actual, 0)`：

- **回归诊断通道**（FC-2 设计意图的恢复），单独不构成 HARD_INFEASIBLE；
- 仅当伴随**源码可证的削减/电气受限语义**时方可升级参与硬判据：
  升级条件（合取）：`tracking_gap > 0` **且** `E_unserved > tol_hard`——
  跟踪缺口与权威削减共现（divergence-fail-closed：P_unserved=0 而 gap>0 ⇒
  纯跟踪动态，不硬判）。**D3：`P_prop_limit_kW` 不作为 FC-3 v1 硬输入**
  （该信号未纳入冻结证据包；仅为未来/次级服务限制一致性诊断的候选项，
  不得为重放制造新数据依赖）。
- 诊断输出保留在记录的 diagnostics 字段（能量/时长/极值/episode 数）。

## 3. FC-3-3 PMIN 判定（D1：源码动态最小功率准入包络）

**不使用任何固定宽限窗（13.3s/26.25s 均已废弃）。** 判据为随时间变化的下包络：

```
t_r = P_min / r = 6.25 s；  P_r = P_floor(t_r) = 53.6505 kW
0 ≤ dt ≤ t_r :  P_floor(dt) = r·[dt − τ·(1 − e^{−dt/τ})]
dt > t_r     :  P_floor(dt) = P_min + (P_r − P_min)·e^{−(dt−t_r)/τ}
合法性：state==ON 且 P_actual(dt) ≥ P_floor(dt) − ε_numeric
```

- dt = t − t_ON_entry（ON 入口为 SYNC 驻留完成时刻，此刻 P_ramp=P_lag=0）；
  指令经工厂自身钳位恒 ≥ P_min ⇒ 最小指令响应是一切合法加载轨迹的下界。
- **ε_numeric = 0.11 kW**：推导 = 半步长量化 r·dt/2（=0.10）+ Euler 全局截断
  上界（=0.00625），与 epoch-2 观测分布无关；源码逐位离散仿真对照闭合式
  最大偏差 0.107 kW ≤ ε ✓。
- 性质：ON 入口后短时低功率合法（沿冻结加载动力学）；包络指数逼近 P_min
  （30s 处 ≈124.4，39.1s 达 P_min−ε）——长期低于 P_min 仍然硬失败；
  **无任何结果驱动的固定豁免窗**。
- 完整推导（含参数、公式、验证、离散裁决语义、不确定性界）见机器可读工件
  `fc3_pmin_envelope_derivation.json`（self `45f15ea9…`）。

**1 Hz 证据离散裁决语义（重放用，前瞻定义）**：t_ON 取 1Hz 状态信号首个
state==3 采样（真值位于其前 1s 内）；**保守裁决用 dt_lower = max(0, dt̂−1)**
（P_floor 单调增 → 下取更宽 → 不会因时序量化产生假硬失败）。1Hz 时序模糊带：
dt=10s→7.46kW、15s→2.75、20s→1.01、26.25s→0.29（≈ε 量级）。**声明精度为
1Hz 而非 0.01s**（原生审计只记违规事件、不含状态跳变时戳）。
- PMAX/ramp/非ON功率合法性/电池安全/储备谓词不变；停机方向源码自洽
  （ON→COOLDOWN 当拍 P=0 且 state 同拍离 3，不触发任何谓词）。

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
- 每条重放输出**至少**包含（D4）：`original_terminal_status`、
  `readjudicated_status`（并列，**不覆写** terminal_status 或任何原始
  可行性字段）、`original_checker_sha`、`fc3_checker_sha`、
  `original_epoch2_freeze_root`、`original_run_id`、`original_trajectory_sha`、
  `original_audit_sha`、`replay_identity`；
- 输出内容：仅可行性分类 + 逐谓词证据（含动态包络逐点判定、registered
  过渡诊断、跟踪诊断统计）。
- **D5：warm-up parity 不重算**（与 FC-3 正交）——按 SHA 绑定已接受的
  epoch-2 parity 工件身份（48/48 PASS，经由 T-A9 归档工件引用）。

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
| Q-FC3-13 | 解析/源码动态 PMIN 包络与冻结机组实现在非正式 fixture 上一致 | 数值对照（含逐位 Euler 仿真） |
| Q-FC3-14 | 最小合法指令加载在全渐近过渡期内保持可准入 | 长窗 fixture |
| Q-FC3-15 | 实际功率实质低于动态下包络 → 硬失败 | 注入违规 fixture |
| Q-FC3-16 | derate 激活但推进足额 → 服务**不**硬失败 | fixture |
| Q-FC3-17 | derate + 实际未服务推进 → 服务硬失败 | fixture |
| Q-FC3-18 | FC3_REPLAY_INPUT_VIEW 无 fuel/J_FEC/性能字段 | 投影断言 |
| Q-FC3-19 | 运行时访问被禁性能字段 → fail-closed | 运行时守卫 |
| Q-FC3-20 | 锁定证据为 D1 提供足够过渡时序分辨率 | 1Hz 语义+模糊带证明 |

补充案例（PMIN 专项）：合法并网一阶加载 / 稳态 ON≥Pmin / 真稳态低于 Pmin /
停机过渡 / 重启动 / 双机转换——全部含于 Q-FC3-6/7/8 的参数化 fixture 族；
**不设任何源自观测结果的宽限窗**（T_load_bound 纯解析推导，§3）。

## 6b. D6 — FC3_REPLAY_INPUT_VIEW（结构性燃料盲视）

不依赖程序性承诺，构造**显式白名单输入投影**：

- 重放引擎对以下字段**结构上不可达**：fuel_raw、J_FEC、B_perfect/B_predicted/
  L_prediction、R、任何臂间比较性能字段；
- 白名单暴露：run 身份、原始证据哈希、time/state、P_unserved、跟踪诊断输入
  （P_prop_demand/actual）、DG 状态/功率、电池/SOP/V/I/安全证据、任务/服务
  证据、原始可行性溯源字段；
- 终端记录中可行性+燃料混居时，先构造**不可变的净化投影**再调用 FC-3
  checker；
- 资格化必须证明被禁字段缺席/不可达；**静态与运行时守卫**在 FC-3 代码试图
  访问被禁字段时 fail-closed（Q-FC3-18/19）。

## 7. 未决事项（FC-3 Design Gate 已裁决：①取动态包络（D1）；②P_prop_limit
不入 v1（D3）；③批准 readjudicated_status 并列字段（D4）；④parity 不重算
（D5）。剩余无。原 §7 四项已全部关闭。）

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

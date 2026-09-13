# Phase 4 研究路线 —— 系统优化重构（Model E / 真实工况 / 新一代预测 / 先进 EMS）

**创建日期**: 2026-09-03
**分支**: `Phase4`（长期主分支；各工作包子分支从此分出，完成后 PR 回合）
**来源**: 用户与外部顾问（GPT）讨论结论 + 本仓库既有规划对照（2026-09-03 session，Q1~Q5 用户裁决）
**状态**: 路线级规划（backlog 级）。**本文档中的 T-4xx 任务清单不构成实施授权**；每个工作包启动时单独走 brainstorm → requirements → design → tasks 四门禁 spec 流程（`.specs/phase4-<包名>/`），定稿以 spec 为准。

---

## 1. Phase 4 定位与边界

### 1.1 定位

Phase 4 = **整个系统的优化重构**。以"增量升级 Model D 为 Model E"为原则（非推倒重来），
把研究重心从 N6/部署侧转回**模型科学性、工况真实性和控制方法**：

```text
A: Model E 船舶物理模型升级 ─┐
B: 真实控制逻辑 / PMS / 驾驶行为 ─┴─► C: 真实工况生成器 v3
        ─► D: 数据集 v3 ─► E: 新一代负荷预测模型 ─► F: 预测式先进 EMS
```

A/B 可部分并行；整体依赖顺序 **A+B → C → D → E → F**。

### 1.2 边界声明（Q1 裁决）

- **本 Phase 暂不做 N6 / HIL / NPU 部署**（E 包 T-444 明确"暂时不考虑 N6"）。
  Phase 3 已完成 N6 + soft-HIL 闭环验证，本轮不受 NPU 算子限制约束预测模型设计。
- 旧 `.specs/phase4-model-e/`（2026-08-31 draft，面向 HIL 的模型重构与实时化，
  T-401~T-407 旧编号）**已被本文档取代（superseded）**，目录保留存档、编号作废。
- **HIL 实时化（经纬恒润平台对接、sample time/solver 决策、接口映射）推后为后续
  Phase 候选**，待 HIL 平台资料（`docs/research_roadmap.md` §8.4.6 清单，至今未到位）
  齐备后再立项。Model E 仍须保持 **HIL-ready**（fixed-step、无代数环、确定性，
  见 T-454），但不以实时化为本 Phase 目标。

### 1.3 与既有规划的衔接

| 既有规划（来源） | Phase 4 落点 |
|---|---|
| roadmap §8.4.8 旧 Phase 4（HIL 实时化） | superseded（§1.2）；实时化内容推后 |
| 2026-08-31 日志「四个后续研究方向」① Simulink 模型设计 | A 包 |
| 同 ② EMS 控制逻辑 | B 包 + F 包 |
| 同 ③ 工况生成器 v3（法规语义差异化表，roadmap §8.4.4） | C 包（已吸收法规语义设计） |
| 同 ④ 预测模型设计 | E 包 |
| roadmap §8.4.5 trajectory-aware EMS（Ê_300→轨迹级优化） | F 包（Oracle Gate → MPC-ECMS） |
| `docs/optimization_backlog.md` B1 航次计划特征（P0） | D 包 T-431（known-future contract）兑现 |
| 同 B2 失败模式门控（P0，C4 上船前置） | **并入 B 包 T-417 Safety/Interlock 层**（Q5 裁决：B2 本质是安全机制而非收益机制） |
| 同 B3 预测入环方式（含 MPC-lite） | F 包 T-446~T-449 |
| 同 B4 预测时域 / B5 换模型 / B6 数据规模 | E 包多步轨迹目标（T-433/T-434）与模型族（T-436~T-441）；D 包 fleet v3（T-432） |
| backlog 重启条件「真实参数到位 ≥50%，不碰假目标」 | **继续生效**：A 包 T-402 参数真实性审计即为此服务；无真实数据时宁可保持 constant/假设值并显式标注（T-405 纪律） |

**结论**：Phase 4 是既有规划的兑现与扩展，而非推翻。

### 1.4 分支与工程策略（Q3/Q4 裁决）

- 主分支：`Phase4`（2026-09-03 自 main 创建，首个 commit = 本文档）。
- 每个工作包从 `Phase4` 开子分支（如 `feat/phase4-a-model-e`），完成后 PR 回合
  `Phase4`；Phase 4 全部收官后 `Phase4` 整体 PR 回 main。
- 任务编号沿用 GPT 清单 **T-401~T-455**；旧 draft 的 T-401~T-407 编号作废（Q4）。
- 每个工作包 = 一个独立 spec（`.specs/phase4-<包名>/`），brainstorm 逐个敲定，
  **一次只执行一个工作包**；spec 定稿时可对本文档任务清单增删改，以 spec 为准。
- Model D 工作区冻结不动；Model E 在新工作区构建（命名待定，A 包 brainstorm 裁决）。
- 全部工程纪律沿袭 AGENTS.md（脚本驱动、机器可读 Gate、不碰假参数、诚实记录失败）。

### 1.5 新 Session 使用方式

新 session 启动某工作包时，agent 应依次研读：

1. 本文档（总体路线 + 该包任务定义）；
2. `docs/research_roadmap.md` §8（Phase 3 收官状态与 §8.4 规划档案）；
3. `docs/optimization_backlog.md`（挂起项与重启条件）；
4. 该包对应的 `.specs/phase4-<包名>/`（若已存在）。

然后按四门禁流程为该包建 spec。涉及 AI/数据集时另读 `docs/agent/AI_MODELING.md`；
涉及固件时另读 `docs/agent/STM32N6_ARCHITECTURE.md`（本 Phase 预期不涉及）。

---

## 2. 工作包 A：Model E 船舶物理模型

> 目标：把 Model D 的物理保真度升级到能支撑"environment → physics → load"因果链。
> 全部 Mac 本机执行。

### T-401 — Model D → Model E 建模审计
不修改代码。逐个检查 Model D 全部 subsystem（Scenario_Manager / Ship_Speed_Controller /
Propulsion_Plant / Load_Measurement / EMS_Controller / Genset / Battery / DC Bus /
DC-DC / Fuel model / Protection），逐项标记 KEEP / REFINE / REPLACE / ADD / REJECT，
并打 Physical Fidelity 与 Real-Time Cost 两个等级。
产物：`modelE_architecture_audit.md`（路径以 spec 为准）。

### T-402 — 参数真实性重新审计
复用现有 parameter audit，按五级来源分类（实船图纸值 / 厂商值 / 文献值 / 估计值 /
假设值），重点找最薄弱项：船体阻力、排水量、吃水、水深、螺旋桨参数、电机效率、
发电机最低负荷、发电机启停过程、电池内阻/OCV、辅机负荷。
目标不是消灭所有假设，而是明确**哪些假设最可能影响论文结论**。

### T-403 — 重构船体阻力模型
当前 R = K_R·V_rel·|V_rel| + res_mult 过于粗糙。升级为
R_total = R_deep + R_shallow + R_bank + R_wind。
第一版实现 deep-water resistance + shallow-water correction + current-relative speed；
bank/wind 后续加入。验证：V↑⇒R↑；h↓⇒R↑；U_against↑⇒P_prop↑。

### T-404 — 水深 / UKC / 浅水效应
新增状态：water depth h、ship draft T、UKC = h−T、Fr_h = V/√(gh)。
Scenario 不再用"阻力 ×1.15"表示浅滩，而是给"水深 = X m"，由 Plant 自算浅水附加阻力。
这是关键科学升级：environment → physics → load 逻辑真正成立。

### T-405 — 推进电机 / 逆变器效率模型升级
η_motor = const 升级为 η_motor = f(n,T) 或 f(n,P)；η_inv = f(P)。优先 LUT。
**纪律：没有可信数据时宁可保持 constant，不自造漂亮的 map。**

### T-406 — 螺旋桨模型真实性检查
检查现有 F = K_Fw·ω|ω| − K_Fv·V_A|V_A|、Q = K_Qu·ω|ω| − K_Qv·V_A|V_A| 是否足以
支撑论文。两种结局：资料不足 → 维持现模型 + 参数敏感性验证；有 D、KT/KQ 或设计点
数据 → 升级为 J = V_A/(nD)、K_T(J)/K_Q(J) LUT。

### T-407 — 双发电机拆分
当前两台 500 kW 聚合为 P_gen∈[0,1000]（最低稳定功率 0 还是 ASSUMED）。
拆为 DG1/DG2 两个独立 Plant，各自独立 power/ramp/inertia/state/fuel。
**本任务只搭 Plant，不做控制策略。**

### T-408 — 发电机启停与运行状态模型
状态机：OFF → STARTING → WARMUP → ON → COOLDOWN → OFF。
参数：start delay、minimum power、rated power、ramp rate、minimum up/down time、
start fuel penalty（不确定可参数化）。这是 EMS 研究"何时启动第二台机组"的前提。

### T-409 — 发电机 BSFC 与机组组合逻辑检查
从 BSFC(P_gen,total) 改为 fuel = fuel_1(P_1) + fuel_2(P_2)，
使"一台 500 kW"与"两台各 250 kW"的经济性可被区分。

### T-410 — Battery 1RC ECM
当前 SOC + efficiency + power derating。升级为 V_t = OCV(SOC) − I·R_0 − V_1，
V̇_1 = −V_1/(R_1C_1) + I/C_1。新增 battery terminal voltage / current / 极化状态。

### T-411 — Battery SOP
基于 T-410 计算 P_dis,max(t) / P_chg,max(t)，替代固定 P_bat,max = 1050 kW。
约束来源：SOC / voltage / current。先进 EMS 与 rule-based EMS 共用同一 SOP。

### T-412 — Model E 整体物理 Gate
完成上述升级后停止施工。标准测试：静水/顺水/逆水/深水/浅水/单机/双机/
battery 充放电/全推进 ramp。检查：energy closure、数值稳定性、speed-power 曲线、
generator fuel、SOC、battery voltage、Vdc、fixed-step runtime。
**只有本 Gate PASS 才允许继续生成新数据。**

---

## 3. 工作包 B：真实控制逻辑 / PMS / 驾驶行为

> 可与 A 后半段部分并行。全部 Mac 本机执行。

### T-413 — 重新定义"驾驶员输入"
当前 v_cmd → PI → P_prop。研究现实船舶应抽象为 lever command / RPM reference /
shaft power reference / speed-hold 中的哪种。最终至少支持 Manual
（lever → RPM/power）与 Automatic voyage（v_ref → propulsion controller）。
**先研究逻辑，不立刻实现。**

### T-414 — Propulsion controller 重构
Operator/Navigation → Propulsion command → RPM/Torque/Power controller → Motor，
区分"航行决策"与"推进执行控制"，不再全部归入 PI speed controller。

### T-415 — Traditional PMS v2 设计（先设计不写代码）
输入：P_load / SOC / Vdc / battery SOP / DG state。
输出：DG1 on/off、DG2 on/off、DG1 power、DG2 power。
逻辑含 start/stop threshold、hysteresis、dwell time、SOC low/high、reserve margin。

### T-416 — Traditional PMS Stateflow 实现
1DG NORMAL →（高负荷）→ DG2 STARTING → 2DG NORMAL →（低负荷+延时）→ 1DG NORMAL；
另有 low SOC override、high SOC charging suppression、fault fallback。
目标：形成真正可信的 **Industrial baseline**——不再只拿简单 rule controller 当对照。

### T-417 — Safety / Interlock 层
安全规则从"EMS 优化"中分离：battery undervoltage、Vdc undervoltage、DG fault、
SOC emergency low、propulsion power limit、loss of communication。
原则：**先进 AI/MPC 不允许绕过 safety layer**。
**（Q5 裁决：本任务吸收 backlog B2「失败模式门控」P0——预测失败模式/异常场景下
切回规则基线的门控作为 safety 层组成部分设计。）**

### T-418 — Baseline controller validation
比较旧 law=0 / law=5 / PMS v2：fuel、DG starts、SOC、低负荷 DG 时间、Vdc、
battery throughput，确认 PMS v2 行为合理。

---

## 4. 工作包 C：真实内河航次与工况生成器 v3

> 核心原则：工况生成逻辑改为 Route → Environment → Navigation Constraint →
> Operator → Plant，而非 segment → speed。全部 Mac 本机执行。

### T-419 — Mission Generator v3 架构设计（先设计不写生成器）

### T-420 — Bridge 工况
Approach → Deceleration → Controlled bridge transit → Exit → Acceleration，
重点形成出桥恢复时的 propulsion transient（法规语义：桥区"保持舵效的安全航速"，
非越慢越好——roadmap §8.4.4）。

### T-421 — Harbor 工况
较长低速段 + 小幅速度调整 + 低加速度 + higher relative hotel load，与 bridge 明确分离
（法规语义：港区"慢速通过"）。

### T-422 — Shallow-water 工况
输入直接是 water depth 而非 resistance multiplier；navigation supervisor 决定
v_limit（UKC 限速）；Plant 根据水深自增阻力（v-P 解耦）。

### T-423 — Upstream / downstream current model
U = −1 m/s 固定值升级为 U(t) = Ū + δU(t)，扰动须有时间相关性
（smooth/bounded process），不要白噪声。

### T-424 — Traffic disturbance
前船减速 / 临时避让 / speed recovery：cruise → slowdown → follow → recover。
一类真实的 AI 预测难点。

### T-425 — Emergency mission（公务船属性）
Standby → Emergency dispatch → Rapid acceleration → High speed → Mission operation，
可同步改变 P_hotel/mission。

### T-426 — Berthing / departure
单独设计离泊（startup → 低速操纵 → 驶入航道加速）与靠泊（减速 → 低速接近 → 靠泊），
避免简单镜像。

### T-427 — Full Voyage Template v3
组合真实任务模板，例如：
- Routine patrol：departure → harbor → cruise → bridge → upstream → patrol → return → berth
- Emergency response：standby → departure → rush → shallow → bridge → mission → return
- Heavy-current mission：departure → cruise → upstream → shallow → work → return

### T-428 — Stochastic Mission Generator
在真实 template 上随机化：segment length / current / water level / driver
aggressiveness / traffic / mission load / resistance uncertainty。
**条件随机，不是各参数独立 random。**

### T-429 — Mission v3 可辨识性审计
检查 bridge/harbor/shallow/upstream/berth 在信号空间是否真的不同
（Phase 2.5 已发现 bridge/harbor 观察等价，此 Gate 必须保留）。

---

## 5. 工作包 D：数据集 v3

> Simulink 仿真优先本地 PC/Mac；若换 PC 可用 Parallel Computing Toolbox。

### T-430 — 新增观测变量定义（Historical）
AI 可见的历史量：P_load / speed / acceleration / propeller RPM / P_gen / P_bat /
SOC / Vdc 等（brainstorm 定稿）。

### T-431 — Known-Future Information 定义（关键）
逐变量标记三类——已知（route / planned speed / bridge location / speed limit /
chart depth）、Forecast（current / water level）、Unknown（future real P_load /
unexpected traffic）。产物：`future_information_contract.md`。
**（兑现 backlog B1（P0）：让预测器看到"可计划信息"，突破纯历史外推 R²≈0.73 天花板。）**

### T-432 — 生成 fleet v3
Model E + Mission v3，500~1000 完整航次；第一轮先 100 条 smoke 再正式生成。

### T-433 — Dataset v3 窗口化
scalar target 改 trajectory target：History 300 s；Future 300 s；
输出 P_10, P_20, ..., P_300 共 30 点。

### T-434 — Quantile target / auxiliary target
主目标 future power trajectory；分位数 q10/q50/q90；辅助目标 E_300
（保持与旧研究连续性）。

### T-435 — Dataset split / leakage audit
沿袭 Phase 2.5 纪律：voyage-level split、near-neighbor audit、no test tuning；
新增 OOD set（stronger current / lower water level / unseen route combination）。

---

## 6. 工作包 E：新一代负荷预测模型

> 此阶段才真正需要 AutoDL。选模裁决**暂不考虑 N6**（T-444）。

### T-436 — 重训旧 MLP-Q
目的不是赢，而是建立 Dataset v3 上的历史 baseline。（AutoDL）

### T-437 — TCN-Q baseline
公平训练 TCN：same split / same target / same loss / same seeds。（AutoDL）

### T-438 — TFT baseline
天然支持 historical observed + known future + static + quantile + multi-horizon，
强烈建议做。（AutoDL）

### T-439 — Transformer baseline
简单 historical encoder + future encoder + attention，不需要大，
用于判断 TFT 的复杂机制是否必要。

### T-440 — Proposed REAT-T 设计（先 brainstorm 不训练）
Past history → TCN ┐
                    ├─ Fusion → trajectory decoder → q10/q50/q90
Future route/env → Transformer ┘
核心不是"大"，是 Past dynamics + Future context。

### T-441 — REAT-T 第一版训练
控制参数量 0.5~3M，不要一上来几十 M。（AutoDL）

### T-442 — Predictor ablation
至少 5 臂：history only / future route only / history+route /
history+route+environment / full REAT-T。回答"提升到底来自哪里"。

### T-443 — Predictor OOD evaluation
unseen current / unseen water depth / unseen mission composition。
可能比 IID RMSE 更有论文价值。

### T-444 — Predictor Winner Freeze
候选最多 MLP-Q / TCN-Q / TFT / REAT-T，按 prediction + probabilistic calibration +
OOD 裁决。**不无限扫模型；暂不考虑 N6。**

> 澄清注记（2026-09-13，Phase 4-E spec FR-609/DGN-E-18 落账，不改路线结构）：
> 本条 "prediction + probabilistic calibration + OOD 裁决" 解释为——**validation 预测与
> 概率证据决定 Winner；OOD 证据进入 T-444 最终科学证据包；OOD 不参与 Winner 选择**
>（单向证据流 TRAIN→VAL HPO→CONFIG FREEZE→3-SEED CONFIRM→VAL-ONLY WINNER→
> PRETERMINAL FREEZE→TEST/OOD UNSEAL→FINAL）。正式候选集以 Phase 4-E spec
>（`.specs/phase4-e-predictor-v3/`）冻结五候选为准：MLP-Q-v3 / TCN-Q / TFT /
> Transformer / REAT-T（winner_pool 五候选，训练前协议澄清）；T-444 实际执行拆分为
> T-444a（PRETERMINAL WINNER SELECTION）→ T-443（TERMINAL TEST/OOD EVALUATION）
> → T-444b（FINAL WINNER EVIDENCE FREEZE）。

---

## 7. 工作包 F：预测式先进 EMS

> 纪律沿袭"先证明收益上限再施工"。全部 Mac 本机执行。

### T-445 — Trajectory Oracle Gate（控制施工前第一道 Gate）
不用 NN，直接给控制器真值未来负荷轨迹 P_future,true，问：知道完整未来轨迹比当前
E_300 scalar 理论上多省多少？**上限不存在则停止。**

### T-446 — Predictive EMS architecture brainstorm（只设计）
Prediction trajectory → Upper predictive layer → SOC_ref / P_gen target / λ schedule
→ Existing ECMS。需裁决优化对象是 SOC_ref trajectory / P_gen trajectory /
λ trajectory 中的哪一个。

### T-447 — Upper MPC baseline
最简单版本：H=300 s，控制间隔 10 s，
J = fuel + w_soc·SOC error + w_r·ΔP_gen²。先不搞 stochastic MPC。

### T-448 — MPC + Traditional PMS integration
层级分离：Stateflow 决定 DG commitment（启停组合），MPC/ECMS 决定 continuous power。

### T-449 — Oracle MPC-ECMS closed loop
仍只用真值未来。比较 PMS v2 / law=5 / law=7 / Oracle MPC-ECMS / DP，
确认控制架构本身有价值。

### T-450 — REAT-T + MPC-ECMS
**只有 T-449 PASS 才接 AI。** 这才是最终 Proposed Controller。

### T-451 — Uncertainty-aware control
第一版不用复杂 chance constraint：P_use = q50 + α·(q90 − q50)，
研究 α = 0 / 0.5 / 1 对 fuel / SOC / safety 的影响。

### T-452 — Full closed-loop matrix
最终只保留 6 个核心控制器：① Industrial PMS v2 ② SOC-Adaptive ECMS
③ Phase 3 MLP-Q G2 ④ Oracle predictive EMS ⑤ REAT-T predictive EMS ⑥ DP。
**不无限增加 controller。**

### T-453 — Robustness matrix
变化：water level / current / initial SOC / BSFC tier / resistance / forecast error /
operator aggressiveness。

### T-454 — Real-time Model E profiling
不优先 N6，但 Plant 必须 HIL-ready：fixed-step、step execution、无代数环、
memory、确定性行为。只评价 Model E，不是 NN。

### T-455 — Phase 4 Paper Gate
最终检查整条逻辑链：
- Model：Model D → Model E 有科学真实性提升；
- Mission：v2 → v3 不再是速度模板；
- Prediction：history-only → route/environment-aware 确实提升长 horizon/OOD；
- Control：E_300 → trajectory 确实带来额外闭环收益。
全部成立 ⇒ **Paper GO**。

---

## 8. 十二大节点压缩视图

```text
1. 审计 Model D ─► 2. 升级船体/浅水 ─► 3. 双机组 + battery ECM
─► 4. 重新设计驾驶与 PMS ─► 5. 完成 Model E（物理 Gate）
─► 6. 重新设计真实工况 ─► 7. 生成 Dataset v3 ─► 8. 加入 future route/environment
─► 9. 训练 TFT / REAT-T ─► 10. Oracle trajectory Gate
─► 11. 设计 MPC-ECMS ─► 12. AI + EMS 闭环验证（Paper Gate）
```

## 9. 本地 / AutoDL 分工

| 工作 | Mac/本地 | AutoDL |
|---|---|---|
| Simulink Model E / Stateflow PMS / Mission Generator | ✅ | |
| Dataset generation / preprocessing | ✅ | 可选 |
| MLP smoke | ✅ | |
| TCN / TFT / REAT-T 正式训练、小规模超参搜索 | | ✅ |
| MPC / ECMS / Closed-loop Simulink / OOD / robustness | ✅ | |
| 后续 HIL | ✅ PC/N6（后续 Phase） | |

M1 8GB 可承担 Phase 4 绝大部分科研施工；AutoDL 主要承担 T-436~T-443 模型训练段。

## 10. 执行顺序与依赖

```text
A (T-401~T-412) ─┐
B (T-413~T-418) ─┴─► C (T-419~T-429) ─► D (T-430~T-435)
                                          ─► E (T-436~T-444) ─► F (T-445~T-455)
```

- A/B 可部分并行；T-412（物理 Gate）是 C/D 的硬前置；
- T-445（Oracle Gate）是 F 包控制施工的前置，FAIL 即停止该方向；
- 每包启动前单独 brainstorm 定稿；包内任务顺序以该包 spec 的 tasks.md 为准。

---

## 变更记录

| 日期 | 变更 | 来源 |
|---|---|---|
| 2026-09-03 | 初版：用户×GPT 讨论结论落库 + 旧规划对照 + Q1~Q5 裁决（旧 phase4-model-e supersede；文档位置；两层分支策略；沿用 T-4xx 编号；B2 并入 T-417）；`Phase4` 分支创建 | 本 session |

# Phase 4-F T-445 Trajectory Oracle Gate Design

Spec: `phase4-f-trajectory-oracle-gate`  
Status: design-approved（R1~R9 闭合，2026-09-15）  
Created: 2026-09-15  
Approved: 2026-09-15（APPROVED WITH 9 REQUIRED CORRECTIONS——R1~R9 全部按令闭合，validation green，无需二次 Design Gate）  
Requirements: `./requirements.md`（requirements-approved，2026-09-15，R1~R7 闭合）  
Brainstorm: `./brainstorm.md`（Q1~Q4 CLOSED）

本文件把 requirements 翻译为可施工设计，并**逐条闭合 OQ-F1~F9**。所有数值冻结项在对应 DGN 条目中标注 🔒。Design Gate R1~R9 修订已在对应条目内标注（R#）。

## 1. 架构总览

```text
                     ┌──────────────────────────────────────────────┐
                     │  冻结真值源（只读）                            │
                     │  datasets_v3/formal/iid/<voyage>.mat          │
                     │  （stream.P_load_total_kW @1Hz + meta）       │
                     └───────┬──────────────────┬───────────────────┘
                             │                  │
                    oracle 窗装配器      形状波动评分/语料选择（arm-independent）
                             │                  │
        ┌────────────────────┼──────────────────┼──────────────────┐
        │ B0: law=5          │ B1: law=6        │ B2: law=6 +      │ B3: 离线全航程 DP
        │ 单次全程 sim        │ K_S=0.01 标量通道 │ K_S=1/30 Δλ 通道  │ （parity 资格后审）
        │                    │ ê(t) 真 ZOH      │ Δλ(t) 真 ZOH      │
        │                    │ 单次全程 sim      │ ← 滚动窗优化器     │
        │                    │                  │   （10s 段 sim ⇄   │
        │                    │                  │    SimState 交替） │
        └────────────────────┴──────────────────┴──────────────────┘
                             │
                逐 run 指标提取（FEC/可行性/干预/负担审计）
                             │
                Python 统计层（cluster bootstrap B=10000）
                             │
                机器可读 Gate JSON（正交裁决字段）
```

**DGN-F-01：B1 与 B2 共乘现役 law=6 变体，零 `.slx` 结构修改（已批准）。**

law=6 核心：λ(t) = λ₀ + K_soc·(SOC_ref + K_S·ê(t) − SOC(t))，λ_eff = clamp(·, 0.43, +∞)。监督通道增益 = K_soc·K_S：

- **B1**：K_S = 0.01（canonical 不变），ê(t) = (E300_true(t0) − E_ref_kWh)/E_scale_kWh（真 ZOH，归一化契约见 §2）；
- **B2**：K_S = 1/K_soc = 1/30（per-run 参数覆盖），ê(t) := Δλ_traj(t0) [kg/kWh]，K_soc·K_S·ê = Δλ_traj 成立（浮点残差 ~1e-16，AC-618 夹具锁定）。

备选（新增 law=8 变体）否决：对冻结基线的无谓结构手术。R7 parity 适配器 = **输入数据预制 + per-run 参数 + per-run ZOH 块参数覆盖（§2）**，law=6 核心控制逻辑零改动。

**B0/B1/B2 的 λ 下限语义差异登记**：Lambda_Floor=0.43 仅在 law=6（K_S≠0）生效，B0（law=5）无 floor。Gate 比较对为 B1 vs B2（两者 floor 完全一致）；B0 仅作 context 锚点（H_scalar）。floor 对 B2 权限的限制登记为**冻结 ECMS 族作用空间限制**（λ₀=0.425 < 0.43，B2 下行权限受限）——不声称其为数学上已证明的单调"保守"效应（R3）。

## 2. OQ-F1 —— 刷新/视界/末端 + B1 归一化契约（DGN-F-02）🔒

### 2.1 刷新与窗语义 🔒

- **刷新节奏**：t0 ∈ {0, 10, 20, …, T_end−10}，**10 s sample-and-hold**（B1/B2 同一节奏，FR-725/733）。
- **oracle 窗**：W(t0) = [t0, min(t0+300, T_end)]；1 Hz 真值 P_load_total_kW(t) 来自冻结航次工件 stream；bin k = [t0+10(k−1), t0+10k)，P̄_k = bin 内 1 Hz 样本均值；bin 能量 E_k = (10/3600)·P̄_k kWh；E300_true(t0) = Σ_k E_k（与 FR-721 公式恒等；实现可用 1 s 分辨率累计积分，二者在 1 Hz 流上数学等价）。
- **末端 = shrinking-horizon**：窗长随剩余时间收缩，**禁零填充**；B1/B2 同一收缩窗。
- **partial final bin**：按真实持续时长加权（1 s 分辨率积分天然满足）。
- **末段停摆**：t > 最后合法 t0 后不再刷新，保持最后值。

### 2.2 B1 归一化契约（R1，全部冻结）🔒

| 字段 | 冻结值 |
|---|---|
| 变换 | ê(t0) = (E300_true(t0) − E_ref_kWh) / E_scale_kWh（仿射，无量纲） |
| **E_ref_kWh** | **27.781415 kWh** |
| **E_scale_kWh** | **13.671476 kWh** |
| 单位 | kWh（E300_true 与常数同单位） |
| 语义 | **全局单一仿射归一化**：所有航次/所有窗（含 shrinking-horizon 末端窗）共用同一对常数；SHALL NOT 逐航次/逐窗重拟合或重归一化 |
| 来源语料 | Dataset v3 formal **train** split 396 航次（val/test 不参与）冻结 stream 的 E300 序列（§2.1 语义，含 shrinking 末端段），n=159,472 个 t0 值，E300∈[0.236, 75.041] kWh |
| 来源工件 | `results/dataset_v3/formal/split_assignment_v3.json`（sha256 ac4c4a76…）∩ `pass1_iid_ledger.json` 396 个 train 航次工件 |
| 来源哈希 | provenance_sha256 = **b9dcf268f533a9a93c73bbaeeed3a6dca6b15227cfd868fb459c46fcc81cf045**（396 条 `voyage_id:artifact_sha256` 排序拼接的 SHA256） |
| 再生成程序 | `python/t445/compute_b1_norm.py`（实现任务；同语料同语义必须逐位复现上表常数，差异即实现 bug） |

### 2.3 真 ZOH 适配器（R2）🔒

- **机制**：run-local `Simulink.SimulationInput.setBlockParameter` 覆盖 law=6 变体内 `oracle_src`（From Workspace）块的 `Interpolate` 为 `'off'`（sample-and-hold），**不落盘、不修改 .slx**；备选方案（仅当覆盖被证明对 variant 内部块不可用时）：per-run 内存态 `set_param(...,'Interpolate','off')` + 跑后恢复，同样零字节落盘。两种实现都必须通过下述机器测试才合法。
- 输入数据仅需 10 s 节点（t0 网格，HoldingFinalValue 收尾）；块 SampleTime 0.01 s + Interpolate=off ⇒ 任意查询时刻（含 ode3 minor stage）返回最近样本 = 数学真 ZOH。
- **ZOH 机器测试**（先决，实现期）：构造已知 10 s 阶梯数据，断言 (a) 主步时刻 t=9.99/10.00/10.01 取值正确；(b) minor-stage 查询无插值中间值（与同数据 Interpolate=on 运行对比，边界邻域必须出现差异）；(c) B1/B2 使用同一适配器实例语义。测试不过 ⇒ 不得执行任何正式臂。
- B1 与 B2 SHALL 使用同一 ZOH 适配器（同一块、同一覆盖参数、同一数据语义）。

## 3. OQ-F2 —— B2 监督作用空间（DGN-F-03，经 R3 修订）🔒

- **决策变量**：Δλ_k ∈ ℝ，k = 1..K_rem（K=30 对应 300 s 窗），每 10 s bin 分段常值。
- **容许集合**：Δλ_k ∈ [−0.10, +0.10] kg/kWh，离散网格步长 0.005（41 levels）🔒。**不设 Δλ 速率限制**（R3：非物理执行机构约束、非已批准 requirements 所要求、收窄 oracle 监督策略空间、且跨重解正确实现需扩展前动作状态——予以移除）。
- **λ_eff 包络处处一致**：0.43 ≤ λ_eff ≤ 0.80，同时适用于真实执行（law=6 家族 floor + 上限硬钳）与 B2 优化器/策略表内模型（λ 网格 [0.43, 0.80]）🔒。
- **诊断**：floor（0.43）与 cap（0.80）激活比例逐 run 报告（执行侧与优化器侧分别计数）。
- **执行链冻结**：B2 只写 Δλ_traj 通道；ECMS 瞬时 argmin（{0}∪[0:20:1000] kW 网格 + SOC 硬约束 0.105/0.945 + 怠速项）→ 07_PMS_v2 → 16_Safety_Interlock → Plant 全部不改（FR-723/724）。
- **浮点说明**：B2 经 K_S=1/30 通道，AC-618 夹具回归验证 λ 合成与直接加法差 < 1e-12。

## 4. OQ-F3 —— B2 终端能量机制（DGN-F-04，经 R4 修订）🔒

窗内优化目标（每 t0 的窗 W）：

```text
min_{Δλ_1..K}  Σ_{k=1..K} fuel_model,k(Δλ_k; x_k)  +  V_term(SOC_H)
```

**V_term = 线性终端价值 + 带外二次罚**：

```text
V_term(SOC_H) = −k_eq^E · Cap · (SOC_H − SOC_target)
              + w_term · max(0, |SOC_H − SOC_target| − δ_term)²
```

- **终端目标**：`SOC_target := SOC0_voyage`（与航次级 parity 对齐，R4）。全语料机器断言（2026-09-15 执行）：全部 576 航次（train/val/test）SOC0 = 0.60 —— 故本语料内 SOC_target ≡ 0.60，等价关系登记于此；实现仍按 SOC0_voyage 参数化，不断言假设。
- **线性项**把终端储能按 k_eq^E 定价（与 FEC 指标同一折算率，DGN-F-06）→ 窗末放空电池的"收益"被等值扣回，机制上消除跨视界 SOC 欠账套利（FR-735）。
- **w_term 标定推导（R4 修订，显式记录）**：
  - 标定偏离点 d_cal = **0.02**（严格在死区 δ_term=0.01 之外；二次罚在 |Δ|=δ_term 处恰为零，属软死区设计语义）；
  - 越带超出部分（d_cal − δ_term = 0.01 SOC）的线性能量信用 = k_eq^E·Cap·0.01 = 0.5296×1071.36×0.01 = **5.674 kg**；
  - 期望关系：d_cal 处罚值 = 2× 越带信用 = 11.348 kg（罚显著压过信用）；
  - w_term·(0.01)² = 11.348 → **w_term = 1.13e5 kg/SOC²** 🔒（单位：kg/SOC²）；
  - δ_term = 0.01 🔒；Cap = 1071.36 kWh。
- **机制性质声明（R4）**：软罚 + 死区 = **反套利有效性**机制，不声称 SOC_H 永不越带的硬保证；AC-613 按有效性测试（见 requirements AC-613 修订文）。

## 5. OQ-F4 —— TERMINAL_SOC_ADMISSIBILITY（DGN-F-05）🔒

- **正式规则**：航次末端 |SOC_end − SOC0| ≤ **0.005**（与 Phase 1 DP 终端带、T-104 标定惯例一致）；
- 适用全部正式臂；SOC0 为冻结航次初始 SOC（全语料机器断言 = 0.60，§4）；
- 越限 run = `INFEASIBLE_OR_NONCOMPARABLE`，FEC 不拯救（FR-744）；持续不可行走 FR-777 → `GATE_VALIDITY_FAIL`（禁成对删除）；
- 本规则先于任何正式 B1/B2 结果观察冻结（本文件即预注册载体）。

## 6. OQ-F5 —— k_eq^E 与 Model E 能量边界审计（DGN-F-06，经 R5 修订）🔒

### 6.1 电池聚合拓扑基线检查（`BATTERY_AGGREGATION_BASIS_CHECK`，R5）🔒

| 检查项 | 结论（config/ModelE_Physics_Parameters.m §6 冻结事实源） |
|---|---|
| 单支路 Ah | 930 Ah（535.68 kWh/支路，DRAWING 20260526设备参数.docx） |
| 支路数/拓扑 | 2 支路聚合（1071.36 kWh = 576 V × 1860 Ah，与 DRAWING 一致） |
| OCV/R0/R1 LUT 归属 | **pack 级聚合等效**：cell→pack 缩放显式记录（K2 LFP26650P 2.6Ah/3.2V，N_s=180 串联、N_p=715.38 连续等效并联，R_pack=R_cell·N_s/N_p）——LUT 直接是聚合模型参数，不存在"单支路值误用于聚合"的口径错位 |
| 参考电流 | I_ref = 0.25C = 465 A 为 **pack 级**电流（0.25×1860 Ah），与模型 I_bat_A 口径一致 |
| 电阻缩放 | 已在 cell→pack 缩放内完成（N_s/N_p），不再二次缩放 |
| 容量-能量基线 | 1860 Ah × 576 V = 1071.36 kWh = Bat_Cap_kWh ✓ |

结论：模型本身就是聚合等效 ECM，DGN-F-06 的参考点计算与模型记账口径自洽。

### 6.2 能量边界审计（`NO_DOUBLE_COUNTING_OF_ENERGY_LOSS`，R5）

| 路径环节 | Model E 实现 | 进 η_path^E？ | 理由 |
|---|---|---|---|
| 机组 AC→母线 | η_rect = 0.97（功率平衡显式） | ✅ | 补给链必经，SOC 记账未含 |
| 母线↔电池端子 | η_dcdc = 0.97（14 号 SOP/换算显式） | ✅ | 同上 |
| 端子→储能 | 1RC ECM：R_0/R_1 I²R 物理损耗 + η_coul=0.98 仅进 SOC 计量（Q5 红线：不乘端口功率） | ✅ 以 ECM 推导参考效率计入一次 | SOC 记账是"已发生事实"，FEC 修正是"补给所需燃料"，环节不同不双计 |
| ASSUMED_Bat_Eff_* (0.95/0.95) | 仅 ECMS 控制器内部 mbe 口径 | ❌ | 与度量折算无关 |

### 6.3 冻结数值 🔒

- BSFC_ref^E = **482.749 g/kWh**（五节点 PCHIP 在 [125,500] kW 的均匀能量加权 ∫P·BSFC/∫P；非算术均值 494.8——算术均值向高 BSFC 低载区偏置）；
- 充电方向参考效率（SOC=0.60、I_ref=465 A）：V_t_chg = OCV+I(R_0+R_1) = 593.42+465×0.01487 = 600.33 V → η_chg_ref = η_coul·OCV/V_t_chg = **0.9687**；
- 放电方向参考效率（同参考点）：V_t_dis = OCV−I(R_0+R_1) = 586.51 V → η_dis_ref = η_coul·V_t_dis/OCV = **0.9686**（方向效率成对使用，不以单个对称均值默默替代——R7）；
- η_path^E = 0.97 × 0.97 × 0.9687 = 0.9111；
- **k_eq^E = (482.749/1000)/0.9111 = 0.5296 kg/kWh** 🔒（Model D 旧值 0.499 废止）；
- 稳健性报告点：0.8× = 0.4237 / 1.2× = 0.6355；break-even 按 FR-745。

### 6.4 预结果资格化（R5）：参考充电/回补 fixture

在任何正式 B1/B2 结果观察**之前**：非正式 Model E fixture（固定功率充电段 SOC 0.58→0.62 + 静置），独立测量母线侧投入能量与储能增量，验证有效回补边界：η_chg_measured = ΔE_stored/∫P_term dt 与 η_chg_ref=0.9687 偏差 ≤ **0.005（绝对）** 🔒。通过 → 0.5296 转 canonical；失败 → 在任何正式结果观察前重算 η_path^E 与 k_eq^E（预结果资格化，非事后调参）。

## 7. OQ-F6 —— 形状波动评分与语料选择（DGN-F-07，经 R6 修订）🔒

- **航次评分**（臂独立，仅用冻结 P_load 真值；R6：测**视界内轨迹形状**而非 B1 已观测的 E300 标量时变）：

```text
对每个完整 300 s 窗 t0（ shrinking 窗不参与评分，避免人为波动）：
  μ(t0)        = mean_k(P̄_k)              （30 bin 均值，kW）
  shape_cv(t0) = std_k(P̄_k) / max(μ(t0), P_epsilon)，  P_epsilon = 1 kW 🔒
航次评分 score_v = mean_{full-horizon t0}( shape_cv(t0) )
```

- 评分公式本身冻结 🔒；数值由代码从冻结数据计算（语料统计量，非常数契约）；
- **禁入项**：控制器输出、燃油、SOC、headroom 或任何 B 臂结果不得进入评分；
- **聚合**：split_group 评分 = 组内 4 成员航次 score_v 算术均值；
- **分层**：27 个 val split_group 按组评分降序 → HIGH 9 / MID 9 / LOW 9；
- **确定性排序**：tier 内 sha256("phase4f-t445-corpus-v1|" + split_group_id) 十六进制升序；
- **初始集**：每层前 3 组（9 组 36 航次）；**预留集**：每层第 4 组（3 组 12 航次）；禁结果导向替换；
- **语料 manifest**：`results/t445_oracle_gate/corpus_manifest.json`（航次 ID/分组/层级/排序哈希/评分值/种子），先于任何正式臂执行冻结（FR-764）；
- **数据来源合法性**：split 指派 = `split_assignment_v3.json`（sha ac4c4a76…）；test 封印不触碰（AC-615 复核 manifest test_sealed=true）。

## 8. OQ-F7 —— B3 资格化协议（DGN-F-08，经 R7 修订）🔒

### 8.1 B3 定义与可行集语义（R7）

B3 = 离线全航程直接功率 DP（复用 `+ecms/dp_problem`/`dp_solve` 机械件），Model E 双机聚合等效：

- **燃油图**：P ≤ 500 → 单机 PCHIP(P)；P > 500 → 2×PCHIP(P/2)（PMS 均分 clamp[125,500] 语义）；怠速 6.795 kg/h；C_start=0.6 kg/机·次；
- **可行集不得人为窄于 B2/Model E 可执行包络（R7）**：
  - P 网格 {0} ∪ [125:20:1000]（单机 125..500、双机聚合 250..1000 → 聚合覆盖 {0}∪[125,1000]，与真 plant 一致）；
  - 爬坡包络：N_on(P_prev) = 0/1/2（P=0 / P≤500 / P>500），ramp ≤ 20·N_on kW/s（双机 40 kW/s——来自真双机约束，非人为保守限制）；
  - 启停：离散事件，启动 +0.6 kg/机，**启动延迟（25 s+5 s）松弛为瞬时**（放宽 = 可行集更宽，dominance 审计方向保持合法：B3 ≥ 真实可实现）；
- **状态/网格**：(SOC, P_prev)，SOC 网格 0.10:0.005:0.95，dt=1 s；终端带 |SOC_T−SOC0| ≤ 0.005；**方向效率成对**：η_chg_ref=0.9687 / η_dis_ref=0.9686（R7，不用对称均值）；
- **负荷真值**：与 B0/B1/B2 同一冻结 stream。

### 8.2 资格化协议（全部通过才 `QUALIFIED_PASS`）🔒

1. 燃油图同一性：B3 复合燃油图 vs FuelModelE per-genset PCHIP 网格比对，max rel err < 0.5%；
2. 网格收敛：dSOC 0.005 vs 0.0025 重解，燃油差 < 0.5%（T-012 判据沿袭）；
3. 能量闭合/可行性：dp_checks 既有套件（电池记账 < 0.1%、SOC 界、爬坡窗、终端带）；
4. **代理-vs-真机 parity fixture（R7 新增）**：固定非正式调度 fixture 序列覆盖——单/双机区、125/500 边界、启停迁移、充电与放电、低/中/高 SOC（0.3/0.6/0.85）——B3 内部记账 vs Model E 真 plant 实测：累计 raw fuel 相对误差 ≤ **0.5%**、终端 SOC 绝对误差 ≤ **0.001**、J_FEC 相对误差 ≤ **0.5%** 🔒；
5. 记账 parity（FR-749）：同 raw fuel/终端 SOC/k_eq^E/FEC/可行性/负担报告。

任一失败 → `b3_context_status = UNQUALIFIED_CONTEXT`，H_arch/H_residual 不产出。

### 8.3 dominance 审计

ε_num = **0.005·J_FEC,B2**（相对 0.5%，对齐 DP 网格精度量级）🔒；违反 J_FEC,B3 ≤ J_FEC,B2 + ε_num → `B3_DOMINANCE_AUDIT_FAIL`。

## 9. OQ-F8 —— B2 窗内优化器（DGN-F-09，经 R3/R8 修订）🔒

- **求解器类**：窗内确定性 DP（后向归纳 + V 线性插值 + 前向重建，复用 dp_solve 数值模式，big-M=1e4）。B2 不称严格数学上界（网格离散 + 代理简化未证明全局最优）。
- **决策步**：10 s（K=30 步/窗），bin 均负荷 P̄_k；
- **状态**：(SOC, P_g_prev)，SOC 网格 0.10:0.01:0.95（86 点），P_g_prev 网格 0:20:1000；
- **转移内模型**：给定 λ_k = clamp(λ₀ + K_soc(SOC_ref−SOC_k) + Δλ_k, 0.43, 0.80)，P_g* = **预计算 ECMS 策略表**查值（λ ∈ [0.43:0.005:0.80] 76 点 × P̄ ∈ [0:20:1300] 66 点 × SOC 区域 {正常/≤0.105 禁放/≥0.945 禁充}，表 = `ecms2.ecms_law` 离线逐位复制品穷举，λ 轴最近邻索引）；SOC 转移用 `ecms.bat_energy_delta_kWh` + **方向效率对**（η_chg=0.9687/η_dis=0.9686，η_dcdc=0.97）；
- **代理简化登记（R8 措辞）**：机组爬坡/惯性/PMS 分配滞后在窗内模型中忽略——**代理简化可能降低可提取余量；正式 Gate 有效性因此依赖优化器资格化（§9.1）**。B2 最终燃油始终在真实冻结 Model-E/PMS/Safety plant 上测量；
- **首段执行**：仅 Δλ_1 注入下一 10 s 段（真 ZOH），下一 t0 重解（FR-730）；
- **不可行回退**：单窗无解 → 本段 Δλ≡0（退化 law=5）+ incident 计数；单航次不可行窗占比 > 5% → persistent-infeasibility 候选走 FR-777 🔒。

### 9.1 B2_OPTIMIZER_QUALIFICATION（R8，正式语料前强制 Gate）🔒

正式语料执行前，B2 优化器必须通过以下资格化（容差全部冻结于此）：

| # | 检查 | 冻结容差 |
|---|---|---|
| Q1 | 策略表 vs `ecms2.ecms_law` 逐位 parity（全部表节点 + 1000 随机 λ/P̄ 点） | 节点容差 0；随机点 P_g* 差 ≤ 1 个 P 网格步（20 kW） |
| Q2 | SOC 网格收敛：0.01 vs 0.005 重解固定非正式 fixture 窗 | 窗最优成本相对差 ≤ 0.5% |
| Q3 | Δλ 网格收敛：0.005 vs 0.0025 重解固定非正式 fixture 窗 | 窗最优成本相对差 ≤ 0.5% |
| Q4 | 固定非正式 300 s fixture：代理预测 vs Model E 实测的窗燃油/SOC | raw fuel rel err ≤ 1.0%；\|ΔSOC err\| ≤ 0.002 |
| Q5 | 确定性首动作重建：同状态同窗同种子两次求解 | Δλ_1 逐位一致 |
| Q6 | Δλ≡0 基线 sanity：优化器在 Δλ=0 固定下的窗成本 vs 同 fixture 纯 law=5 窗仿真 | 相对差 ≤ 1.0% |

**失败语义**：任一不过 ⇒ 不得执行正式语料；若已执行，结果无效化并记 `GATE_VALIDITY_FAIL`（等价 pre-Gate 有效性失败）。**SHALL NOT 从未通过资格化的 B2 优化器产出 `NO_GO_TRAJECTORY_VALUE`**（R8）。

### 9.2 求解预算

单窗 ≈ 30 步 × 86 SOC × 51 P_g × 41 Δλ ≈ 5.4M 转移（查表化 + 向量化），目标 ≤ 2 s/窗；实现期首任务在合成 fixture 上 benchmark（未达标先优化再进正式语料；运行时不削弱科学语义，R6/NFR-714）。

## 10. OQ-F9 —— Simulink/优化器交替执行（DGN-F-10，含机械强化）🔒

- **B0/B1**：单次全程 sim（既有 `model.run_scenario_sim` 链路 + per-run 注入 ECMS_EHat_Data / ECMS_K_S / ZOH 覆盖）。
- **B2 交替循环**（MATLAB 脚本驱动）：
  1. 段边界 t0 = 0,10,20,…；sim 段 [t0, t0+10]，`SaveFinalState` → SimState；
  2. 从 logsout 尾部读闭环状态（SOC 为窗内 DP 初值关键状态；其余由代理模型简化忽略——登记）；
  3. 窗内 DP 求解 → Δλ_1；
  4. 更新 ECMS_EHat_Data（自 t0 起恒为 Δλ_1 的 ZOH 节点）→ SimState 恢复续跑；
  5. 重复至 T_end。
- **SimState 正确性 Gate（先于正式语料）**：同航次 B1 连续跑 vs 分段跑（每 10 s save/restore）全信号逐样本一致（相对差 ≤ 1e-10）——覆盖 From Workspace 绝对时间语义 + Stateflow/persistent 状态续存 + ode3 步进续接；失败 → 升级主 Agent 裁决（备选方案需新设计，不得静默绕路）。
- **checkpoint/resume 身份（机械强化）**：重跑/续跑身份 = **(voyage_id, arm, config_hash, seed, frozen_stream_sha256, model_checksum, segment_t0/index)**——防止跨 plant/输入身份的误续跑；model_checksum = HybridShip_ModelE.slx 文件 sha256 + build_model_e.m sha256 联合。
- **oracle 提取时点**：段边界 t0 从冻结 stream 装配窗（§2.1），与 sim 时钟对齐断言（容差 0.005 s）。
- **运行预算估算**（R6/NFR-714；基于 73 号 ledger 实测 sim_wall 中位 73.6 s/航次）：
  - B0+B1：36×2×~75 s ≈ **1.5 h**；
  - B2：36×(~75 s sim + ~400 窗×2 s) ≈ **8.8 h 串行**（parfor 6 worker ≈ 1.5~2 h）；
  - B3：36×~2 min ≈ **1.2 h**；
  - 初始语料合计 ≈ **4~11 h**；预留 +12 ≈ **+1.5~4 h**；
  - 实际 wall time 执行后报告；运行时不构成修改求解器/语料/统计语义的理由（R6）。

## 11. 机器裁决正交字段与确定性优先级（DGN-F-11，R3）🔒

Gate JSON（`results/t445_oracle_gate/gate_result.json`）：

```json
{
  "schema": "t445_oracle_gate_v1",
  "trajectory_gate_verdict": "GO_TRAJECTORY_VALUE | NO_GO_TRAJECTORY_VALUE | INCONCLUSIVE | FINAL_INCONCLUSIVE | GATE_VALIDITY_FAIL",
  "b3_context_status": "UNQUALIFIED_CONTEXT | QUALIFIED_PASS | B3_DOMINANCE_AUDIT_FAIL",
  "decision_required": false,
  "expansion_used": false
}
```

**确定性优先级**（先于正式结果冻结）：

1. **硬可行性/有效性失败优先**（R9）：任何硬可行性检查失败、持续 B1/B2 不可行 > 0、语料/manifest 完整性失败、`B2_OPTIMIZER_QUALIFICATION` 失败 ⇒ `trajectory_gate_verdict = GATE_VALIDITY_FAIL` + `decision_required=true`，先于普通分类；
2. 否则按 FR-774/775/776：median_g ≥ 2.0% 且 CI 下界 > 0 ⇒ `GO_TRAJECTORY_VALUE`；CI 上界 < 2.0% ⇒ `NO_GO_TRAJECTORY_VALUE`；其余 ⇒ `INCONCLUSIVE`；
3. `INCONCLUSIVE`（初始语料）⇒ `expansion_used=true`，预留集全量重算（FR-776）⇒ 终判 GO / NO-GO / FINAL_INCONCLUSIVE（`decision_required=true`）；
4. `b3_context_status` 独立求值，永不改写 `trajectory_gate_verdict`；
5. 非 GO 终态均阻断 T-446；仅 `INCONCLUSIVE` 允许触发一次扩样（T-445 内）。

## 12. 审计能力矩阵与硬可行性最小集（DGN-F-12，经 R9 修订）🔒

### 12.1 `audit_capability_matrix`（机器可读，config 登记 + 每 run 实例化）

| 信号 | 状态 | 源 |
|---|---|---|
| 任务完成/时长一致性 | AVAILABLE_AND_CHECKED | stream + Voyage_Stop_Logic |
| 速度跟踪 RMS/max | AVAILABLE_AND_CHECKED | stream v_ref/v_ship（对比诊断） |
| 欠供负荷/推进缺口 | AVAILABLE_AND_CHECKED | 09 母线残差 + 08 降额激活时长 |
| SOC 硬界越限时间 | AVAILABLE_AND_CHECKED | stream SOC |
| 电池 SOP/变换器/V/I 越限 | AVAILABLE_AND_CHECKED | stream P_bat_chg/dis_avail、V_bat、I_bat + ECM flags |
| 机组 Pmin/Pmax/ramp 越限 | AVAILABLE_AND_CHECKED | stream state_DG1/2 + P_gen_actual |
| 终端 SOC 容许性 | AVAILABLE_AND_CHECKED | stream SOC（DGN-F-05） |
| 母线能量闭合残差 | AVAILABLE_AND_CHECKED | gate_ctrl.check_c5 复用 |
| Safety S1–S5 计数/时长 | AVAILABLE_AND_CHECKED | logsout 安全 flags（Phase B 已仪表化） |
| PMS override 计数/时长 | AVAILABLE_AND_CHECKED | logsout 九元组（gate_ctrl.audit_rows_of） |
| 启停计数 / TV_Pgen / ramp 激活 | AVAILABLE_AND_CHECKED | stream 派生（对比诊断） |
| E_bat_throughput / EFC_proxy | AVAILABLE_AND_CHECKED | stream P_bat_actual 积分（对比诊断） |

### 12.2 硬可行性最小集与冻结 PASS 判据（R9：全 AVAILABLE_AND_CHECKED ⇒ 全部真物理硬约束入硬集）

| 硬检查 | 冻结 PASS 判据 |
|---|---|
| 任务完成 | sim 由 Voyage_Stop_Logic 正常终止且 \|T_end − T_frozen_stream\| ≤ 1 s 🔒 |
| 欠供负荷/推进缺口 | 08 降额激活时长 = 0 s 且母线欠供能量（P_residual<−1 kW 段积分）= 0 kWh 🔒 |
| SOC 硬界 | SOC∉[0.10, 0.95] 越限时间 = 0 s（模型积分器限幅同域） 🔒 |
| 终端 SOC 容许 | \|SOC_end − SOC0\| ≤ 0.005（DGN-F-05）🔒 |
| 电池 SOP/V/I | \|P_bat\| 超 avail 时长 = 0 s；V_bat∉[360,657] V 时长 = 0 s；\|I_bat\| > 1823 A 时长 = 0 s（容差 1e-6）🔒 |
| 机组 Pmin/Pmax/ramp | ON 态 P∉[125,500] 时长 = 0 s；\|dP/dt\| > 20 kW/s+1e-6 时长 = 0 s；状态机迁移合法性 = 既有冻结判据 C2（`gate_baseline_modele_control.json` v1.0，hash 实现期登记） |
| 母线/Plant 能量闭合 | 既有冻结判据 C5：\|∫P_residual dt − ΔE_cap\|/E_throughput < 0.1%（同上引用） |
| Safety 硬保护事件 | 冻结安全设计分类为硬保护的事件计数 = 0：S1a 非预期丢失 / S1b 启动失败 / S2 realized 供给缺口（C6 语义引用同上冻结基线） |

对比诊断（不硬阻断）：速度跟踪、PMS override、S3/S4/S5 计数、TV_Pgen、ramp 激活、ON-hours、E_bat_throughput、EFC_proxy——除非既有冻结 Model E Gate 已将其分类为硬失败。

**任何硬可行性失败先于普通 GO/NO-GO 分类**（DGN-F-11 优先级 1）。AVAILABLE_AND_CHECKED 不等于 PASS——PASS 以上表冻结判据为准（R9）。

## 13. B0/B1 回归语义（DGN-F-13，R7@requirements）🔒

- **B0 回归**：3 个固定参考航次（S20 家族既有 smoke 航次），law=5 全信号对冻结基线逐样本一致（容差 0，10b/22 号 Gate 链路复用）；
- **B1 核心回归**：law=6 核心 scalar→ECMS 映射与 `+ecms2/test_ecms_law.m` 单测 9 逐位一致（容差 0）+ K_S 通道合成恒等式 |K_soc·K_S·ê − Δλ| < 1e-12；
- **parity 适配器范围**：输入数据预制（真 ZOH 节点、shrinking-horizon E300、§2.2 冻结常数）+ per-run K_S + per-run ZOH 块参数覆盖；不触碰 law=6 核心；不要求正式 B1 全航程与历史 law=6 运行逐点匹配（历史 = 连续插值 + 尾端零填充 + 逐航程归一化）。

## 14. 统计实现（DGN-F-14，FR-770~776）🔒

- 后处理 = Python（`.venv_tf`，numpy）；逐 run JSON（MATLAB 产出）→ h_v 配对 → h_g 组均值 → median_g；
- bootstrap：分层配对 split_group cluster bootstrap，B=10000，种子冻结 `phase4f-t445-bootstrap-v1`；每层有放回重抽 3 组（扩样后 4 组），组完整携带全部成员与配对；95% percentile CI；
- 单测（AC-616）：合成已知效应数据集验证配对完整性/族闭包/分层重抽/CI 覆盖；同种子复现（两次运行 CI 逐位一致）；
- 全部报告携带 `SMALL_N_GATE_CAUTION`（有效 n=9 簇）。

## 15. 事故与不可行处理（DGN-F-15，FR-777）🔒

- 机械/基建失败（崩溃、I/O、许可证）：按 DGN-F-10 七元组身份重跑 + `incidents.jsonl` 记录；
- 持续控制器/物理不可行：默认期望 0；出现即 `GATE_VALIDITY_FAIL` + AUDIT_REQUIRED，禁止成对删除；
- B2 单窗不可行回退（DGN-F-09）超 5%/航次 → persistent-infeasibility 候选。

## 16. 组件与目录

| 路径 | 内容 | 入库 |
|---|---|---|
| `model/simulink_modelE_(Phase4_Physics)/scripts/+t445/` | oracle 窗装配、ZOH 数据预制、B2 窗内 DP、策略表构建、分段 runner、SimState 管理、逐 run 指标提取 | ✅ |
| `model/simulink_modelE_(Phase4_Physics)/config/phase4f_t445_config.m` | 全部 🔒 冻结参数（k_eq^E、E_ref/E_scale、网格、阈值、种子、带） | ✅ |
| `model/simulink_modelE_(Phase4_Physics)/python/t445/` | compute_b1_norm.py（§2.2 复现）、语料选择、形状波动评分、bootstrap、Gate JSON、报告 | ✅ |
| `model/simulink_modelE_(Phase4_Physics)/results/t445_oracle_gate/` | corpus manifest、逐 run JSON、gate_result.json、报告（精选入库） | ✅ 精选 |
| `model/simulink_modelE_(Phase4_Physics)/artifacts/t445/` | SimState checkpoint、段日志（本地） | ❌ 不入库 |
| 单测 | `scripts/+t445/tests/` + `python/t445/tests/` | ✅ |

Model E `.slx` **零修改**（DGN-F-01）；Model E 基线 Gate（10b smoke / GP1 / 控制 Gate）在实现期首任务回归确认不受影响。

## Testing Strategy（§17）

| 层 | 内容 | 对应 AC/资格化 |
|---|---|---|
| 单测（MATLAB） | 窗装配/bin 能量/partial bin；真 ZOH 适配器（major/minor/边界，§2.3）；策略表 vs ecms_law；窗内 DP toy；V_term 反套利有效性（AC-613 修订语义）；SimState 分段一致性 | AC-610/611/612/613/618；B2-QUAL Q1/Q5 |
| 单测（Python） | compute_b1_norm 复现冻结常数；形状波动评分；SHA256 排序复现；语料 manifest；bootstrap 合成覆盖；Gate 裁决全分支+优先级 | AC-615/616/617 |
| 预结果资格化 | B2_OPTIMIZER_QUALIFICATION（§9.1 Q1~Q6）；k_eq^E 充电回补 fixture（§6.4）；B3 parity fixture（§8.2-4） | R5/R7/R8 |
| 集成 | B0 参考航次回归；B1 夹具回归；B2 单窗 smoke（合成负荷） | AC-618 |
| 正式前冻结检查 | config 哈希、语料 manifest、test_sealed 复核、TERMINAL_SOC_ADMISSIBILITY 先于结果观察断言、硬可行性判据表完整性 | AC-614/615 |
| 正式执行 | 36 航次 ×（B0/B1/B2）+ B3 资格化；incident 政策；checkpoint/resume | FR-777 |
| Gate | 机器 JSON + 双字段裁决 + 报告 | AC-617/620 |

## Requirements Traceability（§18，FR → DGN）

| FR | DGN | FR | DGN |
|---|---|---|---|
| FR-720/721/722 | DGN-F-01/02/13 | FR-740~743 | DGN-F-06 |
| FR-723/724 | DGN-F-01/03 | FR-744 | DGN-F-05 |
| FR-725/733 | DGN-F-02 | FR-745 | DGN-F-06 |
| FR-726/727/728 | DGN-F-01（诊断臂复用同一 runner，标记隔离） | FR-746~748/750 | DGN-F-12 |
| FR-730/731/732 | DGN-F-09/10 | FR-749 | DGN-F-08 |
| FR-734 | DGN-F-08/09 | FR-760~764 | DGN-F-07 |
| FR-735 | DGN-F-04 | FR-770~776 | DGN-F-11/14 |
| FR-777 | DGN-F-15 | FR-778 | DGN-F-08/11 |
| FR-779 | DGN-F-08（H_residual 由 B2/B3 J_FEC 派生，B3 资格化后可用） | FR-780/781/782 | DGN-F-11/14 + config 哈希链 |
| NFR-710~716 | DGN-F-01/10/16 | AC-610~620 | §17 测试层映射 |

## 19. 风险与已知限制

1. **B2 代理简化**（机组爬坡/惯性/PMS 滞后忽略）可能降低可提取余量；正式 Gate 有效性依赖 §9.1 资格化（R8 措辞）——最终结果以真实冻结 plant 实测为准；
2. **策略表 λ 最近邻索引**（0.005 网格）引入窗内模型舍入；Q1/Q4 资格化覆盖；
3. **SimState 兼容性**（Stateflow/persistent/From Workspace/ode3）由 DGN-F-10 前置 Gate 验证，失败升级主 Agent；
4. **小样本**（9 簇）CI 宽 → INCONCLUSIVE 概率非平凡；一次性扩样政策已预注册；
5. **law=6 floor（0.43）> λ₀（0.425）** 构成 B2 下行权限的冻结族作用空间限制（不声称已证明的单调保守效应，R3）；floor/cap 激活比例纳入诊断；
6. **小概率 P_load 实现漂移**：B2 调度改变母线压力时，08 降额可能改变 realized P_load vs 冻结真值——硬可行性集（欠供=0）+ parity 断言双重拦截。

## 20. OQ 闭合清单

| OQ | 状态 | DGN |
|---|---|---|
| OQ-F1 刷新/末端（+B1 归一化契约 R1/R2） | ✅ 闭合 | DGN-F-02 |
| OQ-F2 Δλ 容许集（R3） | ✅ 闭合 | DGN-F-03 |
| OQ-F3 终端机制（R4） | ✅ 闭合 | DGN-F-04 |
| OQ-F4 SOC 容许带 | ✅ 闭合 | DGN-F-05 |
| OQ-F5 k_eq^E（R5） | ✅ 闭合（数值 0.5296 + 聚合基线检查 + 回补 fixture） | DGN-F-06 |
| OQ-F6 波动评分（R6 形状评分） | ✅ 闭合 | DGN-F-07 |
| OQ-F7 B3 资格化（R7） | ✅ 闭合 | DGN-F-08 |
| OQ-F8 求解器（R8 资格化 Gate） | ✅ 闭合 | DGN-F-09 |
| OQ-F9 交替执行/预算（+checkpoint 身份强化） | ✅ 闭合 | DGN-F-10 |

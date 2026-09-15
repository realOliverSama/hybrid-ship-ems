# Phase 4-F T-445 Trajectory Oracle Gate Design

Spec: `phase4-f-trajectory-oracle-gate`  
Status: design-draft  
Created: 2026-09-15  
Requirements: `./requirements.md`（requirements-approved，2026-09-15，R1~R7 闭合）  
Brainstorm: `./brainstorm.md`（Q1~Q4 CLOSED）

本文件把 requirements 翻译为可施工设计，并**逐条闭合 OQ-F1~F9**（requirements「Open Questions」节）。所有数值冻结项在对应 DGN 条目中标注 🔒。

## 1. 架构总览

```text
                     ┌──────────────────────────────────────────────┐
                     │  冻结真值源（只读）                            │
                     │  datasets_v3/formal/iid/<voyage>.mat          │
                     │  （stream.P_load_total_kW @1Hz + meta）       │
                     └───────┬──────────────────┬───────────────────┘
                             │                  │
                    oracle 窗装配器      波动评分/语料选择（arm-independent）
                             │                  │
        ┌────────────────────┼──────────────────┼──────────────────┐
        │ B0: law=5          │ B1: law=6        │ B2: law=6 +      │ B3: 离线全航程 DP
        │ 单次全程 sim        │ K_S=0.01 标量通道 │ K_S=1/30 Δλ 通道  │ （parity 资格后审）
        │                    │ ê(t) 10s-ZOH     │ Δλ(t) 10s-ZOH     │
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

**关键设计定案（DGN-F-01）：B1 与 B2 共乘现役 law=6 变体，零 `.slx` 结构修改。**

law=6 核心：λ(t) = λ₀ + K_soc·(SOC_ref + K_S·ê(t) − SOC(t))，λ_eff = max(·, Lambda_Floor)（K_S≠0 时生效）。其监督通道增益为 K_soc·K_S：

- **B1**：K_S = 0.01（canonical 值不变），ê(t) = (E300_true(t0) − E_ref)/E_scale（10s ZOH 楼梯信号，归一化常数冻结，见 DGN-F-02）；
- **B2**：K_S = 1/K_soc = 1/30（per-run 参数覆盖，base workspace 注入），ê(t) := Δλ_traj(t0)（kg/kWh），则 K_soc·K_S·ê = Δλ_traj —— λ_B2 = λ₀ + K_soc·(SOC_ref−SOC) + Δλ_traj 精确成立（30×(1/30)=1 的浮点残差 ~1e-16，可忽略，由 AC-618 夹具回归锁定）。

备选方案（新增 law=8 变体、模型结构手术）**被否决**：对冻结基线的无谓扰动，且 law=6 已有 oracle 专用 From Workspace 通道（ECMS_EHat_Data）与 E5 红线注释。R7 parity 适配器的实现形式 = **输入数据预制（10s ZOH 楼梯化）+ per-run K_S 参数**，law=6 核心控制逻辑零改动。

ZOH 实现：From Workspace 块 Interpolate=on 保持不变，但输入时间序列按 0.01 s 稠密采样且值已按 10 s 楼梯化（相邻节点值相等 → 插值退化为精确 ZOH）。数据量 ~400k×2 double ≈ 6.4 MB/航次，可接受。

## 2. OQ-F1 —— 刷新节奏 / 视界 / 末端政策（DGN-F-02）🔒

- **刷新节奏**：t0 ∈ {0, 10, 20, …, T_end−10}，**10 s sample-and-hold**（与 30×10 s 轨迹表示对齐；B1/B2 同一节奏，FR-725/733）。
- **oracle 窗**：W(t0) = [t0, min(t0+300, T_end)]；窗内 1 Hz 真值 P_load_total_kW(t) 来自冻结航次工件 stream；bin k = [t0+10(k−1), t0+10k)，P̄_k = bin 内 10 个 1 Hz 样本均值；**bin 能量 E_k = (10/3600)·P̄_k kWh**；E300_true(t0) = Σ_k E_k（与 FR-721 公式 (10/3600)ΣP̄_k 恒等）。
- **末端政策 = shrinking-horizon**：航程末端窗长随剩余时间收缩（K_rem < 30），**不做零填充**（历史 law=6 的尾端零填充语义随之废止）；B1 的 E300_true 与 B2 的窗内优化使用**同一收缩窗**。
- **partial final bin**：T_end 不整除 10 s 时，最后一个部分 bin 按真实持续时长加权（E_partial = (Δt_rem/3600)·P̄_partial），B1/B2 同规则。
- **末段 oracle 停摆**：T_end−10 < t 段不再刷新，B1/B2 均保持最后刷新值（HoldingFinalValue 语义已由楼梯数据自然实现）。

## 3. OQ-F2 —— B2 监督作用空间（DGN-F-03）🔒

- **决策变量**：Δλ_k ∈ ℝ，k = 1..K_rem（K=30 对应 300 s 窗），每 10 s bin 分段常值。
- **容许集合**：Δλ_k ∈ [−0.10, +0.10] kg/kWh，离散网格步长 0.005（41 levels）；速率限制 |Δλ_{k+1} − Δλ_k| ≤ 0.02 kg/kWh/bin。🔒
- **执行合成**：λ_B2(t) = max( λ₀ + K_soc·(SOC_ref − SOC(t)) + Δλ(t), Lambda_Floor=0.43 ) —— 与 law=6 家族语义一致（悬崖 0.41 + 0.02 裕量防发电塌缩支路）；λ 上限硬钳 0.80（防御性，正常不触碰，钳位激活计数入审计）。🔒
- **执行链冻结**：B2 只写 Δλ_traj 通道；ECMS 瞬时 argmin（0:20:1000 kW 网格 + SOC 硬约束 0.105/0.945 + 怠速项）→ 07_PMS_v2 → 16_Safety_Interlock → Plant 全部不改（FR-723/724）。
- **已知家族约束（登记非缺陷）**：λ₀=0.425 < Lambda_Floor=0.43，B2 向下权限受 floor 约束（SOC≥SOC_ref 时 λ 不会低于 0.43）——此为冻结 ECMS 族固有语义，方向保守（低估而非高估余量），符合 Gate 安全方向。
- **浮点说明**：B2 经 K_S=1/30 通道，AC-618 夹具回归验证 λ 合成与直接加法差 < 1e-12。

## 4. OQ-F3 —— B2 终端能量机制（DGN-F-04）🔒

窗内优化目标（每 t0 的窗 W）：

```text
min_{Δλ_1..K}  Σ_{k=1..K} fuel_model,k(Δλ_k; x_k)  +  V_term(SOC_H)
```

**V_term = 线性终端价值 + 带外二次罚**：

```text
V_term(SOC_H) = −k_eq^E · Cap · (SOC_H − SOC_ref)
              + w_term · max(0, |SOC_H − SOC_ref| − δ_term)²
```

- 线性项把终端储能按 **k_eq^E**（与 FEC 指标同一折算率，DGN-F-06）定价 → 窗末放空电池的"收益"被等值扣回，**从机制上消除跨视界 SOC 欠账套利**（FR-735）；
- 参数冻结：SOC_ref = 0.60；δ_term = 0.01；w_term = 1.2e5 kg/SOC²（标定逻辑：0.01 带外偏移的线性信用 ≈ k_eq^E·Cap·0.01 ≈ 5.7 kg，二次罚在该点施加 2× 超额 = 11.4 kg → w·(0.01)² = 11.4 → w ≈ 1.14e5，取 1.2e5）；🔒
- Cap = 1071.36 kWh；
- **反作弊验证（AC-613）**：合成常负荷 + 窗末放空诱惑场景，B2 窗内优化解的 SOC_H 不得越出 δ_term 带（除物理不可行外），证据落盘。

## 5. OQ-F4 —— TERMINAL_SOC_ADMISSIBILITY（DGN-F-05）🔒

- **正式规则**：航次末端 |SOC_end − SOC0| ≤ **0.005**（与 Phase 1 DP 终端带、T-104 标定惯例一致）；
- 适用全部正式臂（B0/B1/B2/B3 同一规则）；SOC0 为冻结航次初始 SOC（同一航次各臂相同）；
- 越限 run = `INFEASIBLE_OR_NONCOMPARABLE`，FEC 不拯救（FR-744）；持续不可行走 FR-777 → `GATE_VALIDITY_FAIL`（禁成对删除）；
- 本规则先于任何正式 B1/B2 结果观察冻结（本文件即预注册载体）。

## 6. OQ-F5 —— k_eq^E 与 Model E 能量边界审计（DGN-F-06）🔒

**能量边界审计结论**（`NO_DOUBLE_COUNTING_OF_ENERGY_LOSS` 检查通过）：

| 路径环节 | Model E 实现 | 是否进 η_path^E | 理由 |
|---|---|---|---|
| 机组 AC→母线 | η_rect = 0.97（ASSUMED，功率平衡显式乘除） | ✅ 计入 | 补给链必经，SOC 记账未含 |
| 母线↔电池端子 | η_dcdc = 0.97（14_Battery_Converter SOP/换算显式使用） | ✅ 计入 | 同上 |
| 端子→储能 | 1RC ECM：R_0/R_1 I²R 物理损耗 + 库仑效率 η_coul=0.98 仅进 SOC 计量（Q5 红线注释：不乘端口功率，避免双计） | ✅ 以 ECM 推导参考效率计入一次 | SOC 记账是"已发生事实"，FEC 修正是"补给所需燃料"，二者不同环节，不双计 |
| ASSUMED_Bat_Eff_* (0.95/0.95) | 仅 ECMS 控制器内部 mbe 口径 | ❌ 不计入 k_eq^E | 控制器内部模型与度量折算无关 |

**冻结数值**（本 design 计算并冻结）：

- BSFC_ref^E = **482.749 g/kWh**：Model E 五节点 PCHIP 表 {125:600, 250:547, 375:447, 400:427, 500:453} 在单机工作区间 [125,500] kW 的**均匀能量加权** ∫P·BSFC(P)dP/∫P dP（非五节点算术均值 494.8——算术均值向高 BSFC 的低载区偏置，能量加权反映"燃油主要烧在大功率段"的物理事实，FR-741 合规）；
- 电池充电参考效率：SOC=0.60、I_ref = 0.25C = 465 A 参考点，V_t_chg = OCV + I·(R_0+R_1) = 593.42 + 465×(0.00702+0.00785) = 600.33 V，η_chg_ref = η_coul·OCV/V_t_chg = 0.98×593.42/600.33 = **0.9687**；
- η_path^E = 0.97 × 0.97 × 0.9687 = 0.9111；
- **k_eq^E = (482.749/1000) / 0.9111 = 0.5296 kg/kWh** 🔒（Model D 旧值 0.499 废止，偏差 +6.1%）；
- 稳健性报告点：0.8× = 0.4237，1.2× = 0.6355；break-even 分析按 FR-745 公式。

## 7. OQ-F6 —— 波动评分与语料选择（DGN-F-07）🔒

- **航次波动评分**（臂独立，仅用冻结 P_load 真值）：

```text
score_v = std_t(E300_series(t)) / mean_t(E300_series(t))
```

其中 E300_series(t) = 该航次冻结 stream 在 10 s 采样网格上的收缩窗 E300 序列（与 DGN-F-02 同口径）。评分 = E300 序列的变异系数（CV），无量纲、与负荷水平脱敏、直接对准科学问题（轨迹信息价值存在于 300 s 前瞻能量波动时）。🔒（公式冻结；数值由代码从冻结数据计算）

- **聚合**：split_group 评分 = 组内 4 成员航次 score_v 算术均值；
- **分层**：27 个 val split_group 按组评分降序排名 → HIGH 9 / MID 9 / LOW 9 三分层；
- **确定性排序**：tier 内按 sha256("phase4f-t445-corpus-v1|" + split_group_id) 十六进制升序；
- **初始集**：每层前 3 组（9 组 36 航次）；**预留集**：每层第 4 组（3 组 12 航次）；禁结果导向替换；
- **语料 manifest**：`results/t445_oracle_gate/corpus_manifest.json`（航次 ID/分组/层级/排序哈希/评分值/种子），先于任何正式臂执行冻结（FR-764）；
- **数据来源合法性**：split 指派 = `results/dataset_v3/formal/split_assignment_v3.json`（sha ac4c4a76…）；27 个 val 组 = assignment 中 split=="val" 的 split_group；test 封印不触碰（AC-615 复核 manifest test_sealed=true）。

## 8. OQ-F7 —— B3 资格化协议（DGN-F-08）🔒

**B3 定义**：离线全航程直接功率 DP（复用 `+ecms/dp_problem`/`dp_solve` 机械件），Model E 适配边界：

- **燃油模型**：Model E 真实双机 PCHIP 的聚合复合图：P ≤ 500 kW → 单机 PCHIP(P)；P > 500 kW → 双机均分 2×PCHIP(P/2)（对应 PMS 均分 clamp [125,500] 语义）；怠速流量与 C_start=0.6 kg/次启机按 Model E 参数；启停口径 mode='startstop'，P_min_stable=125，P_max=1000；
- **状态/网格**：(SOC, P_prev)，SOC 网格 0.10:0.005:0.95，P 网格 {0}∪[125:20:1000]，dt=1 s；爬坡 R_gen=20 kW/s（单机保守口径，登记为 B3 内模型简化）；
- **终端**：|SOC_T − SOC0| ≤ 0.005（与 DGN-F-05 同带）；SOC0/Cap/η 链与 DGN-F-06 一致（η_bat 用 0.9686 充放对称参考值——由 η_chg_ref=0.9687 与 η_dis_ref=η_coul·(OCV−I(R0+R1))/OCV=0.9685 取均值，差 < 0.001）；
- **负荷真值**：与 B0/B1/B2 同一冻结 stream（shrinking 不适用——B3 全航程）。

**资格化协议**（全部通过才 `QUALIFIED_PASS`）：

1. 燃油图同一性：B3 复合燃油图 vs 模型内 FuelModelE per-genset PCHIP 在网格点比对，max rel err < 0.5%；
2. 网格收敛：dSOC 0.005 vs 0.0025 重解，全程燃油差 < 0.5%（沿袭 T-012 判据）；
3. 能量闭合/可行性：dp_checks 既有套件（电池记账 < 0.1%、SOC 界、爬坡窗、终端带）；
4. 记账 parity（FR-749）：同 raw fuel 定义、同 k_eq^E、同 FEC、同可行性/负担报告。

**dominance 审计**：ε_num = 0.005·J_FEC,B2（相对 0.5%，对齐 DP 网格精度量级）🔒；违反 J_FEC,B3 ≤ J_FEC,B2 + ε_num → `b3_context_status = B3_DOMINANCE_AUDIT_FAIL`；未资格化 → `UNQUALIFIED_CONTEXT`，H_arch/H_residual 不产出（FR-778/R4）。

## 9. OQ-F8 —— B2 窗内优化器（DGN-F-09）🔒

- **求解器类**：窗内**确定性 DP**（后向归纳，复用 dp_solve 数值模式：big-M=1e4、V 线性插值、前向重建）——B2 不称严格数学上界（网格离散 + 内模型近似，FR/术语节一致）。
- **决策步**：10 s（K=30 步/窗），bin 均负荷 P̄_k；
- **状态**：(SOC, P_g_prev)，SOC 网格 0.10:0.01:0.95（86 点，窗内精度与速度折中），P_g_prev 网格 0:20:1000；
- **转移模型（内模型）**：给定 λ_k = max(λ₀ + K_soc(SOC_ref−SOC_k) + Δλ_k, 0.43)，P_g* = **预计算 ECMS 策略表查值**——策略表 = `ecms2.ecms_law` 离线逐位复制品在 λ ∈ [0.43:0.005:0.80]（76 点）× P̄ ∈ [0:20:1300]（66 点）× SOC 区域 {正常 / ≤0.105 禁放 / ≥0.945 禁充} 上的穷举（含同一 BSFC 表、怠速项、硬约束），表构建时与 `ecms2/test_ecms_law.m` 单测交叉锁定；SOC 转移：`ecms.bat_energy_delta_kWh`（η_dcdc=0.97、η_bat=0.9686 充放对称值）；机组爬坡/惯性/PMS 分配滞后在内模型中忽略（10 s bin ≫ τ=5 s；登记为内模型简化，方向保守——B2 策略在真实 plant 上只会更差 → 低估余量，对 Gate 安全）；
- **目标**：§4 窗内燃油 + V_term；
- **首段执行**：仅 Δλ_1 注入下一 10 s 段（10 s ZOH），其余丢弃，下一 t0 重解（FR-730）；
- **不可行回退**：单窗 DP 无可行解（全 V ≥ big-M）→ 本段 Δλ ≡ 0（退化为 law=5）+ incident 计数；单航次不可行窗占比 > 5% → 该航次标记 persistent-infeasibility 候选，走 FR-777 审计（默认期望 0）；🔒
- **求解预算**：单窗 DP ≈ 30 步 × 86 SOC × 51 P_g × 41 Δλ 转移（查表化）≈ 5.4M 转移/解，向量化 MATLAB 目标 ≤ 2 s/窗（实现期首个任务在合成 fixture 上 benchmark 验证，超标则先优化再进正式语料——运行时压力不削弱科学语义，NFR-714/R6）。

## 10. OQ-F9 —— Simulink/优化器交替执行（DGN-F-10）🔒

- **B0/B1**：单次全程 sim（既有 `model.run_scenario_sim` 链路，EMS_CTRL_LAW=5/6 + per-run ECMS_EHat_Data/ECMS_K_S 注入）；
- **B2 交替循环**（MATLAB 脚本驱动，单会话或 parfor 航次级并行）：
  1. 段边界 t0 = 0,10,20,…；sim 段 [t0, t0+10]，`SaveFinalState` → SimState；
  2. 从 logsout 尾部读闭环状态（SOC、P_gen_actual、commitment 状态）作为窗内 DP 初值（SOC 为关键状态，其余由内模型简化忽略——登记）；
  3. 窗内 DP 求解 → Δλ_1；
  4. 更新 ECMS_EHat_Data（楼梯序列，自 t0 起恒为 Δλ_1）→ 以 SimState 恢复续跑下一段；
  5. 重复至 T_end；
- **SimState 正确性 Gate（先于正式语料）**：同一航次 B1 连续跑 vs 分段跑（每 10 s save/restore）全信号逐样本一致（|Δ| ≤ 1e-10 相对）——验证 From Workspace 绝对时间语义 + Stateflow/persistent 状态续存；
- **checkpoint/resume**：每段持久化 SimState + Δλ 决策日志 + 段指标 → 中断后按 (voyage_id, arm, config_hash, seed) 确定性续跑；重跑身份 = 四元组相同（FR-777 机械重跑语义）；
- **oracle 提取时点**：段边界 t0 从冻结 stream 装配窗（DGN-F-02），与 sim 时钟对齐断言（容差 0.005 s）；
- **运行预算估算**（R6/NFR-714，基于 73 号 ledger 实测 sim_wall 中位 73.6 s/航次）：
  - B0+B1：36 航次 × 2 臂 × ~75 s ≈ **1.5 h**；
  - B2：36 航次 × (~75 s sim 分段 + ~400 窗 × 2 s 求解) ≈ 36 × 875 s ≈ **8.8 h 串行**（parfor 6 worker ≈ 1.5~2 h；checkpoint 覆盖中断）；
  - B3：36 航次 × ~2 min ≈ **1.2 h**；
  - 初始语料合计 ≈ **4~11 h**（串行上限 ~11 h，并行 ~4 h）；预留 +12 航次 ≈ **+1.5~4 h**；
  - 实际 wall time 执行后报告；运行时不构成修改求解器/语料/统计语义的理由（R6）。

## 11. 机器裁决正交字段与确定性优先级（DGN-F-11，R3）🔒

Gate JSON（`results/t445_oracle_gate/gate_result.json`）：

```json
{
  "schema": "t445_oracle_gate_v1",
  "trajectory_gate_verdict": "GO_TRAJECTORY_VALUE | NO_GO_TRAJECTORY_VALUE | INCONCLUSIVE | FINAL_INCONCLUSIVE | GATE_VALIDITY_FAIL",
  "b3_context_status": "UNQUALIFIED_CONTEXT | QUALIFIED_PASS | B3_DOMINANCE_AUDIT_FAIL",
  "decision_required": false,
  "expansion_used": false,
  ...
}
```

**确定性优先级**（先于正式结果冻结）：

1. `GATE_VALIDITY_FAIL` 条件（持续 B1/B2 不可行 > 0、硬性可行性/parity 检查失败、语料/manifest 完整性失败）**先于**普通分类求值，一旦成立直接置 verdict 并 `decision_required=true`；
2. 否则按 FR-774/775/776 分类（GO / NO-GO / INCONCLUSIVE）；
3. `INCONCLUSIVE`（初始语料）→ `expansion_used=true`，启用预留集全量重算（FR-776）→ 终判 GO / NO-GO / FINAL_INCONCLUSIVE（`decision_required=true`）；
4. `b3_context_status` 独立求值，永不改写 `trajectory_gate_verdict`；
5. 非 GO 终态均阻断 T-446；仅 `INCONCLUSIVE` 允许触发一次扩样（T-445 内）。

## 12. 审计能力矩阵与硬可行性最小集（DGN-F-12，R5）🔒

`audit_capability_matrix`（机器可读，config 登记 + 每 run 实例化）：信号源评估结论——冻结 stream（24 通道）+ logsout（Phase B 九元组审计 + S1–S5 flags + Vdc/SOC monitors）覆盖全部 FR-746~748 请求信号。

| 信号 | 状态 | 源 |
|---|---|---|
| 任务完成/时长一致性 | AVAILABLE_AND_CHECKED | stream + Voyage_Stop_Logic |
| 速度跟踪 RMS/max | AVAILABLE_AND_CHECKED | stream v_ref/v_ship |
| 欠供负荷（P_residual 缺口） | AVAILABLE_AND_CHECKED | 09_Bus_Residual + stream 功率链 |
| SOC 硬界越限时间 | AVAILABLE_AND_CHECKED | stream SOC |
| 电池 SOP/变换器/V/I 越限 | AVAILABLE_AND_CHECKED | stream P_bat_chg/dis_avail、V_bat、I_bat + ECM flags |
| 机组 Pmin/Pmax/ramp 越限 | AVAILABLE_AND_CHECKED | stream state_DG1/2 + P_gen_actual |
| 终端 SOC 容许性 | AVAILABLE_AND_CHECKED | stream SOC（DGN-F-05） |
| 母线能量闭合残差 | AVAILABLE_AND_CHECKED | gate_ctrl.check_c5 复用 |
| Safety S1–S5 计数/时长 | AVAILABLE_AND_CHECKED | logsout 安全 flags（Phase B 已仪表化） |
| PMS override 计数/时长 | AVAILABLE_AND_CHECKED | logsout 九元组（gate_ctrl.audit_rows_of 复用） |
| 启停计数 / TV_Pgen / ramp 激活 | AVAILABLE_AND_CHECKED | stream state_DG*/P_gen_actual 派生 |
| E_bat_throughput / EFC_proxy | AVAILABLE_AND_CHECKED | stream P_bat_actual 积分 |

**硬可行性最小集**（缺失即阻断 Gate）：任务完成、欠供负荷、SOC 硬界、终端 SOC 容许、母线能量闭合、Safety S1/S2 零触发。🔒 任何信号不得因"不便测量"静默省略；NOT_INSTRUMENTED/NOT_APPLICABLE 需 design 级登记（当前矩阵无此类）。

## 13. B0/B1 回归语义（DGN-F-13，R7）🔒

- **B0 回归**：36 航次语料外固定 3 个参考航次（S20 家族既有 smoke 航次），law=5 全信号对冻结基线逐样本一致（容差 0，既有 10b/22 号 Gate 链路复用）；
- **B1 核心回归**：law=6 核心 scalar→ECMS 映射在匹配输入夹具下与 `+ecms2/test_ecms_law.m` 单测 9 逐位一致（容差 0）+ K_S 通道合成恒等式 |K_soc·K_S·ê − Δλ| < 1e-12；
- **parity 适配器范围**：仅输入数据预制（ZOH 楼梯化、shrinking-horizon E300、冻结归一化常数）+ per-run K_S；不触碰 law=6 核心；不要求正式 B1 全航程与历史 law=6 运行逐点匹配（历史为连续插值 + 尾端零填充 + 逐航程归一化语义，与新冻结 parity 语义不同）。

## 14. 统计实现（DGN-F-14，FR-770~776）🔒

- 后处理 = Python（`.venv_tf`，numpy）；逐 run JSON（MATLAB 产出）→ 航次级配对 h_v → 组均值 h_g → median_g；
- bootstrap：分层配对 cluster bootstrap，B=10000，种子冻结 `phase4f-t445-bootstrap-v1`；每层有放回重抽 3 组（扩样后 4 组），组完整携带全部成员与配对；95% percentile CI；
- 单测（AC-616）：合成已知效应数据集验证配对完整性、族闭包、分层重抽、CI 覆盖；同种子复现性（两次运行 CI 逐位一致）；
- 小样本警示：全部报告携带 `SMALL_N_GATE_CAUTION`（有效 n=9 簇）。

## 15. 事故与不可行处理（DGN-F-15，FR-777）🔒

- 机械/基建失败（崩溃、I/O、许可证）：同 (voyage_id, arm, config_hash, seed) 重跑 + `incidents.jsonl` 记录（时间戳/错误/重跑次数）；
- 持续控制器/物理不可行：默认期望 0；出现即 `GATE_VALIDITY_FAIL` + AUDIT_REQUIRED，禁止成对删除；
- B2 单窗不可行回退（DGN-F-09）超 5%/航次 → persistent-infeasibility 候选。

## 16. 组件与目录

| 路径 | 内容 | 入库 |
|---|---|---|
| `model/simulink_modelE_(Phase4_Physics)/scripts/+t445/` | oracle 窗装配、ZOH 数据预制、B2 窗内 DP、策略表构建、分段 runner、SimState 管理、逐 run 指标提取 | ✅ |
| `model/simulink_modelE_(Phase4_Physics)/config/phase4f_t445_config.m` | 全部 🔒 冻结参数（k_eq^E、网格、阈值、种子、带） | ✅ |
| `model/simulink_modelE_(Phase4_Physics)/python/t445/` | 语料选择、波动评分、bootstrap、Gate JSON、报告 | ✅ |
| `model/simulink_modelE_(Phase4_Physics)/results/t445_oracle_gate/` | corpus manifest、逐 run JSON、gate_result.json、报告（精选入库） | ✅ 精选 |
| `model/simulink_modelE_(Phase4_Physics)/artifacts/t445/` | SimState checkpoint、段日志（本地） | ❌ 不入库 |
| 单测 | `scripts/+t445/tests/` + `python/t445/tests/` | ✅ |

Model E `.slx` **零修改**（DGN-F-01）；Model E 基线 Gate（10b smoke / GP1 / 控制 Gate）在实现期首任务回归确认不受影响。

## Testing Strategy（§17）

| 层 | 内容 | 对应 AC |
|---|---|---|
| 单测（MATLAB） | 窗装配/bin 能量/partial bin；ZOH 楼梯生成；策略表 vs ecms_law 逐位；窗内 DP toy 收敛；V_term 反作弊（AC-613 合成场景）；SimState 分段一致性 | AC-610/611/612/613/618 |
| 单测（Python） | 波动评分、SHA256 排序复现、语料 manifest；bootstrap 合成数据覆盖；Gate 裁决全分支+优先级 | AC-615/616/617 |
| 集成 | 参考航次 B0 基线回归；B1 夹具回归；B2 单窗 smoke（合成负荷） | AC-618 |
| 正式前冻结检查 | config 哈希、语料 manifest、test_sealed 复核、TERMINAL_SOC_ADMISSIBILITY 先于结果观察断言 | AC-614/615 |
| 正式执行 | 36 航次 ×（B0/B1/B2）+ B3 资格化；incident 政策；checkpoint/resume | FR-777 |
| Gate | 机器 JSON + 双字段裁决 + 报告 | AC-617/620 |

## Requirements Traceability（§18，FR → DGN）

| FR | DGN | FR | DGN |
|---|---|---|---|
| FR-720/721/722 | DGN-F-01/13 | FR-740~743 | DGN-F-06 |
| FR-723/724 | DGN-F-01/03 | FR-744 | DGN-F-05 |
| FR-725/733 | DGN-F-02 | FR-745 | DGN-F-06 |
| FR-726/727/728 | DGN-F-01（诊断臂复用同一 runner，标记隔离） | FR-746~748/750 | DGN-F-12 |
| FR-730/731/732 | DGN-F-09/10 | FR-749 | DGN-F-08 |
| FR-734 | DGN-F-08/09 | FR-760~764 | DGN-F-07 |
| FR-735 | DGN-F-04 | FR-770~776 | DGN-F-11/14 |
| FR-777 | DGN-F-15 | FR-778 | DGN-F-08/11 |
| FR-779 | DGN-F-08（H_residual 由 B2/B3 J_FEC 派生） | FR-780/781/782 | DGN-F-11/14 + config 哈希链 |
| NFR-710~716 | DGN-F-01/10/16 | AC-610~620 | §17 测试层映射 |

## 19. 风险与已知限制

1. **B2 内模型简化**（机组爬坡/PMS 滞后忽略）→ B2 策略次优，H_traj 被低估（保守方向，Gate 安全）；per-window 预测 vs 实测段成本 parity 作为诊断量报告；
2. **策略表网格**（λ 76 × P̄ 66）插值误差 → 由 toy 收敛测试 + 段 parity 诊断监控；
3. **SimState 兼容性**（Stateflow/persistent/From Workspace）→ DGN-F-10 前置 Gate 验证，失败则升级主 Agent 裁决（备选：全程重跑累积段，成本 36×~12 h 不可接受 → 需新方案）；
4. **小样本**（9 簇）：CI 宽 → INCONCLUSIVE 概率非平凡；一次性扩样政策已预注册；
5. **SOC 强反馈**（K_soc=30）压缩 B2 权限 → 轨迹余量可能本就有限；这正是 Gate 要回答的问题，不构成设计缺陷。

## 20. OQ 闭合清单

| OQ | 状态 | DGN |
|---|---|---|
| OQ-F1 刷新/末端 | ✅ 闭合 | DGN-F-02 |
| OQ-F2 Δλ 容许集 | ✅ 闭合 | DGN-F-03 |
| OQ-F3 终端机制 | ✅ 闭合 | DGN-F-04 |
| OQ-F4 SOC 容许带 | ✅ 闭合 | DGN-F-05 |
| OQ-F5 k_eq^E | ✅ 闭合（含数值 0.5296） | DGN-F-06 |
| OQ-F6 波动评分 | ✅ 闭合 | DGN-F-07 |
| OQ-F7 B3 资格化 | ✅ 闭合 | DGN-F-08 |
| OQ-F8 求解器 | ✅ 闭合 | DGN-F-09 |
| OQ-F9 交替执行/预算 | ✅ 闭合 | DGN-F-10 |

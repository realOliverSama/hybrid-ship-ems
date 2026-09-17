# T-011A — 正式可行性根因分析报告（Phase 4-F PPE, formal_epoch = 1）

机器可读版本：`T011A_REPORT.json`（sha256_self 见该文件）· 需求包络：`demand_envelope_stats.json`
授权范围：T-011A 仅做可行性根因分析；禁止任何效应估计（T-011B）与封存（T-012）。

## 1. epoch-1 完整性摘要

- 192/192 终端记录（P0/C0/C1/C2 各 48），48/48 COMMON_WARMUP_PARITY PASS
- `T010_ACCEPTANCE.json` pass=true，problems=0
- 身份统一：freeze `b4920a29…`，复合 `40c5be94…`（12 条过渡身份 P0 已隔离并按当前身份重跑）
- 方法学完整性事故：0；密封 IID/OOD 科学访问：0；无跨纪元混合；无按结局重跑

## 2. 谓词矩阵（按臂，192 条）

| 谓词 | P0 | C0 | C1 | C2 |
|---|---|---|---|---|
| task_complete | 0 | 0 | 0 | 0 |
| load_shortfall | 48 | 48 | 48 | 48 |
| soc_hard_bounds | 0 | 0 | 0 | 0 |
| terminal_soc | 36 | 36 | 36 | 36 |
| battery_sop_v_i | 0 | 0 | 0 | 0 |
| genset_limits | 48 | 48 | 48 | 48 |
| energy_closure_c5 | 0 | 0 | 0 | 0 |
| safety_hard_events | 0 | 0 | 0 | 0 |

分层：terminal_soc 失败率 LOW 64/64（100%）> MID 48/64（75%）> HIGH 32/64（50%）；
load_shortfall 与 genset_limits 在全部 tier/group/航次上无例外（12 个 split_group 全部命中）。
完整 per-voyage 明细见 JSON `per_voyage_detail`。

## 3. load_shortfall 边际分析

- 缺口功率当量（shortfall_kWh ÷ T_eval×3600）：全 192 条落在 **1.03–3.09 kW** 窄带（中位 1.66 kW）
- `derate08_s = 0`（192/192）：推进从未被 derate——真实持续缺功率会把母线压入 derate 带，未发生
- episode 级分析（时机/饱和/次数）：**不可用**——epoch-1 执行器未持久化逐时刻轨迹（设计缺口，如实记录）
- 机理判定：恒定小偏置 × 航次时长 → 1–3 kWh“缺口”。这不是真实的供电不足

## 4. genset_limits 分解

- 检查器语义：`hard_feasibility.m` 期望 `P_gen1_kW/P_gen2_kW`；Model-E 实际记录 `P_DG1_kW/P_DG2_kW`
  → 字段名不匹配 → 静默回退 `Pg1=Pg2=P_gen_actual/2`（均分假设）
- 均分假设错误的结构性证明：PMS v2 按设计不均等 commitment/调度（低载单机、DG1 基载/DG2 峰值）
- 观测：band_s 中位 1121 s（606–2110 s），band=0 的记录 0/192；
  **28/48 航次 band_s 四臂完全一致** → 航次低载时长决定，与控制器无关
- ramp 分量：~0 s（192/192）→ ramp 不是失败模式
- 精确逐机组重分解：需要分机组轨迹（未持久化）→ 局限

## 5. terminal SOC 分解（144/192 的解释）

- |ΔSOC|：min 0.0002 / 中位 0.0078 / max 0.0132（阈值 0.005 ≈ 5.4 kWh/1071 kWh 电池）
- 分布：≤0.005 48 条 | 0.005–0.01 96 条 | 0.01–0.0132 48 条
- **47/48 航次四臂 dSOC 全精度一致** → 终端 SOC 漂移由航次能量结构决定，与控制器无关
- 分层：LOW 100% > MID 75% > HIGH 50%（任务能量结构）
- 定性：边缘性物理不可行（最大超差 ~2.6× 阈值，绝对量 ~14 kWh）+ ±0.005 契约本身是否可达/是否合理的校准问题

## 6. toy fixture 对比（T-005）

T-005（toy S01 40s CONST400）当时即报 `hard_feasibility_pass=false`：shortfall_kWh=0.612（40 秒内）、
genset_band_s=10、ramp_s=0 → 与正式语料**完全相同的两个谓词、同一机理**（缩放不同）。
未阻止正式执行的原因：T-009 门验证的是可行性**仪表**（评估器能产出逐谓词判定），
TG4 规定不可行=科学结局而非 STOP；协议从未承诺语料可行。
**设计教训**：资格化验证的是仪表，不是正式队列的可行性保证。

## 7–10. 根因分类与未决项

| 机制 | 分类 | 关键证据 |
|---|---|---|
| load_shortfall | FEASIBILITY_CONTRACT_MISMATCH + NUMERICAL_OR_BOUNDARY_ARTIFACT | 残差监视器=电容功率一致性量（模型注释原文）；ASSUMED_eta_rect=0.97 INHERITED_LEGACY；真实缺功率信号（P_unserved_kW/P_gen_unserved_kW）存在但检查器未使用；derate=0；1.03–3.09kW 窄带偏置特征 |
| genset_limits | FEASIBILITY_CONTRACT_MISMATCH | 字段名错配→均分回退；均分对 PMS v2 不均等调度结构性错误；band_s 航次决定、臂不变 |
| terminal_soc | TRUE_PHYSICAL_INFEASIBILITY（边缘量）+ 契约校准问题 | 航次决定（47/48 全精度臂不变）；分层结构；量级 ≤0.0132 |

未决项：① 逐机组违规与缺口 episode 的精确分解（轨迹未持久化）；② ASSUMED_eta_rect 与真实整流效率的
偏差量；③ ±0.005 能量中性契约在该任务族上是否对任何持航控制器可达。

## 11. 建议类别（非授权）

**MIXED / DECISION_REQUIRED**
- (i) genset_limits 与 load_shortfall 为检查器语义错配 → 任何未来可行性重评需**检查器修正**
  （使用 P_DG1/P_DG2 分机组功率 + 采用真实缺供信号 P_unserved 语义），且因轨迹未持久化需**重执行语料**；
- (ii) terminal_soc 是真实的边缘性契约问题 → 需对 ±0.005 能量中性要求做**政策决定**；
- (iii) T-011B 效应估计继续 WITHHELD，直至人工 gate 对可行性契约做出裁决。

## 12–13. 当前 estimand 状态与非声明

B_perfect / B_predicted / L_prediction = **WITHHELD_INSUFFICIENT_FEASIBILITY**；R_diagnostic = **R_UNDEFINED**；
研究状态 = FORMAL_EXECUTION_VALID / PRIMARY_EFFECT_ESTIMATION_WITHHELD_DUE_UNIVERSAL_HARD_INFEASIBILITY。
非声明：无 C1−C0/C2−C0 节油结论、无 B_*/L_prediction、无实现率、无 bootstrap CI、无臂排名、
无预测价值 GO/NO-GO、未改任何阈值、未改动 epoch-1 证据。

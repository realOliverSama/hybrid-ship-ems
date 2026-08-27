# Phase 2 NN 预测型 A-ECMS（SOC_ref 前瞻通道） Brainstorm

Spec: `nn-predictive-ecms-phase2`  
Status: brainstorm-complete  
Created: 2026-08-24

## User Intent

## User Intent

Phase 2：训练 NN 预测未来能量需求，驱动 SOC_ref 前瞻通道（law=6 oracle 的实时可实现版本 law=7 = C4），量化 NN 对 oracle 增量（Phase 1 实测 +1.63 pp 上界）的捕获率，为 Phase 3 STM32N6 部署提供决策依据。

## 承继决策

- Phase 1（adaptive-ecms-phase1）：C0/C1/C2/C3 消融链就绪；oracle 增量 S20 主判定 +1.63 pp；SOC_ref 前瞻通道是验证过的有效接法；λ₀=0.425/K_soc=30/K_S=0.01 已标定；tier2 BSFC override 口径。
- E3/E5：参数重标定等真实数据统一重跑；oracle 信息不进实时控制器。
- AI_MODELING.md 全部规则（§1-21）适用于本 Phase。

## 本轮决策（2026-08-25，用户确认）

- F1：**预测目标 = 未来 H=300 s 能量积分 Ê_H（kWh 标量）**。输入 = 过去 300 s P_demand 窗口（1 s 采样，300 维）+ 当前船速/加速度标量；更新周期 1 s。不是峰值（前两轮 NO-GO 教训），不是逐点序列。
- F2：**训练数据 = 参数化随机航次生成器**（用户批准 Q1 方案 a）：S20 验证过的航段积木（离泊/限速/经济巡航/赶班/桥区/逆水/靠泊）随机组合，固定种子可复现；全部标注 simulation-generated。
- F3：**数据划分 = 航次级 split**（AI_MODELING §6）：约 140/30/30 航次 train/val/test；**S20 不进训练集**（头条评估场景）；归一化仅在训练集拟合。
- F4：**模型路线 = MLP 主 + LSTM 对照**（.venv_tf，固定种子）；模型选择只看 val 指标；只有入选模型做 Simulink 集成（law=7，MLP 前向内联展开）。
- F5：**C4 成功标准（用户批准 Q2）**：nn_oracle_capture = (fuel_C2−fuel_C4)/(fuel_C2−fuel_C3)，S20 主判定；≥50% → PROCEED_TO_PHASE3（STM32N6）；30~50% → PARTIAL（部分成功，论文按"预测价值实现率"叙事）；<30% → LIMITED 如实记录。
- F6：**工期门槛（用户 2026-08-25）**：施工前须给工期估计；>2 h 则不立即执行，等用户指令。

## Phase 2 研究问题

- RQ4：NN 预测器能达到什么预测精度（含高/低估不对称性）？
- RQ5：NN-A-ECMS（C4）捕获 oracle 增量的比例（gap closure）？
- RQ6：泛化——留出测试航次上 C4 的表现是否保持（不过拟合 S20/训练分布）？

## Remaining Doubts

1. 数据生成的仿真耗时（200 航次 × ~65 s 串行 ≈ 3.6 h；parsim 并行可压到 ~30-45 min，待验证）；
2. MLP 对长窗口的表达能力是否足够（LSTM 对照兜底）；
3. law=7 的 NN 前向在 Simulink 中内联展开的规模（300 维输入 × 2 层，矩阵运算，嵌入式无虞）。

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.

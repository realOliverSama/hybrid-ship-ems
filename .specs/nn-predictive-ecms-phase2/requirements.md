# Phase 2 NN 预测型 A-ECMS — Requirements

Spec: `nn-predictive-ecms-phase2`
Status: requirements-approved（用户 2026-08-25 确认 Q1/Q2 方案——brainstorm F1~F5）
Created: 2026-08-25
Brainstorm: `./brainstorm.md`
上游: `adaptive-ecms-phase1`（implementation-complete，gate=PROCEED_TO_PHASE2_NN）
规则: `docs/agent/AI_MODELING.md`（§1-21 全部适用）

## Overview

Phase 1 证明：完美未来信息经 SOC_ref 前瞻通道值 +1.63 pp（S20 入带口径）。Phase 2 用 NN 实时预测未来 300 s 能量需求 Ê_H 替代 oracle，构成 law=7（C4 NN-A-ECMS），测量其对 oracle 增量的捕获率（RQ5），并回答预测精度（RQ4）与泛化（RQ6）。达标 → Phase 3 STM32N6 部署立项。

## Scope

### In Scope

- 参数化随机航次生成器（F2）+ 批量 Baseline 仿真数据生成；
- 窗口数据集构建、航次级 split（F3）、归一化（fit-on-train）；
- MLP（主）+ LSTM（对照）训练、超参小网格、val 选模（F4）；
- law=7 变体（Simulink 内联 NN 前向）+ 单元测试；
- C4 实验矩阵 + Phase 2 gate（F5）+ 报告。

### Out of Scope

- STM32N6 实际部署（Phase 3）；TFLite 量化导出细节（Phase 3 前夜）；
- 端到端 AI EMS（AI_MODELING §1：预测与控制保持分离）；
- 新场景物理改动；冻结目录；参数重标定（E3）。

## User Stories

- As a 研究者, I want NN 对 oracle 增量的捕获率, so that 论文能回答"可实现的预测价值实现率"。
- As a 部署工程师, I want 模型在 MCU 预算内（尺寸/延迟/预处理一致性）, so that Phase 3 无缝衔接。

## Functional Requirements

### 数据

- **FR-201**: THE 工程 SHALL 实现参数化随机航次生成器：航段积木库（S20 验证的 7 类）随机组合，种子固定，约束可配（v∈[3,11] kn、段时长 5~20 min、总时长 60~120 min、逆水概率）；每个航次生成前做解析可行性预检（功率需求不超 800 kW 饱和为目标的快速估算），Simulink 抽样验证零降额。
- **FR-202**: THE 数据生成 SHALL 复用 Baseline 闭环仿真 + 剖面导出工具链（因果隔离规则沿用）；全部产物标注 simulation-generated；生成脚本、种子、commit SHA 记录（AI_MODELING §3/§11）。
- **FR-203**: THE 数据集 SHALL 为：输入 [P_demand 过去 300 s（1 s）+ 当前 v_ship + 当前加速度]，目标 Ê_H = 未来 300 s 能量积分（kWh）；窗口步长 ≥10 s（去高度相关）；航次级 split ≈140/30/30；归一化参数仅在 train 拟合并随模型导出保存（§7）。
- **FR-204**: **S20 与 Phase 1 评估场景（S01/S02/S07）不得进入训练集**；test 航次只用于最终一次性评价（§4 红线）。

### 模型

- **FR-205**: MLP（主）与 LSTM（对照）同 Dataset/同 Split/同 Input/同 Target/同归一化（§14）；固定种子；训练在 `.venv_tf`；每次实验记录 §19 全字段（Unknown 不猜测）。
- **FR-206**: 超参搜索限于批准范围（hidden size/层数/lr/batch/epochs/seq len），val 选模；不得用 test 做任何决策（§18）。
- **FR-207**: 预测指标 SHALL 含 MAE/RMSE + 高/低估分解（低估频率/幅度）——EMS 后果不对称（§12/13）。

### 集成与评价

- **FR-208**: THE 工程 SHALL 新增 law=7（NN-A-ECMS）：Simulink 内联 NN 前向（矩阵运算展开，禁用 extrinsic 慢路径），推理更新周期 1 s，SOC_ref(t) = 0.60 + K_S·ê_NN(t)，ê_NN = (Ê_NN − Ē_H)/E_scale（归一化常数与 Phase 1 相同）；单测：Simulink 内联前向 vs Python 推理同输入同输出（量化前容差 1e-5）。
- **FR-209**: C4 矩阵：{C2, C3, C4} × {S20, S01, S02, S07} + test 航次抽样（泛化）；输出 runs_phase2.csv。
- **FR-210**: THE Phase 2 gate SHALL 产出 `results/ecms_phase2/gate_result.json`：nn_oracle_capture（S20 主判定）、泛化摘要、recommendation（≥50% PROCEED_TO_PHASE3 / 30~50% PARTIAL / <30% LIMITED）。
- **FR-211**: 报告 `reports/NN_phase2_report.md`：RQ4/5/6 答案、预测-控制关系分析（§12）、部署预算（尺寸/RAM/延迟估算，§16 前置）、失败实验如实记录。

### 工程纪律

- **FR-212**: 10b smoke 保持绿；复现性（训练固定种子 + 矩阵双跑）；冻结目录只读；大产物不入库。
- **FR-213**: 失败实验不删除不隐藏（§18）；C4 若劣于 C2 如实报告并分析。

## Non-Functional Requirements

- **NFR-201**: 模型预算：MLP ≤ 50 KB 参数量级、单步推理 << 100 ms（MCU 前置约束）；
- **NFR-202**: 数据生成优先 parsim 并行；串行预算超 2 h 时缩减航次数（下限 80）并在报告中记录；
- **NFR-203**: 全 Phase 工期估计 >2 h 时不立即施工（F6，用户 2026-08-25）。

## Acceptance Criteria

- [ ] AC-1（FR-201/202）：生成器 + 数据生成完成，种子/脚本/SHA 可追溯，抽样零降额；
- [ ] AC-2（FR-203/204）：数据集 split 正确，无泄漏检查记录，S20/评估场景不在训练集；
- [ ] AC-3（FR-205~207）：MLP/LSTM 对照公平，实验记录完整，§19 字段齐全；
- [ ] AC-4（FR-208）：law=7 单测过（含 Python-vs-块一致性），10b 绿；
- [ ] AC-5（FR-209/210）：矩阵完成，gate_result.json 字段完整；
- [ ] AC-6（FR-211~213）：报告入库，失败实验记录完整，复现通过。

## Edge Cases

- **NN 预测系统性偏差**（高/低估一边倒）：如实记录；若 C4 因此劣于 C2 → LIMITED 并分析；
- **parsim 不可用**：降级串行 + 缩减航次数（≥80），记录；
- **val 上 LSTM 显著优于 MLP**：允许选 LSTM，但其 Simulink 内联复杂度上升 → 主 Agent 裁决是否值得；
- **C4 在某些场景负增量**：如实记录，不掩盖。

## Dependencies and Constraints

- 复用 Phase 1 全部设施（ecms_law law 参数化、oracle_future_energy、标定值、矩阵/gate 模式）；
- `.venv_tf` 环境；MATLAB R2026a batch；分支 feat/adaptive-ecms-methanol 继续或新开 feat/nn-ecms-phase2（主 Agent 定）；
- 用户批量授权延续（E4 + F6 工期门槛）。

## Open Questions

- [ ] OQ-21：parsim 可用 worker 数与实测加速比；
- [ ] OQ-22：Ê_H 预测的特征充分性（300 s 窗口是否含足够的航段转换先兆）。

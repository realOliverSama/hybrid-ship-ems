# Phase 2.5 NPU 导向模型族优化与工况识别门控 — Requirements

Spec: `nn-predictive-ecms-phase25`
Status: requirements-approved（2026-08-28 外部评审 4+1 项修订后批准）
Created: 2026-08-28
Brainstorm: `./brainstorm.md`
Branch: `models-research`

## Overview

Phase 2 结论（`nn-predictive-ecms-phase2`，implementation-complete）：MLP 302-128-64-1 预测
未来 300 s 能量 Ê_H，经 SOC_ref 前馈通道驱动 A-ECMS（law=7，控制器 C4）。Gate 判定
**PARTIAL**：S20 nn_oracle_capture = 0.3704 < 0.50；留出航次泛化中性（median −0.23%）；
部署预算完全满足。失败模式：低估偏向（under_freq=0.426）耦合航次结构 → S07 桥区场景回退。

Phase 2.5 目标（用户三答，2026-08-28）：

1. **capture 尽量高**（省油，主指标）；
2. **预测精度指标好**（RMSE/MAE/R²，副指标）；
3. **在 1 Hz 实时性前提下模型尽量复杂/精确**，体现 STM32N6 Neural-ART NPU 算力优势。

研究定位不变：**纯历史外推预测**（预测器输入仅含部署时可得的历史/当前量）。
工况信息经由**在线工况识别器**进入控制器（门控/参数切换），不作为预测器的未来信息输入。

## Scope

### In Scope

- 数据集 v2：内河工况导向的生成器多样性扩展 + K-shape 覆盖度审计 + 窗口化重建；
- NPU 导向模型族：TCN（主推）→ CNN-MLP → N-BEATS 对照，全部带多分位数输出头；
- 模型规模扫描：在 NPU 实时预算内放大容量，产出 params/latency/accuracy 权衡；
- 在线工况识别器（CNN/MLP 分类器，输入为部署时可得历史窗口）；
- 失败模式门控设计（S07 类模式保护），经主 Agent 设计门后集成；
- 闭环验证：capture 主指标（S20 判定 + S01/S02/S07 消融 + 留出航次）；
- NPU 部署验证：int8 量化 + STEdgeAI 编译 + N6 实测（与 Phase 3 衔接）。

### Out of Scope

- ❌ 航次计划先验特征（"开船前已知航线规划"作为预测器输入）——用户明确否决；
- ❌ 海况/波浪扰动建模——本船为内河船（长江危化品应急处置船），不适用；
- ❌ NMPC 替换 A-ECMS 骨架——A-ECMS oracle 6.89% vs DP 9.51%，替换边际收益不确定、
  M55 实时求解成本高、研究风险大（文献思想仅吸收"工况识别→参数调整"部分）；
- ❌ LSTM/GRU 路线扩展——Neural-ART NPU 无确认硬件加速（CPU 回落），且 Phase 2 已实测
  LSTM 败于 MLP；与"体现 NPU"目标冲突；
- ❌ DP/PMP 模仿学习 λ 网络（沈崇崇路线）——列为后续候选评估项，不在本 Phase 主线；
- ❌ STM32 固件集成与 Flash 烧录——属 Phase 3。

## User Stories

- As a 研究者, I want NPU 原生模型族（TCN/CNN-MLP/N-BEATS）与 Phase 2 MLP 基线的公平对照,
  so that 模型选择有完整的架构-精度-部署权衡证据链。
- As a 研究者, I want 数据集 v2 的内河工况多样性与 K-shape 覆盖度审计,
  so that 模型上限不被数据集分布过窄所限制（用户核心关切）。
- As a 研究者, I want 在线工况识别 + 失败模式门控,
  so that S07 类场景的 −1.2 kg 回退被消除，C4 可以安全携带进 Phase 3。
- As a 部署工程师, I want int8 量化 + NPU 实测时延,
  so that Phase 3 硬件集成有经过验证的模型与预算数据。

## Functional Requirements

### 数据集 v2

- **FR-301**: THE 数据集 v2 生成器 SHALL 在原 7 类段积木基础上扩展内河工况多样性；
  所有新增扰动机制 SHALL 先经物理论证（内河船适用性：流速变化/航道交通/水位阻力等），
  论证文档经主 Agent 批准后方向可实现。
- **FR-302**: 数据集 v2 SHALL 保持 Phase 2 全部泄漏纪律：航次级 split（种子固定）、
  窗口不跨航次、归一化 fit-on-train、test 零参与、S20/S01/S02/S07 结构性隔离；
  每次生成 SHALL 产出 manifest（含逐文件 md5、split 名单、泄漏断言）。
- **FR-303**: THE 数据集 v2 SHALL 附带 K-shape（或经批准的等效时序聚类）覆盖度审计报告：
  航次聚类分布、类间距离、与 v1 数据集的覆盖对比、发现的分布缺口。
- **FR-304**: IF 数据集 v2 与 v1 并存, THE 实验记录 SHALL 明确标注每个模型训练所用的
  数据集版本（v1 模型在 v2 上重训/重测时须保持公平条件，见 FR-307）。

### 模型族与选模

- **FR-305**: THE 预测模型族 SHALL 按序实现并对照：TCN（扩张因果卷积+残差块+MLP 头，主推）
  → CNN-MLP 混合 → N-BEATS 式全连接栈；基线为 Phase 2 选定 MLP 302-128-64-1。
- **FR-306**: 所有预测模型 SHALL 输出多分位数（至少 q10/q50/q90），
  以支持低估偏向分析（under_freq）与门控不确定性依据；点预测指标基于 q50。
- **FR-307**: 模型对照 SHALL 保持公平纪律（AI_MODELING §14）：同数据集版本、同 split、
  同输入、同目标、同归一化原则、同评价指标；test 集零参与训练/选模/调参。
- **FR-308**: THE 模型规模扫描 SHALL 在 NPU 实时预算内进行：对主推架构产出
  参数/时延/精度权衡曲线（至少 3 个规模点）；预算推导方法见 design §5。
- **FR-309**: 模型结构 SHALL 仅使用 Neural-ART NPU 原生算子族（Conv1D/Dense/池化/逐点激活），
  不得引入需 CPU 回落的循环层（LSTM/GRU）；注意力类结构如引入须先验证工具链可编译。

### 工况识别与门控

- **FR-310**: THE 工况识别器 SHALL 仅以部署时可得量为输入（历史功率/船速窗口），
  输出当前工况类别；训练标签来源（生成器段型真值或聚类标签）须在 design 中固定。
  （v1.1）识别器为**候选**门控组件，不预设进入最终架构；评估指标 SHALL 包含
  macro-F1、per-class recall、失败敏感类（桥区/靠离泊）recall、段切换窗口识别率与
  transition latency；overall accuracy ≥80% 不得作为唯一验收门槛。
- **FR-311**: THE 失败模式门控 SHALL 针对 Phase 2 §6 记录的 S07 模式
  （长时低功率段+突发操纵）提供保护机制（通道增益限幅/回退至 law=5 或 law=6(ê=0)）；
  门控设计 SHALL 经主 Agent 设计门批准后方可集成。
  （v1.1）门控评估 SHALL 包含消融矩阵 G0（无门控）/G1（仅分位数门控）/
  G2（仅工况识别门控）/G3（组合）；仅当 G2/G3 相对 G1 展现明确闭环增量价值时，
  工况识别器才进入最终部署架构（最小复杂度原则）。

### 闭环验证与部署

- **FR-312**: THE 闭环评估 SHALL 以 capture 为主判定指标：S20 复合航次为 capture benefit
  的判定场景（沿用 Phase 2 判定口径），S01/S02/S07 为消融场景；
  预测指标（RMSE/MAE/R²/under_freq/分位数校准）为辅指标。
  （v1.1）S20 决定 benefit 是否改善，但最终 PASS 还须满足 FR-318 留出航次
  non-degradation 门禁——两者同为必要条件。
- **FR-313**: THE 最终选定模型 SHALL 完成 int8 量化 + STEdgeAI 编译验证；
  fp32 vs int8 精度差 SHALL 记录（AI_MODELING §15）。
  （v1.1 修订）离线 STEdgeAI / Developer Cloud 估算仅作中间证据；最终验收的
  on-target 强制要求见 FR-319。
- **FR-314**: THE 实验链 SHALL 可复现：脚本化执行、种子固定、manifest/md5 溯源、
  干净进程重跑关键阶段结果一致（沿用 Phase 2 repro_check 模式）。
- **FR-315**: 失败实验 SHALL 完整保留记录（training_log + 报告失败清单），不得删除。

### Amendment v1.1 增补需求（2026-08-28 外部评审裁决）

- **FR-316 交叉 split 近邻重复审计**: THE 数据集 v2 SHALL 包含 train/val/test 跨 split
  近邻重复审计：对航次 P_demand 剖面计算跨 split 相似度（复用 FR-303 聚类/距离设施），
  输出最相似跨 split 航次对清单与相似度分布；IF 发现模板级近重复（判定方法见 design），
  THEN 须在 manifest 中记录处置（保留并声明 / 重抽），不得静默忽略。
- **FR-317 三级选模协议**: THE Phase 2.5 模型选择 SHALL 采用三级协议：
  ① prediction qualification——val 指标仅作资格筛选，淘汰明显劣势配置；
  ② closed-loop selection——以 capture/消融/留出表现为主（FR-312/FR-318）；
  ③ NPU Pareto selection——闭环合格者中按部署成本（大小/时延/HW coverage）做
  Pareto 权衡，最终 Winner 由主 Agent 综合裁决。
  val RMSE 单独排名仅适用于 v1 回归保护（--dataset v1），不得作为 v2 最终 Winner
  的唯一依据。
- **FR-318 留出航次 non-degradation 门禁**: THE 最终模型 SHALL 通过 ≥10 留出航次门禁：
  零新增降额、零 SOC 越限、无 EMS 异常；C4' vs C2 节油差分布记录中位数/最小值；
  初始判定阈值 median ≥ −0.5%、min ≥ −1.5%（🟡 门限变更仅主 Agent 可裁决）。
- **FR-319 最终部署验收必须 on-target**: THE Phase 2.5 最终部署验收 SHALL 包含真实
  STM32N6 板上 int8 推理验证，至少记录：int8 模型大小、activation RAM、weight memory、
  Neural-ART HW coverage、SW/HYBRID fallback 算子清单、实测推理时延、fp32 vs int8
  精度差、板上输出与 Python reference 的一致性误差。
  固件侧操作遵守 AGENTS.md §11 红线（Build/Gate/Debug/Flash 分阶段授权）。

## Non-Functional Requirements

- **NFR-301 实时性**：预测器 1 Hz 更新；单次推理预算目标 ≤10 ms（STM32N6 NPU int8，
  待实测修正）；识别器同频或更低频，合计推理预算 ≤20 ms。
- **NFR-302 资源**：模型 int8 权重 + 激活 RAM 应落在 STM32N6 预算内
  （Phase 2 参考：47 KB 权重 / <4 KB 激活；本 Phase 允许放大 1~2 个数量级，上限待
  NFR-301 实测约束反推）。
- **NFR-303 可维护性**：所有新脚本沿用 `model/simulink_modelD/` 现有脚本编号与目录约定；
  新增文件同步登记目录 README（AGENTS.md §20）。

## Acceptance Criteria

- [ ] AC-1（FR-301~303）：数据集 v2 manifest 泄漏断言全过；K-shape 审计报告存在且
  结论明确；v2 与 v1 的覆盖差异被量化记录。
- [ ] AC-2（FR-305~309, FR-317）：模型对照表（至少 MLP 基线/TCN/CNN-MLP/N-BEATS）含
  val/test RMSE、MAE、R²、under_freq、分位数校准、参数量、估算时延；
  公平纪律声明齐全；选模按三级协议执行（val 仅资格筛选）。
- [ ] AC-3（FR-310/311）：工况识别器 test 报告含 macro-F1/per-class recall/失败敏感类
  recall/切换窗口识别率/transition latency；门控消融 G0/G1/G2/G3 全部有闭环数据；
  门控设计文档经主 Agent 批准；S07 消融场景下 C4 不再出现 Phase 2 §6 的 −1.2 kg 级
  回退（或回退被量化并论证可接受）。
- [ ] AC-4（FR-312/318）：S20 capture 复测 + 三消融场景 + 留出航次 non-degradation
  门禁结果全部记录；无论是否 PASS，结论诚实记录。
- [ ] AC-5（FR-313/319）：int8 vs fp32 精度差 + STEdgeAI 编译记录归档；
  最终验收含真实 STM32N6 板上 int8 推理数据（HW coverage/fallback/内存/时延/
  reference 一致性）。
- [ ] AC-6（FR-314/315）：repro 检查通过；training_log 含全部失败实验。
- [ ] AC-7（FR-316）：跨 split 近邻重复审计报告存在，发现的近重复对有处置记录。

## Edge Cases

- 数据集 v2 生成器改动导致 P_demand 分布漂移 → y 分布统计断言（沿用 v1 的同分布族检查）
  可能失败；此时必须停下评审，不得放宽断言蒙混（AGENTS.md §6）。
- TCN 感受野 <300 s 时信息截断 → design 中必须给出感受野 ≥300 s 的层数/扩张率推导。
- 工况识别器在段切换瞬态的误识别 → 门控需含迟滞/确认机制（design §7）。
- 留出航次 capture 分布方差大（小样本）→ 沿用 Phase 2 惯例：S20 为唯一判定指标，
  留出航次只报分布不做单航次解读。
- N6 实测时延超预算 → 回退规模点，不得以超实时为代价保精度。

## Dependencies and Constraints

- 依赖 Phase 2 产物：`fleet_windows.npz`（v1，md5 ec7d8a45…已校验）、
  `dataset_manifest.json`、`selected_model_weights.json`、`51_generate_fleet_profiles.m`、
  `python/build_dataset.py`、`python/train_predictor.py`、law=7 集成（52_integrate_law7.m）、
  闭环矩阵脚本（53_run_phase2_matrix.m）、gate 判定（54_phase2_gate_judge.m）。
- Python 环境：`model/simulink/python/.venv_tf`（AI_MODELING 规定，不得另建）。
- Simulink 纪律：模型改动脚本驱动；03_run_smoke_tests + gate_result.json overall=true
  为提交门禁（AGENTS.md §9/§12）。
- 文献锚点：Yan et al. 2025 (doi:10.3390/jmse13020269)；沈崇崇等 2026
  (doi:10.11990/jheu.202503040)。本地转换稿在用户文献目录（不入库）。

## Open Questions

- [ ] OQ-1：数据集 v2 具体扰动机制与 N 规模（200→400?）——design 阶段物理论证后定。
- [ ] OQ-2：TCN 规模上限——中间阶段可用 STEdgeAI 离线分析/Developer Cloud 估算，
  但最终以 N6 板上实测为准（FR-319）；板上资源可用性为 T-311 前置依赖。
- [ ] OQ-3：工况类别集合——直接用生成器 7 段型真值，还是聚类重划分（参照 Yan et al.
  K-means 4 类）？design 阶段定（影响 FR-310 标签来源）。
- [ ] OQ-4：DP 模仿学习 λ 路线（沈崇崇对照）是否立项后续 Phase——本 Phase 末评审。

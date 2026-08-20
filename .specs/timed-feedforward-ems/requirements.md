# Model C: 定时前馈 EMS（Peak + Time-to-Peak 预测调度） Requirements

Spec: `timed-feedforward-ems`  
Status: requirements-draft（v2 修订，待重新批准）  
Created: 2026-08-19  
Amended: 2026-08-19（Phase 1 过门 NO-GO 后用户批准方向 (b) 转向）  
Brainstorm: `./brainstorm.md`

## Amendment Log

- **v1（2026-08-19 approved）**：以"C2 定时前馈节油 ≥2%"为核心假设。
- **v2（本版）**：Phase 1 过门 NO-GO（`reports/TFF_phase1_gate_report.md`）：节油上限被电池往返损耗机理锁死于 ~0.3-0.5%；但实证 C2 以 1/10~1/18 前馈时间实现与 C1 完全相同的 Vdc_min 增益；且发现 t_peak 高估 +5 s 即丧失全部保护的悬崖式敏感性、以及 t<22 s 历史窗口冷启动盲区。**研究问题转向**：从"节油"改为"**安全-代价权衡与预测鲁棒性**"——Phase 2 重定义为（i）真实双输出预测下安全增益不丢失、（ii）t_peak 保守偏置定量化、（iii）冷启动盲区的工程解法。v1 的 FR-011~013 由本版 FR-011'~FR-016 取代。

## Overview

在 hybrid-ship-ems 项目中研究"定时前馈"能量管理控制律：预测模型输出未来 60 s 窗口的负荷峰值幅值 ΔP_peak 与到达时间 t_peak，EMS 据此计算最晚安全启动时刻 t_start = t_peak − ΔP/r_max − τ_margin，仅在 t_start 时刻才抬升发电机目标功率 P_gen_ref，使甲醇发电机组以其最大爬坡率（40 kW/s 聚合）恰好在峰值到达前到位，从而消除现有"即时前馈"策略在峰值等待期间的多发功率（经电池充放往返损耗转化为燃油浪费）。

研究以三种控制律对照为核心：C0 Baseline（无预测）、C1 即时前馈（现有 EMS_AI_Enhanced）、C2 定时前馈（本课题新增）。分两个 Phase 执行：Phase 1 用 oracle（完美预测）与现有 MLP 验证 C2 控制律的节油假设（过门）；Phase 2 在过门后训练并选型双输出预测模型（MLP-dual / CNN1D-dual / TCN-dual），回灌闭环得到端到端结果，并评估 STM32N6 INT8 部署适配性。

## Scope

### In Scope

- 新建独立工程副本 `model/simulink_modelC/`，全部文件与内部引用改用独立 Model C 命名，消除对 `simulink/`（Model A/B）的路径依赖；`.venv_tf` 以符号链接复用，`slprj/`、`results/` 不复制。
- Model C = Model B 直流母线电气层（750 V 聚合单母线、下垂稳压）+ 新增 EMS 定时前馈变体 `EMS_ScheduledFF`；不改写 Model B 的任何源文件。
- 预测接口扩展：ai_predict 输出由 `[Ppred, valid]` 扩展为 `[Ppred, t_peak, valid]`；`AI_Pred_Source=1` (FromWorkspace) 数据格式由 N×2 扩展为 N×3 `[t, P_peak, t_peak]`。
- Phase 1 控制律验证实验 E0/E1/E2/E3（oracle 与现有 MLP 预测源）及 t_peak 误差敏感性分析。
- Phase 2'（转向后）：双输出模型训练（含冷启动窗口、不对称 t_peak 损失）、赢家模型回灌闭环（主指标改为真实预测下 Vdc_min 保持率）、t_peak 保守偏置扫描、冷启动规则式前馈；INT8 量化与 Neural-ART NPU 映射率评估**降级为可选附录**（若赢家模型确定且时间允许）。
- 统一的闭环评价指标体系（燃油、安全、电池、机组）与实验报告。

### Out of Scope

- STM32N6 实物板载部署与 HIL 测试（本课题仅到 stedgeai 分析/benchmark 层面）。
- 12 步负荷轨迹预测（Task C）：仅当双输出回归（Task B）在 Phase 2 中表现不稳定时作为备选启用。
- Quantile 预测（P50/P90）、事件检测双模型、模型 ensemble：记录为未来工作。
- 甲醇机组成员的详细物理建模（保持现有平均模型：限幅+爬坡+一阶惯性）。
- 对 `model/simulink/` 现有 Model A/B 源文件的任何修改。

## User Stories

- As a 研究者, I want 在同一闭环仿真中对照 C0/C1/C2 三种控制律, so that 可以分离"控制律"与"预测模型质量"对油耗与安全的影响。
- As a 研究者, I want 先用 oracle 预测验证 C2 的节油潜力上限, so that 在投入模型训练之前确认课题假设成立。
- As a 研究者, I want 在独立命名的 simulink_modelC 副本中开发, so that 现有 Model A/B 工程保持封存可复现。
- As a 部署工程师, I want Phase 2 输出各候选模型的 INT8 量化损失与 NPU 映射率, so that 最终选型面向 STM32N6 边缘部署而非仅 PC 精度。

## Functional Requirements

- **FR-001**: THE 工程副本 SHALL 位于 `model/simulink_modelC/`，其内所有模型文件、脚本、包函数、模型变量名均使用 Model C 独立命名（如 `HybridShip_ModelC_DCBus.slx`、`build_model_c.m`），运行时不得引用 `model/simulink/` 下的任何文件（`.venv_tf` 符号链接除外）。
- **FR-002**: THE EMS SHALL 提供三种可复现对照的控制律变体：C0 Baseline（负荷跟随+SOC修正）、C1 即时前馈（现有逻辑原样保留）、C2 定时前馈（新增 `EMS_ScheduledFF`），通过变体控制变量一键切换。
- **FR-003**: WHEN C2 变体激活且预测有效时, THE EMS SHALL 计算 t_start = t_peak − ΔP/r_max − τ_margin，并在 t < t_start 期间保持 P_gen_ref 为 baseline 值，t ≥ t_start 后输出 max(baseline, P_load + ΔP)。
- **FR-004**: IF t_peak 小于爬坡所需时间（ΔP/r_max + τ_margin）, THEN THE EMS SHALL 退化为 C1 即时响应行为（立即抬升 P_gen_ref）。
- **FR-005**: IF SOC 低于设定阈值, THEN THE EMS SHALL 将 t_start 按保守偏置提前（偏置档位参数化）。
- **FR-006**: IF 预测无效（AI_valid = 0）或 EMS_WORK_MODE 非全自动, THEN THE EMS SHALL 严格回退 C0 Baseline 行为（与现有安全门控体系一致）。
- **FR-007**: THE ai_predict 接口 SHALL 输出 `[Ppred, t_peak, valid]`，并支持 oracle 数据源（FromWorkspace，N×3 格式 `[t, P_peak, t_peak]`）用于 Phase 1 实验。
- **FR-008**: THE Phase 1 实验矩阵 SHALL 至少包含 E0（C0，无预测）、E1（oracle+C1）、E2（oracle+C2）、E3（现有 MLP 幅值+t_peak 真值，C2），覆盖现有标准工况场景集。
- **FR-009**: THE Phase 1 SHALL 包含 t_peak 误差敏感性分析：在 oracle 基础上对 t_peak 注入 ±5/±10/±20 s 系统偏移，评估 C2 的 Vdc_min 与油耗退化。
- **FR-010**: IF E2 显示 C2 相对 C1 的节油收益低于过门阈值, THEN THE 研究 SHALL 停止 Phase 2 并输出否定结论报告（避免无效模型训练投入）。
- **FR-011'**: WHEN Phase 2' 启动, THE 训练管线 SHALL 训练并评估 3 个双输出模型（MLP-dual、CNN1D-dual、TCN-dual），结构与统一超参数沿用 design §5.2；训练数据 SHALL 显式包含航程起点冷启动窗口（历史不足 W 秒的样本以既有填充规则保留并参与训练），标签为 [ΔP, t_peak_rel]。
- **FR-012'**: THE t_peak 训练目标 SHALL 采用不对称损失：对 t_peak 高估（预测到达晚于真实，导致启动偏晚）施加高于低估的惩罚权重（权重比初值 3:1，validation 微调），使模型输出天然偏保守；或等价地直接训练 t_peak 的下分位数。
- **FR-013'**: THE Phase 2' 赢家模型 SHALL 回灌 Simulink 闭环（Source=3 环内或两遍法），在 {S04,S06,S07,S09} 完成 C0/C1/C2 对照，主评价指标改为**真实预测下的 Vdc_min 保持率**而非节油。
- **FR-014**: THE EMS_ScheduledFF SHALL 增加确定性冷启动前馈通道：当历史窗口未填满（t < W s）或预测无效时，IF v_cmd 出现显著正向阶跃, THEN THE EMS SHALL 立即施加基于 v_cmd→推进需求静态映射的前馈（规则式，不依赖 NN 预测），覆盖初始加速段的母线跌落。
- **FR-015**: THE EMS SHALL 支持 t_peak 保守偏置参数 `ASSUMED_TFF_TPeak_Bias`（默认 −5 s，从 t_peak_rel 中减去）；THE Phase 2' SHALL 在真实模型 t_peak 误差分布下扫描偏置 ∈ {0, −2.5, −5, −7.5, −10} s，以闭环 Vdc_min 与燃油代价定量化最优偏置。
- **FR-016**: Phase 2' 过门准则（取代 v1 的 2% 节油门）：赢家模型 + 保守偏置 + 冷启动前馈的 C2 闭环 SHALL 满足——①每场景 Vdc_min ≥ 0.98 × Vdc_min(oracle+C2)（≈ ≥694 V）；②燃油代价 ≤ C0 +1.0%；③t<22 s 初始加速段 Vdc_min 较 C0 有实质改善。

## Non-Functional Requirements

- **NFR-001**: 主评价指标 SHALL 为航程等效燃油消耗 `fuel_total_kg`（模型内 `∫ P_gen × BSFC(P_gen)/3600` 积分的现有实现）。
- **NFR-002**: 安全约束 SHALL 满足：C2 的 `Vdc_min` 不低于同场景 C0 Baseline 水平（允许容差在 design 中定义）；SOC 终端偏差受限（阈值在 design 中定义）。
- **NFR-003**: 所有实验 SHALL 可重复执行：构建脚本从零重建模型，实验脚本输出标准化结果（CSV/JSON + 图表）至 `simulink_modelC/results/`。
- **NFR-004**: 每个 ASSUMED 参数（τ_margin、SOC 偏置档位、过门阈值等）SHALL 在 config 中显式登记并标注假设等级，与现有 `Model_Parameters.m` 惯例一致。
- **NFR-005**: Phase 2 候选模型 SHALL 面向 STM32N6 部署约束设计：静态形状、INT8 可量化、优先使用 Neural-ART 硬件友好算子（Conv1D k=3 / Dense / ReLU / Pooling），避免 LayerNorm 与动态 MatMul attention。

## Acceptance Criteria

- [ ] AC-1（FR-001）：在 `simulink_modelC/` 中运行构建脚本可从零重建 Model C 并通过冒烟测试；临时断开 `model/simulink/` 访问时仿真仍正常运行（路径依赖验证）。
- [ ] AC-2（FR-002/003/004/005/006）：C2 变体单元级验证：合成阶跃负荷场景下，P_gen_ref 在 t_start 前保持 baseline，t_start 后以 40 kW/s 爬升并在 t_peak 前到位；t_peak 过近/预测失效/非全自动三种情形下行为分别符合 FR-004/005/006。
- [ ] AC-3（FR-008/010）：E0–E3 结果表（fuel_total_kg、Vdc_min、电池 RMS/峰值、SOC 终端），并给出 E2 过门判定结论。
- [ ] AC-4（FR-009）：t_peak 偏移敏感性曲线（±5/±10/±20 s × Vdc_min/油耗）。
- [ ] AC-5（FR-011'/012'）：三模型精度表（ΔP MAE、t_peak MAE、t_peak 高估/低估分离误差、低估 MAE、参数量）；冷启动窗口已纳入训练的证据；不对称损失权重记录。
- [ ] AC-6（FR-013'/014/015/016）：端到端报告——真实模型 C2 闭环 Vdc_min 保持率（对照 oracle+C2 上界与 C0 下界）、燃油代价、冷启动段 Vdc 改善、偏置扫描曲线与最优偏置结论。

## Edge Cases

- t_peak 预测误差导致启动过晚：Vdc 缺口由电池下垂兜底（既有架构），FR-009 量化该风险；极端时触发欠压降额（既有 08_Undervolt_Derate）。
- 预测峰值小于等于当前负荷（ΔP ≤ 0）：C2 不动作，等价 C0。
- 连续多峰/负荷平台期：t_start 计算基于最近一次有效预测，1 Hz 推理刷新，需防抖（hysteresis 或保持逻辑，design 中定义）。
- 仿真起点窗口未填满（前 22 s）：ai_predict 既有初始化逻辑（repmat 首样本填充）沿用。
- oracle 数据时间越界：沿用 lininterp1 端点保持语义。

## Dependencies and Constraints

- 依赖 `model/simulink/` 现有资产（只读）：Model B 构建体系（`+model/build_model_b.m`）、`config/Model_Parameters.m`、数据集导出管线（`05_export_dataset.m`）、现有 MLP 权重与训练管线（`python/`、`.venv_tf`）。
- 场景集沿用现有 T01–T05 标准工况（`results/T01_still_water` 等对应的场景定义）。
- MATLAB/Simulink 环境与现有工程一致；ST Edge AI（stedgeai analyze/benchmark）用于 Phase 2 部署评估。
- 排序约束：目录副本与重命名 → C2 变体实现与单元验证 → E0–E3 → 过门判定 →（Phase 2）模型训练 → 闭环回灌 → 部署评估。

## Open Questions（v2）

- [ ] OQ-5：不对称 t_peak 损失的高估:低估权重比（建议初值 3:1，validation 微调）——待批准。
- [ ] OQ-6：t_peak 保守偏置默认值（建议 −5 s，由 T-008 悬崖位置 +5 s 反推）——待批准。
- [ ] OQ-7：冷启动规则前馈的 v_cmd→功率映射形式（建议复用螺旋桨/船体静力学反解 P_prop(v_cmd)，增益参数化）——design v2 细化。
- [x] OQ-1~4（v1）：已在 Phase 1 使用；OQ-1 的 2% 阈值经实证物理不可达，v2 由 FR-016 取代。

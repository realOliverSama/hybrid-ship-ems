# 面向参数不确定性的多步负荷预测增强型 EMS Requirements

Spec: `multistep-predictive-ems`  
Status: requirements-draft（待用户批准）  
Created: 2026-08-20  
Brainstorm: `./brainstorm.md`  
Supersedes: `.specs/timed-feedforward-ems/`（Phase 1 NO-GO 后转向，原 spec 冻结归档）

## Amendment Log

- **v1（2026-08-20 draft）**：基于 timed-feedforward-ems Phase 1 NO-GO 实证与外部咨询对齐后的研究方向转向。用户已批准总体方案（brainstorm.md 中 D1–D8 固定决策 + 三处修正）。

## Overview

前序研究证明：在航线/航速/环境基本一致的前提下，推进机械能需求固定，EMS 只决定发电机与电池之间的功率时空分配；定时前馈（Peak + t_peak）节油上限被电池往返损耗锁死（实测 0.293%），但其在动态安全上的价值成立（以 1/10~1/18 前馈时间获得同等 Vdc_min 增益）。

本研究转向：**AI 只负责短时多步负荷预测；EMS 读取当前实际发电机参数（P_rated / R_gen / τ_gen）与电池功率裕度，计算未来功率缺口风险，实施最小必要前馈**。发电机参数（当前 1000 kW / 40 kW/s / 5 s 均为设计阶段假设）从固定常数提升为被控对象参数/设计变量，通过参数扫描产出"预测增强 EMS 有效工作区域图"，最终在 STM32N6 完成轻量化部署验证。

研究问题：
- **RQ1**：短时多步负荷预测相比单一未来峰值预测，能否提供更有效的 EMS 前馈信息？
- **RQ2**：发电机额定功率、动态爬坡能力及电池功率裕度如何影响预测增强 EMS 的实际收益？
- **RQ3**：在不同动力系统参数与工况下，何种轻量网络能以较低 STM32N6 部署代价获得最佳闭环控制效果？

## Scope

### In Scope

- **Phase 1（本 spec 主体）**：独立工程副本 `model/simulink_modelC_(oracle_ms)/`；发电机参数化改造；oracle 多步负荷预测通道；发电机可达性 + 电池功率裕度评估；最小必要前馈控制律 `EMS_MinFF`；horizon 扫描 {10,15,20,30,45,60} s；发电机参数与 SOC₀ 扫描；统一闭环指标体系；GO/NO-GO 判定与有效区域图。
- **Phase 2（需求层面定义，Phase 1 GO 后启动）**：MLP / CNN1D / small-TCN / GRU 四模型同一多步预测任务训练（含冷启动窗口）；闭环指标优先选型；x_ship 消融（with/without + 事件位置随机化）；冷启动规则兜底工程组件。
- **Phase 3（需求层面定义，Phase 2 完成后启动）**：Top1~2 模型 INT8 量化 → STM32N6 Neural-ART NPU → UART/CAN/UDP 软硬件闭环。

### Out of Scope

- 继续深挖 Peak + Time-to-Peak 控制律或专门优化 t_peak MAE（D7：冻结为否定性研究结果）。
- 以"AI 节油 ≥ 某阈值"作为过门条件（D1：已证物理不可达）。
- 修改 `model/simulink/`（Model A/B）与 `model/simulink_modelC/`（冻结基线）的任何源文件。
- 重新训练已有 MLP 单峰值模型或改动 `model/simulink_modelC/python/` 既有训练管线（Phase 2 新建独立训练脚本）。
- Phase 1 阶段引入任何神经网络（D5：纯 oracle）。
- 完整笛卡尔积参数扫描（D6：代表性组合）。

## User Stories

- As a 船舶 EMS 研究者, I want 用 oracle 多步负荷信息驱动参数化 EMS 并扫描发电机参数, so that 在投入神经网络训练前先证明"多步预测信息本身具有闭环控制价值"并划定其价值随动力系统参数变化的边界。
- As a 船舶 EMS 研究者, I want 最小必要前馈控制律（电池合理使用而非避免使用）, so that 避免既有 AI EMS "过度发电抬高 SOC"的伪优化（S07 实证：SOC 0.598→0.608 伴随明显额外发电）。
- As a 边缘部署工程师, I want AI 预测层与发电机参数完全解耦, so that 设备选型变化（如 800 kW / 25 kW/s）时无需重训练模型，只需改控制器参数。

## Functional Requirements

### 工程与参数化

- **FR-001**: THE 研究 SHALL 从冻结基线 `model/simulink_modelC/` 复制建立独立工程 `model/simulink_modelC_(oracle_ms)/`，全部文件与内部引用改用独立命名，消除对 `model/simulink/` 与 `model/simulink_modelC/` 的路径依赖；`.venv_tf` 以符号链接复用，`slprj/`、`results/` 不复制。
- **FR-002**: THE 模型 SHALL 将发电机额定功率 `P_gen_rated`、最大爬坡率 `R_gen_max`、一阶惯性时间常数 `tau_gen` 从硬编码常数提升为 config 层参数（`Model_Parameters.m` 惯例），并支持以相对最大负荷的比例 `lambda_P = P_gen_rated / P_load,max` 定义额定功率档位；EMS 控制律 SHALL 在运行时读取这些参数，不得将其编译进 AI 预测接口。

### 多步预测通道（Phase 1：oracle）

- **FR-003**: THE 模型 SHALL 新增多步负荷预测通道：预测接口输出未来窗口内 K 个步长的负荷序列 `[P_{t+h1}, …, P_{t+hK}]`；Phase 1 预测源为 oracle（仿真内取自真实未来负荷轨迹，FromWorkspace 或等效机制），`AI_Pred_Source` 取值扩展并在结果元数据中记录预测源与 horizon。
- **FR-004**: WHEN horizon H 作为实验变量变化（{10,15,20,30,45,60} s），THE 预测通道与 EMS SHALL 仅需修改 config 参数即可切换，无需改模型结构。

### 最小必要前馈控制律

- **FR-005**: THE EMS SHALL 计算发电机可达功率轨迹 `P_gen,reachable(τ)`：从当前 `P_gen` 出发，受 `R_gen_max` 爬坡率、`tau_gen` 一阶惯性与 `[0, P_gen_rated]` 限幅约束的未来可达功率上界。
- **FR-006**: THE EMS SHALL 计算电池允许瞬态功率 `P_bat_allow = f(SOC, Vdc, P_bat_max)`（函数形式为 design 阶段 OQ-2 决策项）。
- **FR-007**: THE EMS SHALL 定义未来功率缺口风险 `R(τ) = P̂_load(t+τ) − P_gen,reachable(t+τ) − P_bat_allow`；仅 WHEN 存在 τ 使 `R(τ) > 0`（或越过 design 定义的安全阈值），THE EMS SHALL 施加前馈；否则保持 Baseline 行为。
- **FR-008**: WHEN 需要干预，THE EMS SHALL 求解最小必要前馈量 `P_ff* = min P_ff`，满足 `Vdc ≥ V_safe`、电池功率约束与机组限幅；前馈叠加于 Baseline 基值并经既有 clamp/动态环节，底层电池下垂自治保持不变。
- **FR-009**: IF 预测无效（oracle 数据缺失/越界），THE EMS SHALL 退化为 Baseline 行为并记录无效事件计数。

### 实验与评价

- **FR-010**: THE Phase 1 SHALL 先固定当前发电机参数（1000 kW / 40 kW/s / 5 s）执行 horizon 扫描 {10,15,20,30,45,60} s × 场景集，确定有效预测时间尺度；再固定最优 1~2 个 horizon 执行发电机参数扫描。
- **FR-011**: THE 参数扫描 SHALL 覆盖 `lambda_P ∈ {0.7,0.8,0.9,1.0,1.1}`、`R_gen ∈ {20,30,40,50,60} kW/s`、`tau_gen ∈ {2,5,8,10} s`、`SOC₀ ∈ {0.4,0.5,0.6,0.7,0.8}` 的代表性组合（组合选择方法为 design 阶段 OQ-3 决策项），禁止全笛卡尔积。
- **FR-012**: THE 实验 SHALL 输出统一闭环指标体系：一级指标 `Vdc_min`、欠压事件次数/持续时间、`P_bat,peak`、`P_bat,RMS`、电池放电能量与吞吐量；二级指标 `SOC_min`、`SOC_end`、发电机爬坡饱和时间、未服务能量、船速 RMSE；约束指标 `fuel_total_kg`。全部结果以标准化 CSV/JSON + 图表输出至 `results/`。
- **FR-013**: THE Phase 1 报告 SHALL 产出"AI 有效区域图"：`Benefit_AI = f(P_gen_rated, R_gen, SOC₀, 负荷动态)` 的可视化与定量结论，回答 RQ2。

### Phase 1 过门（GO/NO-GO）

- **FR-014**: THE Phase 1 GO 判定 SHALL 同时满足（具体阈值在 design 阶段定量化并经用户确认，OQ-1）：在所扫描的合理参数区域内存在非平凡子区域，oracle 多步前馈相对 Baseline 满足——①`P_bat,peak` 显著下降；②`P_bat,RMS` 或电池吞吐量下降；③`Vdc_min` 或欠压裕度改善；④船速性能不下降；⑤`|ΔSOC_end|` 受控；⑥燃油代价 ≤ 可接受阈值。IF 六条不满足（完全知道未来负荷仍无闭环收益），THE 研究 SHALL 停止神经网络投入并输出否定性结论报告。

### Phase 2 / Phase 3（需求框架，GO 后细化）

- **FR-015**: THE Phase 2 SHALL 训练四个候选模型（M0 MLP / M1 CNN1D / M2 small-TCN / M3 GRU）解决同一多步预测任务 `X_{t-L:t} → [P_{t+h1},…,P_{t+hK}]`，训练数据 SHALL 纳入冷启动窗口；不得使用 Test Set 参与训练或调参。
- **FR-016**: THE 模型选型 SHALL 以闭环指标（Vdc_min 保持率、P_bat,peak、吞吐量、燃油代价）为第一判据，离线 MAE/RMSE 仅为参考；同时报告负荷低估误差、突变工况误差与未见工况泛化。
- **FR-017**: THE Phase 2 SHALL 包含 x_ship 消融实验：with/without `x_ship` 输入 × 事件位置随机化对照，区分"动力状态预测"与"航线位置先验记忆"。
- **FR-018**: THE Phase 2 SHALL 实现冷启动规则兜底：WHEN 历史窗口未填满（t < W s）或预测无效，IF v_cmd 出现显著正向阶跃, THEN THE EMS SHALL 施加基于 v_cmd→推进需求静态映射的规则式前馈（不依赖 NN），并单独评估其增量贡献。
- **FR-019**: THE Phase 3 SHALL 将 Top1~2 模型 INT8 量化并经 ST Edge AI 部署至 STM32N6 Neural-ART NPU，评估模型大小、Activation RAM、INT8 精度损失、推理延迟、算子映射率与 1 Hz 实时稳定性，并通过 UART/CAN/UDP 与 Simulink 构成软硬件闭环验证。

## Non-Functional Requirements

- **NFR-001**: 所有实验 SHALL 可重复执行：构建脚本从零重建模型，实验脚本输出标准化结果（CSV/JSON + 图表），300MB 级原始 .mat 不入库。
- **NFR-002**: 每个 ASSUMED 参数（V_safe、安全阈值、P_bat_allow 参数、GO 阈值等）SHALL 在 config 中显式登记并标注假设等级，与 `Model_Parameters.m` 惯例一致。
- **NFR-003**: 候选模型 SHALL 面向 STM32N6 部署约束设计：静态形状、INT8 可量化、优先 Neural-ART 硬件友好算子（Conv1D k=3 / Dense / ReLU / Pooling），避免 LayerNorm 与动态 MatMul attention。
- **NFR-004**: 机器可读 Gate 是最终工程门禁依据；`03` 冒烟与结果校验 FAIL 时通过 `error('GATE:*')` 终止，Gate FAIL 不允许提交。
- **NFR-005**: AI 红线沿用：不得使用 Test Set 训练/调参；不得为更好指标重新划分数据；不得隐藏失败实验；不得修改评价指标制造更好结果；不得偷换预测目标。

## Acceptance Criteria

- [ ] AC-1（FR-001）：在 `simulink_modelC_(oracle_ms)/` 中运行构建脚本可从零重建模型并通过冒烟测试；临时断开 `model/simulink/` 与 `model/simulink_modelC/` 访问时仿真仍正常运行（路径依赖验证）。
- [ ] AC-2（FR-002/005/007/008）：单元级验证：合成阶跃负荷 + 参数化发电机下，`P_gen,reachable(τ)` 与理论解析解一致；`R(τ)≤0` 时 EMS 与 Baseline 逐点一致；`R(τ)>0` 时 `P_ff` 为满足约束的最小值且 `Vdc ≥ V_safe`。
- [ ] AC-3（FR-003/004/010）：horizon 扫描结果表：6 档 horizon × 场景集 × C0/C2ms 对照（一级/二级/约束指标齐全）。
- [ ] AC-4（FR-011/012/013）：参数扫描结果表 + AI 有效区域图（至少 λ_P × R_gen 二维投影 + SOC₀ 分组）。
- [ ] AC-5（FR-014）：GO/NO-GO 判定报告，六条判据逐条给出机器可读 PASS/FAIL 与证据链接。
- [ ] AC-6（FR-015/016/017，Phase 2）：四模型精度表 + 闭环对照表 + x_ship 消融表；冷启动窗口纳入训练的证据。
- [ ] AC-7（FR-018，Phase 2）：冷启动兜底消融：无兜底/仅兜底/兜底+NN 三组在 t<22 s 段的 Vdc_min 对比。
- [ ] AC-8（FR-019，Phase 3）：STM32N6 部署报告（大小/RAM/延迟/映射率/INT8 精度损失）+ HIL 闭环结果。

## Edge Cases

- 预测负荷全部低于发电机可达功率（R(τ) 恒 ≤ 0）：EMS 全程等价 Baseline，前馈零激活（须记录激活时间占比以证实"最小必要"）。
- 渐进负荷场景（无显著阶跃）：前馈结构性闲置为合法行为，不得为制造激活而调参。
- 极端参数组合（λ_P=0.7 且 R_gen=20 kW/s）：发电机本身能力不足导致的欠压属于被控对象极限，报告须区分"EMS 失效"与"系统固有不可行"。
- 多步预测越界（仿真末端不足 H）：沿用端点保持语义并记录。
- SOC 接近下限时 `P_bat_allow → 0`：风险门槛自动收紧，前馈趋于即时前馈行为，须验证平滑过渡无抖振。
- 参数扫描中模型重建：仅改 config 参数不得触发结构重建；扫描脚本须复用已编译模型（fast restart 或等效）。

## Dependencies and Constraints

- 依赖冻结基线 `model/simulink_modelC/`（只读）：构建体系、config 惯例、场景定义、指标体系脚本、既有 EMS 三变体与 sched_ff 实现（作为命名与结构参考）。
- 依赖 `model/simulink/`（只读）：原始 Model B 构建体系与数据集导出管线。
- MATLAB/Simulink 环境与现有工程一致；Phase 2 训练使用 `.venv_tf`；Phase 3 使用 ST Edge AI 工具链。
- 排序约束：副本与参数化 → 多步通道与 EMS_MinFF → 单元验证 → horizon 扫描 → 参数扫描 → GO/NO-GO →（GO 后）Phase 2 spec 细化。
- Git 纪律：本地 main 分叉（4 个未推送提交与 origin/main 内容等价）保留待用户决定，本 spec 工作从 `feat/multistep-predictive-ems`（基于 origin/main）推进。

## Open Questions

- [ ] OQ-1：Phase 1 GO 判据六条的具体阈值（如 P_bat,peak 下降 ≥10%？燃油代价 ≤ C0+1.5%？）——design 阶段给出建议值，用户确认。
- [ ] OQ-2：`P_bat_allow = f(SOC, Vdc, P_bat_max)` 的函数形式（建议：分段线性，SOC/Vdc 越低允许功率越小）——design 阶段细化。
- [ ] OQ-3：参数扫描代表性组合的选择方法（建议：λ_P × R_gen 全因子 5×5=25 为主图，τ_gen 与 SOC₀ 各取 2~3 个代表点做单因素补充）——design 阶段定案。
- [ ] OQ-4：多步预测步长集合（建议固定输出节点 {+5,+10,+15,+20,+30} s，H>30 时线性外推或重复末值；或按 H 动态取节点）——design 阶段定案。

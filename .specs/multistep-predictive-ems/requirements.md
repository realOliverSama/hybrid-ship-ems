# 面向参数不确定性的多步负荷预测增强型 EMS Requirements

Spec: `multistep-predictive-ems`  
Status: requirements-approved（2026-08-20 用户批准，含 v1.2 三项修订）  
Created: 2026-08-20  
Brainstorm: `./brainstorm.md`  
Supersedes: `.specs/timed-feedforward-ems/`（Phase 1 NO-GO 后转向，原 spec 冻结归档）

## Amendment Log

- **v1（2026-08-20 draft）**：基于 timed-feedforward-ems Phase 1 NO-GO 实证与外部咨询对齐后的研究方向转向。用户已批准总体方案（brainstorm.md 中 D1–D8 固定决策 + 三处修正）。
- **v1.1（2026-08-20 draft）**：外部评审（GPT）四条修正全部接受：①D4 控制律改写——`P_ff* = min P_ff s.t. R(τ) ≤ 0`（功率缺口约束），`Vdc` 回归闭环评价指标而非在线优化目标；②horizon 确定改为两级筛选（Phase 1A 三锚点 Fast/Nominal/Slow 粗筛 → Phase 1B 保留 2~3 个 H 进入参数扫描），撤回"单锚点先行"；③OQ-4 改为 5 s 等分辨率真实多步节点（H/5 个），废除末值保持；④OQ-1 GO 判据拆分为非劣化约束 + 收益判据两类；另 `P_bat_allow` 更名 `P_bat_allowable`（EMS 风险阈值，非电池命令），λ_P 分母锁定统一设计参考最大负荷。
- **v1.2（2026-08-20，用户批准后修订）**：外部评审第二轮三项修改全部接受：①新增 FR-013A——Phase 1 增加 C0/C_P(Oracle Peak-only)/C_MS 三对照，C_P 与 C_MS 同 oracle 源同 horizon，使 RQ1 可严格验证；②非劣化约束 `|ΔSOC_end|` 由 0.02 收紧至 0.005（1200 kWh ≈6 kWh；允许 design 重复性分析后放宽，上限 0.01 且须记录依据）——0.02 对应 24 kWh，会重新允许 S07 式"过度发电"伪优化；③FR-002 的 λ_P 分母统一为 `P_load,design,max`；另 Phase 1 产物更名"预测增强 EMS 有效工作区域图"（oracle 阶段不得称 AI）。

## Overview

前序研究证明：在航线/航速/环境基本一致的前提下，推进机械能需求固定，EMS 只决定发电机与电池之间的功率时空分配；定时前馈（Peak + t_peak）节油上限被电池往返损耗锁死（实测 0.293%），但其在动态安全上的价值成立（以 1/10~1/18 前馈时间获得同等 Vdc_min 增益）。

本研究转向：**AI 只负责短时多步负荷预测；EMS 读取当前实际发电机参数（P_rated / R_gen / τ_gen）与电池功率裕度，计算未来功率缺口风险，实施最小必要前馈**。发电机参数（当前 1000 kW / 40 kW/s / 5 s 均为设计阶段假设）从固定常数提升为被控对象参数/设计变量，通过参数扫描产出"预测增强 EMS 有效工作区域图"，最终在 STM32N6 完成轻量化部署验证。

研究问题：
- **RQ1**：短时多步负荷预测相比单一未来峰值预测，能否提供更有效的 EMS 前馈信息？
- **RQ2**：发电机额定功率、动态爬坡能力及电池功率裕度如何影响预测增强 EMS 的实际收益？
- **RQ3**：在不同动力系统参数与工况下，何种轻量网络能以较低 STM32N6 部署代价获得最佳闭环控制效果？

## Scope

### In Scope

- **Phase 1（本 spec 主体）**：独立工程副本 `model/simulink_modelC_(oracle_ms)/`；发电机参数化改造；oracle 多步负荷预测通道；发电机可达性 + 电池功率裕度评估；最小必要前馈控制律 `EMS_MinFF`；horizon 扫描 {10,15,20,30,45,60} s；发电机参数与 SOC₀ 扫描；统一闭环指标体系；GO/NO-GO 判定与预测增强 EMS 有效工作区域图。
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
- **FR-002**: THE 模型 SHALL 将发电机额定功率 `P_gen_rated`、最大爬坡率 `R_gen_max`、一阶惯性时间常数 `tau_gen` 从硬编码常数提升为 config 层参数（`Model_Parameters.m` 惯例），并支持以比例 `lambda_P = P_gen_rated / P_load,design_max` 定义额定功率档位（分母为统一设计参考最大负荷，与 FR-011 一致）；EMS 控制律 SHALL 在运行时读取这些参数，不得将其编译进 AI 预测接口。

### 多步预测通道（Phase 1：oracle）

- **FR-003**: THE 模型 SHALL 新增多步负荷预测通道：预测接口输出未来窗口内 K 个步长的负荷序列 `[P_{t+h1}, …, P_{t+hK}]`；Phase 1 预测源为 oracle（仿真内取自真实未来负荷轨迹，FromWorkspace 或等效机制），`AI_Pred_Source` 取值扩展并在结果元数据中记录预测源与 horizon。
- **FR-004**: WHEN horizon H 作为实验变量变化（{10,15,20,30,45,60} s），THE 预测通道与 EMS SHALL 仅需修改 config 参数即可切换，无需改模型结构；预测通道 SHALL 采用固定最大宽度 12 节点（5 s 分辨率 × 60 s）+ 有效节点数参数，EMS 仅消费前 H/5 个节点。

### 最小必要前馈控制律

- **FR-005**: THE EMS SHALL 计算发电机可达功率轨迹 `P_gen,reachable(τ; P_ff)`：从当前 `P_gen` 出发、施加候选前馈 `P_ff` 后，受 `R_gen_max` 爬坡率、`tau_gen` 一阶惯性与 `[0, P_gen_rated]` 限幅约束的未来可达功率上界；该计算 SHALL 通过一阶惯性 + 爬坡模型前向递推实现，复杂度满足 1 Hz 刷新与嵌入式部署。
- **FR-006**: THE EMS SHALL 定义电池允许瞬态功率阈值 `P_bat_allowable = P_bat,max × f_SOC(SOC) × f_V(Vdc)`（分段线性系数函数，design 细化）；该参数是 **EMS 侧的风险评估阈值**（允许底层下垂电池在预测窗口内承担的瞬态功率上限），不是电池功率命令、不声称是电池物理极限，实际电池输出仍由底层 DC/DC 下垂自治决定。
- **FR-007**: THE EMS SHALL 定义未来功率缺口风险 `R(τ) = P̂_load(t+τ) − P_gen,reachable(t+τ; P_ff) − P_bat_allowable`；仅 WHEN 存在 τ 使 `R(τ) > 0`，THE EMS SHALL 施加前馈；否则保持 Baseline 行为。
- **FR-008**: WHEN 需要干预，THE EMS SHALL 求解最小必要前馈 `P_ff* = min P_ff` 使 `R(τ) ≤ 0, ∀τ∈[0,H]` 且 `0 ≤ P_gen,target ≤ P_rated`；**不要求在线求解 Vdc 约束**——Vdc 由母线电容与电池下垂自治动态闭环形成，`Vdc_min` 作为闭环安全评价指标与 GO/NO-GO 约束；前馈叠加于 Baseline 基值并经既有 clamp/动态环节，底层电池下垂自治保持不变。
- **FR-009**: IF 预测无效（oracle 数据缺失/越界），THE EMS SHALL 退化为 Baseline 行为并记录无效事件计数。

### 实验与评价

- **FR-010**: THE Phase 1 SHALL 采用两级 horizon 筛选：**Phase 1A** 在三个代表性发电机锚点（Fast：高爬坡/小惯性；Nominal：40 kW/s/5 s；Slow：低爬坡/大惯性，具体取值在 design 定案）分别扫描全部 6 档 horizon，识别普遍无效区/有效区/动态敏感区；**Phase 1B** 保留 2~3 个代表性 horizon 进入发电机参数扫描。禁止在单一 nominal 参数点定死唯一 horizon 后推广至全参数空间。
- **FR-011**: THE 参数扫描 SHALL 以 `lambda_P × R_gen` 5×5 全因子为主有效区域图，`tau_gen` 与 `SOC₀` 各取代表值单因素补充，禁止全笛卡尔积；`lambda_P = P_gen_rated / P_load,design_max` 的分母 SHALL 为**统一设计参考最大负荷**（跨场景固定，在 config 登记），禁止逐场景自定义分母。
- **FR-012**: THE 实验 SHALL 输出统一闭环指标体系：一级指标 `Vdc_min`、欠压事件次数/持续时间、`P_bat,peak`、`P_bat,RMS`、电池放电能量与吞吐量；二级指标 `SOC_min`、`SOC_end`、发电机爬坡饱和时间、未服务能量、船速 RMSE；约束指标 `fuel_total_kg`。全部结果以标准化 CSV/JSON + 图表输出至 `results/`。
- **FR-013**: THE Phase 1 报告 SHALL 产出"预测增强 EMS 有效工作区域图"（Predictive EMS Effective Region）：`Benefit_PredEMS = f(lambda_P, R_gen, tau_gen, SOC₀, 负荷动态)` 的可视化与定量结论，回答 RQ2；Phase 1 基于 oracle 而非真实 AI 模型，命名不得使用 "AI"。
- **FR-013A**: THE Phase 1 SHALL 包含三个匹配的受控对照：`C0` Baseline（无预测）、`C_P` Oracle Peak-only、`C_MS` Oracle Multistep + EMS_MinFF；`C_P` 与 `C_MS` SHALL 从同一 oracle 负荷轨迹与同一 horizon 提取未来信息（`P_peak^oracle(H) = max_{τ∈(0,H]} P_load(t+τ)`），使 RQ1 严格隔离"多步时序信息 vs 单峰值信息"的边际控制价值，排除预测模型误差混淆；`C_P` 的控制律映射方式在 design 定案（建议与 EMS_MinFF 共用控制律、仅降级输入信息）。

### Phase 1 过门（GO/NO-GO）

- **FR-014**: THE Phase 1 GO 判定 SHALL 分为两类。**非劣化约束**（所有声称有效的参数点必须全部满足）：①船速 RMSE 退化 ≤1%；②`|ΔSOC_end|` ≤0.005（1200 kWh 电池 ≈6 kWh；IF design 阶段数值重复性分析证明过严，THEN 可放宽但不得超过 0.01 且 SHALL 记录依据）；③燃油 ≤ C0×1.015；④`E_unserved` = 0；⑤不得产生 Baseline 没有的欠压事件。**收益判据**：在 λ_P×R_gen 网格至少 4 个相邻参数组合 × 至少 2 个场景中——⑥`P_bat,peak` 下降 ≥15%；⑦`P_bat,RMS` 或电池吞吐量下降 ≥10%；⑧对于 Baseline 已进入母线压力区（接近欠压降额线）的工况，`Vdc_min` 明显改善或欠压事件减少。GO = 非劣化约束全 PASS 且收益判据⑥⑦（压力区工况含⑧）成立；IF oracle 完全知道未来负荷仍无法满足，THE 研究 SHALL 停止神经网络投入并输出否定性结论报告。

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
- [ ] AC-2（FR-002/005/007/008）：单元级验证：合成阶跃负荷 + 参数化发电机下，`P_gen,reachable(τ; P_ff)` 前向递推与理论解析解一致；`R(τ)≤0` 时 EMS 与 Baseline 逐点一致；`R(τ)>0` 时 `P_ff*` 为使 `R(τ)≤0, ∀τ` 的最小值且不越机组限幅。
- [ ] AC-3（FR-003/004/010/013A）：Phase 1A 三锚点 × 6 档 horizon × C0/C_P/C_MS 三对照结果表与 Phase 1B 入选 horizon 的依据记录；C_P vs C_MS 对比明确回答 RQ1。
- [ ] AC-4（FR-011/012/013）：参数扫描结果表 + 预测增强 EMS 有效工作区域图（λ_P × R_gen 二维主图 + τ_gen/SOC₀ 单因素补充）。
- [ ] AC-5（FR-014）：GO/NO-GO 判定报告，非劣化约束与收益判据逐条给出机器可读 PASS/FAIL 与证据链接。
- [ ] AC-6（FR-015/016/017，Phase 2）：四模型精度表 + 闭环对照表 + x_ship 消融表；冷启动窗口纳入训练的证据。
- [ ] AC-7（FR-018，Phase 2）：冷启动兜底消融：无兜底/仅兜底/兜底+NN 三组在 t<22 s 段的 Vdc_min 对比。
- [ ] AC-8（FR-019，Phase 3）：STM32N6 部署报告（大小/RAM/延迟/映射率/INT8 精度损失）+ HIL 闭环结果。

## Edge Cases

- 预测负荷全部低于发电机可达功率（R(τ) 恒 ≤ 0）：EMS 全程等价 Baseline，前馈零激活（须记录激活时间占比以证实"最小必要"）。
- 渐进负荷场景（无显著阶跃）：前馈结构性闲置为合法行为，不得为制造激活而调参。
- 极端参数组合（λ_P=0.7 且 R_gen=20 kW/s）：发电机本身能力不足导致的欠压属于被控对象极限，报告须区分"EMS 失效"与"系统固有不可行"。
- 多步预测越界（仿真末端不足 H）：沿用端点保持语义并记录。
- SOC 接近下限时 `P_bat_allowable → 0`：风险门槛自动收紧，前馈趋于即时前馈行为，须验证平滑过渡无抖振。
- 参数扫描中模型重建：仅改 config 参数不得触发结构重建；扫描脚本须复用已编译模型（fast restart 或等效）。

## Dependencies and Constraints

- 依赖冻结基线 `model/simulink_modelC/`（只读）：构建体系、config 惯例、场景定义、指标体系脚本、既有 EMS 三变体与 sched_ff 实现（作为命名与结构参考）。
- 依赖 `model/simulink/`（只读）：原始 Model B 构建体系与数据集导出管线。
- MATLAB/Simulink 环境与现有工程一致；Phase 2 训练使用 `.venv_tf`；Phase 3 使用 ST Edge AI 工具链。
- 排序约束：副本与参数化 → 多步通道与 EMS_MinFF → 单元验证 → horizon 扫描 → 参数扫描 → GO/NO-GO →（GO 后）Phase 2 spec 细化。
- Git 纪律：本地 main 分叉（4 个未推送提交与 origin/main 内容等价）保留待用户决定，本 spec 工作从 `feat/multistep-predictive-ems`（基于 origin/main）推进。

## Open Questions

- [x] OQ-1（v1.1 已决）：GO 判据拆分为非劣化约束（船速 RMSE ≤1%、|ΔSOC_end|≤0.02、燃油 ≤C0×1.015、E_unserved=0、无新增欠压）+ 收益判据（≥4 相邻参数组合 × ≥2 场景：P_bat,peak ↓≥15% 且 RMS/吞吐量 ↓≥10%，压力区工况另要求 Vdc_min 改善）——已落入 FR-014。
- [x] OQ-2（v1.1 已决）：`P_bat_allowable = P_bat,max × f_SOC(SOC) × f_V(Vdc)` 分段线性，定位为 EMS 风险评估阈值而非电池命令——已落入 FR-006；系数函数具体折点在 design 定案。
- [x] OQ-3（v1.1 已决）：λ_P×R_gen 5×5 全因子主图 + τ_gen/SOC₀ 代表值单因素补充；λ_P 分母为统一设计参考最大负荷——已落入 FR-011。
- [x] OQ-4（v1.1 已决）：5 s 等分辨率真实多步节点 [P_5,…,P_H]（H/5 个），通道固定最大宽度 12 节点 + 有效节点数参数；Phase 2 四网络统一输出 H*/5 个节点——已落入 FR-004/FR-015。
- [ ] OQ-5：Phase 1A 三锚点（Fast/Nominal/Slow）的具体参数取值——design 阶段定案。
- [ ] OQ-6：`P_load,design,max`（λ_P 统一分母）的取值——design 阶段依据场景负荷统计定案。

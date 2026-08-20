# 面向参数不确定性的多步负荷预测增强型 EMS Brainstorm

Spec: `multistep-predictive-ems`  
Status: brainstorm-complete  
Created: 2026-08-20

## User Intent

## 背景与用户意图

前序研究 timed-feedforward-ems Phase 1 过门 NO-GO：C2 定时前馈（Peak + t_peak 双输出）相对 C1 即时前馈平均节油仅 0.293%（阈值 2%），节油上限被电池充放往返损耗机理锁死（~0.5%）。但实证发现：(1) C2 以 1/10~1/18 前馈激活时间实现与 C1 相同的 Vdc_min 增益（689.9→707.8 V）；(2) t_peak 悬崖式不对称敏感性（高估 +5 s 保护全失，低估 −20 s 仅 +0.1 kg 油）；(3) 真实 MLP 下安全增益消失（t<22 s 历史窗口冷启动盲区）；(4) 原 Peak-only AI EMS 已能降低电池放电能量 30~50% 而船速不变。

用户与 GPT 外部咨询对齐后作出研究方向决策：从"节油"和"Peak+t_peak 控制律"转向"面向发电机参数不确定性的多步负荷预测增强型 EMS"。用户已明确批准本方案（含 D1–D8 固定决策与三处修正），批准即开始执行。

## 已批准的固定决策（D1–D8，不得重新设计）

- D1：放弃节油核心目标。一级指标为动态安全与电池负担（Vdc_min、欠压事件、P_bat,peak、P_bat,RMS、电池吞吐量）；燃油降级为约束/代价指标。研究问题变为"AI 以多少燃油代价换取多少电池负担与动态安全改善"。
- D2：预测目标改为短时多步负荷序列 [P_{t+5},…,P_{t+H}]；horizon 为实验变量 {10,15,20,30,45,60} s，不预设 60 s 最优。
- D3：AI 只预测负荷、不学习控制策略、不依赖发电机参数；EMS 层参数化（P_rated/R_gen/τ_gen/P_bat_allow(SOC,Vdc) 全部提为 config 参数）。
- D4：控制律为最小必要前馈：R(τ)=P̂_load(t+τ)−P_gen,reachable(t+τ)−P_bat,allow>0 才干预，求最小 P_ff 满足 Vdc≥V_safe 与机组限幅；电池合理使用而非避免使用。
- D5：Phase 1 纯 oracle 多步预测、零神经网络；GO 后才训练 NN。
- D6：发电机参数不确定性转化为研究设计：扫描 λ_P∈{0.7..1.1}、R_gen∈{20..60} kW/s、τ_gen∈{2..10} s、SOC₀∈{0.4..0.8}，代表性组合而非全笛卡尔积；最终产出"AI 有效区域图"。
- D7：Peak+t_peak 线冻结为否定性研究结果入论文；−5 s t_peak 偏置仅作附录敏感性，不作创新点；不再专门优化 t_peak MAE。
- D8：timed-feedforward-ems requirements v2 草稿作废；仅 FR-014 冷启动规则兜底保留，降级为 Phase 2 工程组件。

## 用户批准的三处修正（相对 GPT 原建议）

- 修正 1：horizon 扫描先于发电机参数扫描（先固定 1000 kW/40 kW/s 确定有效时间尺度，再扫参数），减少仿真空问。
- 修正 2：冷启动规则兜底移出 Phase 1（oracle 无历史窗口依赖，Phase 1 结果纯净），在 Phase 2 与真实 NN 一起评估其增量贡献（本身构成消融点）。
- 修正 3：x_ship 消融升级为 Phase 2 独立实验项（with/without x_ship + 事件位置随机化），防止"位置记忆"污染 RQ1 结论。

## 研究问题

- RQ1：短时多步负荷预测相比单一未来峰值预测，能否提供更有效的 EMS 前馈信息？
- RQ2：发电机额定功率、动态爬坡能力及电池功率裕度如何影响预测增强 EMS 的实际收益（有效工作区域）？
- RQ3：在不同动力系统参数与工况下，何种轻量网络能以较低 STM32N6 部署代价获得最佳闭环控制效果？

## Phase 划分

- Phase 0（已完成）：Baseline → Peak-only → t_peak 定时前馈 → NO-GO 冻结（simulink_modelC/ 为冻结基线）。
- Phase 1：oracle 多步预测 + 参数化 EMS（最小必要前馈）+ horizon 扫描 → 参数扫描 → GO/NO-GO（六条判据：P_bat,peak↓、P_bat,RMS/吞吐量↓、Vdc_min/欠压裕度↑、航速不降、|ΔSOC_end| 受控、燃油代价可接受）。
- Phase 2：MLP/CNN1D/small-TCN/GRU 四模型同一多步任务训练，闭环指标优先选型，x_ship 消融，冷启动规则兜底。
- Phase 3：Top1~2 模型 INT8 → STM32N6 Neural-ART NPU → UART/CAN/UDP 软硬件闭环（HIL）。

## 执行结构

- 新 spec：.specs/multistep-predictive-ems/；timed-feedforward-ems spec 标记 superseded 归档保留。
- 模型工程：simulink_modelC/ 冻结；Phase 1 复制为 simulink_modelC_(oracle_ms)/；Phase 2 赢家模型再复制 simulink_modelC_(模型名)/。
- Git：main 内容已与 origin/main 等价（PR #8 squash 合并），feat/multistep-predictive-ems 从 origin/main 开出；本地 main 分叉（未推送的 4 个提交）保留待用户决定，不擅自 reset。

## 遗留疑问（design 阶段定量化后再次确认）

- Phase 1 GO 判据的具体阈值（各指标下降/改善幅度、燃油代价上限）。
- P_bat_allow(SOC,Vdc) 的具体函数形式。
- 参数扫描的代表性组合选择方法（正交/拉丁方/单因素）。

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

---

## 第二轮评审修订（2026-08-20，GPT 外部评审，pi 裁决全部接受）

1. **D4 改写**：R(τ) 是功率代数量，不能在线求解 Vdc 约束（Vdc 是母线电容+下垂自治的状态量）。控制律定为 `P_ff* = min P_ff s.t. R(τ)=P̂_load−P_gen,reachable(τ;P_ff)−P_bat,allowable ≤ 0, ∀τ∈[0,H]`；Vdc_min 回归闭环评价指标与 GO 约束。pi 认定原写法隐含不存在的 DC-bus 预测器，属实质错误。
2. **撤回 pi 修正 1（horizon 单锚点先行）**：最优 H 依赖 R_gen/τ_gen，单点定死会污染 RQ2。改为两级筛选：Phase 1A 三锚点（Fast/Nominal/Slow）× 6 horizon 粗筛 → Phase 1B 保留 2~3 个 H 进参数扫描。
3. **OQ-4 废除末值保持**（H>30 零新增信息，属自欺）：改 5 s 等分辨率真实节点 H/5 个；工程上通道固定最大宽度 12 节点 + 有效节点数参数。
4. **OQ-1 GO 拆两类**：非劣化约束（全部必须满足）+ 收益判据（≥4 相邻参数组合 × ≥2 场景）；Baseline Vdc_min=689.9 V 本就高于 675 V 降额线，硬要 +5 V 会误杀有效点。
5. **P_bat_allow → P_bat_allowable**：明确定位为 EMS 风险评估阈值（非电池命令、非物理极限），实际电池输出仍由底层下垂自治。
6. **λ_P 分母锁定统一设计参考最大负荷** P_load,design,max，禁止逐场景自定义。

---

## 第三轮评审修订（2026-08-20，GPT 外部评审，用户裁决"修订后批准"，pi 全部接受）

1. **新增 C_P Oracle Peak-only 对照（FR-013A）**：原 Phase 1 只有 Baseline vs Oracle-MultiStep，回答不了 RQ1（多步 vs 单峰值的边际价值）。三对照 C0/C_P/C_MS 同 oracle 源、同 horizon。pi 提出更优雅的实现留待 design：C_P 与 C_MS 共用 EMS_MinFF，仅把输入信息降级为全窗口恒值 P_peak（最坏时序假设），形成纯粹的信息含量递减实验，排除控制律差异混淆。
2. **|ΔSOC_end| 0.02 → 0.005**：1200 kWh 电池下 0.02=24 kWh 过宽（S07 的 ΔSOC=0.010≈12 kWh 已是过度发电实证）；默认 0.005，design 重复性分析后可放宽至上限 0.01 并记录依据；不采纳 terminal-energy 折算方案（assumed BSFC 引入更多假设）。
3. **λ_P 分母统一**：FR-002 与 FR-011 一致使用 P_load,design,max。
4. **更名**：Phase 1 产物"AI 有效区域图"→"预测增强 EMS 有效工作区域图"（oracle 阶段不得称 AI）。

用户批准状态：requirements v1.1 + v1.2 修订 → **requirements-approved**。design 阶段重点审核项：Fast/Nominal/Slow 三锚点取值、统一 P_load,design,max、f_SOC/f_V 折点、P_ff* 数值求解方法。

---

## Design 阶段定案（2026-08-20，design.md v1 draft）

- **OQ-5 三锚点**：Fast(60 kW/s, 2 s) / Nominal(40, 5) / Slow(20, 10)——D6 上下界 + 现状。
- **OQ-6 P_load,design,max = 800 kW**：Mot_P_nom(700, 图纸聚合电机额定=推进电功率物理天花板) + hotel(100)。现状 1000 kW 机组 = λ_P 1.25。
- **λ_P 网格修正 {0.7..1.1} → {0.8, 0.9, 1.0, 1.1, 1.25}**（偏离 D6 示例值，理由：原网格全部低于现状机组，区域图不含当前设计点）——列为 design 批准点。
- **P_ff* 求解**：爬坡→一阶惯性前向递推（dt_r=0.1 s）+ 对分法 20 迭代（R_max 关于 P_ff 单调不增）。
- **C_P 实现**：EMS_MinFF 内 MSFF_InfoMode=peak 折叠轨迹为全窗口恒值 P_peak，同律同源同 horizon。
- **f_SOC 折点 [0.30, 0.45]，f_V 折点 [690, 720] V**，ASSUMED(low)，±50% 敏感性抽查。
- **仿真预算 ≈550 次**：1A=78（3 锚点×2 场景×13），1B≈470；FastRestart+parsim，降级顺序 S06→单因素密度。

---

## Tasks 批准修订（2026-08-20，用户裁决 approved-with-amendments，pi 全部接受）

1. **T-003 oracle 因果性硬约束**：必须取自欠压降额上游的外生需求负荷，对控制律选择不变——否则 Vdc→降额→实际负荷 的闭环会污染"未来真值"。
2. **T-006 欠压双阈值分离**：N_UV/T_UV @675 V（真实欠压，GO 判据⑤依据）与 N_stress/T_stress @700 V（压力区裕度）分开命名。
3. **T-009 预算参数化 472~572 次**：由 T-008 入选 horizon 数决定；C0 跨 horizon 复用。
4. **T-010 禁止自动放宽 SOC Gate**：重复性 FAIL 先定位非确定性来源；阈值放宽须重新过门。
5. **执行模式裁决**：主 Agent 监督 + 单 implementation subagent 顺序施工，禁止多 subagent 并行操作同一 Simulink 工程；T-003/T-004 后主 Agent 审查，T-008/T-011 决策关卡。

Status: tasks-approved。

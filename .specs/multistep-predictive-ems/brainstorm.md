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

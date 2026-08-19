# Model B：直流母线电气层 + 安全/节能指标卡 + S10 工况 Brainstorm

Spec: `model-b-dc-bus`  
Status: brainstorm-complete  
Created: 2026-08-19

## User Intent

User intent: 保持 Model A（功率平衡层，数据集/AI 对比平台）不动，并行开发 Model B——加回直流母线电气层（750 V 母线电容 + 电池双向 DC/DC + 发电机整流 + 推进逆变，全部平均值模型），使母线电压 V_dc(t) 可观测；用统一"安全+节能"指标卡评估 Baseline（规则门限）与预测增强 EMS；开发结束交付两种控制逻辑的纯文本流程图。
Target users: 用户本人（硕士论文作者）、论文评审、后续 STM32N6 工程复用。
Problem/outcome: Model A 无法观测母线电压（电气层被刻意移除），而内部安全（电压稳定+动力储备）是论文论证 EMS 价值的核心维度。期望产出：Model B 模型、可重复构建/测试脚本、V_dc 观测与储备指标、Baseline vs 预测 EMS 对比证据、纯文本控制逻辑流程图。
In scope: ① Model B 电气层（方案甲平均值模型，复用 Model A 参数/工况/EMS 变体）；② 指标卡落地（外部安全：v_rmse/v_drop/E_unserved；内部安全：V_dc 波动/跌落深度/恢复时间/P_margin_min/SOC 储备/P_bat_max；节能：fuel_kg + FEC 等效燃油，k_eq≈0.505 kg/kWh）；③ 新增 S10 水流反转工况（先逆水后顺水，里程触发渐变，默认 2500 m 处 15 s）；④ Baseline vs AI 在 Model B 上对比运行与报告；⑤ 纯文本流程图（两种 EMS 逻辑）。
Out of scope: 开关级 PWM 模型；母线保护动作/断路器/故障注入；发电机启停逻辑；Model A 的任何修改；HIL 硬件联调；电池老化降额算例（用户明确暂不考虑）；S10 以外的新工况（后续独立迭代）。
Constraints/risks: 双支路等效下电气层取聚合单母线（750 V）+聚合支路，需注明与真实双母线差异；母线电容 C（沿用 0730 的 20 F）、DC/DC 动态参数无图纸来源→全部 ASSUMED_ 标注+敏感性说明；固定步长 ode3 @ 0.01 s 下需验证电气动态数值稳定（C 与变换器带宽匹配，避免刚性）；不得修改 Model A 及其数据集；另一开发线在同仓库工作→独立分支开发。
Proposed approach: 基于最新 main 开分支 → model/simulink/ 下以副本方式扩展构建脚本（新增子系统 DC_Bus、Battery_Converter、Genset_Rectifier、Propulsion_Inverter、Bus_Voltage_Monitor）→ 指标卡计算并入 07 对比脚本 → S10 加入 Model_Scenarios.m → Model B 跑 S01/S04/S07/S10 对比 → 出图出报告 → 交付流程图 → git 提交推送。
Assumptions: 母线额定 750 V；电池侧 576 V 经 DC/DC 接入；C_bus=20 F（沿用 0730，标 ASSUMED）；电压正常带暂按 750 V ±5%（ASSUMED，待确认）；k_eq=BSFC_avg/η_chg≈0.505 kg/kWh（ASSUMED，ECMS 思想）。
Remaining doubts: S10 反转里程点与斜率（默认 2500 m/15 s，需求阶段可定）；750 V 等级的报警/保护门限值待确认（暂按 0730 的 500/600 V 相对 576 V 的比例缩放初设）。

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

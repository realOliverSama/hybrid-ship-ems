# EMS01 CHIL v3 — soft-HIL 真闭环实验 Brainstorm

Spec: `ems01-chil-v3-closedloop`  
Status: brainstorm-complete  
Created: 2026-09-02

## User Intent

用户意图：把 ems01-chil-ecms-v1 的开环激励回放升级为真闭环 soft-HIL——主机被控对象（plant）模型每控制拍（1 s）现场单步推进，板载 EMS 输出 P_gen 实时回馈功率平衡（SOC/油耗由板上决策驱动），验证"控得好"，为论文收官章节提供闭环实验数据。

已裁决决策（2026-09-08 brainstorm）：
- Q1 层② S20 98.877% 边际 FAIL：(a) 接受并记录为口径修订已知代价，v3 基线说明登记，层② 门禁语义降为参考指标；闭环验证以层① 保真 + 闭环指标为主，层③ 继续报告性；门禁不放宽。
- Q2 被控对象推进器实现：P3 主案（被控对象子集模型脚本构建 + Simulink Coder codegen → C + Python ctypes，单一来源由构造保证；Simulink Coder 已确认安装于 R2026a）；P3 受阻时回退 P1（Python 方程转录 + 轨迹等价门禁）并显式说明原因。
- Q3 节拍：lock-step（每 CMD_TICK 一拍：发→收 RSP→推进被控对象→下一拍），固件零改动，CMD_TICK/RSP_TICK 协议不变；1 Hz 实时列入 out-of-scope。
- Q4 实验矩阵：4 臂 × 2 场景（S20 + voyage_002）：(a) 板载 NPU 镜像闭环；(b) 板载 REF 镜像闭环（同一 harness/被控对象，(a)vs(b) 零混杂隔离 NN 增益）；(b′) ModelD 原生仿真 law=7 EML（零新工作量，(b)vs(b′) 兼作被控对象仪器闭环交叉验证）；(c) ModelD 原生仿真 law=5 基线。
- Q5 两层指标：仪器级（闭环 (b) vs 开环 ref_G2 一致性、(b) vs (b′) 轨迹等价）+ 研究级（任务油耗主指标、SOC 终点偏移伴随指标 + 公平性容差带、航速跟踪 RMSE、临界工况行为含 FR-508 回退代价）。容差数值 design 定。
- 术语：统一"被控对象（plant）"——被控对象模型（plant model）、被控对象推进器（plant advancer）、被控对象子集模型（plant-only variant）；handoff 原文"植物"为误译，spec 产物一律用新术语。

范围：被控对象子集构建+codegen 链+推进器；两道仪器门禁（子集 vs 完整 ModelD 开环对照；codegen C vs 解释执行轨迹等价，轨迹级容差不承诺位级）；闭环 harness（基于 ems_inject/ems_fault_inject 骨架）；4 臂实验；两层指标与机器可读结果；板上会话（用户在场/远程授权+令牌）；报告与进展日志。
非范围：1 Hz 实时、真 HIL/快速原型、路线 B 分类器改进、law=5 固件化、层② 收敛研究、AI 建模层改动。
约束/风险：Simulink 全脚本驱动、主 ModelD 不动；门禁机器可读禁放宽；fw_gate 缓存构建陷阱沿用；P3 实操可行性未验证（回退已定）；SOC 公平性容差带与闭环 (b) vs 开环参考判据数值留 requirements/design。
遗留疑虑：被控对象子集提取若发现植物与 EMS 内部状态隐藏耦合，须 escalate 而非硬拆。
建议 slug：ems01-chil-v3-closedloop（项目根 .specs/，EMS 工程 .specs/ 建符号链接沿袭 v1 模式）。

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

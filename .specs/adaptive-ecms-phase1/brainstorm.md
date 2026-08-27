# Phase 1 实时 ECMS 控制器链与 DP gap 测量 Brainstorm

Spec: `adaptive-ecms-phase1`  
Status: brainstorm-complete  
Created: 2026-08-24

## User Intent

## User Intent

Phase 1：在 Phase 0 已证明的 DP 节油上限（连续口径同可行域 3.6~4.6%）基础上，构建**实时 ECMS 控制器链**并测量其与 DP 的 optimality gap。最终目标（Phase 2/3 另行立项）：NN 预测增强 A-ECMS 部署 STM32N6。

## 承继决策（adaptive-ecms-methanol brainstorm D1–D13）

- D1：上限验证先行（已完成）；D6：启停分层；D9：主链路一维定速 BSFC；D10：三档稳健性分级；D11：不预设负荷预测（Oracle 先行验证增量）；D12：工程严谨性打包；D13：GO_TO_PHASE1 + 研究诚信红线（不得人为恶化参数匹配膨胀收益，场景多样化只走真实运行工况）。

## 本轮新决策（2026-08-24，用户确认/授权）

- E1：**Phase 1 范围 = 选项 (a)**：Fixed ECMS + SOC-Adaptive ECMS + Oracle Predictive A-ECMS + DP gap 测量；NN 训练不在本 Phase（Phase 2 独立立项，届时以 C3−C2 gap 作为 NN 必要性的量化证据）。
- E2：**复合航次为头条评估场景**（主 Agent 设计草案 V1 已获用户认可方向：90 min/21 km、11 航段、低负荷段占 ~53%）；基础 8 场景保留作回归。
- E3：**评估基准不冻结**：现有参数（含假设项）足以证明控制逻辑有效性；设备方/设计方数据（船舶主尺度、排水量、设计航速、试航速度-功率曲线、机组最低稳定负荷/爬坡率/分工况油耗）回来后统一重标定重跑。用户估计实船约 500 t 级（现模型 1000 t 为 legacy 假设，待确认）。
- E4：**用户批量授权（2026-08-24）**：后续任务不再逐个征求意见，直接施工；执行工作委派 subagent；主 Agent 保留设计决策与最终验收。🟡/🔴 纪律项仍按 AGENTS.md 执行（不 merge main、不改 gate 阈值、不碰冻结目录）。
- E5：研究诚信红线适用于 Phase 1 全部实验设计：对比组参数一致、场景构造仅走真实工况、oracle 仅用于上界/增量验证不得进入实时控制器声称。

## Phase 1 研究问题

- RQ1：固定等效因子 ECMS 能捕获 DP 上限的多大比例（optimality gap）？
- RQ2：SOC 反馈自适应（A-ECMS）相对固定 λ 的增量？
- RQ3：完美未来信息（Oracle Predictive A-ECMS）相对 C2 的增量——即"预测有没有经济性价值"的上界判定（Phase 2 NN 立项的量化依据）。

## 实验矩阵（消融链）

C0 Baseline（规则）/ C1 Fixed ECMS / C2 SOC-AECMS / C3 Oracle Predictive A-ECMS / DP 离线标杆。场景：复合航次（头条）+ S01/S02/S07（代表子集）+ 全 8 场景回归。

## Remaining Doubts

1. 复合航次的速度-功率标定在当前 1000 t 假设下进行；500 t 若证实，统一重标定重跑（E3）；
2. λ 的标定方法（电量维持）在 design 定案；
3. Oracle predictive 的 λ 前瞻律具体形式在 design 定案；
4. 设备方/设计方数据回收时间不确定，不阻塞 Phase 1。

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

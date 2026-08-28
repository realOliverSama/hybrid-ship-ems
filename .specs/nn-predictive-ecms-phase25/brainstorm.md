# Phase 2.5 NPU 导向模型族优化与工况识别门控 Brainstorm

Spec: `nn-predictive-ecms-phase25`  
Status: brainstorm-complete  
Created: 2026-08-28

## User Intent

用户意图：在 models-research 分支开展 Phase 2.5——在 Phase 2（MLP 302-128-64-1，capture=37% PARTIAL）成果上优化预测模型。成功标准（用户三答）：① capture 尽量高（省油，主指标）；② 预测精度指标好（副指标）；③ 在保证 1 Hz 实时性前提下模型尽量复杂/精确，以体现 STM32N6 Neural-ART NPU 的算力优势。

已确认决策：
1. 模型族按顺序研究：TCN（扩张因果卷积+残差块+MLP 头，主推——Neural-ART NPU 原生加速 Conv1D/Dense；LSTM/GRU 会回落 CPU，不用于 NPU 展示）→ CNN-MLP 混合 → N-BEATS 式全连接栈；均带多分位数输出头（q10/q50/q90，对症低估偏向 under_freq=0.426 并给门控提供不确定性依据）。
2. 输入信息边界（用户明确）：不允许使用"开船前已知的航线规划"作为输入特征（不符合实船可得性设定）；允许"航行中根据当前航行状态在线工况识别（逆水/顺水/高机动等）→ 参数调整/门控"——即预测器输入保持纯历史外推，工况信息经由识别器进入控制器而非预测器。
3. 扰动设定：本船为内河船（长江危化品应急处置船），不做"海况扰动增强"。当前模型实际只有确定性段级水流（顺水/逆水 ±1 m/s）与阻力带，无随机环境扰动；Phase 2 R²≈0.73 的不可解释方差主要来自未来航段调度的结构不可知性。数据集 v2 的多样性增强须以内河工况机制为主（段参数范围/组合/段内流速变化/交通避让减速等），且须先经物理论证评审。
4. 数据集划分科学性确认：保持航次级 split（voyage-level，140/30/30 或按 N 等比）、窗口不跨航次、归一化 fit-on-train、S20/S01/S02/S07 结构性隔离；测试集为复合航次（每条含 3~6 个中间段），非单一工况。用户关注点"数据集多样性决定模型上限"纳入 WP：K-shape 聚类做覆盖度审计。
5. NanoEdge AI 弃用（回归库不走 NPU，且以 Ridge 类轻量线性模型为主，不适合本目标）。
6. 执行模式：主 Agent 只产出 spec/plan，实施由用户安排的低成本执行模型（subagent）完成。

文献锚点（用户提供，已转换阅读）：
- Yan, Chen, Gao (JMSE 2025, doi:10.3390/jmse13020269)：NMPC+工况识别；实船 CNOOC 257 数据（98,143 条@1Hz），K-means+肘部法 4 工况（速度+负荷功率），CNN+ECA 注意力分类器（3 卷积+2 ECA+3 池化+2 FC，NPU 友好），按工况调整 NMPC；结果 −5.5% 燃油（无噪声）/−2.6%（10% 噪声）。佐证：在线工况识别→控制器调整是文献标准做法；CNN 分类器完全适配 N6 NPU。
- 沈崇崇等（哈尔滨工程大学学报 2026, doi:10.11990/jheu.202503040）：最优经验注入多 NN——PMP 全局最优序列训练 NN；在线 GRNN 识别航行模式→激活子网络输出等效因子 λ→SOC+Aheff 双状态反馈修正；vs A-ECMS 总成本 −7.17%。佐证：NN→等效因子+模式门控路线；与我们 Phase 1 law6 v1（加性 λ 通道，失败 +2.1%）的差异在训练信号（最优经验模仿 vs 预测驱动）、门控（子网络 vs 全局单通道）、反馈（双状态 vs 单 SOC）。我们已有 Phase 0 DP 轨迹可作"最优经验"来源（候选方向，本 Phase 列为评估项不列主线）。

开放疑问：① 数据集 v2 具体扰动机制与 N 规模待 design 评审；② TCN 规模上限需按 N6 NPU int8 实测预算推导；③ DP 模仿学习 λ 路线是否立项为后续 Phase；④ 门控触发新控制律（law=8?）需主 Agent 设计门。

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

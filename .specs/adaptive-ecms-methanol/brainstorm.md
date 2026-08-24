# 边缘AI自适应ECMS甲醇混合动力船舶EMS Brainstorm

Spec: `adaptive-ecms-methanol`  
Status: brainstorm-complete  
Created: 2026-08-24

## User Intent

## User Intent

新研究方向：**基于边缘 AI 自适应等效燃料消耗最小化（A-ECMS）的甲醇混合动力船舶能量管理策略**（GPT 建议，用户认可作为主线）。

核心思想：利用动力电池作为能量缓冲，使甲醇发电机组尽量运行在 BSFC 高效率区；直流母排下机组转速与推进负荷解耦，利用负荷预测实现前瞻性能量调度（预知高负载→提前在低效区少发电/在高效点预充电，避免机组被迫快速冲到高功率低效点），最终以节省甲醇燃料为目标。边缘 AI 指最终部署到 STM32N6。

## 前序研究碰壁总结（两条 NO-GO 路线，作为本方向的反面教材）

1. **timed-feedforward-ems（C2 定时前馈）**：节油仅 0.293%（阈值 2%）→ NO-GO。根因：优化变量是功率时序，节油上限被电池充放往返损耗锁死（~0.5%），属机理级失败、参数不敏感。
2. **multistep-predictive-ems（oracle 多步预测+最小必要前馈）**：T-011 机器判定 NO-GO——有效工作区域为空。根因：电池瞬态能力（≥1000 kW）+ 机组容量覆盖所有场景需求峰值，oracle 上界无可优化空间。**注意：此结论参数依赖**，仅在被扫参数域内成立，不可过度推广。
3. **共同教训（本项目第一纪律）**：必须先用离线最优（DP/网格搜索）验证收益上限存在，再投入控制器施工。前两次都是做到 Phase 1 末尾才发现上限不存在。

## 本方向与前两次的本质区别

- 优化变量从"功率时序"变为"BSFC 运行点"，收益来源是 BSFC 曲线非线性（低效区 vs 高效区差异 20–40%），属机理级真实节油；
- 预测从"主角"降级为"增量改进"：ECMS 不依赖预测也能工作，研究架构更稳健，消融实验可分离贡献；
- 机组长期部分负荷运行（负荷跟随）= 低效区驻留时间长 = 运行点优化空间大。

## 已确认的设备真实参数（来源：20260526设备参数.docx，单侧值×2=聚合）

- 发电机组：500 kW/侧 → 1000 kW；400V AC/50Hz/1500rpm；✅ 与模型吻合
- 推进电机：350 kW/侧 → 700 kW；✅ 吻合
- 母线 DC 750V；电池 576V；齿轮箱 6.08:1；✅ 吻合
- **电池容量真实值 535.68 kWh/侧 → 1071.4 kWh**（模型现值 1200 kWh，高估 11.8%，施工时须修正）
- 电池放电上限：DC/DC 瓶颈 750V×700A=525 kW/侧 → **1050 kW 聚合**（模型 1000 kW 偏保守，可接受）
- 机型锁定：**潍柴 WH17ME 纯甲醇系列，大概率为 6WH17C680ME5（500 kW @ 1500 rpm）**，火花塞点火+进气道多点喷射
- **BSFC 两个真实锚点：最优点 427 g/kWh（厂家标称，η≈42.3%）；额定点 453(+5%) g/kWh @ 500 kW（η≈39.9%）**
- 螺旋桨最大推力 90 kN/桨 vs 模型系柱推力 ≈85.5 kN 聚合（约 2× 偏差，仅影响近零速工况，EMS 研究暂不处理）

## 已批准的方向性决策

- D1：Phase 0 = 离线收益上限验证（DP/网格搜索 + 真实锚点 BSFC 曲线），GO/NO-GO 门先行，用户认可此验证方式；
- D2：转速按**可变速（解耦）**推进，BSFC 按二维 map 设计；保留一维（1500 rpm 定速线）退化分析作为对照（防范硬件核实后翻案）；
- D3：BSFC 图构造 = 文献典型中速甲醇机曲线形状 + 双锚点标定（427 min / 453 rated）+ 低负荷恶化三档敏感性（25% 负荷处 +30%/+50%/+70%）；
- D4：用户授权后续施工时对模型参数做合理修正（容量 1071.4 等）；
- D5：当前阶段禁止施工；正式施工前禁止压缩上下文，靠本文件持续固化共识对抗丢失；
- D6：**启停策略分层纳入**——Phase 0 离线计算同时输出"仅连续运行"与"允许启停"两个上限（两档结果本身就是论文素材）；Phase 1 先做连续运行 ECMS；启停逻辑推迟到 Phase 2+ 视 Phase 0 两档差距决定；最低稳定负荷在 Phase 0 用典型值参数化（如 30%/40%/50% 额定三档）一并出敏感性；
- D7：**Phase 0 GO 判据（主判据）**：连续运行口径下节油上限 **≥5%**（同时要求非平凡——非仅由极低 SOC 工作点或端部效应产生）；<2% NO-GO；2~5% 灰区由用户裁决。

## Remaining Doubts（待澄清，按优先级）

1. AC/DC 变流器类型待硬件核实（二极管整流→定速；有源前端→可变速）——决定二维 map 是主线还是扩展情景；
2. 仍缺真实数据的参数：爬坡率（现 20 kW/s×2）、τ（现 5 s）、最低稳定负荷真实值、低负荷 BSFC 形状、电池充放电效率（现 0.95）；最低稳定负荷与爬坡率建议向设备方索取；
3. 预测在论文中的定位与消融设计；
4. Phase 0 的具体技术方案细节（DP 状态/控制网格离散化、场景选择、BSFC 曲线族具体形状参数）——留待 requirements/design 阶段定量化。

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

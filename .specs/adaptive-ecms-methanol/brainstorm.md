# 边缘AI自适应ECMS甲醇混合动力船舶EMS Brainstorm

Spec: `adaptive-ecms-methanol`  
Status: brainstorm-complete  
Created: 2026-08-24

## User Intent

## User Intent

新研究方向：**基于边缘 AI 自适应等效燃料消耗最小化（A-ECMS）的甲醇混合动力船舶能量管理策略**（GPT 建议，用户认可作为主线）。

核心思想：利用动力电池作为能量缓冲，使甲醇发电机组尽量运行在 BSFC 高效率区；直流母排下机组转速与推进负荷解耦（按可变速推进，但主 Gate 不依赖二维 map——D9）。**不预设负荷预测**（D11）：Phase 2 先研究状态反馈自适应等效因子，预测需消融证明后才引入。边缘 AI 指最终部署到 STM32N6。

## 前序研究碰壁总结（两条 NO-GO 路线，作为本方向的反面教材）

1. **timed-feedforward-ems（C2 定时前馈）**：节油仅 0.293%（阈值 2%）→ NO-GO。根因：优化变量是功率时序，节油上限被电池充放往返损耗锁死（~0.5%），属机理级失败、参数不敏感。
2. **multistep-predictive-ems（oracle 多步预测+最小必要前馈）**：T-011 机器判定 NO-GO——有效工作区域为空。根因：电池瞬态能力（≥1000 kW）+ 机组容量覆盖所有场景需求峰值，oracle 上界无可优化空间。**注意：此结论参数依赖**，仅在被扫参数域内成立，不可过度推广。
3. **共同教训（本项目第一纪律）**：必须先用离线最优（DP/网格搜索）验证收益上限存在，再投入控制器施工。

## 本方向与前两次的本质区别

- 优化变量从"功率时序"变为"BSFC 运行点"，收益来源是 BSFC 曲线非线性（低效区 vs 高效区差异 20–40%），属机理级真实节油；
- 预测不预设：ECMS 不依赖预测也能工作，预测仅在与 DP 有 gap 且消融证明有效时加入；
- 机组长期部分负荷运行（负荷跟随）= 低效区驻留时间长 = 运行点优化空间大。

## 已确认的设备真实参数（来源：20260526设备参数.docx，单侧值×2=聚合）

- 发电机组：500 kW/侧 → 1000 kW；400V AC/50Hz/1500rpm；✅ 与模型吻合
- 推进电机：350 kW/侧 → 700 kW；✅ 吻合
- 母线 DC 750V；电池 576V；齿轮箱 6.08:1；✅ 吻合
- **电池容量真实值 535.68 kWh/侧 → 1071.4 kWh**（模型现值 1200 kWh，高估 11.8%，施工时须修正）
- 电池放电上限：DC/DC 瓶颈 750V×700A=525 kW/侧 → **1050 kW 聚合**（模型 1000 kW 偏保守，可接受）
- 机型锁定：**潍柴 WH17ME 纯甲醇系列，大概率为 6WH17C680ME5（500 kW @ 1500 rpm）**，火花塞点火+进气道多点喷射
- **BSFC 两个真实锚点：最优点 427 g/kWh（潍柴官方产品页 product.weichai.com WH17ME 页，访问 2026-08-24，施工时存档 PDF——FR-018）；额定点 453(+5%) g/kWh @ 500 kW（docx）**
- 螺旋桨最大推力 90 kN/桨 vs 模型系柱推力 ≈85.5 kN 聚合（约 2× 偏差，仅影响近零速工况，EMS 研究暂不处理）

## 已批准的方向性决策

- D1：Phase 0 = 离线收益上限验证（DP/网格搜索 + 真实锚点 BSFC 曲线族），GO/NO-GO 门先行，用户认可此验证方式；
- D2：转速按可变速（解耦）设计，但——见 D9 修正；
- D3：BSFC 图构造 = 文献典型中速甲醇机曲线形状 + 双锚点标定（427 min / 453 rated）+ 低负荷恶化三档敏感性（25% 负荷处 +30%/+50%/+70%）；
- D4：用户授权后续施工时对模型参数做合理修正（容量 1071.4 等）；
- D5：正式施工前禁止压缩上下文，靠本文件持续固化共识对抗丢失；
- D6：启停策略分层纳入——Phase 0 同时输出"仅连续运行"与"允许启停"两个上限；Phase 1 先做连续 ECMS；最低稳定负荷在 Phase 0 用 {0,30,40,50}% 档位参数化；
- D7：Phase 0 GO 判据基线：连续口径节油上限 ≥5% GO；<2% NO-GO；2~5% 灰区用户裁决——经 D10 升级为三档稳健性分级；
- D8（2026-08-24，用户裁决）：**双机独立调度本期不讨论**，Phase 0 保持聚合单机（默认两侧一同出力），协调出力留待后续；GPT 评审的"假 NO-GO"风险以 NO-GO 措辞限定聚合口径 + 登记未来扩展（机组群最小燃耗函数 ṁ_f,min(P_total, N_on)）对冲；
- D9（2026-08-24，采纳 GPT 评审）：**主 Gate 仅用 1500 rpm 定速一维 BSFC 曲线**；二维 map 降为条件敏感性/扩展分析，待真实机组转速范围/整流器形式/二维燃耗 map 获取后方可升级；
- D10（2026-08-24，采纳）：Gate 三档分级——GO_ROBUST（保守档 +30% 恶化下 ≥4/8 场景 ≥5% 且非平凡）/ DECISION_REQUIRED（含仅中高档达标的 CONDITIONAL）/ NO-GO（三档全 <2%）；
- D11（2026-08-24，采纳）：Phase 2 不预设负荷预测；先研究 s* = f(SOC, 负荷, 工况, ...) 状态反馈自适应等效因子；预测仅在 Fixed/A-ECMS 与 DP 有明显 gap 且消融证明有效时加入；
- D12（2026-08-24，采纳）：工程严谨性打包修正——终端 SOC 对称带 |ΔSOC|≤0.005；P_demand 因果隔离（降额上游提取 + 双控制律不变性验证）；DP/Baseline 同物理可行域条件校验；电池符号约定写死；427 锚点来源存档（FR-018）。

## Remaining Doubts（待澄清，按优先级）

1. AC/DC 变流器类型待硬件核实——仅影响二维扩展分析可否升级，不阻塞主判定；
2. 仍缺真实数据的参数：爬坡率（现 20 kW/s×2）、τ（现 5 s）、最低稳定负荷真实值、低负荷 BSFC 形状、电池充放电效率（现 0.95）；最低稳定负荷与爬坡率建议向设备方索取；
3. Phase 0 具体技术细节（DP 网格、场景集、启动代价初值）已在 design v1.1 定案；
4. 预测消融实验设计——Phase 2 立项时再定。

## 评审存档

- `reviews/gpt-design-review-20260824.md`：GPT 对 requirements v1 + design v1 的评审与采纳裁决表。

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

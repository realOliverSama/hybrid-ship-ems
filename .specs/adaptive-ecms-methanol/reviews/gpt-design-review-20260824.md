# GPT 外部评审 — design v1（2026-08-24）

来源：用户转述 GPT 对 `.specs/adaptive-ecms-methanol/` requirements v1 + design v1 的评审。
用户裁决：第 1 条（双机粒度）**明确不讨论**，Phase 0 保持聚合单机、默认两侧一同出力，协调出力留待后续需要时再设计。其余条款的采纳情况见下表。

## 采纳裁决汇总（主 Agent 评估，用户确认流程中）

| # | 意见 | 裁决 |
|---|---|---|
| 1 | 双机独立启停粒度（N_on ∈ {0,1,2}）进入上限验证 | 不采纳（用户裁决），以 NO-GO 措辞限定 + 未来扩展登记对冲"假 NO-GO"风险 |
| 2 | 二维可变速 BSFC map 退出主 Gate，降为条件敏感性 | 采纳 |
| 3 | DP 与 Baseline 必须同物理可行域（条件校验） | 采纳 |
| 4 | 终端 SOC 改对称带 \|ΔSOC\| ≤ 0.005 | 采纳 |
| 5 | P_demand 因果隔离（欠压降额上游提取 + 不变性验证） | 采纳 |
| 6 | 电池充放电符号约定写死 | 采纳 |
| 7 | Gate 三档稳健性分级（GO_ROBUST / DECISION_REQUIRED / NO-GO） | 采纳 |
| 8 | 427 g/kWh 锚点原始来源存档 | 采纳（来源：潍柴 WH17ME 官方产品页 product.weichai.com，访问日期 2026-08-24；施工时存档页面 PDF） |
| 9 | Phase 2 不预设负荷预测（先状态反馈自适应等效因子，预测需消融证明） | 采纳 |

## 评审原文要点（非逐字，逐字稿见会话记录）

- 方向评价 9/10；技术链 DP → ECMS → AI-AECMS → STM32N6 正确；DP 作为离线全局最优 benchmark、ECMS 适合实时船舶 EMS 有综述支持（doi:10.3390/en19133033）。
- 核心风险排序：双机粒度 > 二维 map 主 Gate > 同可行域/同终端 SOC > P_demand 因果隔离。
- "推进电机可变速 ≠ 甲醇发电机组一定可变速"——二维 map 需真实机组转速范围/整流器形式/二维燃耗图支持。
- DP 可能通过人为构造的最优转速线获得虚假节油。
- 若 Baseline 可行域大于 DP（如 P < P_min_stable），"DP ≤ Baseline"校验前提不成立。
- SOC₀=0.60 时 ≥ 下界写法允许 DP 借用 5.36 kWh 初始电量，在 2~5% 判定量级下不可接受。
- P_demand 必须控制不变、取自欠压降额上游（援引本项目前次 oracle 因果污染教训）。
- Gate 不应由单一人为中档 BSFC 曲线决定。
- 427 g/kWh 精确数值需保存原始厂家资料引用，否则答辩不可辩护。
- Phase 2 建议先研究 s* = f(SOC, P_load, P_gen, v_cmd, operating mode, route state, ...)，只有 Fixed/A-ECMS 与 DP 存在明显 gap 且消融证明未来信息有效时才加预测。

# 毕业论文写作启动包（thesis writing kickoff）

> **本文件的用途**：在新的写作 session 中（配合
> [research-writing-skill](https://github.com/norman-bury/research-writing-skill) 工作流），
> 把本 prompt 与材料清单直接交给写作 agent。文件自包含，写作 session 无需本对话上下文。
> 生成于 2026-09-20，对应研究成果状态：Phase 4 全部收官（Phase4 分支 65df467e）。

---

## 0. 使用步骤（给用户）

1. 建论文工作目录（建议与工程仓库分离）：
   ```bash
   mkdir -p ~/Projects/thesis-hybrid-ems && cd ~/Projects/thesis-hybrid-ems
   git clone https://github.com/norman-bury/research-writing-skill
   # 按该仓库 README 将 skill 安装到本目录
   ```
2. 在该目录打开新的 agent 会话，首条 prompt 发送：
   > 请阅读 `/Users/oliverlyu/Projects/hybrid-ship-ems/docs/thesis_writing_kickoff.md`，
   > 按该文件第 5 节的指令启动论文写作工作流。
3. brainstorm 问答中，本文第 1 节已预答大部分问题，agent 只需补问学校模板信息。

---

## 1. 任务与论文信息（brainstorm 预答）

我要写**学位论文初稿**，请按 research-writing-skill 的工作流执行
（brainstorm → 章节规划 → 逐章写作 → 自审）。预答如下：

- **类型/语言**：工学硕士学位论文，中文（代码标识符、变量名、文件名、
  臂名 P0/C0/C1/C2 等保留英文原形）；
- **学校模板**：【待填：学校名/院系/篇幅要求/模板文件位置】；
- **题目方向**：混合动力船舶能量管理系统的负荷预测与预测式节能控制
  ——基于 Simulink 仿真与预注册对照实验（题目可优化）；
- **研究一句话**：在 Simulink Model E 平台上，用 48 条冻结航次 × 4 臂
  （P0 规则基线 / C0 持续性预测 / C1 完美信息 / C2 神经网络 Predictor-v3）
  = 192 条运行的预注册对照仿真实验证明：预测型 EMS 相对传统规则控制
  节能约 6.5%（燃料当量 J_FEC），且不牺牲硬安全约束；并解释预测精度
  在闭环中的价值上限（λ 饱和机制）；
- **方法学亮点（务必突出）**：预注册纪律（冻结航次表/冻结统计链/冻结种子
  bootstrap）、机器可读门禁（G1–G9 验收全 PASS）、证据哈希链
  （每个工件带 sha256，evidence_index.json 密封）——这是与常见论文的
  最大差异化。

## 2. 章节结构建议（可调整）

1. 绪论（背景、文献综述、研究内容与创新点）
2. 混合动力船舶系统建模（Model E：物理模型、PMS 安全层、工况生成器 v3）
3. 负荷预测模型（Dataset v3；Predictor-v3 五架构对比 → reatt 获胜；
   IID R²=0.9931，OOD R² 0.914~0.980）
4. 预测式 EMS 控制律（law6 预测 ECMS + Δλ 修正网格；J_FEC 与 k_eq 定义）
5. 正式对照实验（预注册设计、192 运行、FC-3 可行性重判、效果分析：
   B_perfect +0.156pp / B_predicted +0.148pp / L 不显著 / R=0.944 /
   C* vs P0 ≈ −14.5 kg/航次，p≈2.5×10⁻⁹）
6. 安全性与鲁棒性分析（非劣化结论、V_dc 裕量、k_eq 敏感性与 break-even）
7. 总结与展望（三条结论 + 未做实验的边界：OOD 闭环、多种子、MPC 变体、
   NPU 部署实测、HIL 实时化）

## 3. 权威材料清单（一切数字从此引用，禁止自行重算或编造）

工程仓库根：`/Users/oliverlyu/Projects/hybrid-ship-ems/`

| 材料 | 路径（相对仓库根） | 用途 |
|---|---|---|
| **结果章核心** | `model/simulink_modelE_(Phase4_Physics)/results/ppe_practical/formal_epoch3_genB/_analysis/EPOCH3_GENB_EFFECT_REPORT.md` | 全部关键数字、8 条 Limitations、3 条可直接使用的结论措辞 |
| 论文图表 5 张 | 同上 `_analysis/figures/fig1..fig5*.png` | 英文标签；如需中文标签可用 `_analysis/scripts/ep3_figures.py` 改造重绘 |
| 效果分析工件 | 同上 `_analysis/effect_analysis_*_epoch3.json`（6 个） | 逐航次数据、bootstrap CI、敏感性、P0 对比 |
| 安全性对比 | 同上 `_analysis/safety_descriptive_epoch3.json` | V_dc/跟踪分臂对比（结论=非劣化） |
| 预测器精度 | 同上 `_analysis/predictor_v3_offline_accuracy_summary.json`；`results/predictor_v3/FINAL_WINNER_EVIDENCE_PACKAGE.json` | IID/OOD/三种子/架构对照 |
| 部署附录 | `results/predictor_v3/st_cloud_notes.md` | ST Edge AI 评测记录（附录素材） |
| 验收与密封 | `formal_epoch3_genB/EPOCH3_GENB_FORMAL_ACCEPTANCE.json`、`evidence_index.json` | G1–G9 门禁、25 工件哈希链（完整性叙事） |
| 方法学叙事 | `docs/phase4_research_roadmap.md`（A–F 链）、`docs/research_roadmap.md`（Phase 1–3 前史） | 方法章骨架与"研究历程" |
| 模型结构 | 各模型目录 `README_Model.md`（如 `model/simulink_modelD_(EMS_NN_AECMS)/`、Model E 工作区） | 建模章节素材 |
| 过程诚实性素材 | `项目开发进展.md`（仓库根） | Attempt-A 回归与封存、判据演进、失败教训——绪论/讨论章的诚信素材 |
| 早期阶段报告 | `docs/reports/`（soft-HIL 总结报告 docx + 图） | Phase 2.5/3 章节素材（如论文覆盖全周期） |

## 4. 硬性规则（写作 agent 必须遵守）

1. **数字零编造**：所有实验数字必须出自第 3 节工件；不确定就问，不要估。
2. **结论边界不得夸大**（EFFECT_REPORT §7 已给措辞）：
   - 安全性只能写"**非劣化**"（硬约束全满足、零有效 fallback、母线距
     675 V 降额线裕量 ≥29 V），**不能写"安全性显著优化"**（V_dc std
     C* 臂还略高 +2.3~2.6 V）；
   - **R=0.944 不得解读为"预测模型非常准确"**——模型准确性的主证据是
     离线指标（IID R²=0.9931）；R 的含义是"精度足以榨取该控制配置下的
     可用收益"，且受 λ 饱和（84–86% 决策压 −0.10 下界）限制；
   - 节油量级必须绑定 canonical k_eq=0.5457 kg/kWh（1.2×k_eq 下 B 量级
     趋于 ~0.01pp，符号稳健但量级条件性必须披露）；
   - C 系臂间排序（C2 vs C1 vs C0）统计不可分辨，不得宣称优劣。
3. **文献零编造**：参考文献必须真实可溯源；不确定的标 `[TODO: 核实]`，
   不得虚构作者/年份/期刊。
4. **数据来源声明**：全部为 Simulink 仿真数据（dataset v3），不是实船实测；
   论文任何位置不得暗示后者。
5. **术语表**（全文一致）：P0=传统规则基线(law5 SOC 自适应)；C0=持续性
   预测；C1=完美未来信息参照；C2=Predictor-v3(reatt 架构，交付种子 s0)；
   J_FEC=燃料当量（fuel + ΔSOC×1071.36 kWh×k_eq）；FC-3=可行性判据 v3
   （动态 PMIN 包络）；tier=负荷波动分层 LOW/MID/HIGH。

## 5. 给写作 agent 的第一步指令

按 skill 工作流启动 brainstorm；对已预答的条目直接确认，只向我追问
【学校模板/篇幅/导师格式要求】与【章节结构调整偏好】；然后产出章节规划
（`chapters/`），逐章写作时先读第 3 节对应材料再动笔。

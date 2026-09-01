# HANDOFF — v3：CHIL 闭环测试（soft-HIL 闭环，研究收官）

> 前置状态（2026-09-08 晨）：ems01-chil-ecms-v1 全部完成 + workclf 口径修订 v1.1 已实施并板上复验
> （层① 100% exact；层③ 98.4%+；板载分类器对设计真值 75.8%、upstream 9.7%≈设计 10.9%）。
> 遗留：层② S20 98.877% 边际 FAIL 待用户裁决（M2 报告 §11.4）——**不阻塞 v3 启动**。
> 必读（按序）：`docs/agent/AI_MODELING.md` §22 → `m2_port_report.md` §11 →
> `.specs/ems01-chil-ecms-v1/design-amendment-workclf-caliber-draft.md` → 进展日志 09-07/09-08 条目。

## 目标

把现有"开环激励回放"升级为**真闭环 soft-HIL**：主机 Simulink 植物模型现场单步推进，
板载 EMS 的 P_gen 实时回馈功率平衡（SOC/油耗由板子决策驱动），验证"控得好"——
为整个研究收官提供闭环实验数据（论文核心章节素材）。

## 为什么现在能做（已有资产）

1. **协议形状已闭环就绪**：CMD_TICK（7 状态量→板）/ RSP_TICK（P_gen←板）就是锁步交换格式；
2. **板载固件已验证**：层① 移植保真 100%、层② NPU 端口误差受控、FR-508 回退鲁棒；
3. **参考链可信**：ref_G2_*.csv（v1.1 口径）可作为闭环初值的开环对照基线。

## 技术要点（brainstorm→design 需逐项定案）

1. **植物模型侧**：Simulink 模型（HybridShip_ModelD_DCBus，law=7 G2 已修订口径）拆出
   植物子集（船速/功率平衡/SOC/油耗），每控制拍（1 s）单步推进：
   输入 P_gen（来自板）→ 功率平衡 → SOC(k+1)、P_load(k+1)、v_ship(k+1)。
2. **节拍与同步**：1 Hz 实时节拍 vs lock-step 握手二选一（串口 RTT ~5ms，1 Hz 余量巨大；
   建议 lock-step 起步——确定性最好，论文可复现）。
3. **闭环协议增补**：现有 CMD_TICK 无需改；主机侧新增"植物推进器"（MATLAB 引擎绑定或
   预生成+插值？——需设计决策）；SOC 反馈闭合后，开环参考 SOC 轨迹不再适用，需定义
   闭环评价指标（油耗 vs 开环基线、SOC 轨迹保持、临界工况行为）。
4. **对照实验设计**：闭环下 (a) 板载 NN-EMS vs (b) Simulink 纯软件 EMS vs (c) law=5 基线
   三方油耗/SOC 对比——论文收官数据。
5. **回退行为在闭环下的语义**：FR-508 的 ê=0 回退在闭环中意味着什么（安全降级），
   需在 design 中定义验收。

## 约束与红线（沿袭）

- Simulink 全脚本驱动；门禁机器可读；禁止为 PASS 调整阈值；
- 板操作需用户在场（或远程明确授权 + 令牌）；fw_gate 缓存构建陷阱（M2 §11.5）；
- AI 建模相关先读 AI_MODELING.md；层② S20 边际 FAIL 的裁决结论影响 v3 基线选择。

## 建议启动动作

1. 本提示词作为 `.specs/` 新 spec（如 `ems01-chil-v3-closedloop`）的 brainstorm 输入；
2. 用户裁决层② S20 遗留（影响 v3 基线口径）；
3. design 定案节拍/协议/指标后，按 T-508 模式复用 ems_inject/ems_fault_inject 骨架
   开发闭环推进器。

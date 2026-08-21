# Phase 1 报告：多步负荷预测增强 EMS（EMS_MinFF）GO/NO-GO 判定

Spec: `multistep-predictive-ems`（T-011，AC-5 / FR-014）
日期：2026-08-21 ｜ 判定：**NO-GO**（机器判定，`results/ms_phase1/gate_result.json`，git `f673b8c5`）
分支：`feat/multistep-predictive-ems`

---

## 1. 执行摘要

**Phase 1 结论：NO-GO——在合理参数域内，"oracle 多步负荷预测 → 最小必要前馈"（EMS_MinFF）对
本船型（1000 kW 机组 + 1000 kW 电池 vs 800 kW 设计负荷）无收益；有效工作区域为空。**

关键数字：
- 非劣化约束：**74/74 配对全部 PASS**（Phase 1A 36 对 + lite 32 对 + T-008B 6 对）；
- 收益判据：**0 组合 × 0 场景**满足（阈值 ≥4 相邻组合 × ≥2 场景）→ **FAIL**；
- 机理证据：控制权限存在（正对照 max|ΔP_gen_actual|=190.9 kW、提前 45 s 平台期爬坡），
  但域内所有真实工况下 CMS 与 C0 **指标逐位一致**（14/16 严格零，2/16 角点为 9.2 kW 负收益噪声瞬态）。

---

## 2. 研究问题回顾

- **RQ1（信息含量）**：C_P（仅峰值标量）vs C_MS（完整轨迹）是否产生差异？
- **RQ2（有效工作区域）**：在 λ_P×R_gen 参数域内，预测增强 EMS 在何处带来
  P_bat,peak↓≥15% 且 RMS/吞吐量↓≥10% 的收益？

---

## 3. 方法与可信度

### 3.1 工程链路（可复现、机器门禁）

| 环节 | 产物 | 状态 |
|---|---|---|
| 独立副本 | `simulink_modelC_(oracle_ms)/`（从冻结 `simulink_modelC/` 派生） | ✅ |
| 构建+冒烟 | `20_build_modelCms.m` + `20b_smoke_modelCms.m` | ✅ 3/3 PASS（本次复跑确认） |
| 单元验证 | `20c_unittest_minff.m`（U1–U6） | ✅ 全 PASS |
| 参数化 | `apply_gen_params.m` / genEff 结构 | ✅ |
| oracle 因果性 | `make_oracle_ms.m`（降额上游外生需求负荷） | ✅ 12 节点抽查 <1e-6 |
| 复现性硬门禁 | `v5_validate_repro.m`（R1 跨进程逐位 / R2 顺序无关 / R3 ref 级激活 / R4 冻结回归） | ✅ R1–R4 全 PASS（`repro/v5_compare_20260821_082030.log`） |
| 冻结回归 | Nominal×S04 C0 vs 冻结基线 S04,E0 | ✅ max-rel-diff=0 |

### 3.2 T-004R 返工记录（工程教训）

T-007 暴露 T-004 实现缺陷：`MSFF_Horizon_s/MSFF_InfoMode` 被实现为 MATLAB Function
**Parameter 作用域（编译期）数据**，sim() 循环中 base workspace 变更不可靠触发重编译，
叠加 `set_ai_logging` 端口记录状态改动干扰变更检测 → law=3 跨会话不可复现（首批次
`phase1A_INVALID_20260821/` 已隔离作废）。

修复（主 Agent 决策，`f498033f`）：
1. 两参数改 **Constant 块驱动的 chart 输入端口**（每次 update diagram 重新求值）；
2. 批运行器重构 `run_ms_sim.m`：所有 per-run 变量经 `Simulink.SimulationInput.setVariable` 注入；
3. 新增 v5 复现性硬门禁（跨进程逐位一致），双进程实测全 PASS。
4. R3 判据修正：nominal 锚点 fuel 逐位相同是设计预期（ff 窗口内爬坡全程饱和，
   `max|ΔP_gen_actual|=0.000000 kW` 实证），改为 ref 级证据（max|ΔP_gen_ref|>1 kW）。

**教训**：编译期 Parameter 作用域数据不得用于 sim() 循环切换；参数扫描必须经
`SimulationInput.setVariable` 或等价运行时注入机制。

---

## 4. 结果

### 4.1 Phase 1A（78 次，3 锚点 × S04/S07 × 6 horizon × C0/C_P/C_MS）

| 锚点 | C0 fuel [kg] (S04/S07) | C_P/C_MS（全部 6 horizon） | ff_active | max\|ΔP_gen_actual\| |
|---|---|---|---|---|
| Fast (R=60,τ=2) | 91.62 / 78.63 | = C0 逐位一致 | **0 s**（可达性判定无缺口） | 0 |
| Nominal (R=40,τ=5) | 91.17 / 78.36 | = C0 逐位一致 | 9 s（冷启动） | 0 |
| Slow (R=20,τ=10) | 90.15 / 77.66 | = C0 逐位一致 | 20 s（冷启动） | 0 |

全部指标（fuel/Vdc_min/SOC_end/P_bat_peak/P_bat_rms/E_bat）在每组合内逐位一致。

### 4.2 T-008B 弱机点（W1: λ_P=0.8, R_gen=20, τ=10；S07/S09 × H{20,45,60}，8 次）

- 全部 6 个 CMS 运行 **max|ΔP_gen_actual|=0.000000 kW**；ff 仅冷启动 [3,23] s，爬坡饱和度 100%；
- S07 阶跃（t≈566 s，幅值 606 kW）窗口 R_max=−394.7 kW：控制律正确判定无缺口
  （gen 640 + 电池 1000 = 1640 kW > 需求峰值 ~1000 kW），Vdc 仅探至 719.6 V；
- 机理数据包：`results/ms_phase1/t008b/mechanism_analysis.md` + 双场景时序图 PNG。

### 4.3 正对照实验（控制权限直接检验，主 Agent 执行）

注入合成 1900 kW 未来阶跃（超出 gen 1000 + 电池 1000 包络），Nominal CMS H45
（`t008b/positive_control_S07_H45.mat`，本次复核数值）：

| 证据 | 数值 |
|---|---|
| ff 激活时刻（相对假阶跃 t=300） | **t=255.4 s（提前 45 s = H）** |
| P_ff\*（最小必要，非满幅） | ≈192 kW |
| 平台期 pre-ramp（P_gen_actual） | 709 → 900 kW |
| max\|ΔP_gen_actual\| | **190.9 kW** |
| 窗口过后 | ff 自动归零、actual 回归基线 |

旁证：注入 1300 kW（未超包络）时控制律正确保持静默——**最小必要性质双向成立**。

### 4.4 T-009-lite 稀疏域确认（16 点 λ_P×R_gen × S07/S09 × C0/CMS，64 次 + H60 补测 2 次）

| λ_P | R_gen=20 | 30 | 40 | 60 |
|---|---|---|---|---|
| 0.80 | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
| 0.90 | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
| 1.00 | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
| 1.25 | 0.000000 | 0.000000 | 0.000000 | **9.212807** |

（max|ΔP_gen_actual| [kW]，S07 与 S09 相同）

- **14/16 严格零**；角点 (1.25, 60) 9.2 kW 噪声级瞬态（ff 释放时 τ=10 惯性回溢 ~15 s，
  Δfuel=+0.013% 负收益方向），H60 补测相同 → 非 horizon 依赖；
- 包络裕度：min margin ∈ {−160, −80, 0, +200} kW（负值=冷启动 f_V 塌缩段，与 ff 激活窗口
  100% 重叠），mean margin ∈ [892, 1353] kW；
- **矛盾信号（margin<0 且 ff 未激活）= 0**；
- 图：`results/ms_phase1/phase1B_lite/lite_maps_S07.png` / `lite_maps_S09.png`
  （control-authority + envelope-margin 双面板热力图）。

---

## 5. RQ1 回答：信息含量在域内不可区分

Phase 1A 全量 + lite + T-008B 中，C_P 与 C_MS **逐位一致**（同为 0 收益）——
在"控制权限未被行使"的工况下，信息表示（轨迹 vs 峰值折叠）无可区分。

正对照表明：**控制律对轨迹信息响应正确**（提前量随 H 精确作用）——信息含量问题只在
"有权限工况"（预测缺口超出机组+电池包络且非爬坡饱和段）下才有意义；本船型包络内不存在此工况。

**结论（RQ1）**：信息含量差异在本研究域内不可观测；判定对信息表示不敏感。

---

## 6. RQ2 回答：有效工作区域为空

在 λ_P ∈ {0.8..1.25} × R_gen ∈ {20..60} kW/s × S04/S07/S09（τ=10、SOC₀=0.6）
所覆盖的合理参数域内，**没有**任何 (组合 × 场景) 满足收益判据（0/0 vs 阈值 4/2）。

**物理边界（域属性，非实现缺陷）**：

```text
收益存在性的必要条件：
  预测缺口 max_τ(P_hat(τ) − P_gen,reachable(τ) − P_bat_allowable(τ)) > 0
  且 该缺口不在"爬坡饱和段"（幅值前馈物理不可达）
```

本船型（gen 1000 kW + 电池 1000 kW 瞬态能力 vs 800 kW 设计负荷）：
- 场景需求峰值 ≤ ~1000 kW < gen 1000 + 电池 1000 = 2000 kW 包络；
- 唯一真实缺口（冷启动 f_V 塌缩）落在爬坡饱和段——机组已按最大爬坡率运行，
  幅值前馈无法改变实际出力（实证 max|ΔP_gen_actual|=0.000000 kW）；
- 参数扫描降低 gen 容量/爬坡后（λ_P=0.8 → 640 kW），包络仍 640+1000=1640 kW > 需求峰值。

**结论（RQ2）**：有效工作区域在合理参数域内为空；零收益为系统域属性。

---

## 7. NO-GO 否定性结论框架

对"多步负荷预测增强 EMS"研究方向的含义：

1. **收益存在性预检不等式**（可推广为同类系统的先验判据）：
   `P_load,max < P_gen,rated + P_bat,max,dis` 时，预测增强 EMS 无收益；
   且冷启动/大阶跃段的爬坡饱和使幅值前馈失效——预测型前馈的适用前提是
   **存在低负荷平台期** 且 **缺口超出电池瞬态裕度**。
2. **本船型不满足该前提**：电池瞬态能力（1000 kW）主导裕度，机组容量覆盖稳态负荷，
   预测信息无行使空间。
3. **研究价值定位**：本结论为"oracle 完全已知未来负荷仍无收益"的否定性证据——
   按 FR-014 条款"IF oracle 完全知道未来负荷仍无法满足，THE 研究 SHALL 停止
   神经网络投入并输出否定性结论报告"，**Phase 2/3（NN 训练、STM32N6 部署）不予执行**。
4. **正对照保留价值**：1900 kW 合成注入证明控制律实现正确、机理完备（提前量=H、
   最小必要双向成立）——"律正确、域无用"边界清晰，为后续改型（如电池弱化船型/
   冷启动预充）提供可直接复用的验证方法学。

---

## 8. 局限与偏差记录

| 项目 | 说明 |
|---|---|
| T-009 全量 472 次 → 64 次稀疏网格 | 用户裁决（2026-08-21）：目标改为"验证区域为空"；16 点 × S07/S09 × C0/CMS，H=45；角点 H60 补测 |
| T-010 未独立执行 | 重复性由 v5 门禁覆盖（跨进程逐位一致，R1b/R2b PASS）；折点敏感性在零收益域内无符号可翻转（moot）——经用户确认纳入 NO-GO 路径 |
| horizon 验证范围 | Phase 1A {10,15,20,30,45,60} 全覆盖；lite 固定 45 + 边界 60 补测；选型 {20,45} 因零收益无实际影响 |
| C_P 折叠定义 | 恒值折叠为"仅知峰值"的最坏时序解释（design 已声明）；因零收益未产生差异，不影响结论 |
| f_SOC/f_V 折点 | ASSUMED(low)；零收益域内敏感性无意义，未触发 escalate 条件 |

---

## 9. 产物索引

| 类别 | 路径 |
|---|---|
| 判定 | `results/ms_phase1/gate_result.json`、`results/ms_phase1/gate_judge_20260821_123801.log` |
| Phase 1A 数据 | `results/ms_phase1/phase1A/runs.csv`（78）、`summary_1A.csv`、`run_1A_20260821_082052.log` |
| lite 数据 | `results/ms_phase1/phase1B_lite/runs.csv`（64）、`summary_lite.csv`、`lite_facts.md`、`lite_maps_S07.png`、`lite_maps_S09.png` |
| T-008B 数据 | `results/ms_phase1/t008b/runs.csv`（8）、`mechanism_analysis.md`、`mechanism_metrics.csv`、`t008b_S07_H45.png`、`t008b_S09_H45.png`、`positive_control_S07_H45.mat` |
| 复现性门禁 | `results/ms_phase1/repro/v5_compare_20260821_082030.log` |
| 报告 | `reports/MSFF_phase1A_horizon.md`（T-008 + T-008B 补记）、本文件 `reports/MSFF_phase1_gate_report.md` |
| 脚本 | `21_run_ms_horizon.m`、`21b/21c`（T-008B）、`22_run_phase1B_lite.m`、`22b_analyze_phase1B_lite.m`、`23_ms_gate_judge.m` |
| 关键提交 | `f498033f`（T-004R+1A）、`dd6df5e9`（T-008B）、`f673b8c5`（gate 基线） |

---

## 10. Phase 2/3 处置

按用户裁决与 FR-014 条款：**不进入 Phase 2（NN 训练）与 Phase 3（STM32N6 部署）**。
正对照保留为"控制律 control authority + 系统域属性"的机制证据；
收益存在性预检不等式（§7.1）作为后续研究方向的先验筛选工具。

---

*报告由 `23_ms_gate_judge.m` 的机器判定结果驱动撰写；所有数字与 `gate_result.json` / 各 runs.csv 一致。*

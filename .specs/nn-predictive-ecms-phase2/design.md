# Phase 2 NN 预测型 A-ECMS — Design

Spec: `nn-predictive-ecms-phase2`
Status: design-approved（用户 2026-08-25 确认 Q1/Q2——brainstorm F1~F5）
Created: 2026-08-25
Requirements: `./requirements.md`（v1）

## 1. 总体架构

```text
50_voyage_generator.m → 航次定义集（种子固定）→ 51_generate_fleet_profiles.m
   （parsim 批量 Baseline 闭环 → results/ecms_phase2/fleet_profiles/*.mat）
        ↓
python/build_dataset.py → 窗口化 + 航次级 split + 归一化（fit-on-train）
        ↓ datasets/fleet_windows.npz（+norm 参数）
python/train_predictor.py → MLP / LSTM（同数据同 split）→ val 选模
        ↓ 入选模型权重导出（JSON: W/b + norm）
52_integrate_law7.m → build_model_c.m law=7 内联前向 → 单测（块 vs Python）
        ↓
53_run_phase2_matrix.m（{C2,C3,C4}×{S20,S01,S02,S07} + test 航次抽样）
        ↓
54_phase2_gate_judge.m → results/ecms_phase2/gate_result.json
```

## 2. 航次生成器（FR-201/202）

- 航段积木（类型, v 范围 kn, 时长范围 min）：离泊(3, 3~6)、港区限速(5, 5~12)、经济巡航(8, 10~25)、赶班巡航(11, 8~18)、桥区限速(5, 6~12)、逆水(11 + 水流−1, 6~12)、靠泊(3, 3~6)；
- 组合规则：离泊开头、靠泊结尾，中间随机 3~6 段；总时长 60~120 min；种子 `ECMS_FLEET_SEED`（config）；
- 解析预检：P ≈ Hull_K_R·v³/η链 + 100 kW 逐段估算，任一段 >850 kW → 拒绝重抽；
- 抽样 Simulink 验证：每 20 个航次抽 1 个全仿真确认零降额；
- 输出 `results/ecms_phase2/fleet/voyages_seed<S>.json`（航次定义）+ 剖面 .mat（不入库）；
- N 默认 200；parsim 不可用时降级 ≥80（NFR-202）。

## 3. 数据集（FR-203/204）

- 窗口：步长 10 s；输入 [P_demand(t−299:t) 300 维, v_ship(t), accel(t)]；目标 Ê_H(t) = ∫_t^{t+300} P_demand dt / 3600 [kWh]（尾端丢弃不足 300 s 的窗口）；
- Split：按航次 140/30/30（种子固定，打乱在航次级）；S20/S01/S02/S07 排除在生成集之外（结构性保证）；
- 归一化：输入 per-feature mean/std、目标 mean/std，仅在 train 拟合 → `datasets/fleet_norm.json`；
- 泄漏检查记录：split 后航次名单入库；归一化仅用 train；窗口不跨航次。

## 4. 模型与训练（FR-205~207）

- MLP：300+2 输入 → Dense 64 relu → Dense 32 relu → Dense 1（Ê_H 归一化）；~22k 参数 ≈ 88 KB fp32（NFR-201 内）；可选小网格 {hidden 64/128, lr 1e-3/3e-4, epochs ≤100, early stop on val}；
- LSTM 对照：输入 reshape [300,1] → LSTM 32 → Dense 1；同 split 同指标；
- 指标：val MAE/RMSE + 低估频率/幅度（Ê 低估 = 未来需求低估）；选模只看 val；
- 种子固定；实验记录 §19 全字段 → `results/ecms_phase2/training_log.csv`（失败实验保留）。

## 5. law=7 集成（FR-208）

- 权重导出 JSON（W1/b1/W2/b2/W3/b3 + norm）；Simulink 侧 MATLAB Function 内联前向：`y = W3·relu(W2·relu(W1·x+b1)+b2)+b3`（约 22k 次乘加/秒，无性能问题）；
- 输入装配：P_load 历史缓冲（300×1 s Ring Buffer）+ v_ship + accel；ê_NN = (y·std+mean − Ē_H)/E_scale；SOC_ref = 0.60 + K_S·ê_NN（K_S=0.01 沿用）；λ 钳位 0.43 沿用；
- 单测：随机 100 输入，块输出 vs Python 推理 |Δ|<1e-5（fp32 一致）；Ē_H/E_scale 缺失回退 ê=0（≡law=5 行为）；
- 10b smoke 绿。

## 6. 实验与 Gate（FR-209/210）

- C4 矩阵：{C2,C3,C4} × {S20,S01,S02,S07}（C2/C3 复用 Phase 1 数值，C4 新跑）+ test 航次抽 10 个跑 C2/C4（泛化，只跑一次）；
- nn_oracle_capture(S20) = (fuel_C2 − fuel_C4)/(fuel_C2 − fuel_C3) = (330.66 − fuel_C4)/5.70；
- recommendation：≥50% PROCEED_TO_PHASE3；30~50% PARTIAL；<30% LIMITED；
- 泛化摘要：test 航次上 C4 vs C2 的 saving 差分布（中位数/最小值）。

## Testing Strategy

| 层级 | 内容 | FR |
|---|---|---|
| 单元 | law=7 块 vs Python 前向一致性（100 随机输入，1e-5）；回退行为 | FR-208 |
| 数据 | 泄漏检查（split 名单、归一化来源、窗口不跨航次） | FR-203/204 |
| 公平性 | MLP/LSTM 同数据同指标记录 | FR-205 |
| 集成 | 10b smoke；C4 仿真无降额/SOC 入带检查 | FR-212 |
| 复现 | 训练种子固定；矩阵关键行双跑 | FR-212 |
| 门禁 | gate_result.json schema + 构造单测 | FR-210 |

## Requirements Traceability

| FR | 设计章节 |
|---|---|
| FR-201/202 | §2 |
| FR-203/204 | §3 |
| FR-205~207 | §4 |
| FR-208 | §5 |
| FR-209/210 | §6 |
| FR-211~213 | §1/Testing |

## 已知局限

- 训练数据全部 simulation-generated（明确标注；实船数据到达后可增量重训）；
- 生成器航段库来自 S20 的工况类型——泛化声明限于"同类内河机动航次"；
- K_S 沿用 S20 标定（in-sample 结构超参，对 C3/C4 对称，公平性可接受）；
- Ê_H 单点能量积分不含时序结构信息——若 val 指标差，备选目标（多 H 头/分位）属主 Agent 决策（§21 Escalate）。

## Design Amendment Log

- v1.1（2026-08-25，T-201 实施修订，主 Agent 裁决）：航次生成器解析预检由**未约束三次方理论功率**改为**送达功率口径**：P_delivered = min(Hull_K_R·v_eff³/η链/1000, Mot_P_nom=700) + hotel(100)。原因：原公式将逆水段（11 kn + 水流−1 m/s → 理论 1080 kW）全部拒绝，导致 200 航次中逆水段 0 段（覆盖率缺口）；而实际物理是速度 PI 饱和封顶（S20 段 9 已实证：800 kW 持续、零降额、Vdc 健康），饱和形态是训练数据需要覆盖的真实工况。同时增加约束：每航次逆水段最多 1 段。

- v1.2（2026-08-25 预置；**2026-08-25 晚主 Agent 正式激活**）：内联路线经系统性攻坚未根因定位（证据链见项目开发进展.md 存档），为避免无限试错消耗，启用兜底——**NN 前向从 MATLAB Function 块内联改为 `coder.extrinsic` 调用** `ecms2.nn_predict_ehat(t, P_load, v_ship)`（MATLAB 函数，内含 persistent 环形缓冲与权重惰性加载，t 用于仿真间状态重置）。(1) 控制周期 1 s → 每航次仅 ~5520 次调用，extrinsic 开销可忽略（T-103 拒绝 extrinsic 的理由是 100 Hz，不适用于 1 Hz）；(2) 块/测试一致性由构造保证（同调一个函数）；(3) 架构上更贴近 Phase 3 部署形态——NN 推理由 NPU/TFLite 执行、控制逻辑在 MCU，仿真中的 extrinsic 边界正好对应这个分工。损失分析：科学上零损失（C4/C3/C2 消融语义不变），工程上仅多块↔函数接线层。E5 红线不变。

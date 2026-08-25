# Phase 2 NN 预测型 A-ECMS — Tasks

Spec: `nn-predictive-ecms-phase2`
Status: implementation-complete
Created: 2026-08-25
Brainstorm: `./brainstorm.md` | Requirements: `./requirements.md`（v1）| Design: `./design.md`（v1）

## Implementation Plan

- [x] 1. 航次生成器 + 数据生成
  - ID: T-201
  - Requirement(s): FR-201, FR-202
  - Files/areas: `scripts/50_voyage_generator.m`、`scripts/51_generate_fleet_profiles.m`、`config/`（ECMS_FLEET_SEED）、`results/ecms_phase2/fleet/`
  - Validation: 航次定义 JSON 入库；解析预检零超功率；抽样 Simulink 验证零降额；种子复现同一航次集
  - Escalate if: parsim 不可用且串行预算 >2 h（降级 N≥80 需记录）

- [x] 2. 数据集构建 + 泄漏检查
  - ID: T-202
  - Requirement(s): FR-203, FR-204
  - Files/areas: `python/build_dataset.py`、`datasets/fleet_windows.npz`（不入库）、`results/ecms_phase2/dataset_manifest.json`
  - Validation: split 名单入库（S20/S01/S02/S07 不在 train）；归一化仅 fit train；窗口不跨航次断言

- [x] 3. MLP/LSTM 训练 + val 选模
  - ID: T-203
  - Requirement(s): FR-205, FR-206, FR-207
  - Files/areas: `python/train_predictor.py`、`results/ecms_phase2/training_log.csv`、`models/`（.keras 不入库，权重 JSON 入库）
  - Validation: §19 全字段记录；val 指标 + 高/低估分解；失败实验保留
  - Escalate if: LSTM 显著优于 MLP（Simulink 内联复杂度裁决归主 Agent）

- [x] 4. law=7 集成 + 单测
  - ID: T-204
  - Requirement(s): FR-208, FR-212
  - Files/areas: `scripts/52_integrate_law7.m`、`build_model_c.m`（05_EMS_Controller 第 7 变体）、`+ecms2/test_law7.m`
  - Validation: 块 vs Python 前向 100 随机输入 |Δ|<1e-5；回退行为正确；10b smoke 绿

- [x] 5. C4 矩阵 + Phase 2 Gate
  - ID: T-205
  - Requirement(s): FR-209, FR-210
  - Files/areas: `scripts/53_run_phase2_matrix.m`、`54_phase2_gate_judge.m`、`results/ecms_phase2/runs_phase2.csv`、`gate_result.json`
  - Validation: {C2,C3,C4}×{S20,S01,S02,S07} + 10 个 test 航次泛化行；gate 构造单测；C4 S20 数值与标定一致性抽查

- [x] 6. 复现 + Phase 2 报告
  - ID: T-206
  - Requirement(s): FR-211, FR-212, FR-213, AC-1~6
  - Files/areas: `reports/NN_phase2_report.md`（主 Agent 撰写）、repro_check
  - Validation: RQ4/5/6 答案明确；部署预算估算；失败实验清单；复现通过

## Verification Checklist

- [x] Requirements IDs are referenced by tasks.
- [x] Design testing strategy is represented by tasks.
- [x] Each task has a clear validation command or manual check.
- [x] Rollback/migration tasks are included when applicable.（law=7 为新增变体，不改既有 law；S20/Phase 1 结果不回改）

## Execution Notes

- 顺序即编号；T-201→T-202→T-203 链式依赖；T-204 依赖 T-203 的入选模型；T-205 依赖 T-204。
- **F6 工期门槛**：主 Agent 已估计总工期 ~5~6.5 h（详见会话报告），超 2 h → 不立即施工，待用户指令。
- 数据生成是最大的时间不确定项（parsim 可用性）。

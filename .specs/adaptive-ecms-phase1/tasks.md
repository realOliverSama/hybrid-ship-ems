# Phase 1 实时 ECMS 控制器链 — Tasks

Spec: `adaptive-ecms-phase1`
Status: tasks-approved（依据用户 2026-08-24 批量授权——brainstorm E4）
Created: 2026-08-24
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`（v1）
Design: `./design.md`（v1）

## Implementation Plan

- [ ] 1. 场景框架时间序列扩展 + S20 复合航次定义
  - ID: T-101
  - Requirement(s): FR-101, FR-102
  - Files/areas: `model/simulink_modelD/scripts/+model/build_model_c.m`（01_Scenario 时间序列路径）、`config/Model_Scenarios.m`（SCENARIOS(20)）+ canonical_data 镜像同步、`scripts/+ecms2/voyage_s20.m`
  - Validation: S01 Baseline 重跑 P_demand 与 Phase 0 剖面逐位一致（回归）；S20 可构建可仿真
  - Escalate if: 时间序列路径无法在不破坏既有场景的前提下加入

- [ ] 2. S20 Baseline 剖面 + DP 上限
  - ID: T-102
  - Requirement(s): FR-103
  - Files/areas: 复用 `30_export_load_profiles.m` / `+ecms/`；输出 `results/ecms_phase1/profiles/S20_composite_voyage.mat` + DP（tier2，连续口径，P_min ∈ {0,400}）
  - Validation: 无降额事件（有则 Escalate）；能量闭合 <0.1%；DP 校验全过；报告 S20 的 DP 节油上限
  - Escalate if: S20 出现降额事件（功率需求超装备能力，需重审航段设计）

- [ ] 3. Fixed ECMS 变体（law=4）+ 单元测试
  - ID: T-103
  - Requirement(s): FR-104, FR-108
  - Files/areas: `build_model_c.m`（05_EMS_Controller 新增变体）、`scripts/+ecms2/ecms_law.m`、`+ecms2/test_ecms_law.m`
  - Validation: 五项单测全过（design §3.5）；Simulink 块与离线实现一致性锁定；10b smoke 绿
  - Escalate if: coder.extrinsic 性能不可接受且内联展开与离线实现无法一致

- [ ] 4. λ 标定
  - ID: T-104
  - Requirement(s): FR-105
  - Files/areas: `scripts/41_calibrate_lambda.m`、`results/ecms_phase1/lambda_calibration.json`
  - Validation: S01 上 SOC_end ∈ 0.60±0.005 收敛；迭代 ≤40 次；不收敛则 Escalate（不得放宽终端带）

- [ ] 5. SOC-Adaptive ECMS（law=5）+ K_soc 标定
  - ID: T-105
  - Requirement(s): FR-106
  - Files/areas: 同 T-103 变体机制 + `41_calibrate_lambda.m` 扩展
  - Validation: 单测通过；K_soc 标定收敛；10b smoke 绿

- [ ] 6. Oracle Predictive A-ECMS（law=6）
  - ID: T-106
  - Requirement(s): FR-107
  - Files/areas: `+ecms2/oracle_future_energy.m`（E_future_H 预计算）、law=6 变体
  - Validation: 单测通过；E_future 信号与剖面积分交叉验证（随机 10 时刻手算比对 <0.1%）

- [ ] 7. 实验矩阵
  - ID: T-107
  - Requirement(s): FR-109
  - Files/areas: `scripts/42_run_phase1_matrix.m`；输出 `results/ecms_phase1/runs_phase1.csv`
  - Validation: 主矩阵 {C0,C1,C2,C3}×{S20,S01,S02,S07} + 回归 {C0,C1,C2}×8 场景无缺项；C1 为负节油如实记录不掩盖
  - Escalate if: 仿真总时长超 2 h（NFR-102）

- [ ] 8. Phase 1 Gate 判定
  - ID: T-108
  - Requirement(s): FR-110
  - Files/areas: `scripts/43_phase1_gate_judge.m`；输出 `results/ecms_phase1/gate_result.json`
  - Validation: 字段完整（capture_rate/oracle_increment_pp/recommendation）；判定函数构造用例单测

- [ ] 9. 复现性验证
  - ID: T-109
  - Requirement(s): FR-114
  - Files/areas: 42 重跑对比（环境变量输出目录切换，沿用 T-015 模式）
  - Validation: 双跑逐位一致（除 wall_time）

- [ ] 10. Phase 1 报告 + 验收
  - ID: T-110
  - Requirement(s): FR-111, FR-112, FR-113, AC-1~AC-6
  - Files/areas: `model/simulink_modelD/reports/ECMS_phase1_ecms_vs_dp.md`（主 Agent 撰写）
  - Validation: RQ1~3 答案明确；图表齐全；AC 全核；git status 无冻结目录改动

## Verification Checklist

- [x] Requirements IDs are referenced by tasks.
- [x] Design testing strategy is represented by tasks.
- [x] Each task has a clear validation command or manual check.
- [x] Rollback/migration tasks are included when applicable.（副本制隔离；变体新增不改既有 law 行为，回归验证兜底）

## Execution Notes

- 顺序即编号；T-101→T-102 强依赖；T-103→T-106 链式依赖；T-107 依赖前全部。
- 执行委派 worker（用户已授权）；主 Agent 验收点：T-102 的 S20 剖面与 DP 数值、T-107 矩阵结果、T-108 gate、T-110 报告。
- λ 标定迭代涉及多次仿真，worker 须控制迭代上限并打印轨迹。

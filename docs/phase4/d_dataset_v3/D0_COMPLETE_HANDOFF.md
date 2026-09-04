# D0 COMPLETE HANDOFF — Phase 4-D 数据集 v3（D0 → C 包 / 后续 session）

> 创建：2026-09-05（D0 施工完成，T-001~T-017 全勾）。
> **STOP 纪律**：spec 保持 `implementation-in-progress`（D0 complete / T-432 BLOCKED）；
> **T-018/019/020（T-432 formal fleet 与 T-435-formal）保持 BLOCKED**——blocked_by:
> Phase 4-C Mission-v3 producer-interface freeze + C/D Interface Gate overall=true（正式版）
> + Unblock Checklist 八项全绿。tasks.md 批准与 D0 完成均不构成解锁。

## 1. D0 交付物清单（全部在本分支 `feat/phase4-d-dataset-v3`）

| 类别 | 交付物 |
|---|---|
| 机器契约 | `model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_signal_registry.json`（90 信号 × 八 role，self_hash=9ab11dea…）；`config/dataset_v3_producer_schema.json`（11 能力族 + provenance 六字段 + formal 航次合格判据槽位） |
| 契约文档 | `docs/phase4/d_dataset_v3/`：observation / future_information / c_producer_interface / dataset_v3_schema 四件（解释层）+ t432_activation_record.md / t432_unblock_checklist.json 模板 |
| Python 包 | `model/simulink_modelE_(Phase4_Physics)/dataset_v3/`（聚合/窗口/target/split/L1-L3/OOD/manifest/E2E/契约校验 + 负向套件；164 pytest） |
| MATLAB 件 | `scripts/70/71/72` 薄入口 + `+dataset_v3/export_pilot.m` + `+gate_e/{ccs_eligibility_mask, reset_bench_nominal}.m` + 10b/40/42/43/44 接线 |
| hygiene | 10b CCS 稳态 eligibility mask（DGN-14，修复前基线 FAIL 证据 `/tmp` 已失、结论登记 design.md D-3/D-4；修复后 `reports/modelD_smoke_summary.md` 含 mask 证据列）+ case 隔离不变量（DGN-15） |
| pilot 证据 | `results/dataset_v3/pilot/`（pilot_export_manifest / pilot_gate_result / manifest / split_assignment / norm）；`results/dataset_v3/cd_interface_gate_result.json`（NON-FORMAL 骨架自检） |
| 偏差登记 | `.specs/phase4-d-dataset-v3/design.md` 偏差区 D-1~D-12 |

## 2. C 包开工输入（FR-495/DGN-03）

1. **必读**：`docs/phase4/d_dataset_v3/c_producer_interface_contract.md` +
   `config/dataset_v3_producer_schema.json`（机器权威）；
2. C Mission v3 必须输出十一项能力（truth_stream / s_plan / planned_future_trajectory /
   static_constraints / input_k_snapshots / input_f_snapshots / forecast_provenance /
   oracle_counterpart / voyage_metadata / generation_provenance / ood_config_surface）；
3. 硬性因果红线：s_plan 由计划产生（禁 realized future 冒充）；INPUT_F 满足
   F_{t₀}=g(ℐ_{≤t₀})（provenance class=CAUSAL_GENERATOR，source_cutoff≤issue_time）；
4. K/F 快照布局 = (issue_time_s (M×1), lead_s (M×30), 各列 (M×30) issue-major)
   ——与 `dataset_v3/windowing.build_windows` 消费接口逐字段一致（pilot 70 号已实证该布局）；
5. formal 航次合格判据数值（min_duration_s / min_canonical_windows）由 activation record
   预注册，schema/契约不预填。

## 3. T-432 解锁链（FR-496/498，全部机器化）

```text
C 完成 → C/D Gate 正式版 overall=true（十项，72 号 + cd_interface_gate.py）
→ t432_unblock_checklist.json 八项全绿 → activation record 填值并经用户批准
→ T-432 Stage A（≈100 qualification，73 号 activation 时施工）→ Stage B formal fleet
→ T-435-formal 审计 → Dataset-v3 Freeze（此时 spec 方可 implementation-complete）
```

## 4. 已知边界与注意（细节见 design.md 偏差区 D-1~D-15）

- registry 已删 `below_min_DG1`（运行时 logsout 无该命名元素，D-7）；
- `reset_bench_nominal` 禁止在 base 重跑 config 脚本（循环变量覆写事故，D-5）；
  其 PMS_SOURCE_MODE 恢复值=0 与 config 默认 1 的语义差异见 D-14（law=0 下功能等价已逐位实证；C 包 formal harness 必须显式设定 src，不得依赖 reset）；
- 43 号注入场景 reset 在 prep_fn 之前（D-6）；artifact 格式 .mat v7（D-2）；
- pilot split_seed="pilot-split-seed-v1"、全部 pilot 产物标 PILOT_ONLY，
  **pilot 统计不得晋升 Dataset v3 科学基线**（FR-489）；
- L3 formal 调用必须传 train_stats 且 `require_train_stats=True`（D-10）；
- C/D Gate 骨架的十项检查粒度与 ⑤⑩ 收紧为**正式执行前必改**（D-15）；
  正式 activation JSON 文件名钉死 `docs/phase4/d_dataset_v3/t432_activation_record.json`（72 号消费）。

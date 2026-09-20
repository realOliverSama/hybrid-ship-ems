# Feasibility Contract Amendment — Tasks（DRAFT，待 Amendment Tasks Gate）

状态：**DRAFT — 本文件本身即 Amendment Tasks Gate 评审对象**。
上游：Design Gate 裁决（APPROVED WITH A1–A7，已全部落实）+ 三份修正案文档。
执行约束：实现类任务在 **Amendment Tasks Gate 通过后**才可动工；
正式/结果性工作（T-A8+）另有独立授权门。

## 任务 DAG

```
T-A1 checker_v2 实现 ──┬──> T-A2 资格化套件 Q1–Q26 ──┐
T-A3 轨迹写入器 v1 ────┼──> T-A4 轨迹资格化 Q17–Q18 ──┼──> T-A7 epoch-2 预执行冻结 ──> (人工 Epoch-2 Gate)
T-A5 runner 集成 ──────┘                              │                              ├──> T-A8 epoch-2 正式执行 [BLOCKED: 人工授权]
T-A6 FEC 稳健性预注册 ────────────────────────────────┘                              └──> T-A9 epoch-2 验收与完整性核验
T-A0 文档/审计基线（本文件+gate 记录） ──（已完成于本提交）
```

## 任务清单

| ID | 任务 | 依赖 | 产出 | 约束 |
|---|---|---|---|---|
| T-A1 | `feasibility_contract_v2` 检查器实现（D-8：状态感知分机组判定、FAIL CLOSED、诊断通道分离；旧检查器保留不删） | Tasks Gate | scripts/+ppe/feasibility_contract_v2.m + 合成单测 | 不接入正式评估路径（Gate 后任务才切换） |
| T-A2 | 资格化套件 Q1–Q26 全绿（design D-9/D-9b 扩展矩阵） | T-A1 | qualification/feasibility_v2_qual.json | 合成/玩具 fixture；0 正式语料消费 |
| T-A3 | 1 Hz 轨迹写入器（D-7：25 通道、原子写、sidecar hash） | Tasks Gate | scripts/+ppe/write_traj_v1.m + 单测 | 仅日志提取，不改 Model-E 物理 |
| T-A4 | 轨迹资格化（schema/完整性/hash/原子/resume 注入） | T-A3 | qualification/traj_v1_qual.json | — |
| T-A5 | runner 集成：pack 调用轨迹写入器 + `trajectory_sha256` + 审计工件（FR-C5/C8） | T-A1,T-A3 | run_formal_voyage/pack 增量 + 单测 | 臂原子策略不变；不改数值 |
| T-A6 | FEC 稳健性预注册实现（D-12：0.8×/1.2×/break-even 合成计算） | Tasks Gate | qualification/fec_robustness_qual.json | 合成数据；禁止正式数据 |
| T-A7 | epoch-2 预执行冻结：`feas.genset.state_aware_v2`/`feas.shortfall.unserved_v2`/`feas.terminal.band_v2`/`traj.v1` 冻结 JSON（自哈希）+ Pre-Execution Gate 重审（判据清单按 A1–A7 更新） | T-A2,T-A4,T-A5,T-A6 | freeze/epoch2_pre_execution_freeze.json + GATE | overall=true 才可进入 T-A8 |
| T-A8 | epoch-2 正式执行（48 航次 × 4 臂，26 worker 模板，轨迹强制） | **T-A7 + 人工 Epoch-2 Gate 授权** | formal_epoch=2 记录 + 1 Hz 轨迹 + 审计工件 | BLOCKED 任务（授权前置） |
| T-A9 | epoch-2 验收与完整性核验（192 终端记录 + 192 轨迹 + 192 审计 + parity 48/48） | T-A8 | ACCEPTANCE_v2 + 同步/提交 | STOP before T-011B |

## Gate 与授权链

```
Amendment Design Gate（本裁决）APPROVED
  → Amendment Tasks Gate（本文件，待审）
    → 非正式实现与资格化（T-A1..T-A7）
      → epoch-2 Pre-Execution Gate（T-A7 产出，人工审）
        → 人工授权 formal_epoch=2
          → T-A8 执行 → T-A9 验收 → （T-011B/T-012 仍需独立授权）
```

## 禁止项（全程有效）

epoch-1 证据不可变；不重跑 epoch-1 航次；不重分类；密封 IID/OOD 零接触；
不改 C*/预测器/forecast 语义/语料/bootstrap；资格化期间禁止观察任何 epoch-2 正式结局。

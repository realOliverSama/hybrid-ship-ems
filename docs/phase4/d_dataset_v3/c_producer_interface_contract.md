# D→C Producer Interface Contract（c_producer_interface_contract.md）

> Spec: `.specs/phase4-d-dataset-v3`（T-004；FR-495~499；design DGN-03/16/17）
> **机器权威**：`config/dataset_v3_producer_schema.json`（`schema=dataset_v3_producer_schema_v1`，
> self_hash 见文件）。本文件是解释层，供 **Phase 4-C Mission v3 开工作为输入**；
> 字段冲突时以 schema 为准。

## 1. 接口定位

Phase 4-D 先行冻结**消费侧**契约（本文件 + schema）；Phase 4-C Mission v3 实现**生产侧**。
T-432 formal fleet 解锁链（FR-460/496/498）：

```text
C Mission-v3 完成 → C/D Interface Gate overall=true（十项，DGN-16）
→ T-432 Unblock Checklist 八项全绿（FR-498）→ T-432 BLOCKED→READY
→ Stage A（≈100 qualification）→ Stage B（activation record 预注册 formal fleet）
```

tasks.md 批准不构成解锁；C 包 PR 合并不自动启动 formal fleet。

## 2. C 必须提供的十一项能力（FR-495，schema `capability_families`）

| # | 能力族 | 内容 | causality 要求 |
|---|---|---|---|
| 1 | `truth_stream` | voyage-level realization-time truth（native 0.01 s；time_s + registry INPUT_H1/H2/TARGET 全列 + scenario_id/resistance_multiplier） | realized truth，单时间轴 |
| 2 | `s_plan` | s_plan_m / v_plan_mps（planned along-track） | x(t₀+k\|t₀)；由计划前向积分，**禁用 realized future 冒充** |
| 3 | `planned_future_trajectory` | planned/reference 未来轨迹（K 类载体） | 与 K/F 网格同源（30×10 s） |
| 4 | `static_constraints` | h_chart_m、speed_limit_zones、bridge_zones（空集合法，须如实记录） | 静态先验 |
| 5 | `input_k_snapshots` | (issue_time_s, lead_s) 双索引 K 列快照 | 禁从最终 future series 反推 vintage |
| 6 | `input_f_snapshots` | (issue_time_s, lead_s) 双索引预报快照 | F_{t₀}=g(ℐ_{≤t₀})；禁未来真值后处理 |
| 7 | `forecast_provenance` | 六字段（generator id/version/hash、issue_time_s、source_cutoff_time_s、class） | **不变量 source_cutoff ≤ issue_time**；formal 仅 CAUSAL_GENERATOR |
| 8 | `oracle_counterpart` | u/wl 的 realization 真值对应物 | 与 INPUT_F 物理分离双存（oracle_only） |
| 9 | `voyage_metadata` | voyage_id / voyage_family_id / route_template_id / mission_template_id / split_group_id / scenario_stratum | split 隔离与 strata 代表性依据 |
| 10 | `generation_provenance` | generation_seed、model_baseline_ref（物理 v1.1+控制 v1.0；A2 historical v1.0 不得混同）、producer_script_sha、fixture_kind | formal 禁 `PILOT_ONLY` |
| 11 | `ood_config_surface` | ood_axis / ood_def_version / physics_domain_ref | 越域标 PHYSICS_EXTRAPOLATION/STRESS_SET（FR-486） |

每字段五元组（name/type/shape_semantics/required/causality_rule）见 schema；
schema 校验：`python -m dataset_v3.validate_contracts --producer-schema`。

## 3. Formal 航次合格判据（FR-495 第 7 项）

600 s 仅为几何最小 history+future 跨度（300+300），**非充分条件**。formal 训练航次必须
满足**预注册**的可度量判据（以最小航次时长和/或每航次最小 canonical 窗口数表达）。
判据数值属 design/T-432 activation record 预注册项——本契约与 schema **不预填数值**
（activation-time 值纪律）；C/D Gate 项⑦在 activation 后核对实际值。

## 4. C/D Interface Gate 十项（FR-496/DGN-16，C 就绪后正式执行）

① C 输出字段满足 frozen producer schema；② s_plan[k|t₀] 存在且 issue-time causal；
③ K/F 为 (issue_time, lead_time) snapshot；④ INPUT_F generator 不消费 future truth；
⑤ INPUT_K/F 与 ORACLE 角色物理分离；⑥ voyage/family/template/group metadata 完整；
⑦ 航次长度满足预注册 formal-voyage 合格判据；⑧ OOD 配置面存在；⑨ dataset schema/
manifest validator PASS；⑩ Model-E 冻结物理/控制基线未被非授权修改。
`overall=true` 是 T-432 解锁**必要条件**之一。实现：
`dataset_v3/cd_interface_gate.py` + `scripts/72_run_cd_interface_gate.m`（D0 交付骨架，
对 pilot fixture 的自检结果标 NON-FORMAL）。

## 5. Gate reopening 纪律（FR-499）

- C 对已批准契约的 **conforming implementation**：不重走四门禁；
- K/F 认识论语义、s_plan causality、target/split/OOD 契约的**实质漂移**：重开最早受影响的门；
- 非语义实现变化（如文件格式）：design/task amendment（偏差登记）。

## 6. T-432 Unblock Checklist（FR-498，八项机器化）

① C Mission-v3 完成；② C/D Interface Gate overall=true；③ producer schema version/hash
accepted；④ D0 pilot pipeline qualification PASS；⑤ formal fleet generation plan 预注册；
⑥ OOD 定义预注册；⑦ Model-E 物理基线完整性 PASS；⑧ 控制基线完整性 PASS。
全部满足才允许 T-432=READY。模板：`docs/phase4/d_dataset_v3/t432_unblock_checklist.json`
+ `t432_activation_record.md`（activation 时填值）。

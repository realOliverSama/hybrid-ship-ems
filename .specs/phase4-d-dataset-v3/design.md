# Phase 4-D 数据集 v3 Design

Spec: `phase4-d-dataset-v3`  
Status: design-approved  
Created: 2026-09-04  
Approved: 2026-09-04（CONDITIONAL PASS + R1~R8 修正闭合）  
Brainstorm: `./brainstorm.md`（D-D1~D-D9，2026-09-04 批准）  
Requirements: `./requirements.md`（requirements-approved，2026-09-04，R1~R5 闭合）

## Summary

本设计把 requirements 的 FR-458~500 落成可施工方案：**机器可读契约为唯一执行事实源**（signal/role registry + producer schema，JSON），**两段式管线**（MATLAB producer `70_export_dataset_v3_pilot.m` 导出 voyage-level 原生速率数据 + K/F causal fixtures → Python `dataset_v3/` 包完成聚合、窗口化、target/E300 构建、split、三级泄漏审计、OOD 装载与 manifest 冻结），并交付 validation hygiene 修复（10b 稳态 mask、case 隔离不变量）、D→C producer 接口契约、C/D Interface Gate 与 T-432 两阶段激活机制。

设计决策编号 **DGN-01~DGN-17**（与 brainstorm D-Dn、B 包 DD-n 区分）。

## Goals and Non-Goals

### Goals

- G1（FR-461~468）：观测/未来信息契约落地为机器可读 registry + 四份 Markdown 解释层文档；
- G2（FR-469~480）：窗口化/target/E300/view 管线在 pilot 数据上端到端正确；
- G3（FR-481~489）：split/leakage/OOD 审计实现并通过 pilot qualification（含负向检出）；
- G4（FR-490~491）：10b/E10 validation hygiene 修复，40 号 Gate 复验无回归；
- G5（FR-495~500）：D→C producer 契约、C/D Interface Gate、T-432 激活机制就位。

### Non-Goals

- 不实现 C Mission v3（C 包）；不训练任何模型（E 包）；不改 Model E 物理/控制/基线（NFR-420）；
- 不删除/移动 legacy 资产（FR-492/493）；不定义 predictor runtime period（FR-474）；
- 不在 D0 产出任何科学统计结论（FR-489）。

## Architecture

```text
[Model E 冻结基线 v1.1 + 控制 v1.0]   （NFR-420，零改动）
        │  sim（fixed-step 0.01 s，logsout 原生速率）
        ▼
MATLAB producer: scripts/70_export_dataset_v3_pilot.m
  - 按航次导出 realization-time stream（truth + H1/H2 + ORACLE + METADATA）
  - K/F issue-time snapshots：pilot 期由 causal fixtures 生成（DGN-13，PILOT_ONLY）
  - 消费 config/dataset_v3_signal_registry.json（角色/聚合规则不在脚本内硬编码）
        │  每航次一个 .mat（voyage artifact）+ 每航次 manifest 条目
        ▼
Python 包: model/simulink_modelE_(Phase4_Physics)/dataset_v3/
  build_dataset_v3.py     —— 编排（role 白名单 view、窗口、target、split、审计、manifest）
  aggregate.py            —— channel-aware causal aggregation（DGN-05）
  windowing.py            —— anchor 网格/区间 bin/窗口合法性（DGN-07）
  targets.py              —— native-rate 能量差 target + E300（DGN-08）
  split.py                —— deterministic hash group-aware split（DGN-09）
  audit_leakage.py        —— L1/L2/L3（DGN-10）
  audit_ood.py            —— OOD 装载/域检查（DGN-11）
  manifest.py             —— manifest 构建+hash（DGN-12）
  validate_contracts.py   —— registry/schema/artifact 校验器
  cd_interface_gate.py    —— C/D Interface Gate（DGN-16，T-432 解锁前不执行正式版）
  tests/                  —— 单元 + 负向 + 确定性测试
        ▼
产物: datasets_v3/pilot/（npz/mat 大数据，本地不入库）
      results/dataset_v3/（manifest JSON、pilot qualification 报告，精选入库）
文档: docs/phase4/d_dataset_v3/（四契约文档，解释层）
```

**新资产全部置于 70+ 编号段与 `dataset_v3/`、`datasets_v3/` 新目录**；50~66 号 legacy 链与 `python/`、`datasets/`、`legacy_reference/` 冻结不动（FR-493）。

## 详细设计

### DGN-01 产物布局与命名冻结（FR-465/488/492/493）

| 产物 | 路径 | 入库策略 |
|---|---|---|
| signal/role registry | `model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_signal_registry.json` | 入库（机器事实源） |
| C-producer interface schema | `model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_producer_schema.json` | 入库 |
| MATLAB pilot producer | `scripts/70_export_dataset_v3_pilot.m` | 入库 |
| pilot 管线 Gate | `scripts/71_run_dataset_v3_pilot_gate.m`（薄入口，调用 Python 管线+校验） | 入库 |
| C/D Interface Gate | `scripts/72_run_cd_interface_gate.m` + `dataset_v3/cd_interface_gate.py` | 入库（T-432 解锁时执行） |
| T-432 formal 生成 | `scripts/73_generate_fleet_v3.m`（activation 时施工，本设计只冻结接口） | 入库 |
| Python 包 | `model/simulink_modelE_(Phase4_Physics)/dataset_v3/` | 入库 |
| pilot 大数据 | `datasets_v3/pilot/`（**本地不入库**，.gitignore） | 不入库 |
| manifest/报告 | `results/dataset_v3/`（精选入库） | 入库 |
| 契约文档 | `docs/phase4/d_dataset_v3/`（4 份 md） | 入库 |

选择 JSON 而非 YAML/MATLAB struct：语言中立（MATLAB `jsondecode` / Python `json` 双消费）、diff 友好、可 schema 校验。MATLAB 侧只读 registry 的 role/aggregation 字段，不复制清单（FR-465）。

### DGN-02 signal/role registry schema（FR-461/464/465/470）

`dataset_v3_signal_registry.json`：顶层 `{version, hash_inputs, signals: [...]}`；每信号条目冻结字段（D-D9 §2）：

```json
{
  "name": "P_load_total_kW",
  "unit": "kW",
  "role": "INPUT_H1",
  "source_signal": "logsout:P_load_total_kW",
  "temporal_domain": "history",
  "aggregation": "interval_mean",
  "issue_time_semantics": "none",
  "predictor_access": "default"
}
```

- `role` ∈ 八类枚举（INPUT_H1/INPUT_H2/INPUT_K/INPUT_F/TARGET/ORACLE/METADATA/FORBIDDEN）；
- `temporal_domain` ∈ {history, future, both, none}；
- `aggregation` ∈ DGN-05 枚举；
- `issue_time_semantics` ∈ {none, issue_snapshot}（K/F 必须为 issue_snapshot）；
- `predictor_access` ∈ {default, extended, oracle_only, forbidden, metadata_only}；
- H3 分类法映射（R1）：PMS audit/EMS 内部 → `FORBIDDEN`；`scenario_id` → `METADATA`；`resistance_multiplier` → `METADATA`（predictor_access=metadata_only）；这些字段物理保留供 provenance/审计，不进任何 model view。
- registry 自带内容 hash（manifest 记录）；validator 校验枚举合法性、H1/H2 变量名与 FR-461 清单一致性、K/F 条目 issue_time_semantics 约束。

### DGN-03 C-producer interface schema（FR-495）

`dataset_v3_producer_schema.json`：声明 C Mission v3 必须输出的字段清单与类型（voyage truth stream / s_plan / INPUT_K / INPUT_F / ORACLE / metadata 五族 metadata 字段 / OOD 配置面），每字段含 {name, type, shape_semantics, required, causality_rule}。`cd_interface_gate.py` 消费此 schema 对 C 产物做机器校验（FR-496）。C 包开工输入 = schema + `c_producer_interface_contract.md`（解释层）。

### DGN-04 数据文件格式（FR-466/467/473）

**每航次 voyage artifact（.mat v7.3 / HDF5 语义）**：

1. `stream`（realization-time 表）：`time_s`（native 0.01 s）× 全部 truth/H1/H2/ORACLE/METADATA 列；
2. `kf_snapshots`（issue-time 表，long-table）：`issue_time_s | lead_s | <INPUT_K 列…> | <INPUT_F 列…>`，lead ∈ {10,20,…,300}；
3. `meta`：voyage_id、scenario/template 标识、generation_seed、family/group 字段位（D-D7 §1 预留，pilot 期填 voyage 自身）、模型/基线版本、provenance hash。

Python 侧内部统一转 parquet/numpy；最终窗口化产物 npz：`X_H [N,300,F_H view]`、`X_KF [N,30,F_KF]`、`Y [N,30]`、`E300 [N]`、`metadata [N]`（voyage_id/anchor_t0_s/window_index_in_voyage）、split 标记。truth 单索引、forecast 双索引的 schema 级区分在 voyage artifact 层即强制（FR-466）。

### DGN-05 channel-aware causal aggregation 冻结表（FR-470，C2）

bin (a,b] 含 native 样本 {t : a < t ≤ b}。逐列规则（写进 registry `aggregation` 字段，不在代码散置）：

| 规则枚举 | 语义 | 适用列 |
|---|---|---|
| `interval_mean` | bin 内算术均值（功率/速度/加速度/RPM/水深/流速/电压/电流） | P_load_total_kW, v_ship_mps, acceleration_mps2, n_prop_rpm, P_hotel_kW, v_current_mps, water_depth_m, P_prop_demand_kW, P_gen_actual_kW, P_bat_actual_kW, V_dc_V, V_bat_V, I_bat_A |
| `interval_endpoint` | bin 右端点值（状态积累量/约束快照） | SOC, P_bat_dis_avail_kW, P_bat_chg_avail_kW, v_ref_mps, manual_power_cmd_pct |
| `last_mode` | bin 内最后采样值（离散模式/状态） | ctrl_mode, state_DG1, state_DG2 |
| `interval_any` | bin 内任一为真（事件/flag，仅 METADATA/ORACLE 列） | domain_warning 等 |

能量语义自检：功率列 interval_mean 满足 Σ_k P̄_k·Δt ≈ ∫P dt（unit test 容差 1e-9 相对）。

### DGN-06 ctrl_mode 非激活通道表征（R3 落案）

冻结：**zero-fill + 模式指示**。ctrl_mode=AUTO(0) 时 `manual_power_cmd_pct` 聚合列填 0.0；ctrl_mode=MANUAL(1) 时 `v_ref_mps` 聚合列填 0.0；`ctrl_mode`（last_mode）始终存在使两通道语义可区分。确定性、无 NaN、无双写。ORACLE 层仍保留原始双通道真值供审计。

### DGN-07 窗口化实现（FR-469/471/472/473）

- anchor 网格：t₀ ∈ {t_start+300, t_start+310, …, ≤ t_end−300}（stride 10 s，bin 语义右闭）；
- 合法性：hist (t₀−300,t₀] 与 fut (t₀,t₀+300] 全在同一 voyage_id 内，否则丢弃（不 padding）；
- 每样本 METADATA：voyage_id/anchor_t0_s/window_index_in_voyage（voyage 内 anchor 序号）；
- X_KF 取 kf_snapshots 中 issue_time == t₀ 的 30 行（严格 vintage 对齐；缺失即该窗口判 invalid 并计数记录）。

### DGN-08 target 构建（FR-476/477）

- 对每 voyage 的 native-rate `P_load_total_kW` 先做累计能量 E(t)=∫₀ᵗ P dt（梯形积分，native 0.01 s）；
- bin 能量 E_k = E(t₀+10k)−E(t₀+10(k−1))；y_k = E_k·3600/10（kW）；E300 = ΣE_k/3600（kWh）；
- 不变量单测：Σ_{k=1..30} E_k ≡ E(t₀+300)−E(t₀)（机器精度）；与 interval_mean 聚合路径交叉验证（相对差 <1e-9）；
- 禁止从 1 Hz 历史流反算 target（代码层隔离：targets.py 只消费 native stream）。

### DGN-09 split 机制（FR-481/482）

- assignment key：pilot 期 = voyage_id；formal 期 = split_group_id（C 生成机制确定后冻结 grouping policy，requirements 不冻结）；
- 算法（deterministic、构建顺序无关）：`h = SHA256(split_seed ‖ group_key ‖ route_template_id)` → u = int(h[:8],16)/2⁶⁴；按预声明 strata（scenario family）分层，层内按 u 排序切 70/15/15；输出 `split_assignment_v3.json`（group→split 显式映射）+ artifact SHA256 入 manifest；
- split_seed 为 registry/manifest 冻结字段；任何 split 变更须 spec amendment（禁止按模型误差调整）。

### DGN-10 泄漏审计实现（FR-484；R3 修正）

- **L1（Hard FAIL）**：voyage_id/group_key 跨 split 交集非空；raw voyage artifact hash 重复；窗口级 exact duplicate。**exact-window content hash 覆盖范围（冻结）**：完整 canonical H1/H2 history payload + INPUT_K + INPUT_F + TARGET Y（不只 X_H+Y）；metadata identity 字段不参与 content hash。任一触发 → gate FAIL；
- **L2（provenance-gated，禁自动剔除）**：numerical near-exact detector（两 voyage 的 1 Hz time-mean P_load 序列 max 绝对差 < ε 且长度一致；**ε=1e-6 kW 仅定位为 numerical near-identical candidate 阈值，不得解释为物理相似阈值**）只产生 `L2_candidate`；必须结合 generation provenance / template / seed / artifact lineage 分类为 `CONFIRMED_DUPLICATE` / `LEGITIMATE_NEAR_REPLICATE` / `UNRESOLVED`——只有 provenance 支持时才判 confirmed duplicate。**Formal Stage B 不允许静默删除 confirmed duplicate 后继续**：confirmed duplicate 使对应 preregistered slot 失效，须按预声明 retry/disposition policy 处理；最终 preregistered allocation 不满足 ⇒ Gate FAIL；
- **L3（SBD Suspect Flag）**：表征 = voyage 的 1 Hz time-mean P_load 序列（z-score 用 train 统计）；距离 = SBD（v2 同法）；参照分布 = train-vs-train 最近邻 SBD；阈值 policy **冻结为** T_SBD = Q0.01(train-train NN)（policy 先冻结、数值由 formal train 在 T-435 时计算）；跨 split 对 SBD < T_SBD → 标 `suspected_near_neighbor`，pair/distance/provenance/disposition 入 manifest；**similarity ≠ leakage**：不自动删数据、不单独判泄漏、不自动 Gate FAIL。

### DGN-11 OOD 预注册（FR-485/486）

本设计冻结 OOD 定义框架与数值范围（activation record 确认 counts/seeds；变更须 spec amendment）：

| 轴 | 训练支持（待 C 确认上界） | OOD 域（预注册） | 物理域检查 |
|---|---|---|---|
| A stronger current | \|U\| ≤ 2.0 m/s | 2.0 < \|U\| ≤ 2.8 m/s | 11 kn 逆水叠加后推进功率 ≤ 700 kW 推进容量（C/D Gate 验证，越界→STRESS_SET） |
| B lower water level | H/T ≥ 2.0 | 1.3 ≤ H/T < 2.0 | H/T ≥ 1.2 为 Model E 已验证浅水下界（clamp 域），留 0.1 margin |
| C compositional | 各因子单独出现 | 组合 {桥区×强逆流, 浅水×港区, 应急冲刺×浅水} 训练中不出现 | 各单因子在训练域内 |

- OOD counts/seeds：T-432 activation record 冻结（建议结构：OOD 航次 ≈ formal 规模 10%，三轴约均分，activation 时定案）；
- 越物理域场景标 `PHYSICS_EXTRAPOLATION/STRESS_SET`，单独报告不混 OOD 指标（FR-486）；
- OOD 装载器与域检查在 pilot 期用合成标记数据验证机制正确性（FR-489）。

### DGN-12 manifest schema（FR-488）

`results/dataset_v3/dataset_v3_manifest.json` 字段（冻结）：requirements FR-488 十五项 + `dataset_anchor_stride_s=10`、`predictor_runtime_period_s="UNDEFINED_IN_D"`、window geometry 五元组、registry hash、producer schema hash、aggregation 表版本、生成脚本路径+SHA、MATLAB/Python 版本、构建时间。所有 hash 字段为 SHA256。

### DGN-13 pilot causal fixtures（FR-500）

现有 S/M 工况无 Mission v3，pilot 期 K/F 由 producer 内 causal stub 生成：

- **K 类**：`v_plan/s_plan` = 该 scenario 的 v_ref 计划剖面前向延伸（scenario 定义本身是先验计划，因果合法）；`h_chart` = 场景水深设定（海图已知量，K 类本就精确）；桥区/限速 = 静态空集（pilot 场景无桥区，fixture 语义如实记录）；
- **F 类**：`U_fcst(t₀+k|t₀) = U_actual(t₀)`（persistence forecast，仅消费 t₀ 及以前信息，满足 F_{t₀}=g(ℐ_{≤t₀})）；`WL_fcst` 同理 persistence；
- 全部 fixture 行打 `fixture_kind="PILOT_ONLY"` 标记；validator 拒绝含该标记的 voyage 进入任何 formal 产物；
- **严禁**用未来真值后处理构造 F（代码审查点 + 负向测试：故意注入 future-truth 构造的 F 必须被检出）。

### DGN-14 10b CCS 稳态 eligibility mask（FR-490，hygiene）

冻结规则（预声明，不看误差结果）：

- eligible(t) = (t > 60 s) ∧（v_ref 在 [t−30 s, t] 内无阶跃变化）∧（无注入事件处于激活窗口）；
- CCS 稳态带判据（阈值不变）只在 eligible 样本上评估；
- 记录：transient_exemption_windows（起止）、transient_max_deviation、steady_window、steady_max_deviation、steady PASS/FAIL；
- 实现位置：`10b_smoke_modelE.m` 内判据段（不改阈值、不改场景、不改模型）。

### DGN-15 case 隔离不变量（FR-491，hygiene）

- 新增 `scripts/+gate_e/reset_bench_nominal.m`：显式恢复全部 bench 注入通道 nominal（assignin 机制，与 run_scenario_sim 同通道）；
- 40/42/43/44 及 E10 所属链每个 case 运行前调用（不变量：case 拥有完整输入初始化，不依赖上一 case 清理）；
- 修复 E10 携带 E09 注入后：重跑 E10 + 40 号 Gate（overall=true 为回归证据）。

### DGN-16 C/D Interface Gate 实现（FR-496）

`72_run_cd_interface_gate.m`（MATLAB 侧基线完整性检查 ⑩）+ `cd_interface_gate.py`（schema 符合性 ①~⑨）：十项检查各产 machine-readable 子结果，汇总 `results/dataset_v3/cd_interface_gate_result.json`，`overall=true` 为 T-432 解锁必要条件之一（FR-498）。D0 阶段交付 Gate 代码 + 对 pilot fixture 的骨架自检（标 NON-FORMAL），正式执行在 C 就绪后。

### DGN-17 T-432 激活机制（FR-497/498）

- `T-432 activation record`（`docs/phase4/d_dataset_v3/t432_activation_record.md`，activation 时创建）：冻结 formal voyage exact count、strata/template/group allocation、IID/OOD counts、generation seeds、OOD definition version、formal-voyage 合格判据（最小时长和/或最小窗口数，R4）；
- Unblock Checklist 八项（FR-498）以 checklist JSON 机器化，全绿才允许执行 `73_generate_fleet_v3.m`；
- Stage A ≈100 条 qualification：稳定性/导出完整性/schema 一致/吞吐/failed-voyage 处理（剔除+记录，不静默修补）/审计管线；Stage B 按 activation record 预注册执行。

## Error Handling

- voyage 导出缺列/缺 snapshot：该 voyage 标记 failed 并记录原因，pipeline 继续其余 voyage（禁静默补数）；
- registry/schema 校验失败：构建立即 abort（fail-fast，契约错误不降级为警告）；
- 窗口 K/F vintage 缺失：窗口 invalid + 计数（DGN-07），超过阈值（0）即 pipeline FAIL；
- L1 泄漏：Hard FAIL，输出违规清单；
- MATLAB 侧 Gate FAIL：`error('GATE:*')` 语义沿用（AGENTS.md §12）。

## Security and Privacy

无外部数据/凭证；仿真数据明确标注 simulation-generated（AI_MODELING §3）；`target_vessel_confirmed=false` 表述纪律（FR-494）。

## Testing Strategy

- **单元测试**（`dataset_v3/tests/`）：聚合能量守恒（DGN-05/08）、bin 边界 off-by-one（DGN-05 bin 语义）、窗口合法性/越界丢弃、role 白名单（H3/FORBIDDEN 不进 view）、vintage 对齐（issue==t₀ 精确匹配）、ctrl_mode zero-fill、deterministic split（同输入同 assignment、顺序无关）、E300 与 trajectory 一致性；
- **负向测试**：构造 L1 跨 split voyage/重复 artifact/exact duplicate window 必须检出；future-truth 冒充 INPUT_F 必须检出；FORBIDDEN 列入 view 必须被拒；
- **确定性**：同一 pilot 输入连跑两次，产物 hash 一致；
- **pilot E2E**：S01~S36/M1~M3 子集导出 → 构建 → 审计 → manifest，71 号 Gate overall PASS（标 PILOT）；
- **回归**：10b/E10 修复后 40 号物理 Gate 复跑 overall=true（43/44 不受影响——不动控制链，但 44 负向涉及 case 隔离时同步复跑）；
- **T-432 解锁后**（本 spec 内、D0 之后）：C/D Gate 正式版、Stage A/B、T-435 formal 审计（另行激活记录约束）。

## Rollout and Migration

1. 契约层：registry/schema/四文档 → 2. hygiene 修复（10b/E10/case 隔离）+ 40 复验 → 3. MATLAB producer + fixtures → 4. Python 管线 + 单测/负向 → 5. pilot E2E + 71 Gate → 6. C/D Gate 骨架 + activation 模板 → 7. README 两级同步/legacy 标记/路径扫描 → **停（D0 complete，T-432 BLOCKED）** →（C 包后）8. C/D Gate → 9. T-432 Stage A/B → 10. T-435 formal → Dataset-v3 freeze。
无数据迁移；legacy 资产原地冻结。

## Requirements Traceability

| Requirement | Design Decision | Validation |
|---|---|---|
| FR-458/459/460 | Architecture（执行链）、DGN-17 | spec 状态纪律 + AC-430~432 |
| FR-461 | DGN-02（H1/H2 清单 registry 化） | validator + 单测 |
| FR-462/463 | DGN-02/03/04（K/F/U + issue-time） | 契约文档 + validator |
| FR-464/465 | DGN-01/02（八 role、机器事实源） | 负向测试（FORBIDDEN 入 view 拒收） |
| FR-466/467/468 | DGN-04/13（双索引、causal fixture、s_plan） | 单测 vintage 对齐 + 负向 future-truth 检出 |
| FR-469~475 | DGN-05/06/07/08 + Architecture | 聚合/bin/窗口单测 + pilot E2E |
| FR-476~480 | DGN-08/12（target/E300/view） | 能量不变量单测 + view 构建测试 |
| FR-481~483 | DGN-09 + manifest.py | deterministic split 单测 |
| FR-484 | DGN-10 | L1/L2/L3 负向测试 |
| FR-485/486 | DGN-11 | OOD 装载器 pilot 机制验证 |
| FR-487/488 | DGN-12（sealed 字段/manifest） | manifest 校验单测 |
| FR-489/500 | DGN-13 + 71 Gate | pilot 两阶段纪律 + fixture 标记校验 |
| FR-490/491 | DGN-14/15 | 10b/E10 复验 + 40 Gate overall=true |
| FR-492/493 | DGN-01（70+ 段/新目录/legacy 冻结） | README 标记 + 路径扫描 |
| FR-494 | N/A（表述纪律，无代码） | 文档措辞审查 |
| FR-495~499 | DGN-03/16/17 | C/D Gate 骨架自检（NON-FORMAL） |
| NFR-420 | Architecture（基线零改动前提） | Model E sha/Gate 复验 |
| NFR-421/422/423 | DGN-01/12 | hash 字段、路径扫描、README 同步 |
| NFR-424~426 | DGN-09/10/11（统计层级/解释边界） | manifest + 报告措辞审查 |

## Risks and Trade-offs

- **风险**：pilot fixture 与 formal K/F 语义差距导致管线在 formal 切换时暴露集成问题 → 缓解：producer schema 先行冻结 + C/D Gate 十项机器检查（DGN-03/16）；
- **风险**：OOD 训练支持上界（|U|≤2.0、H/T≥2.0）依赖 C 设计确认 → 缓解：C/D Interface Gate 项⑦/⑧显式核对，越界降级 STRESS_SET（DGN-11）；
- **风险**：JSON registry 与代码漂移 → 缓解：validator fail-fast + registry hash 入 manifest（DGN-02/12）；
- **权衡**：stride 10 s 样本量 vs 重叠度——采纳 D-D5 裁决（voyage 为独立层级，重叠允许）；
- **权衡**：10b mask 60 s/30 s 常数为工程判断（启动 25+5 s 物理 + 收敛裕量），阈值语义不变、规则预声明（DGN-14）。

## 偏差登记

（D0 施工期偏差逐项登记如下；均为非语义实现变化/笔误修正/审查闭合，D-D1~D-D9 科学契约不变。）

| # | DGN | 偏差 | 原因 | 影响 | 状态 |
|---|---|---|---|---|---|
| D-1 | DGN-08 | design 原文 "y_k=E_k·3600/10" 为笔误，实现按 y_k=E_k/10 [kW] | FR-476 与 HANDOFF §5 权威口径（kW·s 能量差÷10s）；targets.py/registry/dataset_v3_schema.md 三处同口径登记 | 无（FR-476 字面即正确公式） | 已实施 |
| D-2 | DGN-04 | voyage artifact 用 .mat **v7**（scipy.io.loadmat 直读），非 "v7.3/HDF5" 字面 | MATLAB-HDF5 字符串/struct 编码互操作风险；v2 读链成熟；单航次恒 <2GB | 无（双索引 schema 级区分等科学契约不变；FR-499 非语义实现变化） | 已实施 |
| D-3 | DGN-14 | mask 的 DG settling 窗常数取 30 s；v_ref 阶跃容差 0.10 m/s/采样 | design 冻结规则未钉该常数；30s=v_ref 窗同值（启动 25+5s+收敛裕量同理）；容差>最大指令斜坡 0.20 m/s² 之半 | 阈值语义不变，mask 规则仍预声明 | 已实施 |
| D-4 | DGN-14 | 恢复判据（t_rec ≤1.5s）与稳态判据同样限定 eligible 域 | 启动暂态跌落属豁免窗（基线实证：修复前 t_rec=12.92s 全 case FAIL ← 启动全局最低点；修复后 rec_masked=0） | 阈值不变，仅判据域（FR-490 语义） | 已实施 |
| D-5 | DGN-15 | reset_bench_nominal 为纯 assignin 通道恢复，**禁止**在 base 重跑 config 脚本 | 事故实证：TestScenarios 内部 `for k=1:10` 覆写脚本型 Gate 调用方同名循环变量（40 E05 六档全退化 S10，R 逐位相同） | 无（DGN-15 本身规定 assignin 机制） | 已实施 |
| D-6 | DGN-16 | 43 号注入场景的 case 隔离 reset 位于 prep_fn **之前**（runOne 增 skipReset） | 事故实证：runOne 内 reset 抹掉 prep_fn 已施加注入（S1 GEN_CMD_STOP_TS/S3 force/S5 EMS_HEALTH_TS → C6 FAIL） | 无（注入场景隔离不变量保持） | 已实施 |
| D-7 | DGN-02 | registry 删除 below_min_DG1 条目（91→90） | configureLogging 有登记但运行时 logsout 无该命名元素（build 期线命名失败；探针实证 MISSING_FROM_DS=below_min_DG1、below_min_DG2 在）；Model E 零改动不修模型 | 无（FORBIDDEN 存档条目，非 H1/H2 契约集） | 已实施 |
| D-8 | DGN-05 | interval_any 用于 FORBIDDEN flag 存档列 | DGN-05 括注"仅 METADATA/ORACLE"针对契约列；存档 flag 列沿用同一聚合语义 | 无（view 白名单下零管线影响） | 已实施 |
| D-9 | DGN-02 | AI 变体内部信号（P_pred_max_kW/t_peak_pred_s/AI_valid/P_ff_kW/t_fire_latched）与 DG_cmd_stop 不入 registry | 前者仅活动变体可记录（law=0 pilot 不存在）；后者为 bench 注入向量信号（非标量聚合语义） | 无（registry 未登记即不导出，零泄漏面） | 已实施 |
| D-10 | DGN-09/12 | split hash 分隔符 "‖" 落案 ASCII `\|`（SEPARATOR 常量集中）；manifest run_metadata 补 DGN-12 冻结槽位强制（脚本路径+SHA/版本/构建时间）；L3 接口补 provenance 接线 + formal require_train_stats fail-fast | 交叉审查（R0/R1/R2）闭合项 | 无（审查修复，回归测试覆盖） | 已实施 |
| D-11 | 环境 | pytest 安装进 `.venv_tf`（9.1.1） | tasks 指定 `python -m pytest` 验证命令，venv 原缺 | 开发工具，非科学依赖 | 已实施 |
| D-12 | 证据 | hygiene 复验证据分两段（10b/40/42 一段；43/44 于 D-6 修复后一段） | 43 注入顺序修复只动 run_gate.m（不影响 10b/40/42 代码路径） | 两段均为最终代码态证据 | 已实施 |
| D-13 | DGN-13 | pilot 航次表移除 S09_emergency_sprint（19 航次）；S31~S36 航程 3000→5000 m（<600 s 零窗）；<600 s 航次（M1/M2）零快照合法导出 | S09 与 S01 实现轨迹逐位相同（v_ref 同为常数表、accel_ramp 不约束常数剖面）→ L1③ exact-duplicate 检出（机制正确性实证）；浅水层需真实窗口；FR-472 边界 | L1 冻结规则零改动；应急坡道语义属 C 包 Mission v3 | 已实施 |
| D-14 | DGN-15 | reset_bench_nominal 的 PMS_SOURCE_MODE 恢复为 0（BASELINE_POLICY），与 Control 参数文件默认 1（nominal source）不同 | law=0 下 Source_Selector 双源共享同一 Baseline 链（功能同源）——40 号 golden Gate 接线前后逐位一致实证（G2 六档 R 与 B 包基线逐位相等）；0 与 pilot meta/43 号 P2 臂"src=0 确定性需求源"约定一致 | 无（经验证据等价）；C 包 formal harness 不得继承此歧义，须显式设定 src | 已实施 |
| D-15 | DGN-16 | C/D Gate 骨架的十项检查折叠为 5 个机器键（c01/c02/c07/c09/c10），③④⑤⑥⑧ 以违规消息前缀归因；⑤ oracle 分离为列名 marker 扫描；⑩ 独立运行 python 时缺省 pass（经 72 号路径 fail-closed） | NON-FORMAL 骨架自检粒度；正式版（C 就绪后）须逐项独立键并收紧 ⑤⑩ | 正式 C/D Gate 前必改（见 D0_COMPLETE_HANDOFF） | 已登记（骨架级） |

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-04 | design 批准（CONDITIONAL PASS + R1~R8 闭合）：R1 统一 time-weighted 积分语义（`time_mean`）；R2 split hash 修正（64-bit 归一化、group 层 identity、strata fail-fast、rounding/tie-breaking 冻结）；R3 L2 provenance-gated 三分类 + L1 content hash 覆盖 H1/H2/K/F/Y；R4 OOD 轴 A 数值改 activation-time（Otherwise 路径：C 语义未冻结，不满足 bench 验证前提）+ 删未批准的 10% 默认建议 + 最小 requirements amendment（FR-485）；R5 pilot s_plan/v_plan fixture 语义修正（planned along-track SOG，禁无条件复用 v_ref）；R6 forecast provenance 机器可验证（source_cutoff≤issue_time 不变量 + provenance class + 负向测试）；R7 10b mask 增加 DG 稳态条件 + fail-closed；R8 证据分阶段目录隔离 + path roots + formal failed-voyage retry policy + deterministic hash（payload/run metadata 分离、自引用 hash 排除） | 用户 design gate 评审 |

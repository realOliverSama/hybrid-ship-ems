# Phase 4-C — 真实内河航次与工况生成器 v3（Mission v3，T-419~T-429） Design

Spec: `phase4-c-mission-v3`  
Status: design-draft  
Created: 2026-09-05  
Brainstorm: `./brainstorm.md`（C-D1~C-D7 + Closeout A/B/C）
Requirements: `./requirements.md`（FR-501~562 / NFR-427~433 / AC-433~440，2026-09-05 requirements-approved）

## Summary

Mission Generator v3 按"Route → Environment → Navigation Constraint → Operator → Plant"生成真实内河航次：**Route/Template 层**定义航线与任务段拓扑；**Environment 层**在 regime 条件下生成水流/水位/交通真值实现；**Navigation Constraint 层**把桥区/限速/UKC 转为计划约束；**Operator/Plan 层**把任务语义编译为速度计划与 hotel 计划；**Scenario Compiler** 把上述一切编译为 Model E 既有 SCN/base-variable 接口（零模型改动）；**Producer** 跑仿真并按冻结 producer schema 导出 truth stream + K/F snapshots + provenance；**Audit 层**执行合格判据/失败分类学/拒绝审计/T-429 可辨识性/strata 审计。

关键既有接口实证（2026-09-05 勘察 `config/ModelE_TestScenarios.m`）：SCN 场景结构含 `v_knots_x/v`（空间速度计划）、`v_time_s/v_value_mps` + `speed_profile_mode='time'`（**时间模速度计划**）、`current_knots_x/v`（空间流场）、`current_time_s/current_value_mps`（**时间模流场**）、`water_depth_m`（逐航次标量，经 `VOYAGE_WATER_DEPTH_M` 编译期注入）、`accel/decel_ramp_mps2`、`hotel_kW`、`initial_SOC`、`random_seed`。T-423 的时间相关扰动 δU(t) 经**时间模流场接口**原生表达；浅水经逐航次常量水深表达（航次级浅水暴露，段级浅水语义 = 低 H/T 航次 × 段角色组合）。

## Goals and Non-Goals

### Goals

- T-419 架构落地：conditional-DAG 驱动的分层生成器（FR-513~520）；
- T-420~T-426 七类任务级工况族 + T-427 三 mission-archetype（FR-505~512）；
- producer schema 十一项能力族 conforming 实现（FR-501/558~562）；
- 科学 seed 树 + 干预层 + grouping/must-link 机器实现（FR-521~525/532）；
- 模型一致因果预报 GOOD/DEGRADED（FR-526~531）；
- T-429 可辨识性硬 Gate（FR-547~551）；
- strata registry + MISSION_V3_PREQUALIFICATION 审计（FR-552~557）；
- C/D Interface Gate 就绪（FR-534/546）。

### Non-Goals

- Model E 任何改动（FR-502/NFR-427）；
- D/T-432 Stage A/B 与 T-435-formal（D 侧下游）；
- E/F 包预测模型与 EMS；
- 集合/概率环境预报（FR-531 v1 单轨）；
- tidal regime（FR-514）。

## Architecture

### DGN-01 总体架构与模块分解（T-419）

```text
config/mission_v3/                         ← 全部机器可读注册表（JSON）
  mission_template_registry.json           DGN-02
  conditional_dag_config.json              DGN-03
  environment_regime_params.json           DGN-04
  id_support_bounds.json                   DGN-05
  forecast_quality_profiles.json           DGN-06
  seed_tree_policy.json                    DGN-07
  scenario_stratum_registry.json           DGN-13
  heldout_token_registry.json              DGN-11
  identifiability_contrast_registry.json   DGN-12

scripts/+mission_v3/                       ← 包函数（薄入口+包函数惯例，NFR-432）
  dag_sample.m            % L0~L3 条件抽取（DGN-03）
  env_realize.m           % 环境真值实现：δU(t)/水位/交通（DGN-04）
  nav_constraints.m       % 桥区/限速/UKC → speed_limit_zones/bridge_zones（FR-505/507）
  plan_compose.m          % 速度计划+hotel 计划（Operator 层，DGN-08/09）
  scenario_compile.m      % → SCN struct + VOYAGE_* 注入集（零 Model E 改动）
  forecast_issue.m        % INPUT_F 因果预报（DGN-06）
  kf_snapshots.m          % K/F issue-major 快照构建（DGN-08）
  seed_tree.m             % 命名 substream 派生（DGN-07）
  family_expand.m         % L4 干预展开 + base_draw/family/split_group 元数据（FR-518/521~525）
  eligibility.m           % 合格判据 + 失败分类学（DGN-10）
  producer_export.m       % truth stream + kf + meta → .mat（对齐 export_pilot 布局）
  stack_identity.m        % qualified generation-stack 复合 hash（DGN-14）
  audits/identifiability.m% T-429（DGN-12）
  audits/strata_report.m  % 预资格 stratum 审计（DGN-13）

scripts/80_run_mission_v3_prequal.m        ← 薄入口：MISSION_V3_PREQUALIFICATION
scripts/81_run_mission_v3_t429.m           ← 薄入口：T-429 Gate
```

数据流：`dag_sample`（L0~L3）→ BASE DRAW 落盘（含 seed lineage）→ `family_expand`（L4）→ 每成员 `env_realize`/`plan_compose`/`scenario_compile` → `model.run_scenario_sim`（Model E 原样）→ `producer_export` + `forecast_issue`/`kf_snapshots` → `eligibility` 判定 → artifact + manifest。**BASE DRAW 先落盘后干预**——must-link/连通闭包从落盘 lineage 机器可证（FR-522/523）。

### DGN-02 第一项设计决策：HEAVY_CURRENT 正交性裁决（FR-512/553，registry 冻结前置）

**裁决提案（Design Gate 待批准）**：路线 §4 的 "heavy-current mission"（departure→cruise→upstream→shallow→work→return）其定义性特征即水文条件（强流+浅水），与 environment regime **非正交**——若保留为模板则 HEAVY_CURRENT×NORMAL/DRY 语义自相矛盾。按 C-D7 §4 授权的合法路径，**重构第三槽位为环境无关任务原型**：

| 槽位 | mission_template_id | 语义 | 段序列（环境无关表述） |
|---|---|---|---|
| 1 | `ROUTINE_PATROL` | 例行巡航执法 | departure→harbor→cruise→bridge→patrol→return→berth |
| 2 | `EMERGENCY_RESPONSE` | 应急出动 | standby→departure→rush→(bridge)→mission→return |
| 3 | `WORK_MISSION` | 作业任务（巡航→作业区低速作业+高 mission load→返航） | departure→cruise→work→return |

"强流任务"语义改由组合表达：`WORK_MISSION × FLOOD`（及上游段/浅水暴露由 L1 regime + route/reach 条件化，非模板硬编码）。效果：三模板均与 regime 正交 → **scenario_stratum_registry 9 格全部 ACTIVE**（无 STRUCTURAL_NOT_APPLICABLE 格）。舰队规模后果（C-D7 §17）：9 ACTIVE 格 × ≥14 组 = **≥126 unique split groups** 的 formal 下限（另加 family 倍数与 OOD 配额；精确值 T-432 activation 推导）。

### DGN-03 conditional-DAG 具体节点表（FR-513~518）

| 节点 | 父节点 | 分布族（候选） | 说明 |
|---|---|---|---|
| template | — | 配额驱动（非随机） | L0 |
| regime | — | 配额驱动 | L1：NORMAL/FLOOD/DRY |
| reach（河段特征） | template | 每 template 固定 reach 序列 | 桥区/港区/浅滩段位置载体 |
| operator_profile | template | 类别分布（EMERGENCY→URGENT 先验 1.0；ROUTINE→{CALM .3, STANDARD .6, URGENT .1}；WORK→{CALM .2, STANDARD .7, URGENT .1}） | L2 |
| Ū（均值流，带符号） | regime × direction | 截断正态（DGN-05 支撑界） | 方向=up/downstream 段 |
| δU 过程参数 (σ_u, L_u) | regime | 查表（DGN-04） | 时间相关扰动强度 |
| water_depth（逐航次常量） | regime | 截断正态（DGN-05） | Model E 标量接口约束 |
| traffic 暴露率 λ | regime × reach（桥/港段 λ 高） | Poisson 率参数 | DGN-04 |
| segment 长度拉伸因子 | template | U[0.8, 1.25] | L3 |
| hotel plan 参数 | template × phase | 查表 + 小幅扰动 | DGN-09 |
| ΔP_hotel,unplanned 过程 | — | AR(1) 小幅度 | U 类残差（FR-516） |
| resistance_mult | — | U[0.95, 1.05]（候选） | base draw 共享（FR-517） |
| 干预变量 | base draw | 预声明 matrix | L4（FR-518/521） |

除表内父子边外全部条件独立；DAG config 落 `conditional_dag_config.json`（hash 入 stack identity）。

### DGN-04 环境真值过程（FR-508；T-423 落地）

- **δU(t)**：Ornstein-Uhlenbeck 实现，经**时间模流场接口**（`current_time_s`/`current_value_mps`，1 s 网格 ZOH，From Workspace Interpolate off 纪律）注入：U(t) = Ū + δU(t)，δU 平稳 OU（dX = −X/τ dt + σ√(2/τ) dW，候选 τ=900 s；σ_regime：NORMAL 0.10 / FLOOD 0.20 / DRY 0.08 m/s，候选值）；**有界性**由 DGN-05 支撑界 reject/resample 保证（禁 clamp，FR-520）。
- **水位**：逐航次常量（Model E 接口约束）——从 regime 条件分布抽取（DGN-05）；航次内日尺度慢变在 v1 不建模（航次时长 ≤1 h ≪ 水位日变尺度；候选裁决）。
- **交通事件**：Poisson 到达（λ 条件于 regime×reach）；事件 = 前船 slowdown→follow→recover（duration ~ U[60,180] s、speed ratio ~ U[0.5,0.8]，候选），作用于计划层（operator 响应前船），非直接改 Plant。

### DGN-05 ID 支撑界与 guard band（候选值，FR-519）

| 轴 | IID 支撑（候选） | guard band（候选） | formal OOD（activation 冻结） |
|---|---|---|---|
|  adverse current \|U\|（逆水） | ≤ 1.8 m/s | (1.8, 2.0] | 轴 A：>2.0（≤2.8，T-432 activation） |
| 顺水 \|U\| | ≤ 1.5 m/s | (1.5, 1.8] | 轴 A 同框架 |
| H/T | ≥ 1.5 | [1.35, 1.5) | 轴 B：(1.2, 1.35) |

依据：T-412 E05 验证域 H/T∈{3.0,2.7,2.0,1.6,1.4,1.2}（单调零违例，clamp 界 1.2）；轴 B 必须留经验证的 Model-E 域内（>1.2）。**数值为 design 候选，Design Gate 冻结**；OOD 数值仍走 T-432 activation。

### DGN-06 预报生成器（FR-526~531）

- **GOOD_V1**：u_fcst(t₀+h|t₀) = Ū + (u_obs(t₀)−Ū)·e^{−h/τ}，u_obs(t₀) = U(t₀)（OBSERVED——流速为部署可测量，H1 列）；wl_fcst = 航次常量水深的部署可得估计（海图值；K 类精确语义下 wl forecast 与 h_chart 的关系：wl_fcst = chart-derived causal estimate，GOOD 下 = 真值常量）。
- **DEGRADED_V1**（候选参数，Design Gate 冻结）：u_obs(t₀) = U(t₀−Δ_lag) + b_voyage + η(t₀)，Δ_lag=1800 s、b_voyage~N(0, 0.05²) 航次级偏置、η 为 AR(1)（τ_η=600 s，σ_η=0.05 m/s）时间相关观测误差——全部仅消费 t≤t₀ 信息；source_cutoff=t₀−Δ_lag；wl 同构（bias 主导）。
- provenance：generator_id=`mission_v3_env_forecast`、version、hash=forecast_issue.m SHA256、issue/cutoff 逐快照、class=CAUSAL_GENERATOR、`forecast_quality_profile_id`∈{GOOD_V1, DEGRADED_V1}。
- **因果性机器验证**：forecast_issue 只接受 t≤t₀ 的流切片（函数接口上 truth 向量截断传入）；负向测试注入 future truth 必须被检出（AC-436）。

### DGN-07 科学 seed 树（Closeout A / FR-532）

- 算法（design 冻结候选）：`seed(branch_path) = uint32(SHA256(policy_version ‖ formal_generation_seed ‖ base_draw_index ‖ branch_name)[1:4])`；MATLAB 侧 `RandStream('Threefry4x64', 'Seed', seed)`，每命名分支独立流；分支清单：current_truth / water_level / traffic / operator_residual / hotel_residual / resistance / segment_geometry / forecast_obs_noise（DEGRADED）/ intervention 专属分支。
- 不变量实现：INFORMATION counterfactual 成员共用全部 truth 分支 seed（仅 forecast_obs_noise 分支不同）→ truth hash 相同的机器校验（FR-529）；PHYSICAL counterfactual 仅声明干预分支 seed 不同。
- seed_tree_policy.json 登记算法版本/分支清单/hash；每航次 meta 记录全分支 seed（auditable lineage，FR-522）。

### DGN-08 s_plan / K/F 构建（FR-558~562）

- 计划载体：plan_compose 产出**计划速度表**（time-mode：v_plan(t) 由模板段序列+operator profile+nav constraints 合成；emergency/bridge/harbor 语义在此层成形）。
- s_plan/v_plan[k|t₀]：从 **issue 时刻计划位置** x_plan(t₀) 按计划表前向积分（1 s 子步，同 pilot 布局惯例）；位置相关约束（h_chart/speed_limit_zones/bridge_zones）沿**计划轨迹** interval-aware 采样（FR-560）；K/F 网格严格 {10..300}×30 bin（FR-561）。
- 布局与 export_pilot 对齐：kf.issue_time_s (M×1)、lead_s (M×30)、各列 (M×30) issue-major；artifact `.mat -v7` {stream, kf, meta}。
- 缺快照 = FAILED_CONTRACT（FR-562），禁填补。

### DGN-09 hotel 计划分解（FR-516）

P_hotel,plan：template×phase 查表（如 ROUTINE_PATROL patrol 段 100 kW、WORK_MISSION work 段 180 kW、EMERGENCY mission 段 220 kW——候选值，Design Gate 冻结）；ΔP_unplanned：AR(1)（τ=300 s、σ=8 kW，候选）；实际注入 Model E 的 hotel_kW 通道 = plan + 已实现的 unplanned 残差；K 列只暴露 plan 分量（P_hotel_plan_kW），残差只存在于 truth/ORACLE 侧。

### DGN-10 合格判据与失败分类学机器实现（FR-535~540）

`eligibility.m` 输出逐候选记录 {disposition ∈ ACCEPT / REJECTED_SAMPLING_ELIGIBILITY(reason: DURATION_BELOW_MIN / SUPPORT_BOUND / PHYSICAL_DOMAIN_BOUND) / REJECTED_STRUCTURAL_HELDOUT(token_id) / FAILED_CONTRACT / FAILED_GENERATION / FAILED_PHYSICAL_DOMAIN} + seed lineage + stratum；expected/valid/invalid windows 计数（FR-536）；retry 同 slot/config/seed（FR-537）；拒绝率报告按 stratum 分组（FR-539）；**拒绝率合格阈值候选 50%**（单 stratum 约束拒绝率 >50% ⇒ 预资格 FAIL 并暴露生成设计问题，Design Gate 冻结）。

### DGN-11 OOD 生成面与 token 谓词（FR-541~546）

heldout_token_registry.json 三 token（C-D5 机器名）谓词形式化：
- C1：`segment.role==BRIDGE_PASSAGE ∧ U_adverse(segment) ∈ HIGH_ID_BAND`；HIGH_ID_BAND 候选 = adverse |U| ∈ [1.4, 1.8]（ID 支撑上带，Design Gate 冻结）；
- C2：`segment.role==HARBOR ∧ H/T_voyage ∈ LOW_ID_BAND`；LOW_ID_BAND 候选 = H/T ∈ [1.5, 2.0]；
- C3：`segment.role==EMERGENCY_SPRINT（mission-plan 段角色） ∧ H/T_voyage ∈ LOW_ID_BAND`；
- 谓词只消费生成侧 metadata；IID 生成器 EXCLUDE / OOD 生成器 REQUIRE 消费同一注册表文件；边际覆盖义务：每 token 的 A∧¬B 与 ¬A∧B 各 ≥ **10 unique split groups**（候选，activation 复核）；C-3 可行性前置验证：预资格阶段实测 C3 谓词满足航次的可达性（rush 段在低 H/T 下无 domain_warning 聚集、SOP 不饱和），证据落报告。
- ood_config_surface 输出：ood_axis/ood_def_version=`C-D5-v1`/physics_domain_ref=`ModelE validated domain (T-412 v1.1; H/T∈[1.2,…] E05)`；越域标 PHYSICS_EXTRAPOLATION/STRESS_SET。

### DGN-12 T-429 可辨识性审计实现（FR-547~551）

- **诊断切片**：SEGMENT_PURE_DIAGNOSTIC_SLICE = 段角色区间内去边界缓冲后的定长切片：候选 时长 240 s、边界缓冲 60 s、stride 120 s、1 Hz 聚合（同 D time_mean 语义）；不足 3 组（split_group）或 10 切片/类 ⇒ INSUFFICIENT_IDENTIFIABILITY_EVIDENCE fail-closed。
- **SBD**：复用 D 的 SBD 引擎（1 Hz P_load_total_kW shape）；R = median(cross)/max(Q0.95 within₁, Q0.95 within₂)；**R_min=1.5 候选，Gate 首跑前冻结**；group bootstrap（1000 次重抽样 split_group，95% LCB ≥ 1.0 且点估计 ≥ R_min）。
- **探针**：logistic 回归（liblinear 式固定配置：L2 C=1.0、class_weight balanced、seed 固定），特征 = 切片级 H1 派生摘要统计（mean/std/max of P_load_total, v_ship, accel, n_prop, P_hotel, v_current + hotel 占比 + 低速时间占比；候选编码，Gate 前冻结）；CV 按 split_group_id 分 5 折，scaler train-only；**A_min=0.80 候选（BA，Gate 首跑前冻结）**。
- **contrast registry 初始清单**（design 候选，Design Gate 冻结）：SEG_BRIDGE_VS_HARBOR（控制：同 regime×direction）、SEG_BERTH_VS_DEPARTURE（同 template）、DIR_UPSTREAM_VS_DOWNSTREAM（同 regime×reach）、DEPTH_LOW_ID_VS_NORMAL_ID（同 template×regime）、SEG_EMERGENCY_SPRINT_VS_CRUISE（同 regime）、SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE（同 regime×reach）、SEG_HARBOR_VS_SHALLOW_CRUISE、SEG_BRIDGE_VS_SHALLOW_CRUISE。
- FAIL 处置：仅允许回到 T-420~T-426 重设计；阈值/registry 冻结后不得因失败修改（FR-551）。

### DGN-13 strata registry 与预资格审计（FR-552~557）

scenario_stratum_registry.json：9 格全 ACTIVE（DGN-02 裁决后），ID 形如 `STRAT_ROUTINE_PATROL__NORMAL`；每格记 mission_template_id/environment_regime/status/reason/version。预资格（≈100 航次 nominal，工程规模）产出 `results/mission_v3/prequal_strata_audit.json`：FR-557 全字段（含 14 组下限的当前满足度投影——**不宣称 Stage-B 配额满足**）；预资格同时演练 token 双向谓词、失败分类学、stack identity、T-429 管线（小规模证据）。

### DGN-14 合格生成栈身份（Closeout C / FR-534）

`results/mission_v3/generation_stack_identity.json`：十项组件 {mission_v3_generator (包目录复合 SHA256), mission_template_registry, conditional_dag_config, seed_tree_policy, forecast_generator (forecast_issue.m SHA256), scenario_stratum_registry, heldout_token_registry, modele_physical_baseline (v1.1 hash), modele_control_baseline (v1.0 hash), d_producer_schema (self_hash 7993e141…)} 各记 version+sha256；复合 hash = SHA256(canonical JSON)。C/D Gate/预资格/T-432 链各环节记录并比对；不一致 ⇒ qualification invalid。

### DGN-15 产物布局

- 代码：`model/simulink_modelE_(Phase4_Physics)/scripts/+mission_v3/` + 薄入口 80/81；注册表 `config/mission_v3/`（入库）；
- 航次产物：`datasets_v3/mission_v3/prequal/<voyage_id>.mat`（本地不入库，同 pilot 纪律）；formal 产物路径由 T-432 activation 定；
- 报告：`model/simulink_modelE_(Phase4_Physics)/results/mission_v3/`（prequal manifest/strata 审计/T-429 报告/stack identity；机器可读 JSON + 人读 MD）；
- 文档：`docs/phase4/c_mission_v3/`（架构说明 + 预资格报告 + T-429 报告，README 索引同步，NFR-430）。

### DGN-16 与 D 侧接口的衔接

C 产物逐字段对 producer schema；C/D Interface Gate 正式版（含 D-15 必改：十项独立键+⑤⑩收紧）在 C 完成后由 D 侧执行——C 侧义务 = 产物 conforming + 预资格证据 + stack identity 可比对。FR-499：conforming implementation 不重走四门禁。

## Data Model

- **BASE DRAW 落盘记录**（每 base draw 一条）：base_draw_id、template/regime/operator/reach、全部 L1~L3 抽取值、全分支 seed、stratum 归属（FR-552）。
- **Voyage artifact**：同 pilot 三 struct 布局（stream/kf/meta，.mat v7），meta 增 base_draw_id/voyage_family_id/split_group_id/intervention 描述/forecast_quality_profile_id/stratum/dataset_domain/ood 字段（producer schema 五族 metadata + C-D1 扩展字段）。
- **Family 清单**：family_id → member voyage_ids + intervention matrix + 声明的共享 ancestry（must-link 证据）。
- **注册表六件**：见 DGN-01 架构图（全部 JSON + 自描述版本字段）。

## API / Interface Changes

- 新增 MATLAB 包 `+mission_v3`（上表函数签名在 tasks 阶段细化）；薄入口 `80_run_mission_v3_prequal.m` / `81_run_mission_v3_t429.m`（数字开头脚本经 eval(fileread) 惯例）。
- 不改 Model E 任何接口；SCN 字段消费仅限既有字段集（DGN Summary 勘察清单）；新增场景条目经运行时 SCN struct 构造（不写回 ModelE_TestScenarios.m——**配置脚本只读**，避免 D-5 类事故；VOYAGE_* 注入经显式 assignin 白名单，PMS_SOURCE_MODE 显式设定，NFR-432/D-14）。

## Error Handling

- 失败分类学六类 disposition（DGN-10）；FAILED_* 为 slot failure（同 seed retry，禁换 seed）；REJECTED_* 为正常抽样/结构性事件（审计计数）；
- 单航次导出失败：记录原因继续（同 pilot 纪律，禁静默补数）；任何 FAILED ⇒ 薄入口 error('GATE:*') 终止；
- 约束违反（支撑界/物理域/token）：reject + 计数，禁 clamp；
- T-429 证据不足/对比失败：fail-closed（INSUFFICIENT_IDENTIFIABILITY_EVIDENCE / Gate FAIL），升级主 Agent。

## Testing Strategy

- **单元测试**（`+mission_v3/tests/`）：seed 树确定性（同 lineage 同实现/跨分支独立）、OU 预报解析式对照、计划前向积分不变量、谓词注册表双向一致、eligibility 分类学、must-link 闭包构造；
- **负向测试**（AC-436 清单逐项）：future-truth 冒充 INPUT_F、realized future 冒充 s_plan/v_plan、沿 realized 位置取约束、lead 网格偏移/vintage 反推、缺快照填补、simulator-latent 预报源、跨组未声明 truth 复用、GOOD/DEGRADED truth 漂移、PILOT_ONLY 混 formal、FORBIDDEN/ORACLE 越权；
- **集成**：预资格端到端（≈100 航次）+ producer schema validator + C/D Gate 自检 + 确定性两跑（payload hash 一致）；
- **T-429 干跑**：预资格证据上执行完整 Gate 管线；
- **回归**：Model E 基线五件 sha256 快照比对（72 号同款机制，证零改动）。

## Rollout and Migration

施工顺序（tasks 细化）：注册表六件 + 架构文档（T-419 交付物）→ 环境/计划/预报/seed 树单元 → 工况编译器（T-420~426）→ 模板（T-427）→ 生成器主链（T-428）→ 预资格 → T-429（Gate 首跑前冻结阈值）→ C/D Gate 就绪包。无数据迁移；pilot 产物不动。

## Requirements Traceability

| Requirement | Design Decision | Validation |
| ----------- | --------------- | ---------- |
| FR-501~504（契约遵从/零改动/基线/阶段隔离） | DGN-01/08/15/16 | AC-435/440；schema validator；基线 hash 快照 |
| FR-505~511（七工况族） | DGN-01（nav_constraints/plan_compose）/04/09 | AC-434；逐工况确定性复跑 |
| FR-512（三 archetype） | DGN-02 | AC-434/439；registry 冻结记录 |
| FR-513~520（DAG/regime/operator/load/resistance/干预/guard-band/禁 clamp） | DGN-03/04/05/10 | 单测 + 预资格拒绝审计 |
| FR-521~525（family/base_draw/must-link） | DGN-01（family_expand）/07/15 | AC-436/438；闭包单测 |
| FR-526~531（预报语义/profile/truth 隔离/provenance/单轨） | DGN-06/07 | AC-436/438；负向注入 |
| FR-532（seed 树） | DGN-07 | AC-438；两跑 hash |
| FR-533（无旁路） | DGN-01（架构边界）/15 | AC-436 越权入 view 用例 |
| FR-534（stack identity） | DGN-14 | AC-438 |
| FR-535~540（合格判据/失败分类/短航次/审计/activation 纪律） | DGN-10/13 | AC-435/439；分类学单测 |
| FR-541~546（轴 C token/谓词/互斥/可行性/配置面） | DGN-11 | AC-439；谓词一致性校验 |
| FR-547~551（T-429） | DGN-12 | AC-437；Gate 报告 |
| FR-552~557（strata/registry/14 组/无回退/OOD 隔离/审计） | DGN-02/13 | AC-439 |
| FR-558~562（认识论红线） | DGN-08 | AC-436 负向逐项 |
| NFR-427~433 | DGN-01/15 + Error Handling | AC-440；扫描/两跑/零改动快照 |

## Risks and Trade-offs

- **H/T OOD 空间狭窄**（验证域下沿 1.2，guard 后轴 B 仅 (1.2,1.35)）：轴 B 科学价值有限——缓解：轴 B 数值 activation 时再评估，必要时轴 B 降级为 stress-set 证据；备选：不动验证域（红线）。
- **9 格 × 14 组 ⇒ ≥126 组的 formal 下限**：舰队规模与单航次仿真时长（Model E 全物理 0.01 s 步长，20~60 min 航次）决定 Stage-B 墙钟成本——预资格实测吞吐后由 activation 定夺；不在本 design 承诺规模。
- **δU 经 ZOH 时间模注入**：1 s 网格对 0.01 s 模型步长足够平滑（OU τ=900 s ≫ 1 s）；备选空间模注入被否（航速变化会破坏时间相关性语义）。
- **水位逐航次常量**：放弃航次内水位日变（≪航次时长）；若 E 包证明需要再经 spec amendment。
- **T-429 阈值候选 1.5/0.80 未经校核**：Gate 首跑前以预资格数据做**盲冻结**（看分布定夺是否调整候选——调整须 amendment 记录，且首跑后锁定）。

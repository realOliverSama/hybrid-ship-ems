# Mission Generator v3 工程架构（T-419 / Phase 4-C）

> Spec: `.specs/phase4-c-mission-v3/`（brainstorm → requirements → design → tasks 全部 approved）
> 本文是 **T-003 交付物（AC-433）**：把 design DGN-01~16 落为工程架构说明——模块边界、数据流、
> 注册表清单、与 D 侧 producer schema 十一项能力族的接口映射、seed 树与 stack identity、无旁路边界。
> **数值/语义唯一事实源仍是 spec 三件 + 九件机器注册表；本文不得与其冲突，只做工程化转述与落点索引。**
> 状态：**用户已批准（AC-433，2026-09-05 Day-1 Gate Ruling，CONDITIONAL PASS → A1/A2/A3 收尾项已全部落地）**；
> **PHASE2_ENTRY_GATE = true**（T-003 批准 ∧ T-007 phase1_freeze.json + 修正案 PHASE1-FREEZE-AMEND-D6）。
> Phase 2 生成器施工（T-008 起）已解锁。
> 创建：2026-09-05；批准：2026-09-05。

## 1. 定位与权威链

Mission Generator v3 是 Dataset v3 的**生产侧（C 包）**：按
"Route → Environment → Navigation Constraint → Operator → Plant" 生成真实内河航次，
对已冻结的 D 侧消费契约 conforming 实现（FR-499：不重走四门禁）。

```text
权威链（冲突时自上而下）：
.specs/phase4-c-mission-v3/{brainstorm,requirements,design,tasks}.md   ← 科学裁决与验收
model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_producer_schema.json   ← D 侧机器契约（self_hash=7993e141…）
model/simulink_modelE_(Phase4_Physics)/config/mission_v3/*.json        ← C 侧机器注册表（九件，候选态）
本文（architecture.md）                                                ← 工程落点索引
```

## 2. 总体架构与模块分解（DGN-01）

```text
model/simulink_modelE_(Phase4_Physics)/
  config/mission_v3/                      ← 机器可读注册表九件（候选态，T-001/T-002 落库）
    mission_template_registry.json         DGN-02  三 archetype 段序列/hotel base/operator prior
    conditional_dag_config.json            DGN-03  十四节点 DAG（L0~L4+BASE_DRAW）+ R1 河流坐标语义
    environment_regime_params.json         DGN-04  三 regime 流/OU/水位/traffic/海图水深
    id_support_bounds.json                 DGN-05  三轴 IID/guard/OOD(ACTIVATION_CANDIDATE)
    forecast_quality_profiles.json         DGN-06  GOOD_V1/DEGRADED_V1 + prehistory 1800 s
    seed_tree_policy.json                  DGN-07  SHA256 血统 + Threefry 引擎映射 + 八分支
    scenario_stratum_registry.json         DGN-13  3×3=9 格全 ACTIVE
    heldout_token_registry.json            DGN-11  C1/C2/C3 谓词 + 字段白名单
    identifiability_contrast_registry.json DGN-12  六必需对比 + 二 SECONDARY_DIAGNOSTIC
  scripts/+mission_v3/                    ← 生成器 MATLAB 包（薄入口+包函数惯例）
    validate_registries.m                  注册表 validator（九件，canonical JSON+SHA256 跨语言）
    dag_sample.m                           L0~L3 条件抽取（消费冻结 DAG config）
    env_realize.m                          环境真值：U_river/δU(t) OU/ΔWL/交通（reject/resample 禁 clamp）
    nav_constraints.m                      桥区/限速/UKC → speed_limit_zones/bridge_zones
    plan_compose.m                         速度计划+hotel 计划（Operator/Plan 层）
    scenario_compile.m                     → 运行时 SCN struct + VOYAGE_* 注入集（零模型改动）
    forecast_issue.m                       INPUT_F 因果预报（GOOD/DEGRADED，t≤t₀ 截断接口）
    kf_snapshots.m                         K/F issue-major 快照（s_plan 计划前向积分）
    seed_tree.m                            命名 substream 派生（全 digest 血统）
    family_expand.m                        L4 干预展开 + base_draw/family/split_group 元数据
    eligibility.m                          合格判据 + 六类失败分类学
    producer_export.m                      {stream,kf,meta} .mat v7 导出（对齐 export_pilot 布局）
    stack_identity.m                       十项组件复合 hash（DGN-14）
    +audits/feasibility_sweep.m            T-005 联合可行性扫描（已建）
    +audits/segment_duration_census.m      T-004 段时长普查（已建）
    +audits/identifiability.m              T-429 Gate（T-022 建）
    +audits/strata_report.m                预资格 stratum 审计（T-020 建）
    tests/                                 单测/负向 harness
  scripts/75~81_*.m                        薄入口（75 可行性扫描、76 段普查已建；80 prequal / 81 T-429 待建）
  results/mission_v3/                      机器可读证据（validator 记录/普查/扫描/冻结件/审计）
docs/phase4/c_mission_v3/                  本文档 + 报告归档
```

**分层生成语义**（FR-513，与 DAG config 一致）：

```text
L0 template（配额驱动，非随机） → L1 regime（配额）+ reach 序列（模板确定性派生）
  → L2 operator profile（模板条件类别分布）
  → L3 realization（U_river 均值、δU 过程、ΔWL、traffic λ、heading、段拉伸、hotel plan）
  → BASE DRAW（delta_p_hotel_unplanned、resistance_mult——逐航次常量聚合层）
  → L4 intervention（预声明 matrix；PHYSICAL/INFORMATION 分型）
除声明父子边外全部条件独立；DAG 冻结于 conditional_dag_config.json（hash 入 stack identity）。
```

## 3. 数据流（BASE DRAW 先落盘后干预）

```text
dag_sample（L0~L3）→ BASE DRAW 落盘（含全分支 seed lineage）
  → family_expand（L4 干预展开，每 family 预声明 matrix）
  → 每成员：env_realize → nav_constraints/plan_compose → scenario_compile
      → model.run_scenario_sim（Model E 原样，零改动）
      → forecast_issue / kf_snapshots → producer_export → eligibility 判定
  → artifact {stream,kf,meta}.mat v7 + manifest（lineage/hash/disposition 全登记）
```

不变量：**BASE DRAW 先落盘、后干预**——must-link/连通闭包从落盘 lineage 机器可证（FR-522/523）；
INFORMATION 成员共享全部 truth 分支（仅 forecast_obs_noise 异）⇒ truth hash 相同机器校验（FR-529）；
未声明跨族 truth 复用 = Gate FAIL（FR-524）。

## 4. 与 D 侧 producer schema 十一项能力族的接口映射（FR-501）

| producer schema 能力族 | C 侧实现载体 | 关键契约点 |
|---|---|---|
| truth_stream | `producer_export` stream | realization-time 单轴；**native Model-E realization-rate（0.01 s）真值流**（Day-1 Gate Ruling §A1 更正：1 Hz 是 D 侧历史 H1/H2 聚合视图，不是 C producer native truth——两者不得混同）；双存隔离的 ORACLE 侧 |
| s_plan | `kf_snapshots` | 计划前向积分（1 s 子步、bin 右端点、issue 时刻计划位置起步） |
| planned_future_trajectory | `kf_snapshots` | v_plan/s_plan 沿计划轨迹；禁 realized future |
| static_constraints | `scenario_compile` | h_chart/speed_limit_zones/bridge_zones → INPUT_K 侧 |
| input_k_snapshots | `kf_snapshots` | (M×1)/(M×30) issue-major，lead∈{10..300} |
| input_f_snapshots | `forecast_issue` + `kf_snapshots` | F_{t₀}=g(ℐ_{≤t₀})；缺快照=FAILED_CONTRACT 禁填补 |
| forecast_provenance | `forecast_issue` | 六字段 + forecast_quality_profile_id∈{GOOD_V1,DEGRADED_V1}；formal 仅 CAUSAL_GENERATOR |
| oracle_counterpart | `producer_export` | 与 INPUT_F 物理分离存储（双存隔离） |
| voyage_metadata | `scenario_compile`/`producer_export` meta | C-D1 扩展：base_draw_id/voyage_family_id/split_group_id/intervention/stratum/dataset_domain |
| generation_provenance | `seed_tree` lineage 登记 | branch_path ↔ lineage_id ↔ engine seed 全量入 manifest |
| ood_config_surface | `heldout_token_registry` 消费面 | ood_axis/ood_def_version=C-D5-v1/physics_domain_ref（DGN-11 三字段） |

artifact 表示格式消费 D 契约冻结值：`.mat -v7` {stream,kf,meta}（D design 偏差 D-2），C 不重述不改。

## 5. seed 树与 stack identity（Closeout A/C）

- **seed 树（DGN-07）**：`lineage_id = SHA256(policy_version|formal_generation_seed|base_draw_index|branch_name)`
  全 32 字节 hex = 权威科学身份；Threefry4x64 uint32 引擎 seed = digest 前 4 字节 big-endian（仅引擎输入）；
  碰撞 fail-closed；八分支 + `intervention_<axis>` 干预分支（seed_tree_policy.json 冻结清单）。
- **stack identity（DGN-14）**：十项组件 {mission_v3_generator 复合、template registry、DAG config、
  seed_tree_policy、forecast generator、stratum registry、token registry、Model E 物理基线 v1.1、
  控制基线 v1.0、D producer schema} 各 version+sha256，复合 hash=SHA256(canonical JSON)。
  实现于 `stack_identity.m`（T-016），最终捕获在 T-019（代码冻结后）；跨 Gate/Stage 不一致 ⇒ qualification invalid。
  测试文件是否计入 generator 复合 hash 域：T-016 实现时显式裁决并写入 manifest 字段。

## 6. 无旁路边界（Closeout B / FR-533）

```text
C（本包）──producer contract──▶ D（Dataset v3）──▶ E/F（预测/EMS）
```

- C 只按 frozen producer schema 产 artifact；E/F 不得对 Mission-v3 simulator 内部建独立 predictor 接口；
- simulator 内部量（水文 latent、未来随机创新、resistance latent、traffic 未来真值、privileged generator
  state、regime 内部）**不因 simulator 可知而可用**：进入信息面的唯一通道是 D 治理的 storage-role/access
  控制（INPUT_H1/可选 H2/K/F；ORACLE 仅显式 Oracle 实验）；
- C 侧机器保障：forecast_issue 函数接口只接受 t≤t₀ 截断切片（future-truth 注入必须被检出，AC-436）；
  observability 白名单四类，FUTURE_TRUTH/ORACLE_STATE/SIMULATOR_LATENT_PRIVILEGED 拒收；
  formal fixture_kind=NONE 断言（禁 PILOT_ONLY，FR-500）。

## 7. 与 Model E 的零改动契约（FR-502/NFR-427）

- Plant/EMS/PMS/Safety 参数、模型结构、Solver/Sample Time、Gate 阈值、`.slx`、`ModelE_*.m` 配置脚本全部禁改；
- SCN 字段消费仅限既有字段集（design Summary 勘察清单：时间模速度表/时间模流场/water_depth_m/hotel_kW/
  accel/decel_ramp/initial_SOC/random_seed 等）；新增场景**运行时 SCN struct 构造**，禁写回 `ModelE_TestScenarios.m`；
- VOYAGE_* 注入经显式 assignin 白名单；PMS_SOURCE_MODE 显式设定（D-14）；
- 零改动证明机制：Model E 基线五件 sha256 快照比对（72 号同款）入 T-018 回归与预资格 manifest。

## 8. Phase-1 证据链与冻结点（Day-1 终态：T-004/T-005/T-006/T-007 全部 PASS）

Phase 1 只读可行性证据已完成闭合，冻结配置以**不可变修正案链**维护：

```text
phase1_freeze.json（self_hash 67b9b81e…，基础冻结）
  + phase1_freeze_amendment_d6.json（self_hash dd84f965…，Day-1 Gate Ruling 修正案）
生效 Phase-1 配置复合 hash：f665b953d9f18df0d38cf3a08973a608e4fb7a7a5bc1d5d4a1932d9602af4492
PHASE2_ENTRY_GATE = true（T-003 用户批准 ∧ T-007 + 修正案）
```

**Day-1 最终证据清单**（裁决 §A2 要求项逐条）：

1. **最终诊断切片几何**：slice 120 s / edge buffer 30 s（两侧）/ stride 60 s——T-004 普查（D6 重绑定后重跑）对六必需对比全部宿主 PASS（`segment_duration_census.json`）。
2. **departure 宿主长度修订**：500→700 m（离泊操纵场景宿主全长：startup→低速操纵→驶入航道加速）；**bridge 宿主长度修订**：400→1000 m（**BRIDGE_OPERATION 场景宿主全长：Approach→Deceleration→Controlled transit→Exit→Acceleration**）。裁决 §A3 语义边界：`L_bridge_operation ≠ L_bridge_zone`——1000 m 不是物理桥孔长度、桥区几何长度或限速区长度，除非独立航线证据使两者相等；departure 同理非泊位几何长度。T-010 在宿主内布置子阶段，禁静默 reinterpret 为物理航段长度。
3. **联合可行性前沿证据**（T-005 PASS，裁决 §B 采纳）：246 点 600 s 全格扫 + 83 点 3000 s 稳态精化 + 真深水锚点。**签名约定（采纳形式）**：`V_SOG,max = W_max(H/T) + U_along`（U_along > 0 顺流、< 0 逆流；与实测逆流形式 `W_max − U_adverse` 等价，避免重复符号逻辑）；W_max 表 {HT1.2: 6.6943 / 1.35: 6.7692 / 1.5: 6.7928 / 2.0: 7.1609 / 3.0: 7.2492 / 深水 20 m: 7.5207} m/s。
4. **真深水锚点闭环**（正式 Phase-1 证据）：20 m 处实测 14.6190 kn vs G2 E02 14.6202 kn（−0.008%）。**已采纳解释**：600 s 案例本已达稳态（末段斜率 ~e-18，"暂态截断"解释被测量否决）；早期锚点偏差源于水深口径；H/T=3.0 仍含浅水增量阻力（+12.5% R，与 G2 R_by_tier 定量自洽）；20 m 真深水复现 G2 能力锚点。
5. **rush 计划速度政策**（T-007 冻结，裁决 §C 采纳）：HT∈[1.5,2.0) → 6.5 m/s；HT≥2.0 → 7.0 m/s。≈14.620 kn 保持为只读 Model-E 能力锚点，**不是** Mission-v3 选定速度指令。**T-009/T-010 SHALL 继续用冻结的速度/流/H-T 能力规则执行联合可行性**——6.5/7.0 表不授权在逆流降低可行 SOG 时强行下达不可行组合。
6. **C3 可行性结果**（T-006 PASS）：C3_SUPPORTED_CANDIDATE——冻结 rush 速度在 LOW_ID 带静水精确可达，domain_warning/SOP/跳机全零（FR-545 三条件）。
7. **生效冻结 artifact/hash**：见本节头部修正案链；九件注册表生效 hash 登记于修正案 `effective_phase1_configuration.registry_hashes`（contrast `b980a457…`、template `f4079f91…` 为修正案后值）。
8. **D6 裁决**（§D，APPROVED WITH HOST REBINDING）：`SEG_EMERGENCY_SPRINT_VS_CRUISE` → **`SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE`**（class_A=rush/EMERGENCY_SPRINT，class_B=**return** 稳态返航宿主；mission 段宿主被否决——mission-load/hotel 语义经 P_load_total 形成强混杂捷径）。cohort 加强：模板内 ∧ 同 regime ∧ 同 operator profile ∧（可行时）同 reach ∧（可能时）同 base-draw cohort；标签保持生成侧 metadata，禁由 realized 速度反推。属窄域 registry/design-note 修正，非 spec/DGN-02 amendment。
9. **水深接口修正**（裁决 §F 批准）：`chart_depth_by_reach` 降级为文档/未来能力值；`chart_depth_voyage_base` 为 v1 生成的物理/K 表征。formal v1 中 `h_chart(s) = h_chart,voyage`（沿航线常量的已知函数）；**不提供**与当前 Plant 实现不对应的 route-varying 海图水深预报特征。Model E 零改动。

## 9. 施工顺序与门禁（Day-1 终态）

```text
Phase 0（本文档所在）: T-001 ✅ → T-002 ✅ → T-003 ✅（APPROVED，AC-433 闭合）
Phase 1 只读证据: T-004 ✅ → T-005 ✅ → T-006 ✅ → T-007 ✅（+ D6 修正案）
  【PHASE2_ENTRY_GATE = true】
Phase 2 生成器核心: T-008 seed 树（进行中）→ T-009 环境 → T-010 工况/计划 → T-011 编译
  → T-012 预报 → T-013 K/F → T-014 干预层
Phase 3 导出与审计: T-015 导出/判据 → T-016 stack identity → T-017 负向套件 → T-018 集成回归
Phase 4 预资格与交付: T-019 预注册+捕获 → T-020 预资格 → T-021 T429_FREEZE → T-022 T429_EVALUATE
  → T-023 C/D 就绪包 → T-024 归档+PR
【EXT-CD-GATE（owner=Phase 4-D）达成才 spec implementation-complete；C 侧完成即停】
```

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-05 | 初稿（T-003 交付物，含 §8 Phase-1 在途证据快照） | 主 Agent |
| 2026-09-05 | Day-1 Gate Ruling 收尾：A1 truth_stream 更正为 native 0.01 s realization-rate（1 Hz 属 D 侧历史聚合视图）；A2 §8/§9 更新为 Day-1 终态（T-004~T-007 PASS + 证据清单）；A3 bridge=BRIDGE_OPERATION 宿主语义边界（L_bridge_operation≠L_bridge_zone）；B 采纳签名前沿约定 V_SOG,max=W_max(H/T)+U_along；C rush 政策确认 + 联合可行性强制；D6 SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE 宿主重绑定；F chart_depth_voyage_base 生效/h_chart(s)=航次常量；AC-433=APPROVED、PHASE2_ENTRY_GATE=true | 用户 Day-1 Gate Ruling + 主 Agent 收尾 |

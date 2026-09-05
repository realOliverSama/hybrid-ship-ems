# Phase 4-C — Mission v3（T-419~T-429）Tasks

Spec: `phase4-c-mission-v3`  
Status: tasks-draft  
Created: 2026-09-05  
Brainstorm: `./brainstorm.md`（C-D1~C-D7 + Closeout A/B/C 批准）  
Requirements: `./requirements.md`（requirements-approved + R1~R7 闭合）  
Design: `./design.md`（design-approved，DGN-01~16 + R1~R9 闭合）

> 任务编号 T-001~T-021 为本 spec 局部编号；每项标注路线文档 T-4xx 映射与 FR/DGN 追溯。
> 顶层任务所有权保持路线文档 T-419~T-429（本文件不另立顶层编号）。
> **执行 session 开工前必须研读**：brainstorm.md、requirements.md、design.md、
> `docs/phase4/c_mission_v3_HANDOFF.md`、D 包契约四件 + producer schema（机器权威）、
> Model E 工作区 README_Model.md、`项目开发进展.md` 2026-09-04/05 条目（R2026a 坑清单 + D0 事故记录）。
> **全部契约/数值/规则以 requirements.md + design.md 为唯一事实源，禁止凭记忆施工。**

## 执行约束（全任务通用）

- 分支 `feat/phase4-c-mission-v3`；逐文件 add；禁 merge 至 Phase4（待用户 PR 审批）；Git 红线全适用；
- **Model E 零改动**（FR-502/NFR-427）：Plant/EMS/PMS/Safety 参数、模型结构、Solver/Sample Time、Gate 阈值、`.slx`、`ModelE_*.m` 配置脚本全部禁改；SCN 消费仅限既有字段集（design Summary 勘察清单）；新增场景经运行时 SCN struct 构造（**配置脚本只读**，防 D-5 类事故）；VOYAGE_* 注入经显式 assignin 白名单；PMS_SOURCE_MODE 显式设定（D-14）；
- **契约红线**：issue-time causality / 双索引 schema / 双存隔离 / 阶段隔离（formal 禁 PILOT_ONLY）/ 基线法律角色（物理 v1.1+控制 v1.0；A2 historical v1.0 仅参照）——违反=返工；
- **禁绝对路径**（NFR-429）；**机器注册表唯一事实源**（禁脚本内独立 hard-coded 清单）；
- R2026a 坑清单（NFR-432）：数字开头薄入口 eval(fileread)；薄入口+包函数；From Workspace Interpolate off；禁链式索引；-batch 多行落临时 .m；
- 每任务完成立即 `spec_task_check`；关键结果主 Agent 独立复核（AGENTS.md §14）；
- **委派授权**（HANDOFF §5）：大规模生成/多文件实现类任务可委派 subagent（写 + 独立审查 crosscheck）；主 Agent 保留设计/决策/终验；
- **STOP 行为**：Gate FAIL（`error('GATE:*')`）/ 需改 Model E / 设计与实现实质冲突 / 数值证据与设计假设矛盾 → 停下升级用户裁决。

## 阶段总览

```text
Phase 0 注册表与架构（T-419 交付）: T-001 → T-002 → T-003（架构文档，AC-433）
Phase 1 只读可行性证据（设计冻结前置）: T-004（段时长普查）→ T-005（联合可行性扫描）→ T-006（C-3 可行性）
Phase 2 生成器核心（T-420~T-428）: T-007 seed 树 → T-008 环境 → T-009 工况/计划 → T-010 编译
   → T-011 预报 → T-012 K/F → T-013 干预层
Phase 3 导出与审计链: T-014 导出+合格判据 → T-015 stack identity → T-016 负向套件 → T-017 单测集成
Phase 4 预资格与交付: T-018 预资格（≈100）→ T-019 T-429 Gate → T-020 C/D Gate 就绪 → T-021 归档+PR
```

## Phase 0 — 注册表与架构（T-419）

- [ ] 1. 生成侧注册表四件 + validator
  - ID: T-001
  - Requirement(s): FR-512, FR-513, FR-514, FR-519, FR-528, FR-532；Design: DGN-02/03/04/05/06/07
  - 路线映射: T-427（模板槽位）/ T-428（DAG/seed 策略）
  - depends_on: 无（首批）
  - Files/areas: `model/simulink_modelE_(Phase4_Physics)/config/mission_v3/` 下 `mission_template_registry.json`（三 archetype：ROUTINE_PATROL/EMERGENCY_RESPONSE/WORK_MISSION，DGN-02 裁决落地）/ `conditional_dag_config.json`（DGN-03 节点表，河流坐标语义 R1）/ `environment_regime_params.json`（NORMAL/FLOOD/DRY 参数）/ `id_support_bounds.json`（边际包线+guard band 候选值，OOD 栏标 ACTIVATION_CANDIDATE）/ `forecast_quality_profiles.json`（GOOD_V1/DEGRADED_V1 + prehistory_s=1800）/ `seed_tree_policy.json`（分支清单+digest 映射规则）；`+mission_v3/validate_registries.m`（新建）
  - Scope: 六件注册表的字段设计按 design 对应 DGN；每件含 schema/version/self_hash（SHA256 canonical JSON，同 D 惯例）；validator 校验枚举/引用完整性/self-hash
  - Non-scope: 不写 strata/token/contrast 注册表（T-002）；不实现生成逻辑
  - Outputs: 六件 JSON + validator + 校验通过记录
  - Validation: validator 全过；负向：非法枚举/缺字段/篡改后 hash 不符必须 FAIL
  - Completion evidence: validator 输出 + 六件 hash 清单

- [ ] 2. strata / held-out token / identifiability contrast 注册表三件 + validator
  - ID: T-002
  - Requirement(s): FR-541~545, FR-550, FR-552~556；Design: DGN-11/12/13
  - 路线映射: T-428（OOD 面）/ T-429（contrast registry）
  - depends_on: T-001（stratum 引用 template/regime 枚举）
  - Files/areas: `config/mission_v3/scenario_stratum_registry.json`（9 格 ACTIVE，DGN-02 裁决记录入 reason 字段）/ `heldout_token_registry.json`（C1/C2/C3 谓词，id_action=EXCLUDE/ood_action=REQUIRE，物理可行性规则引用）/ `identifiability_contrast_registry.json`（R9 清理后六项必需对比 + 混杂控制声明；两项混轴对比标 SECONDARY_DIAGNOSTIC）；validator 扩展
  - Scope: token 谓词形式化（只消费生成侧因果 metadata 的字段白名单校验）；contrast registry 全字段（contrast_id/factor_axis/class_A/class_B/cohort_predicate/nuisance_controls/signal_view/SBD_required/probe_required/minimum_group_coverage=10 组）
  - Non-scope: 谓词执行引擎（T-008/T-018）；T-429 计算
  - Outputs: 三件 JSON + validator
  - Validation: validator 全过；谓词字段白名单负向（引用 realized 信号的谓词必须拒收）
  - Completion evidence: validator 输出

- [ ] 3. T-419 架构文档（先设计交付物）
  - ID: T-003
  - Requirement(s): FR-501, FR-513, FR-533, FR-534；AC-433；Design: DGN-01
  - 路线映射: T-419
  - depends_on: T-001, T-002
  - Files/areas: `docs/phase4/c_mission_v3/architecture.md`（新建）+ 目录 README
  - Scope: DGN-01~16 落为工程架构说明（模块边界/数据流/注册表清单/与 producer schema 十一项的接口映射/seed 树/stack identity）；明确 C→D→E/F 无旁路边界（Closeout B）
  - Non-scope: 不开始生成器施工
  - Outputs: 架构文档 + README 索引
  - Validation: 主 Agent 对照 requirements/design 逐项核对；**用户批准（AC-433）**
  - Completion evidence: 文档落库 + 用户批准记录

## Phase 1 — 只读可行性证据（设计冻结前置义务）

- [ ] 4. 段时长普查 → T-429 切片几何冻结提案
  - ID: T-004
  - Requirement(s): FR-547；Design: DGN-12（R8）
  - 路线映射: T-429
  - depends_on: T-001, T-002
  - Files/areas: `+mission_v3/audits/segment_duration_census.m`（新建）；`results/mission_v3/segment_duration_census.json`
  - Scope: 对 template registry 全部必需段角色计算 usable duration 分布（名义段长 × 拉伸因子下界）；验证 `slice + 2×buffer ≤ min usable duration`；输出切片几何冻结提案（起始评估候选 120/30/60）
  - Non-scope: 不执行 T-429 度量计算
  - Outputs: 普查 JSON + 冻结提案（主 Agent 裁决后回写 contrast registry/slice 配置）
  - Validation: 每条必需对比的段角色覆盖；提案满足硬约束的算术自检
  - Completion evidence: 普查报告 + 主 Agent 冻结记录（偏差登记若偏离候选）

- [ ] 5. 只读联合可行性矩阵扫描（ID 包线冻结前置）
  - ID: T-005
  - Requirement(s): FR-519, FR-520；Design: DGN-05（R2/R3）
  - 路线映射: T-428
  - depends_on: T-001
  - Files/areas: `+mission_v3/audits/feasibility_sweep.m`（新建）；`results/mission_v3/id_feasibility_sweep.json`
  - Scope: 只读扫描 planned SOG × 沿迹相对流 × H/T 网格上的 Model-E 可行性（推进需求 vs 执行器/Plant 约束、UKC、domain_warning）；重点高计划速度 × 高 adverse 流组合；H/T=1.50/1.35 边界点 sanity（若边界保留）；read-only——禁改 Model E 任何参数
  - Non-scope: 不修改支撑界数值（证据提交主 Agent 裁决）；不生成正式航次
  - Outputs: 可行性矩阵 JSON + 报告
  - Validation: 网格点全覆盖无 NaN；与 T-412 G2 包线证据交叉一致；若发现候选界内不可行组合 → 升级主 Agent（支撑界修订走 amendment）
  - Completion evidence: 扫描报告 + 主 Agent 对 ID 包线的冻结/修订裁决

- [ ] 6. C-3 物理可行性前置验证
  - ID: T-006
  - Requirement(s): FR-545；Design: DGN-11
  - 路线映射: T-428（OOD 面）
  - depends_on: T-005
  - Files/areas: 同 T-005 工具链扩展；`results/mission_v3/c3_feasibility.json`
  - Scope: EMERGENCY_SPRINT × LOW_ID_HT 组合的非零可行支撑证明（rush 段计划速度在低 H/T 下可达、无 domain_warning 聚集、SOP 不饱和）；read-only
  - Non-scope: 不得为保 C-3 改 Model E 或放宽物理约束；不可行 ⇒ 升级用户裁决（token 替换/删除需 spec amendment）
  - Outputs: 可行性证据 JSON
  - Validation: 证据满足 FR-545 三条件
  - Completion evidence: 报告 + 主 Agent 确认

## Phase 2 — 生成器核心（T-420~T-428）

- [ ] 7. 科学 seed 树（R6：full-digest 血统）
  - ID: T-007
  - Requirement(s): FR-522, FR-529, FR-532；Design: DGN-07
  - 路线映射: T-428
  - depends_on: T-001（seed_tree_policy.json）
  - Files/areas: `+mission_v3/seed_tree.m`（新建）+ `+mission_v3/tests/test_seed_tree.m`
  - Scope: lineage_id=完整 SHA256 branch-path digest（权威身份，全量存储）；digest→uint32 引擎映射（仅 Threefry 输入）；命名分支清单消费 seed_tree_policy.json；碰撞检测 fail-closed；INFORMATION/PHYSICAL 干预分支不变量实现
  - Non-scope: 生成逻辑调用方（T-008 起消费）
  - Outputs: 包函数 + 单测
  - Validation: 单测——同 lineage 同实现 / 异分支独立 / 构造碰撞必须检出 FAIL / digest 登记完整
  - Completion evidence: 单测 PASS 记录

- [ ] 8. 环境真值生成（河流坐标 OU + prehistory + 水位偏移 + 交通）
  - ID: T-008
  - Requirement(s): FR-508, FR-513, FR-514, FR-520；Design: DGN-03（R1）/04/06（R5）
  - 路线映射: T-423 / T-424
  - depends_on: T-007
  - Files/areas: `+mission_v3/dag_sample.m`、`+mission_v3/env_realize.m`（新建）+ 单测
  - Scope: L0~L3 条件抽取（DAG config 驱动）；河流坐标 OU 实现（t 自 −1800 s prehistory 起）；水位偏移逐航次常量；交通 Poisson 事件序列；支撑界/token 谓词的 reject/resample + 六类 disposition 记录（REJECTED_SAMPLING_ELIGIBILITY 含 SUPPORT_BOUND/PHYSICAL_DOMAIN_BOUND、REJECTED_STRUCTURAL_HELDOUT）；禁 clamp
  - Non-scope: 预报（T-011）；计划编译（T-009）
  - Outputs: 包函数 + 单测 + disposition 记录结构
  - Validation: 单测——OU 平稳性/自相关与 τ 一致；prehistory 长度 ≥1800 s；越界实现必 reject 且计数；往返段共享同一流实现（R1 不变量）
  - Completion evidence: 单测 PASS

- [ ] 9. 工况语义与计划层（T-420~T-427 施工核心）
  - ID: T-009
  - Requirement(s): FR-505~512, FR-515；Design: DGN-01/02
  - 路线映射: T-420 / T-421 / T-422 / T-425 / T-426 / T-427
  - depends_on: T-008
  - Files/areas: `+mission_v3/nav_constraints.m`、`+mission_v3/plan_compose.m`（新建）+ 单测
  - Scope: 七类任务级工况族语义编译——bridge（Approach→Decel→Controlled transit→Exit→Accel，"保持舵效的安全航速"语义，出桥 transient）/ harbor（长低速+小调整+低加速度，与 bridge 分离）/ shallow（UKC 限速由 nav supervisor 从 h 推导 v_limit）/ traffic（slowdown→follow→recover 注入计划层）/ emergency（standby→dispatch→rapid accel→high speed→mission，高速段用证据包线上段，FR-510 只读引用 T-412 G2）/ berthing 与 departure 非镜像分别设计；三 archetype 段序列组合（T-427）；operator profile  voyage 级 vs 段级约束分离（FR-515）；输出计划速度表 + speed_limit_zones/bridge_zones + 段角色 metadata（供 token 谓词与 T-429 标签）
  - Non-scope: 预报/K/F 快照（T-011/012）
  - Outputs: 包函数 + 单测
  - Validation: 单测——每工况计划剖面结构断言（段序/限速区/UKC）；bridge vs harbor 计划语义差异断言；emergency 高速段不越证据包线
  - Completion evidence: 单测 PASS + 计划剖面示例图（入报告）

- [ ] 10. 场景编译器（SCN 构造 + assignin 白名单注入）
  - ID: T-010
  - Requirement(s): FR-502, FR-516, FR-517；Design: DGN-08/09（R7 Path B）
  - 路线映射: T-420~T-427（落地层）
  - depends_on: T-009
  - Files/areas: `+mission_v3/scenario_compile.m`（新建）+ 单测
  - Scope: 计划/环境 → 运行时 SCN struct（时间模速度表 + 时间模流场表 + water_depth_m 标量=h_chart+ΔWL + hotel_kW 标量=plan base+δ_hotel + resistance_mult + initial_SOC + random_seed=base_draw lineage digest 派生）；VOYAGE_* / 注入通道经显式 assignin 白名单；PMS_SOURCE_MODE 显式设定；禁写回 ModelE_TestScenarios.m
  - Non-scope: 不修改任何 Model E 文件
  - Outputs: 包函数 + 单测
  - Validation: 单测——编译产物字段完备性对照既有 SCN schema；hotel 标量 = plan+δ 恒等式；水深 = h_chart+ΔWL 恒等式；注入白名单覆盖检查
  - Completion evidence: 单测 PASS + Model E 零改动 sha256 快照比对

- [ ] 11. 因果预报器（GOOD/DEGRADED）
  - ID: T-011
  - Requirement(s): FR-526~531；Design: DGN-06（R4/R5）
  - 路线映射: T-423（预报语义载体）
  - depends_on: T-008（prehistory）
  - Files/areas: `+mission_v3/forecast_issue.m`（新建）+ 单测
  - Scope: OU 条件均值解析式；GOOD=OBSERVED t₀ 状态；DEGRADED=lag 1800 s+bias+AR(1)（消费 prehistory，source_cutoff=t₀−1800）；provenance 六字段+forecast_quality_profile_id；observability source class 白名单断言（OBSERVED/CAUSALLY_ESTIMATED/STATIC_KNOWN_PARAMETER/PREDECLARED_MODEL_PARAMETER；SIMULATOR_LATENT_PRIVILEGED 拒收）；函数接口只接受 t≤t₀ 截断切片
  - Non-scope: K/F 布局组装（T-012）
  - Outputs: 包函数 + 单测
  - Validation: 单测——解析式与数值积分一致；cutoff≤issue 不变量；future-truth 注入必须检出；DEGRADED 误差时间相关性（非 iid jitter）统计断言
  - Completion evidence: 单测 PASS

- [ ] 12. K/F issue-major 快照构建（s_plan 计划前向积分）
  - ID: T-012
  - Requirement(s): FR-558~562；Design: DGN-08
  - 路线映射: T-431 接口的 C 侧实现
  - depends_on: T-009, T-011
  - Files/areas: `+mission_v3/kf_snapshots.m`（新建）+ 单测
  - Scope: issue 时刻计划位置 + 计划表前向积分（1 s 子步，bin 右端点取值语义同 pilot 惯例）；位置相关约束沿计划轨迹 interval-aware 采样；布局 (M×1)/(M×30) issue-major、lead∈{10..300}；缺快照=FAILED_CONTRACT 标记
  - Non-scope: truth stream 导出（T-014）
  - Outputs: 包函数 + 单测
  - Validation: 单测——s_plan 不含 realized future（与 realized 轨迹相关性负向断言）；网格严格 {10..300}；布局维度断言；与 export_pilot 布局逐字段一致
  - Completion evidence: 单测 PASS

- [ ] 13. L4 干预层与 grouping（family/base_draw/must-link）
  - ID: T-013
  - Requirement(s): FR-518, FR-521~525, FR-529；Design: DGN-01/07
  - 路线映射: T-428
  - depends_on: T-007, T-008
  - Files/areas: `+mission_v3/family_expand.m`（新建）+ 单测
  - Scope: 干预 matrix 生成前确定（intervention 注册）；PHYSICAL/INFORMATION 分型执行（PHYSICAL 仅改声明干预分支 lineage；INFORMATION 仅改 forecast_obs_noise 分支）；base_draw_id/family_id/split_group_id 派生（must-link 连通闭包算法）；same-truth hash 校验（GOOD/DEGRADED truth 逐位一致）；未声明跨族 truth 复用检出=Gate FAIL
  - Non-scope: split 分配本身（D 侧）
  - Outputs: 包函数 + 单测 + family 清单结构
  - Validation: 单测——闭包传递性；INFORMATION 成员 truth hash 相同；构造未声明复用必须检出；同模板异 draw 不合并（FR-525）
  - Completion evidence: 单测 PASS

## Phase 3 — 导出与审计链

- [ ] 14. producer 导出 + 合格判据/失败分类学
  - ID: T-014
  - Requirement(s): FR-501, FR-504, FR-535~540；Design: DGN-10/15
  - 路线映射: T-430 接口的 C 侧实现
  - depends_on: T-012, T-013
  - Files/areas: `+mission_v3/producer_export.m`、`+mission_v3/eligibility.m`（新建）+ 单测
  - Scope: artifact .mat v7 {stream,kf,meta}（对齐 export_pilot 布局 + C-D1 扩展 metadata 字段）；min_duration 1200/60 窗/unexpected_invalid=0 判定；expected/valid/invalid windows + reason counts；六类 disposition 全实现；FAILED slot retry 同 seed；formal fixture_kind=NONE 断言
  - Non-scope: 预资格主循环（T-018）
  - Outputs: 包函数 + 单测
  - Validation: 单测——判据边界（1199.99 s reject / 1200 accept）；窗计数算术（1590→100）；PILOT_ONLY 混入 formal 检出
  - Completion evidence: 单测 PASS

- [ ] 15. qualified generation-stack identity
  - ID: T-015
  - Requirement(s): FR-534；Design: DGN-14
  - 路线映射: T-432 解锁链支撑
  - depends_on: T-001, T-002
  - Files/areas: `+mission_v3/stack_identity.m`（新建）；`results/mission_v3/generation_stack_identity.json`
  - Scope: 十项组件 version+sha256 采集（含 Model E 基线五件 hash 快照复用 72 号机制）；复合 hash=SHA256(canonical JSON)；每次预资格/Gate 运行记录并比对
  - Non-scope: C/D Gate 本体（D 侧）
  - Outputs: 包函数 + identity JSON
  - Validation: 负向——改动任一注册表后复合 hash 必变；十项缺一必 FAIL
  - Completion evidence: identity JSON + 负向记录

- [ ] 16. 负向测试套件（AC-436 全清单）
  - ID: T-016
  - Requirement(s): FR-524, FR-526~533, FR-558~562；AC-436
  - 路线映射: 横切
  - depends_on: T-011~T-014
  - Files/areas: `+mission_v3/tests/test_negative.m`（新建）
  - Scope: AC-436 注入清单逐项：future-truth 冒充 INPUT_F / realized future 冒充 s_plan·v_plan / 沿 realized 位置取约束 / lead 网格偏移·vintage 反推 / 缺快照静默填补 / simulator-latent 预报源 / 未声明跨组 truth 复用 / GOOD-DEGRADED truth 漂移 / PILOT_ONLY 混 formal / FORBIDDEN·ORACLE 越权入 view
  - Outputs: 负向套件 + 全检出记录
  - Validation: 每项注入必须被对应机器检查检出（100% 检出率）
  - Completion evidence: 检出报告

- [ ] 17. 单元测试集成与确定性回归
  - ID: T-017
  - Requirement(s): NFR-428, NFR-432；AC-438
  - 路线映射: 横切
  - depends_on: T-007~T-016
  - Files/areas: `+mission_v3/tests/run_all.m`（新建）
  - Scope: 全部单测/负向一套入口；确定性两跑（同 seed+同配置 ⇒ 科学产物 payload hash 一致）；Model E 零改动快照比对入链
  - Outputs: 测试入口 + 两跑一致性记录
  - Validation: 全 PASS；两跑 hash 一致
  - Completion evidence: 测试报告

## Phase 4 — 预资格与交付

- [ ] 18. MISSION_V3_PREQUALIFICATION 执行（≈100 航次）
  - ID: T-018
  - Requirement(s): FR-534~540, FR-546, FR-557；AC-434/438/439；Design: DGN-10/13
  - 路线映射: T-428（生成器整体验证）
  - depends_on: T-014, T-015, T-017
  - Files/areas: `scripts/80_run_mission_v3_prequal.m`（薄入口，eval(fileread) 惯例）；`datasets_v3/mission_v3/prequal/`（本地不入库）；`results/mission_v3/prequal_strata_audit.json` + manifest
  - Scope: ≈100 航次端到端（工程 nominal）；七工况族+三 archetype 覆盖；FR-557 全字段机器可读 stratum 审计（含 14 组下限当前满足度投影——**不宣称 Stage-B 配额满足**）；拒绝率审计（FR-539）；失败分类学实战；FAILED slot 行为=记录→retry→继续→受控末汇 FAIL（R8）；确定性两跑；吞吐实测（Stage-B 成本估算输入）
  - Non-scope: 非 D/T-432 Stage A（法律分离，FR 措辞 R3）；T-429 Gate（T-019）
  - Outputs: 预资格产物 + 审计报告 + manifest
  - Validation: producer schema validator PASS；审计报告全字段；两跑 hash 一致；拒绝率无异常 stratum
  - Completion evidence: 报告 + 主 Agent 复核
  - Delegation: 可委派 subagent 执行批量生成（主 Agent 复核证据，HANDOFF §5 授权）

- [ ] 19. T-429 可辨识性 Gate
  - ID: T-019
  - Requirement(s): FR-547~551；AC-437；Design: DGN-12（R8/R9）
  - 路线映射: T-429
  - depends_on: T-004（切片几何冻结）, T-018（预资格语料）
  - Files/areas: `+mission_v3/audits/identifiability.m`（新建）；`scripts/81_run_mission_v3_t429.m`（薄入口）；`results/mission_v3/identifiability_*.json/md`
  - Scope: **Gate 首跑前盲冻结** R_min/A_min/探针编码/切片几何（冻结记录入报告；首跑后锁定）；SEGMENT_PURE_DIAGNOSTIC_SLICE 构建（段角色 metadata 标签；边界缓冲）；SBD 分离比（LCB₉₅≥R_min，group bootstrap 1000×split_group）；logistic 探针（split_group 5-fold、train-only scaler、LCB₉₅(BA)≥A_min）；六项必需对比逐一 PASS ∧ 才 overall=true；证据不足 fail-closed；混杂控制按 registry 声明执行
  - Non-scope: 阈值事后调整（FR-551 红线）；D-L3 语义（分离）
  - Outputs: Gate 机器可读结果 + 报告
  - Validation: overall=true；任一对比失败 ⇒ Gate FAIL 升级主 Agent（工况重设计路径）
  - Completion evidence: Gate 报告 + 主 Agent 复核

- [ ] 20. C/D Interface Gate 就绪包
  - ID: T-020
  - Requirement(s): FR-501, FR-534；AC-435；Design: DGN-16
  - 路线映射: T-432 解锁链第①②环
  - depends_on: T-018, T-019
  - Files/areas: `results/mission_v3/cd_gate_readiness.json`；D 侧交接说明更新（`docs/phase4/d_dataset_v3/c_producer_interface_contract.md` 标 C 实现完成状态）
  - Scope: C 产物对 frozen producer schema 的 conforming 自检（validate_contracts --producer-schema PASS）；stack identity 可比对证明；T-432 Unblock Checklist 第①项（C Mission-v3 完成）证据包；**不执行 D 侧正式 Gate**（D-15 必改后由 D 侧跑）
  - Outputs: 就绪包 JSON + 文档更新
  - Validation: schema validator PASS；就绪包字段完备
  - Completion evidence: 就绪包 + 主 Agent 复核

- [ ] 21. 归档与 PR
  - ID: T-021
  - Requirement(s): NFR-429, NFR-430, NFR-433；AC-440
  - 路线映射: 横切
  - depends_on: T-018~T-020
  - Files/areas: `docs/phase4/c_mission_v3/`（索引+报告归档）；相关 README 两级同步；`项目开发进展.md` 当日条目
  - Scope: README 两级同步（新增文件全登记）；绝对路径扫描零命中；dev log（含延伸问答记录）；spec 状态推进；C→Phase4 PR 创建交用户审批
  - Outputs: 归档 + PR
  - Validation: 扫描零命中；git status 干净（除既定不入库目录）
  - Completion evidence: PR 链接 + 日志条目

## 追溯总表

| 顶层任务 | 本 spec 任务 | 主要 FR | AC |
|---|---|---|---|
| T-419 架构设计 | T-001~T-003 | FR-501/512~514/533/534 | AC-433 |
| T-420~T-426 七工况族 | T-008~T-010 | FR-505~511/515~517 | AC-434 |
| T-427 模板 | T-001/T-009 | FR-512 | AC-434/439 |
| T-428 条件随机生成器 | T-007/T-008/T-013/T-018 | FR-513~525/532 | AC-434/438/439 |
| T-429 可辨识性 | T-004/T-019 | FR-547~551 | AC-437 |
| D 接口（producer schema） | T-011/T-012/T-014/T-020 | FR-501/526~531/558~562 | AC-435/436 |
| 横切（审计/seed/归档） | T-005/T-006/T-015~T-017/T-021 | FR-534~546/552~557 | AC-436/438/439/440 |

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-05 | tasks 初稿（T-001~T-021） | 本 session |

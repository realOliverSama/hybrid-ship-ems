# Phase 4-C — Mission v3（T-419~T-429）Tasks

Spec: `phase4-c-mission-v3`  
Status: tasks-approved  
Created: 2026-09-05  
Approved: 2026-09-05（CONDITIONAL PASS + R1~R8 修正闭合）  
Brainstorm: `./brainstorm.md`（C-D1~C-D7 + Closeout A/B/C 批准）  
Requirements: `./requirements.md`（requirements-approved + R1~R7 闭合）  
Design: `./design.md`（design-approved，DGN-01~16 + R1~R9 闭合）

> 任务编号 T-001~T-024 为本 spec 局部编号；每项标注路线文档 T-4xx 映射与 FR/DGN 追溯。
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
- **T-429 产物命名纪律**：一律 `identifiability_*`；永不复用 D-L3 `leakage_sbd_*` 语义/命名（FR-547/C-D6 §16）；
- 每任务完成立即 `spec_task_check`；关键结果主 Agent 独立复核（AGENTS.md §14）；
- **委派授权**（HANDOFF §5）：大规模生成/多文件实现类任务可委派 subagent（写 + 独立审查 crosscheck）；主 Agent 保留设计/决策/终验；
- **STOP 行为**：Gate FAIL（`error('GATE:*')`）/ 需改 Model E / 设计与实现实质冲突 / 数值证据与设计假设矛盾 → 停下升级用户裁决。

## 硬依赖屏障（Tasks Gate R1/R2/R3/R4/R6 冻结）

```text
PHASE2_ENTRY_GATE = T-003（架构文档用户批准）∧ T-007（Phase-1 冻结快照）
T-008 起的全部生成器任务 transitively 继承此屏障；
PREQUAL_PLAN_GATE   = T-019（预资格计划预注册 + qualification stack 捕获）先于 T-020 生成；
T429_BLIND_GATE     = T-021（不可变冻结 artifact + hash）先于 T-022 任何度量计算；
EXT-CD-GATE（外部里程碑，owner=Phase 4-D）：formal C/D Interface Gate overall=true
  ——C 不执行该 Gate；C spec 的 implementation-complete 须等待此里程碑（AC-435 保持 open 至其达成）。
```

## 阶段总览

```text
Phase 0 注册表与架构（T-419）: T-001 → T-002 → T-003（架构文档，用户批准 AC-433）
Phase 1 只读可行性证据: T-004 段时长普查 → T-005 联合可行性扫描 → T-006 C-3 可行性 → T-007 冻结快照
  【PHASE2_ENTRY_GATE】
Phase 2 生成器核心（T-420~T-428）: T-008 seed 树 → T-009 环境 → T-010 工况/计划 → T-011 编译
   → T-012 预报 → T-013 K/F → T-014 干预层
Phase 3 导出与审计链: T-015 导出+合格判据 → T-016 stack identity 实现 → T-017 负向套件 → T-018 单测集成
Phase 4 预资格与交付: T-019 预资格计划+stack 捕获 → T-020 预资格执行
   → T-021 T429_FREEZE → T-022 T429_EVALUATE → T-023 C/D 就绪包 → T-024 归档+PR
   【EXT-CD-GATE：D 侧正式 Gate 回来后 spec 方可 implementation-complete】
```

## Phase 0 — 注册表与架构（T-419）

- [x] 1. 生成侧注册表六件 + validator
  - ID: T-001
  - Requirement(s): FR-512, FR-513, FR-514, FR-519, FR-528, FR-532；Design: DGN-02/03/04/05/06/07
  - 路线映射: T-427（模板槽位）/ T-428（DAG/seed 策略）
  - depends_on: 无（首批）
  - Files/areas: `model/simulink_modelE_(Phase4_Physics)/config/mission_v3/` 下 `mission_template_registry.json`（三 archetype：ROUTINE_PATROL/EMERGENCY_RESPONSE/WORK_MISSION，DGN-02 裁决落地）/ `conditional_dag_config.json`（DGN-03 节点表，河流坐标语义 R1）/ `environment_regime_params.json`（NORMAL/FLOOD/DRY 参数）/ `id_support_bounds.json`（边际包线+guard band **候选值**，OOD 栏标 ACTIVATION_CANDIDATE）/ `forecast_quality_profiles.json`（GOOD_V1/DEGRADED_V1 + prehistory_s=1800）/ `seed_tree_policy.json`（分支清单+digest 映射规则）；`+mission_v3/validate_registries.m`（新建）
  - Scope: 六件注册表的字段设计按 design 对应 DGN；每件含 schema/version/self_hash（SHA256 canonical JSON，同 D 惯例）；validator 校验枚举/引用完整性/self-hash。**本任务产出为候选态注册表**——数值冻结经 Phase 1 证据 + T-007 快照生效
  - Non-scope: 不写 strata/token/contrast 注册表（T-002）；不实现生成逻辑
  - Outputs: 六件 JSON（候选态）+ validator + 校验通过记录
  - Validation: validator 全过；负向：非法枚举/缺字段/篡改后 hash 不符必须 FAIL
  - Completion evidence: validator 输出 + 六件 hash 清单

- [x] 2. strata / held-out token / identifiability contrast 注册表三件 + validator 扩展
  - ID: T-002
  - Requirement(s): FR-541~545, FR-550, FR-552~556；Design: DGN-11/12/13
  - 路线映射: T-428（OOD 面）/ T-429（contrast registry）
  - depends_on: T-001（stratum 引用 template/regime 枚举）
  - Files/areas: `config/mission_v3/scenario_stratum_registry.json`（9 格 ACTIVE，DGN-02 裁决记录入 reason 字段）/ `heldout_token_registry.json`（C1/C2/C3 谓词，id_action=EXCLUDE/ood_action=REQUIRE）/ `identifiability_contrast_registry.json`（R9 清理后六项必需对比 + 混杂控制声明；两项混轴对比标 SECONDARY_DIAGNOSTIC）；validator 扩展
  - Scope: token 谓词形式化（只消费生成侧因果 metadata 的字段白名单校验）；contrast registry 全字段（contrast_id/factor_axis/class_A/class_B/cohort_predicate/nuisance_controls/signal_view/SBD_required/probe_required/minimum_group_coverage=10 组）；**候选态**（minimum_group_coverage 等经 T-004 证据确认后由 T-007 冻结）
  - Non-scope: 谓词执行引擎（T-009/T-020）；T-429 计算
  - Outputs: 三件 JSON + validator
  - Validation: validator 全过；谓词字段白名单负向（引用 realized 信号的谓词必须拒收）
  - Completion evidence: validator 输出

- [x] 3. T-419 架构文档（先设计交付物）
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

## Phase 1 — 只读可行性证据（PHASE2_ENTRY_GATE 前置）

- [x] 4. 段时长普查 → T-429 切片几何冻结提案
  - ID: T-004
  - Requirement(s): FR-547；Design: DGN-12（R8）
  - 路线映射: T-429
  - depends_on: T-001, T-002
  - Files/areas: `+mission_v3/audits/segment_duration_census.m`（新建）；`results/mission_v3/segment_duration_census.json`
  - Scope: 对 template registry 全部必需段角色计算 usable duration 分布（名义段长 × 拉伸因子下界）；验证 `slice + 2×buffer ≤ min usable duration` 对每条必需对比成立；输出切片几何冻结提案（起始评估候选 120/30/60）
  - Non-scope: 不执行 T-429 度量计算
  - Outputs: 普查 JSON + 冻结提案（主 Agent 裁决后经 T-007 冻结）
  - Validation: 每条必需对比的段角色覆盖；提案满足硬约束的算术自检
  - Completion evidence: 普查报告 + 主 Agent 裁决记录

- [x] 5. 只读联合可行性矩阵扫描（ID 包线冻结前置）
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

- [x] 6. C-3 物理可行性前置验证
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

- [x] 7. Phase-1 配置冻结快照（PHASE2_ENTRY_GATE 组成）
  - ID: T-007
  - Requirement(s): FR-519, FR-545, FR-547；Design: DGN-05/11/12；Tasks Gate R2
  - 路线映射: 横切（屏障任务）
  - depends_on: T-004, T-005, T-006
  - Files/areas: `results/mission_v3/phase1_freeze.json`（新建）；受影响的 `config/mission_v3/*.json` 修订
  - Scope: 证据 → 必要的 config/registry 修订（主 Agent 裁决）→ validator PASS → self_hash 刷新 → **机器可读冻结快照**，至少记录：最终切片几何配置引用/hash、最终 design 侧 IID 支撑 registry hash、联合可行性证据 hash、C-3 可行性状态/证据 hash、token/support registry hash、适用 amendment ID、批准/版本信息
  - Non-scope: 不开始生成器施工
  - Outputs: phase1_freeze.json + 冻结后注册表
  - Validation: 快照字段完备；validator 全过；自此 T-008 起只消费快照指向的冻结配置（"盘上候选注册表"与"证据批准注册表"不得混用）
  - Completion evidence: phase1_freeze.json + 主 Agent 确认

## Phase 2 — 生成器核心（T-420~T-428）【PHASE2_ENTRY_GATE 后】

- [x] 8. 科学 seed 树（full-digest 血统）
  - ID: T-008
  - Requirement(s): FR-522, FR-529, FR-532；Design: DGN-07（R6）
  - 路线映射: T-428
  - depends_on: T-003, T-007
  - Files/areas: `+mission_v3/seed_tree.m`（新建）+ `+mission_v3/tests/test_seed_tree.m`
  - Scope: lineage_id=完整 SHA256 branch-path digest（权威身份，全量存储）；digest→uint32 引擎映射（仅 Threefry 输入，非科学身份）；命名分支清单消费冻结 seed_tree_policy.json；碰撞检测 fail-closed（不同 branch path→同一有效 RNG 状态 = FAIL）；INFORMATION/PHYSICAL 干预分支不变量实现
  - Non-scope: 生成逻辑调用方（T-009 起消费）
  - Outputs: 包函数 + 单测
  - Validation: 单测——同 lineage 同实现 / 异分支独立 / 构造碰撞必须检出 FAIL / digest 登记完整
  - Completion evidence: 单测 PASS 记录

- [x] 9. 环境真值生成（河流坐标 OU + prehistory + 水位偏移 + 交通）
  - ID: T-009
  - Requirement(s): FR-508, FR-513, FR-514, FR-520；Design: DGN-03（R1）/04/06（R5）
  - 路线映射: T-423 / T-424
  - depends_on: T-008
  - Files/areas: `+mission_v3/dag_sample.m`、`+mission_v3/env_realize.m`（新建）+ 单测
  - Scope: L0~L3 条件抽取（冻结 DAG config 驱动）；河流坐标 OU 实现（t 自 −1800 s prehistory 起）；水位偏移逐航次常量；交通 Poisson 事件序列；支撑界/token 谓词的 reject/resample + 六类 disposition 记录（REJECTED_SAMPLING_ELIGIBILITY 含 SUPPORT_BOUND/PHYSICAL_DOMAIN_BOUND、REJECTED_STRUCTURAL_HELDOUT）；禁 clamp
  - Non-scope: 预报（T-012）；计划编译（T-010）
  - Outputs: 包函数 + 单测 + disposition 记录结构
  - Validation: 单测——OU 平稳性/自相关与 τ 一致；prehistory 长度 ≥1800 s；越界实现必 reject 且计数；往返段共享同一流实现（R1 不变量）
  - Completion evidence: 单测 PASS

- [ ] 10. 工况语义与计划层（T-420~T-427 施工核心）
  - ID: T-010
  - Requirement(s): FR-505~512, FR-515；Design: DGN-01/02
  - 路线映射: T-420 / T-421 / T-422 / T-425 / T-426 / T-427
  - depends_on: T-009
  - Files/areas: `+mission_v3/nav_constraints.m`、`+mission_v3/plan_compose.m`（新建）+ 单测
  - Scope: 七类任务级工况族语义编译——bridge（Approach→Decel→Controlled transit→Exit→Accel，"保持舵效的安全航速"，出桥 transient）/ harbor（长低速+小调整+低加速度，与 bridge 分离）/ shallow（UKC 限速由 nav supervisor 从 h 推导 v_limit）/ traffic（slowdown→follow→recover 注入计划层）/ emergency（standby→dispatch→rapid accel→high speed→mission，高速段用证据包线上段，FR-510 只读引用 T-412 G2 value_kn=14.620）/ berthing 与 departure 非镜像分别设计；三 archetype 段序列组合（T-427）；operator profile voyage 级 vs 段级约束分离（FR-515）；输出计划速度表 + speed_limit_zones/bridge_zones + 段角色 metadata（供 token 谓词与 T-429 标签）
  - Non-scope: 预报/K/F 快照（T-012/013）
  - Outputs: 包函数 + 单测
  - Validation: 单测——每工况计划剖面结构断言（段序/限速区/UKC）；bridge vs harbor 计划语义差异断言；emergency 高速段不越证据包线
  - Completion evidence: 单测 PASS + 计划剖面示例图（入报告）

- [ ] 11. 场景编译器（SCN 构造 + assignin 白名单注入）
  - ID: T-011
  - Requirement(s): FR-502, FR-516, FR-517；Design: DGN-08/09（R7 Path B）
  - 路线映射: T-420~T-427（落地层）
  - depends_on: T-010
  - Files/areas: `+mission_v3/scenario_compile.m`（新建）+ 单测
  - Scope: 计划/环境 → 运行时 SCN struct（时间模速度表 + 时间模流场表 + water_depth_m 标量=h_chart+ΔWL + hotel_kW 标量=plan base+δ_hotel + resistance_mult + initial_SOC + random_seed=base_draw lineage digest 派生）；VOYAGE_* / 注入通道经显式 assignin 白名单；PMS_SOURCE_MODE 显式设定；禁写回 ModelE_TestScenarios.m
  - Non-scope: 不修改任何 Model E 文件
  - Outputs: 包函数 + 单测
  - Validation: 单测——编译产物字段完备性对照既有 SCN schema；hotel 标量 = plan+δ 恒等式；水深 = h_chart+ΔWL 恒等式；注入白名单覆盖检查
  - Completion evidence: 单测 PASS + Model E 零改动 sha256 快照比对

- [ ] 12. 因果预报器（GOOD/DEGRADED）
  - ID: T-012
  - Requirement(s): FR-526~531；Design: DGN-06（R4/R5）
  - 路线映射: T-423（预报语义载体）
  - depends_on: T-009（prehistory）
  - Files/areas: `+mission_v3/forecast_issue.m`（新建）+ 单测
  - Scope: OU 条件均值解析式；GOOD=OBSERVED t₀ 状态；DEGRADED=lag 1800 s+bias+AR(1)（消费 prehistory，source_cutoff=t₀−1800）；provenance 六字段+forecast_quality_profile_id；observability source class 白名单断言（OBSERVED/CAUSALLY_ESTIMATED/STATIC_KNOWN_PARAMETER/PREDECLARED_MODEL_PARAMETER；SIMULATOR_LATENT_PRIVILEGED 拒收）；函数接口只接受 t≤t₀ 截断切片
  - Non-scope: K/F 布局组装（T-013）
  - Outputs: 包函数 + 单测
  - Validation: 单测——解析式与数值积分一致；cutoff≤issue 不变量；future-truth 注入必须检出；DEGRADED 误差时间相关性（非 iid jitter）统计断言
  - Completion evidence: 单测 PASS

- [ ] 13. K/F issue-major 快照构建（s_plan 计划前向积分）
  - ID: T-013
  - Requirement(s): FR-558~562；Design: DGN-08；Tasks Gate R7
  - 路线映射: T-431 接口的 C 侧实现
  - depends_on: T-010, T-012
  - Files/areas: `+mission_v3/kf_snapshots.m`（新建）+ 单测
  - Scope: issue 时刻计划位置 + 计划表前向积分（1 s 子步，bin 右端点取值语义同 pilot 惯例）；位置相关约束沿计划轨迹 interval-aware 采样；布局 (M×1)/(M×30) issue-major、lead∈{10..300}；缺快照=FAILED_CONTRACT 标记；**API 边界**：kf_snapshots 函数签名不接收无界 future-realization 数组（只接受计划对象 + issue 时刻状态）
  - Non-scope: truth stream 导出（T-015）
  - Outputs: 包函数 + 单测
  - Validation: 单测——**因果不变量测试（R7，替代相关性断言）**：固定 issue 时刻计划与全部合法 K 输入，变异 t₀ 之后的 realized 未来轨迹，重建 s_plan/INPUT_K 必须逐位不变（Δactual_future ⇏ Δs_plan）；网格严格 {10..300}；布局维度断言；与 export_pilot 布局逐字段一致；**计划与实现的高统计相关性不得作为泄漏证据**
  - Completion evidence: 单测 PASS

- [ ] 14. L4 干预层与 grouping（family/base_draw/must-link）
  - ID: T-014
  - Requirement(s): FR-518, FR-521~525, FR-529；Design: DGN-01/07
  - 路线映射: T-428
  - depends_on: T-008, T-009
  - Files/areas: `+mission_v3/family_expand.m`（新建）+ 单测
  - Scope: 干预 matrix 生成前确定（intervention 注册）；PHYSICAL/INFORMATION 分型执行（PHYSICAL 仅改声明干预分支 lineage；INFORMATION 仅改 forecast_obs_noise 分支）；base_draw_id/family_id/split_group_id 派生（must-link 连通闭包算法）；same-truth hash 校验（GOOD/DEGRADED truth 逐位一致）；未声明跨族 truth 复用检出=Gate FAIL
  - Non-scope: split 分配本身（D 侧）
  - Outputs: 包函数 + 单测 + family 清单结构
  - Validation: 单测——闭包传递性；INFORMATION 成员 truth hash 相同；构造未声明复用必须检出；同模板异 draw 不合并（FR-525）
  - Completion evidence: 单测 PASS

## Phase 3 — 导出与审计链

- [ ] 15. producer 导出 + 合格判据/失败分类学
  - ID: T-015
  - Requirement(s): FR-501, FR-504, FR-535~540；Design: DGN-10/15
  - 路线映射: T-430 接口的 C 侧实现
  - depends_on: T-013, T-014
  - Files/areas: `+mission_v3/producer_export.m`、`+mission_v3/eligibility.m`（新建）+ 单测
  - Scope: artifact {stream,kf,meta} 布局对齐 export_pilot + C-D1 扩展 metadata 字段；**artifact 表示格式消费 D producer 契约冻结值**（v1=.mat v7 per D design 偏差 D-2；经 D 侧 registry/schema 常量引用，本任务层不得重述/更改格式假设）；min_duration 1200/60 窗/unexpected_invalid=0 判定；expected/valid/invalid windows + reason counts；六类 disposition 全实现；FAILED slot retry 同 seed；formal fixture_kind=NONE 断言
  - Non-scope: 预资格主循环（T-020）
  - Outputs: 包函数 + 单测
  - Validation: 单测——判据边界（1199.99 s reject / 1200 accept）；窗计数算术（1590→100）；PILOT_ONLY 混入 formal 检出
  - Completion evidence: 单测 PASS

- [ ] 16. stack identity 实现与测试（非最终捕获）
  - ID: T-016
  - Requirement(s): FR-534；Design: DGN-14；Tasks Gate R6
  - 路线映射: T-432 解锁链支撑
  - depends_on: T-001, T-002
  - Files/areas: `+mission_v3/stack_identity.m`（新建）+ 单测
  - Scope: 十项组件 version+sha256 采集机制（含 Model E 基线五件 hash 快照复用 72 号机制）；复合 hash=SHA256(canonical JSON)；**明确测试专用文件是否计入 mission_v3_generator 复合 hash 域（须显式裁决并写入实现注释/manifest 字段）**；缺组件 FAIL 与篡改检出测试。**本任务不产出最终 qualified stack identity**——最终捕获在 T-019（代码冻结后）
  - Non-scope: qualification_stack_identity.json 捕获（T-019）
  - Outputs: 包函数 + 单测
  - Validation: 负向——改动任一注册表后复合 hash 必变；十项缺一必 FAIL
  - Completion evidence: 单测 PASS + 测试文件归属裁决记录

- [ ] 17. 负向测试套件（AC-436 全清单）
  - ID: T-017
  - Requirement(s): FR-524, FR-526~533, FR-558~562；AC-436
  - 路线映射: 横切
  - depends_on: T-012~T-015
  - Files/areas: `+mission_v3/tests/test_negative.m`（新建）
  - Scope: AC-436 注入清单逐项：future-truth 冒充 INPUT_F / realized future 冒充 s_plan·v_plan / 沿 realized 位置取约束 / lead 网格偏移·vintage 反推 / 缺快照静默填补 / simulator-latent 预报源 / 未声明跨组 truth 复用 / GOOD-DEGRADED truth 漂移 / PILOT_ONLY 混 formal / FORBIDDEN·ORACLE 越权入 view
  - Outputs: 负向套件 + 全检出记录
  - Validation: 每项注入必须被对应机器检查检出（100% 检出率）
  - Completion evidence: 检出报告

- [ ] 18. 单元测试集成与确定性回归
  - ID: T-018
  - Requirement(s): NFR-428, NFR-432；AC-438
  - 路线映射: 横切
  - depends_on: T-008~T-017
  - Files/areas: `+mission_v3/tests/run_all.m`（新建）
  - Scope: 全部单测/负向一套入口；确定性两跑（同 seed+同配置 ⇒ 科学产物 payload hash 一致）；Model E 零改动快照比对入链
  - Outputs: 测试入口 + 两跑一致性记录
  - Validation: 全 PASS；两跑 hash 一致
  - Completion evidence: 测试报告

## Phase 4 — 预资格与交付

- [ ] 19. 预资格计划预注册 + qualification stack 捕获（PREQUAL_PLAN_GATE）
  - ID: T-019
  - Requirement(s): FR-534, FR-537, FR-539, FR-557；Design: DGN-10/13/14；Tasks Gate R4/R5/R6
  - 路线映射: T-428
  - depends_on: T-018（全部生成器代码/配置/测试冻结后）
  - Files/areas: `config/mission_v3/prequal_plan.json`（新建）；`results/mission_v3/qualification_stack_identity.json`（捕获）
  - Scope: **生成前预注册**预资格计划（机器可读）：精确 base-draw/split-group 数（≈100 量级但执行值精确）、template×regime 配额、必需对比/cohort 覆盖目标、direction/operator/intervention 覆盖、family 倍数、seed root/生成 seed 计划、合法抽样 retry 策略、拒绝策略、**预资格拒绝率合格阈值（R5：首个生成 run 前冻结，禁看分布后选定）**；随后捕获 qualification_stack_identity.json（T-016 机制的正式捕获，入预资格 manifest）
  - Non-scope: 非 T-432 formal 样本量预注册（C 工程预资格）；不执行生成
  - Outputs: prequal_plan.json + qualification_stack_identity.json
  - Validation: 计划字段完备；stack identity 与 T-018 末态代码/配置一致；阈值冻结记录先于任何生成
  - Completion evidence: 两件 JSON + 主 Agent 批准计划

- [ ] 20. MISSION_V3_PREQUALIFICATION 执行
  - ID: T-020
  - Requirement(s): FR-534~540, FR-546, FR-557；AC-434/438/439；Design: DGN-10/13；Tasks Gate R3/R4/R6
  - 路线映射: T-428（生成器整体验证）
  - depends_on: T-019
  - Files/areas: `scripts/80_run_mission_v3_prequal.m`（薄入口，eval(fileread) 惯例）；`datasets_v3/mission_v3/prequal/`（本地不入库）；`results/mission_v3/prequal_strata_audit.json` + manifest
  - Scope: 按 T-019 冻结计划端到端生成；七工况族+三 archetype 覆盖；FR-557 全字段机器可读 stratum 审计（含 14 组下限当前满足度投影——**不宣称 Stage-B 配额满足**）；拒绝率按 T-019 冻结阈值判定（FR-539）；失败分类学实战（FAILED slot=记录→同 seed retry→继续→受控末汇 FAIL，R8/design Error Handling）；确定性两跑；吞吐实测（Stage-B 成本估算输入）。**禁止事项（R3/R4）**：不得计算或暴露任何 T-429 结果度量（SBD/BA/置信界/混淆矩阵）——仅可报告非结果就绪信息（组计数/类覆盖/段时长覆盖/fold 可行性/缺数/schema 诊断）；不得看结果后增补航次（计划修订须显式登记的 amendment）
  - Non-scope: 非 D/T-432 Stage A（法律分离）；T-429 Gate（T-021/022）
  - Outputs: 预资格产物 + 审计报告 + manifest（含 qualification stack identity）
  - Validation: producer schema validator PASS；审计报告全字段；两跑 hash 一致；拒绝率按冻结阈值 PASS/FAIL 明确；**预资格 manifest 中 T-429 结果度量零存在**
  - Completion evidence: 报告 + 主 Agent 复核
  - Delegation: 可委派 subagent 执行批量生成（主 Agent 复核证据，HANDOFF §5 授权）

- [ ] 21. T429_FREEZE（盲冻结 artifact，先于一切度量计算）
  - ID: T-021
  - Requirement(s): FR-547, FR-549, FR-550；Design: DGN-12；Tasks Gate R3
  - 路线映射: T-429
  - depends_on: T-004（切片几何冻结）, T-020（预资格语料 manifest）
  - Files/areas: `results/mission_v3/t429_freeze.json`（新建，不可变）
  - Scope: **首个 SBD/探针结果计算前**写入并 hash 的不可变冻结 artifact：R_min、A_min、切片时长/边界缓冲/stride、探针特征编码、探针预处理/超参、分组 CV 规则、bootstrap 规则与 bootstrap seed、required contrast registry hash、nuisance-control 配置 hash、预资格语料 manifest hash、qualified generation-stack hash、T-429 实现版本/hash；记录 `frozen_before_metric_evaluation=true` + freeze SHA256
  - Non-scope: 任何度量计算
  - Outputs: t429_freeze.json
  - Validation: artifact 字段完备 + hash 链完整；写入后其引用对象任何变更 ⇒ 后续 Gate 结果无效（机器校验）
  - Completion evidence: freeze artifact + 主 Agent 确认

- [ ] 22. T429_EVALUATE（可辨识性 Gate 执行）
  - ID: T-022
  - Requirement(s): FR-547~551；AC-437；Design: DGN-12（R8/R9）；Tasks Gate R3
  - 路线映射: T-429
  - depends_on: T-021
  - Files/areas: `+mission_v3/audits/identifiability.m`（新建）；`scripts/81_run_mission_v3_t429.m`（薄入口）；`results/mission_v3/identifiability_*.json/md`
  - Scope: SEGMENT_PURE_DIAGNOSTIC_SLICE 构建（段角色 metadata 标签；冻结切片几何）；SBD 分离比（LCB₉₅(R)≥R_min，group bootstrap 1000×split_group，冻结 bootstrap seed）；logistic 探针（split_group 5-fold、train-only scaler、LCB₉₅(BA)≥A_min）；六项必需对比逐一 PASS ∧ 才 overall=true；证据不足 fail-closed（INSUFFICIENT_IDENTIFIABILITY_EVIDENCE）；混杂控制按 registry 声明执行；**执行前机器校验 t429_freeze.json 存在且其引用对象零变更**
  - Non-scope: 阈值事后调整（FR-551 红线）；D-L3 语义（分离）
  - Outputs: Gate 机器可读结果 + 报告
  - Validation: overall=true；任一对比失败 ⇒ Gate FAIL 升级主 Agent（工况重设计路径）
  - Completion evidence: Gate 报告 + 主 Agent 复核

- [ ] 23. C/D Interface Gate 就绪包
  - ID: T-023
  - Requirement(s): FR-501, FR-534；AC-435；Design: DGN-16；Tasks Gate R8
  - 路线映射: T-432 解锁链第①②环
  - depends_on: T-020, T-022
  - Files/areas: `results/mission_v3/cd_gate_readiness.json`；D 侧交接说明更新（`docs/phase4/d_dataset_v3/c_producer_interface_contract.md` 标 C 实现完成状态）
  - Scope: C 产物对 frozen producer schema 的 conforming 自检（validate_contracts --producer-schema PASS）；stack identity 可比对证明（T-020/T-022 同一冻结身份）；T-432 Unblock Checklist 第①项（C Mission-v3 完成）证据包；**不执行 D 侧正式 Gate**（D-15 必改后由 D 侧跑）；**readiness ≠ Gate PASS**——就绪包明确标注 EXT-CD-GATE 待达
  - Outputs: 就绪包 JSON + 文档更新
  - Validation: schema validator PASS；就绪包字段完备
  - Completion evidence: 就绪包 + 主 Agent 复核

- [ ] 24. 归档与 PR（spec 收官受 EXT-CD-GATE 约束）
  - ID: T-024
  - Requirement(s): NFR-429, NFR-430, NFR-433；AC-440；Tasks Gate R8
  - 路线映射: 横切
  - depends_on: T-020~T-023
  - Files/areas: `docs/phase4/c_mission_v3/`（索引+报告归档）；相关 README 两级同步；`项目开发进展.md` 当日条目
  - Scope: README 两级同步（新增文件全登记）；绝对路径扫描零命中；dev log（含延伸问答记录）；C→Phase4 PR 创建交用户审批；**spec 状态推进至 EXT-CD-GATE 为止**：AC-435 保持 open、spec 不升 implementation-complete，直至 D-owned formal C/D Interface Gate 返回 overall=true（外部里程碑 EXT-CD-GATE，owner=Phase 4-D）
  - Outputs: 归档 + PR
  - Validation: 扫描零命中；git status 干净（除既定不入库目录）
  - Completion evidence: PR 链接 + 日志条目

## 外部里程碑

- **EXT-CD-GATE**（owner: Phase 4-D；depends_on: T-023）：formal C/D Interface Gate overall=true。C 包不执行该 Gate；C spec 的 implementation-complete 以此里程碑达成为前提。达成前 C 侧完成态 = "C 施工完成 + 就绪包交付 + PR 待批"。

## 追溯总表

| 顶层任务 | 本 spec 任务 | 主要 FR | AC |
|---|---|---|---|
| T-419 架构设计 | T-001~T-003 | FR-501/512~514/533/534 | AC-433 |
| T-420~T-426 七工况族 | T-009~T-011 | FR-505~511/515~517 | AC-434 |
| T-427 模板 | T-001/T-010 | FR-512 | AC-434/439 |
| T-428 条件随机生成器 | T-008/T-009/T-014/T-019/T-020 | FR-513~525/532 | AC-434/438/439 |
| T-429 可辨识性 | T-004/T-021/T-022 | FR-547~551 | AC-437 |
| D 接口（producer schema） | T-012/T-013/T-015/T-023 | FR-501/526~531/558~562 | AC-435/436 |
| 横切（可行性/审计/seed/归档） | T-005~T-007/T-016~T-018/T-024 | FR-534~546/552~557 | AC-436/438/439/440 |

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-05 | tasks 初稿（T-001~T-021） | 本 session |
| 2026-09-05 | tasks 批准（CONDITIONAL PASS + R1~R8 闭合）：R1 PHASE2_ENTRY_GATE 硬依赖屏障（T-008 起传递继承）；R2 新增 T-007 Phase-1 冻结快照（phase1_freeze.json，候选/批准注册表消歧）；R3 T-429 拆 T-021 FREEZE/T-022 EVALUATE 两任务（不可变冻结 artifact 先于一切度量计算；T-020 禁出结果度量）；R4 新增 T-019 预资格计划预注册（精确配额/阈值/seed 计划，禁看结果增补）；R5 拒绝率阈值生成前冻结；R6 T-016 实现与 T-019 最终捕获分离 + 测试文件 hash 域裁决；R7 T-013 因果不变量测试替代相关性断言（Δactual_future⇏Δs_plan + API 边界）；R8 新增外部里程碑 EXT-CD-GATE（implementation-complete 前提）；小修正：T-001 标题四件→六件、artifact 格式改引 D 契约冻结值、T-429 命名纪律入执行约束；任务总数 21→24 | 用户 tasks gate 评审 |

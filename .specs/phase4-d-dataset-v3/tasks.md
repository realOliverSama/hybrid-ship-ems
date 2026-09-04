# Phase 4-D 数据集 v3（T-430~T-435）Tasks

Spec: `phase4-d-dataset-v3`  
Status: tasks-approved  
Created: 2026-09-04  
Approved: 2026-09-04（CONDITIONAL PASS + R1~R7 修正闭合）  
Brainstorm: `./brainstorm.md`（D-D1~D-D9 批准）  
Requirements: `./requirements.md`（requirements-approved + R1~R5 闭合 + FR-485 最小修订）  
Design: `./design.md`（design-approved，DGN-01~DGN-17 + R1~R8 闭合）

> 任务编号 T-001~T-021 为本 spec 局部编号；每项标注路线文档 T-4xx 映射与 FR/DGN 追溯。
> 顶层任务所有权保持路线文档 T-430~T-435（本文件不另立顶层编号）。
> **执行 session 开工前必须研读**：brainstorm.md、requirements.md、design.md、
> `docs/phase4/d_dataset_v3_HANDOFF.md`、Model E 工作区 README_Model.md、
> `项目开发进展.md` 2026-09-04 条目（R2026a 工程坑清单）。
> **全部契约/数值/规则以 requirements.md + design.md 为唯一事实源，禁止凭记忆施工。**

## 执行约束（全任务通用）

- 分支 `feat/phase4-d-dataset-v3`；逐文件 add；禁 merge 至 Phase4（待用户 PR 审批）；Git 红线全适用；
- **Model E 物理/控制零改动**（NFR-420）：Plant/EMS/PMS/Safety 参数、模型结构、Gate 阈值、`ModelE_Physics_Parameters.m`、`ModelE_Control_Parameters.m` 全部禁改；formal 生成基线 = 物理 v1.1 + 控制 v1.0；A2 historical v1.0 仅 immutable 参考资产；
- **Legacy 冻结**（FR-492/493）：03/05/30/60/66、`python/`、`datasets/`、`legacy_reference/` 不删不搬不改行为，只加 README superseded 标记；
- **hygiene 纪律**（NFR-426）：T-005/T-006 为 validation infrastructure hygiene / prerequisite maintenance，不得用于调节任何冻结参数凑 PASS；
- **禁绝对路径**（NFR-422）：MATLAB 用 mfilename+pwd 兜底、Python 用 `__file__` 推导；
- **机器契约唯一事实源**（FR-465）：role/aggregation/白名单一律消费 registry，禁脚本内独立 hard-coded list；
- 每任务完成立即 `spec_task_check`；关键结果主 Agent 独立复核（AGENTS.md §14）；
- **STOP 行为**：Gate FAIL（`error('GATE:*')`）/ 需改 Model E 模型或冻结参数 / 设计与实现实质冲突 → 停下升级用户裁决，不得自行解决。

## 阶段总览

```text
D0（本 spec 立即可施工）:
  T-430 观测契约 → T-431 未来信息契约 → 卫生前置 → T-433/T-434 管线 → T-435 pilot qualification
  → D0 COMPLETE（T-432 BLOCKED，spec 保持 implementation-in-progress）
Formal（C 包就绪后，需 T-432 Unblock Checklist 全绿）:
  C/D Interface Gate → T-432 Stage A → T-432 Stage B → T-435-formal → Dataset-v3 Freeze
```

## T-430 — 观测变量契约（FR-461）

- [ ] 1. signal/role registry + registry 校验器
  - ID: T-001
  - Requirement(s): FR-461, FR-464, FR-465, FR-470；Design: DGN-01/02/05/06
  - depends_on: 无（首批）
  - Files/areas: `config/dataset_v3_signal_registry.json`（新建）、`dataset_v3/validate_contracts.py`（新建）
  - Scope: 按 design DGN-02 字段集（name/unit/role/source_signal/temporal_domain/aggregation/issue_time_semantics/predictor_access）编写全部 H1/H2/H3 映射条目（H1→INPUT_H1、H2→INPUT_H2、PMS audit/EMS 内部→FORBIDDEN、scenario_id→METADATA、resistance_multiplier→METADATA metadata_only）；aggregation 枚举按 DGN-05 冻结表；validator 校验枚举合法/角色约束/H1H2 清单与 FR-461 一致/self-hash=SHA256(canonicalized content 排除自 hash 字段)
  - Non-scope: 不写 Markdown 契约文档（T-002）；不实现聚合计算（T-008）
  - Outputs: registry JSON + validator + 校验通过记录
  - Validation: `python -m dataset_v3.validate_contracts --registry` 全过；负向：注入非法 role/缺字段必须 FAIL
  - Completion evidence: validator 输出 + registry hash

- [ ] 2. observation_contract.md（解释层）
  - ID: T-002
  - Requirement(s): FR-461, FR-465；Design: DGN-01/02
  - depends_on: T-001
  - Files/areas: `docs/phase4/d_dataset_v3/observation_contract.md`（新建）
  - Scope: H1/H2/H3 三层表 + 部署可获得性论证 + 每列 aggregation 语义 + H3→storage role 映射规则（引用 registry，不形成第二事实源）；ctrl_mode 非激活通道 zero-fill 语义（DGN-06）
  - Non-scope: 不复制 registry 数值表为独立事实源
  - Outputs: 文档
  - Validation: 主 Agent 对照 registry 逐行核对；README 登记
  - Completion evidence: 文档入库 + 核对记录

## T-431 — Known-Future 契约（FR-462~468, FR-495）

- [ ] 3. producer schema + forecast provenance 契约
  - ID: T-003
  - Requirement(s): FR-462, FR-463, FR-466, FR-467, FR-495；Design: DGN-03/04/13
  - depends_on: T-001
  - Files/areas: `config/dataset_v3_producer_schema.json`（新建）、`dataset_v3/validate_contracts.py`（扩展）
  - Scope: C-producer 11 项能力字段化（truth stream/s_plan/INPUT_K/INPUT_F/ORACLE/五族 metadata/seed/OOD 配置面）；forecast provenance 六字段（forecast_generator_id/version/hash、issue_time_s、source_cutoff_time_s、forecast_provenance_class）；不变量 `source_cutoff_time_s ≤ issue_time_s`；ORACLE-derived class 拒收规则
  - Non-scope: 不实现 C 侧生成器；不写 Markdown（T-004）
  - Outputs: schema JSON + validator 扩展
  - Validation: validator 自检；负向：`source_cutoff > issue_time`、forbidden provenance class 必须 FAIL
  - Completion evidence: validator 输出 + schema hash

- [ ] 4. 三份契约文档（future_information / c_producer_interface / dataset_v3_schema）
  - ID: T-004
  - Requirement(s): FR-462~468, FR-495；Design: DGN-02/03/04/12/13
  - depends_on: T-001, T-003
  - Files/areas: `docs/phase4/d_dataset_v3/future_information_contract.md`、`c_producer_interface_contract.md`、`dataset_v3_schema.md`（新建）
  - Scope: K/F/U 分类+issue-time causality+vintage 语义+predictor-access 限制；D→C 生产侧接口（C 包开工输入）；realization-time 流与 (issue,lead) 快照 schema、八 role 语义、manifest 字段、Dataset views；一律引用机器契约为事实源
  - Non-scope: 不预注册 activation-time 值（OOD 轴 A 数值/counts/seeds/fleet 规模/formal 航次合格判据数值）
  - Outputs: 三份文档
  - Validation: 主 Agent 对照 schema/registry 核对；README 登记
  - Completion evidence: 文档入库 + 核对记录

## 前置卫生任务（validation infrastructure hygiene / prerequisite maintenance，D-D8 ②③）

- [ ] 5. 10b CCS 稳态 eligibility mask
  - ID: T-005
  - Requirement(s): FR-490；Design: DGN-14
  - depends_on: 无（可与 T-001 并行）
  - Files/areas: `scripts/10b_smoke_modelE.m`（判据段）
  - Scope: eligible(t) = t>60s ∧ 前30s无v_ref阶跃 ∧ 无注入激活 ∧ DG 稳态条件（state_DG1/2==ON、无 STARTING/SYNCHRONIZING/COOLDOWN、settling 窗内无迁移）；fail-closed `eligible_sample_count==0 ⇒ FAIL`；暂态/稳态指标分别记录；**CCS 阈值/场景/模型零改动**
  - Non-scope: 不改判据数值；不改其他 Gate
  - Outputs: 修改后 10b + 运行记录
  - Validation: 10b 复跑 PASS（含 mask 生效证据）；负向：mask 置全不 eligible 必须 FAIL
  - Gate/STOP: 复跑 FAIL → 停止升级
  - Completion evidence: 10b 输出（含 eligible_sample_count>0）

- [ ] 6. case 隔离不变量 + E10 注入残留修复
  - ID: T-006
  - Requirement(s): FR-491；Design: DGN-15
  - depends_on: 无（可与 T-001 并行）
  - Files/areas: `scripts/+gate_e/reset_bench_nominal.m`（新建）、E10 所属 case 接线（+gate_e/）、相关 Gate 入口
  - Scope: 每 case 运行前显式 restore nominal bench + 只施加本 case 注入；E10 不再携带 E09 遗留注入；40/42/43/44 及 E10 链接入
  - Non-scope: 不改物理/控制/判据语义
  - Outputs: reset helper + 接线 + 复验结果
  - Validation（**每个被修改的 validation entrypoint 都必须有修改后证据**）：E10 重跑无 E09 残留注入证据 + **40 号物理 Gate 复跑 overall=true** + **42 负向相关回归 PASS** + **43 控制 Gate 相关回归 PASS** + **44 负向套件 PASS**（卫生修复不免验证）
  - Gate/STOP: 任一复验 FAIL → 停止升级
  - Completion evidence: 40 号 gate_result.json overall=true + 42/43/44 复验记录 + E10 残留检查记录

## T-433/T-434 — 窗口化与 target 管线（FR-469~480）

- [ ] 7. MATLAB pilot producer（70 号）
  - ID: T-007
  - Requirement(s): FR-466, FR-467, FR-475, FR-500；Design: DGN-01/04/13
  - depends_on: T-001, T-003, T-005, T-006
  - Files/areas: `scripts/70_export_dataset_v3_pilot.m`（新建）
  - Scope: 按 registry/source_signal 从 logsout 导出 voyage-level native stream（truth+H1/H2+ORACLE+METADATA，realization-time）+ K/F issue-time snapshots（causal fixtures：v_plan_fixture=planned along-track SOG、s_plan_fixture 由专用 planned-route fixture 构造、U/WL persistence forecast、bridge/限速空集如实记录）；forecast provenance 六字段；`fixture_kind=PILOT_ONLY`；输出至 `datasets_v3/pilot/`；消费 registry 不硬编码清单
  - Non-scope: 不改 Model E 模型/场景定义；不生成 formal 数据；不实现 C 语义
  - Outputs: pilot voyage artifacts（本地）+ 每航次 manifest 条目
  - Validation: 导出 schema 校验 PASS；fixture 标记存在；provenance 不变量成立
  - Completion evidence: 校验记录 + 产物清单

- [ ] 8. dataset_v3 包骨架 + channel-aware 聚合
  - ID: T-008
  - Requirement(s): FR-469, FR-470；Design: DGN-01/05
  - depends_on: T-001
  - Files/areas: `dataset_v3/__init__.py`、`aggregate.py`、`tests/test_aggregate.py`（新建）
  - Scope: time_mean（trapezoidal 时间加权）/interval_endpoint/last_mode/interval_any 四规则实现，按 registry aggregation 字段驱动；bin (a,b] 右闭语义
  - Non-scope: 窗口化/target（T-009）
  - Outputs: 模块 + 单测
  - Validation: `python -m pytest dataset_v3/tests/test_aggregate.py` 全过（bin 边界 off-by-one 用例）
  - Completion evidence: pytest 输出

- [ ] 9. 窗口化 + target/E300 构建
  - ID: T-009
  - Requirement(s): FR-469, FR-471, FR-472, FR-473, FR-476, FR-477；Design: DGN-07/08
  - depends_on: T-007, T-008
  - Files/areas: `dataset_v3/windowing.py`、`targets.py`、`tests/test_windowing.py`、`tests/test_targets.py`（新建）
  - Scope: anchor 网格（stride 10s）/窗口合法性（同 voyage、禁 padding）/样本 METADATA 三字段；K/F vintage 精确对齐（issue==t₀，缺失即 invalid 计数）；targets.py 只消费 native stream：trapezoidal 累计能量（内部 kW·s）→ bin 差分 → y_k[kW]、E300[kWh]
  - Non-scope: split/审计（T-012/013）
  - Outputs: 模块 + 单测
  - Validation: pytest 全过；能量不变量 ΣE_k≡E_raw 差（同 trapezoidal 定义，1e-9 相对）；聚合-target 交叉验证同定义等价
  - Completion evidence: pytest 输出 + 不变量记录

- [ ] 10. role 白名单 model views + target/quantile 契约落账
  - ID: T-010
  - Requirement(s): FR-464, FR-477, FR-478, FR-479, FR-480；Design: DGN-02/04/12
  - depends_on: T-009
  - Files/areas: `dataset_v3/build_dataset_v3.py`（view 构建部分）、`dataset_v3_schema.md`（target 节，T-004 文档增补）、`tests/test_views.py`（新建）
  - Scope: 按 registry 白名单构造 H1-view/H2-view/trajectory target view/legacy scalar E300 view；quantile 契约落账：数据集不物化 q 标签、pinball 学习语义、q10≤q50≤q90 不变量声明、bin realized q 仅 ORACLE 诊断命名（v1 不物化）；E300 view 与 trajectory 一致性校验
  - Non-scope: 不训练模型；不实现 pinball loss（E 包）
  - Outputs: view 构建 + 文档增补 + 单测
  - Validation: pytest 全过；负向：FORBIDDEN/H3 列入 view 必须被拒
  - Completion evidence: pytest 输出 + 文档 diff

## T-435 — Split / Leakage / OOD 审计（D0 = pilot qualification；formal 见 T-021）

- [ ] 11. deterministic group-aware split
  - ID: T-011
  - Requirement(s): FR-481, FR-482；Design: DGN-09
  - depends_on: T-009
  - Files/areas: `dataset_v3/split.py`、`tests/test_split.py`、`results/dataset_v3/pilot/split_assignment_v3.json`（产物）
  - Scope: group 层 identity（pilot=voyage_id）；`u=int(SHA256(split_seed‖split_group_id)[:16],16)/2⁶⁴`；group→stratum 唯一映射 fail-fast；stratum 内 u 升序 70/15/15（half-up rounding + 字典序破平局）；显式 assignment artifact + SHA256
  - Non-scope: formal split_group_id policy（activation）
  - Outputs: 模块 + 单测 + pilot assignment artifact
  - Validation: pytest 全过（顺序无关/同输入同 assignment/tie-breaking/多 stratum 映射 fail-fast）
  - Completion evidence: pytest 输出 + artifact hash

- [ ] 12. 三级泄漏审计 + OOD 装载/域检查
  - ID: T-012
  - Requirement(s): FR-484, FR-485, FR-486；Design: DGN-10/11
  - depends_on: T-011
  - Files/areas: `dataset_v3/audit_leakage.py`、`audit_ood.py`、`tests/test_audit.py`（新建）
  - Scope: L1（跨 split voyage/group、raw hash 重复、exact duplicate——content hash 覆盖 H1/H2/K/F/Y 全载荷、metadata identity 除外）；L2（ε=1e-6 kW numerical candidate → provenance 三分类 CONFIRMED_DUPLICATE/LEGITIMATE_NEAR_REPLICATE/UNRESOLVED，禁自动剔除）；L3（SBD suspect flag，阈值 policy=Q0.01(train-train NN)，不自动判泄漏）；OOD loader + 物理域检查（越域标 PHYSICS_EXTRAPOLATION/STRESS_SET）
  - Non-scope: formal 阈值数值/OOD 正式结论
  - Outputs: 模块 + 单测
  - Validation: pytest 全过；负向套件见 T-013
  - Completion evidence: pytest 输出

- [ ] 13. 负向测试套件（审计/契约/角色/阶段隔离）
  - ID: T-013
  - Requirement(s): FR-464, FR-484, FR-500, AC-427；Design: DGN-10/13 + R6/R8
  - depends_on: T-010, T-012
  - Files/areas: `dataset_v3/tests/test_negative.py`（新建）
  - Scope: 构造缺陷必须全部检出——L1 三种形态；`source_cutoff>issue_time`；forbidden forecast provenance class；FORBIDDEN 入 view；PILOT_ONLY 进 formal 路径；split 多 stratum 映射
  - Outputs: 负向测试 + 全检出记录
  - Validation: pytest 全过（每缺陷独立断言）
  - Completion evidence: pytest 输出（检出率 100%）

- [ ] 14. manifest + deterministic payload hash
  - ID: T-014
  - Requirement(s): FR-474, FR-488；Design: DGN-12 + R8
  - depends_on: T-010, T-011, T-012
  - Files/areas: `dataset_v3/manifest.py`、`tests/test_manifest.py`（新建）
  - Scope: manifest 全字段（FR-488 十五项 + stride/runtime 分列 + 几何五元组 + registry/schema hash + 脚本 SHA + 版本）；`scientific_payload_sha256` 与 `run_metadata` 分离（build_timestamp 不入 payload hash）
  - Outputs: 模块 + 单测 + pilot manifest
  - Validation: pytest 全过；确定性：同输入两跑 payload hash 一致（timestamp 不同不影响）
  - Completion evidence: 两跑 hash 一致记录

- [ ] 15. pilot E2E + 71 号 pilot Gate（T-435-D0 pilot qualification）
  - ID: T-015
  - Requirement(s): FR-489, AC-426；Design: DGN-01/16 + Testing Strategy
  - depends_on: T-007, T-010, T-011, T-012, T-013, T-014
  - Files/areas: `scripts/71_run_dataset_v3_pilot_gate.m`（新建薄入口）、`results/dataset_v3/pilot/`（产物）
  - Scope: S01~S36/M1~M3 子集导出 → 构建 → split → 审计 → manifest 全链；机器可读 `pilot_gate_result.json`（标 PILOT）；仅证明管线正确性，**不产出科学统计**
  - Non-scope: formal fleet；科学基线
  - Outputs: pilot gate 结果 + qualification 报告
  - Validation: 71 Gate overall PASS
  - Gate/STOP: FAIL → 停止升级
  - Completion evidence: `pilot_gate_result.json` overall=true

## T-432 解锁准备（C/D Gate 骨架 + activation 设施，D0 内完成）

- [ ] 16. C/D Interface Gate 骨架 + T-432 activation 设施
  - ID: T-016
  - Requirement(s): FR-496, FR-497, FR-498, FR-499；Design: DGN-16/17
  - depends_on: T-003, T-015
  - Files/areas: `scripts/72_run_cd_interface_gate.m`（新建）、`dataset_v3/cd_interface_gate.py`（新建）、`docs/phase4/d_dataset_v3/t432_activation_record.md`（模板）、unblock checklist JSON 模板
  - Scope: 十项检查实现（schema 符合/s_plan causal/KF snapshot 形式/F provenance 合法/KF 与 ORACLE 物理分离/metadata 完整/航次合格判据预注册值核对/OOD 配置面/manifest validator/Model E 基线完整性 hash）；对 pilot fixture 做骨架自检（结果标 NON-FORMAL）；activation record 与 checklist 模板落库
  - Non-scope: 正式执行 C/D Gate（C 就绪后）；填写 activation 值
  - Outputs: Gate 代码 + 骨架自检记录 + 模板
  - Validation: 骨架自检 PASS；负向：违反任一检查项的构造输入必须 FAIL
  - Completion evidence: 骨架自检记录（标 NON-FORMAL）

## D0 收官

- [ ] 17. D0 归档与状态标注
  - ID: T-017
  - Requirement(s): FR-492, FR-493, AC-429；NFR-422/423
  - depends_on: T-002, T-004, T-015, T-016
  - Files/areas: 相关 README（model 两级、docs/phase4/、scripts 索引等）、legacy 标记、`docs/phase4/d_dataset_v3/README.md`
  - Scope: §20 README 两级同步；03/05/30/60/66+python/datasets/legacy_reference 标 LEGACY/SUPERSEDED；绝对路径扫描零命中；spec 状态标注 `D0 contract/pipeline complete; T-432 BLOCKED`（保持 implementation-in-progress）；D0 偏差登记（design.md）；HANDOFF（移交 C 包/后续 session）
  - Non-scope: 删 legacy；升 implementation-complete
  - Outputs: README/标记/扫描记录/HANDOFF
  - Validation: 路径扫描零命中；git status 核对无越界改动
  - Completion evidence: 扫描输出 + README diff

## T-432 — Formal Fleet 生成（BLOCKED）

> **状态：BLOCKED**。blocked_by: Phase 4-C Mission-v3 producer-interface freeze。
> 解锁须 Unblock Checklist 八项全绿（FR-498）+ C/D Interface Gate overall=true（FR-496）。
> tasks.md 批准本身不构成解锁。activation-time 值（fleet 规模/strata/seeds/OOD 轴 A 数值/OOD counts/formal 航次合格判据数值）只能在 activation record 预注册后消费，本文件不预填。

- [ ] 18. T-432 Stage A — generation qualification（BLOCKED）
  - ID: T-018
  - Requirement(s): FR-497；Design: DGN-17
  - blocked_by: T-432 Unblock Checklist（FR-498 全绿）
  - Files/areas: `scripts/73_generate_fleet_v3.m`（activation 时施工）、`datasets_v3/qualification/`、`results/dataset_v3/qualification/`
  - Scope: ≈100 条 qualification 航次（engineering nominal）；验证生成稳定性/导出完整性/schema 一致/吞吐/failed-voyage 处理（记录后继续）/审计管线
  - Non-scope: 科学结论；formal 目录写入
  - Validation: qualification gate PASS + 审计管线全绿
  - Completion evidence: qualification 报告

- [ ] 19. T-432 Stage B — formal fleet（BLOCKED）
  - ID: T-019
  - Requirement(s): FR-497；Design: DGN-17 + R8
  - depends_on: T-018；blocked_by: 同 T-018 + activation record 预注册完成
  - Files/areas: `datasets_v3/formal/iid/`、`datasets_v3/formal/ood/`、`results/dataset_v3/formal/`
  - Scope: 按 activation record 预注册 exact count/strata/seeds/OOD 定义生成 formal fleet；failed slot 按预声明 retry policy（同 config/seed，禁换新 seed）；allocation 未满足 ⇒ Gate FAIL
  - Non-scope: 任何 activation 值的事后调整
  - Validation: schema/契约校验全过 + allocation 满足 + 基线完整性 PASS
  - Completion evidence: formal fleet manifest + allocation 核对记录

## T-435-formal — 正式审计与冻结（BLOCKED）

- [ ] 20. T-435-formal split/leakage/OOD 证据 + Dataset-v3 freeze（BLOCKED）
  - ID: T-020
  - Requirement(s): FR-484~489, AC-431, AC-432；Design: DGN-10/11/12
  - depends_on: T-019；blocked_by: T-432 完成
  - Files/areas: `results/dataset_v3/formal/`（manifest/审计证据）
  - Scope: formal split assignment 冻结 + L1 零违规/L2 处置记录/L3 SBD（阈值数值由 formal train 按预冻结 policy 计算）+ OOD 三轴正式证据 + manifest 全字段（test_sealed/ood_sealed=true）+ scientific_payload hash 冻结；**pilot 统计不得用于满足本任务**
  - Non-scope: 模型训练/评估（E 包）
  - Validation: 全部审计机器可读结果 PASS
  - Completion evidence: formal manifest + 审计报告；spec 方可申请 implementation-complete

## Verification Checklist

- [x] Requirements IDs 被任务引用（FR-458~500/NFR-420~426 全覆盖，见追溯节）
- [x] Design 测试策略被任务覆盖（单测 T-008/009/011/012/014、负向 T-013/016、确定性 T-014、pilot E2E T-015、回归 T-005/006）
- [x] 每任务有明确 validation 命令或人工核对
- [x] T-432（T-018/019）与 T-435-formal（T-020）保持 BLOCKED，activation 值不预填
- [x] D0 收官（T-017）后 STOP：T-432 BLOCKED、spec 保持 implementation-in-progress

## Requirement 覆盖速查

| 任务 | FR/NFR |
|---|---|
| T-001/002 | FR-461/464/465/470 |
| T-003/004 | FR-462/463/466/467/495 |
| T-005/006 | FR-490/491, NFR-426 |
| T-007 | FR-466/467/475/500 |
| T-008/009/010 | FR-469~473, FR-476~480 |
| T-011~015 | FR-474, FR-481~489, AC-426/427 |
| T-016 | FR-496~499 |
| T-017 | FR-492/493, AC-429, NFR-422/423 |
| T-018/019 | FR-497/498 |
| T-020 | FR-484~489, AC-431/432 |
| 全程 | FR-458~460, NFR-420/421/424/425 |

## Execution Notes

- 保持 `Status: tasks-draft` 直至用户书面批准 tasks 门；批准后改 `tasks-approved` 并跑 implementation-readiness 校验再施工；
- 施工开始改 `implementation-in-progress`；**D0 完成（T-001~T-017 全勾）后保持 implementation-in-progress 并显式标注 D0 complete / T-432 BLOCKED——不得升 implementation-complete**（T-018~T-020 未完成）；
- 依赖允许时任务可并行（T-001/T-005/T-006 首批并行；契约层先行于管线层）；单写者纪律：同一文件同一时间一个写者；
- 每任务 validation 通过后立即 `spec_task_check`；
- 施工期偏差先登记 design.md 偏差区再实施；
- T-018~T-020 的激活不自动发生：须 C 包完成 + Unblock Checklist 全绿 + activation record 预注册。

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-04 | tasks 批准（CONDITIONAL PASS + R1~R7 闭合）：R1 局部编号勘正 T-001~T-020；R2 T-005 DG 稳态语义修正（relevant/dispatching 稳态 ON，非双机同开；消费现有状态机编码，禁新造枚举）；R3 T-006 修改过 entrypoint 全量回归（40/42/43/44/E10 各自证据）；R4 T-007 完整 producer schema 必填集硬要求 + h_chart/P_hotel_plan fixture 语义；R5 执行约束顶部转录 DGN-01/R8 path roots；R6 T-011 partition completeness 不变量 + small-stratum 用例 + 严格实现 DGN-09 冻结算法；R7 T-018/019 qualified-stack freeze（Stage A PASS 后冻结八项 hash，变更即失效重跑）+ T-020 OOD evidence 限定 Dataset 层 | 用户 tasks gate 评审 |

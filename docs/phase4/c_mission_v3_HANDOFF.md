# HANDOFF — Phase 4-C Mission v3（新 session 开工提示词）

> 写给接手 Phase 4-C 的 session。本文件 + 路线文档 §4 + D 包冻结契约即开工包。
> 创建：2026-09-05（D 包 D0 完成后，[PR #21](https://github.com/realOliverSama/hybrid-ship-ems/pull/21) 交用户审批）。
> **D 包状态**：D0（契约与管线层）已完成——C 的开工输入已冻结；D 的 formal 收尾（T-432/T-435-formal）在 C 完成后进行。

## 0. 开工仪式（必须先做）

```bash
git fetch origin && git status -sb && git log --oneline -3
```

- 工作区两个未跟踪目录 `firmware/.../Host/fault_out/`、`model/simulink_modelD_(EMS_NN_AECMS)/slprj_v1_stale/`
  为既定不入库产物，**不清理、不提交、不担心**；
- 分支策略：**待 PR #21（feat/phase4-d-dataset-v3 → Phase4）由用户合并后**，从 Phase4 新头开出
  `feat/phase4-c-mission-v3`（勿从 D 分支 stacked——契约已随 D0 PR 进 Phase4）；
- 工作区若有其他未提交改动 → 停止报告用户。

## 1. Phase 4 当前进度（截至 2026-09-05）

| 包 | 状态 |
|---|---|
| A2 / B | implementation-complete（PR #19 / #20 已并入 Phase4） |
| **D（数据集 v3）** | **D0 complete**（PR #21 待批）：契约层/hygiene/两段式管线/审计/负向全交付；T-432/T-435-formal BLOCKED |
| **C（Mission v3，T-419~T-429）** | **本包（spec 未建）——关键路径，D formal 收尾的硬前置** |
| E/F | 未启动（依赖 Dataset-v3 Freeze，C 完成前不可开工） |

**C 的输入已冻结（D0 交付）**：
- `docs/phase4/d_dataset_v3/c_producer_interface_contract.md`（十一项能力 + 因果红线 + T-432 解锁链）；
- `model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_producer_schema.json`（机器权威，self_hash 入 manifest）；
- `docs/phase4/d_dataset_v3/D0_COMPLETE_HANDOFF.md`（解锁链 + D-15 必改项）。

按 FR-499：C 对已批准契约的 **conforming implementation 不重走四门禁**；但 C 包自身的架构/生成机制走自己的四门禁 spec 流程。

## 2. C 包任务范围（路线 §4，T-419~T-429 + D 侧接口实现）

| 任务 | 内容 | D 侧接口约束 |
|---|---|---|
| T-419 | Mission Generator v3 架构设计（先设计不写生成器） | 输出必须落 producer schema 十一项能力 |
| T-420 | Bridge 工况（出桥 propulsion transient；法规语义） | v_limit 属 static_constraints.speed_limit_zones |
| T-421 | Harbor 工况（与 bridge 明确分离） | — |
| T-422 | Shallow-water 工况（输入=water depth，UKC 限速） | water_depth 经既有 VOYAGE_WATER_DEPTH_M 接口 |
| T-423 | U(t)=Ū+δU(t) 时间相关扰动（禁白噪声） | 同时产生 INPUT_F 真实预报语义的载体 |
| T-424 | Traffic disturbance（slowdown→follow→recover） | — |
| T-425 | Emergency mission（公务船属性；Model E 极速 14.5 kn 第一次有真实空间） | — |
| T-426 | Berthing / departure（离泊/靠泊非镜像） | — |
| T-427 | Full Voyage Template v3（routine patrol / emergency response / heavy-current） | route_template_id / mission_template_id 语义载体 |
| T-428 | Stochastic Mission Generator（**条件随机**，非独立 random） | generation_seed / split_group_id 着落点 |
| T-429 | 可辨识性审计（bridge/harbor/shallow/upstream/berth 信号空间可分） | 与 D 的 L3-SBD/OOD 审计衔接 |

**D→C 接口必做项**（producer schema 逐项实现，formal 禁 PILOT_ONLY）：
voyage-level truth stream（native 0.01 s）、s_plan[k|t₀] 直出（计划前向积分，**禁 realized future 冒充**）、INPUT_K/INPUT_F issue-time snapshots（布局 (M×1)/(M×30)/(M×30) issue-major，lead∈{10..300}，K/F 网格=target 网格 30×10 s）、forecast provenance 六字段（formal class=CAUSAL_GENERATOR，source_cutoff≤issue_time）、ORACLE truth 对应物物理分离、五族 metadata（voyage/family/template/group/stratum）、generation seed、OOD 配置面。

## 3. 必读材料（按序，禁止凭记忆开工）

1. `docs/phase4_research_roadmap.md` §4（C 包全节）+ §10（依赖图）；
2. `docs/phase4/d_dataset_v3/c_producer_interface_contract.md` + `config/dataset_v3_producer_schema.json`（**机器权威**）；
3. `docs/phase4/d_dataset_v3/D0_COMPLETE_HANDOFF.md`（解锁链 + D-15 必改项）；
4. `docs/phase4/d_dataset_v3/` 其余三份契约文档（observation / future_information / dataset_v3_schema）；
5. `model/simulink_modelE_(Phase4_Physics)/README_Model.md`（16 块结构/PMS v2/Safety/浅水接口现状）；
6. `model/simulink_modelE_(Phase4_Physics)/scripts/+dataset_v3/export_pilot.m`（pilot producer——artifact 布局与 issue-major 快照的**参考实现**；formal 版把 causal fixtures 换成真实生成器）；
7. `docs/agent/AI_MODELING.md`（数据纪律红线）；
8. `项目开发进展.md` 2026-09-04/05 条目（R2026a 工程坑清单 + D0 事故记录）。

## 4. 契约红线（违反=返工；来自已批准的 D 契约，C 不得重议）

- **issue-time causality**：x(t₀+k|t₀) 只消费 ℐ_{≤t₀}；s_plan 由计划前向积分；禁任何 realized future 进入 K/F；
- **双索引 schema**：truth 单轴 realization-time；K/F 按 (issue, lead) 快照；禁从最终 future series 反推 vintage；
- **双存隔离**：INPUT_F 预报与 ORACLE 真值物理分离（角色表分立）；
- **阶段隔离**：任何 pilot fixture/PILOT_ONLY 标记不得进入 formal 路径（FR-500）；
- **Model E 零改动**：全部工况经既有接口表达（SCN 表 / VOYAGE_* base 变量 / water_depth 输入），不改 .slx/参数/Gate；
- **基线法律角色**：formal 生成基线 = 物理 v1.1 + 控制 v1.0（meta.model_baseline_ref 须如实引用；A2 historical v1.0 不得混同）。

## 5. 纪律（与 D 包同款，违反=返工）

- spec 四门禁（brainstorm 一次一问逐项裁决 → requirements → design → tasks），tasks-approved 前禁施工；
- 禁绝对路径（§20.2 扫描零命中）；§20 README 两级同步；逐文件 add、禁 force-push；
- 每任务完成即 spec_task_check；Gate FAIL（`error('GATE:*')`）即停升级；
- R2026a 坑清单继续生效（数字开头脚本 eval(fileread)、薄入口+包函数、禁 base 重跑含循环的配置脚本、From Workspace Interpolate off 等）；
- 大规模生成类任务可在 tasks 批准后委派 subagent（写+独立审查 crosscheck）。

## 6. brainstorm 必须先裁决的问题（建议清单）

1. **split_group_id grouping policy**（D-D7 明确留给 C）：paired/counterfactual 航次族怎么定义、何时同族；
2. **INPUT_F 真实预报语义**：pilot 用 persistence fixture；formal 的"预报"是什么（persistent/平滑外推/带误差注入的模拟预报服务）——这决定 E 包"预测器+预报"研究的上界口径；
3. **T-428 条件随机化的条件结构**：segment length/current/water level/aggressiveness/traffic/mission 哪些条件耦合、分布族与边界（须与 D 的 OOD 轴定义对接：轴 A 2.0<|U|≤2.8 预注册框架）；
4. **formal 航次合格判据提案**（min_duration_s / min_canonical_windows 数值提案，activation record 预注册）；
5. **OOD 轴 C held-out 组合的 token 对齐**（桥区×强逆流 / 浅水×港区 / 应急冲刺×浅水）；
6. **T-429 可辨识性审计的判据**（与 D 的 L3-SBD 衔接：模板级 SBD 距离 vs 航次级）；
7. **strata 分类法**（scenario family 的正式定义，D 的 split 分层直接消费）。

## 7. 完成定义（C 包 STOP 条件）

C 包 spec 四门禁完成 + implementation 完成（producer schema 十一项全部落地 + T-429 审计 PASS）→ **停**：
创建 C→Phase4 PR 交用户审批；随后回到 D 包做 formal 收尾（另一 session/另立项均可）：
C/D Interface Gate 正式版（先做 D-15 必改：十项独立键 + ⑤⑩ 收紧）→ T-432 activation record 填值并经用户批准 → Stage A（≈100 qualification，73 号施工）→ Stage B formal fleet → T-435-formal 审计 → Dataset-v3 Freeze（D spec 方可 implementation-complete）。

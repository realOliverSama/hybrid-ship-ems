# HANDOFF — Phase 4-D 数据集 v3 · D0 施工（新 session 开工提示词）

> 写给接手 Phase 4-D D0 施工的 session。本文件 + spec 四件 + 路线文档即开工包。
> 创建：2026-09-04（四门禁全部批准，spec_validate implementation 14/14）。
> **授权范围：仅 D0（T-001~T-017）施工。T-432/T-435-formal（T-018~T-020）保持 BLOCKED，本 session 不得触碰。**

## 0. 开工仪式（必须先做）

```bash
git fetch origin && git status -sb && git log --oneline -3
git checkout feat/phase4-d-dataset-v3   # 若不在该分支
```

- 工作区两个未跟踪目录 `firmware/.../Host/fault_out/`、`model/simulink_modelD_(EMS_NN_AECMS)/slprj_v1_stale/`
  为既定不入库产物，**不清理、不提交、不担心**；
- 工作区若有其他未提交改动 → 停止报告用户。

## 1. 授权状态（截至 2026-09-04）

- spec：`.specs/phase4-d-dataset-v3/`——brainstorm（D-D1~D-D9）→ requirements（R1~R5 修正闭合）
  → design（DGN-01~17，R1~R8 修正闭合）→ tasks（T-001~T-020，R1~R7 修正闭合）
  **四门禁全部用户书面批准**；`spec_validate implementation` 14/14 PASS；
- 分支 `feat/phase4-d-dataset-v3`（基点 8e9f05be = B 包 PR #20 合并后 Phase4 头）；
- **施工授权：T-001~T-017（D0）**；T-018/019（T-432 formal fleet）与 T-020（T-435-formal）
  **BLOCKED**——blocked_by: Phase 4-C Mission-v3 producer-interface freeze + C/D Interface Gate
  overall=true + Unblock Checklist 八项全绿。tasks.md 批准不构成解锁。

## 2. 必读材料（按序，禁止凭记忆开工）

1. `.specs/phase4-d-dataset-v3/` 四件（**全部契约/数值/规则的唯一事实源**）；
2. `docs/phase4/d_dataset_v3_HANDOFF.md`（本包立项 handoff，含 R2026a 坑清单与挂账背景）；
3. `model/simulink_modelE_(Phase4_Physics)/README_Model.md`（模型现状：16 顶层块/logsout 打点清单/PMS v2/Safety）；
4. `docs/agent/AI_MODELING.md`（数据纪律红线，含 §22 口径契约事故）；
5. Phase 2.5 纪律资产（`model/simulink_modelD_(EMS_NN_AECMS)/python/build_dataset_v2.py`——
   **reuse discipline, not schema**，适配前只读）。

## 3. D0 施工范围（T-001~T-017）

| 任务 | 内容 | 关键交付 |
|---|---|---|
| T-001/002 | signal/role registry + validator + observation_contract.md | `config/dataset_v3_signal_registry.json` |
| T-003/004 | producer schema + 三份契约文档 | `config/dataset_v3_producer_schema.json` + docs/phase4/d_dataset_v3/×3 |
| T-005 | 10b CCS 稳态 eligibility mask（**hygiene**） | 阈值不动+预声明 mask+fail-closed |
| T-006 | case 隔离 + E10 修复（**hygiene**） | reset_bench_nominal + 40/42/43/44/E10 全量复验 |
| T-007 | 70 号 MATLAB pilot producer（含 causal fixtures） | datasets_v3/pilot/（本地不入库） |
| T-008~010 | 聚合/窗口化/target/view（Python `dataset_v3/`） | time_mean 积分语义/区间 bin/能量差 target |
| T-011~014 | split/三级泄漏审计/OOD loader/manifest | deterministic group split + L1/L2/L3 |
| T-013 | 负向测试套件 | 检出率 100% |
| T-015 | 71 号 pilot Gate E2E | pilot_gate_result.json overall PASS（标 PILOT） |
| T-016 | 72 号 C/D Gate 骨架 + activation 模板 | NON-FORMAL 骨架自检 |
| T-017 | D0 归档：README 两级/legacy 标记/路径扫描/偏差登记 | spec 标注 D0 complete / T-432 BLOCKED |

依赖：T-001 首批（可与 T-005/T-006 并行）→ T-002/T-003 → T-004 → T-007（需 T-005/006 完成）
→ T-008~010 → T-011~014 → T-015 → T-016 → T-017 **STOP**。

## 4. 冻结事实源（禁改）

- **Model E 零改动**：Plant/EMS/PMS/Safety 参数、模型结构、Gate 阈值、
  `ModelE_Physics_Parameters.m`、`ModelE_Control_Parameters.m` 全部禁改；
- formal 生成基线法律角色：物理 v1.1 + 控制 v1.0 = generation baseline；
  A2 historical v1.0 = immutable 参考资产（不混同）；
- Legacy 冻结：03/05/30/60/66、`python/`、`datasets/`、`legacy_reference/`
  不删不搬不改行为（只加 README superseded 标记）；
- 科学契约（design R6 清单）：300s@1Hz / 30×10s / stride 10s / 区间均值 target / E300 /
  predictive quantiles / 八 storage role / issue-time vintage / time-weighted 聚合 /
  70/15/15 group split / L1-L3 法律地位 / sealed test/OOD / OOD∩物理域 / forecast provenance /
  目录阶段隔离 / 70-73 编号 / hygiene 语义——**tasks/implementation 阶段不得重新设计**。

## 5. 关键设计参数速查（design-approved，直接照做）

- 聚合：`time_mean`=trapezoidal 时间加权（非 mean(samples)）；SOC/SOP/v_ref/manual_cmd=interval_endpoint；
  ctrl_mode/state_DG=last_mode；flag=interval_any；bin (a,b] 右闭；
- target：native-rate trapezoidal 累计能量（内部 kW·s）→ bin 差分 → y_k=E_k/10 [kW]；E300=ΣE_k/3600 [kWh]；
- ctrl_mode 非激活通道：zero-fill + 模式指示（无 NaN）；
- split：`u=int(SHA256(split_seed‖split_group_id)[:16],16)/2^64`；group→stratum 唯一 fail-fast；
  half-up rounding + 字典序破平局；partition 完备性三不变量 + small-stratum n=1..5 用例；
- L2：ε=1e-6 kW 仅 numerical candidate 阈值；provenance 三分类；Stage B 禁静默删除；
- L3：SBD suspect flag，T_SBD=Q0.01(train-train NN) policy 先冻结；similarity≠leakage；
- forecast provenance 六字段 + `source_cutoff_time_s ≤ issue_time_s` 不变量；
- 目录隔离：`datasets_v3/{pilot,qualification,formal/{iid,ood}}`、`results/dataset_v3/{pilot,qualification,formal,cd_interface}`；
  PILOT_ONLY 禁入 formal；
- 10b mask：t>60s ∧ 30s 无 v_ref 阶跃 ∧ 无注入 ∧ relevant DG 稳态（消费现有状态机编码）∧
  `eligible_sample_count==0 ⇒ FAIL`；
- Path roots：`REPO_ROOT`；`MODELE_ROOT=REPO_ROOT/model/simulink_modelE_(Phase4_Physics)`；
  `config|scripts|dataset_v3|datasets_v3|results` 相对 MODELE_ROOT；`docs/`、`.specs/` 相对 REPO_ROOT。

## 6. 纪律（违反=返工）

- 每任务完成立即 `spec_task_check`；Gate FAIL（`error('GATE:*')`）即停升级；
- material design conflict → 停，返回 design 门，不得在施工中静默解决；
- 禁绝对路径（验收扫描零命中）；§20 README 两级同步；R2026a 坑清单沿用；
- 逐文件 add（禁 -A/.）；禁 force-push；本分支可自由 commit/push（feat 分支）；
- activation-time 值（OOD 轴 A 数值/counts、fleet 规模、strata、seeds、formal 航次合格判据数值）
  **一律不预填**，只消费 activation record；
- pilot 统计不得晋升为科学基线；fixtures 全标 PILOT_ONLY；
- Subagent 使用：用户已授权"必要时积极调用 subagent"——执行性工作（代码编写/测试/批量运行）
  可委派 delegate（默认路由）；**决策性工作（契约措辞、参数裁决、验收）主 Agent 保留**；
  委派前按 AGENTS.md §4 写清 Goal/Scope/Do-Not-Change/Validation/Escalate-If。

## 7. D0 完成定义（STOP 条件）

T-001~T-017 全部 spec_task_check 勾销 + AC-425~429 证据齐 → **D0 COMPLETE，必须停下**：
spec 保持 `implementation-in-progress`（显式标注 D0 complete / T-432 BLOCKED），
**不得升 implementation-complete**；向用户提交 D0 收官报告（AGENTS.md §15 模板）+
开发日志条目。后续 = C 包（另立项）。

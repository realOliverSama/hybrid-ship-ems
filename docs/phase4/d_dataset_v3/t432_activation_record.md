# T-432 Activation Record（模板——activation 时填值，本文件不预填任何 activation 数值）

> Spec: `.specs/phase4-d-dataset-v3`（DGN-17；FR-495/497/498；tasks T-018/T-019 BLOCKED 条款）
> **法律地位**：本 record 在 T-432 启动前由用户/activation session 填写并冻结；
> 填写后 formal fleet 生成（`73_generate_fleet_v3.m`）按本 record 预注册值执行，
> 禁止"跑出效果再决定停在哪"（FR-497）。operational freeze 不重走四门禁；
> D-D1~D-D9 科学契约漂移才重开最早受影响的门（FR-499）。
> 机器可读副本：`t432_activation_record.json`（`formal_voyage_qualification` 节供
> `cd_interface_gate.py` 项⑦核对）。

## 0. 解锁前置（填写前必须全绿，FR-498 Unblock Checklist 八项）

见 `t432_unblock_checklist.json`——八项全绿才允许填本 record 并执行 73 号。

## 1. Formal fleet 规模与分配（FR-497 Stage B 预注册； activation 时填）

| 项 | 值（activation 冻结） |
|---|---|
| exact voyage count（IID） | `_FILL_` |
| strata allocation（预声明 scenario strata 代表性） | `_FILL_` |
| route/mission/template/group allocation | `_FILL_` |
| split_group_id grouping policy（C 生成机制确定后冻结，FR-481） | `_FILL_` |
| generation seeds（主 seed + per-stratum 规则） | `_FILL_` |
| failed slot retry/disposition policy（同 config/seed，禁换新 seed） | `_FILL_` |

## 2. OOD 定义（FR-485/486；DGN-11 框架已冻结，此处冻结数值）

| 项 | 值（activation 冻结） |
|---|---|
| OOD definition version | `_FILL_`（≥ DGN-11-v1） |
| 轴 A stronger current 数值 ranges（out(frozen ID support) ∩ in(evidenced feasible domain)） | `_FILL_`（设计预注册框架 2.0 < \|U\| ≤ 2.8 m/s 须 activation 确认） |
| 轴 B lower water level 域 | 框架冻结：1.3 ≤ H/T < 2.0（物理域下界 H/T ≥ 1.2 + 0.1 margin） |
| 轴 C compositional held-out 组合 | 框架冻结：桥区×强逆流 / 浅水×港区 / 应急冲刺×浅水（token 对齐 C 包） |
| OOD counts（三轴分配） | `_FILL_` |
| OOD generation seeds | `_FILL_` |

## 3. Formal 航次合格判据（FR-495 第 7 项；cd_interface_gate 项⑦核对）

| 项 | 值（activation 冻结） |
|---|---|
| min_duration_s（最小航次时长） | `_FILL_`（600 s 仅为几何最小跨度，非充分条件） |
| min_canonical_windows（每航次最小 canonical 窗口数） | `_FILL_` |

## 4. Stage A qualification（≈100 条，工程 nominal；FR-497）

- 验证项：生成稳定性 / 导出完整性 / schema 一致性 / 吞吐 / failed-voyage 处理
  （剔除+记录，不静默修补）/ 审计管线；
- **qualified-stack freeze**：Stage A PASS 后冻结八项 hash（变更即失效重跑，tasks R7）：
  `[_FILL_: 73 号脚本 SHA / producer schema hash / registry hash / 基线四件 hash /
  Stage A fleet manifest hash / OOD def version / split policy version / checklist json hash]`

## 5. 冻结确认（activation session 填写）

- [ ] 本 record 全部 `_FILL_` 已由 activation session 填写并经用户书面批准；
- [ ] Unblock Checklist 八项全绿证据已归档；
- [ ] C/D Interface Gate **正式版**（非 NON-FORMAL 骨架）overall=true；
- [ ] 基线完整性：物理 v1.1 + 控制 v1.0 hash 与 D0 期快照一致（72 号 baseline_integrity.json）。

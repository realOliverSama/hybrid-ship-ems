# T432_ACTIVATION_AMENDMENT_01 — A2 退役与 formal fleet 缩减（qualification-triggered）

> **状态**: FROZEN（用户 Final Ruling 2026-09-07，option (a′) precision-corrected；本 amendment 不重开、不复评 T-429，不动 700 kW 物理域守卫）
> **触发**: T-018 Phase 1 smoke（k=950 disposable 段）中条件 A2 控制性失败 `GATE:FLEETV3:OOD:UNREACHABLE`——200/200 候选被 700 kW 推进容量守卫判 STRESS_SET
> **法律性质**: activation record 的 operational amendment（FR-497/FR-540 纪律；不改 D-D1~D-D9 科学契约；原冻结 record `t432_activation_record.md` 正文不覆盖，本文件 + 机器增量 `t432_activation_amendment_01.json` 为其版本化修正层）

## 1. 裁决要点（用户 Final Ruling，option (a′)）

1. **轴 A formal 条件只保留 A1**：`2.0 < |U_river| ≤ 2.4 m/s`；
2. **A2 退役**：`2.4 < |U_river| ≤ 2.8 m/s` 从 formal Dataset-v3 OOD benchmark 移除，`FORMAL_OOD_STATUS = RETIRED_AFTER_QUALIFICATION_SMOKE`；`(2.4, 2.8]` 转为**非 formal 的 stress/domain-boundary investigation band**；
3. **不放宽** 700 kW 冻结物理域守卫；**不缩带**保留 A2 标签；
4. **科学解释措辞**（R2 式约束）：**不得**声称 (2.4,2.8] 逐点已被证明物理不可行——smoke 证据表明的是"A2 作为完整预注册 formal OOD 条件，在其 proposal 分布与冻结物理域守卫下，不能在已验证运行包线内稳健生成"；guard 重构将名义 700 kW 边界置于 |U|≈2.5 m/s；
5. **stress 候选区分**（schema 支持处）：feasible-at-edge 与 fails-guard 用既有 STRESS_SET 表示 + reason metadata 区分，不新增 schema enum；
6. **Stage-E 后果**：移除 A1→A2 剂量-响应 formal 主张；**禁止**为再造该主张而发明新 A1 子带；T-443 对 A1 仍按已批准三件套（input-shift / target-shift / degradation）评测；A2/>2.4 m/s 行为可另报 stress/domain-boundary characterization，**不得混入 formal OOD 指标**。

## 2. formal fleet 修订

| 项 | 原（activation record v1） | **修订后** |
|---|---|---|
| formal IID | 576（9 格 × 16 families × 4） | **576（不变）** |
| formal OOD 条件 | 6（A1/A2/B1/C1/C2/C3） | **5（A1/B1/C1/C2/C3）** |
| formal OOD 航次 | 240（6 × 10 families × 4） | **200（5 × 10 families × 4）** |
| formal fleet 合计 | 816 | **776** |
| Stage A | 96（72 IID + 24 OOD） | **92（72 IID + 20 OOD = 5 条件 × 1 family × 4）** |

缩减性质：**physics-qualification-driven，非 effect-size-driven**（amendment v1 分配原则的物理域特例）。所有未受影响项（IID 结构/seeds/retry/拒绝率门/合格判据 1200/60/0/轴 B/轴 C/guard band 不变量/700 kW 守卫公式）一律不变。

## 3. 证据链（hash 冻结）

| 证据 | sha256 / 路径 |
|---|---|
| 原 activation record（修订前机器副本） | `ca8926dcd32a83069d882ec1e25671a0ea700248255efe278d299376ac0250a9` |
| smoke run 日志（A2 UNREACHABLE 全程） | `results/dataset_v3/qualification_smoke/smoke_run_20260907_a2_unreachable.log` = `379ef28261e30093eead5721f3b0d5d9c56dab0cc779fa397e5d4d74411f17ca` |
| A2 失败台账 | `attempts=200；formal accepts=0；STRESS_SET removals=200（0 guard-band 拒、0 no-target 拒）；GATE:FLEETV3:OOD:UNREACHABLE（见上日志尾）`——注：失败发生于增量 STRESS 台账修复之前，200 行明细未落盘，证据 = 运行日志 + 守卫公式复算（可复现）；修复后重跑将逐候选落盘 |
| A1 feasible-edge 证据 | `FLTS_OOD_A1_D00970_M1..M4.mat`（datasets_v3/qualification_smoke/ood/，k=950 disposable）：`5d278ac4…` / `a13087e8…` / `0baeead2…` / `9abdf583…`；M1 实测 sup=2.3917 m/s、P=684.6 kW feasible（贴近包线边缘） |
| 700 kW 守卫公式 | 不变（`fleet_v3.propulsion_feasibility_check`，保守解析估算，Model E 冻结系数）；复算边界：P(2.45)=691.6 ✓ / P(2.50)=700.2 ✗ / P(2.80)=752.7 ✗ → 名义边界 |U|≈2.50 m/s |

## 4. 生效机制

- `t432_activation_record.json`（机器副本）增补 `amendment_chain` 与生效值（条件 5、OOD 200、fleet 776、Stage A 92、axis_a.conditions 仅 A1、axis_a.stress_investigation_band=(2.4,2.8]）；
- `fleet_v3.plan_load` 校验 amendment 链（fail-closed）并将 amendment sha 记入 provenance；
- Unblock Checklist evidence 刷新引用本 amendment（状态保持 ALL_GREEN，T-432 保持 READY）；
- 原 `t432_activation_record.md` 冻结正文不动（本文件为其版本化修正层）。

## 5. 执行授权（用户 Final Ruling §7）

amendment 冻结后：① smoke 续跑 B1/C1/C2/C3（A1 已 PASS 的 4 件证据继续有效，k=950 段重生亦等效）；② 五个存留 formal OOD 条件必须全部 smoke PASS；③ Stage A 92 条两跑确定性；④ qualified-stack identity 采集；⑤ Stage B 776 条。除非 B1/C1/C2/C3 暴露另一科学契约冲突（非实现 bug），无需再次批准。

## 6. 实现层生成器变更登记（T429-AMEND-V2 registry_and_generator_changes 先例）

| 文件 | 变更 | 性质 |
|---|---|---|
| `scripts/+mission_v3/plan_compose.m` | 'compose' 加性 `OODMode` name-value：OOD 面下界 = activation hard floor 1.3（IID 面默认 1.5 不变，env_realize 一线拒收不变——二线防御语义不弱化）；zones 调用透传 | OOD 面必需（B1 [1.3,1.5) 在 IID 支撑之下、物理域之内） |
| `scripts/+mission_v3/nav_constraints.m` | 同上 OOD 面开关；UKC factor 在 [1.3,1.5) **clamp 于 HT=1.5 值（0.5）**——FD3 公式不向其已验证支撑之外外推（保守选择） | 同上 |
| `scripts/+mission_v3/tests/test_plan_compose.m` | P2 bridge/harbor 模态计算排除 traffic 事件样本：T429-AMEND-V2 traffic 时长 U[225,320] 冻结后，follow plateau 可合法主导实现窗模态（harbor 实现窗 531 s 被单事件覆盖 82%），原断言相对冻结变更过时；对照实验（stash 前后）证实与 OOD 改动无关 | C 侧测试维护（首次全量重跑暴露） |
| `scripts/+fleet_v3/member_exec.m` | OOD 成员向 plan_compose 传 OODMode | 接线 |
| `scripts/73_generate_fleet_v3.m` | verdict 门：IID 非 ACCEPT 成员 / OOD 条件成员不满员 ⇒ GATE:FLEETV3:VERDICT（堵住"B1 成员全失败仍 PASS"的验收漏洞） | 门语义补全 |
| `scripts/+fleet_v3/gen_ood.m` | STRESS 移除行逐候选持久化（FR-537 先例） | 证据链 |

C 侧全回归（run_all 11 套件）ALL GREEN（含上述测试维护后）；fleet_v3 单测 6 套件 ALL PASS；pytest 197 全绿。

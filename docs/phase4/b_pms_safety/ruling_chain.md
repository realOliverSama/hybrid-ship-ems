# Phase 4-B 裁决链索引（PMS v2 + Safety-Interlock）

> spec：`.specs/phase4-b-pms-safety`（brainstorm / requirements / design / tasks 四门禁 2026-09-04 全部用户书面批准；
> spec_validate implementation 14/14）。本文档为施工证据与裁决的索引，正式裁决原文以 spec 四件为准。

## 1. 裁决链（brainstorm D-B1~D-B9 + 终审 5 项修正 → requirements/design/tasks）

| 裁决 | 内容（摘要） | 落点 |
|---|---|---|
| D-B1 | 模块化双源 + 共享 Commitment 四层架构（A′）：Demand（连续规则不进 Stateflow）→ Commitment（Stateflow 唯一写机组启停）→ Dispatch（确定性均分）→ Safety（独立最高优先级） | 07_PMS_v2 四子块 + 16_Safety_Interlock |
| D-B2/D-B3 | Baseline 需求律 = 工业简化型 Load-Following + SOC 恢复；四参数 60 s/0.55/100 kW/0.60 | `config/ModelE_Control_Parameters.m` + code_pms_demand_e |
| D-B4/D-B5 | Commitment 0.85/30 s/0.70/120 s + min_up=300/min_down=0 + 并行计时（最短 ON=恰 300 s）；P_commit=LPF(P_load) 不含恢复偏置 | Stateflow chart + harness C2/C5 |
| D-B5 | min_down=0；safety 旁路经济 dwell 但不旁路 25+5 s 物理；v1 无 0DG（one-genset-online baseline 假设措辞） | chart 迁移优先级 + FR-439 |
| D-B6 | Dispatch：均分 clamp[125,500]、is_dispatching 单一谓词、DG1=lead 固定、EXTERNAL_PER_UNIT 仅登记 | code_dispatch_e + T-003 单测 |
| D-B7/D-B9 | Safety 恰五条执行性 override（S1~S5）+ 08 号唯一推进限制器 + 三臂验证 + 规则合法性替代 starts 硬阈值 + transition reason 审计 | code_safety_interlock_e + 43/44 号 Gate |
| D-B7 终审① | 仲裁顺序：Hard availability/fault veto > Safety override > Normal commitment（源码显式固化） | chart 迁移创建顺序 + harness C7 |
| 终审②④ | S2/S4 命名分离（realized/prospective）+ T-418 Gate 职责分离（R0/R5 燃油不参与判据） | reason 枚举 4/5 + 43 判据设计 |
| D-B9 | 基线 candidate→validation→promotion 两阶段（v1.0 永久保留；首 PASS 禁自动冻结） | DD-9 + make_candidate_v1_1 + T-014 |

requirements 评审 5 项修订（canonical 功率域 FR-451A / 三臂全过 Safety / M3 门槛化 /
S3 单健康机 flag-only / S2·S4 reason 分离）与 design 评审 6 项修订（S2 realized 口径 /
is_dispatching 单一谓词 / S5 显式健康契约 / DD-9 两阶段 / M3 时序 / Euler 措辞）已并入
requirements.md / design.md（FR-445 S2/S5 联动修订）。

## 2. 施工实况（T-001~T-013，2026-09-04）

| 任务 | 交付 | 验证 |
|---|---|---|
| T-001 | 控制参数事实源 + 全入口加载接线（FR-451） | 乱序拦截/26 参数/manifest 登记 PASS |
| T-002~T-004 | Demand/Dispatch/Safety 块源码+单测（三链 delegate 并行，手动 worktree） | 主树复跑 ALL PASS×3 |
| T-005 | Commitment Stateflow chart + harness | 9 用例 ALL PASS + 两建幂等 |
| T-006 | 01/02 改组 + MANUAL M1/M2/M3 | AUTO 逐位不变实证 / M1M2 rate limiter 精确 |
| T-007 | 顶层集成 07_PMS_v2+16_Safety（主 Agent 独占） | 七变体 update 过/指纹幂等 920 blocks/拍序恰一拍/M3 冒烟 |
| T-008 | 七变体编译矩阵 + law=0/5 smoke | 7/7 + smoke PASS |
| T-009 | 43 号 Gate + +gate_ctrl 判据包 | QUICK 骨架自检 PASS |
| T-010 | 注入场景 S1~S5 | 五场景 C6+C7 全 PASS |
| T-011 | 44 号负向验证 | 6/6 缺陷正确检出 |
| T-012 | candidate v1.1 + T-412 against candidate + 43 全矩阵 | 双 Gate overall=true；DD-9 机器检查 4/4 |
| T-013 | 本归档（README 两级/证据文档/D-14 补记/偏差核对/路径扫描） | 见下节 |

## 3. 偏差登记索引（详见 spec design.md §12 偏差登记）

- **D-B16**（T-014 批准，分类 = **controlled interface defect correction**）：06 号停机通路
  激活（In5 + Max 合成）——PMS v2 因果停机的机械缺口修复；M3 全链与 T-412(candidate) 逐位
  bench 背书。正式表述：No uncontrolled spec drift. One approved implementation deviation
  D-B16 modifies only the previously unconnected stop-command routing of subsystem 06;
  genset physical state-machine equations, power dynamics and parameters remain unchanged.
- **D-B17**：16 号不消费 dispatch 诊断三件套（D-B14 联动），仅 logging。
- **D-B18**（T-014 amendment 1 更新）：normal path S4 preempts S2（S4 阈值严格低 50 kW——
  prospective margin 不对称）；**专用 deliverability-deficit bench 证实 S2 reason=4 端到端**
  （audit 1→2 @t=45.35 reason=4，injection_start_reason 断言 PASS）；S2 定位 = S4 的
  deliverability/fault/ramp 后备防线。
- **D-B19**（T-014 amendment 2 更新）：scenario contamination fixed（E01b 前恢复 bench
  nominal）；**checker 本身已 fail-closed**（缺字段/尺寸不匹配/计数 0 → 整体 FAIL；元数据
  compared=required=96、skipped=0；44 D7 负向证实旧空洞形态被拒）。
- **D-B20**（新增）：G6/C1 确定性判据 fail-closed 化（REQUIRED 对称并集 / skipped 计数 /
  comparability_pass 元数据 / N_compared=N_required ∧ N_skipped=0 门禁不变量）。

## 4. 基线纪律（DD-9 执行记录）

| Step | 状态 |
|---|---|
| A v1.0 永久保留不回写 | ✓（默认 40 号行为不变） |
| B candidate 生成 + 机器检查 | M1 物理参数 sha 一致 / M2 Model D 四哈希一致 / M3 判据一致性（G3/G4 force bench 逐位=DD-8 证据；G1/G2/G5 结构一致；**G6 fail-closed 专项：comparability_pass ∧ 96/96/0**）/ M4 E10 ok ✓（amendment 2 后重生成） |
| C T-412 against candidate | overall=true（G1~G6 全过；**T-014 amendment 2 后以 fail-closed G6 复验：96/96/0**） |
| D field-level diff | `baseline_v1_0_to_v1_1_diff.json`（本目录） |
| E promotion | **T-014 裁决 = HOLD（2 targeted amendments 已闭合，待最终 APPROVED）**；promotion 时将同步显式保存 `gate_baseline_modele_v1_0.json`（immutable historical）→ `gate_baseline_modele.json` 指向 current v1.1 + 冻结 `gate_baseline_modele_control.json` v1.0（版本化资产三件套，用户建议） |

## 5. 配套产物

- 三臂对比报告：`b_arm_comparison_report.md`（本目录）
- 基线 diff：`baseline_v1_0_to_v1_1_diff.json`（本目录）
- 机器可读结果：`model/simulink_modelE_(Phase4_Physics)/results/gate_control_result.json`
  （overall + C1~C7 133 项 + 三臂/注入指标 + negative_control 6/6）、`results/gate_result.json`（T-412 against candidate）
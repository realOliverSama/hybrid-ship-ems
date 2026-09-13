# Scientific Roadmap Amendment v1 — T-429 FINAL FAIL 后的 contrast/OOD 框架路线裁决

> **状态**: **APPROVED — Final Ruling 路线 (c′)**（用户裁决 2026-09-07；含 R1–R4 强制措辞修正，修正已并入正文；裁决原文全文见附录 A）
> **裁决人**: 用户
> **生效链**: `T429 FINAL_FAIL + Scientific Roadmap Amendment v1 (c′) → T432 downstream unblock`（**不是** `T429 PASS`）
> **证据基线**: `results/mission_v3/t429_final_verdict.json`（self_hash `eb389ae8…`，Day-3 终判，不再第三轮）
> **法律性质**: 本文件是 C 契约 FR-551 预留的合法路径载体——"evidence → spec amendment → 用户批准 → 族/对比语义修订"。本文件不修改任何已冻结结果、不复评任何已判 Gate、不将 T-429 转为 PASS。

**永久状态（Final Ruling 冻结）**：`T429 = FINAL_FAIL`；`AC-437 = NOT_MET`；无第三轮 T-429；无阈值调整；无改标造 PASS；不重生成 T-429 confirmation 数据。负结果证据链完整保留为科学发现（见 §6.2）。

---

## 0. 裁决问题与结果

**问题**：T-429 可辨识性 Gate 终判 FINAL FAIL（0/6）之后，Dataset v3 的 contrast 框架与 OOD 三轴框架按哪条路线继续 D/E/F？

**裁决结果**：**(c′) = 现在执行 (c) + 将 (b1)/(b2) 登记为非关键路径的 Phase 4.5 / E 并行研究项。** 本裁决不重开、不修订、不复评 T-429。

---

## 1. 既判事实（不可重开）

### 1.1 终判与逐项身份

`t429_final_verdict.json`（2026-09-07T05:42:38，self_hash `eb389ae8…`）：

- **overall**: `FINAL FAIL (fail-closed, 0/6 required contrasts PASS)`；
- 逐项身份（冻结）：

| contrast | 终态身份 | 度量（R_lcb95 / BA_lcb95，conformance rerun） |
|---|---|---|
| SEG_BRIDGE_VS_HARBOR | VALID FAIL | 0.6815 / 0.6844 |
| SEG_BERTH_VS_DEPARTURE | VALID FAIL under frozen SBD∧probe rule | 0.8231 / 0.9712 |
| DIR_UPSTREAM_VS_DOWNSTREAM | **INVALID/UNRESOLVED**（matched-segment 实现缺陷） | 0.6252 / 0.6354 |
| DEPTH_LOW_ID_VS_NORMAL_ID | **INSUFFICIENT EVIDENCE** under frozen grouped-CV rule | 0.5461 / probe 无效 fold |
| SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE | VALID FAIL under frozen SBD∧probe rule | 0.6217 / 0.9161 |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | VALID FAIL after pure-host repair | 0.5049 / 0.6648 |

### 1.2 度量与门槛（t429_freeze_v2，盲冻结维持）

- SBD：`1 − max NCC`（z-normalized 等长 1 Hz 切片，零填充 2n FFT 互相关）；判定 `LCB95(R) ≥ R_min = 1.5`（DGN-12 候选值，盲冻结维持）。
- probe：logistic L2（C=1.0, balanced），12 维冻结切片摘要编码，split-group 分组 5 折 CV，判定 `LCB95(BA) ≥ A_min = 0.8`。
- DIR 专用 gate profile：`PASS_DIR = PASS_paired_physical_effect AND PASS_probe`（SBD 为 SECONDARY_DIAGNOSTIC）。

### 1.3 三层发现（按 Final Ruling R1–R3 口径修正后的规范表述）

1. **SBD 度量-因子兼容性不佳（R3 口径）**：预注册的 NCC/SBD 判据对多个 Mission-v3 对比表现出**度量-因子兼容性不佳**——尤其在 probe 可分性强而 SBD 分离弱并存的对比上（BERTH BA_lcb95 0.971 / R_lcb95 0.823、SPRINT 0.916 / 0.622 为最干净的例证）。全部六对比 R_lcb95 ∈ [0.50, 0.82]，低于盲冻结候选值 R_min=1.5；终判将其登记为 **DGN-12 candidate-calibration finding**（度量校准预实验为后续课题，P45-1）。**本条不构成"SBD 对全部六对比被正式证明结构性无效"的结论**；DIR 依其 amendment profile 本属 SBD-not-applicable；DEPTH 的最终证据状态为 insufficient，不作为 SBD 结论载体。
2. **DEPTH 证据不足（R1 口径）**：在当前 Model-E/H1 保真度下，T-429 **未获得深度可辨识性的充分证据**（严格 layer selector 下，冻结贪心 5-fold 出现单类 fold，probe INSUFFICIENT）。该结果**与可能存在可辨识性限制相一致，但不证明观察等价、也不构成"已证不可辨"的否定性结论**。终态身份 = `INSUFFICIENT_EVIDENCE under frozen grouped-CV rule`。v2 生成前冻结条款（`depth_still_fail_legal_cohort: "accept possible depth-identifiability limit at current Model-E/H1 fidelity"`）预接受的正是这一"可能限制"语义。
3. **DIR 未决（R2 口径）**：终态 `INVALID/UNRESOLVED`（matched-segment 组级聚合实现缺陷致配对符号反转：delta_point −94.3 kW、sign_test k=14/60）。D1-A 配对诊断（ΔP +83.8 kW / BA 0.944）为 **development-diagnostic 证据（非 Gate 证据）**，为轴 A 提供物理动机；跨航次 pooled probe（BA 0.59–0.64）不适用于该因子，paired cohort probe 设计为 v2 遗留课题。

### 1.4 已冻结的失败后果条款（T429-AMENDMENT-V2 生成前冻结）

`"insufficient_coverage": "STOP, no third-round top-up"`；`"depth_still_fail_legal_cohort": "accept possible depth-identifiability limit at current Model-E/H1 fidelity"`。

### 1.5 C 契约红线（FR-551）

必需对比失败的唯一合法修复是 Mission/scenario 生成重设计；禁止降阈值/删对比/并族改标签/事后选易切片/见结果改特征视图；语义修订唯一合法路径 = evidence → spec amendment → 用户批准 → 族/对比语义修订。本文件即按该路径批准。

---

## 2. 影响评估：什么受影响、什么不受影响

### 2.1 不受影响（任何路线下）

| 项 | 依据 |
|---|---|
| Dataset v3 核心任务（H1 P_load_total 30×10 s 多步预测）的数据效用 | 载荷预测数据本身不依赖场景标签可辨识性；prequal(108)/t429conf_v2(144) 经 formal C/D Gate 十项检查 overall=true（EXT-CD-GATE，2026-09-07） |
| C/D 接口与既有交付物 | producer schema / registry 冻结；语料两跑 hash 逐对一致 |
| IID formal fleet 生成机制（T-019 Stage B） | strata 多样性是描述性覆盖，不需要对比可辨识性资格 |
| OOD 三轴定义（DGN-11-v1，轴 A 强流 / 轴 B 低水位 / 轴 C 组合） | `audit_ood.py` 消费 OOD 轴常量，**不消费 contrast registry**——两框架机制独立 |
| 轴 A / 轴 C 的框架地位 | 定义不变。轴 A：DIR 配对**发展性诊断证据提供物理动机**；更大流速的 OOD 是否产生更大目标层 shift 是 **Phase-E/T-443 的显式假设，非既定结论**。轴 C：组合外推具有结构性构成差异 |

### 2.2 受影响

1. **contrast 层科学主张**：六对比"人为场景标签 → 独立负荷模式"的可辨识性主张在预注册度量与现行签名下未获支持（4 VALID FAIL + DIR 未决 + DEPTH 证据不足）。论文不得声称"六类工况对比可辨识"。
2. **OOD 轴 B 的 target-level shift 预期**：DEPTH 证据不足结果（support 内 HT [1.5,2.0] vs [2.2,2.6] 未获充分证据）提示轴 B（H/T ∈ [1.3, 2.0)）的 target 层 shift 可能较弱——但轴 B 外推区间比 DEPTH 对比所测更深（下探 1.3），浅水阻力随 H/T 减小非线性增长，且 UKC 限速在浅水改变工况构成。**DEPTH 证据不足既不推出轴 B 无信号，也不推出轴 B 必有信号**；轴 B 是本 amendment §7 诊断框架下的显式 known-risk case。
3. **E 包 T-443 的解释框架**：若轴 B 上出现 OOD≈IID，必须能区分"模型稳健"与"目标层无 shift 可检"——Final Ruling 已批准 T-443 增补强制 OOD shift 双诊断要求（见 §7）。
4. **论文叙事**：Phase 4 Paper Gate（T-455）中 "history-only → route/environment-aware 提升长 horizon/OOD" 的主张须与 contrast 层负结果一致措辞，并以 §6.2 证据链呈现。

### 2.3 关键辨析：contrast 框架 ≠ OOD 三轴

- contrast 框架是 **C 侧生成器资格 Gate**（评价构造：同 cohort 对比切片的 SBD/probe/配对效应），对象是 prequal/confirmation 语料，不进入 formal fleet 的逐航次标注（formal 语料 meta 携带 stratum/intervention/layer/depth_class 等描述性字段）；
- OOD 三轴是 **D 侧外推域定义**（FR-485/486：outside training support ∩ inside validated physical domain），数值与 counts/seeds 在 T-432 activation record 冻结（checklist ⑥）。
- 因此 T-429 FAIL 不机械阻塞 T-019 的任何机器环节；其下游解锁的法律依据由本 amendment 提供（§0 生效链）。

---

## 3. 候选路线（裁决过程记录）

### 3.1 (a) 照原设计继续
零改动、负结果入档；代价：轴 B 弱信号风险在 E 侧不可解释（T-443 现规划无 shift 诊断），届时被迫被动补丁。**未获采纳。**

### 3.2 (b) 信号层补强 → 重设计 T-429
(b1) SBD 度量校准预实验；(b2) P_prop 专用通道评测签名（`P_prop_demand_kW` 已在 truth_stream 契约导出中）；之后新冻结协议 + 新语料 + 新 Gate 实例（T-429-v3，非"第三轮"，但需显式延拓 Day-3 ruling 适用域）。改动面：C spec amendment + FR-499 最早受影响门禁重开 + registry 修订 + 新语料。仿真便宜（≈122 航次/h）而真瓶颈在协议与门禁周期；且 DEPTH 即便换签名也可能仍无法获得充分证据。**未获采纳为当前路线**——(b1)/(b2) 转为 §8 研究项。

### 3.3 (c) 对比框架降级重定义（已按 (c′) 批准执行）

1. **contrast 层降级**：从 Dataset-v3 资格 Gate 降为 **DESCRIPTIVE / DIAGNOSTIC SCIENTIFIC EVIDENCE LAYER**。既有证据与 provenance 永久保留；SBD/probe/pairing 输出保留为描述性科学证据；任何已冻结 T-429 结果不得改动。contrast 结果**不得**用于：formal Dataset-v3 航次的准入/拒绝；声称六语义条件全部独立可辨识；要求 Phase-E predictor 分类 mission 标签；自动推断某 OOD 轴必产生 target 层 shift。
2. **描述性 probe 身份（R4 冻结，禁事后新造强/中/弱门槛）**：BERTH/DEPARTURE = strong descriptive probe separability；SPRINT/RETURN = strong descriptive probe separability；BRIDGE/HARBOR = below frozen probe criterion；TRAFFIC/FREE = below frozen probe criterion after pure-host repair；DIR = unresolved；DEPTH = insufficient evidence。
3. **DIR** 保持 INVALID/UNRESOLVED 搁置（matched-segment 修复 + paired cohort probe 设计列后续项，不进关键路径）。**DEPTH** 降为诊断性对比（依据 = §1.3-2 与 §1.4 预接受条款）。
4. **OOD 三轴定义不动**（DGN-11-v1）。activation 冻结 exact counts 依据：① 所需统计/评测精度；② 预声明 OOD 条件/token 数；③ 可复现 seed/allocation 纪律——**不依据预期效应强度**。不得因轴 B 预期弱 target shift 削减其 counts；轴 C 可因 token 数获得更大总量（condition-count-driven allocation，非 result-driven A/C 偏好）。Exact counts 属 activation record 决定。
5. **E 包 T-443 增补**：见 §7（Final Ruling 已即席批准，E spec 开门时落实）。

### 3.4 (c′) = 批准路线
§3.3 全部内容 + §8 Phase 4.5 研究项登记。

---

## 4. 主 Agent 推荐（历史记录；裁决已采纳并附加修正）

起草时推荐 (c′)，理由（措辞已按 R1–R3 修正）：

1. 载荷预测数据集的核心效用不依赖场景标签可辨识性资格，contrast 层资格失败不应阻塞 T-019 → T-020 → E/F；
2. 负结果本身有论文价值：(i) SBD 度量-因子兼容性发现（probe 强可分与 SBD 弱分离并存的干净例证）+ 度量校准课题；(ii) DIR 配对发展性诊断证据与 pooled 设计不适用性；(iii) DEPTH "可能可辨识性限制"在 v2 预接受条款下的合法入档；
3. (b) 的收益可后补，且 DEPTH 即便补做也可能仍无法获得充分证据；
4. (a) 留下轴 B 不可解释缺口；
5. (c′) 完全在 FR-551 合法路径内，不重开 C 门禁、不触碰冻结数值。

Final Ruling 采纳 (c′)，并附加 R1–R4 措辞修正、§7 T-443 诊断要求冻结与 §8 研究项登记。

---

## 5. 裁决记录（已填写）

```
裁决人: realOliverSama（用户）          日期: 2026-09-07

选定路线: (c′) = 现在执行 (c) + (b1)/(b2) 登记为 Phase 4.5 / E 并行研究项

附加裁决（正文各节落实）:
  R1  DEPTH 措辞: 终态 INSUFFICIENT_EVIDENCE——"未获得充分证据，与可能的可辨识性限制
      相一致，但不证明观察等价"（§1.3-2 已修正）
  R2  DIR/轴 A 措辞: D1-A 为 development-diagnostic 证据；更大流速 OOD 的更强 shift 是
      Phase-E/T-443 显式假设而非既定结论（§1.3-3、§2.1 已修正）
  R3  SBD 措辞: 度量-因子兼容性不佳（BERTH/SPRINT 为最干净例证），非"全组结构性无效
      证明"（§1.3-1 已修正）
  R4  probe 描述身份冻结: strong / below frozen criterion / unresolved / insufficient
      四类，禁事后新造门槛（§3.3-2 已落实）
  OOD counts 分配原则: 按精度+条件数+seed 纪律，不按预期效应强度；轴 B counts 不削减；
      轴 C 可 condition-count-driven 扩大（§3.3-4 已落实）
  E spec T-443 诊断要求: 即席批准冻结（§7）
  Phase 4.5: P45-1/P45-2 登记且不阻塞关键路径（§8）

生效: 本文件 R1–R4 修正提交后即 APPROVED；T-432 activation =
      UNBLOCKED_BY_APPROVED_ROADMAP_AMENDMENT，无需再一次评审。
```

裁决原文全文见**附录 A**（含全部条款原文，具同等效力）。

---

## 6. 生效状态与下游解锁

### 6.1 状态表（Final Ruling §6）

| 项 | 状态 |
|---|---|
| Scientific Roadmap Amendment v1 | **APPROVED**（路线 c′） |
| T-429 | 保持 `FINAL_FAIL` |
| AC-437 | 保持 `NOT_MET`（C spec 注记，见 §9） |
| contrast 层 | `DESCRIPTIVE_DIAGNOSTIC` |
| OOD DGN-11-v1 | 不变 |
| Dataset-v3 producer/interface | 不变 |
| T-432 activation | `UNBLOCKED_BY_APPROVED_ROADMAP_AMENDMENT` |
| T-019 Stage-B formal fleet / formal Dataset-v3 路径 | 正常完成 T-432 activation checklist 后即可进行 |
| E / F | 仍处于正常 Dataset-v3 freeze 序列下游 |

**本 amendment 不将 T-429 转化为 PASS。**

### 6.2 负结果证据链（论文保留，作为科学发现而非工程缺陷擦除）

```
T429 v1 FAIL → mechanism diagnostics（D1-A/D1-B）→ AMENDMENT_V2 →
fresh confirmation（T429_CONFIRMATION_V2, 144×2 两跑一致）→
evaluator conformance repair（EXECFIX-01）→ FINAL overall FAIL →
roadmap semantic downgrade (c′)
```

---

## 7. E 包 T-443 增补 — OOD shift 诊断强制要求（Final Ruling 即席批准）

E specification 开门时（brainstorm/design 阶段）加入以下**强制**要求；本 amendment 只冻结诊断要求本身，确切的统计 shift 度量须在 Phase-E spec 流程中**选择并预注册**，禁止事后依 OOD 结果挑选。

对**每个 OOD 轴**，T-443 SHALL 分别报告：

1. **输入空间 shift 诊断**——合法 predictor 输入（H1/K/F）在 IID 与 OOD 域之间是否实际 shift；
2. **目标空间 shift 诊断**——未来 P_load 目标分布/轨迹统计在 IID 与 OOD 域之间是否实际 shift；
3. **predictor 性能退化**——冻结评测协议下的 IID vs OOD 预报误差。

解释至少区分四种情形：

| 情形 | 解释约束 |
|---|---|
| X shifts，Y shifts，误差不变 | 与 predictor 鲁棒性相一致的证据 |
| X shifts，Y barely shifts，误差不变 | **不得**仅凭 OOD≈IID 误差声称鲁棒性 |
| X shifts，Y shifts，误差恶化 | 真实 OOD 退化 |
| X barely shifts | 审计该名义 OOD 轴是否实际进入 predictor 信息空间 |

**Axis B 为本诊断框架下的显式 known-risk case。**

---

## 8. Phase 4.5 / 并行研究项登记（非关键路径）

### P45-1 — SBD / contrast-metric calibration
研究幅度敏感或其他因子兼容的对比度量，并在未来任何判据冻结前完成校准。**不得修改或复评 T-429 v1/v2。**

### P45-2 — P_prop identifiability study
研究 `P_prop_demand_kW` 或其他推进/阻力专用信号是否暴露在 H1 `P_load_total` 中弱或未决的深度/方向机制。属新科学研究：自有冻结协议 + 新证据。**不得**将 P_prop 静默提升为 Phase-E predictor 主输入角色；任何 predictor 角色变更须走独立 D/E 契约 amendment。

P45-1 与 P45-2 均不阻塞 Dataset-v3 formal 生成、Phase E、Phase F。

---

## 9. 执行清单（路线 c′）

| 步骤 | 状态 |
|---|---|
| 本文件 R1–R4 修正 + 状态 APPROVED | ✅（本提交） |
| C spec AC-437 注记（终态 NOT_MET + 本 amendment 引用） | ✅（本提交，`.specs/phase4-c-mission-v3/requirements.md`） |
| contrast registry / 生成器 / 语料 / 阈值数值 | 不动（零变更） |
| T-432 activation record 填值（fleet 规模 / strata / seeds / OOD 轴 A 数值与 counts〔按 §3.3-4 分配原则〕/ 合格判据）→ checklist ⑤⑥ → 用户批准 | ⏳ 下一步（activation session） |
| T-018 Stage A（≈100 qualification，73 号施工） | 待 checklist 全绿 |
| T-019 Stage B / T-020（T-435-formal）→ Dataset-v3 Freeze | 待其依赖链 |
| E spec 开门时落实 §7 T-443 诊断要求 | 待 E 包开工 |
| P45-1 / P45-2 登记 | ✅（§8） |
| 项目开发进展.md 裁决记录 | ✅（本提交） |

---

## 附录 A — Final Ruling 原文（用户裁决，2026-09-07，逐字）

# Scientific Roadmap Amendment v1 — Final Ruling

## Decision

**APPROVED ROUTE: (c′)**

`(c′) = execute (c) now + register (b1)/(b2) as non-critical-path Phase 4.5 / E-parallel research items.`

This ruling does NOT reopen, revise, or re-evaluate T-429.

Permanent status:

- `T429 = FINAL_FAIL`
- `AC-437 = NOT_MET`
- no third T-429 round
- no threshold adjustment
- no re-labelling to manufacture PASS
- no regeneration of T-429 confirmation data

The downstream legal basis becomes:

`T429 FINAL_FAIL + Scientific Roadmap Amendment v1 (c′) -> T432 downstream unblock`

not `T429 PASS`.

After the documentation corrections below are committed, proceed directly to T-432 activation without another review.

---

## 1. Contrast framework

Downgrade the C-side contrast framework from a Dataset-v3 qualification Gate to a:

`DESCRIPTIVE / DIAGNOSTIC SCIENTIFIC EVIDENCE LAYER`

Keep all existing evidence and provenance permanently.

Do not use contrast results to:

- admit/reject formal Dataset-v3 voyages;
- claim that all six semantic conditions are independently identifiable;
- require the Phase-E predictor to classify mission labels;
- infer automatically that an OOD axis must produce target-level shift.

Retain SBD/probe/pairing outputs as descriptive scientific evidence.

No frozen T-429 result may be altered.

---

## 2. Required wording corrections before marking this amendment APPROVED

### R1 — DEPTH

Do not state that DEPTH was proven unidentifiable or that final BA was random-level.

Final identity remains:

`INSUFFICIENT_EVIDENCE under frozen grouped-CV rule`.

Use wording equivalent to:

“Under current Model-E/H1 fidelity, T-429 did not obtain sufficient evidence for depth identifiability. This is consistent with a possible identifiability limitation but does not prove observational equivalence.”

Correct §1.3, §4, and any downstream wording that currently upgrades this to a proven negative result.

### R2 — DIR / Axis A

DIR final status remains:

`INVALID/UNRESOLVED`.

The D1-A `+83.8 kW / BA=0.944` result is development-diagnostic evidence only.

Do not state as an established result that Axis-A extrapolation to higher current "will have a stronger signal".

Use wording equivalent to:

“Paired development evidence provides a physical motivation for Axis A. Whether larger-current OOD produces a larger target-space shift remains an explicit Phase-E/T-443 hypothesis.”

### R3 — SBD

Do not claim that SBD was formally proven structurally invalid for all six contrasts.

Use wording equivalent to:

“The preregistered NCC/SBD criterion showed poor metric-factor compatibility for multiple Mission-v3 contrasts, particularly where strong probe separability coexisted with weak SBD separation.”

BERTH and SPRINT are the cleanest supporting examples.

DIR remains SBD-not-applicable under its amended profile; DEPTH final evidence remains insufficient.

### R4 — descriptive probe language

Do not invent new post-hoc "strong / medium / weak" thresholds.

Recommended descriptive identities:

- BERTH/DEPARTURE: strong descriptive probe separability;
- SPRINT/RETURN: strong descriptive probe separability;
- BRIDGE/HARBOR: below frozen probe criterion;
- TRAFFIC/FREE: below frozen probe criterion after pure-host repair;
- DIR: unresolved;
- DEPTH: insufficient evidence.

---

## 3. OOD three-axis framework

Keep DGN-11-v1 axes unchanged:

- Axis A: strong-current extrapolation;
- Axis B: low-depth extrapolation;
- Axis C: compositional OOD.

Do NOT reduce Axis-B counts merely because current evidence suggests it may have a weak target-level shift.

At T-432 activation, freeze exact counts according to:

1. required statistical/evaluation precision;
2. number of predeclared OOD conditions/tokens;
3. reproducible seed/allocation discipline;

not according to expected effect strength.

Axis C may have a larger total count if this follows from providing adequate counts to each predeclared compositional token. This is condition-count-driven allocation, not result-driven A/C preference.

Exact counts remain an activation-record decision.

---

## 4. Phase-E T-443 amendment — APPROVED NOW

Add a mandatory OOD shift-diagnostic requirement when the E specification is opened.

For every OOD axis, T-443 SHALL separately report:

1. **input-space shift diagnostics**
   - whether legal predictor inputs H1/K/F actually shift between IID and the OOD domain;

2. **target-space shift diagnostics**
   - whether the future P_load target distribution/trajectory statistics actually shift between IID and the OOD domain;

3. **predictor performance degradation**
   - IID vs OOD forecast error under the frozen evaluation protocol.

The interpretation must distinguish at least:

- X shifts, Y shifts, error does not worsen -> evidence consistent with predictor robustness;
- X shifts, Y barely shifts, error does not worsen -> do NOT claim robustness solely from OOD≈IID error;
- X shifts, Y shifts, error worsens -> genuine OOD degradation;
- X barely shifts -> audit whether the nominal OOD axis meaningfully enters the predictor information space.

This amendment freezes the diagnostic requirement only.

Exact statistical shift metrics SHALL be selected and preregistered during the Phase-E brainstorm/design/spec process; do not select them post hoc from OOD outcomes.

Axis B is an explicitly known-risk case for this diagnostic.

---

## 5. Phase 4.5 / parallel research items

Register, outside the current critical path:

### P45-1 — SBD / contrast-metric calibration

Investigate amplitude-sensitive or otherwise factor-compatible contrast metrics and perform calibration before freezing any future criterion.

This work must not modify or re-evaluate T-429 v1/v2.

### P45-2 — P_prop identifiability study

Investigate whether `P_prop_demand_kW` or another propulsion/resistance-specific signal exposes depth/directional mechanisms that were weak or unresolved in H1 `P_load_total`.

This is a new scientific study with its own frozen protocol and new evidence.

Do not silently promote P_prop into the main Phase-E predictor input role. Any downstream predictor-role change requires its own D/E contract amendment.

Neither P45-1 nor P45-2 blocks Dataset-v3 formal generation, Phase E, or Phase F.

---

## 6. Downstream status after approval

After R1-R4 documentation corrections:

- Scientific Roadmap Amendment v1 -> `APPROVED`
- selected route -> `(c′)`
- T-429 -> remains `FINAL_FAIL`
- AC-437 -> remains `NOT_MET`
- contrast layer -> `DESCRIPTIVE_DIAGNOSTIC`
- OOD DGN-11-v1 -> unchanged
- Dataset-v3 producer/interface -> unchanged
- T-432 activation -> `UNBLOCKED_BY_APPROVED_ROADMAP_AMENDMENT`
- T-019 Stage-B formal fleet / formal Dataset-v3 path -> may proceed after normal T-432 activation checklist completion
- E/F -> remain downstream of the normal Dataset-v3 freeze sequence

Do not represent this amendment as converting T-429 into a PASS.

Preserve the complete negative-result evidence chain for the paper:

`T429 v1 FAIL -> mechanism diagnostics -> AMENDMENT_V2 -> fresh confirmation -> evaluator conformance repair -> FINAL overall FAIL -> roadmap semantic downgrade (c′)`.

This negative result is retained as a scientific finding, not erased as an engineering defect.

（附录 A 完）

---

## 附录 B — 本文件引用的证据文件索引

| 文件 | self_hash / 标识 |
|---|---|
| `results/mission_v3/t429_final_verdict.json` | eb389ae8… |
| `results/mission_v3/identifiability_gate_result.json` | EXECFIX-01 conformance rerun（2026-09-06T18:33:58Z） |
| `results/mission_v3/t429_amendment_v2.json` | 796a8b07…（frozen_before_generation=true） |
| `results/mission_v3/t429_freeze_v2.json` | R_min=1.5 / A_min=0.8 盲冻结 |
| `results/mission_v3/d1a_dir_paired_diagnostic.json` / `d1b_traffic_overlap_diagnostic.json` | D1 双诊断（DEVELOPMENT_DIAGNOSTIC） |
| `config/mission_v3/identifiability_contrast_registry.json` | 2274da4a…（version 1.0.0-candidate） |
| `results/mission_v3/prequal_amendment_01.json` | a0484664… |
| `项目开发进展.md` 2026-09-06 / 09-06（续）/ 09-07 条目 | 三层发现 / 终判 / EXT-CD-GATE / Final Ruling |
| `.specs/phase4-c-mission-v3/requirements.md` FR-547~551 / AC-437 | C 契约红线 |
| `docs/phase4_research_roadmap.md` E 包 T-436~T-444 / F 包 T-445~T-455 | 下游消费 |
| `.specs/phase4-d-dataset-v3/design.md` DGN-11 / requirements FR-485/486/499 | OOD 三轴与 Gate reopening 纪律 |

# Scientific Roadmap Amendment v1 — T-429 FINAL FAIL 后的 contrast/OOD 框架路线裁决

> **状态**: DRAFT — 待用户裁决（本文件 §5 为裁决记录栏）
> **裁决人**: 用户
> **裁决 deadline**: T-019 Stage B formal fleet 生成前（activation record 填值之前）
> **触发依据**: `HANDOFF_D_SIDE_SESSION.md` §2.4；`c_producer_interface_contract.md` §7；`t429_final_verdict.json` downstream.note
> **证据基线**: `results/mission_v3/t429_final_verdict.json`（self_hash `eb389ae8…`，Day-3 终判，不再第三轮）
> **法律性质**: 本文件是 C 契约 FR-551 预留的合法路径载体——"确证物理/观察等价者唯一合法路径 = evidence → spec amendment → 用户批准 → 族/对比语义修订"。本文件不修改任何已冻结结果、不复评任何已判 Gate。

---

## 0. 裁决问题（一句话）

**T-429 可辨识性 Gate 终判 FINAL FAIL（0/6）之后，Dataset v3 的 contrast 框架与 OOD 三轴框架按哪条路线继续 D/E/F？**

四条候选：(a) 照原设计继续；(b) 信号层补强后重设计 T-429；(c) 对比框架降级重定义；(c′) = (c) 现在执行 + (b) 要素转为后续研究项（主 Agent 推荐，见 §4）。

---

## 1. 既判事实（不可重开）

以下事实已终判/冻结，任何路线下均不变：

### 1.1 终判与逐项身份

`t429_final_verdict.json`（2026-09-07T05:42:38，self_hash `eb389ae8…`）：

- **overall**: `FINAL FAIL (fail-closed, 0/6 required contrasts PASS)`；
- **ruling**: "Day-3 FINAL Ruling: … no third T-429 round; T-432 not unlocked"；
- 逐项身份（冻结）：

| contrast | 身份结论 | 度量（R_lcb95 / BA_lcb95，conformance rerun） |
|---|---|---|
| SEG_BRIDGE_VS_HARBOR | VALID FAIL | 0.6815 / 0.6844 |
| SEG_BERTH_VS_DEPARTURE | VALID FAIL under frozen SBD∧probe rule | 0.8231 / **0.9712** |
| DIR_UPSTREAM_VS_DOWNSTREAM | **INVALID/UNRESOLVED** due matched-segment implementation defect | 0.6252 / 0.6354（配对诊断 0.944，见 §1.3） |
| DEPTH_LOW_ID_VS_NORMAL_ID | **INSUFFICIENT EVIDENCE** under frozen grouped-CV rule | 0.5461 / probe 无效 fold |
| SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE | VALID FAIL under frozen SBD∧probe rule | 0.6217 / **0.9161** |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | VALID FAIL after pure-host repair | 0.5049 / 0.6648 |

### 1.2 度量与门槛（t429_freeze_v2，盲冻结维持）

- SBD：`1 − max NCC`（z-normalized 等长 1 Hz 切片，零填充 2n FFT 互相关；Paparrizos & Gravano 2015 语义）；判定 `LCB95(R) ≥ R_min = 1.5`（DGN-12 候选值，来源注明"盲冻结维持（无预资格期证据支持调整）"）。
- probe：logistic L2（C=1.0, balanced），12 维冻结切片摘要编码，split-group 分组 5 折 CV，train-only scaler，判定 `LCB95(BA) ≥ A_min = 0.8`。
- DIR 专用 gate profile：`PASS_DIR = PASS_paired_physical_effect AND PASS_probe`（SBD 为 SECONDARY_DIAGNOSTIC）。

### 1.3 三层根因（Day-3 裁决采纳，`项目开发进展.md` 2026-09-06 逐字）

1. **SBD 度量-负载空间失配**：1−max NCC 幅度不变距离对全部六对比（R 0.50-0.82）系统性低于 R_min=1.5——包括探针 0.92-0.97 的强信号对比。DGN-12 候选值 1.5 的乐观预设是根因；未来应先做度量校准预实验再冻结。
2. **DEPTH 可辨识性负结果**：合法 token-safe 条件层（WORK×DRY 12+12 组）上 H1 P_load 签名在浅水阻力增量层面不可辨（BA≈随机）。可能需要 P_prop 或阻力专用信号而非 P_load_total。
3. **DIR 配对机理已证但 pooled probe 不适用**：方向效应真实（配对 BA 0.944、ΔP +83.8 kW，D1-A 诊断）但跨航次 pooled probe 被 regime/负载方差淹没——paired cohort probe 设计是 v2 遗留课题；conformance 重跑中组级聚合的 matched-segment 实现缺陷把配对符号反转（delta_point −94.3 kW，sign_test k=14/60）。

### 1.4 已冻结的失败后果条款（T429-AMENDMENT-V2 预接受）

`t429_amendment_v2.json` v2_fail_consequences_frozen（生成前冻结）：

- `"insufficient_coverage": "STOP, no third-round top-up"`；
- `"depth_still_fail_legal_cohort": "accept possible depth-identifiability limit at current Model-E/H1 fidelity"`——**DEPTH 在当前保真度下的可辨识性限制已被生成前条款预接受为合法结局**。

### 1.5 C 契约红线（FR-551，本裁决的边界）

> 必需对比失败的唯一合法修复是 Mission/scenario 生成重设计；**禁止降阈值/删对比/并族改标签/事后选易切片/见结果改特征视图**；确证物理/观察等价者唯一合法路径 = evidence → spec amendment → 用户批准 → 族/对比语义修订。

即：任何"把 v2 判 PASS"的动作非法；对框架语义的修订只能走本文件这样的 amendment + 用户批准路径；改特征视图（P_prop 签名）同样须 amendment 且**不得用于复评 v2**，只能配新冻结协议与新语料。

---

## 2. 影响评估：什么受影响、什么不受影响

### 2.1 不受影响（任何路线下）

| 项 | 依据 |
|---|---|
| Dataset v3 核心任务（H1 P_load_total 30×10 s 多步预测）的数据效用 | 载荷预测数据本身不依赖场景标签可辨识性；prequal(108)/t429conf_v2(144) 语料经 formal C/D Gate 十项检查 overall=true（EXT-CD-GATE，2026-09-07） |
| C/D 接口与既有交付物 | producer schema / registry 冻结；语料两跑 hash 逐对一致 |
| IID formal fleet 生成机制（T-019 Stage B） | strata 多样性是描述性覆盖，不需要对比可辨识性资格 |
| OOD 三轴定义（DGN-11-v1，轴 A 强流 / 轴 B 低水位 / 轴 C 组合） | `audit_ood.py` 消费 OOD 轴常量，**不消费 contrast registry**——两框架机制独立 |
| 轴 A / 轴 C 的 OOD 信号预期 | DIR 配对效应真实（+83.8 kW @ |U|≤2.0）；轴 A 外推至 2.8 m/s 信号更强；轴 C 组合外推有结构性构成差异 |

### 2.2 受影响

1. **contrast 层科学主张**：六对比"人为场景标签 → 独立负荷模式"的可辨识性主张在当前度量与签名下不可得（4 VALID FAIL + DIR 未决 + DEPTH 证据不足）。论文不能声称"六类工况对比可辨识"。
2. **OOD 轴 B 的 target-level shift 预期**：DEPTH 负结果（support 内 HT [1.5,2.0] vs [2.2,2.6] 在 P_load 签名不可辨）弱化轴 B（H/T ∈ [1.3, 2.0)）的信号预期。**但注意数值衔接的 nuance**：轴 B 外推区间比 DEPTH 对比测的 support 内区分更浅（下探 1.3），浅水阻力随 H/T 减小非线性增长，且 UKC 限速在浅水改变工况构成（速度-负荷联合迁移）——DEPTH 负结果**不直接推出**轴 B 无信号，只是把"轴 B 可能弱信号"升级为需要在 E 侧可解释的已知风险。
3. **E 包 T-443（Predictor OOD evaluation）的解释框架**：若轴 B 上出现 OOD≈IID，必须能区分"模型稳健"与"目标层无 shift 可检"——否则结论不可解释。这是路线 (a) 下不被现有 E 规划覆盖的缺口（roadmap T-443 目前只有 "unseen current / unseen water depth / unseen mission composition" 评测语句）。
4. **论文叙事**：Phase 4 Paper Gate（T-455）中 "history-only → route/environment-aware 提升长 horizon/OOD" 的主张需要与 contrast 层负结果一致地措辞。

### 2.3 关键辨析：contrast 框架 ≠ OOD 三轴

- contrast 框架是 **C 侧生成器资格 Gate**（评价构造：同 cohort 对比切片的 SBD/probe/配对效应），其对象是 prequal/confirmation 语料，**不进入 formal fleet 的逐航次标注**（formal 语料 meta 携带 stratum/intervention/layer/depth_class 等描述性字段，实测 prequal/t429conf 全部 `ood_axis='NONE'`）；
- OOD 三轴是 **D 侧外推域定义**（FR-485/486：outside training support ∩ inside validated physical domain），数值与 counts/seeds 在 T-432 activation record 冻结（checklist ⑥）。
- 因此 T-429 FAIL **不机械阻塞** T-019 的任何机器环节；它阻塞的是科学路线选择——在什么框架主张下生成与消费 formal fleet。这正是本裁决要回答的。

---

## 3. 候选路线

### 3.1 (a) 照原设计继续

- **内容**：contrast/OOD 框架零改动。T-429 负结果作为已知限制入档（已有：终判 + dev log + contract §7 注记）。activation record 按 DGN-11-v1 填 OOD 数值，formal fleet 照常生成。
- **改动面**：零（无 C/D spec 修改）。
- **法律路径**：无需 amendment（负结果已合法入档）。
- **日程**：批准 activation record 即解锁 T-019 预注册。
- **科学代价**：contrast 层主张缺失（无替代证据层）；轴 B 弱信号风险在 E 侧**不可解释**（T-443 现规划无 shift 诊断双报告，OOD≈IID 无法归因）。
- **风险**：E 阶段若轴 B 结果含糊，届时被迫回头打补丁（等于晚做的、更仓促的 (c)）。

### 3.2 (b) 信号层补强 → 重设计 T-429

- **内容**：两个子项——
  - (b1) **SBD 度量校准预实验**：R_min=1.5 是 DGN-12 乐观候选（终判 scientific_findings 已定位为 "metric-loadspace mismatch … candidate-calibration finding for future work"）；校准实验重定度量/门槛（或对水平移位型对比改用幅度敏感度量）。
  - (b2) **P_prop 专用通道**：T-429 的 signal_view 从 P_load_total 换为 P_prop_demand_kW / 阻力类信号（该列已在 truth_stream 契约导出中；DEPTH 阻力增量的理论落点 = P_prop 层，经 `04_Load_Measurement` 求和进入 P_load 后被方差淹没）。
  - 之后：新冻结协议（registry 语义修订）→ 新语料 → **新 Gate 实例（T-429-v3）**。
- **与 Day-3 "不第三轮" 裁决的关系**：ruling 禁止的是**同一协议下**的补跑/top-up（v2_fail_consequences_frozen 的 no third-round top-up）。协议重设计后的新 Gate 实例不是"第三轮"，但需要用户在本裁决中**显式豁免/延拓**该 ruling——这是本路线额外的裁决负担。
- **改动面**：C spec amendment（FR-551 路径 + FR-499：对比语义属实质漂移，须重开最早受影响的 C 门禁——brainstorm/design 局部重审）；contrast registry 修订（self_hash 链变更）；新 confirmation 语料 + 新 Gate。
- **日程**：仿真本身便宜（t429conf_v2 规模 144 航次 ≈ 2.44 h/pass，吞吐 122 航次/h；formal 500~1000 航次 ≈ 4~8 h/pass），**真瓶颈是协议重设计 + 门禁重开 + 验证周期**（以 C 包 Day-1~3 的 amendment 经验为锚，量级为天~周）。
- **科学收益**：contrast 层主张可能最严谨（probe 已示 BERTH 0.97 / SPRINT 0.92 强信号——换度量后大概率可判 PASS）。
- **风险**：(i) 关键路径（T-019 → T-020 → Dataset-v3 Freeze → E/F 开工）整体后移；(ii) **DEPTH 大概率仍是负结果**——v2 冻结条款已预接受该限制；若 P_prop 通道也证不出，则最终仍要执行 (c) 的降级动作，只是晚做且多付一轮语料+Gate 成本；(iii) FR-551 明禁"见结果改特征视图"，(b2) 的合法性完全依赖 amendment 先行 + 新语料不复评 v2，流程纪律负担最重。

### 3.3 (c) 对比框架降级重定义（FR-551 合法路径内的语义修订）

- **内容**：
  1. contrast 层从"Gate 资格"降为**描述性双层证据**：(i) probe 可辨识性证据（既有计算：BERTH 0.97 / SPRINT 0.92 为强证据；BRIDGE 0.68 / TRAFFIC 0.66 为中等）；(ii) SBD 保留为形状距离诊断（非 gate 判据），附数学论证（幅度不变距离对水平移位型因子结构性不敏感——这是度量性质，非经验偶然）。**不复评 v2、不改任何门槛数值、终判 FAIL 永久保留**。
  2. DIR 保持 INVALID/UNRESOLVED 搁置（实现缺陷已登记；matched-segment 修复 + paired cohort probe 设计列为后续项，不进本 Phase 关键路径）。
  3. DEPTH 降为诊断性对比，依据 = v2 生成前冻结条款已预接受的 "depth-identifiability limit at current Model-E/H1 fidelity"。
  4. OOD 三轴定义不动（DGN-11-v1）；activation 时由用户决定 counts 是否向轴 A/C 倾斜（轴 B 保留但预期弱信号如实预注册）。
  5. **E 包开工时 T-443 增补要求**（写入 E spec，届时走其四门禁）：OOD 评测必须双报告输入空间 shift 与目标空间 shift 诊断，使 "OOD≈IID" 可归因（模型稳健 vs 无 shift 可检）。
- **改动面**：本文件（批准即生效）+ C spec AC-437 状态注记（终判 FAIL 不改判，补"框架降级依据本 amendment"引用）+ E spec 开工时的 T-443 增补。不动生成器、注册表数值、语料。
- **法律路径**：FR-551 明文的唯一合法路径（evidence → spec amendment → 用户批准 → 族/对比语义修订）。
- **日程**：批准即解锁 T-019 预注册。
- **科学代价**：放弃"六对比可辨识"主张（该主张在现行度量下本就不可得）；论文以"负结果 + 描述性证据"呈现 contrast 层。
- **风险**：答辩/审稿可能问"为何不修度量再判一次"——回应已在案（幅度不变性的数学论证 + v2 预接受条款 + 度量校准列为后续研究项）；需在论文写作时显式组织这段论证。

### 3.4 (c′) 推荐：分阶段组合 = 现在 (c) + (b) 要素转为后续研究项

= §3.3 全部内容，另将 (b1) SBD 度量校准预实验与 (b2) P_prop 通道可辨识性研究登记为 **Phase 4.5 / E 包并行研究项**（新冻结协议、新语料；不占用本 Phase 关键路径，不触碰 Day-3 "不第三轮" ruling 的适用域）。

---

## 4. 主 Agent 推荐与理由

**推荐 (c′)。**

1. **关键路径解耦**：载荷预测数据集的核心效用（H1 P_load_total 多步预测的训练/评测数据 + OOD 外推域）不依赖场景标签可辨识性资格。contrast 层资格失败不应阻塞 T-019 → T-020 → E/F——这是数据集科学定位的自然推论，不是迁就进度。
2. **负结果本身有论文价值**：三层根因中 (1) 度量-负载空间失配有干净数学论证，(3) DIR 配对机理已证（+83.8 kW / BA 0.944）只是 pooled 设计不适用，(2) DEPTH 物理等价发现在 v2 冻结条款中已被预接受。(c′) 把这些作为结果呈现，而非当作待修复的障碍。
3. **(b) 的收益可后补、且 DEPTH 大概率补不动**：probe 强信号的对比换度量后大概率 PASS（可后补为加强证据）；但 DEPTH 若 P_prop 层也证不出（物理等价的可能结局），最终仍回到降级动作。(c′) 把这个不确定性移出关键路径。
4. **(a) 留下不可解释的缺口**：轴 B 弱信号风险在 T-443 现规划下无法归因；届时被动补丁不如现在预注册诊断要求。
5. **纪律成本低**：(c′) 完全在 FR-551 预留的合法路径内，不重开任何 C 门禁、不触碰冻结数值、不豁免 Day-3 ruling。

---

## 5. 裁决记录（用户填写）

```
裁决人: ________________          日期: ____-__-__

选定路线:  [ ] (a) 照原设计继续
           [ ] (b) 信号层补强后重设计 T-429（含对 Day-3 "不第三轮" ruling 的显式豁免）
           [ ] (c) 对比框架降级重定义
           [ ] (c′) 推荐——(c) + (b) 要素转 Phase 4.5 / E 并行研究项
           [ ] 其他: ______________________________________

附加决定（可选，activation 时再定亦可）:
           [ ] OOD counts 向轴 A/C 倾斜（轴 B 保留弱信号预期）
           [ ] E spec T-443 增补 OOD shift 双诊断要求（开工时落实）

备注: _______________________________________________
```

---

## 6. 裁决后的执行清单（按路线）

| 步骤 | (a) | (b) | (c)/(c′) |
|---|---|---|---|
| 本文件状态行 → APPROVED（含裁决记录） | ✓ | ✓ | ✓ |
| C spec AC-437 注记（终判 FAIL + 路线引用） | 简单注记 | amendment + 门禁重开评估 | 注记"框架降级依据本 amendment" |
| contrast registry / 生成器 / 语料 | 不动 | 修订 + 新语料 + T-429-v3 | 不动（数值零变更） |
| T-432 activation record 填值（OOD 轴 A 数值 / counts / seeds / fleet 规模 / 合格判据）→ checklist ⑤⑥ | ✓ | 推迟至 (b) 完成 | ✓（counts 倾斜为可选项） |
| T-018 Stage A / T-019 Stage B / T-020（T-435-formal） | 解锁 | 后移 | 解锁 |
| E 包 spec（T-443 OOD 评测条款） | 现状 | 现状 | 增补 shift 双诊断要求 |
| Phase 4.5 研究项登记（(b1) 度量校准 / (b2) P_prop 通道） | 无 | 即期执行 | ✓（后续研究项） |

---

## 附：本文件引用的证据文件索引

| 文件 | self_hash / 标识 |
|---|---|
| `results/mission_v3/t429_final_verdict.json` | eb389ae8… |
| `results/mission_v3/identifiability_gate_result.json` | EXECFIX-01 conformance rerun（2026-09-06T18:33:58Z） |
| `results/mission_v3/t429_amendment_v2.json` | 796a8b07…（frozen_before_generation=true） |
| `results/mission_v3/t429_freeze_v2.json` | R_min=1.5 / A_min=0.8 盲冻结 |
| `results/mission_v3/d1a_dir_paired_diagnostic.json` / `d1b_traffic_overlap_diagnostic.json` | D1 双诊断（DEVELOPMENT_DIAGNOSTIC） |
| `config/mission_v3/identifiability_contrast_registry.json` | 2274da4a…（version 1.0.0-candidate） |
| `results/mission_v3/prequal_amendment_01.json` | a0484664… |
| `项目开发进展.md` 2026-09-06 / 09-06（续）/ 09-07 条目 | 三层根因 / 终判 / EXT-CD-GATE |
| `.specs/phase4-c-mission-v3/requirements.md` FR-547~551 / AC-437 | C 契约红线 |
| `docs/phase4_research_roadmap.md` E 包 T-436~T-444 / F 包 T-445~T-455 | 下游消费 |
| `.specs/phase4-d-dataset-v3/design.md` DGN-11 / requirements FR-485/486/499 | OOD 三轴与 Gate reopening 纪律 |

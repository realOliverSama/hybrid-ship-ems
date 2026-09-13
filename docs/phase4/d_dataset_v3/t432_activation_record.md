# T-432 Activation Record（FROZEN）

> Spec: `.specs/phase4-d-dataset-v3`（DGN-17；FR-495/497/498；tasks T-018/T-019）
> **状态**: **FROZEN**——D1~D5 Final Ruling 全部批准（含窄修正），用户裁决 2026-09-07；修正已并入正文，机器副本 `t432_activation_record.json` 已生成。除非实现暴露科学契约冲突（非机械 bug），无需再批准。
> **法律地位**：formal fleet 生成（`73_generate_fleet_v3.m`）按本 record 预注册值执行，禁止"跑出效果再决定停在哪"（FR-497）。operational freeze 不重走四门禁；D-D1~D-D9 科学契约漂移才重开最早受影响的门（FR-499）。
> **上游依据**：Scientific Roadmap Amendment v1（APPROVED 路线 c′，2026-09-07）——OOD counts 按「统计精度 + 预声明条件数 + seed 纪律」分配；轴 B 不削减；轴 C condition-count-driven。
> 机器可读副本：`t432_activation_record.json`（`formal_voyage_qualification` 节供 `cd_interface_gate.py` 项⑦核对）。

## 0. 解锁前置（FR-498 Unblock Checklist 八项）

② EXT-CD-GATE（2026-09-07）；①③④⑦⑧ 证据归档；⑤ = 本 record 冻结；⑥ = 本 record §2 冻结 → **ALL_GREEN，T-432 READY**（见 `t432_unblock_checklist.json`）。

## 1. Formal fleet 规模与分配（FR-497 Stage B 预注册）—— FROZEN（D1）

| 项 | 冻结值 | 依据 |
|---|---|---|
| exact voyage count（IID） | **576** = 9 ACTIVE strata × 16 base-draw families × 4 members | D1 批准（不用 744 贴 floor / 960 选项）；16 split_groups/格 ≥ FR-554 floor 14 |
| strata allocation | 九格全 ACTIVE 等额：每格 16 base draws（64 航次/格） | scenario_stratum_registry 九格；FR-557 |
| route/mission/template/group allocation | template = 格 registry 固定映射；每 base draw 一个 2×2 干预 family（traffic ON/OFF × forecast GOOD_V1/DEGRADED_V1，PHYSICAL×INFORMATION，M1~M4） | prequal family_plan 先例；FR-525/FR-481 |
| split_group_id grouping policy（FR-481） | split_group = family 闭包（`SG#`+16-hex SHA256 over must-link keys；one family → one split_group）；split.py 70/15/15 stratum-aware（每格 16 组 → ~11/3/2 组）；split_seed = `dataset-v3-formal-split-seed-v1`；**OOD 记录不入 IID splitter（独立评测集合，D1）** | FR-481/DGN-09；FR-554；FR-556 |
| generation seeds | formal_generation_seed = **`MISSION_V3_T432_FORMAL_SEED_2026_09_07`**；stratum i（0-based）base_draw_index ∈ [i·1000, (i+1)·1000)，每格 k=0..15；OOD 条件 ≥10000 段（§2.4）；Stage A 独立 seed root（§4） | seed_tree_policy lineage 公式；命名惯例先例 |
| failed slot retry/disposition policy | `slot_failed_retry_max = 2`；FAILED_* 同 slot/同 config/同科学 seed 重试（FR-537/R8），ledger 记录；REJECTED_* 不占槽；拒绝率门 per_stratum ≤ 0.50 ∧ overall ≤ 0.50 | prequal 冻结政策逐字沿用（实测 18.18% PASS） |

**边际覆盖义务（FR-541）**：fleet-wide 计数、floor=10；Stage B manifest 审计；自然采样不足 → 如实登记 known coverage limitation，禁事后增补（no_post_hoc_augmentation）。

## 2. OOD 定义（FR-485/486；DGN-11-v1 数值冻结）—— FROZEN（D2/D3/D4）

### 2.1 OOD definition version
`DGN-11-v1`（三轴定义不动；id_support_bounds OOD 数值 `ACTIVATION_CANDIDATE` 的冻结权 = 本 record）。

### 2.2 轴带、guard band 与预声明条件

**轴 A（strong-current extrapolation，双向统一按 \|U_river\| 判）**：

| 项 | 冻结值 |
|---|---|
| A1 温和外推 | `2.0 < \|U_river\| ≤ 2.4 m/s` |
| A2 强外推 | `2.4 < \|U_river\| ≤ 2.8 m/s` |
| 方向特定 IID 支撑上限（不扩） | adverse 1.8 / following 1.5（id_support_bounds 冻结值；**禁为带对称而扩大 IID 支撑**） |
| guard band（通用不变量） | `GUARD(direction) = (IID_SUPPORT_MAX(direction), 2.0]` ⇒ **adverse guard (1.8, 2.0]**、**following guard (1.5, 2.0]**——完整覆盖方向特定支撑与公共 OOD 门槛之间的间隙 |
| guard band 语义 | 非 formal IID、非 formal OOD（FR-519）；formal 生成不落带（落带即 reject-resample） |

**轴 B（LOW_EFFECTIVE_DEPTH / LOW_EFFECTIVE_HT EXTRAPOLATION——低有效水深/低有效 H/T 外推；定义变量为 effective H/T，机制可同时变化静态海图水深与水文实现，非纯"水位外推"）**：

| 项 | 冻结值 |
|---|---|
| B1 数值外推 | `1.3 ≤ H/T_effective < 1.5` |
| AXIS_B_TRAIN_SUPPORT_MIN_HT | **1.5**（audit_ood 常量随本 record 生效；[1.5, 2.0) 保持在数值 IID 支撑内） |
| 语义规则（D3） | `[1.5, 2.0) 不自动归轴 C`——轴 C 成员资格**仅由预声明组合 token 谓词命中判定**：`LOW_ID_HT alone ≠ OOD`；`LOW_ID_HT + held-out 语义因子 → 轴 C`（C2=HARBOR×LOW_ID_HT、C3=EMERGENCY_SPRINT×LOW_ID_HT） |
| 域界 | H/T ∈ [1.2,1.3) 不入 formal OOD（域下界+margin；如生成 → STRESS_SET 单独报告）；< 1.2 禁生成（PHYSICAL_DOMAIN_BOUND） |

**轴 C（compositional held-out，D4）**：

| 条件 | 谓词（冻结） | 宿主 | 机器映射（D4 登记） |
|---|---|---|---|
| C1 = 桥区×强逆流 | segment.role=bridge ∧ adverse\|U\| ∈ **[1.4, 1.8]**（HIGH_ID_BAND） | ROUTINE_PATROL × FLOOD | bridge × high-ID adverse-current token |
| C2 = 港区×低有效 H/T | segment.role=harbor ∧ ht_effective ∈ **[1.5, 2.0]**（LOW_ID_BAND） | ROUTINE_PATROL × DRY | harbor × low-ID-HT token |
| C3 = 应急冲刺×低有效 H/T | segment.role=rush ∧ ht_effective ∈ **[1.5, 2.0]** | EMERGENCY_RESPONSE × DRY | emergency-sprint × low-ID-HT token |

**D4 实现约束（冻结）**：73 号与 audit_ood 的轴 C 判定 **SHALL 消费 heldout_token_registry 的 canonical token ID/谓词，禁止在 audit_ood.py 内维护独立手抄 token 阈值**；OOD provenance SHALL 记录 token-registry version/self_hash；**轴 C token 优先级高于同时命中的数值轴**。轴 B 成员资格按 realized ht_effective 判定（见 §2.5-B1）。

### 2.3 OOD counts（condition-count-driven）—— FROZEN

| 条件 | families | 航次（×4） | split_groups |
|---|---|---|---|
| A1 / A2 / B1 / C1 / C2 / C3（各） | 10 | 40 | 10 |
| **合计** | **60** | **240** | **60** |

OOD:IID = 240:576（对 DGN-11「≈10% 均分」建议的显式偏离登记：amendment v1 condition-count-driven 原则取代，轴 B 未削减，轴 C 因 3 token 获 120 条最大份额）。

### 2.4 OOD generation seeds —— FROZEN
同一 seed root `MISSION_V3_T432_FORMAL_SEED_2026_09_07`；OOD 条件 c（0-based，A1/A2/B1/C1/C2/C3 序）base_draw_index ∈ **[10000+c·1000, 10000+(c+1)·1000)**，每条件 k=0..9；与 IID [0,9000) 段不相交；seed_tree 碰撞检测 fail-closed。

### 2.5 OOD 定向生成机制（73 号实现）—— FROZEN（D5）

**管线（冻结）**：`预注册 proposal 采样器 → 常规随机实现 → 数值带/REQUIRE 谓词判定 → 物理域 guard → accept / reject-resample → OOD provenance/meta`。

1. **不修改冻结 IID 注册表数值**；OOD override/配置**自有冻结 provenance/hash**（随 manifest 记录）。
2. **数值带定向采样**（proposal sampler，效率导向，非科学定义）：
   - A1：u_river_mag ~ TN(2.2, 0.15, lower=2.05, upper=2.4)；A2：~ TN(2.6, 0.15, 2.45, 2.8)；成员资格按 realized sup\|U_river\| 终判（含 OU 实现与预史），误带 reject-resample；
   - B1：h_chart ~ TN(**3.86, 0.15, lower=3.64, upper=4.08**)（使 E[ht_effective | DRY ΔWL~TN(−0.8,0.4)] ≈ 1.40；**科学定义仅为 `1.3 ≤ realized ht_effective < 1.5`——成员资格一律按 DRY 水文实现后的 realized H/T_effective 判定**，proposal 分布仅为效率采样器；非纯"水位外推"）；
   - C1~C3：环境量按宿主 regime 冻结分布自然采样 + **canonical REQUIRE 谓词**（消费 heldout_token_registry 同一文件，禁手工双版本）。
3. **域 guard（fail-closed）**：sup\|U\| > 2.8 或 ht_effective < 1.3 越带即拒；轴 A 物理域检查（11 kn 逆水叠加后推进功率 ≤ 700 kW 推进容量）为 OOD eligibility 前置项。**物理域违例 = STRESS_SET / 被拒的 formal-OOD 候选，不作为 A/B/C 样本。**
4. **meta 标注（FR-556）**：`dataset_domain='OOD'`、`ood_axis∈{A,B,C}`、`ood_condition_id`、`ood_token_id`（适用时）、`ood_def_version='DGN-11-v1'`、`physics_domain_ref`、token-registry version/self_hash；IID formal 航次 `dataset_domain='IID'`、`ood_axis='NONE'`。
5. **OOD 记录永不进入 IID splitter**（独立评测集合）。
6. **audit_ood.py 对齐**（随 73 号施工落码 + 单测）：AXIS_B train support 2.0→1.5；轴 C 判定改为消费 heldout_token_registry canonical 谓词（移除手抄组合常量）；子带分类消费 `ood_condition_id`。

## 3. Formal 航次合格判据（cd_interface_gate 项⑦核对）—— FROZEN

| 项 | 冻结值 |
|---|---|
| min_duration_s | **1200** |
| min_canonical_windows | **60** |
| unexpected_invalid_canonical_windows | **0** |

（FD2/DGN-10/FR-535 冻结候选；252 formal 航次实证零违规。）

## 4. Stage A qualification（工程 nominal；FR-497）—— FROZEN

- **规模与构成**：**96** = IID nominal 72（9 格 × 2 draws × 4，格内 k=900..901）+ OOD smoke 24（6 条件 × 1 draw × 4，条件段 k=900）；独立 seed root `MISSION_V3_T432_STAGEA_SEED_2026_09_07`；工程语料，disposable，不入 formal fleet。
- **验证项**（DGN-17）：生成稳定性（两跑 payload hash 一致）/ 导出完整性 / schema 一致（C/D Gate formal 模式跑 Stage A 语料）/ 吞吐 / failed-voyage 处理（剔除+记录，不静默修补）/ 审计管线全链；OOD smoke 额外：C1~C3 REQUIRE 定向可达性、A/B 带内命中率、域 guard 与 STRESS_SET 剔除路径、C3 低 H/T 可行性（FR-545）。
- **qualified-stack freeze**（Stage A PASS 后回填八项 hash）：`[_FILL_ Stage A 后: 73 号脚本 SHA / producer schema hash / registry hash / 基线四件 hash / Stage A fleet manifest hash / OOD def version / split policy version / checklist json hash]`。
- **吞吐预算**（122 航次/h 实测锚点）：Stage A ≈0.8 h/pass；Stage B IID ≈4.7 h/pass + OOD ≈2 h/pass；两 pass 合计 ≈15 h。

## 5. 冻结确认 —— 已完成（2026-09-07）

- [x] §1~§4 全部值经用户 D1~D5 Final Ruling 批准（含窄修正，已并入正文）
- [x] Unblock Checklist 八项全绿证据归档（①~⑧ 全 true，见 checklist JSON）
- [x] C/D Interface Gate 正式版 overall=true（EXT-CD-GATE 双语料）
- [x] 基线完整性：物理 v1.1 + 控制 v1.0 hash 与 D0 快照一致（72 号刷新后逐字节一致）

**裁决记录**：D1 APPROVED（816，不用 744/960）；D2 APPROVED + guard band 修正（方向特定 guard (1.8,2.0]/(1.5,2.0]，通用不变量 GUARD=(IID_MAX,2.0]，禁扩 IID 支撑求对称）；D3 APPROVED + 语义澄清（[1.5,2.0) 留数值 IID 支撑、不自动归轴 C；谓词驱动；轴 B 更名 LOW_EFFECTIVE_HT EXTRAPOLATION）；D4 APPROVED（canonical token 消费、禁手抄、provenance 记 registry hash、token 优先）；D5 APPROVED（管线六段 + 六项要求 + B1 科学定义仅为 realized ht_effective 带、proposal 采样器非科学定义）。

## 6. 生效序列（§7，执行中）

1. ✅ 本 record FROZEN + 修正并入；
2. ✅ 机器副本 `t432_activation_record.json` 生成；
3. ✅ checklist 八项全绿 → **T-432 READY**；
4. ⏳ T-018 施工 73 号（IID + OOD 生成模式、audit_ood 对齐、单测）→ Stage A 96 条 → qualified-stack freeze 回填 §4；
5. ⏳ T-019 Stage B formal fleet（816 条两跑确定性）→ T-020（T-435-formal 审计冻结）→ Dataset-v3 Freeze → E/F 解锁。

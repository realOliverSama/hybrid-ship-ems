# T-432 Activation Record（PROPOSED — 待用户逐项批准）

> Spec: `.specs/phase4-d-dataset-v3`（DGN-17；FR-495/497/498；tasks T-018/T-019 BLOCKED 条款）
> **状态**: DRAFT-PROPOSED（2026-09-07 activation session 起草；用户批准 §6 裁决点后去 PROPOSED 标记、勾选 §5、生成机器副本 `t432_activation_record.json`）
> **法律地位**：填写冻结后 formal fleet 生成（`73_generate_fleet_v3.m`）按本 record 预注册值执行，禁止"跑出效果再决定停在哪"（FR-497）。operational freeze 不重走四门禁；D-D1~D-D9 科学契约漂移才重开最早受影响的门（FR-499）。
> **上游依据**：Scientific Roadmap Amendment v1（APPROVED 路线 c′，2026-09-07）——OOD counts 按「统计精度 + 预声明条件数 + seed 纪律」分配，不按预期效应强度；轴 B 不削减；轴 C condition-count-driven。
> 机器可读副本：`t432_activation_record.json`（`formal_voyage_qualification` 节供 `cd_interface_gate.py` 项⑦核对）。

## 0. 解锁前置（FR-498 Unblock Checklist 八项）

② 已绿（EXT-CD-GATE，2026-09-07）；**①③④⑦⑧ 证据已备齐，随本 record 批准一并置绿；⑤ = 本 record 批准本身；⑥ = 本 record §2 批准本身** → 批准即 ALL_GREEN。

## 1. Formal fleet 规模与分配（FR-497 Stage B 预注册）—— PROPOSED

| 项 | 提案值（activation 冻结） | 依据 |
|---|---|---|
| exact voyage count（IID） | **576** = 9 strata × 16 base draws × 4 family members | 16 split_groups/格 ≥ FR-554 floor 14（"floor 非配额，formal 通常应超过"）；family 倍数（members_per_family=4）；总盘 816（+OOD 240）落 roadmap T-432 500~1000 锚点内 |
| strata allocation | 九格全 ACTIVE 等额：每格 16 base draws（64 航次/格） | scenario_stratum_registry 九格（正交性裁决后合法完备格）；FR-557 配额从冻结分类法推导 |
| route/mission/template/group allocation | template = 格 registry 固定映射（ROUTINE_PATROL / EMERGENCY_RESPONSE / WORK_MISSION × NORMAL/FLOOD/DRY）；每 base draw 展开一个 2×2 干预 family（traffic ON/OFF × forecast GOOD_V1/DEGRADED_V1，PHYSICAL×INFORMATION 分型，members M1~M4） | prequal family_plan 先例（full 2×2 factorial）；FR-525/FR-481 |
| split_group_id grouping policy（FR-481） | split_group = family 闭包（C 侧 family_expand：`SG#`+16-hex SHA256 over sorted must-link member keys；one family → exactly one split_group）；D 侧 split.py 按 split_group 70/15/15 stratum-aware 切分（每格 16 组 → ~11/3/2 组 ≈ 44/12/8 航次）；**OOD 记录不入 IID splitter（第四集合，FR-556）**；split_seed = `dataset-v3-formal-split-seed-v1` | FR-481/DGN-09 冻结；FR-554（16≥14）；pilot 先例 `pilot-split-seed-v1` |
| generation seeds（主 seed + per-stratum 规则） | formal_generation_seed = **`MISSION_V3_T432_FORMAL_SEED_2026_09_07`**；draw_index 规则沿用 prequal：stratum i（0-based）base_draw_index ∈ [i·1000, (i+1)·1000)，每格取 k=0..15；OOD 条件用 ≥10000 段（§2.4）；Stage A 独立 seed root（§4） | seed_tree_policy lineage 公式（SHA256(seed\|draw\|branch)，跨 corpus 不同 seed root 无碰撞）；命名惯例 `MISSION_V3_<语料>_SEED_<日期>`（prequal/T429CONF 先例） |
| failed slot retry/disposition policy | `slot_failed_retry_max = 2`；FAILED_* 同 slot/同 config/同科学 seed 重试（FR-537/R8），retry ledger 记录；REJECTED_* 不占槽（按冻结采样政策重采样）；拒绝率门 per_stratum ≤ 0.50 ∧ overall ≤ 0.50（DGN-10 冻结候选；prequal 实测 18.18% PASS） | prequal retry_policy / rejection_threshold_frozen 逐字沿用 |

**边际覆盖义务（FR-541：token A∧¬B / ¬A∧B 各 ≥10 unique split_groups——候选值 activation 复核）**：采用 **fleet-wide 计数、floor=10**，Stage B manifest 审计实际计数；自然采样不足时**如实登记 known coverage limitation，禁事后增补**（no_post_hoc_augmentation 纪律）。低 H/T（<2.0）自然概率 NORMAL/FLOOD ≈7%/DRY ≈23%（h_chart TN(6,1,≥3.5)+ΔWL 合成 σ≈0.51），边际覆盖主要依赖 DRY 三格，属预期内紧约束。

## 2. OOD 定义（FR-485/486；DGN-11 框架已冻结，此处冻结数值与机制）—— PROPOSED

### 2.1 OOD definition version
`DGN-11-v1`（amendment v1 §3.3-4：三轴定义不动）；本 record 为 activation 数值冻结 + 机制落地层（id_support_bounds 全部 OOD 数值本标 `ACTIVATION_CANDIDATE`、冻结权 = T-432 activation）。

### 2.2 三轴数值带与预声明条件

| 轴 | 条件 id | 数值带（冻结提案） | 宿主（template × regime） | 说明 |
|---|---|---|---|---|
| A 强流 | **A1 温和外推** | sup\|u_river\| ∈ **(2.0, 2.4]** m/s | ROUTINE_PATROL × FLOOD | 双向统一按 \|U\| 判（DGN-11 "\|U\|" 口径 + audit_ood 门槛 2.0）；id_support_bounds 顺水 (1.8,2.0] 段冻结为 guard band（非 IID 非 formal OOD，FR-519）——裁决点 D2 |
| A 强流 | **A2 强外推** | sup\|u_river\| ∈ **(2.4, 2.8]** m/s | ROUTINE_PATROL × FLOOD | 子带支持 amendment §7 剂量-响应诊断 |
| B 低水位 | **B1 数值外推** | voyage ht_effective ∈ **[1.3, 1.5)** | WORK_MISSION × DRY | 裁决点 D3：DGN-11 轴 B 包络 [1.3,2.0) 拆分——[1.3,1.5) 为真数值外推（低于 C 支撑下界 1.5）；[1.5,2.0) 属 IID 数值支撑内、其 held-out 组合结构由轴 C token（C2/C3）承载。audit_ood `AXIS_B_TRAIN_SUPPORT_MIN_HT` 2.0→**1.5** 随本 record 生效（该常量本就 PREREGISTERED 待 activation）。H/T∈[1.2,1.3) 不入 formal OOD（域下界+margin；如生成则 STRESS_SET 单独报告）；<1.2 禁生成（PHYSICAL_DOMAIN_BOUND） |
| C 组合 | **C1 桥区×强逆流** | segment.role=bridge ∧ adverse\|U\| ∈ **[1.4, 1.8]**（HIGH_ID_BAND） | ROUTINE_PATROL × FLOOD | = heldout token C1（ood_action=REQUIRE）；裁决点 D4：本 record 登记 token↔audit_ood 组合映射——C1↔{bridge_zone,strong_counter_current}；C2↔{shallow_water,port_area}；C3↔{emergency_sprint,shallow_water}（audit_ood L49 明示对齐责任在 activation record） |
| C 组合 | **C2 港区×低 H/T** | segment.role=harbor ∧ ht_effective ∈ **[1.5, 2.0]**（LOW_ID_BAND） | ROUTINE_PATROL × DRY | = heldout token C2 |
| C 组合 | **C3 应急冲刺×低 H/T** | segment.role=rush ∧ ht_effective ∈ **[1.5, 2.0]** | EMERGENCY_RESPONSE × DRY | = heldout token C3（FR-545：rush×低 H/T 须 nonzero feasible support、无 domain_warning 聚集/SOP 饱和——Stage A OOD smoke 验证项） |

**优先级规则（冻结）**：同一航次同时命中数值带与组合 token 时标**轴 C（token 优先）**；`ood_axis` 单值 ∈ {A,B,C}，OOD 记录另携 `ood_condition_id`（A1/A2/B1/C1/C2/C3）与 `ood_token_id`（仅 C 类）。

### 2.3 OOD counts（condition-count-driven 分配，amendment v1 §3.3-4）

| 条件 | base draws（families） | 航次（×4） | split_groups |
|---|---|---|---|
| A1 / A2 / B1 / C1 / C2 / C3（各） | 10 | 40 | 10 |
| **合计** | **60** | **240** | **60** |

精度依据：每条件 10 split_groups ≥ C 侧 group 覆盖惯例（10~12 组，分组 bootstrap/LCB95 可用）；每条件窗量 ≈ 40×~360 ≈ 1.4 万窗；OOD:IID = 240:576 ≈ 42%——**对 DGN-11「≈10% 三轴均分」建议结构的显式偏离登记**：该建议被 amendment v1 condition-count-driven 原则取代（6 预声明条件 × 10 组下限的必然结果；轴 B 未因预期弱 shift 削减；轴 C 因 3 token 获 120 条最大份额）。

### 2.4 OOD generation seeds
同一 formal seed root `MISSION_V3_T432_FORMAL_SEED_2026_09_07`；OOD 条件 c（0-based，按 A1/A2/B1/C1/C2/C3 序）base_draw_index ∈ **[10000+c·1000, 10000+(c+1)·1000)**，每条件取 k=0..9（与 IID [0,9000) 段不相交；lineage 全局无碰撞，seed_tree 碰撞检测 fail-closed 兜底）。

### 2.5 OOD 定向生成机制（预注册，73 号实现）—— 裁决点 D5

**现状（事实）**：现行 `env_realize('sample'/'realize')` 将 \|U\| 超界（adverse>1.8 / following>1.5）与 H/T<1.5 判 SUPPORT_BOUND 拒收；`ood_action=REQUIRE` 无消费点；`meta.ood_axis` 硬编码 `'NONE'`——OOD 航次在现行生成器内不可产出，73 号须新增 OOD 采样模式（D 包职责，C 契约 L36；FR-543 消费同一谓词注册表，禁手工双版本）：

1. **数值带定向采样**：OOD 模式对宿主 regime 目标环境量改用本 record 预注册目标分布（**不修改冻结 IID 注册表文件**；将 ParamOverride 的"既有键递归合并、schema/self_hash 键不可覆写"机制升级为生产级 OOD 配置通道）：
   - A1：u_river_mag ~ TN(2.2, 0.15, lower=2.05, upper=2.4)；A2：~ TN(2.6, 0.15, 2.45, 2.8)（sup 校验终判带，误带 reject-resample）；
   - B1：h_chart ~ TN(2.93, 0.10, lower=2.85, upper=3.28)（h_chart/2.188 ∈ [1.30,1.50) 目标带；ΔWL 按 DRY 冻结分布自然采样；ht_effective 超带 reject-resample）；
   - C1~C3：环境量按宿主 regime 冻结分布自然采样 + **token REQUIRE 判定**（谓词真才接受）。
2. **域 guard（fail-closed）**：sup\|U\| > 2.8 或 ht_effective < 1.3 越带即拒；DGN-11 轴 A 物理域检查（11 kn 逆水叠加后推进功率 ≤ 700 kW 推进容量）实现为 OOD eligibility 前置项，越界判 STRESS_SET 剔除并记录。
3. **meta 标注（FR-556）**：`dataset_domain='OOD'`、`ood_axis∈{A,B,C}`、`ood_condition_id`、`ood_token_id`（C 类）、`ood_def_version='DGN-11-v1'`、`physics_domain_ref`；IID formal 航次 `dataset_domain='IID'`、`ood_axis='NONE'`。
4. **合格判据与 retry**：与 IID 相同（§3 / §1）。
5. **audit_ood.py 常量对齐**（activation 值生效，随 73 号施工落码 + 单测）：AXIS_B train support 2.0→1.5；轴 C token 机器映射接通 C1~C3；子带分类消费 `ood_condition_id`。

## 3. Formal 航次合格判据（FR-495 第 7 项；cd_interface_gate 项⑦核对）—— PROPOSED

| 项 | 提案值 | 依据 |
|---|---|---|
| min_duration_s | **1200** | FD2/DGN-10/FR-535 冻结候选；C 侧 252 formal 航次实证零违规 |
| min_canonical_windows | **60** | 同上（N(D)=⌊(D−600)/10⌋+1） |
| unexpected_invalid_canonical_windows | **0** | 同上（三条件 AND） |

## 4. Stage A qualification（≈100 条工程 nominal；FR-497）—— PROPOSED

- **规模与构成**：**96** = IID nominal 72（9 格 × 2 base draws × 4，draw_index = 格内 k=900..901）+ OOD smoke 24（6 条件 × 1 draw × 4，条件段 k=900）；独立 seed root `MISSION_V3_T432_STAGEA_SEED_2026_09_07`；**工程语料，不入 formal fleet，disposable**。
- **验证项**（DGN-17）：生成稳定性（两跑 payload hash 一致）/ 导出完整性 / schema 一致（C/D Gate formal 模式跑 Stage A 语料）/ 吞吐 / failed-voyage 处理（剔除+记录，不静默修补）/ 审计管线（窗口化/split/L1-L3/OOD 装载全链）；OOD smoke 额外验证：C1~C3 REQUIRE 定向可达性、A/B 带内命中率、域 guard 与 STRESS_SET 剔除路径、C3 低 H/T 可行性（FR-545）。
- **qualified-stack freeze**（Stage A PASS 后回填八项 hash，变更即失效重跑，tasks R7）：`[_FILL_ Stage A 后: 73 号脚本 SHA / producer schema hash / registry hash / 基线四件 hash / Stage A fleet manifest hash / OOD def version / split policy version / checklist json hash]`。
- **吞吐预算**（prequal 实测 122 航次/h 推算）：Stage A ≈0.8 h/pass；Stage B IID 576 ≈4.7 h/pass + OOD 240 ≈2 h/pass；两 pass 合计 ≈15 h（跨 session 执行）。

## 5. 冻结确认（用户批准时勾选）

- [ ] §1/§2/§3/§4 全部 PROPOSED 值经用户书面批准（§6 裁决点 D1~D5）
- [ ] Unblock Checklist 八项全绿证据归档（①③④⑦⑧ 随本 record 批准置绿）
- [ ] C/D Interface Gate 正式版 overall=true（✅ EXT-CD-GATE 双语料，2026-09-07）
- [ ] 基线完整性：物理 v1.1 + 控制 v1.0 hash 与 D0 快照一致（✅ 72 号刷新后逐字节一致）

## 6. 裁决点清单（需用户显式批准的设计决策）

| # | 决策 | 提案 | 备选 |
|---|---|---|---|
| D1 | formal fleet 规模 | IID 576（16 families/格）+ OOD 240（6 条件 × 10 families）= 816 | 紧：IID 504（14/格贴 floor）+ OOD 240 = 744；大：IID 720（20/格）+ OOD 240 = 960 |
| D2 | 轴 A 数值带 | 双向统一 (2.0,2.8]，拆 A1/A2 两子带；顺水 (1.8,2.0] 定 guard band | 单带 (2.0,2.8] 不拆子带（OOD −40 条）；顺水 1.8 起（需改 audit_ood 门槛，与 DGN-11 \|U\| 口径冲突，不推荐） |
| D3 | 轴 B 包络拆分 | B1=[1.3,1.5) 数值外推；[1.5,2.0) 归轴 C 组合结构；audit_ood train support 2.0→1.5 | 维持 DGN-11 字面 [1.3,2.0) 全带为轴 B（与 C 支撑下界 1.5 矛盾：IID 内会出现同航次既训练又标 OOD，不推荐） |
| D4 | token 命名对齐 | 本 record 登记 C1/C2/C3 ↔ audit_ood 三组合映射，73 号落机器映射 | 无实质备选（audit_ood L49 明示对齐责任在 activation record） |
| D5 | OOD 生成机制 | 73 号新增 OOD 定向采样模式（目标分布 + REQUIRE 谓词 + 域 guard + meta 标注，不改冻结 IID 注册表） | scenario_compile 手工注入序列（失去 OU 动力学/预史结构，不推荐）；推迟 OOD（违反 FR-485/543，不可行） |

## 7. 批准后执行序列

1. 本 record 去 PROPOSED 标记 + 用户签字 → 冻结；
2. 生成机器副本 `docs/phase4/d_dataset_v3/t432_activation_record.json`（含 `formal_voyage_qualification` 节）；
3. checklist ①③④⑦⑧⑤⑥ 置绿 → ALL_GREEN → T-432 READY；
4. T-018 施工 73 号（IID + OOD 生成模式、audit_ood 常量对齐、单测）→ Stage A 96 条 → qualified-stack freeze 回填 §4；
5. T-019 Stage B formal fleet（816 条两跑确定性）→ T-020（T-435-formal 审计冻结）→ Dataset-v3 Freeze → E/F 解锁。

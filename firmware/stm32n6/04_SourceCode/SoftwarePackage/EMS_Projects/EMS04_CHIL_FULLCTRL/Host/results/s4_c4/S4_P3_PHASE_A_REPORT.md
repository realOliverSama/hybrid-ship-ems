# S4-P3 Phase A 执行报告 —— formal 臂电池选择（纯 host、只读盘点 + 冻结规则草案）

- 生成时间：2026-09-29 11:18 (+08:00)
- 生成者：worker-1 P3 Phase A（branch `feat/phase4-g`，无 git 写操作，无 MATLAB，无板/串口/调试器）
- 解释器：`.venv_tf/bin/python`（scipy.io.loadmat / numpy / json）
- 产出：本报告 + `S4_P3_FORMAL_ARM_FREEZE_DRAFT.json`（同目录）。未修改任何既有文件。
- 关联范本：`S4_C4_BATTERY_FREEZE.json`（冻结格式与元数据规则写法来源）

---

## 1. 任务与范围

P3 将 S4-C4 电池测试的覆盖从 pilot fixture 家族扩展到 formal 臂：从
`model/simulink_modelE_(Phase4_Physics)/datasets_v3/formal/iid/` 选 FLTA 航次，
M1 臂与 M2 臂各 ≥2 航次。Phase A 只做：A1 元数据盘点、A2 选择规则草案、
A3 SCENARIOS 存在性核查、A4 两个产出文件。判据/阈值零结果依赖；选择纯元数据驱动。

## 2. A1 盘点结果（formal/iid 全量 576 航次）

### 2.1 完整性检查（576/576 全通过）

| 检查项 | 结果 |
|---|---|
| 文件格式 | 576/576 = MATLAB 5.0 MAT-file（v7 兼容，**scipy loadmat 可读，非 v7.3/HDF5，无需 h5py**） |
| sha256 对账（文件 vs `_iid_ledger.json artifact_sha256`） | 576/576 一致 |
| `meta.voyage_id` == 文件名、`meta.voyage_duration_s` == ledger duration_s | 576/576 一致 |
| kf 白名单 8 字段（issue_time_s/lead_s/v_plan_mps/s_plan_m/h_chart_m/P_hotel_plan_kW/u_fcst_mps/wl_fcst_m） | 576/576 齐全 |
| kf issue 网格（first=300.0、步长唯一=10.0） | 576/576 通过 |
| kf lead 网格（(n,30)，取值集合 10:10:300） | 576/576 通过 |
| kf 6 内容列形状 (n,30) | 576/576 通过 |
| forecast_provenance 6 子段齐全 | 576/576 通过 |
| stream runner 列（SCENARIO_COLS 5 列 + time_s + P_load_total_kW） | 576/576 在位 |
| fixture_kind / provenance_class / disposition | 全 `NONE` / 全 `CAUSAL_GENERATOR` / 全 `ACCEPT`（真 formal 数据，非 fixture） |

### 2.2 臂结构（ledger + mat meta 双源一致，576/576）

| 臂 | n | arms_forecast | traffic | load_mean 域 (kW) | 臂中位 load_mean (kW) |
|---|---|---|---|---|---|
| M1 | 144 | **GOOD_V1** | ON | [221.7, 483.4] | 301.51 |
| M2 | 144 | **DEGRADED_V1** | ON | [221.7, 483.4] | 301.51 |
| M3 | 144 | GOOD_V1 | OFF | [253.3, 508.4] | 321.95 |
| M4 | 144 | DEGRADED_V1 | OFF | [253.3, 508.4] | 321.95 |

- 时长域：[3222, 4818] s，中位 4070 s——**全部 ≥600 s，dur 域约束不淘汰任何航次**。
- 每航次 kf issue 行数（= 正式拍数上界）：[263, 422]，中位 347.5。
- S 段 × 臂：36 组合（S1..S9 × M1..M4），每组合恰 16 航次，完全平衡。
- **M1/M2 同族配对**：144/144 对同 `base_draw_index`，同对 load_mean 一致（<0.05 kW，
  负荷流相同，仅 forecast 质量不同）——天然配对臂对照，同族对跨臂差分可隔离预报臂效应。

### 2.3 S 码语义（A3 前置事实）

`FLTA_S{n}_D{d}_M{m}` 的 **S 段 = mission_v3 fleet 生成的 stratum 计划索引**，不是
ModelE SCENARIOS 表场景码。证据：

1. `scripts/+fleet_v3/gen_iid.m` L262：`voyage_id = sprintf('%s_S%d_D%05d_M%d', prefix, si_plan, bdi, k)`——S 段即 `si_plan`；
2. S1..S9 与 9 个 stratum 一一对应（3 航线模板 × 3 regime）：S2=`STRAT_ROUTINE_PATROL__FLOOD`、S6=`STRAT_EMERGENCY_RESPONSE__DRY`；
3. mat 内 `meta.scenario_id` = 航次 id 本身（如 `FLTA_S2_D01006_M1`）——mission_v3 航次自为场景，**并非 SCENARIOS 表条目的实例化**。

## 3. A2 选择规则草案与选中清单

### 3.1 规则（纯元数据、先冻结后执行，仿 C4 风格）

1. 域：formal/iid 全体 ledger ACCEPT 航次 ∧ `voyage_duration_s ≥ 600` ∧ Phase A 完整性检查全通过 → 本域 = 全体 576（无淘汰）。
2. 臂域：仅 M1（GOOD_V1, traffic ON）与 M2（DEGRADED_V1, traffic ON）；M3/M4（traffic OFF）不入电池域。
3. 负荷轴：`load_mean = stream.P_load_total_kW` 全程均值（元数据面，同 C4 口径）。
4. 每臂独立取三点：R_low = load_mean 最小者；R_high = 最大者；R_mid = |load_mean − median(本臂)| 最小者（平局取 voyage_id 字典序小者；与已选重合则顺延次近）。
5. 每臂 ≥2 由 R_low/R_high 天然满足（实际 3+3 = 6）；两臂独立，禁跨臂调剂。
6. 规则只用元数据（臂/负荷均值/时长/完整性），先冻结后执行，禁按结果挑样（G6 §2）。

规则只用 G6 §2 明文允许的三个元数据轴（臂、负荷均值、时长）加上完整性硬门；
未引入任何结果面信息。

### 3.2 选中航次清单（6 航次 = 2 臂 × 低/中/高）

| role | voyage_id | 臂（ledger 权威） | S 码（stratum） | dur (s) | load_mean (kW) | 正式拍数 (kf issues) | issue 网格 | 首拍水深 (m) |
|---|---|---|---|---|---|---|---|---|
| M1_R_low | FLTA_S2_D01006_M1 | M1 = GOOD_V1 | S2 (PATROL/FLOOD) | 4679 | 221.73 | 408 | 300:10:4370 | 9.046 |
| M1_R_mid | FLTA_S2_D01005_M1 | M1 = GOOD_V1 | S2 (PATROL/FLOOD) | 4420 | 301.87 | 383 | 300:10:4120 | 7.509 |
| M1_R_high | FLTA_S6_D05003_M1 | M1 = GOOD_V1 | S6 (EMERG/DRY) | 3790 | 483.41 | 320 | 300:10:3490 | 5.956 |
| M2_R_low | FLTA_S2_D01006_M2 | M2 = DEGRADED_V1 | S2 (PATROL/FLOOD) | 4679 | 221.73 | 408 | 300:10:4370 | 9.046 |
| M2_R_mid | FLTA_S2_D01005_M2 | M2 = DEGRADED_V1 | S2 (PATROL/FLOOD) | 4420 | 301.87 | 383 | 300:10:4120 | 7.509 |
| M2_R_high | FLTA_S6_D05003_M2 | M2 = DEGRADED_V1 | S6 (EMERG/DRY) | 3790 | 483.41 | 320 | 300:10:3490 | 5.956 |

- R_mid 边距：两臂均为 D01005（|dev|≈0.36 kW），次近 D02001（|dev|≈0.36 kW）近似平局，
  由 voyage_id 字典序（S2<S3）决出——两臂同序，配对性保持（裁决过程同 C4 冻结件
  S08/S10 平局风格）。第三近 D07017（|dev|≈1.00）。
- 6 航次合计 2222 正式拍（C4 电池为 92 拍，约 24 倍规模）；备选 4 航次方案
  （每臂 R_low+R_high，1456 拍）已在草案 JSON 登记，取舍由主 Agent 冻结时决定。
- 草案 JSON 中每航次附：family_id、base_draw_index、intervention、generator id/version、
  artifact_sha256、配对航次号、kf 白名单齐全性。

## 4. A3 SCENARIOS 存在性核查结论 —— **ESCALATED**

- 选中 S 码：S2、S6。`ModelE_TestScenarios.m` 实有场景：S01~S10、S20、S31~S36
  （注意：S11~S19、S21~S29 槽位不存在；任务书"只含 S01~S36/S20"的表述略宽于实表）。
- 数字上 S2/S6 落在表内（SCENARIOS(2)=S02_downstream、SCENARIOS(6)=S06_resistance_step），
  但 §2.3 证据链表明 S 段是 stratum 索引：S2 航次是 FLOOD regime 巡逻航线、S6 是
  DRY regime 应急出动 mission_v3 航次，与 5000 m 定速顺水/阻力阶跃场景无生成关系。
  `meta.scenario_id` = 航次自身 id，进一步排除"表场景实例化"解释。**数字重合属伪映射。**
- 侧证：`plantstep_advancer.py` L74 `SCENARIOS_E` 植物注册表只有 4 个 pilot 烘焙 dylib
  场景（S20_composite_voyage/S02_downstream/S08_resistance_bands/S03_upstream），
  无任何 formal 航次条目——与 C4 冻结件 `arm_coverage_deviation` "formal FLTA 家族
  无 PlantStepE 植物域"的登记一致。

### §5 Escalation 格式

```text
Observed:
选中 formal 航次的 S 码（S2/S6）是 mission_v3 stratum 索引，不是 SCENARIOS 表场景码；
数字上与 S02/S06 槽位重合但语义无关；meta.scenario_id = 航次自身 id；
SCENARIOS_E 植物注册表无任何 formal 航次条目。

Investigated:
gen_iid.m 命名模板（L262）；S1..S9↔9 stratum 映射与 36 组合×16 平衡性；
选中 6 航次 mat 的 meta/stream/kf 全量核查；ModelE_TestScenarios.m 全表；
plantstep_advancer.py SCENARIOS_E 注册段；s4_c4_battery.py / s4_c3_pack_kf_table.py 执行面契约。

Blocker:
formal 臂闭环的植物场景与 SCN 参数来源不存在现成定义；
按任务书 A3 约定禁由本 worker 从 mission_v3 流重建 SCN 参数；
A3"表内存在真实映射"路径不成立，冻结文本的 SCENARIOS 映射结论无法闭合。

Candidates:
A) 确认 SCENARIOS 映射缺失，P3 formal 臂植物域另行设计（如按航次流烘焙 PlantStepE
   dylib，或泛化 stream 驱动植物入口）——主 Agent 设计决策；
B) 若主 Agent认定数字映射（S2→SCENARIOS(2) 等）在其设计语境中另有含义，需给出
   映射依据并冻结为显式表——本 worker 未发现支持该解释的证据；
C) 推迟 formal 臂电池，先扩 SCENARIOS_E/烘焙管线（Phase B 前置工程）。

Decision Required:
E1：formal 臂 SCN/植物场景来源如何定义（选 A/B/C 或其它），以及 A3 结论如何写入正式冻结件。
E2：臂标签裁决——G6 §2/任务书标签（M1=DEGRADED/M2=GOOD）与 ledger 权威编码
（M1=GOOD_V1/M2=DEGRADED_V1，576/576 一致）相反；本草案按 ledger 权威书写并登记偏差；
选择集在两种标签下相同（两臂各取），仅影响冻结文本命名。
```

## 5. kf 白名单齐全性结论

- **576/576 全部齐全**：8 个 kf 白名单字段在位且形状合规（issue 300:10 唯一步长、
  leads 10:10:300、6 内容列 (n,30)）；meta 溯源 6 子段（generator id/version/hash、
  issue_time_s、source_cutoff_time_s、provenance_class=CAUSAL_GENERATOR）在位。
- header 字段长度预检域（voyage_id≤32 ASCII 等）对本批航次 id 全部满足。
- 打包侧注意（Phase B 设计点，Phase A 未改任何文件）：
  `s4_c3_pack_kf_table.py` 冻结 `profile_code=3`（PILOT_FIXTURE）且 AST 禁读约束
  （禁未来真值列/预测真值层）依然适用；formal 两臂应使用已登记未占用的
  `1=GOOD_V1 / 2=DEGRADED_V1`，需 profile_code 泛化。
- `s4_c4_battery.py` 执行面注意：数据源硬编码 `datasets_v3/pilot/` 与 kf 表目录，
  `--voyages` 仅校验冻结集合成员；formal 臂需要 runner 泛化/派生（主 Agent 决策）。

## 6. 风险与阻塞项汇总

| # | 类型 | 内容 | 影响 |
|---|---|---|---|
| E1 | 阻塞（冻结前） | A3 SCENARIOS 映射缺失（伪映射），formal 植物域无现成定义 | 正式冻结件与 Phase B 执行设计需主 Agent 裁决 |
| E2 | 命名（不阻塞选择） | 臂标签 G6 文档 vs ledger 相反 | 冻结文本命名需裁决；选择集不受影响 |
| R1 | 规模 | 6 航次 = 2222 正式拍（C4 的约 24 倍） | 板会话时长规划；备选 4 航次方案已登记 |
| R2 | 执行面 | runner 数据源硬编码 pilot 路径；packer profile_code=3 冻结 | Phase B 需泛化，均属既有文件修改（本阶段禁改） |
| R3 | 观测 | formal 航次内嵌变水深（首拍 5.96~9.05 m，非 20 m 深水恒值） | runner ring.begin 用 stream[0] 水深，机制兼容；浅水轴覆盖为额外收益，非本规则目标 |
| R4 | 登记 | 两个新文件位于 vendor 子树（EMS_Projects），§20 目录索引机制豁免 | 无需 README 登记；如主 Agent 要求登记再补 |

## 7. 可复现性

- 盘点方法：单遍只读扫描 576 个 .mat（格式头识别 → sha256 对账 → loadmat →
  meta/kf/stream 白名单核查 → load_mean/中位边距计算）；无样本读取任何控制/闭环结果。
- 规则确定性：给定 `_iid_ledger.json` + formal/iid .mat 快照（sha256 已对账），
  规则输出唯一；中位平局与字典序裁决均已记录边距。
- 本 Phase A 未执行任何 gate、未调用 MATLAB MCP、未触碰板与串口、无 git 写操作。

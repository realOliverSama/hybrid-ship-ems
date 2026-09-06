# Phase 4-C — 真实内河航次与工况生成器 v3（Mission v3，T-419~T-429） Requirements

Spec: `phase4-c-mission-v3`  
Status: requirements-approved  
Created: 2026-09-05  
Approved: 2026-09-05（CONDITIONAL PASS + R1~R7 修正闭合）  
Brainstorm: `./brainstorm.md`（C-D1~C-D7 + Closeout 约束 A/B/C，2026-09-05 用户逐项书面批准 + 书面收官）

## Overview

Phase 4-C：Mission Generator v3——真实内河航次与工况生成器，生成逻辑 Route → Environment → Navigation Constraint → Operator → Plant（非 segment→speed）。C 是 Dataset v3 的**生产侧**：D 包已冻结消费侧契约（`docs/phase4/d_dataset_v3/c_producer_interface_contract.md` + `model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_producer_schema.json` 机器权威，self_hash=7993e141…），C 必须 conforming 实现 producer schema 十一项能力族。

覆盖：T-419 架构设计（先设计不写生成器）、T-420~T-426 七类任务级工况族（bridge/harbor/shallow/current/traffic/emergency/berthing-departure；berthing/departure 为 T-426 内的配对语义设计，不新增任务）、T-427 Full Voyage Template v3、T-428 条件随机生成器、T-429 可辨识性审计。

本文件只把已批准的 C-D1~C-D7 与 Closeout A/B/C 翻译为 FR/NFR/AC，不新增科学裁决；尚属 design/activation 的参数以 bounded obligation 形式给出（见「不得冻结事项」节）。

**契约红线（D 已冻结，C 不重议，违反=返工）**：issue-time causality（s_plan 由计划前向积分，禁 realized future 入 K/F）；双索引 schema（truth 单轴 realization-time；K/F 按 (issue, lead) 快照，(M×1)/(M×30) issue-major，lead∈{10..300}，K/F 网格=target 网格 30×10 s）；双存隔离（INPUT_F 预报与 ORACLE 真值物理分离）；阶段隔离（formal 禁 PILOT_ONLY，FR-500）；Model E 零改动；基线法律角色（formal=物理 v1.1+控制 v1.0；A2 historical v1.0 仅历史参照）。

## Scope

### In Scope

- T-419 Mission Generator v3 架构设计（conditional-DAG、模块边界、与 D producer schema 的接口映射；**先设计后施工**）；
- T-420~T-426 七类任务级工况族的生成语义与实现（全部经 Model E 既有接口表达）；
- T-427 Full Voyage Template v3（routine patrol / emergency response / heavy-current）；
- T-428 Stochastic Mission Generator（条件随机 + 科学 seed 树 + 干预层）；
- T-429 可辨识性审计（硬 Gate）；
- D→C 接口必做项：producer schema 十一项能力族逐项实现（truth stream native 0.01 s、s_plan 直出、K/F issue-time snapshots、forecast provenance 六字段+quality profile、ORACLE 双存分离、五族 metadata+base_draw/seed-lineage、generation seed、OOD 配置面）；
- 机器可读注册表：mission/route template registry、conditional-DAG config、scenario-stratum registry、compositional held-out token registry、required-identifiability contrast registry、forecast-quality profile registry；
- C/D Interface Gate 正式版执行配合（D-15 必改项的施工在 D 侧，C 侧提供被检产物与证据）；
- MISSION_V3_PREQUALIFICATION（C 自有预资格生成验证，≈100 条量级，工程 nominal）：生成器稳定性/producer-schema 正确性/provenance/拒绝审计/strata 与 token 初步覆盖/T-429 证据/C/D Gate 就绪度。**它不是、也不替代 D/T-432 Stage A**——正式 Stage A 在 C/D Interface Gate overall=true、T-432 READY 后由 D 侧执行。

### Out of Scope

- Model E 物理/控制任何改动（含 Gate 阈值、冻结参数、`.slx` 结构）；
- T-432 Stage A qualification / Stage B formal fleet 生成与 T-435-formal 审计执行（均为 D 包职责，C 完成 + C/D Gate overall=true 后 D 侧另立项收尾；C 包交付至 C/D Gate 就绪 + MISSION_V3_PREQUALIFICATION + T-429 PASS）；
- Phase 4-E 预测模型（T-436~T-444）与 Phase 4-F 预测式 EMS（T-445~T-455）；
- N6/HIL/部署侧工作；
- formal OOD 轴 A/B 数值、fleet 精确规模（T-432 activation record 冻结）；
- 500 t 实船参数确认（外部依赖）；
- 集合/概率环境预报（v1 单轨；扩展另立项）。

## User Stories

- 作为研究者，我要真实内河工况（桥区/港区/浅水/水流/交通/应急/靠离泊）生成的航次数据，使负荷预测模型训练分布具有任务真实性，而非合成速度剖面的堆叠。
- 作为研究者，我要条件随机化与反事实族（同一物理 truth × 不同信息/干预视图），使 Phase 4-E/F 能做因果干净的"预测信息价值"研究。
- 作为研究者，我要 T-429 可辨识性硬 Gate，保证声明不同的工况在合法信号空间真的可分（Phase 2.5 bridge/harbor 观察等价教训），论文结论可辩护。
- 作为 D 包实施者，我要 C 的产物逐字段满足冻结 producer schema，使 C/D Interface Gate 一次通过、T-432 解锁链可推进。

## Functional Requirements

### A. 契约遵从与范围（FR-499 衔接）

- **FR-501**: C 包产物 SHALL conforming 实现 `dataset_v3_producer_schema.json`（self_hash=7993e141…）全部十一项能力族（truth_stream / s_plan / planned_future_trajectory / static_constraints / input_k_snapshots / input_f_snapshots / forecast_provenance / oracle_counterpart / voyage_metadata / generation_provenance / ood_config_surface）；字段冲突时以机器 schema 为准；conformance SHALL 由 schema validator + C/D Interface Gate 机器校验。
- **FR-502**: Model E 零改动——全部工况 SHALL 经既有接口表达（SCN 场景表 / VOYAGE_* base 变量 / water_depth 输入 / ctrl_mode / MANUAL_CMD 通道等既有通道）；禁止修改 `.slx`、模型参数、Solver/Sample Time、Gate 阈值；若契约信号存在缺口 SHALL 升级用户裁决，不得改模型。
- **FR-503**: 基线法律角色——formal 生成 SHALL 使用 current frozen Model-E physical baseline v1.1 + control baseline v1.0；A2 historical v1.0 SHALL remain immutable historical regression/reference asset，不得作为 Mission-v3 formal 生成源；`meta.model_baseline_ref` SHALL 如实引用（不得混同三角色）。
- **FR-504**: 阶段隔离——formal 产物 `fixture_kind=NONE`；任何 PILOT_ONLY/fixture 标记不得进入 formal 路径；pilot causal fixtures（persistence 等）仅用于管线验证。

### A2. 生产侧认识论红线（D 契约内化为 C 编号需求）

- **FR-558**: `v_plan_mps` SHALL 为 planned/reference along-track SOG（计划语义），不是 realized future speed；禁止无条件复用 realized v_ref（D design R5）。
- **FR-559**: `s_plan_m[k|t₀]` SHALL 由 t₀ 时刻已知的计划轨迹/航线信息前向积分构造（x(t₀+k|t₀)）；`s_actual,future`、realized future speed、realized future position SHALL NOT 以任何形式进入 INPUT_K。
- **FR-560**: 位置相关的未来约束（海图水深、限速区、桥区）SHALL 经 planned route / s_plan 语义的 interval-aware sampling 取值（沿计划轨迹采样），不得沿 realized 未来位置采样。
- **FR-561**: K/F SHALL 为 issue-major (issue_time_s (M×1), lead_s (M×30), 各列 (M×30)) 双索引快照；lead 网格严格等于冻结的 {10,20,…,300} 且与 30×10 s target 网格同源对齐；禁止从最终 future series 反推 vintage。
- **FR-562**: formal 缺失 K/F 快照 = 契约/数据质量失败（FAILED_CONTRACT，FR-537），SHALL NOT 通过重建 future truth 填补；缺 snapshot 的窗口按 D 侧 DGN-07 规则 invalid 计数（阈值 0 ⇒ pipeline FAIL）。

### B. 工况语义（T-420~T-426，七类任务级工况族）

- **FR-505**: Bridge 工况 SHALL 表达为 Approach → Deceleration → Controlled bridge transit → Exit → Acceleration 段序列，重点形成出桥恢复的 propulsion transient；法规语义为"保持舵效的安全航速"（非越慢越好）；桥区几何与限速 SHALL 落 `static_constraints.bridge_zones` / `speed_limit_zones`（K 类静态约束）。
- **FR-506**: Harbor 工况 SHALL 表达为较长低速段 + 小幅速度调整 + 低加速度 + higher relative hotel load（"港区慢速通过"语义）；与 bridge 工况 SHALL 在生成语义与 metadata 标签上明确分离。
- **FR-507**: Shallow-water 工况 SHALL 以 water depth 为输入（经既有 `VOYAGE_WATER_DEPTH_M` 接口），navigation supervisor 决定 UKC 限速 v_limit；阻力增加 SHALL 由 Model E 既有浅水机制按水深自增（v-P 解耦）；不得绕过 Plant 直接注入 resistance multiplier 冒充浅水效应。
- **FR-508**: 水流工况 SHALL 将 U = −1 m/s 固定值升级为 U(t) = Ū + δU(t)，δU(t) 为时间相关的 smooth/bounded 随机过程（禁白噪声）；该实现 SHALL 同时作为 INPUT_F 真实预报语义（C-D2）的真值载体。
- **FR-509**: Traffic disturbance SHALL 表达为 cruise → slowdown → follow → recover 事件序列（前船减速/临时避让/速度恢复），作为 AI 预测难点工况。
- **FR-510**: Emergency mission SHALL 表达为 Standby → Emergency dispatch → Rapid acceleration → High speed → Mission operation 序列（公务船属性），可同步改变 P_hotel/mission load；高速段 SHALL 使用冻结 Model-E 可行/已验证速度包线的证据支持上段，而非人为便利限速。能力证据（继承只读引用，非 C 包选定参数）：T-412 v1.1 Gate G2 `full_power_speed_band_12.5_15.5_kn`，实测 `value_kn=14.620`（`model/simulink_modelE_(Phase4_Physics)/results/gate_result.json`，2026-09-05，band [12.5, 15.5] kn）；实现 SHALL NOT 硬编码新的 Plant 能力假设，具体计划高速区间在 design 从只读 Model-E 证据冻结。
- **FR-511**: Berthing 与 departure SHALL 分别设计（离泊 = startup → 低速操纵 → 驶入航道加速；靠泊 = 减速 → 低速接近 → 靠泊），禁止简单镜像复用。

### C. Full Voyage Template v3（T-427）

- **FR-512**: v1 SHALL 冻结三个 mission-archetype 槽位：routine patrol、emergency response、以及第三个 work/transit/high-demand 任务原型——第三槽位的最终身份/语义服从 FR-553 正交性裁决（`HEAVY_CURRENT` 保留为工作候选名；候选段序列 departure→cruise→upstream→shallow→work→return 见路线 §4/T-427）。硬顺序：**HEAVY_CURRENT 正交性审查 → 第三模板语义冻结 → template/stratum registry 冻结**；若与 environment regime 语义正交则保留，若重复编码水文条件则按 C-D7 授权重构为环境无关任务原型。routine patrol（departure→harbor→cruise→bridge→upstream→patrol→return→berth）与 emergency response（standby→departure→rush→shallow→bridge→mission→return）段序列按路线 §4 落 registry；模板组合段序列在 mission/route template registry 机器可读登记；template 是 `route_template_id` / `mission_template_id` 的语义载体。

### D. 条件随机化架构（T-428 / C-D3）

- **FR-513**: 随机生成 SHALL 表达为显式 conditional-dependency DAG（p(X)=∏p(X_i|Pa(X_i))）：层结构 L0 template → L1 environment regime → L2 operator profile → L3 segment/voyage realization → BASE DRAW → L4 受控干预层；除声明的父子关系外变量默认条件独立；禁止未声明的隐藏相关；DAG 配置机器可读（conditional-DAG config，hash 入 provenance）。
- **FR-514**: Environment regime v1 SHALL 冻结为 {NORMAL, FLOOD, DRY} 三类，作为水文变量（current/water level）的主耦合潜变量；环境实现可合法依赖 regime + route/reach；regime 语义是合成水文替代物，不得表述为普适真实河流规律；v1 不设第四 tidal regime（目标路线显式含感潮段时可经 spec amendment 增设）。
- **FR-515**: Operator profile v1 SHALL 为 {CALM, STANDARD, URGENT}，先验条件于 mission template；voyage 级 operator profile 与 segment 级操纵约束 SHALL 分离（URGENT 航次在 harbor/bridge 段仍受局部约束；禁静默改写全局 profile）。
- **FR-516**: Mission load SHALL 分解为 P_hotel,actual = P_hotel,plan + ΔP_hotel,unplanned：plan 分量条件于 mission template/phase/预声明 schedule，可作 INPUT_K；unplanned 残差为 future-unknown truth 分量（predictor-forbidden）；禁止只生成 realized 轨迹再贴标签为 plan。
- **FR-517**: Resistance uncertainty v1 SHALL 为 bounded voyage/base-draw 变量：属 base draw、family 成员共享同一实现（除非 family 显式声明 resistance 为干预轴）、predictor-forbidden、可作 METADATA/ORACLE 审计。
- **FR-518**: L4 干预 SHALL 分型：PHYSICAL_COUNTERFACTUAL（物理 truth 仅经声明干预改变，非干预 base-draw ancestry 固定）与 INFORMATION_COUNTERFACTUAL（物理/环境 truth 不变，仅信息产物/INPUT_F 改变）；family metadata SHALL 暴露 intervention type / axis / level，使 validator 能判定哪些 truth 字段必须不变。
- **FR-519**: ID/OOD guard-band 原则：连续变量的 IID 支撑与 OOD 支撑之间 SHALL 留显式缓冲带（δ>0），缓冲带不属 IID train/val/test 也不属 formal OOD 科学评估；H/T 方向反转；正式 ID 支撑界在 design 冻结，OOD 数值走 T-432 activation 纪律。
- **FR-520**: 禁止静默 clamp——随机抽取越出声明支撑或 Model E 验证域时 SHALL reject/resample 并记录（attempts、原因、按约束/stratum 的拒绝计数与率）；此类**接受前约束抽样拒绝**属正常生成行为（类别见 FR-537 的 REJECTED_SAMPLING_ELIGIBILITY，reason=SUPPORT_BOUND / PHYSICAL_DOMAIN_BOUND），不是 generator/契约缺陷；拒绝率过高 SHALL 暴露为生成设计问题（阈值留 design/C 预资格预注册）。

### E. Grouping 与 split 隔离（C-D1）

- **FR-521**: voyage_family_id SHALL 定义为 same base_draw + predeclared controlled intervention set（允许 factorial intervention matrix，不限单因子）；intervention matrix / family size SHALL 在生成前确定，不得为随机生成的偶然结果。
- **FR-522**: base_draw_id SHALL 标识干预之外共享的 realized ancestry（至少覆盖适用的：route/mission template identity、initial-condition draw、current/water-level/traffic/operator/hotel-residual realization seed-lineage 及其他外生扰动 lineage）；C SHALL 提供机器可审计的 seed-lineage provenance；same template ≠ same base draw。
- **FR-523**: split_group_id SHALL 等价于 leakage-relevant must_link 关系（same base_draw_id、shared realization lineage、paired/counterfactual sibling、同一物理航次的多信息视图、derived/re-encoded 产品等）的连通闭包；硬不变量：one voyage→one family、one family→one split_group、one base_draw_id 不跨 split_group；split_group 可包含多个 family。
- **FR-524**: 未声明的 cross-family truth/base-realization 复用 SHALL 判 generation/provenance defect = Gate FAIL（禁静默 merge）；预声明的共享 SHALL 显式登记并强制 same split_group，split validator SHALL 验证无 shared base ancestry 跨 split。
- **FR-525**: 以下字段相同 SHALL NOT 单独构成 must-link：route_template_id、mission type、bridge/zone 几何、nominal/mean current、nominal water level、operator mode、场景标量参数值（realized stochastic/base draw 独立即可属不同 split group）。

### F. INPUT_F formal 预报（C-D2）

- **FR-526**: formal INPUT_F SHALL 为 MODEL_CONSISTENT_CAUSAL_CONDITIONAL_MEAN_FORECAST：X̂(t₀+h|t₀)=E[X_{t₀+h}|ℐ_{t₀}]（或经 causal state estimate x̂(t₀)=E(ℐ_{≤t₀}) 的等价形式）；OU/AR 等具体过程仅为实现族，不改变预报语义；water-level forecast 同构但允许不同 dynamics/correlation scale。
- **FR-527**: Observability 边界——forecast 消费的信息 SHALL 部署可观测或因果可估计：source class 白名单 {OBSERVED, CAUSALLY_ESTIMATED, STATIC_KNOWN_PARAMETER, PREDECLARED_MODEL_PARAMETER}；禁止 FUTURE_TRUTH / ORACLE_STATE / SIMULATOR_LATENT_PRIVILEGED（部署不可观测的 latent state SHALL 经 causal estimator 形成 x̂(t₀)，不得直读 simulator 真态）。
- **FR-528**: Forecast-quality profile——GOOD = 声明的部署信息模型下最优因果信息态经正式过程模型的条件均值前向预测（不得无条件定义为读取 simulator 真态）；DEGRADED SHALL 由 issue 时刻信息态的因果退化实现（observation/telemetry lag、状态估计噪声、慢变 bias、时间相关观测误差，仅消费 t≤t₀ 信息）；DEGRADED 噪声 SHALL NOT 以逐 issue iid jitter 为正式默认（建议 voyage-level bias / 时间相关观测误差过程）；具体 lag/noise/bias/相关参数在 design 预注册；profile（如 GOOD_V1/DEGRADED_V1）SHALL 可追溯到 frozen config/hash。
- **FR-529**: Forecast-quality counterfactual truth-isolation——GOOD/DEGRADED 族 Truth SHALL 完全相同（actual current/water-level/traffic/physical truth），仅 INPUT_F 不同； SHALL 共享 base_draw_id/family/split_group；C SHALL 提供 same-truth hash/provenance lineage 机器校验。
- **FR-530**: Forecast provenance SHALL 含冻结六字段 + forecast_quality_profile_id；formal provenance class 仅 CAUSAL_GENERATOR；ORACLE-derived / future-truth-derived / simulator-latent-privileged source SHALL 一律拒收。
- **FR-531**: v1 预报表示 SHALL 为单轨 deterministic point conditional mean（每 lead 一列）；不增加 ensemble/PDF/confidence 维度；environmental forecast ensemble 与 Phase 4-E 负荷 predictive quantiles 概念分离（后者不得被表述为前者的替代品）。

### G. 确定性科学 seed 树（Closeout A）

- **FR-532**: formal 随机生成 SHALL 使用显式命名可复现科学 seed 树（formal_generation_seed → base_draw_seed → current_truth/water_level_truth/traffic_truth/operator_residual/hotel_residual/resistance/其他声明分支/干预专属分支）；同一 seed lineage + 同一冻结配置 ⇒ 同一科学实现；禁止隐藏/全局 RNG 状态决定 formal 科学随机性；各随机分量 SHALL 有 auditable 命名 seed/substream provenance；INFORMATION counterfactual SHALL NOT 修改 physical-truth seed 分支；PHYSICAL counterfactual SHALL 仅改变显式声明的干预分支；未声明 truth/seed-lineage 复用 = Gate FAIL（C-D1）；派生/hash/substream 算法在 design 冻结。

### H. C→E/F 无旁路（Closeout B）

- **FR-533**: 架构边界 C → D producer contract → Dataset v3 → E/F SHALL 保持：Phase 4-E/F 不得对 Mission-v3 simulator 内部建立独立 predictor 接口；Mission-v3 内部变量（hydrologic latent state、未来随机创新、resistance latent draw、traffic 未来真值、privileged generator state、regime 内部等）SHALL NOT 因 simulator 可知而绕过 Dataset-v3 storage-role/access 控制；合法预测器信息仅经 INPUT_H1/H2/K/F（ORACLE 仅显式 Oracle 实验）。

### I. 合格生成栈身份（Closeout C）

- **FR-534**: C SHALL 维护机器可审计的 qualified generation-stack 身份（至少：Mission-v3 generator version/hash、mission/route template registry hash、conditional-DAG config hash、seed-tree policy/version、forecast generator hash、scenario-stratum registry hash、held-out token registry hash、Model-E 物理基线 hash、控制基线 hash、D producer-schema hash），跨 C/D Interface Gate → Stage A → Stage B 一致；qualified_stack ≠ formal_stack ⇒ qualification invalid；qualification 后的实质科学生成变更 SHALL 重跑相关 interface/qualification 检查后方可进 Stage B；复合 hash 实现留 design。

### J. Formal 航次合格判据（C-D4）

- **FR-535**: formal accepted voyage SHALL 同时满足：min_duration_s=1200 ∧ min_canonical_windows=60 ∧ unexpected_invalid_canonical_windows=0（全局 AND，不分 template；N(D)=⌊(D−600)/10⌋+1）；该判据为 artifact-eligibility 判据而非独立样本量声明；全局下限 SHALL NOT 用于静默延长/修改已抽取的任务轨迹。
- **FR-536**: 每 formal artifact SHALL 记录 expected_canonical_windows / valid_canonical_windows / invalid_canonical_windows / invalid_window_reason_counts；缺 K/F snapshot、schema 字段、forecast provenance、stream 数据 SHALL 判 producer 数据质量缺陷，不得被"剩余窗口≥60"掩盖。
- **FR-537**: 失败/排除分类学——SHALL 区分：
  - **REJECTED_SAMPLING_ELIGIBILITY**：接受前的正常约束抽样拒绝（合法抽取但不满足接受条件），reason 至少区分 `DURATION_BELOW_MIN`（D<1200）、`SUPPORT_BOUND`（越 ID 支撑）、`PHYSICAL_DOMAIN_BOUND`（越冻结物理域边界）；处置：不入 formal accepted、不占配额、记录 attempt/seed lineage/原因、仅按预注册 constrained-sampling seed-lineage 策略重采样；**不是 generator/契约失败**；
  - **REJECTED_STRUCTURAL_HELDOUT**：命中组合 held-out token 的结构性剔除（intentional support design，非缺陷，FR-543）；
  - **FAILED_CONTRACT**：schema/provenance 违例（缺 K/F 快照/缺必填字段/cutoff>issue 等）——slot failure；
  - **FAILED_GENERATION**：仿真/导出运行时失败——slot failure；
  - **FAILED_PHYSICAL_DOMAIN**：保留给**意外的域契约失败**——本已通过冻结生成器/域检查的候选或产物事后被发现物理非法/不一致（非正常的接受前支撑界拒绝）；
  契约/生成/意外域失败 SHALL NOT 静默换无关新 seed 顶替；formal retry SHALL 走同 slot/同 config/同科学 seed 策略；最终配额不符 = Gate FAIL。核心不变量：**expected constrained-sampling rejection ≠ generator/domain defect**。
- **FR-538**: formal accepted fleet 禁 D<1200 s 航次（含 <600 s 零窗口航次）；pilot/smoke 可故意含短航次/边界/拒绝用例验证管线；禁止"抽样不足→拼接延长→接受"式静默时长修补（除非延长本身是预注册生成器语义）。
- **FR-539**: MISSION_V3_PREQUALIFICATION SHALL 按相关 stratum（mission template / environment regime / operator profile 等）报告 attempts/accepted/拒绝计数与率/原因（含 SUPPORT_BOUND/PHYSICAL_DOMAIN_BOUND/STRUCTURAL_HELDOUT 分类），检测全局合格下限是否隐性重塑某一科学 stratum。
- **FR-540**: Activation 纪律——design 携带 C-D4 数值；T-432 activation record 通常原样复制；无"一次免费修订"；数值变更 SHALL 在 Phase-4E 结果检视前经显式 design/activation amendment（理由+影响分析+provenance）；activation 冻结后不得为迁就生成/模型结果改值。

### K. OOD 配置面与组合 held-out（C-D5）

- **FR-541**: 轴 C（compositional OOD）SHALL 定义为：组成因子各自在冻结 IID 支撑内且训练中各自可见（P_train(A∧¬B)>0 ∧ P_train(¬A∧B)>0，边际覆盖下限 design/activation 预注册），联合组合结构性缺席（P_IID(A∧B)=0）；成立需同时满足生成器支撑合法 + 实现的训练边际覆盖；边际越支撑者归轴 A/B，不得双重计入。
- **FR-542**: v1 冻结三 token 机器名与谓词语义：C1_BRIDGE_X_HIGH_ID_ADVERSE_CURRENT（BRIDGE_PASSAGE ∧ U_adverse∈HIGH_ID_BAND，带严格在 ID 支撑内，沿迹 adverse 分量按声明符号约定）、C2_HARBOR_X_LOW_ID_HT（HARBOR_SEGMENT 由 route/segment metadata 或预声明任务语义定义，禁 realized speed/下游结果反推）、C3_EMERGENCY_SPRINT_X_LOW_ID_HT（EMERGENCY_SPRINT_SEGMENT 由 mission-plan/segment-role/planned-intent metadata 定义，禁 realized speed/power/P_load target/DG commitment 反推）；谓词 SHALL 仅消费生成侧因果 metadata（禁 future P_load target/预测误差/燃油/DG/PMS/SOC/事后控制器响应）；OOD 成员资格 SHALL 在 E 包评估前可定义。
- **FR-543**: C SHALL 建单一权威机器可读 held-out token registry（token_id/predicate_version/factor_a/factor_b/predicate_definition/predicate_scope/support_band_refs/id_action=EXCLUDE/ood_action=REQUIRE/physical_feasibility_rule）；IID 与 OOD 生成器 SHALL 消费同一谓词定义（禁手工双版本）；IID 侧命中 token 的候选 SHALL 在接受前结构性剔除并记 REJECTED_STRUCTURAL_HELDOUT（intentional support design，非缺陷，不得归入 FAILED_* 类）；OOD 侧 SHALL 定向要求谓词为真；validator SHALL 机器证明 IID 三 split 产物 P(token)=0 且 OOD 含意图 token；token 带边界在 formal 生成前预注册（显式数值或预注册生成分布分位带；禁事后用 formal-train 经验分位）。
- **FR-544**: primary 轴 C 科学评估集 SHALL 满足 token_match_count=1 且 axis_A_ood=false ∧ axis_B_ood=false；多 token 命中标 COMPOSITIONAL_MULTI_TOKEN_STRESS（保留为附加 stress 证据，不入主指标）；同时越连续支撑且命中 token 者标 marginal/multi-axis stress set；v1 token 集固定为三个，增删/语义替换需显式 spec amendment。
- **FR-545**: C-3 物理可行性前置——formal activation 前 SHALL 证明 C-3 在冻结 Model-E plant/control 与所选带/强度范围下具有非零可行支撑（验证域内、执行器/任务可行、无隐藏 clipping）；不得为保 C-3 修改 Model E 或放宽物理约束；若无可行测度，token 替换/删除需显式 spec amendment。
- **FR-546**: OOD 配置面——C 产物 SHALL 输出 ood_axis / ood_def_version / physics_domain_ref；越出 Model E 已验证物理域的场景 SHALL 标 PHYSICS_EXTRAPOLATION/STRESS_SET，不混入 OOD 指标；组合 held-out SHALL 预注册并在 IID 生成前结构性排除（禁事后按模型表现定 held-out）；MISSION_V3_PREQUALIFICATION SHALL 报告剔除前后计数（按 token/stratum）与剔除后边际因子覆盖。

### L. T-429 可辨识性审计（C-D6）

- **FR-547**: T-429 SHALL 为硬 Gate：验证声明科学可分的场景/段条件在合法信号空间产生可区分结构；与 D 包 L3 泄漏审计语义/阈值/PASS-FAIL 规则分离（可复用 SBD 引擎；artifact 命名分离 identifiability_sbd_* vs leakage_sbd_*）；主分析单元 SHALL 为 SEGMENT_PURE_DIAGNOSTIC_SLICE（固定时长/固定采样聚合定义/完全含于单一声明段角色区间/固定边界缓冲/无跨段污染/标签继承生成 metadata；参数 design 预注册）；证据不足 SHALL 返回 INSUFFICIENT_IDENTIFIABILITY_EVIDENCE 并 fail-closed；禁用混合段窗口制造证据；canonical Dataset-v3 窗口仅可作次要非主诊断。
- **FR-548**: 标签与信号边界——类别标签 SHALL 仅来自生成侧语义 metadata（segment/route/mission role、direction、声明环境条件），禁从 realized 响应反推；探针/距离计算 SHALL NOT 消费 label-leaking metadata（segment_role/scenario_id/template/family/split_group/intervention 标签/ORACLE/FORBIDDEN 字段）；主监督探针 SHALL 使用冻结的 H1 派生因果/部署可得特征视图（编码在 Gate 运行前 design 冻结）；SBD SHALL 沿用定长 1 Hz P_load_total_kW shape 表示（与 Phase 2.5/D 工具连续）。
- **FR-549**: 双度量硬门——每必需对比 PASS(C_i)=PASS_SBD(C_i) ∧ PASS_probe(C_i)：SBD 分离比 R(F₁,F₂)=median(D_cross)/max(Q0.95(D_within,F₁),Q0.95(D_within,F₂)) ≥ R_min（或 design 批准的置信界等价）；探针为固定低容量分类器（logistic/固定线性探针为主，浅 MLP 仅非主诊断），pairwise balanced accuracy ≥ A_min（或置信界等价）；R_min/A_min SHALL 在首次正式 Gate 执行前预注册，见失败不得放宽；统计推断 SHALL 在 split_group_id/base_draw 层（group bootstrap，如 R 的 95% 下置信界），禁把重叠切片当独立观测；探针 CV SHALL 按 split_group_id 分组且预处理 train-only；探针配置（特征/预处理/超参/类加权/seed/判定规则）Gate 前冻结，禁逐对调参。
- **FR-550**: 必需对比注册表——SHALL 建 required_identifiability_contrast_registry（contrast_id/factor_axis/class_A/class_B/cohort_predicate/nuisance_controls/signal_view/SBD_required/probe_required/minimum_group_coverage），禁机械全配对多类矩阵（标签属不同语义轴且可重叠）；每对比 SHALL 声明混杂控制（matched cohort 或显式分层 over regime/route/direction/operator/mission context），对比不得因无关生成条件差异而通过；T-429 PASS ⟺ registry 全部对比 PASS；覆盖不足的必需对比 SHALL fail-closed，禁静默删除。
- **FR-551**: FAIL 处置红线——必需对比失败的唯一合法修复是 Mission/scenario 生成重设计（T-420~T-426 语义/机制）；禁止降阈值/删对比/并族改标签/事后选易切片/见结果改特征视图；确证物理/观察等价者唯一合法路径 = evidence → spec amendment → 用户批准 → 族/对比语义修订；T-429 与 D-L3 互不决定对方 PASS/FAIL，阈值独立治理。

### M. Strata 分类法（C-D7）

- **FR-552**: canonical scenario_stratum 键 SHALL 为 mission_template_id × environment_regime；direction/operator profile/traffic/resistance 等为格内 metadata/variation，不入 split 键；stratum 是 base-draw/split-group 属性：base_draw_id→exactly one stratum、split_group_id→exactly one stratum（一组多格=Gate FAIL）；L4 干预 SHALL NOT 改变 stratum。
- **FR-553**: C SHALL 建预注册机器可读 scenario_stratum_registry（scenario_stratum_id/mission_template_id/environment_regime/status∈{ACTIVE, STRUCTURAL_NOT_APPLICABLE, OUT_OF_SCIENTIFIC_SCOPE}/reason/version）；非 ACTIVE 格 SHALL 在 formal 生成前预注册并附科学/物理可辩护理由，禁事后空格化；ACTIVE 格不满足代表性要求 = 生成/设计缺陷；registry 冻结前 SHALL 完成 mission template 与 environment regime 的语义正交性检查（特别裁决 HEAVY_CURRENT 是否已编码水文条件；合法处置 = 重构为环境无关任务原型，或声明不兼容格非 ACTIVE；禁为凑九格硬做 Cartesian 完备）。
- **FR-554**: 代表性下限——每个 ACTIVE 格 SHALL 满足 n_unique_split_groups ≥ 14（计数单位=unique split_group_id，非 voyage/view 记录；70/15/15 下约 10/2/2；floor 非统计功效声明、非强制均衡配额，formal 配额通常应超过）；不足 = STRATUM_REPRESENTATION_DEFECT → Gate FAIL；pilot 单成员 stratum 全落 train 行为 SHALL NOT 延续到 formal。
- **FR-555**: 禁止自动/数据依赖的粗化回退（mission×regime→regime-only→template-only）；分类法粗化唯一合法路径 = evidence → 显式 design/spec amendment → 用户批准 → 冻结修订 → formal 重新生成（须 Stage-B 前且在 E 包结果使用前）；粗投影（mission/regime projection）仅作审计/报告，不替换 canonical 2D 键；组合 OOD held-out 不自动清空 stratum（格级总体与格内 held-out 区分）。
- **FR-556**: OOD 记录 SHALL 携显式 domain metadata（dataset_domain=OOD、ood_axis、ood_token_id 如适用）；仅 dataset_domain==IID 记录入 IID 70/15/15 stratum-aware splitter；OOD 记录的 scenario_stratum_id 仅为审计参照；格内 direction/operator/traffic 覆盖 SHALL 在预资格/formal 报告（合法条件排除不算失败，但不对称须可见且与预注册 DAG 一致）。
- **FR-557**: MISSION_V3_PREQUALIFICATION SHALL 产出**实际执行的机器可读 stratum 审计报告**（非仅存在检查器）：对每个候选/注册格报告 registry status、attempted base draws、accepted unique split groups、voyage/view 计数、family-size 分布、投影 train/val/test 组数、direction/operator 覆盖、structural-heldout 与 eligibility/support/domain 拒绝计数、当前样本对 14 组下限的满足情况；预资格 SHALL NOT 宣称最终 formal Stage-B 配额已满足——「∀ ACTIVE 格 n_unique_split_groups≥14」的最终义务留在 D/T-432 qualification/activation 链（Stage B 前约束）；formal fleet 规模 SHALL 从冻结分类法/配额推导（联合满足 stratum floor、family 倍数、IID/OOD 配额、C-D5 边际覆盖义务），不得拍通用区间。

## Non-Functional Requirements

- **NFR-427**: Model E 零改动与 Gate 纪律——Gate FAIL（`error('GATE:*')`）即停并升级用户裁决；禁改冻结参数/阈值凑通过；脚本驱动，禁 GUI 手工修改形成不可复现状态。
- **NFR-428**: 可复现性——generation seed tree、config/registry hash、artifact hash、provenance（版本/脚本/commit SHA）机器记录；正式结果可通过正式脚本重跑；同 seed+同配置两跑科学产物 hash 一致。
- **NFR-429**: 仓库内可执行脚本禁止硬编码绝对路径（MATLAB 用 mfilename+pwd 兜底，Python 用 `__file__` 推导；仓库外依赖走环境变量优雅降级）；验收时绝对路径扫描零命中。
- **NFR-430**: §20 README 两级同步——新增/移动/删除文件同步更新所在目录 README；docs/phase4/ 下新建 c_mission_v3/ 目录索引；涉及模型/预测模型清单变化时同步 model/ 两级文档。
- **NFR-431**: 统计解释纪律——窗口是训练样本而非独立统计单位，独立层级至少为 voyage/split_group；split/bootstrap/CI 按组聚合；similarity ≠ leakage；AI OOD ≠ Plant physics extrapolation；条件随机分布族与相关系数全部预注册。
- **NFR-432**: R2026a 工程坑清单与 D 包事故教训继续生效：数字开头脚本 eval(fileread)；薄入口+包函数惯例；禁 base 重跑含循环的配置脚本（reset 走显式 assignin 白名单，D-5/D-14）；From Workspace Interpolate off（ZOH）；MATLAB 禁链式索引；-batch 多行命令落临时 .m；PMS_SOURCE_MODE 显式设定（不得依赖 reset 恢复值，D-14）。
- **NFR-433**: 诚实记录纪律——AI_MODELING.md 数据红线适用（禁隐藏失败实验/禁偷换目标/禁改指标造好结果）；T-429 失败与 NO-GO 结论同为正式记录内容。

## Acceptance Criteria

- [x] **AC-433**: T-419 架构设计交付——Mission Generator v3 架构文档（conditional-DAG、L0~L4 层语义、模块边界、与 producer schema 的接口映射、全部机器可读 registry 清单）落库并经用户批准（先设计后施工）。**✅ 2026-09-05 Day-1 Gate Ruling：CONDITIONAL PASS，A1/A2/A3 收尾项（truth_stream 0.01s 口径/Day-1 终态/BRIDGE_OPERATION 宿主语义）已落地 → APPROVED；PHASE2_ENTRY_GATE=true**
- [ ] **AC-434**: 工况与模板实现——T-420~T-426 七类任务级工况族 + T-427 三模板在 Model E 零改动下生成并仿真成功（机器可读结果）；每工况/模板有确定性复跑证据（同 seed 同结果）。
- [ ] **AC-435**: Producer schema 十一项能力落地——C 产物经 `validate_contracts --producer-schema` PASS + C/D Interface Gate 正式版（含 D-15 必改项）overall=true（FR-501/526~531/535~536/541~546）。
- [ ] **AC-436**: 因果与隔离负向验证——注入缺陷全部正确检出：future-truth 冒充 INPUT_F、realized future 冒充 s_plan/v_plan（FR-558/559）、沿 realized 未来位置取位置约束（FR-560）、lead 网格偏移或 vintage 反推（FR-561）、formal 缺快照被静默填补（FR-562）、simulator-latent-privileged 预报源、未声明跨 split_group truth 复用、GOOD/DEGRADED truth 漂移、PILOT_ONLY 混入 formal、FORBIDDEN/ORACLE 越权入 view（FR-524/526~533/504/558~562）。
- [ ] **AC-437**: T-429 可辨识性 Gate overall PASS——required contrast registry 全对比双度量（SBD 分离比 + split-grouped 探针 BA）PASS，group-level 推断与混杂控制证据落库（FR-547~551）。
- [ ] **AC-438**: Seed tree 与生成栈身份——同 seed lineage+同配置两跑科学产物 payload hash 一致；INFORMATION counterfactual 成员 truth hash 相同；qualified generation-stack 十项身份机器可审计（FR-532/534/529）。
- [ ] **AC-439**: Strata/OOD 审计——scenario_stratum_registry 冻结（含 HEAVY_CURRENT 正交性裁决记录，硬顺序 FR-512）；MISSION_V3_PREQUALIFICATION 的机器可读 stratum 审计报告实际产出且全项（FR-557），N_grp≥14 检查机制存在且 fail-closed 被实证执行；**预资格不宣称 Stage-B 配额已满足**——ACTIVE 格 14 组下限的最终满足留 D/T-432 qualification/activation 链；token registry 双向谓词一致性机器校验（FR-543）。
- [ ] **AC-440**: 归档与卫生——README 两级同步、绝对路径扫描零命中、D 侧接口文档（c_producer_interface_contract）标注 C 实现完成状态；C→Phase4 PR 创建交用户审批。

## Edge Cases

- 航次生成失败（FAILED_GENERATION/FAILED_CONTRACT）：slot failure 记录 + 同 seed 策略 retry，禁静默换 seed（FR-537）；
- 合法抽取但 D<1200：REJECTED_SAMPLING_ELIGIBILITY，重采样须预注册策略+审计（FR-537/538）；
- 随机实现越出 ID 支撑/冻结物理域边界：REJECTED_SAMPLING_ELIGIBILITY（reason=SUPPORT_BOUND/PHYSICAL_DOMAIN_BOUND）+ reject/resample+计数，禁 clamp——正常约束抽样拒绝，非缺陷（FR-520/537）；
- 本已通过冻结检查的候选/产物事后发现物理非法：FAILED_PHYSICAL_DOMAIN（意外域契约失败，slot failure）（FR-537）；
- 组合 token 命中：REJECTED_STRUCTURAL_HELDOUT（intentional，非缺陷）（FR-543/537）；
- 某 stratum 拒绝率异常高：生成设计问题暴露，预资格报告（FR-539/557）；
- T-429 证据不足：INSUFFICIENT_IDENTIFIABILITY_EVIDENCE fail-closed（FR-547）；
- 两工况确证物理等价：spec amendment 路径，禁静默并族（FR-551）；
- forecast source_cutoff>issue 或 provenance class 非法：validator 拒收（FR-530/526）。

## Dependencies and Constraints

- **硬依赖（已冻结输入）**：D 包四份契约文档 + producer schema（self_hash=7993e141…）+ signal registry（self_hash=9ab11dea…）；Model E 冻结基线（物理 v1.1+控制 v1.0）；`+dataset_v3/export_pilot.m` 为 artifact 布局与 issue-major 快照参考实现；
- **下游锁定**：D 包 T-432/T-435-formal 在 C 完成 + C/D Gate overall=true 前保持 BLOCKED（FR-496/498 链）；E/F 不可开工（依赖 Dataset-v3 Freeze）；
- **环境**：MATLAB R2026a（坑清单 NFR-432）；Python `.venv_tf`；全部 Mac 本机执行；
- **纪律**：AGENTS.md 全文（Git 纪律/质量门禁/§20）；AI_MODELING.md 数据红线；FR-499 gate reopening 纪律（契约实质漂移重开最早受影响门）；
- **委派授权**：大规模生成类任务可在 tasks 批准后委派 subagent（写+独立审查 crosscheck），主 Agent 保留设计/决策/终验（HANDOFF §5）。

## Requirements 中不得冻结的事项（bounded design/activation obligations）

以下事项本文件只冻结义务与法律地位，具体值由 design / C 预资格预注册 / T-432 activation 冻结，且 SHALL NOT 从 Phase-4E 模型结果回调：

1. current/H-T 的 IID 支撑界数值与 guard-band δ（design 冻结；OOD 数值仍走 activation）；
2. HIGH_ID_BAND / LOW_ID_BAND 边界（formal 生成前预注册）；
3. DEGRADED profile 的 lag/noise/bias/correlation 参数（design 预注册）；
4. 环境过程（δU、水位）具体参数与 traffic 过程族/细节（design）；
5. T-429 诊断切片时长/边界缓冲/stride、H1 探针特征编码、R_min/A_min、group-bootstrap 置信规则（design，Gate 首跑前冻结）；
6. required_identifiability_contrast_registry 完整清单与每对比 nuisance controls（design）；
7. 轴 C 边际覆盖下限（design/C 预资格预注册）；
8. 拒绝率合格阈值（design/C 预资格预注册）；
9. scenario_stratum_registry active/non-active 格清单（design；HEAVY_CURRENT 正交性裁决为其冻结前置）；
10. seed 派生/hash/substream 算法与 qualified-stack 复合 hash 实现（design）；
11. formal fleet 精确规模与配额（T-432 activation record，由冻结分类法推导）；
12. 各 machine-readable registry 的具体文件名/格式（design）。

## Open Questions

- [x] 无阻塞性开放问题。HEAVY_CURRENT 正交性裁决为 design 阶段 scenario_stratum_registry 冻结的前置项（FR-553），非 requirements 阻塞。

## 修订记录

> **FR-549 Amendment Record（Day-3 T429 V2 Gate Ruling，2026-09-06）**：DIR_UPSTREAM_VS_DOWNSTREAM 的 SBD_required=false（NCC 幅度不变性，D1-A 实证 R_paired=0.315 vs 方向 BA 0.944）；DIR Gate = PASS_paired_physical_effect AND PASS_probe（paired physical effect：same base_draw/split_group directional pair、组级聚合 signed paired delta、符号必须正确、bootstrap 95% CI 排除零、配对符号检验 alpha=0.05、禁从 D1-A 83.8 kW 推幅度阈值）；probe 不变（A_min=0.80 + 冻结组级不确定度规则）；SBD 转为 SECONDARY_DIAGNOSTIC_NOT_APPLICABLE_TO_LEVEL_SHIFT_FACTOR 仍报告不判 Gate。机器记录：results/mission_v3/t429_amendment_v2.json（self_hash 796a8b07…）。

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-05 | requirements 初稿（C-D1~C-D7 + Closeout A/B/C 翻译） | 本 session |
| 2026-09-05 | requirements 批准（CONDITIONAL PASS + R1~R7 闭合）：R1 T-420~T-426 更正为七类任务级工况族；R2 FR-512 改三 mission-archetype 槽位制（第三槽位服从 FR-553 正交性裁决，硬顺序冻结）；R3 C 自有 ≈100 规模生成验证更名 MISSION_V3_PREQUALIFICATION（与 D/T-432 Stage A 法律分离）；R4 预资格须实际产出机器可读 stratum 审计、14 组下限最终义务留 T-432 链；R5 失败分类学拆分——接受前约束抽样拒绝（SUPPORT_BOUND/PHYSICAL_DOMAIN_BOUND）≠ 意外域契约失败（FAILED_PHYSICAL_DOMAIN）；R6 生产侧认识论红线提升为 FR-558~562（v_plan/s_plan 计划语义、禁 realized future 入 K、interval-aware 约束采样、issue-major 快照、缺快照=FAILED_CONTRACT）；R7 FR-510 极速值补上游证据（T-412 v1.1 G2 value_kn=14.620，继承只读引用） | 用户 requirements gate 评审 |

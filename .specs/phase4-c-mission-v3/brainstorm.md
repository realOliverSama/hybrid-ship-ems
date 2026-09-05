# Phase 4-C — 真实内河航次与工况生成器 v3（Mission v3，T-419~T-429） Brainstorm

Spec: `phase4-c-mission-v3`  
Status: brainstorm-complete  
Created: 2026-09-05

## User Intent

## 用户意图

按 docs/phase4/c_mission_v3_HANDOFF.md 启动 Phase 4-C：实现 Mission Generator v3（T-419~T-429），作为 Dataset v3 的生产侧（D 包已冻结消费侧契约：c_producer_interface_contract.md + config/dataset_v3_producer_schema.json 机器权威）。C 必须输出 producer schema 十一项能力族，满足因果红线（s_plan 计划前向积分、INPUT_F 满足 F_{t₀}=g(ℐ_{≤t₀})、formal 仅 CAUSAL_GENERATOR、ORACLE 物理分离、formal 禁 PILOT_ONLY、Model E 零改动）。生成逻辑改为 Route → Environment → Navigation Constraint → Operator → Plant。C 对已批准契约的 conforming implementation 不重走四门禁（FR-499），C 自身架构/生成机制走本 spec 四门禁。

## Brainstorm 七问裁决（C-D1~C-D7，全部用户书面批准）

### C-D1 split_group_id grouping policy（APPROVED WITH GROUPING REFINEMENT）
- voyage_family_id = same base_draw + predeclared controlled intervention set（不限单因子，允许 factorial family 如 traffic×forecast 2×2）；intervention matrix/family size 生成前确定。
- base_draw_id 标识 intervention 之外共享的 realized ancestry（route/template、初值抽取、current/water-level/traffic/operator/hotel residual realization seed-lineage 等）；same template ≠ same base draw；C design 必须提供机器可审计 seed-lineage provenance。
- split_group_id = voyage_family_id 仅为默认映射；硬不变量：family→exactly one split_group（family 绝不可跨 split）；split_group 可更粗（one group ⊇ multiple families）。
- split_group_id = leakage-relevant must_link 关系（same base_draw、shared realization lineage、paired/counterfactual sibling、derived products 等）的连通闭包/transitive closure。
- 未声明的 cross-family truth/base-realization 复用 = provenance defect = Gate FAIL；有意共享必须显式登记并强制 same split_group。
- 同 route_template/mission/几何/标称流速/水位/operator mode/场景标量相同不单独构成 must-link。
- forecast-quality counterfactual：one physical voyage truth + multiple forecast/information views（存储实现留 design）；共享 base ancestry + same family/split_group。
- grouping metadata 机器可审计字段：voyage_id/base_draw_id/voyage_family_id/split_group_id/route_template_id/mission_template_id/intervention 标识/seed-lineage；不变量：one voyage→one family→one split_group；one base_draw_id 不跨 split_group；未声明跨组 truth 复用=Gate FAIL。

### C-D2 Formal INPUT_F 预报语义（APPROVED WITH OBSERVABILITY REFINEMENT）
- formal INPUT_F = MODEL_CONSISTENT_CAUSAL_CONDITIONAL_MEAN_FORECAST：X̂(t₀+h|t₀)=E[X_{t₀+h}|ℐ_{t₀}]，或经 causal state estimate x̂(t₀)=E(ℐ_{≤t₀}) 的等价形式；OU 等具体过程仅为实现族（û=Ū+(û(t₀)−Ū)e^{−h/τ}），不冻结具体方程；water-level 同构但允许不同 dynamics/correlation scale。
- observability/simulator-privilege 边界：forecast 消费的信息必须部署可观测或因果可估计；source class 允许 OBSERVED/CAUSALLY_ESTIMATED/STATIC_KNOWN_PARAMETER/PREDECLARED_MODEL_PARAMETER；禁 FUTURE_TRUTH/ORACLE_STATE/SIMULATOR_LATENT_PRIVILEGED（部署不可观测的 latent state 必须经 causal estimator，不得直读 simulator 真态）。
- GOOD = 声明的部署信息模型下最优因果信息态（非无条件读 simulator 真态）；DEGRADED = issue 时刻信息态的因果退化（lag + 状态估计噪声 + 慢变 bias + 时间相关观测误差），禁逐 issue iid jitter 为正式默认；参数 design 预注册。
- GOOD/DEGRADED counterfactual truth-isolation：Truth^GOOD = Truth^DEGRADED（含 current/water-level/traffic/physical truth），仅 INPUT_F 不同；同 base_draw/family/split_group；建议机器校验 same-truth hash/provenance lineage。
- provenance 六字段保留 + 增 forecast_quality_profile_id（如 GOOD_V1/DEGRADED_V1，可追溯到 frozen config/hash）；formal 仅 CAUSAL_GENERATOR；ORACLE-derived/future-truth-derived/simulator-latent-privileged 一律拒收。
- v1 = 单轨 deterministic point conditional mean（30×F）；无 ensemble/PDF/confidence 维度；environmental ensemble ≠ load predictive quantiles（概念分离，E 包 q10/q50/q90 是未来 10s 平均负荷的条件预测分位数，非环境集合预报替代品）。

### C-D3 分层条件随机化（APPROVED WITH CONDITIONAL-DAG REFINEMENT）
- 显式 conditional-dependency DAG：p(X)=∏p(X_i|Pa(X_i))，声明父节点外默认条件独立，禁未声明隐藏相关。
- 层结构：L0 mission/route template（拥有 nominal 结构/segment 拓扑，可条件下游）→ L1 environment regime（NORMAL/FLOOD/DRY 三类冻结，为水文主耦合潜变量；环境实现可合法依赖 regime+route/reach；regime 是合成水文替代物，不得宣称为普适真实河流规律；v1 无第四 tidal regime）→ L2 operator profile（CALM/STANDARD/URGENT 先验条件于 template；voyage 级 profile 与 segment 级操纵约束分离——URGENT 航次在 harbor/bridge 段仍受局部约束，禁静默改写全局 profile）→ L3 segment/voyage realization（segment 长度/几何、current/water-level 过程参数与实现、traffic/event 过程、mission-load 实现、resistance uncertainty 等；具体过程族如 Poisson/renewal 不在 brainstorm 冻结）→ BASE DRAW → L4 受控干预层（非普通随机层）→ family members。
- mission load 分解：P_hotel,actual = P_hotel,plan + ΔP_hotel,unplanned；plan 部分条件于 template/phase/预声明 schedule，可作 INPUT_K；unplanned 残差为 future-unknown/predictor-forbidden truth 分量；禁只生成 realized 轨迹再贴标签为 plan。
- resistance uncertainty：v1 有界 voyage/base-draw 变量；属 base draw、族内共享（除非声明为干预轴）、predictor-forbidden、可作 METADATA/ORACLE 审计。
- L4 干预分型：PHYSICAL_COUNTERFACTUAL（如 traffic OFF/ON，物理 truth 仅经声明干预改变）vs INFORMATION_COUNTERFACTUAL（如 forecast GOOD/DEGRADED，truth 不变仅 INPUT_F 变）；family metadata/design 暴露 intervention type/axis/level 供 validator 判定哪些 truth 字段必须不变。
- ID/OOD guard-band 原则：U_OOD,min > U_ID,max 且 δ>0；guard band 区间既不属 IID train/val/test 也不属 formal OOD 科学评估；H/T 方向反转；候选 1.8/2.0 m/s 不在 brainstorm 冻结——正式 ID 支撑界 design 冻结，OOD 数值仍走 T-432 activation 纪律（outside frozen ID support ∩ inside evidenced Model-E domain，E 包结果检视前完成）。
- 禁静默 clamp：draw → 约束检查 → accept 或 reject/resample+record（attempts/原因/按约束与 stratum 的拒绝计数与率）；拒绝率过高=生成设计问题，不得被无限重采样掩盖；阈值留 design/Stage-A。
- 组合 OOD：预注册 + 结构性剔除 + 机器证明 P_ID(held-out)=0；不得事后从模型表现定 held-out。

### C-D4 Formal 航次合格判据（APPROVED WITH NUMERICAL REVISION）
- 冻结 v1：min_duration_s=1200、min_canonical_windows=60、unexpected_invalid_canonical_windows=0（全局 AND，不分 template；拒绝 1800/100——会系统性排除合法 20–29 min 航次并隐式改变 template 分布）。
- 窗口算术：N(D)=⌊(D−600)/10⌋+1（D≥600）；1200→61、1500→91、1590→100、1800→121。100 窗对应 1590 s 而非 1600 s。
- 统计解释：独立统计单元 = voyage/split_group，非重叠窗口；最小窗数是 artifact-eligibility/usable-window 一致性判据，非独立样本量声明。
- 窗完整性不变量：artifact 记录 expected/valid/invalid_canonical_windows + invalid_window_reason_counts；缺 K/F snapshot/schema 字段/provenance/stream 数据=producer 数据质量缺陷，不得被"剩余窗≥60"掩盖。
- 失败分类学：REJECTED_SAMPLING_ELIGIBILITY（合法抽取但 D<1200：不入 formal、不占配额、记录 attempt/seed lineage/原因、仅按预注册 constrained-sampling 策略重采样）/ FAILED_CONTRACT（缺快照/schema/provenance 违例：slot failure）/ FAILED_GENERATION（仿真/导出运行时失败：slot failure）/ FAILED_PHYSICAL_DOMAIN（违反冻结物理域规则：单独记录）；契约/生成失败不得静默换新随机 seed 顶替。
- formal Stage-B retry：预注册配额约束不变、失败 slot 保持未满足、retry 走同 slot/同 config/同科学 seed 策略、配额不符=Gate FAIL。
- 短航次：pilot/smoke 可故意含 <600 s 零窗口/边界/拒绝用例验证管线；formal accepted fleet 禁 D<1200（含禁 <600 s）。
- 禁静默时长修补（抽样不足→拼接延长→接受）；template 条件抽取→合格检查→accept 或 reject/resample+audit。
- Stage-A 按 stratum（template/regime/operator 等）报 attempts/accepted/拒绝计数与率/原因，检测全局下限是否隐性重塑某 stratum；拒绝率阈值留 design/Stage-A。
- activation 纪律：design 携带 C-D4 值，T-432 activation record 通常原样复制；无"一次免费修订"——数值变更须在 E 包结果检视前经显式 design/activation amendment（理由+影响分析+provenance）；activation 冻结后不得为迁就结果改值。

### C-D5 组合 OOD held-out token（APPROVED WITH COMPOSITIONAL-PURITY REFINEMENT）
- 轴 C 定义：组成因子各自在冻结 IID 支撑内且训练中各自可见（P_train(A∧¬B)>0、P_train(¬A∧B)>0，边际覆盖下限 design/activation 预注册），联合组合结构性缺席（P_IID(A∧B)=0）；需同时满足生成器支撑合法+实现的训练边际覆盖。
- 边际纯度：primary 轴 C 样本须 token_predicate=true ∧ 连续因子在 ID 支撑内 ∧ axis_A_ood=false ∧ axis_B_ood=false；越支撑归轴 A/B，不得双重计入。
- 三 token 冻结机器名：C1_BRIDGE_X_HIGH_ID_ADVERSE_CURRENT（BRIDGE_PASSAGE ∧ U_adverse∈HIGH_ID_BAND；带严格在 ID 支撑内；沿迹 adverse 分量符号约定；若向量流则用沿迹 adverse 分量）、C2_HARBOR_X_LOW_ID_HT（HARBOR_SEGMENT 由 route/segment metadata 或预声明任务语义定义，禁从 realized speed/下游结果定义；LOW_ID_HT 在 ID 支撑内且非轴 B）、C3_EMERGENCY_SPRINT_X_LOW_ID_HT（EMERGENCY_SPRINT_SEGMENT 由 mission-plan/segment-role/planned-intent metadata 定义，禁 realized speed/power/P_load target/DG commitment 响应）。
- 谓词信息边界：只消费生成侧因果 metadata（segment role/route role/mission role/direction/环境实现 metadata/H-T/流值或 regime/支撑带成员）；禁 future P_load target/预测误差/燃油结果/DG/PMS 结果/SOC 结果/事后控制器响应；OOD 成员资格须在 E 包评估前可定义。
- 带定义纪律：HIGH_ID/LOW_ID 边界不在 brainstorm 冻结，formal 生成前预注册（显式数值区间或预注册生成分布的分位带）；禁事后用 formal-train 经验分位定带；activation 核对无免费修订权。
- 机器可读 held-out token 注册表（权威谓词）：token_id/predicate_version/factor_a/factor_b/predicate_definition/predicate_scope/support_band_refs/id_action=EXCLUDE/ood_action=REQUIRE/physical_feasibility_rule；IID 与 OOD 生成器消费同一谓词定义，禁手工双版本。
- IID 结构性剔除：命中 token 的候选在接受前结构性剔除，单独记 REJECTED_STRUCTURAL_HELDOUT（token_id/stratum/seed lineage/计数）——是 intentional support design，非 generator 缺陷（不得归入 FAILED_CONTRACT/GENERATION/PHYSICAL_DOMAIN）。
- OOD 定向包含：轴 C OOD 生成器要求谓词为真；validator 机器证明 IID 三 split 产物 P(token)=0 且 OOD 含意图 token。
- 边际覆盖审计 + Stage-A 剔除前后计数按 token/stratum 报告（防剔除机制抹掉因子覆盖）。
- primary token 互斥：primary 轴 C 科学评估集 token_match_count=1；多 token 命中标 COMPOSITIONAL_MULTI_TOKEN_STRESS（保留为附加 stress 证据，不入主指标）。跨轴互斥：同时越连续支撑且命中 token 者标 marginal/multi-axis stress set，非纯轴 C 证据。
- C-3 物理可行性前置：formal activation 前须证在冻结 Model-E plant/control 与所选 LOW_ID_HT/emergency-intent 范围下非零可行支撑（验证域内、执行器/任务可行、无隐藏 clipping）；不得为保 C-3 改 Model E 或放宽物理约束；若无可行测度，token 替换/删除需显式 C-spec amendment。
- v1 固定三 token；增删/语义替换需显式 spec amendment。

### C-D6 T-429 可辨识性审计（APPROVED WITH IDENTIFIABILITY-AUDIT REFINEMENT）
- 法律目的：硬 Gate，验证声明科学可分的场景/段条件在合法信号空间产生可区分结构；与 D-L3 泄漏审计分离（可复用 SBD 引擎，禁共享阈值语义或 PASS/FAIL 规则）。
- 主分析单元 = SEGMENT_PURE_DIAGNOSTIC_SLICE（固定时长/采样聚合定义/完全含于单一声明段角色区间/固定边界缓冲/无跨段污染/标签继承生成 metadata）；参数 design 预注册；证据不足=INSUFFICIENT_IDENTIFIABILITY_EVIDENCE fail-closed；禁用混合段窗口制造证据；canonical 600 s 窗仅作次要非主诊断。
- 标签信息边界：只来自生成侧语义 metadata；禁信号反推（v_ship 阈值→HARBOR 等）；同 C-D5 边界。
- 信号视图：探针/距离计算禁消费 label-leaking metadata（segment_role/scenario_id/template/family/split_group/intervention 标签/ORACLE/FORBIDDEN）；主探针用 H1 派生的冻结因果/部署可得特征视图；SBD 沿用定长 1 Hz P_load_total_kW shape（与 Phase 2.5/D 工具连续）；两诊断回答不同问题（负荷形态可分性 vs 预测器可观测信号空间可分性）；H1 探针编码 Gate 运行前 design 冻结。
- SBD 分离比：R(F₁,F₂)=median(D_cross)/max(Q0.95(D_within,F₁),Q0.95(D_within,F₂))（对称保守分母）；R≥R_min 或 design 批准的置信界等价；R_min=1.5 仅候选，Gate 首跑前预注册且见失败不得放宽。
- 统计独立性：切片非独立单元；推断/稳健性至少在 split_group_id/base_draw 层（group bootstrap，如 R 的 95% 下置信界）；禁重叠窗口的虚高置信。
- 监督探针：第二必制度量；主探针=固定低容量（logistic 回归或等价固定线性探针），浅 MLP 仅可作非主诊断；特征视图/预处理/正则超参/类加权/seed/判定规则 Gate 前冻结；禁逐对调参。
- 分组 CV：split_group_id 分组；同 ancestry 同 fold；预处理 train-only（scaler 只 fit 训练组）。
- 探针指标：pairwise balanced accuracy BA≥A_min 或置信界等价；A_min=0.80 仅候选，首跑前预注册。
- 双度量硬门：PASS(C_i)=PASS_SBD∧PASS_probe；缺一不可；覆盖不足/无效 fold/未定义度量 fail-closed。
- 禁机械全配对多类标签矩阵（标签属不同语义轴且可重叠）；建 required_identifiability_contrast_registry（contrast_id/factor_axis/class_A/class_B/cohort_predicate/nuisance_controls/signal_view/SBD_required/probe_required/minimum_group_coverage；示例 SEG_BRIDGE_VS_HARBOR/SEG_BERTH_VS_DEPARTURE/DIR_UPSTREAM_VS_DOWNSTREAM/DEPTH_LOW_ID_VS_NORMAL_ID）；清单 design 冻结。
- 混杂控制：对比不得因无关生成条件差异而通过（如 BRIDGE 全 FLOOD、HARBOR 全 DRY）；每对比用 matched cohort 或显式分层（regime/route/direction/operator/mission context 按需在 design 声明）；目标是区分意图语义因子而非生成器偶然混杂。
- 完成规则：registry 全部对比 PASS 才 T-429 PASS；覆盖不足=INSUFFICIENT_IDENTIFIABILITY_EVIDENCE→Gate FAIL；禁静默删除对比。
- FAIL 红线：仅允许真实 Mission/scenario 生成重设计；禁降阈值/删对比/并族改标签/事后选易切片/见结果改特征视图；阈值与 registry Gate 前冻结。
- 物理等价族：确证等价则唯一合法路径=evidence→spec amendment→用户批准→族/对比语义修订；禁 agent 侧静默并删。
- 与 D-L3 关系：互供参照分布但阈值独立治理；artifact 命名分离（identifiability_sbd_*/leakage_sbd_*）；same distance engine ≠ same audit semantics。

### C-D7 strata 分类法（APPROVED WITH ACTIVE-CELL/GROUP-COUNT REFINEMENT）
- canonical stratum key = mission_template_id × environment_regime；direction/operator profile/traffic/resistance 为格内 metadata/variation，不入 split 键。
- stratum 是 base-draw/split-group 属性：base_draw_id→exactly one stratum；split_group_id→exactly one stratum；一组多格=Gate FAIL；L4 干预不改 stratum。
- 名义 3×3 格≠九格自动 ACTIVE：预注册机器可读 scenario_stratum_registry（scenario_stratum_id/mission_template_id/environment_regime/status∈{ACTIVE,STRUCTURAL_NOT_APPLICABLE,OUT_OF_SCIENTIFIC_SCOPE}/reason/version）；非 ACTIVE 须生成前预注册+科学/物理可辩护理由。
- 正交性检查（active-cell registry 冻结前置）：每 mission template 是否与 environment regime 语义独立；特别注意 HEAVY_CURRENT 是否已编码水文条件（若冗余/不一致：重构为环境无关任务原型，或声明不兼容格非 ACTIVE）；禁为保 Cartesian 表硬凑九格。
- 非 ACTIVE 格不得事后声明（ACTIVE 格生成不足=生成/设计缺陷；证据改状态需 Stage-B 前显式 spec/design amendment）。
- 组合 OOD held-out 不自动清空 stratum（格级科学总体 vs 格内结构性 held-out 区分；禁用 C-D5 剔除当空格借口）。
- 拒 N_grp=5（val/test 各仅 1 组太脆弱）；冻结 N_grp,min=14 unique split_group_id/ACTIVE 格（≈10 train/2 val/2 test；最小非退化代表 floor，非统计功效声明；formal 配额通常应超过）；计数单位=unique split groups 而非 voyage/view 记录（40 记录 10 组=10）。
- ACTIVE 格 <14 组=STRATUM_REPRESENTATION_DEFECT→Gate FAIL；pilot 单成员 stratum 全落 train 行为不得延续到 formal。
- 14 组 floor≠强制均衡（80/25/… 合法，须预注册配额+不随模型结果改权重）。
- 禁自动/数据依赖粗化回退（mission×regime→regime-only→template-only 拒绝）；分类法粗化=evidence→spec/design amendment→用户批准→冻结修订→重新生成，须 Stage-B 前且在 E 包结果使用前。
- 粗投影（mission_template_projection/regime_projection）仅作审计/报告，不替换 canonical 2D 键。
- 格内覆盖审计：Stage-A/formal 报 direction/operator/traffic 覆盖；合法条件排除不算失败，但所有不对称须可见且与预注册 DAG 一致。
- OOD 记录携 nominal stratum 仅审计参照 + 显式 domain metadata（dataset_domain=OOD/ood_axis/ood_token_id）；仅 dataset_domain==IID 入 IID 70/15/15 分层 splitter。
- 机器枚举 ID（如 STRAT_ROUTINE_PATROL__NORMAL）+ 组件字段分列 + validator 校验复合键与注册组件一致；禁自由文本。
- Stage-A stratum 审计：registry status/attempted base draws/accepted unique split groups/voyage-view 计数/family-size 分布/投影 train/val/test 组数/direction/operator 覆盖/structural-heldout 与 eligibility 拒绝计数；ACTIVE 格 ≥14 组才支撑 Stage-B activation 提案。
- formal fleet 规模推导：须联合满足 active-stratum floor、counterfactual family 倍数、IID 配额、OOD 配额、C-D5 边际覆盖义务等预注册要求——从冻结分类法/配额推导，而非拍"500–1000"区间。

## Closeout 约束（用户书面追加，与 C-D1~C-D7 同级）

### Closeout A — 确定性科学 seed 树
formal 随机生成用显式命名可复现 seed 树：formal_generation_seed→base_draw_seed→current_truth/water_level_truth/traffic_truth/operator_residual/hotel_residual/resistance/其他声明分支/intervention 专属分支。同 seed lineage+同冻结配置⇒同科学实现；禁隐藏/全局 RNG 状态；各随机分量具可审计命名 seed/substream provenance；INFORMATION counterfactual 不得改 physical-truth seed 分支；PHYSICAL counterfactual 仅可改显式声明的干预分支；未声明 truth/seed-lineage 复用=C-D1 缺陷=Gate FAIL。派生/hash/substream 算法留 design。

### Closeout B — 禁 C→E/F 预测器旁路
架构边界：C→D producer contract→Dataset v3→E/F。E/F 不得对 Mission-v3 simulator 内部建独立 predictor 接口；合法预测器信息由 D 治理（INPUT_H1/可选 H2/K/F；ORACLE 仅显式 Oracle 实验）；simulator 内部量（水文 latent state/未来随机创新/resistance latent/traffic 未来真值/privileged generator state/regime 内部）不得因 simulator 知道就绕过 Dataset-v3 role/access 控制。保持 D-D2/D-D3/C-D2 认识论边界。

### Closeout C — 合格生成栈身份
C/D Interface Gate→Stage A→Stage B 全程机器可审计的科学生成器身份：Mission-v3 generator version/hash、mission/route template registry hash、conditional-DAG config hash、seed-tree policy/version、forecast generator hash、stratum registry hash、held-out token registry hash、Model-E 物理基线 hash、控制基线 hash、D producer-schema hash。qualified_stack ≠ formal_stack ⇒ qualification invalid；qualification 后实质科学生成变更须重跑相关 interface/qualification 检查再进 Stage-B。复合 hash/文件实现留 design。

### D-基线措辞卫生
纠正过时措辞：current physical baseline v1.1=正式物理生成基线；current control baseline v1.0=正式控制生成基线；A2 historical v1.0=不可变历史回归/参照资产，不得当作 Mission-v3 formal 生成源。

## 留 design/activation 解决项（非 brainstorm 阻塞，禁从 E 包结果回调）
current/H-T ID 支撑界、HIGH_ID/LOW_ID 带、轴 A/B OOD 数值域、DEGRADED lag/noise/bias/相关参数、环境过程参数、traffic 过程族/细节、T-429 切片时长/缓冲/stride、H1 探针编码、R_min/A_min、group-bootstrap 规则、required contrast registry 清单、边际覆盖下限、Stage-A 拒绝率阈值、stratum active/non-active registry、HEAVY_CURRENT 正交性处置（stratum registry 冻结前置）。

## 契约红线（D 已冻结，C 不重议）
issue-time causality（s_plan 计划前向积分；禁 realized future 入 K/F）；双索引 schema（truth 单轴/KF (issue,lead) 快照）；双存隔离（INPUT_F vs ORACLE 物理分离）；阶段隔离（formal 禁 PILOT_ONLY，FR-500）；Model E 零改动（SCN 表/VOYAGE_* base 变量/water_depth 输入）；基线法律角色（formal=物理 v1.1+控制 v1.0）。

## 完成定义（C 包 STOP 条件）
spec 四门禁 + implementation 完成（producer schema 十一项全部落地 + T-429 审计 PASS）→ 停：C→Phase4 PR 交用户审批；D 包 formal 收尾另进行（C/D Gate 正式版含 D-15 必改→T-432 activation→Stage A→Stage B→T-435-formal→Freeze）。

## 剩余疑点
无 brainstorm 阻塞项；HEAVY_CURRENT 正交性为 design 冻结前必决项。

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.

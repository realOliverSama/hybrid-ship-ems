# Phase 4-D 数据集 v3 Brainstorm

Spec: `phase4-d-dataset-v3`  
Status: brainstorm-complete  
Created: 2026-09-04

## User Intent

## Phase 4-D 数据集 v3 — Brainstorm 裁决链（D-D1~D-D9，2026-09-04 用户逐项书面批准）

### D-D1 D/C 顺序（Q1）
APPROVED (a′) D-first contract / C-production / D-finalization。Phase 4-D 单 spec 持有 T-430~T-435；T-430/431/433/434/435 立即以非冻结 pilot 数据开展契约与管线工作；T-432 显式阻塞于 Phase 4-C Mission-v3 接口门；D 先定消费侧 Known-Future/观测契约，C 实现生产侧接口，接口冻结后 T-432 生成正式 fleet、T-435 出具最终 leakage/OOD 证据；pilot 统计不得晋升为 Dataset v3 科学基线。

### D-D2 Historical 观测契约（Q2）
三层分类：H1（默认进主 predictor）：P_load_total_kW/v_ship_mps/acceleration_mps2/n_prop_rpm/P_hotel_kW/v_current_mps(历史)/water_depth_m(历史)/P_prop_demand_kW/ctrl_mode/v_ref_mps/manual_power_cmd_pct；H2（extended-state，保留进 Dataset 非默认特征）：P_gen_actual_kW/P_bat_actual_kW/SOC/V_dc_V/state_DG1/2/V_bat_V/I_bat_A/P_bat_dis·chg_avail_kW(SOP)；H3（禁止进主 predictor）：PMS audit 族/EMS 变体内部/scenario latent（resistance_multiplier/scenario_id 等）。

### D-D3 K/F/U epistemic 契约（Q3）
K 类（计划/海图确定）：v_plan(=planned/reference 非真实未来航速)/h_chart(s)/桥区限速区位置约束/P_hotel_plan(仅 planned component，实际 residual 属 U)/route plan；F 类（预报）：U_fcst/WL_fcst；U 类（predictor-forbidden，数据集可物理存储但角色隔离 TARGET/ORACLE/METADATA）：未来实际 P_load/环境真值/突发交通/随机扰动。新增 issue-time causality 契约：x(t₀+k|t₀) 防 hindsight leakage。

### D-D4 未来表征（Q4）
X_KF(t₀)∈R^{K×F}；K/F 网格 = T-433 target 网格（不在 Q4 独立冻结）；s_plan[k|t₀] 由 C Mission v3 直接产生，严禁 s_actual,future 用于 future feature 构造；位置相关约束用 interval-aware sampling；INPUT_F+ORACLE 双存；forecast 生成器必须满足 F_{t₀}=g(ℐ_{≤t₀})（因果生成，禁未来真值后处理），保留 vintage/issue-time 语义；八类 storage role 批准：H1/H2/K/F/TARGET/ORACLE/METADATA/FORBIDDEN；Dataset Builder 按 manifest 白名单构造 model view；FORBIDDEN 永不可解锁，ORACLE 仅显式 Oracle 实验可用。

### D-D5 窗口几何（Q5）+ 五条实现约束
几何冻结：L_H=300s@1Hz / H_F=300s / K=30 / Δ_F=10s / stride=10s / 航次内完整窗（t₀∈[t_start+300, t_end−300]，禁 padding/跨 voyage）。C1 canonical history≠模型必消费全部，公平性靠 Dataset view（legacy view/H1-view），写入 T-436 接口纪律；C2 1Hz 用 channel-aware causal aggregation（功率区间均值/SOC endpoint/状态 last/flag any()），规则入 manifest，禁 raw[::100]；C3 历史 bin 区间语义 H_j=(t₀−j,t₀−j+1]；C4 target 统计语义移交 Q6；C5 dataset_anchor_stride_s=10 ≠ predictor_runtime_period_s=UNDEFINED_IN_D（manifest 分列）；C6 窗口非独立统计单位（独立层级=voyage，禁 random window split）；C7 每样本强制 METADATA（voyage_id/anchor_t0_s/window_index_in_voyage）；C8 <600s 航次零训练窗口，写入 D→C producer contract（正式训练航次应显著长于 600s）；C9 K/F 必须 issue-time snapshot（(issue_time,lead_time) long-table 或三维数组，禁从最终 future series 反推 vintage）；C10 truth 按 realization time、forecast 按 (issue,lead) 索引——schema 级区分；C11 reuse discipline not schema（新建 build_dataset_v3.py）；C12 长航次窗口权重偏差挂账 T-435/E 包。MATLAB 导出+Python 窗口化两段式批准。

### D-D6 Target 语义（Q6）
主目标 y_k=P̄_load,k=10s 区间均值 bus-side 总负荷功率（P_load_total_kW，dc_bus 域，TARGET，kW，shape [30]）；必须直接由 native-rate truth 累计能量差构建（禁从 1Hz 流二次计算；bin 无重复/遗漏、能量严格可加、规则入 manifest）；E_300=(10/3600)ΣP̄_k kWh 为 mandatory derived scalar（legacy 兼容锚点+evaluation view，默认不要求独立 output head/multi-task loss，如需则显式 ablation）；分位数=predictive quantiles（τ∈{0.10,0.50,0.90}，pinball loss 从 realized target 学习，数据集不预生成 q 标签，q10≤q50≤q90 不变量，防 crossing 留 E 包）；bin 内 realized quantiles≠predictive uncertainty（如需存 P_bin_realized_q* ORACLE 诊断，v1 不强制物化）；quantile 验证契约挂账 E 包（pinball/coverage 80%/0-60/60-180/180-300s 分段 calibration/crossing check）；q90=未来 10s 平均负荷预测分位数，非瞬时峰值，不承担 sub-10s 保护；Dataset view=trajectory[30]+legacy scalar E300。

### D-D7 Split/Leakage/OOD（Q7）+ 十项细化
IID split 70/15/15 voyage-level（若 C 产生 paired/counterfactual 航次族必须用 split_group_id 整体隔离；metadata 预留 voyage_family_id/route_template_id/mission_template_id/generation_seed/split_group_id；正式 split policy 待 C 生成机制确定后冻结；预声明 scenario strata 代表性，禁按模型误差反向调整）；split 资产=seed+显式 assignment artifact+hash（优先 deterministic hash/group assignment）；归一化 train-only，val/test/OOD 只 transform，默认禁 clip 回 train min/max（仅预定义物理 saturation 允许且入 feature contract）；三级泄漏审计 L1 provenance/identity（Hard FAIL）/L2 near-exact duplication/L3 SBD 异常相似（Suspect Flag，similarity≠leakage，不自动删数据；阈值 T_SBD=Q0.01(train-vs-train NN) 或 design 预冻结等价 policy，须在正式 fleet 结果前冻结）；OOD=第四独立集合（stronger current/lower water level/compositional unseen combination，ranges/held-out 清单/counts/seeds 在 spec design 预注册，禁事后挑选）；OOD∩Model E 已验证物理域（越域标 PHYSICS_EXTRAPOLATION/STRESS_SET 不混报）；pilot/formal 两阶段（pilot 只验证管线）；sealed test/OOD（evaluate→modify→evaluate 即降级 development evidence；E 包训练工具默认不加载 Test/OOD）；manifest 15 项字段（含 test_sealed/ood_sealed=true）。

### D-D8 挂账处理（Q8）
① 03 号脚本 isolate and preserve（LEGACY/SUPERSEDED/非 Model E 验证入口，新链不得调用）；② 10b CCS 判据=阈值不动+预声明稳态 eligibility mask（暂态豁免须记录豁免窗/最大偏差/稳态窗/PASS-FAIL；禁按误差动态选区间；design 冻结）——validation-harness semantic correction；③ E10 遗留注入=通用 case 隔离不变量（每 case 显式 restore nominal+只施加本 case 注入，禁依赖上一 case 清理），修复后重跑 E10+40 Gate 复验——test-harness hygiene；④ 旧 05/30/60/66+datasets/python/legacy_reference 冻结标 superseded（不删不搬，README 登记）+新建 Model-E-native 双段链（MATLAB producer 输出 truth/H1H2/KF snapshots/ORACLE/METADATA；Python build_dataset_v3.py 全责）；⑤ 500t=外部非阻塞依赖+invalidation rule（material 参数更新→模型修订→物理重验→必要时 fleet 重生成）；科研表述=representative 500-t-class surrogate；⑥ ②③ 登记为 validation infrastructure hygiene，禁用于调节已冻结物理/控制/Gate 参数。

### D-D9 交付物布局与 T-432 阻塞管理（Q9）+ 十四项细化
文档布局 docs/phase4/d_dataset_v3/：observation_contract.md/future_information_contract.md/c_producer_interface_contract.md/dataset_v3_schema.md（explanation layer）；必须增加 machine-readable authoritative contracts（signal/role registry + C-producer interface schema/validator，文件名留 design；字段 name/unit/role/source_signal/temporal_domain/aggregation/issue_time_semantics/predictor_access；Builder/exporter/validator 消费机器契约，禁多脚本独立 hard-coded list；Markdown 引用机器契约而非第二事实源）；D→C producer interface 11 项能力清单（s_plan 直出/K/F issue-time snapshots/F_{t₀}=g(ℐ_{≤t₀})/metadata/OOD 配置面等）；formal 航次显著长于 600s（600s 非充分条件）；T-432 保留本 spec 标 BLOCKED（blocked_by: C Mission-v3 producer-interface freeze），T-432 未 formal 完成前 spec 不得 implementation-complete（保持 implementation-in-progress + D0 completed 标注）；C/D Interface Gate 10 项检查（schema 符合/s_plan causal/KF snapshot 形式/F 不消费未来真值/KF 与 ORACLE 物理分离/metadata 完整/航次长度/OOD 配置面/manifest validator PASS/基线零改动）overall=true 才解锁 T-432；gate reopening 纪律（conforming implementation 不重走四门禁；实质契约漂移重开最早受影响门）；pilot 数据源=S01~S36/M1~M3+现有 scenario 机制，仅验证管线；pilot 缺 K/F 时用 causal fixtures（标 PIPELINE_FIXTURE/PILOT_ONLY，严禁 future truth 冒充 INPUT_F/K，不得进 formal fleet）；pilot 统计不得晋升基线；T-432 两阶段（Stage A ≈100 条 generation qualification / Stage B formal fleet 预注册 exact count/strata/seeds/OOD 定义，activation record 冻结，operational freeze 不重走四门禁）；T-432 Unblock Checklist 8 项机器化；最终执行顺序 D0→C→C/D Gate→T-432→T-435 formal→Dataset-v3 Freeze。

### 纪律边界
Model E 模型零改动；基线三件套事实源禁改；Gate FAIL 即停升级；R2026a 坑清单；禁绝对路径；§20 README 两级同步；每任务完成即 spec_task_check。

## Clarifying Questions Asked

按"一次一问"共 9 问，全部获用户书面裁决（裁决全文见上 D-D1~D-D9）：

1. **Q1 D/C 顺序**：T-432 硬依赖 C 包 Mission v3，D 先行定义还是 C/D 交替？→ (a′) D-first contract / C-production / D-finalization，单 spec 持有 T-430~T-435。
2. **Q2 Historical 观测变量集**：从 Model E 现有 ~100 个 logsout 打点中哪些进历史观测契约？→ 三层分类 H1/H2/H3（含 v_current/water_depth 历史侧进 H1；P_prop_demand+ctrl_mode+v_ref/manual_cmd 增补 H1；P_gen/P_bat/SOC/Vdc 降 H2；PMS audit/EMS internals/scenario latent 禁入）。
3. **Q3 K/F/U 变量级三分类**：Known/Forecast/Unknown 逐变量归属？→ 批准 + v_plan=planned/reference、P_hotel_plan 仅 planned component、U 类改 predictor-forbidden+storage role、新增 issue-time causality 契约 x(t₀+k|t₀)。
4. **Q4 K/F 表征与编码**：时间索引 vs 距离索引、序列 vs 事件？→ 统一时间网格序列编码（K/F 网格=T-433 target 网格）；s_plan[k|t₀] 由 C 直出（禁 s_actual,future）；F_{t₀}=g(ℐ_{≤t₀}) 因果生成；八类 storage role 白名单。
5. **Q5 窗口化几何**：300s@1Hz 历史 / 30×10s 未来 / stride 10s / 航次内完整窗 / 两段式构建？→ 批准 + 五条实现约束（Dataset view 公平性、channel-aware causal aggregation 禁 raw decimation、区间 bin 语义、stride≠runtime period、K/F issue-time snapshot 格式、长航次窗口权重挂账等，共 C1~C12）。
6. **Q6 Target 统计语义**：bin 均值 vs 瞬时、分位数语义 (a)/(b)？→ 区间均值主目标（native-rate 能量差构建）；E_300 mandatory derived scalar 无独立 head；predictive quantiles（pinball，τ={0.1,0.5,0.9}）；bin 内 realized q 仅 ORACLE 诊断。
7. **Q7 Split/Leakage/OOD 框架**：70/15/15 voyage split / train-only 归一化 / SBD v3 / OOD 三轴 / 两阶段 / no test tuning？→ 批准 + 十项细化（split_group_id 层级、assignment artifact+hash、禁 clip 回 train range、L1/L2/L3 分级、OOD∩物理域、sealed test/OOD、manifest 15 项）。
8. **Q8 挂账事项**：03 号脚本 / 10b CCS 判据 / E10 遗留注入 / 旧导出链 / 500t 挂账？→ 五项批准（isolate-preserve / 稳态 mask 修正 / case 隔离通用化 / 冻结+新链 / 外部依赖+invalidation rule），②③ 登记为 validation hygiene 禁调冻结参数。
9. **Q9 交付物布局与 T-432 阻塞管理**：文档布局 / T-432 状态 / pilot 数据源？→ 批准 + 机器可读契约为权威事实源、C/D Interface Gate 10 项、gate reopening 纪律、causal fixtures（PILOT_ONLY）、T-432 两阶段+Unblock Checklist 8 项、D0/Final 两层验收。

## Proposed Approach

D-first 三阶段路线（D-D1 a′）：

```text
D0 Contract/Pipeline（本 spec 立即可施工，pilot 数据）
  T-430 观测契约 + T-431 Known-Future 契约（文档 + 机器可读 registry）
  T-433/T-434 窗口化/target/分位数契约 + build_dataset_v3.py 管线
  T-435 split/leakage/OOD 审计实现 + pilot qualification
        ↓
Phase 4-C Mission v3（实现 D→C producer interface）
        ↓
C/D Interface Gate（10 项检查 overall=true）
        ↓
T-432 BLOCKED→READY→formal fleet（Stage A ≈100 qualification → Stage B 预注册正式生成）
        ↓
T-435 formal split/leakage/OOD 证据
        ↓
Dataset-v3 Freeze（spec 方可 implementation-complete）
```

技术主线：Model E 零改动仿真导出（MATLAB producer）→ Python `build_dataset_v3.py`（reuse discipline not schema）→ 八类 storage role 白名单 view；机器可读契约为唯一执行事实源，Markdown 仅解释层。

## Assumptions

1. Model E 物理/控制基线三件套（物理 v1.1 + 控制 v1.0 + A2 historical v1.0）为数据生成事实源，D 包零改动；
2. Model E 现有 logsout 打点（~100 信号）覆盖 H1/H2/H3 契约候选池，无需为 D0 新增模型打点（若 design 发现缺口须升级裁决，不得改模型）；
3. Pilot 数据源（S01~S36/M1~M3 + 现有 scenario 机制）足以验证管线正确性；缺失 K/F 时用标 PILOT_ONLY 的 causal fixtures；
4. Phase 2.5 v2 数据集纪律（voyage split/train-only 归一化/SBD 审计/provenance manifest）可复用其方法，不继承 schema；
5. `.venv_tf` 为 Python 既定环境；MATLAB R2026a 工程坑清单继续生效；
6. `target_vessel_confirmed=false` 贯穿 D 包，科研表述 = representative 500-t-class surrogate；
7. T-432 的 formal 规模/编号/文件名等 operational 参数在 activation record 冻结，不重走四门禁。

## Remaining Doubts

以下均为 **non-blocking design-pending** 事项（不重新打开 brainstorm，由 design/activation 阶段冻结）：

1. 10b CCS steady-state eligibility mask 的具体 state/time rule（design 冻结）；
2. machine-readable signal/schema artifact 的具体文件形式（JSON/YAML/MATLAB struct，design 冻结）；
3. formal OOD ranges / held-out combinations / counts / seeds（spec design 预注册）；
4. formal fleet exact count / strata allocation / seeds（T-432 activation record 冻结）；
5. C Mission-v3 生成机制确定后的 `split_group_id` 最终 grouping policy（C 就绪后冻结）；
6. SBD 的具体实现细节（requirements 只冻结其 audit 法律地位与预注册要求）；
7. MATLAB export script 最终编号/文件名（design 冻结）。

## Decision

- [x] User approved creating the spec skeleton and drafting requirements.（2026-09-04，含 pre-init housekeeping 与 requirements drafting discipline 指示）

# Phase 4-D 数据集 v3 Requirements

Spec: `phase4-d-dataset-v3`  
Status: requirements-approved  
Created: 2026-09-04  
Approved: 2026-09-04（CONDITIONAL PASS + R1~R5 修正闭合）  
Brainstorm: `./brainstorm.md`（D-D1~D-D9，2026-09-04 用户逐项书面批准）

## Overview

Phase 4-D 数据集 v3：在 Model E 零改动前提下定义观测契约（H1/H2/H3）、Known-Future 契约（K/F/U + issue-time causality）、窗口化/target 语义（300 s@1 Hz 历史、30×10 s 区间均值目标、predictive quantiles）、voyage/group split + 三级泄漏审计 + OOD 框架，构建 Model-E-native MATLAB 导出 + Python `build_dataset_v3.py` 两段式管线；T-432 formal fleet 阻塞于 Phase 4-C Mission-v3 接口门。

本文件只把已批准的 D-D1~D-D9 翻译为 FR/NFR/AC，不新增科学裁决；尚属 design/activation 的参数以 bounded obligation 形式给出（见「不得冻结事项」节）。

## Scope

### In Scope

- T-430 观测变量契约（H1/H2/H3 三层分类 + aggregation 语义 + 部署可获得性论证）；
- T-431 Known-Future 契约（K/F/U 认识论分类 + issue-time causality + forecast vintage + storage role）；
- 机器可读权威契约（signal/role registry + C-producer interface schema/validator）与四份 Markdown 解释层文档；
- T-433/T-434 窗口化 + target/E300/quantile 契约与管线（MATLAB 导出 + Python `build_dataset_v3.py`）；
- T-435 split/leakage/OOD 审计框架实现 + pilot qualification；
- Validation infrastructure hygiene：10b CCS 稳态 eligibility mask、E10/case 隔离不变量；
- Legacy 链 isolate/preserve/superseded 标记；
- D→C producer interface contract（C 包开工输入）；
- C/D Interface Gate 实现（C 就绪后执行）；
- T-432 formal fleet 生成（BLOCKED，gate 通过后方可施工）；
- T-435 formal split/leakage/OOD 证据与 Dataset-v3 冻结。

### Out of Scope

- Phase 4-C Mission v3 生成器实现（C 包职责；本包只产出消费侧接口契约）；
- Phase 4-E 预测模型训练/选型（T-436~T-444）与 Phase 4-F 预测式 EMS（T-445~T-455）；
- Model E 物理/控制任何改动（含 Gate 阈值、冻结参数）；
- N6/HIL/部署侧工作；predictor runtime period 定义（E/F 包职责）；
- 旧 Model D 时代资产的删除/移动（后续独立维护操作）；
- 500 t 实船参数确认（外部依赖，到位后走 spec 修订）；
- 独立的 E300 output head / multi-task loss（如需，E 包显式 ablation）。

## User Stories

- 作为研究者，我要一份明确的观测/未来信息契约，使预测器输入在部署时真实可得且无未来泄漏，用于检验 Known-Future 信息是否能缓解或突破历史-only R²≈0.73 的信息上限（backlog B1 兑现）。
- 作为研究者，我要 Dataset v3 的窗口/target/分位数语义严格定义，使 TCN/TFT/Transformer 等候选模型在统一口径下公平比较。
- 作为研究者，我要 voyage/group 级 split 与泄漏/OOD 审计的机器可读证据，使论文结论可辩护。
- 作为 C 包实施者，我要 D 包冻结的 producer interface 契约，使 Mission v3 输出一次到位不返工。

## Functional Requirements

### A. 范围与顺序（D-D1）

- **FR-458**: Phase 4-D SHALL 在单一 spec（本 spec）内持有 T-430~T-435 全部六个任务，不得将 T-432 拆分为独立 spec。
- **FR-459**: THE execution order SHALL be: D0 Contract/Pipeline → Phase 4-C Mission-v3 → C/D Interface Gate → T-432 Formal Fleet → T-435 Formal Audit → Dataset-v3 Freeze。在 T-432 与 T-435 formal 证据完成前，不得声称 Dataset v3 已正式冻结。
- **FR-460**: T-432 SHALL remain BLOCKED until the C/D Interface Gate reports `overall=true`；T-432 未 formal 完成前，本 spec 不得标记 `implementation-complete`（保持 `implementation-in-progress` 并显式标注 D0 completed / T-432 blocked）。

### B. 观测契约（T-430，D-D2）

- **FR-461**: THE Dataset v3 SHALL 定义三层历史观测分类（historical-observation taxonomy，注意：H1/H2/H3 是观测分类法，不是 storage-role 枚举，见 FR-464）：H1（默认进主 predictor）= {P_load_total_kW, v_ship_mps, acceleration_mps2, n_prop_rpm, P_hotel_kW, v_current_mps（历史）, water_depth_m（历史）, P_prop_demand_kW, ctrl_mode, v_ref_mps, manual_power_cmd_pct}；H2（extended-state，保留进 Dataset 但非默认特征）= {P_gen_actual_kW, P_bat_actual_kW, SOC, V_dc_V, state_DG1, state_DG2, V_bat_V, I_bat_A, P_bat_dis_avail_kW, P_bat_chg_avail_kW}；H3（predictor-forbidden 的历史/记录类信号）= {PMS audit 族, EMS 变体内部信号, scenario latent 变量}。每列 SHALL 附部署可获得性论证与 aggregation 语义。`ctrl_mode` 决定哪个操作员指令通道语义激活；非激活通道的表征 SHALL 是确定性的并在 design 冻结（具体表征不在 requirements 阶段选择）。

### C. Known-Future 契约（T-431，D-D3/D-D4）

- **FR-462**: THE Dataset v3 SHALL 定义未来信息三分类契约：K 类（计划/海图确定：v_plan=planned/reference（非真实未来航速）、h_chart(s)、桥区/限速区/航线几何、P_hotel_plan（仅 planned component）、route plan）；F 类（预报：U_fcst、WL_fcst）；U 类（predictor-forbidden：未来实际 P_load、环境真值、突发交通、随机扰动）。
- **FR-463**: 一切未来向特征 SHALL 满足 issue-time causality：x(t₀+k|t₀) 表示 t₀ 时刻可获得的关于 t₀+k 的信息；禁止 hindsight leakage。
- **FR-464**: THE Dataset v3 SHALL 使用八类 storage role：`INPUT_H1 / INPUT_H2 / INPUT_K / INPUT_F / TARGET / ORACLE / METADATA / FORBIDDEN`。Dataset Builder SHALL 按 manifest 白名单构造 model view；FORBIDDEN 永不可解锁；ORACLE 仅显式 Oracle 实验可用。H1/H2/H3 观测分类法（FR-461）与 storage role 的映射 SHALL 显式定义：H1→`INPUT_H1`、H2→`INPUT_H2`；H3 不等价于 FORBIDDEN——PMS audit 族/EMS 变体内部信号通常映射 `FORBIDDEN`，`scenario_id` 映射 `METADATA`，`resistance_multiplier` 等 scenario latent 可映射为 predictor-forbidden 的 `METADATA`/`ORACLE` 审计字段；不得因信号属于 H3 就物理删除用于 provenance/OOD/error-attribution 的合法 metadata/oracle 字段。
- **FR-465**: 机器可读契约（signal/role registry + C-producer interface schema）SHALL 为执行层唯一事实源；Builder/exporter/validator SHALL 消费该机器契约，不得在多个脚本中维护独立 hard-coded feature-role list；Markdown 文档仅作解释层并引用机器契约。
- **FR-466**: K/F 特征 SHALL 以 (issue_time, lead_time) 二维索引的 issue-time snapshot 存储；realized truth SHALL 以 realization time 单索引存储；二者在 schema 级区分；禁止从最终 future series 反推历史 issue time 的 K/F vintage。
- **FR-467**: INPUT_F 预报生成 SHALL 满足 F_{t₀} = g(ℐ_{≤t₀})（因果生成）；禁止把未来真值后处理定义为预报；forecast vintage/issue-time 语义必须保留；INPUT_F 与 ORACLE truth SHALL 双存且角色物理隔离。
- **FR-468**: s_plan[k|t₀] SHALL 由 C Mission v3 直接产生；D 包禁止用 s_actual,future 构造任何 future feature；位置相关约束 SHALL 使用 interval-aware sampling。

### D. 窗口几何（T-433，D-D5）

- **FR-469**: Canonical 历史窗 SHALL be 300 s @ 1 Hz = 300 个因果 1 s bin，H_j = (t₀−j, t₀−j+1]，j=300..1（无 300/301 歧义）。
- **FR-470**: 1 Hz 历史构建 SHALL 使用 channel-aware causal aggregation（功率量区间均值、SOC endpoint、状态量 last、event flag 用 any() 等逐列规则）；禁止 naïve raw decimation（raw[::100]）；aggregation 规则 SHALL 写入 manifest。
- **FR-471**: Canonical 未来窗 SHALL be 300 s = 30 个 10 s bin，F_k = (t₀+(k−1)·10, t₀+k·10]，k=1..30；K/F future-feature 网格与 target 网格严格同源。
- **FR-472**: Dataset anchor stride SHALL be 10 s；每个窗口的 (t₀−300, t₀] 历史与 (t₀, t₀+300] 未来 SHALL 完整属于同一 voyage_id；禁止 padding、跨 voyage/reset/scenario boundary 构造窗口。短于 600 s 的航次合法产生零训练窗口。
- **FR-473**: 每个样本 SHALL 携带 METADATA：voyage_id、anchor_t0_s、window_index_in_voyage。
- **FR-474**: Manifest SHALL 分列 `dataset_anchor_stride_s = 10` 与 `predictor_runtime_period_s = UNDEFINED_IN_D`；stride 不得被解释为部署推理周期。
- **FR-475**: Dataset v3 构建 SHALL 采用两段式：MATLAB/Model E producer（voyage-level 导出）→ Python `build_dataset_v3.py`（窗口化/审计）；v2 科研纪律可复用，v2 schema 不得继承（reuse discipline, not schema）。

### E. Target 语义（T-433/T-434，D-D6）

- **FR-476**: 主 TARGET SHALL be 30 个 10 s bin 的 bus-side 总负荷能量等价平均功率：y_k = (1/10 s)·∫_{F_k} P_load_total(t) dt（source signal = P_load_total_kW，dc_bus 域，unit kW，shape [30]）；target SHALL 直接由 native-rate realization-time truth 的累计能量差构建（E_k = E(t_k)−E(t_{k−1})），禁止从 1 Hz 降采样流二次计算；相邻 bin 无边界重复/遗漏，30 bin 能量严格可加；构建规则写入 manifest。
- **FR-477**: E_300 = (10/3600)·Σ_{k=1..30} P̄_k（kWh）SHALL be mandatory derived scalar target / evaluation metric / legacy scalar view；默认不要求模型独立 E300 output head 或 multi-task loss（如需，须显式 ablation）。
- **FR-478**: 分位数目标 SHALL be conditional predictive quantiles（τ ∈ {0.10, 0.50, 0.90}），由模型经 pinball loss 从 realized target 学习；数据集不得预生成 target_q10/q50/q90 标签；输出语义不变量 q10 ≤ q50 ≤ q90（防 crossing 实现属 E 包）。
- **FR-479**: bin 内 realized quantiles（若需要）SHALL 仅以 `P_bin_realized_q10/q50/q90` 命名存为 ORACLE diagnostic，不得作为训练 target，不得与 predictive quantile 混淆；v1 不强制物化。
- **FR-480**: Dataset v3 SHALL 至少支持 trajectory target view [P̄_1..P̄_30] 与 legacy scalar view E_300；模型公平比较 SHALL 通过 Dataset view 实现，不得修改 canonical target 定义（旧模型 legacy view 接口纪律适用于 T-436）。

### F. Split / Leakage / OOD（T-435，D-D7）

- **FR-481**: IID split SHALL be 70/15/15，最小隔离单位 = voyage_id，禁止 random-window split；若存在 paired/counterfactual/near-replicate 航次族，SHALL 定义并使用 split_group_id 使同族整体落入同一 split；metadata SHALL 预留 voyage_family_id/route_template_id/mission_template_id/generation_seed/split_group_id；正式 split policy 待 C 生成机制确定后冻结；split SHALL 在 group isolation 约束下对预声明 scenario strata 保持合理代表性；禁止根据模型误差反向调整 split。
- **FR-482**: Split 资产 SHALL 包含 split_seed + 显式 voyage/group→split assignment artifact + assignment hash；优先采用构建顺序无关的 deterministic hash/group assignment；正式生成后 split map 不得因文件顺序或代码变化漂移。
- **FR-483**: 一切 data-dependent preprocessing SHALL 仅在 train split 拟合；val/test/OOD 只 transform 不 refit；预处理参数 SHALL 作为模型配套 artifact 保存；默认禁止将 val/test/OOD clip 回 train observed min/max；仅预定义、有物理意义的固定 saturation 允许且必须进入 feature contract/manifest。
- **FR-484**: Leakage audit SHALL 分三级：L1 provenance/identity leakage（同 voyage_id 或 split_group_id 跨 split、raw artifact exact hash duplicate、exact duplicated window 等——任一发生即 Hard FAIL）；L2 near-exact duplication（严格容差内近乎复制且 provenance 支持重复生成判断——必须处置并记录）；L3 SBD anomalous similarity（Suspect Flag：标记 suspected_near_neighbor、保存 pair/distance/provenance、分类处置写入 manifest；SBD 不得单独定义泄漏，不得自动删数据）；SBD 阈值 policy SHALL 在 formal fleet 结果出现前冻结。
- **FR-485**: OOD SHALL 为第四独立集合（不计入 IID 70/15/15），三轴 = stronger current / lower water level (H/T) / compositional unseen route-mission combination；OOD 定义框架、轴 B candidate envelope、轴 C held-out combination list SHALL 在 spec design 中预注册；轴 A 数值 ranges 与全部 OOD counts/seeds SHALL 在 T-432 activation record 中、任何 E 包模型结果出现前冻结（轴 A 数值 = out(frozen ID support) ∩ in(evidenced Model-E feasible domain)，且冻结前须完成 read-only 物理可行性验证）；禁止事后挑选或增强 OOD。
- **FR-486**: 正式 OOD SHALL 满足 outside training support ∩ inside validated Model-E physical domain；超出物理域的场景 SHALL 单独标记 PHYSICS_EXTRAPOLATION/STRESS_SET，不得与正式 OOD 泛化指标混报。
- **FR-487**: IID Test 与 OOD SHALL be sealed terminal evaluation（不参与训练/早停/超参/架构/feature 选择；禁止 evaluate→modify→evaluate 循环）；若某集合被查看后发生任何模型开发决策，该集合降级为 development evidence，须生成新 sealed confirmation set 才能再次声称无偏评估；E 包训练工具应默认不加载 Test/OOD（访问门）。
- **FR-488**: Dataset manifest SHALL 至少记录：split policy/version、split seed、split group key、assignment artifact/hash、train/val/test voyage counts、normalization source=train_only、preprocessing artifact hash、OOD definition version、OOD generation seed、provenance duplicate count、exact duplicate count、SBD threshold policy、SBD suspect count+disposition、test_sealed=true、ood_sealed=true。
- **FR-489**: Pilot（D0）SHALL 仅验证管线正确性（splitter/group isolation/train-only normalization/role whitelist/leakage checker/SBD pipeline/OOD loader/负向检出）；pilot 统计（归一化统计量、SBD 分布、split 统计、OOD 表现等）不得晋升为 Dataset v3 科学基线；正式证据 SHALL 在 T-432 formal fleet 后由 T-435 出具。

### G. Validation hygiene（D-D8 ②③）

- **FR-490**: 10b CCS 稳态带判据 SHALL 保持数值阈值不变，仅将适用域限定为预声明稳态 eligibility mask（startup/synchronization/settling transient 不进入稳态 PASS/FAIL；eligibility 由预定义 control/plant state + fixed time-window rule 决定；禁止按仿真误差结果动态选择区间）；暂态 SHALL 记录 transient exemption flag/duration/max deviation 与 steady-state window/max deviation/PASS-FAIL；mask 规则在 design 冻结。
- **FR-491**: 每个独立验证 case SHALL 拥有完整输入初始化（运行前显式 restore/establish nominal bench state + 只施加本 case 注入；禁止依赖上一 case 清理状态）；E10 携带 E09 遗留注入 SHALL 修复；修复后 SHALL 重跑 E10 与所属 40 号物理 Gate 链确认无残留注入、无新 FAIL。

### H. Legacy 与外部依赖（D-D8 ①④⑤）

- **FR-492**: `03_run_smoke_tests.m` SHALL be isolated and preserved（不删除、不修改历史行为；README 标记 LEGACY/SUPERSEDED/NOT A MODELE VALIDATION ENTRYPOINT；Dataset-v3 新链不得调用/依赖；其结果不作为 Model E/Dataset v3 科学证据）。
- **FR-493**: Model D 时代导出脚本（05/30/60/66）与 datasets/python/legacy_reference 资产 SHALL 冻结标 superseded（不删除、不破坏历史复现；README 标记 SUPERSEDED FOR MODELE DATASET V3 / REPRODUCTION ONLY）；新 Dataset-v3 管线不得继承 Model-D feature/target/schema 假设；旧件删除不在本包范围。
- **FR-494**: `target_vessel_confirmed` SHALL remain false；科研表述 SHALL be "representative 500-t-class research vessel / literature- and engineering-derived surrogate"；invalidation rule：material target-vessel 参数更新 ⇒ Model-E 参数/spec 修订 ⇒ 物理重验 ⇒ 受影响时 formal Dataset-v3 重生成/重审计（material 含排水量/船体几何/吃水/螺旋桨/推进参数/机组定额与动态/电池容量功率 ECM/主要效率）。

### I. D→C 接口与 T-432（D-D1/D-D9）

- **FR-495**: D 包 SHALL 产出 `c_producer_interface_contract.md`（+ 机器可读 schema/validator），C Mission-v3 至少须提供：voyage-level realization-time truth、s_plan[k|t₀]、planned/reference future trajectory、static route/chart/zone constraints、INPUT_K issue-time snapshots、INPUT_F issue-time forecast snapshots、forecast vintage/issue-time metadata、ORACLE truth counterpart、voyage/family/template/group metadata、generation seed/provenance、OOD generation configuration surface。600 s SHALL 仅为几何最小 history+future 跨度；design/T-432 activation SHALL 预注册可度量的 formal-voyage 合格判据（以最小航次时长和/或每航次最小 canonical 窗口数表达）；formal 训练航次 SHALL 满足该预注册判据。
- **FR-496**: C/D Interface Gate SHALL 至少检查：① C 输出字段满足 frozen producer schema；② s_plan[k|t₀] 存在且 issue-time causal；③ K/F 为 (issue_time, lead_time) snapshot；④ INPUT_F generator 不消费 future truth；⑤ INPUT_K/F 与 ORACLE 角色物理分离；⑥ voyage/family/template/group metadata 完整；⑦ 航次长度满足预注册的 formal-voyage 合格判据（FR-495）；⑧ OOD 配置面存在；⑨ dataset schema/manifest validator PASS；⑩ Model-E 冻结物理/控制基线未被非授权修改；overall=true 后 T-432 方可 BLOCKED→READY。
- **FR-497**: T-432 SHALL 两阶段执行：Stage A generation qualification（约 100 条，验证生成稳定性/导出完整性/schema 一致性/吞吐/failed-voyage 处理/审计管线；工程 nominal 非样本量结论）；Stage B formal fleet（启动前预注册 exact voyage count、scenario/strata allocation、route/mission/template/group allocation、IID/OOD counts、generation seeds、OOD definition version；禁止"跑出效果再决定停在哪"）；formal fleet size 在 T-432 activation record 冻结（不改变 D-D1~D-D9 科学契约时不重走四门禁）。
- **FR-498**: T-432 Unblock Checklist SHALL 显式化：C Mission-v3 完成 ∧ C/D Interface Gate overall=true ∧ producer schema version/hash accepted ∧ D0 pilot pipeline qualification PASS ∧ formal fleet generation plan 预注册 ∧ OOD 定义预注册 ∧ Model-E 物理基线完整性 PASS ∧ 控制基线完整性 PASS；全部满足才允许 T-432=READY；不得仅因 C 包 PR 合并自动启动 formal fleet。
- **FR-499**: Gate reopening 纪律：C 对已批准契约的 conforming implementation 不需重走四门禁；K/F 认识论语义、s_plan causality、target/split/OOD 契约的实质漂移 SHALL 重开最早受影响的门；非语义实现变化（如文件格式）走 design/task amendment。
- **FR-500**: Pilot 缺失 K/F 时 SHALL 使用标 PIPELINE_FIXTURE/PILOT_ONLY 的 causal fixtures（contract fixture/synthetic causal forecast stub/static planned-route stub）；严禁用 future realization truth 冒充 INPUT_F/K；任何 pilot fixture 不得进入 formal Dataset-v3 fleet。

## Non-Functional Requirements

- **NFR-420**: Model E 零改动——D 包不得修改 Plant/EMS/PMS/Safety 任何已冻结参数、模型结构或 Gate 阈值；Gate FAIL（`error('GATE:*')`）即停并升级用户裁决，禁改参数凑数据。基线法律角色：formal Dataset-v3 生成 SHALL 使用 current frozen Model-E physical baseline v1.1 与 control baseline v1.0；A2 historical v1.0 SHALL remain immutable as a historical regression/reference asset，不得被当作当前 formal fleet 的 data-generation baseline；三者均可纳入 integrity/hash audit，但法律角色不得混同。
- **NFR-421**: 可复现性——split seed、生成 seed、artifact hash、provenance（模型版本/脚本/参数/commit SHA 条件允许时）应记录；正式结果可通过正式脚本重跑。
- **NFR-422**: 仓库内可执行脚本禁止硬编码绝对路径（MATLAB 用 mfilename+pwd 兜底，Python 用 `__file__` 推导；仓库外依赖走环境变量优雅降级）；验收时绝对路径扫描零命中。
- **NFR-423**: §20 README 两级同步——新增/移动/删除文件同步更新所在目录 README；docs/phase4/d_dataset_v3/ 建立索引条目；新增模型/预测模型时同步 model/ 两级文档。
- **NFR-424**: 统计解释纪律——窗口是训练样本而非独立统计单位；独立层级至少为 voyage；split/bootstrap/CI/scenario 表现按 voyage 聚合；similarity ≠ leakage；AI OOD ≠ Plant physics extrapolation。
- **NFR-425**: R2026a 工程坑清单与脚本驱动纪律继续生效（禁 GUI 手工修改形成不可复现状态；-batch 多行命令落临时 .m 等）。
- **NFR-426**: Q8-②/③ 修复登记为 validation infrastructure hygiene，不得用于重新调节任何已冻结 Model E 物理/PMS/Safety 参数或 Gate 阈值。

## Acceptance Criteria

### D0 Contract/Pipeline Completion（Phase 4-C 前可达成）

- [ ] **AC-425**: T-430/T-431 契约交付——`observation_contract.md`、`future_information_contract.md`、`dataset_v3_schema.md`、`c_producer_interface_contract.md` + 机器可读 registry/schema 落库；FR-461~468 逐项可追溯。
- [ ] **AC-426**: Pilot 管线端到端 PASS——MATLAB 导出 → `build_dataset_v3.py` 窗口化/target/E300/聚合 → split/归一化/审计，在 pilot 数据（S01~S36/M1~M3 + causal fixtures）上机器可读结果 overall PASS（FR-469~480、489 验证）。
- [ ] **AC-427**: 审计负向验证——构造的 L1 泄漏（跨 split voyage/group）、role 违规（FORBIDDEN/H3 入 view）、future-truth 冒充 forecast 等缺陷被正确检出（FR-464/484/500）。
- [ ] **AC-428**: Validation hygiene 闭合——10b 稳态 mask 生效且阈值不变、E10 注入残留修复、E10+40 号 Gate 重跑 PASS（FR-490/491）。
- [ ] **AC-429**: Legacy 标记与 README 两级同步完成；绝对路径扫描零命中（FR-492/493、NFR-422/423）。

### Final Phase 4-D Completion（T-432 解锁后）

- [ ] **AC-430**: C/D Interface Gate overall=true（FR-496 十项）且 T-432 Unblock Checklist（FR-498）全绿，T-432 状态 BLOCKED→READY。
- [ ] **AC-431**: T-432 Stage A qualification PASS + Stage B 预注册 formal fleet 生成完成（FR-497）。
- [ ] **AC-432**: T-435 formal split/leakage/OOD 证据出具（L1 零违规、L2/L3 处置记录、OOD 三轴结果、manifest FR-488 全字段、test_sealed/ood_sealed=true），Dataset-v3 artifacts/manifest 冻结，spec 方可申请 implementation-complete。

## Edge Cases

- 航次 <600 s：合法产生零训练窗口，仅 smoke/audit 用途（FR-472）；
- 窗口锚点越界（t₀ 超出 [t_start+300, t_end−300]）：丢弃，不 padding（FR-472）；
- Forecast 缺失（pilot）：仅允许 PILOT_ONLY causal fixture；formal 缺失 = Gate FAIL（FR-500/496）；
- 单航次导出失败：Stage A 须定义 failed-voyage 处理（剔除+记录，不静默修补）（FR-497）；
- OOD 场景越出 Model E 已验证物理域：标 PHYSICS_EXTRAPOLATION/STRESS_SET，不混入 OOD 指标（FR-486）；
- Battery SOP/欠压等 sub-10 s 瞬态保护：不由 q90 承担（D-D6 §7 语义边界）；
- Test/OOD 污染事件：集合降级 + 新 sealed confirmation set（FR-487）。

## Dependencies and Constraints

- **硬依赖**：Phase 4-C Mission-v3 producer interface（T-432 阻塞源；FR-460/496/498）；
- **事实源**：Model E 冻结基线三件套；Model E 现有 logsout 打点为观测候选池（若 design 发现契约信号缺口须升级用户裁决，不得改模型）；
- **环境**：MATLAB R2026a（坑清单见 HANDOFF §5）；Python `.venv_tf`（项目既定环境）；
- **纪律**：AI_MODELING.md 全文适用（数据纪律/训练红线）；AGENTS.md Git/质量门禁纪律；
- **顺序约束**：FR-459 执行链不得颠倒；pilot 统计与 formal 证据严格分层（FR-489）。

## Requirements 中不得冻结的事项（bounded design/activation obligations）

以下事项本文件只冻结其义务与法律地位，具体值由 design/T-432 activation 冻结：

1. 机器可读 schema 的具体 JSON/YAML/MATLAB struct 文件名（design）；
2. MATLAB export script 最终编号/文件名（design）；
3. 10b steady-state settle/window 具体数值（design）；
4. formal OOD 轴 A 数值 ranges（T-432 activation record，任何 E 包结果出现前）、OOD counts/seeds（T-432 activation record）；
5. formal fleet exact voyage count 与 strata allocation（T-432 activation record）；
6. split_group_id 最终 grouping policy（C 生成机制确定后）；
7. SBD 具体实现细节（本文件只冻结其 audit 法律地位与预注册要求）；
8. formal-voyage 最小航次时长/最小窗口数合格判据（design 或 T-432 activation 预注册）。

## Open Questions

- [x] 无阻塞性开放问题；design-pending 清单见 brainstorm.md「Remaining Doubts」（均为 non-blocking）。

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-04 | requirements 批准（CONDITIONAL PASS + R1~R5 闭合） | 用户 requirements gate 评审 |
| 2026-09-04 | 最小修订（design gate R4 授权）：FR-485 轴 A OOD 数值 ranges 与 counts/seeds 由 design 预注册改为 T-432 activation record 冻结（C route/speed 语义未冻结，无法满足 design 期 read-only 可行性验证前提）；「不得冻结」第 4 项同步。D-D1~D-D9 科学契约不变 | 用户 design gate 评审 R4（Otherwise 路径） |

# Phase 4-A2 — Model E 施工段（T-403~T-412） Brainstorm

Spec: `phase4-a2-model-e-construction`  
Status: brainstorm-approved（2026-09-04 用户批准）  
Created: 2026-09-04

## User Intent

## User Intent

Phase 4 工作包 A 第二段 **A2 = T-403~T-412 Model E 施工段**（路线文档 docs/phase4_research_roadmap.md）：船体阻力重构（T-403）、水深/UKC/浅水效应（T-404）、推进效率口径升级（T-405）、螺旋桨 ±20% 敏感性兜底（T-406）、双发电机拆分（T-407）、启停状态机（T-408）、per-genset BSFC（T-409）、Battery 1RC ECM（T-410）、Battery SOP（T-411）、Model E 整体物理 Gate（T-412）。**本 session 只完成 spec 四门禁，不实施**（用户将换模型执行，spec 必须自包含、任务描述须达到无歧义执行粒度）。

## 裁决记录（Q1~Q5 + C_B 追加，用户逐项批准）

- **Q1 (a) 敏感性最小范围**：A2 只做螺旋桨参数族 ±20% 扫掠（A1 架构审计 a2_consequence 承诺项）+ T-412 标准工况物理 Gate；P_min∈{125/200/250} kW、ramp∈{10/20/50} kW/s、τ∈{2/5/10} s、C_start∈{0.5/1/2} kg 等敏感性档位在参数文件中**冻结登记、不在 A2 扫掠**（扫掠依赖 B 包 PMS v2 才有意义）。
- **Q2 (a) 参数落库 = 集中式**：Model E 工作区新建单一来源 `config/ModelE_Physics_Parameters.m`，逐参数带 provenance 标记（DRAWING/VENDOR/LITERATURE_NOMINAL/LITERATURE_RANGE/DERIVED）与冻结 sensitivity 档位；A1 参数总表 CSV（docs/phase4/a1_model_audit/modelE_parameter_audit.csv）同步回填 user_value/user_reference 作证据链闭环；顾问裁决文档归档 docs/phase4/a2_*/。
- **Q3 排水量 = 500 t nominal**（推翻顾问原案 1000 t）：依据 = 实船工作假定（2026-08-25 用户裁决 docs/data_request_list.md）+ 动力配套自洽性（700 kW/500 t=1.4 kW/t 处公务船带内；1000 t 仅 0.7 kW/t 为货船量级、极速仅 11.5 kn 的根源）+ 用户 2026-08-25 既定意图。**明确放弃与 Phase 1~3 的绝对量连续性**（A1 参数审计 #1 已论证 Δ∝K_R 缩放下节油率等无量纲比值结论方向稳健）。Ship_Mass: 1.0e6 → 5.0e5。
- **Q4 K_R: 1780 → 890 N/(m/s)²**（线性 Δ 缩放 = data_request_list 预估值；拒 Δ^(2/3)≈1121：Fn≈0.33 时纯摩擦相似低估阻力）。新口径：服务航速 ≈13.9 kn（含 15% 裕量）、功率受限极速 ≈14.5 kn。螺旋桨简化模型系数仍在 11 kn 设计点锚定；常规巡航工况仍跑 11 kn。
- **C_B 追加裁决 = 0.60**（推翻顾问原案继承 Rhine86 的 0.86——丰满货船线型与公务船不符）：三腿证据 = Alexander 临界公式（巡航 11 kn→Fn=0.247→临界 0.665 > 0.60 成立）+ Troost 最优回归（巡航最优 0.63 / 满功率 0.53，0.60 居中）+ CCS《海上公务船船体结构指南》2021（Cb=0.6 为公务船规范上界点）。几何重算（保 Rhine86 L/B、B/T 比例，Δ=500 t 守恒，λ=0.6250）：**Lpp=53.45 m / B=7.13 m / T=2.188 m / S_W=554.2 m²**（推导值非文献原值）。H/T 验证水深档：3.0→6.56 / 2.7→5.91 / 2.0→4.38 / 1.6→3.50 / 1.4→3.06 / 1.2→2.63 m。
- **Q5 (a) 电池电气口径**：取消端口级 0.95 效率（避免与 1RC ECM 内阻损耗 double-count）；损耗 = ECM 内阻（显式 I²R+极化）+ η_coul=0.98（仅进 SOC 库仑计量）+ DC/DC η=0.97（保持）。T-410 实施时须先核对现有功率符号约定。

## Fixed Inputs（A1 + 顾问裁决，不经重议）

- 工作区 = Model D 复制分叉 `model/simulink_modelE_(Phase4_Physics)/`（A1 Q-A2），Model D 冻结只读。
- 架构处置：A1 架构审计 14 顶层 subsystem 处置 + 5 项 ADD（ShallowWater_UKC / DG1+DG2 / Genset_StartStop / Battery_1RC_ECM / Battery_SOP）；螺旋桨 keep_simplified（D/K_T/K_Q 为 INTENTIONALLY UNRESOLVED）；EMS 控制律七变体不动（A2 只动 Plant 物理）。
- 顾问参数集（全部 LITERATURE_NOMINAL/LITERATURE_RANGE，绝不标实测）：Zhang 2023 浅水公式（Δk=0.644·(H/T)^−1.72；c1=1.1680/c2=−0.5238/c3=−1.472；w/t LUT(H/T)；η_R=1.0/η_S=0.97）；效率链 η_inv=0.98/η_motor=0.96/η_gear=0.97/η_shaft=0.98（Elec_Eff=0.92 保留、provenance 升级）；DG P_min=125 kW/unit、P_opt=375 kW；t_start=30 s（25 starting+5 sync）；ramp=20 kW/s/unit；τ=5 s；C_start=0.6 kg/start（derived）；BSFC 五点 600/547/447/427/453 g/kWh（PCHIP 插值，Fridell 2021 形状 × WH17ME 427 锚定）；LFP surrogate 180s×715.38p，R_0/R_1/C_1/OCV 全 SOC-LUT（Tran 2021 HPPC + Pérez Vidal 2011）；电池绝对电压边界 360~657 V；SOP 以 DC/DC 1050 kW 图纸约束优先；P_hotel=100 kW。
- 引用纪律：推导值（缩尺几何、pack 级 ECM 参数、1823 A、0.6 kg/start 等）≠ 文献原值，论文中须写"根据文献[×]按公式 ×× 推导"。
- 证据链归档：全部 17 项参考文献（GB/T 7714 + 链接）+ C_B 佐证已记入 项目开发进展.md 2026-09-04 条目。

## Proposed Approach

按 spec 四门禁推进：requirements（FR 映射 T-403~T-412 + 参数 provenance/标注要求 + Gate 验收）→ design（定稿开放点：w/t LUT 在 C_B=0.60 下的处置、BSFC PCHIP 节点与实现、ModelE_Physics_Parameters.m 结构、T-412 Gate 机器可读判据与阈值、Model D 继承的 smoke/scenario 流水线适配）→ tasks（施工顺序：工作区创建→参数文件→T-403/404→T-405→T-407/408/409→T-410/411→T-406 敏感性→T-412 Gate→收尾登记）。

## Assumptions

- Model E 全程 500 t 口径；1000 t 仅存于 Phase 1~3 历史与 A1 审计记录。
- 分支：从 `Phase4` 开 `feat/phase4-a2-model-e`（实施 session 执行）。
- 执行分工（主 Agent/subagent）由执行 session 按 AGENTS.md §3 决定，本 spec 不预设。
- T-412 Gate 未 PASS 前不允许生成任何新数据集（路线文档铁律）。

## Remaining Doubts

- w 取值张力（Harvald 双桨估计 0.10 vs Zhang LUT 0.22~0.32）——design 阶段裁决，不阻塞 requirements。
- T-412 Gate 具体阈值（energy closure 容差、speed-power 单调性判据等）——design 阶段定稿。
- 500 t 实船确认仍挂账（data_request_list），真实资料到位后走 spec 修订。

## Decision（本轮）

- [x] 待用户批准进入 requirements 起草。（2026-09-04 用户批准）

## Clarifying Questions Asked

- Q1：A2 敏感性验证范围（最小/中等/全量）→ 用户选最小：仅螺旋桨 ±20% + T-412 Gate，其余冻结登记。
- Q2：参数集落库形态（集中式/就地/CSV 驱动）→ 用户选集中式 + A1 CSV 回填 + 裁决文档归档。
- Q3：船体缩尺档位（500 t / 1000 t / 双档）→ 用户追问"先前 Phase 3 是 500 t 还是 1000 t""为什么缩尺""动力配套是否适合 1000 t"，主 Agent 查证分析（Phase 3 模型实跑 1000 t、实船假定 500 t、0.7 vs 1.4 kW/t）后用户裁决 500 t nominal + K_R 重缩放。
- Q4：K_R 缩放律（890 线性 / 1121 Δ^(2/3)）→ 用户选 890。
- C_B 追加：用户要求取典型公务船 ≈0.6 且须文献佐证 → 三腿证据收敛后裁决 0.60，几何重算 Lpp=53.45/B=7.13/T=2.188/S_W=554.2。
- Q5：电池电气口径（顾问方案 ecm-only / 双口径对照）→ 用户选顾问方案（取消端口 0.95）。

## Decision

- [x] 用户批准创建 spec 骨架并起草 requirements（2026-09-04）。

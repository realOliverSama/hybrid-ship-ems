# Phase 4-A2 — Model E 施工段（T-403~T-412）Requirements

Spec: `phase4-a2-model-e-construction`  
Status: requirements-approved（2026-09-04 用户批准，GPT 评审 7 项修订中采纳 5 项+2 项部分，详见 FR-412/415/416/420/421/422/423/424/426 与 OQ-4~9）  
Created: 2026-09-04  
Brainstorm: `./brainstorm.md`

## Overview

Phase 4（系统优化重构，`Phase4` 分支）工作包 A「Model E 船舶物理模型升级」的第二段（**施工段**）。
本 spec 覆盖路线文档 `docs/phase4_research_roadmap.md` §2 中的 **T-403~T-412**，在 A1 审计结论
（`docs/phase4/a1_model_audit/`）与用户裁决的 **Model E v1 Literature-Surrogate 参数集**
（证据链见 `项目开发进展.md` 2026-09-04 条目）基础上，把 Model E 的物理保真度升级到能支撑
"environment → physics → load"因果链，并以 T-412 整体物理 Gate 作为唯一放行判据。

与 A1 的零施工纪律相反，本 spec 是**施工 spec**：创建 Model E 工作区并实施全部物理升级。
关键边界：

- 只动 **Plant 物理**；EMS 控制律七变体（law=0/1/2/4/5/6/7）**保持不修改即可运行**（A1 KEEP 决策）；
- 全部新参数为 **Literature-Surrogate**（LITERATURE_NOMINAL / LITERATURE_RANGE / DERIVED 标记），
  绝不标成目标船实测值；
- 敏感性工作**最小范围**（Q1 裁决）：只做螺旋桨参数族 ±20% 扫掠；其余敏感性档位
  （P_min / ramp / τ / C_start）在参数文件中冻结登记、不在本 spec 扫掠；
- 本 spec 由后续 session（可能不同模型）执行，**任务描述须自包含**。

## Scope

### In Scope

- Model E 工作区创建：`model/simulink_modelE_(Phase4_Physics)/`（Model D 整个复制分叉，
  A1 Q-A2 裁决），从 `Phase4` 开 `feat/phase4-a2-model-e` 分支；
- 集中式参数文件 `config/ModelE_Physics_Parameters.m`（provenance 逐参数标记 +
  冻结 sensitivity 档位）+ A1 参数总表 CSV 回填（user_value/user_reference）；
- T-403+T-404：船体阻力重构（R_total = R_deep + R_shallow，预留 bank/wind 接口）+
  水深/UKC/浅水效应（500 t 几何：Lpp=53.45 / B=7.13 / T=2.188 / S_W=554.2 / C_B=0.60；
  K_R=890；Zhang 2023 浅水公式与 w/t LUT；Scenario 输出"水深=X m"）；
- T-405：推进/逆变效率链口径升级（Elec_Eff=0.92 保留常数、provenance 升级为
  literature_nominal；η_inv/η_motor/η_gear/η_shaft 分量登记）；
- T-406：螺旋桨 keep_simplified + 参数族 ±20% 敏感性验证（K_Fw/K_Fv/K_Qw/K_Qv/wake_fraction）；
- T-407+T-408+T-409：双发电机拆分（DG1/DG2 各 500 kW 独立 Plant）+ 启停状态机
  （OFF→STARTING→SYNCHRONIZING→ON→COOLDOWN→OFF，参数化）+ per-genset BSFC
  （五点 PCHIP：600/547/447/427/453 g/kWh @ 25/50/75/80/100%）；
- T-410+T-411：Battery LFP 1RC ECM（R_0/R_1/C_1/OCV 全 SOC-LUT，η_coul=0.98，
  取消端口级 0.95）+ SOP（SOC/电压/电流三约束，DC/DC 1050 kW 图纸约束优先，
  rule-based 与先进 EMS 共用同一 SOP）；
- T-412：Model E 整体物理 Gate（标准测试矩阵 + 机器可读判据 + `gate_result.json`）；
- 归档：参数证据链文档（`docs/phase4/a2_*/`）、§20 README 登记、开发日志收尾。

### Out of Scope

- Model D 工作区任何修改（冻结只读，A1 纪律延续）；
- EMS 控制律修改、PMS v2 / Safety 层（B 包 T-413~T-418）、真实驾驶行为抽象（B 包）；
- 工况生成器 v3 / 数据集 v3 / 预测模型 / 预测式 EMS（C/D/E/F 包）；
- 敏感性扫掠实验（除螺旋桨 ±20% 外）——档位冻结登记，扫掠推迟到 B/F 包；
- 螺旋桨 D、K_T/K_Q、P/D 图谱获取与 LUT 升级（INTENTIONALLY UNRESOLVED，
  资料到位后走 spec 修订）；
- 岸壁（bank）效应、风阻（预留接口，不实现）；
- N6 / HIL / NPU 相关任何工作（Phase 4 边界）；Model E 保持 HIL-ready 属性
  （fixed-step、无代数环、确定性）但不做实时化；
- 电机/逆变器 2D 效率 map（Q 裁决：无可信数据，保持常数）。

## User Stories

- 作为研究者，我需要 Model E 的每个新参数都有显式 provenance 标记和文献出处，
  以便论文中能说清每个值的证据等级，且真实资料到位后能逐参数替换。
- 作为研究者，我需要"浅水"由环境输入（水深）驱动而非阻力倍率，
  以便 environment → physics → load 因果链在论文中成立。
- 作为研究者，我需要双发电机独立建模（各自功率/爬坡/启停/燃油），
  以便"一台 500 kW vs 两台各 250 kW"的经济性可被区分研究。
- 作为研究者，我需要电池 1RC ECM + 真实 SOP 约束，
  以便 SOC/电压/电流安全边界对 rule-based 与先进 EMS 一致且物理可信。
- 作为执行者（可能是另一个模型/session），我需要 spec 自包含、
  参数值与出处全部就位、Gate 判据机器可读，以便无需重新决策即可施工。

## Functional Requirements

### 工作区与参数基线

- **FR-410**: THE spec implementation SHALL 创建 Model E 工作区
  `model/simulink_modelE_(Phase4_Physics)/`（Model D 整个复制分叉），
  从 `Phase4` 分支开 `feat/phase4-a2-model-e`；Model D 目录保持零改动
  （以 `git status` + 关键文件 sha256 前后对照证明）。
- **FR-411**: THE implementation SHALL 新建 `config/ModelE_Physics_Parameters.m` 作为
  **Model E 完整独立参数事实源**（用户裁决：参数即使数值继承，也必须独立成套，
  不得运行时依赖 legacy 参数文件）——Model E 所需的**全部**参数在该文件中声明，
  含继承项（provenance 标 INHERITED_DRAWING / INHERITED_LEGACY 等并注明证据链出处）
  与新增项；逐参数带 provenance 注释（DRAWING / VENDOR / LITERATURE_NOMINAL /
  LITERATURE_RANGE / DERIVED / ASSUMED / INHERITED_*）与文献编号（对应开发日志
  2026-09-04 条目 [1]~[17]）；含冻结 sensitivity 档位注释块（P_min {125/200/250} kW、
  ramp {10/20/50} kW/s、τ {2/5/10} s、C_start {0.5/1/2} kg）。
  复制分叉带入的 legacy config 文件（Model_Parameters.m / Model_Scenarios.m /
  Model_GateThresholds.m / Ecms_Phase0_Config.m 等）SHALL 隔离至 `config/_legacy_reference/`
  **不入路径、不参与运行**（仅作证据对照；EMS 变体内部所需参数并入 Model E 参数文件，
  标 INHERITED_LEGACY + 待 B/F 包重评注记）；
  A1 参数总表 CSV SHALL 回填 user_value/user_reference 列（不改已批准的审计结论）。

### 船体与浅水（T-403 / T-404）

- **FR-412**: THE hull model SHALL 采用 500 t 口径参数：Ship_Mass=5.0e5 kg；
  几何 Lpp=53.45 m / B=7.13 m / T=2.188 m / S_W=554.2 m² / C_B=0.60
  （Rhine86 比例缩尺推导值 [2]，标记 DERIVED+LITERATURE_NOMINAL）；
  深水阻力系数 Hull_K_R=890 N/(m/s)²（线性 Δ 缩放 [data_request_list]，用户裁决 nominal）；
  ρ_water=1000 kg/m³。
  上述几何为**按 500 t 排水量约束构造的 representative hull**，provenance 必须标注
  `target_vessel_confirmed = false`，任何文档/论文不得将其表述为目标船实船几何；
  500 t 本身仍是未确认工作假定（docs/data_request_list.md 挂账，真实资料到位后走 spec 修订）。
- **FR-413**: THE resistance model SHALL 重构为 R_total = R_deep + R_shallow
  （预留 bank/wind 接口但不实现）；R_deep 保持 K_R·V_rel·|V_rel| 锚定形式；
  R_shallow 采用 Zhang 2023 [1] 体系：形状因子修正 Δk=0.644·(H/T)^(−1.72)（Millward [4]）
  + 摩擦修正系数 c1=1.1680 / c2=−0.5238 / c3=−1.472（Rhine86 标定族 [3]），
  并登记已知局限（标定族 C_B=0.86 与本船 0.60 的型差外推）。
- **FR-414**: THE scenario interface SHALL 新增水深环境输入（water_depth h），
  Plant 自算 UKC=h−T 与 Fr_h=V/√(gh)；浅水附加阻力由 Plant 物理计算，
  旧"阻力 ×1.15"浅滩语义在 Model E 中退役（C 包工况生成器另行重做）；
  wake fraction 与 thrust deduction SHALL 支持随 H/T 变化（Zhang 2023 LUT），
  具体实现形态（含 w 取值张力的裁决）在 design 阶段定稿。
- **FR-415**: H/T 标准验证档 SHALL 可配置，nominal 档集 = {3.0, 2.7, 2.0, 1.6, 1.4, 1.2}
  （对应水深 {6.56, 5.91, 4.38, 3.50, 3.06, 2.63} m）。浅水经验模型 SHALL 只在
  验证域内使用，**禁止静默外推**：H/T>3 按深水渐近处理（浅水修正趋零）；
  1.2≤H/T≤3 为正常有效域；H/T<1.2 须置 MODEL_DOMAIN_WARNING 并按 design 裁决
  （clamp 到 1.2 / 停止该 case / 标记 invalid simulation）处理；
  **禁止在公式发散后用 Saturation 等块把结果"修正常"**。

### 推进效率与螺旋桨（T-405 / T-406）

- **FR-416**: THE propulsion efficiency SHALL 保持常数 Elec_Eff=0.92，
  provenance 从 estimated 升级为 LITERATURE_NOMINAL；不实现 2D 效率 map。
  效率链物理边界 SHALL 显式定义：**Elec_Eff = η_inv(0.98) × η_motor(0.96) × η_shaft(0.98) ≈ 0.922，
  不含齿轮箱**（η_gear=0.97 独立存在于转矩链 [5]）；实施 SHALL 附效率链审计
  （efficiency-chain audit），证明推进能量路径上每个效率因子恰好出现一次、无重复计损。
- **FR-417**: THE propeller model SHALL 维持简化二次模型
  （F=K_Fw·ω|ω|−K_Fv·V_A|V_A|，Q 同理；A1 keep_simplified 决策），
  但四个系数 SHALL 在 Model E 语境下**重新标定**（11 kn 设计点 + **w_deep=0.10**——
  w 变更后旧 K 不再满足设计点推力/扭矩平衡，禁止沿用 Model D 推导值）；
  并 SHALL 实施参数族 ±20% **OAT** 敏感性验证（K_Fw/K_Fv/K_Qw/K_Qv/w_deep 五参数，
  一次一参数 × {0.8,1.0,1.2}，共 15 cases，非 full factorial），
  产出机器可读敏感性报告；螺旋桨 D、K_T/K_Q、P/D
  保持 INTENTIONALLY UNRESOLVED，不得填入借用值。

### 双发电机（T-407 / T-408 / T-409）

- **FR-418**: THE genset plant SHALL 拆分为 DG1/DG2 两个独立 500 kW Plant
  （WH17ME 6WH17C680ME5，VENDOR：P_rated=500 kW、n_rated=1500 rpm、n_idle=600 rpm [16][17]），
  各自独立 power/ramp/inertia/state/fuel；P_min=125 kW/unit（25%，[6]）、
  P_opt=375 kW/unit、ramp=20 kW/s/unit、一阶 τ=5 s（engineering-calibrated 标记）。
- **FR-419**: THE genset SHALL 具有启停状态机
  OFF→STARTING(≈25 s)→SYNCHRONIZING(≈5 s)→ON→COOLDOWN(≈30 s)→OFF（[7][8]，合计启动约 30 s），
  参数化：start delay、sync time、min power、rated、ramp、start fuel penalty
  C_start=0.6 kg/start（DERIVED surrogate：125 kW×0.6 kg/kWh×30/3600）。
  **min_up/min_down time 不在本状态机内**（属 unit commitment 约束，B 包 T-415/T-416 职责；
  2026-09-04 GPT 评审 A2 修订，用户批准）。
- **FR-420**: THE fuel model SHALL 改为 fuel = fuel_1(P_1) + fuel_2(P_2)，
  per-genset BSFC 五点 PCHIP 插值：600/547/447/427/453 g/kWh @ 25/50/75/80/100% 负荷
  （[11] 实测形状 × [16] WH17ME 427 锚定归一，100% 点 453 沿用项目既有 rated anchor；
  全部标记 LITERATURE_NOMINAL/DERIVED，不得标实测）。
  BSFC LUT **只在机组 ON 且 P≥P_min 时有效，禁止向 0 kW 外推**；严格区分：
  OFF → fuel=0；STARTING/SYNCHRONIZING/COOLDOWN → 状态专属燃油模型（startup surrogate，
  C_start 计入）；ON 且 P≥P_min → 才使用 BSFC map。禁止出现"ON @ 0 kW 且零油耗"的
  物理漏洞。
- **FR-421**: A2 SHALL 提供最小占位机组组合逻辑（legacy adapter：
  P_total_ref≤500→仅 DG1；>500→DG1=500、DG2=余量；scenario 可脚本化强制组合），
  显式标记 PLACEHOLDER——**PMS v2 属 B 包，本 spec 不做控制策略**。
  **Genset Plant 本体接口为 DG1_enable/P_DG1_ref/DG2_enable/P_DG2_ref**，
  PLACEHOLDER 是 Plant 之外的可拔插 adapter（B 包 PMS v2 直接替换 adapter，
  不动被控对象）；adapter 不得在机组 STARTING/SYNC 期间虚构其输出功率
  （缺口由电池/母线自然响应）。**PLACEHOLDER 组合逻辑不得用于任何正式
  EMS 经济性/节油结论**（只允许 plumbing、变体兼容、组件测试、T-412 Gate）。

### 电池（T-410 / T-411）

- **FR-422**: THE battery model SHALL 升级为 LFP 1RC ECM：
  V_t = OCV(SOC) − I·R_0(SOC) − V_1，V̇_1 = −V_1/(R_1·C_1) + I/C_1；
  R_0/R_1/C_1/OCV 全部 SOC-LUT（K2 LFP26650P HPPC [12] × 180s×715.38p 缩放 + [13] OCV，
  pack 级值为 DERIVED）；新增端电压/电流/极化状态输出；
  绝对电压边界 360~657 V（cell 2.00~3.65 V × 180s）。
  **LFP 为 literature surrogate chemistry**：项目资料未确认实船电池化学体系，
  任何文档/论文不得表述为"目标船采用 LFP"；真实 chemistry 到位后整套 ECM 参数替换。
- **FR-423**: 电池 SOC 更新 SHALL 采用**电流（电荷）积分**：
  SOC_{k+1} = SOC_k − η_I·I_k·Δt/Q_nom（充/放电分方向定义 coulombic efficiency，
  η_coul=0.98 [14]，Q_nom=1860 Ah），**禁止继续使用端口能量积分**（∫P dt/E_nom）
  与 ECM 并行的混合口径。损耗口径 SHALL 为：ECM 内阻显式损耗（I²R_0+极化）
  + η_coul（仅进 SOC 计量）+ DC/DC η=0.97；**旧 η_bat=0.95 不再作为额外 battery
  power efficiency 乘到端口功率上**（避免 double-count，Q5 裁决）；
  实施前 SHALL 核对现有功率符号约定并在 design 中写明。
- **FR-424**: THE battery SOP SHALL 计算 P_dis,max(t)/P_chg,max(t)，
  形式为**四约束取最小**：P_SOP = min(P_current, P_voltage, P_SOC, P_converter)，
  其中 P_converter=1050 kW 为 DC/DC 图纸约束，是**额外上界**而非凌驾于电压/电流
  安全边界之上；1050 kW 的 bus-side 与 battery-terminal-side 口径差异在 design 冻结（OQ-9）。
  电流约束项命名 `I_bat_limit_surrogate_A`（≈1823 A，由 1050 kW/576 V 推导，
  **DERIVED surrogate，非独立 BMS 电流额定**，不得表述为实测电流上限）。
  SOP 替代固定 1050 kW 上限；rule-based 与全部 EMS 变体 SHALL 共用同一 SOP 模块。

### 整体物理 Gate（T-412）

- **FR-425**: THE spec implementation SHALL 提供 T-412 整体物理 Gate：
  标准测试矩阵（静水/顺水/逆水/深水/浅水(H/T 档)/单机/双机/电池充放电/全推进 ramp），
  机器可读判据（energy closure、数值稳定性、speed-power 单调性与方向性检查
  （V↑⇒R↑；h↓⇒R↑；U_against↑⇒P_prop↑）、generator fuel 一致性、SOC/V_bat/Vdc 边界、
  fixed-step runtime），输出 `gate_result.json`（`overall: true/false`），
  FAIL 时非零退出码终止；**Gate FAIL 不允许提交**（AGENTS.md §12 纪律延续）。
- **FR-426**: EMS 接口兼容性分两级验收：**全部七变体（law=0/1/2/4/5/6/7）必须
  编译/构建成功**（clone 工作区中的既有控制器不随 Plant 重构而接线断裂）；
  law=0/law=5 降为**兼容 smoke**（compile + 短程运行无错误即可，**不作为
  T-412 物理 Gate 判据**——Plant 科学性不依赖历史控制器行为，旧控制器不理解
  P_min/双机组属预期）；law=6/7 仅 compile。T-412 物理 Gate SHALL 由**专用确定性
  Plant 测试台**（直接注入推进/机组/母线指令，不经 EMS 控制器）执行。

### 归档与登记

- **FR-427**: THE implementation SHALL 归档参数证据链文档于 `docs/phase4/a2_model_e/`
  （参数总表：值/provenance/文献编号/推导公式；与开发日志 2026-09-04 条目的
  17 项参考文献对应），并按 AGENTS.md §20 同步登记全部受影响目录的 README
  （model/、model 工作区、docs/phase4/ 等）；开发日志按 §18 收尾登记。

## Non-Functional Requirements

- **NFR-405**: 全部施工脚本驱动、可复现；禁止 GUI 手工修改形成不可复现状态；
  `.slx` 修改由 builder 脚本生成，修改前后做结构差异检查。
- **NFR-406**: 脚本禁止硬编码绝对路径（AGENTS.md §20.2），用 `mfilename`/`__file__`
  推导并加 pwd 兜底；验收时执行绝对路径扫描。
- **NFR-407**: Model D 工作区零改动（FR-410 的证据义务延续至 spec 收官）。
- **NFR-408**: "不改假参数凑指标"红线延续：Gate FAIL 时不允许通过改参数/放宽阈值
  凑 PASS；发现设计层问题须 escalate 而非绕行。
- **NFR-409**: Model E SHALL 保持 HIL-ready 属性：fixed-step 可解、无代数环、
  确定性行为（新 ECM/状态机不得引入代数环；若引入须以 design 记录的离散化/
  松弛方案消解）；本 Phase 不做实时化。
- **NFR-410**: 参数诚实标记：所有 Literature-Surrogate 参数在参数文件、证据链文档、
  论文素材中一律带 provenance；推导值附推导公式。
- **NFR-411**: 数值稳健性：SOC→0/1、H/T→低界、零速等边界点不得产生除零/NaN/
  非物理值（插值域外 clamp + 报警，不静默外推）。

## Acceptance Criteria

- [ ] AC-408（FR-410/411）：工作区与分支就位；参数文件含全部 T-403~T-411 所需参数、
  provenance 无空缺；A1 CSV 回填完成；Model D 零改动证据（git status + sha256）。
- [ ] AC-409（FR-412~415）：静水工况 V↑⇒R↑；固定 V 下 h↓⇒R↑；逆水工况
  U_against↑⇒P_prop↑；H/T 档集可配置且域外报警。
- [ ] AC-410（FR-416/417）：Elec_Eff provenance 升级落盘且物理边界
  （=η_inv×η_motor×η_shaft，不含齿轮箱）写明；效率链审计证明无重复计损；
  螺旋桨 ±20% 敏感性报告产出，结论稳健域明确写入。
- [ ] AC-411（FR-418~421）：双机组独立运行/启停/爬坡行为正确；单机 500 kW 与
  双机各 250 kW 的燃油差异可区分；启停一次产生 C_start 燃油代价；PLACEHOLDER 标记就位
  且未用于任何经济性结论。
- [ ] AC-412（FR-422~424）：ECM 端电压随电流方向正确偏移；SOC 按电流积分更新且
  与端口能量自洽；SOP 以 min(current, voltage, SOC, converter) 四约束形式在边界正确收缩；
  rule-based 与 ECMS 变体共用同一 SOP；损耗口径无双算（Q5）。
- [ ] AC-413（FR-425/426）：T-412 Gate `overall: true`（专用 Plant 测试台判据）；
  七变体 compile 全过；law=0/law=5 兼容 smoke 通过（不作物理判据）；
  Gate FAIL 路径经人为构造缺陷验证（能正确报错）。
- [ ] AC-414（FR-427）：证据链文档与 README 登记齐全；绝对路径扫描零命中；
  开发日志收尾。
- [ ] AC-415（用户评审门）：T-412 Gate 结果与证据链文档经用户书面批准
  （本文件 Status 升级即视为批准记录）；Gate PASS 前不允许生成任何新数据集。

## Edge Cases

- SOC 逼近 0/1 或 H/T 低于 1.2：插值域外 clamp + 报警（NFR-411），不得 NaN；
- 电池 1RC ECM 引入的潜在代数环（V_t 依赖 I、I 依赖负载）：按 NFR-409 以
  design 记录的方案消解（如一步延迟/寄生电容），禁止静默引入；
- 双机全停 + 电池不足：现有 08_Undervolt_Derate 保护链须在 Model E 中仍生效
  （T-412 测试矩阵含该工况）；
- 启停状态机与 ramp/一阶惯性的衔接：STARTING/SYNCHRONIZING 期间机组输出功率为零，
  COOLDOWN 期间不可再启动（min down time）；
- K_R 变更后与螺旋桨设计点锚定（11 kn）的推力平衡一致性：T-412 Gate 含
  推力平衡检查（新平衡点速度 ≈13.9 kn 附近，偏差须在 design 定稿的容差内）；
- Model D 与 Model E 绝对结果不可比是**设计意图**（Q3/Q4 裁决），
  Gate 判据不得包含"与 Model D 绝对值一致"类检查，只允许方向性/守恒性对照；
- 500 t 实船确认仍挂账：真实资料到位导致参数翻转时走 spec 修订，不溯及本 spec 已批准结论。

## Dependencies and Constraints

- 前置（已批准 Fixed Decisions）：A1 双审计报告 + `gate_result_a1.json`
  （`.specs/phase4-a1-model-e-audit/`，implementation-complete，2026-09-03 用户批准）；
- 参数依据：顾问 Model E v1 Literature-Surrogate 参数集 + 本 session Q1~Q5/C_B 裁决
  （brainstorm.md；证据链 = `项目开发进展.md` 2026-09-04 条目，17 项参考文献含链接）；
- 输入（只读）：`model/simulink_modelD_(EMS_NN_AECMS)/` 全部内容、
  `docs/phase4_research_roadmap.md`、`docs/data_request_list.md`、
  `docs/bsfc_anchors_sources.md`、`docs/optimization_backlog.md`；
- 执行 session 开工前必须研读：brainstorm.md、本文件、A1 两份审计报告、
  开发日志 2026-09-04 条目；
- 纪律：AGENTS.md 全部适用；Gate Threshold 修改与 Golden Baseline 重建属 🟡 操作；
- 后继：T-412 PASS + 用户批准后，B 包（T-413~T-418）方可立项；
  T-412 PASS 前禁止任何新数据生成（路线文档铁律）。

## Open Questions

- [ ] OQ-4：w/t LUT 在 C_B=0.60 船型下的具体实现形态（直接用 Zhang LUT /
  按 Harvald 深水位 w≈0.10 重新锚定 / 混合），design 阶段裁决；
  GPT 倾向：1-D LUT vs H/T，插值用 **linear**（防 overshoot，物理可控）。
- [ ] OQ-5：T-412 Gate 各项判据的具体阈值（energy closure 容差、推力平衡偏差容差、
  speed-power 单调性判据形式、runtime 上限），design 阶段定稿；
  GPT 倾向：energy closure 0.1% 量级、单调性零反例、runtime 先 informational；
  Gate 至少含六类——G1 结构（fixed-step/无代数环/无 NaN/Model D 零改动）、
  G2 船体浅水方向性、G3 机组状态机与 P_min 域、G4 电池方向性与 SOP 四边界、
  G5 energy closure、G6 运行时架构（确定性/零 solver 失败/profiling）。
- [ ] OQ-6：启停状态机命名与 Model D 既有语义的映射（WARMUP 是否并入
  SYNCHRONIZING 段；COOLDOWN 时长取值），design 阶段定稿；
  GPT 倾向：OFF→STARTING→SYNC→ON→COOLDOWN 足够，无独立物理参数前不加 WARMUP。
- [ ] OQ-7：T-412 是适配 Model D 继承的 03/06 流水线脚本，还是新建独立 Gate 脚本，
  design 阶段裁决；GPT 倾向：一个总 orchestrator + 每模块独立 JSON result，
  最终 aggregate `gate_result.json`。
- [ ] OQ-8：参数加载/覆盖顺序与命名冲突消解，design 阶段定稿；
  GPT 倾向三层：`Model_Parameters.m`（legacy 只读）→ `ModelE_Physics_Parameters.m`
  （nominal override）→ sensitivity profile override；effective configuration 导出
  manifest/hash；禁止反向修改 Model D 文件。
- [ ] OQ-9：电池两个 design 冻结项：① SOP 的 1050 kW 是 bus-side 还是
  battery-terminal-side（η_dcdc=0.97 使两者不同）；② 1RC 电流求解采用解析二次式
  R_0·I² − (OCV−V_1)·I + P = 0 直接求 I（无迭代、HIL-friendly），避免代数环——
  GPT 推荐，design 定稿。

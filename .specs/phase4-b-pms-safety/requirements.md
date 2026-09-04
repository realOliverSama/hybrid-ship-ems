# Phase 4-B — 真实控制逻辑 / PMS v2 / Safety-Interlock（T-413~T-418）Requirements

Spec: `phase4-b-pms-safety`  
Status: requirements-draft  
Created: 2026-09-04  
Brainstorm: `./brainstorm.md`（九轮裁决 D-B1~D-B9 + 终审 5 项修正，用户书面批准）

## Overview

Phase 4（系统优化重构，`Phase4` 长期主分支）工作包 B「真实控制逻辑 / PMS / 驾驶行为」
（路线文档 `docs/phase4_research_roadmap.md` §3，**T-413~T-418**）的施工 spec。
在 A2 已 Gate 收官的 Model E 物理基座（T-412 `overall=true`，基线 v1.0 冻结）之上，
重建**控制/安全侧**：驾驶输入抽象与双模式推进控制（T-413/414）、模块化 PMS v2
（T-415/416，替换 `Legacy_Commitment_Adapter` PLACEHOLDER）、独立 Safety/Interlock 层
（T-417，含 backlog B2 失败模式门控）、三臂 baseline 验证与 43 号控制 Gate（T-418）。

关键边界：

- 只动**控制/安全侧**；Model E **Plant 物理零改动**，以 T-412 重跑 `overall=true` 背书，
  **禁止改参数凑 PASS**；
- `Legacy_Commitment_Adapter` 被 `07_PMS_v2` 直接替换（A2 FR-421 契约：Plant 之外的可拔插
  adapter，替换不动被控对象）；Genset Plant 接口 `DG1_enable/P_DG1_ref/DG2_enable/P_DG2_ref`
  不变；SOP 信号 `P_bat_dis/chg_avail` 同名同义消费（A2 FR-424 契约）；
- EMS 七变体内部**不修改**（A2 FR-426 契约延续：全部编译成功，law=0/5 短程运行）；
- PMS v2 双重角色（D-B1）：完整工业 baseline 链 + F 包预测式 EMS 的可复用 commitment 层
  （T-448「Stateflow 决定 commitment，MPC/ECMS 决定 continuous power」契约）；
- brainstorm 全部裁决（D-B1~D-B9 + 终审 5 项）为本 spec **Fixed Decisions**，requirements
  只做可验证化转写，不重新讨论参数。

## Scope

### In Scope

- T-413/T-414：`01_Operator_Interface`（改组）+ `02_Propulsion_Controller`（改组），
  AUTO（现有速度 PI 原样）/MANUAL（归一化功率指令）双模式，工况内模式固定；
- T-415/T-416：`07_PMS_v2` 顶层子系统——`Baseline_Demand_Policy` + source selector +
  `DG_Commitment_Supervisor`（Stateflow）+ `DG_Dispatch_Allocator`，替换 PLACEHOLDER adapter；
- T-417：`16_Safety_Interlock` 顶层块——执行性 override S1~S5 + 纯监视 flag +
  EMS 健康回退机制（B2 门控）；既有保护链（08/15/09/机组与电池 flag）纳入统一组织；
- T-418：三臂对比（R0/R5/P2）+ `scripts/43_run_modelE_control_gate.m` 控制 Gate +
  T-412 重跑 + 负向验证（42 号模式延续）；
- 控制参数独立配置文件（provenance 逐参数标记，敏感性档位冻结登记不扫）；
- MANUAL 最小工况 M1/M2/M3 与 Safety 确定性注入场景（S1~S5 各一）；
- 归档：§20 两级 README 同步、§18 开发日志、A2 spec design.md 补记 D-14 偏差条目。

### Out of Scope

- Model E 设备物理任何改动（船体/螺旋桨/机组/电池/母线参数与动态）；
- 0DG 电池-only 运行模式（v1 `voyage active ⇒ N_DG_on ≥ 1`；C 包靠泊/港内工况再评）；
- runtime AUTO⇄MANUAL 切换动态（bumpless transfer/积分器管理/切换防抖）；
- load shedding、断路器联锁、blackout recovery、重合闸/再同步逻辑、电机热限；
- lead/lag 轮换、BSFC-aware dispatch、不对称加载（留作先进 EMS 研究空间）；
- P_opt 吸附、死区、比例 SOC 调节、主动电池 load shifting（D-B2 拒绝项）；
- EMS 七变体内部控制律修改；预测器本体（E 包）；MPC/Oracle EMS（F 包）；
- 数据集生成（D 包）；N6/HIL/实时化（后续 Phase）；Manual rate limiter 的精细整定。

## User Stories

- 作为论文研究者，我需要一个**可信的 conventional industrial PMS baseline**
  （load-following + SOC recovery + threshold/timer commitment + 均分 load sharing），
  使先进 EMS 的对比结论不建立在 strawman 上；
- 作为先进 EMS（ECMS/未来 MPC）研究的接入方，我需要**共享 commitment 层**：
  经济层只给聚合 P_gen_ref，启停组合由 PMS 统一拥有，避免每个控制器复制 unit commitment；
- 作为安全验证责任人，我需要 **Safety/Interlock 层独立于经济控制**且不可被先进
  AI/MPC 绕过，全部 override 可追溯、可断言测试；
- 作为工况生成（C 包）的下游，我需要 **AUTO/MANUAL 双模式推进控制**与
  "航行决策 vs 推进执行"的层分离，Mission 只表达驾驶意图；
- 作为工程维护者，我需要控制规则**简单、参数有 provenance、transition 有 reason 审计**，
  而不是叠 timer 的复杂 PLC。

## Functional Requirements

### 驾驶输入与推进控制（T-413 / T-414）

- **FR-428**: `01_Scenario_Manager` SHALL 改组为 `01_Operator_Interface`：在既有航次/环境
  输出基础上新增驾驶意图输出 `{ctrl_mode, v_ref_mps, manual_power_cmd_pct}`；
  `ctrl_mode` 由编译期变量 `VOYAGE_CTRL_MODE`（AUTO/MANUAL）确定，**单个仿真工况内
  模式固定**；水深/VOYAGE_* 等既有注入通道语义不变。
- **FR-429**: `02_Ship_Speed_Controller` SHALL 改组为 `02_Propulsion_Controller`：
  AUTO 路径保留现有 PI speed-hold（v_ref → PI 抗饱和 → P_prop_demand，参数不变）；
  MANUAL 路径 `P_prop_ref = manual_power_cmd_pct · P_prop_max`（归一化**线性**映射，
  `manual_power_cmd_pct ∈ [0,1]`）+ command-side rate limiter（独立参数，
  **不得与 DG 20 kW/s 物理 ramp 共用**）；两路径汇聚为唯一输出 `P_prop_demand_kW`；
  **禁止**新建 RPM 控制环或 torque 域接口；`03_Propulsion_Plant` 接口与内部不变。
- **FR-430**: T-413 选型证据 SHALL 以 design.md 正式小节落档：七行决策表
  （lever→RPM 否 / lever→torque 否 / lever→power 是 / AUTO speed-hold 是 /
  runtime 切换否 / 线性映射是 / 立方映射否）+ assumptions/limitations/interface
  contract/future extension；接口命名 `manual_power_cmd_pct`；一切文档/论文表述须注明
  "normalized operator propulsion command surrogate; not a measured bridge-lever
  transfer function"；不单独产出文献综述文档。
- **FR-431**: SHALL 新增 MANUAL 最小工况三个：M1（step，30%→70%）、M2（ramp，20%→80%）、
  M3（staged maneuver，30%→70%→40%，验证 1DG→2DG→1DG 链）；MANUAL 不复制 AUTO 工况
  矩阵；工况注入沿用 `VOYAGE_*` 编译期变量 + assignin 通道。

### Baseline Demand Policy（T-415）

- **FR-432**: `Baseline_Demand_Policy` SHALL 计算
  `P_gen_req = LPF(P_load) + u_recovery · P_chg_recovery`：LPF 为一阶低通
  `τ = PMS_LoadLPF_tau_s = 60 s`；输入 P_load（04_Load_Measurement）、SOC、
  commitment state 反馈；以 MATLAB Function / Simulink blocks 实现，**不进 Stateflow**。
- **FR-433**: SOC recovery 滞回 SHALL 为：`u_recovery` 在 `SOC <= 0.55`（`PMS_SOC_lo`）
  置 1、`SOC >= 0.60`（`PMS_SOC_recovered`）清 0，初值 false；比较符号按此冻结；
  v1 不设高 SOC 主动放电项。
- **FR-434**: recovery 偏置 SHALL 为 **non-firm/opportunistic**：实际偏置
  `P_recovery_actual = max(0, min(P_chg_recovery=100 kW, 500·N_dispatch − LPF(P_load)))`；
  削减只发生在 Baseline source 内部；advanced EMS source 的 `P_gen_req = P_EMS`
  **不得削减**，mismatch 以 dispatch 诊断量暴露。
- **FR-435**: 四参数（60 s / 0.55 / 100 kW / 0.60）SHALL 标记 ENGINEERING_SURROGATE；
  敏感性档位 τ∈{30,120} 冻结登记、本 spec 不扫掠。

### DG Commitment Supervisor（T-415 / T-416）

- **FR-436**: `DG_Commitment_Supervisor` SHALL 以 **Stateflow** 实现离散运行模式机
  （ONE_DG ⇄ STARTING_SECOND ⇄ TWO_DG ⇄ …），且是全模型**唯一**写
  `DG1_cmd_onoff / DG2_cmd_onoff` 的块（单一写入者原则）。
- **FR-437**: 正常经济启停判据 SHALL 作用于 `P_commit = LPF(P_load)`（**不含 recovery
  偏置**）：单机态 `P_commit > 425 kW`（α_start=0.85×500）持续 `30 s` → START DG2；
  双机态 `P_commit < 350 kW`（α_stop=0.70×500，remaining-unit 口径）持续 `120 s`
  且 `t_DG2_ON ≥ 300 s` → STOP DG2；min-up 与 stop-hold **并行计时**（AND 汇合，
  最短 ON = 恰好 300 s）；`PMS_min_up_s=300 / PMS_min_down_s=0`；滞回带 [350, 425] 内
  状态由历史保持。
- **FR-438**: 仲裁顺序 SHALL 冻结为
  **Hard availability/fault veto > Safety override > Normal commitment**：
  fault/unavailable 机组禁止 START、必要时 FORCE_STOP；Safety 只能 FORCE_START
  `healthy ∧ available ∧ offline` 机组；Safety 旁路 start/stop hold、min-up 等经济
  dwell，**不得旁路** 25 s STARTING + 5 s SYNC 等 Plant 物理；源码中以显式注释固化
  该顺序。
- **FR-439**: v1 常规 baseline SHALL 在 `voyage active` 期间保持 `N_DG_on ≥ 1`
  （无 0DG 态）；文档措辞采用 "adopts a one-genset-online minimum operating policy as
  the conventional baseline assumption"，禁止表述为真实船舶普适事实；0DG 留 future
  extension。
- **FR-440**: 每次 commitment transition SHALL 记录审计条目
  `{timestamp, old_state, new_state, reason_code, P_commit, normal_start_timer,
  normal_stop_timer, online_timer, safety_override}`；`reason_code ∈
  {NORMAL_HIGH_LOAD, NORMAL_LOW_LOAD, SAFETY_DG_FAULT, SAFETY_FIRM_DEFICIT,
  SAFETY_MIN_GEN_SURPLUS}`（可扩展）；审计入 `12_Data_Logging` 并供 43 号 Gate 做
  规则可追溯性校验。

### DG Dispatch Allocator（T-416）

- **FR-441**: `DG_Dispatch_Allocator` SHALL 执行分配律
  `P_i,ref = clamp(P_gen_req_eff / N_dispatch, P_min=125, P_max=500)`；
  `N_dispatch = #(state=ON ∧ dispatch_eligible)`（v1 `dispatch_eligible=(state==ON)`；
  STARTING/SYNC/COOLDOWN 不计入，不得虚构其功率——A2 契约延续）；文档措辞
  "equal-rated parallel gensets under symmetric load-sharing configuration"。
- **FR-442**: SHALL 输出诊断量 `P_gen_unserved = max(0, P_req − ΣP_i,ref)`、
  `P_gen_surplus = max(0, ΣP_i,ref − P_req)`、`dispatch_min_clamp_active`（控制器意图
  语义；与 Plant `below_min_flag` 物理语义区分，实施前核查 A2 flag 定义并在 design
  写死）。
- **FR-443**: 机组优先级 SHALL 固定 DG1=lead / DG2=lag（deterministic，不轮换）；
  `DISPATCH_MODE` 接口文档保留 `0=AGGREGATE_AUTO（implemented）/ 1=EXTERNAL_PER_UNIT
  （reserved）`，本 spec **不写**第二模式代码分支；**禁止** BSFC-aware 分配与
  不对称加载。

### Safety / Interlock 层（T-417）

- **FR-444**: SHALL 新增顶层块 `16_Safety_Interlock`（离散 0.01 s，确定性，全部
  override 事件入日志）；Safety **不直接写 Plant**——override 请求送入 Commitment
  Supervisor 与 source selector 作为高优先级输入。
- **FR-445**: 执行性 override SHALL 恰为五条：
  - **S1 DG fault**：expected committed unit 的非预期状态跳变/外部强制丢失（**不是**
    字面 state≠command 判据——STARTING/SYNC 期间 cmd=ON 而 state≠ON 属合法）→
    故障机 `dispatch_eligible=false` + FORCE_STOP；另一机 OFF 且航次激活 →
    FORCE_START 健康机；
  - **S2 Realized supply deficit**：`P_gen_unserved > P_bat_dis_avail` 持续 2 s →
    FORCE_START 可用健康离线机组（缺口已由 Dispatch 兑现且电池 SOP 补不上）；
  - **S3 Min generation surplus**：`N_dispatch·125 − P_load > P_bat_chg_avail` →
    FORCE_STOP DG2（旁路 dwell）；
  - **S4 Prospective firm-capacity deficit**：`D_firm = P_load,req,bus + 50 −
    P_firm,avail,bus > 0` 持续 2 s，其中
    `P_firm,avail,bus = N_ON,healthy · P_DG,rated,bus + P_bat_dis_avail`、
    `P_load,req,bus = P_prop,dem,bus + P_hotel/service`（commanded demand）→
    FORCE_START 可用健康离线机组（目标机 STARTING/SYNC 不重复启动仅 flag；双机 ON
    或无可启机组 flag-only）；S4 是 **capacity adequacy** 判据，**不是** instantaneous
    ramp adequacy（ramp/τ 瞬态缺口归 battery/S2/Vdc 链）；`P_DG,rated,bus` 的
    rectifier 前后口径在 design 门源码核查后写死；
  - **S5 EMS health fallback**：`ems_health_ok=0`（freeze/NaN/超时）→ source selector
    切回 `Baseline_Demand_Policy`（唯一确定性热备份；先进 EMS 永不直写 Plant）+
    `baseline_fallback_active` 置位 + alarm 锁存。
- **FR-446**: 纯监视（flag 入日志，不独立动作）SHALL 含：SOC ≤ 0.20、Vdc < 600 V、
  电池 V/I/LUT 边界（沿用 ECM 既有 flag）；**`08_Undervolt_Derate` 保持唯一执行性
  propulsion limiter**，Safety 不得新增第二条推进功率限制路径。
- **FR-447**: Out-of-scope 声明 SHALL 写入 spec：load shedding、断路器联锁、blackout
  recovery、重合闸/再同步、0DG、电机热限、第二条 propulsion limiter。
- **FR-448**: 验证注入通道 SHALL 复用/新增：S1 复用 `GEN_CMD_STOP_TS`；S5 新增
  `EMS_HEALTH_TS`（From Workspace + `'Interpolate','off'`，经 assignin 通道）；
  S2/S3/S4 经确定性 bench 注入（`gate_assign_bench_inputs` 机制扩展）。

### 接口、集成与参数管理

- **FR-449**: 新顶层子系统 `07_PMS_v2`（内含 Baseline_Demand_Policy + source selector +
  DG_Commitment_Supervisor + DG_Dispatch_Allocator）SHALL **替换**
  `Legacy_Commitment_Adapter`；Genset Plant 接口不变；FR-421 防线随替换转移：
  **论文经济性对照只允许使用 P2 臂**（R0 仅行为上下文）。
- **FR-450**: source selector SHALL 以 `PMS_SOURCE_MODE` 编译期选择
  `BASELINE_POLICY | EMS_AGGREGATE`；EMS 七变体聚合 `P_gen_ref_kW` 经 EMS_AGGREGATE
  通道接入共享 Commitment/Dispatch；**全部七变体编译/构建成功**（A2 FR-426 契约
  延续），law=0/law=5 短程运行无错误。
- **FR-451**: 控制参数 SHALL 集中于独立配置文件（**不动** `ModelE_Physics_Parameters.m`
  的 Plant 事实源；文件名与加载顺序 design 定），逐参数 provenance 标记；敏感性注册表
  （α_start∈{0.80,0.85,0.90}、α_stop∈{0.60,0.70,0.80}、min_up∈{0,300,600}、
  firm_margin∈{0,100}、τ∈{30,120}）冻结登记不扫；min_down 不登记。

### Baseline 验证与控制 Gate（T-418）

- **FR-452**: SHALL 实现三臂对比：**R0**=旧 law=0 + 新 Commitment/Dispatch（historical
  context）、**R5**=law=5 + 新链（advanced integration）、**P2**=BaselineDemand +
  Commitment + Dispatch + Safety（**唯一 conventional economic baseline**）；
  **R0/R5 的燃油优劣不参与 B 包 PASS/FAIL**（只过 interface/determinism/safety/
  no-crash/no-illegal-command）；P2 独承 behavioral-quality Gate。
- **FR-453**: 工况矩阵 SHALL 为：AUTO（S20 复合航次 + S01 静水巡航 + 顺水/逆水 +
  浅水代表档 S31/S34/S36）× 三臂；MANUAL（M1/M2/M3）仅 P2 臂；Safety 确定性注入
  **S1~S5 各至少一个独立场景**。
- **FR-454**: 指标体系 SHALL 机器可读取入 `results/`：fuel_total、DG starts
  （分 `DG2_start_normal_count / DG2_start_safety_count`）、`DG1_online_time_s /
  DG2_online_time_s`、双机低负荷 dwell 时间、SOC min/end、Vdc min/越限时长、
  battery throughput（kWh 充/放分列）、unserved/surplus 能量、override 事件计数。
- **FR-455**: SHALL 新增 `scripts/43_run_modelE_control_gate.m`（数字开头文件名，
  `eval(fileread(...))` 入口）输出 `results/gate_control_result.json`（overall +
  per-check + per-arm/per-scenario metrics + Safety assertions + transition audit），
  FAIL 时 `error('GATE:*')` 非零退出；判据集 SHALL 含：
  1. **确定性**：同工况两跑逐位一致（G6 口径）；
  2. **规则合法性**（替代 starts 硬阈值）：每个正常 start 满足 425 kW×30 s
     （除非 `SAFETY_OVERRIDE`）；每个正常 stop 满足 <350 kW×120 s 且 ON≥300 s；
     `N_starts,normal ≤ N_eligible_high-load_episodes`（episode 分割算法 design 定）；
     `starts≤6` 类数字仅作 report warning，不作硬 Gate；
  3. P2 臂 S20 `|SOC_end − 0.60| ≤ 0.05`（**"未长期掏空电池"行为 Gate，
     非燃油公平比较带**；终端 SOC 公平性留 F 包论文对比解决）；
  4. **Vdc 分层**：正常场景全程 Vdc ≥ 600 V（硬 Gate）；注入场景用场景特定断言
     （expected override 触发 / critical flag 一致 / recovery 发生 / 无 NaN 失控；
     Vdc 越限须被正确记录）；
  5. 能量闭合 <0.1%（G5 口径）；
  6. **S1~S5 断言式验证**（含负向：不该触发时不触发）；
  7. **transition reason 审计**：每次 start/stop 可由合法 reason_code 解释；
  8. **T-412 重跑 `overall=true`**（Plant 零回归硬防线；禁改参数凑 PASS）。
- **FR-456**: 基线 `config/gate_baseline_modele_control.json` SHALL **只在**
  43 PASS + T-412 PASS + 用户评审后冻结 v1.0（禁止首 PASS 自动冻结）。

### 归档与登记

- **FR-457**: SHALL 按 AGENTS.md §20 同步全部受影响目录 README（Model E 工作区
  README.md + README_Model.md §2/§3 控制侧描述更新、docs/phase4/ 新建 b 包文档
  目录索引等）；§18 开发日志登记（含延伸问答记录）；A2 spec design.md 补记 **D-14**
  偏差条目（T-013 遗留：R_0 充放电共用 / SOC 降额族来源升级 / k_form 降级 /
  t_starting 标签改 DERIVED+LITERATURE_SURROGATE），随本包落账。

## Non-Functional Requirements

- **NFR-412**: Model E Plant 物理零改动——builder 中 03/06/10/14 等 Plant 子系统源码
  不得变更；T-412 重跑 `overall=true` 为唯一背书，Gate FAIL 时 `error('GATE:*')`
  停下升级，禁止调参凑 PASS。
- **NFR-413**: Model D 工作区冻结只读，sha256 零改动持续证明（G1 口径延续）。
- **NFR-414**: 全部模型修改脚本驱动（builder 扩展，禁 GUI 手工修改）；结构指纹更新
  （新块入指纹）且幂等（两建逐位一致）；修改 `.slx` 后做结构差异检查。
- **NFR-415**: R2026a 工程坑约束：MATLAB Function 块接线用句柄式 add_line；
  persistent 状态块设 `EMChart.ChartUpdate='DISCRETE'`；From Workspace 矩阵数据
  `'Interpolate','off'`；数字开头脚本以 `eval(fileread(...))` 执行；**禁止调用
  `model.ensure_canonical`**；仿真注入经 assignin 通道（不用 SiN.setVariable 于
  From Workspace 数据源）。
- **NFR-416**: 禁硬编码绝对路径（§20.2 扫描零命中）；仓库外依赖环境变量注入。
- **NFR-417**: 确定性——fixed-step、离散逻辑 0.01 s 采样、无新代数环；同工况两跑
  逐位一致。
- **NFR-418**: provenance 与措辞纪律——禁止无据声明（如"OEM 普遍要求 15–30 min
  minimum run"）；surrogate 参数如实标记；不过度声称 lever 真实性；推导值与文献
  原值区分（A2 引用纪律延续）。
- **NFR-419**: Git 纪律——stacked 分支 `feat/phase4-b-pms-safety`（自 A2 最终用户批准
  冻结 head 叠开，当前为 4faf9a87；hash 非长期架构要求）；逐文件 add；禁 merge 至
  Phase4（待用户 PR 审批）；全部红线（§7）适用。

## Acceptance Criteria

- **AC-416**（FR-428~431）：双模式构建成功；AUTO 既有工况行为不变（T-412 重跑背书）；
  MANUAL M1~M3 运行无错误，指令方向/rate limiter 行为正确；T-413 决策小节落档。
- **AC-417**（FR-432~435）：Baseline Demand 单测通过（LPF 阶跃响应、滞回进出点含
  边界等号、削减公式含 `max(0,…)` 与单机/双机容量两档）。
- **AC-418**（FR-436~440）：Commitment 单测通过——threshold/hold 计时、滞回带驻留、
  并行计时最短 ON=恰好 300 s、min_down=0 即时重启路径、仲裁顺序（veto > safety >
  normal）、审计条目完整性与 reason_code 合法性。
- **AC-419**（FR-441~443）：Dispatch 单测通过——均分、clamp 边界、N_dispatch 排除
  非 ON 机组、诊断量公式、STARTING 机组零功率（A2 契约）。
- **AC-420**（FR-444~448）：S1~S5 注入场景断言全过；监视 flag 正确置位；
  08 唯一 propulsion limiter 结构检查通过。
- **AC-421**（FR-449~451）：七变体编译成功；law=0/5 经新链短程运行无错误；
  参数文件 provenance 完整、敏感性注册表登记。
- **AC-422**（FR-452~456）：43 号 Gate `overall=true` 且 T-412 重跑 `overall=true`；
  负向验证（构造违规 transition / 应触发未触发的 override）正确 FAIL 并非零退出。
- **AC-423**（FR-457）：README 两级同步、开发日志、D-14 补记齐全；绝对路径扫描
  零命中。
- **AC-424**：用户评审门——43 Gate 结果 + 三臂对比报告经用户**书面批准**后冻结
  控制基线 v1.0，B 包收官（spec 升 implementation-complete）。

## Edge Cases

- 双机最低出力盈余 > 电池充电 SOP（Q6 死角 → S3 FORCE_STOP）；
- 恢复充电与启动门槛协调（Commitment 信号语义 (b) 结构性解决，D-B4）；
- STARTING/SYNC 期间负荷回落（aborted-start 不可中断 + pending_stop，A2 契约延续）；
- 滞回带 [350, 425] 驻留（状态由历史决定，非缺陷）；
- EMS 输出 freeze/NaN/超时（S5 回退 Baseline）；
- SOC 降额带内 SOP 收缩与 commitment 相互作用（S2 兜底）；
- 800→300 kW 回落后 DG2 停机 ≈258 s 的已知保守现象（T-418 报告 dwell，不加滤波）；
- DG fault 发生于另一机 STARTING 中（不重复启动，flag-only，A2 时序语义延续）；
- 单机 ON 且 req>500 的瞬态缺口（battery 下垂 + S2 判据，不属 dispatch 职责）。

## Dependencies and Constraints

- A2 spec 接口契约：FR-421（adapter 替换位）/FR-424（SOP 同名同义）/FR-426（七变体
  兼容）/design D-4（min up/down 属控制层）/D-13（禁 ensure_canonical）；
- F 包 T-448 层级分离前瞻契约（commitment 层可复用）；
- 路线文档 §3 任务定义 + Q5 裁决（B2 并入 T-417）；
- 参考文献链：`项目开发进展.md` 2026-09-04 条目 [1]~[19] + brainstorm 新增
  Kongsberg/Høglund/ComAp/Siemens PMA 300/IFAC 2017/GB/T 47666—2026 引用
  （provenance 按 D-B4 修正后的组合口径表述）；
- design 门待核查/待决六项：① `P_prop_demand_kW` 电气/机械口径；② 500 kW 的
  rectifier 前后口径（`P_DG,rated,bus` 是否乘 η_rect）；③ A2 `below_min_flag` 精确
  语义；④ 控制参数文件名与加载顺序；⑤ Manual rate limiter 数值；⑥
  `N_eligible_high-load_episodes` 机器判定算法；⑦ EMS 变体接入 source selector 接线。

## Open Questions

无需求级开放问题（九轮 brainstorm 已收口）。上述七项为 design 级待决，不构成需求
模糊；若 design 核查推翻某假设（如口径语义），按 §5 Escalation 回主 Agent 裁决。

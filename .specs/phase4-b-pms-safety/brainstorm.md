# Phase 4-B — 真实控制逻辑 / PMS v2 / Safety-Interlock（T-413~T-418） Brainstorm

Spec: `phase4-b-pms-safety`  
Status: brainstorm-complete  
Created: 2026-09-04

## User Intent

# Phase 4-B Brainstorm 记录（九轮裁决 + 终审修正）

**日期**：2026-09-04｜**状态**：brainstorm-approved（用户书面批准，APPROVED WITH EDITORIAL AMENDMENTS，5 项修正已并入）｜**路线文档**：`docs/phase4_research_roadmap.md` §3（T-413~T-418）｜**前置**：A2 spec（`.specs/phase4-a2-model-e-construction`）implementation-complete，T-412 Gate overall=true，基线 v1.0 冻结。

## User intent

在 Model E 物理基座（A2 已冻结）之上重建真实控制/安全侧：驾驶输入抽象与推进控制重构（T-413/414）、可信工业 PMS v2 baseline（T-415/416）、独立 Safety/Interlock 层（T-417，吸收 backlog B2 失败模式门控）、baseline 控制器验证（T-418）。全程 spec 四门禁，中文工作语言。

## 九轮裁决（D-B1 ~ D-B9）

### D-B1 — 模块化双源 + 共享 Commitment 架构（A′，用户原创裁决）
四层分离：`Baseline_Demand_Policy`（发多少电，连续规则，MATLAB Function/Blocks，不进 Stateflow）→ `DG_Commitment_Supervisor`（开几台机，Stateflow 离散模式机）→ `DG_Dispatch_Allocator`（在线机组分配，确定性）→ Safety/Interlock（独立最高优先级层）。论文意义上的 Industrial PMS baseline = 整条链。职责红线：Commitment 只改 u₁,u₂∈{0,1}，不重新优化连续功率；Dispatch 只做可行化（P_min/P_max/ramp/启动不可用/safety），正常情况下 P₁+P₂≈P_gen_req。Vdc 不进 Commitment 普通经济逻辑输入（异常归 Safety override）。`DISPATCH_MODE` 预留 AGGREGATE_AUTO/EXTERNAL_PER_UNIT 接口设计，B 包只实现 AGGREGATE_AUTO。满足 F 包 T-448 层级分离契约。

### D-B2 — Baseline 需求律 = 候选 (iv) 工业简化型 Load-Following + SOC 恢复
拒绝 P_opt 吸附/死区/在线 BSFC 查表/比例 SOC 调节/复杂电池 load shifting。依据：真实船舶 PMS 复杂度集中在启停时序/联锁/保护，经济调度核心是 threshold+timer+state（Kongsberg/Høglund/ComAp 产品功能、IFAC 2017 load-dependent start-stop table、Siemens PMA 300 示例）。控制层级叙事：Conventional PMS → ECMS → Predictive EMS，先进性逐层出现。

### D-B3 — Demand Policy 四数（冻结）
`PMS_LoadLPF_tau_s=60 s`（敏感性 {30,120} 登记不扫）/ `PMS_SOC_lo=0.55`（用户自 0.50 改：死区过宽会重引入 terminal-SOC 公平性问题；53.6 kWh 欠账 ≈34 min 恢复）/ `PMS_P_chg_recovery_kW=100`（≈0.09 C）/ `PMS_SOC_recovered=0.60`（=SOC_init，charge-sustaining 口径自动对齐）。比较符号冻结：`SOC<=0.55` 进、`SOC>=0.60` 出、`recovery_active` 初值 false。全部 ENGINEERING_SURROGATE。文案纪律：DG2 并网延迟仅量级估计不作参数依据；100 kW 与启动门槛协调性由 Q4 结构性验证。

### D-B4 — Commitment 四数 + 信号语义（冻结）
信号 = `P_commit = LPF(P_load)`（**不含 recovery 偏置**，方案 b）。`α_start=0.85`（425 kW；措辞 "15% rated-capacity headroom" 非 spinning reserve）/ `T_start=30 s` / `α_stop=0.70`（350 kW，remaining-unit 口径，非 0.70×1000；用户自 0.65 改：避免 325~425 驻留导致双机长期 190 kW/unit 低效运行）/ `T_stop=120 s`。滞回带 [350,425]。provenance = LITERATURE-ANCHORED ENGINEERING_SURROGATE（Siemens 85% 门槛 × Perabo 30/120 s 延迟组合；Perabo 原表实为 60%/30 s 启、25%/120 s 停；Siemens 默认 85%/5 s 启、70%/120 s 停——禁写成单一直引）。**结构性规则**：SOC recovery 是 opportunistic/non-firm 需求——`P_recovery_actual = max(0, min(100, 500·N_dispatch − P̄_load))`，负荷服务优先于恢复充电，recovery 不得单独触发 DG2 启动；Safety/SOP override 可独立 FORCE_START。已知现象备案：800→300 kW 回落后 DG2 停机 ≈258 s（LPF 138 s + hold 120 s），不加额外滤波，T-418 报告 two-DG low-load dwell。

### D-B5 — min dwell（冻结）
`PMS_min_up_s=300`（ENGINEERING_SURROGATE，anti-short-cycle backstop 定位；禁写"OEM 普遍 15–30 min"——无可靠证据）/ `PMS_min_down_s=0`（Q4 的 425 kW+30 s hold+75 kW 滞回已防 restart 抖动；避免"正常启动被禁→电池硬扛→safety 被迫旁路"绕行路径）。**min-up 与 stop-hold 并行计时**（AND 汇合）：最短 ON=恰好 300 s，写入 Stateflow spec。优先级：`Safety > Plant 物理 > PMS 经济 dwell`——safety 旁路 min-up/min-down/hold/hysteresis，但不旁路 25 s STARTING + 5 s SYNC。v1 无 0DG（`voyage active ⇒ N_DG_on ≥ 1`）为 baseline scope 声明，论文措辞 "adopts a one-genset-online minimum operating policy as the conventional baseline assumption"，禁普适化；0DG 留 future extension（C 包靠泊/港内再评）。敏感性注册：min_up∈{0,300,600}；min_down 不登记。

### D-B6 — Dispatch 分配律（冻结）
`P_i,ref = clamp(P_gen_req_eff / N_dispatch, 125, 500)`；`N_dispatch = #(state=ON ∧ dispatch_eligible)`（v1 eligible==ON；STARTING/SYNC/COOLDOWN 不计入，不虚构功率——A2 契约）。措辞 "equal-rated parallel gensets under symmetric load-sharing configuration"。低负荷死角封闭：`N_dispatch·125 − P_load > P_bat_chg_avail` → 上报 `min_generation_surplus`，Safety FORCE_STOP DG2 旁路 dwell（Dispatch 不自行解决）。诊断量：`P_gen_unserved = max(0, P_req − ΣP_i,ref)`、`P_gen_surplus = max(0, ΣP_i,ref − P_req)`、`dispatch_min_clamp_active`（控制器意图语义，与 Plant below_min_flag 物理语义分离，实施前核查 A2 定义）。recovery 削减只存在于 Baseline source；advanced EMS 请求直送不削减。DG1=lead/DG2=lag 固定（不轮换）。EXTERNAL_PER_UNIT 仅接口文档保留。拒绝项：不对称加载/BSFC-aware 分配/轮换。

### D-B7 — Safety 架构四原则（冻结）
① 唯一执行性 propulsion limiter = 08_Undervolt_Derate；Safety 加 flag/override/commitment 动作，无第二条推进功率限制路径。② 单一写入者：Commitment Supervisor 是唯一写 DG1/DG2_cmd_onoff 的块，Safety 只写 override request。③ Baseline_Demand_Policy 是唯一确定性热备份（ems_health_ok=0 时接管发电需求生成，先进 EMS 永不直写）。④ 仲裁顺序源码显式注释固化。既定判据：lag start 失败 40 s 窗口、low-SOC 用户确认 override、`propulsion_power_unmet_kw`、`baseline_fallback_active`、全 override 事件入 16_Safety_Interlock 日志、VI-1 唯一版本。**S1 定义修正**（Q8 用户提醒）：不实现字面 state≠command→fault（STARTING/SYNC 期间 cmd=ON 而 state≠ON 合法）；S1 = expected committed unit 的非预期状态跳变/外部强制丢失。

### D-B8 — 双模式推进控制（冻结）
Manual = 归一化推进功率指令 `manual_power_cmd_pct∈[0,1]`，线性 `P_ref = u·P_max` + command-side rate limiter（与 DG 20 kW/s 物理 ramp 参数分离；数值 design 定，v1 可从宽）；弃 RPM 闭环/立方映射；命名不用 lever_pct（不过度声称真实车钟特性，注明 "normalized operator propulsion command surrogate"）。AUTO = 现有速度 PI 原样保留。两模式汇聚唯一输出 `P_prop_demand_kW`；01 改组 `01_Operator_Interface`、02 改组 `02_Propulsion_Controller`；Plant 不动。工况内模式固定（`VOYAGE_CTRL_MODE` 编译期常量），runtime 切换动态 out-of-scope。MANUAL 仅 3 个最小工况（M1 step / M2 ramp / M3 staged maneuver 验 1DG→2DG→1DG）。T-413 产出 = design.md 决策小节（七行选型表 + assumptions/limitations/interface contract/future extension），不做独立文献综述。

### D-B9 — S4 定案 + T-418 验证矩阵（冻结，含公式修正）
**S4（Prospective firm-capacity deficit）**：`D_firm = P_load,req,bus + 50 − P_firm,avail,bus`，`D_firm>0` 持续 2 s 成立；`P_firm,avail,bus = N_ON,healthy·P_DG,rated,bus + P_bat_dis_avail`；`P_load,req,bus = P_prop,dem,bus + P_hotel/service`（commanded demand 保持前瞻性）；统一 DC-bus 域；margin=50 kW（ENGINEERING_SURROGATE，注册 {0,100}）；S4 是 capacity adequacy 判据，**不是** instantaneous ramp adequacy（ramp/τ 瞬态缺口归 battery/S2/Vdc，文案分离）。动作泛化：FORCE_START available healthy offline unit（不写死 DG2；目标机 STARTING/SYNC 不重复启动仅 flag；双机 ON/无可启机组 flag-only）；不新增 propulsion limiter。**T-418 三臂**：R0=旧 law=0+新 Commitment/Dispatch（historical context）/ R5=law=5+新链（advanced integration）/ **P2=完整 PMS v2 链（唯一 conventional economic baseline）**；**R0/R5 燃油优劣不参与 PASS/FAIL**，P2 独承 behavioral-quality Gate。Safety 注入场景 S1~S5 五类各一。指标补 DG1/DG2_online_time_s + starts 分 normal/safety 列。**删 DG2 starts≤6 硬 Gate** → 规则合法性判据（正常 start=425 kW×30 s 除非 SAFETY_OVERRIDE；正常 stop=<350 kW×120 s 且 ON≥300 s；N_starts,normal ≤ N_eligible_high-load_episodes 一对一防 chatter）；6 次降级 report warning。`|SOC_end−0.60|≤0.05` = "未长期掏空电池"行为 Gate，非燃油公平比较带（终端 SOC 公平性留 F 包）。Vdc≥600 分层：正常场景硬 Gate；注入场景用场景特定断言。新增规则可追溯性 Gate：transition 审计 `{timestamp, old_state, new_state, reason_code, P_commit, timers, safety_override}`，reason_code∈{NORMAL_HIGH_LOAD, NORMAL_LOW_LOAD, SAFETY_DG_FAULT, SAFETY_FIRM_DEFICIT, SAFETY_MIN_GEN_SURPLUS}，每个 start/stop 须可由合法 reason 解释。43 号 Gate 脚本 + gate_control_result.json + error('GATE:*') 批准；基线只在 43 PASS+T-412 PASS+用户评审后冻结 v1.0（禁首 PASS 自动冻结）。

## 终审 5 项 editorial amendments（已并入上文）

1. **仲裁优先级改写**：`Hard availability/fault veto > Safety override > Normal commitment`——Safety 不能命令 fault/unavailable 机组启动；"Safety 最高"是相对经济控制规则而言，不高于物理 availability veto；
2. **S2/S4 命名分离**：S2=Realized supply deficit（unserved>SOP_dis 持续 2 s，缺口已兑现）；S4=Prospective firm-capacity deficit（预见容量余度不足）；S4=preemptive、S2=realized；
3. **S4 完整公式入档**（见 D-B9）；
4. **T-418 Gate 职责分离入 Summary**（防 requirements 误写三臂燃油优劣判据）；
5. **分支表述**：B 包从 A2 最终用户批准并冻结的 head 叠开（当前该 head 为 4faf9a87）——hash 非长期架构要求，A2 若有不改 Plant 行为的最终修正 commit，B 跟最终 approved head。

## Scope

**In scope**：T-413~T-418 全部；四层 PMS v2 链 + Safety 层 + 双模式推进控制 + 43 号控制 Gate + T-412 重跑；新顶层块 01_Operator_Interface（改组）/02_Propulsion_Controller（改组）/07_PMS_v2（替换 Legacy_Commitment_Adapter）/16_Safety_Interlock（新增）；控制参数独立配置文件；A2 D-14 偏差条目补记随本包落账。

**Out of scope**：Model E 设备物理任何改动；0DG 电池-only 模式；runtime AUTO⇄MANUAL 切换；load shedding/断路器联锁/blackout recovery/重合闸；lead/lag 轮换；BSFC-aware dispatch；EMS 七变体内部修改；预测器本体（E 包）；MPC（F 包）；N6/HIL；数据集生成（D 包）；P_opt 吸附/死区/比例 SOC 调节。

## Constraints / risks

分支 stacked 自 A2 冻结 head；Model D sha256 零改动持续证明（G1）；T-412 重跑 overall=true 为 Plant 零回归硬防线（禁改参数凑 PASS）；Git 红线（逐文件 add、禁 merge 至 Phase4 待 PR 审批）；R2026a 工程坑清单全部适用（句柄式 add_line / persistent 块 EMChart.ChartUpdate='DISCRETE' / From Workspace Interpolate=off / assignin 注入通道 / 禁 model.ensure_canonical / 数字开头脚本 eval(fileread) 入口）；provenance 纪律（禁虚构 OEM 惯例、surrogate 标记准确、不过度声称 lever 真实性）；敏感性档位一律冻结登记不扫。

## Assumptions（design 门待核查项）

① `P_prop_demand_kW` 电气/机械口径（决定 S4 的 η_chain 是否为 1）；② 500 kW 的 rectifier 前后口径（P_DG,rated,bus 是否乘 η_rect）；③ A2 `below_min_flag` 精确语义；④ 控制参数文件落位；⑤ Manual rate limiter 数值；⑥ `N_eligible_high-load_episodes` 机器判定算法；⑦ EMS 变体输出接入 source selector 的具体接线。500 t 口径沿用（实船确认挂账不受影响）。

## Remaining doubts

无需求级遗留疑点；上述 7 项为 design 级待决，已在 Dependencies 登记。A2 PR 待用户审批（不阻塞 B 包 stacked 施工）。

## Suggested spec slug

`phase4-b-pms-safety`（分支 `feat/phase4-b-pms-safety`，已从 4faf9a87 叠开）

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

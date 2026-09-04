# Phase 4-B — 真实控制逻辑 / PMS v2 / Safety-Interlock（T-413~T-418）Design

Spec: `phase4-b-pms-safety`  
Status: design-draft  
Created: 2026-09-04  
Requirements: `./requirements.md`（requirements-approved，FR-428~457 / NFR-412~419 / AC-416~424）  
Brainstorm: `./brainstorm.md`（D-B1~D-B9 + 终审 5 项修正，全部 Fixed Decisions）

## 1. 概述

本设计把 B 包四层控制架构（Baseline_Demand_Policy → DG_Commitment_Supervisor →
DG_Dispatch_Allocator + Safety/Interlock + 双模式推进控制）落到 Model E 既有 16 块
顶层结构上。核心施工面：

- **改组**：`01_Scenario_Manager` → `01_Operator_Interface`；`02_Ship_Speed_Controller` →
  `02_Propulsion_Controller`（块内重构，对外仅 `P_prop_demand_kW` 一个 actuator 级输出）；
- **替换**：`Legacy_Commitment_Adapter` → `07_PMS_v2`（含四层中前三层 + source selector）；
- **新增**：`16_Safety_Interlock`（独立最高优先级层）；
- **不动**：03/04/06/08/09/10/11/13/14/15 全部 Plant 与既有保护链（NFR-412）。

所有已裁决参数值以 requirements.md 为准，本文档不重复论证取值理由，只定义实现语义。

## 2. Design 门源码核查结论（requirements Dependencies 七项待决的裁决）

核查方式：delegate 只读静态阅读 + 主 Agent 抽查四处关键证据逐字复核（2026-09-04）。
证据锚点：`build_model_e.m:233-235/262-266/944-946/1052-1061/1185-1191`、
`code_genset_unit_e.m:103-104`、`config/ModelE_Physics_Parameters.m:171-175`、
`battery_sop_limits.m:44-46/97-101`。

| # | 待决项 | 裁决 |
|---|---|---|
| ① | `P_prop_demand_kW` 口径 | **电气 bus-side**（02 输出 = Saturation[0, Mot_P_nom=700] 直出；Elec_Eff=0.92 施加在其下游 03 内部「电功率→机械功率」点；04 的 P_load 同为 bus-side） |
| ② | 500 kW 口径 | **genset-side（rectifier 前）单机**；`ASSUMED_eta_rect=0.97` 在 09/13 入口施加（`P_bus = P_gen×0.97`）；P_min=125/P_max=500/BSFC 域 [125,500] 同域；电池 SOP 输出与 P_conv=1050 已含 η_dcdc 折算、为 **bus-side**（A2 OQ-9 冻结） |
| ③ | `below_min_flag` 语义 | **控制器意图**（`st==3 && P_ref_kW < DG_P_min_kW`，与实际输出无关）→ FR-442 的 `dispatch_min_clamp_active` 与之天然区分；Safety 若需「实际欠载」判据须以 `state==3 && P_DGi_kW<125` 另建（v1 不需要，登记） |
| ④ | 控制参数文件 | **新建 `config/ModelE_Control_Parameters.m`**，在 builder/Gate 入口中于物理参数之后运行（§8） |
| ⑤ | Manual rate limiter | **20 %/s**（满行程 5 s；ENGINEERING_SURROGATE；与 DG 20 kW/s 物理 ramp 完全独立） |
| ⑥ | high-load episode 算法 | §6.3（Gate 后处理，基于审计日志 timer 饱和事件） |
| ⑦ | EMS 接入 source selector 接线 | §5.1（替换点 = adapter 整块；端口契约原样+扩展） |

**附带发现（登记为实施注意）**：adapter 四输出与 `P_bat_chg_avail_kW` 当前无 logging
打点（B 包必须新增，§5.4）；`12_Data_Logging` mask 文本引用旧 `build_model.m`
（文档瑕疵，随 B 包修正，偏差登记 D-B12）；`ASSUMED_Bat_Eff_*`=0.95 已停用不得复用；
源码无现成 shaft 功率信号（如需要须显式乘 Elec_Eff×Gear_Eff，v1 不需要）。

## 3. DD-1：canonical power domain（FR-451A 落案）

**裁决：canonical 域 = genset-side（generator-equivalent kW）。**

理由：① 机组额定/阈值/clamp（500/425/350/125）与 EMS 聚合输出（clamp Gen_P_max=1000）
天然 genset-side，选此域则全部冻结数字字面可用、dispatch 输出直连 Plant 零换算；
② 换算点最少（仅 07_PMS_v2 输入边界三处）；③ 避免 bus-side 方案产生
412.25/339.5/121.25 类非圆整阈值。

**换算规则（唯一允许位置：`07_PMS_v2` 输入边界的三个显式 conversion 环节）**：

```text
P_load_gen        = P_load_total_kW   / ASSUMED_eta_rect    % bus → genset-equivalent
P_bat_dis_avail_g = P_bat_dis_avail_kW / ASSUMED_eta_rect
P_bat_chg_avail_g = P_bat_chg_avail_kW / ASSUMED_eta_rect
```

物理解释：P_bus = η_rect·P_gen ⇒ 485 kW 母线负荷需要 500 kW 机组侧出力；不等式两边
同除正数 η_rect 保序，FR 中所有阈值判据在 genset 域内逐字成立（425/350/125/500/100/50）。
EMS 聚合 `P_gen_ref_kW`（genset-side）与 Plant 接口 `P_DGi_ref_kW`（genset-side）零换算
直通。源码中以块名 `X_b2g_*`（bus-to-genset）显式标记 conversion 环节，注释引用本条目。

S4 的实现域随之确定：`P_load,req = (P_prop_dem + P_hotel)/η_rect`（genset 域）、
`P_firm,avail = N_ON,healthy·500 + P_bat_dis_avail_g`、`D_firm = P_load,req + 50 −
P_firm,avail`（50 kW margin 按 genset 域字面使用）；`η_chain=1` 成立（① 实证
P_prop_demand 为 bus-side 电气口径，进入 canonical 域只需与 P_load 相同的 ÷η_rect）。

## 4. T-413 选型决策表（FR-430 落案）

| 抽象 | 采用 | 理由 |
|---|---|---|
| lever→RPM | 否 | 需新增 RPM actuator/控制环，Model E 无此执行器层 |
| lever→torque | 否 | 当前控制接口非 torque 域 |
| lever→power | 是 | 与 Model E power-flow 接口一致 |
| AUTO speed hold | 是 | 现有 PI 原样复用 |
| runtime AUTO/MANUAL 切换 | 否 | 无研究需求，避免积分器/无扰切换状态管理 |
| 线性 manual map | 是 | 最小假设，无目标船 lever map |
| 立方 manual map | 否 | 无目标船映射证据，surrogate 过度声称 |

Assumptions：Manual 指令为 "normalized operator propulsion command surrogate; not a
measured bridge-lever transfer function"（一切文档/论文措辞遵守）。Limitations：v1 不
模拟真实车钟死区/回差。Interface contract：§5.2。Future extension：C 包 Mission v3 的
operator aggressiveness 随机化可驱动 manual 指令序列；真实 lever map 到位后替换线性映射。

## 5. 详细设计

### 5.1 顶层结构与接线（07_PMS_v2 替换 adapter）

```text
01_Operator_Interface ──{ctrl_mode, v_ref_mps, manual_power_cmd_pct}
        │（既有航次/环境输出不变）
02_Propulsion_Controller ──P_prop_demand_kW（bus-side，唯一 actuator 输出）
        │
05_EMS_Controller ──P_gen_ref_kW（genset-side，clamp [0,1000]）──┐
                                                                  ▼
04:P_load_total / 14:SOC / 13:Vdc / 14:SOP avails ──► 07_PMS_v2 ──► 06_Genset_System
   （经 X_b2g_* 换算入 canonical 域）                    │ DG1_enable / P_DG1_ref_kW
                                                       │ DG2_enable / P_DG2_ref_kW
16_Safety_Interlock ◄── 诊断回流（unserved/surplus/DG state/cmd/SOP/Vdc/SOC/ems_health）
        │ override 请求（经 UnitDelay 0.01 s，§5.6 防代数环）
        └──────► 07_PMS_v2（Commitment 高优先级输入 + Source selector fallback）
```

**07_PMS_v2 内部**（四个子块 + 选择器）：

```text
EMS:P_gen_ref_kW ──┐
                    ├─ Source_Selector（PMS_SOURCE_MODE + S5 runtime override）
Baseline_Demand_Policy ──P_gen_req_baseline ──┘        → P_gen_req_eff
        ▲（P_load_gen, SOC, N_dispatch 反馈）
P_gen_req_eff ──► DG_Commitment_Supervisor（Stateflow）──► DG1/DG2_cmd_onoff
        │              ▲ Safety override（force_start/stop、fault latch）
        └─► DG_Dispatch_Allocator（P_gen_req_eff, N_dispatch）──► P_DG1/2_ref_kW
                      └──► 诊断：P_gen_unserved / P_gen_surplus / dispatch_min_clamp_active
```

**端口契约**（替换点兼容，build 接线行对应旧 1185-1189）：

- 输入：P_gen_ref_kW（05）、P_load_total_kW（04）、SOC（14）、Vdc（13/15）、
  P_bat_dis/chg_avail_kW（14）、P_prop_demand_kW（02）、P_hotel（01 既有）、
  DG1/DG2_state、P_DG1/2_kW（06 反馈）、force 通道（见 DD-8）、EMS_HEALTH_TS（新增
  From Workspace，Interpolate=off）、Safety override 总线（16）；
- 输出：DG1_enable、P_DG1_ref_kW、DG2_enable、P_DG2_ref_kW（→06，语义不变）、
  commitment_state、审计总线（§5.5）、诊断三件套（→16 + logging）。

### 5.2 01/02 改组（FR-428/429）

- `01_Operator_Interface`：既有输出全部保留（含 water_depth_m Out6 等）；新增
  `ctrl_mode`（Constant=VOYAGE_CTRL_MODE，0=AUTO/1=MANUAL，编译期）、
  `v_ref_mps`（既有 v_cmd 通路改名输出）、`manual_power_cmd_pct`（Constant=
  VOYAGE_MANUAL_CMD_TS 查表/常量注入，经 From Workspace+Interpolate=off 支持
  M1/M2/M3 序列）。
- `02_Propulsion_Controller`：内部两路径——AUTO：既有 PI（Kp/Ki/抗饱和/Saturation
  [0,Mot_P_nom]）原样保留；MANUAL：`P_ref = manual_power_cmd_pct × Mot_P_nom` →
  Rate Limiter（`Manual_Cmd_RateLimit_pct_s=20 %/s` 折算 kW/s = 0.2×Mot_P_nom/s）
  → 同一 Saturation；Switch（ctrl_mode）选路 → 唯一输出 `P_prop_demand_kW`。
  **PI 参数与结构零改动**（AC-416 的行为不变指本条）。模式工况内固定，Switch 输入
  为编译期常量。

### 5.3 Baseline_Demand_Policy（FR-432~435）

实现：MATLAB Function 块（离散 0.01 s，EMChart DISCRETE，R2026a 纪律），源码
`+model/code_pms_demand_e.m`（单测同源字符串，A2 模式）。

```text
P_load_filt: 一阶 LPF，y[k] = y[k-1] + (Ts/τ)(x[k] − y[k-1])，τ=60 s，y[0]=x[0]
u_recovery:  persistent bool；SOC<=0.55 → true；SOC>=0.60 → false；初值 false
P_recovery_actual = u_recovery × max(0, min(100, 500·N_dispatch − P_load_filt))
P_gen_req_baseline = P_load_filt + P_recovery_actual
```

输入 P_load_gen（已换算）、SOC、N_dispatch（dispatch 反馈，前一拍——经 UnitDelay
切断拍内环）；输出 P_gen_req_baseline + u_recovery 状态（供 logging）。全部量在
canonical 域。LPF 离散化用前向欧拉（Ts=0.01 ≪ τ=60，数值无条件稳定；documentation
注明非双线性，避免与 Plant 其他滤波器混用口径）。

### 5.4 DG_Commitment_Supervisor（FR-436~440，Stateflow）

**实现形态**：Stateflow Chart（**离散**，ChartUpdate=DISCRETE，Ts=0.01 s），
**显式整数 tick 计数器**实现全部 timer（与 GensetUnitE 的 OQ-6 整数 tick 判据同风格；
不用 `after()` 时序算子，避免语义歧义）。状态图：

```text
ONE_DG_NORMAL ──[P_commit>425 持续 3000 tick]──► DG2_START_PENDING（等待 Plant 进 ON）
DG2_START_PENDING ──[state_DG2==ON]──► TWO_DG_NORMAL
TWO_DG_NORMAL ──[P_commit<350 持续 12000 tick ∧ online_timer≥30000]──► ONE_DG_NORMAL
+ Safety overlay（迁移优先级自上而下 = 仲裁顺序）：
  fault veto：fault_latch_i → 禁 START i / 强制 STOP i（最高）
  safety：force_start_dg2_req / force_stop_dgi_req（绕过全部 timer，中）
  normal：上述阈值迁移（最低）
+ 计时器（并行，D-B5）：start_hold_timer / stop_hold_timer / online_timer
  均为独立 tick 计数，条件各自连续满足才累计（违反即清零，strict reset）
```

- **min_down=0**：OFF 后无额外计时，start_hold 满足即可重启；
- **aborted-start**（A2 契约延续）：DG2_START_PENDING 期间负荷回落不打断 Plant 的
  25+5 s 物理过程；chart 记录 pending_stop 语义交由 Plant 状态机既有行为；
- **审计输出**（FR-440）：每次迁移拍输出刷新 `{timestamp, old_state, new_state,
  reason_code, P_commit, start_hold_timer, stop_hold_timer, online_timer,
  safety_override}` 九元组信号组（迁移后保持至下次迁移），经 logging 记录，
  Gate 后处理提取迁移表；
- **reason_code 枚举**（1..N 整型常量）：1=NORMAL_HIGH_LOAD、2=NORMAL_LOW_LOAD、
  3=SAFETY_DG_FAULT、4=SAFETY_REALIZED_SUPPLY_DEFICIT、
  5=SAFETY_PROSPECTIVE_FIRM_CAPACITY_DEFICIT、6=SAFETY_MIN_GEN_SURPLUS、
  7=FORCED（force 通道，DD-8；43 正常 Gate 中出现即 FAIL——兼作负向验证向量）；
- **S5 fallback** 不产生 commitment transition：Source_Selector 侧记录
  `baseline_fallback_active` + `baseline_fallback_reason` 事件信号。

### 5.5 DG_Dispatch_Allocator（FR-441~443）

MATLAB Function 块（离散 0.01 s），源码 `+model/code_dispatch_e.m`：

```text
输入：P_gen_req_eff、DG1/2_state、DG1/2_dispatch_eligible（commitment/fault 裁决后）
N_dispatch = #(state_i==ON ∧ eligible_i)；STARTING/SYNC/COOLDOWN/OFF 不计
若 N_dispatch==0（不应发生于 voyage active）：P_ref=0 双机 + 断言 flag（防御）
P_raw = P_gen_req_eff / N_dispatch
P_i_ref = eligible_i ? clamp(P_raw, 125, 500) : 0
诊断：P_gen_unserved = max(0, P_req − ΣP_i_ref)
      P_gen_surplus  = max(0, ΣP_i_ref − P_req)
      dispatch_min_clamp_active = any(eligible_i ∧ P_raw<125)
```

`DISPATCH_MODE` 仅在块注释与接口文档登记（0=AGGREGATE_AUTO 实现 / 1=EXTERNAL_PER_UNIT
保留），**不写代码分支**（D-B1/FR-443）。

### 5.6 16_Safety_Interlock（FR-444~448）

MATLAB Function 块（离散 0.01 s），源码 `+model/code_safety_interlock_e.m`。
**不直写 Plant**；输出 override 请求总线经 **UnitDelay（0.01 s）** 回 07_PMS_v2
（commitment 与 selector 的高优先级输入），切断 dispatch→safety→commitment 的
拍内直接通路（NFR-417 无新代数环；0.01 s 决策延迟对安全判据无影响）。

**S1 DG fault 检测**（D-B7 修正后语义，非字面 state≠command）：

```text
S1a 非预期丢失：state_i 进入 COOLDOWN/OFF（自非 OFF）∧ cmd_onoff_i==1 ∧
                无 force_stop_i 在令 → fault_latch_i = 1
S1b 启动失败：state_i ∈ {STARTING, SYNC} 连续 > 40 s（start_fail_window）→ fault_latch_i = 1
fault_latch_i：锁存至仿真结束（v1 无 run 内复位通道，登记 future extension）
响应：fault_latch_i → 该机组 dispatch_eligible=0 + veto START（Commitment 最高优先级）；
      另一机 OFF∧healthy∧航次激活 → force_start 该机
```

**S2**：`P_gen_unserved > P_bat_dis_avail_g` 连续 200 tick（2 s）→ force_start 请求
（目标 = healthy∧available∧offline 机组；无目标则 flag-only）。
**S3**：`N_dispatch·125 − P_load_gen > P_bat_chg_avail_g` ∧ `N_healthy_ON ≥ 2` →
force_stop_dg2；单健康机时 flag-only + `minimum_generation_surplus_unresolved=1`
（known out-of-scope，FR-445）。
**S4**：`D_firm = (P_prop_dem+P_hotel)/η_rect + 50 − (N_ON_healthy·500 +
P_bat_dis_avail_g) > 0` 连续 200 tick → force_start 请求（语义同 S2 目标选择；
capacity adequacy 判据，不检查 ramp 可实现性）。
**S5**：`ems_health_ok = EMS_HEALTH_TS(t) ∧ ¬isnan(P_gen_ref_kW)`；`ems_health_ok==0`
→ `baseline_fallback_active=1`（锁存，v1 不自动恢复）+ `baseline_fallback_reason`
事件码 + selector 切 BASELINE_POLICY。**v1 不做 temporal freeze 检测**（legacy 变体可
合法输出常数；冻结检测待 F 包真实预测器接入时定义契约——登记）。

**纯监视 flag**（FR-446）：soc≤0.20、Vdc<600、电池三 flag 透传；全部入 logging。
**全 override/flag 事件入日志**（VI-1 唯一版本，D-B7）。

### 5.7 既有保护链关系（T-417 纳入而非复制）

`08_Undervolt_Derate`（675→600 线性降额）保持**唯一执行性 propulsion limiter**——
16_Safety_Interlock 与 08 无任何信号直连，两者作用通道完全分离（08→推进限功率；
16→机组启停/source 回退/flag）。`09`/`15`/ECM flag 作为 16 的监视输入复用。

### 5.8 Logging 扩展（DD-10）

`configureLogging` 新增源端口打点：07 的 DGx_enable、P_x_ref、commitment_state、
reason_code、三 timer、P_gen_req_eff、P_gen_unserved/surplus、min_clamp_active、
u_recovery、P_recovery_actual；16 的 fault_latch×2、force 请求、S4 D_firm、
baseline_fallback_active/reason、监视 flags；14 的 P_bat_chg_avail_kW（补盲区）。
全部在信号**源端口**打点（A2 既有机制）。

## 6. 43 号控制 Gate 设计（FR-452~456）

### 6.1 结构与入口

`scripts/43_run_modelE_control_gate.m`（`eval(fileread(...))` 入口，Model E 根执行）：

```text
Phase 0: 先执行 40_run_modelE_gate（T-412 重跑，overall=true 才继续；否则 GATE:* 退出）
Phase 1: 构建（build_model_e）+ 结构指纹/配置 manifest 更新校验（§7 DD-9）
Phase 2: AUTO 工况矩阵 × 三臂（S20/S01/顺水/逆水/S31/S34/S36 × R0/R5/P2）
Phase 3: MANUAL M1/M2/M3 × P2
Phase 4: Safety 注入场景 S1~S5 各一（确定性 bench，gate_assign_bench_inputs 扩展）
Phase 5: 负向验证（构造非法 transition/应触发未触发 override → 断言正确 FAIL）
输出：results/gate_control_result.json（overall + per-check + arms 对比表 + 审计摘要）
```

三臂配置（编译期变量，assignin 注入）：R0 = {EMS_CTRL_LAW=0, PMS_SOURCE_MODE=
EMS_AGGREGATE}；R5 = {law=5, EMS_AGGREGATE}；P2 = {PMS_SOURCE_MODE=BASELINE_POLICY}
（law 值无关，selector 不消费 EMS 输出）。**三臂 Safety 恒在**（FR-452 修正）。

### 6.2 判据实现映射

| FR-455 判据 | 实现 |
|---|---|
| 确定性 | 每工况同配置两跑，logsout 逐位比较（G6 口径函数复用） |
| 规则合法性 | 审计表逐迁移断言：reason 合法 + timer 饱和值合规（start≥3000 tick / stop≥12000 tick ∧ online≥30000 tick，SAFETY_*/FORCED 豁免） |
| episode 一对一 | §6.3 |
| SOC 行为 Gate | P2/S20：abs(SOC_end−0.60)≤0.05 |
| Vdc 分层 | 正常场景 min(Vdc)≥600；注入场景逐场景断言集（expected trigger/flag/recovery/no-NaN） |
| 能量闭合 | G5 函数复用（<0.1%） |
| S1~S5 断言 | 每注入场景：expected override 发生 ∧ 非预期 override 零发生 |
| transition 审计 | 迁移表无非法 reason_code、无缺失字段 |
| T-412 | Phase 0 |

### 6.3 episode 分割算法（DD-7）

Gate 后处理（纯 MATLAB，审计日志输入）：

```text
1. 重建 normal_start_timer 时间序列（0.01 s 网格）；
2. eligible high-load episode = 该 timer 的饱和事件（从 <3000 到 =3000 tick 的穿越）计数；
   （stop 迁移要求 P_commit<350 持续 120 s ⇒ 两次正常 start 之间必有 timer 清零 ⇒
    每次正常 start 唯一对应一个饱和事件）
3. 断言：N_starts(reason==NORMAL_HIGH_LOAD) ≤ N_saturation_events；
4. 每正常 start 断言其审计记录 normal_start_timer==3000（饱和）且 reason 合法。
```

### 6.4 指标计算（FR-454）

由 logsout 后处理：fuel_total（10 号既有输出终值）、start 计数（审计表 reason 分列
normal/safety/forced）、online_time（commitment_state 积分）、双机低负荷 dwell
（TWO_DG ∧ P_commit<350 的累计时间）、SOC/Vdc 极值与越限时长、battery throughput
（P_bat_bus 充/放分列积分）、unserved/surplus 能量、override 计数。结果写入
`results/gate_control_result.json` 的 per-arm/per-scenario 节。

### 6.5 MANUAL 工况电平（FR-431 门槛化落案）

计算口径（canonical 域，P_hotel=100 kW nominal，η_rect=0.97，Mot_P_nom=700）：

```text
P_commit(u) = (u×700 + 100)/0.97
u_low  = 0.30 → P_commit = 319.6 kW < 350 ✓（必然停 DG2）
u_high = 0.70 → P_commit = 608.2 kW > 425 ✓（必然启 DG2）
```

- **M1**（step）：0.30→0.70 @ t=30 s，总长 300 s（验证方向/rate limiter/电池瞬态）；
- **M2**（ramp）：0.20→0.80 斜升 60 s（247→680 kW），总长 300 s；
- **M3**（staged）：0.30（0–120 s）→ 0.70（120–360 s）→ 0.30（360–900 s）；
  时序推演：high 段 LPF 穿越 425 ≈ 28 s + hold 30 s → DG2 ON ≈ 178 s（段内）；
  low 段 LPF 跌破 350 ≈ 135 s + stop hold 120 s 且 min_up 300 s 并行 ⇒ 停机遇
  ≈low 段 240~300 s 处发生，段长 540 s 足够观察完整 1DG→2DG→1DG 链 + COOLDOWN。
  实施时以 bench 实测校准段长（校准只动段长，不动电平与阈值）。

## 7. Gate 基线维护（DD-9，🟡 用户批准项）

B 包结构性变更（01/02 改名、07 替换、16 新增、logging 扩展）使 A2 冻结的
`gate_baseline_modele.json` v1.0 结构指纹（687 blocks/695 lines）与 config manifest
sha256 必然变化。处置：

1. B 实施完成后**重建**基线为 **v1.1**：仅结构指纹节与 config manifest 节更新；
   **G2~G6 物理判据阈值零改动**；Model D 参照哈希节零改动；
2. T-412「重跑 PASS」的零回归证据 = 物理判据（G2~G6 + E01~E10 bench）在新结构上
   全部通过且 bench 注入语义不变（DD-8 保证）；
3. 基线重建属 🟡 操作，在 T-418 用户评审门随 43 Gate 结果一并报批；
   控制基线 `gate_baseline_modele_control.json` 同场冻结 v1.0（FR-456 纪律）。

## 8. 参数文件（DD-2）

新建 `config/ModelE_Control_Parameters.m`（builder 与全部 Gate/仿真入口在
`ModelE_Physics_Parameters.m` 之后运行；单测按需单独运行）。内容（全 ENGINEERING_
SURROGATE，除注明外）：

| 变量 | 值 | 说明 |
|---|---|---|
| PMS_LoadLPF_tau_s | 60 | D-B3；注册 {30,120} 不扫 |
| PMS_SOC_lo / PMS_SOC_recovered | 0.55 / 0.60 | D-B3 |
| PMS_P_chg_recovery_kW | 100 | D-B3 |
| PMS_alpha_start / T_start_hold_s | 0.85 / 30 | D-B4；LITERATURE-ANCHORED ENGINEERING_SURROGATE（Siemens 门槛×Perabo 延迟组合） |
| PMS_alpha_stop / T_stop_hold_s | 0.70 / 120 | 同上 |
| PMS_min_up_s / PMS_min_down_s | 300 / 0 | D-B5；注册 min_up∈{0,300,600} |
| PMS_firm_margin_kW | 50 | D-B9；注册 {0,100} |
| PMS_S2_hold_s / PMS_S4_hold_s | 2 / 2 | D-B7/D-B9 |
| PMS_start_fail_window_s | 40 | D-B7（>25+5 s 物理时序 + 裕量） |
| Manual_Cmd_RateLimit_pct_s | 20 | DD-3 |
| PMS_SOURCE_MODE / VOYAGE_CTRL_MODE | 编译期选择器 | BASELINE_POLICY/EMS_AGGREGATE；AUTO/MANUAL |
| EMS_HEALTH_TS / GEN_CMD_STOP_TS 等注入表 | 场景数据 | From Workspace + Interpolate=off |

## Testing Strategy（测试策略）

单测（A2 模式：块源码字符串与单测同源，纯 MATLAB）：

| 测试 | 覆盖 |
|---|---|
| `tests/test_pms_demand_e.m` | LPF 阶跃响应（τ 校验）、滞回进出含等号边界、削减公式（max(0,…) + 单/双机容量档）、初值 |
| `tests/test_dispatch_e.m` | 均分、clamp 125/500、N_dispatch 排除 STARTING/SYNC/COOLDOWN、诊断三件套公式、N=0 防御 |
| `tests/test_safety_interlock_e.m` | S1a/S1b/S2/S3（含单健康机 flag-only）/S4/S5 触发与不误触发、监视 flags、UnitDelay 拍序 |
| `tests/build_commitment_harness.m` + `tests/test_commitment_chart_e.m` | Stateflow 脚本化 harness（禁 GUI）：阈值/hold/滞回驻留/并行计时最短 ON=恰 300 s/min_down=0 即时重启/仲裁顺序 veto>safety>normal/审计九元组完整性 |
| 43 Gate | §6 全矩阵 + 负向验证（`scripts/44_run_control_gate_negative.m`，42 号模式） |

## 10. R2026a 实施注意（NFR-415 落案）

句柄式 addL 接线（子系统内 MATLAB Function 块）；persistent 块 EMChart DISCRETE；
From Workspace 注入表 Interpolate=off；43/44 数字开头脚本 eval(fileread) 入口；
禁 model.ensure_canonical；注入经 assignin 通道；regexp `.*` 跨行；struct() cell 展开。

## 11. 风险与缓解

| 风险 | 缓解 |
|---|---|
| Stateflow chart 单测环境复杂 | 脚本化 harness（build→sim→assert），纳入 43 Phase 1 前置 |
| force 通道破坏 legality 判据 | FORCED reason 即 FAIL；T-412 bench 依赖 force 通道 ⇒ DD-8 保名兼容 |
| T-412 bench 经 adapter force 通道（gate_assign_bench_inputs） | DD-8：07_PMS_v2 保留 `ADAPTER_FORCE_MODE/P1/P2` 同名常量通道与等价语义（PMS_FORCE 别名注释）；bench 结果应逐位一致——Phase 0 实测验证，不一致即停 |
| S2/S4 的 2 s 判据在 SOP 噪声边沿抖动 | SOP 信号为 ECM 确定性输出（无噪声源）；200 tick 连续判据 + 触发后锁存语义 |
| 双域换算遗漏 | FR-451A + X_b2g_* 显式块名 + 43 结构断言（conversion 块清单检查） |

## 12. 偏差登记

| # | 内容 | 理由/来源 |
|---|---|---|
| D-B10 | 07_PMS_v2 保留 `ADAPTER_FORCE_*` 变量名（adapter 已退役） | T-412 bench（gate_assign_bench_inputs）零改动兼容；命名遗留作注释说明 |
| D-B11 | S5 v1 无 temporal freeze 检测、fallback 锁存不自动恢复 | legacy 变体可合法输出常数；契约待 F 包真实预测器定义（brainstorm D-B7 框架内细化） |
| D-B12 | 12_Data_Logging mask 文本修正（build_model.m→build_model_e.m） | delegate 核查发现的文档瑕疵，随 B 包顺手修正（行为零变化） |
| D-B13 | `gate_baseline_modele.json` 结构指纹/manifest 节重建为 v1.1 | B 包结构变更必然；物理判据节零改动；🟡 用户评审门报批（§7） |

## Requirements Traceability

| Requirement | Design Decision | Validation |
|---|---|---|
| FR-428/429/430 | §5.2 双模式改组 + §4 决策表 | AC-416 |
| FR-431 | §6.5 门槛化电平/段长 | AC-416（M3 确定性 1DG→2DG→1DG） |
| FR-432~435 | §5.3 code_pms_demand_e | test_pms_demand_e + AC-417 |
| FR-436~440 | §5.4 Stateflow chart + 审计九元组 | test_commitment_chart_e + AC-418 |
| FR-441~443 | §5.5 code_dispatch_e | test_dispatch_e + AC-419 |
| FR-444~448 | §5.6 code_safety_interlock_e + §5.7 | test_safety_interlock_e + AC-420 |
| FR-449/450 | §5.1 顶层接线 + source selector | AC-421（七变体编译 + law=0/5 短程） |
| FR-451/451A | §8 参数文件 + §3 canonical 域 | AC-421 + 43 结构断言（X_b2g_* 清单） |
| FR-452~456 | §6 全节（43 Gate/判据/episode/指标/电平）+ §7 基线纪律 | AC-422（43 overall + T-412 重跑 + 负向） |
| FR-457 | §2 附带发现 + 归档任务 | AC-423 |
| NFR-412/413 | §1 不动清单 + §6.1 Phase 0 | T-412 重跑 + G1 |
| NFR-414/415/417 | §5.4~5.6 离散实现 + §10 | 结构指纹幂等 + 确定性判据 |
| NFR-416/418/419 | §8 provenance + §11/§12 | 绝对路径扫描 + 评审门 |

## Risks and Trade-offs

见 §11。核心权衡：canonical 域选 genset-side 以保全冻结阈值的字面性，代价是输入边界
三处显式换算（以 X_b2g_* 块名与结构断言兜底）；Stateflow 唯一写入者 + UnitDelay 防环
牺牲 0.01 s 决策即时性（对安全判据无影响）换取无代数环确定性。

（实施期新增偏差按 D-B14+ 续登。）

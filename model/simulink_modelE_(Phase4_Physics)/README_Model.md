# README_Model.md — Model E（Phase 4 Physics）工作区说明书

> spec：`.specs/phase4-a2-model-e-construction`（四门禁 2026-09-04 用户批准）+ **`.specs/phase4-b-pms-safety`**（B 包四门禁 2026-09-04 批准；分支 `feat/phase4-b-pms-safety`）
> 参数证据链：`docs/phase4/a2_model_e/modelE_parameter_evidence.md`；Model D 版本历史见
> `model/simulink_modelD_(EMS_NN_AECMS)/README_Model.md`（本文件只描述 Model E 差异与现状）。
> B 包控制侧证据：`docs/phase4/b_pms_safety/`（三臂报告/裁决链/基线 diff）。

## 1. 用了什么 Simulink 模型

- **`model/HybridShip_ModelE.slx`**——Model E 主模型（builder `scripts/+model/build_model_e.m`
  全脚本驱动生成，禁 GUI 修改）。Model 体系 = **Model D 复制分叉 + Plant 物理升级**
  （Phase 4 工作包 A 第二段 T-403~T-411 施工，T-412 Gate 验收）。
- 配套变体：无（PlantStep/HIL 实时化推迟后续 Phase；`HybridShip_ModelE.slx` 为唯一正式模型）。
- 500 t representative hull 口径（K_R=890、几何 53.45/7.13/2.188/C_B=0.60；`target_vessel_confirmed=false`）；
  **与 Model D 绝对量不可比是设计意图**。

## 2. Simulink 模型结构（顶层 16 块，由总到分）

顶层功率流（B 包 T-007 后）：`01_Operator_Interface` → `02_Propulsion_Controller` →
`03_Propulsion_Plant` → `04_Load_Measurement` → `13_DC_Bus`（750 V 聚合单母线）⇄ `14_Battery_Converter`；
`05_EMS_Controller`（七变体）→ **`07_PMS_v2`（四层 PMS + source selector，替换 Legacy_Commitment_Adapter）**
→ `06_Genset_System` → 母线/燃油；**`16_Safety_Interlock`（独立 Safety 层，override 经 UnitDelay
0.01 s 回 07）**；`08_Undervolt_Derate`、`09_Bus_Residual_Monitor`、`10_Fuel_Consumption`、
`11_Voyage_Stop_Logic`、`12_Data_Logging`、`15_Bus_Voltage_Monitor`。

关键子系统内部（Model E 相对 Model D 的变更全部标注 ★；B 包变更标 ★B）：

- **`01_Scenario_Manager`** ★B **改组为 `01_Operator_Interface`**（FR-428，T-006）：既有航次/环境输出
  （v_current/resistance_multiplier/P_hotel/scenario_id/water_depth_m）不变；新增驾驶意图输出
  `ctrl_mode`（VOYAGE_CTRL_MODE 编译期，0=AUTO/1=MANUAL）、`v_ref_mps`（原 v_cmd_mps 改名）、
  `manual_power_cmd_pct`（VOYAGE_MANUAL_CMD_TS From Workspace+Interpolate off，支持 M1/M2/M3 序列）。
- **`02_Propulsion_Controller`** ★B **改组自 02_Ship_Speed_Controller**（FR-429，T-006）：AUTO 路径
  PI 船速环（Kp/Ki 抗饱和，参数/结构零改动）原样保留；MANUAL 路径 `P_ref = manual_power_cmd_pct ×
  Mot_P_nom`（归一化线性映射）→ command-side Rate Limiter（140 kW/s，与 DG 20 kW/s 物理 ramp
  独立）→ Switch（ctrl_mode 编译期选路）→ 同一 Saturation → 唯一输出 `P_prop_demand_kW`；
  禁 RPM 环/torque 域（T-413 决策表见 spec design §4）。
- **`03_Propulsion_Plant`** ★ 核心施工区：
  - `Ship_Hull_Dynamics_E`（替代 CODE_HULL）：R_deep = res_mult·K_R·V_rel|V_rel| 锚定 +
    **浅水相对增量 Δr**（Δk=0.644(H/T)^−1.72 Millward[4] + C_F* 修正 c1/c2/c3 Zhang[1][3]；
    H/T>3 深水渐近 Δr=0；H/T<1.2 clamp+MODEL_DOMAIN_WARNING）；输出 dVdt/R_total/R_shallow/
    **HT_ratio（H/T 单一事实源输出）**/domain_warning。
  - `Wake_vs_HT` ★ 新模块：w = w_deep(0.10, Molland 回归 D-3) + Δw_Zhang(H/T)（相对增量、
    linear 插值 clamp [1.2,2.7]）；输入=hull HT_ratio 经 **Unit Delay**（NFR-409 代数环松弛，
    水深工况内恒定物理零影响）。
  - `Propeller_Hydrodynamics`：简化二次模型保持（F=K_Fw·ω|ω|−K_Fv·V_A|V_A|），**wake_fraction
    由 Parameter 改输入信号 w_signal**；四 K 按 w_deep=0.10 重标定（D-8：K_Fw=128.107/
    K_Fv=1099.02/K_Qw=37.410/K_Qv=160.47，设计点推力复原 F≈57005 N）；D/K_T/K_Q/P/D
    **INTENTIONALLY UNRESOLVED**。
  - 转矩链/轴系/齿轮结构继承（Gear_Ratio=6.08、Gear_Eff=0.96 D-1）。
- **`04_Load_Measurement`**（KEEP）：P_load = max(P_prop,0) + P_hotel。
- **`05_EMS_Controller`**（KEEP，七变体 Variant Subsystem 不动）：law=0 Baseline / 1 AI_Enhanced /
  2 ScheduledFF / 4 FixedECMS / 5 SOC-Adaptive / 6 Oracle / 7 NN-AECMS；输出仍为聚合
  P_gen_ref_kW（FR-426 接口契约）。
- **`07_PMS_v2`** ★B **新模块（替换 Legacy_Commitment_Adapter，FR-449/450，T-007）**：四层 PMS +
  选择器——`X_b2g_*` 输入边界转换（canonical=genset-side，DD-1：÷η_rect=0.97 三处）→
  `PMS_Demand_Policy`（MATLAB Function 离散 0.01 s：LPF τ=60 s + SOC 滞回 0.55/0.60 +
  non-firm 削减，N_dispatch 前一拍 UD 反馈）→ `Source_Selector`（PMS_SOURCE_MODE 编译期 ∨ S5
  runtime override）→ `PMS_Commitment_Supervisor`（Stateflow chart，离散 0.01 s，整数 tick
  计时器，迁移优先级 veto>safety>normal，审计九元组，reason_code 1~7）→
  `PMS_Dispatch_Allocator`（均分 clamp[125,500]，is_dispatching 单一谓词，诊断三件套）→
  输出级（AUTO：enable=cmd_onoff/ref=dispatch/stop=[1−onoff]；force: A2 语义逐位兼容
  ADAPTER_FORCE_*，DD-8/D-B10）。`DG_cmd_stop` [1×2] 停机向量 → 06/5（D-B16）。
- **`16_Safety_Interlock`** ★B **新模块（FR-444~448，T-007）**：S1a 非预期丢失/S1b 40 s 启动失败/
  S2 realized supply deficit（D-B14 口径）/S3 min gen surplus（单健康机 flag-only）/S4 prospective
  firm-capacity + S5 EMS health fallback（EMS_HEALTH_TS From Workspace + isfinite，仅
  EMS_AGGREGATE 报警）+ 纯监视 flags（SOC≤0.20/Vdc<600/电池三 flag）；不直写 Plant，override
  经 8 根 UnitDelay 0.01 s 回 07（拍序恰一拍实测背书）；与 08 无信号直连（08 保持唯一推进
  限制器）。
- **`06_Genset_System`** ★（替代 06_Genset_Average_Plant）★B：DG1/DG2 各 500 kW 独立 Plant，
  每机组 `GensetUnitE`（MATLAB Function persistent，**离散 0.01 s 采样**）：
  状态机 OFF→STARTING(2500 tick/25 s)→SYNCHRONIZING(500/5 s)→ON→COOLDOWN(3000/30 s)→OFF
  （整数 tick 判据 OQ-6；aborted-start 不可中断+pending_stop；**OFF 态 stop 优先**（2026-09-04
  主 Agent 裁决）；无 min up/down（D-4））；ON 态 ref clamp[P_min=125, 500]+below_min_flag；
  爬坡 20 kW/s + 一阶 τ=5 s；P_gen_actual=P1+P2。★B 停机通路：In5（DG_cmd_stop）激活，
  内部 Max 合成（stop = max(注入表, 07 外部停机向量)）——状态机/功率链零改动（D-B16）。
- **`08_Undervolt_Derate`**（KEEP）：CCS 2026 欠压降额保护链（E09 验证生效）。
- **`09_Bus_Residual_Monitor`**（KEEP）：P_residual=P_gen·η_rect+P_bat−P_load（G5 能量闭合基础）。
- **`10_Fuel_Consumption`** ★ 重构：`FuelModelE` per-genset——双路独立 BSFC PCHIP
  （节点 {125:600, 250:547, 375:447, 400:427, 500:453} g/kWh，[11] 形状×[16] 427 锚定），
  域守卫（ON 且 P≥125 才查表，禁止外推）；STARTING/SYNC 均摊 C_start=0.6 kg（只算一次）；
  OFF/COOLDOWN=0；三个 Integrator → fuel_1/fuel_2/fuel_total_kg。
- **`11_Voyage_Stop_Logic`** `12_Data_Logging` `13_DC_Bus` `15_Bus_Voltage_Monitor`（KEEP，继承）。
- **`14_Battery_Converter`** ★ 重构：droop grid-forming 保留 → `BatteryECM`（MATLAB Function
  persistent V_1，离散 0.01 s，20 入参全端口）：LFP 1RC——V_t=OCV(SOC)−I·R_0(SOC)−V_1、
  V̇_1=−V_1/(R_1C_1)+I/C_1；**解析求 I**（OQ-9：I=2P_t/(E+√(E²−4R_0P_t) 小根，无迭代无代数环）；
  D<0 → P_actual=P_max,R + 双 flag（power-limit semantics）；R_0 充放电共用（乘子置 1.0，A2 D-14①）；
  **SOC 电荷积分**（放电 −I·dt/Q、充电 −η_coul·I·dt/Q，η_coul=0.98 [14]，禁能量积分 Q5）；
  LUT 域 [0.1,0.9] 域外 clamp+`ecm_lut_boundary_flag`；**SOP 四约束** P_avail=min(P_current,
  P_voltage, P_SOC, P_converter=1050 kW bus-side)（`+model/battery_sop_limits.m` 逐字内联）；
  新输出 V_bat_V/I_bat_A/三 flag。

## 3. EMS 控制逻辑（A2 继承部分不修改；B 包新增控制侧）

### 3.1 EMS 七变体（INHERITED，零修改）

七变体语义与 Model D 完全一致（law 语义/EMS_WORK_MODE 手动半自动全自动/k_eq 折算）；
ECMS 内部 BSFC 仍用聚合旧表（INHERITED_LEGACY），与 Plant 侧 per-genset PCHIP **并存双口径**——
EMS 内部口径不动（FR-426/D-9），law=7 权重经 `config/ecms_nn_weights.mat`（extrinsic 运行期
load，缺失时 ê=0≡law=5 回退）。
★ SOP 信号升级：`P_bat_dis/chg_avail` 端口**同名同义**（值从 SOC 降额升级为四约束 min），
对 EMS 透明（FR-424）。

## 3A. PMS v2 控制逻辑（★B，FR-432~450）

- **Baseline_Demand_Policy**（FR-432~435）：`P_gen_req = LPF_60(P_load_gen) + u_recovery·100`；
  SOC 滞回 `<=0.55` 进 / `>=0.60` 出（含等号，初值 false）；recovery 削减
  `max(0, min(100, 500·N_dispatch − P̄_load))` 仅在 Baseline source 内部；
  `P_commit = P̄_load`（不含恢复偏置，D-B4 方案 b）。
- **Source_Selector**（FR-450）：`source_effective = (ems_health_ok==0) ? BASELINE_POLICY :
  PMS_SOURCE_MODE`（编译期 nominal + S5 runtime override）。
- **Commitment**（FR-436~440）：ONE_DG_NORMAL ⇄ DG2_START_PENDING ⇄ TWO_DG_NORMAL；
  正常 start=P_commit>425 持续 30 s；正常 stop=P_commit<350 持续 120 s ∧ t_ON≥300 s
  （并行计时 AND 汇合）；仲裁 veto>safety>normal；审计九元组每次迁移记录
  （reason_code 1~7，7=FORCED 仅 bench force 通道语义，正常 Gate 零出现）。
- **Dispatch**（FR-441~443）：`P_i,ref = clamp(P_gen_req_eff/N_dispatch, 125, 500)`；
  `is_dispatching = (state==ON) ∧ eligible`；诊断 unserved/surplus/min_clamp（纯诊断，D-B14）。
- **Safety/Interlock**（FR-444~448）：恰五条执行性 override——S1a/S1b 故障锁存（非字面
  state≠command）、S2 realized supply deficit（realized 口径，D-B14）、S3 min gen surplus
  （单健康机 flag-only）、S4 prospective firm-capacity（commanded 口径 + 50 kW margin）、
  S5 EMS health fallback（显式通道 + isfinite，仅 EMS_AGGREGATE 模式报警）；08 号保持唯一
  执行性推进限制器。
- **canonical 功率域 = genset-side（generator-equivalent，DD-1）**：P_load/P_bat_avail 在
  07_PMS_v2 输入边界三处 `X_b2g_*` 显式 ÷η_rect=0.97 换算，禁混域运算；EMS 聚合输出与
  Plant 接口零换算直通；措辞纪律：battery genset-equivalent ≠ 物理过 rectifier。

## 4. 预测模型清单

Model E 的 **Plant 侧无预测模型**（纯物理，确定性测试台直注指令）。EMS 七变体的预测组件
**完整继承 Model D**（未修改）：law=7 的 MLP-Q（302→128→64→1 权重外注入）、law=1/2 的
AI_Pred/TFF 通道、law=6 的 ECMS_EHat_Data oracle 通道——均 INHERITED_LEGACY，数值与形状
与 Model D 逐位一致；真实预测器接入研究属 **E 包**（B 包已建 PMS v2 接入面，F 包预测式
EMS 复用同一 Commitment/Dispatch/Safety 共享链）。
选择理由（选模依据）与 A1/A2 审计链不变：MLP-Q 为 Phase 2.5 闭环 Winner（TCN 换型臂行为
一致性已证）；Model E 的角色是**物理可信被控对象 + 真实控制 baseline（P2 臂）**，为 F 包
预测式 EMS 提供 "environment → physics → load" 因果链基座。

**Phase 4-F T-445（施工中）**：不接入任何 NN 预测器。B0=law=5，B1=law=6 完美 E300
标量 oracle，B2=同一 law=6 + Δλ 轨迹监督；oracle 真值来自冻结航次 P_load 流。E 段
winner（REAT-T）只读不消费。预测模型清单不变。Amendment C：J_FEC 的 k_eq^E=0.5457 kg/kWh
按标称 SOC kWh（1071.36·ΔSOC）折算，OCV 效率 0.9687 仅诊断；w_term=1.17e5 与 k_eq 同时冻结。

## 5. 验证状态

- **T-412 整体物理 Gate against candidate：overall=true**（2026-09-04，B 包 T-012 重跑，
  `results/gate_result.json`）；G1~G6 全过——巡航功率带 298.4 kW∈[274,335]、极速带 14.6 kn、
  浅水六档单调零反例、机组时序 25/5/30 s 精确、C_start=0.600 kg；**G6 为修复后真实逐位
  比较**（D-B19：E01b 前恢复 bench 注入 nominal）。force 模式 bench（E06/E07/E09）观测量
  与 A2 v1.0 逐位一致（DD-8 兼容证据，DD-9 M3）。
- **43 号控制层 Gate：overall=true**（2026-09-04，B 包 T-012，`results/gate_control_result.json`）：
  AUTO 7 工况×三臂 + MANUAL×P2 + 注入 S1~S5 + 确定性两跑，判据 C1~C7 共 133 项全过。
- **44 号负向验证：6/6 缺陷正确检出**（FORCED/timer/应触发未触发/确定性/episode）。
- 单元测试：A2 6 套（hull/wake/propeller/statemachine/fuel/ecm+sop）+ B 包 5 套
  （test_pms_demand/test_dispatch/test_safety_interlock/test_commitment_chart/t008 变体矩阵）ALL PASS。
- 基线：A2 v1.0 永久保留；**candidate v1.1 已生成（DD-9 机器检查 4/4），promotion 待 T-014
  用户书面批准**；`gate_baseline_modele_control.json` v1.0 待同场冻结。
# README_Model.md — Model E（Phase 4 Physics）工作区说明书

> spec：`.specs/phase4-a2-model-e-construction`（四门禁 2026-09-04 用户批准）；分支 `feat/phase4-a2-model-e`
> 参数证据链：`docs/phase4/a2_model_e/modelE_parameter_evidence.md`；Model D 版本历史见
> `model/simulink_modelD_(EMS_NN_AECMS)/README_Model.md`（本文件只描述 Model E 差异与现状）。

## 1. 用了什么 Simulink 模型

- **`model/HybridShip_ModelE.slx`**——Model E 主模型（builder `scripts/+model/build_model_e.m`
  全脚本驱动生成，禁 GUI 修改）。Model 体系 = **Model D 复制分叉 + Plant 物理升级**
  （Phase 4 工作包 A 第二段 T-403~T-411 施工，T-412 Gate 验收）。
- 配套变体：无（PlantStep/HIL 实时化推迟后续 Phase；`HybridShip_ModelE.slx` 为唯一正式模型）。
- 500 t representative hull 口径（K_R=890、几何 53.45/7.13/2.188/C_B=0.60；`target_vessel_confirmed=false`）；
  **与 Model D 绝对量不可比是设计意图**。

## 2. Simulink 模型结构（顶层 16 块，由总到分）

顶层功率流：`01_Scenario_Manager` → `02_Ship_Speed_Controller` → `03_Propulsion_Plant` →
`04_Load_Measurement` → `13_DC_Bus`（750 V 聚合单母线）⇄ `14_Battery_Converter`；
`05_EMS_Controller`（七变体）→ `Legacy_Commitment_Adapter`（PLACEHOLDER）→ `06_Genset_System`
→ 母线/燃油；`08_Undervolt_Derate`、`09_Bus_Residual_Monitor`、`10_Fuel_Consumption`、
`11_Voyage_Stop_Logic`、`12_Data_Logging`、`15_Bus_Voltage_Monitor`。

关键子系统内部（Model E 相对 Model D 的变更全部标注 ★）：

- **`01_Scenario_Manager`** ★ 新增 `water_depth_m` 输出（Out6，Constant=VOYAGE_WATER_DEPTH_M
  编译期求值、可 SimulationInput 逐航次注入）；resistance_multiplier 保留为通用不确定度旋钮
  （浅滩语义退役，D-7 不改名 Environment_Input）。
- **`02_Ship_Speed_Controller`**（KEEP，不动）：PI 船速环（Kp/Ki 抗饱和）→ P_prop_demand_kW。
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
- **`Legacy_Commitment_Adapter`** ★ 新模块（**PLACEHOLDER，FR-421 防线：不得用于任何正式
  EMS 经济性/节油结论**；B 包 PMS v2 直接替换）：聚合 P_gen_ref_kW → rule（≤500→仅 DG1；
  >500→DG1=500+DG2 余量）+ force 通道（scenario 强制组合测试用）。
- **`06_Genset_System`** ★（替代 06_Genset_Average_Plant）：DG1/DG2 各 500 kW 独立 Plant，
  每机组 `GensetUnitE`（MATLAB Function persistent，**离散 0.01 s 采样**）：
  状态机 OFF→STARTING(2500 tick/25 s)→SYNCHRONIZING(500/5 s)→ON→COOLDOWN(3000/30 s)→OFF
  （整数 tick 判据 OQ-6；aborted-start 不可中断+pending_stop；**OFF 态 stop 优先**（2026-09-04
  主 Agent 裁决）；无 min up/down（D-4））；ON 态 ref clamp[P_min=125, 500]+below_min_flag；
  爬坡 20 kW/s + 一阶 τ=5 s；P_gen_actual=P1+P2。
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
  D<0 → P_actual=P_max,R + 双 flag（power-limit semantics）；充电方向 R_0×1.15；
  **SOC 电荷积分**（放电 −I·dt/Q、充电 −η_coul·I·dt/Q，η_coul=0.98 [14]，禁能量积分 Q5）；
  LUT 域 [0.1,0.9] 域外 clamp+`ecm_lut_boundary_flag`；**SOP 四约束** P_avail=min(P_current,
  P_voltage, P_SOC, P_converter=1050 kW bus-side)（`+model/battery_sop_limits.m` 逐字内联）；
  新输出 V_bat_V/I_bat_A/三 flag。

## 3. EMS 控制逻辑（继承，本 spec 不修改）

- 七变体语义与 Model D 完全一致（law 语义/EMS_WORK_MODE 手动半自动全自动/k_eq 折算）；
  ECMS 内部 BSFC 仍用聚合旧表（INHERITED_LEGACY 待 B 包 PMS v2 重评），与 Plant 侧 per-genset
  PCHIP **并存双口径**——EMS 内部口径不动（FR-426/D-9），law=7 权重经
  `config/ecms_nn_weights.mat`（extrinsic 运行期 load，缺失时 ê=0≡law=5 回退）。
- ★ SOP 信号升级：`P_bat_dis/chg_avail` 端口**同名同义**（值从 SOC 降额升级为四约束 min），
  对 EMS 透明——rule-based 与全部变体共用同一 SOP 模块（FR-424）。
- ★ PLACEHOLDER 组合逻辑只在 Plant 侧拆分，EMS 不感知 P_min/双机（预期，law=0/5 兼容 smoke
  不作物理判据）。

## 4. 预测模型清单

Model E 的 **Plant 侧无预测模型**（纯物理，确定性测试台直注指令）。EMS 七变体的预测组件
**完整继承 Model D**（未修改）：law=7 的 MLP-Q（302→128→64→1 权重外注入）、law=1/2 的
AI_Pred/TFF 通道、law=6 的 ECMS_EHat_Data oracle 通道——均 INHERITED_LEGACY，数值与形状
与 Model D 逐位一致；真实预测器接入研究属 B/F 包。
选择理由（选模依据）与 A1/A2 审计链不变：MLP-Q 为 Phase 2.5 闭环 Winner（TCN 换型臂行为
一致性已证）；Model E 的角色是**物理可信被控对象**，为 B 包 PMS v2 与 F 包预测式 EMS 提供
"environment → physics → load"因果链基座。

## 5. 验证状态

- **T-412 整体物理 Gate：overall=true**（2026-09-04 夜班，`results/gate_result.json`）；
  G1~G6 + E10 + negative_validation 全部通过；巡航功率带 298.4 kW∈[274,335]、极速带 14.5 kn
  级平衡、浅水六档单调零反例、机组时序 25/5/30 s 精确、C_start=0.600 kg、燃油差 23.7%。
- 单元测试 6 套（hull/wake/propeller/statemachine/fuel/ecm+sop）ALL PASS。
- 基线冻结：**待 T-013 用户评审**（design §5.1 防 bugged model + bugged gate 互相背书）。
# Phase 4-A2 — Model E 施工段（T-403~T-412）Design

Spec: `phase4-a2-model-e-construction`  
Status: design-approved（2026-09-04 用户批准，GPT 评审修订 A2/A3/A4 + B1/B2/B3 + §三/§六/§十三采纳，A1 及一切 Model D 回归建议按用户裁决废弃，详见 §9 偏差登记 D-4/D-5）  
Created: 2026-09-04  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`

## Summary

A2 在 Model D 复制分叉工作区 `model/simulink_modelE_(Phase4_Physics)/` 中实施 Model E 物理升级。
核心设计思想：

1. **参数单一事实源**（OQ-8，用户裁决修订）：Model E 参数独立成套——
   `ModelE_Physics_Parameters.m` 是**完整可独立加载**的参数文件（继承值也逐条重新声明、
   标 INHERITED_* provenance），不运行时依赖 legacy 文件；复制分叉带入的 legacy config
   隔离至 `config/_legacy_reference/`（不入路径，仅证据对照）；sensitivity tier 由
   `ModelE_Sensitivity_Profile.m` 可选覆写；
2. **浅水 = 深水锚定 + 相对增量**（§4.1）：R_deep 保持 K_R=890 设计点锚定（用户裁决不动），
   Zhang/Millward 公式只负责计算**相对**浅水增量 Δr(H/T, Re)；
3. **双机 = 接口不变 + 占位适配**（§4.3）：EMS 七变体仍输出聚合 P_gen_ref_kW，
   新增 PLACEHOLDER 适配器拆分为 DG1/DG2 参考值，Plant 侧双机组各自带状态机；
4. **电池 = 无代数环解析解**（§4.5）：1RC ECM 电流由二次式解析求解，
   SOC 改电荷积分，SOP 四约束取 min；
5. **Gate = 物理断言而非基线回归**（§5）：Model D 继承的回归基线在 Model E 作废删除，
   T-412 用方向性/守恒性/边界/时序断言；Model E 自身基线经人工评审后才冻结（防
   bugged model + bugged gate 互相背书）。

## Goals and Non-Goals

### Goals

- FR-410~427 全部需求的施工设计定稿（追溯表见文末）；
- OQ-4~OQ-9 全部定稿（§3）；
- T-412 物理 Gate 六类判据与阈值（§5）；
- 施工任务分解到"另一个模型可无歧义执行"的粒度（→ tasks.md）。

### Non-Goals

- EMS 控制律、PMS v2、Safety 层（B 包）；工况生成器（C 包）；数据集（D 包）；
- 继承的 05/07 等数据集/AI 对比脚本在 Model E 的适配（D 包；A2 只保证
  02 build / 03 smoke / 40 gate 链路与七变体 compile）；
- 敏感性扫掠实验（除螺旋桨 ±20%）；bank/wind 阻力分量；2D 电机效率 map。

## 1. 资产布局与工作区结构

```text
model/simulink_modelE_(Phase4_Physics)/        # Model D 整个复制分叉（A1 Q-A2）
├── config/
│   ├── _legacy_reference/                     # 复制带入的 legacy config 隔离区
│   │                                          # （Model_Parameters/Model_Scenarios/
│   │                                          #  Model_GateThresholds/Ecms_Phase0_Config 等，
│   │                                          #  不入路径不参与运行，仅证据对照）
│   ├── ModelE_Physics_Parameters.m            # 新增：Model E 完整独立参数事实源（FR-411）
│   ├── ModelE_TestScenarios.m                 # 新增：Model E 测试工况（含浅水 H/T 六档）
│   └── ModelE_Sensitivity_Profile.m           # 新增：敏感性档位选择器（§2）
├── scripts/+model/build_model_e.m             # 由 build_model_c.m 复制演进（§4）
├── scripts/40_run_modelE_gate.m               # 新增：T-412 Gate orchestrator（§5）
├── scripts/41_run_prop_sensitivity.m          # 新增：螺旋桨 ±20% 扫掠（FR-417）
├── scripts/tests/                             # 新增：单元/集成测试 harness（§7）
└── model/HybridShip_ModelE.slx                # builder 生成（脚本驱动，禁 GUI）

docs/phase4/a2_model_e/
├── README.md                                  # §20 索引
├── modelE_parameter_evidence.md               # FR-427 参数证据链（值/来源/推导式）
└── prop_sensitivity_report.md                 # FR-417 敏感性报告（脚本渲染 + 结论保留块）
```

继承的 `gate_baseline_smoke.json` 随 legacy config 一并隔离至 `_legacy_reference/`：
其数值是 Model D 绝对口径，Model E 绝对量有意不同（Q3/Q4 裁决），留在运行路径会造成
误回归。隔离动作与理由写入 tasks 与提交说明。

## 2. 参数体系（OQ-8 定稿，用户裁决修订：独立成套）

加载结构（入口脚本统一）：

```text
ModelE_Physics_Parameters.m    % 完整独立参数事实源（Model E 所需全部参数）
ModelE_TestScenarios.m         % Model E 测试工况
ModelE_Sensitivity_Profile.m   % 可选：敏感性档位（nominal 档 = 全零改动）
```

**纪律**：

- `ModelE_Physics_Parameters.m` 完整自包含——**禁止**先 run legacy 参数文件再 override；
  继承值逐条重新声明并标 provenance（INHERITED_DRAWING / INHERITED_LEGACY / DRAWING /
  VENDOR / LITERATURE_NOMINAL / LITERATURE_RANGE / DERIVED / ASSUMED）+ 文献编号 [1]~[17]；
- 复制分叉带入的 legacy config（Model_Parameters.m / Model_Scenarios.m /
  Model_GateThresholds.m / Ecms_Phase0_Config.m / 既有 .mat 资产）隔离至
  `config/_legacy_reference/`，**不入 addpath、不参与运行**；EMS 变体内部所需参数
  （如 BSFC tier、k_eq 派生所需量）并入 Model E 参数文件，标 INHERITED_LEGACY +
  「待 B/F 包重评」注记；
- 顶部块注释：500 t representative hull 声明（`target_vessel_confirmed=false`）、
  推导值 ≠ 文献原值纪律、冻结 sensitivity 档位表（P_min {125/200/250} kW、
  ramp {10/20/50} kW/s、τ {2/5/10} s、C_start {0.5/1/2} kg）；
- manifest：每次 build/gate 写 `results/config_manifest.json`——Model E 三个 config
  文件 sha256 + 关键有效值快照 + `historical_reference_hashes`（Model D 四文件 sha256，
  证明历史资产未动）；每个 override 记录 parameter/base_value/effective_value/
  source_class/reference/profile_name。

override 清单（关键项，全表以参数文件为准；base_value 指 Model D 证据链中的原值）：

| 参数 | legacy | Model E nominal | provenance |
|---|---|---|---|
| Ship_Mass | 1.0e6 | 5.0e5 | DERIVED（500 t 工作假定） |
| Hull_K_R | 1780 | 890 | DERIVED（线性 Δ 缩放，用户裁决） |
| Hull_Lpp_m / Hull_B_m / Hull_T_m / Hull_Sw_m2 / Hull_CB | 不存在 | 53.45 / 7.13 / 2.188 / 554.2 / 0.60 | DERIVED+LITERATURE_NOMINAL |
| wake_fraction | 0.15 const | w(H/T) 信号化（§3 OQ-4） | 混合（Harvald+Zhang） |
| Gear_Eff | 0.96 | **0.96 不变**（见 §9 偏差登记 D-1） | INHERITED_LEGACY |
| Elec_Eff | 0.92 | 0.92 不变，provenance 升级 | LITERATURE_NOMINAL |
| ASSUMED_Bat_Eff_* | 0.95 | **保留定义**（ECMS k_eq 引用），Plant 侧停用（§4.5） | INHERITED_LEGACY |
| ASSUMED_Gen_MinStablePower | 0 | 125（kW/unit） | LITERATURE_NOMINAL |

## 3. Open Questions 定稿

### OQ-4：w/t LUT 形态（裁决）

**w(H/T) = w_deep + Δw_Zhang(H/T)**，其中：

- w_deep = **0.10**：Harvald 双桨数据回归（Molland/Turnock/Hudson 拟合式
  w_T=(0.71−2.39C_B+2.33C_B²)+0.12C_B⁴(6.5−L/B)，代入 C_B=0.60、L/B=53.45/7.13≈7.50
  → w_T≈0.099）；provenance 记录为该回归式而非 generic 典型值（LITERATURE_NOMINAL）；
- Δw_Zhang(H/T) = w_Zhang(H/T) − w_Zhang(2.7)，取 Zhang 2023 双桨表
  {2.7:0.22, 2.0:0.27, 1.8:0.23, 1.6:0.20, 1.4:0.26, 1.2:0.32} 的**相对变化量**
  （Δ 在 2.7 处为零），**linear 插值**（防 overshoot、物理可控）；
- 定义域 clamp 到 [1.2, 2.7]；H/T>2.7 → Δw=0（深水渐近）；
- **声明**：深度相关伴流修正为 transferred literature surrogate，
  不得表述为目标船经验证的伴流场（Zhang 标定族 C_B=0.86 ≠ 本船 0.60）；
- **Gate 不要求 w(H/T) 单调**（Zhang 原数据本身非单调）；单调性判据只作用于
  R_total(H/T)/P_required(H/T)（§5.3 G2）；
- w 从参数变为**信号**（输入 Propeller_Hydrodynamics），K_Fv 等系数仍在 11 kn 设计点、
  w=0.15 旧锚定推导——设计点推导值不变，w 的运行时变化由 T-412 推力平衡检查兜底；
- **t（推力减额）不引入为独立乘子**：其效应已隐含于设计点锚定链
  （P_eff = η_prop·P_shaft 的标定包含全部推进损失）；Zhang 的 t 表登记为
  LITERATURE_REFERENCE 入参数文件注释，不接线。理由：keep_simplified 下引入 t
  会与锚定链 double-count。

### OQ-5：T-412 Gate 阈值（定稿，§5 详述）

- energy closure：全程 |∫P_residual dt| / E_throughput < **0.1%**；稳态段 |P_residual| 均值 < 1 kW；
- 单调性/方向性：**零反例**（数值容差 1e-6 相对）；
- 极速带：静水全功率平衡航速 ∈ **[12.5, 15.5] kn**（期望 ≈14.5）；
- 11 kn 巡航电功率带：P_prop_elec ∈ **[274, 335] kW**（锚定链推得 304.5 kW ±10%）；
- 时序：STARTING 25±1 s、SYNC 5±0.5 s；
- 边界：SOC/V_bat/V_dc 硬边界零越限；solver 零失败、零 NaN/Inf；
- runtime：informational 记录（不做门禁）。

### OQ-6：启停状态机（定稿）

```text
OFF ──cmd_start──► STARTING(25 s) ──► SYNC(5 s) ──► ON ──cmd_stop──► COOLDOWN(30 s) ──► OFF
```

- **不加 WARMUP**（无独立物理参数支撑）；
- 状态机只表达 **plant physical dynamics**（命令→机组响应延时）；
  **min_up/min_down time 不属于 A 包 Plant**——那是 PMS/unit commitment 约束
  （B 包 T-415/T-416 职责），写入 Plant 会把控制策略偷进被控对象；
  机械 restart lockout 如确需保留须另有厂家依据，本 spec 不定义（FR-419 同步修订）；
- COOLDOWN 期间 P=0、fuel=0（标记 surrogate；登记局限：怠速燃油未计，无数据）；
- 启动燃油**只算一次**：C_start=0.6 kg 均摊于 STARTING+SYNC 30 s（方案 A event-level
  surrogate），STARTING/SYNC 不再另计 idle fuel——禁止双算；
- ON 态 P 域 [P_min=125, 500] kW；ref < P_min 时 clamp 到 P_min 并置 `dg_below_min_flag`；
- 实现：MATLAB Function block（persistent 状态 + 离散 0.01 s 采样），
  状态驻留用**整数 tick 计数**（N_start=2500、N_sync=500、N_cooldown=3000 ticks）
  做转移判据，禁止浮点时间比较（确定性、codegen-friendly）；
  不用 Stateflow（B 包 PMS 才引入）。

### OQ-7：Gate 脚本形态（定稿）

新建 `scripts/40_run_modelE_gate.m` orchestrator：每类判据一个独立 check 函数
（`scripts/+gate_e/check_*.m`），各输出 JSON fragment，最终聚合
`results/gate_result.json` = `{overall, timestamp, checks:{G1..G6}, manifest}`。
继承的 03/06 流水线保留可运行（作为 law=0 工况载具），但 **Model E 的提交门禁
= 40 号 Gate**；03 的基线回归判据在 Model E 不适用（§5.1）。

### OQ-8：参数加载结构（定稿，用户裁决修订）

见 §2：**单一事实源 + sensitivity profile**（非 legacy 三层继承）。补充纪律：
`_legacy_reference/` 内文件禁止被任何运行路径引用；Model D 目录本体零改动纳入 G1
（sha256 对照，证明历史资产未动）。

### OQ-9：电池两个冻结项（定稿）

1. **1050 kW = bus-side**（DC/DC 母线侧端口，图纸额定按设备输出端口惯例）；
   terminal 侧换算方向感知：放电 P_term_max = 1050/η_dcdc = 1082.5 kW；
   充电 P_term_max = 1050·η_dcdc = 1018.5 kW。
2. **1RC 电流解析解**：令 E = OCV−V_1（均为状态，无代数依赖），解
   R_0·I² − E·I + P_t = 0，取**小电流物理根**；实现采用数值稳定形式
   （与小根等价，避免 E−√(E²−ε) 大数相减）：
   **I = 2·P_t / (E + √(E² − 4·R_0·P_t))**；
   判别式 D<0 的物理语义 = 请求功率超过该 ECM 状态的物理极限 P_max,R = E²/(4R_0)：
   此时 **P_actual = P_max,R** 并置 `battery_power_limited` /
   `battery_discriminant_limit` 双 flag（power-limit semantics），
   **不是**简单 clamp 判别式后假装请求被满足；SOP 层再取
   min(P_R, P_V, P_I, P_DC/DC)；无迭代、无代数环、HIL-friendly。

## 4. 子系统施工设计

### 4.1 船体与浅水（FR-412~415）

新 `Ship_Hull_Dynamics_E`（MATLAB Function，替代 CODE_HULL）：

```text
输入: v_ship, F_prop, v_current, F_env, resistance_multiplier, water_depth_m
V_rel  = v_ship − v_current（对水速度，沿用 Model D 定义）
R_deep = res_mult · K_R · V_rel·|V_rel|            % 890 锚定，不变
H/T    = water_depth_m / Hull_T_m
域处理: H/T > 3   → Δr = 0（深水渐近）
        1.2 ≤ H/T ≤ 3 → 下式
        H/T < 1.2 → clamp H/T=1.2 计算 + MODEL_DOMAIN_WARNING 输出置 1
Δk   = 0.644 · (H/T)^(−1.72)                        % Millward [4]
Re   = |V_rel| · Lpp / ν（ν=1.19e−6 淡水 15℃近似）
C_F  = 0.075/(log10 Re − 2)²                        % ITTC-57
C_F* = 0.08468/(log10 Re − 1.631)² · [1 + c1/(log10 Re + c2) · (H/T)^c3]
       c1=1.1680, c2=−0.5238, c3=−1.472             % Zhang/Rhine86 [1][3]
Δr   = (1 + k + Δk)·C_F* / ((1 + k)·C_F) − 1        % k=0.2 nominal（1+k=1.2，
                                                    %  LITERATURE_RANGE 1.15~1.30）
R_total = R_deep · (1 + Δr)
dv/dt  = (F_prop − R_total + F_env) / Ship_Mass
```

设计要点：

- **相对增量法**：Zhang 公式只计算 Δr（无量纲增量），绝对阻力水平由 K_R=890 锚定——
  文献公式的船型绝对差异被锚定吸收，只借用其 H/T 物理形状；
- 已知局限登记：c1/c2/c3 标定族 C_B=0.86 vs 本船 0.60（型差外推）；
  未含浅水兴波临界区（Fr_h→1）分量——H/T≥1.2 域限 + C 包 UKC 限速负责规避；
- `resistance_multiplier` 输入保留（通用不确定度旋钮，默认 1.0），
  其浅滩语义退役（scenario 一律 water_depth 表达浅水）；
- 新增输出：`R_shallow_N`、`HT_ratio`、`domain_warning`（logging 用）。

### 4.2 螺旋桨重新标定与 w(H/T)（FR-417 + OQ-4，GPT 评审修订）

**重新标定（必须）**：w_deep 从 0.15 改为 0.10 后 V_A=(1−w)·V 在所有工况点变化，
旧 K 系数不再满足设计点推力/扭矩平衡——禁止沿用 Model D 推导值。
按原推导链在 Model E 语境重算四个 K：

```text
设计点：11 kn（巡航桨设计点，保留）、w_deep=0.10 → V_A_design = 0.90×5.66 = 5.09 m/s
P_shaft_design = Mot_P_nom·Elec_Eff·Gear_Eff/(1+SeaMargin) ≈ 537.6 kW（链不变）
F_eff_design = η_prop·P_shaft_design/V_design ≈ 322.5kW/5.66 ≈ 56.98 kN
F_bollard = 1.5·F_eff；K_Fw = F_bollard/ω_prop_nom²；K_Fv = (F_bollard−F_eff)/V_A_design²
Q 同理（Q_bollard = 1.2·Q_prop_nom）
```

`Propeller_Hydrodynamics` 的 wake_fraction 由参数改为输入信号；
`Wake_vs_HT` 模块（w = w_deep + Δw_Zhang(H/T)，linear 插值 clamp [1.2,2.7]）的
**H/T 输入来自 Hull 模块输出的 HT_ratio 信号**（单一事实源，禁止两处各自计算）。
敏感性扫掠作用于重标定后的 Model E nominal（T-011，OAT）。
### 4.3 双发电机组与状态机（FR-418/419/421 + OQ-6，GPT 评审修订）

```text
EMS: P_gen_ref_kW（聚合，不变）
        ↓
Legacy_Commitment_Adapter          % 显式 PLACEHOLDER，Plant 之外的可拔插适配器
  rule: P_ref ≤ 500 → DG1；P_ref > 500 → DG1=500, DG2=余量
  （scenario 可 override 强制组合用于测试；B 包 PMS v2 直接替换本 adapter）
        ↓ DG1_enable / P_DG1_ref / DG2_enable / P_DG2_ref（Genset Plant 真实输入）
06_Genset_System（DG1_Plant / DG2_Plant，各自：
  状态机(OQ-6) → ref clamp[P_min,500]（ON 态）→ ramp(20 kW/s) → 一阶 τ=5 s → P_i_actual）
        ↓
P_gen_actual = P1 + P2（送 13_DC_Bus / 09_Bus_Residual_Monitor）
```

- 每机组输出：P_i_actual、state、below_min_flag、start_count；
- **aborted-start 语义**：STARTING/SYNC 不可中断；期间收到 stop 只置 pending_stop，
  进入 ON 后才执行停机（状态机简单 + start fuel 计量唯一）；
- **启动缺口**：机组 STARTING/SYNC 期间 P_i=0，adapter 不得虚构其功率；
  缺口由电池下垂/母线自然响应（T-006 单测覆盖该 case）；
- PLACEHOLDER 块加永久注释与模型内标注（FR-421 防线：不得用于经济性结论）。
### 4.4 per-genset 燃油（FR-420）

`10_Fuel_Consumption` 重构：fuel = Σ_i [∫ P_i·BSFC(P_i)/3.6e6 dt + start 事件燃油]。

- BSFC LUT（PCHIP，定义域 [125, 500] kW）：节点
  {125:600, 250:547, 375:447, 400:427, 500:453} g/kWh；
- 域守卫：ON 且 P≥P_min 才查表；STARTING/SYNC 按 OQ-6 均摊 C_start；
  OFF/COOLDOWN fuel=0；**禁止 P<P_min 外推**；
- 每台机组独立积分 + 汇总输出（fuel_1_kg, fuel_2_kg, fuel_total_kg）。

### 4.5 电池 1RC ECM（FR-422/423 + OQ-9）

`14_Battery_Converter` 重构（下垂 grid-forming 保留）：

```text
V_dc → droop: P_bus_req = K_droop·(V_dc_nom − V_dc)        % 不变
     → SOP clamp（§4.6，bus-side）
     → terminal 换算: 放电 P_t = P_bus/η_dcdc；充电 P_t = P_bus·η_dcdc
     → 解析求 I（OQ-9 二次式；R_0(SOC)、OCV(SOC) LUT）
     → V_t = OCV − I·R_0 − V_1；V̇_1 = −V_1/(R_1C_1) + I/C_1（状态）
     → SOC 电荷积分: 放电 ΔSOC = −I·Δt/Q_nom；
                     充电 ΔSOC = +η_coul·|I|·Δt/Q_nom（η_coul=0.98，Q_nom=1860 Ah）
     → 输出: P_bat_bus, SOC, V_bat=V_t, I_bat, SOP 上下限, flags
```

- 符号约定（FR-423 核对结论）：**I > 0 = 放电**（与 P_bat_bus > 0 = 向母线放电一致）；
- SOC 电荷积分方向定义（η_coul=0.98 为 charge accounting efficiency，
  **不是**再次作用于端口功率的 energy efficiency）：
  放电（I>0）：ΔSOC = −I·Δt/Q_nom；充电（I<0）：ΔSOC = −η_coul·I·Δt/Q_nom
  （Q_nom=1860 Ah；即充电时入账电量打折，放电全计）；
- `ASSUMED_Bat_Eff_Discharge/Charge` 参数定义保留（ECMS `ASSUMED_k_eq_FEC` 派生引用），
  Plant 侧 SOC/功率路径不再使用——控制律内部口径不动（EMS 不修改红线）；
- R_0/R_1/C_1/OCV 全 SOC-LUT（linear 插值 + 域外 clamp，表值 = 顾问包 pack 级推导值，
  见证据链文档）；SOC 定义域 [0.10, 0.95] 沿用；**LUT 覆盖 [0.1, 0.9]，SOC∈(0.9,0.95]
  时 clamp 到 0.9 节点值并置 `ecm_lut_boundary_flag`**（禁止 LUT 默认外推）。

### 4.6 Battery SOP（FR-424 + OQ-9）

每步由状态 (SOC, V_1) 计算（bus-side 口径）：

```text
P_SOP_dis = min( P_current, P_voltage, P_SOC, 1050 kW )
  P_current = I_bat_limit_surrogate_A · V_t / η_dcdc
              （≈1823 A @576V 名义，**DERIVED surrogate，由 1050 kW/576 V 推导，
              非独立 BMS 电流额定**——论文不得表述为实测电流上限）
  P_voltage = 由 V_t ≥ V_min=360 反解 I 上限 → 换算 bus-side
  P_SOC     = SOC ≤ DisDerate_Start(0.25) 线性降额 → SOC_min(0.10) 归零（沿用现有降额族）
P_SOP_chg 同理（V_max=657，ChgDerate_Start 0.90 → SOC_max 0.95，1050 kW 上界）
```

### 4.7 EMS 接口兼容（FR-426）

- EMS 七变体输入/输出信号契约**不变**：输出仍为聚合 P_gen_ref_kW；
  拆分发生在 Plant 侧 Placeholder（§4.3）；
- SOP 信号对 EMS 暴露方式与现有 P_bat_dis_avail/chg_avail 端口同名同义
  （值从 SOC 降额升级为四约束 min，语义不变——"当前可用功率"）；
- **FR-426** 验收：七变体全部 compile；law=0/law=5 兼容 smoke 无错误（不作物理判据）；
  T-412 由专用 Plant 测试台执行（§5.2）；law=6/7 仅 compile。

### 4.8 Scenario 水深接口（FR-414）

- `01_Scenario_Manager` 新增输出 `water_depth_m`；
- legacy 工况全部由 `ModelE_TestScenarios.m` patch 默认水深 20 m（H/T≈9.2，深水渐近，
  行为与无浅水的 Model D 一致——保证继承 smoke 语义可复用）；
- 新增浅水测试工况族（H/T 六档，FR-415）供 T-412 使用；
- v_cmd/水流/resistance_multiplier 语义不变。

## 5. T-412 Gate 设计（FR-425 + OQ-5）

### 5.1 基线策略

Model D 的 `gate_baseline_smoke.json` 随 legacy config 隔离至 `_legacy_reference/`
（绝对口径作废；
用户裁决：GPT 提出的 Model D component-level regression 建议废弃，Model E 不做任何
对 Model D 的回归对照——含 KEEP 模块）。Model E 自身基线**不在首 PASS 自动冻结**：
T-412 PASS → 主 Agent/用户评审（config manifest、物理曲线、敏感性、Gate JSON）
→ 批准后才由 40 号脚本生成 `config/gate_baseline_modele.json`（v1.0 基线；
重建属 🟡 操作），防止 bugged model + bugged gate 互相背书。

### 5.2 测试矩阵（GPT 评审修订：专用 Plant 测试台）

T-412 由**确定性 Plant 测试台**执行——直接注入推进/机组/母线指令，不经 EMS 控制器
（Plant 科学性不依赖历史控制器行为）；EMS 变体仅做编译与兼容 smoke（§4.7）。

| 工况 | 配置 | 检查类别 |
|---|---|---|
| E01 Hull steady 11 kn | deep（20 m），固定推进指令 | G1/G2/G5/G6 + 巡航功率带 |
| E02 静水全功率 ramp | deep，推进指令 0→700 kW 阶跃 | G1/G5 + 极速带 + 推力平衡 |
| E03 顺水 / E04 逆水 | U=±1 m/s，固定工况点 | G2 方向性（U_against↑⇒P_prop↑） |
| E05 浅水六档 | H/T={3.0,2.7,2.0,1.6,1.4,1.2} | G2（H/T↓⇒R↑ 零反例 + 域警告） |
| E06 DG 启停 | 直接 command DG1（含 aborted-start 注入） | G3（状态机时序、P_min 域） |
| E07 双机与燃油区分 | 直接 command DG1/DG2（1×500 vs 2×250） | G3（燃油可区分、启动缺口不虚构） |
| E08 电池充放/SOP | 直接施加母线缺口/盈余，逼近 SOP 边界 | G4（方向性、V_t 符号、SOP 收缩） |
| E09 组合暂态 | 全机停机+欠压（保护链生效） | G5 + 保护链断言 |
| E10 law=0/law=5 兼容 smoke | 经 Legacy_Commitment_Adapter 短程运行 | 无错误即可（**不作物理判据**） |
### 5.3 判据与阈值

| 检查 | 判据 | 阈值 | gate_class |
|---|---|---|---|
| G1 结构 | fixed-step ode3 0.01 s；无代数环；零 solver 失败；零 NaN/Inf；Model D 目录零改动（sha256 对照） | 硬性 | physics_invariant |
| G2 船体/浅水 | V↑⇒R↑、H/T↓⇒R_total↑（固定 V；**不要求 w 单调**——Zhang 原数据非单调）、U_against↑⇒P_prop↑；E02 极速 ∈ [12.5,15.5] kn；E01 巡航电功率 ∈ [274,335] kW | 单调性零反例（rel tol 1e-6） | physics_invariant（单调性）/ design_derived（极速与功率带——内部 sanity，非独立实船验证） |
| G3 机组 | OFF→STARTING(2500±100 ticks)→SYNC(500±50 ticks)→ON 时序（整数 tick 判据）；ON 态 P≥P_min 或 below_min_flag；启停一次 C_start=0.6±0.01 kg；1×500 vs 2×250 燃油可区分（差异 > 1%） | 硬性 | physics_invariant / external_literature（时序量级） |
| G4 电池 | 放电 I>0→SOC↓、V_t<OCV；充电反向；SOP 四约束各自边界收缩正确（注入测试）；无双算（SOC 路径无 0.95） | 断言式 | physics_invariant |
| G5 energy closure | \|∫P_residual dt\|/E_throughput < 0.1%；稳态 \|P_residual\| 均值 < 1 kW | 硬性 | physics_invariant |
| G6 运行时 | 确定性（同配置两跑逐位一致）；runtime 记录 | informational | regression |

Gate JSON 每项检查带 `gate_class` 字段（physics_invariant / external_literature /
design_derived / regression）：design_derived 类（极速带、巡航功率带）只证明
"模型接线与量级正确"，论文中不得表述为独立实船验证。
输出 `results/gate_result.json`：`{overall, timestamp, manifest, checks:{...}}`；
FAIL → `error('GATE:*')` 非零退出（沿用项目惯例）。

## 6. Error Handling

- 电池请求功率超物理极限（判别式 D<0）：P_actual = P_max,R（power-limit semantics）
  + `battery_power_limited`/`battery_discriminant_limit` 双 flag（不报错中断，OQ-9）；
- H/T < 1.2：clamp + MODEL_DOMAIN_WARNING（E05 之外出现即在 Gate 记录）；
- PCHIP/LUT 域外：clamp + warning（NFR-411）；
- Placeholder 规则遇到 DG 启动中功率缺口：电池下垂自然补足（设计意图），
  欠压保护兜底（E08 验证）；
- 任何 Gate FAIL：禁止提交、禁止改参数凑 PASS（NFR-408），escalate 主 Agent。

## Testing Strategy

- **单元**（`scripts/tests/`，纯 MATLAB 脚本 harness，不需 Simulink）：
  解析二次式求 I（手算对照）、SOP min 逻辑、BSFC PCHIP 域守卫、
  浅水 Δr 公式对 Zhang 表手算值、状态机转移时序、w(H/T) LUT；
- **集成**：子系统级 harness sim（机组单测：启停/燃油；电池单测：充放 V_t；
  船体单测：浅水单调性）；
- **Gate**：§5 全矩阵 + law=0/5 变体；
- **负向**：人为构造缺陷（改坏一个 LUT / 删一个判据输入）确认 Gate 正确 FAIL；
- **幂等**：builder 连建两次，结构指纹（find_system 导出 JSON）逐位一致；
- **兼容性**：七变体 compile 全过（FR-426）。

## 8. Rollback

回退 = 删除 `model/simulink_modelE_(Phase4_Physics)/` 与 `docs/phase4/a2_model_e/` +
废弃 `feat/phase4-a2-model-e` 分支；Model D 从未触碰（G1 持续证明）。

## 9. 偏差登记（相对顾问裁决文档）

| # | 内容 | 理由 |
| D-1 | Gear_Eff 保持 0.96，不采纳顾问 η_gear=0.97 | 0.96 在文献带（0.96~0.98）内且为标定链一致值；最小必要修改原则；0.97 入参数文件注释备查（GPT 批准） |
| D-2 | Zhang t（推力减额）不接线 | 设计点锚定链已隐含推进损失，独立乘子会 double-count（OQ-4；GPT 批准：未来升级 K_T(J)/K_Q(J) 物理桨模时再引入 1−t） |
| D-3 | w_deep 用 Harvald/Molland 回归 0.10 而非 Zhang 0.22 | C_B=0.60 船型适配（Zhang 绝对值属 C_B=0.86 标定族），只借 Zhang 相对变化量（GPT 复核 w_T≈0.099 认可） |
| D-4 | min_up/min_down time 移出 Plant，推迟 B 包 PMS（FR-419 同步修订） | GPT A2：min up/down 是 unit commitment 约束而非机组物理动态，写入 Plant 会把控制策略偷进被控对象 |
| D-5 | 拒绝 GPT A1（KEEP 模块对 Model D 的 component-level regression） | 用户裁决：与 Model D 相关的回归建议全部废弃 |
| D-6 | 拒绝 GPT 清零重建工作区建议，维持 clone-and-modify（T-001） | 用户裁决：在 Model D 基础上改可以；A1 Q-A2 复制分叉决策不变 |
| D-7 | 拒绝 Environment_Input 替代 Scenario_Manager / res_mult 改名 | A1 审计已定 Scenario_Manager=REFINE（C 包才 REPLACE）；最小修改原则；res_mult 保留为通用不确定度旋钮并文档化 |
| D-8 | 螺旋桨四 K 随 w_deep=0.10 重新标定（不沿用 Model D 推导值） | GPT 评审指出的真实物理矛盾：w 变更后旧 K 不再满足设计点平衡；按原推导链在 Model E 语境重算（§4.2） |
| D-9 | T-412 改用确定性 Plant 测试台；law=0/5 降为兼容 smoke | GPT 评审：Plant 科学性不依赖历史 EMS 行为；七变体 compile 保留（clone 后零成本，抓接线断裂） |
| D-3 | w_deep 用 Harvald 0.10 而非 Zhang 0.22 | C_B=0.60 船型适配（Zhang 绝对值属 C_B=0.86 标定族），只借 Zhang 相对变化量 |

## 10. 执行期偏差登记（A2 实施 session，2026-09-04 夜班）

| # | 内容 | 理由 |
|---|---|---|
| D-10 | 纯算法与 MATLAB Function block 源码以**独立文件**交付：`+model/code_*_e.m`（block 源码字符串）、`+model/battery_sop_limits.m`（纯函数）、`scripts/tests/test_*.m`（单元测试）与 `scripts/tests/chain_[abc]_integration_notes.md`（集成规格）；`build_model_e.m` 的集成接线由主 Agent 独占执行（原任务书 Files/areas 写"build_model_e.m 的 XX 构建函数"） | 三链并行（T-003链/T-006链/T-008链，用户授权）与 AGENTS.md §8 单写者约束下的文件分区方案：block 源码字符串=单测同一字符串源（防漂移），物理/接口/数值零变更，仅实现文件结构分层 |
| D-11 | 子代理隔离采用**手动 detached git worktree**（`/tmp/phase4a2-wt/chain{A,B,C}`，commit 95038407）而非 pi-subagents 托管 worktree | 托管隔离检查要求 `git status --porcelain` 全净，主树存在两个非本任务未跟踪目录（`firmware/.../Host/fault_out/`、`slprj_v1_stale/`，§0 禁止清理）；子代理显式 cd 到各自 worktree，主树仅主 Agent 写 |
| D-12 | T-002 电池九节点 LUT 数值来源补充：spec/日志仅有散文提法（"LFP surrogate 180s×715.38p，全 SOC-LUT"），无具体数值；实施 session 从 2026-09-04 会话存档（01a067e3）**原文恢复**顾问参数集（OCV 541.08~600.55 V、R_0 6.32~8.66 mΩ、R_1 7.05~18.87 mΩ、C_1 1697~3058 F、缩放式 N_s=180/N_p=715.38）后写入参数文件 | 唯一事实源完整性要求；恢复过程识别并拦截一次 session_query 幻觉（伪造 LUT 数值，与散文推法矛盾），以 grep 会话原文交叉验证为准（教训记入开发日志） |
| D-13 | Model E 全部入口禁止调用 `model.ensure_canonical`（Model D 自愈机制会将 Model D canonical config/01-06/README_Model.md 强制恢复进 Model E，破坏 `_legacy_reference/` 隔离） | T-003 过渡态构建时发现并修复（commit 95038407）；ensure_canonical 属 Model D 专属自愈机制，与 OQ-8 参数独立成套纪律冲突 |

## Requirements Traceability

| Requirement | Design Decision | Validation |
|---|---|---|
| FR-410 | §1 工作区 + G1 零改动检查 | AC-408 |
| FR-411 | §2 三层参数 + manifest + override 表 | AC-408 |
| FR-412/413 | §4.1 相对增量法 + 参数表 | G2 + AC-409 |
| FR-414/415 | §4.1 域处理 + §4.8 水深接口 | E05 + AC-409 |
| FR-416 | §2 override 表（效率边界）+ 效率链审计 | AC-410 |
| FR-417 | §4.2 + 41 号扫掠脚本 | AC-410 |
| FR-418/419/421 | §4.3 + OQ-6 | G3 + AC-411 |
| FR-420 | §4.4 | G3 + AC-411 |
| FR-422/423 | §4.5 + OQ-9 | G4 + AC-412 |
| FR-424 | §4.6 | G4 + AC-412 |
| FR-425 | §5 全节 | AC-413（含负向） |
| FR-426 | §4.7 | E01/E09 + compile 检查 |
| FR-427 | §1 文档布局 | AC-414 |
| NFR-405~411 | §2/§4/§5/§6 分散落实 | G1/G6 + 幂等验证 |

## Risks and Trade-offs

- **风险**：相对增量法把 Zhang 绝对公式降级为形状来源——物理上保守但 Δr 幅值
  精度依赖标定族型差。对策：已知局限登记 + 后续真实试航数据到位后重标定。
- **风险**：w 运行时信号化改变推力平衡（设计点推导 w=0.15 vs 运行时 w_deep=0.10）。
  对策：T-412 推力平衡与极速带判据兜底；±20% 敏感性含 w。
- **权衡**：Placeholder commitment 在 DG 启动 30 s 内造成功率缺口由电池下垂补足——
  物理上真实（电网暂态由储能支撑），但 law=0 基线下 DG 启动频次可能偏高；
  接受（A2 不出经济性结论，FR-421 防线）。
- **风险**：500 t 未获实船确认。对策：representative-hull 标记（FR-412）+
  参数文件单层 override，未来翻转只需改 ModelE_Physics_Parameters.m。

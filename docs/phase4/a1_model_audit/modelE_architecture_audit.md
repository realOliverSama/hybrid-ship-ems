# Model E 架构审计报告（Phase 4-A1）

> 本文件由 `scripts/render_audit_reports.py` 从 `architecture_audit.json` 渲染，幂等可重跑；手工内容仅存在于附录保留块（`<!-- MANUAL:... -->`）。

## 元信息

| 字段 | 值 |
|---|---|
| spec | phase4-a1-model-e-audit |
| date | 2026-09-03 |
| model_under_audit | model/simulink_modelD_(EMS_NN_AECMS) |

## Subsystem 清单（FR-401）

builder 清点（`builder_subsystem_list`，14 项）：`01_Scenario_Manager`、`02_Ship_Speed_Controller`、`03_Propulsion_Plant`、`04_Load_Measurement`、`05_EMS_Controller`、`06_Genset_Average_Plant`、`14_Battery_Converter`、`08_Undervolt_Derate`、`09_Bus_Residual_Monitor`、`10_Fuel_Consumption`、`11_Voyage_Stop_Logic`、`12_Data_Logging`、`13_DC_Bus`、`15_Bus_Voltage_Monitor`

| name | builder_ref | summary | disposition | physical_fidelity | realtime_cost | rationale |
|---|---|---|---|---|---|---|
| 01_Scenario_Manager | build_model_c.m:121 buildScenarioManager | 工况注入：v_cmd 剖面（距离节点 + VOYAGE_* 时间序列表，编译期求值可逐航次注入）、水流剖面（距离/时间双模式）、resistance_multiplier 阻力倍率剖面（距离触发）、P_hotel 常量、scenario_id | REFINE | 低 | 低 | 浅滩用'阻力 ×1.15'语义表达环境，违反 environment→physics→load 因果链（路线文档 T-404）。A 包需新增 water_depth 环境输入接口（阻力倍率的浅滩语义退役）；工况生成器 v3 整体重做属 C 包（届时 REPLACE）。桥区/港区/浅滩观察等价问题（Phase 2.5 已证）由 C 包解决，不在 A 包。 |
| 02_Ship_Speed_Controller | build_model_c.m:200 buildSpeedController | PI 船速环（Kp/Ki 抗饱和限幅）→ P_prop_demand_kW；lever/RPM 语义不存在 | KEEP | 中 | 低 | A 包不动控制链。驾驶行为真实性（lever command / RPM reference / speed-hold 抽象）是 B 包 T-413/T-414 的议题；本包维持现状作为对照基线。 |
| 03_Propulsion_Plant | build_model_c.m:229 buildPropulsionPlant | 功率限幅→驱动滞后一阶 TF→Elec_Eff→转矩链（齿轮箱）→轴系动力学（Prop_J/Shaft_B）→简化螺旋桨（F=K_Fw·ω\|ω\|−K_Fv·V_A\|V_A\|）→船体（R=res_mult·Hull_K_R·V_rel\|V_rel\|） | REFINE | 低 | 中 | A 包核心施工对象：T-403 阻力模型拆分（deep+shallow，预留 bank/wind）、T-404 水深/UKC 输入、T-405 电机/逆变器效率（无可信数据则保持 constant，参数表已标 Elec_Eff=estimated/high）、T-406 螺旋桨见 propeller_decision。轴系/齿轮结构本身可保留。 |
| 04_Load_Measurement | build_model_c.m:324 buildLoadMeasurement | P_load_total = max(P_prop_actual,0) + P_hotel | KEEP | 中 | 低 | 纯汇总逻辑，物理含义清晰。P_hotel 真实性（辅机负荷谱，P_hotel_nom=100 kW estimated/high）是参数问题（T-402 已标高优先级），不是结构问题。 |
| 05_EMS_Controller | build_model_c.m:341 buildEMSControllerC | Variant Subsystem，七变体（law=0 Baseline / 1 AI_Enhanced / 2 ScheduledFF / 4 FixedECMS / 5 SOC-Adaptive ECMS / 6 Oracle ECMS / 7 NN-AECMS），EMS_WORK_MODE 手动/半自动/全自动 | KEEP | 中 | 低 | A 包不动 EMS 控制律——现有变体是 A2 物理 Gate 与后续 B/F 包的对照基线。PMS v2 / Safety 层（T-415~T-417）属 B 包；预测式 EMS 属 F 包。 |
| 06_Genset_Average_Plant | build_model_c.m:842 buildGensetPlant | 两台 500 kW 聚合为单 Plant：P∈[ASSUMED_Gen_MinStablePower=0, 1000]，爬坡 40 kW/s，一阶惯性 τ=5 s，发电机常开 | REPLACE | 低 | 低 | T-407 双机组拆分（DG1/DG2 独立 power/ramp/inertia/state/fuel）+ T-408 启停状态机（OFF/STARTING/WARMUP/ON/COOLDOWN）。最低稳定功率=0 是 ASSUMED 且物理不可信（data_request_list 优先索取项）；'何时启动第二台机组'是 Phase 4 EMS 研究的核心问题之一，聚合模型无法表达。 |
| 14_Battery_Converter | build_model_c.m:865 buildBatteryConverter | 电压下垂 grid-forming 并网（K_droop）；SOC 积分（充/放电效率折算 + DC/DC 效率）；SOC 降额式充放电可用功率 | REFINE | 中 | 低 | T-410 升级 1RC ECM（V_t=OCV(SOC)−I·R_0−V_1，极化状态），T-411 SOP（SOC/电压/电流三约束的 P_dis,max/P_chg,max 替代固定 1050 kW）。下垂并网机制保留。参数表已确认：电池内阻/OCV 参数当前完全不存在（ADD 依赖）。 |
| 08_Undervolt_Derate | build_model_c.m:913 buildUndervoltDerate | 母线欠压推进降额（CCS 稳态 675 V 以下线性降至瞬态 600 V），输出 P_prop_limit 与 P_unserved | KEEP | 中 | 低 | CCS 2026 规范驱动的保护逻辑（参数表已分类 literature），结构合理。B 包 T-417 Safety/Interlock 层会把它纳入更大的安全框架，但本模块本体保留。 |
| 09_Bus_Residual_Monitor | build_model_c.m:946 buildBusResidualMonitor | 母线功率残差监视（P_gen·η_rect + P_bat_bus − P_load_total，用于后处理校验电气层一致性） | KEEP | 高 | 低 | 纯监视无控制作用，是能量闭合校验的基础设施（T-412 物理 Gate 的 energy closure 检查依赖它）。 |
| 10_Fuel_Consumption | build_model_c.m:966 buildFuelConsumption | 聚合 P_gen → BSFC 1-D LUT（ASSUMED 抛物线族 + 官方双锚点 override）→ 积分燃油消耗 | REPLACE | 低 | 低 | T-409：fuel = fuel_1(P_1) + fuel_2(P_2)，随 T-407 双机拆分改为 per-genset BSFC。低负荷段形状仍是假设值（data_request_list 优先索取）；不改假参数凑指标红线继续生效。 |
| 11_Voyage_Stop_Logic | build_model_c.m:988 buildVoyageStop | x_ship ≥ 航程 → Stop Simulation | KEEP | 高 | 低 | 仿真控制逻辑，无物理含义问题。 |
| 12_Data_Logging | build_model_c.m:1001 buildDataLoggingDoc | 无端口说明块（signal logging 配置在 builder 层） | KEEP | 高 | 低 | 纯文档块。D 包新增观测变量时扩充 logging 清单即可。 |
| 13_DC_Bus | build_model_c.m:1011 buildDCBus | 聚合单母线电容积分：C·V·dV/dt = P_gen·η_rect + P_bat_bus − P_load_total（C_bus=40 F ASSUMED） | KEEP | 中 | 低 | 0.01 s 动态是模型中最快的时间尺度（未来 HIL 实时化的关键约束，Real-Time Cost 信息性标注供后续 Phase 参考）。A 包不改动；若 T-412 Gate 发现数值稳定性问题再议。 |
| 15_Bus_Voltage_Monitor | build_model_c.m:1041 buildBusVoltageMonitor | CCS 2026 稳态/瞬态电压带越限报警（只报警不干预） | KEEP | 高 | 低 | 规范判据（literature），纯监视。B 包 Safety 层整合时保留。 |

## ADD 清单（FR-402，Model E 新增模块）

| name | category | summary | physical_fidelity | realtime_cost | rationale |
|---|---|---|---|---|---|
| ShallowWater_UKC（水深/UKC 环境输入与浅水附加阻力） | shallow_water_ukc | 新增状态 h（水深）/T（吃水）/UKC=h−T/Fr_h=V/√(gh)；Scenario 输出'水深=X m'，Plant 自算浅水附加阻力（T-404） | 高 | 低 | 路线文档 T-404；参数表现状：水深/吃水参数完全不存在（T-402 已确认），属新增。浅水修正公式选型（如 Tuck / Schlichting 修正）在 A2 design 定稿。 |
| DG1 / DG2 双发电机独立 Plant | dual_genset | 两台 500 kW 各自独立 power/ramp/inertia/state/fuel（T-407）；本任务只搭 Plant，控制策略在 B 包 PMS v2 | 高 | 低 | 路线文档 T-407；替换 06 聚合模型。使'一台 500 kW vs 两台各 250 kW'经济性可被研究。 |
| Genset_StartStop 启停状态机 | genset_startstop | OFF→STARTING→WARMUP→ON→COOLDOWN→OFF；参数化 start delay / min power / ramp / min up/down time / start fuel penalty（T-408） | 高 | 低 | 路线文档 T-408；cfg.C_start_kg=2 kg 等启动代价参数现为 ASSUMED（参数表 high）。 |
| Battery_1RC_ECM 等效电路 | battery_ecm | V_t = OCV(SOC) − I·R_0 − V_1，V̇_1 = −V_1/(R_1C_1) + I/C_1；新增端电压/电流/极化状态（T-410） | 高 | 低 | 路线文档 T-410；参数表现状：R_0/R_1/C_1/OCV 表全部不存在（ADD 依赖参数，需文献典型值或用户回填）。 |
| Battery_SOP 状态功率约束 | battery_sop | 基于 1RC ECM 的 P_dis,max(t)/P_chg,max(t)（SOC/电压/电流三约束），替代固定 1050 kW（T-411）；rule-based 与先进 EMS 共用同一 SOP | 高 | 低 | 路线文档 T-411；依赖 battery_ecm 先行。 |

## 螺旋桨分支结论（FR-403）

- **choice**: `keep_simplified`
- **basis**: 资料清点：螺旋桨直径 D、K_T/K_Q 图谱、设计点数据全部缺失——docs/data_request_list.md A 节仅存 η_prop=0.60 行业区间估计（待确认），docs/reference/ 无任何螺旋桨资料，试航速度-功率曲线（可直接标定 Hull_K_R）亦待索取。升级 J-K_T(J)/K_Q(J) LUT 无数据基础。
- **a2_consequence**: A2 维持现有简化模型 F=K_Fw·ω\|ω\|−K_Fv·V_A\|V_A\| / Q=K_Qw·ω\|ω\|−K_Qv·V_A\|V_A\|，并对螺旋桨参数族（K_Fw/K_Fv/K_Qw/K_Qv/wake_fraction，参数表均 estimated/high）做参数敏感性验证（±20% 扫掠对 S20/静水工况 speed-power 曲线与油耗结论的影响）；若用户后续回填真实螺旋桨资料，经 A2 spec 修订升级 LUT。

## 发现（findings，不修复，零改动纪律）

1. builder 注释与实际不符：build_model_c.m §2 注释写'构建 12 个子系统'，实际构建 14 个（13_DC_Bus/14_Battery_Converter/15_Bus_Voltage_Monitor 为电气层后加）——仅记录，不修复（Model D 零改动纪律）。
2. 参数表发现：电池内阻 R_0、OCV 曲线、吃水 T、水深 h 参数在 Model D 中完全不存在——T-404/T-410 是纯新增而非升级，参数需文献典型值或用户回填。
3. 参数表发现：ASSUMED_Gen_MinStablePower=0（发电机可在 0 kW'稳定运行'）物理不可信，是 T-407/T-408 的关键假设参数（data_request_list B 节优先索取项）。
4. 参数表发现：data_request_list B 节记录爬坡率工作假定 200 kW/s，而模型 Gen_Ramp_Up=40 kW/s（聚合）——两处口径不一致，待真实参数到位时统一（已在参数表 notes 标注优先索取）。
5. 参数表发现：Ecms_Phase0_Config.m 的 BSFC tier 参数（bsfc_tiers_d25）与怠速油耗（6.795 kg/h，15% 负荷按 453 外推）均为假设值——BSFC 低负荷形状是'不改假参数凑指标'红线的核心对象。
6. 既有 reports/parameter_audit.csv 实际 108 数据行（107 唯一参数，AI_Pred_Data 重复 2 次），且有 8 个宽行含未加引号逗号——新表已按确定性锚点重组合并其 unit/source 信息，旧表只读不动。

## 附录：Model D 零改动证据
<!-- MANUAL:zero_change_evidence -->
审计全程（2026-09-03）对 Model D 工作区只读。前后证据对照：

| 文件 | sha256（前） | sha256（后） | 一致 |
|---|---|---|---|
| `config/Model_Parameters.m` | 830eec47… | 830eec47… | ✅ |
| `config/Model_Scenarios.m` | cafd670f… | cafd670f… | ✅ |
| `scripts/+model/build_model_c.m` | 90b7c9c9… | 90b7c9c9… | ✅ |
| `model/HybridShip_ModelD_DCBus.slx` | 50359e65… | 50359e65… | ✅（与 2026-09-02 截图会话记录一致，模型自该时零变化） |

`git status --short model/` 前后均仅有 `slprj_v1_stale/`（会话前已存在的未跟踪目录，与本审计无关）。
A1 全部产物位于 `docs/phase4/a1_model_audit/`，Model D 目录零新增零修改。
<!-- /MANUAL:zero_change_evidence -->

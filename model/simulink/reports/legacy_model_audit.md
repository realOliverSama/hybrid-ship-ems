# 原模型审计报告 (legacy_model_audit.md)

- 审计对象：`HybridShip0730.slx`（当前打开/legacy_reference 中的原模型）
- 模型路径：`/Users/oliverlyu/ModelA_MVP_K3/legacy_reference/HybridShip0730.slx`
- 参数脚本：`legacy_reference/ship_parameters0616.m`（GBK 原始副本 + `_utf8.m` 可读副本）
- 审计脚本：`scripts/01_audit_legacy_model.m`（本报告可由其重新生成）

## 1. 顶层模块与子系统层级（实时采集）

| 顶层模块 | 类型 |
|---|---|
| Add | Sum |
| Auto | Constant |
| Bat_fault_Inject | ManualSwitch |
| Battery_Pack_Plant | SubSystem |
| Constant | Constant |
| Constant1 | Constant |
| Constant2 | Constant |
| Constant3 | Constant |
| Constant4 | Constant |
| Constant5 | Constant |
| Controller | SubSystem |
| DCDC 响应延迟 | TransferFcn |
| DC_Grid_Node_Model | SubSystem |
| Display | Display |
| Divide | Product |
| Divide1 | Product |
| Divide2 | Product |
| EV | Constant |
| Electric_Drive_Plant | SubSystem |
| Forced_Auto | Constant |
| Fuel_Consumption_Calc | SubSystem |
| Gain | Gain |
| Gain2 | Gain |
| Gain3 | Gain |
| Gain4 | Gain |
| Gearbox_Plant | SubSystem |
| Genset | Constant |
| Integrator | Integrator |
| MODE | Constant |
| Manual Switch | ManualSwitch |
| Methanol_Genset_Plant | SubSystem |
| Mode_User_Req | MultiPortSwitch |
| Motor_fault_Inject | ManualSwitch |
| Mux | Mux |
| Mux1 | Mux |
| Propeller_Hydrodynamics | SubSystem |
| Rate Limiter | RateLimiter |
| Scope | Scope |
| Scope1 | Scope |
| Scope2 | Scope |
| Scope3 | Scope |
| Ship_Dynamics_Plant | SubSystem |
| Ship_Hull_Dynamics | SubSystem |
| Switch | Switch |
| Terminator | Terminator |
| Terminator2 | Terminator |
| Unit Delay | UnitDelay |
| Unit Delay1 | UnitDelay |
| Unit Delay2 | UnitDelay |
| Unit Delay3 | UnitDelay |
| 全船电压 (直流母排) | Scope |
| 动力与负荷 | Scope |
| 发电机转速 | Scope |
| 日用负载 | Constant |
| 河流流速 | Constant |
| 燃料消耗量 | Scope |
| 环境额外力 | Constant |
| 电池电量 (SOC) | Scope |
| 船速（knots） | Scope |
| 转速追踪 | Scope |

子系统层级摘要：
- `Battery_Pack_Plant`：OCV 查表 + 内阻 + SOC 积分 + ±500A 限流 + SOC 报警
- `DC_Grid_Node_Model`：DC_Bus_1 / DC_Bus_2 双母线电容积分模型 + 母联 R_tie 开关
- `Electric_Drive_Plant`：转速 PI + 动态功率限幅 + 电气效率折算（含回馈）
- `Gearbox_Plant`：T_drive=T·Gear_Ratio·Gear_Eff；n_motor=n_prop·Gear_Ratio
- `Ship_Dynamics_Plant`：轴系 dω/dt=(T_drive−B·ω−Q_prop)/Prop_J
- `Methanol_Genset_Plant`：调速 PI + 1s 燃油延迟 + J=50 转子 + SR 触发器启停 + 故障注入
- `Propeller_Hydrodynamics` / `Ship_Hull_Dynamics` / `Controller`：MATLAB Function
- `Fuel_Consumption_Calc`：∫P·BSFC(P)/3600/1000 → kg

## 2. 物理对象 / 控制器 / 工况输入的边界
- 物理对象：发电机组（含启停/暖机）、电池簇、直流母线（双段）、推进电机+变频器、齿轮箱、轴系、螺旋桨、船体。
- 控制器：单个 MATLAB Function `Controller`（EMS），参数全部硬编码在函数内部。
- 工况输入：顶层 Constant（n_cmd=1500/500 rpm、River_Current_mps、F_env_const、EMS_P_Hotel、MODE 模式常量）。
**不存在工况生成器、不存在目标航速闭环、不存在航程终止逻辑。**

## 3. 子系统输入/输出与单位
| 子系统 | 输入 | 输出 | 单位 |
|---|---|---|---|
| Controller | Actual_Mode,n_cmd,Vdc,Pprop,Gen_RPM,Gen_running,Gen_fault,Bat_fault,Motor_fault,SOC | Override_Flag,Pgen_ref,Pbat_ref,n_ref,P_limit,P_regen_limit,Gen_start,Gen_stop | kW/V/rpm |
| Battery_Pack_Plant | Pbat_actual[kW] | Bat_fault,V_bat,SOC | kW→A,V,- |
| DC_Grid_Node_Model | I_Bat_1,I_Gen_1,I_Motor_1,Cmd_BusTie,I_Bat_2,I_Gen_2,I_Motor_2 | V_Bus_1,V_Bus_2 | A,V |
| Electric_Drive_Plant | n_ref,P_limit,P_regen_limit,n_actual_motor | T_motor,P_elec_actual | rpm,kW,Nm |
| Gearbox_Plant | T_motor,n_actual_prop | T_drive,n_actual_motor | Nm,rpm |
| Ship_Dynamics_Plant | T_drive,Q_prop | n_actual_prop | Nm,rpm |
| Propeller_Hydrodynamics | n_prop_rpm,V_ship_mps,V_current_mps | F_prop_eff,Q_prop,P_hydro_kW,V_A | N,Nm,kW,m/s |
| Ship_Hull_Dynamics | V_ship,F_prop_eff,V_current,F_env | dVdt,V_rel,R_hull | m/s,N |
| Methanol_Genset_Plant | Pgen_ref,Gen_start,Gen_stop | Gen_RPM,Gen_running,Gen_fault,Pgen_actual,P_mech | kW,rpm |
| Fuel_Consumption_Calc | P_mech | Total_Fuel_kg | kW,kg |
- **单位体系混乱**：kW 与 W 混用（母线侧 ×1000 求电流）；rpm 与 rad/s 混用；全模型无信号单位标注。

## 4. 求解器 / 固定步长 / 仿真时间（实时采集）

- SolverType=Fixed-step, Solver=ode3, FixedStep=0.01, StopTime=10000
- SignalLogging=on (logsout), SaveOutput=on, AlgebraicLoopMsg=warning

## 5. Workspace 参数及使用位置
见 `legacy_reference/ship_parameters0616_utf8.m` 全文。关键参数与使用位置：

| 参数 | 值 | 使用位置 |
|---|---|---|
| V_bus_nom | 576 | DC_Bus_1 积分初值；Controller 内硬编码同值 |
| C_bus / R_tie | 20 / 0.002 | 母线电容 / 母联电阻 |
| Bat_Cap_kWh / Bat_Cap_Ah | 535.68 / 930 | 电池能量 / SOC 积分 |
| Bat_R_int / Bat_SOC_init | 0.02 / 0.6 | 内阻 / SOC 初值 |
| Gen_P_max / Gen_RPM_nom / Gen_J / Gen_Fric_B | 500 / 1500 / 50 / 0.1 | 发电机组 |
| Gen_PI_P / Gen_PI_I | 50 / 20 | 调速 PI |
| Mot_RPM_nom / Mot_P_nom / Mot_T_max | 1500 / 350 / 2228 | 推进电机 |
| Gear_Ratio / Gear_Eff / Elec_Eff | 6.08 / 0.96 / 0.92 | 齿轮箱 / 电气效率 |
| Mot_PI_P / Mot_PI_I | 20 / 5 | 推进转速 PI |
| Prop_J / Shaft_B | 800 / 0 | 轴系 |
| Ship_Mass / Hull_K_R | 1e6 / ≈2652 | 船体 |
| wake_fraction / K_Fw / K_Fv / K_Qw / K_Qv | 0.15 / ≈101 / ≈1836 / ≈23.4 / ≈213 | 螺旋桨 |
| River_Current_mps / F_env_const / EMS_P_Hotel | −1.0 / 0 / 100 | 顶层 Constant |
| EMS_Ramp_Rate / EMS_K_droop_bat | 20 / 2 | 脚本定义，但 Controller 内硬编码同值（双份维护） |

## 6. 当前数据记录方式
- Scope 块 11 个、Display 1 个；ToWorkspace 0 个；顶层 Outport 0 个；被记录端口 1 个。
- SignalLogging=on（logsout）但几乎无信号被标记记录；无批量导出能力。

## 7. 硬编码参数清单（实时采集）

| 位置 | 硬编码内容 |
|---|---|
| Controller (MATLAB Function) | Vdc_nom=576, Vdc_low_cut=500, Vdc_regen_derate_start=590, Vdc_regen_cut=600, P_gen_opt=400, P_gen_max=500, P_prop_max=350, P_bat_discharge_max=500, P_bat_charge_max=500, SOC_START=0.25, SOC_STOP=0.85, SOC_FORCE_EV_STOP=0.20, SOC_REGEN_DERATE=0.90, SOC_REGEN_STOP=0.95, dt=0.1, Ramp_Rate=20, K_droop_bat=2, K_droop_gen=5, P_regen_deadband=2, Gen_warmup_rpm=1450, 故障 P_limit=100 |
| Battery_Pack_Plant | ±500 A 限流；SOC 报警 0.05/0.95 |
| Methanol_Genset_Plant | 扭矩上限 30000·Gen_P_max/(Gen_RPM_nom·π)；暖机转速 1450（Controller 内） |
| 顶层 | n_cmd=1500/500；MODE=1/EV=2/Genset=3；Forced_Auto=1；Auto=1；±50 rpm/s；DCDC 1/(0.1s+1)；DC_Bus_2 初值 576 |
| 查表 | BSFC 表、OCV 表内嵌于块（见下） |

### 查表数据（实时采集）
| 查表 | 数据 |
|---|---|
| HybridShip0730/Battery_Pack_Plant/1-D Lookup
Table | BPs=[0, 0.1, 0.2, 0.5, 0.8, 0.9, 1.0] | Tbl=[504, 558, 576, 594, 603, 621, 657] |
| HybridShip0730/Fuel_Consumption_Calc/BSFC | BPs=[0, 50, 100, 200, 300, 400, 500] | Tbl=[1200, 700, 550, 480, 450, 430, 460] |

### 传递函数（实时采集）
| 传递函数 | 系数 |
|---|---|
| HybridShip0730/DCDC 响应延迟 | Num=[1] Den=[0.1 1] |
| HybridShip0730/Methanol_Genset_Plant/喷油到燃烧产生扭矩的1s物理延迟 | Num=[1] Den=[1 1] |

### 积分器初值（实时采集）
| 积分器 | 初值 |
|---|---|
| HybridShip0730/Battery_Pack_Plant/计算SOC
初始电池电量60% | IC=Bat_SOC_init |
| HybridShip0730/DC_Grid_Node_Model/DC_Bus_1/计算电压
（初始值母线电压576V） | IC=V_bus_nom |
| HybridShip0730/DC_Grid_Node_Model/DC_Bus_2/Integrator1 | IC=576 |
| HybridShip0730/Fuel_Consumption_Calc/Integrator | IC=0 |
| HybridShip0730/Integrator | IC=0 |
| HybridShip0730/Methanol_Genset_Plant/默认冷启动
限制最高转速 | IC=0 |
| HybridShip0730/Ship_Dynamics_Plant/Integrator | IC=0 |

### 顶层常量（实时采集）
| 常量 | 值 |
|---|---|
| Auto | 1 |
| Constant | 1 |
| Constant1 | 1500 |
| Constant2 | 0 |
| Constant3 | 1 |
| Constant4 | 0 |
| Constant5 | 500 |
| EV | 2 |
| Forced_Auto | 1 |
| Genset | 3 |
| MODE | 1 |
| 日用负载 | EMS_P_Hotel |
| 河流流速 | River_Current_mps |
| 环境额外力 | F_env_const |

## 8. 未连接端口 / 代数环 / 单位冲突 / 符号问题
- 未连接/悬空：V_bat→Terminator；V_Bus_2→Terminator（母线2 空载）。模型检查（unconnected_ports/lines）通过。
- 代数环：AlgebraicLoopMsg 置 error 后编译通过（已恢复 warning），无代数环；P/V 除算仅通过积分状态反馈。
- 单位冲突：无单位标注；kW/W、rpm/rad/s 混用。
- 符号：R_hull=K·V_rel·|V_rel| 恒阻碍；F_prop_eff 顺水水涡轮效应自洽；电池 Pbat>0 放电符号正确；**无充放电效率区分**。

## 9. 已知问题核查结论
- **A. V_bat_nom(576) 与 V_dc_nom(750) 混淆**：成立。全模型仅一个 V_bus_nom=576。Model A 已分离。
- **B. 参数来源**：Gen_P_max/Mot_P_nom/Gear_Ratio/Gear_Eff/Elec_Eff/Ship_Mass/EMS_Ramp_Rate 均来自 ship_parameters0616.m，已作为 LEGACY 参数带入。
- **C. 单支路额定**：图纸额定（600 kWh/576 V/750 V/500 kW/350 kW）与旧模型 535.68 kWh 不同；Model A 采用图纸额定 + 单支路等效。
- **D. EMS 遗漏日用负荷**：成立。Controller 只感知 Pprop（滞后一拍），EMS_P_Hotel 只在母线电流侧叠加。Model A 的 P_load_total 已含日用负荷。
- **E. 第二段直流母线不参与功率流**：成立。Constant 0 注入全部 I_*_2，V_Bus_2 悬空。Model A 为单母线功率平衡架构。
- **F. 推进输入为电机转速而非目标航速**：成立。n_cmd=1500 rpm 常量，无航速闭环。Model A 新增 02_Ship_Speed_Controller。

## 10. 可复用模块评估
| 模块 | 建议 | 说明 |
|---|---|---|
| Propeller_Hydrodynamics | 复用 | 二次模型简洁、符号自洽 |
| Ship_Hull_Dynamics | 复用（增加阻力倍率输入） | 与任务规格一致 |
| Gearbox_Plant / Ship_Dynamics_Plant | 复用逻辑（内联） | 增益与一阶惯性 |
| Electric_Drive_Plant | 复用逻辑、重建 | 保留 转速PI+功率/转矩限幅+效率折算 |
| Fuel_Consumption_Calc | 复用结构、重新参数化 | BSFC 表 → ASSUMED_BSFC_* |
| Battery_Pack_Plant | 重建 | 充放电效率、功率限制替代 ±500A 硬编码 |
| Methanol_Genset_Plant | 不复用 | Model A 采用平均功率模型 |
| DC_Grid_Node_Model | 不复用 | 功率平衡架构无需电压动态 |
| Controller | 重建 | Baseline + AI 变体，参数全部外置 |
| 工况输入 | 重建 | 01_Scenario_Manager |

## 11. Model A 应重建的子系统
01_Scenario_Manager / 02_Ship_Speed_Controller / 04_Load_Measurement / 05_EMS_Controller(含Variant) / 06_Genset_Average_Plant / 07_Battery_Energy_Plant / 08_Power_Availability_Limiter / 09_Power_Balance_Monitor / 10_Fuel_Consumption / 11_Voyage_Stop_Logic / 12_Data_Logging；物理内核（螺旋桨/船体/轴系/齿轮箱/驱动）复用并重新接线。

## 12. 补充发现（Model A 设计输入）
- 原模型设计航速 8 kn 时稳态电功率约 380 kW > 单侧电机额定 350 kW（单支路 + η_prop_guess=0.55 标定不自洽），实际被 P_limit=350 限功率，稳态船速约 4.0 m/s。Model A v2.0 采用双支路等效（聚合 700 kW）并将 η_prop 修正为 0.60、增加海况裕量 15%，8 kn 恢复为合法设计点（电功率 ≈609 kW ≈ 87% 聚合额定）。
- EMS 内部 dt=0.1 s 与模型 0.01 s 步长不一致。Model A 统一为模型步长。
- 电池电流按 OCV 除算（随 SOC 变化），且充放电对称。Model A 改为功率/能量积分 + 效率区分。

---
*本报告由 01_audit_legacy_model.m 生成；原模型与原参数脚本未被修改。*

# Model A 构建报告 (ModelA_build_report.md)

## 1. 概述
- 新模型: `model/HybridShip_ModelA_MVP_K3.slx`（HybridShip0730 高保真 HIL 模型的简化 MVP，用于硕士论文最小闭环验证）
- 求解器: Fixed-step ode3 @ 0.01 s；数据集导出采样 1 s；StopTime 上限 7200 s，由 11_Voyage_Stop_Logic 按航程终止。
- v2.1：双推进支路等效（电机 700 kW / 发电机 1000 kW / 电池 1200 kWh），服务航速 11 kn（公务船定位，R/W 校核），Hull_K_R≈1780 N/(m/s)^2；新增 S07 桥区机动 / S08 浅滩群 / S09 应急出动工况；
- AI 预测：MLP 环内推理（AI_Pred_Source=3，python/train_load_predictor.py 训练，窗口 22 s 经粗网格+二分细化自动选优，int8 TFLite 可部署 STM32N6），对比结果见 reports/AI_vs_Baseline_report.md。
- 顶层 12 个子系统：01_Scenario_Manager / 02_Ship_Speed_Controller / 03_Propulsion_Plant / 04_Load_Measurement / 05_EMS_Controller(Variant) / 06_Genset_Average_Plant / 07_Battery_Energy_Plant / 08_Power_Availability_Limiter / 09_Power_Balance_Monitor / 10_Fuel_Consumption / 11_Voyage_Stop_Logic / 12_Data_Logging。

## 2. 复用的原模块（HybridShip0730）
| 模块 | 处理 | 说明 |
|---|---|---|
| Propeller_Hydrodynamics | 原样复用 | K_Fw/K_Fv/K_Qw/K_Qv 二次模型，符号自洽 |
| Ship_Hull_Dynamics | 复用+扩展 | 增加 resistance_multiplier 输入（阻力剖面） |
| Gearbox_Plant / Ship_Dynamics_Plant | 逻辑内联 | 增益与一阶轴系惯量 |
| Electric_Drive_Plant | 逻辑重建 | 转速/功率需求 → 转矩链 + 效率折算 |
| Fuel_Consumption_Calc | 结构复用 | BSFC 查表 + 积分，参数重新外置 |

## 3. 重建的模块与原因
| 模块 | 重建原因 |
|---|---|
| 01_Scenario_Manager | 原模型无工况生成（n_cmd 常量） |
| 02_Ship_Speed_Controller | 原模型无航速闭环（仅转速指令） |
| 04_Load_Measurement | 原 EMS 遗漏日用负荷（问题 D） |
| 05_EMS_Controller | 原控制器参数全部硬编码；需 Baseline/AI 变体 |
| 06_Genset_Average_Plant | 任务要求平均功率模型（不研究启停/并网） |
| 07_Battery_Energy_Plant | 需充放电效率区分、功率限制替代 ±500A 硬编码 |
| 08_Power_Availability_Limiter | 原模型无可用功率限制概念 |
| 09_Power_Balance_Monitor | 新增功率守恒校验 |
| 11_Voyage_Stop_Logic | 原模型无航程终止 |
| 12_Data_Logging | 原模型仅 Scope 记录，无 logsout |

## 4. 已修复的原模型问题（详见 reports/legacy_model_audit.md）
- A. V_bat_nom(576 V) 与 V_dc_nom(750 V) 分离：Model A 不再合并 V_bus_nom。
- D. EMS 负荷跟踪包含日用负荷：P_load_total = max(P_prop_actual,0) + P_hotel。
- E. 第二段直流母线移除：单母线功率平衡架构。
- F. 推进输入由电机转速改为目标航速闭环。
- 附加发现（v1 单支路时）：原模型 8 kn 设计航速稳态电功率 ~380 kW 超单侧电机额定 350 kW；v2.0 双支路等效 700 kW 解决功率不足，v2.1 按长江危化品应急处置船（公务船）定位将服务航速标定为 11 kn（Fn∇≈0.57，R/W≈0.0058 交叉校核，静水极速 ≈11.5 kn）。

## 5. 参数来源与假设（63 项，见 reports/parameter_audit.csv）
- 参数文件哈希: ca1a5bb6464fe440481fee4e8e3186a8
- DRAWING：V_dc_nom=750, V_bat_nom=576；单支路额定 600 kWh/500 kW/350 kW × N=2 支路聚合为 1200 kWh/1000 kW/700 kW（用户确认左右舷双推进，对称工况等效）。
- LEGACY：Ship_Mass=1e6, Gear_Ratio=6.08, Gear_Eff=0.96, Elec_Eff=0.92, Gen_Ramp_Up/Down=40(20×2), 水动力系数按新设计点派生等。
- ASSUMED（需确认，共 29 项）：发电机时间常数 5 s、电池充放限 1000 kW(500×2)、效率 0.95、PI 增益、BSFC 表、目标航速 4.115 m/s(8 kn)、航程 5000 m、推进效率 0.60、海况裕量 0.15 等。
- **BSFC 数据未取得厂家曲线前，燃油消耗只能用于策略间相对比较，不能作为实船绝对油耗结论。**

## 6. 未解决问题
- 1) 电池充放电功率限制(±1000 kW 聚合)与效率(0.95)沿用原模型硬编码/经验值，需电池厂数据确认。
- 2) BSFC 曲线为原模型暂定值，需发动机台架试验数据。
- 3) 船体阻力系数 Hull_K_R 由 η_prop_guess=0.55 估计，需实船/CFD 数据。
- 4) 船体阻力/推进效率/海况裕量为工程估计（11 kn 设计点电功率 ≈609 kW ≈ 87% 电机额定，R/W≈0.0058）；需实船/CFD 数据校核。
- 5) P_unserved 在额定系统容量下恒为 0（电机 700 kW 聚合额定为先行瓶颈）；限功率机制通过 P_prop_actual<=P_prop_limit 不变式与船速回落验证（T03 逆水 4.924 m/s ≈ 理论极限 4.93）。
- 6) AI 预测已接入 MLP 环内推理（source=3）；外部 STM32N6 通道（source=2）协议预留，未做硬件联调。
- 7) 未建模：发电机启停/并网/暖机、多机组、制动回馈、母线电压动态。

## 7. 烟雾测试结果（详见 reports/smoke_summary.md）
- T01 静水: 静止起步 -> 5.66 m/s (11 kn) 巡航 -> 1200 m 终止；负荷含日用 100 kW；功率平衡误差 ~0。
- T02 顺水(+1 m/s): 负荷 585.4 kW < 静水；船速 5.663。
- T03 逆水(-1 m/s): 负荷 798.8 kW > 静水；船速 4.924 m/s（700 kW 电机功率受限的理论极限 4.93 m/s）。
- T04 阻力渐增(+40%): 推进需求 611.7 -> 697.4 kW（接近电机额定饱和），发电机爬坡+电池补偿。
- T05 阻力渐减(-30%): 发电机平滑降功率。
- 全部通过（含无 NaN/Inf、SOC 范围、功率不越界、航程终止、功率平衡<1%、限幅不变式）。
- 跨工况: 顺水负荷 < 逆水负荷 ✓

## 8. 数据生成能力
- 12 个完整航次（6 工况 × Baseline/AI），datasets/raw/run_XXXX.{mat,csv,metadata.json}。
- 数据集: train 6 航次 / validation 4 航次 / test 2 航次（按完整航次划分，1 s 采样）。
- 附加监督列: P_load_future_max_60s（未来 60 s 负荷峰值）。

## 9. 后续接入 AI 模型的端口（05_EMS_Controller/EMS_AI_Enhanced）
| 信号 | 方向 | 说明 |
|---|---|---|
| P_load_total_kW | 输入 | 当前总负荷（推进+日用） |
| SOC | 输入 | 电池荷电状态 |
| v_cmd_mps / v_ship_mps | 输入 | 目标/实际船速 |
| acceleration_mps2 | 输入 | 加速度（船体 dVdt） |
| t | 输入 | 仿真时间（预测查表用） |
| P_pred_max_kW | 输出 | 预测峰值负荷（AI 输出，日志） |
| AI_valid | 输出 | 预测有效性（false 时自动退回 Baseline） |
| P_gen_ref_kW | 输出 | 发电机功率参考（经与 Baseline 相同的限幅/爬坡/惯性） |
| AI_Pred_Source | 参数 | 0=persistence, 1=workspace 回放(AI_Pred_Data), 2=外部(STM32_Pred_Data) |
| ASSUMED_AI_DataValid | 参数 | 预测数据有效性总开关（接入真实预测时置 true） |

## 10. 后续接入 STM32N6 的端口
- 预留: AI_Pred_Source=2 分支读取 STM32_Pred_Data [t, P]（与 AI_Pred_Data 同构）；AI_Pred_Timeout_s 为通信超时阈值（v1 未实现通信，超时语义预留）。
- 推荐接入路径: STM32N6 (UDP/串口) -> MATLAB 工作区变量 STM32_Pred_Data -> 模型 05_EMS_Controller 的 ai_predict 函数。
- 模型侧无需改动：预测值经有效性检查（有限、0<=P<=ASSUMED_Pred_Bound_kW、DataValid）后参与前馈；无效/超时自动 P_ff=0 退回 Baseline。

## 11. 验收核对
- [通过] 原模型未被修改：hash=06676ea0516a (一致)
- [通过] 模型独立打开+Update：OK
- [通过] 产物存在: reports/legacy_model_audit.md：/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink/reports/legacy_model_audit.md
- [通过] 产物存在: reports/parameter_audit.csv：/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink/reports/parameter_audit.csv
- [通过] 产物存在: reports/smoke_summary.md：/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink/reports/smoke_summary.md
- [通过] 全部航次数据校验：18 个航次，0 个异常
- [通过] Baseline vs AI 一致性 (persistence)：max|ΔP_gen|=0 kW (AI_Pred_Source=0 时两模式应一致)
- [通过] AI 无效时自动回退 Baseline：AI_valid 全 0=1，max|ΔP_gen| vs Baseline=0 kW
- **总体: 全部验收项通过**

---
*本报告由 scripts/06_validate_results.m 生成。原模型与原参数脚本未被修改。*

# HybridShip Model D — NN 预测型 A-ECMS（Phase 2.5 工作区）

> **本文件夹**：`simulink_modelD_(EMS_NN_AECMS)` —— Model D 本体 + 神经网络预测型 A-ECMS（law=7）。
> Phase 2.5 研究（spec：`.specs/nn-predictive-ecms-phase25`，任务 T-301~T-312）在此目录独立演进，
> 自 `model/simulink_modelD/`（Phase 2 冻结副本）完整复制而来。
>
> 本文档为**详细说明**（由总到分：Simulink 模型 → 模型结构 → EMS 控制逻辑 → 预测模型清单）；
> 目录级概括见 `model/README.md`（含各文件夹使用的预测模型）。

---

## 1. 使用的 Simulink 模型

| 项 | 值 |
|---|---|
| 模型文件 | `model/HybridShip_ModelD_DCBus.slx` |
| 体系归属 | Model D（由 Model B 直流母线电气层封存基线复制、独立命名） |
| 被控对象 | 直流母线船舶混合动力（双推进支路等效聚合：电机 700kW×2 / 发电机 1000kW×2 / 电池 1200kWh，聚合爬坡 40kW/s） |
| 船型背景 | 长江危化品应急处置船/公务船（服务航速 11kn≈5.66m/s，Fn∇≈0.57，Hull_K_R≈1780 N/(m/s)²） |
| 求解器 | Fixed-step ode3 @ 0.01s；EMS 控制周期 1s；数据集重采样 1s |
| 重建入口 | 脚本驱动（禁止 GUI 手工修改，见 AGENTS.md §9）；模型快照按构建产物对待 |

## 2. Simulink 模型结构（由总到分）

### 2.1 顶层子系统（15 个）

```
01_Scenario_Manager   工况/航次剖面生成（v_cmd、阻力带、水流）
02_Ship_Speed_Controller   船速跟踪控制器（指令 → 推进需求）
03_Propulsion_Plant    推进器动力学（螺旋桨 + 船体阻力/动力学）
04_Load_Measurement    负荷测量（推进功率 → 总电负荷 P_load）
05_EMS_Controller      ★ EMS 控制器（Variant：law 变体切换，见 §3）
06_Genset_Average_Plant   发电机组平均模型
07_Battery_Energy_Plant   电池能量模型（SOC 积分）
08_Undervolt_Derate    欠压降额保护
09_Bus_Residual_Monitor   母线功率平衡监视
10_Fuel_Consumption    油耗累计
11_Voyage_Stop_Logic   航次终止逻辑
12_Data_Logging        数据记录
13_DC_Bus              直流母线电气层（电压动态）
14_Battery_Converter   电池变换器（限流/效率）
15_Bus_Voltage_Monitor 母线电压监控
```

### 2.2 关键子系统：05_EMS_Controller

```
05_EMS_Controller/
├── EMS_AI_Enhanced（活动变体，由 EMS_CTRL_LAW 选择，§3）
│   ├── ai_predict      AI 预测 chart（预测源选择：oracle/内联 MLP/外部）
│   ├── ems_target      ECMS 目标计算（等效因子 λ_eff → P_gen_ref）
│   ├── is_fullauto     自动/手动模式
│   └── 信号调理（K_ff 前馈、clamp 饱和、gate 门控、oracle_src 等）
├── Inport: P_load_total_kW / SOC / v_cmd_mps / v_ship_mps /
│           acceleration_mps2 / x_ship_m / t
└── Outport: P_gen_ref_kW
```

### 2.3 关键子系统：EMS_NN_AECMS / ecms_step（law=7 核心）

```
05_EMS_Controller/EMS_NN_AECMS/
├── Inport: P_load_total_kW / SOC / v_cmd_mps / v_ship_mps /
│           acceleration_mps2 / x_ship_m / t
├── nnconst_ECMS_NN_*   Constant 块（权重/归一化常数注入源，绕开 R2026a Parameter 端口固化缺陷）
├── ecms_step            Stateflow EML chart（控制律核心，见 §3.3）
│     ├── 预测体（内联：MLP-Q 前向 + G1/G2 门控，或按 GATE_MODE 变体）
│     ├── ECMS 决策（SOC_ref_eff / λ_eff → P_gen_ref）
│     └── 回退（ECMS_NN_WeightsOK=0 / NaN → ê=0 ≡ law=5）
└── Outport: P_gen_ref_kW
```

## 3. EMS 控制逻辑

### 3.1 控制器变体（EMS_CTRL_LAW）

| law | 名称 | 语义 | 角色 |
|---|---|---|---|
| 0 | 无 ECMS | 固定分配（无优化） | 最差参照 |
| 5 | 逻辑门限/SOC-自适应 ECMS | SOC 阈值 + 功率门限规则表 | **基线**（原方案） |
| 6 | Oracle 预测 A-ECMS | 未来真值能量 ê（离线上界） | 上界参照 |
| 7 | **NN 预测 A-ECMS（本项目）** | MLP 预测未来 300s 能量 ê → 成本函数实时最优分配 | **新方案** |

### 3.2 A-ECMS 控制律（law=6/7 同构）

```
SOC_ref_eff = SOC_ref + K_S · ê_NN          （预测调制 SOC 参考）
λ_eff = λ₀ + K_soc · (SOC_ref_eff − SOC)
λ_eff = max(λ_eff, Lambda_Floor)             （K_S≠0 时下限钳位）
P_gen_ref = 使等效油耗 J 最小的发电机功率（穷举/解析）
```

- **ê 来源**（唯一差异）：law=6 从 oracle 端口读真值；law=7 由内联 MLP 实时计算（C3/C4 消融差异）
- **标幺**：ê_NN = (Ê_NN − Ē_H)/E_scale；Ē_H/E_scale 为 S20 常数（kS_calibration.json）

### 3.3 门控机制（T-309/T-310，G0~G3 消融）

| 门控 | 分位数门控（q90−q10 宽度） | 工况门控（逆水段识别） | 含义 |
|---|---|---|---|
| G0 | ✗ | ✗ | 裸预测（对照组） |
| G1 | ✓ | ✗ | 不确定性超阈值 → 预测降权（K_q 线性 1→0） |
| **G2** | ✗ | ✓ | 识别逆水段 → 预测关断（K=0，回退 law=5）← **Winner** |
| G3 | ✓ | ✓ | 两者组合（K = K_q × K_w） |

门控常数（val P75/P95，禁调参）：S1=28.95 kWh、S2=40.77 kWh、迟滞 m=3 窗（30s）、upstream_class_id=6。

## 4. 预测模型清单

### 4.1 已进入闭环的模型（T-310）

| 模型 | 架构 | 输入 | 输出 | 参数 | int8 | 角色 |
|---|---|---|---|---|---|---|
| **MLP-Q（Winner）** | Dense 302→64→32→3（分位数头 q10/q50/q90） | [302]（300s 功率 + v + a） | [3] q10/q50/q90 | 21,571 | 26 KB | law=7 预测器（ê=q50；q90−q10=不确定性） |
| 工况识别器 | Conv1D×3+Pooling+FC（T-308 workclf_v1） | 30 点降采样功率 + 速度 | 7 类段型 | ~20k | ~50 KB | G2 门控（识别逆水段，upstream recall 0.95） |

### 4.2 已训练未进闭环（T-307 选模落选 / 闭环对比候选）

| 模型 | 架构 | 参数 | int8 | 落选/待定原因 |
|---|---|---|---|---|
| TCN f64 | 扩张因果卷积 7 块 + 残差 | 165,851 | 52 KB | 量化退化 +1.9%（MLP 仅 +0.24%）；val RMSE 9.84 vs MLP 9.93；待闭环对比 |
| TCN 家族（f16~f256） | 同上 | 12k~2.58M | 52~220 KB | f256 过拟合；量化退化 +1.5~1.9% |
| CNN-MLP | Conv1D×2+Pooling+FC | 626k | — | val RMSE 10.12，精度不足 |
| N-BEATS | 全连接栈（generic） | 1.45M | ~1.5MB | val RMSE 9.99；int8 大；待闭环对比 |
| 集成（MLP×0.7+TCN×0.3） | 两模型加权 | — | — | val RMSE 9.185（最优）；需双模型部署 |

### 4.3 选模依据（T-307，FR-317 协议）

- **资格筛选**（val 指标）→ **NPU Pareto**（int8 体积/量化退化/精度）→ **闭环终审**（T-310 capture + FR-318 留出门禁）
- MLP-Q 全面占优：精度（RMSE 9.93/MAE 4.95）+ int8 最小（26KB）+ 量化退化最小（+0.24%）
- **Winner = G2（MLP-Q + 逆水工况门控）**：闭环 capture 0.589、S20 省 1.02%、FR-318 通过（用户裁决 2026-08-30）
- 未进闭环模型的闭环对比 → 排入 CHIL（N6 控制器 + Simulink plant）逐个换模型验证（见 `项目开发进展.md` 2026-08-30 决策记录）

## 5. 数据与产物（Phase 2.5）

| 路径 | 内容 |
|---|---|
| `datasets/` | v2 数据集（155,087 窗口/400 航次，split 280/60/60，fit-on-train 归一化）+ fleet_norm_v2.json |
| `results/ecms_phase25/fleet/` | 航次生成（voyages_seed20260829.json，400 航次） |
| `results/ecms_phase25/models/` | 全部训练模型 .keras（MLP 家族/TCN 家族/N-BEATS/CNN） |
| `results/ecms_phase25/classifier/` | 工况识别器 workclf_v1.keras |
| `results/ecms_phase25/deploy/` | 部署权重 JSON（selected_model_weights_v2.json） |
| `results/ecms_phase25/gating/` | T-310 门控闭环产物（eml_ecms_step_mode{0..3}.m、gate_result_v2.json、runs_gate_v2.csv、runs/ 66 行缓存、t310_report/decision_summary） |
| `python/` | 训练/导出/桥脚本（train_predictor.py、train_workclf.py、architectures.py、export/、load_models_mcp.py、nn_predict_keras.m 外部预测桥） |
| `scripts/` | 61~64 流水线（61 门控单测 / 62 权重导出 / 63 集成 / 64 矩阵）+ `+ecms25/` 工具包 |

## 6. 关键约定

- 功率 kW、速度 m/s、距离 m；SOC ∈ [0,1]；P_bat_actual>0 放电、<0 充电
- `EMS_CTRL_LAW` 切换 law 变体；`EMS_MODE` 选择 AI 预测源（0 persistence / 3 环内 MLP 等）
- 预测输入口径：x = [P(t−299)..P(t) 300s | v_ship | accel]（302 维，升序，fleet_norm_v2 归一化）
- 目标：Ê_H(t) = Σ_{s=t+1..t+300} P_demand(s)/3600 [kWh]
- 数据集 v2 语义冻结（T-301 约束 3）；门控阈值禁调参（T-309）
- `.venv_tf/`、`slprj/`、`codegen/`、`*.slxc` 不入库；数据/实验产物按 AGENTS.md §13 策略

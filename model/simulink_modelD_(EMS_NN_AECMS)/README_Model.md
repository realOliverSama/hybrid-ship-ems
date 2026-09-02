# HybridShip Model D — 定时前馈 EMS 研究副本

> **Model D（本项目）**：由 Model B（直流母线电气层）封存基线复制而来、完全独立命名的研究工程。
> 用于研究基于 `(ΔP_peak, t_peak)` 双输出负荷预测的**定时前馈 EMS**（C0 Baseline / C1 即时前馈 / C2 定时前馈 三控制律对照）。
> 本副本所有文件/函数/模型名均使用 Model D 独立命名，不依赖 `model/simulink/`（Model A/B 封存工程）的路径引用（`.venv_tf` 符号链接除外）。
> 构建入口：`scripts/10_build_modelD.m`（重建 `model/HybridShip_ModelD_DCBus.slx`）。

---

以下为源自 Model B 的原 README 内容（Model D 沿用其基线说明）：

# HybridShip Model (MVP) — 最小闭环验证模型

硕士论文最小闭环验证用简化模型：静止起步 → 目标船速巡航 → 固定航程终止，
用于生成负荷预测数据集、比较传统规则型 EMS 与预测增强 EMS，并为 STM32N6 推理接入预留接口。

**v2.1 更新（2026-08）**：
1. **双推进支路等效**：实船为左右舷双推进（图纸单支路额定 ×2），对称工况下聚合建模——
   电机 700 kW / 发电机 1000 kW / 电池 1200 kWh / 聚合爬坡 40 kW/s；
2. **按船型重新标定**（长江危化品应急处置船/公务船）：服务航速 11 kn（5.66 m/s），
   Fn∇≈0.57、R/W≈0.0058 文献区间交叉校核，Hull_K_R≈1780 N/(m/s)²，
   静水功率受限极速 ≈11.5 kn；除发动机/电池/推进电机为图纸确定值外，
   其余参数均为 ASSUMED 估计值（需船模试验/CFD 确认）；
3. **新增机动工况**：S07 桥区限速-急加速 / S08 浅滩群阻力带 / S09 应急出动急加速
   （距离触发 v_cmd 剖面 + 多段阻力带，01_Scenario_Manager 已扩展）；
4. **AI 闭环验证**：MLP 负荷预测器环内推理（AI_Pred_Source=3），
   历史窗口 22 s 经粗网格+二分细化自动选优（python/train_load_predictor.py），
   int8 TFLite 13.7 KB 可部署 STM32N6；对比报告见 reports/AI_vs_Baseline_report.md。

## 目录结构

```
Model_MVP/
├── model/            HybridShip_Model_MVP.slx（可独立打开）
├── scripts/          01~07 可重复执行脚本（+model/ 工具函数包）
├── config/           Model_Parameters.m（统一参数表, 69 项）
│                     Model_Scenarios.m（S01~S06 工况定义）
├── python/           train_load_predictor.py（STM32N6 导向训练程序）
│                     export/（.keras / fp32+int8 .tflite / mlp_weights.mat / training_report.json）
├── scenarios/        工况数据（预留，SCN 由 config 生成，S01~S09）
├── results/          烟雾测试图 results/T0*/*.png；AI 对比 results/ai_compare/S0*/*.png
├── datasets/         航次数据 raw/run_XXXX.{mat,csv,metadata.json}；train/val/test
├── reports/          legacy_model_audit.md, parameter_audit.csv, Model_build_report.md,
│                     smoke_summary.md, AI_vs_Baseline_report.md, ai/*.png
└── legacy_reference/ 原模型只读副本 HybridShip0730.slx 及参数脚本
```

## 使用流程

```matlab
cd Model_MVP
% 注意：当前 MATLAB 版本的 run() 不接受数字开头的脚本名，请用 eval(fileread(...)) 方式：
eval(fileread('scripts/01_audit_legacy_model.m'))   % 审计原模型（只读）
eval(fileread('scripts/02_build_model.m'))         % 从零重建模型
eval(fileread('scripts/03_run_smoke_tests.m'))      % 5 项烟雾测试 + 结果图
eval(fileread('scripts/04_run_scenarios.m'))        % 批量 6 工况 × 2 EMS 模式 → datasets/raw
eval(fileread('scripts/05_export_dataset.m'))       % 按完整航次划分 train/val/test（1 s 重采样）
eval(fileread('scripts/06_validate_results.m'))     % 汇总校验 + 最终报告
```

AI 预测器训练与闭环对比：

```bash
.venv_tf/bin/python python/train_load_predictor.py   # 训练 + int8 量化 + 导出权重
```
```matlab
eval(fileread('scripts/07_run_ai_comparison.m'))     % Baseline vs AI 闭环对比 + 报告
```

## 模型顶层结构（12 个子系统）

01_Scenario_Manager → 02_Ship_Speed_Controller → 03_Propulsion_Plant
→ 04_Load_Measurement → 05_EMS_Controller(Variant: Baseline/AI) → 06_Genset_Average_Plant
07_Battery_Energy_Plant 08_Power_Availability_Limiter 09_Power_Balance_Monitor
10_Fuel_Consumption 11_Voyage_Stop_Logic 12_Data_Logging

## 关键约定

- 双支路等效：所有动力聚合额定 = 图纸单支路值 × DRAWING_N_PropStrings(=2)；
  非对称/单侧故障研究需另建双支路模型（本模型不覆盖）。
- 功率一律 kW，速度 m/s，距离 m；SOC ∈ [0,1]。
- P_bat_actual > 0 放电，< 0 充电。
- 求解器：Fixed-step ode3 @ 0.01 s；数据集导出重采样 1 s。
- EMS_MODE=0 Baseline / =1 AI Enhanced（Variant 由工作区变量切换）。
- AI 预测源：0 persistence，1 workspace 回放，2 STM32N6 外部（预留），3 环内 MLP 推理。
- 原模型与参数脚本只读；本目录所有文件均可由脚本重建。

---

## v3 被控对象子集变体（PlantStep）与闭环脚本族（v3 soft-HIL，2026-09-02）

- **HybridShip_ModelD_PlantStep.slx**（T-601，`scripts/20_build_plantstep_model.m`）：
  被控对象子集变体——装载主模型 → SaveAs 原位变换，删除 05_EMS_Controller 与
  12_Data_Logging，根级 Inport `P_gen_ref_kW` 替代 05 输出，11 项根级 Outport 暴露
  闭环接口；剩余 **12 个子系统**（01_Scenario_Manager / 02_Ship_Speed_Controller /
  03_Propulsion_Plant / 04_Load_Measurement / 06_Genset_Average_Plant /
  08_Undervolt_Derate / 09_Bus_Residual_Monitor / 10_Fuel_Consumption /
  11_Voyage_Stop_Logic / 13_DC_Bus / 14_Battery_Converter / 15_Bus_Voltage_Monitor）；
  主模型 `HybridShip_ModelD_DCBus.slx` 零改动（sha256 前后复核，
  `results/ecms_phase25/plantstep/plantstep_build_audit.json`）。
- **codegen**（T-604，`scripts/23_codegen_plantstep.m`）：按场景烘焙 VOYAGE_*/SCN →
  Simulink Coder 生成 C（ert.tlc 拒连续时间块，内置回退 **grt.tlc**，ode3 @0.01s
  ×100 子步/拍）→ EMS01 `Host/plantstep/<scenario>/`（build_manifest.json 指纹）。
- **X 直读接口**（design §4.4a）：ERT/GRT 根 Outport 写出在子步末级阶段时刻
  t=(k+1)−0.0025；拍边界位级真值经 shim `plantstep_get_states()` 直读 9 个连续状态
  （x_ship/v_ship/omega_prop/DriveLag/SOC/V_dc/inertia/Ki_int/fuel_int；fuel CSTATE
  为 g 口径 ×1e-3 = kg）。
- **v3 闭环脚本族**：`scripts/21_run_v3_native_arms.m`（T-610/611：原生臂 (b′) law=7 /
  (c) law=5 × 两场景，双遍 REPRO）；`scripts/22_run_gp1_gate.m`（T-602/603：G-P1 子集
  保真门禁，双场景位级 PASS）；`scripts/23_codegen_plantstep.m`（T-604）；
  `scripts/24_export_gp2_simref.m`（T-605 配套：解释执行运行采样 logsout，导出 G-P2
  sim 参考 16 列 CSV，模型零改动）。闭环产物与统一报告：
  `results/ecms_phase25/closedloop/`（native_arms/ + `v3_closedloop_report.md`）。
- **预测模型**（§20.1 一行概括）：(b′) 臂 = MLP-Q（EML 内嵌 double）+ workclf_v1
  （G2 门控）闭环；(c) 臂 = 无预测（SOC-Adaptive ECMS 基线）。

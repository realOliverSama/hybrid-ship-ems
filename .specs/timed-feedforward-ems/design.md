# Model C: 定时前馈 EMS（Peak + Time-to-Peak 预测调度） Design

Spec: `timed-feedforward-ems`  
Status: design-approved  
Created: 2026-08-19  
Requirements: `./requirements.md`（requirements-approved）

## 1. 总体架构

```
┌───────────────────────── simulink_modelC/ ─────────────────────────┐
│  HybridShip_ModelC_DCBus.slx（Model B 电气层原样继承）               │
│                                                                    │
│  04_Load_Measurement ──► 05_EMS_Controller（Variant: EMS_CTRL_LAW）  │
│    01_Scenario_Manager        ├─ EMS_Baseline      (C0, law=0)     │
│    02/03 推进                  ├─ EMS_AI_Enhanced   (C1, law=1)     │
│                               └─ EMS_ScheduledFF  (C2, law=2)★新增 │
│                                      │                             │
│                            ai_predict [Ppred, t_peak, valid]★扩展  │
│                                      │                             │
│                               sched_ff ★新增（定时门控）             │
│                                      ▼                             │
│  06_Genset（限幅→±40kW/s 爬坡→τ=5s 惯性）◄── P_gen_ref              │
└────────────────────────────────────────────────────────────────────┘

Phase 1: oracle/MLP 预测源 × C0/C1/C2 → E0-E3 + t_peak 敏感性 → 过门判定
Phase 2: 双输出模型训练(MLP/CNN1D/TCN) → 闭环回灌 → INT8/NPU 评估
```

设计原则：**预测与控制分离**——ai_predict 只负责预测（输出 Ppred/t_peak/valid），控制律差异全部在 EMS 变体内部；C1 变体逻辑原样保留，C2 为新增变体，两者共享下游安全门控模式。

## 2. 控制律变体设计（FR-002 ~ FR-006）

### 2.1 变体切换

新增 base workspace 变量 `EMS_CTRL_LAW ∈ {0,1,2}`，三个变体的 VariantControl 分别为 `EMS_CTRL_LAW==0/1/2`。为兼容既有日志列（`ems_mode`）与冒烟脚本，构建时同步 `assignin('base','EMS_MODE', EMS_CTRL_LAW>0)`。Model C 副本内全部脚本改用 EMS_CTRL_LAW（副本独立命名约定，不影响 simulink/ 封存工程）。

### 2.2 ai_predict 接口扩展（FR-007）

输出由 `[Ppred, valid]` 扩展为 `[Ppred, t_peak, valid]`。

**实现修订（T-002 实施发现，2026-08-19）**：原工程将 oracle 表作为 MATLAB Function 图级参数传入并在函数内 lininterp——但参数声明为固定尺寸 `[1 2]` 且 Stateflow 图表不支持变尺寸参数，Source=1 实为**死代码**（原工程只跑过 Source=3）。修订为 **Simulink 信号通道方案**：

- 在变体内部新增 **From Workspace 块**读取 base 变量 `AI_Oracle_Data`（N×3 矩阵：第 1 列航次内绝对时间、第 2 列 P_peak、第 3 列 t_peak_rel；开启 Interpolate data，final value 保持），输出 2 维向量经 Demux 得标量 `P_pred_ext`、`t_peak_ext`，作为 ai_predict 的第 7、8 输入端口；
- ai_predict 函数签名变为 `[Ppred, t_peak, valid] = ai_predict(t, P_load, v_cmd, v_ship, accel, x_ship, P_pred_ext, t_peak_ext)`，**删除 AI_Pred_Data/STM32_Pred_Data 表参数**，函数内不再做任何插值；
- 实验脚本负责 `assignin('base','AI_Oracle_Data', <N×3 表>)`（Source=1 时取 make_oracle_pred 产物；Source=2 预留为 STM32 数据）。

| AI_Pred_Source | Ppred | t_peak | 用途 |
|---|---|---|---|
| 0 Persistence | P_load（ΔP=0，C2 天然不动作） | 0 | 禁用/基线 |
| 1 FromWorkspace | P_pred_ext（信号） | t_peak_ext（信号） | **oracle（AI_Oracle_Data N×3）**，E1/E2 |
| 2 External(STM32) | P_pred_ext（信号） | t_peak_ext（信号） | 后续上板（同一信号通道） |
| 3 EmbeddedML | 环内 MLP 推理（幅值） | 由 AI_TPeak_Source 决定 | E3 及 Phase 2 环内 |

新增参数 `AI_TPeak_Source ∈ {0,1}`：0=预测源自带 t_peak；1=强制使用 `t_peak_ext`（oracle 信号第 2 路）覆盖。E3 = Source 3 + AI_TPeak_Source 1（MLP 幅值 + t_peak 真值）。

t_peak 语义：**相对秒数**（预测时刻起多少秒后到达峰值，0~60），与数据集标签 `P_load_future_tmax_60s` 一致。oracle 表时间列为航次内绝对时间。

### 2.3 C2 定时前馈变体 EMS_ScheduledFF（FR-003/004/005）

新增 MATLAB Function 块 `sched_ff`（离散采样 0.01 s，内部逻辑按 1 Hz 预测刷新对齐）：

```
输入: t, P_load, P_pred, t_peak_rel, valid, SOC
参数: Gen_Ramp_Up(r_max), ASSUMED_Gen_TimeConstant(τ),
      ASSUMED_TFF_TauMargin(默认 10 s, OQ-2), ASSUMED_EMS_FF_Gain(K_ff),
      ASSUMED_EMS_FF_Max_kW, ASSUMED_TFF_SOC_Thresh(0.40, OQ-3),
      ASSUMED_TFF_SOC_AdvancePct(0.25, OQ-3)

每个 1 Hz 预测更新:
  ΔP = min(max(P_pred − P_load, 0) × K_ff, FF_Max)
  if ΔP ≤ 0 or valid==0: 事件清除, P_ff = 0          # FR-006 回退语义
  else:
    T_ramp  = ΔP / r_max + ASSUMED_TFF_TauMargin      # 爬坡+惯性裕量
    if SOC < SOC_Thresh: T_ramp = T_ramp × (1 + AdvancePct)   # FR-005
    if t_peak_rel ≤ T_ramp:  触发模式 = 立即           # FR-004 退化 C1
    else:                    触发时刻 t_fire = t + t_peak_rel − T_ramp
    事件锁存 {t_fire, ΔP, t_peak_abs = t + t_peak_rel}

每个仿真步:
  if 无事件:            P_ff = 0
  elif 立即模式:        P_ff = ΔP                      # 与 C1 相同
  elif t ≥ t_fire:      P_ff = ΔP(锁存值)               # 定时触发
  else:                 P_ff = 0                        # 等待期 → baseline
```

**事件锁存与防抖**（Edge Case：预测闪烁）：事件触发后（t ≥ t_fire）保持锁存的 ΔP 直至 t > t_peak_abs + τ 或新的 1 Hz 更新给出 ΔP ≤ 0 / valid=0；触发前若新预测的 t_fire 更新，则以最新预测为准（跟随刷新，无滞回——刷新率 1 Hz 下等待期反复重排是无害的，因为 P_ff=0 不改变任何物理量）。

**下游复用 C1 模式**：P_ff → gate（× valid × 全自动）→ load_ff = P_load + P_ff → P_gen_ref = max(ems_target, load_ff) → clamp [0, Gen_P_max]。P_ff=0 时退化为 max(baseline, P_load)，与 C1 失效回退语义完全一致（FR-006）。

### 2.4 与机组接口的关系（已确认可行性）

机组模型 `06_Genset_Average_Plant` = 限幅 → Rate Limiter(±40 kW/s) → 一阶惯性(τ=5 s) → 限幅。**定时启动不需要改动机组**：C2 仅在 t_fire 时刻把 P_gen_ref 从 baseline 阶跃到 P_load+ΔP，Rate Limiter 自动以 40 kW/s 爬升，τ_margin 覆盖一阶惯性滞后，使 P_gen_actual 在 t_peak 前到位。

## 3. 新增信号记录与参数登记（NFR-003/004）

**新增 logging**（configureLogging 扩展）：
- `EMS_ScheduledFF/sched_ff` 输出 → `P_ff_kW`、`t_fire_latched`（观测定时行为）
- `06_Genset_Average_Plant/ramp` 输出 → `P_gen_rampout_kW`（计算爬坡饱和时间：t_sat = ΣΔt·1{|P_gen_ref_限幅后 − P_gen_rampout| > 1 kW}）
- `EMS_ScheduledFF/clamp` 输出 → `P_gen_ref_kW`（与既有变体同名记录，活动变体生效）

**config/Model_Parameters.m 增补**（副本内，ASSUMED 惯例登记）：

| 参数 | 值 | 等级 | 来源 |
|---|---|---|---|
| ASSUMED_TFF_TauMargin | 10 s | ASSUMED(low) | OQ-2，E2 参数扫描细化 |
| ASSUMED_TFF_SOC_Thresh | 0.40 | ASSUMED(low) | OQ-3 |
| ASSUMED_TFF_SOC_AdvancePct | 0.25 | ASSUMED(low) | OQ-3 |
| AI_TPeak_Source | 0/1 | 实验开关 | E3 用 |

## 4. Phase 1 实验设计（FR-008/009/010）

### 4.1 Oracle 预测表生成

新增 `+model/make_oracle_pred.m`：对指定场景先跑 C0 仿真，从 logsout 取 `P_load_total_kW(t)`（1 Hz 重采样），逐时刻计算未来 60 s 窗口的 max 及其 argmax 偏移，生成 N×3 表 `[t, P_peak, t_peak_rel]` 存至 `results/phase1/oracle_Sxx.mat`（使用时 load 后 `assignin('base','AI_Oracle_Data', D)`，From Workspace 块自动插值与末端保持）。

**近似声明**：oracle 由 C0 轨迹生成，而 C1/C2 下负荷轨迹可能因 EMS 作用略有差异（欠压降额→推进受限→负荷略降）。该差异在二阶小量级别，且对三控制律一致施加，不影响对照公平性。

### 4.2 实验矩阵

场景集（与既有 07/09 对比实验一致，完整 5000 m 航次）：**S04 阻力渐增、S06 阻力阶跃（训练未见）、S07 桥区机动、S09 应急出动（训练未见）**。

| 实验 | EMS_CTRL_LAW | AI_Pred_Source | AI_TPeak_Source | 目的 |
|---|---|---|---|---|
| E0 | 0 (C0) | 0 | — | 基线油耗/安全分布 |
| E1 | 1 (C1) | 1 (oracle) | 0 | 完美预测+即时前馈 |
| E2 | 2 (C2) | 1 (oracle) | 0 | **完美预测+定时前馈（核心假设）** |
| E3 | 2 (C2) | 3 (环内 MLP) | 1 (t_peak 真值) | 幅值真实误差下 C2 鲁棒性 |

E2 附带 τ_margin 扫描 {5, 10, 15, 20 s}（仅在 S04/S06 两场景），确定最终取值。

### 4.3 t_peak 敏感性（FR-009）

在 oracle 表第 3 列注入系统偏移 δ ∈ {−20, −10, −5, +5, +10, +20 s}（δ<0 = 预测峰值比实际更晚=启动偏晚的最危险方向），仅跑 C2、场景 S04/S06/S09，共 3×6=18 次仿真。

### 4.4 指标与过门判定

新增 `+model/tff_metrics.m`（复用 extract_logsout/ai_metrics 基础）：fuel_total_kg（主）、Vdc_min、SOC_end/SOC_min、电池放电能量/RMS/峰值功率、t_sat、未服务能量、船速 RMSE。

**过门条件（FR-010，OQ-1/4 定量化）**，四场景平均：

1. 节油：(fuel_C1 − fuel_C2)/fuel_C1 ≥ **2%**；
2. 安全：Vdc_min_C2 ≥ 0.98 × Vdc_min_C0（每场景）；
3. SOC：|SOC_end_C2 − SOC_end_C0| ≤ 0.05（每场景）。

三条全满足 → Phase 2 启动；否则输出否定/修正结论报告（`reports/TFF_phase1_gate_report.md`）。

### 4.5 脚本编号（副本内独立编号体系）

| 脚本 | 功能 |
|---|---|
| `12_run_phase1_control_law.m` | E0–E3 + τ_margin 扫描 + 过门判定 + 报告 |
| `13_run_phase1_tpeak_sensitivity.m` | t_peak 偏移敏感性 |
| `14_run_phase2_e2e.m` | Phase 2 赢家模型端到端 C0/C1/C2 |

## 5. Phase 2 设计（FR-011/012/013）

### 5.1 数据集与标签

副本内 `05_export_dataset.m` 扩展附加列 `P_load_future_tmax_60s`（未来 60 s 内峰值出现的相对秒数，取首个最大值位置；末端 NaN）。用 Model C + C0 重跑场景导出 train/val/test（**划分与既有 README 完全一致**：train=S01/S02/S04/S07/S08，val=S03/S05，test=S06/S09，按完整航次）。因 C0 下负荷轨迹与既有导出一致，可验证新导出与旧 CSV 的 `P_load_future_max_60s` 列逐值相同作为回归检查。

### 5.2 双输出模型

`python/train_load_predictor_dual.py`（继承既有管线：窗口构造、归一化、TFLite 导出、report JSON）：

| 模型 | 结构（输入 W×5，默认沿用搜窗结果 W；候选 {15,22,30,60}） | 输出头 |
|---|---|---|
| MLP-dual | Flatten→Dense64→ReLU→Dense32→ReLU | Dense→2 |
| CNN1D-dual | Conv1D(5→16,k3)→ReLU→Conv1D(16→32,k3)→ReLU→MaxPool→Conv1D(32→32,k3)→ReLU→Flatten→Dense32 | Dense→2 |
| TCN-dual | 残差块×3（k=3，d=1/2/4，通道 16→32→32，块内 Conv-ReLU-Conv+skip）→GlobalAvgPool→Dense32 | Dense→2 |

输出 y = [ΔP, t_peak_rel]，分别 z-score 归一化；损失 = MSE(ΔP) + λ·MSE(t_peak)，λ 初值 = σ²_ΔP/σ²_t_peak（方差均衡），在 validation 上微调 λ ∈ {0.5,1,2}×初值。统一：Adam、lr=1e-3、batch=256、epochs≤200、early-stop patience=20、同一种子集 {0,1,2} 三次训练取中位。

约束（NFR-005）：纯 Conv1D(k=3)/Dense/ReLU/MaxPool/GlobalAvgPool/Add；禁止 BatchNorm/LayerNorm（折叠进权重的 BN 也禁止，保持量化简单）；静态形状。

### 5.3 评估与选型（FR-012）

PC 端指标：ΔP MAE/RMSE、**t_peak MAE（条件于真实 ΔP > 50 kW 的事件样本）**、峰值低估 MAE（max(0, 真值−预测) 的 MAE）、参数量、模型尺寸。
部署评估：TFLite INT8（现有 PTQ 流程）→ ΔMAE_quant = MAE_INT8 − MAE_FP32；导出 ONNX（tf2onnx）→ stedgeai analyze 获取 Neural-ART 算子映射率与 RAM/ROM/延迟估计（若本机 stedgeai 不可用，降级为 TFLite 算子覆盖静态分析并在报告中标注）。
选型：加权评分 Accuracy（ΔP+t_peak+低估）> QuantizationLoss > NPU 映射 > 尺寸，取 Top1；若 Top1 为 CNN/TCN 且与 MLP-dual 的 ΔP MAE 差 <5%，优先 MLP-dual（部署简单性 tie-break，写入报告）。

### 5.4 闭环回灌（FR-013）

两阶段：
1. **MLP-dual 环内**：ai_predict Source=3 扩展为双头矩阵推理（W3 变为 32×2），跑 14_run_phase2_e2e.m 的 C2——**诚实闭环**（预测看到的就是当前闭环轨迹）。
2. **CNN/TCN 两遍法**（若赢家非 MLP）：pass1 跑 C0 全量记录 5 通道；pass2 Python 批量滑窗推理生成 N×3 表；pass3 以 Source=1 跑 C2。报告中标注"预测输入为 C0 轨迹"的近似，并以 MLP-dual 环内结果交叉验证近似可接受性。

产出：`reports/TFF_phase2_e2e_report.md`（C0/C1/C2 × 赢家模型端到端对照 + 与 Phase 1 oracle 上界的差距分析）。

## Requirements Traceability（可追溯性）

| 需求 | 设计章节 |
|---|---|
| FR-001 | §1（副本由已完成的重命名任务交付，tasks 含验收） |
| FR-002 | §2.1 |
| FR-003/004/005 | §2.3 |
| FR-006 | §2.2（Source 0/valid 语义）+ §2.3（下游门控复用） |
| FR-007 | §2.2 |
| FR-008 | §4.1/4.2 |
| FR-009 | §4.3 |
| FR-010 | §4.4 |
| FR-011 | §5.1/5.2 |
| FR-012 | §5.3 |
| FR-013 | §5.4 |
| NFR-001~005 | §4.4（指标）/§3（参数登记）/§5.2（部署约束） |

## Testing Strategy（测试策略）

分四级验证，与 tasks.md 一一对应：

1. **静态验收**（T-001）：副本独立构建 + 冒烟测试 + 路径依赖断开验证（AC-1）。
2. **单元级行为验证**（T-006，AC-2）：合成阶跃场景 + 自动断言，逐一检验 FR-003（定时触发时序）、FR-004（来不及退化 C1）、FR-005（SOC 保守提前）、FR-006（失效/非全自动回退）。
3. **回归验证**（T-002/T-010）：C1 行为与改造前逐点一致；新数据集 `P_load_future_max_60s` 列与旧 CSV 逐值一致。
4. **闭环实验验证**（T-007/008/013，AC-3/4/6）：E0–E3 对照 + t_peak 敏感性 + 端到端回灌，以 T-009 过门三条件（节油 ≥2%、Vdc_min ≥ 0.98×C0、|ΔSOC_end| ≤ 0.05）作为 Phase 1 总验收。

项目 config 的 `principles.tdd: false`，不采用 red-green-refactor；仿真类验证以脚本自动断言 + 指标表 + 图形人工复核组合进行。

## 风险与对策

| 风险 | 对策 |
|---|---|
| oracle 近似（C0 轨迹）掩盖 C2 下负荷变化 | §4.1 声明 + E3 用真实模型幅值交叉验证 |
| t_peak 标签在多峰/平台期 argmax 不稳定 | 取首个最大值位置；Phase 2 评估时以 ±5 s 容差带计命中率作为辅助指标 |
| C2 触发后预测刷新导致 ΔP 锁存值过时 | 锁存期内每 1 Hz 比较新预测 ΔP_new > ΔP_latched 时上调（只增不减，保守方向） |
| 航程末端窗口截断 | oracle 表尾部 t_peak 截断处理（§4.1）；训练样本 NaN 剔除（既有惯例） |
| stedgeai 本机不可用 | §5.3 降级方案 |
| 副本与封存工程漂移 | Model C 独立演进，禁止反向合并；simulink/ 只读 |

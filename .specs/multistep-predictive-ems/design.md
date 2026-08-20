# 面向参数不确定性的多步负荷预测增强型 EMS Design

Spec: `multistep-predictive-ems`  
Status: design-approved（2026-08-20 用户批准，含 λ_P 网格修正与 800 kW 分母）  
Created: 2026-08-20  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`（v1.2, approved）

## Summary

Phase 1 在冻结基线 `simulink_modelC/` 的副本 `simulink_modelC_(oracle_ms)/` 中实现 **EMS_MinFF（最小必要前馈）** 第四变体（`EMS_CTRL_LAW=3`）：oracle 多步负荷通道以 5 s 分辨率输出未来轨迹（固定 12 节点宽度 + 有效节点数），EMS 用一阶惯性+爬坡模型前向递推计算机组可达功率，与 `P_bat_allowable` 阈值比较得到功率缺口 `R(τ)`，以对分法求解最小 `P_ff*` 使 `R(τ)≤0, ∀τ∈[0,H]`。三对照 C0/C_P/C_MS 共用同一控制律、仅信息含量递减。实验分 Phase 1A（三锚点 × 6 horizon 粗筛，回答 RQ1）与 Phase 1B（λ_P×R_gen 参数域扫描 + τ_gen/SOC₀ 单因素，产出预测增强 EMS 有效工作区域图，回答 RQ2），最后按 FR-014 两类判据 GO/NO-GO。

## Goals and Non-Goals

### Goals

- G1（FR-001/002）：独立工程副本 + 发电机三参数 config 化。
- G2（FR-003/004/005/006/007/008/009）：oracle 多步通道与 EMS_MinFF 控制律，含 C_P 信息降级模式。
- G3（FR-010/011/012/013/013A）：Phase 1A/1B 实验与指标体系、有效工作区域图。
- G4（FR-014）：机器可读 GO/NO-GO 判定。

### Non-Goals

- Phase 2 神经网络训练（GO 后另行 spec 细化，本 design 仅预留接口约定）。
- 在线 Vdc 预测/约束求解（v1.1 已排除）。
- 修改 `model/simulink/` 与 `model/simulink_modelC/` 任何源文件。

## Architecture

### 工程结构（FR-001）

```text
model/simulink_modelC_(oracle_ms)/        ← 从 simulink_modelC/ 全量复制
├── model/HybridShip_ModelCms_DCBus.slx   ← 重命名，消除对 modelC 的引用
├── scripts/
│   ├── +model/build_model_cms.m          ← 由 build_model_c.m 派生
│   ├── 20_build_modelCms.m               ← 对应 10_build_modelC.m
│   ├── 20b_smoke_modelCms.m              ← 对应 10b
│   ├── 20c_unittest_minff.m              ← 单元验证（对应 10c）
│   ├── 21_run_ms_horizon.m               ← Phase 1A：锚点×horizon×三对照
│   ├── 22_run_ms_paramsweep.m            ← Phase 1B：λ_P×R_gen+单因素
│   └── 23_ms_gate_judge.m                ← GO/NO-GO 机器判定
├── config/Model_Parameters.m             ← 新增参数登记（见 Data Model）
├── results/ms_phase1/                    ← CSV/JSON + 图
└── reports/MSFF_phase1_gate_report.md
```

`.venv_tf` 符号链接复用；`slprj/`、`results/`、`datasets/` 不复制。副本内部所有 `simulink_modelC` 字符串引用批量替换为 `simulink_modelC_(oracle_ms)`，冒烟测试中临时 rename 源目录验证路径独立性（AC-1）。

### EMS_MinFF 变体（FR-005/006/007/008）

在 `05_EMS_Controller` 下新增第四变体 `EMS_MinFF`（VariantControl `EMS_CTRL_LAW==3`），结构沿用 EMS_ScheduledFF 的插线模式（Baseline 基值 + clamp + 同名信号记录），内部两个 MATLAB Function 块：

```text
ai_predict_ms（离散 0.01 s，内部 1 Hz 刷新）
  输入: 仿真时间 t
  输出: P_hat[12] (kW), valid_node_count = H/5, pred_valid
  数据源: AI_Pred_Source=1 → FromWorkspace N×13 [t, P_5..P_60]
          （oracle 生成器 make_oracle_ms.m 由负荷记录离线生成）

min_ff（离散 0.01 s，1 Hz 决策刷新 + 防抖保持）
  输入: P_hat[12], valid_node_count, P_gen_now, SOC, Vdc, config 参数
  输出: P_ff_kW, R_max, reach 诊断量
```

**可达性前向递推（FR-005）**：给定候选 `P_ff`，目标功率 `P_tgt = min(P_base + P_ff, P_gen_rated)`；以内部步长 `dt_r=0.1 s` 递推：

```text
P_cmd(τ)  = P_tgt                                      （目标阶跃）
P_rl(τ)   = P_gen(τ-1) + clamp(P_cmd - P_gen(τ-1),    （爬坡率限幅）
                               -R_down·dt_r, R_up·dt_r)
P_gen(τ)  = P_rl(τ) + (P_cmd... 一阶惯性: P_gen(τ) = P_rl(τ) 经
            τ_gen 一阶滞后离散化（前向欧拉，dt_r ≤ τ_gen/10 保证稳定）
P_gen,reachable(τ; P_ff) = P_gen(τ)
```

离散顺序固定为「先爬坡限幅、后一阶惯性」，与模型内 06_Genset 实际环节顺序一致（implementation 时核对，若不一致以模型实际为准并记录）。`dt_r=0.1 s`，H=60 s → 600 步/次递推。

**最小前馈求解（FR-007/008）**：定义 `R_i = P_hat[i] − P_gen,reachable(τ_i; P_ff) − P_bat_allowable`，`i = 1..valid_node_count`。`R_max(P_ff) = max_i R_i` 关于 `P_ff` 单调不增 → **对分法**：`P_ff ∈ [0, P_gen_rated − P_base]`，20 次迭代（精度 < 0.1 kW）。若 `R_max(0) ≤ 0` 则不干预（等价 Baseline）。每次 1 Hz 决策 ≤ 20×600 次递推 ≈ 1.2 万次标量运算，MCU 可行。防抖：新 `P_ff*` 与保持值差 < 5 kW 时维持原值（ASSUMED 登记）。

**C_P 信息降级模式（FR-013A）**：config 开关 `MSFF_InfoMode ∈ {peak, traj}`。`peak` 时 EMS_MinFF 内部将输入轨迹折叠为全窗口恒值 `P_peak = max(P_hat[1..valid])`——仅峰值标量、无时序信息，按最坏时序假设必然得出"立即前馈"，与 C_MS 共用同一控制律与求解器，纯粹隔离信息含量差异。oracle 生成器同一数据源同时输出两模式所需的同一 `P_hat` 序列（C_P 折叠在 EMS 内完成，保证同源同 horizon）。

### 电池允许阈值（FR-006）

```text
P_bat_allowable = P_bat,max_dis × f_SOC(SOC) × f_V(Vdc)

f_SOC: SOC ≥ 0.45 → 1.0；0.30 → 0.0；其间线性
f_V:   Vdc ≥ 720 V → 1.0；690 V → 0.0；其间线性
```

折点取值理由：SOC 工作区间历史上界 0.60/下界 ~0.55（Phase 1 实测），0.45/0.30 位于正常工作区之外，仅在高风险时收紧；Vdc 720 V ≈ 750×0.96（稳态带内），690 V ≈ 689.9 V 实测最低点 + 0，低于此值认为母线已进入压力区、不再允许电池承担更多瞬态。全部登记为 `ASSUMED_MSFF_*`（low 级），Phase 1B 附加 ±50% 折点敏感性抽查（单点验证非结论敏感）。

### 实验设计（FR-010/011/013A）

**OQ-5 三锚点**（建议值）：

| 锚点 | R_gen [kW/s] | τ_gen [s] | 理由 |
|---|---|---|---|
| Fast | 60 | 2 | D6 上界：现代机组乐观情形 |
| Nominal | 40 | 5 | 当前设计假设（现状锚点） |
| Slow | 20 | 10 | D6 下界：老旧/重载机组悲观情形 |

**Phase 1A**（21_run_ms_horizon.m）：3 锚点 × 6 horizon {10,15,20,30,45,60} × 2 代表场景（S07 桥区机动=动态代表、S04 阻力渐增=渐进代表）× 3 对照。C0 与 horizon 无关（每锚点×场景 1 次）；C_P/C_MS 各 6 档 → 每锚点×场景 13 次仿真，共 **78 次**。判读：C_P vs C_MS 差值（RQ1）；收益-H 曲线形态（ plateau / 单调 / 敏感）→ 选 2~3 个代表 horizon（预期候选 {15, 30} 或 {15, 30, 60}，以数据为准）。

**Phase 1B**（22_run_ms_paramsweep.m）：
- 主图：λ_P × R_gen = 5×5 = 25 组合 × 4 场景（S04/S06/S07/S09）× 入选 horizon × C0+C_MS → 25×4×(1+n_H) 次（n_H=2 时 300 次）。
- 单因素补充：τ_gen ∈ {2,5,10} × 3 个代表 (λ_P,R_gen) 点 × 4 场景（36 次）；SOC₀ ∈ {0.4,0.6,0.8} 同理（36 次）。
- C_P 仅在最终入选 H* 上随同运行（+100 次），用于区域图上的信息对照。
- 总仿真预算 ≈ **550 次**。启用 Simulink FastRestart（仅改 config 参数不触发重建）+ `parsim` 并行（若许可证可用）；单次仿真墙钟目标 < 30 s，总预算 < 5 h 机时。若超标，先砍 S06（与 S04 机理相近）再砍单因素密度——在报告中记录。

**OQ-6 统一分母（建议值）**：`P_load,design,max = 800 kW` = `Mot_P_nom`(700 kW 图纸聚合电机额定) + `hotel_kW`(100 kW)。电机转矩上限由 `Mot_P_nom` 派生（`Mot_T_max`），即 700 kW 是推进电功率的物理天花板，hotel 恒定 100 kW → 800 kW 是有图纸依据的设计最大电负荷。由此**现状 1000 kW 机组对应 λ_P = 1.25**。

**λ_P 网格修正（偏离 brainstorm D6 示例值，需用户知晓）**：D6 的 {0.7..1.1} 是外部咨询的示例值。以 800 kW 为分母时，{0.7..1.1} → {560..880} kW **全部低于现状机组**，区域图不包含当前设计点，工程解释性差。建议改为 **λ_P ∈ {0.8, 0.9, 1.0, 1.1, 1.25}** → P_rated ∈ {640, 720, 800, 880, 1000} kW，覆盖"弱机 → 现状"全区间且以现状锚点收尾。R_gen 网格维持 {20,30,40,50,60} kW/s。

### GO/NO-GO 判定（FR-014，23_ms_gate_judge.m）

输出 `results/ms_phase1/gate_result.json`：

```json
{
  "non_degradation": {"v_rmse": true, "dsoc_end": true, "fuel": true,
                      "e_unserved": true, "no_new_undervolt": true},
  "benefit": {"p_bat_peak_15pct_cells": [...], "rms_or_throughput_10pct": true,
              "bus_pressure_vdc": true},
  "overall": true
}
```

- 非劣化约束：逐 (参数点×场景) 检查；任一"有效声称点"违反即该点失效。
- 收益判据：在 λ_P×R_gen 网格找 ≥4 个相邻（四连通）组合点 × ≥2 场景同时满足 P_bat,peak↓≥15% 且 (RMS↓≥10% 或吞吐量↓≥10%)；母线压力区工况（C0 的 Vdc_min < 700 V）附加 Vdc_min 改善检查。
- `|ΔSOC_end| ≤ 0.005` 先用数值重复性分析验证：同一配置 3 次重复运行的 SOC_end 极差 < 0.002 则阈值有效；否则放宽至 ≤0.01 并在报告中记录依据（FR-014 条款②）。

### 指标体系（FR-012）

复用 `tff_metrics.m`/`dcbus_metrics.m` 并扩展：新增 `E_bat_throughput_kWh = ∫|P_bat|dt/3600`、`N_UV/T_UV`（Vdc < 700 V 压力区事件，注意与 675 V 降额线区分）、`ff_active_s`、`R_max` 时间序列记录。所有 run 写 `ms_phase1/runs.csv`，聚合脚本产出区域图（λ_P×R_gen 热力图：ΔP_bat,peak%、ΔVdc_min、Δfuel%）。

## Data Model

新增 config 参数（`Model_Parameters.m` 惯例，全部登记假设等级）：

| 参数 | 值 | 等级 | 说明 |
|---|---|---|---|
| `EMS_CTRL_LAW` | 扩展取值 3 | — | 3=EMS_MinFF |
| `MSFF_InfoMode` | traj / peak | — | FR-013A C_P 开关 |
| `MSFF_Horizon_s` | 10~60 | — | H；valid_node_count = H/5 |
| `MSFF_dt_reach` | 0.1 s | ASSUMED(low) | 可达性递推步长 |
| `MSFF_FF_Hyst_kW` | 5 | ASSUMED(low) | 前馈防抖 |
| `MSFF_SOC_Knee` | [0.30 0.45] | ASSUMED(low) | f_SOC 折点 |
| `MSFF_Vdc_Knee` | [690 720] V | ASSUMED(low) | f_V 折点 |
| `P_load_design_max` | 800 kW | DRAWING 派生 | λ_P 统一分母（OQ-6） |
| `Gen_P_max / Gen_Ramp_Up/Down / ASSUMED_Gen_TimeConstant` | 参数化 | — | FR-002：扫描时由实验脚本覆写 |

oracle 数据格式：`results/ms_phase1/oracle_<SID>.mat` → workspace 变量 N×13 `[t, P_5..P_60]`，由 `+model/make_oracle_ms.m` 从 C0 基准运行的负荷记录生成（与既有 `make_oracle_pred.m` 同模式，端点保持语义沿用）。

## API / Interface Changes

- `AI_Pred_Source=1` 数据格式从 N×3 扩展为 N×13（仅 EMS_MinFF 变体消费；旧变体接口不变）。
- `EMS_CTRL_LAW` 新增取值 3；取值 0/1/2 行为不变（冻结基线语义）。
- 实验脚本输出目录 `results/ms_phase1/`，不触碰既有 `results/phase1/`。

## Error Handling

- oracle 数据缺失/越界 → `pred_valid=0`，EMS_MinFF 退化为 Baseline 并计数（FR-009）。
- 对分法不收敛（理论上不可能，单调性保证）→ 防御性回退 `P_ff = P_ff,max` 并记录 warning 事件。
- 扫描中某参数组合仿真失败（如不收敛）→ 记录 NaN 行并在区域图标注，不得静默丢弃。
- Gate 判定 FAIL → `error('GATE:MSFF:*')` 终止（NFR-004 惯例）。

## Testing Strategy

- **单元验证（20c_unittest_minff.m，AC-2）**：
  - U1：解析对照——恒定负荷阶跃 + 解析可达轨迹，`P_gen,reachable` 递推误差 < 1 kW；
  - U2：`R(τ)≤0` 全程 → 与 Baseline 逐点一致（容差 1e-6）；
  - U3：合成阶跃负荷 → `P_ff*` 等于手算最小值（±0.5 kW），且前馈后 `R(τ)≤0`；
  - U4：C_P 折叠模式 → 与"全窗口恒值 P_peak"手算结果一致；
  - U5：SOC/Vdc 扫点 → f_SOC/f_V 折点行为与 P_bat_allowable 单调性；
  - U6：预测失效 → 退化 Baseline + 计数器。
- **冒烟**：副本构建后 `20b_smoke_modelCms.m` 全 PASS；路径独立性验证（AC-1）。
- **重复性**：同配置 3 次重复运行 SOC_end 极差检查（Gate 前置）。
- **回归**：每次实验批次重跑 Nominal 锚点 C0，与既有 `results/phase1/` 同场景 C0 指标比对（容差 1e-6），证明副本未引入漂移。

## Rollout and Migration

顺序：副本+重命名 → 参数化+冒烟 → EMS_MinFF+单元验证 → oracle 生成器 → Phase 1A → Phase 1B → Gate。任何一步 FAIL 停止上行。回滚：整个 `simulink_modelC_(oracle_ms)/` 为新增目录，删除即回滚，主库零侵入。

## Requirements Traceability

| Requirement | Design Decision | Validation |
|---|---|---|
| FR-001 | 工程结构/重命名/路径独立 | AC-1，冒烟+rename 测试 |
| FR-002 | config 参数表，实验脚本覆写 | AC-2，U1 |
| FR-003/004 | ai_predict_ms，12 节点+valid_node_count | U3/U6，AC-3 |
| FR-005 | 可达性前向递推（爬坡→惯性，dt_r=0.1） | U1 |
| FR-006 | P_bat_allowable 双折点系数 | U5 |
| FR-007/008 | R(τ) 定义 + 对分法 min P_ff | U2/U3，AC-2 |
| FR-009 | pred_valid 退化路径 | U6 |
| FR-010 | Phase 1A 三锚点（OQ-5 表） | AC-3 |
| FR-011 | 1B 主图+单因素，λ_P 网格 {0.8..1.25} | AC-4 |
| FR-012 | 指标扩展（throughput/N_UV/T_UV） | 每次 run 的 runs.csv |
| FR-013 | 区域图（热力图×指标） | AC-4 |
| FR-013A | MSFF_InfoMode 折叠，同源同 horizon | U4，AC-3 |
| FR-014 | gate_result.json 两类判据 | AC-5，23_ms_gate_judge.m |
| FR-015~019 | Phase 2/3 预留（接口约定已含：固定节点格式、config 化） | GO 后 spec 细化 |
| NFR-001/002/004 | 脚本化、ASSUMED 登记、GATE 错误惯例 | 全流程 |

## Risks and Trade-offs

- **仿真预算 550 次**：若单次 > 30 s 则机时失控 → FastRestart + parsim，降级顺序 S06→单因素密度（已定义）。
- **λ_P 网格偏离 D6 示例**：{0.8..1.25} 替代 {0.7..1.1}，理由为覆盖现状设计点；用户若坚持原网格，P_load,design,max 需重新定夺——**此为 design 批准点之一**。
- **C_P 折叠假设**：恒值折叠是"仅知峰值"的最坏时序解释，科学上最严格但可能低估"峰值+经验时序"的实际价值 → 在报告中明确该定义，避免过度宣称。
- **f_SOC/f_V 折点为 ASSUMED(low)**：±50% 敏感性抽查兜底；若结论对折点敏感则 escalate。
- **对分法单调性**：依赖 `R_max(P_ff)` 单调不增——爬坡/惯性/限幅下成立（P_ff 增大 ⇒ 可达轨迹处处不降）；若 implementation 发现反例（如 MinStablePower 非零引入死区）则 escalate。
- **重复性纪律**：SOC_end 极差 FAIL 时禁止自动放宽 Gate，先定位非确定性来源（FastRestart 状态残留/persistent 未复位/workspace 残留/随机源），修复后重测；阈值放宽须重新过门。

## Design Amendment Log

- **v1.1（2026-08-20，随 tasks 批准修订）**：①oracle 因果性硬约束（取点在降额上游、控制律不变性）；②欠压双阈值命名分离（675 V 欠压 / 700 V 压力区）；③仿真预算参数化（472~572，C0 复用）；④重复性 FAIL 禁止自动放宽 SOC Gate。

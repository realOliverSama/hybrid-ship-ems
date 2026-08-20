# Model B：直流母线电气层 + 安全/节能指标卡 + S10 工况 Design

Spec: `model-b-dc-bus`  
Status: design-approved  
Created: 2026-08-19  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`

## Summary

Model B 在 Model A 的功率流架构上增加**闭环直流母线电气层**：母线电容积分产生 V_dc，电池 DC/DC 以**电压下垂（droop）模式**充当 grid-forming 源（CCS 方案 A：储能稳压、发电机电流/功率模式），推进逆变器在母线欠压时降额——由此把"功率缺口→电压跌落→推进受限→船速回落"的真实物理链路闭合。EMS 仍处于上层（只发功率参考值），符合 CCS"系统稳定性不能完全依赖 PMS/EMS"的原则。

Model B 不改动 Model A 的任何文件与行为：新建独立构建函数 `+model/build_model_b.m`（自包含，复制必要的子系统构建器，不复用 Model A 的局部函数），生成 `model/HybridShip_ModelB_DCBus.slx`。

## Goals and Non-Goals

### Goals

- G1 母线电压 V_dc(t) 可观测、可记录、可判定（CCS 2026 稳态/瞬态带）→ FR-001/003/004
- G2 底层稳定由变换器下垂保证，EMS 失效/AI 失效时电压不失控 → FR-002、FR-005
- G3 EMS 三模式（手动/半自动/全自动）可切换，异常只报警 → FR-005b
- G4 指标卡（安全+节能+FEC）统一计算并出对比报告 → FR-007/010
- G5 新增 S10 水流反转工况 → FR-006
- G6 交付两种 EMS 的纯文本流程图 → FR-009

### Non-Goals

- 开关级模型、PWM 纹波、短路电流计算、FMEA/EMC 分析（CCS 条款列入论文展望，不建模）
- 母联/双分段重构（本模型保持单母线聚合等效；CCS 双分段要求写入"与实船差异"说明）
- 发电机启停状态机、Blackout Recovery 状态机（电源常在线；留作后续）
- Model A 的任何行为变更

## Architecture

### 总体数据流

```
01_Scenario_Manager ─ v_cmd / v_current(剖面) / res_mult / P_hotel ─┐
                                                                    ↓
02_Ship_Speed_Controller → P_prop_demand → 03_Propulsion_Plant ─→ v_ship/x_ship（与 Model A 相同）
                                              ↑ P_prop_limit（欠压降额）
04_Load_Measurement → P_load_total ─→ 05_EMS_Controller（Variant: Baseline/AI，三模式）
                                              │ P_gen_ref
                                              ↓
                                    06_Genset_Average_Plant（功率模式，与 Model A 相同）
                                              │ P_gen_actual
   ┌──── 电气层（Model B 新增） ───────────────┼─────────────────────┐
   │  13_DC_Bus:  C·V·dV/dt = P_gen_conv + P_bat − P_load_total      │
   │              P_gen_conv = P_gen_actual × η_rect                 │
   │  14_Battery_Converter: P_bat = K_droop·(V_dc_nom − V_dc)        │
   │              经充放电限值/SOC 降额截断；SOC 积分（沿用 07 逻辑）   │
   │  15_Bus_Voltage_Monitor: CCS 稳态/瞬态带判定、报警、ΔU、t_rec    │
   │  欠压降额: P_prop_limit = min(Mot_P_nom, f_derate(V_dc))        │
   └─────────────────────────────────────────────────────────────────┘
```

### 关键设计决策

**D1：电池下垂稳压（而不是残差功率）**。Model A 中电池被动吸收残差（P_bat = P_load − P_gen），母线永远平衡、无电压概念。Model B 改为：
- 发电机：功率模式，跟随 EMS 参考（沿用 06 的限幅+爬坡+一阶惯性）；
- 电池 DC/DC：`P_bat = K_droop × (V_dc_nom − V_dc)`，无 EMS 功率参考——电压稳态偏差即残差功率的体现：`V_ss = V_nom − P_residual/K_droop`；
- 母线：`C·V·dV/dt = P_gen·η_rect + P_bat − P_load_total`；
- 推进逆变欠压降额：`f_derate = 1`（V≥675 V）；线性降至 0（600 V）（对应 CCS 稳态下限与瞬态下限）。

**D2：下垂系数选型**。ASSUMED_K_droop = 10 kW/V（初值）：200 kW 残差 → 20 V（2.7%）稳态偏差，在 CCS 稳态带（−10%）内且留有裕量。实现时做一次小灵敏度扫描 K_droop ∈ {5, 10, 20} kW/V，选满足"稳态偏差 <5% 且数值稳定"的值后固化（回答需求开放问题 3）。

**D3：EMS 三模式实现**（FR-005b）。`EMS_WORK_MODE` ∈ {0,1,2} 作为 EMS 变体内部的前置开关：
- 0 手动：`P_gen_ref = ASSUMED_EMS_Manual_Pgen_kW`（常量参数），EMS 仅输出报警信号；
- 1 半自动：规则负荷跟随 + SOC 修正（即 Baseline 逻辑），不含预测前馈；
- 2 全自动：半自动 + （EMS_MODE=1 时）预测前馈。
设备启停/并网膜态不建模（电源常在线），半自动与全自动的差别即"是否启用预测前馈"——在报告中如实说明这一简化。任何模式下越限只报警、不切换模式。

**D4：S10 水流剖面**。01_Scenario_Manager 中水流由 Constant 改为里程剖面函数（与 v_cmd 剖面同款模式）：`SCN.current_knots_x / current_knots_v`（最多 4 节点，−1 结束），节点间按距离线性插值。S10：节点 [0, 2500, 2500+85] m 对应 [−1, −1, +1] m/s（85 m ≈ 15 s × 5.66 m/s 过渡）。S01–S09 默认单节点常量，语义不变。

**D5：指标卡计算位置**。仿真后处理（脚本 `+model/dcbus_metrics.m`），输入 logsout 提取的 ds，输出指标 struct；由 `scripts/09_run_modelB_comparison.m` 驱动 S01/S04/S07/S10 四工况 × Baseline/AI 对比并生成 `reports/ModelB_comparison_report.md` 与 `results/modelB_compare/` 图。不在模型内增加计算负担（NFR-006/007）。

**D6：新增参数**（全部进 `config/Model_Parameters.m`，ASSUMED_ 前缀，入 parameter_audit.csv）：
- `ASSUMED_C_bus = 40` F（沿用 0730 每段 20 F × 2 段并联聚合；对称双母线等效为单母线时电容翻倍）；
- `ASSUMED_K_droop = 10` kW/V（灵敏度扫描后固化）；
- `ASSUMED_eta_rect = 0.97`（发电机整流效率）；
- `ASSUMED_eta_dcdc = 0.97`（电池 DC/DC 效率，作用于 P_bat 到母线侧）；
- `ASSUMED_EMS_Manual_Pgen_kW = 400`（手动模式功率设定）；
- `ASSUMED_V_floor = 50` V（功率-电流换算除法保护下限）；
- CCS 判据常量（**非** ASSUMED，来源=CCS 2026 指南，置信度 high）：`V_dc_nom=750`（已有）、稳态带 [675, 795]、瞬态带 [600, 802.5]、t_recovery ≤ 1.5 s。

### 数值稳定性（NFR-001）
母线时间尺度：C=40 F、750 V 等级下，500 kW 失衡 → dV/dt ≈ 17 V/s → 穿越 120 V 稳态带需 ~7 s ≫ 步长 0.01 s，无刚性风险。下垂增益与 C 构成一阶闭环：τ ≈ C·V/K_droop ≈ 40×750/10e3 = 3.0 s > 5×dt ✓。ODE3 定步长稳定。

## Data Model

新增 logsout 记录信号（Model B）：`V_dc_V`、`I_gen_A`、`I_bat_A`、`I_load_A`、`P_bat_droop_kW`、`alarm_steady`、`alarm_transient`、`EMS_WORK_MODE`（常量记录）。数据集格式与 Model A 一致（run_XXXX.{mat,csv,json} + metadata 增加 model_variant="B" 字段）。

指标 struct（dcbus_metrics.m 输出）：`V_min, V_max, dU_max, t_recovery, t_steady_viol, t_transient_viol, P_margin_min, T_reserve_low, SOC_min, SOC_end, T_soc_low, P_bat_max, fuel_kg, FEC_kg, v_rmse, v_drop_max, E_unserved_kWh`。

## API / Interface Changes

- 新增脚本：`scripts/08_build_modelB.m`、`scripts/09_run_modelB_comparison.m`；
- 新增包函数：`+model/build_model_b.m`（自包含）、`+model/dcbus_metrics.m`、`+model/modelb_plot.m`；
- `config/Model_Scenarios.m`：SCN 结构体增加 `current_knots_x / current_knots_v` 字段（S01–S09 给默认值，行为不变）+ S10 定义；
- `config/Model_Parameters.m`：新增 D6 参数与 ParamMeta 行；
- 向后兼容：Model A 的 02–07 脚本签名与行为不变。

## Error Handling

- 母线电压接近 0：`P = V·I` 换算统一用 `max(V_dc, ASSUMED_V_floor)` 保护；
- AI 权重缺失/无效 → 既有 AI_valid=0 回退 Baseline 逻辑不变；
- 电池 SOC 触限 → 放电降额为 0 → 下垂自然加深、监视器记录越限（不发散、不中止）；
- 参数未加载即 Update Diagram → 报错误信息明确指向先运行 config 脚本；
- Model B 构建失败 → Model A 文件不受影响（独立构建函数、独立 .slx 文件）。

## Security and Privacy

不涉及（本地仿真资产；git 提交不含敏感信息；参数全部来自图纸/公开指南/标注假设）。

## Testing Strategy

- **构建验证**：08 构建后 EMS_MODE=0/1 × EMS_WORK_MODE=0/1/2 全部 Update Diagram 通过；
- **烟雾测试**（Model B 版，新增 T01B/T04B/T10B）：
  - T01B（S01 静水）：巡航 V_dc ∈ [675, 795] V，无 NaN/Inf，稳态偏差 <5%；
  - T04B（S04 阻力渐增）：扰动段出现可观测电压跌落，最深点在负荷上升段，t_recovery ≤1.5 s 参照判定；
  - T10B（S10 水流反转）：全程无发散，逆水段电压水平低于顺水段（负荷更高的体现）；
- **下垂灵敏度扫描**：K_droop ∈ {5, 10, 20} kW/V × S04，选值依据写入报告；
- **回归**：Model A 的 02/03 脚本重跑，烟雾测试与质量门（config/Model_GateThresholds.m + gate_baseline_smoke.json）不得退化；
- **对比验收**：09 脚本产出四工况指标卡对比表 + 图 + 报告，AI 无优势时如实呈现（NFR-005）；
- **回退验证**：MLP_WeightsLoaded=0 时 Model B AI 模式行为 == Baseline。

## Rollout and Migration

1. 基于 `main`（d977c9f8）开分支 `feat/model-b-dc-bus`；
2. 参数/工况扩展（S10 + 新参数）→ Model A 回归验证（证明未破坏 A）；
3. 实现 Model B 构建函数与电气层 → 构建验证 + 烟雾测试；
4. 指标卡与 09 对比脚本 → 四工况对比 → 报告与图；
5. 下垂扫描固化参数 → 纯文本流程图（`reports/ems_control_flowcharts.md`）；
6. 提交并推送；回滚方式 = 删除分支（main 不受影响的保证：Model A 文件零改动由 git diff 验证）。

## Requirements Traceability

| Requirement | Design Decision | Validation |
| ----------- | --------------- | ---------- |
| FR-001 | D1 电气层三件套 + 母线积分 | 构建验证 + T01B |
| FR-002 | D1 电池下垂 + D2 系数扫描 | 灵敏度扫描 + T04B |
| FR-003 | 数据模型新增信号（logsout） | T01B 检查信号存在且有限 |
| FR-004 | Bus_Voltage_Monitor + CCS 带 | T04B/T10B 越限记录非空且不中断 |
| FR-005 | EMS 变体原样复用 | 回退验证（AI 失效==Baseline） |
| FR-005b | D3 三模式前置开关 | 三模式 Update Diagram + 手动模式定功率仿真 |
| FR-006 | D4 水流程控剖面 + S10 | T10B |
| FR-007 | D5 dcbus_metrics | 09 报告含全部指标字段 |
| FR-008 | 测试策略三条烟雾测试 | 全部 PASS |
| FR-009 | 实施任务末尾产出流程图 | 文件存在 + 内容评审 |
| FR-010 | 09 对比报告含 fuel+FEC | 报告检查 |
| NFR-001 | 数值稳定性分析（τ≈1.5 s） | 烟雾测试无刚性报错 |
| NFR-002 | 08 脚本可重建 | 删除 .slx 重建验证 |
| NFR-003 | D6 参数入审计表 | parameter_audit.csv 含新行 |
| NFR-004 | 分支策略 + Model A 零改动 | git diff main 检查 |
| NFR-005 | 报告如实性 | 人工评审 |
| NFR-006 | D5 后处理计算、聚合建模 | 单航次挂钟时间记录 |
| NFR-007 | 平均值建模，无开关级 | 设计审查（本文 D1） |
| NFR-008 | ASSUMED_ 体系 | parameter_audit.csv |

## Risks and Trade-offs

- **下垂 vs 残差（D1）**：电池不再精确抵消残差 → 稳态电压有静差（droop 固有特性）。取舍：这正是真实直流组网的行为，且给论文提供 ΔU 指标；Model A 的功率平衡结果仍作为对照保留。
- **构建代码复制（不复用 Model A 局部函数）**：build_model_b.m 自包含导致 ~60% 代码重复。取舍：换取 Model A 零风险冻结；后续若两者都稳定可再抽象公共包（记录为技术债）。
- **EMS 三模式的半自动 vs 全自动差异较薄**（电源常在线，无启停逻辑）：如实写入报告"简化说明"，不把模式切换包装成更大贡献。
- **聚合单母线 vs CCS 双分段**：CCS 2026 要求≥2 母排分段；本模型为单母线聚合等效，在报告"与实船/CCS 架构差异"节明确声明，双分段重构列为后续工作。
- **K_droop 初值无来源**：灵敏度扫描 + ASSUMED 标注 + 待设计方核定流程兜底。

## 需求开放问题的设计结论

- S10 反转点 2500 m / 过渡 15 s → 采用默认值（D4）；
- 电压门限 → 已由 CCS 2026 关闭（需求阶段）；
- K_droop 灵敏度扫描 → 纳入实施（D2）。

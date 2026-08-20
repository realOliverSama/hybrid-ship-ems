# Model C（直流母线电气层）Baseline vs AI-Enhanced EMS 对比报告

## 1. 实验设置
- 模型: `model/HybridShip_ModelC_DCBus.slx`（Model A 功率流 + 聚合单母线 750 V 电气层，
  电池 DC/DC 下垂稳压 K_droop=10 kW/V，CCS 2026 电能质量判据：稳态 675~795 V / 瞬态 600~802.5 V / t_rec≤1.5 s）
- EMS_WORK_MODE=2（全自动）；AI 支路为 MLP 环内推理（窗口 22 s，AI_Pred_Source=3）
- 下垂系数选择依据：K_droop=5 时起步段 V_min=660 V 越出稳态带且 t_rec=7.25 s 超 1.5 s；
  K_droop=10/20 均满足 CCS；取 10 以兼顾电池瞬态应力（扫描数据：results/droop_sweep.mat）
- FEC 等效燃油 = fuel + (0.60−SOC_end)×1200×k_eq，k_eq≈0.499 kg/kWh（ECMS 折算，ASSUMED）

## 2. 指标卡（完整航次 5000 m）

| 工况 | 模式 | ΔU_max [V] | t_rec [s] | 稳态越限 [s] | P_margin_min [kW] | SOC_min | SOC_end | P_bat_max [kW] | fuel [kg] | FEC [kg] | v_rmse [m/s] |
|---|---|---|---|---|---|---|---|---|---|---|---|
| S01_still_water | Baseline | 60.1 | 0.00 | 0.0 | 1077 | 0.593 | 0.593 | 601 | 80.69 | 84.94 | 0.9698 |
| S01_still_water | AI | 51.3 | 0.00 | 0.0 | 1071 | 0.595 | 0.595 | 513 | 81.70 | 84.54 | 0.9698 |
| S04_resistance_inc | Baseline | 60.1 | 0.00 | 0.0 | 1077 | 0.592 | 0.592 | 601 | 91.17 | 95.91 | 1.0133 |
| S04_resistance_inc | AI | 51.3 | 0.00 | 0.0 | 1071 | 0.593 | 0.593 | 513 | 91.63 | 95.74 | 1.0133 |
| S07_bridge_maneuver | Baseline | 60.1 | 0.00 | 0.0 | 1077 | 0.593 | 0.593 | 601 | 78.36 | 82.38 | 0.9801 |
| S07_bridge_maneuver | AI | 51.3 | 0.00 | 0.0 | 1071 | 0.597 | 0.603 | 513 | 83.24 | 81.62 | 0.9801 |
| S10_current_reversal | Baseline | 60.1 | 0.00 | 0.0 | 1077 | 0.593 | 0.593 | 601 | 79.26 | 83.24 | 1.1424 |
| S10_current_reversal | AI | 51.6 | 0.00 | 0.0 | 1070 | 0.594 | 0.594 | 516 | 79.74 | 83.12 | 1.1424 |

## 3. 结果解读
- **S01_still_water**：电池峰值放电 601→513 kW，SOC 末值 0.593→0.595，ΔU_max 60.1→51.3 V，FEC 84.94→84.54 kg。
- **S04_resistance_inc**：电池峰值放电 601→513 kW，SOC 末值 0.592→0.593，ΔU_max 60.1→51.3 V，FEC 95.91→95.74 kg。
- **S07_bridge_maneuver**：电池峰值放电 601→513 kW，SOC 末值 0.593→0.603，ΔU_max 60.1→51.3 V，FEC 82.38→81.62 kg。
- **S10_current_reversal**：电池峰值放电 601→516 kW，SOC 末值 0.593→0.594，ΔU_max 60.1→51.6 V，FEC 83.24→83.12 kg。

## 4. 与实船/CCS 架构的差异声明
- 本模型为聚合单母线等效（对称双支路合并）；CCS 2026 要求母排≥2 分段，分段故障隔离/母联重构不在本期范围；
- 电压动态为平均值模型（无 PWM 纹波）；断路器/保护动作不建模（只监视不切除）；
- 所有电气参数（C_bus、K_droop、效率）为 ASSUMED 预置值，待设计方核定。

---
*由 scripts/11_run_modelC_comparison.m 生成*

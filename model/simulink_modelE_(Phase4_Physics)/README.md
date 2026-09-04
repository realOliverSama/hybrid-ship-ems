# simulink_modelE_(Phase4_Physics)/ — Model E 工作区（Phase 4-A2 + 4-B）

> **入库策略**: 入库（`slprj*/`、`*.slxc`、codegen 等生成产物不入库）。

## 用途

**Model E——Phase 4「船舶物理升级 + 真实控制/PMS v2/Safety」工作区**。
A2（spec `.specs/phase4-a2-model-e-construction`）：自 Model D 复制分叉，升级 Plant 物理
（T-403~T-411），EMS 七变体不修改。**B 包（spec `.specs/phase4-b-pms-safety`，分支
`feat/phase4-b-pms-safety`，2026-09-04）重建控制/安全侧**：驾驶输入抽象与双模式推进控制
（01/02 改组）、模块化 PMS v2 四层链（`07_PMS_v2` 替换 Legacy_Commitment_Adapter）、
独立 Safety/Interlock 层（`16_Safety_Interlock`）、43 号控制层 Gate 与三臂 baseline 验证。
Plant 物理与 EMS 七变体零改动（NFR-412/A2 FR-426 契约延续）；Model D 目录本体冻结只读
（sha256 零改动纳入 G1 持续证明）。

**500 t representative hull 口径**（`target_vessel_confirmed=false`，未确认工作假定）：
K_R=890 N/(m/s)²（线性 Δ 缩放，Q4 用户裁决）、几何 53.45/7.13/2.188 m/C_B=0.60（Rhine86 相似缩尺推导）。
**Model D 与 Model E 绝对结果不可比是设计意图**（Q3/Q4 裁决），只允许方向性/守恒性对照。

## 目录结构

| 条目 | 说明 |
|---|---|
| `config/ModelE_Control_Parameters.m` | ★ **B 包控制侧参数事实源**（FR-451：PMS Demand/Commitment/Safety/Manual/Selector 全量参数 + DERIVED 阈值/tick 换算 + 逐参数 provenance + 敏感性注册表；在物理参数之后加载，乱序 assert 兜底） |
| `config/ModelE_Physics_Parameters.m` | ★ **Model E 完整独立参数事实源**（OQ-8，单独 run 即定义全部参数；provenance 逐参数标记 + 文献 [1]~[17]；冻结敏感性档位登记；**零改动**——DD-9 机器检查项） |
| `config/ModelE_TestScenarios.m` | ★ 测试工况（S01~S10 + S20 复合航次 + **浅水 H/T 六档 S31~S36** + VOYAGE_* 编译期变量 + **B 包 MANUAL 工况 M1/M2/M3 与 VOYAGE_MANUAL_CMD_TS/EMS_HEALTH_TS 注入通道** + GEN_CMD_STOP_TS/ADAPTER_FORCE_*） |
| `config/ModelE_Sensitivity_Profile.m` | 敏感性档位选择器（nominal 零改动；Q1 冻结档位登记不扫掠） |
| `config/gate_baseline_modele.json` | A2 冻结物理基线 v1.0（**永久保留不回写**，DD-9） |
| `config/gate_baseline_modele_candidate_v1_1.json` | ★ B 包 candidate 基线 v1.1（DD-9 Step B 机器检查 4/4 过；**promotion 需 T-014 用户书面批准**） |
| `config/_legacy_reference/` | 复制分叉带入的 legacy config（Model_Parameters/Scenarios/GateThresholds/Ecms_Phase0_Config/*.mat/gate_baseline_smoke.json）——**不入路径、不参与运行**，仅证据对照（OQ-8） |
| `scripts/+model/` | ★ builder 与子系统源码：`build_model_e.m`（顶层构建器）、`code_hull_e/wake_ht_e/propeller_e`（船体浅水/w(H/T)/螺旋桨 block 源码）、`code_genset_unit_e/fuel_e`（双机组/燃油）、`battery_sop_limits/code_battery_ecm_e`（SOP/1RC ECM）、**`code_pms_demand_e/code_dispatch_e/code_safety_interlock_e/build_commitment_chart_e/build_pms_v2_e/build_safety_interlock_e`（B 包 PMS v2 四层/Stateflow chart/Safety 层）**、`write_config_manifest`/`sha256_file`/`export_structure_fingerprint`/`run_scenario_sim` |
| `scripts/+gate_e/` | ★ T-412 物理 Gate：`check_g1~g6` 六类判据 + 测试台 runner + `gate_assign_bench_inputs`（确定性指令注入） |
| `scripts/+gate_ctrl/` | ★ **B 包 43 号控制层 Gate**：`run_gate`（编排）、`get_ds/metrics_of/audit_rows_of/saturation_events`（数据与指标后处理）、`check_c1~c7` 七类判据、`injection_scenarios`（S1~S5 注入定义）、`make_candidate_v1_1`/`diff_baseline_v1_1`（DD-9 基线纪律）、`run_negative`（44 号主体）、`write_result` |
| `scripts/tests/` | 单元/集成测试 harness（纯 MATLAB）+ 三链集成规格 notes + **T-002/003/004/005 单测 + t008 变体编译矩阵 + build_commitment_harness** |
| `scripts/40_run_modelE_gate.m` | ★ **T-412 整体物理 Gate**（可选 GATE40_BASELINE_PATH candidate 记录模式，DD-9 Step C；E01b 前恢复 bench 注入 nominal——D-B19） |
| `scripts/41_run_prop_sensitivity.m` | 螺旋桨参数族 ±20% OAT 敏感性扫掠（15 cases） |
| `scripts/42_run_modelE_gate_negative.m` | 物理 Gate 负向验证 |
| `scripts/43_run_modelE_control_gate.m` | ★ **B 包控制层 Gate**（三臂 AUTO 矩阵 + MANUAL×P2 + 注入 S1~S5 + 确定性两跑，判据 C1~C7，输出 `results/gate_control_result.json`） |
| `scripts/44_run_control_gate_negative.m` | ★ **B 包控制 Gate 负向验证**（六类缺陷注入，结果附入 gate_control_result.json） |
| `scripts/10_build_modelE.m` / `10b_smoke_modelE.m` | builder 入口 / smoke（10/10b 的 ensure_canonical 调用已移除——A2 D-13，B 包 T-001） |
| `model/HybridShip_ModelE.slx` | builder 生成（脚本驱动，禁 GUI 修改；B 包顶层 16 块含 07_PMS_v2/16_Safety_Interlock） |
| `results/` | `gate_result.json`（T-412 against candidate overall=true）、`gate_control_result.json`（43+44：overall + C1~C7 + 三臂指标 + negative_control）、`config_manifest.json`（4 config sha256+有效值快照） |
| `docs/` | 证据文档指向仓库级 `docs/phase4/b_pms_safety/`（field-level diff / 三臂对比 / 裁决链） |
| `reports/parameter_audit.csv` | builder 生成的参数审计（ParamMeta，40 项） |
| `datasets/` `python/` `legacy_reference/` | 自 Model D 分叉继承（D 包适配前保持原样；legacy_reference 为历史模型只读副本） |

**注意**：`scripts/01~07/10~66` 等编号脚本为 Model D 流水线继承件——Model E 只保证
**02 build / 03 smoke / 40 gate** 链路；05/07 等数据集/AI 对比脚本适配属 D 包。禁止在 Model E
任何入口调用 `model.ensure_canonical`（Model D 自愈机制会破坏参数隔离，design D-13）。

## 快速入口

```text
Build:  cd model/simulink_modelE_(Phase4_Physics)
        eval(fileread('scripts/10_build_modelE.m'))
Gate:   eval(fileread('scripts/40_run_modelE_gate.m'))        % 物理提交门禁（AGENTS.md §12）
        eval(fileread('scripts/43_run_modelE_control_gate.m')) % 控制层 Gate（B 包 T-012）
Smoke:  eval(fileread('scripts/10b_smoke_modelE.m'))
单测:   matlab -batch "run('scripts/tests/test_hull_shallow.m')"   （test_* 均纯 MATLAB）
```

详细模型结构/EMS 逻辑/预测模型清单见 `README_Model.md`；参数证据链见
`docs/phase4/a2_model_e/modelE_parameter_evidence.md`。
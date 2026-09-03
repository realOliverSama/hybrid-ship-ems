# simulink_modelE_(Phase4_Physics)/ — Model E 工作区（Phase 4-A2）

> **入库策略**: 入库（`slprj*/`、`*.slxc`、codegen 等生成产物不入库）。

## 用途

**Model E——Phase 4「船舶物理升级」工作区**（spec `.specs/phase4-a2-model-e-construction`，分支
`feat/phase4-a2-model-e`）。自 Model D（`simulink_modelD_(EMS_NN_AECMS)`）**整个复制分叉**，
只升级 **Plant 物理**（T-403~T-411），EMS 七变体控制律**不修改即可运行**（A1 KEEP 决策）。
Model D 目录本体冻结只读（sha256 零改动纳入 Gate G1 持续证明）。

**500 t representative hull 口径**（`target_vessel_confirmed=false`，未确认工作假定）：
K_R=890 N/(m/s)²（线性 Δ 缩放，Q4 用户裁决）、几何 53.45/7.13/2.188 m/C_B=0.60（Rhine86 相似缩尺推导）。
**Model D 与 Model E 绝对结果不可比是设计意图**（Q3/Q4 裁决），只允许方向性/守恒性对照。

## 目录结构

| 条目 | 说明 |
|---|---|
| `config/ModelE_Physics_Parameters.m` | ★ **Model E 完整独立参数事实源**（OQ-8，单独 run 即定义全部参数；provenance 逐参数标记 + 文献 [1]~[17]；冻结敏感性档位登记） |
| `config/ModelE_TestScenarios.m` | ★ 测试工况（S01~S10 + S20 复合航次 + **浅水 H/T 六档 S31~S36** + VOYAGE_* 编译期变量 + GEN_CMD_STOP_TS/ADAPTER_FORCE_* 注入通道） |
| `config/ModelE_Sensitivity_Profile.m` | 敏感性档位选择器（nominal 零改动；Q1 冻结档位登记不扫掠） |
| `config/_legacy_reference/` | 复制分叉带入的 legacy config（Model_Parameters/Scenarios/GateThresholds/Ecms_Phase0_Config/*.mat/gate_baseline_smoke.json）——**不入路径、不参与运行**，仅证据对照（OQ-8） |
| `scripts/+model/` | ★ builder 与子系统源码：`build_model_e.m`（顶层构建器）、`code_hull_e/wake_ht_e/propeller_e`（船体浅水/w(H/T)/螺旋桨 block 源码）、`code_genset_unit_e/commitment_adapter_e/fuel_e`（双机组/占位适配器/燃油）、`battery_sop_limits/code_battery_ecm_e`（SOP/1RC ECM）、`write_config_manifest`/`sha256_file`（config manifest）、`export_structure_fingerprint`（结构指纹）、`run_scenario_sim`（水深注入仿真入口） |
| `scripts/+gate_e/` | ★ T-412 Gate：`check_g1~g6` 六类判据 + 测试台 runner（`run_dir_sim`/`analyze_e06`/`run_e07`）+ `gate_assign_bench_inputs`（确定性指令注入） |
| `scripts/tests/` | 单元/集成测试 harness（纯 MATLAB）+ 三链集成规格 notes + 锚点勘误登记 |
| `scripts/40_run_modelE_gate.m` | ★ **T-412 整体物理 Gate**（E01~E10 测试矩阵，输出 `results/gate_result.json`，FAIL 时 error('GATE:*') 非零退出；数字开头文件名须以 `eval(fileread(...))` 方式执行） |
| `scripts/41_run_prop_sensitivity.m` | 螺旋桨参数族 ±20% OAT 敏感性扫掠（15 cases） |
| `scripts/42_run_modelE_gate_negative.m` | Gate 负向验证（构造缺陷确认判据正确 FAIL） |
| `scripts/10_build_modelE.m` / `10b_smoke_modelE.m` | builder 入口 / 过渡态 smoke（law=0 载具） |
| `model/HybridShip_ModelE.slx` | builder 生成（脚本驱动，禁 GUI 修改） |
| `results/` | `gate_result.json`（overall + G1~G6 + negative_validation）、`config_manifest.json`（config sha256+有效值快照+Model D 参照哈希）、`prop_sensitivity.json`、结构指纹 |
| `reports/parameter_audit.csv` | builder 生成的参数审计（ParamMeta，40 项） |
| `datasets/` `python/` `legacy_reference/` | 自 Model D 分叉继承（D 包适配前保持原样；legacy_reference 为历史模型只读副本） |

**注意**：`scripts/01~07/10~66` 等编号脚本为 Model D 流水线继承件——Model E 只保证
**02 build / 03 smoke / 40 gate** 链路；05/07 等数据集/AI 对比脚本适配属 D 包。禁止在 Model E
任何入口调用 `model.ensure_canonical`（Model D 自愈机制会破坏参数隔离，design D-13）。

## 快速入口

```text
Build:  cd model/simulink_modelE_(Phase4_Physics)
        eval(fileread('scripts/10_build_modelE.m'))
Gate:   eval(fileread('scripts/40_run_modelE_gate.m'))        % 提交门禁（AGENTS.md §12）
Smoke:  eval(fileread('scripts/10b_smoke_modelE.m'))
单测:   matlab -batch "run('scripts/tests/test_hull_shallow.m')"   （6 个 test_* 均纯 MATLAB）
```

详细模型结构/EMS 逻辑/预测模型清单见 `README_Model.md`；参数证据链见
`docs/phase4/a2_model_e/modelE_parameter_evidence.md`。
# 边缘AI自适应ECMS甲醇EMS — Phase 0 Tasks

Spec: `adaptive-ecms-methanol`
Status: tasks-approved (v1，2026-08-24 用户批准；用户已授权执行任务委派 subagent)
Created: 2026-08-24
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`（v1.1 已批准）
Design: `./design.md`（v1.1 已批准）

## Implementation Plan

施工总纪律：冻结目录（`model/simulink/`、`model/simulink_modelC/`、`model/simulink_modelC_(oracle_ms)/`）全程只读；全部工作于分支 `feat/adaptive-ecms-methanol`；大型 .mat 不入库；每个任务完成后立即勾选。

- [x] 1. 建立 modelD 独立工程副本
  - ID: T-001
  - Requirement(s): FR-001, NFR-001
  - Files/areas: `model/simulink_modelC/` → `model/simulink_modelD/`（排除 `.venv_tf`（符号链接复用）、`slprj`、`results`、`*.slxc`）；副本内 Model C 标识统一改 Model D（文件名+内部引用）；核对副本内 scripts 既有编号，确定 Phase 0 脚本编号段（设计建议 30 起，避让占用）
  - Validation: 副本可独立加载参数与场景配置（`Model_Parameters.m` + `Model_Scenarios.m` 运行无错）；`git status` 无冻结目录改动

- [x] 2. 真实参数修正 + BSFC 覆盖机制
  - ID: T-002
  - Requirement(s): FR-002
  - Files/areas: `model/simulink_modelD/config/Model_Parameters.m`（`DRAWING_Bat_Cap_kWh_perString`→535.68、`Bat_Cap_kWh`→1071.36、充放电限值→1050、BSFC 额定锚点 460→453，ParamMeta 来源/置信度同步）；`10_Fuel` 查表覆盖 hook（`config/Ecms_Phase0_Config.m` 提供覆盖曲线时优先）
  - Validation: 参数加载打印值与修正清单一一对应；`parameter_audit.csv` 中修正项来源 = `DRAWING: 20260526设备参数.docx`

- [x] 3. modelD smoke 门禁（含基线重建）
  - ID: T-003
  - Requirement(s): FR-003
  - Files/areas: `model/simulink_modelD/scripts/03_run_smoke_tests.m`、`config/gate_baseline_smoke.json`
  - Validation: smoke overall=true。**注意**：基线重建属 🟡 操作，执行前须用户单独批准
  - Escalate if: 参数修正导致 smoke 出现非基线漂移类 FAIL（说明修正破坏了模型行为，而非参数平移）

- [x] 4. BSFC 曲线族生成器
  - ID: T-004
  - Requirement(s): FR-004, FR-005
  - Files/areas: `model/simulink_modelD/scripts/+ecms/bsfc_family.m`、`31_build_bsfc_family.m`；输出 `results/ecms_phase0/bsfc/bsfc_family.mat` + PNG（一维三档叠加图、二维等高线图）
  - Validation: 自检全过——锚点偏差 <0.1%；[ξ_opt, 1] 区间单调不降；恶化档 d10>d25>d50≥0；加密采样无过冲（最优点为全域最小值）；二维 map 在转速维退化为 {1500} 时与一维曲线一致

- [x] 5. BSFC 锚点来源存档
  - ID: T-005
  - Requirement(s): FR-018
  - Files/areas: `docs/bsfc_anchors_sources.md`（新建）：453 来源 = docx 条目；427 来源 = 潍柴 WH17ME 官方产品页 URL + 访问日期 2026-08-24 + 检索路径；尝试存档页面 PDF/截图
  - Validation: 文档入库；URL 可达性记录；若页面失效，记录 Web Archive 快照链接

- [ ] 6. 负荷剖面导出（8 场景 Baseline 仿真）
  - ID: T-006
  - Requirement(s): FR-006, FR-007
  - Files/areas: `model/simulink_modelD/scripts/30_export_load_profiles.m` + `+ecms/load_profile.m`；输出 `results/ecms_phase0/profiles/<SCN>.mat`（P_demand 取自推进需求指令侧——降额上游，含 P_gen/P_bat/SOC/累计燃油 Baseline 参照轨迹；Baseline 燃油按 §5 注入同档一维 BSFC 曲线）
  - Validation: 8 场景（S01/S02/S03/S04/S07/S08/S09/S10）剖面齐备；能量闭合检查；导出日志无欠压降额事件（有则该剖面作废并标记，Escalate）

- [ ] 7. P_demand 因果隔离验证
  - ID: T-007
  - Requirement(s): FR-006
  - Files/areas: `model/simulink_modelD/scripts/+ecms/verify_demand_invariance.m`（EMS_MODE=0 vs EMS_MODE=1 双控制律重复提取）
  - Validation: 逐点偏差 < 0.1%（全 8 场景）；任一场景违反 → Escalate（说明需求被控制律/降额污染，剖面定义须重新设计）

- [ ] 8. DP 求解器核心
  - ID: T-008
  - Requirement(s): FR-008, FR-009
  - Files/areas: `model/simulink_modelD/scripts/+ecms/dp_solve.m`、`+ecms/dp_problem.m`、`+ecms/speed_cost_min.m`；两口径（连续/启停）；符号约定与终端对称带严格按 design §4.1
  - Validation: 玩具问题单测——手工可解的两步/三步问题，DP 给出理论最优；sign convention 单测（P_bat=0 → E 不变；放电必减、充电必增）

- [ ] 9. DP 内置正确性校验
  - ID: T-009
  - Requirement(s): FR-010
  - Files/areas: `model/simulink_modelD/scripts/+ecms/dp_checks.m`（能量闭合 <0.1%；Baseline 可行性预检 + 条件 DP≤Baseline 校验；SOC 贴底滑行非平凡性检查）
  - Validation: 对 T-008 玩具问题与 1 个真实场景试运行，校验项全部按预期触发/通过

- [ ] 10. 主矩阵运行（192 实例）
  - ID: T-010
  - Requirement(s): FR-012
  - Files/areas: `model/simulink_modelD/scripts/32_run_phase0_matrix.m`；输出 `results/ecms_phase0/runs/*.mat`（不入库）+ `runs.csv`（入库）
  - Validation: 192 实例全部完成；runs.csv 每行含完整配置列与指标列；总耗时记录（NFR-003 ≤ 4h）

- [ ] 11. 单因素敏感性扫描
  - ID: T-011
  - Requirement(s): FR-012
  - Files/areas: `model/simulink_modelD/scripts/33_run_singlefactor_sweeps.m`（R_gen ∈ {20,40,80} kW/s；Bat_Cap ∈ {643, 1071.36, 1500} kWh；C_start ∈ {1,2,5} kg（启停口径））
  - Validation: 扫描结果写入 runs.csv（标记 scan 类型）；趋势合理性人工抽查（爬坡越松/电池越大 → 节油不降）

- [ ] 12. DP 数值收敛性检查
  - ID: T-012
  - Requirement(s): FR-011
  - Files/areas: `model/simulink_modelD/scripts/34_dp_convergence_check.m`（S07：δSOC 0.005→0.0025，δP 20→10 kW）
  - Validation: 加密复算燃油差 < 0.5%；不通过 → 加密基准网格并重跑主矩阵（Escalate 并记录）

- [ ] 13. 二维 map 扩展分析（不进主判定）
  - ID: T-013
  - Requirement(s): FR-004, FR-012
  - Files/areas: 复用 `+ecms/speed_cost_min.m` + `32_run_phase0_matrix.m` 的扩展模式；输出独立子目录 `results/ecms_phase0/ext_2d/`
  - Validation: 输出与主矩阵隔离（文件名/目录明确标记 EXTENSION）；报告中标注定速主判定 vs 可变速扩展的差异

- [ ] 14. Phase 0 GO/NO-GO 机器判定
  - ID: T-014
  - Requirement(s): FR-013
  - Files/areas: `model/simulink_modelD/scripts/35_phase0_gate_judge.m`；输出 `results/ecms_phase0/gate_result.json`
  - Validation: JSON schema 完整（overall/tier/per_scenario/config_hash）；三档判定逻辑用构造数据单测（各档边界情形）；NO-GO 措辞限定检查

- [ ] 15. 复现性验证
  - ID: T-015
  - Requirement(s): FR-016
  - Files/areas: 重跑 `32_run_phase0_matrix.m`（同 config）→ `runs_repro.csv` 对比
  - Validation: 两次运行 summary 指标逐位一致（或文档化容差内）

- [ ] 16. Phase 0 研究报告
  - ID: T-016
  - Requirement(s): FR-014
  - Files/areas: `model/simulink_modelD/reports/ECMS_phase0_upper_bound.md`（+ `36_make_phase0_report.m` 生成汇总图表）
  - Validation: 报告含全部必需节（方法/参数修正清单/曲线族/主矩阵/敏感性/GO 证据/局限/Phase 1 建议）；主 Agent 审阅后提交

- [ ] 17. 收尾验收
  - ID: T-017
  - Requirement(s): FR-015, FR-017, NFR-001, AC-1~AC-9
  - Files/areas: 全 spec
  - Validation: AC-1~AC-9 逐项核验；`git status` 无冻结目录改动；无大体积产物入库；spec 收尾 implementation-complete

## Verification Checklist

- [x] Requirements IDs are referenced by tasks.（FR-001~018、NFR-001/003 均有映射）
- [x] Design testing strategy is represented by tasks.（玩具问题单测 T-008、条件校验 T-009、收敛 T-012、因果隔离 T-007、复现 T-015、门禁 T-003/T-014）
- [x] Each task has a clear validation command or manual check.
- [x] Rollback/migration tasks are included when applicable.（T-003 基线重建已标注 🟡 批准要求；副本制天然隔离，删除 modelD 即回滚）

## Execution Notes

- 执行顺序即编号顺序；T-005 可与 T-004 并行；T-013 仅需主矩阵完成后即可，与 T-014/015 无强依赖。
- T-003 的 gate 基线重建是 🟡 操作，届时需用户单独批准。
- 委派策略：T-001~T-015 为明确执行任务，可按 AGENTS.md §3.3 询问用户后委派 worker；T-016 报告与 T-017 验收由主 Agent 负责。

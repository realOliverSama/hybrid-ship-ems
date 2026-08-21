# 面向参数不确定性的多步负荷预测增强型 EMS Tasks

Spec: `multistep-predictive-ems`  
Status: implementation-complete — Phase 1 NO-GO（2026-08-21 用户正式接受机器判定；gate_result.json verdict=NO-GO）  
Created: 2026-08-20  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`（v1.2, approved）  
Design: `./design.md`（v1, approved）

## Implementation Plan

范围说明：本任务表仅覆盖 **Phase 1**（FR-001~014）。Phase 2/3（FR-015~019）在 Phase 1 GO 后另行细化任务。

- [x] 1. 建立独立工程副本 `simulink_modelC_(oracle_ms)/`
  - ID: T-001
  - Requirement(s): FR-001
  - Files/areas: `model/simulink_modelC_(oracle_ms)/`（全量复制自 `model/simulink_modelC/`，排除 `slprj/ results/ datasets/`）；`.slx` 重命名为 `HybridShip_ModelCms_DCBus.slx`；`scripts/+model/build_model_cms.m`（由 build_model_c.m 派生）；`20_build_modelCms.m`、`20b_smoke_modelCms.m`；内部所有 `simulink_modelC` 字符串引用替换
  - Validation: 运行 `20_build_modelCms.m` 从零重建 + `20b_smoke_modelCms.m` 全 PASS；临时 rename `model/simulink/` 与 `model/simulink_modelC/` 后仿真仍正常（AC-1 路径独立性）

- [x] 2. 发电机三参数 config 化与扫描覆写机制
  - ID: T-002
  - Requirement(s): FR-002, FR-011
  - Files/areas: `config/Model_Parameters.m`（登记 `P_load_design_max=800`、`MSFF_*` 系列参数，标注假设等级）；实验脚本覆写 `Gen_P_max / Gen_Ramp_Up/Down / ASSUMED_Gen_TimeConstant` 的机制（confirm 不触发结构重建，FastRestart 兼容）
  - Validation: 修改 `Gen_Ramp_Up` 后模型行为变化（斜坡时间戳偏移）且无需 rebuild；参数登记表包含全部新增项

- [x] 3. oracle 多步预测通道
  - ID: T-003
  - Requirement(s): FR-003, FR-004, FR-009
  - Files/areas: `+model/make_oracle_ms.m`（由 C0 基准运行负荷记录生成 N×13 `[t, P_5..P_60]`，端点保持）；`ai_predict_ms` 块（固定 12 节点宽度 + `valid_node_count=H/5` + `pred_valid`）；`AI_Pred_Source=1` 数据格式扩展
  - **因果性硬约束**：oracle SHALL 取自**外生需求负荷**（欠压降额/EMS 作用之前的负荷请求值），oracle 目标对 C0/C_P/C_MS 控制律选择不变；若现有负荷记录取自降额后实际负荷，必须改取降额前信号——禁止闭环因果污染
  - Validation: 对 S04/S07 生成 oracle，抽查 t=100 s 处 12 节点值与负荷记录前向窗一致（误差 < 1e-6）；**信号取点验证：确认所取信号位于 08_Undervolt_Derate 上游**（用降额激活时段的数据证明 oracle 序列不受 Vdc 影响）；`MSFF_Horizon_s` 改 config 即切换有效节点数；缺失数据触发 `pred_valid=0`

- [x] 4. EMS_MinFF 第四变体实现
  - ID: T-004
  - Requirement(s): FR-005, FR-006, FR-007, FR-008, FR-013A
  - Files/areas: `+model/build_model_cms.m` 新增 `EMS_CTRL_LAW=3` 变体 `EMS_MinFF`（沿用 EMS_ScheduledFF 插线模式）；`min_ff` MATLAB Function 块（可达性前向递推 dt_r=0.1 s、对分法 20 迭代、防抖 5 kW、`MSFF_InfoMode` peak/traj 折叠）；`P_bat_allowable` 双折点系数
  - Validation: 变体编译通过；`EMS_CTRL_LAW=0/1/2` 行为与冻结基线逐点一致（回归，容差 1e-6）；结构差异检查记录

- [x] 5. 单元验证 U1–U6
  - ID: T-005
  - Requirement(s): FR-005~009, FR-013A（AC-2）
  - Files/areas: `20c_unittest_minff.m`；`results/ms_unittest/`
  - Validation: U1 可达性 vs 解析解 <1 kW；U2 R(τ)≤0 时与 Baseline 逐点一致；U3 合成阶跃下 P_ff*=手算最小值 ±0.5 kW；U4 peak 折叠与手算一致；U5 折点与单调性；U6 失效退化+计数。全 PASS 才允许进入实验阶段

- [x] 6. 指标体系扩展与结果工具链
  - ID: T-006
  - Requirement(s): FR-012, FR-013
  - Files/areas: `+model/` 指标脚本扩展——`E_bat_throughput_kWh`、`ff_active_s`、`R_max` 序列；**欠压双阈值分离**：`N_UV/T_UV @ 675 V`（真实欠压降额线，FR-014 非劣化约束⑤以此判定）与 `N_stress/T_stress @ 700 V`（母线压力区/动态裕度分析），禁止混用命名；`results/ms_phase1/runs.csv` 写入器；区域图绘制脚本（λ_P×R_gen 热力图：ΔP_bat,peak% / ΔVdc_min / Δfuel%）
  - Validation: 用既有 `results/phase1/` 数据离线回放，新增指标与手算抽查一致；热力图脚本在合成数据上出图

- [x] 7. Phase 1A：horizon×锚点×三对照实验（78 次仿真）
  - ID: T-007
  - Requirement(s): FR-010, FR-013A（AC-3）
  - Files/areas: `21_run_ms_horizon.m`；`results/ms_phase1/phase1A/`
  - Validation: 78 次运行全部落盘（3 锚点 × {S04,S07} × (1 C0 + 6 C_P + 6 C_MS)）；每锚点 C0 与冻结基线同场景 C0 回归比对（Nominal 容差 1e-6）；失败运行记录 NaN 不静默丢弃

- [x] 8. Phase 1A 判读与 horizon 选型
  - ID: T-008
  - Requirement(s): FR-010（AC-3）
  - Files/areas: `reports/MSFF_phase1A_horizon.md`
  - Validation: 报告给出 C_P vs C_MS 差值（RQ1 初步）、收益-H 曲线形态判读、2~3 个入选 horizon 及数据依据；**选型结论需主 Agent 审阅确认后才进入 T-009**（Escalation 点）

- [x] 9. Phase 1B：参数域扫描
  - ID: T-009
  - Requirement(s): FR-011, FR-012, FR-013（AC-4）
  - Files/areas: `22_run_ms_paramsweep.m`；`results/ms_phase1/phase1B/`
  - Validation: 主图 25 组合 × 4 场景 × 入选 horizon × (C0+C_MS)；τ_gen/SOC₀ 单因素各 36 次；C_P 仅于入选 H* 随同；**预算参数化：入选 2 个 horizon ≈472 次、3 个 ≈572 次，由 T-008 决策决定；C0 跨 horizon 复用不重复运行**；区域图产出（AC-4）

- [N/A] 10. 重复性分析与折点敏感性抽查 —— **WAIVED / NOT APPLICABLE BY GATE DECISION（2026-08-21 用户裁决），非实验 PASS**
  - ID: T-010
  - 豁免理由：① 重复性要求已由 v5 复现性硬门禁覆盖并超越（跨进程逐位一致 R1b/R2b PASS + 顺序无关 R2a + 冻结回归 R4 max-rel-diff=0，见 `results/ms_phase1/repro/v5_compare_20260821_082030.log`），优于原定的“3 次重复 SOC_end 极差 <0.002”；② 折点敏感性抽查的目的是确认“收益符号不随折点翻转”——实测域内全零收益（真实零权限域），无符号可翻转，敏感性分析 moot；③ 本豁免不改变任何 Gate 阈值，NO-GO 结论不依赖被豁免项
  - （原 Validation 条款保留备查：代表配置 3 次重复 SOC_end 极差 < 0.002；折点 ±50% 抽查不翻转）

- [x] 11. GO/NO-GO 机器判定与 Phase 1 报告
  - ID: T-011
  - Requirement(s): FR-014（AC-5）
  - Files/areas: `23_ms_gate_judge.m`；`results/ms_phase1/gate_result.json`；`reports/MSFF_phase1_gate_report.md`
  - Validation: gate_result.json 含非劣化约束与收益判据逐项 PASS/FAIL + 证据链接；FAIL 时 `error('GATE:MSFF:*')`；报告含 RQ1/RQ2 回答、有效工作区域图、NO-GO 时的否定性结论框架；**判定结论需主 Agent 与用户确认**

## Verification Checklist

- [x] Requirements IDs are referenced by tasks（FR-001~014 全覆盖；FR-015~019 显式排除并注明 GO 后细化）。
- [x] Design testing strategy is represented by tasks（T-005 单元 / T-007 回归 / T-010 重复性+敏感性 / T-011 Gate）。
- [x] Each task has a clear validation command or manual check。
- [x] Rollback：`simulink_modelC_(oracle_ms)/` 为纯新增目录，删除即回滚；T-004 含旧变体回归验证。

## Closeout 记录（2026-08-21）

- **T-009 lite 化**：经用户裁决由全量 472 次改为 64 次稀疏参数域确认（16 点 × S07/S09 × C0/CMS，H=45，边界点 H60 补测）——目标从“寻找有效区域”改为“验证 T-008B 包络分析预测的合理域内有效区域为空”，已确认（14/16 严格零 + 2 角点噪声瞬态）。
- **T-008B 机理门禁**（T-008 补充验证，用户裁决）：最弱点 W1 全零 + 1900 kW 正对照证明控制权限真实存在 → 零收益为系统域属性。详见 `reports/MSFF_phase1A_horizon.md` §8。
- **证据链保留**：正对照 `t008b/positive_control_S07_H45.mat`、T-008B 包络分析、64 次稀疏确认、`gate_result.json` 全部入库。
- **关闭范围**：仅关闭 multistep-predictive-ems / EMS_MinFF 研究线；不代表论文整体或边缘 AI EMS 方向关闭。Phase 2/3 未启动，等待下一轮研究路线决策。

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run implementation-readiness validation before coding.
- When coding starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering.
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.
- T-008 与 T-011 为主 Agent 决策点，不得由执行者自行越过。
- 执行模式（用户已裁决）：主 Agent 监督 + 单个 implementation subagent 顺序施工；禁止多 subagent 并行操作同一 Simulink 工程；T-003/T-004 实现后主 Agent 审查 oracle 因果性与结构；T-010 异常不得自行修改 Gate。
- Simulink 红线适用：模型全部脚本驱动；Gate FAIL 不允许提交。

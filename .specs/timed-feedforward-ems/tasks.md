# Model C: 定时前馈 EMS（Peak + Time-to-Peak 预测调度） Tasks

Spec: `timed-feedforward-ems`  
Status: implementation-in-progress
Created: 2026-08-19  
Requirements: `./requirements.md`（approved） · Design: `./design.md`（approved）

执行策略：主 agent 负责监督、关键设计裁决与最终验收；实现与批量实验委派给 worker subagent（用户已授权）。每个任务完成后立即 spec_task_check。

---

## Phase 0 — 工程副本验收

- [x] **T-001** simulink_modelC 构建与冒烟验证（FR-001, AC-1）
  - ID: T-001
  - Requirement(s): FR-001, AC-1
  - 文件：`model/simulink_modelC/scripts/10_build_modelC.m`、`+model/build_model_c.m`
  - 步骤：在 `model/simulink_modelC` 下运行 `eval(fileread('scripts/10_build_modelC.m'))` 从零重建 `HybridShip_ModelC_DCBus.slx`；运行 `10b_smoke_modelC.m` 通过冒烟测试；确认 MATLAB 路径中移除 `model/simulink` 后仿真仍正常（路径依赖验证）
  - 验收：构建无报错；冒烟测试通过；grep 无 `HybridShip_ModelB` 残留（legacy_reference 除外）；`.venv_tf` 符号链接有效
  - 执行：subagent（worker）

## Phase 1 — 控制律实现与验证

- [x] **T-002** ai_predict 三输出接口扩展（FR-007；design §2.2）
  - ID: T-002
  - Requirement(s): FR-007
  - 文件：`+model/build_model_c.m` 内 `CODE_AI_PREDICT` → 改 `CODE_AI_PREDICT_C`；`configureLogging` 更新端口记录
  - 步骤：输出 `[Ppred, valid]` → `[Ppred, t_peak, valid]`；Source=1 支持 N×3 `[t, P_peak, t_peak_rel]` 双列插值；新增 `AI_TPeak_Source` 参数（0=自带，1=oracle 表第3列）；Source=3 幅值推理保留（t_peak 暂输出 0，由 AI_TPeak_Source=1 覆盖）
  - 验收：`EMS_CTRL_LAW=1`（C1）+ Source=0/1 仿真行为与改造前 C1 完全一致（同一 oracle 表第 2 列下 P_gen_ref 轨迹逐点相同）
  - 执行：subagent

- [x] **T-003** EMS_CTRL_LAW 三变体切换（FR-002；design §2.1）
  - ID: T-003
  - Requirement(s): FR-002
  - 文件：`+model/build_model_c.m`（`buildEMSControllerB` → `buildEMSControllerC`）、`emsPortMap`
  - 步骤：VariantControl 改为 `EMS_CTRL_LAW==0/1/2`；新增第三变体 `EMS_ScheduledFF`（空壳先接通 baseline 直通）；构建时同步 `EMS_MODE = EMS_CTRL_LAW>0`；副本脚本内 EMS_MODE 引用改为 EMS_CTRL_LAW
  - 验收：三种 law 下模型均可更新图（Ctrl+D）并仿真；`ems_mode` 日志列正确
  - 执行：subagent

- [x] **T-004** sched_ff 定时门控实现（FR-003/004/005/006；design §2.3）
  - ID: T-004
  - Requirement(s): FR-003, FR-004, FR-005, FR-006
  - 文件：`+model/build_model_c.m`（EMS_ScheduledFF 变体完整构建）
  - 步骤：实现 design §2.3 的 1 Hz 事件锁存逻辑（ΔP 计算、T_ramp、SOC 偏置、立即退化、t_fire 触发、锁存期只增不减）；下游复用 gate/load_ff/pick/clamp 模式；记录 `P_ff_kW`、`t_fire_latched`
  - 验收：代码评审通过（主 agent）；静态检查参数引用均已登记
  - 执行：subagent 实现，主 agent 评审

- [x] **T-005** 参数登记与工具函数（NFR-004；design §3/§4.1/§4.4）
  - ID: T-005
  - Requirement(s): NFR-004
  - 文件：`config/Model_Parameters.m`（副本）、`+model/make_oracle_pred.m`（新）、`+model/tff_metrics.m`（新）、`configureLogging` 增 `P_gen_rampout_kW`
  - 步骤：登记 ASSUMED_TFF_* 三参数 + AI_TPeak_Source；make_oracle_pred 从 C0 logsout 生成 N×3 oracle 表（尾部截断处理）；tff_metrics 输出 fuel/Vdc_min/SOC/电池/t_sat/未服务能量/船速 RMSE
  - 验收：对 S04 跑 C0 生成 oracle 表，抽查 3 个时刻的 P_peak/t_peak 与手工计算一致；tff_metrics 字段完整
  - 执行：subagent

- [x] **T-006** C2 单元级行为验证（AC-2；FR-003/004/005/006）
  - ID: T-006
  - Requirement(s): AC-2, FR-003, FR-004, FR-005, FR-006
  - 文件：`scripts/10c_unittest_schedff.m`（新）
  - 步骤：合成场景（恒定负荷 + 人工 oracle 表注入单峰）：①正常定时触发（P_gen_ref 在 t_fire 前=baseline，之后以 40 kW/s 爬升，t_peak 前到位）；②t_peak < T_ramp → 立即响应；③SOC<40% → t_start 提前 25%；④valid=0 / 非全自动 → P_ff=0
  - 验收：四种行为全部符合预期（脚本自动断言 + 图存 `results/schedff_unittest/`）
  - 执行：subagent 实现，主 agent 评审断言逻辑

- [x] **T-007** E0–E3 实验与 τ_margin 扫描（FR-008；design §4.2；AC-3）
  - ID: T-007
  - Requirement(s): FR-008, AC-3
  - 文件：`scripts/12_run_phase1_control_law.m`（新）
  - 步骤：场景 {S04,S06,S07,S09} × {E0,E1,E2,E3} 全航程仿真；E2 附 τ_margin ∈ {5,10,15,20}（仅 S04/S06）；输出 CSV/JSON + 对比图至 `results/phase1/`
  - 验收：16+8 次仿真全部完成；结果表字段完整；E1 vs E0  sanity check（完美预测即时前馈应改善 Vdc_min 或不劣于 C0）
  - 执行：subagent

- [x] **T-008** t_peak 敏感性分析（FR-009；design §4.3；AC-4）
  - ID: T-008
  - Requirement(s): FR-009, AC-4
  - 文件：`scripts/13_run_phase1_tpeak_sensitivity.m`（新）
  - 步骤：oracle 表第 3 列注入 δ ∈ {−20,−10,−5,+5,+10,+20} s，C2 × {S04,S06,S09}，18 次仿真；输出 Vdc_min/油耗 vs δ 曲线
  - 验收：曲线单调性合理（δ<0 方向 Vdc_min 退化）；报告附图
  - 执行：subagent

- [x] **T-009** 过门判定与 Phase 1 报告（FR-010；design §4.4；AC-3）
  - ID: T-009
  - Requirement(s): FR-010, AC-3
  - 文件：`reports/TFF_phase1_gate_report.md`
  - 步骤：三条件评估（四场景平均节油 ≥2%；每场景 Vdc_min_C2 ≥ 0.98×Vdc_min_C0；|ΔSOC_end| ≤ 0.05）；确定 τ_margin 终值；明确 GO/NO-GO
  - 验收：**主 agent 亲自审核数据并向用户汇报判定结果；NO-GO 则终止后续任务并出否定结论报告**
  - 执行：主 agent（不可委派）

## Phase 2 — 双输出模型（T-009 GO 后启动）

- [ ] **T-010** 数据集标签扩展与重导（FR-011；design §5.1）
  - ID: T-010
  - Requirement(s): FR-011
  - 文件：`scripts/05_export_dataset.m`（副本内扩展）、`datasets/`
  - 步骤：新增 `P_load_future_tmax_60s` 列（首个最大值相对秒数，末端 NaN）；Model C + C0 重跑导出 train/val/test；与旧 CSV 逐值比对 `P_load_future_max_60s` 列作为回归检查；更新 datasets/README.md
  - 验收：回归检查逐值一致；新标签列抽查正确；划分与旧版相同
  - 执行：subagent

- [ ] **T-011** 双输出模型训练（FR-011；design §5.2；AC-5）
  - ID: T-011
  - Requirement(s): FR-011, AC-5
  - 文件：`python/train_load_predictor_dual.py`（新，继承既有管线）
  - 步骤：MLP-dual / CNN1D-dual / TCN-dual 三模型 × 种子 {0,1,2}；λ 方差均衡 + {0.5,1,2}× 微调；统一超参数；输出 keras/TFLite/mat + report JSON
  - 验收：三模型收敛；report 含 ΔP MAE、t_peak MAE（条件 ΔP>50 kW）、低估 MAE、参数量
  - 执行：subagent

- [ ] **T-012** 部署评估与选型（FR-012, NFR-005；design §5.3；AC-5）
  - ID: T-012
  - Requirement(s): FR-012, NFR-005, AC-5
  - 文件：`python/export/`、`reports/TFF_model_selection_report.md`
  - 步骤：TFLite INT8 PTQ → ΔMAE_quant；ONNX 导出 → stedgeai analyze（不可用则 TFLite 算子覆盖静态分析并标注）；加权评分选型（MLP 5% tie-break 规则）
  - 验收：选型报告含完整指标表与明确赢家；主 agent 复核选型结论
  - 执行：subagent 执行，主 agent 复核

- [ ] **T-013** 端到端闭环回灌（FR-013；design §5.4；AC-6）
  - ID: T-013
  - Requirement(s): FR-013, AC-6
  - 文件：`scripts/14_run_phase2_e2e.m`（新）、ai_predict Source=3 双头扩展、`reports/TFF_phase2_e2e_report.md`
  - 步骤：MLP-dual 环内推理（W3→32×2）跑 C2；若赢家为 CNN/TCN 则用两遍法（C0 轨迹→离线推理→Source=1）；C0/C1/C2 端到端对照；与 Phase 1 oracle 上界差距分析
  - 验收：端到端报告完成；主 agent 审核并向用户汇报最终结论
  - 执行：subagent 执行，主 agent 终审

## 依赖关系

```
T-001 → T-002 → T-003 → T-004 → T-005 → T-006 → T-007 → T-008 → T-009 (GATE)
                                                                    ├─ NO-GO → 终止+报告
                                                                    └─ GO → T-010 → T-011 → T-012 → T-013
```

## 全局约束

- 所有工作仅在 `model/simulink_modelC/` 与 `.specs/timed-feedforward-ems/` 内进行；`model/simulink/` 只读封存
- 实验结果写入 `simulink_modelC/results/`，报告写入 `simulink_modelC/reports/`
- 每个 subagent 任务完成后主 agent 验证验收条件再 spec_task_check
- hooks.after_task `npm test` 不适用本项目（MATLAB 工程），忽略

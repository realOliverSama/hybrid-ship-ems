# EXECUTION-PATH DEFECT RECOVERY / SCOPE AUDIT — 交接报告（EP3 修复关）

**结论一句话**：正式 C\* 零结果已按裁决标记为 `IMPLEMENTATION_INVALID_ZERO_RESULT` 并保留原件；
影响范围审计判定 **Dataset-v3 / Predictor-v3 / C1 真值 / Mission 语料均不受污染**（决定性证据：
冻结语料推进降额从未激活），因此可以**沿用同一冻结语料**；执行路径修复、写入点审计与运行时
遥测已**实现**（代码 + 绑定表 + 冻结生成器）；**Q-EP3 非正式资格化尚未执行**，因为它必须在
真实 Model-E runtime 上跑，而本机 MATLAB 受用户既有指令限制、云端实例按本关禁令不得启动
⇒ **epoch-3 预执行冻结未生成**（生成器已 fail-closed 拒绝并写出 `..._BLOCKED.json`），
按要求 STOP 交人工门裁决。

## A. 科学状态更正（原文保留，仅新增分类）

| 项 | 处置 |
|---|---|
| 正式零结果（B_perfect=B_predicted=L_prediction=0.000 pp，CI [0,0]） | **IMPLEMENTATION_INVALID_ZERO_RESULT**（撤销此前 `VALID_NEGATIVE_ARCHITECTURAL_RESULT`） |
| 数值本身 | `CORRECT_FOR_THE_EXECUTED_DEFECTIVE_EXPERIMENT`；状态 `SUPERSEDED_FOR_SCIENTIFIC_EFFECT_INFERENCE`；**禁止**用作"预测无控制价值"的证据 |
| 原件 | epoch-1/epoch-2 证据、FC-3 replay、effect-analysis、bootstrap、control-path attribution **逐字节保留**（未删除、未改写；本关新增分类工件，不改历史文件） |
| FC-3 | **契约不撤销**（语义修正与源选择缺陷无关）；历史 192/192 `REJUDICATED_FEASIBLE` **仅**适用于 baseline 路由轨迹，不得外推 epoch-3 |
| 新纪元 | `formal_epoch = 3`（同一科学问题、修正后的执行纪元；非新估计目标） |

工件：`invalid_evidence_reclassification.json`

## B/C. 影响范围审计（只读）

| 研究工件 | PMS_SOURCE_MODE 值/来源 | EMS 本应驱动 plant | 依赖闭环状态 | 受影响分支能否改变它 | 分类 |
|---|---|---|---|---|---|
| Phase 4-F formal epoch-1/2（192 run + ctrace/traj/audit） | 0 / `scenario_compile.m:293` → `inject_voyage.m:12-15` | 是 | 是 | 是 | **SCIENTIFICALLY_INVALIDATED** |
| effect-analysis（B/L、bootstrap） | 继承 0 | 是 | 是 | 是 | **SCIENTIFICALLY_INVALIDATED** |
| FC-3 契约与 checker | 不适用 | 否 | 否 | 否 | UNAFFECTED |
| Dataset-v3 IID（576 航次 + windows npz） | 0（**设计如此**，meta 记录 `P2 bench, law=0`） | 否 | 否（实测无降额） | 否 | **AFFECTED_BUT_INTENDED** |
| Dataset-v3 OOD（200 航次） | 0（同上） | 否 | 否（实测无降额） | 否 | **AFFECTED_BUT_INTENDED** |
| Dataset-v3 目标 Y / E300 | 不适用（负荷真值） | 否 | 否 | 否 | UNAFFECTED |
| Predictor-v3（H1 11 + K/F 6；训练/HPO/winner/confirm） | 不适用（不读 H2，FR-582） | 否 | 否 | 否 | UNAFFECTED |
| C1 完美信息真值存储（`windows_v3_formal.npz::Y`） | 不适用 | 否 | 否 | 否 | UNAFFECTED |
| Mission-v3 语料（定义/seeds/split/tier/T_eval） | 不适用（仿真前生成） | 否 | 否 | 否 | UNAFFECTED |
| Phase 4-B/C 控制门（43/44） | **1**（`run_gate.m:181` 显式写臂值：R0 law0/src1、R5 law5/src1、P2 law0/src0） | 是 | 是 | 否 | UNAFFECTED |
| T-445 oracle gate | 1（`+t445/inject_scenario.m` 从不写该变量 ⇒ config 标称） | 是 | 是 | 否 | UNAFFECTED（且 `formal_corpus_observed=false`、`runs/` 为空 ⇒ 无结果性语料） |
| epoch-3 执行（未发生） | 1（新绑定层） | 是 | 是 | 是 | REQUIRES_FURTHER_EVIDENCE |

**决定性只读检验（derate-free 判定）**：预测目标与 H1 负荷特征与 EMS 的唯一耦合通道是
推进降额 `P_prop_limit = Mot_P_nom·clamp((V_dc−600)/75, 0, 1)`（起点 675 V）。

| 语料 | 样本数 | min V_dc | <675 V 采样 | 裕量 |
|---|---|---|---|---|
| IID（576 航次，197,448 窗） | 59,234,400 | **694.54 V** | **0（0.000%）** | **+19.54 V** |
| OOD（200 航次） | 842,288 | **696.01 V** | **0（0.000%）** | **+21.01 V** |

⇒ 冻结语料中推进降额**从未激活** ⇒ `P_load_total_kW`（预测目标、H1 负荷特征、C1 真值来源）
与 PMS 源选择/EMS 动作**无关** ⇒ Dataset-v3 目标、C1 存储、Predictor-v3 训练数据**均可沿用**。
残余注意：1 Hz 采样无法排除亚秒级凹陷（裕量 ~20 V 给出暴露上界）；修复后执行的 V_dc 须在
epoch-3 作为 train/runtime 一致性断言监控（本项列为 REQUIRES_FURTHER_EVIDENCE）。

工件：`impact_scope_audit.json`

### 特别例外说明（子代理审计的两条附带发现，事实登记，不改变分类）

1. 形式语料 stream 以 **1 Hz** 落盘（`member_exec.m:92-104`），而 producer schema 描述原生
   0.01 s；目标由 1 Hz 流计算。此为**先于**本缺陷存在的登记事项，交由 owner 处置。
2. Predictor-v3 工件**不记录**生成窗口时的 EMS 律/源模式（npz 仅含 voyage/anchor/split），
   修复后语料与冻结语料存在静默混用风险；已在审计中登记，epoch-3 冻结应绑定语料身份。

## D. 预测器 / 数据集专项

- 特征族分类：**EXOGENOUS/MISSION-DEFINED** 8 个（P_hotel、v_current、water_depth、ctrl_mode、
  v_ref、manual_cmd、u_fcst、wl_fcst、v_plan、h_chart、P_hotel_plan）＋ **PLANT-LOOP 5 个**
  （P_load_total、v_ship、accel、n_prop_rpm、P_prop_demand）——后者与 EMS 的耦合**只经降额**，
  实测语料从未降额 ⇒ 语义未变。
- 目标 Y：未来 10 s 负荷能量（`targets.py:64-90`），与 `PMS_SOURCE_MODE` 无关（同上）。
- H2 闭环通道（SOC/P_gen/P_bat/V_dc…）**不是**预测器输入（FR-582），仅存档。
- 结论：**无需重生成**；不触发 §D 的 STOP 条件。

## E/F/H. 执行路径修复（已实现）

1. **单一权威绑定阶段**（§E/§F）：`ppe.arm_runtime_bind(arm)` 在**通用 mission 注入之后**显式
   绑定全部控制关键变量并逐项**回读断言**（fail-closed）。绑定表由
   `ppe.control_critical_vars.m` 单一事实源给出（含源语义出处），禁止别处复制。
2. **段级守卫**（§F）：`ppe.assert_control_bindings(rec)` 由 `run_cstar` 的 `BindingGuard`
   选项在**每段开头**调用（`run_cstar.m` 附加参数，缺省为空 ⇒ 历史语义逐位不变）；
   任何"后来者"改写关键绑定 ⇒ `PPE:ARMBIND:DRIFT` 立即终止。
3. **运行时源选择遥测**（§H）：`model.set_source_selection_logging` 以**内存内**方式打开冻结
   模型中既有输出端口的记录（`P_gen_ref_kW` / `P_gen_req_baseline_kW` / `P_gen_req_eff_kW` /
   `selector_baseline_active` / `P_gen_unserved_kW` / `P_gen_surplus_kW` /
   `out_baseline_fallback_active` / `ems_health_ok`），`ppe.write_sourcesel_v1` 落盘
   1 Hz 端点值 + 秒内 min/max 的独立工件（新 schema `ppe_sourcesel_v1`），**植物轨迹契约
   `ppe_plant_traj_v1` 不变**（FC-3 消费面不变）。不修改、不保存 .slx。

### P0/C0/C1/C2 运行时绑定表

| arm | EMS_CTRL_LAW | PMS_SOURCE_MODE | EMS_MODE | EMS_WORK_MODE | AI_Pred_Source | ADAPTER_FORCE_MODE | EMS_HEALTH_TS | forecast_source | controller identity |
|---|---|---|---|---|---|---|---|---|---|
| P0 | 5 | **1** | 1 | 2 | 0 | 0 | [0 1;1e6 1] | N/A_NO_FORECAST | law5_soc_adaptive_ecms |
| C0 | 6 | **1** | 1 | 2 | 0 | 0 | [0 1;1e6 1] | oracle（因果持续性窗） | law6_oracle_ecms |
| C1 | 6 | **1** | 1 | 2 | 0 | 0 | [0 1;1e6 1] | c1_stored（冻结完美信息） | law6_oracle_ecms |
| C2 | 6 | **1** | 1 | 2 | 0 | 0 | [0 1;1e6 1] | live_bridge（Predictor-v3） | law6_oracle_ecms |

源语义推导（非猜测）：`PMS_SOURCE_MODE = 1 → EMS_AGGREGATE`（EMS 参考值进入 dispatch），
`0 → BASELINE_POLICY`（EMS 输出丢弃）；判据点 `07_PMS_v2/Source_Selector`
`cmp_mode_baseline = (PMS_SOURCE_MODE < 0.5)`；同一变量还是 `16_Safety_Interlock` 的
`nominal_is_ems`（决定 S5 fallback 锁存是否使能）。四臂实验意图均为"EMS 律驱动 plant"
⇒ 一律 1；baseline 分支保留给 S5 安全 fallback。**无运行时默认值**（绑定阶段必须显式写）。

## G. 注入链冻结（写入点审计完成，冻结待 epoch-3）

`PMS_SOURCE_MODE` 全部写入者（12 处，含 file:line）与 `EMS_CTRL_LAW` 关键写入者（5 处）见
`runtime_writer_audit.json`。**未被任何既有冻结绑定**的写入者已逐条列出（`inject_voyage.m`、
`run_scenario_sim.m`、`+gate_e/*`、`+gate_ctrl/*`、`+fleet_v3/member_exec.m`、
`+mission_v3/+audits/feasibility_sweep.m`、`+ppe/qual_*.m`）。epoch-3 冻结生成器
（`python/ppe/ep3_freeze.py`）已把这些写入者列为**必须绑定**项，任一缺失 ⇒ `overall=false`。

另登记两项结构性风险：`EMS_CTRL_LAW` 的配置默认值为 **0（Baseline）**（忘写即跑 baseline 律）；
`build_model_e.m:101-102` builder 结束会把 base 留在 `law=7`。

## I. Q-EP3 非正式资格化 —— **未执行（运行时阻断）**

已实现 `ppe.qual_ep3`（Q-EP3-1..15，真实 runtime path、合成短时域、fail-closed、不消费
formal/IID/OOD、不据以调参 C\*）：1/2 分支选择；3/4 不同 EMS 参考 → 不同选中参考 → 发电机可测响应；
5..8 四臂律与绑定；9 不同预测窗 → 不同 L5 动作出现在选中参考上（走 `ppe.run_cstar` 分段路径）；
10 通用注入不能改写绑定、绑定后改写被守卫捕获；11 未知臂/非预期值 fail-closed；
12 遥测如实报告分支；13 baseline fallback 在其安全条件下仍会触发；14 FC-3 checker 兼容；
15 不消费 formal/IID/OOD。

阻断原因：Q-EP3 必须在真实 Model-E runtime（MATLAB/Simulink）上执行，而——
- 本机 MATLAB 受用户既有指令限制（"不要在本地登录 MATLAB"）；
- 本关明令 **不得** 启动/租用云端正式执行；
- 仓库内无云端连接信息，无法远程执行。

⇒ 未执行，未伪造结果。`epoch3_pre_execution_freeze_BLOCKED.json` 记录：
`CONTROL_ACTION_REACHES_PMS = NOT_PROVEN`、`PMS_SOURCE_MODE_ARM_BOUND = false`，
`ALL_CONTROL_CRITICAL_WRITERS_FROZEN = true`、`FORMAL_EPOCH3_RUNS = 0`、
`IID_TEST_ACCESS = 0`、`OOD_ACCESS = 0`、`overall = false`。
**需要用户裁决**：授权本地 MATLAB 跑非正式 Q-EP3，或提供云端 runtime。

## J/K/L. 纪元与语料策略

- 新纪元 `formal_epoch = 3`（同一 Phase 4-F 科学问题；不因前一结果无效而改估计量）。
- **语料沿用**：48 航次 / 192 run 清单 / T_eval / split_group / tier / C1 真值全部保留，
  epoch-3 绑定**同一 corpus identity**；因运行时注入语义改变，另建新的执行/冻结身份。
- 效应分析协议（B_perfect / B_predicted / L_prediction / canonical FEC / group-tier 聚合 /
  bootstrap / R 守卫 / development-stage 标签）**原样保留**，不据零结果调参控制器。

## M. epoch-3 预执行冻结 —— **未生成**（前置条件未满足）

生成器 `python/ppe/ep3_freeze.py` 已实现（绑定修复提交、写入者审计、臂运行时清单、
注入链身份、Model-E/PMS/Safety、C\*、P0..C2、Predictor-v3、C1 存储、FC-3、语料、192-run
清单、FEC/统计规范、遥测、资格化工件），并在缺 Q-EP3 时 **fail-closed 拒绝**写出冻结。
**epoch-3 freeze root：不存在（未生成）**。

## 计数与纪律

| 项 | 值 |
|---|---|
| FORMAL_EPOCH3_RUNS | **0** |
| IID_TEST_ACCESS / OOD_ACCESS | **0 / 0**（ledger 按 purpose 计数；跨全史同样为 0） |
| Simulink 重跑（本关） | **无**（未启动任何仿真；无云端操作） |
| 修改的正式工件 | **无**（只新增分类/审计/修复代码工件） |
| epoch-2 复跑能力 | 修复改动 `run_cstar.m` 后，epoch-2 冻结的字节绑定会 fail-closed 拒绝复跑（预期且正确；epoch-2 已判定无效，其历史工件逐字节保留） |

## 待裁决事项（交人工/ChatGPT Epoch-3 门）

1. **Q-EP3 运行时**：授权本地 MATLAB（非正式、短时域）还是提供云端实例？这是 epoch-3 冻结的
   唯一未满足前置条件。
2. 语料沿用已由审计支持，但**修复后执行的 V_dc 裕量**（train/runtime 一致性）需在 epoch-3
   作为断言监控——若修复后 EMS 使 V_dc 跌破 675 V，则需回到本审计重判。
3. 登记事项：形式流 1 Hz vs schema 0.01 s；Predictor-v3 工件缺 EMS 生成身份。

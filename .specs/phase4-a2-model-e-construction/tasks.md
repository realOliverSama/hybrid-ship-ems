# Phase 4-A2 — Model E 施工段（T-403~T-412）Tasks

Spec: `phase4-a2-model-e-construction`  
Status: implementation-complete（2026-09-04 夜班 T-001~T-012 全部施工验证 + T-013 归档；AC-415 用户裁决 T-412 APPROVED / T-013 CONDITIONAL APPROVAL，4 项修正完成后执行冻结 v1.0）  
Created: 2026-09-04  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`  
Design: `./design.md`

> 任务编号 T-001~T-013 为本 spec 局部编号；每项标注路线文档
> `docs/phase4_research_roadmap.md` 的 T-4xx 映射与 FR 追溯。
> **执行 session 开工前必须研读**：brainstorm.md、requirements.md、design.md、
> A1 双审计报告（`docs/phase4/a1_model_audit/`）、`项目开发进展.md` 2026-09-04 条目
> （17 项参考文献证据链）。
> **第三轮 GPT 评审修订已并入**（2026-09-04 用户裁决）：clone-and-modify 维持（拒绝清零重建）、
> 参数独立成套、螺旋桨四 K 重标定、T-412 改专用 Plant 测试台、H/T 单一事实源。

## Implementation Plan

- [x] 1. 分支、工作区复制分叉与零改动基线
  - ID: T-001
  - Requirement(s): FR-410, NFR-407
  - Roadmap: 路线文档 §1.4 分支策略
  - Files/areas: 从 `Phase4` 开 `feat/phase4-a2-model-e`；复制
    `model/simulink_modelD_(EMS_NN_AECMS)/` → `model/simulink_modelE_(Phase4_Physics)/`；
    副本内 legacy config（`Model_Parameters.m`/`Model_Scenarios.m`/`Model_GateThresholds.m`/
    `Ecms_Phase0_Config.m`/既有 .mat 资产/`gate_baseline_smoke.json`）移入
    `config/_legacy_reference/` 隔离（不入路径不参与运行，design §2）；
    副本内模型与 builder 改名 ModelE 口径（`HybridShip_ModelE.slx`、`build_model_e.m`）；
    记录基线证据：`git status --short model/` + Model D 四个关键文件
    （`Model_Parameters.m`/`Model_Scenarios.m`/`build_model_c.m`/`.slx`）sha256
  - Validation: 工作区存在；Model D 目录 sha256 与 A1 附录一致；
    `git status --short model/simulink_modelD*` 为空
  - Notes: 本任务不做任何物理修改；隔离而非删除 legacy config（保留证据对照能力）

- [x] 2. Model E 独立参数事实源与 config manifest
  - ID: T-002
  - Requirement(s): FR-411, FR-412, NFR-410
  - Roadmap: T-402 收尾（回填）+ 全部施工任务的参数基础
  - Files/areas: 新建 `config/ModelE_Physics_Parameters.m`——**完整自包含**（禁止先 run
    legacy 再 override）：Model E 所需全部参数逐条声明 + provenance + 文献编号，
    含继承项（标 INHERITED_DRAWING/INHERITED_LEGACY + 证据出处 + 「待 B/F 包重评」注记）
    与新增项：Ship_Mass=5.0e5、Hull_K_R=890、Hull_Lpp/B/T/Sw/CB=53.45/7.13/2.188/554.2/
    0.60、rho_water=1000、浅水系数（Δk 0.644/−1.72；c1/c2/c3=1.1680/−0.5238/−1.472；
    k=0.2）、w_deep=0.10（Molland/Harvald 回归注明）+ Zhang Δw 表、效率链边界、
    DG P_min=125/P_opt=375/ramp=20/τ=5/t_start=25 s/t_sync=5 s/t_cooldown=30 s/
    C_start=0.6 kg、BSFC 五节点 {125:600,250:547,375:447,400:427,500:453}、
    电池 LFP 1RC 全 SOC-LUT（R_0/R_1/C_1/OCV 九节点）/Q_nom=1860 Ah/η_coul=0.98/
    电压边界 360~657 V/I_bat_limit_surrogate_A≈1823 A（DERIVED 注明非 BMS 实测）；
    新建 `config/ModelE_TestScenarios.m`（测试工况定义：深水默认 20 m + 浅水 H/T 六档
    {6.56,5.91,4.38,3.50,3.06,2.63} m + Plant 测试台指令剖面）；
    新建 `config/ModelE_Sensitivity_Profile.m`（冻结档位登记 + nominal 档零改动）；
    config manifest 机制（`results/config_manifest.json`：Model E 三文件 sha256 +
    有效值快照 + historical_reference_hashes=Model D 四文件 sha256）；
    A1 CSV 回填 user_value/user_reference
  - Validation: 参数文件**单独** run 即可定义 Model E 全部参数（无 legacy 依赖，
    用干净 workspace 验证）；provenance 无空缺；manifest 落盘；A1 CSV 回填完成
  - Notes: 数值以本 spec + design §2 + 开发日志 2026-09-04 条目为唯一事实源

- [x] 3. Scenario_Manager 水深输出
  - ID: T-003
  - Requirement(s): FR-414, FR-415
  - Roadmap: T-404（接口部分）
  - Files/areas: `build_model_e.m` 的 Scenario_Manager 构建函数新增 `water_depth_m`
    输出（由 Model E 测试工况字段驱动，编译期求值，与 v_cmd/水流剖面同机制）
  - Validation: 构建后模型存在该输出端口；深水工况输出 20 m；浅水工况输出设定值
  - Notes: resistance_multiplier 端口保留（默认 1.0，语义=通用阻力不确定度旋钮，
    浅滩语义退役——注释写明）；**不改名为 Environment_Input**（D-7：C 包才重构工况语义）

- [x] 4. 船体阻力重构与浅水增量（含 H/T 单一事实源与单元测试）
  - ID: T-004
  - Requirement(s): FR-412, FR-413, FR-415, NFR-411
  - Roadmap: T-403 + T-404（Plant 部分）
  - Files/areas: `build_model_e.m` 新 `Ship_Hull_Dynamics_E`（design §4.1 公式链：
    R_deep=res_mult·K_R·V_rel|V_rel|；Δr 由 Δk(H/T)+C_F*(H/T,Re) 相对增量；
    H/T>3 Δr=0；H/T<1.2 clamp+MODEL_DOMAIN_WARNING）；**H/T 只在此处计算一次**，
    `HT_ratio` 作为信号输出供螺旋桨模块使用；新增 R_shallow_N/HT_ratio/
    domain_warning 输出；`scripts/tests/test_hull_shallow.m` 单元测试
  - Validation: 单元测试通过（Δr 公式对 Zhang 表手算值、域边界行为、零速保护）；
    集成 smoke：静水 V↑⇒R↑、固定 V 下 h↓⇒R↑
  - Notes: ν=1.19e−6 m²/s；K_R=890 冻结（唯一事实源，禁止执行中重新论证）

- [x] 5. 螺旋桨四 K 重标定 + w(H/T) 信号化 + 效率链审计
  - ID: T-005
  - Requirement(s): FR-416, FR-417（部分）, FR-412 连带
  - Roadmap: T-405 + T-406（模型部分）
  - Files/areas: 按 design §4.2 推导链在 Model E 语境**重算 K_Fw/K_Fv/K_Qw/K_Qv**
    （11 kn 设计点 + w_deep=0.10 → V_A_design=5.09 m/s；推导过程写入参数文件注释）；
    `Propeller_Hydrodynamics` 的 wake_fraction 改输入信号；新增 `Wake_vs_HT` 模块
    （w = w_deep + Δw_Zhang(H/T)，linear 插值 clamp [1.2,2.7]，**输入为 T-004 的
    HT_ratio 信号**）；效率链审计（Elec_Eff=η_inv×η_motor×η_shaft 不含齿轮箱，
    每因子出现恰好一次）写入证据链文档；`scripts/tests/test_propeller_e.m`
  - Validation: 单元测试（重标定值手算对照、w(H/T) LUT 端点/插值/域外 clamp）；
    模型编译；效率链审计落盘
  - Notes: **禁止沿用 Model D 的 K 推导值**（D-8：w 变更后旧 K 不满足设计点平衡）

- [x] 6. 双发电机组 DG1/DG2 与启停状态机
  - ID: T-006
  - Requirement(s): FR-418, FR-419, FR-421
  - Roadmap: T-407 + T-408
  - Files/areas: `build_model_e.m`：替换 06_Genset_Average_Plant 为 `06_Genset_System`
    （DG1_Plant/DG2_Plant，Plant 真实输入 = DG1_enable/P_DG1_ref/DG2_enable/P_DG2_ref；
    各机组：状态机 OFF→STARTING(2500 ticks)→SYNC(500 ticks)→ON→COOLDOWN(3000 ticks)
    →OFF，整数 tick 判据；aborted-start=不可中断+pending_stop；ON 态 clamp
    [125,500] kW + below_min_flag；ramp 20 kW/s；一阶 τ=5 s）；
    `Legacy_Commitment_Adapter` 为 Plant 之外的可拔插模块（EMS 聚合 P_gen_ref →
    双机 enable/ref）；P_gen_actual=P1+P2 接线至 13_DC_Bus/09_Bus_Residual_Monitor；
    `scripts/tests/test_genset_statemachine.m`
  - Validation: 单元测试：状态机 tick 精确转移；aborted-start pending_stop 行为；
    **DG2 启动期间缺口测试**（ref 800 kW、DG2 STARTING → DG2 实际输出=0、
    adapter 不虚构功率、缺口由电池/母线响应）；
    集成：adapter 规则（≤500→DG1；>500→DG1=500+DG2 余量）验证
  - Notes: MATLAB Function persistent 实现（不用 Stateflow）；无 min up/down（D-4）；
    PLACEHOLDER 永久标注

- [x] 7. per-genset 燃油模型
  - ID: T-007
  - Requirement(s): FR-420
  - Roadmap: T-409
  - Files/areas: `build_model_e.m` 重构 10_Fuel_Consumption：双路独立
    BSFC PCHIP（节点 {125:600,250:547,375:447,400:427,500:453}，域守卫 ON 且
    P≥125 才查表）+ STARTING/SYNC 均摊 C_start=0.6 kg（只算一次）+ OFF/COOLDOWN
    fuel=0；输出 fuel_1_kg/fuel_2_kg/fuel_total_kg；
    `scripts/tests/test_fuel_model.m`
  - Validation: 单元测试（PCHIP 值对照、域守卫、启动燃油一次性 0.6±0.01 kg）；
    集成：1×500 kW 与 2×250 kW 同负荷燃油差 >1%（经济性可区分）
  - Notes: 禁止 P<P_min 外推（FR-420 红线）

- [x] 8. 电池 1RC ECM
  - ID: T-008
  - Requirement(s): FR-422, FR-423, NFR-409, NFR-411
  - Roadmap: T-410
  - Files/areas: `build_model_e.m` 重构 14_Battery_Converter（design §4.5 链：
    droop→SOP clamp→terminal 换算→解析求 I=2P_t/(E+√(E²−4R_0P_t))→V_t/V_1/SOC）；
    SOC 电荷积分（放电 −I/Q_nom，充电 −η_coul·I/Q_nom）；
    D<0 → P_actual=P_max,R + battery_power_limited/battery_discriminant_limit；
    **SOC∈(0.9,0.95] 时 LUT clamp 到 0.9 节点 + `ecm_lut_boundary_flag`**；
    新增 V_bat/I_bat 输出；`scripts/tests/test_battery_ecm.m`
  - Validation: 单元测试（解析解手算对照、判别式边界、SOC 方向、LUT 边界 clamp）；
    集成：放电 V_t<OCV、充电反向、零 NaN
  - Notes: I>0=放电；ASSUMED_Bat_Eff_* 定义保留（ECMS k_eq 引用，INHERITED_LEGACY）
    但 Plant 侧停用

- [x] 9. Battery SOP 四约束
  - ID: T-009
  - Requirement(s): FR-424
  - Roadmap: T-411
  - Files/areas: `build_model_e.m` SOP 计算（P_SOP=min(P_current,P_voltage,P_SOC,
    1050 kW bus-side)，design §4.6；P_current 用 `I_bat_limit_surrogate_A` 命名；
    terminal 换算方向感知）；P_bat_dis_avail/chg_avail 端口同名同义对接；
    `scripts/tests/test_battery_sop.m`
  - Validation: 单元测试四约束各自起控点（注入测试）；集成：SOP 收缩时 droop
    输出被正确限幅
  - Notes: 依赖 T-008 完成

- [x] 10. builder 集成、Plant 接口定义与七变体编译
  - ID: T-010
  - Requirement(s): FR-426, NFR-405
  - Roadmap: T-403~T-411 集成
  - Files/areas: `build_model_e.m` 全链路集成；`model/HybridShip_ModelE.slx` 生成；
    Plant 控制接口与输出清单落盘（design §4.7 + 证据链文档：推进指令/DG enable×2/
    P_ref×2 ↔ v_ship/P_load/Vdc/SOC/Vbat/Ibat/Pgen1/Pgen2/DG state/SOP 上下限）；
    结构指纹导出脚本（find_system → JSON）
  - Validation: 七变体（law=0/1/2/4/5/6/7）全部 compile 成功；law=0/law=5 经
    Legacy_Commitment_Adapter 兼容 smoke 无错误；builder 连建两次结构指纹逐位一致；
    无代数环诊断
  - Notes: 依赖 T-003~T-009 全部完成；兼容 smoke 不作物理判据（D-9）

- [x] 11. 螺旋桨 ±20% OAT 敏感性扫掠
  - ID: T-011
  - Requirement(s): FR-417
  - Roadmap: T-406（验证部分）
  - Files/areas: 新建 `scripts/41_run_prop_sensitivity.m`：**OAT**（一次一参数 ×
    {0.8,1.0,1.2}，K_Fw/K_Fv/K_Qw/K_Qv/w_deep 共 15 cases，非 full factorial），
    作用于 T-005 重标定后的 Model E nominal；静水工况 speed-power 与油耗影响；
    机器可读 JSON + `docs/phase4/a2_model_e/prop_sensitivity_report.md`（结论稳健域
    保留块）
  - Validation: 15 cases 全部跑通零失败；报告含"结论稳健域"明确陈述
  - Notes: A1 架构审计 a2_consequence 承诺项；其余敏感性档位只登记不扫（Q1）

- [x] 12. T-412 整体物理 Gate（专用 Plant 测试台）
  - ID: T-012
  - Requirement(s): FR-425, FR-426
  - Roadmap: T-412
  - Files/areas: 新建 `scripts/40_run_modelE_gate.m` + `scripts/+gate_e/check_*.m`
    （G1~G6 六类判据，design §5.2 测试矩阵 E01~E10——**确定性 Plant 测试台直接注入
    指令，不经 EMS 控制器**；§5.3 阈值 + gate_class 字段）；输出
    `results/gate_result.json`；负向验证（构造缺陷副本确认 FAIL）
  - Validation: 全矩阵 overall=true；负向用例正确 FAIL+非零退出；
    law=0/law=5 兼容 smoke 无错误（E10，不作物理判据）
  - Notes: **Gate FAIL 禁止提交**（NFR-408）；PASS 后不自动冻结基线——
    交 T-013 用户评审（design §5.1）

- [x] 13. 归档、README 登记与用户评审门
  - ID: T-013
  - Requirement(s): FR-427, AC-408~AC-415
  - Files/areas: `docs/phase4/a2_model_e/modelE_parameter_evidence.md`（参数证据链：
    值/provenance/文献编号 [1]~[17]/推导公式；含效率链审计与螺旋桨重标定推导）；
    §20 README 登记（model/、Model E 工作区、docs/phase4/ 及受影响目录）；
    绝对路径扫描；开发日志收尾；Model D 零改动终验（历史资产未被修改）
  - Validation: AC-408~AC-414 全过；用户书面批准 T-412 结果与证据链（AC-415）；
    批准后冻结 `config/gate_baseline_modele.json` v1.0
  - Notes: 只有本任务批准后 A2 才收官；B 包方可立项

## 任务依赖

```text
T-001 ─► T-002 ─► T-003 ─► T-004 ─► T-005 ─┐
              ├─► T-006 ─► T-007 ──────────┤
              └─► T-008 ─► T-009 ──────────┴─► T-010 ─► T-011 ─► T-012 ─► T-013
```

T-003→T-004→T-005 严格先后（H/T 单一事实源：T-004 产出 HT_ratio，T-005 消费）；
T-006→T-007、T-008→T-009 严格先后；T-010 起严格按序。
{T-003 链} 与 {T-006 链} 与 {T-008 链} 三者相互独立可并行。

## Verification Checklist

- [x] Requirements IDs are referenced by tasks（FR-410~427、NFR-405~411、AC-408~415 全覆盖）
- [x] Design testing strategy is represented by tasks（单元=T-004/005/006/007/008/009；
  集成=T-010；Gate=T-012；负向=T-012；幂等=T-010；敏感性=T-011；用户评审=T-013）
- [x] Each task has a clear validation command or manual check
- [x] Rollback/migration tasks are included when applicable（design §8：回退=删除
  Model E 工作区 + docs/phase4/a2_model_e/ + 废弃分支；Model D 从未触碰）

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run implementation-readiness validation before coding.
- When coding starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering（三条独立链内部按序、链间可并行）。
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.
- **执行 session 注意**：本 spec 设计为自包含（用户以不同模型执行）；执行分工
  （主 Agent/subagent）由执行 session 按 AGENTS.md §3 决定；subagent 默认不得
  git add/commit/push（AGENTS.md §8）；Gate FAIL 禁止改参数凑 PASS（NFR-408）；
  参数独立成套纪律（`_legacy_reference/` 不参与运行）；K_R=890、BSFC 节点、
  电池 LUT、重标定 K 值等数值以本 spec + design 为唯一事实源——若 Gate 显示
  其不合理，正确行为是 Gate=FAIL 并回到 brainstorm/design 修订，**不是**
  执行中调参让 Gate PASS。

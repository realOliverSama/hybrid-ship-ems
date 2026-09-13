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
| `scripts/10_build_modelE.m` / `10b_smoke_modelE.m` | builder 入口 / smoke（10/10b 的 ensure_canonical 调用已移除——A2 D-13，B 包 T-001；**10b 已接入 T-005 CCS 稳态 eligibility mask**（DGN-14，阈值不变，暂态豁免留证 + fail-closed） |
| `config/dataset_v3_signal_registry.json` | ★ **D 包机器事实源**（T-001，FR-465）：Dataset v3 signal/role registry（91 条信号 × 八 storage role × aggregation 枚举 + self_hash；H1/H2 集合与 FR-461 逐名锚定） |
| `config/dataset_v3_producer_schema.json` | ★ **D 包机器事实源**（T-003，FR-495）：C-producer interface schema（十一项能力族 + forecast provenance 六字段 + formal 航次合格判据槽位，activation 值不预填） |
| `scripts/+gate_e/ccs_eligibility_mask.m` | ★ T-005 CCS 稳态 eligibility mask（预声明规则纯函数；单测 `tests/test_ccs_eligibility_mask.m`） |
| `scripts/+gate_e/reset_bench_nominal.m` | ★ T-006 case 隔离不变量（bench 注入通道显式恢复 nominal；10b/40/42/43/44 全 case 接线；⚠ 禁止在 base 重跑 config 脚本——循环变量覆写事故见文件头） |
| `scripts/+dataset_v3/export_pilot.m` | ★ T-007 pilot 导出主体（registry 驱动 stream / K-F causal fixtures / PILOT_ONLY；70 号薄入口调用） |
| `scripts/70_export_dataset_v3_pilot.m` | ★ **T-007 pilot producer 薄入口**（导出 `datasets_v3/pilot/*.mat` + manifest；FAILED 航次即 GATE FAIL） |
| `scripts/71_run_dataset_v3_pilot_gate.m` | ★ **T-015 pilot Gate 薄入口**（调用 Python `run_pipeline`；`results/dataset_v3/pilot/pilot_gate_result.json` overall=true 方 PASS，标 PILOT） |
| `scripts/72_run_cd_interface_gate.m` | ★ **T-016 C/D Interface Gate 薄入口**（⑩ 基线完整性 hash 快照 + Python 十项独立键检查；env 参数化 `EMS_CD_VOYAGE_DIR` / `EMS_CD_FORMALITY` / `EMS_CD_RESULT`——D-15，缺省 = pilot NON-FORMAL 骨架自检，`EMS_CD_FORMALITY=FORMAL` 即 EXT-CD-GATE） |
| `scripts/73_generate_fleet_v3.m` | ★ **T-018 Phase 4-D Fleet v3 生成薄入口**（`FLEET_MODE='stage_a'\|'smoke'`；消费 T-432 activation 机器副本 docs/phase4/d_dataset_v3/t432_activation_record.json——拒绝率门/retry/1200-60-0 判据/OOD 轴带/guard band/proposal 采样器全部从 record 消费；Stage B 双模式 `stage_b_iid|stage_b_ood` 已启用（T-019；FR-497；amendment 01 预注册生效——5×40 OOD/576 IID）；薄入口惯例 `cd <MODELE_ROOT>; matlab -batch "eval(fileread('scripts/73_generate_fleet_v3.m'))"`；产物 `datasets_v3/qualification*/` + manifest `results/dataset_v3/qualification*/`） |
| `scripts/rerun_stage_a_amended.sh` | ★ **AMENDMENT_02 Stage-A 重资格化驱动**（Final Ruling §4：92 条两跑 + payload-hash 确定性比对 + STAGE_A_RERUN_DONE/FAILED 标记；pass 级重试 ≤3/断点续跑/挂起看门狗/attempt 级日志；`FLEET_WORKERS` 可调，串行 face `FLEET_PARALLEL=off`） |
| `config/mission_v3/prequal_plan.json` | ★ **T-019 预资格计划预注册**（FROZEN_PRE_REGISTRATION：9 格×12 accepted=108、3 base draws×2×2 family、seed/draw 段位、retry 政策、拒绝率阈值 0.50 生成前冻结、eligibility 语义精化裁决入册；self_hash） |
| `config/mission_v3/` | ★ **C 包生成侧注册表九件**（Phase 4-C T-001/T-002，候选态 CANDIDATE；模板三 archetype / conditional-DAG 十四节点 / 环境 regime 参数 / ID 支撑界 / 预报 quality profiles / seed 树策略 / **scenario strata 九格 / heldout token C1~C3 / identifiability contrast 六必需+二诊断**；各含 self_hash；详见该目录 README.md；**预测模型：无**——纯配置注册表） |
| `scripts/+mission_v3/producer_prequal.m` | ★ **T-020 预资格主循环**（ctx/run/member/compare 四模式：hotel 逐航次守卫/traffic OFF 剥离/DEGRADED arm/白名单注入/1 Hz 网格化/payload hash 记账/NORMAL 复用机器验证/PREQUAL-AMEND-01 引用/T429 写前自检） |
| `scripts/+mission_v3/` | ★ **C 包 Mission v3 生成器 MATLAB 包**（T-001 起建：`validate_registries.m` 注册表 validator——canonical JSON + SHA256 跨语言复算；**`seed_tree.m` T-008 科学 seed 树——SHA256 branch-path 血统/uint32 引擎映射/碰撞 fail-closed/干预不变量**；**`dag_sample.m`+`env_realize.m` T-009 环境真值生成——L0~L3+BASE DRAW 条件抽取（八分支血统子流）/河流坐标 OU（prehistory −1800 s 起）/R1 往返共享/reject-resample+六类 disposition 台账**；**`nav_constraints.m`+`plan_compose.m` T-010 工况语义计划层——BRIDGE_OPERATION 五子阶段/UKC 限速/traffic min 合成/泊离非镜像/rush 平台=min(档位, 前沿钳位) fail-closed**；**`stack_identity.m` T-016 十项组件复合 hash（generator 域=顶层包函数，tests/+audits/ 排除——FD2 裁决 manifest 字段登记；PENDING 通道容忍 T-012 在途）**；**`forecast_issue.m` T-012 因果预报器——GOOD_V1 OU 条件均值解析式/DEGRADED_V1 滞后+航次偏置+AR(1)、t≤t₀ 截断接口 fail-closed（FUTURE_INPUT/NO_PREHISTORY）、observability 白名单、provenance 六字段+self_hash**；**`scenario_compile.m` T-011 场景编译器——28 字段 SCN 对齐既有集（时间模速度表/带符号流场 R1/hotel 与水深恒等式/VOYAGE_* 注入白名单 fail-closed/random_seed=current_truth 引擎种子）**；**`kf_snapshots.m` T-013 K/F issue-major 快照——s_plan 计划前向积分（bin 右端点，同 export_pilot 布局）/lead 严格 10:10:300/缺快照 FAILED_CONTRACT/因果 API 边界（签名无 realization 形参）**；**`family_expand.m` T-014 干预层与 grouping——2×2 factorial family 展开PHYSICAL/INFORMATION 分型/truth hash/未声明复用三重 fail-closed/并查集 split_group 闭包**；**`eligibility.m`+`producer_export.m` T-015 合格判据与 producer 导出——窗算术 N(D)=⌊(D−600)/10⌋+1（D 契约推导）/三条件 AND/六类 disposition+retry 记录/{stream,kf,meta}.mat -v7 对齐 export_pilot/FR-500 阶段隔离**；**`+audits/feasibility_sweep.m` T-005 联合可行性扫描 + `+audits/segment_duration_census.m` T-004 段时长普查**；后续 T-011~T-015 生成链函数按 DGN-01 布局增补） |
| `scripts/+mission_v3/tests/` | C 包单测/负向 harness（`test_validate_registries.m` 九件 validator 正/负向 9/9、`test_seed_tree.m` seed 树 8/8、`test_env_realize.m` 环境真值 8/8、`test_plan_compose.m` 计划层 9/9、`test_stack_identity.m` stack identity 7/7、`test_forecast_issue.m` 预报器 8/8（解析式 vs 数值积分/因果门负向/衰减形状/AR(1) 时间相关/白名单/provenance/确定性/wl 通道）、`test_scenario_compile.m` 编译器 8/8（字段完备/hotel+水深恒等式/R1 符号映射/白名单/确定性）、`test_kf_snapshots.m` 快照 7/7（布局/R7 因果不变量+BAD_ARG/积分/缺快照/zones/网格门/确定性）、`test_family_expand.m` 干预层 8/8、`test_producer_export.m` 导出判据 8/8、`test_negative.m` **AC-436 十项注入集成负向套件 10/10**（future-truth/realized future/realized 约束/网格与 vintage/禁填补/latent 源/未声明复用/truth drift/PILOT_ONLY/越权源——全链真实组装+阳性对照+检出报告 JSON）、**`producer_prequal` 主循环、`run_all.m` 十套件集成入口（run/tworun：全链四级确定性 hash 两跑+stack_identity Strict 零改动快照入回归记录 regression_report.json）**；注入参数/tempdir 运行不触真文件） |
| `scripts/+fleet_v3/` | ★ **T-018 Phase 4-D D 侧 Fleet 生成包**（`plan_load.m` activation 机器副本解析——带区间字符串机械解码/proposal 采样器参数/override 键路径/seed-draw 段/provenance 三 hash（activation 文件 SHA + OOD 配置序列化 SHA + registry self_hash）；`predicate_eval.m` canonical token 谓词求值（heldout_token_registry 三字段白名单 fail-closed，D4）；`propulsion_feasibility_check.m` 700 kW 推进容量保守解析 guard（11 kn 恒速逆水 + K_R/浅水链/sea margin/η 全 Model E 冻结系数，D5 段④）；`ood_judge.m` OOD 验收判定——域 guard(\|U\|≤2.8, H/T≥1.3)/guard band 拒(FR-519)/REQUIRE 谓词/数值带/token 优先于数值轴；`ctx_build.m`+`member_exec.m` 与 C 链同构的 plan 驱动成员执行（compile→hotel guard→注入→仿真→1 Hz→forecast→K/F→eligibility 1200/60/0 注入→producer_export）；`export_ood.m` FORMAL 过桥 + load-append OOD meta 重写（producer_export 面零改动，D5 OOD provenance）；`gen_iid.m` IID 段生成（env_realize IID 判定面 + 拒绝率门）；`gen_ood.m` D5 六段定向管线（reject-resample + STRESS_SET 剔除台账 + UNREACHABLE fail-closed）；`manifest_stage_a.m` Stage A 汇总 manifest（per-corpus 计数/OOD 命中统计/C3 FR-545 证据/provenance hashes/吞吐 + T429 黑名单自检）；`h2_surface_assert.m` fail-closed H2/Dataset 导出面不变量（AMENDMENT_02：registry INPUT_H2 ⊆ schema required ⊆ stream_map 且导出面精确一致，Final Ruling §3；ctx_build 接线）；`tests/` 七套件 run_all（含 test_ood_plan_face/test_h2_surface）） |
| `scripts/75_run_mission_v3_feasibility_sweep.m` | ★ **T-005 ID 联合可行性扫描薄入口**（SOG×沿迹流×H/T 网格只读 Model E 扫描；600 s 腿全格 + 3000 s 稳态精化） |
| `scripts/76_run_mission_v3_segment_census.m` | ★ **T-004 段时长普查薄入口**（注册表算术普查：usable duration 分布 + 切片几何硬约束判定 + 冻结提案） |
| `scripts/81_run_mission_v3_t429.m` | ★ **T-422 T-429 Gate 薄入口**（T429_MODE=evaluate|verify；freeze 机器校验→切片→SBD/probe→六必需对比→fail-closed 判定） |
| `scripts/80_run_mission_v3_prequal.m` | ★ **T-020 预资格执行薄入口**（T020_MODE=pilot|run|tworun；108 航次全链生成+两跑确定性+拒绝率冻结门） |
| `results/mission_v3/` | C 包精选证据（T-001 validator 记录、T-005 可行性扫描三件、T-004 普查、T-017 `negative_detection_report.json`、T-018 `regression_report.json`、T-019 `qualification_stack_identity.json`、T-020 prequal 四件（strata_audit/manifest/reconciliation/amendment_01）、T-021 `t429_freeze.json`（盲冻结 f8152ffe…）、T-022 `identifiability_gate_result.json` + `identifiability_report.md`（**Gate FAIL_INSUFFICIENT_EVIDENCE 已落盘，处置待裁决**）） |
| `dataset_v3/` | ★ **D 包 Python 构建与审计包**（T-008~T-016：聚合/窗口/target/split/L1-L3 审计/OOD/manifest/E2E 编排/契约校验 + pytest；详见该目录 README.md） |
| `results/dataset_v3/` | D 包精选证据（pilot_export_manifest / pilot_gate_result / cd_interface_gate_result / **cd_interface_formal_prequal / cd_interface_formal_t429conf_v2**（EXT-CD-GATE formal 语料结果，D-15）/ split_assignment 等 JSON；大数据 `datasets_v3/` 不入库）。T-432 证据（2026-09-10/12 补录）：`qualification/` Stage-A 双 Gate 日志（gate_iid/ood_am03、pass1/2_stage_a_a1、STAGE_A_RERUN_DONE）+ `_amendment03_prerun_quarantine/`（AM03 前隔离账本）+ `_old_logs/`；`formal/` 失败尝试日志（pass1_stage_b_iid_a1 / pass1_stage_b_ood_{mac,pc}——09-10 宿主 crash 诊断证据，正式四段日志/ledger 另见 formal/ 入库件） |
| `model/HybridShip_ModelE.slx` | builder 生成（脚本驱动，禁 GUI 修改；B 包顶层 16 块含 07_PMS_v2/16_Safety_Interlock） |
| `results/` | `gate_result.json`（T-412 against candidate overall=true）、`gate_control_result.json`（43+44：overall + C1~C7 + 三臂指标 + negative_control）、`config_manifest.json`（4 config sha256+有效值快照） |
| `docs/` | 证据文档指向仓库级 `docs/phase4/b_pms_safety/`（field-level diff / 三臂对比 / 裁决链） |
| `reports/parameter_audit.csv` | builder 生成的参数审计（ParamMeta，40 项） |

## Legacy / Superseded（FR-492/493：不删不搬不改行为，仅 README 标记）

| 条目 | 标记 | 说明 |
|---|---|---|
| `scripts/03_run_smoke_tests.m` | **LEGACY / SUPERSEDED / NOT A MODELE VALIDATION ENTRYPOINT** | Model D 时代 smoke（`model.ensure_canonical` 依赖）；Model E 验证入口 = 10b/40/43；Dataset v3 新链禁止调用，其结果不作 Model E/Dataset v3 科学证据 |
| `scripts/05_export_dataset.m` | **SUPERSEDED FOR MODELE DATASET V3 / REPRODUCTION ONLY** | Model D 时代导出脚本；Dataset v3 导出 = `70/71` 号 + `dataset_v3/`（两段式），schema/特征/目标假设不继承 |
| `scripts/30_export_load_profiles.m` | **SUPERSEDED FOR MODELE DATASET V3 / REPRODUCTION ONLY** | Model D 时代负荷剖面导出 |
| `scripts/60_generate_voyages_v2.m` `scripts/61_generate_fleet_profiles_v2.m` `scripts/63_integrate_gate_v2.m` `scripts/64_run_matrix_v2.m` `scripts/66_export_fw_reference.m` | **SUPERSEDED FOR MODELE DATASET V3 / REPRODUCTION ONLY** | Model D Phase 2/2.5 fleet/固件参考链；formal fleet 生成 = T-432 `73_generate_fleet_v3.m`（BLOCKED，activation 后施工） |
| `python/` | **SUPERSEDED FOR MODELE DATASET V3 / REPRODUCTION ONLY** | Phase 2.5 训练/数据集链（build_dataset_v2.py 等）；v2 纪律 reuse-only、schema 不继承；Dataset v3 构建在 `dataset_v3/` |
| `datasets/` | **SUPERSEDED FOR MODELE DATASET V3（历史产物只读）** | Phase 2/2.5 窗口化 npz/norm JSON；不作为 Model E 研究数据 |
| `legacy_reference/` | **LEGACY（历史模型只读副本）** | 分叉继承件，仅证据对照 |

**注意**：`scripts/01~07/10~66` 等编号脚本为 Model D 流水线继承件——Model E 只保证
**02 build / 40 gate / 43 控制门**链路；旧数据集链（05/30/60/66）已标 SUPERSEDED，
Dataset v3 新链 = `70/71/72` + `dataset_v3/`。禁止在 Model E
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
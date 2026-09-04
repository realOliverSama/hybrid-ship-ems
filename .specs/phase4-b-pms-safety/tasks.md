# Phase 4-B — 真实控制逻辑 / PMS v2 / Safety-Interlock（T-413~T-418）Tasks

Spec: `phase4-b-pms-safety`  
Status: tasks-approved（2026-09-04 用户批准：APPROVED WITH MINOR AMENDMENTS——
① T-012 bench 等值比较限定为 A2 历史物理观测量/指标，B 包新增控制/logging 信号不参与；
② T-004 只测 Safety 函数本体，UnitDelay 一拍拍序断言移至集成层 T-007；
③ 依赖图澄清 T-006 为单写者资源串行而非设计依赖；用户指示：施工移交新 session，
handoff 见 `./HANDOFF.md`）  
Created: 2026-09-04  
Brainstorm: `./brainstorm.md`（D-B1~D-B9 + 终审 5 项修正）  
Requirements: `./requirements.md`（requirements-approved + design 联动修订 FR-445 S2/S5）  
Design: `./design.md`（design-approved，DD-1~DD-9 + 偏差登记 D-B10~D-B15）

> 任务编号 T-001~T-014 为本 spec 局部编号；每项标注路线文档
> `docs/phase4_research_roadmap.md` 的 T-4xx 映射与 FR 追溯。
> **执行 session 开工前必须研读**：brainstorm.md、requirements.md、design.md、
> A2 spec 四件（接口契约 FR-421/424/426 + design D-4/D-13）、
> Model E 工作区 README_Model.md、`项目开发进展.md` 2026-09-04 两条目
> （参考文献链 [1]~[19] + R2026a 工程坑清单）。
> **全部参数值/公式以 requirements.md + design.md 为唯一事实源，禁止凭记忆施工。**

## 执行约束（全任务通用）

- 分支 `feat/phase4-b-pms-safety`（stacked on A2 approved head）；逐文件 add；
  禁 merge 至 Phase4（待用户 PR 审批）；Git 红线全适用；
- **Model E Plant 物理零改动**（03/04/06/08/09/10/11/13/14/15 与其参数）；
  `ModelE_Physics_Parameters.m` 零改动（DD-9 Step B 机器检查项）；
- **Model D 目录零改动**（sha256 持续证明）；
- `build_model_e.m` **主 Agent 独占编辑**（A2 D-10 先例）；并行施工时 delegate 只交付
  独立文件（块源码字符串/单测/独立脚本），交接面 = 函数签名 + 端口清单；
- R2026a 工程坑清单适用（design §10）；禁绝对路径；禁 `model.ensure_canonical`；
- 每个任务完成后立即 `spec_task_check`；Gate FAIL 时 `error('GATE:*')` 停下升级用户，
  **禁改参数凑 PASS**。

## Implementation Plan

- [x] 1. 控制参数事实源 + 全入口加载接线
  - ID: T-001
  - Requirement(s): FR-451, FR-451A, NFR-416, NFR-418
  - Roadmap: T-415 参数基础
  - Files/areas: 新建 `config/ModelE_Control_Parameters.m`（design §8 参数表全量 +
    provenance 逐参数标记 + 敏感性注册表注释）；builder/40/41/42/run_scenario_sim/
    43（后续任务创建）各入口在物理参数之后加载；干净 workspace 验证
  - Validation: 参数文件单独 run 即定义全部 PMS/Manual/Selector 参数；provenance
    无空缺；物理参数文件 diff 为空
  - Notes: canonical 域换算常数直接引用 `ASSUMED_eta_rect`（物理事实源），
    不在控制参数文件复制数值

- [x] 2. Baseline_Demand_Policy 块源码 + 单测（并行链 A，delegate）
  - ID: T-002
  - Requirement(s): FR-432, FR-433, FR-434, FR-435
  - Roadmap: T-415（Demand 层）
  - Files/areas: 新建 `scripts/+model/code_pms_demand_e.m`（块源码字符串，design §5.3
    伪代码逐行对应）+ `scripts/tests/test_pms_demand_e.m`（LPF τ 阶跃响应数值校验、
    滞回进出含等号边界 `<=0.55`/`>=0.60`、初值 false、削减公式 max(0,min(100,
    500·N_dispatch − P̄_load)) 单/双机两档、N_dispatch 反馈延迟一拍语义）
  - Validation: `matlab -batch "run('scripts/tests/test_pms_demand_e.m')"` 全过
  - Notes: 交付独立文件两枚，不碰 build_model_e.m；发现 spec 语义矛盾立即停止升级

- [x] 3. DG_Dispatch_Allocator 块源码 + 单测（并行链 B，delegate）
  - ID: T-003
  - Requirement(s): FR-441, FR-442, FR-443
  - Roadmap: T-416（Dispatch 层）
  - Files/areas: 新建 `scripts/+model/code_dispatch_e.m` + `scripts/tests/test_dispatch_e.m`
    （均分、clamp 125/500、**is_dispatching 单一谓词**——必含
    `STARTING+healthy+available ⇒ P_ref==0` case、N=0 防御、诊断三件套公式数值校验）
  - Validation: 单测全过
  - Notes: 同上独立交付；诊断量命名与 design §5.5 逐字一致

- [x] 4. 16_Safety_Interlock 块源码 + 单测（并行链 C，delegate）
  - ID: T-004
  - Requirement(s): FR-444, FR-445, FR-446, FR-447
  - Roadmap: T-417
  - Files/areas: 新建 `scripts/+model/code_safety_interlock_e.m` +
    `scripts/tests/test_safety_interlock_e.m`（S1a 非预期丢失/S1b 40 s 启动失败窗、
    S2 realized 口径 `max(0,P_load−ΣP_DG_actual)>SOP_dis` 持续 2 s、S3 含单健康机
    flag-only 分支、S4 firm headroom 公式、S5 isfinite+EMS_AGGREGATE 门控、
    监视 flags、fault_latch 锁存、全部触发/不误触发对）
  - Validation: 单测全过
  - Notes: S2/S4 公式以 design §5.6（评审修订版）为准，禁止引用 brainstorm 旧口径；
    **本任务只测 Safety 函数本体**；Safety→Commitment 的 UnitDelay 一拍拍序
    （override(t) ⇒ t+0.01 s 恰好一拍到达、非两拍、无代数环）是集成层断言，
    移交 T-007

- [x] 5. DG_Commitment_Supervisor Stateflow chart + harness 单测
  - ID: T-005
  - Requirement(s): FR-436, FR-437, FR-438, FR-439, FR-440
  - Roadmap: T-415/T-416（Commitment 层）
  - Files/areas: 新建 `scripts/+model/build_commitment_chart_e.m`（Stateflow API
    脚本化建 chart：离散 ChartUpdate=DISCRETE Ts=0.01、整数 tick 计时器、
    状态 ONE_DG_NORMAL/DG2_START_PENDING/TWO_DG_NORMAL + 迁移优先级
    veto>safety>normal、审计九元组输出、reason_code 枚举 1~7）+
    `scripts/tests/build_commitment_harness.m` + `scripts/tests/test_commitment_chart_e.m`
    （阈值/30 s start hold/120 s stop hold/滞回驻留/并行计时最短 ON=恰 300 s/
    min_down=0 即时重启/仲裁顺序/审计完整性/force 通道 FORCED reason）
  - Validation: harness 单测全过；chart 可被 builder 重复生成逐位一致
  - Notes: 不用 `after()` 时序算子；chart 内部结构以 design §5.4 为准

- [x] 6. 01/02 改组 + MANUAL 工况定义
  - ID: T-006
  - Requirement(s): FR-428, FR-429, FR-430, FR-431
  - Roadmap: T-413/T-414
  - Files/areas: builder 内 `01_Operator_Interface`/`02_Propulsion_Controller` 构建段
    重构（AUTO PI 零改动、MANUAL 线性映射+20 %/s rate limiter、Switch 编译期选路）；
    `config/ModelE_TestScenarios.m` 新增 VOYAGE_CTRL_MODE/VOYAGE_MANUAL_CMD_TS 注入
    通道与 M1/M2/M3 定义（design §6.5 电平/段长）；design §4 决策表已落档（本任务
    仅代码侧）
  - Validation: 双模式构建成功；AUTO 既有 smoke 行为不变；M1/M2 短程运行无错误
  - Notes: 块改名导致结构指纹变化为预期（DD-9 流程处理）

- [x] 7. builder 顶层集成（07_PMS_v2 + 16_Safety_Interlock + 接线替换）
  - ID: T-007
  - Requirement(s): FR-449, FR-450, FR-451A, NFR-414, NFR-417
  - Roadmap: T-415/T-416/T-417 集成面
  - Files/areas: `build_model_e.m`（**主 Agent 独占**）：07_PMS_v2 构建函数
    （四子块 + Source_Selector + X_b2g_* 换算环节）、16_Safety_Interlock 块、
    adapter 删除与接线替换（05→07→06，端口契约 design §5.1）、UnitDelay 防环、
    Unit Delay 输入反馈、logging 扩展（design §5.8 清单）、force 通道保名（DD-8）、
    EMS_HEALTH_TS From Workspace 通道
  - Validation: 构建无错误；结构指纹幂等（两建逐位一致）；无代数环（编译诊断）；
    **集成拍序断言：Safety override(t) ⇒ Commitment 于 t+0.01 s 恰好一拍收到
    （非零拍/非两拍）**
  - Notes: 集成前 T-002~T-006 必须全部完成；X_b2g_* 块名清单入 43 结构断言

- [x] 8. 七变体兼容 + law=0/5 smoke
  - ID: T-008
  - Requirement(s): FR-450, FR-426(A2 契约延续)
  - Roadmap: T-418 前置
  - Files/areas: 变体编译矩阵脚本（tests/t008_variant_compile_check.m）；law=0/5 经
    EMS_AGGREGATE 通道短程运行
  - Validation: 七变体编译/构建成功；law=0/5 短程无错误
  - Notes: 不作物理判据（A2 既定口径）

- [ ] 9. 43 号控制 Gate 脚本 + 检查函数包 + 指标后处理
  - ID: T-009
  - Requirement(s): FR-452, FR-454, FR-455（判据 1~7）, FR-456
  - Roadmap: T-418
  - Files/areas: 新建 `scripts/43_run_modelE_control_gate.m` +
    `scripts/+gate_ctrl/`（check_c1_determinism/check_c2_transition_legality/
    check_c3_soc_band/check_c4_vdc/check_c5_energy_closure/check_c6_safety_assertions/
    check_c7_audit_completeness + metrics 后处理 + episode 算法 design §6.3）+
    `results/gate_control_result.json` 输出
  - Validation: 脚本骨架在已知好工况上自洽运行（判据函数单测级验证）
  - Notes: T-412 重跑以 Phase 0 显式 baseline_path 机制接入（T-012 汇合）

- [ ] 10. Safety 注入场景 S1~S5 + bench 扩展
  - ID: T-010
  - Requirement(s): FR-448, FR-453
  - Roadmap: T-417 验证面
  - Files/areas: `gate_assign_bench_inputs` 机制扩展（或 +gate_ctrl 等价物）：
    S1（GEN_CMD_STOP_TS 非指令停机）、S2（确定性母线缺口）、S3（盈余注入）、
    S4（高 demand 低 firm 预见缺口）、S5（EMS_HEALTH_TS 拉低）；每场景预期断言表
  - Validation: 五场景在 43 Phase 4 中按断言通过
  - Notes: S2/S3/S4 注入必须确定性（assignin 通道，禁 setVariable）

- [ ] 11. 44 号负向验证
  - ID: T-011
  - Requirement(s): FR-455（判据 6 负向 + FORCED 负向）
  - Roadmap: T-418（负向验证，42 号模式延续）
  - Files/areas: 新建 `scripts/44_run_control_gate_negative.m`：构造非法 transition
    （force 通道绕过阈值 → reason=FORCED）、应触发未触发 override（屏蔽 S4 输出）等
    缺陷注入，确认 43 判据正确 FAIL 且非零退出
  - Validation: 全部构造缺陷被正确检出
  - Notes: 与 42 号物理负向脚本并存，不改 42

- [ ] 12. candidate v1.1 + T-412 against candidate + 43 全矩阵
  - ID: T-012
  - Requirement(s): FR-455（判据 8）, FR-456, NFR-412, NFR-413
  - Roadmap: T-412 重跑 + T-418 主验证
  - Files/areas: 40 号 Gate 增显式 `baseline_path` 参数（默认 v1.0 行为不变）；
    生成 `config/gate_baseline_modele_candidate_v1_1.json`（机器检查：物理参数 sha
    一致/G2~G6 阈值一致/Model D hash 一致）；43 全矩阵（AUTO×3 臂 + MANUAL×P2 +
    Safety×5 + 确定性两跑）至 overall=true；v1.0→v1.1 field-level diff 报告
  - Validation: **43 overall=true ∧ T-412(candidate) overall=true**；**A2 历史物理
    bench 观测量/指标（原 Gate 指定的 physical observables/metrics）在 DD-8 force
    兼容适用范围内逐位一致；B 包新增控制/logging 信号不参与该等值比较**；
    Model D sha256 零改动
  - Notes: 任一 FAIL → error('GATE:*') 停下升级用户；禁改参数凑 PASS

- [ ] 13. 归档（README/文档/日志/D-14 补记）
  - ID: T-013
  - Requirement(s): FR-457, NFR-416, NFR-418
  - Roadmap: T-418 收尾
  - Files/areas: Model E 工作区 README.md + README_Model.md（§2 顶层结构/§3 控制逻辑
    改写为 PMS v2 + Safety 描述、§5 验证状态更新）；`docs/phase4/b_pms_safety/`
    证据文档（裁决链 + 三臂对比报告 + candidate diff）；`项目开发进展.md` 当日条目
    （含延伸问答记录）；**A2 spec design.md 补记 D-14**（R_0 充放电共用/SOC 降额族
    来源升级/k_form 降级/t_starting 标签——T-013 遗留）；D-B10~D-B15 偏差核对；
    绝对路径扫描零命中
  - Validation: §20 检查（新增文件全部登记）；扫描零命中
  - Notes: A2 spec 文件修改仅限追加 D-14 条目（已批准事项的补登记）

- [ ] 14. 用户评审门 + 基线 promotion + 收官
  - ID: T-014
  - Requirement(s): AC-422, AC-423, AC-424, FR-456
  - Roadmap: T-418 收官
  - Files/areas: 向用户提交：43 + T-412(candidate) 双 overall=true 证据、
    v1.0→v1.1 field-level diff、三臂对比报告、D-B 偏差全录；**用户书面批准后**执行
    promotion（candidate → `gate_baseline_modele.json` v1.1）+ 冻结
    `gate_baseline_modele_control.json` v1.0 + spec 升 implementation-complete
  - Validation: 用户书面批准；promotion 后 40（默认 v1.1）+ 43 双双重跑确认
  - Notes: 未批准前禁止 promotion；PR（feat/phase4-b-pms-safety → Phase4）在收官后
    创建交用户审批

## 依赖与并行策略

```text
T-001 ──┬─ T-002（链 A delegate） ┐
        ├─ T-003（链 B delegate） ├─ 逻辑并行四链
        ├─ T-004（链 C delegate） ┤
        ├─ T-005（chart harness） ┘
        └─ T-006（01/02 改组——**逻辑上同样并行**；仅因 build_model_e.m 主 Agent
                  单写者纪律而与 T-007 串行于主树，属 resource/write serialization，
                  不是设计依赖）
T-002~T-006 ──► T-007（主 Agent 独占集成）──► T-008 ──► T-009 ──► T-010 ──► T-011
                                                        └──────► T-012 ──► T-013 ──► T-014（用户门）
```

- T-002~T-005 为纯文件交付（块源码 + 单测 + chart 构建脚本），交接面 = design §5
  的函数签名/端口清单/伪代码；
- T-007 起串行（单写者）；T-012 为 Gate 总汇合；
- delegate 越界/歧义立即停升级主 Agent（AGENTS.md §5 Escalation 格式）。

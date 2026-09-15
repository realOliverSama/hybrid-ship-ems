# Phase 4-F T-445 Trajectory Oracle Gate Tasks

Spec: `phase4-f-trajectory-oracle-gate`  
Status: implementation-in-progress（T-001 起，2026-09-15）  
Created: 2026-09-15  
Approved: 2026-09-15（APPROVED WITH 8 REQUIRED CORRECTIONS——R1~R8 全部按令闭合；Implementation 仍需单独显式授权）  
Brainstorm: `./brainstorm.md`（Q1~Q4 批准）  
Requirements: `./requirements.md`（requirements-approved，R1~R7 闭合）  
Design: `./design.md`（design-approved，DGN-F-01~F-15 + R1~R9 闭合）

> 任务编号 T-001~T-016 为本 spec 局部编号，整体对应路线文档 **T-445**（roadmap §7）。
> **全部契约/数值/规则以 requirements.md + design.md（含 R1~R9）为唯一事实源，禁止凭记忆施工。**
> 本文件不引入任何新的科学或设计语义。

## 执行约束（全任务通用）

- 分支 `feat/phase4-f`；逐文件 add（禁 `-A`/`git add .`）；Git 红线全适用；subagent 纪律按 AGENTS.md §2~§8（委派前用户授权，EMS 路由政策适用）；
- **dataset v3 零字节改动**（NFR-711）：`datasets_v3/`、`results/dataset_v3/` 只读；T-445 派生产物只写 `results/t445_oracle_gate/`（精选入库）与 `artifacts/t445/`（本地不入库）；
- **sealed 纪律**：禁止一切 IID test / OOD 语料访问（本 spec 不需要；test 封印留 T-452）；语料仅用 validation split（FR-760）；
- **预注册纪律（FR-782）**：正式 B1/B2 结果观察前必须完成 B 阶段全部冻结/资格化；结果观察后禁止任何结果驱动的 config/语料/阈值/统计/可行性准则变更；
- **禁绝对路径**（NFR-715）：MATLAB 用 `mfilename('fullpath')` + pwd 兜底；Python 用 `__file__` 推导；
- **零 `.slx` 持久结构修改**（DGN-F-01）；
- 机器可读证据全部 JSON 落盘 + hash；失败/事故如实留账（`incidents.jsonl`）禁删（FR-777）；
- 每任务完成立即 `spec_task_check`；关键结果主 Agent 独立复核（AGENTS.md §14）；
- **STOP 行为**：需改冻结设计语义/资格化失败/sealed 污染/SimState 不等价/持续不可行 → 停下升级用户裁决，不得自行解决；
- **运行时压力不得削弱求解器/语料/统计语义**（R6/NFR-714）。

## 阶段总览（证据流 DAG，对应 Tasks Drafting Gate 裁决 A~F；含 Tasks Gate R1/R3 排序修正）

```text
A 基础:  T-001(冻结config[含 qualification_fixture_manifest]+基线完整+env/E-artifact 守卫)
         → T-002(窗装配+ZOH) → T-003(B0/B1 harness+回归+R4 λ_eff 真实路径包络验证)
         → T-004(指标+审计机器) → T-005(语料选择+manifest) → T-006(统计+Gate裁决代码)
         → T-008(B2优化器实现，depends T-007 PASS)
         → T-009(B2分段runner实现)
B 资格化（强制）: T-007(k_eq回补fixture→CANONICAL) → T-010(SimState等价Gate + B2_OPTIMIZER_QUALIFICATION Q1~Q6)
B 资格化（上下文）: T-011(B3适配+parity资格化)——必须完成，QUALIFIED_PASS / UNQUALIFIED_CONTEXT 均合法，不阻断主链
B 入场券: T-012(正式前冻结总检，含正式访问台账零计数验证)
C 正式:  T-013(初始语料36航次×B0/B1/B2 [+B3若QUALIFIED]；COMPLETE_FORMAL_CORPUS / ABORTED_GATE_VALIDITY_FAIL)
D 裁决:  T-014(初始Gate统计+机器裁决+final_gate_pointer.json；支持 validity-only 路径)
E 扩样:  T-015(仅INCONCLUSIVE解锁：+12航次全量重算；reserve 执行同样 fail-closed)
F 收官:  T-016(最终报告+证据索引；仅消费 final_gate_pointer.json；全终态可机械封印)
```

## A — 基础 / 冻结契约实现（非结果承载）

- [x] 1. 冻结 config 实体化 + 基线完整性 + 目录/README 登记
  - ID: T-001
  - Requirement(s): FR-740~745, FR-780~782；NFR-710/711/712/713/715/716；Design: DGN-F-03/04/05/06/07/11 + design.md §16（组件与目录）；AC-614
  - depends_on: 无（首批）
  - Files/areas: `config/phase4f_t445_config.m`（新建）、`config/phase4f_t445_config.json`（导出）、`results/t445_oracle_gate/`（新建）、`results/t445_oracle_gate/qualification_fixture_manifest.json`（新建，R2）、`results/t445_oracle_gate/env_manifest_t445.json`（新建，R8/NFR-712）、`artifacts/t445/`（本地）、Model E 工作区 README/README_Model.md 与 `.gitignore`（§20 登记）
  - Scope: 将 design 全部 🔒 冻结参数实体化为单一 config 事实源（k_eq^E=0.5296 及其推导字段、E_ref=27.781415/E_scale=13.671476+provenance hash、Δλ∈[−0.10,0.10]×0.005、**λ_eff=max(λ_raw,0.43) 无人为上限**、策略表 λ 原语+派生 `lambda_table=0.43:0.005:15.525` n=3020 及从原语复算 max 的机器断言、`policy_table_oob_query_count` 要求=0、V_term(δ=0.01,w=1.13e5)、|ΔSOC_end|≤0.005、X=2.0/Y=0、种子两枚、ε_num=0.5%、bootstrap B=10000、网格参数、硬可行性判据表；**Amendment a′ 2026-09-15：旧 config_sha256 保留溯源并被新 hash 取代**）——**例外：k_eq^E 在 config 中标记 `PROVISIONAL_PENDING_QUALIFICATION`，直至 T-007 PASS 转 `CANONICAL` 并冻结 config hash（R1）**；导出 JSON + config_sha256；基线完整性：复核 `gate_baseline_modele.json`/`gate_baseline_modele_control.json` hash 与 HybridShip_ModelE.slx sha256 登记（不动模型）；**qualification_fixture_manifest（R2）**：在任何资格化执行前，以机器可读 manifest 冻结全部非正式 fixture 身份——B0 回归航次、SimState 等价参考航次、B2 Q2/Q3/Q4/Q6 fixture 窗、B3 parity fixtures、合成 fixture 配置、Q1 确定性 1000 点样本身份/种子；每 fixture 记录 fixture_id/来源类型/voyage_id/segment_t0（如适用）/工件 SHA256/config 或生成器 hash/确定性种子或 hash 规则/资格化目的/`formal_val_overlap=false`/`iid_test_overlap=false`/`ood_overlap=false`；fixture 不得按资格化结果选择或替换；**环境守卫（R8/NFR-712）**：env_manifest_t445.json 记录 Mac 本机身份（platform/host 类别）、MATLAB R2026a 版本、无 AutoDL/云端 result-bearing 路径的机器断言；**E 段只读守卫（R8/NFR-713）**：E 段工件目录（results/predictor_v3/、candidates/、winner 工件）hash 快照基线 + T-445 harness 代码静态扫描断言零 .keras/winner 模型加载、零 E-additional 训练调用
  - Non-scope: 不写任何 harness/优化器代码；不运行任何航次；不触碰 dataset v3
  - Outputs: config .m + .json + config_sha256 记录
  - Validation: config 单测（全部冻结字段存在且数值=design 值；k_eq^E 状态字段=`PROVISIONAL_PENDING_QUALIFICATION`）；绝对路径扫描零命中；README 登记完整；fixture manifest schema 校验 + 三 overlap 标志全 false 断言；env_manifest 断言（macOS 本机 + MATLAB R2026a + 无云端 result-bearing 路径，NFR-712）；E 段只读守卫基线快照 + 静态扫描零命中（NFR-713）
  - Gate/STOP: 任何冻结值与 design 不一致 → 停，修正 config（design 为事实源）；fixture manifest 缺项/overlap 违例 → 停
  - Completion evidence: config_sha256 落盘 + 单测 PASS + fixture manifest + env manifest + E 守卫基线 + README diff

- [x] 2. oracle 窗装配器 + 真 ZOH 适配器 + 单测
  - ID: T-002
  - Requirement(s): FR-721, FR-725, FR-733；Design: DGN-F-02（含 §2.2 契约/§2.3 ZOH）；AC-610
  - depends_on: T-001
  - Files/areas: `scripts/+t445/assemble_oracle_window.m`、`scripts/+t445/prep_ehat_zoh.m`、`scripts/+t445/make_simin_zoh.m`、`scripts/+t445/tests/`
  - Scope: 窗装配（10s bin 均值/bin 能量/E300_true/shrinking-horizon/partial-bin 时长加权/与 1s 积分等价性断言）；ZOH 数据预制（t0 网格节点序列）；`Simulink.SimulationInput.setBlockParameter` 对 law=6 `oracle_src` 的 Interpolate=off run-local 覆盖（备选 in-memory set_param 不落盘）；**ZOH 机器测试**（major-step/minor-stage/10s 边界前后取值，与 Interpolate=on 对照证明边界差异）；B1/B2 同一适配器
  - Non-scope: 不跑任何正式/非正式航次 sim（ZOH 测试用合成信号 harness，不进 Model E 航程）
  - Outputs: 模块 + 单测证据 JSON
  - Validation: 窗装配单测（含 shrinking/partial-bin/等价性）；ZOH 机器测试全过；AC-610 派生一致性（E300_true 由轨迹派生，逐 t0 断言）
  - Gate/STOP: ZOH 测试失败（minor-stage 插值泄漏）→ 停，升级用户（不得用稠密楼梯伪 ZOH 绕路）
  - Completion evidence: 单测 PASS JSON + ZOH 边界证据

- [x] 3. B0/B1 harness + B0 全回归 + B1 核心夹具回归 + B1 归一化复现
  - ID: T-003
  - Requirement(s): FR-720/721/728；Design: DGN-F-01/02/13；AC-618
  - depends_on: T-001, T-002
  - Files/areas: `scripts/+t445/run_arm_b0b1.m`、`scripts/+t445/verify_lambda_envelope.m`、`python/t445/compute_b1_norm.py`、`scripts/+t445/tests/`
  - Scope: B0/B1 单次全程 runner（`model.run_scenario_sim` 链路 + per-run ECMS_EHat_Data/ECMS_K_S/ZOH 覆盖注入）；B0 在 qualification_fixture_manifest 登记的 3 个固定参考航次对冻结基线全信号逐样本一致（容差 0）；B1 核心回归（`+ecms2/test_ecms_law` 单测 9 逐位 + K_S 通道恒等式 |K_soc·K_S·ê−Δλ|<1e-12）；`compute_b1_norm.py` 从冻结 train 语料复现 E_ref/E_scale=27.781415/13.671476 与 provenance hash b9dcf268…（逐位一致断言）；既有基线 Gate 不受影响确认（10b smoke + 控制 Gate 快链）；**R4 λ_eff 原生包络验证（Amendment a′）**：验证冻结 law=6 原生执行语义而非不存在的 cap——(i) 下限 floor=0.43（K_S≠0）；(ii) 无隐藏上限钳；(iii) fixture `λ_raw>0.80` 经 law=6 处理后仍 >0.80；(iv) B1 与 B2 共用同一 law=6 核心路径；(v) `.slx` checksum 相对 T-001 基线不变。成功证据标记 `LAW6_NATIVE_ENVELOPE_CONFIRMED`。既有 `R4_CAP_NOT_LEGALLY_IMPLEMENTABLE` 工件保留为 incident/修订溯源，**禁删**。诊断报告 `lambda_floor_activation_fraction`、`lambda_eff_min`、`lambda_eff_max`（无 cap 激活比例）。B2 策略表 λ 轴必须覆盖从冻结原语导出的全域；全部回归 run 写入访问台账 purpose=REGRESSION（R5）；fixture 仅限 manifest 登记项（R2）
  - Non-scope: 不跑 val 语料；不跑 B2；不产生任何 headroom 证据
  - Outputs: runner 模块 + 回归证据 JSON + 归一化复现证据 + λ_eff 包络验证 JSON + 台账条目
  - Validation: AC-618 全项；归一化常数逐位复现；基线 Gate 绿；λ_eff 原生包络 JSON 为 `LAW6_NATIVE_ENVELOPE_CONFIRMED`
  - Gate/STOP: B0 回归不一致 → 停，升级用户；归一化复现失败 → 停（实现 bug 或契约错误，禁改冻结值）；隐藏 cap 或 `.slx` checksum 变化 → 停
  - Completion evidence: 回归 PASS JSON + 复现 PASS + 基线 Gate 绿记录 + `LAW6_NATIVE_ENVELOPE_CONFIRMED` JSON（旧 R4 STOP 工件仍在）

- [ ] 4. FEC/审计/可行性指标机器 + 单测
  - ID: T-004
  - Requirement(s): FR-740~748, FR-750；Design: DGN-F-06/12；AC-619
  - depends_on: T-001
  - Files/areas: `scripts/+t445/metrics_fec.m`、`scripts/+t445/audit_matrix.m`、`scripts/+t445/hard_feasibility.m`、`scripts/+t445/tests/`
  - Scope: J_FEC 计算（k_eq^E 路径，raw 六联输出）；audit_capability_matrix 实例化（12 类信号状态登记）；硬可行性检查器（DGN-F-12 §12.2 冻结判据逐项：任务完成 |T_end−T_frozen|≤1s、欠供=0、SOC 硬界 0s、终端 0.005、SOP/V/I 0s、机组限值+C2 引用、能量闭合 C5 复用、Safety 硬事件 0）；干预审计（S1–S5/PMS 计数时长，gate_ctrl 复用）；负担代理（启停/TV_Pgen/ramp 激活/E_bat_throughput/EFC_proxy）；合成 logsout fixture 单测
  - Non-scope: 不跑真实航次；不产出任何正式指标
  - Outputs: 模块 + 单测证据
  - Validation: 合成 fixture 上已知答案比对；矩阵枚举闭合（无未登记信号）
  - Gate/STOP: 某硬可行性信号无法从 stream/logsout 获得（矩阵出现非计划 NOT_INSTRUMENTED）→ 停，升级用户（R5 纪律）
  - Completion evidence: 单测 PASS + audit_capability_matrix 全 AVAILABLE_AND_CHECKED 登记

- [ ] 5. 语料选择 + corpus manifest 生成（arm-independent）
  - ID: T-005
  - Requirement(s): FR-760~764；Design: DGN-F-07；AC-615
  - depends_on: T-001
  - Files/areas: `python/t445/corpus_select.py`、`python/t445/tests/`、`results/t445_oracle_gate/corpus_manifest.json`
  - Scope: 形状波动评分（shape_cv，P_epsilon=1 kW，仅完整 300s 窗）；27 个 val split_group 聚合/三分层；SHA256 排序（种子 phase4f-t445-corpus-v1）；初始 9 组 36 航次 + 预留 3 组 12 航次 manifest 落盘（含评分/排序哈希/种子）；test_sealed 复核断言；单测（合成数据复现排序/分层/族闭包）
  - Non-scope: 不跑任何臂；不读 test/OOD；**manifest 在此生成，正式冻结断言归 T-012**
  - Outputs: corpus_manifest.json + 单测
  - Validation: 同种子复现一致；27 组三分层 9/9/9；初始 9 组+预留 3 组互不相交；test_sealed=true 断言通过
  - Gate/STOP: val 组数 ≠ 27 或航次数 ≠ 108（split 结构变化）→ 停，升级用户
  - Completion evidence: manifest 落盘 + 单测 PASS + test_sealed 复核记录

- [ ] 6. 统计推断 + Gate 裁决代码 + 单测
  - ID: T-006
  - Requirement(s): FR-770~780；Design: DGN-F-11/14；AC-616/617
  - depends_on: T-001
  - Files/areas: `python/t445/stats_pairing.py`、`python/t445/bootstrap_cluster.py`、`python/t445/gate_verdict.py`、`python/t445/tests/`、gate JSON schema
  - Scope: h_v/h_g/median_g 链；分层配对 cluster bootstrap（B=10000，种子 phase4f-t445-bootstrap-v1，每层抽 3/4 组，组完整携带配对）；正交裁决字段（trajectory_gate_verdict × b3_context_status × decision_required × expansion_used）+ 确定性优先级（GATE_VALIDITY_FAIL 先于普通分类）；Gate JSON schema；合成已知效应数据单测（GO/NO-GO/INCONCLUSIVE/FINAL_INCONCLUSIVE/GATE_VALIDITY_FAIL 全分支 + 优先级 + 正交组合）；同种子复现逐位一致
  - Non-scope: 不消费任何真实 run 结果
  - Outputs: 模块 + schema + 单测证据
  - Validation: AC-616/617 全项（合成覆盖 + 复现性）
  - Gate/STOP: 单测任一分支失败 → 修复后重测；不得降低分支覆盖要求
  - Completion evidence: 单测 PASS JSON + schema 定稿

- [ ] 7. 【B 阶段·资格化】k_eq^E 参考充电/回补 fixture
  - ID: T-007
  - Requirement(s): FR-741/742；Design: DGN-F-06 §6.4；AC-614
  - depends_on: T-001, T-003（runner 复用）
  - Files/areas: `scripts/+t445/fixture_charge_restoration.m`、`results/t445_oracle_gate/qualification/`
  - Scope: 非正式 Model E fixture（固定功率充电段 SOC 0.58→0.62 + 静置，fixture 身份限于 qualification_fixture_manifest 登记项），独立测量母线侧投入能量与储能增量；η_chg_measured vs η_chg_ref=0.9687，容差 ±0.005（绝对）；**PASS → k_eq^E 转 `CANONICAL` 并冻结 config hash（R1）**；run 写访问台账 purpose=QUALIFICATION（R5）
  - Non-scope: 非正式 fixture，不属于正式语料；不产出 headroom 证据；**实现代码不得发明替代 η_path/k_eq 规则——任何替代值需显式预结果批准的设计/config 修订，且全部依赖证据须在新 config hash 下重新生成（R1）**
  - Outputs: qualification JSON（PASS/FAIL + 测量明细）+ config canonical 标记 + 台账条目
  - Validation: 见 Scope 容差
  - Gate/STOP: **PASS → k_eq^E=0.5296 转 canonical；FAIL → STOP 并升级用户（R1；禁自行重算继续）**；T-008/T-011 等全部 k_eq 依赖证据必须在本任务 PASS 后的 canonical config hash 下产出，旧 hash 下的过期证据一律作废重生成（R1）
  - Completion evidence: qualification JSON +（若 PASS）config canonical 标记

- [ ] 8. B2 窗内优化器实现（窗内 DP + 策略表 + V_term）+ 单元/toy 测试
  - ID: T-008
  - Requirement(s): FR-723/724/730~735；Design: DGN-F-03/04/09；AC-611/612/613
  - depends_on: T-001, T-002, T-004, **T-007 PASS（R1：k_eq^E 依赖——V_term 线性项与窗内目标使用 k_eq^E，必须在 canonical config hash 下施工与留证）**
  - Files/areas: `scripts/+t445/b2_window_dp.m`、`scripts/+t445/build_policy_table.m`、`scripts/+t445/b2_objective.m`、`scripts/+t445/tests/`
  - Scope: 策略表构建（λ `0.43:0.005:15.525` 3020×P̄ 66×SOC 3 区域，ecms_law 逐位复制品穷举；λ 轴由冻结原语导出并机器复算 max）；窗内 DP（K=30×10s，SOC 网格 0.01，Δλ 41 级，无速率限制，λ_eff=max(λ_raw,0.43) 无上限，方向效率对）；**禁止越域查询/外推**（`policy_table_oob_query_count=0`）；V_term 目标（DGN-F-04）；不可行回退 Δλ≡0 + 计数；**作用边界结构断言**（优化器仅输出 Δλ，无任何 P_gen/P_bat/commitment 通道——AC-611）；**信息合法性审计钩子**（每窗记录输入窗区间 [t0,t0+300]，供运行期断言——AC-612）；**反套利有效性测试**（AC-613 修订语义：诱惑 fixture (a)(b)(c) 条款）；toy DP 收敛单测
  - Non-scope: 不执行 Q1~Q6 正式资格化（归 T-010）；不跑正式语料
  - Outputs: 模块 + 单测证据
  - Validation: toy 测试 + AC-611/612/613 语义测试 PASS
  - Gate/STOP: 反套利测试失败 → 停，检查 V_term 实现（不得改冻结参数）
  - Completion evidence: 单测 PASS + 反套利证据 JSON

- [ ] 9. B2 分段 runner 实现（SimState 交替 + checkpoint/resume + incident）
  - ID: T-009
  - Requirement(s): FR-730/731/732, FR-777, FR-781；Design: DGN-F-10/15；NFR-714
  - depends_on: T-001, T-002, T-008
  - Files/areas: `scripts/+t445/run_arm_b2.m`、`scripts/+t445/simstate_mgr.m`、`scripts/+t445/incident_log.m`、`artifacts/t445/`（checkpoint 本地）
  - Scope: 10s 段交替循环（SaveFinalState→读 SOC→窗内 DP→注入 Δλ_1 ZOH→恢复续跑）；checkpoint/resume 七元组身份（voyage_id+arm+config_hash+seed+stream_sha256+model_checksum+segment_t0）；incident.jsonl；合成 toy 航次 smoke（短航次端到端跑通，非正式证据）
  - Non-scope: 不跑正式语料；SimState 等价性正式验证归 T-010
  - Outputs: runner 模块 + toy smoke 证据
  - Validation: toy 航次端到端完成；checkpoint 中断-续跑一致性（同身份续跑结果逐位一致）
  - Gate/STOP: SimState 保存/恢复报错或状态不连续 → 停，升级用户（备选方案需新设计，禁静默绕路）
  - Completion evidence: toy smoke PASS + checkpoint/resume 一致证据

## B — 预结果资格化 Gate（全部通过才解锁 C）

- [ ] 10. SimState 等价 Gate + B2_OPTIMIZER_QUALIFICATION（Q1~Q6）
  - ID: T-010
  - Requirement(s): FR-730, FR-781；Design: DGN-F-09 §9.1 / DGN-F-10；AC-612
  - depends_on: T-003, T-008, T-009
  - Files/areas: `scripts/+t445/qual_simstate_equiv.m`、`scripts/+t445/qual_b2_optimizer.m`、`results/t445_oracle_gate/qualification/`
  - Scope: (1) **SimState 等价 Gate**：1 个非正式参考航次，B1 连续跑 vs 分段跑（每 10s save/restore）全信号逐样本相对差 ≤ 1e-10；(2) **B2_OPTIMIZER_QUALIFICATION**：Q1 策略表 parity 覆盖修订全 λ 域 `0.43:0.005:15.525`（节点容差 0 + 1000 随机点覆盖该域而非 [0.43,0.80]，≤20kW）+ `LAMBDA_DOMAIN_COVERAGE` + `policy_table_oob_query_count=0`、Q2 SOC 网格收敛（0.01 vs 0.005，≤0.5%）、Q3 Δλ 网格收敛（0.005 vs 0.0025，≤0.5%）、Q4 300s fixture 代理-vs-真机（fuel ≤1.0%、|ΔSOC|≤0.002）、Q5 确定性首动作（逐位）、Q6 Δλ≡0 sanity（≤1.0%）
  - Non-scope: 不跑正式语料；不产出 headroom 证据
  - Outputs: 两项 qualification JSON（机器可读 PASS/FAIL + 明细）
  - Validation: 冻结容差逐项过
  - Gate/STOP: **SimState 不等价 → B2 正式执行阻断，停，升级用户；Q1~Q6 任一 FAIL → 正式轨迹 Gate 执行阻断，且不得出具 NO_GO_TRAJECTORY_VALUE（R8）**
  - Completion evidence: 两个 qualification JSON 全 PASS

- [ ] 11. B3 适配（Model E 双机聚合 DP）+ B3 parity 资格化
  - ID: T-011
  - Requirement(s): FR-734, FR-749, FR-778；Design: DGN-F-08；AC-614（共享 k_eq^E）
  - depends_on: T-001, T-004, **T-007 PASS（R1：B3 记账/J_FEC 依赖 k_eq^E canonical）**
  - Files/areas: `scripts/+t445/b3_dp_modele.m`（+ecms/dp_* 复用适配层）、`scripts/+t445/qual_b3_parity.m`、`results/t445_oracle_gate/qualification/`
  - Scope: 双机聚合燃油图/P 网格 {0}∪[125:20:1000]/ramp 20·N_on/启停离散 +0.6kg 瞬时松弛/方向效率对/终端带 0.005；资格化五条件：燃油图同一性 <0.5%、SOC 网格收敛 <0.5%、dp_checks 套件、**代理-vs-真机 fixture**（单/双机区+125/500 边界+启停+充放电+低中高 SOC：raw fuel ≤0.5%、|ΔSOC_end|≤0.001、J_FEC ≤0.5%）、记账 parity
  - Non-scope: B3 不影响 trajectory_gate_verdict（b3_context_status 独立）；不在正式语料前先跑 B3 航次（fixture 为非正式）
  - Outputs: B3 求解器 + qualification JSON
  - Validation: 五条件全过 → `QUALIFIED_PASS`；任一失败 → `UNQUALIFIED_CONTEXT`（H_arch/H_residual 不产出，轨迹 Gate 不受损）
  - Gate/STOP: **本任务必须完成，但 QUALIFIED_PASS / UNQUALIFIED_CONTEXT 均为合法终态（R3）**；UNQUALIFIED_CONTEXT 不阻断 T-012 与 B1-vs-B2 主链，仅禁用 H_arch/H_residual 与 B3 dominance 解释；run 写访问台账 purpose=QUALIFICATION（R5）
  - Completion evidence: qualification JSON（PASS 或 UNQUALIFIED_CONTEXT + 失败明细）

- [ ] 12. 正式前冻结总检（Pre-Execution Freeze Gate）
  - ID: T-012
  - Requirement(s): FR-760~764, FR-782；Design: DGN-F-05/07/12；AC-614/615/619
  - depends_on: T-005, T-006, T-007, T-010, T-011
  - Files/areas: `python/t445/pre_execution_freeze.py`、`results/t445_oracle_gate/freeze_record.json`
  - Scope: 机器断言总检——corpus manifest 冻结在位（初始+预留同预注册）；config_sha256 与冻结值一致；k_eq^E 已转 canonical；B2 资格化 PASS；SimState 等价 PASS；T-011 已完成（QUALIFIED_PASS / UNQUALIFIED_CONTEXT 均接受，R3）；test_sealed=true 复核；TERMINAL_SOC_ADMISSIBILITY 规则冻结核验（|ΔSOC|≤0.005 在 config 且先于结果）；硬可行性判据表完整（DGN-F-12 §12.2 逐项非空）；audit_capability_matrix 无计划外 NOT_INSTRUMENTED；**正式访问台账验证（R5，替代笼统“零结果”断言）**：机器核验台账证明——初始 36 航次正式结果计数=0、预留 12 航次正式结果计数=0、无任何正式 val 语料 headroom 工件存在、IID-test 科学访问计数=0、OOD 科学访问计数=0；所有历史 run 均带 purpose ∈ {QUALIFICATION, REGRESSION, SYNTHETIC_SMOKE} 标签且不计入正式访问；NFR-712/713 复核（env manifest 在位 + E 段只读守卫快照不变）
  - Non-scope: 不执行任何航次
  - Outputs: freeze_record.json（全项机器断言）
  - Validation: 全断言 PASS
  - Gate/STOP: 任一断言 FAIL → 停，回到对应任务修复；**本记录是 C 阶段唯一入场券**
  - Completion evidence: freeze_record.json overall=true

## C — 正式初始语料（36 航次 = 9 split_group）

- [ ] 13. 正式执行：36 航次 × B0/B1/B2（+B3 若 QUALIFIED）
  - ID: T-013
  - Requirement(s): FR-720~728, FR-744, FR-746~750, FR-770, FR-777, FR-781；Design: DGN-F-01/02/03/10/12/15；AC-610/611/612/619
  - depends_on: T-012
  - Files/areas: `scripts/+t445/run_formal_corpus.m`、`results/t445_oracle_gate/runs/`（逐 run JSON）、`artifacts/t445/`（checkpoint 本地）、`results/t445_oracle_gate/incidents.jsonl`
  - Scope: 按冻结 manifest 执行 36 航次 × B0/B1/B2 三臂（B3 仅在 QUALIFIED_PASS 下按资格化语义执行）；每 run 产出：raw 六联（fuel_raw/SOC0/SOC_end/ΔSOC/修正量/J_FEC）+ 硬可行性逐项 + 干预审计 + 负担代理 + **λ_eff 离线重建诊断（`lambda_floor_activation_fraction`、`lambda_eff_min`、`lambda_eff_max`；无 cap 激活比例）** + B2 信息合法性日志（每窗区间断言记录）+ **`policy_table_oob_query_count`（B2，须=0）**；B1/B2 完全配对；incident 政策执行（机械失败同身份重跑+记录；持续不可行禁成对删除）；**执行终止状态机（R6）**：本任务 SHALL 至少产出两种机器终态之一——`COMPLETE_FORMAL_CORPUS` / `ABORTED_GATE_VALIDITY_FAIL`；持续控制器/物理不可行发生时：(1) 保全肇事证据；(2) 停止后续 result-bearing 航次执行；(3) 写机器可读 abort/status 工件；(4) 不成对删除；(5) 放行 T-014 进入 validity-only 路径；正式 run 写访问台账 purpose=FORMAL
  - Non-scope: 不执行预留集（T-015 专属）；不修改任何冻结参数；不看 test/OOD
  - Outputs: 36×3（+36 B3）逐 run JSON + incidents.jsonl + 实际 wall time 记录（NFR-714）
  - Validation: 每 run 硬可行性+审计矩阵齐全（AC-619）；run 级完整性校验（信号齐全、时长一致、hash 自洽）
  - Gate/STOP: 任一 run 持续不可行（非机械）→ 按 R6 状态机 ABORTED_GATE_VALIDITY_FAIL 收束（保全证据+机器工件+升级用户），不得静默剔除后续跑；运行时压力不得触发任何参数/语料/统计变更（R6/NFR-714）
  - Completion evidence: 全部 run JSON 落盘 + 配对完整性机器断言 + 审计矩阵全覆盖

## D — 初始 Gate 裁决

- [ ] 14. 初始统计 + 机器裁决
  - ID: T-014
  - Requirement(s): FR-770~780；Design: DGN-F-11/14；AC-616/617（真实数据实例化）
  - depends_on: T-013
  - Files/areas: `python/t445/run_gate.py`、`results/t445_oracle_gate/gate_result.json`
  - Scope: 配对 h_v → h_g → median_g；分层 cluster bootstrap B=10000；95% CI；确定性优先级求值（有效性 → GO/NO-GO/INCONCLUSIVE；b3_context_status 独立）；gate_result.json 落盘；**R6 validity-only 路径**：若 T-013 终态 = ABORTED_GATE_VALIDITY_FAIL，本任务跳过普通 bootstrap/headroom 分类，直接出具 `trajectory_gate_verdict = GATE_VALIDITY_FAIL` 的机器 Gate JSON（附 abort 工件引用），不得因工作流未跑完语料而无机器 Gate 状态；**R7 canonical 终版身份**：本任务（及 T-015）负责写出 `final_gate_pointer.json`——标识 canonical 来源（initial/expanded/validity-fail）、来源工件 SHA256、最终 trajectory_gate_verdict、最终 b3_context_status、expansion_used、corpus hash、config hash、代码 commit
  - Non-scope: 不触发扩样执行（INCONCLUSIVE 时仅标记 expansion 资格，执行归 T-015）；不改阈值
  - Outputs: gate_result.json（正交字段 + 统计量 + CI + 语料/config 引用）
  - Validation: 裁决逻辑与 T-006 单测路径一致（同代码）；schema 校验；SMALL_N_GATE_CAUTION 在场
  - Gate/STOP: verdict=GATE_VALIDITY_FAIL → 停，升级用户；INCONCLUSIVE → 仅解锁 T-015；GO/NO-GO → 跳过 T-015 直接 T-016
  - Completion evidence: gate_result.json + **final_gate_pointer.json** + 分支路径记录

## E — 条件扩样（仅初始 INCONCLUSIVE 解锁）

- [ ] 15. 预留集执行 + 48 航次全量重算
  - ID: T-015
  - Requirement(s): FR-776；Design: DGN-F-11；AC-617
  - depends_on: T-014 且 verdict=INCONCLUSIVE（唯一解锁条件；GO/NO-GO 时本任务显式 SKIP 并记录理由）
  - Files/areas: 同 T-013/T-014 链路；`results/t445_oracle_gate/gate_result_expanded.json`
  - Scope: 冻结预留集 12 航次（3 组）× B0/B1/B2（+B3 若 QUALIFIED）同代码同 config 执行；合并 48 航次/12 组全量重算（同统计同 bootstrap 同阈值；禁合并两个 CI）；产出终判（GO / NO-GO / FINAL_INCONCLUSIVE+decision_required=true）并更新 final_gate_pointer.json（canonical 来源=expanded）；**R6 fail-closed 同样适用于预留执行**：预留执行中持续不可行 → 同一 ABORTED_GATE_VALIDITY_FAIL 状态机收束，T-014 逻辑以 validity-only 路径出终态（pointer 来源=validity-fail）
  - Non-scope: 禁第二次扩样；禁结果导向替换预留组；扩样后仍不定即停（FINAL_INCONCLUSIVE）
  - Outputs: 扩样 run JSON + gate_result_expanded.json
  - Validation: 同 T-013/T-014；预留集身份与 manifest 一致断言
  - Gate/STOP: FINAL_INCONCLUSIVE → 停，`decision_required=true`，升级用户
  - Completion evidence: 终判 gate JSON + 更新的 final_gate_pointer.json +（若 SKIP）跳过理由记录

## F — 收官

- [ ] 16. 最终报告 + 证据索引 + Gate 封印
  - ID: T-016
  - Requirement(s): FR-779~782, FR-745；Design: DGN-F-06/08/11；AC-614/620
  - depends_on: T-014（GO/NO-GO）或 T-015（扩样后终判）
  - Files/areas: `results/t445_oracle_gate/final_report.md`、`results/t445_oracle_gate/evidence_index.json`、相关 README 同步
  - Scope: **仅消费 `final_gate_pointer.json` 指定的 canonical 终版 Gate 身份（R7）**；最终报告（verdict + 主统计 + CI + 分层汇总 + context 分解 H_scalar/H_traj/[H_arch/H_residual 若 QUALIFIED] + k_eq 敏感性 0.8×/1.2×/break-even/K_EQ_INSENSITIVE 条款 + 负担代理 + 干预审计 + 资格化证据链 + incident 汇总 + SMALL_N_GATE_CAUTION + DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE 声明 + 运行预算实测 vs 估算）；证据索引（全部工件 hash 链）；README 两级同步；**T-446 放行语义（R7）：仅 GO_TRAJECTORY_VALUE 自动放行 T-446；NO_GO / FINAL_INCONCLUSIVE / GATE_VALIDITY_FAIL 全部保持下游阻断，除非用户另行签发路线图裁决**；**全部四种终态（GO / NO-GO / FINAL_INCONCLUSIVE / GATE_VALIDITY_FAIL）都必须可机械封印证据与报告——FINAL_INCONCLUSIVE 停止科学采样但不阻止最终报告/证据封印（R7）**
  - Non-scope: 不启动 T-446；不做任何超出 T-445 的科学声称（不声称在线学习控制器可达 ≥2%）
  - Outputs: final_report.md + evidence_index.json
  - Validation: 报告全部强制节在场；索引 hash 与工件一致；Gate JSON 与报告一致
  - Gate/STOP: 证据链断点 → 停，修复索引
  - Completion evidence: 报告 + 索引入库，T-445 spec 状态可转 implementation-complete

## Verification Checklist

- [x] Requirements IDs 被任务引用（FR-720~782 / NFR-710~716 / AC-610~620 全覆盖，见各任务 Requirement(s) 行）
- [x] Design 测试策略落为任务（§17 各测试层 → T-002/003/004/006/008/009/010/011/013）
- [x] 每任务有明确 Validation 与 Gate/STOP
- [x] A→F 证据流保序：B 阶段资格化全过才进 C；E 仅 INCONCLUSIVE 解锁；F 收官不启动 T-446
- [x] 无新科学/设计语义（全部数值/规则引自 requirements/design）

## Execution Notes

- 保持 `Status: tasks-draft` 直至用户书面批准本任务计划；批准后改 `tasks-approved` 并跑 `spec_validate phase=implementation` 再施工；
- 施工开始改 `implementation-in-progress`；全部任务勾选+验证通过后改 `implementation-complete`；
- 任务按 DAG 顺序执行，重排需用户批准；T-007/T-010/T-011 三个资格化任务可并行（依赖允许时）；
- 每任务完成后立即 `spec_task_check`；关键结果（资格化、正式 run、Gate 裁决）主 Agent 独立复核；
- 偏离 design 的实现决策必须先记入 design.md 再施工。

# Phase 4-E 预测器 v3（T-436~T-444）Tasks

Spec: `phase4-e-predictor-v3`  
Status: implementation-complete  
Created: 2026-09-13  
Approved: 2026-09-13（APPROVED WITH 8 REQUIRED CORRECTIONS，R1~R8 闭合）  
Brainstorm: `./brainstorm.md`（E-Q1~E-Q9 批准）  
Requirements: `./requirements.md`（requirements-approved，R1~R5 闭合）  
Design: `./design.md`（design-approved，DGN-E-01~E-18 + R1~R9 闭合）

> 任务编号 T-001~T-019 为本 spec 局部编号（E-Q9 批准任务地图）；每项标注路线文档 T-4xx 映射与 FR/DGN/AC 追溯。
> 顶层任务所有权保持路线文档 T-436~T-444；T-444 为拆分里程碑（T-017=T-444a、T-018=T-443、T-019=T-444b）。
> **全部契约/数值/规则以 requirements.md + design.md（R1~R9 修正后）为唯一事实源，禁止凭记忆施工。**
> 本文件不引入任何新的科学或设计语义。

## 执行约束（全任务通用）

- 分支 `feat/phase4-e`；逐文件 add（禁 `-A`/`git add .`）；Git 红线全适用；subagent 纪律按 AGENTS.md §2~§8（委派前用户授权）；
- **Dataset v3 零字节改动**（FR-572/575）：`datasets_v3/`、`results/dataset_v3/`、`dataset_v3/` 包全部只读；E 派生产物只写 `artifacts/predictor_v3/`（本地）与 `results/predictor_v3/`（精选入库）；
- **sealed 纪律**（FR-606，tasks gate R1 收紧）：T-018 campaign 授权前，禁止一切科学性 IID-test/OOD 访问——含预测器推理、模型评估、分布诊断、指标计算、绘图、样本值查看、科学/人工分析；访问门 fail-closed。**唯一例外 = T-003 `SEALED_PREPROCESS_ONLY`**（只读冻结 raw OOD 航次、调用冻结物化管线、写 E-owned 派生 OOD 窗口工件、只产 schema/计数/hash/完整性元数据——禁产科学分布/target·输入摘要/预测指标/图/样本值）；T-003 产物保持 sealed，T-018 前不得经正常 DatasetView/模型/评估器 API 暴露；T-007 若在解封前传输 OOD 工件到 AutoDL，仅限不透明字节复制 + hash 校验（禁反序列化/科学查看）；
- **单向证据流**（FR-573/Q8）：HPO→CONFIG_FREEZE→MULTISEED_CONFIRMATION→VAL-ONLY WINNER→PRETERMINAL FREEZE→TEST/OOD UNSEAL→FINAL；禁止逆向回流；
- **禁绝对路径**（NFR-422/NFR-442）；机器可读证据全部 JSON 落盘 + canonical hash；失败试验/事故如实留账禁删；
- **平台纪律**（FR-591）：本地 Mac = 开发/单测/smoke/CPU 探针；**result-bearing 运行只在 AutoDL 4090D canonical CUDA 环境**；本地 Metal/MPS 不得静默替代 canonical 后端出正式证据；
- 每任务完成立即 `spec_task_check`；关键结果主 Agent 独立复核（AGENTS.md §14）；
- **STOP 行为**：需改冻结数据集/设计语义/架构实质变更/sealed 污染事件/canonical 环境不可用 → 停下升级用户裁决，不得自行解决。

## 阶段总览

```text
E0:  T-001 → T-002 → T-003 → T-004 → T-005 → T-006 → T-007(EXTERNAL_DEPENDENCY)
E1:  T-008(MLP-Q-v3) → T-009(TCN-Q) → T-010(Transformer) → T-011(TFT)
E2:  T-012(arch json) → T-013(实现+smoke) → T-014(HPO+freeze)
E3:  T-015(5候选×3种子确认) ∥ T-016(消融臂×3种子)
T-444a: T-017(val-only winner + PRETERMINAL 包 + reload 探针)
T-443:  T-018(一次性终评 campaign：候选+消融臂×3种子×test/OOD+三轴)
T-444b: T-019(最终报告 + FINAL 包 + 审计 + 收官申请)
```

## E0 — 基础设施

- [ ] 1. 本地环境与 lockfile + 兼容 smoke + 措辞澄清注记
  - ID: T-001
  - Requirement(s): FR-591, FR-592, FR-609；NFR-442/443；Design: DGN-E-01/15/18；AC-450
  - depends_on: 无（首批）
  - Files/areas: 仓库根 `.venv_tf/`（不入库）、**环境锁冻结路径 `config/predictor_v3/env/`**（Model E 工作区）：`requirements_tf_base.txt` / `requirements_tf_macos_lock.txt`（本任务拥有；`requirements_tf_cuda_lock.txt` 归 T-007）、`results/predictor_v3/env_manifest_macos.json`、`results/predictor_v3/env_smoke_macos.json`、`docs/phase4_research_roadmap.md`（T-444 条目澄清注记）、相关 README + `.gitignore`（`.venv_tf/`、`artifacts/predictor_v3/`、`experiments/predictor_v3/` 条目——Design Gate R1 授权范围）
  - Scope: 建 venv（Python 3.12.7）；装 TF 2.21.0 首选 + tensorflow-metal + numpy 2.x + scipy + pytest；兼容 smoke（合成 1-epoch 小 MLP 前向反向 + 设备清单 + npz 读冒烟）；三件套中本任务只产出 base + macOS lock + macOS manifest（CUDA lock 归 T-007）；roadmap T-444 措辞澄清注记（FR-609/DGN-E-18——正式训练前硬时序）；README/`.gitignore` 登记
  - Non-scope: 不建 AutoDL 环境（T-007）；不训练任何候选；不写预测器代码
  - Outputs: lockfile×2 + manifest + smoke 证据 JSON + roadmap 注记 diff
  - Validation: smoke JSON 记录 PASS；`pip freeze` 与 lock 一致；绝对路径扫描零命中
  - Gate/STOP: 首选 TF/metal 组合 smoke 失败 → 证据驱动降钉 TF 2.20.x 重 smoke（记录）；仍失败 → 升级用户
  - Completion evidence: `env_smoke_macos.json` PASS + manifest/lock 入库记录

- [ ] 2. sealed 数据访问层与访问门 + 负向测试
  - ID: T-002
  - Requirement(s): FR-572, FR-575, FR-577, FR-606；NFR-444；Design: DGN-E-02/16(6)；AC-450
  - depends_on: T-001
  - Files/areas: `predictor_v3/data_access.py`、`predictor_v3/tests/test_data_access.py`、`results/predictor_v3/data_access/`（分片目录）
  - Scope: DatasetView（懒加载 IID npz / OOD npz）；`load_split("train"|"val")` 默认；`load_terminal(...)` 六项核验（campaign 自哈希/PRETERMINAL 工件 hash/Winner 身份/时间戳序/code+eval config 一致/状态机）；campaign 生命周期五态 + INTERRUPTED；per-run 访问日志分片 `data_access/<campaign_id>/<run_id>.jsonl`（开发期 `data_access/dev/<run_id>.jsonl`）
  - Non-scope: 不实现 transform/特征装配（T-004/T-005）；不登记真实 campaign（T-018）
  - Outputs: 模块 + 单测 + 负向测试证据
  - Validation: pytest 全过——三负向形态必须 raise `SealedAccessError`（未登记 campaign 访问 test/OOD；train API 越权 test；OOD 缺 `ood=True`）；六项核验逐项构造性正/反用例
  - Completion evidence: pytest 输出 + 负向检出记录

- [ ] 3. OOD 窗口物化 + 双 hash 确定性/完整性
  - ID: T-003
  - Requirement(s): FR-576, FR-577；Design: DGN-E-03；AC-450
  - depends_on: T-001
  - Files/areas: `predictor_v3/ood_materialize.py`、`predictor_v3/tests/test_ood_materialize.py`、`artifacts/predictor_v3/data/ood_windows_v3_formal.npz`（本地产物）、`results/predictor_v3/ood_windows_integrity.json`
  - Scope: **SEALED_PREPROCESS_ONLY**（tasks gate R1：T-018 前唯一获准的 OOD 预处理例外）——薄驱动只读调用冻结 `dataset_v3`（load_voyage→build_windows→targets→build_views）；realized 条件标签映射（`formal_freeze_result.json` voyage_evidence；缺映射即 FAIL）；不写 `splits` 列；schema 与 IID npz 平行（含 H2 仅审计平行，模型不消费）；双 hash 登记（文件 sha256 + canonical scientific payload sha256）；本地双跑确定性；**只产 schema/计数/hash/完整性元数据——禁产科学分布/target·输入摘要/预测指标/图/样本值；产物保持 sealed，T-018 前不得经正常 DatasetView/模型/评估器 API 暴露**
  - Non-scope: 不改/不复制冻结管线逻辑；不进 `datasets_v3/` 写任何字节；不评估任何模型
  - Outputs: OOD npz（本地）+ integrity JSON（入库）+ 双跑证据
  - Validation: 两跑 canonical payload hash 一致（文件 sha256 另报）；per-condition 窗口数 >0；schema 平行断言；负向：构造缺映射航次必须 FAIL
  - Gate/STOP: 管线行为与 IID 不一致 → 停止升级（不得改冻结代码）
  - Completion evidence: `ood_windows_integrity.json` + 双跑记录

- [ ] 4. `predictor_norm_ext_v1.json` 拟合/冻结/transform 契约
  - ID: T-004
  - Requirement(s): FR-580, FR-581, FR-582；Design: DGN-E-04；AC-450
  - depends_on: T-001, T-002
  - Files/areas: `predictor_v3/norm_ext.py`、`predictor_v3/tests/test_norm_ext.py`、`config/predictor_v3/predictor_norm_ext_v1.json`
  - Scope: K/F 6 通道 per-(channel,lead-bin) train-only 拟合（window-uniform；std<1e-12 → scale=1）；transform-only 应用；禁 refit/clip；常值通道 `OUT_OF_TRAIN_CONSTANT_SUPPORT` 诊断钩子（count/fraction/deviation，不 clamp 不拒样本）；工件自哈希 + 来源 hash 引用（manifest/split/formal_norm_v3）
  - Non-scope: 不拟合 H2；不改 formal_norm_v3.json；不做任何模型训练
  - Outputs: norm 工件（入库）+ 单测
  - Validation: pytest 全过——fit 只用 train 的构造性验证、transform-only、禁 clip、常值诊断触发用例、自哈希复算一致
  - Completion evidence: 工件 + pytest 输出

- [ ] 5. 统一训练/评估/统计 harness
  - ID: T-005
  - Requirement(s): FR-584, FR-593, FR-594, FR-598, FR-599, FR-602, FR-604；Design: DGN-E-05/06/09/10/11/12/16；AC-450
  - depends_on: T-001, T-002
  - Files/areas: `predictor_v3/{features.py, quantile.py, metrics.py, inference_stats.py, seeds.py, train.py, registry.py}`、`predictor_v3/tests/`（test_quantile/test_metrics/test_inference_stats/test_seeds/test_features）、`config/predictor_v3/{training_budget.json, eval_config.json, diagnostics_config.json}`
  - Scope: 特征装配（H1/KF view dict）；`MonotonicQuantileTransform` 共用模块（禁二次投影；稳定 softplus 逆；s0 来源记录）+ kW 域 pinball（30×3 等权）；指标栈（R7 层级原语聚合：禁 mean-of-RMSE/平均 R²；三视图；分段 10-60/70-180/190-300；E300 派生 population R²）；bootstrap（paired split_group B=10000 分母重采样 δ_rel*；two-sample cluster；UNDEFINED fail-safe）；种子派生（冻结公式）；训练循环/回调治理（DGN-E-09 数值冻结）；registry（run/trial）；config 自哈希；训练预算/eval/diagnostics 三 config 冻结落盘（`eval_config.json` 预注册 `canonical_delivery_seed_index=0`——R5）
  - Non-scope: 不实现具体模型（E1/E2）；不执行 HPO/训练；不触 test/OOD
  - Outputs: 模块 + 三 config + 单测全绿
  - Validation: pytest——pinball 对拍参考实现；变换单调性 + 无二次投影断言；指标手算小例；三视图聚合；bootstrap 已知数据带内/带外；种子派生确定性；s0 记录
  - Completion evidence: pytest 输出 + config 文件 hash 记录

- [ ] 6. 本地 CPU 确定性探针
  - ID: T-006
  - Requirement(s): FR-594；Design: DGN-E-10；AC-450
  - depends_on: T-001, T-005
  - Files/areas: `predictor_v3/env_tools.py`（probe 部分）、`results/predictor_v3/determinism_probe_cpu.json`
  - Scope: 固定小配置（合成数据 + 小 MLP，非正式候选）两独立 CPU 进程同种子双跑（deterministic ops 启用）→ 比较预测/指标/权重 hash；bitwise 或数值容差如实记录
  - Non-scope: 不用正式候选；不在 MPS 上声称 bitwise
  - Outputs: 探针证据 JSON
  - Validation: 两跑对比字段齐全；结论如实（bitwise true/false + 容差）
  - Completion evidence: `determinism_probe_cpu.json`

- [ ] 7. AutoDL canonical CUDA 环境 + CUDA smoke/可复现探针
  - ID: T-007
  - **EXTERNAL_DEPENDENCY: USER_PROVIDES_AUTODL_4090D_INSTANCE**
  - Requirement(s): FR-591, FR-592, FR-594；Design: DGN-E-10/15；AC-450
  - depends_on: T-001, T-003, T-004, T-005 + 用户开通 AutoDL 4090D 实例
  - Files/areas: `requirements_tf_cuda_lock.txt`、`results/predictor_v3/env_manifest_cuda.json`、`results/predictor_v3/env_smoke_cuda.json`、`results/predictor_v3/determinism_probe_cuda.json`、传输/重建协议文档（`docs/phase4/e_predictor_v3/autodl_protocol.md`）
  - Scope: AutoDL 镜像/venv 复建（base lock 为源）；cuda lock + manifest（GPU 身份/驱动/CUDA/cuDNN）；传输协议执行（git commit 钉版代码 + rsync 数据 + hash 校验；**解封前 OOD 工件仅不透明字节复制 + hash 校验，禁反序列化/科学查看**——R1）；CUDA 兼容 smoke；CUDA 可复现探针（同种子双跑）；**本地 Metal/MPS 不得替代本环境出 result-bearing 证据**
  - Non-scope: 不执行任何候选 HPO/正式训练（仅环境资格化）；AutoDL 开通/计费（用户侧）
  - Outputs: cuda lock + manifest + smoke + 探针证据 + 协议文档
  - Validation: smoke PASS + 探针证据落盘 + 传输 hash 校验一致记录
  - Gate/STOP: AutoDL 不可用/环境无法资格化 → 升级用户（不得静默改用本地出正式证据）
  - Completion evidence: 三份 JSON + 协议文档入库

## E1 — 基线（T-436~T-439）

> 四任务同构：**实现（本地，非 result-bearing smoke 含 shape/count_params 容量断言）→ HPO 试验清单七步生成并冻结 → canonical 环境正式 HPO（seed_index=0，预算 5/20/20/20）→ selected_config.json 冻结 → model_config.json + input_contract.json + 复杂度证据（参数量/MACs/FP32 体积；训练时长在 T-015 补全）**。
> HPO 只在 canonical CUDA 环境出 result-bearing 运行；本地仅合成数据 smoke。
> **HPO 治理（tasks gate R3 冻结，适用 T-008~T-011 与 T-014）**：每任务必须产出 trial_generation_audit、冻结正式 trial list、完整 trial registry、预算消耗摘要、selected-config 证据；只有具备有效有限 val 证据 + 有效冻结 checkpoint 的 trial 才可分类 `COMPLETED_ELIGIBLE`；只有 COMPLETED_ELIGIBLE trial 可成为 selected_config；`eligible_trial_count==0` ⇒ 任务 FAIL/STOP；预算用尽但有 ≥1 eligible ⇒ 按冻结目标选最优 eligible 并记录限制，禁扩预算；已开始 result-bearing 训练的 trial 即使 OOM/NaN/失败也耗预算位；纯基础设施中断可同 trial_id 重跑 + incident 记录；AC-451 复杂度证据（参数量/MACs·FLOPs/FP32 体积/选中 HPO run 训练时长/可得峰值内存）在 config freeze 时即齐套——T-015 只增补多种子资源统计，不是首来源。

- [ ] 8. MLP-Q-v3 实现/HPO/config freeze
  - ID: T-008
  - Requirement(s): FR-574, FR-583, FR-585, FR-587~589, FR-595~597；NFR-441, NFR-445；Design: DGN-E-07(a)/08/16；AC-451；路线 T-436
  - depends_on: T-001, T-002, T-005, T-007
  - Files/areas: `predictor_v3/models/mlp_q.py`、`predictor_v3/tests/test_mlp_q.py`、`results/predictor_v3/hpo/mlpq_*`、`results/predictor_v3/candidates/mlpq/`
  - Scope: DGN-E-07(a) 冻结结构（H1-only，flatten→64→32→Dense90→reshape→MonotonicQuantileTransform）；shape/容量 smoke（≈0.2M 断言）；试验清单（lr{5e-4,1e-3}×batch{256,512} 全网格 4 ≤5）；正式 HPO；selected_config + model_config + input_contract + 复杂度证据
  - Non-scope: 不继承 v2 任何资产；不用 KF；不触 test/OOD；不定义/不物化 predictor_runtime_period_s（FR-578）
  - Validation: smoke 断言过；trial registry 全量（≤5，含失败）；COMPLETED_ELIGIBLE 规则（E1 preamble R3）机器核对；selected_config 字段/hash 齐；input_contract 校验过
  - Gate/STOP: 预算用尽未收敛 → 如实记录选当前最优并升级说明，禁扩预算
  - Completion evidence: `selected_config.json` + trial list/registry hash + contract 校验记录

- [ ] 9. TCN-Q 实现/HPO/config freeze
  - ID: T-009
  - Requirement(s): FR-574, FR-579, FR-583, FR-587~589, FR-595~597；NFR-441, NFR-445；Design: DGN-E-07(b)/08/16；AC-451；路线 T-437
  - depends_on: T-001, T-002, T-005, T-007
  - Files/areas: `predictor_v3/models/tcn_q.py`、对应测试与 `hpo/tcnq_*`、`candidates/tcnq/`
  - Scope: DGN-E-07(b)（H1-only TCN；容量过滤 ≤1.5M 前置）；空间 C{32,64,96}×dilations×dropout×lr{3e-4,1e-3}，batch=256；≤20 trials；其余同 T-008
  - Non-scope: 同 T-008；不用 KF
  - Validation: 同 T-008 同构 + 容量过滤 audit 在册
  - Completion evidence: 同 T-008 同构

- [ ] 10. Transformer baseline 实现/HPO/config freeze
  - ID: T-010
  - Requirement(s): FR-574, FR-579, FR-583, FR-587~589, FR-595~597；NFR-441, NFR-445；Design: DGN-E-07(c)/08/16 + R3（future self-attn 非因果）；AC-451；路线 T-439
  - depends_on: T-001, T-002, T-004, T-005, T-007
  - Files/areas: `predictor_v3/models/transformer_baseline.py`、对应测试与 `hpo/transformer_*`、`candidates/transformer/`
  - Scope: DGN-E-07(c)（H1+K+F；future self-attention 非因果——30 位合法 K/F 全互见；padding mask 允许）；≤1.5M；空间 d{64,128}×N_e{2,3}×N_d{1,2}×dropout×lr{1e-4,3e-4,1e-3}，batch=128；≤20 trials
  - Non-scope: 禁因果掩码藏合法 K/F；禁 static；不触 test/OOD；不定义 predictor_runtime_period_s（FR-578）
  - Validation: 同 T-008 + K/F 可见性单测（掩码矩阵断言非因果）
  - Completion evidence: 同 T-008 同构

- [ ] 11. TFT-static-free 实现/HPO/config freeze
  - ID: T-011
  - Requirement(s): FR-574, FR-579, FR-583, FR-587~589, FR-595~597；NFR-441, NFR-445；Design: DGN-E-07(d)/08/16 + R3；AC-451；路线 T-438
  - depends_on: T-001, T-002, T-004, T-005, T-007
  - Files/areas: `predictor_v3/models/tft.py`、对应测试与 `hpo/tft_*`、`candidates/tft/`
  - Scope: DGN-E-07(d) static-free 适配（VSN/LSTM enc-dec/interpretable attention；无 static enrichment；未来位不加因果掩码）；≤1.5M；空间 d{64,128}×LSTM{1,2}×dropout×lr{1e-4,3e-4,1e-3}，batch=128；≤20 trials；报告中明示"适配版"
  - Non-scope: 不引 static 协变量；禁 METADATA 提特征；不触 test/OOD；不定义 predictor_runtime_period_s（FR-578）
  - Validation: 同 T-010 同构
  - Completion evidence: 同 T-008 同构

## E2 — REAT-T（T-440~T-441）

- [ ] 12. REAT-T 架构机器可读化（`reat_t_arch.json`）
  - ID: T-012
  - Requirement(s): FR-579, FR-586；NFR-441；Design: DGN-E-07(e)（已批准设计的操作化，**非第二设计门**）；AC-451
  - depends_on: T-001（design 已批准为前提）
  - Files/areas: `config/predictor_v3/reat_t_arch.json`、`predictor_v3/tests/test_reat_t_arch.py`
  - Scope: 把 DGN-E-07(e) 张量级规范落为机器可读架构 config（张量接口/past TCN 分支/future Transformer 分支/lead 位置编码/融合/decoder/norm·residual·dropout 政策/quantile 输出参数化/参数信封 0.5~3.0M + 锚点 Cp=96/df=128/Nf=3/d_dec=128）+ 自哈希；一致性校验器（config ↔ DGN-E-07(e) 条款逐项核对）
  - Non-scope: 不训练；不改设计语义——实质偏差必须停止升级（非本任务可自行吸收）
  - Outputs: arch json + 自哈希 + 一致性校验证据
  - Validation: pytest 一致性校验全过；自哈希复算一致
  - Completion evidence: `reat_t_arch.json` + 校验记录

- [ ] 13. REAT-T 实现 + 单元/shape/容量 smoke
  - ID: T-013
  - Requirement(s): FR-579, FR-586, FR-587；NFR-441；Design: DGN-E-07(e)/08；AC-451；路线 T-441（实现段）
  - depends_on: T-012, T-005
  - Files/areas: `predictor_v3/models/reat_t.py`、`predictor_v3/tests/test_reat_t.py`
  - Scope: 按 `reat_t_arch.json` 实现；合成数据 shape smoke（输出 [B,30,3]）+ count_params 落信封断言（锚点 ≤1.5M；空间极值 ≤3.0M）；K/F 全可见断言；消融臂零替换构造器（同一拓扑，通道 mask 记录——R2）
  - Non-scope: **不消费任何 result-bearing 验证证据**（smoke 全合成数据，不占 HPO 预算）；不训练
  - Outputs: 模型代码 + smoke 证据（含 count_params 机器记录）
  - Validation: pytest 全过（shape/信封/可见性/零替换构造正确性）
  - Completion evidence: smoke 输出 + 参数量记录

- [ ] 14. REAT-T 有界 HPO + selected_config freeze
  - ID: T-014
  - Requirement(s): FR-574, FR-579, FR-586~589, FR-595~597；NFR-441, NFR-445；Design: DGN-E-07(e)/08/16；AC-451；路线 T-441（HPO 段）
  - depends_on: T-013, T-004, T-007
  - Files/areas: `results/predictor_v3/hpo/reatt_*`、`results/predictor_v3/candidates/reatt/`
  - Scope: 试验清单七步（空间 Cp{64,96,128}×df{64,128}×Nf{2,3,4}×d_dec{64,128}×dropout×lr{1e-4,3e-4,1e-3}，batch=128；锚点注入；≤1.5M 有效配置存在性断言）；正式 HPO ≤20 trials（canonical）；selected_config + model_config + input_contract + 复杂度证据
  - Non-scope: 不得因 proposed 身份扩预算；不触 test/OOD；不定义 predictor_runtime_period_s（FR-578）
  - Validation: 同 T-008 同构 + 锚点配置在最终清单的机器断言
  - Gate/STOP: 同 T-008
  - Completion evidence: `selected_config.json` + 清单/registry hash

## E3 — 确认与消融（T-442）

- [ ] 15. 五候选 `{0,1,2}` 正式多种子确认
  - ID: T-015
  - Requirement(s): FR-593, FR-596；NFR-441, NFR-445；Design: DGN-E-10/12（聚合语义）；AC-452；路线 T-436~T-441 确认段
  - depends_on: T-008, T-009, T-010, T-011, T-014（五个 selected_config 全冻结）
  - Files/areas: `results/predictor_v3/confirm/`（逐 run 证据）、`results/predictor_v3/candidates/*/complexity_report.json`
  - Scope: 五候选 × seeds{0,1,2} 在 canonical 环境按 frozen config 训练确认（**非调参——结果不得重开 HPO**）；逐种子 val 指标落盘（per-split_group 值保留，供 DGN-E-12/13 聚合）；增补多种子资源统计（AC-451 复杂度首来源已在 T-008~T-014 config freeze 时齐套——R3）；逐种子证据独立保存（median/mean/std/min/max 汇总）；**每 run 冻结不可变模型工件（5×3=15 个 .keras）**，各登记 model_id/config_hash/seed_index/工件 SHA256/环境 manifest hash/code commit/run 证据 hash（R5）
  - Non-scope: 不改 config；不重开 HPO；不触 test/OOD；不做 winner 选择（T-017）；禁按 val 表现选种子
  - Outputs: 15 个确认 run 证据 + 15 个 .keras 工件登记 + 汇总 JSON
  - Validation: **硬门 = 15/15 有效正式确认 run（R4）**——科学/模型侧失败的正式种子不计数（如实留账），不得减种子/换种子/重开 HPO；持续失败 ⇒ 阻塞升级；val per-group 指标文件齐；种子派生记录与 DGN-E-10 公式一致
  - Completion evidence: 15 有效 run + 工件登记 + 汇总表

- [ ] 16. 架构保持的 REAT-T 信息消融 ×`{0,1,2}`
  - ID: T-016
  - Requirement(s): FR-579, FR-593, FR-596, FR-607；NFR-441；Design: DGN-E-07(e)（R2 零替换）/12；AC-452；路线 T-442
  - depends_on: T-014（frozen REAT-T config）, T-007；可与 T-015 并行
  - Files/areas: `results/predictor_v3/ablation/`
  - Scope: 三臂 `REAT-T(H1)`/`REAT-T(H1+K)`/`REAT-T(H1+K+F)`——**同一冻结拓扑/参数量**，归一化零通道替换（H1=6 通道全零；H1+K=2 个 F 通道置零；H1+K+F 复用 T-015 的 REAT-T 确认运行不重复训练）；非全臂独立训练 ×{0,1,2}（H1 三新跑 + H1+K 三新跑 = **6 个新 .keras 工件冻结**，登记字段同 T-015——R5）；消融 config/contract 记录 active-information-channel mask；val 归因报告（K 增量=H1+K−H1；F 增量=H1+K+F−H1+K；逐种子+聚合，paired split_group bootstrap CI）。**完成条件硬依赖（R4）**：H1/H1+K 训练可与 T-015 并行开始，但 T-016 不得早于 T-015 的 REAT-T {0,1,2} 工件有效冻结而完成；T-016 暴露 9 条 arm-seed 证据条目（6 新训 + 3 引用）
  - Non-scope: 不建 K-only/+H2 臂（未批准）；消融臂不进 winner pool；不触 test/OOD（val 归因在此；test/OOD 属 T-018 campaign）
  - Outputs: 消融 run 证据（9 arm-seed 条目）+ 6 新 .keras 工件登记 + val 归因报告 JSON
  - Validation: 臂间拓扑/参数量一致断言（count_params 相等）；mask 记录齐；归因报告机器可读；H1/H1+K 各 3 有效种子硬门（R4，失败处理同 T-015）
  - Completion evidence: 消融证据包 + 归因报告

## T-444a — Winner 预选（T-017）

- [ ] 17. val-only Winner 选择 + PRETERMINAL 包 + reload 探针
  - ID: T-017
  - Requirement(s): FR-604, FR-605, FR-589, FR-590；NFR-445；Design: DGN-E-12/13/16(5)/17；AC-453；路线 T-444a
  - depends_on: T-015, T-016（E3 全齐——R4 对齐 E3→T-444a rollout）
  - Files/areas: `predictor_v3/winner.py`、`predictor_v3/packages.py`、`results/predictor_v3/winner/`
  - Scope: DGN-E-13 冻结算法逐行实现（pinball → MAE(q50) → 有符号 ΔCE80 → 参数量 → model_id 字典序行政平局；δ_rel 分母重采样；UNDEFINED fail-safe）；`WINNER_SELECTED_PRETERMINAL` 哈希落盘（**先于任何 test/OOD 访问**）；PRETERMINAL_WINNER_PACKAGE（.keras/model_config/input_contract/复杂度/val 证据/reload 探针/包 manifest）；**canonical 交付工件 = 固定 seed_index=0 的正式 .keras**（预注册于 eval_config.json，R5——科学 Winner 身份 = model_id+config_hash，科学证据 = 全部 {0,1,2} 种子按冻结聚合规则；seed1/2 工件保持引用证据；禁按 val 表现选种子）；新鲜进程 reload 探针硬门（canonical 环境，对固定 seed0 工件，val 前 64 窗，max|Δ|≤1e-5）
  - Non-scope: **不访问 test/OOD**；不做任何终评；不重选
  - Outputs: WINNER_SELECTED_PRETERMINAL.json + PRETERMINAL 包 + reload_probe.json
  - Validation: winner 算法单测（构造已知排序/等价/平局/UNDEFINED 用例）；**T-017 时刻只校验已存在事实（R6）**：WINNER_SELECTED_PRETERMINAL 已冻结+哈希、当前无 campaign 处于 UNSEAL_AUTHORIZED/RUNNING/COMPLETE、访问日志无科学性 test/OOD 访问、reload 探针 PASS；不得声称验证未来终评授权时间戳（`winner_selected_utc < terminal_authorized_utc` 的机器校验在 T-018 六核验时执行）
  - Gate/STOP: reload 探针 FAIL ⇒ 停止升级（不得修模型凑过）
  - Completion evidence: 探针 PASS + preterminal 证据 hash

## T-443 — 一次性终评 campaign（T-018）

- [ ] 18. 终评 campaign：候选+消融臂 ×3 种子 ×（IID test + OOD 五条件）+ 三轴诊断
  - ID: T-018
  - Requirement(s): FR-600~603, FR-606~608；NFR-441, NFR-445；Design: DGN-E-02/12/14/16(6,7) + terminal.py；AC-454；路线 T-443
  - depends_on: T-017（PRETERMINAL 冻结）, T-016（消融臂冻结）, T-003（OOD 窗口）, T-005, T-007
  - Files/areas: `predictor_v3/terminal.py`、`predictor_v3/ood_diagnostics.py`、`results/predictor_v3/terminal_campaigns/<campaign_id>.json`、`results/predictor_v3/terminal/`
  - Scope: campaign 登记（DGN-E-16(6) 全字段 + 生命周期 DRAFT→REGISTERED_LOCKED→UNSEAL_AUTHORIZED→RUNNING→COMPLETE）；六核验含 **`winner_selected_utc < terminal_authorized_utc` 机器校验（R6）**；评估器 val 预演证据先行；轴①②数据集级诊断算一次（W1 hash bottom-k、落出率、|z|、分位 p01/p05/p50/p95/p99、top-5、常值通道单列、UNDEFINED_CONSTANT_SUPPORT 路径）；轴③模型级（候选+消融臂 ×3 种子 × test/OOD 五条件）：**canonical 多种子聚合冻结（R7）——每 split_group 先取 seed{0,1,2} median 再构造 IID-test/OOD 条件组值 → two-sample cluster bootstrap；种子永非独立重复单元**；R_deg/ΔCE80/ΔPICP/ΔR²/crossing；正交字段 + 派生标签；SMALL_N_CAUTION 全条件；**消融 H1+K+F 与 REAT-T 候选共享同一模型工件——按工件 hash 去重推理/评估，两个逻辑身份引用同一 canonical 证据（禁产第二份独立计算副本）**；21 个唯一正式模型工件（15 候选 + 6 消融新训）；per-seed metrics_result 全保留；全指标三视图 + 分段；incident ledger（空亦在册）；campaign 关闭合并访问日志审计
  - Non-scope: 不改任何模型/config/winner；不补样；终评结果不回流
  - Outputs: metrics_result ×（模型×种子×集合）+ ood_diagnostics.json + incident ledger + campaign 关闭审计
  - Validation: 六核验通过才解封的机器记录；三轴 schema 校验；small-n 旗标断言；ledger 在册；访问日志合并审计无 test/OOD 前置访问
  - Gate/STOP: 污染事件（FR-608 触发条件）→ 停止升级用户，按污染后果分类处置
  - Completion evidence: campaign 证据包 + 审计记录

## T-444b — 收官（T-019）

- [ ] 19. 最终科学报告 + FINAL_WINNER_EVIDENCE_PACKAGE + 审计
  - ID: T-019
  - Requirement(s): FR-578, FR-579, FR-609, FR-610, FR-611；NFR-440~446；Design: DGN-E-16(10)/17/18；AC-455；路线 T-444b
  - depends_on: T-018
  - Files/areas: `results/predictor_v3/final_report.md`、`results/predictor_v3/FINAL_WINNER_EVIDENCE_PACKAGE/`、相关 README（两级同步）、`项目开发进展.md`
  - Scope: 最终报告（三视图主指标 + T-443 三轴全证据 + 公平性声明〔同数据集/split/target/loss/种子/预算治理〕+ 限制声明〔长航次 window-uniform 加权、simulation-generated、representative 500-t surrogate、小 n OOD 队列、种子变异 vs split_group bootstrap 区分〕）；FINAL 包封印（PRETERMINAL 全成员 + terminal 证据 + 报告 + 最终 manifest）；**机器校验 winner_identity(T-444a)==winner_identity(T-444b)**；sealed 纪律审计（访问日志证明无 test/OOD→model 回流）；README 两级同步（含 model/ 新增预测模型登记）；绝对路径扫描零命中；spec implementation-complete 申请
  - Non-scope: 不重开 winner 选择；不改任何终评证据；不删失败实验
  - Outputs: 最终报告 + FINAL 包 + 审计记录
  - Validation: 身份相等机器校验 PASS；路径扫描零命中；README/索引核对；§15 汇报模板齐项
  - Gate/STOP: 身份不一致/审计发现回流证据 ⇒ 停止升级（科学诚实事件，不得掩盖）
  - Completion evidence: FINAL manifest + 审计输出 + 报告入库

## Verification Checklist

- [ ] Requirements IDs 被任务引用（FR-570~611/NFR-440~446 全覆盖，见追溯节）
- [ ] Design 测试策略被任务覆盖（单测 T-002/004/005/013、负向 T-002/003、确定性 T-003/006/007、smoke T-001/007/008~014、机器校验 T-017/018/019）
- [ ] 每任务有明确 validation 命令或机器可读核对
- [ ] T-018 前无任何任务触 test/OOD（单向信息流；访问门 fail-closed）
- [ ] T-007 EXTERNAL_DEPENDENCY 显式标注；本地 MPS 不替代 canonical

## Requirement 覆盖速查

| 任务 | FR/NFR | DGN | AC | 路线 |
|---|---|---|---|---|
| T-001 | FR-591/592/609, NFR-442/443 | E-01/15/18 | AC-450 | — |
| T-002 | FR-572/575/577/606, NFR-444 | E-02/16(6) | AC-450 | — |
| T-003 | FR-576/577 | E-03 | AC-450 | — |
| T-004 | FR-580/581/582 | E-04 | AC-450 | — |
| T-005 | FR-584/593/594/598/599/602/604 | E-05/06/09/10/11/12/16 | AC-450 | — |
| T-006 | FR-594 | E-10 | AC-450 | — |
| T-007 | FR-591/592/594 | E-10/15 | AC-450 | — |
| T-008~T-011 | FR-583/585/587~589/595~597 | E-07(a~d)/08/16 | AC-451 | T-436/437/439/438 |
| T-012/013/014 | FR-586~589, FR-595~597 | E-07(e)/08/16 | AC-451 | T-440/441 |
| T-015 | FR-593/596 | E-10/12 | AC-452 | T-436~441 确认 |
| T-016 | FR-593/596/607 | E-07(e)/12 | AC-452 | T-442 |
| T-017 | FR-604/605/589/590 | E-12/13/16(5)/17 | AC-453 | T-444a |
| T-018 | FR-600~603/606~608 | E-02/12/14/16(6,7) | AC-454 | T-443 |
| T-019 | FR-610/611/609, NFR-440~446 | E-16(10)/17/18 | AC-455 | T-444b |
| 全程（**总览行，非唯一追溯来源**——具体任务 Requirement(s) 字段为准，R8） | FR-570~575, FR-578~584 | E-01~18 | — | — |

## Execution Notes

- 保持 `Status: tasks-draft` 直至用户书面批准 tasks 门；批准后改 `tasks-approved` 并跑 `spec_validate phase=implementation` 再施工；
- E0 不齐不进 E1（AC-450 为门禁）；T-012 未 freeze 不进 T-014；T-017 未冻结 winner 不进 T-018；
- 依赖允许时 T-003/T-004/T-005 可并行，T-015/T-016 可并行；单写者纪律：同一文件同一时间一个写者；
- 施工期偏差先登记 design.md 偏差区再实施；实质架构/设计语义变更 → 停止升级；
- AutoDL 传输/运行全部记录 commit + 环境 manifest hash + 数据 hash（DGN-E-15）。

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-13 | tasks 草案（T-001~T-019；按批准的 E-Q9 任务地图 + R1~R9 后设计语义） | 主 Agent 起草 |
| 2026-09-13 | **tasks 批准（APPROVED WITH 8 REQUIRED CORRECTIONS，R1~R8 闭合）**：R1 sealed 机械预处理 vs 科学访问区分（T-003 = SEALED_PREPROCESS_ONLY 唯一例外；T-007 解封前 OOD 传输限不透明字节复制）；R2 环境锁冻结路径 `config/predictor_v3/env/`；R3 HPO 合格性/记账/证据（COMPLETED_ELIGIBLE、eligible=0 ⇒ FAIL/STOP、复杂度证据 config freeze 即齐套）；R4 T-015 15/15 有效 run 硬门 + T-016 9 arm-seed 条目/完成依赖 T-015 REAT-T 工件 + T-017 depends_on=T-015,T-016；R5 21 个唯一正式工件冻结 + `canonical_delivery_seed_index=0` 预注册 + 禁按 val 选种子；R6 T-017 只校验已存在事实、时间戳序校验移 T-018 六核验；R7 T-018 canonical 多种子聚合（per-group seed median→two-sample bootstrap）+ 按工件 hash 去重；R8 FR/NFR 具体任务级覆盖（FR-574/578/579、NFR-441/445 落入具体 Requirement(s) 字段） | 用户 tasks gate Final Ruling |
| 2026-09-15 | **implementation-complete（E 段收官）**：T-001~T-019 全部完成（详见上表各行证据；五架构 selected_config、reatt WINNER 4.0047 全局最低、终评 90/90、FINAL 包 96 成员封印、winner 身份校验 True） | 主 Agent 执行 |

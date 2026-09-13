# Phase 4-E 预测器 v3（NN 负荷预测器训练/评估） Requirements

Spec: `phase4-e-predictor-v3`  
Status: requirements-approved  
Created: 2026-09-13  
Approved: 2026-09-13（APPROVED WITH 5 REQUIRED CORRECTIONS，R1~R5 闭合 + 建议项采纳）  
Brainstorm: `./brainstorm.md`（E-Q1~E-Q9 全部 CLOSED/APPROVED，2026-09-13；Q4/Q5 经补充裁决闭合）

## Overview

Phase 4-E 在**已冻结** Dataset v3（Phase 4-D T-020 formal freeze：`test_sealed=true / ood_sealed=true`，split 396/108/72 voyage，几何 L_H=300s@1Hz / H_F=300s / K=30×10s / stride=10s，target `y_k=E_k/10` kW 轨迹 + E300 派生标量）上训练与评估 NN 负荷预测器。覆盖路线文档 T-436~T-444：四基线（MLP-Q-v3 / TCN-Q / TFT / Transformer）+ Proposed REAT-T（design-first）+ T-442 信息消融 + T-443 三轴 OOD 诊断 + T-444 Winner Freeze。

核心治理：**单向证据流**（`TRAIN → VAL HPO → CONFIG FREEZE → 3-SEED CONFIRMATION → VAL-ONLY WINNER SELECTION → WINNER PRETERMINAL FREEZE → TEST/OOD TERMINAL UNSEAL → FINAL SCIENTIFIC EVIDENCE`；禁止 `TEST/OOD → 模型/配置/Winner` 回流）；val-only 选模；test/OOD 一次性终评；平台分离（本地 Mac 开发 + AutoDL 4090D CUDA canonical 正式训练）。

本文件只把已批准的 E-Q1~E-Q9 翻译为 FR/NFR/AC，不新增科学裁决；design-pending 参数以 bounded obligation 形式列出（见「不得冻结事项」节）。

## Scope

### In Scope

- E0 基础设施：本地 `.venv_tf` 重建 + 三 lockfile + 环境 manifest；sealed 数据 loader + 访问门；OOD 窗口物化（冻结 dataset_v3 管线只读消费 raw OOD 语料）；`predictor_norm_ext_v1.json`（K/F 归一化扩展）；统一训练/评估 harness（pinball、anti-crossing、指标栈、bootstrap、registry）；CPU 确定性探针；AutoDL canonical CUDA 环境 + CUDA 可复现探针。
- E1 基线（T-436~T-439）：MLP-Q-v3 / TCN-Q / Transformer / TFT 实现 + 有界 HPO + config 冻结。
- E2 REAT-T（T-440~T-441）：设计冻结（design gate 增补）→ 实现 + 非 result-bearing smoke → 有界 HPO + config 冻结。
- E3（T-442）：五候选 `{0,1,2}` 多种子确认；正式预注册消融臂（REAT-T(H1)/(H1+K)/(H1+K+F)）×`{0,1,2}`。
- T-444a：val-only Winner 选择 + `WINNER_SELECTED_PRETERMINAL` + PRETERMINAL_WINNER_PACKAGE（含新鲜进程 reload 探针）。
- T-443：一个预注册终评活动——5 候选 + 全部冻结消融臂 ×3 种子 ×（IID test + OOD A1/B1/C1/C2/C3 realized 五条件）× Q2 全指标栈 + 三轴诊断 + incident ledger。
- T-444b：FINAL_WINNER_EVIDENCE_PACKAGE 封印 + 最终科学报告 + README 两级同步 + 绝对路径扫描 + spec 收官申请。

### Out of Scope

- Dataset v3 任何字节的再生成/修改（split/norm/target/几何/OOD 定义均冻结，属 D）；
- EMS 控制价值优化、预测入环（属 F 包 T-445~T-455）；
- INT8/PTQ/QAT/TFLite/STM32N6 算子兼容/NPU 编译/N6 时延与内存验证/部署精度退化（属未来部署 Phase，另立 spec）；
- `predictor_runtime_period_s` 定义（保持 UNDEFINED，属 F）；
- K-only / +H2 消融臂（未获批准；如需须显式设计决策）；
- H2 特征面归一化与建模（v1 不建；未来研究需新工件版本/修订）；
- 按 E 结果回头调数据集（禁）；按 test/OOD 结果调模型（禁）；
- 500t 实船参数确认（外部依赖，invalidation rule 沿用 FR-494 语义）。

## User Stories

- 作为研究者，我要五个候选预测器在同一冻结数据集、同一 split、同一 target/loss/种子、同一训练预算治理下公平比较，使架构结论可辩护（AI_MODELING §14）。
- 作为研究者，我要 REAT-T 的信息消融（H1 → H1+K → H1+K+F）分别识别 Known 与 Forecast 未来信息的增量价值，回答"提升到底来自哪里"（backlog B1 兑现的测量装置）。
- 作为研究者，我要三轴 OOD 诊断（输入漂移/target 漂移/IID-vs-OOD 退化）的机器可读证据，使 OOD 泛化结论与归因分离且诚实（T-443；可能比 IID RMSE 更有论文价值）。
- 作为研究者，我要 val-only 选模 + winner 先冻结后解封 + 一次性终评的协议证据，使论文的 test/OOD 数字是无偏终评而非选择污染产物。
- 作为后续部署工程师，我要 winner 的 `.keras` + `input_contract.json` + 复杂度报告 + reload 探针证据，使未来 Keras→量化→N6 链不重犯 T-508 口径事故。

## Functional Requirements

### A. 范围与边界（E-Q1）

- **FR-570**: Phase 4-E SHALL 在单一 spec（本 spec）内持有 T-436~T-444 全部九项路线任务，不得拆分 baselines 与 REAT-T/Winner-Freeze 为独立 spec。
- **FR-571**: E 科学边界 SHALL be `sealed Dataset-v3 → predictor 训练/评估 → Winner Freeze`；E MAY 重训/比较架构、实现五候选、执行冻结特征面消融、评估 IID 与 OOD A/B/C、执行 T-443 三轴诊断、选定并冻结 winner。
- **FR-572**: E SHALL NOT 重生成或修改 Dataset-v3 航次；不修改 train/val/test split；不修改归一化工件（D 侧）；不修改 target/窗口几何；不修改冻结 OOD 定义；不按 E 结果回头调数据集；不按 test/OOD 结果调模型；不执行 EMS 控制价值优化；不把 STM32N6 部署设为硬验收门。数据集语义属 D、预测证据属 E、预测→控制价值属 F。
- **FR-573**: 实现组织 SHALL 按内部阶段 E0（环境/sealed loader/共用 harness）→ E1（基线）→ E2（REAT-T）→ E3（确认+消融）→ T-444a → T-443 → T-444b；T-444 SHALL 记录为拆分里程碑 `T-444a PRETERMINAL WINNER SELECTION → T-443 TERMINAL TEST/OOD EVALUATION → T-444b FINAL WINNER EVIDENCE FREEZE`，路线编号不要求与执行顺序单调一致。

### B. 数据消费与继承（E-Q2-①/④，T-020 语义继承）

- **FR-574**: 全部候选 SHALL 消费未改动的 Dataset-v3 几何与 target 契约（L_H=300s@1Hz、H_F=300s、30×10s、30-bin 未来均值功率轨迹）；一次模型调用预测完整 30-bin 轨迹；禁止重定义 target/horizon/几何；禁止独立训练的短/中 horizon 变体。评估分段（非重叠）SHALL be short 10–60s / mid 70–180s / long 190–300s。`E300_hat=(10/3600)·Σ_k ŷ_k`；不授权独立 E300 预测 head。
- **FR-575**: Dataset v3 零字节改动。E 证据 SHALL 引用冻结资产 hash 链：`windows_v3_formal.npz` SHA256（`203dc92d…`）、`formal_norm_v3.json`（`b64210bc…`）、`split_assignment_v3.json` canonical hash（`ac4c4a76…`）、manifest `scientific_payload_sha256`（`ee285091…`）；开工核验记录入 E 证据。
- **FR-576**: OOD 窗口物化 SHALL 用冻结 `dataset_v3` 管线代码只读消费 200 条 OOD raw 航次（`datasets_v3/formal/ood/FLTB_*.mat`），产物本地留存 + integrity hash 链入库；禁止事后补样、再平衡或修改 realized 分类；物化确定性 SHALL 可复跑验证。
- **FR-577**: OOD 评估 SHALL 使用 realized exclusive 分类（唯一权威）：A1=36 voyages/9 split_groups、B1=40/10、C1=44/11、C2=40/10、C3=40/10；planned→realized transition 引用 `formal_freeze_result.json`；STRESS_SET=0（formal 语料无越域分流）；禁止复活 T-429 已失败的 mission-semantic 分类任务（contrast layer 保持 DESCRIPTIVE_DIAGNOSTIC）。
- **FR-578**: `predictor_runtime_period_s` SHALL remain UNDEFINED（E 不改 manifest 字段）；E 把预测器评估为"一个合法观测/未来信息状态 → 一条 300s 预测"的映射；显式不变量 `dataset stride 10s ≠ predictor runtime period`；运行节奏由 F 包以控制动态/刷新语义/算力证据冻结。

### C. 特征面与归一化（E-Q3-③，E-Q4）

- **FR-579**: 默认特征面冻结：MLP-Q-v3=H1；TCN-Q=H1；Transformer=H1+K+F；TFT=H1+K+F（static 协变量 v1 为空）；REAT-T=H1+K+F。禁止把 scenario/mission 等 METADATA 字段提为 TFT static 特征或任何模型输入。架构声明边界（E-Q3-③）SHALL 同步遵守：仅 `MLP-Q(H1) vs TCN-Q(H1)` 支持历史编码器架构级结论；仅 `TFT/Transformer/REAT-T`（同 H1+K+F）支持全信息架构级结论；`REAT-T(H1)→REAT-T(H1+K)→REAT-T(H1+K+F)` 为信息价值消融链；五模型 T-444 排名 SHALL 表述为 **predictor-system performance ranking** 而非纯架构排名；跨信息面差异不得解释为纯架构效应。
- **FR-580**: E SHALL 新建冻结工件 `predictor_norm_ext_v1.json`（E 侧所有，不改 D 字节）：K/F 连续通道按 `feature × future-lead-bin` 从 **train 窗口**拟合 `mean[c,k]/scale[c,k]`；拟合总体与训练一致 window-uniform；val/test/OOD 只 transform 禁 refit；禁 clip 回 train 支持；支持外取值归一化后保留原样；工件记录版本/Dataset-v3 manifest hash/train-split 身份与 hash/feature·lead 顺序/拟合语义/常值政策/自哈希 SHA256；每个正式候选 input contract 引用其 hash。有效归一化契约 = `formal_norm_v3（H1/Y）+ predictor_norm_ext_v1（K/F）`，不得复制 D 归一化真相为失控副本。
- **FR-581**: 常值 train 通道（ctrl_mode/manual_power_cmd_pct）SHALL 沿用 scale=1 + train_constant=true 约定保留于 H1 面；评估/推理遇常值支持外取值 SHALL 产生显式 `OUT_OF_TRAIN_CONSTANT_SUPPORT` 诊断（count/fraction/deviation 机器可读），不 clamp、不拒样本；常值通道禁算标准化距离比（轴①单独走非零计数/比例/偏差范围）。
- **FR-582**: v1 SHALL NOT 预建 H2 归一化、不把 H2 作为任何正式候选/消融面；未来 H2 预测研究需显式新归一化工件版本或 spec 修订。

### D. 模型与输出契约（E-Q2-②，E-Q3，E-Q7）

- **FR-583**: 正式候选集冻结为五架构：MLP-Q-v3（T-436）、TCN-Q（T-437）、TFT（T-438）、Transformer（T-439）、REAT-T（T-440/441）；`winner_pool = {五候选}`（训练前协议澄清，非结果驱动）。
- **FR-584**: 统一输出面 `[30 lead bins, 3 quantiles]`，τ=[0.1,0.5,0.9]；统一 pinball loss 定义；v1 默认 30 bin × 3 quantile 等权（异权须训练前预注册）；anti-crossing 政策全候选统一（统一单调参数化或统一 crossing penalty，design 定）；**禁止对 test/OOD 预测事后排序**；crossing rate 始终机器审计（即使参数化使 crossing 结构不可能）。
- **FR-585**: MLP-Q-v3 结构冻结 `H1[300,11] → flatten → Dense(64,ReLU) → Dense(32,ReLU) → 共用 quantile 输出适配 → [30,3]`；旧实现仅提供架构 lineage；不继承 v2 schema/归一化/权重/split/target 几何；历史 R²≈0.73 只称 "legacy anchor / historical reference"，不得称"天花板"。
- **FR-586**: REAT-T design-first（T-440）：任何 result-bearing 训练前，design（增补节）SHALL 冻结可复现粒度——张量接口/past encoder（TCN branch）/KF future 编码（Transformer branch）/positional·lead 编码/融合机制/decoder 拓扑/residual·norm·dropout 政策/quantile 输出参数化/参数信封——并产出机器可读架构 config + 自哈希；训练后实质变更（encoder 类型/融合/信息流/decoder 拓扑/loss/容量信封）需新架构版本身份，科学上重大的升级用户裁决；禁止在同一实验身份下按观测结果反复改 REAT-T。
- **FR-587**: 容量信封：MLP-Q-v3 ≈0.2M 近固定；TCN-Q/TFT/Transformer ≤1.5M；REAT-T 0.5~3.0M 且搜索空间 SHALL 含 ≥1 个 ≤1.5M 有效配置；winner 不要求 ≤1.5M。
- **FR-588**: 每个正式候选/配置 SHALL 落机器可读复杂度证据：trainable 参数量、可测 MACs/FLOPs、序列化 FP32 体积、训练时长、可得峰值训练内存；informational，不左右 winner 选择；不得因下游工具链算子支持性改 E 架构（科学 winner 与未来部署衍生模型允许是不同工件）。
- **FR-589**: 每个正式冻结候选 SHALL 随附 `input_contract.json`（硬工件）：Identity（contract 版本/model ID/config hash/Dataset-v3 manifest hash/归一化工件 hash 引用）；Input tensors（名称/dtype/精确 shape/batch 语义/通道顺序）；Feature semantics（canonical 名/H1·K·F 角色/registry 身份/单位/raw 域语义/索引/归一化引用）；Temporal semantics（history 长度/采样率/horizon/bin 数/bin 宽/顺序/参考时刻/KF issue·lead 语义）；Preprocessing（归一化引用/常值通道处置/zero-fill·缺失值政策/NaN·Inf 政策/mask）；Output 冻结 `shape=[batch,30,3]`、顺序 `[q10,q50,q90]`、单位 kW、E300 为派生值。归一化数值权威属冻结归一化工件，contract 内不得造失控副本。HPO scratch trials 豁免完整 contract。
- **FR-590**: Winner 包最小集 + reload 探针硬门：PRETERMINAL_WINNER_PACKAGE 含选中身份、`.keras`、model_config.json+自哈希、input_contract.json、归一化引用、复杂度证据、val 选择证据、`WINNER_SELECTED_PRETERMINAL` hash/时间戳、**新鲜进程 reload 探针证据**（冻结 canonical 环境新鲜进程 reload → 冻结探针输入 → 推理 → 对比保存前参考：reload 成功/I·O shape 精确/输出有限/quantile 语义顺序一致/预测在预注册容差内或 bitwise）；探针不过 ⇒ 不得过 T-444a freeze。

### E. 训练治理（E-Q6，E-Q8）

- **FR-591**: 平台分离：本地 Mac `.venv_tf` = 开发/loader 单测/架构 smoke/小前向·1-epoch smoke/工件检查/CPU 探针；**canonical 正式训练环境 = AutoDL Linux + NVIDIA 4090D CUDA**（全部正式候选训练/HPO/确认跑/正式消融/T-444 证据）；禁止跨 Metal/CUDA 混跑 result-bearing 候选；正式训练开始后 canonical 环境版本变更须再资格化。macOS 环境 = TF+tensorflow-metal（smoke 不过允许证据驱动 fallback）；两平台尽量同 TF/Keras 核心版本，不一致时 canonical CUDA 对正式工件权威。
- **FR-592**: 环境契约工件：`requirements_tf_base.txt` + `requirements_tf_macos_lock.txt` + `requirements_tf_cuda_lock.txt` + result-bearing 环境 manifest（Python/TF/Keras/NumPy 版本、OS/平台、CPU/GPU 身份、Metal 或 CUDA/cuDNN、NVIDIA 驱动（适用时）、依赖锁 hash、git commit、Dataset-v3 manifest/hash）；`pip freeze` 不得声称含包 hash；文件系统路径不是科学环境身份。初始首选 Python 3.12.7 / TF 2.21.0 / NumPy 2.x，以兼容 smoke 证据落锁。
- **FR-593**: 种子治理：根 `phase4e-predictor-v3-seed-v1`；派生 `SHA256(seed_root‖model_id‖config_hash‖seed_index)` + 文档化稳定整数映射；开发/HPO 只用 seed_index=0；正式候选确认与正式消融 = `{0,1,2}`（全候选同集）；多种子确认不得变成第二轮调参；逐种子证据独立保存；汇总至少 median/mean/std/min/max；种子变异不替代 split_group bootstrap。
- **FR-594**: 确定性声明口径 = `environment-frozen, seed-controlled reproducibility`；不声称跨 Metal/CUDA/硬件/平台 bitwise。控制并记录 PYTHONHASHSEED/Python/NumPy/TF-Keras RNG/初始化/shuffle/随机层种子；支持处启用 TF deterministic ops。E0 SHALL 交付 CPU 确定性探针（本地两独立进程双跑比较预测/指标/权重 hash）与 CUDA 可复现探针（canonical 环境同种子双跑）；bitwise 或容差如实落盘。正式可复现锚 = 数据集 hash + config hash + seed + 环境锁/manifest + code commit + 硬件/后端元数据。
- **FR-595**: HPO 治理：正式试验上限 MLP-Q-v3≤5 / TCN-Q≤20 / TFT≤20 / Transformer≤20 / REAT-T≤20（不得因 proposed 身份加预算；REAT-T 设计/单元/shape smoke 不消费 result-bearing val 证据做架构选择时不计 HPO）；全部试验 seed_index=0、只用 train/val、预注册有界搜索空间、冻结 HPO 引擎/政策、config 自哈希、seed/val 指标/时长资源留账、失败与低分试验禁删；主目标 = **validation split_group_macro pinball**，checkpoint/early-stopping 监控与之同口径；预算用尽即止；扩预算需显式批准且不得仅为提升排名。
- **FR-596**: 每架构 HPO 结束 SHALL 产出 `selected_config.json`（架构 ID/搜索空间 hash/trial registry hash/消耗预算/选中 trial/选中 config+hash/seed-0 val 证据/时间戳/commit）；冻结后 `HPO → CONFIG_FREEZE → MULTISEED_CONFIRMATION` 单向；`{0,1,2}` 确认结果不得重开 HPO。
- **FR-597**: 共同优化治理：v1 优化器族 Adam；scheduler/early-stopping/val 监控/max-epoch 上限/checkpoint·best-weight 恢复政策共同治理；架构专属有界 lr/batch 范围仅当 design 预注册；禁止观察正式结果后临时改 lr/batch/patience/scheduler/epoch 预算。

### F. 评估与统计（E-Q2-③/⑤，E-Q5）

- **FR-598**: IID 指标栈冻结：主点预测 q50（条件中位数，如实表述）；主指标 MAE(q50)、次 RMSE(q50)；nMAE/nRMSE 次要且分母仅 train 派生（per-lead 指标用 train per-lead 尺度；global 分母显式冻结）；**禁用 OOD 派生方差归一化 OOD 误差**；raw kW 指标恒保留。概率证据机器可读：mean pinball、per-quantile pinball、per-lead/segment pinball、PICP@80、`coverage_error_80=|PICP80−0.80|`、MPIW、short/mid/long calibration、crossing rate；PICP 不得解释为越高越好（与 MPIW/pinball 联合判读）。E300 仅由 q50 轨迹导出，报 MAE/RMSE/R²（小子集 R² 不稳定时如实报告状态，不硬凑数值）；v2 时代 E300 R² 仅 legacy 描述锚点。
- **FR-599**: 统计层级 `window → voyage → split_group`：指标先 voyage 内聚合、再 split_group 内聚合，split_group 层为主推断估计；CI/bootstrap 重采样单位 = split_group（窗口非独立观测、同族航次非独立单元）；三视图报告 `split_group_macro`（主）/ `voyage_macro`（次要描述）/ `window_pooled`（次要运行视图，禁用于制造窄 CI）；calibration 与 OOD 评估同纪律。训练 = window-uniform 经验风险（全候选同分布同权重；v1 不引入 voyage 归一化权重；长航次加权登记为 declared limitation）。
- **FR-600**: T-443 轴①输入漂移（数据集级证据，按冻结 OOD 定义算一次）：每 OOD 条件 × 每合法连续输入通道/lead 表征至少报告 train p01/p99 支持外比例、p95/p99/max |train-z|、归一化 Wasserstein-1、top-shifting 通道排名；train 为冻结参照；主终评对比 = OOD-vs-IID-test；常值通道单列（FR-581）；**模型级解释只能用该模型实际消费的特征**（F 通道漂移不得解释 H1-only 模型退化）；描述/归因证据，非 Gate。
- **FR-601**: T-443 轴② target 漂移（数据集级）：E300 为主标量；显式注明 `E300 = ȳ/12`（与 30-bin 均值线性等价，不得计为两条独立证据）；至少报告 mean/std/选定分位/train p01/p99 外比例/per-lead mean·std/short-mid-long 分段诊断/E300 与适当 lead·segment 视图归一化 Wasserstein-1；主终评对比 OOD-vs-IID-test；解释不变量 `target shift ≠ predictor failure`（边缘无强漂移不证明无条件分布预测困难）；非 Gate。
- **FR-602**: T-443 轴③预测退化（模型级，每候选/臂分别产出）：**禁 IID-test↔OOD paired bootstrap**；用 two-sample split_group cluster bootstrap（双侧独立重采样→冻结指标→预注册 ratio/difference→区间）；`R_deg=metric_OOD/metric_IID_test` 仅用于正误差/proper-loss 指标（pinball/MAE/RMSE），不得机械套用全指标栈；差值语义：`ΔCE80`、ΔPICP 与 CE80 同报、MPIW 仅语境、`ΔR²`、crossing rate 仅诊断。退化证据 SHALL 用**正交字段**记录（禁止单一互斥多标签字段造成实现相关分类）：(i) 实际效应带 `effect_band ∈ {SMALL (R≤1.2), MODERATE (1.2<R≤1.5), LARGE (R>1.5)}`（实际效应带非显著性定义）；(ii) bootstrap 支持布尔字段 `ci_lower_exceeds_1`（CI_lower(R)>1.0）与 `ci_upper_within_20pct`（CI_upper(R)≤1.2）；(iii) `substantial_degradation=true` iff `R_point>1.5 AND CI_lower(R)>1.2`；可派生汇总标签（`MAINTAINED_WITHIN_20PCT` / `SUPPORTED_DEGRADATION` / `UNCERTAIN`），其 precedence 由 design 冻结；正交效应带/支持字段为 canonical。
- **FR-603**: 小 n 纪律：`split_group_count < 20 ⇒ SMALL_N_CAUTION`（当前全部 OOD 条件命中；非失败条件）；禁止用大窗口数暗示大独立样本量；置信/推断保持 split_group 层；避免小队列强 p-value 式声明。T-443 终评结果不得回流进训练/HPO/特征选择/消融定义/Winner 身份。

### G. 选模与终评（E-Q8，E-Q9）

- **FR-604**: Winner 选择 SHALL be 纯 validation（test/OOD 证据不参与模型/配置/Winner 选择）；层级：① val split_group_macro pinball → ② 仅在预注册实际等价下 val MAE(q50) → ③ |CE80| 更小 → ④ trainable 参数量更小；同 split_group paired 比较；禁止用两个独立 CI 视觉重叠定等价；等价裕度结果查看前预注册（推荐主裕度 = 相对 pinball 差 1%；优选强规则 = paired split-group bootstrap 相对差 CI 落在预注册等价带如 [−1%,+1%]；精确实现 design 冻结）；参数量仅最终平局打破，不作部署惩罚。多种子聚合义务：`{0,1,2}` 确认种子的 Winner 比较聚合/嵌套规则 SHALL 在正式多种子结果被查看前冻结；种子跑不得当作独立 split_group 重复单元；禁止 best-seed 选择；全部候选用同一聚合规则；paired 候选比较保持在共享 split_group 层。
- **FR-605**: Winner 身份 SHALL 在 test/OOD 解封前选定并哈希：`WINNER_SELECTED_PRETERMINAL`（winner 模型/配置身份、候选集、val 证据 hash、选择规则版本/hash、时间戳、code commit）；终评结果不得改变该身份；val-winner 在 test/OOD 表现差 ⇒ 如实报告为科学发现，不重选。
- **FR-606**: 访问门：训练/HPO loader 默认 fail-closed 拒载 IID test 与正式 OOD；解锁需显式 terminal-evaluation 模式 + 已登记 terminal campaign 身份；完整评估器解封前先在 validation 数据上演练；终评 = 一个预注册评估活动（可跨多进程/作业，同 campaign 身份与冻结代码/配置）。
- **FR-607**: 终评范围：5 正式候选 + 全部冻结消融臂 × `{0,1,2}` ×（IID test + OOD A1/B1/C1/C2/C3 realized 条件）× Q2 冻结全指标 + T-443 三轴；轴①②数据集级证据不按模型重算；消融臂不进 winner pool、不改 winner 身份、不重开 HPO、不触发架构/特征变更；意外消融终评结果 = 科学发现非重选触发。
- **FR-608**: 终评事故政策：机械重跑仅当模型工件/配置/归一化/输入契约/指标语义/Winner 身份全不变；每次失败尝试与重跑进 terminal incident ledger；若已查看 test/OOD 结果后又改模型/超参/特征面/归一化/指标定义/聚合语义/OOD 定义/Winner 规则 → 受影响证据失去 untouched-terminal 状态，按 FR-487 污染后果分类。
- **FR-609**: T-444 措辞澄清落账：正式训练前 SHALL 更新本 spec/路线文档措辞——roadmap "prediction + probabilistic calibration + OOD 裁决" 解释为 val 预测与概率证据决定 Winner、OOD 证据进入 T-444 最终科学证据包、OOD 不参与 Winner 选择。

### H. 报告与工程纪律

- **FR-610**: 最终科学报告 SHALL 含：三视图指标（split_group_macro 主）、三轴 OOD 诊断全证据、公平性声明（同数据集/split/target/loss/种子/预算治理）、限制声明（长航次 window-uniform 加权、simulation-generated、representative 500-t-class surrogate、小 n OOD 队列）、口径声明（input_contract 引用）。
- **FR-611**: T-444b SHALL 创建/封印 `FINAL_WINNER_EVIDENCE_PACKAGE`：PRETERMINAL 包全内容 + IID test 证据 + OOD 证据 + T-443 诊断 + 限制/公平性声明 + 最终报告 + 最终工件/证据 manifest；T-444a→T-444b 之间 Winner 身份不变。

## Non-Functional Requirements

- **NFR-440**: 可复现性——正式结果可通过正式脚本重跑；可复现锚 = 数据集 hash + config 自哈希 + seed + 环境锁/manifest + code commit + 硬件/后端元数据（非文件路径）；禁止 notebook 未保存临时状态作为正式结果来源。
- **NFR-441**: 全部评估指标机器可读落盘（`results/…` 精选入库）；失败实验如实记录禁删；HPO trial registry 全量保留。
- **NFR-442**: 仓库内可执行脚本禁硬编码绝对路径（Python 基于 `__file__` 推导；仓库外依赖走环境变量优雅降级）；验收时绝对路径扫描零命中。
- **NFR-443**: §20 README 两级同步——新增/移动/删除文件同步更新所在目录 README；新增模型同步 `model/` 两级文档（README.md 概括 + README_Model.md 详细）。
- **NFR-444**: 训练代码默认不加载 test/OOD（访问门 fail-closed，FR-606）；评估代码路径复用同一冻结实现（禁终评特供版指标）。
- **NFR-445**: AI_MODELING.md 全文红线适用（Test 禁调参/禁重划分/禁隐藏失败/禁改指标/禁偷换预测目标/禁不公平实验条件/仿真数据不称实测）；T-508 口径契约（§22）经 FR-589 落案。
- **NFR-446**: Git 纪律沿用 AGENTS.md §7/§8（逐文件 add、禁 force-push/reset-hard/clean、subagent 默认不 commit）。

## Acceptance Criteria

- [ ] **AC-450**（E0 就绪）按平台分列——本地/macOS：`requirements_tf_base.txt` + `requirements_tf_macos_lock.txt` + macOS 环境 manifest + 本地兼容 smoke + CPU 确定性探针证据；canonical AutoDL：`requirements_tf_cuda_lock.txt` + CUDA 环境 manifest + CUDA 兼容性/可复现探针证据。三 lockfile 在 E0 完成时全部存在，但 CUDA lock 由 AutoDL canonical 环境任务（T-007）拥有/冻结，不由本地 Mac 环境冒充。访问门负向测试 FAIL-closed；OOD 窗口物化确定性复跑一致 + integrity 链入库；`predictor_norm_ext_v1.json` 冻结含自哈希；harness 单测全绿（pinball/指标栈/bootstrap/访问门/常值诊断）。
- [ ] **AC-451**（E1/E2 模型就绪）：五候选实现 + 各自 HPO 在预算上限内完成 + `selected_config.json` ×5 冻结（trial registry 全量在册）；REAT-T 设计冻结记录先于其 result-bearing 训练。**正式候选工件验收（全五候选，非仅 Winner）**：每候选 `input_contract.json` 存在且校验通过；model/config/Dataset-v3/归一化 hash 引用一致；机器可读复杂度证据齐套（FR-588 清单）。T-444 Winner reload 探针保持为独立的 T-444a 硬门（FR-590）。
- [ ] **AC-452**（E3 确认+消融）：五候选 ×`{0,1,2}` 确认证据逐种子在册；消融臂（REAT-T(H1)/(H1+K)/(H1+K+F)）×`{0,1,2}` val 归因报告机器可读。
- [ ] **AC-453**（T-444a）：`WINNER_SELECTED_PRETERMINAL` 哈希时间戳早于任何 test/OOD 访问记录；PRETERMINAL_WINNER_PACKAGE 齐套；新鲜进程 reload 探针 PASS。
- [ ] **AC-454**（T-443）：单 campaign 终评完成——5 候选 + 冻结消融臂 ×3 种子 ×（IID test + OOD 五条件）× 全指标 + 三轴诊断机器可读落盘；incident ledger 在册（含空记录）；消融臂未入 winner pool 证据。
- [ ] **AC-455**（T-444b）：FINAL_WINNER_EVIDENCE_PACKAGE 封印；最终科学报告齐套（FR-610）；README 两级同步；绝对路径扫描零命中；sealed 纪律审计（访问登记证明无 test/OOD→model 回流）；机器校验 `winner_identity(T-444a) == winner_identity(T-444b)`。

## Edge Cases

- OOD 航次窗口物化缺 snapshot/越界：继承 D 纪律——窗口 invalid + 计数记录，禁静默修补；
- 常值通道在 OOD/任何集合出现非零值：`OUT_OF_TRAIN_CONSTANT_SUPPORT` 诊断 + 指标如实报告（FR-581）；
- macOS 首选 TF/metal 组合 smoke 失败：证据驱动 fallback 后落锁（FR-591/592）；
- AutoDL 不可用/环境漂移：canonical 环境为硬依赖，升级用户裁决（不得静默改用本地 MPS 出正式证据）；
- 终评中途崩溃：FR-608 事故政策（机械重跑条件 + ledger）；
- 小评估子集 R² 不稳定：如实报告状态而非数值硬凑（FR-598）；
- val-winner 在 test/OOD 灾难性退化：科学发现如实报告，不重选（FR-605）；
- 种子间方差大：逐种子证据 + median/min-max 如实呈现，不得借多种子阶段重开调参（FR-593）。

## Dependencies and Constraints

- **硬依赖**：Dataset v3 冻结资产（2026-09-13 核验在位，hash 链一致）；冻结 `dataset_v3` 管线代码（只读复用于 OOD 物化）；AutoDL 4090D 实例（外部依赖，用户开通）；
- **环境**：Python 3.12.7；TF 2.21.0 首选（兼容 smoke 落锁）；本地 macOS arm64 + AutoDL Linux CUDA；
- **纪律**：AGENTS.md（Git/质量门禁/subagent 规则）、AI_MODELING.md 全文；MATLAB/Simulink 不需要（E 纯消费 npz/窗口产物）；
- **顺序约束**：FR-573 阶段链与 Q8 单向信息流不得颠倒；REAT-T 设计冻结先于其训练；winner 冻结先于 test/OOD 解封。

## Requirements 中不得冻结的事项（bounded design obligations）

以下事项本文件只冻结义务与法律地位，具体值由 design 冻结：

1. anti-crossing 统一实现（单调参数化 vs crossing penalty）；
2. winner 等价带 paired bootstrap 精确实现（推荐 ±1% 相对 pinball 带）；
3. 各架构 HPO 搜索空间精确边界与架构专属 lr/batch 例外范围；
4. REAT-T 架构全细节（FR-586 清单粒度）；
5. 全局训练预算数值（max epochs/patience/batch 默认/lr 起点）；
6. OOD 窗口物化本地路径与 integrity 登记格式（沿用 D npz_integrity 风格）；
7. `WINNER_SELECTED_PRETERMINAL` / PRETERMINAL·FINAL 包 / incident ledger 的精确 JSON schema；
8. 归一化 Wasserstein-1 的精确归一化口径（轴①②共用定义）；
9. 多种子 Winner 聚合与 paired-bootstrap 嵌套语义（如 per-split_group seed median 后再 paired split_group bootstrap；须在正式多种子结果查看前预注册）。

## Open Questions

- [x] 无阻塞性开放问题；design-pending 清单见上节（均为 non-blocking）。

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-13 | requirements 草案（E-Q1~E-Q9 裁决链翻译；FR-570~611 / NFR-440~446 / AC-450~455） | 主 Agent 起草 |
| 2026-09-13 | **requirements 批准（APPROVED WITH 5 REQUIRED CORRECTIONS，R1~R5 闭合 + 建议项采纳）**：R1 FR-579 补架构声明边界（三层比较 + predictor-system ranking）；R2 FR-602 退化证据改正交字段（effect_band/ci_lower_exceeds_1/ci_upper_within_20pct/substantial_degradation，派生标签 precedence 留 design）；R3 FR-604 + 不得冻结事项第 9 项（多种子 Winner 聚合/嵌套查看前预注册，禁 best-seed，split_group 层 paired）；R4 AC-450 按平台分列 lockfile 归属（CUDA lock 属 T-007）；R5 AC-451 全五候选 input_contract/hash 一致性/复杂度证据验收（reload 探针保持 T-444a 独立硬门）；建议项 AC-455 加 `winner_identity(T-444a)==winner_identity(T-444b)` 机器校验 | 用户 requirements gate Final Ruling |

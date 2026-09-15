# Phase 4-E 预测器 v3（NN 负荷预测器训练/评估） Brainstorm

Spec: `phase4-e-predictor-v3`  
Status: brainstorm-complete  
Created: 2026-09-13

## User Intent

在已冻结 Dataset v3（Phase 4-D T-020 formal freeze，`test_sealed=true / ood_sealed=true`）上训练/评估 NN 负荷预测器，兑现路线文档工作包 E（T-436~T-444）：基线族（MLP-Q 重训 / TCN-Q / TFT / Transformer）+ Proposed REAT-T + 信息消融 + 三轴 OOD 诊断 + Winner Freeze。全程单 spec（`.specs/phase4-e-predictor-v3/`），内部分阶段 E0→E5b。

冻结资产开工核验（2026-09-13，全部一致）：`windows_v3_formal.npz` SHA256 `203dc92d…91497` == `npz_integrity.json`；`formal_norm_v3.json` `b64210bc…65ba` == manifest `preprocessing_artifact_sha256`；`split_assignment_v3.json` canonical-content hash `ac4c4a76…809f` == manifest 记录（管线自有 canonical JSON 配方复算）；manifest `test_sealed/ood_sealed=true`、几何 `L_H=300/H_F=300/K=30/Δ_F=10/stride=10`、target `y_k=E_k/10 kW、E300=ΣE_k/3600 kWh`；npz 仅含 IID 576 航次（197448 窗口：train 136052 / val 36160 / test 25236）；OOD 200 航次（FLTB_*）仅有 raw .mat 语料；realized exclusive OOD：A1=36v/9g、B1=40/10、C1=44/11、C2=40/10、C3=40/10；STRESS_SET=0。

## Phase 4-E Brainstorm 裁决链（E-Q1~E-Q9，2026-09-13 用户逐项书面批准；Q4/Q5 经补充裁决闭合）

### E-Q1 Spec 边界与命名 — APPROVED

- 单 spec 持有完整 T-436~T-444（不拆 E1/E2）；理由：T-444 Winner Freeze 需全部候选在同一证据/治理链下（同 sealed 数据集身份/split/norm/target/特征契约/训练预算/选模协议/val-only 纪律/一次性终评规则）。
- slug 冻结 `.specs/phase4-e-predictor-v3/`；概念链 `Dataset-v3 → Predictor-v3 → Predictive-EMS`。
- E 科学边界：`sealed Dataset-v3 → predictor 训练/评估 → Winner Freeze`。MAY：重训/比较架构、五候选实现、冻结特征面消融、IID 评估、OOD A/B/C 评估、T-443 三轴诊断、选定并冻结 winner。SHALL NOT：重生成/修改数据集航次、改 split/norm/target/几何/OOD 定义、回头按 E 结果调数据集、按 test/OOD 结果调模型、EMS 控制价值优化（F 包）、把 N6 部署设为 E 硬门。
- 数据集语义属 D；预测证据属 E；预测→控制价值属 F。
- 内部阶段：E0（环境/ sealed loader / 共用训练评估 harness，共享前置层不需新路线编号）→ E1（基线 T-436~439）→ E2（REAT-T T-440/441）→ E3（消融 T-442）→ E4（OOD 诊断 T-443）→ E5（选模与 Winner Freeze T-444）。
- Winner 语义：T-444 冻结**预测 winner**，只用冻结选模协议允许的证据；禁止用下游 EMS 节油/控制价值结果选 E winner。

### E-Q2 Target 语义继承与消费 — APPROVED WITH NARROW MODIFICATIONS

- Q2-① 继承冻结几何/target：L_H=300s@1Hz、H_F=300s、K=30×10s、y_k 轨迹 + E300 派生；一次调用输出完整 30-bin 轨迹；禁重定义 target/horizon/几何、禁独立短/中 horizon 变体。评估分段（非重叠）：**short 10-60s / mid 70-180s / long 190-300s**。`E300_hat=(10/3600)·Σŷ_k`；不授权独立 E300 head。
- Q2-② 统一输出面：全候选 `[30 lead bins, 3 quantiles]`，τ=[0.1,0.5,0.9]，统一 pinball loss 定义与 bin/quantile 权重（v1 默认等权，否则须训练前预注册）。anti-crossing 政策全候选统一（统一单调参数化 **或** 统一 crossing penalty，design 定）；**禁止对 test/OOD 预测做事后排序**；crossing rate 始终机器审计。
- Q2-③ IID 指标栈：主点预测=q50（条件中位数，如实表述非均值）；主指标 MAE(q50)，次 RMSE(q50)；不建 mean head/不加 MSE 辅 loss；nMAE/nRMSE 次要且分母只能 train 派生（per-lead 用 train per-lead 尺度；global 分母显式冻结）；**禁用 OOD 方差归一化 OOD 误差**；raw kW 指标恒保留。概率证据（机器可读）：mean pinball、per-quantile pinball、per-lead/segment pinball、PICP@80、`coverage_error_80=|PICP80−0.80|`、MPIW、short/mid/long calibration、crossing rate；PICP 不得解释为越高越好（与区间宽度/pinball 联合判读）。E300 只由 q50 轨迹导出，报 MAE/RMSE/R²（R² 数学不稳定时如实报告状态而非硬凑数值）；v2 时代 R²≈0.73 仅 legacy 描述锚点，严格基线 = v3 重训 MLP-Q。
- Q2-④ `predictor_runtime_period_s` 保持 UNDEFINED（F 包裁决运行节奏）；E 把预测器视作"一个合法观测/未来信息状态 → 一条 300s 预测"的映射；不改 manifest 字段；显式不变量 `stride 10s ≠ runtime period`。
- Q2-⑤ 长航次加权与统计单位：训练用 window-uniform 经验风险（全候选同分布同权重；v1 不引入 voyage 归一化权重；登记为 declared limitation）；评估层级 `window→voyage→split_group`：先 voyage 内聚合、再 split_group 内聚合、**split_group 层为主推断估计**；CI/bootstrap 重采样单位 = **split_group**（禁止把窗口当独立观测、禁止把同族航次当独立单元）；三视图报告 `split_group_macro`（主，科学比较/CI/T-444 证据）、`voyage_macro`（次要描述）、`window_pooled`（次要运行/曝光加权，**禁用于制造窄 CI**）；calibration 与 OOD 评估同纪律。

### E-Q3 基线集与架构路线 — APPROVED WITH MODIFICATIONS

- Q3-① 五正式候选冻结：**MLP-Q-v3 / TCN-Q / TFT / Transformer / REAT-T，全部进入 T-444 Winner Pool**（不得因 Transformer 是机制判别基线而排除；此为训练前协议澄清而非结果驱动修订——任何模型结果被查看前钉死 `winner_pool={五候选}`）。
- Q3-② MLP-Q-v3 冻结：`H1[300,11] → flatten → Dense(64,ReLU) → Dense(32,ReLU) → 共用 quantile 输出适配 → [30,3]`；H1-only；旧实现仅提供架构 lineage，**不继承** v2 schema/归一化/权重/split/target 几何；不得称历史 R²≈0.73 为"天花板"，用 "legacy anchor / historical reference"；输出实现服从 Q2 统一单调性政策；训练预算/patience/schedule/seeds 由 Q8 统一治理。
- Q3-③ 默认特征面：MLP-Q-v3=H1；TCN-Q=H1；Transformer=H1+K+F；TFT=H1+K+F；REAT-T=H1+K+F。TFT static 协变量 v1 保持为空；**禁止把 scenario/mission 等 METADATA 提为 static 特征**。三层比较纪律冻结：A. 历史编码器比较 `MLP-Q(H1) vs TCN-Q(H1)`（同信息面可谈架构）；B. 全信息架构比较 `TFT vs Transformer vs REAT-T`（同 H1+K+F 可谈架构）；C. 信息价值消融 T-442 至少含 `REAT-T(H1)→REAT-T(H1+K)→REAT-T(H1+K+F)`（不得塌缩成 H1 vs H1+KF，K 与 F 增量分别识别）。五模型排名是预测器系统排名；**跨信息面差异（如 REAT-T vs TCN）不得解释为纯架构效应**。
- Q3-④ 容量政策：不强制等参数；预注册架构信封 MLP-Q-v3≈0.2M 近固定、TCN-Q/TFT/Transformer ≤1.5M、REAT-T 0.5~3.0M；**REAT-T 搜索空间必含 ≥1 个 ≤1.5M 有效配置**（容量重叠参照）；winner 不要求 ≤1.5M。每正式候选/配置机器可读复杂度证据：trainable 参数量、可测 MACs/FLOPs、序列化 FP32 体积、训练时长、可得峰值训练内存。INT8/N6 指标 informational，不左右 winner（除非 Q7 改判——Q7 未改判）。
- Q3-⑤ REAT-T design-first：T-440 由 E design gate 兑现（任何 result-bearing 训练前）；初始骨架 `Past H1→TCN branch / Future K/F→Transformer branch / Fusion→trajectory decoder→[30,3]`；design.md 须冻结到可复现粒度（张量接口/past encoder/KF 编码/positional·lead 编码/融合机制/decoder 拓扑/residual·norm·dropout 政策/quantile 输出参数化/参数信封）+ 机器可读架构 config 自哈希；训练后小幅实现修正走偏差登记；encoder 类型/融合机制/信息流/decoder 拓扑/loss/容量信封的实质变更需新架构版本身份，科学上重大的升级用户裁决；禁止在同一实验身份下按 val/test/OOD 观察反复改 REAT-T。

### E-Q4 特征面与消融映射（残留项补充裁决）— APPROVED WITH SCOPE CLARIFICATION

- （Q4-①②已被 E-Q3-③ / E-Q9 Q9-2 裁决覆盖：默认面冻结；消融核心臂 = REAT-T(H1)/(H1+K)/(H1+K+F)；K-only 与 +H2 臂未获批准、默认不建。）
- Q4-③ 归一化扩展：**E 侧新建冻结工件 `predictor_norm_ext_v1.json`**，不改 Dataset-v3 / `formal_norm_v3.json` 任何字节。有效归一化契约 = `D formal_norm_v3（H1/Y 统计） + E predictor_norm_ext_v1（K/F 统计）`；不得把 D 的归一化真相复制成失控的第二份（引用 hash）。K/F 连续输入按 `feature × future-lead-bin` 从 **train 窗口**拟合 `mean[c,k]/scale[c,k]`；拟合总体与训练一致的 window-uniform 曝光语义；val/test/OOD 永不参与拟合/重拟合；transform-only、禁 clip 回 train 支持、支持外取值归一化后保留原样。常值 train 通道沿用已批准约定（scale=1 + train_constant=true + train 支持元数据保留）；评估值越出常值支持 ⇒ 显式 `OUT_OF_TRAIN_CONSTANT_SUPPORT` 诊断（count/fraction/deviation 证据），**不 clamp 不拒样本**。v1 只含 K/F；**不预建 H2 归一化**（H2 非批准面；未来 H2 研究需新工件版本或修订）。工件至少记录：版本、Dataset-v3 manifest hash、train-split 身份/hash、feature/lead 顺序、拟合语义、常值政策、自哈希 SHA256。每个正式候选 input contract 必须引用冻结归一化工件/hash。
- Q4-④ 常值通道告警义务按开题 brief 原文落案（推理/评估遇常值支持外取值必须显式告警，不得当普通 in-support 归一化处理）——机制见 Q4-③ `OUT_OF_TRAIN_CONSTANT_SUPPORT`。

### E-Q5 T-443 三轴 OOD 诊断 — APPROVED WITH STATISTICAL CORRECTIONS

T-443 保持诊断性质，**不构造新模型 PASS/FAIL Gate**。使用冻结 realized-exclusive 正式 OOD 总体（A1 36v/9g、B1 40/10、C1 44/11、C2 40/10、C3 40/10）；禁补样/再平衡/复活 T-429 语义分类。

- 轴① 输入漂移：对每正式 OOD 条件 × 每合法连续输入通道/lead 表征，至少报告——train p01/p99 支持外比例、p95/p99/max |train-z|、归一化 Wasserstein-1、top-shifting 通道排名；train 为冻结支持/归一化参照；**主终评对比 = OOD-vs-IID-test**；常值 train 通道单独走"非常值计数/比例/偏差范围"，禁算零方差通道的不稳定标准化距离比；漂移证据可按 H1/K/F 联合计算，但**模型级解释只能用该模型实际消费的特征**（如 F 通道漂移不得用于解释 H1-only MLP/TCN 的退化）。描述/归因证据，非 Gate。
- 轴② target 漂移：**E300 为主标量**；显式注明 `E300 = ȳ/12`（30×10s 几何下与 30-bin 均值线性等价，**不得计为两条独立科学证据**；均值功率同伴量仅为单位可解释性可报）。至少报告 mean/std/选定分位/train p01/p99 外比例/per-lead mean·std/short-mid-long 分段 target 诊断/E300 与适当 lead·segment 视图的归一化 Wasserstein-1；主终评对比 OOD-vs-IID-test，train 仅支持/语境参照。解释不变量：`target shift ≠ predictor failure`，且边缘 target 无强漂移本身不证明无条件分布预测困难。描述/归因证据，非 Gate。
- 轴③ 预测退化：每个 frozen 正式候选 + 每个 frozen 正式 ablation 臂，在 IID-test 与每 realized OOD 条件上评估。**禁 IID-test↔OOD paired bootstrap**（split_group 非匹配统计单元）；用 **two-sample split_group cluster bootstrap**（各自独立重采样→各算冻结指标→算预注册 ratio/difference→区间）。ratio 只用于数学上有意义的正误差/proper-loss 指标（pinball/MAE/RMSE）：`R_deg=metric_OOD/metric_IID_test`；不得机械套用到全部 Q2 指标；其余用差值语义：`ΔCE80=CE80_OOD−CE80_IID`、ΔPICP 与 CE80 同报、MPIW ratio/difference 仅语境、ΔR²=R²_OOD−R²_IID、crossing rate 仅诊断。**效应带冻结**（正误差 ratio）：R≤1.2 小退化带 / 1.2<R≤1.5 中带 / R>1.5 大带（实际效应带非统计显著性定义）。机器可读标签：`MAINTAINED_WITHIN_20PCT`（仅当 CI_upper(R)≤1.2）、`SUPPORTED_DEGRADATION`（CI_lower(R)>1.0）、`SUBSTANTIAL_DEGRADATION`（R_point>1.5 且 CI_lower(R)>1.2）、否则 `INCONCLUSIVE_OR_INTERMEDIATE`；点估计 R≤1.2 但区间不支持时不得标 maintained。
- 小 n 纪律冻结：`split_group_count<20 ⇒ SMALL_N_CAUTION`（当前全部 OOD 条件命中；非失败条件）；禁止用大窗口数暗示大独立样本量；置信/推断保持 split_group 层；避免小队列强 p-value 式声明。
- 证据所有权：轴①②为数据集级 T-443 证据（每冻结 OOD 定义/视图算一次，不按模型重算）；轴③为模型级证据（每候选/臂分别产出）。T-443 终评结果不得回流进训练/HPO/特征选择/消融定义/Winner 身份。

### E-Q6 训练环境与随机性 — APPROVED WITH PLATFORM-SEPARATION MODIFICATIONS

- Q6-① 本地环境：仓库根 `.venv_tf/`（venv 本体不入库）；禁止把单一跨平台 `pip freeze` 当完整环境契约。至少维护 `requirements_tf_base.txt` + `requirements_tf_macos_lock.txt` + `requirements_tf_cuda_lock.txt` + result-bearing 环境机器可读 manifest（Python/TF/Keras/NumPy 版本、OS/平台、CPU/GPU 身份、Metal 或 CUDA/cuDNN 信息、NVIDIA 驱动（适用时）、依赖锁 hash、git commit、Dataset-v3 manifest/hash 身份）。`pip freeze` 不得声称含包 hash；如需 hash-locked 依赖须用适当锁定工作流显式生成。文件系统路径不是科学环境身份的一部分。
- Q6-② 版本与 canonical 后端：初始首选 Python 3.12.7 / TensorFlow 2.21.0 / Keras 随冻结栈 / NumPy 2.x；以兼容 smoke 证据落锁（非版本号假设）。macOS：TF+tensorflow-metal 用于开发/smoke，首选组合不过 smoke 允许证据驱动 fallback。**正式 result-bearing 训练 canonical 后端 = AutoDL Linux + NVIDIA 4090D CUDA**：全部正式候选训练、HPO、选中模型确认跑、正式消融、T-444 证据用同一冻结 canonical CUDA 环境；macOS/CUDA 尽量同 TF/Keras 核心版本；不一致时 canonical CUDA 对正式模型工件权威；**禁止跨 Metal/CUDA 混跑 result-bearing 候选**；正式训练开始后 canonical 环境版本变更须再资格化，不得静默制造混合环境比较。
- Q6-③ 种子：根 `phase4e-predictor-v3-seed-v1` 冻结；派生 `SHA256(seed_root‖model_id‖config_hash‖seed_index)` + 文档化稳定整数映射。开发/HPO 只用 seed_index=0；正式候选确认 = 全候选同 `{0,1,2}`；T-442 正式消融（H1/H1+K/H1+K+F）也用 `{0,1,2}`；多种子确认阶段不得变成第二轮调参；逐种子证据独立保存；汇总至少 median/mean/std/min/max；种子变异不替代 Q2 的 split_group bootstrap（不同变异源）。
- Q6-④ 确定性声明：`environment-frozen, seed-controlled reproducibility`；不声称跨 Metal/CUDA/硬件/平台 bitwise。控制并记录 PYTHONHASHSEED/Python/NumPy/TF-Keras RNG/初始化种子/shuffle·data-order 种子/随机层种子；支持处启用 TF deterministic ops。**CPU 确定性探针**（E0，本地，两独立 CPU 进程同配置双跑，比较预测/指标/权重 hash，bitwise 或容差如实）+ **CUDA 可复现探针**（E0，canonical 环境同种子双跑，记录环境/设备身份、双跑差异、是否 bitwise、否则容差）。正式可复现锚 = sealed Dataset-v3 hash + model/config hash + seed + 环境锁/manifest + code commit + 硬件/后端元数据（非文件路径）。
- 本地-vs-云端分工：本地 Mac = 开发/loader 单测/架构 smoke/小前向·1-epoch smoke/工件检查/CPU 探针；AutoDL 4090D = 正式训练/HPO/多种子确认/消融/最终证据。无要求本地做全量正式训练。

### E-Q7 int8/STM32N6 边界 — APPROVED WITH ARTIFACT-INTEGRITY CLARIFICATION

- Q7-① E 验收与 T-444 winner 选择**不含**：INT8 量化、PTQ/QAT、TFLite 导出、STM32N6 算子兼容、NPU 编译、N6 时延、MCU/NPU 内存验证、部署精度退化（下游部署关注，需未来独立 spec）。复杂度证据仍机器可读记录（Q3-④ 清单），不左右 winner 选择；不得因下游工具链可能不支持某算子而改 E 架构；**科学 winner 与未来部署衍生模型允许是不同工件**。
- Q7-② 强制 `input_contract.json`（硬工件，非 README 惯例）：至少含——Identity（contract 版本/model ID/config hash/Dataset-v3 manifest hash/归一化工件 hash 引用）；Input tensors（逻辑/张量名、dtype、精确 shape、batch 语义、特征/通道顺序）；Feature semantics（每特征 canonical 名、H1/K/F 角色、source/registry 身份、物理单位、raw 域语义、索引/顺序、归一化引用）；Temporal semantics（history 长度/采样率/horizon/future-bin 数/bin 宽/顺序/参考时刻约定/KF issue-time·lead-time 语义）；Preprocessing（归一化工件/hash、常值通道处置、zero-fill/缺失值政策、NaN/Inf 政策、mask 如适用）；归一化数值权威仍属冻结归一化工件（不在 contract 内造失控副本）；Output 冻结 `shape=[batch,30,3]`、quantile 顺序 `[q10,q50,q90]`、单位 kW、E300 为派生值非模型输出（`E300=(10/3600)·Σq50[1:30]`）。HPO scratch trials 不需完整 contract；每个选中/冻结正式候选必须有。
- Q7-③ Winner 包最小集 + **新鲜进程 reload 探针硬门**：T-444 winner 包至少含 canonical `.keras`、model_config.json+自哈希、input_contract.json、归一化引用/hash、complexity_report.json、canonical 训练环境 manifest/hash、正式 run/seed 身份、源码 git commit、Dataset-v3 manifest/hash、正式评估证据引用、工件 manifest/自哈希。reload 探针：保存 .keras → 冻结 canonical 环境新鲜 Python 进程 → 支持路径 reload → 加载冻结探针输入 → 推理 → 对比保存前参考预测；要求独立 reload 成功、I/O shape 精确、输出有限、quantile 语义/顺序与冻结契约一致、预测在预注册容差内相等（或 bitwise）；探针输入身份/参考输出 hash/观察 delta/verdict 入 winner 证据；**不能独立 reload 复现冻结推理契约的模型不得过 T-444 freeze**。
- 部署边界：E 冻结 `科学预测器工件 + 完整机器可读推理契约`；未来部署 Phase 可做 `Keras→转换→量化/压缩→N6 兼容→时延/内存/精度验证`，不回头改 E winner 选择语义。

### E-Q8 选模协议与 sealed 终评 — APPROVED WITH FAIRNESS AND SEALED-EVALUATION MODIFICATIONS

- Q8-① HPO：正式试验上限 **MLP-Q-v3≤5、TCN-Q≤20、TFT≤20、Transformer≤20、REAT-T≤20**（不得因 REAT-T 是 proposed 而给更大预算；REAT-T 设计/单元/shape smoke 只要不消费 result-bearing validation 证据做架构选择即不计 HPO）。正式 HPO 全部：seed_index=0、只用 train/val、预注册有界搜索空间、冻结 HPO 引擎/搜索政策、config 自哈希、seed、val 指标、时长/资源证据、失败与低分试验全留账（禁删）。主目标 = **validation split_group_macro pinball**；checkpoint/early-stopping 的 val 监控应与主 HPO 目标一致（不得静默优化 window-pooled 口径）。预算用尽即止；扩预算需显式批准（test/OOD 仍 sealed）且不得仅为提升某候选排名。
- Q8-② Winner 选择 = **纯 validation**。test/OOD 证据不参与模型/配置/Winner 选择。终评前先冻结五个正式候选配置并完成 `{0,1,2}` 种子确认。层级：① val split_group_macro pinball；② 仅在预注册实际等价下 val MAE(q50)；③ |CE80| 更小；④ 最终平局 = trainable 参数量更小。同 split_group 上 paired 候选比较；**禁止用两个独立 CI 视觉重叠定等价**；等价裕度在结果查看前预注册（推荐主裕度 = 相对 pinball 差 1%；优选强等价规则 = paired split-group bootstrap 的相对 pinball 差 CI 落在预注册等价带内，如 [−1%,+1%]；最终实现 design 冻结）。参数量仅最终平局打破，不作部署惩罚。
- **Winner preterminal freeze**：Winner 身份在 test/OOD 解封**之前**选定并哈希，证据等价物 `WINNER_SELECTED_PRETERMINAL`（winner 模型/配置身份、候选集、val 证据 hash、选择规则版本/hash、时间戳、code commit）；终评结果不得改变该身份；val-winner 若在 test/OOD 表现差，如实报告为科学发现，不重选。
- **T-444 措辞澄清**：roadmap "prediction + probabilistic calibration + OOD 裁决" 解释为——validation 预测与概率证据决定 Winner；OOD 证据进入 T-444 最终科学证据包；OOD 不参与 Winner 选择。正式训练前更新 Phase-E spec/roadmap 措辞消除歧义。
- Q8-③ 终评访问纪律：训练/HPO loader 默认 fail-closed 拒载 IID test 与正式 OOD；解锁需显式 terminal-evaluation 模式 + 已登记 terminal campaign 身份；完整评估器在解封前先在 validation 数据上演练；正式终评覆盖 **5 候选 × {0,1,2}** ×（IID test + OOD A/B/C + Q2 冻结全指标 + T-443 轴①②③）。"一次性终评"= 一个预注册评估活动（可跨多进程/作业，同 campaign 身份与冻结代码/配置）。
- 终评事故政策：机械重跑仅当模型工件/配置/归一化/输入契约/指标语义/Winner 身份全不变（如序列化/路径/崩溃/推理批处理缺陷）；每次失败尝试与重跑进 terminal incident ledger；若已查看 test/OOD 结果后又改模型/超参/特征面/归一化/指标定义/聚合语义/OOD 定义/Winner 规则 → 受影响 test/OOD 证据失去 untouched-terminal 状态，按 FR-487 污染后果分类。
- Q8-④ 共同优化治理：v1 优化器族 Adam；共同治理覆盖 scheduler 语义、early-stopping 语义、val 监控、max-epoch 上限政策、trial 预算纪律、checkpoint/best-weight 恢复政策；架构专属有界 lr/batch 范围仅当 design 预注册；不得强制每架构 lr=1e-3（若 design 预注册了架构适当范围）；不得在观察正式结果后临时改 lr/batch/patience/scheduler/epoch 预算。
- **Formal config freeze**：每架构 HPO 结束产出 `selected_config.json`（架构/模型 ID、搜索空间 hash、trial registry hash、消耗预算、选中 trial ID、选中 config 及其 hash、seed-0 val 证据、选择时间戳、code commit）；冻结后 `HPO→CONFIG_FREEZE→MULTISEED_CONFIRMATION` 单向；`{0,1,2}` 确认结果不得重开 HPO。
- **信息流不变量冻结**：`TRAIN → VALIDATION HPO → CONFIG FREEZE → 3-SEED CONFIRMATION → VALIDATION-ONLY WINNER SELECTION → WINNER PRETERMINAL FREEZE → TEST/OOD TERMINAL UNSEAL → FINAL SCIENTIFIC EVIDENCE`；禁止逆向路径 `TEST/OOD → 模型/配置/Winner 修改`。

### E-Q9 里程碑与证据形态 — APPROVED WITH STRUCTURAL MODIFICATIONS

- Q9-1 编号 vs 时序：T-444 为拆分里程碑 `T-444a PRETERMINAL WINNER SELECTION → T-443 TERMINAL TEST/OOD EVALUATION → T-444b FINAL WINNER EVIDENCE FREEZE`；局部映射 T-017=T-444a、T-018=T-443、T-019=T-444b；T-017 在任何 test/OOD 访问前冻结 Winner 身份；T-019 在终评证据附加到已冻结 Winner 身份后才闭合 T-444；spec 写显式依赖注记；路线编号不要求与执行顺序单调一致。
- Q9-2 消融终评范围（修改我原 val-only 提案）：正式 T-442 消融臂在预注册、训练、终评解封前冻结后，**进入同一一次性终评活动**，获得 IID test + 正式 OOD + Q2 点/概率指标 + lead 分段诊断 + 退化证据；轴①②数据集级诊断可跨候选/臂共享（不按模型无谓重算）；消融臂**不进 Winner pool、不改已冻结 Winner 身份、不重开 HPO、不在终评结果后触发架构/特征变更**；意外终评消融结果是科学发现非重选触发；核心消融臂 = REAT-T(H1)/(H1+K)/(H1+K+F)；K-only/+H2 臂仅当已被相关 Q4/design 决策显式批准冻结才进 T-016/T-018（当前未批准，默认不建）；不得因任务表草稿出现就新造消融臂。
- 任务图修正 A：T-001 只拥有本地 `.venv_tf` + base/macOS lock + macOS manifest + 本地兼容 smoke；不得在 AutoDL 环境存在前声称 canonical CUDA lock 冻结。T-007 拥有 AutoDL canonical CUDA 环境创建 + CUDA 兼容 smoke + 最终 cuda lock + canonical manifest + 传输/重建协议。
- 任务图修正 B：T-006 = 本地 CPU 确定性探针；T-007 = AutoDL 环境 + CUDA 可复现探针（消除 CUDA 探针排在 CUDA 环境前的依赖倒置）。
- 任务图修正 C：T-017 创建 `PRETERMINAL_WINNER_PACKAGE`（选中 Winner 模型/配置身份、.keras、input contract、归一化引用、复杂度证据、val 选择证据、新鲜进程 reload 探针、WINNER_SELECTED_PRETERMINAL hash/时间戳）——不得称最终 T-444 证据包；T-019 在 T-018 终评后创建/封印 `FINAL_WINNER_EVIDENCE_PACKAGE`（追加 IID test 证据、OOD 证据、T-443 诊断、限制/公平性声明、最终报告、最终工件/证据 manifest）；T-017→T-019 之间 Winner 身份不变。
- 最终批准局部执行结构：E0 = T-001 本地环境+base/macOS lock / T-002 sealed loader+访问门 / T-003 OOD 窗口物化 / T-004 归一化扩展 / T-005 统一训练评估 harness / T-006 CPU 确定性探针 / T-007 AutoDL CUDA 环境+CUDA 探针；E1 = T-008~T-011（MLP-Q-v3/TCN-Q/Transformer/TFT 实现+有界 HPO+config freeze）；E2 = T-012 REAT-T 设计冻结 / T-013 实现+非 result-bearing smoke / T-014 有界 HPO+config freeze；E3 = T-015 五候选 {0,1,2} 确认 / T-016 正式预注册消融 ×{0,1,2}；E5a/T-444a = T-017 val-only winner 选择+PRETERMINAL 冻结+预选包；E4/T-443 = T-018 一个预注册终评活动（5 候选+全部冻结消融臂 ×3 种子 ×IID test+正式 OOD+Q2 全栈+T-443 三轴+incident ledger）；E5b/T-444b = T-019 最终科学报告+三视图/三轴证据+公平性/限制声明+最终 winner 证据包封印+README 同步+绝对路径扫描+spec implementation-complete 申请。**终评结果不得回流进 HPO/模型配置/特征选择/消融定义/Winner 身份**。

## Clarifying Questions Asked

按"一次一问"共 9 问（Q4/Q5 经补充裁决闭合），全部获用户书面裁决：

1. **E-Q1 spec 边界**：单 spec 持 T-436~T-444 还是拆分？slug？→ 单 spec + `phase4-e-predictor-v3` + E 科学边界/内部阶段/winner 语义（见裁决链）。
2. **E-Q2 target 继承与消费**：几何/target 继承确认、统一 [30,3] quantile 输出面、点预测语义与指标栈、runtime period 处置、长航次加权 → 批准 + 分段切片/等价/统计层级修正（见裁决链）。
3. **E-Q3 基线集与架构路线**：五候选、winner pool 边界、MLP-Q-v3 定义、特征面、容量政策、REAT-T design-first → 批准 + Transformer 入 pool/三层比较纪律/≤1.5M 锚点（见裁决链）。
4. **E-Q4 特征面与消融映射**：KF 归一化扩展工件、常值通道告警、H2 地位、消融臂 → 补充裁决（predictor_norm_ext_v1 契约、OUT_OF_TRAIN_CONSTANT_SUPPORT、v1 不建 H2）。
5. **E-Q5 三轴 OOD 诊断量化**：轴①②③指标与判读、CI 策略、小 n 纪律 → 批准 + 统计修正（two-sample cluster bootstrap、效应带标签体系、Wasserstein-1、E300=ȳ/12 等价声明）。
6. **E-Q6 环境与随机性**：venv/lockfile、版本钉板、种子、确定性声明 → 批准 + 平台分离（canonical = AutoDL 4090D CUDA；本地 Mac 开发/探针）。
7. **E-Q7 int8/N6 边界**：不入验收、input_contract.json 硬工件、winner 包形态 + reload 探针硬门 → 批准 + 工件完整性清单。
8. **E-Q8 选模协议与 sealed 纪律**：HPO 预算、winner 纯 val（方案 a）、等价带、访问门、终评范围、事故政策、共同优化治理、单向信息流 → 批准 + REAT-T 同预算/等价带实现 design 冻结/消融臂进终评。
9. **E-Q9 里程碑与证据形态**：T-001~T-019 任务图 → 批准 + T-444a/T-443/T-444b 拆分、T-001/T-006/T-007 责任修正、PRETERMINAL/FINAL 包生命周期。

## Proposed Approach

单向证据流（Q8 冻结）：

```text
TRAIN → VALIDATION HPO → CONFIG FREEZE → 3-SEED CONFIRMATION
  → VALIDATION-ONLY WINNER SELECTION → WINNER PRETERMINAL FREEZE
  → TEST/OOD TERMINAL UNSEAL → FINAL SCIENTIFIC EVIDENCE
禁止：TEST/OOD → 模型/配置/Winner 修改
```

施工顺序（E-Q9 批准结构）：E0 基础设施（本地环境 → sealed loader → OOD 物化 → 归一化扩展 → harness → CPU 探针 → AutoDL 环境+CUDA 探针）→ E1 四基线（实现+有界 HPO+config freeze）→ E2 REAT-T（设计冻结→实现 smoke→有界 HPO）→ E3 五候选 3 种子确认 + 消融 ×3 种子 → T-444a val-only winner 预选+包 → T-443 一次性终评（候选+消融臂 ×3 种子 × test/OOD 全指标+三轴）→ T-444b 最终证据包封印+报告。

技术主线：冻结 Dataset v3 只读消费（OOD 窗口由冻结 dataset_v3 管线代码物化，本地留存+integrity 链）；统一 harness（pinball/anti-crossing/Q2 指标栈/三视图聚合/split_group paired+two-sample bootstrap/config 自哈希/种子派生/run·trial registry/访问门 fail-closed）；全部评估机器可读落盘。

## Assumptions

1. Dataset v3 冻结资产在位且 hash 核验一致（2026-09-13 已实证：npz/norm/split/manifest 全一致）；
2. npz 只含 IID 窗口——OOD 200 航次 raw .mat 语料在位，可由冻结 dataset_v3 管线代码确定性物化为窗口（只读消费，不改数据集字节）；
3. formal K/F 六通道带真实信号（已核：非零方差；u_fcst 为因果预报机制非 persistence 常量）；KF 无 D 侧归一化统计，由 E 侧 predictor_norm_ext_v1 补（E-Q4 批准）；
4. 本地 Mac 可承担开发/单测/smoke/CPU 探针；正式训练由 AutoDL 4090D CUDA 承担（E-Q6 平台分离裁决；AutoDL 开通为用户侧外部依赖）；
5. `.venv_np2`（numpy 2.5.3 数值栈）为 dataset 管线专用，E 不动它；E 新建 `.venv_tf`；
6. MATLAB R2026a 在位但 E 训练/评估纯消费 npz/窗口产物，不需跑 Simulink；
7. `target_vessel_confirmed=false` 表述纪律延续；仿真数据明确 simulation-generated；
8. AI_MODELING.md 全文红线适用（Test 禁调参/禁重划分/禁隐藏失败/禁改指标/禁偷换目标；§22 T-508 口径契约）。

## Remaining Doubts

以下均为 **non-blocking design-pending**（由 design 阶段冻结，不重开 brainstorm）：

1. anti-crossing 统一实现二选一（单调参数化 vs crossing penalty）——Q2-② 授权 design 定；
2. winner 等价带 paired bootstrap 精确实现（推荐 ±1% 相对 pinball）——Q8-② 要求 design 冻结；
3. 各架构 HPO 搜索空间精确边界与 lr/batch 架构例外范围——Q8-①/④ 授权 design 预注册；
4. REAT-T 架构全细节（Q3-⑤ 清单）——T-012 design 增补冻结；
5. 全局训练预算数值（max epochs/patience/batch 默认）——Q8-④ 授权 design 冻结；
6. OOD 窗口物化的本地路径与 integrity 登记格式——design 定（沿用 D 的 npz_integrity 风格）；
7. `WINNER_SELECTED_PRETERMINAL` 与 PRETERMINAL/FINAL 包的精确 JSON schema——design 定。

## Decision

- [x] User approved creating the spec skeleton and drafting requirements.（2026-09-13，E-Q9 裁决："Proceed to consolidate the rulings into brainstorm.md, create the spec skeleton, and prepare the requirements draft for review."）

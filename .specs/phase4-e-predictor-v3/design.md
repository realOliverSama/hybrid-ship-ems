# Phase 4-E 预测器 v3 Design

Spec: `phase4-e-predictor-v3`  
Status: design-approved（实现完成，见 tasks implementation-complete）  
Created: 2026-09-13  
Approved: 2026-09-13（APPROVED WITH 9 REQUIRED CORRECTIONS，R1~R9 闭合）  
Brainstorm: `./brainstorm.md`（E-Q1~E-Q9，2026-09-13 批准）  
Requirements: `./requirements.md`（requirements-approved，2026-09-13，R1~R5 闭合）

## Summary

本设计把 requirements 的 FR-570~611 / NFR-440~446 / AC-450~455 落成可施工方案，并**冻结全部 9 项 bounded design obligations**（anti-crossing、等价带/bootstrap 算法、HPO 搜索空间、REAT-T 全架构、训练预算、OOD 物化、工件 schema 集、归一化 W1、多种子聚合嵌套）。设计决策编号 **DGN-E-01~DGN-E-18**（与 D 包 DGN-01~17 区分）。

单一事实源纪律沿用 D 包：机器可读 config/契约优先，Markdown 为解释层；全部 hash 配方 = canonical JSON（`json.dumps(sort_keys=True, separators=(',',':'), ensure_ascii=False)`）SHA256、排除自引用 hash 字段——与 `dataset_v3.validate_contracts.canonical_json` 同一配方。

## Goals and Non-Goals

### Goals

- G1（FR-574~578, FR-576）：sealed 数据访问层 + OOD 窗口物化 + 归一化扩展，零字节改动 Dataset v3；
- G2（FR-583~590, FR-586）：五候选架构可复现规范 + 统一 quantile 输出契约 + input_contract + winner 包；
- G3（FR-591~597）：平台分离环境工程 + 种子/确定性治理 + HPO 治理 + 共同优化治理；
- G4（FR-598~603）：指标栈 + 统计推断 + T-443 三轴诊断实现规范；
- G5（FR-604~609, FR-611）：val-only winner 选择算法 + 访问门/终评活动/事故账 + FINAL 包。

### Non-Goals

- 不重开任何已冻结科学决策（见 requirements 与 brainstorm 裁决链）；
- 不实现 EMS 控制侧任何内容；不做 int8/tflite/N6 工程；
- 不定义 predictor_runtime_period_s；不建 H2 面、不建 K-only/+H2 消融臂；
- AutoDL 账号开通与计费决策（用户侧外部动作）。

## Architecture

```text
[冻结输入]（只读）
  model/simulink_modelE_(Phase4_Physics)/
    datasets_v3/formal/windows_v3_formal.npz        ← IID 窗口（本地，136MB，hash 核验）
    datasets_v3/formal/ood/FLTB_*.mat               ← OOD raw 语料（200 航次，入库）
    results/dataset_v3/formal/{formal_norm_v3.json, split_assignment_v3.json,
                               dataset_v3_formal_manifest.json, formal_freeze_result.json}
    dataset_v3/                                      ← 冻结管线代码（只读复用）

[E 新增代码]（入库）
  model/simulink_modelE_(Phase4_Physics)/predictor_v3/
    data_access.py        —— sealed loader + 访问门 + access log（DGN-E-02）
    ood_materialize.py    —— OOD 窗口物化薄驱动（DGN-E-03）
    norm_ext.py           —— predictor_norm_ext_v1 拟合/加载/transform（DGN-E-04）
    features.py           —— 特征面装配 H1/KF view → 模型输入 dict（DGN-E-05）
    quantile.py           —— 统一 quantile 头/anti-crossing/pinball（DGN-E-06）
    models/               —— mlp_q.py / tcn_q.py / transformer_baseline.py / tft.py / reat_t.py（DGN-E-07）
    hpo.py                —— 试验清单预注册 + 预算执行 + trial registry（DGN-E-08）
    train.py              —— 训练循环/回调/预算治理（DGN-E-09）
    seeds.py              —— 种子派生与 RNG 固定（DGN-E-10）
    metrics.py            —— Q2 指标栈 + 三视图聚合（DGN-E-11）
    inference_stats.py    —— paired/two-sample cluster bootstrap + 等价带（DGN-E-12）
    winner.py             —— winner 选择算法 + PRETERMINAL 证据（DGN-E-13）
    ood_diagnostics.py    —— T-443 三轴诊断（DGN-E-14）
    terminal.py           —— 终评活动编排 + incident ledger（DGN-E-16 schema）
    packages.py           —— winner 包/reload 探针/FINAL 封印（DGN-E-17）
    env_tools.py          —— lockfile/manifest/smoke 工具（DGN-E-15）
    tests/                —— pytest 单元 + 负向 + 确定性
  model/simulink_modelE_(Phase4_Physics)/config/predictor_v3/
    hpo_spaces.json            训练预算 training_budget.json    eval_config.json
    diagnostics_config.json    reat_t_arch.json（T-012 产物）

[E 产物]
  results/predictor_v3/         ← 机器可读证据/报告（精选入库）
  artifacts/predictor_v3/data/ood_windows_v3_formal.npz  ← OOD 窗口（E-owned 本地命名空间，不入库 + integrity JSON 入库；**不写入冻结 datasets_v3/ 命名空间**）
  experiments/predictor_v3/     ← 训练中间产物/checkpoint（本地不入库）

[平台]  本地 Mac .venv_tf（开发/单测/smoke/CPU 探针）
        AutoDL Linux 4090D CUDA = canonical（全部 result-bearing 运行，DGN-E-15）
```

## 详细设计

### DGN-E-01 布局与命名（FR-573/575；NFR-442/443）

上表路径冻结。`predictor_v3` 包与 D 包 `dataset_v3` 平级共存于 Model E 工作区；不改 `dataset_v3/` 任何字节（只读 import）。大数据（OOD npz、checkpoint、TensorBoard log）本地不入库；机器可读证据 JSON/报告精选入库。禁绝对路径（`__file__` 推导 `MODELE_ROOT`；AutoDL 侧用环境变量 `EMS_PREDICTOR_V3_ROOT` 注入并优雅降级）。`artifacts/predictor_v3/`（锚定 Model E 工作区根）为 E-owned 本地产物命名空间——Dataset-v3 冻结文件/目录保持只读输入，E 派生产物不得写入 `datasets_v3/`；该目录的 `.gitignore` 条目新增属 Design Gate R1 授权范围，将在 tasks 显式登记。

### DGN-E-02 数据访问层与 sealed 访问门（FR-572/575/577/606；NFR-444）

- API：
  - `load_split("train"|"val") -> DatasetView`：默认路径，任何训练/HPO/val 评估代码使用；
  - `load_terminal("test"|"ood", campaign_id) -> DatasetView`：**不得仅凭 campaign JSON 存在或某状态字符串授权**——返回终评 DatasetView 前必须六项核验（字段见 DGN-E-16(6)）：① campaign 工件自哈希一致；② 引用的 PRETERMINAL winner 工件存在且 hash 匹配；③ 冻结 Winner 身份匹配；④ Winner 选择时间戳早于终评授权时间戳；⑤ 代码 commit/eval config 身份与 campaign 冻结一致；⑥ campaign 状态机允许终评访问。任一不过 ⇒ `raise SealedAccessError`；
  - DatasetView 懒加载 npz，返回规范化张量 dict（见 DGN-E-05）。
- 访问日志（R9 分片——避免多进程/云执行对单一共享文件的非同步写）：不可变 per-run 分片 `results/predictor_v3/data_access/<campaign_id>/<run_id>.jsonl`（开发期单进程写 `data_access/dev/<run_id>.jsonl` 同构分片）；campaign 关闭时合并/审计；字段 timestamp/split/campaign_id/run_id/caller 摘要/git commit——sealed 纪律审计（AC-455）的机器证据。
- OOD 访问需 `ood=True` 显式形参 + campaign 登记；常值通道检测挂钩在 transform 内（DGN-E-04）。
- 负向测试：未登记 campaign 访问 test/OOD、`train` API 越权传 `split="test"`、OOD 无 `ood=True`——三种形态必须 raise（pytest）。

### DGN-E-03 OOD 窗口物化（FR-576/577；obligation 6）

- 驱动 `predictor_v3/ood_materialize.py`（E 所有新代码），**只读调用冻结实现**：`dataset_v3.run_pipeline.load_voyage` → `windowing.build_windows`（stride=10，同一冻结几何/合法性规则）→ `targets` 同构 target 构建 → `build_views` 白名单。禁止复制/改写冻结代码逻辑。
- 每窗口标签：`voyage_id / anchor_t0_s / window_index_in_voyage` + `ood_condition`（realized exclusive，来自 `formal_freeze_result.json.ood_evidence.voyage_evidence` 的 `voyage_id→ood_condition_id` 映射；映射缺失任一航次即 FAIL）；**不写 `splits` 列**（OOD 无 train/val/test 语义）。
- 视图与 IID npz schema 平行（H1/H2/KF/Y/E300 同名同形），使同一 loader 通吃；H2 物化仅为 schema 平行与审计完整性，v1 模型不消费（FR-582）。
- 产物：`artifacts/predictor_v3/data/ood_windows_v3_formal.npz`（E-owned 本地命名空间，savez_compressed）+ `results/predictor_v3/ood_windows_integrity.json`（入库）：**双 hash**——npz 文件 sha256（存储/完整性）+ **canonical scientific payload sha256**（对 array name/dtype/shape/raw bytes 的 canonical 哈希）；n_windows、per-condition 窗口/航次计数、生成命令、`dataset_v3` 包代码 hash（文件集 canonical hash）、驱动脚本 SHA256、源证据 hash（formal_freeze_result.json + realized 映射）、源 OOD 语料清单 hash（200 文件名+各自 sha256）。
- 确定性验证：本地连跑两次——**两跑科学确定性以 canonical payload hash 一致为门**；npz 文件 sha256 逐位一致可另报但不得作为唯一确定性定义；记录双跑证据；任一 per-condition 窗口数 == 0 即 FAIL。

### DGN-E-04 归一化扩展 `predictor_norm_ext_v1.json`（FR-580/581；obligation 内嵌）

- Schema：`{schema:"predictor_norm_ext_v1", version, fit_on:"train_only", source:{dataset_manifest_sha256, split_assignment_sha256, formal_norm_v3_sha256}, kf:{<channel>:{mean:[30], scale:[30]}}, constant_policy:{scale_one:true, train_constant_fields:[...]}, window_uniform_fit:true, artifact_sha256}`。
- 拟合：train split 全部窗口（window-uniform），每 (channel, lead-bin) 的 mean/std（std<ε 时 scale=1 保护，ε=1e-12）；拟合代码 `norm_ext.py fit` 只在 train 上运行；val/test/OOD 只 transform。
- transform：`(x−mean[c,k])/scale[c,k]`；禁 clip；支持外取值原样保留。
- 常值通道（H1 侧 ctrl_mode/manual_power_cmd_pct，D 已 scale=1）：transform 后检测 `|x_raw − train_constant_value| > 0` ⇒ 记 `OUT_OF_TRAIN_CONSTANT_SUPPORT` 诊断（count/fraction/deviation min/max、按 split/condition 分组）写入评估证据；不 clamp、不拒样本。
- H2：v1 不拟合、不暴露（FR-582）。

### DGN-E-05 特征面装配（FR-579）

- `features.py` 输出模型输入 dict：`"hist"` float32 [B,300,11]（H1，frozen formal_norm_v3 transform）、`"kf"` float32 [B,30,6]（KF，predictor_norm_ext_v1 transform）；H1-only 模型只收 `"hist"`。
- 通道顺序 = 冻结 registry/npz 列序（input_contract 逐字段引用）；batch 语义 = 窗口感知训练（window-uniform，FR-599）。
- 禁止 METADATA/scenario 字段进输入；TFT static 输入空集。

### DGN-E-06 统一 quantile 头与 anti-crossing（FR-584；obligation 1）

**决策：统一单调参数化**（不采用 crossing penalty）。接口分两段冻结——**架构专属 raw 投影**（每架构自己产出 raw `[B,30,3]=(m,a,b)`）+ **共用变换模块 `MonotonicQuantileTransform`**（全体五候选同一实现，模块内不得再含 Dense(3) 等二次投影）：

```text
（架构专属投影）→ raw_k = (m_k, a_k, b_k)      # 每个 lead bin k=1..30
（共用 MonotonicQuantileTransform）
q50_k = m_k
q10_k = m_k − softplus(a_k)
q90_k = m_k + softplus(b_k)                   # softplus(t)=log(1+e^t)
⇒ q10 ≤ q50 ≤ q90 结构保证（softplus ≥ 0）
投影形态：MLP-Q/TCN-Q = Dense(90) → reshape[30,3]；Transformer/TFT/REAT-T = per-lead Dense(3)
```

- 输出张量 `[B,30,3]`，末维顺序 `[q10,q50,q90]`（FR-589 输出契约）；
- **Loss = kW 域 pinball**：`L = (1/90)·Σ_k Σ_τ ρ_τ(y_k − q_τ,k)`，`ρ_τ(u)=max(τu,(τ−1)u)`，30 bin × 3 τ 等权（Q2-② v1 默认）；target 不做归一化（保持 bin 间等权语义；输入侧归一化照常）；
- crossing rate 仍逐窗审计（`mean(q10>q50+0 or q50>q90+0)`，容差 0），结构上应恒 0，机器可读记录；
- 数值细节：softplus 用 `tf.nn.softplus`；`a,b` 的 bias 初始化用**数值稳定 softplus 逆**：`softplus_inv(s) = s（s>20）否则 log(expm1(s))`——**禁止 naive `log(exp(s0)−1)`**（大 s0 溢出）；s₀ = train 全局 y_std（来源=eval_config.json 记录值），s0 来源与数值写入 model_config。

### DGN-E-07 五候选架构规范（FR-583/585/586/587/588；obligation 3/4 的架构部分）

全部候选：输入 per DGN-E-05；输出 = 架构专属 raw 投影产出 [B,30,3]=(m,a,b)，再经共用 `MonotonicQuantileTransform`（DGN-E-06），float32。参数量以 T-013 smoke 的 `model.count_params()` 机器证据为准，下表为设计估计。

**K/F 合法可见域（R3 冻结）**：全部 30 个 future bin 的 K/F 于 issue 时刻 t₀ 合法可得（非 realized future target）；任何 H1+K+F 候选不得通过 target 式因果掩码向早 lead 隐藏晚 lead 的合法 K/F——Transformer future self-attention 非因果、TFT 未来注意力无隐藏合法 K/F 的掩码、REAT-T future encoder 保持 30 位全可见；padding mask 允许；future realized target/ORACLE/U 信息仍禁止。此约定保持 `TFT vs Transformer vs REAT-T` 的共同有效信息面。

**(a) MLP-Q-v3**（H1-only；≈0.2M 近固定）：`hist[B,300,11] → Flatten(3300) → Dense(64,ReLU) → Dense(32,ReLU) → Dense(90) → reshape[30,3]=(m,a,b) → MonotonicQuantileTransform`（共用变换，DGN-E-06）。参数 ≈ 3300·64+64·32+32·90 ≈ 216K。

**(b) TCN-Q**（H1-only；≤1.5M）：`hist → 输入投影 Conv1D(C,1) → 6 残差块{dilated Conv1D(C,k=3,d) + LayerNorm + GELU + Dropout + Add}，d∈(1,2,4,8,16,32) → 末时刻切片 ⊕ 注意力池化 concat → Dense(256,GELU) → Dropout → Dense(90) → reshape[30,3]=(m,a,b) → MonotonicQuantileTransform`。默认 C=64（≈224K）；搜索空间见 DGN-E-08。

**(c) Transformer baseline**（H1+K+F；≤1.5M）：
```text
hist → Linear(11→d) + learned pos[300] → PreLN Encoder×N_e（MHA h头 + FFN 2d）→ past_seq[B,300,d]
kf   → Linear(6→d) + learned lead-pos[30] → fut[B,30,d]
fut  → PreLN Decoder×N_d（self-MHA **非因果**：30 位合法 K/F 全互见 + cross-MHA(past_seq) + FFN）→ [B,30,d] → per-lead Dense(3)=(m,a,b) → MonotonicQuantileTransform
```
默认 d=128, h=4, N_e=2, N_d=2（≈0.9M）。

**(d) TFT**（H1+K+F，无 static；≤1.5M；Lim et al. 2021 的 static-free 适配）：
```text
VSN_past：11 个 per-channel GRN 嵌入（每步）→ softmax 变量权重 → 加权和 [B,300,d]
VSN_future：6 通道同理 [B,30,d]
LSTM encoder（past 300 步）/ decoder（future 30 步，喂 VSN_future）→ 序列 [B,330,d]
Gated skip（GLU）→ Interpretable multi-head attention（h=4；未来位间**不加因果掩码**——30 位 K/F 合法全可见；padding mask 允许）→ GLU+Add&Norm
→ position-wise GRN → 取未来 30 位 → per-lead Dense(3)=(m,a,b) → MonotonicQuantileTransform
```
默认 d=64、LSTM 1 层（≈0.7M）。static enrichment 模块整体省略（FR-579）。

**(e) REAT-T**（H1+K+F；0.5~3.0M；T-440 design-first 本节冻结）：
```text
Past branch（TCN）：hist → Conv1D(Cp,1) 投影 → 6 残差块（k=3，d∈1,2,4,8,16,32；LayerNorm+GELU+Dropout+Add）
   → past_seq[B,300,Cp] → 注意力池化（learned query）→ h_past[B,Cp]
Future branch（Transformer）：kf[ B,30,6] → Linear(6→df) + learned lead-pos[30,df]
   → PreLN Transformer Encoder×Nf（MHA h头 + FFN 4df）→ fut_seq[B,30,df]
Fusion：h_past broadcast 到 30 位 ⊕ fut_seq concat → [B,30,Cp+df]
   → LayerNorm → Dense(df,GELU) → Dropout → fused[B,30,df]
Decoder（per-lead 共享）：fused → Dense(d_dec,GELU) → Dropout → Dense(3)=(m,a,b) → MonotonicQuantileTransform → [B,30,3]
```
锚点配置（≤1.5M 义务锚点）：Cp=96, df=128, Nf=3, h=4, d_dec=128 → 估计 ≈0.78M（TCN ≈333K + future ≈394K + fusion/decoder ≈45K）。默认搜索空间内最大配置 ≤3.0M（DGN-E-08 强制 envelope 检查，超界 trial 自动无效并记录）。
消融臂形态（T-016，**架构保持**——R2 冻结）：全部消融臂与 REAT-T 候选共享**完全相同的拓扑**（同一 6 通道 KF 张量形状、Future Transformer branch、输入投影、lead 位置编码、融合、decoder、参数量/拓扑），信息面差异用**归一化零通道替换**实现：`REAT-T(H1)` = 6 个 KF 通道全替换为归一化零；`REAT-T(H1+K)` = 4 个 K 通道真实值 + 2 个 F 通道归一化零；`REAT-T(H1+K+F)` = 全真实值（复用正式 REAT-T 候选运行，不重复训练）。非全信息臂用冻结的选中 REAT-T 超参独立训练（同一 `{0,1,2}` 种子政策）；消融 config/input contract 记录 active-information-channel mask；**不得仅为本实验新增运行时 mask 张量**。归一化零 = transform 后 0 值（即 train 均值语义），不产生新归一化事实。

**(f) 复杂度证据**：`complexity_report.json` per 候选：count_params、MACs（30-bin 全输出单次前向，tf.profiler 或等效）、序列化 FP32 字节、训练墙钟、峰值内存（可得时）。

### DGN-E-08 HPO 引擎与搜索空间（FR-595；obligation 3）

- **试验清单生成算法（R5 冻结七步）**：① 枚举离散笛卡尔配置全集；② canonicalize 配置；③ **model-build/count_params 容量过滤**（先于任何 result-bearing 训练：TCN-Q/TFT/Transformer ≤1.5M；REAT-T ∈[0.5M,3.0M]）；④ 被拒配置记入 `trial_generation_audit`；⑤ 注入强制锚点配置（REAT-T 必含锚点 Cp=96/df=128/Nf=3/d_dec=128；过滤后全集必须仍含 ≥1 个 ≤1.5M 有效配置）；⑥ 用派生 HPO 种子 `SHA256(seed_root‖"hpo"‖arch_id)` 对剩余有效配置**均匀无放回抽样**至预算数；⑦ HPO 启动前冻结最终 trial list JSON（每 trial = 完整 config + config 自哈希 + trial_id），落 `results/predictor_v3/hpo/<arch>_trial_list.json`。**只有最终清单上的配置才是正式试验**；预算用尽即止。
- 主目标：val split_group_macro pinball（DGN-E-11 口径）；EarlyStopping/最佳权重恢复同口径。
- 搜索空间（冻结边界）：

| 架构 | 空间（离散/范围） |
|---|---|
| MLP-Q-v3（≤5） | lr∈{5e-4,1e-3}，batch∈{256,512}（hidden 固定 64/32；网格 4 组全跑） |
| TCN-Q（≤20） | C∈{32,64,96}，dilations∈{(1..32)6块,(1..64)7块}，dropout∈{0,0.1}，lr∈{3e-4,1e-3}，batch=256 |
| Transformer（≤20） | d∈{64,128}，N_e∈{2,3}，N_d∈{1,2}，h=4，FFN=2d，dropout∈{0,0.1}，lr∈{1e-4,3e-4,1e-3}，batch=128 |
| TFT（≤20） | d∈{64,128}，LSTM层∈{1,2}，h=4，dropout∈{0,0.1}，lr∈{1e-4,3e-4,1e-3}，batch=128 |
| REAT-T（≤20） | Cp∈{64,96,128}，df∈{64,128}，Nf∈{2,3,4}，d_dec∈{64,128}，dropout∈{0,0.1}，lr∈{1e-4,3e-4,1e-3}，batch=128；**强制含锚点配置（Cp=96,df=128,Nf=3,d_dec=128）；容量过滤后全集必须仍含 ≥1 个 ≤1.5M 有效配置** |

- 架构专属 lr/batch 例外 = 上表即全部例外（除此之外无自由改动）；优化器/schedule/ES 共同治理见 DGN-E-09。
- trial registry：每次运行追加 `hpo/<arch>_trials.jsonl`（trial_id/config_hash/seed_index=0/val 指标/时长/资源/状态含 FAILED）；禁删。
- **预算记账（R5 冻结）**：已开始 result-bearing 训练的正式 trial 即使以 OOM/NaN/Inf/训练失败收场也消耗一个预算位；纯基础设施中断可用同一 `trial_id` 重跑 + incident 记录，不产生新配置机会。

### DGN-E-09 训练预算与优化治理（FR-597；obligation 5）

冻结数值：max_epochs=100；EarlyStopping(monitor=val split_group_macro pinball，patience=10，restore_best_weights=True)；ReduceLROnPlateau(factor=0.5，patience=5，min_lr=1e-5)；optimizer=Adam；gradient clip global_norm=1.0；lr/batch 一律由 DGN-E-08 冻结空间给出（无空间外默认值）；数据管道：`tf.data.from_tensor_slices` → shuffle(buffer=50000，per-run 派生种子) → batch → prefetch；full float32（不开 mixed precision）；训练目标 kW 域 pinball（DGN-E-06）。

### DGN-E-10 种子派生与确定性（FR-593/594）

- 派生规则（冻结）：`payload = f"{seed_root}|{model_id}|{config_hash}|{seed_index}".encode()`；`digest = hashlib.sha256(payload).digest()`；`seed_int = int.from_bytes(digest[:8], "big") % (2**31)`；model_id ∈ {mlpq, tcnq, tft, transformer, reatt}（消融臂 `reatt_abl_<face>`）。
- 固定点：冻结进程级 PYTHONHASHSEED + Keras/TensorFlow 确定性种子 API（`keras.utils.set_random_seed` / `tf.random.set_seed`）、python `random`、numpy `default_rng`、数据 shuffle 种子、Dropout 等随机层随全局；`tf.config.experimental.enable_op_determinism()` 在支持处启用。
- 探针协议：CPU 探针（本地）= 固定小配置两独立进程双跑 → 比较预测/val 指标/权重 SHA256，bitwise 或数值容差如实落盘；CUDA 探针（canonical 环境）同法双跑。产物 `results/predictor_v3/determinism_probe_{cpu,cuda}.json`。

### DGN-E-11 指标栈实现（FR-598/599）

- 逐窗指标：pinball（30×3 均值）、MAE(q50)（30 bin 均值）、RMSE(q50)（30 bin 均方根）、PICP80（q10≤y≤q90 命中率）、MPIW（q90−q10 均值）、crossing rate；分段（10–60/70–180/190–300s，即 bins 1–6/7–18/19–30）同指标子集。
- 聚合层级（R7 冻结——**禁止 mean-of-RMSE、禁止平均 R²**）：voyage 层先对全部合格原子观测聚合原语（pinball=原子均值；MAE=窗×lead 绝对误差均值；RMSE=sqrt(窗×lead 平方误差均值)；PICP=命中数/合格总数；MPIW=区间宽均值；E300-MAE=窗级 |E300 误差| 均值；E300-RMSE=sqrt(窗级 E300 误差平方均值)），再 `voyage → split_group 均值 → split_group_macro`（主推断单位）；三视图：`split_group_macro`（组均值再均）、`voyage_macro`、`window_pooled`。
- E300 派生：ŷ300 = Σ q50_k·(10/3600)；E300 MAE/RMSE 按上条层级规则；**R² 为派生 population/subset 统计**——对该视图全部合格 E300 目标/预测对一次计算，严禁平均 per-window/per-voyage R²；R² 区间 = split_group 重采样内重算 population R²；目标方差退化（<1e-12）记 `R2_UNDEFINED` 如实报告。
- 归一化报告指标：nMAE/nRMSE per-lead 分母 = frozen formal_norm_v3 `y_std[k]`；全局分母 = train 全局 y std（E 侧 `eval_config.json` 记录数值+来源 hash）；OOD 误差禁 OOD 分母。
- `coverage_error_80 = |PICP80−0.80|`；calibration 分段报告 = 分段 PICP/CE80/MPIW。

### DGN-E-12 统计推断（FR-599/602/604；obligation 2/9 的算法落点）

- **paired split_group bootstrap（val，同 split 内候选对比）**：B=10000；每次重采样 27 个 val split_group（有放回）；组统计 = 组内 voyage 均值再均值；CI=percentile 2.5/97.5%。**相对差 replicate 定义 `δ_rel* = (M_c* − M_b*)/M_b*`**——分母用重采样后的参照指标本身；分母数值非正/未定义 ⇒ 该比较 fail-safe 记 `UNDEFINED`（保守不判等价）。
- **two-sample cluster bootstrap（OOD vs IID-test，轴③）**：双侧独立重采样各自 split_group（IID-test 18 组 / OOD 条件 n_g 组）→ 各算冻结指标 → 比/差；B=10000；ratio 仅 pinball/MAE/RMSE（正值指标）；差值用于 ΔCE80/ΔPICP/ΔR²。
- **多种子聚合（obligation 9 冻结规则）**：先 `m̃(c,g)=median over seeds{0,1,2} of m(c,s,g)`（per-split_group seed median），再跨组聚合/配对 bootstrap；种子不是独立重复单元；禁 best-seed；全候选同规则。
- **等价带（obligation 2 冻结规则）**：候选 c vs 参照 b 的相对差 `(M_c−M_b)/M_b` 的 paired bootstrap CI ⊆ [−1%,+1%] ⇒ 实际等价；CI 由 DGN-E-12 paired 算法产出；实现位置 `inference_stats.py`，单测覆盖（构造已知差数据验证带内/带外判定）。

### DGN-E-13 Winner 选择算法（FR-604/605；obligation 2）

伪代码冻结（`winner.py` 实现须逐行对应）：

```text
输入：5 候选 × seeds{0,1,2} 的 val 逐 split_group 指标（pinball、MAE(q50)、CE80）+ 参数量
1. 每候选每指标按 DGN-E-12 聚合：M_c（split_group_macro，per-group seed-median）
2. S0 = 全候选；best = argmin M_c(pinball)
3. S1 = {c ∈ S0 : δ_rel(pinball; c vs best) 的 paired bootstrap CI ⊆ [−1%,+1%]}
   （δ_rel 定义见 DGN-E-12：分母为重采样参照指标；分母非正/未定义 ⇒ UNDEFINED 保守不判等价）
4. 若 |S1|==1 → winner = best，转 9
5. S2 = {c ∈ S1 : δ_rel(MAE(q50); c vs argmin_{S1} MAE) CI ⊆ [−1%,+1%]}
   若 |S2|==1 → winner = argmin_{S1} MAE(q50)，转 9
6. bestCE = argmin_{c∈S2} M_c(|CE80|)；
   S3 = {c ∈ S2 : CI(Δ_CE80) ⊆ [−0.01,+0.01]}，其中 Δ_CE80 = CE80_c − CE80_bestCE
   （**有符号 paired 差** 对带符号区间；禁止先取 abs 再算 CI）
7. 若 |S3|==1 → winner = S2 中 |CE80| 最小者，转 9
8. winner = S3 中 trainable 参数量最小者；若参数量仍并列 → canonical `model_id` 字典序
   （行政性平局打破，无科学解释，仅保证确定性 Winner 身份）
9. 写 WINNER_SELECTED_PRETERMINAL（winner model_id/config_hash/候选集/val 证据 hash/本算法版本 hash/时间戳/commit）
```

注意不变量：本算法**只消费 val 证据**；执行后 test/OOD 方可解封（访问门 campaign 登记以本证据 hash 为前置校验）。

### DGN-E-14 T-443 三轴诊断实现（FR-600~603；obligation 8）

- **W1 归一化（义务 8 冻结定义）**：1D Wasserstein-1 = `scipy.stats.wasserstein_distance`（精确经验分布）；归一化分母 = train 参照 std（历史通道：train 全窗全 bin 池化 per-channel std；KF：per-(channel,lead) 用 predictor_norm_ext_v1 的 scale[c,k]，池化摘要用 KF train 池化 std；target：E300/ȳ 用 train 池化 std）；**分母有效为零 ⇒ `UNDEFINED_CONSTANT_SUPPORT`** 并转常值支持诊断路径。子采样（**禁等距 stride**——数组序可能按 voyage/时间结构化）：N≤200000 全取；N>200000 用**确定性顺序无关 hash bottom-k**——稳定观测键 `dataset/set|voyage_id|anchor_t0|channel|lag_or_lead`，取确定性 hash 值最小的 200000 个观测；diagnostics_config.json 记录采样算法/版本、selected count、selected-key-set hash、参照集身份/hash。target 分布分位数冻结为 p01/p05/p50/p95/p99；top-shifting 通道排名 = top-5。
- **轴①**：per 条件 × per 通道（H1 11ch 池化 + per-bin 摘要；KF 6ch per-lead + 池化摘要）：p01/p99 落出率（train 分位冻结于 diagnostics_config）、p95/p99/max|z|、归一化 W1、top-5 shifting 通道排名；常值通道单列（FR-581）；对比对 = OOD-condition vs IID-test（主）+ OOD vs train（参照）；模型级解释过滤 = 各模型只展示其实际消费通道（H1-only 模型不挂 F 通道归因）。
- **轴②**：E300 主（显式注明 E300=ȳ/12）；mean/std/分位（p01/p05/p50/p95/p99）/落出率/per-lead mean·std/分段诊断/归一化 W1；同对比对。
- **轴③**：per 候选/臂 × per 条件：R_deg（pinball/MAE/RMSE）+ ΔCE80/ΔPICP/ΔR² + crossing rate；two-sample cluster bootstrap（DGN-E-12）；**正交字段**（R2 修正后语义）：`effect_band ∈ {SMALL|MODERATE|LARGE}`（R≤1.2 / ≤1.5 / >1.5）、`ci_lower_exceeds_1`、`ci_upper_within_20pct`、`substantial_degradation = (R_point>1.5 AND CI_lower>1.2)`；派生汇总标签（`MAINTAINED_WITHIN_20PCT` 若 ci_upper_within_20pct；`SUPPORTED_DEGRADATION` 若 ci_lower_exceeds_1；否则 `UNCERTAIN`——precedence 即此序，正交字段 canonical）；全部 OOD 条件自动附 `SMALL_N_CAUTION`（n_g<20）。
- 轴①②算一次（数据集级），轴③按模型级分别产出（FR-603）。

### DGN-E-15 环境工程（FR-591/592；Q6 平台分离）

- 本地 `.venv_tf`（仓库根，python3.12 -m venv）：`requirements_tf_base.txt`（手钉最小集：tensorflow==2.21.0〔smoke 落锁为准〕、numpy 2.x、scipy、pytest）；`requirements_tf_macos_lock.txt` = 本地 `pip freeze` 全量 + 关键包 sdist/wheel sha256 清单（显式 hash 文件，不谎称 freeze 含 hash）；macOS manifest JSON（字段 per FR-592）。
- 兼容 smoke（本地）：合成数据 1-epoch 小 MLP 前向+反向 + `tf.config.list_physical_devices()` 输出 + OOD loader 冒烟；证据 `results/predictor_v3/env_smoke_macos.json`。首选组合失败 → 降钉 TF 2.20.x 重 smoke（证据驱动）。
- AutoDL canonical：镜像选择→venv/lock 复建→`requirements_tf_cuda_lock.txt` + CUDA manifest（GPU 身份/driver/CUDA/cuDNN）→ CUDA 兼容 smoke + 可复现探针（DGN-E-10）→ 冻结。
- 传输协议：代码 = git commit 钉版（`git archive` 或 clone+checkout commit，禁现场改码；改动只能回本仓库走 git）；数据 = rsync 传输 npz/OOD npz/norm/config + hash 校验（传输后 sha256 对照）；结果 = rsync 回传 `results/predictor_v3/` 精选入库；每次 canonical 运行记录 commit + 环境 manifest hash + 数据 hash。
- 禁跨后端混跑 result-bearing 运行（FR-591）；canonical 版本变更 ⇒ 再资格化（重跑 CUDA 探针 + 一个 frozen config 复跑对拍）。

### DGN-E-16 机器可读工件 schema 集（obligation 7；FR-589/596/605~608）

统一约定：JSON；含 `schema` 版本串；`artifact_sha256` = canonical JSON（排除该字段）SHA256；时间戳 UTC ISO8601；路径一律仓库相对。

1. **model_config.json**：`{schema:"predictor_v3_model_config_v1", model_id, arch, config:{…架构/训练超参全量}, feature_surface:"H1"|"H1+K+F", param_count, config_sha256}`。
2. **trial_registry（jsonl 行）**：`{trial_id, arch, config_sha256, seed_index, status, val:{pinball_sgmacro, mae_q50_sgmacro, ce80_sgmacro}, duration_s, peak_mem_bytes?, env_manifest_sha256, git_commit, error?}`。
3. **selected_config.json**：`{schema, arch, search_space_sha256, trial_list_sha256, trial_registry_sha256, budget_cap, budget_consumed, selected_trial_id, config:{…}, config_sha256, seed0_val_evidence:{…}, selected_utc, git_commit, artifact_sha256}`。
4. **input_contract.json**：字段 = FR-589 全清单（identity/inputs/features/temporal/preprocessing/output），所有 hash 引用指向冻结工件；`contract_sha256`。
5. **WINNER_SELECTED_PRETERMINAL.json**：`{schema, winner:{model_id, config_sha256, artifact_dir}, candidates:[…], selection_rule:{version:"dgn-e-13-v1", algorithm_sha256}, val_evidence_sha256:[…], selected_utc, git_commit, artifact_sha256}`。
6. **terminal_campaign.json**：`{schema, campaign_id, intent:"TERMINAL_EVAL_T443", scope:{candidates:[…], ablation_arms:[…], seeds:[0,1,2], sets:["iid_test","ood"]}, preterminal_winner_artifact_sha256, winner_identity_sha256, candidate_artifact_hashes:[…], ablation_artifact_hashes:[…], eval_config_sha256, code_git_commit, val_rehearsal_evidence_sha256, registered_utc, authorized_utc, status, artifact_sha256}`；**生命周期冻结：`DRAFT → REGISTERED_LOCKED → UNSEAL_AUTHORIZED → RUNNING → COMPLETE`（FR-608 事故协议下允许 `INTERRUPTED`）**；访问门按 DGN-E-02 六项核验此件。
7. **terminal_incident_ledger.json**：`{schema, campaign_id, incidents:[{utc, phase, symptom, root_cause_class, action, rerun_allowed_by:"FR-608", verdict}], artifact_sha256}`（空数组合法）。
8. **metrics_result.json**（per 模型×seed×集合）：`{schema, model_id, seed_index, set:"val"|"test"|"ood_<cond>", views:{split_group_macro:{…全指标}, voyage_macro:{…}, window_pooled:{…}}, segments:{short|mid|long:{…}}, e300:{mae,rmse,r2}, per_group_values_ref, env_manifest_sha256, git_commit, artifact_sha256}`。
9. **ood_diagnostics.json**：`{schema, axis1:{per_condition:{…通道级}}, axis2:{…}, axis3:{per_model:{per_condition:{R_deg:{pinball:{point,ci},…}, delta_ce80…, effect_band, ci_lower_exceeds_1, ci_upper_within_20pct, substantial_degradation, summary_label, small_n_caution:true}}}, config_sha256, artifact_sha256}`。
10. **包 manifest（PRETERMINAL/FINAL）**：`{schema, package:"PRETERMINAL_WINNER_PACKAGE"|"FINAL_WINNER_EVIDENCE_PACKAGE", winner:{…}, members:[{path, sha256, role}], artifact_sha256}`；FINAL 追加 terminal 证据引用与 `winner_identity_unchanged:true`（机器校验比对 T-444a/T-444b 身份字段）。
11. **predictor_norm_ext_v1.json**：schema 见 DGN-E-04。

### DGN-E-17 Winner 包与 reload 探针（FR-590/611）

- PRETERMINAL_WINNER_PACKAGE 成员：`.keras`、model_config.json、input_contract.json、complexity_report.json、val 证据引用、WINNER_SELECTED_PRETERMINAL.json、reload_probe.json、包 manifest。
- reload 探针协议（冻结）：保存后 → canonical 环境新鲜进程 → `keras.models.load_model`（含自定义对象注册表）→ 探针输入 `probe_input.npz`（val 前 64 窗，身份=文件 sha256，**val 来源合法非 test/OOD**）→ 推理 → 对照保存前参考输出：shape 精确 [64,30,3]、有限性、quantile 顺序/语义、容差 `max|Δ| ≤ 1e-5`（预注册；bitwise 更佳如实记录）→ verdict PASS/FAIL；FAIL ⇒ T-444a 不得闭合。
- **canonical 交付种子（tasks gate R5 镜像澄清，不改任何已批准设计语义）**：`canonical_delivery_seed_index=0` 预注册于 eval_config.json；科学 Winner 身份 = model_id+config_hash；科学 Winner 证据 = 全部 {0,1,2} 种子按 DGN-E-12 冻结聚合规则；PRETERMINAL_WINNER_PACKAGE 中单一 canonical `.keras` = 固定 seed0 正式工件；reload 探针对该固定 seed0 工件执行；seed1/seed2 工件保持为引用科学证据；禁止按观测 val 表现选择种子。
- FINAL 封印（T-444b）：PRETERMINAL 全成员 + terminal 证据（metrics_result × 集合、ood_diagnostics、incident ledger）+ 最终报告 + 限制/公平性声明 + FINAL manifest；机器校验 `winner_identity(T-444a)==winner_identity(T-444b)`。

### DGN-E-18 T-444 措辞澄清落账（FR-609）

本 design 批准时同步更新：本 spec requirements/brainstorm 已含澄清语义（val-only 决定 + OOD 入证据包）；另在 `docs/phase4_research_roadmap.md` T-444 条目追加一句澄清注记（沿用裁决原文语义，不改路线结构）。

## Error Handling

- 访问门越权：`SealedAccessError`（fail-closed）；
- OOD 物化航次缺 realized 映射/窗口数为零：FAIL + 计数记录（禁静默修补）；
- K/F vintage 缺失窗口：invalid + 计数（继承 DGN-07 语义）；
- HPO trial 失败：状态 FAILED 留账继续；超信封配置记 invalid 不占预算；
- 训练 NaN/Inf：trial FAILED + 留账（禁静默换种子重试——同 config 重试须登记 incident）；
- 终评崩溃：FR-608 事故政策 + incident ledger；
- 环境 smoke 失败：证据驱动降钉（本地）/ 升级用户（canonical）。

## Security and Privacy

无外部数据/凭证；AutoDL 凭证属用户侧管理（不入库、不进脚本）；仿真数据表述纪律延续（simulation-generated）。

## Testing Strategy

- **单测（pytest，本地 .venv_tf，全绿为 E0/E1 前置）**：pinball 数值对拍参考实现；MonotonicQuantileTransform 单调性（随机 (m,a,b) 输入 q10≤q50≤q90 恒成立 + 模块内无二次投影断言）；指标栈对拍（手算小例）；三视图聚合正确性；paired/two-sample bootstrap 已知数据验证；种子派生确定性；访问门三负向形态；常值通道 OUT_OF_TRAIN_CONSTANT_SUPPORT 触发；norm_ext 禁 refit/禁 clip/transform-only；OOD 物化双跑 canonical payload hash 一致（文件 sha256 另报）+ schema 与 IID npz 平行断言；
- **shape smoke（非 result-bearing，不占 HPO）**：五候选合成小数据前向 shape + count_params 落 envelope（T-013 证据）；
- **确定性探针**：CPU（本地）+ CUDA（canonical）双跑证据；
- **评估器预演**：terminal evaluator 在 val 上完整跑通（`rehearsed_on_val`）后才允许 campaign 登记；
- **机器校验**：winner_identity T-444a==T-444b；访问日志审计（无 test/OOD 在 campaign 登记前被载）。

## Rollout

E0（T-001~T-007）→ E1（T-008~T-011）→ E2（T-012 设计冻结 → T-013 smoke → T-014 HPO）→ E3（T-015/T-016）→ T-444a（T-017）→ T-443（T-018）→ T-444b（T-019）。门禁：E0 不齐不进 E1；REAT-T 设计未 freeze 不进 T-014；CONFIG_FREEZE 不回开；WINNER PRETERMINAL 未哈希不解封。

## Requirements Traceability

| Requirement | Design | Verification |
|---|---|---|
| FR-570~573（范围/阶段） | Architecture + DGN-E-01 + Rollout | spec 状态纪律 + AC-450~455 |
| FR-574/578（几何/target/runtime） | DGN-E-05/06/11 | shape smoke + 指标单测 |
| FR-575（零字节/hash 链） | DGN-E-01/02/03 | 开工核验记录 + integrity JSON |
| FR-576/577（OOD 物化/realized） | DGN-E-03 | 双跑确定性 + per-condition 计数 + 负向缺映射 FAIL |
| FR-579（特征面/架构声明边界） | DGN-E-05/07 | features 单测 + 报告措辞审查 |
| FR-580~582（norm ext/常值/H2 禁令） | DGN-E-04 | pytest（fit-only-train/transform-only/常值诊断） |
| FR-583~588（候选/输出/容量/复杂度） | DGN-E-06/07 | shape smoke + envelope 断言 + complexity_report |
| FR-589/590（input_contract/reload） | DGN-E-16(4)/17 | contract 校验器 + reload 探针 PASS |
| FR-591/592（平台/环境工件） | DGN-E-15 | 三 lockfile + 双 manifest + smoke 证据 |
| FR-593/594（种子/确定性） | DGN-E-10 | 派生单测 + 双探针证据 |
| FR-595~597（HPO/config freeze/优化治理） | DGN-E-08/09/16(2,3) | trial registry 审计 + 预算计数 + 单向门检查 |
| FR-598/599（指标/统计层级） | DGN-E-11/12 | 指标单测 + bootstrap 单测 |
| FR-600~603（T-443 三轴） | DGN-E-14 | ood_diagnostics schema 校验 + 小样本旗标断言 |
| FR-604/605（val-only 选择/preterminal） | DGN-E-13/16(5) | winner 算法单测 + 时间戳先于解封的机器校验 |
| FR-606~608（访问门/终评/事故） | DGN-E-02/16(6,7) + terminal.py | 访问门负向 + ledger schema + campaign 单例 |
| FR-609（措辞澄清） | DGN-E-18 | roadmap 注记 diff |
| FR-610/611（报告/FINAL 包） | DGN-E-17 | FINAL manifest + 报告齐套核对 |
| NFR-440~446 | DGN-E-01/02/10/15/16 | 路径扫描 + README 同步 + 访问日志审计 |
| AC-450~455 | Rollout 门禁 + 各 DGN 验证列 | 逐 AC 证据齐套（见 tasks 映射） |

## Risks and Trade-offs

- **风险**：TFT static-free 适配与官方实现有差异 → 缓解：组件级 shape/单测 + 设计内明示"适配版"，报告如实表述；
- **风险**：AutoDL 环境不可用/漂移阻塞正式训练 → 缓解：T-007 独立任务 + 用户外部依赖显式化 + 再资格化协议；
- **数值保护（保留项）**：grad clip global_norm=1.0 作为常规数值保护保留；注意 pinball 的局部输出梯度分片有界（|∂L/∂q|≤max(τ,1−τ)），不得声称 raw-kW pinball 仅因误差数值大就必然产生大输出梯度；lr 空间下限与 ES 如常治理；
- **权衡**：统一单调参数化使 q50 不再是无约束中位数的"纯"参数化（m 同时服务于三个 τ 的 pinball）——换来结构零 crossing 与全候选统一；crossing rate 仍审计；
- **权衡**：window-uniform 训练保留长航次加权（declared limitation，Q2-⑤），不引入 voyage 权重。

## 偏差登记

| 日期 | 偏差 | 来源/理由 | 处置 |
|---|---|---|---|
| 2026-09-13 | DGV-E-01（T-003 实施期）：OOD npz 在 DGN-E-03 窗口标签清单（voyage_id / anchor_t0_s / window_index_in_voyage / ood_condition）之外**附加 `split_group_id` 元数据列**。 | FR-577 per-condition split_group 计数（A1=36v/9g…）与 FR-602/DGN-E-12 two-sample cluster bootstrap（重采样单位 = split_group）的必需元数据；不写 splits 列的禁令不受影响（split_group 是聚类身份，非 IID train/val/test 分割语义）；非科学分布值，不泄露窗口内容。 | 已实施（`predictor_v3/ood_materialize.py`；integrity JSON 的 per_condition.split_groups 计数消费该列）；integrity/scientific payload hash 已覆盖该列。 |

（施工期偏差逐项登记于此；实质架构/设计语义变更不在此列，须升级用户。）

## 修订记录

| 日期 | 修订 | 来源 |
|---|---|---|
| 2026-09-13 | design 草案（DGN-E-01~E-18；9 项 bounded obligations 冻结解） | 主 Agent 起草 |
| 2026-09-13 | **design 批准（APPROVED WITH 9 REQUIRED CORRECTIONS，R1~R9 闭合）**：R1 OOD 派生物移出冻结命名空间（`artifacts/predictor_v3/`）+ 双 hash（文件 sha256 + canonical scientific payload sha256，确定性以 payload hash 为门）；R2 消融臂改架构保持的归一化零通道替换（同拓扑同参数量，禁运行时 mask 张量）；R3 K/F 30 位全合法可见（禁 target 式因果掩码隐藏合法 K/F，三候选统一）；R4 quantile 头拆分为架构专属 raw 投影 + 共用 `MonotonicQuantileTransform`（禁二次投影，稳定 softplus 逆）；R5 HPO lr/batch 修订 + 七步试验清单生成算法 + 容量过滤前置 + 预算记账；R6 winner 算法确定性化（CE80 有符号差、分母重采样、UNDEFINED fail-safe、参数量并列走 model_id 字典序行政平局）；R7 层级聚合修正（禁 mean-of-RMSE/平均 R²，R² 为 population 派生统计 + 组重采样内重算）；R8 W1 子采样改确定性顺序无关 hash bottom-k + UNDEFINED_CONSTANT_SUPPORT + 分位 p01/05/50/95/99 + top-5；R9 终评解封六项核验 + campaign 生命周期五态 + 访问日志 per-run 分片；附：Risks 梯度措辞修正 + 种子派生公式冻结 | 用户 design gate Final Ruling |
| 2026-09-13 | 非实质澄清（tasks gate R5 授权镜像）：DGN-E-17 增补 canonical 交付种子语义（`canonical_delivery_seed_index=0` 固定交付工件；科学 Winner 身份/证据语义不变；Design Gate 不重开） | 用户 tasks gate Final Ruling R5 |

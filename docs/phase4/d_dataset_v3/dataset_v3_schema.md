# Dataset v3 Schema（dataset_v3_schema.md）

> Spec: `.specs/phase4-d-dataset-v3`（T-004 + T-010 target 节；FR-464/466/469~480/488/474；design DGN-01/02/04/05/07/08/12）
> **机器权威**：`config/dataset_v3_signal_registry.json`（role/聚合枚举）、
> `config/dataset_v3_producer_schema.json`（C 侧字段）。本文件是解释层。

## 1. Voyage artifact（每航次一个，DGN-04）

| 表 | 索引 | 内容 |
|---|---|---|
| `stream` | `time_s` 单轴（native 0.01 s） | truth + H1/H2 + ORACLE + METADATA 列（registry `temporal_domain ∈ {history, none, both}` 且非 fixture 源的条目；scenario 常量导出为常量列） |
| `kf_snapshots` | `(issue_time_s, lead_s)` 双索引 long-table | INPUT_K/INPUT_F 列（registry `issue_time_semantics=issue_snapshot` 条目），lead ∈ {10,…,300} |
| `meta` | 标量/字符串 | voyage_id、scenario/template 标识、generation_seed、family/group 字段（D-D7 预留，pilot 期填 voyage 自身）、模型/基线版本、provenance hash、fixture_kind |

**schema 级区分**（FR-466）：truth 按 realization time 单索引、forecast 按 (issue, lead) 双索引；
禁止从最终 future series 反推 vintage。每航次附 manifest 条目；导出缺列/缺 snapshot ⇒
该航次标 failed 并记录原因，pipeline 继续其余航次（禁静默补数，design Error Handling）。

## 2. 八类 storage role（FR-464，registry 枚举）

`INPUT_H1`（默认特征）/ `INPUT_H2`（extended-state）/ `INPUT_K`（计划/海图，issue_snapshot）/
`INPUT_F`（预报，issue_snapshot）/ `TARGET`（标签）/ `ORACLE`（真值对应物与原始双通道，
仅显式 Oracle 实验）/ `METADATA`（provenance/审计，predictor 禁入）/ `FORBIDDEN`（永不可解锁）。

- H1/H2/H3 观测分类法与 role 的映射规则：见 `observation_contract.md` §1.3（H3→FORBIDDEN/METADATA，物理保留、view 封锁）；
- role → predictor_access 绑定冻结于 registry `role_predictor_access_binding`；
- Builder **按 manifest 白名单构造 model view**；FORBIDDEN/H3 入 view 必须被拒（负向测试）；
  FORBIDDEN 永不可解锁；ORACLE 仅显式 Oracle 实验可用。

## 3. 窗口化几何（FR-469~473，DGN-07）

- 历史：300 个 1 s bin `H_j=(t₀−j, t₀−j+1]`（j=300..1），channel-aware causal aggregation
  （规则=registry `aggregation`，禁 raw decimation）；
- 未来：30 个 10 s bin `F_k=(t₀+(k−1)·10, t₀+k·10]`（k=1..30），与 K/F、target 网格严格同源；
- anchor：stride 10 s，t₀ ∈ {t_start+300, …, ≤ t_end−300}；航次内完整窗，禁 padding/跨 voyage；
- 每样本强制 METADATA：`voyage_id` / `anchor_t0_s` / `window_index_in_voyage`；
- X_KF 取 `issue_time == t₀`（±1e-9）快照行，缺失 ⇒ 窗口 invalid 计数（阈值 0 ⇒ FAIL）；
- <600 s 航次合法产生零训练窗口（仅 smoke/audit 用途）。

## 4. Target 语义与量化契约（FR-476~480，T-010 落账）

- **主 TARGET**：`y_k = (1/10)·∫_{F_k} P_load_total(t) dt` [kW]，shape [30]；由 native-rate
  realization truth 的累计梯形能量差构建（E_k=E(t_k)−E(t_{k−1})，内部 kW·s；y_k=E_k/10）；
  **禁从 1 Hz 降采样流二次计算**（targets.py 只消费 native stream，代码层隔离）；
  相邻 bin 无重复/遗漏，30 bin 能量严格可加（ΣE_k ≡ E(t₀+300)−E(t₀)，<1e-12 相对）；
  与 time_mean 聚合路径同定义等价（交叉验证 <1e-9）。
- **E_300 = ΣE_k/3600 [kWh]**：mandatory derived scalar target / evaluation metric /
  legacy scalar view；默认**不要求**独立 output head 或 multi-task loss（如需，E 包显式 ablation）。
- **Predictive quantiles**（τ ∈ {0.10, 0.50, 0.90}）：由模型经 pinball loss 从 realized target
  **学习**；**数据集不物化 target_q10/q50/q90 标签**（builder 提供守卫断言）；输出语义不变量
  q10 ≤ q50 ≤ q90（防 crossing 实现属 E 包）。q90 是未来 10 s **平均**负荷的分位数，
  不是瞬时峰值，不承担 sub-10s 保护（SOP/欠压链承担）。
- **Bin realized quantiles**（如需）只能以 `P_bin_realized_q10/q50/q90` 命名存 ORACLE
  diagnostic，不得作为训练 target；v1 不物化。

## 5. Dataset views（FR-480，公平比较机制）

| view | 内容 | 用途 |
|---|---|---|
| H1-view | registry INPUT_H1 白名单列的 X_H[300×11] + X_KF[30×6] | 默认特征集 |
| H2-view | INPUT_H1+INPUT_H2（21 列） | extended-state 实验 |
| trajectory target view | Y[30]（P̄_1..P̄_30） | 主目标 |
| legacy scalar view | E_300 | 旧模型/评价兼容锚点 |

模型公平比较**只经 view 实现**，不改 canonical target 定义；旧模型 legacy view 接口纪律
属 T-436（E 包）。归一化 train-only（FR-483），禁 clip 回 train min/max（预定义物理
saturation 除外且须入 feature contract/manifest）。

## 6. Manifest（FR-488 十五项 + DGN-12，deterministic payload hash）

`results/dataset_v3/dataset_v3_manifest.json` 必备：split policy/version、split seed、
split group key、assignment artifact/hash、train/val/test voyage counts、normalization
source=train_only、preprocessing artifact hash、OOD definition version、OOD generation seed、
provenance duplicate count、exact duplicate count、SBD threshold policy、SBD suspect
count+disposition、`test_sealed=true`、`ood_sealed=true`；另含 `dataset_anchor_stride_s=10`
与 `predictor_runtime_period_s="UNDEFINED_IN_D"` **分列**（stride 不得解释为部署推理周期）、
窗口几何五元组、registry/producer schema hash、aggregation 表版本、脚本 SHA、版本。

Hash 纪律（design R8）：`scientific_payload_sha256`（确定性科学内容）与 `run_metadata`
（build_timestamp/wall/主机）**分离**——同输入两跑 payload hash 必须一致。
sealed 纪律（FR-487）：Test/OOD 不参与训练/早停/超参/特征选择；evaluate→modify→evaluate
即降级 development evidence；E 包训练工具默认不加载 Test/OOD。

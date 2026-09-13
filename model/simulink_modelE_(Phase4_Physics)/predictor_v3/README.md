# predictor_v3/ — Phase 4-E 预测器 v3 Python 包

> **入库策略**: 入库（代码与单测）；训练中间产物不入库（`experiments/predictor_v3/`）、
> E 派生大数据不入库（`artifacts/predictor_v3/`）、机器可读证据精选入
> `results/predictor_v3/`。

## 用途

Phase 4-E NN 负荷预测器训练/评估主包（spec `.specs/phase4-e-predictor-v3/`，
T-436~T-444；design DGN-E-01~18）。在冻结 Dataset v3（`datasets_v3/` /
`results/dataset_v3/` 零字节改动只读消费）上训练五候选（MLP-Q-v3 / TCN-Q / TFT /
Transformer / REAT-T）+ 信息消融 + 三轴 OOD 诊断 + val-only Winner Freeze。
单向证据流：HPO→CONFIG_FREEZE→3-SEED CONFIRM→VAL-ONLY WINNER→PRETERMINAL
FREEZE→TEST/OOD UNSEAL→FINAL（禁回流）。

**预测模型**：施工作业中——截至 T-005 无已训练模型工件；模型实现 E1/E2（T-008~T-014）。

## 目录结构

| 条目 | 说明 |
|---|---|
| `data_access.py` | ★ T-002 sealed 数据访问层与访问门（DGN-E-02）：`load_split("train"\|"val")` 默认入口 fail-closed；`load_terminal("test"\|"ood", campaign_id, ood=...)` 六项核验（自哈希/PRETERMINAL 工件/Winner 身份/时间戳序/代码+eval config/状态机）全过才返回 DatasetView；`DatasetView` 懒加载 raw 窗口（IID split 过滤 / OOD ood_condition，无 transform）；per-run 访问日志分片 `results/predictor_v3/data_access/<campaign>/<run>.jsonl`；campaign 生命周期五态 + INTERRUPTED fail-closed |
| `ood_materialize.py` | ★ T-003 OOD 窗口物化薄驱动（DGN-E-03，SEALED_PREPROCESS_ONLY）：只读调用冻结 dataset_v3 管线（load_voyage→aggregate→build_windows→targets→build_views 白名单）；realized 映射缺失即 FAIL；不写 splits 列；写 `artifacts/predictor_v3/data/ood_windows_v3_formal.npz`（E-owned 本地）+ `results/predictor_v3/ood_windows_integrity.json`（双 hash + 双跑确定性门）；只产 schema/计数/hash/元数据 |
| `norm_ext.py` | ★ T-004 predictor_norm_ext_v1 拟合/加载/transform（DGN-E-04）：K/F 6 通道 per-(channel,lead-bin) train-only window-uniform 拟合（std<1e-12→scale=1）；transform-only 禁 clip/refit；`OUT_OF_TRAIN_CONSTANT_SUPPORT` 诊断钩子（H1 常值通道 ctrl_mode/manual_power_cmd_pct 声明）；工件 `config/predictor_v3/predictor_norm_ext_v1.json`（自哈希 `18824e08…`，136052 train 窗拟合） |
| `features.py` | ★ T-005 特征面装配（DGN-E-05）：registry 冻结通道序（INPUT_H1 11 / INPUT_K+F 6）；"hist" [B,300,11]（formal_norm_v3 transform）+"kf" [B,30,6]（norm_ext transform）；registry 外通道（METADATA/scenario）拒绝 |
| `quantile.py` | ★ T-005 统一 quantile 头（DGN-E-06）：共用 `MonotonicQuantileTransform`（q50=m、q10=m−softplus(a)、q90=m+softplus(b)；无二次投影）；kW 域 pinball（30×3 等权）；稳定 softplus 逆 + bias 初始化工具（s0=train y_std）；crossing rate 审计 |
| `metrics.py` | ★ T-005 Q2 指标栈 + 三视图聚合（DGN-E-11）：原子/原语载荷（R7 禁 mean-of-RMSE/平均 R²）；split_group_macro（主）/voyage_macro/window_pooled；分段 bins 1-6/7-18/19-30；E300 派生 + population R²（R2_UNDEFINED fail-safe）；nMAE/nRMSE train 分母；PICP80/CE80/MPIW |
| `inference_stats.py` | ★ T-005 bootstrap 推断（DGN-E-12）：paired split_group bootstrap（δ_rel 分母重采样；UNDEFINED fail-safe）；two-sample cluster bootstrap（ratio/diff）；per-group seed median 多种子聚合（禁 best-seed）；等价带 ±1%；轴③正交字段（effect_band/ci_*/substantial_degradation + 派生标签 precedence） |
| `seeds.py` | ★ T-005 种子派生（DGN-E-10）：`SHA256(seed_root\|model_id\|config_hash\|seed_index)[:8] % 2^31`；HPO 种子 `SHA256(seed_root\|hpo\|arch)`；全局 RNG 固定点（PYTHONHASHSEED 记录/keras/np/random/determinism） |
| `train.py` | ★ T-005 训练治理（DGN-E-09）：Adam+global_clipnorm=1.0；EarlyStopping/ReduceLR 监控 `val_sgmacro_pinball`（与主 HPO 目标同口径）；tf.data shuffle(50000,per-run seed)→batch→prefetch；float32 全精度；模型由 E1/E2 注入 |
| `registry.py` | ★ T-005 config 自哈希 + trial registry：append-only jsonl（DGN-E-16(2) schema）；预算记账（COMPLETED_ELIGIBLE/FAILED 耗位；INFRA_INTERRUPTED 不耗）；eligible 过滤 |
| `env_tools.py` | ★ T-006 确定性探针（DGN-E-10）：固定小配置（合成数据+小 MLP，非正式候选）两独立 CPU 进程同种子双跑（deterministic ops + GPU 显式隐藏）→ 比较预测/指标/权重 hash；CPU 探针 PASS_BITWISE；CUDA 探针（T-007 canonical）复用同一 `--once` 协议 |
| `tests/` | pytest 单测（本地 `.venv_tf` 全绿为 E0 前置；T-005 共 45 例） |
| `tests/test_data_access.py` | T-002 单测 14 例：负向三形态 + 六项核验正反 + 生命周期 + 日志分片 + DatasetView 切片 |
| `tests/test_ood_materialize.py` | T-003 单测 4 例：合成航次双跑 payload hash 一致 + schema 平行 + 缺映射 FAIL + integrity 白名单字段 |
| `tests/test_norm_ext.py` | T-004 单测 6 例：fit 只用 train 构造验证 / transform-only 禁 clip / 禁 refit（篡改检测）/ 常值诊断 / scale=1 保护 / 自哈希复算 |
| `tests/test_quantile.py` | pinball 对拍 / 单调性 + 无二次投影 / softplus 逆稳定 / crossing 审计 |
| `tests/test_metrics.py` | 指标手算对拍 / 三视图逐层手算 / R7 原语 / 分段 / E300+R² / full_report |
| `tests/test_inference_stats.py` | 带内带外判定 / 分母重采样复算 / UNDEFINED fail-safe / two-sample / seed median / 正交字段 |
| `tests/test_seeds_features.py` | 种子派生手算对拍 + 特征装配 registry 序/拒绝/transform 数值对拍 |
| `tests/test_train_smoke.py` | budget 加载 + 合成小模型 2 epoch 监控链 smoke（非 result-bearing） |

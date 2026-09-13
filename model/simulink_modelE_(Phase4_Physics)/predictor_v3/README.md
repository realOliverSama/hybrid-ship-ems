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

**预测模型**：施工作业中——截至 T-002 无已训练模型工件；模型实现 E1/E2（T-008~T-014）。

## 目录结构

| 条目 | 说明 |
|---|---|
| `data_access.py` | ★ T-002 sealed 数据访问层与访问门（DGN-E-02）：`load_split("train"\|"val")` 默认入口 fail-closed；`load_terminal("test"\|"ood", campaign_id, ood=...)` 六项核验（自哈希/PRETERMINAL 工件/Winner 身份/时间戳序/代码+eval config/状态机）全过才返回 DatasetView；`DatasetView` 懒加载 raw 窗口（无 transform——T-004/T-005 职责）；per-run 访问日志分片 `results/predictor_v3/data_access/<campaign>/<run>.jsonl`；campaign 生命周期五态 + INTERRUPTED fail-closed |
| `tests/` | pytest 单测（本地 `.venv_tf` 全绿为 E0 前置） |
| `tests/test_data_access.py` | T-002 单测 14 例：负向三形态（未登记 campaign 访问 test/OOD、train API 越权 test、OOD 缺 `ood=True`）必须 raise `SealedAccessError`；六项核验逐项构造性正/反用例；生命周期单向；访问日志字段；DatasetView 切片/元数据（合成小 npz，不触真实数据值） |

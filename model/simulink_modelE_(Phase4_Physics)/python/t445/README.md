# python/t445/ — Phase 4-F T-445 Trajectory Oracle Gate Python 包

> **入库策略**: 入库（代码与单测）；机器可读证据精选入 `results/t445_oracle_gate/`；
> checkpoint / SimState 等本地大文件入 `artifacts/t445/`（不入库）。

## 用途

T-445 Trajectory Oracle Gate（spec `.specs/phase4-f-trajectory-oracle-gate/`）的 Python 侧：
冻结 config 导出、B1 归一化复现、语料选择、cluster bootstrap、Gate 裁决。
不消费 E 段 winner 模型，不访问 IID test / OOD。

**预测模型**：无（本包不训练、不加载 NN）。

## 目录结构

| 条目 | 说明 |
|---|---|
| `frozen_config.py` | ★ T-001 冻结参数表（design 🔒 数值，k_eq^E 初始 PROVISIONAL） |
| `freeze_t001.py` | ★ T-001 导出驱动（config JSON / fixture·env·E-guard manifest） |
| `paths.py` / `hashutil.py` | 相对路径与 SHA256 工具 |
| `tests/test_t001_config.py` | T-001 单测（冻结字段 / overlap 三 false / env / E 守卫） |
| `verify_lambda_envelope.py` | T-003 R4 静态核查（law=6 floor 在、0.80 cap 不在且无法经批准 run-local 注入） |
| 后续 T-003+ | `compute_b1_norm.py` / `corpus_select.py` / bootstrap / Gate（T-003 因 R4 STOP 未完成） |

MATLAB 对侧（`scripts/+t445/`，非本目录）：T-002 起窗装配 / 真 ZOH 适配器。

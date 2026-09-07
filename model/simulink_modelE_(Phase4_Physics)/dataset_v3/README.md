# dataset_v3/ — Dataset v3 Python 构建与审计包（Phase 4-D D0）

> **入库策略**: 入库（本包纯代码与测试；大数据在 `datasets_v3/`，不入库）。

## 用途

Phase 4-D（spec `.specs/phase4-d-dataset-v3`）Dataset v3 两段式管线的 Python 侧：
聚合、窗口化、target/E300、split、三级泄漏审计、OOD 机制、manifest 与机器契约校验。
机器事实源 = `config/dataset_v3_signal_registry.json` +
`config/dataset_v3_producer_schema.json`（经 `contracts.py` 单一访问层消费，FR-465）。

**模型角色**：本包不做预测（Model E Plant 侧无预测模型）；它为 Phase 4-E 预测模型
提供窗口/目标/审计纪律约束的数据集产物（trajectory target [30] + legacy scalar E300）。

## 内容清单

| 条目 | 说明 |
|---|---|
| `__init__.py` | 包标识与版本 |
| `contracts.py` | registry/producer schema 单一访问层（role/aggregation/白名单消费） |
| `validate_contracts.py` | 机器契约校验器（FR-465 fail-fast；registry/producer schema/provenance 六字段 + `--write-hash`） |
| `aggregate.py` | channel-aware causal aggregation（T-008：time_mean trapezoidal / interval_endpoint / last_mode / interval_any，bin (a,b] 右闭含容差；DGN-06 zero-fill） |
| `windowing.py` | anchor 网格/窗口合法性/K-F vintage 精确对齐（T-009；DGN-07） |
| `targets.py` | native-rate 能量差 target y_k=E_k/10 + E300（T-009；DGN-08；design 原文 ×3600/10 为登记笔误） |
| `build_dataset_v3.py` | role 白名单 model views + quantile 契约守卫（T-010；E2E 编排在 run_pipeline.py） |
| `split.py` | deterministic group-aware split（T-011；DGN-09：SHA256 64-bit u + half-up + 完备性三不变量） |
| `audit_leakage.py` | L1/L2/L3 三级泄漏审计（T-012；DGN-10：L1 Hard FAIL / L2 provenance 三分类禁自动剔除 / L3 SBD suspect flag） |
| `audit_ood.py` | OOD 装载/物理域检查（T-012；DGN-11 三轴 + PHYSICS_EXTRAPOLATION/STRESS_SET） |
| `manifest.py` | FR-488 十五项 manifest + deterministic payload hash（T-014；DGN-12 + R8 payload/run_metadata 分离） |
| `run_pipeline.py` | pilot E2E 编排（T-015）：.mat 航次 → 聚合/窗口/target → split → 审计 → 归一化 → manifest → pilot_gate_result.json |
| `cd_interface_gate.py` | C/D Interface Gate 十项独立机器键 c01~c10（T-016/D-15；DGN-16；result schema v2；`--formality FORMAL\|NON-FORMAL` 双域——FORMAL=EXT-CD-GATE（⑤ 载体双存/embedded self_hash 核对/⑥ fixture+producer_script_sha/⑦ embedded eligibility），⑩ 基线快照消费 fail-closed；NON-FORMAL 骨架自检不构成 T-432 解锁） |
| `tests/` | 单元 + 负向 + 确定性测试（178 例，pytest，`.venv_tf` 环境；含 `test_cd_interface_gate.py`——Gate v2 十键独立/双域/⑤ 收紧/⑩ fail-closed 负向） |

## 使用

```bash
# 契约校验（fail-fast）
python -m dataset_v3.validate_contracts --registry --producer-schema
# pilot E2E（前置：scripts/70 已导出 datasets_v3/pilot/*.mat）
python -m dataset_v3.run_pipeline
```

## 维护规则

新增/移动/重命名/删除本目录内任何文件，必须同步更新本 README（`AGENTS.md` §20）。

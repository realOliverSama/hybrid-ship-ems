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
| `audit_ood.py` | OOD 装载/物理域检查（T-012；DGN-11 三轴 + PHYSICS_EXTRAPOLATION/STRESS_SET；**T-018 重构**：轴 B 训练支持下界 2.0→1.5（T-432 activation record `train_support_min_ht` 生效），轴 C held-out 组合改由冻结 `config/mission_v3/heldout_token_registry.json` 加载（D4——canonical token 禁手抄，composition 词表 = registry factor_a/factor_b 名，手写组合常量移除，classify_ood 签名向后兼容） |
| `manifest.py` | FR-488 十五项 manifest + deterministic payload hash（T-014；DGN-12 + R8 payload/run_metadata 分离） |
| `run_pipeline.py` | pilot E2E 编排（T-015）：.mat 航次 → 聚合/窗口/target → split → 审计 → 归一化 → manifest → pilot_gate_result.json |
| `cd_interface_gate.py` | C/D Interface Gate 十项独立机器键 c01~c10（T-016/D-15；DGN-16；result schema v2；`--formality FORMAL\|NON-FORMAL` 双域——FORMAL=EXT-CD-GATE（⑤ 载体双存/embedded self_hash 核对/⑥ fixture+producer_script_sha/⑦ embedded eligibility），⑩ 基线快照消费 fail-closed；NON-FORMAL 骨架自检不构成 T-432 解锁） |
| `run_formal_freeze.py` | T-020 formal freeze 主编排（pilot 链 FORMAL 化改写，底层函数全复用）：STAGE_B 标记门 → 航次 formal 门（fixture 反转：PILOT_ONLY 禁入/provenance stage=formal）→ 聚合/窗口/target → split 冻结（split_seed 自 activation record 消费，group=split_group_id）→ L1 零违规/L2 三分类+allocation 扣减/L3 require_train_stats → 两跑 payload hash 复验 → OOD 集成 → sealed manifest；`EMS_FORMAL_FREEZE_DRYRUN=1` 走 NON-FORMAL 干跑（stage=FORMAL_DRYRUN，产物不入 results/formal） |
| `freeze_ood.py` | T-020 formal OOD 三轴正式证据（FR-485/486）：allocation 仅由 activation counts 派生（amendment 01 后 5×40=200，陈旧 total_voyages 字段从不读取）、realized 域指标出证、退役条件（A2）出现即拒收、STRESS_SET/UNCLASSIFIED 分流且 purity 从严 FAIL、FR-485 splitter 声明；域判定全复用 audit_ood |
| `freeze_manifest.py` | T-020 sealed manifest（FR-487/488+DGN-12 强化）：FR-488 十五项必填全集派生自 manifest.py 常量、test_sealed/ood_sealed 严格布尔 True（禁默认置位）、stage=FORMAL 强制、OOD 定义/seed 与 activation record 交叉核对；assert_sealed 复核 payload hash 锚定与 run_metadata 分离性 |
| `tests/` | 单元 + 负向 + 确定性测试（260 例，pytest，`.venv_tf` 环境；含 `test_cd_interface_gate.py`——Gate v2 十键独立/双域/⑤ 收紧/⑩ fail-closed 负向、`test_audit_ood.py`——T-018 activation 生效专项、`test_freeze_ood.py`——OOD 三轴证据合成语料单测（含真实 activation 5×40=200 冻结用例）、`test_freeze_manifest.py`——sealed manifest 29 例（缺键/sealed 非严格 True/stage 不符/篡改 hash/确定性）、`test_run_formal_freeze.py`——formal 门反转/两跑复验/dryrun 语义 16 例、`test_h2_surface.py`——H2 面 fail-closed 双向一致（AMENDMENT_02）6 例 |

## 使用

```bash
# 契约校验（fail-fast）
python -m dataset_v3.validate_contracts --registry --producer-schema
# pilot E2E（前置：scripts/70 已导出 datasets_v3/pilot/*.mat）
python -m dataset_v3.run_pipeline
```

## 维护规则

新增/移动/重命名/删除本目录内任何文件，必须同步更新本 README（`AGENTS.md` §20）。

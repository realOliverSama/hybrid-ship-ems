# results/t445_oracle_gate/ — T-445 Trajectory Oracle Gate 机器可读证据

> **入库策略**: 精选入库（manifest / Gate JSON / 报告）；逐 run 大日志与 checkpoint 不在此目录
> （本地 `artifacts/t445/`）。dataset v3 只读，不在此写入。

## 用途

Phase 4-F T-445 的证据落盘根。T-001 写入冻结身份与守卫；正式 B1/B2 结果仅在
`freeze_record.json overall=true`（T-012）之后进入 `runs/`。

**预测模型**：无（oracle 真值轨迹，非 NN）。

## 目录结构

| 条目 | 说明 |
|---|---|
| `qualification_fixture_manifest.json` | ★ T-001 非正式 fixture 身份冻结（三 overlap 标志全 false） |
| `env_manifest_t445.json` | ★ T-001 环境守卫（macos_local + MATLAB R2026a + 无云端 result-bearing） |
| `e_stage_readonly_guard.json` | ★ T-001 E 段只读快照 + 静态扫描零 .keras/winner 加载 |
| `baseline_integrity_t001.json` | Model E 基线 / `.slx` / builder sha256 登记（模型零修改） |
| `t001_unit_test_result.json` | T-001 导出摘要 |
| `t002_unit_test_result.json` | T-002 窗装配 + ZOH 机器测试证据 |
| `lambda_envelope_r4.json` | T-003 原 R4 STOP（`R4_CAP_NOT_LEGALLY_IMPLEMENTABLE`）溯源，禁删 |
| `lambda_envelope_r4_native.json` | Amendment a′ 后 R4：`LAW6_NATIVE_ENVELOPE_CONFIRMED` |
| `b1_norm_repro.json` | T-003 B1 归一化复现（E_ref/E_scale/n_t0） |
| `t003_10b_smoke.json` | T-003 10b smoke 不受影响 |
| `t003_b1_core.json` | T-003 B1 核心：K_S 恒等式 + Model E 块 vs 离线容差 0 |
| `incidents.jsonl` | 事故账本（禁删，FR-777） |
| `access_ledger.jsonl` | 访问台账（purpose ∈ QUALIFICATION/REGRESSION/SYNTHETIC_SMOKE/FORMAL） |
| `qualification/` | B 阶段资格化 JSON（T-007/T-010/T-011） |
| `runs/` | 正式逐 run JSON（T-013 起；T-001 仅建目录） |
| 后续 | `corpus_manifest.json` / `freeze_record.json` / `gate_result.json` / `final_report.md` |

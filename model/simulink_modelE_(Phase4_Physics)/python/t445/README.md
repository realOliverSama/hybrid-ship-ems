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
| `frozen_config.py` | ★ T-001 冻结参数表（Amendment C：k_eq^E=0.5457 标称 SOC FEC + w_term=1.17e5；状态 PROVISIONAL 至 T-007 identity） |
| `freeze_t001.py` | ★ T-001 导出驱动（config JSON / fixture·env·E-guard manifest） |
| `qualify_t007_identity.py` | ★ T-007 能量边界身份资格化（记账/库仑/DC-DC/pack/FEC 构造；保留 FAIL 工件） |
| `rebind_config_identity.py` | Amendment C 后 T-002/003/005 证据 SHA 重绑（不改科学语义；语料 identity 断言） |
| `paths.py` / `hashutil.py` | 相对路径与 SHA256 工具 |
| `tests/test_t001_config.py` | T-001 单测（冻结字段 / overlap 三 false / env / E 守卫） |
| `tests/test_t007_identity.py` | T-007 身份资格化单测（保留 FAIL、ladder 记账、FEC 构造） |
| `verify_lambda_envelope.py` | T-003 R4 原生包络（Amendment a′：`LAW6_NATIVE_ENVELOPE_CONFIRMED`；旧 STOP JSON 保留） |
| `compute_b1_norm.py` | T-003 从 train 语料复现 E_ref/E_scale |
| `corpus_select.py` | ★ T-005 val-only 形状评分/三分层/SHA 排序，写 `corpus_manifest.json` |
| `tests/test_corpus_select.py` | T-005 合成单测（shape_cv、shrinking 窗、分层、族闭包、种子复现） |
| `stats_pairing.py` / `bootstrap_cluster.py` / `gate_verdict.py` | ★ T-006 h_v→h_g→median_g、分层 cluster bootstrap、确定性 Gate 裁决 |
| `gate_schema.json` | T-006 Gate JSON schema（t445_oracle_gate_v1） |
| `tests/test_t006_gate.py` | T-006 合成边界单测（GO/NO-GO/INCONCLUSIVE/VALIDITY/B3 正交/种子复现） |

MATLAB 对侧（`scripts/+t445/`，非本目录）：T-002 窗/ZOH；T-008 策略表 + 窗内 DP；T-010 PMS 感知燃油代理（动作仍仅为 Δλ）。

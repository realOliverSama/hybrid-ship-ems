# replay/ — T-809 车道 B：TCN 预测器 host-int8 参考离线复演产物

Spec: `ems01-chil-tcn64-swap`（T-809，FR-710 预备）。预测器 = TCN-f64 int8
tflite（`Model/tcn_q/tcn_q.tflite`，sha16 `a3406b8466b5ae10`，双输入
[1,300,1]窗+[1,2]aux，QDQ float32 I/O），由 `gen_reference_int8.py --model tcnq`
/ `gen_host_int8_pgen.py --model tcnq` 生成。**文件名一律 tcnq_ 前缀**，与既有
v1 mlpq 参考目录 `Host/reference/`（及本目录根部 v1 并行车道产物）并存不覆盖；
CSV schema 与 mlpq 逐列一致（Fixed Decisions 10：不新造 schema）。

## 文件清单

| 文件 | 内容 | 生成命令（仓库根，`.venv_tf` python） |
|---|---|---|
| `tcnq_vectors_raw.csv` | val200 ×302 原始物理量向量（seed=20260831，与 v1 同源同值） | `Host/tools/gen_reference_int8.py --model tcnq --sample-val200 Host/results/tcn_swap/replay` |
| `tcnq_vectors_norm.csv` | 同上归一化域（R8 口径） | 同上 |
| `tcnq_reference_int8_val200.csv` | val200 int8 q10/q50/q90 [kWh] + e_hat + cls（TCN tflite 前向） | 同上 |
| `tcnq_reference_int8_val200_logits.csv` | workclf int8 logits（识别器不换型，与 v1 同值） | 同上 |
| `tcnq_reference_int8_trace_S20.csv` | S20 注入轨迹逐 tick int8 参考（模式 B，5521 tick） | `gen_reference_int8.py --model tcnq --trace <WS>/results/ecms_phase25/fw_reference/ref_G2_S20_composite_voyage_matrix.csv --out <本文件>` |
| `tcnq_reference_int8_trace_test_voyage_002.csv` | voyage002 注入轨迹逐 tick int8 参考（4212 tick） | 同构（ref_G2_test_voyage_002_v2.csv） |
| `tcnq_reference_int8_pgen_S20.csv` | S20 host-int8 全链 P_gen 参考（同一 ems_ctrl.c，5521 tick） | `gen_host_int8_pgen.py --model tcnq --trace <S20 ref_G2> --modeb <S20 modeB 产物> --out <本文件>` |
| `tcnq_reference_int8_pgen_test_voyage_002.csv` | voyage002 host-int8 全链 P_gen 参考（4212 tick） | 同构 |
| `tcnq_crosscheck_keras_vs_int8.json` | q3 int8 vs keras float 参考（`Host/results/tcn_swap/ref_keras_*.csv`，另一车道 T-802 产物）交叉核对记录 | `/tmp/tcn_laneB/crosscheck_q3_vs_keras.py`（工作树外脚本，见该文件头） |

## 交叉核对结论（记录不设门，T-809 口径）

1 LSB_TCN = 0.0161389485 × y_std 16.495129 = **0.266214 kWh**（design 先验）。
|Δq50| int8 vs keras（全集 + 稳态 tick≥300 分列）见 JSON；量级 = int8 量化差：

- val200：mean 0.58 / max 5.23 kWh（≈2.2 / 19.7 LSB_TCN）
- S20 稳态（n=5221）：mean 0.89 / p99 2.76 / max 4.79 kWh
- voyage002 稳态（n=3912）：mean 0.68 / p99 3.68 / max 8.21 kWh
- 启动 flat-fill 窗（tick<300，OOD）int8 输出饱和钳位（上限 73.34 kWh），
  与 v1 mlpq 同签名（v1 S20 tick1-4 钳位于 70.63 kWh）；极少数
  高负荷 tick（每轨迹 2~4 个）keras q50 超出 int8 表示范围。

## 门禁与回归

- 生成过程硬门：T-307/T-312 冻结指标全 val 复现 OK + tcnq sha16 身份门 OK
  （每次运行自动执行）。
- 默认行为回归：无 `--model`（=mlpq）重跑 mode A / mode B / pgen，输出与
  `Host/reference/` 既有产物逐字节一致（S20/voyage002 × trace/pgen × modeA
  4 文件全部 diff IDENTICAL）。
- `ems_inject.py --selftest` PASS（--model tcnq 增补后离线自测通过）。
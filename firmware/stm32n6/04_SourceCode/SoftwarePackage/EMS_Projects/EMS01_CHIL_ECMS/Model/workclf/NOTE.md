# workclf 部署包状态 NOTE（T-502，2026-08-31）

## 当前状态：真 int8 部署包已生成，本 NOTE 转为缺陷存档记录

`workclf.tflite`（sha256 `7c3571e4eb7c9f0f2834a758c5b35b0a8cad0bd6921bf8ab7d10a0e4418f293d`，
源 = `results/ecms_phase25/classifier/workclf_v1_int8.tflite`，T-312 重导出，
`Optimize.DEFAULT` + val 前 1000 样本仅校准，QDQ float I/O + int8 权重，
与 mlp_q 同形态）已完成 analyze / generate / `Binary/network-data-workclf.hex`
@0x70400000 全链路，双重跑幂等验证。当前 `analyze_report.txt` 即真 int8 报告。

## 缺陷存档（误标 float32 模型，已取代）

- 误标文件（旧 `workclf_v1_int8.tflite`，现改名 `workclf_v1_float.tflite`，
  sha256 `3dd5a031…d739`）经 stedgeai v2.2.0 analyze 证实为**全 float32**：
  `model_fmt=float`、weights 31,716 B（≈7871 params × 4 B）、"+232(+0.7%) vs
  float model"、全部计算算子告警 "not quantized"、epochs 21/33 为 SW。
- 根因：`train_workclf.py` 导出段裸 `TFLiteConverter.convert()` 无量化参数；
  T-307 int8 量化从未覆盖 workclf 分类器。权威缺陷记录见
  `results/ecms_phase25/classifier/workclf_int8_report.json` 的 `defect_record`
  （主 Agent 验收）。
- **存档方式说明（如实记录）**：float 时期的原始 `analyze_report.txt` 在本任务
  恢复执行 int8 analyze 时被同名覆盖（上轮"保留不删"裁决未落实到改名保档，
  执行 slips）；其关键数值已完整保留于本 NOTE 上节与上轮任务报告，权威缺陷
  记录以上述 json 为准。float 时期的 `st_ai_output/`、`st_ai_ws/` 中间产物
  同样已被 int8 产物覆盖（工具同名输出）。

## 经验教训（供主 Agent 记录）

"证据文件保留"裁决必须在收到文件的第一时间执行**改名保档**（如
`analyze_report.float_defect.txt`），不能默认后续步骤不会同名覆盖。

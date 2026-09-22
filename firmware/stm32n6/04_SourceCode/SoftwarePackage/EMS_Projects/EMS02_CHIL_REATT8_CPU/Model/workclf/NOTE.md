# workclf 部署包状态 NOTE（T-502，2026-08-31；4.0.1 stai 迁移 2026-09-03）

## 4.0.1 stai 迁移（层②，2026-09-03）

随 P3 TCN 槽位迁移到 ST Edge AI **4.0.1**（用户指令"最新工具链重试层②"），workclf
用 4.0.1 CLI 以 `--name workclf` 重生成（脚本 `generate-n6-model-stai401.sh`，
日志 `generate-stai401.log`）：

- 源 tflite 不变（sha256 `7c3571e4…f293d`，量化口径与 2.2 部署一致，Conv1D SW
  fallback 属预期）；I/O 契约不变（IN_1=8B 旁路 / IN_2=120B Pdec / OUT_1=28B
  logits，`workclf.h` static_assert 防漂移）。
- 产物符号带网络名后缀（`stai_workclf_*` / `LL_ATON_*_workclf` / `NN_Interface_workclf`），
  与 TCN（`--name network`）直接同链接，2.2 时代的宏改名不再需要。
- 重生成动因：2.2 生成 `network.c` 的版本自检（`LL_ATON_VERSION_* == 1.1.1-14`）
  与 4.0.1 运行时头（1.1.3-275）硬冲突，且运行时库换 NetworkRuntime1201。
- 2.2 生成物保留：`network.{c,h}` / `network_ecblobs.h` / `network_data.xSPI2.bin`
  （泛名 Default 符号）——EMS_PREDICTOR=mlpq 可逆性出口（`ems_npu_workclf.c`，
  2.2 LL_ATON 栈 + NetworkRuntime1020）仍引用；2.2 版 hex 备份于
  `Host/results/tcn_swap/network-data-workclf.hex.ll_aton221.bak`。
- `Binary/network-data-workclf.hex` 现为 **4.0.1 版**（raw 源 `workclf_data.xSPI2.bin`，
  与 2.2 版 raw 差 30/8129 字节——atonn 代间布局差异，各自配对各自 network.c）。
- 包装 TU：`../../Appli/App/ems_npu_workclf_stai.c`（tcnq 分支选编，随 4.0.1
  运行时栈链接）。

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

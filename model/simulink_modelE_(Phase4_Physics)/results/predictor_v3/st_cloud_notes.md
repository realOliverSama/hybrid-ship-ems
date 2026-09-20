# ST Edge AI Developer Cloud 评测记录（2026-09-15，E 段部署附录）

**性质**：部署探索性评测，不属于 E 段验收范围（E-Q7：INT8/N6 出 E 段验收）。
Winner 权威数值以 FINAL 包内 FP32 Keras 工件为准。

## 模型：REAT-T winner（reatt-t018，1.25M 参数，FP32 12.5MB）

| 交付候选 | 体积 | 数值状态 |
|---|---|---|
| reatt_winner_s0.keras（FP32） | 12.5 MB | 科学权威（FINAL 包内 sha256 在册） |
| reatt_winner_fp32.tflite | 4.1 MB | vs Keras delta 3e-4，corr ≈1.0 |
| reatt_final6_int8.onnx（QDQ） | 1.8 MB | corr 0.91 / q50 MAE ~59 kW |
| reatt_final6_int8_qop.onnx（QOperator） | 1.5 MB | corr 0.94 / q50 MAE ~62 kW |

## ST 云端实测结果（stedgeai-dc.st.com，STM32N6 / Neural-ART，Core 4.0.1）

| 尝试 | 格式 | 结果 |
|---|---|---|
| 1 | .keras | E102: NPU 编译器不接受 keras_dump 输入 |
| 2 | .onnx（opset 17） | INTERNAL ERROR: No Op registered for Erfc (opset 17) |
| 3 | .onnx（opset 13） | NOT IMPLEMENTED: Unsupported layer types: Erfc |
| 4 | .onnx noerfc（tanh 替换） | INTERNAL ERROR: Unknown dimensions: W |
| 5 | .onnx shapeinf | 同 4 |
| 6 | .tflite | 量化入口只支持 ONNX；benchmark 入口未开放 TFLite |
| 7 | 本地 onnxruntime INT8（QDQ/QOperator） | 成功产出 1.5-1.8MB INT8 模型；corr 0.91-0.94 |

## 结论

1. **算子层面**：GELU 精确式（Erfc）是 NPU 编译器不支持的算子——tanh 等价替换
   （数值 corr 0.999）可绕过；
2. **图层面**：tf2onnx 导出的动态 Shape/Reshape 链导致 "Unknown dimensions: W"，
   ST 编译器的静态 shape 推导比 ORT 严格——需常量化全部 Reshape shape 输入；
3. **多输入**：ST 云端量化服务对双输入模型支持不佳（npz 读取 rank 错误）；
4. **可行部署线**：本地 INT8 量化（corr 0.91-0.94）+ TFLite FP32（corr 1.0）两条线
   均已产出可用工件，NPU 实测需 ST 本地工具链（stedgeai CLI）或 T-019 后另立部署 spec；
5. **对本论文**：部署可行性调查为附加证据，REAT-T 的科学结论（HPO/E3/终评）
   不受影响。

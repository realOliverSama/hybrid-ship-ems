# ST Edge AI 部署评测上传目录（2026-09-15）
#
# 有效交付物（不入库，含模型权重）：
#   reatt_winner_s0.keras          原 Keras winner（科学权威工件）
#   reatt_winner_fp32.tflite       TFLite FP32（delta 3e-4，TFLM 部署线可用）
#   reatt_final6_int8.onnx         ONNX INT8 QDQ（corr 0.91，本地量化）
#   reatt_final6_int8_qop.onnx     ONNX INT8 QOperator（corr 0.94）
#   quantization_calib_1000.npz    校准集（train 1000 窗归一化输入）
#   input_contract.json            输入契约说明
#
# _deprecated_onnx/ 为转换调试中间产物（Erfc/动态 shape 问题的历史版本）。
# 结论与清单见 results/predictor_v3/deployment_candidates.json
# 详见 results/predictor_v3/st_cloud_notes.md

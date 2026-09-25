# -*- coding: utf-8 -*-
"""npu_quantize_int8.py — 静态 attention+tanh 版 ONNX 的 int8 QDQ 量化（NPU 形态）。

标定 = 部署口径（归一化 train 窗，与 CPU 形态 tflite 完全同抽样/同变换）。
产物：reatt_static_int8.onnx（QDQ）+ 与 fp32 ONNX 的对拍统计。
"""
import os, sys, json
import numpy as np

def _find_repo_root(start):
    d = os.path.abspath(start)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("ERROR: 无法定位仓库根")
        d = parent


REPO = _find_repo_root(os.path.dirname(os.path.abspath(__file__)))
MODELE = os.path.join(REPO, "model", "simulink_modelE_(Phase4_Physics)")
OUT = os.path.join(REPO, "firmware", "stm32n6", "04_SourceCode", "SoftwarePackage",
                   "EMS_Projects", "EMS02_CHIL_REATT8_NPU", "Model", "reatt8_q_npu")
sys.path.insert(0, os.path.join(MODELE, "python")); sys.path.insert(0, MODELE)

from onnxruntime.quantization import (quantize_static, QuantFormat, QuantType,
                                      CalibrationDataReader)

FR = os.path.join(OUT, "reatt_concat_fp32.onnx")   # Concat 零填充版（v3）
DQ = os.path.join(OUT, "reatt_concat_int8.onnx")

# ---- 标定数据（部署口径：normalized train windows；与 CPU 形态同 seed/同变换）----
from predictor_v3 import features, norm_ext
formal_norm = features.load_formal_norm(); ne = norm_ext.load_artifact()
order = features.registry_channel_order()
z = np.load(os.path.join(MODELE, "datasets_v3", "formal", "windows_v3_formal.npz"), allow_pickle=True)
tr = np.nonzero(z["splits"] == "train")[0]
rng = np.random.default_rng(20260922)
cal_idx = np.sort(rng.choice(tr, size=1024, replace=False))

def assemble(idx):
    n = len(idx)
    h = np.zeros((n, 300, 11), np.float32); k = np.zeros((n, 30, 6), np.float32)
    for j, c in enumerate(order["h1"]):
        m = np.asarray(formal_norm["x_h1_mean"][c], np.float64)
        s = np.asarray(formal_norm["x_h1_std"][c], np.float64)
        h[:, :, j] = ((z[f"X_H1::{c}"][idx].astype(np.float64) - m) / s).astype(np.float32)
    for j, c in enumerate(order["kf"]):
        k[:, :, j] = np.asarray(norm_ext.transform_channel(
            ne, c, z[f"X_KF::{c}"][idx].astype(np.float64)), np.float32)
    return h, k

h_cal, k_cal = assemble(cal_idx)

class Reader(CalibrationDataReader):
    def __init__(self):
        self.i = 0
    def get_next(self):
        if self.i + 1 > len(h_cal):
            return None
        b = slice(self.i, self.i + 1)
        self.i += 1
        return {"hist_input": h_cal[b], "kf_input": k_cal[b]}
    def rewind(self):
        self.i = 0

quantize_static(FR, DQ, Reader(), quant_format=QuantFormat.QDQ,
                activation_type=QuantType.QInt8, weight_type=QuantType.QInt8,
                per_channel=True)
print("int8 ONNX:", DQ, os.path.getsize(DQ), "bytes")

# ---- 对拍：int8 ONNX (ORT) vs fp32 ONNX (ORT) ----
import onnxruntime as ort
so8 = ort.InferenceSession(DQ, providers=["CPUExecutionProvider"])
so32 = ort.InferenceSession(FR, providers=["CPUExecutionProvider"])
va = np.nonzero(z["splits"] == "val")[0]
ref_idx = np.sort(rng.choice(va, size=64, replace=False))
h_r, k_r = assemble(ref_idx)
y8 = np.concatenate([so8.run(None, {"hist_input": h_r[i:i+1], "kf_input": k_r[i:i+1]})[0]
                     for i in range(len(h_r))])
y32 = np.concatenate([so32.run(None, {"hist_input": h_r[i:i+1], "kf_input": k_r[i:i+1]})[0]
                      for i in range(len(h_r))])
d = np.abs(y8 - y32)
print(f"int8 vs fp32(onnx): max|d|={d.max():.2f} mean={d.mean():.3f} kW | corr(m)={np.corrcoef(y8[...,0].ravel(), y32[...,0].ravel())[0,1]:.5f}")

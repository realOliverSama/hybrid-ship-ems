#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_reference_split.py — 拆分形态 host 参考链（phase4-g NPU 线）。

背景（用户 2026-09-23 裁决）：整图 NPU 形态输出退化后改为拆分部署——
  N1 = TCN 支路（CPU 形态，编译自 reatt_n1_int8.onnx，f32 边界 I/O）
  N2 = Attention/Dense/Fusion/Decoder（NPU 形态，编译自 reatt_n2_int8.onnx，
       kf 边界 int8 / h_past 边界 f32 / 输出边界 int8）

本工具生成与**板上完全同口径**的 host 参考（用于板级 L1 对拍硬门）：
  hist_raw,kf_raw ──冻结归一化（formal_norm_v3 + norm_ext_v1）──> 归一化窗
    → ORT(reatt_n1_int8.onnx)                      → h_past f32（= 板上 N1 输出）
    → ORT(reatt_n2_int8.onnx, kf 归一化, h_past)   → raw f32
    → **NPU 输出边界量化**（scale 3.608830452 / zp −128，Q.json 实测）→ int8
    → 反量化 → 单调分位变换（稳定 softplus）→ q_split [n,30,3] kW

另存 raw_ort（不经输出边界量化）以分离「NPU 执行误差」与「输出量化误差」。

产物：Host/reference/reatt/reatt_ref_vectors_split.npz
        keys: hist_raw, kf_raw, q_keras, q_split, q_ort, h_past, raw_ort, ref_idx
      Host/results/reatt_board/split_ref_report.json（量化参数 + 一致性摘要）

路径纪律：全部由 __file__ 相对推导（AGENTS.md §20.2）。
用法（仓库根 .venv_tf）：python3 <本文件>
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np


def find_repo_root(start_dir):
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("ERROR: 无法定位仓库根")
        d = parent


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = find_repo_root(SCRIPT_DIR)
MODELE = os.path.join(REPO_ROOT, "model", "simulink_modelE_(Phase4_Physics)")
PROJ = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
NPU_DIR = os.path.join(PROJ, "Model", "reatt8_q_npu")
REF_DIR = os.path.join(PROJ, "Host", "reference", "reatt")
RES_DIR = os.path.join(PROJ, "Host", "results", "reatt_board")

REF_IN = os.path.join(REF_DIR, "reatt_ref_vectors.npz")
REF_OUT = os.path.join(REF_DIR, "reatt_ref_vectors_split.npz")

ONNX_N1 = os.path.join(NPU_DIR, "reatt_n1_int8.onnx")
ONNX_N2 = os.path.join(NPU_DIR, "reatt_n2_int8.onnx")

# NPU 张量边界量化参数（生成物实测：stai_reattn2n_get_info 同源 = Q.json）
KF_SCALE, KF_ZP = 0.022380275651812553, -19
OUT_SCALE, OUT_ZP = 3.608830451965332, -128


def softplus_stable(x):
    ax = np.abs(x)
    return np.maximum(x, 0.0) + np.log1p(np.exp(-ax))


def raw_to_q(raw):
    m = raw[..., 0]
    return np.stack([m - softplus_stable(raw[..., 1]), m,
                     m + softplus_stable(raw[..., 2])], axis=-1)


def normalize(hist_raw, kf_raw, formal_norm, norm_ext, ne, h1_order, kf_order):
    """冻结变换（f64 域 → f32；= 固件 ems_reatt_params 口径）。"""
    hist = np.zeros_like(hist_raw, dtype=np.float32)
    kf = np.zeros_like(kf_raw, dtype=np.float32)
    for j, c in enumerate(h1_order):
        mean = np.asarray(formal_norm["x_h1_mean"][c], dtype=np.float64)
        std = np.asarray(formal_norm["x_h1_std"][c], dtype=np.float64)
        hist[:, :, j] = ((hist_raw[:, :, j].astype(np.float64) - mean)
                         / std).astype(np.float32)
    for j, c in enumerate(kf_order):
        t = norm_ext.transform_channel(ne, c, kf_raw[:, :, j].astype(np.float64))
        kf[:, :, j] = np.asarray(t, dtype=np.float32)
    return hist, kf


def add_repo_paths():
    for p in (os.path.join(MODELE, "python"), MODELE):
        if p not in sys.path:
            sys.path.insert(0, p)


def main() -> int:
    import onnxruntime as ort
    add_repo_paths()
    from predictor_v3 import features, norm_ext  # type: ignore  # noqa

    formal_norm = features.load_formal_norm()
    ne = norm_ext.load_artifact()
    order = features.registry_channel_order()
    h1_order, kf_order = order["h1"], order["kf"]

    z = np.load(REF_IN)
    hist_raw, kf_raw = z["hist_raw"], z["kf_raw"]
    q_keras = z["q_keras"]
    n = hist_raw.shape[0]
    hist_n, kf_n = normalize(hist_raw, kf_raw, formal_norm, norm_ext, ne,
                             h1_order, kf_order)

    s1 = ort.InferenceSession(ONNX_N1, providers=["CPUExecutionProvider"])
    s2 = ort.InferenceSession(ONNX_N2, providers=["CPUExecutionProvider"])
    i1 = s1.get_inputs()[0].name
    h_past = np.zeros((n, 96), dtype=np.float32)
    raw_ort = np.zeros((n, 30, 3), dtype=np.float32)
    for i in range(n):
        hp = s1.run(None, {i1: hist_n[i:i + 1]})[0]
        h_past[i] = np.asarray(hp, dtype=np.float32).reshape(96)
        raw_ort[i] = np.asarray(
            s2.run(None, {"kf_input": kf_n[i:i + 1],
                          "h_past_input": hp.astype(np.float32)}),
            dtype=np.float32).reshape(30, 3)

    # 部署口径：NPU 输出边界量化（int8 90 B）后再反量化
    q_i8 = np.clip(np.rint(raw_ort / OUT_SCALE) + OUT_ZP, -128, 127)
    raw_deq = (q_i8 - OUT_ZP) * OUT_SCALE
    q_split = raw_to_q(raw_deq.astype(np.float64)).astype(np.float32)
    q_ort = raw_to_q(raw_ort.astype(np.float64)).astype(np.float32)

    np.savez(REF_OUT,
             hist_raw=hist_raw, kf_raw=kf_raw, q_keras=q_keras,
             q_split=q_split, q_ort=q_ort, h_past=h_past,
             raw_ort=raw_ort, ref_idx=z["ref_idx"])

    d_outq = np.abs(q_split[..., 1] - q_ort[..., 1])
    d_keras = np.abs(q_split[..., 1] - q_keras[..., 1])
    report = {
        "schema": "phase4g_reatt_split_reference_v1",
        "n": int(n),
        "source": {"npz": os.path.basename(REF_IN),
                   "n1": os.path.basename(ONNX_N1), "n2": os.path.basename(ONNX_N2)},
        "deploy_quant": {"kf_scale": KF_SCALE, "kf_zp": KF_ZP,
                         "out_scale": OUT_SCALE, "out_zp": OUT_ZP},
        "h_past_stats": {"min": float(h_past.min()), "max": float(h_past.max()),
                         "mean": float(h_past.mean()), "std": float(h_past.std())},
        "out_boundary_quant_effect_kW": {
            "q50_mae": float(d_outq[:, 1].mean()), "q50_max": float(d_outq[:, 1].max())},
        "vs_keras_kW": {"q50_mae": float(d_keras[:, 1].mean()),
                        "q50_max": float(d_keras[:, 1].max())},
        "h_past_head": [float(v) for v in h_past[0, :4]],
    }
    os.makedirs(RES_DIR, exist_ok=True)
    with open(os.path.join(RES_DIR, "split_ref_report.json"), "w") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())

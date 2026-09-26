#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""quantize_reatt.py — REAT-T winner 量化部署链（phase4-g，T-004）。

冻结源（只读）：
  ModelE/experiments/predictor_v3/confirm/reatt/reatt_s0.keras   FR-836 冻结 winner
  ModelE/results/dataset_v3/formal/formal_norm_v3.json           H1 逐 bin 归一化
  ModelE/config/predictor_v3/predictor_norm_ext_v1.json          KF 逐 lead 归一化
  ModelE/config/dataset_v3_signal_registry.json                  通道序事实源
  ModelE/datasets_v3/formal/windows_v3_formal.npz                冻结窗（train 标定）

链（spec .specs/phase4-g-reatt-board-deployment D1/D2）：
  1. 部署图裁剪：输出 = raw_proj (m,a,b) [30,3]（TFLite SOFTPLUS 溢出对策，
     单调分位变换由固件稳定 softplus 实现）；
  2. fp32 tflite（from_keras_model，权重冻结；对拍 keras）；
  3. int8 tflite：全整数量化（float32 I/O），标定集 = **归一化 train 窗**
     1024 个（seed 20260922）——部署口径标定（AI_MODELING §22.3 硬规则：
     固件实际喂入分布 = 原始域→冻结归一化→f32，标定样本同域同变换）；
  4. 产物：Model/reatt8_q/reatt_raw_int8.tflite（板部署锚点）+
     Host/reference/reatt/（对拍向量 + host 参考输出）+
     Host/results/reatt_board/quant_report.json（L0 指标 + 三口径一致检查）。

确定性：窗抽样与标定序固定（seed）；tflite 字节 = TF 版本 + 源权重的确定函数。
--check：不重跑量化，校验现存 tflite sha256 与源身份并重算对拍指标。

用法（.venv_tf）：
    python Host/tools/quantize_reatt.py [--check] [--n-cal 1024] [--n-ref 64]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


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


REPO_ROOT = find_repo_root(SCRIPT_DIR)
MODELE = os.path.join(REPO_ROOT, "model", "simulink_modelE_(Phase4_Physics)")
PROJ = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
MODEL_DIR = os.path.join(PROJ, "Model", "reatt8_q")
REF_DIR = os.path.join(PROJ, "Host", "reference", "reatt")
RES_DIR = os.path.join(PROJ, "Host", "results", "reatt_board")

KERAS = os.path.join(MODELE, "experiments", "predictor_v3", "confirm",
                     "reatt", "reatt_s0.keras")
FR836_KERAS_SHA = ("d8da6af4667875c45de7de90383d2b6b750420f5af"
                   "62c7dcdcb1e5997897f894")
NPZ = os.path.join(MODELE, "datasets_v3", "formal", "windows_v3_formal.npz")

TFLITE_INT8 = os.path.join(MODEL_DIR, "reatt_raw_int8.tflite")
TFLITE_FP32 = os.path.join(MODEL_DIR, "reatt_raw_fp32.tflite")

SEED = 20260922
L0_GATE = {"q50_mae_kW_max": 25.0, "q50_corr_min": 0.98}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def softplus_stable(x):
    ax = np.abs(x)
    return np.maximum(x, 0.0) + np.log1p(np.exp(-ax))


def raw_to_q(raw):
    m = raw[..., 0]
    return np.stack([m - softplus_stable(raw[..., 1]), m,
                     m + softplus_stable(raw[..., 2])], axis=-1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="校验现存产物身份 + 重算对拍指标（不重量化）")
    ap.add_argument("--n-cal", type=int, default=1024)
    ap.add_argument("--n-ref", type=int, default=64,
                    help="对拍参考向量数（train/val 各半，无 test）")
    args = ap.parse_args()

    os.makedirs(REF_DIR, exist_ok=True)
    os.makedirs(RES_DIR, exist_ok=True)

    keras_sha = sha256_file(KERAS)
    if keras_sha != FR836_KERAS_SHA:
        raise SystemExit(f"FATAL: keras 身份漂移 {keras_sha} != FR836 冻结值")

    import tensorflow as tf  # noqa: 延迟导入（--check 无 TF 环境时报错清晰）
    from predictor_v3.hpo import _custom_objects  # type: ignore
    from predictor_v3 import features, norm_ext  # type: ignore

    model = tf.keras.models.load_model(
        KERAS, compile=False, custom_objects=_custom_objects())
    deploy = tf.keras.Model(inputs=model.inputs,
                            outputs=model.get_layer("raw_proj").output)
    n_params = int(model.count_params())

    formal_norm = features.load_formal_norm()
    ne = norm_ext.load_artifact()
    order = features.registry_channel_order()
    h1_order, kf_order = order["h1"], order["kf"]

    z = np.load(NPZ, allow_pickle=True)
    splits = z["splits"]
    tr_idx = np.nonzero(splits == "train")[0]
    va_idx = np.nonzero(splits == "val")[0]
    rng = np.random.default_rng(SEED)
    cal_idx = np.sort(rng.choice(tr_idx, size=args.n_cal, replace=False))
    ref_idx = np.sort(np.concatenate([
        rng.choice(tr_idx, size=args.n_ref // 2, replace=False),
        rng.choice(va_idx, size=args.n_ref - args.n_ref // 2, replace=False)]))

    def assemble_raw(idx):
        """原始域窗（线上传输形态：hist [n,300,11] t 主序，kf [n,30,6]）。"""
        hist = np.zeros((len(idx), 300, 11), dtype=np.float32)
        kf = np.zeros((len(idx), 30, 6), dtype=np.float32)
        for j, c in enumerate(h1_order):
            hist[:, :, j] = z[f"X_H1::{c}"][idx].astype(np.float32)
        for j, c in enumerate(kf_order):
            kf[:, :, j] = z[f"X_KF::{c}"][idx].astype(np.float32)
        return hist, kf

    def normalize(hist_raw, kf_raw):
        """冻结变换（f64 域 → f32；= 固件 ems_reatt.c 的 f32 实现参考口径）。"""
        n = hist_raw.shape[0]
        hist = np.zeros_like(hist_raw, dtype=np.float32)
        kf = np.zeros_like(kf_raw, dtype=np.float32)
        for j, c in enumerate(h1_order):
            mean = np.asarray(formal_norm["x_h1_mean"][c], dtype=np.float64)
            std = np.asarray(formal_norm["x_h1_std"][c], dtype=np.float64)
            hist[:, :, j] = ((hist_raw[:, :, j].astype(np.float64) - mean)
                             / std).astype(np.float32)
        for j, c in enumerate(kf_order):
            t = norm_ext.transform_channel(ne, c,
                                           kf_raw[:, :, j].astype(np.float64))
            kf[:, :, j] = np.asarray(t, dtype=np.float32)
        return hist, kf

    cal_raw = assemble_raw(cal_idx)
    ref_raw = assemble_raw(ref_idx)
    cal_hist, cal_kf = normalize(*cal_raw)
    ref_hist, ref_kf = normalize(*ref_raw)

    # ---- 量化 / 校验 ----
    if args.check:
        if not os.path.isfile(TFLITE_INT8):
            raise SystemExit("FATAL: 缺 int8 tflite（先完整跑一次）")
        tfl_i8 = open(TFLITE_INT8, "rb").read()
    else:
        conv8 = tf.lite.TFLiteConverter.from_keras_model(deploy)
        conv8.optimizations = [tf.lite.Optimize.DEFAULT]

        def rep():
            for s in range(0, args.n_cal, 64):
                yield [cal_hist[s:s + 64], cal_kf[s:s + 64]]

        conv8.representative_dataset = rep
        tfl_i8 = conv8.convert()
        open(TFLITE_INT8, "wb").write(tfl_i8)
        conv = tf.lite.TFLiteConverter.from_keras_model(deploy)
        tfl_fp = conv.convert()
        open(TFLITE_FP32, "wb").write(tfl_fp)

    def run_b1(content, h, k):
        inter = tf.lite.Interpreter(model_content=content)
        inter.allocate_tensors()
        ind, outd = inter.get_input_details(), inter.get_output_details()
        ys = []
        for i in range(h.shape[0]):
            inter.set_tensor(ind[0]["index"], h[i:i + 1])
            inter.set_tensor(ind[1]["index"], k[i:i + 1])
            inter.invoke()
            ys.append(inter.get_tensor(outd[0]["index"]).copy())
        return np.concatenate(ys, axis=0)

    raw_keras = deploy([ref_hist, ref_kf], training=False).numpy()
    q_keras = raw_to_q(raw_keras)
    raw_i8 = run_b1(tfl_i8, ref_hist, ref_kf)
    q_i8 = raw_to_q(raw_i8.astype(np.float64))
    y_truth = z["Y"][ref_idx].astype(np.float32)

    d50 = np.abs(q_i8[..., 1] - q_keras[..., 1])
    d10 = np.abs(q_i8[..., 0] - q_keras[..., 0])
    d90 = np.abs(q_i8[..., 2] - q_keras[..., 2])
    corr = float(np.corrcoef(q_i8[..., 1].ravel(), q_keras[..., 1].ravel())[0, 1])

    l0 = {
        "q50_mae_kW": float(d50.mean()), "q50_max_kW": float(d50.max()),
        "q50_p99_kW": float(np.percentile(d50, 99)),
        "q10_mae_kW": float(d10.mean()), "q90_mae_kW": float(d90.mean()),
        "q50_corr": corr,
        "finite_rate": float(np.isfinite(q_i8).mean()),
        "q50_mae_vs_truth_int8": float(np.abs(q_i8[..., 1] - y_truth).mean()),
        "q50_mae_vs_truth_keras": float(np.abs(q_keras[..., 1] - y_truth).mean()),
    }
    gate_ok = (l0["q50_mae_kW"] <= L0_GATE["q50_mae_kW_max"]
               and l0["q50_corr"] >= L0_GATE["q50_corr_min"]
               and l0["finite_rate"] == 1.0)

    # 三口径一致检查（AI_MODELING §22.3 硬门，机器可读）
    caliber = {
        "training": "windows_v3_formal splits=='train'，frozen formal_norm_v3/"
                    "norm_ext_v1 归一化域（features.assemble 同变换）",
        "deployment_feed": "固件 ems_reatt.c：原始域 hist/kf → ems_reatt_params"
                           "（formal_norm_v3/norm_ext_v1 冻结常数，f32 域）→ f32 张量",
        "int8_calibration": "本脚本标定集 = train 窗原始域 → 同冻结变换 → 归一化域"
                            f"（n={args.n_cal}, seed={SEED}）",
        "channel_orders": {"h1": h1_order, "kf": kf_order},
        "consistent": True,
        "note": "三口径同为归一化域同变换；与 T-508 事故模式（raw 喂归一化标定模型）"
                "相反，本链为正向样板（同 MLP-Q）。",
    }

    provenance = {
        "keras_sha256": keras_sha,
        "fr836_frozen": True,
        "tflite_int8_sha256": hashlib.sha256(tfl_i8).hexdigest(),
        "tflite_int8_bytes": len(tfl_i8),
        "tf_version": tf.__version__,
        "n_params": n_params,
        "deploy_output": "raw_proj (m,a,b) [30,3]; quantile transform in firmware",
    }

    # 参考向量（板级 L1/L2 对拍输入 = 原始域 + host 参考输出）
    np.savez(os.path.join(REF_DIR, "reatt_ref_vectors.npz"),
             hist_raw=ref_raw[0], kf_raw=ref_raw[1],
             q_keras=q_keras.astype(np.float32),
             q_int8=q_i8.astype(np.float32),
             y_truth=y_truth, ref_idx=ref_idx)
    report = {
        "schema": "phase4g_reatt_quant_report_v1",
        "l0_gate": {**L0_GATE, "pass": bool(gate_ok)},
        "l0_metrics": l0,
        "provenance": provenance,
        "caliber_contract": caliber,
        "seed": SEED, "n_cal": args.n_cal, "n_ref": args.n_ref,
    }
    out = os.path.join(RES_DIR, "quant_report.json")
    json.dump(report, open(out, "w"), indent=2, ensure_ascii=False)
    print(json.dumps({"l0_gate_pass": gate_ok, "l0": l0,
                      "tflite_int8_sha256": provenance["tflite_int8_sha256"]},
                     indent=2))
    return 0 if gate_ok else 1


if __name__ == "__main__":
    sys.path.insert(0, os.path.join(MODELE, "python"))
    sys.path.insert(0, MODELE)
    sys.exit(main())

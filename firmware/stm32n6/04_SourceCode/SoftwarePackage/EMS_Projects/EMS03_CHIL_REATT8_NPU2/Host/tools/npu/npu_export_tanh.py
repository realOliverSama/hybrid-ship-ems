# -*- coding: utf-8 -*-
"""npu_export_tanh.py — REAT-T NPU 形态部署变体导出（phase4-g，本地迭代）。

背景：NPU 编译器（ST Edge AI Core 4.0.1）不支持 Erfc（精确 GELU）；
E 段已证 tanh 近似 GELU 数值 corr 0.999（st_cloud_notes.md）。本脚本由
冻结构建器（predictor_v3.models.reat_t）复制改造：GELU → gelu(approximate=True)
（唯一差异），加载冻结 winner 权重（层名不变），产出：
  Model/reatt8_q_npu/reatt_tanhgelu_fp32.onnx（静态 shape，批=1，opset 17）

数值对拍在脚本内完成（tanh 变体 vs 冻结 winner，raw m,a,b 输出）。
"""
from __future__ import annotations

import json
import os
import sys

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
                   "EMS_Projects", "EMS01_CHIL_REATT8", "Model", "reatt8_q_npu")
sys.path.insert(0, os.path.join(MODELE, "python"))
sys.path.insert(0, MODELE)

os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
import tensorflow as tf  # noqa: E402
from tensorflow.keras import layers  # noqa: E402

from predictor_v3.hpo import _custom_objects  # noqa: E402
from predictor_v3.models import common as _common  # noqa: E402
from predictor_v3.models.reat_t import (TilePast, DILATIONS,  # noqa: E402
                                        NUM_HEADS, _sp_inv)

KERAS = os.path.join(MODELE, "experiments", "predictor_v3", "confirm",
                     "reatt", "reatt_s0.keras")
SELECTED = os.path.join(MODELE, "results", "predictor_v3", "candidates",
                        "reatt", "selected_config.json")
EVAL_CFG = os.path.join(MODELE, "config", "predictor_v3", "eval_config.json")


def gelu_tanh(x):
    """tanh 近似 GELU（E 段部署替换，corr 0.999；无 Erfc 算子）。"""
    return tf.keras.activations.gelu(x, approximate=True)


def build_tanh(cfg: dict, s0: float):
    cp, df, nf, d_dec, drop = cfg["Cp"], cfg["df"], cfg["Nf"], cfg["d_dec"], cfg["dropout"]
    hist = tf.keras.Input(shape=(300, 11), name="hist_input")
    kf = tf.keras.Input(shape=(30, 6), name="kf_input")
    x = layers.Conv1D(cp, 1, padding="same", name="past_proj")(hist)
    for i, d in enumerate(DILATIONS):
        y = layers.Conv1D(cp, 3, padding="causal", dilation_rate=d,
                          name=f"past_dil{i}")(x)
        y = layers.LayerNormalization(name=f"past_ln{i}")(y)
        y = layers.Activation(gelu_tanh, name=f"past_gelu{i}")(y)
        if drop:
            y = layers.Dropout(drop, name=f"past_drop{i}")(y)
        x = layers.Add(name=f"past_add{i}")([x, y])
    h_past = _common.LearnedQueryAttentionPooling(name="past_pool")(x)
    fut = layers.Dense(df, name="fut_proj")(kf)
    fut = _common.LearnedPositionalEncoding(30, df, name="lead_pos")(fut)
    for i in range(nf):
        n = layers.LayerNormalization(name=f"fut{i}_ln1")(fut)
        a = layers.MultiHeadAttention(NUM_HEADS, df // NUM_HEADS,
                                      name=f"fut{i}_mha")(n, n)
        if drop:
            a = layers.Dropout(drop, name=f"fut{i}_d1")(a)
        fut = layers.Add(name=f"fut{i}_add1")([fut, a])
        n = layers.LayerNormalization(name=f"fut{i}_ln2")(fut)
        f = layers.Dense(4 * df, activation=gelu_tanh, name=f"fut{i}_ffn1")(n)
        f = layers.Dense(df, name=f"fut{i}_ffn2")(f)
        if drop:
            f = layers.Dropout(drop, name=f"fut{i}_d2")(f)
        fut = layers.Add(name=f"fut{i}_add2")([fut, f])
    h_tile = TilePast(30, name="h_tile")(h_past)
    fused = layers.Concatenate(name="fusion_cat")([fut, h_tile])
    fused = layers.LayerNormalization(name="fusion_ln")(fused)
    fused = layers.Dense(df, activation=gelu_tanh, name="fusion_dense")(fused)
    if drop:
        fused = layers.Dropout(drop, name="fusion_drop")(fused)
    z = layers.Dense(d_dec, activation=gelu_tanh, name="dec_dense")(fused)
    if drop:
        z = layers.Dropout(drop, name="dec_drop")(z)
    raw = layers.Dense(3, name="raw_proj")(z)
    return tf.keras.Model([hist, kf], raw, name="reat_t_tanh")


def main() -> int:
    sel = json.load(open(SELECTED, encoding="utf-8"))
    cfg = sel["config"]
    # s0 仅作新建模型 raw_proj 偏置初始化；权重将由 load_weights 全量覆盖，
    # 故取冻结 y_std 均值即可（无需精确复刻训练期标量）。
    fn = json.load(open(os.path.join(
        MODELE, "results", "dataset_v3", "formal", "formal_norm_v3.json"),
        encoding="utf-8"))
    s0 = float(np.mean(np.asarray(fn["y_std"], dtype=np.float64)))
    print("config:", cfg, "| s0 =", s0)

    winner = tf.keras.models.load_model(KERAS, compile=False,
                                        custom_objects=_custom_objects())
    win_raw = tf.keras.Model(inputs=winner.inputs,
                             outputs=winner.get_layer("raw_proj").output)

    tanh_model = build_tanh(cfg, s0)
    # 冻结权重加载（层名对齐；raw_proj bias 初始差异由权重覆盖）
    tanh_model.load_weights(KERAS)
    print("params:", tanh_model.count_params())

    # 对拍（同一批窗：train/val 抽样 64）
    z = np.load(os.path.join(MODELE, "datasets_v3", "formal",
                             "windows_v3_formal.npz"), allow_pickle=True)
    from predictor_v3 import features, norm_ext
    formal_norm = features.load_formal_norm()
    ne = norm_ext.load_artifact()
    order = features.registry_channel_order()
    tr = np.nonzero(z["splits"] == "train")[0]
    va = np.nonzero(z["splits"] == "val")[0]
    rng = np.random.default_rng(20260922)
    idx = np.sort(np.concatenate([rng.choice(tr, 32, replace=False),
                                 rng.choice(va, 32, replace=False)]))
    hist = np.zeros((len(idx), 300, 11), np.float32)
    kf = np.zeros((len(idx), 30, 6), np.float32)
    for j, c in enumerate(order["h1"]):
        m = np.asarray(formal_norm["x_h1_mean"][c], np.float64)
        s = np.asarray(formal_norm["x_h1_std"][c], np.float64)
        hist[:, :, j] = ((z[f"X_H1::{c}"][idx].astype(np.float64) - m) / s).astype(np.float32)
    for j, c in enumerate(order["kf"]):
        kf[:, :, j] = np.asarray(norm_ext.transform_channel(
            ne, c, z[f"X_KF::{c}"][idx].astype(np.float64)), np.float32)
    y_tanh = tanh_model([hist, kf], training=False).numpy()
    y_win = win_raw([hist, kf], training=False).numpy()
    d = np.abs(y_tanh - y_win)
    corr = float(np.corrcoef(y_tanh[..., 0].ravel(), y_win[..., 0].ravel())[0, 1])
    print(f"parity tanh vs exact: max|d|={d.max():.4f} kW, mean={d.mean():.4f} kW, corr(m)={corr:.6f}")
    np.save(os.path.join(REPO, "experiments", "reatt_p4g", "tanh_parity.npy"),
            np.stack([y_tanh[..., 0], y_win[..., 0]]))

    # 静态 ONNX 导出（批=1；opset 17）
    import tf2onnx
    spec = (tf.TensorSpec((1, 300, 11), tf.float32, name="hist_input"),
            tf.TensorSpec((1, 30, 6), tf.float32, name="kf_input"))
    onnx_path = os.path.join(OUT, "reatt_tanhgelu_fp32.onnx")
    tf2onnx.convert.from_keras(tanh_model, input_signature=spec, opset=17,
                               output_path=onnx_path)
    import onnx
    m = onnx.load(onnx_path)
    from collections import Counter
    ops = Counter(n.op_type for n in m.graph.node)
    print("ONNX:", onnx_path, os.path.getsize(onnx_path), "B |", len(m.graph.node), "nodes")
    print("Erfc count:", ops.get("Erfc", 0))
    print("ops:", dict(ops))
    return 0


if __name__ == "__main__":
    sys.exit(main())

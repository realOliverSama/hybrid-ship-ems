# -*- coding: utf-8 -*-
"""npu_export_static.py — REAT-T NPU 形态 v2：静态化 attention + tanh GELU。

动机（phase4-g NPU 线）：Keras MHA 的内部分解图使用运行时 shape 计算
（tf.shape→reshape 链），ST NPU 编译器两条路（TFLite 内部转换 / tf2onnx
ONNX）都过不去（"ONNX shape inference fails" / "Unknown dimensions: W"）。
本脚本用**显式常量 Reshape/Permute/MatMul/Softmax** 重写 attention（数学
与 Keras MHA 完全等价——batch=1、attention_axes=(1,)、无 mask），权重由
winner MHA 平移；GELU 一并换 tanh 近似（Erfc 不支持）。

产物：Model/reatt8_q_npu/reatt_static_fp32.onnx（静态）+ 对拍报告。
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "..", ".."))
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
from predictor_v3.models.reat_t import TilePast, DILATIONS, NUM_HEADS  # noqa: E402

KERAS = os.path.join(MODELE, "experiments", "predictor_v3", "confirm",
                     "reatt", "reatt_s0.keras")
SELECTED = os.path.join(MODELE, "results", "predictor_v3", "candidates",
                        "reatt", "selected_config.json")

HEADS, KEY_DIM = 4, 32
SCALE = 1.0 / np.sqrt(KEY_DIM)


def gelu_tanh(x):
    return tf.keras.activations.gelu(x, approximate=True)


class StaticMHA(layers.Layer):
    """显式静态 attention（数学 == Keras MHA：batch=1, attention_axes=(1,), 无 mask）。

    weights: [q_k(128,128), q_b(128), k_k, k_b, v_k, v_b, o_k(128,128), o_b(128)]
    布局与 Keras MHA 子层关系：kernel(128,4,32) reshape 为 (128,128)、bias(4,32)
    reshape 为 (128,)；输出侧 kernel(4,32,128) reshape 为 (128,128)。
    """

    def __init__(self, n_lead=30, d_model=128, **kw):
        super().__init__(**kw)
        self.n_lead = n_lead
        self.d_model = d_model
        self.q = layers.Dense(d_model, use_bias=True, name="q")
        self.k = layers.Dense(d_model, use_bias=True, name="k")
        self.v = layers.Dense(d_model, use_bias=True, name="v")
        self.o = layers.Dense(d_model, use_bias=True, name="o")
        self.rs_h = layers.Reshape((self.n_lead, HEADS, KEY_DIM), name="rs_heads")
        self.pt = layers.Permute((2, 1, 3), name="pt_heads")   # [B,L,H,D]->[B,H,L,D]
        self.pt_k = layers.Permute((1, 3, 2), name="pt_kt")    # [B,H,L,D]->[B,H,D,L]
        self.pt_back = layers.Permute((2, 1, 3), name="pt_back")
        self.rs_f = layers.Reshape((self.n_lead, d_model), name="rs_flat")

    def call(self, x):
        q = self.pt(self.rs_h(self.q(x)))                       # [B,H,L,D]
        k = self.pt(self.rs_h(self.k(x)))
        v = self.pt(self.rs_h(self.v(x)))
        kt = self.pt_k(k)                                       # [B,H,D,L]
        s = tf.matmul(q, kt) * SCALE                            # [B,H,L,L]
        s = tf.nn.softmax(s, axis=-1)
        c = tf.matmul(s, v)                                     # [B,H,L,D]
        y = self.rs_f(self.pt_back(c))                          # [B,L,d]
        return self.o(y)


def build_static(cfg: dict, s0: float):
    from predictor_v3.models.reat_t import _sp_inv
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
        a = StaticMHA(name=f"fut{i}_mha")(n)
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
    raw = layers.Dense(3, bias_initializer=tf.keras.initializers.Constant(
        np.array([0.0, _sp_inv(s0), _sp_inv(s0)])), name="raw_proj")(z)
    return tf.keras.Model([hist, kf], raw, name="reat_t_static")


def transplant(src_model, dst_model, nf: int):
    """把 winner（tanh 变体亦可）权重平移到静态模型（含 MHA 子层映射）。"""
    for layer in dst_model.layers:
        if isinstance(layer, StaticMHA):
            i = layer.name.split("_")[0]           # e.g. fut0_mha -> fut0
            src = src_model.get_layer(layer.name)  # Keras MHA
            w = {v.path.split("/")[-2] + "/" + v.path.split("/")[-1]: v.numpy()
                 for v in src.weights}
            qk = w["query/kernel"].reshape(128, 128)
            qb = w["query/bias"].reshape(128)
            kk = w["key/kernel"].reshape(128, 128)
            kb = w["key/bias"].reshape(128)
            vk = w["value/kernel"].reshape(128, 128)
            vb = w["value/bias"].reshape(128)
            ok = w["attention_output/kernel"].reshape(128, 128)
            ob = w["attention_output/bias"]
            # StaticMHA 内部子层权重顺序：q,k,v,o
            q = [qk, qb]; k = [kk, kb]; v = [vk, vb]; o = [ok, ob]
            layer.q.set_weights(q)
            layer.k.set_weights(k)
            layer.v.set_weights(v)
            layer.o.set_weights(o)
        else:
            try:
                src = src_model.get_layer(layer.name)
                if src.weights:
                    layer.set_weights(src.get_weights())
            except Exception:
                pass


def main() -> int:
    sel = json.load(open(SELECTED, encoding="utf-8"))
    cfg = sel["config"]
    fn = json.load(open(os.path.join(
        MODELE, "results", "dataset_v3", "formal", "formal_norm_v3.json"),
        encoding="utf-8"))
    s0 = float(np.mean(np.asarray(fn["y_std"], dtype=np.float64)))

    winner = tf.keras.models.load_model(KERAS, compile=False,
                                        custom_objects=_custom_objects())
    win_raw = tf.keras.Model(inputs=winner.inputs,
                             outputs=winner.get_layer("raw_proj").output)
    static = build_static(cfg, s0)
    transplant(winner, static, cfg["Nf"])
    print("params:", static.count_params())

    # 对拍（与 winner 精确 GELU 相比：含 tanh 近似 + attention 重写两项差异）
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
    y_static = static([hist, kf], training=False).numpy()
    y_win = win_raw([hist, kf], training=False).numpy()
    d = np.abs(y_static - y_win)
    corr = float(np.corrcoef(y_static[..., 0].ravel(), y_win[..., 0].ravel())[0, 1])
    print(f"parity static vs winner: max|d|={d.max():.4f} kW mean={d.mean():.4f} corr={corr:.6f}")

    import tf2onnx
    spec = (tf.TensorSpec((1, 300, 11), tf.float32, name="hist_input"),
            tf.TensorSpec((1, 30, 6), tf.float32, name="kf_input"))
    p = os.path.join(OUT, "reatt_static_fp32.onnx")
    tf2onnx.convert.from_keras(static, input_signature=spec, opset=17, output_path=p)
    import onnx
    from collections import Counter
    m = onnx.load(p)
    ops = Counter(n.op_type for n in m.graph.node)
    print("ONNX:", os.path.getsize(p), "B,", len(m.graph.node), "nodes")
    print("shape-ops: Shape=%d Gather=%d ReduceProd=%d Reshape=%d" % (
        ops.get("Shape", 0), ops.get("Gather", 0), ops.get("ReduceProd", 0),
        ops.get("Reshape", 0)))
    print("Erfc:", ops.get("Erfc", 0), "| ops:", dict(ops))
    return 0


if __name__ == "__main__":
    sys.exit(main())

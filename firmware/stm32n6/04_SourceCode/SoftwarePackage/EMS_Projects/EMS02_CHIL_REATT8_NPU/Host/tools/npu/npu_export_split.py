# -*- coding: utf-8 -*-
"""npu_export_split.py — REAT-T 分体网络导出（N1=TCN 段 / N2=注意力-Dense 段）。

路线（用户裁决 2026-09-23）：TCN 卷积段在 NPU 上历史性语义分歧 → N1 走 CPU 形态；
N2（Dense/attention，workclf 类简单结构）独立编译上 NPU 尝试加速。两段权重自冻结
winner 平移，数值严格等价（切点 = past_pool 输出 h_past[1,96]）。

v3 动机：v2 的中间维 Pad 手术（Transpose-Pad-Transpose）虽让工具编过，但板端
数值退化（工具对该模式的内部变换错）。v3 用 Slice→Mul(0)→Concat 构造左零填充，
Conv1D 改 padding='valid'——全静态、无 Pad、无中间转置，因果语义不变。

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


def build_split(cfg: dict, s0: float, part: str):
    """part='n1'：hist→h_past[1,96]；part='n2'：kf+h_past→(m,a,b)[1,30,3]。"""
    from predictor_v3.models.reat_t import _sp_inv
    cp, df, nf, d_dec, drop = cfg["Cp"], cfg["df"], cfg["Nf"], cfg["d_dec"], cfg["dropout"]

    if part == "n1":
        hist = tf.keras.Input(shape=(300, 11), name="hist_input")
        x = layers.Conv1D(cp, 1, padding="same", name="past_proj")(hist)
        for i_, d in enumerate(DILATIONS):
            pad = 2 * d
            z = layers.Lambda(lambda t, p=pad: t[:, :p, :] * 0.0,
                              name=f"past_zpad{i_}")(x)
            y = layers.Concatenate(axis=1, name=f"past_ccat{i_}")([z, x])
            y = layers.Conv1D(cp, 3, padding="valid", dilation_rate=d,
                              name=f"past_dil{i_}")(y)
            y = layers.LayerNormalization(name=f"past_ln{i_}")(y)
            y = layers.Activation(gelu_tanh, name=f"past_gelu{i_}")(y)
            if drop:
                y = layers.Dropout(drop, name=f"past_drop{i_}")(y)
            x = layers.Add(name=f"past_add{i_}")([x, y])
        h_past = _common.LearnedQueryAttentionPooling(name="past_pool")(x)
        return tf.keras.Model([hist], h_past, name="reatt_n1")

    kf = tf.keras.Input(shape=(30, 6), name="kf_input")
    h_in = tf.keras.Input(shape=(cp,), name="h_past_input")
    fut = layers.Dense(df, name="fut_proj")(kf)
    fut = _common.LearnedPositionalEncoding(30, df, name="lead_pos")(fut)
    for i_ in range(nf):
        n = layers.LayerNormalization(name=f"fut{i_}_ln1")(fut)
        a = StaticMHA(name=f"fut{i_}_mha")(n)
        if drop:
            a = layers.Dropout(drop, name=f"fut{i_}_d1")(a)
        fut = layers.Add(name=f"fut{i_}_add1")([fut, a])
        n = layers.LayerNormalization(name=f"fut{i_}_ln2")(fut)
        f = layers.Dense(4 * df, activation=gelu_tanh, name=f"fut{i_}_ffn1")(n)
        f = layers.Dense(df, name=f"fut{i_}_ffn2")(f)
        if drop:
            f = layers.Dropout(drop, name=f"fut{i_}_d2")(f)
        fut = layers.Add(name=f"fut{i_}_add2")([fut, f])
    h_tile = TilePast(30, name="h_tile")(h_in)
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
    return tf.keras.Model([kf, h_in], raw, name="reatt_n2")


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
    import json as _json
    sel = _json.load(open(SELECTED, encoding="utf-8"))
    cfg = sel["config"]
    fn = _json.load(open(os.path.join(
        MODELE, "results", "dataset_v3", "formal", "formal_norm_v3.json"),
        encoding="utf-8"))
    s0 = float(np.mean(np.asarray(fn["y_std"], dtype=np.float64)))

    winner = tf.keras.models.load_model(KERAS, compile=False,
                                        custom_objects=_custom_objects())
    win_raw = tf.keras.Model(inputs=winner.inputs,
                             outputs=winner.get_layer("raw_proj").output)

    n1 = build_split(cfg, s0, "n1")
    n2 = build_split(cfg, s0, "n2")
    transplant(winner, n1, cfg["Nf"])
    transplant(winner, n2, cfg["Nf"])
    print("N1 params:", n1.count_params(), "| N2 params:", n2.count_params())

    # 数据
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

    h1 = n1(hist, training=False).numpy()
    y_split = n2([kf, h1], training=False).numpy()
    y_win = win_raw([hist, kf], training=False).numpy()
    d = np.abs(y_split - y_win)
    print(f"split(N1->N2) vs winner: max|d|={d.max():.6f} kW mean={d.mean():.6f}")

    import tf2onnx
    for name, model, spec in (
            ("reatt_n1_fp32.onnx", n1, (tf.TensorSpec((1, 300, 11), tf.float32,
                                                      name="hist_input"),)),
            ("reatt_n2_fp32.onnx", n2, (tf.TensorSpec((1, 30, 6), tf.float32,
                                                      name="kf_input"),
                                        tf.TensorSpec((1, 96), tf.float32,
                                                      name="h_past_input")))):
        p = os.path.join(OUT, name)
        tf2onnx.convert.from_keras(model, input_signature=spec, opset=17,
                                   output_path=p)
        import onnx
        from collections import Counter
        mm = onnx.load(p)
        ops = Counter(nd.op_type for nd in mm.graph.node)
        print(f"{name}: {os.path.getsize(p)}B {len(mm.graph.node)} nodes "
              f"Erfc={ops.get('Erfc',0)} Pad={ops.get('Pad',0)} "
              f"Shape={ops.get('Shape',0)}")
    np.save(os.path.join(OUT, "h_past_ref.npy"), h1)  # 校准 h_past 参考（部署口径）
    return 0


if __name__ == "__main__":
    sys.exit(main())

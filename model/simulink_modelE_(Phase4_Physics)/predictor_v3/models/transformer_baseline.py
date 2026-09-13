# -*- coding: utf-8 -*-
"""models/transformer_baseline.py — Transformer baseline（T-010；DGN-E-07(c) + R3）。

冻结结构：hist → Linear(11→d) + learned pos[300] → PreLN Encoder×N_e
（MHA h头 + FFN 2d）→ past_seq[B,300,d]；kf → Linear(6→d) + learned lead-pos[30]
→ fut[B,30,d] → PreLN Decoder×N_d（self-MHA **非因果**：30 位合法 K/F 全互见
+ cross-MHA(past_seq) + FFN）→ [B,30,d] → per-lead Dense(3)=(m,a,b) → 共用
MonotonicQuantileTransform。H1+K+F；≤1.5M。

trial config 键：{"d": {64|128}, "N_e": {2|3}, "N_d": {1|2}, "dropout": {0|0.1},
"lr": {1e-4|3e-4|1e-3}, "batch": 128}（h=4、FFN=2d 固定）。

K/F 可见性契约（R3）：future self-attention **无因果掩码**（30 位全互见）——
单测以注意力矩阵全 1 非零断言（禁止 target 式因果掩码隐藏合法 K/F）。
"""
from __future__ import annotations

import numpy as np
import tensorflow as tf

MODEL_ID = "transformer"
ARCH_ID = "transformer"
FEATURE_SURFACE = "H1+K+F"
ENVELOPE = (0, 1_500_000)

NUM_HEADS = 4


def _preln_encoder_block(x, d, drop, name):
    from tensorflow.keras import layers
    n = layers.LayerNormalization(name=f"{name}_ln1")(x)
    a = layers.MultiHeadAttention(NUM_HEADS, d // NUM_HEADS, name=f"{name}_mha")(n, n)
    a = layers.Dropout(drop, name=f"{name}_drop1")(a) if drop else a
    x = layers.Add(name=f"{name}_add1")([x, a])
    n = layers.LayerNormalization(name=f"{name}_ln2")(x)
    f = layers.Dense(2 * d, activation="gelu", name=f"{name}_ffn1")(n)
    f = layers.Dense(d, name=f"{name}_ffn2")(f)
    f = layers.Dropout(drop, name=f"{name}_drop2")(f) if drop else f
    return layers.Add(name=f"{name}_add2")([x, f])


def _preln_decoder_block(y, mem, d, drop, name):
    """非因果 self-MHA（无 mask）+ cross-MHA(past_seq) + FFN（R3：30 位全互见）。"""
    from tensorflow.keras import layers
    n = layers.LayerNormalization(name=f"{name}_ln1")(y)
    s = layers.MultiHeadAttention(NUM_HEADS, d // NUM_HEADS, name=f"{name}_self")(
        n, n)                                            # 无 causal mask
    s = layers.Dropout(drop, name=f"{name}_drop1")(s) if drop else s
    y = layers.Add(name=f"{name}_add1")([y, s])
    n = layers.LayerNormalization(name=f"{name}_ln2")(y)
    c = layers.MultiHeadAttention(NUM_HEADS, d // NUM_HEADS, name=f"{name}_cross")(n, mem)
    c = layers.Dropout(drop, name=f"{name}_drop2")(c) if drop else c
    y = layers.Add(name=f"{name}_add2")([y, c])
    n = layers.LayerNormalization(name=f"{name}_ln3")(y)
    f = layers.Dense(2 * d, activation="gelu", name=f"{name}_ffn1")(n)
    f = layers.Dense(d, name=f"{name}_ffn2")(f)
    f = layers.Dropout(drop, name=f"{name}_drop3")(f) if drop else f
    return layers.Add(name=f"{name}_add3")([y, f])


def build(cfg: dict, s0: float):
    import tensorflow as tf
    from tensorflow.keras import layers
    from predictor_v3.models.common import quantile_head

    d = cfg["d"]
    drop = cfg["dropout"]

    hist = tf.keras.Input(shape=(300, 11), name="hist_input")
    kf = tf.keras.Input(shape=(30, 6), name="kf_input")

    past = layers.Dense(d, name="past_proj")(hist)
    past = past + _learned_pos(300, d, "past_pos")
    for i in range(cfg["N_e"]):
        past = _preln_encoder_block(past, d, drop, f"enc{i}")
    fut = layers.Dense(d, name="fut_proj")(kf)
    fut = fut + _learned_pos(30, d, "lead_pos")
    for i in range(cfg["N_d"]):
        fut = _preln_decoder_block(fut, past, d, drop, f"dec{i}")

    raw = layers.Dense(
        3, bias_initializer=tf.keras.initializers.Constant(
            np.array([0.0, _sp_inv(s0), _sp_inv(s0)])),
        name="raw_proj")(fut)                            # per-lead Dense(3)
    out = quantile_head(raw, s0, n_lead=1)               # raw 已 [B,30,3]
    return tf.keras.Model([hist, kf], out, name="transformer_baseline")


def _learned_pos(n: int, d: int, name: str):
    """learned positional embedding：[n, d]（与 [B,n,d] 相加广播）。"""
    from tensorflow.keras import layers
    return layers.Embedding(n, d, name=name)(tf.range(n))


def _sp_inv(s: float) -> float:
    from predictor_v3.quantile import softplus_inv
    return softplus_inv(s)


SEARCH_SPACE = {
    "d": [64, 128],
    "N_e": [2, 3],
    "N_d": [1, 2],
    "dropout": [0.0, 0.1],
    "lr": [1e-4, 3e-4, 1e-3],
    "batch": [128],
}

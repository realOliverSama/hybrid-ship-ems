# -*- coding: utf-8 -*-
"""models/tft.py — TFT static-free 适配（T-011；DGN-E-07(d)，Lim et al. 2021）。

冻结结构（static enrichment 整体省略；FR-579：static 协变量 v1 为空、
禁 METADATA 提特征）：
  VSN_past：11 个 per-channel GRN 嵌入（每步）→ softmax 变量权重 → 加权和 [B,300,d]
  VSN_future：6 通道同理 [B,30,d]
  LSTM encoder（past 300 步）/ decoder（future 30 步，喂 VSN_future）→ [B,330,d]
  Gated skip（GLU）→ InterpretableMHA（h=4；未来位间**不加因果掩码**）→ GLU+Add&Norm
  → position-wise GRN → 取未来 30 位 → per-lead Dense(3)=(m,a,b) → 共用
  MonotonicQuantileTransform。H1+K+F；≤1.5M；报告中明示"适配版"。

trial config 键：{"d": {64|128}, "n_lstm": {1|2}, "dropout": {0|0.1},
"lr": {1e-4|3e-4|1e-3}, "batch": 128}（h=4 固定）。

变量权重实现（冻结记录）：w = softmax_m(Dense(m)(concat(per-channel GRN 嵌入)))
（TFT 论文 GRN_χi + GRN_3→softmax 的 Dense 近似；model_config 记录）。
"""
from __future__ import annotations

import numpy as np

MODEL_ID = "tft"
ARCH_ID = "tft"
FEATURE_SURFACE = "H1+K+F"
ENVELOPE = (0, 1_500_000)

NUM_HEADS = 4


def _variable_selection(seqs, d, drop, name):
    """per-channel GRN（每步）→ softmax 变量权重 → 加权和。seqs: [m 个 [B,T,1]]。"""
    from tensorflow.keras import layers
    from predictor_v3.models.common import GRN
    m = len(seqs)
    embs = [GRN(d, dropout=drop, name=f"{name}_grn{i}")(seqs[i]) for i in range(m)]
    xi = layers.Concatenate(name=f"{name}_cat")(embs)          # [B,T,m*d]
    w = layers.Dense(m, activation="softmax", name=f"{name}_w")(xi)  # [B,T,m]
    out = embs[0] * w[..., 0:1]
    for i in range(1, m):
        out = out + embs[i] * w[..., i:i + 1]                  # [B,T,d]
    return out


def _lstm_stack(x, d, n_layers, drop, name, initial_states=None):
    """多层 LSTM（全层 return_sequences=True；逐层状态链）。

    initial_states：decoder 侧传 encoder 各层 states（第 i 层用 encoder 第 i 层
    final state，TFT 论文语义）；encoder 侧为 None。返回 (序列, [每层 states])。
    """
    from tensorflow.keras import layers
    y = x
    all_states = []
    for i in range(n_layers):
        lstm = layers.LSTM(d, return_sequences=True, return_state=True,
                           name=f"{name}_{i}")
        init = initial_states[i] if initial_states is not None else None
        if init is not None:
            y, *st = lstm(y, initial_state=init)
        else:
            y, *st = lstm(y)
        if drop:
            y = layers.Dropout(drop, name=f"{name}_drop{i}")(y)
        all_states.append(st)
    return y, all_states


def build(cfg: dict, s0: float):
    import tensorflow as tf
    from tensorflow.keras import layers
    from predictor_v3.models.common import (GLU, GRN, InterpretableMHA,
                                            quantile_head)

    d = cfg["d"]
    drop = cfg["dropout"]

    hist = tf.keras.Input(shape=(300, 11), name="hist_input")
    kf = tf.keras.Input(shape=(30, 6), name="kf_input")

    # VSN（split 最后一维为通道）
    from predictor_v3.models.common import ChannelSlice
    hist_ch = [ChannelSlice(i, name=f"hsplit{i}")(hist) for i in range(11)]
    kf_ch = [ChannelSlice(i, name=f"ksplit{i}")(kf) for i in range(6)]
    past = _variable_selection(hist_ch, d, drop, "vsn_past")   # [B,300,d]
    fut = _variable_selection(kf_ch, d, drop, "vsn_fut")       # [B,30,d]

    # LSTM encoder/decoder（逐层状态传递；无 static context）
    enc, enc_states = _lstm_stack(past, d, cfg["n_lstm"], drop, "enc")
    dec, _ = _lstm_stack(fut, d, cfg["n_lstm"], drop, "dec",
                         initial_states=enc_states)            # [B,30,d]
    seq = layers.Concatenate(axis=1, name="enc_dec")([enc, dec])  # 时间维 [B,330,d]

    # Gated skip → InterpretableMHA（无因果掩码）→ GLU+Add&Norm → position-wise GRN
    seq = GLU(d, name="glu_skip")(seq)
    att = InterpretableMHA(d, NUM_HEADS, name="imha")(seq, seq, seq)
    if drop:
        att = layers.Dropout(drop, name="imha_drop")(att)
    seq = layers.LayerNormalization(name="post_att_ln")(
        layers.Add(name="post_att_add")([seq, GLU(d, name="glu_att")(att)]))
    seq = GRN(d, dropout=drop, name="pos_grn")(seq)

    from predictor_v3.models.common import TakeFuture
    fut_seq = TakeFuture(300, name="take_future")(seq)         # 取未来 30 位
    raw = layers.Dense(
        3, bias_initializer=tf.keras.initializers.Constant(
            np.array([0.0, _sp_inv(s0), _sp_inv(s0)])),
        name="raw_proj")(fut_seq)                              # per-lead Dense(3)
    out = quantile_head(raw, s0, n_lead=1)
    return tf.keras.Model([hist, kf], out, name="tft_static_free")


def _sp_inv(s: float) -> float:
    from predictor_v3.quantile import softplus_inv
    return softplus_inv(s)


SEARCH_SPACE = {
    "d": [64, 128],
    "n_lstm": [1, 2],
    "dropout": [0.0, 0.1],
    "lr": [1e-4, 3e-4, 1e-3],
    "batch": [128],
}

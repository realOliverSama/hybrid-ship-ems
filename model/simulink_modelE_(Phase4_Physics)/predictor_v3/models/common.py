# -*- coding: utf-8 -*-
"""models/common.py — 五候选共享组件（DGN-E-06/07）。

- quantile head 组装（架构专属 raw 投影 + 共用 MonotonicQuantileTransform；
  raw bias 用稳定 softplus 逆初始化，s0 = eval_config 冻结 train 全局 y_std）；
- TCN/TFT/REAT-T 共用：AttentionPooling（learned query）、GRN、InterpretableMHA；
- 全部自定义层带 get_config（.keras reload 探针 T-017 硬门要求）。
"""
from __future__ import annotations

import json
import os

import numpy as np
import tensorflow as tf
from tensorflow.keras import layers

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
EVAL_CONFIG_PATH = os.path.join(MODELE_ROOT, "config", "predictor_v3",
                                "eval_config.json")


def load_s0() -> float:
    """s₀ = train 全局 y_std（来源 = eval_config.json 冻结记录值，DGN-E-06）。"""
    with open(EVAL_CONFIG_PATH, encoding="utf-8") as fh:
        cfg = json.load(fh)
    return float(cfg["normalization_denominators"]["global"]["value"])


def quantile_head(raw, s0: float, *, n_lead: int = 30):
    """架构 raw 投影输出 [B,30,3]=(m,a,b) → 共用 MonotonicQuantileTransform。

    raw 的生成层（架构侧）bias 必须以 tile(quantile_bias_init(s0), n_lead) 初始化
    ——本 helper 只组装变换；bias 初始化由各架构的 Dense 层参数完成。
    """
    import tensorflow as tf
    from predictor_v3.quantile import build_monotonic_quantile_transform
    if raw.shape[-1] != 3:
        raw = tf.keras.layers.Reshape((n_lead, 3))(raw)
    return build_monotonic_quantile_transform()(raw)


def raw_bias_init(s0: float, n_lead: int = 30) -> np.ndarray:
    """Dense(3*n_lead) raw 投影 bias 初始化（每 bin (m,a,b) 三位）。"""
    from predictor_v3.quantile import quantile_bias_init
    return np.tile(quantile_bias_init(s0), n_lead)


class LearnedQueryAttentionPooling(layers.Layer):
    """learned query 注意力池化（DGN-E-07(b)/(e)）：score = x·q
    （Dense(1, use_bias=False)）→ softmax over 时间位 → 加权和 [B, d]。"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.score = layers.Dense(1, use_bias=False)

    def call(self, x):
        w = self.score(x)                       # [B,T,1]
        w = tf.nn.softmax(w, axis=1)
        return tf.reduce_sum(x * w, axis=1)     # [B,d]

    def get_config(self):
        return super().get_config()


class GLU(layers.Layer):
    """Gated Linear Unit：sigmoid(Dense(a)) ⊙ Dense(b)（TFT/AWD 标准）。"""

    def __init__(self, units, **kwargs):
        super().__init__(**kwargs)
        self.units = units
        self.gate = layers.Dense(units, activation="sigmoid")
        self.lin = layers.Dense(units)

    def call(self, x):
        return self.gate(x) * self.lin(x)

    def get_config(self):
        cfg = super().get_config()
        cfg["units"] = self.units
        return cfg


class GRN(layers.Layer):
    """Gated Residual Network（TFT 论文标准；DGN-E-07(d) 组件）。

    GRN(Ω) = LayerNorm(Ω + GLU(η1))，η1 = W1·ELU(W2·Ω + b2) + b1。
    """

    def __init__(self, units, dropout=0.0, **kwargs):
        super().__init__(**kwargs)
        self.units = units
        self.dropout_rate = dropout
        self.w2 = layers.Dense(units)
        self.elu = layers.ELU()
        self.w1 = layers.Dense(units)
        self.glu = GLU(units)
        self.ln = layers.LayerNormalization()
        self.drop = layers.Dropout(dropout) if dropout else None

    def call(self, x, training=None):
        eta = self.w1(self.elu(self.w2(x)))
        if self.drop is not None:
            eta = self.drop(eta, training=training)
        return self.ln(x + self.glu(eta))

    def get_config(self):
        cfg = super().get_config()
        cfg.update({"units": self.units, "dropout": self.dropout_rate})
        return cfg


class InterpretableMHA(layers.Layer):
    """TFT 可解释多头注意力（DGN-E-07(d)；未来位间不加因果掩码）。

    W_Q/W_K 每头独立、W_V 头间共享；头上下文平均后线性输出。
    """

    def __init__(self, units, num_heads=4, **kwargs):
        super().__init__(**kwargs)
        assert units % num_heads == 0
        self.units = units
        self.num_heads = num_heads
        self.head_dim = units // num_heads
        self.wq = layers.Dense(units, use_bias=False)
        self.wk = layers.Dense(units, use_bias=False)
        self.wv = layers.Dense(units, use_bias=False)
        self.out = layers.Dense(units)

    def call(self, q, k=None, v=None):
        k = q if k is None else k
        v = q if v is None else v
        B = tf.shape(q)[0]
        Tq = tf.shape(q)[1]
        Tk = tf.shape(k)[1]
        qh = tf.reshape(self.wq(q), [B, Tq, self.num_heads, self.head_dim])
        kh = tf.reshape(self.wk(k), [B, Tk, self.num_heads, self.head_dim])
        vh = tf.reshape(self.wv(v), [B, Tk, self.num_heads, self.head_dim])
        scores = tf.einsum("bthd,bshd->bhts", qh, kh) / np.sqrt(self.head_dim)
        attn = tf.nn.softmax(scores, axis=-1)               # 无因果掩码（R3）
        ctx = tf.einsum("bhts,bshd->bthd", attn, vh)
        ctx = tf.reduce_mean(ctx, axis=2)                   # 头间平均（interpretable）
        return self.out(ctx)

    def get_config(self):
        cfg = super().get_config()
        cfg.update({"units": self.units, "num_heads": self.num_heads})
        return cfg

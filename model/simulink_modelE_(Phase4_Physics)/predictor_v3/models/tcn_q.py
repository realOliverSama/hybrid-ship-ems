# -*- coding: utf-8 -*-
"""models/tcn_q.py — TCN-Q（T-009；DGN-E-07(b)）。

冻结结构：hist → 输入投影 Conv1D(C,1) → 6 残差块{dilated Conv1D(C,k=3,d) +
LayerNorm + GELU + Dropout + Add}，d∈(1,2,4,8,16,32) → 末时刻切片 ⊕ 注意力池化
concat → Dense(256,GELU) → Dropout → Dense(90) → reshape[30,3] → 共用
MonotonicQuantileTransform。H1-only；≤1.5M（容量过滤前置，DGN-E-08 ③）。

trial config 键：{"C": {32|64|96}, "dilations": "6b"|"7b", "dropout": {0|0.1},
"lr": {3e-4|1e-3}, "batch": 256}；dilations 6b=(1,2,4,8,16,32)、7b=(1,2,4,8,16,32,64)。
padding=causal（TCN 标准时间因果语义；model_config 记录）。
"""
from __future__ import annotations

MODEL_ID = "tcnq"
ARCH_ID = "tcnq"
FEATURE_SURFACE = "H1"
ENVELOPE = (0, 1_500_000)              # ≤1.5M（FR-587）

DILATIONS_6B = (1, 2, 4, 8, 16, 32)
DILATIONS_7B = (1, 2, 4, 8, 16, 32, 64)


def build(cfg: dict, s0: float):
    import numpy as np
    import tensorflow as tf
    from tensorflow.keras import layers
    from predictor_v3.models.common import (LearnedQueryAttentionPooling,
                                            quantile_head)

    c = cfg["C"]
    dil = DILATIONS_6B if cfg["dilations"] == "6b" else DILATIONS_7B
    drop = cfg["dropout"]

    inp = tf.keras.Input(shape=(300, 11), name="hist_input")
    x = layers.Conv1D(c, 1, padding="same", name="input_proj")(inp)
    for i, d in enumerate(dil):
        y = layers.Conv1D(c, 3, padding="causal", dilation_rate=d,
                          name=f"dilated_{i}")(x)
        y = layers.LayerNormalization(name=f"ln_{i}")(y)
        y = layers.Activation("gelu", name=f"gelu_{i}")(y)
        if drop:
            y = layers.Dropout(drop, name=f"drop_{i}")(y)
        x = layers.Add(name=f"add_{i}")([x, y])
    last = x[:, -1, :]                                   # 末时刻切片
    pool = LearnedQueryAttentionPooling(name="att_pool")(x)
    z = layers.Concatenate(name="concat")([last, pool])  # [B, 2C]
    z = layers.Dense(256, activation="gelu", name="head_dense")(z)
    if drop:
        z = layers.Dropout(drop, name="head_drop")(z)
    raw = layers.Dense(
        90, bias_initializer=tf.keras.initializers.Constant(
            np.tile([0.0, _sp_inv(s0), _sp_inv(s0)], 30)),
        name="raw_proj")(z)
    out = quantile_head(raw, s0)
    return tf.keras.Model(inp, out, name="tcn_q")


def _sp_inv(s: float) -> float:
    from predictor_v3.quantile import softplus_inv
    return softplus_inv(s)


SEARCH_SPACE = {
    "C": [32, 64, 96],
    "dilations": ["6b", "7b"],
    "dropout": [0.0, 0.1],
    "lr": [3e-4, 1e-3],
    "batch": [256],
}

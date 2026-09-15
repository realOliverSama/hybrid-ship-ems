# -*- coding: utf-8 -*-
"""models/mlp_q.py — MLP-Q-v3（T-008；DGN-E-07(a)，FR-585）。

冻结结构：hist[B,300,11] → Flatten(3300) → Dense(64,ReLU) → Dense(32,ReLU)
→ Dense(90) → reshape[30,3]=(m,a,b) → 共用 MonotonicQuantileTransform（≈0.2M）。
H1-only；不继承 v2 任何资产；不定义 predictor_runtime_period_s（FR-578）。

trial config 键（DGN-E-08 冻结空间）：{"lr": {5e-4|1e-3}, "batch": {256|512}}
（hidden 64/32 固定，无自由度）。
"""
from __future__ import annotations

MODEL_ID = "mlpq"
ARCH_ID = "mlpq"
FEATURE_SURFACE = "H1"
ENVELOPE = (150_000, 300_000)          # ≈0.2M 近固定（FR-587；smoke 断言窗）


def build(cfg: dict, s0: float):
    import numpy as np
    import tensorflow as tf
    from tensorflow.keras import layers
    from predictor_v3.models.common import quantile_head

    hidden = (cfg.get("hidden1", 64), cfg.get("hidden2", 32))
    inp = tf.keras.Input(shape=(300, 11), name="hist_input")
    x = layers.Flatten()(inp)
    x = layers.Dense(hidden[0], activation="relu")(x)
    x = layers.Dense(hidden[1], activation="relu")(x)
    raw = layers.Dense(
        90, bias_initializer=tf.keras.initializers.Constant(
            np.tile([0.0, _sp_inv(s0), _sp_inv(s0)], 30)))(x)
    out = quantile_head(raw, s0)
    return tf.keras.Model(inp, out, name="mlp_q_v3")


def _sp_inv(s: float) -> float:
    from predictor_v3.quantile import softplus_inv
    return softplus_inv(s)


SEARCH_SPACE = {
    "lr": [5e-4, 1e-3],
    "batch": [256, 512],
    "hidden1": [64],
    "hidden2": [32],
}

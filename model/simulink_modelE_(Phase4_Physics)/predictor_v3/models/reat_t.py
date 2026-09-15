# -*- coding: utf-8 -*-
"""models/reat_t.py — REAT-T（T-013；DGN-E-07(e) 的实现，机器契约 = reat_t_arch.json）。

结构（转录，非再设计）：
  Past branch（TCN）：hist → Conv1D(Cp,1,same) → 6 残差块{causal dilated Conv1D(Cp,3)
    → LayerNorm → GELU → Dropout → Add}，d∈(1,2,4,8,16,32) → h_past[B,Cp]（learned
    query 注意力池化）
  Future branch：kf → Dense(df) + LearnedPositionalEncoding(30,df) → PreLN Encoder×Nf
    （MHA h=4 头 + FFN 4df；30 位合法 K/F 全互见，无因果掩码——R3）
  Fusion：h_past tile[B,30,Cp] ⊕ fut_seq concat → LayerNorm → Dense(df,GELU) → Dropout
  Decoder：Dense(d_dec,GELU) → Dropout → Dense(3)=(m,a,b) → MonotonicQuantileTransform

消融臂（R2 架构保持）：同拓扑同参数量；信息面差异 = 数据侧归一化零通道替换，
mask 由 ABLATION_FACES 冻结记录；禁新增运行时 mask 张量。
"""
from __future__ import annotations

import numpy as np
import tensorflow as tf
import keras

MODEL_ID = "reatt"
ARCH_ID = "reatt"
FEATURE_SURFACE = "H1+K+F"
ENVELOPE = (500_000, 3_000_000)        # 0.5~3.0M（FR-587）

NUM_HEADS = 4
DILATIONS = (1, 2, 4, 8, 16, 32)

# R2 消融臂（架构保持的归一化零通道替换；4K + 2F 语义）
KF_CHANNELS = ["v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW",
               "u_fcst_mps", "wl_fcst_m"]
K_CHANNELS = ["v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW"]
F_CHANNELS = ["u_fcst_mps", "wl_fcst_m"]

ABLATION_FACES = {
    "reatt_abl_h1": {"zero_kf_channels": list(KF_CHANNELS)},
    "reatt_abl_h1k": {"zero_kf_channels": list(F_CHANNELS)},
    "reatt_abl_h1kf": {"zero_kf_channels": [], "reuse": "正式 REAT-T 候选运行"},
}


def ablation_zero_channels(face: str) -> list:
    """消融零替换通道清单（数据侧 transform 后置 0；模型拓扑不变）。"""
    if face not in ABLATION_FACES:
        raise KeyError(f"未知消融面 {face}（{sorted(ABLATION_FACES)}）")
    return list(ABLATION_FACES[face]["zero_kf_channels"])


def _sp_inv(s: float) -> float:
    from predictor_v3.quantile import softplus_inv
    return softplus_inv(s)


@keras.saving.register_keras_serializable(package="predictor_v3")
class TilePast(tf.keras.layers.Layer):
    """h_past [B,Cp] → [B,n_lead,Cp]（Fusion broadcast；R3/DGN-E-07(e)）。"""

    def __init__(self, n_lead=30, **kwargs):
        super().__init__(**kwargs)
        self.n_lead = n_lead

    def call(self, t):
        import tensorflow as tf
        return tf.tile(t[:, None, :], [1, self.n_lead, 1])

    def get_config(self):
        cfg = super().get_config()
        cfg["n_lead"] = self.n_lead
        return cfg


def build(cfg: dict, s0: float):
    import tensorflow as tf
    from tensorflow.keras import layers
    from predictor_v3.models.common import (LearnedPositionalEncoding,
                                            LearnedQueryAttentionPooling,
                                            quantile_head)
    cp = cfg["Cp"]
    df = cfg["df"]
    nf = cfg["Nf"]
    d_dec = cfg["d_dec"]
    drop = cfg["dropout"]

    hist = tf.keras.Input(shape=(300, 11), name="hist_input")
    kf = tf.keras.Input(shape=(30, 6), name="kf_input")

    # ---- Past branch（TCN）----
    x = layers.Conv1D(cp, 1, padding="same", name="past_proj")(hist)
    for i, d in enumerate(DILATIONS):
        y = layers.Conv1D(cp, 3, padding="causal", dilation_rate=d,
                          name=f"past_dil{i}")(x)
        y = layers.LayerNormalization(name=f"past_ln{i}")(y)
        y = layers.Activation("gelu", name=f"past_gelu{i}")(y)
        if drop:
            y = layers.Dropout(drop, name=f"past_drop{i}")(y)
        x = layers.Add(name=f"past_add{i}")([x, y])
    h_past = LearnedQueryAttentionPooling(name="past_pool")(x)      # [B,Cp]

    # ---- Future branch（Transformer，PreLN，FFN 4df，无因果掩码）----
    fut = layers.Dense(df, name="fut_proj")(kf)
    fut = LearnedPositionalEncoding(30, df, name="lead_pos")(fut)
    for i in range(nf):
        n = layers.LayerNormalization(name=f"fut{i}_ln1")(fut)
        a = layers.MultiHeadAttention(NUM_HEADS, df // NUM_HEADS,
                                      name=f"fut{i}_mha")(n, n)
        if drop:
            a = layers.Dropout(drop, name=f"fut{i}_d1")(a)
        fut = layers.Add(name=f"fut{i}_add1")([fut, a])
        n = layers.LayerNormalization(name=f"fut{i}_ln2")(fut)
        f = layers.Dense(4 * df, activation="gelu", name=f"fut{i}_ffn1")(n)
        f = layers.Dense(df, name=f"fut{i}_ffn2")(f)
        if drop:
            f = layers.Dropout(drop, name=f"fut{i}_d2")(f)
        fut = layers.Add(name=f"fut{i}_add2")([fut, f])
    fut_seq = fut                                                    # [B,30,df]

    # ---- Fusion ----
    h_tile = layers.Lambda(lambda t: tf.tile(t[:, None, :], [1, 30, 1]),
                           name="h_tile")(h_past) if False else \
        _tile_past(h_past, 30)
    fused = layers.Concatenate(name="fusion_cat")([fut_seq, h_tile])  # [B,30,Cp+df]
    fused = layers.LayerNormalization(name="fusion_ln")(fused)
    fused = layers.Dense(df, activation="gelu", name="fusion_dense")(fused)
    if drop:
        fused = layers.Dropout(drop, name="fusion_drop")(fused)

    # ---- Decoder（per-lead 共享）----
    z = layers.Dense(d_dec, activation="gelu", name="dec_dense")(fused)
    if drop:
        z = layers.Dropout(drop, name="dec_drop")(z)
    raw = layers.Dense(
        3, bias_initializer=tf.keras.initializers.Constant(
            np.array([0.0, _sp_inv(s0), _sp_inv(s0)])),
        name="raw_proj")(z)
    out = quantile_head(raw, s0, n_lead=1)
    return tf.keras.Model([hist, kf], out, name="reat_t")


def _tile_past(h_past, n_lead: int = 30):
    """h_past [B,Cp] → [B,n_lead,Cp]（模块级可序列化层）。"""
    return TilePast(n_lead, name="h_tile")(h_past)


SEARCH_SPACE = {
    "Cp": [64, 96, 128],
    "df": [64, 128],
    "Nf": [2, 3, 4],
    "d_dec": [64, 128],
    "dropout": [0.0, 0.1],
    "lr": [1e-4, 3e-4, 1e-3],
    "batch": [128],
}
ANCHOR = {"Cp": 96, "df": 128, "Nf": 3, "d_dec": 128,
          "dropout": 0.1, "lr": 3e-4, "batch": 128}

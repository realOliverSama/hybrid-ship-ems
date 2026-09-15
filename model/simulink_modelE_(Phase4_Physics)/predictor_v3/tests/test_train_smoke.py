# -*- coding: utf-8 -*-
"""T-005 单测：训练治理链 smoke（DGN-E-09——非 result-bearing，全合成数据）。"""
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import train as tr  # noqa: E402
from predictor_v3.train import MONITOR_KEY  # noqa: E402


def _tiny_data(n=64, with_kf=True):
    rng = np.random.default_rng(7)
    hist = rng.normal(0, 1, size=(n, 300, 11)).astype(np.float32)
    kf = rng.normal(0, 1, size=(n, 30, 6)).astype(np.float32) if with_kf else None
    y = np.abs(rng.normal(300, 30, size=(n, 30))).astype(np.float32)
    groups = np.array([f"G{i % 4}" for i in range(n)])
    return {"hist": hist, "kf": kf, "y": y, "voyage_group": groups}


def _tiny_model(with_kf=True):
    import tensorflow as tf
    from predictor_v3.quantile import (build_monotonic_quantile_transform,
                                       quantile_bias_init)
    inp = tf.keras.Input(shape=(300, 11), name="hist_input")
    x = tf.keras.layers.Flatten()(inp)
    if with_kf:
        inp2 = tf.keras.Input(shape=(30, 6), name="kf_input")
        x2 = tf.keras.layers.Flatten()(inp2)
        x = tf.keras.layers.Concatenate()([x, x2])
    h = tf.keras.layers.Dense(16, activation="relu")(x)
    raw = tf.keras.layers.Dense(
        90, bias_initializer=tf.keras.initializers.Constant(
            np.tile(quantile_bias_init(30.0), 30)))(h)
    out = build_monotonic_quantile_transform()(
        tf.keras.layers.Reshape((30, 3))(raw))
    model = tf.keras.Model([inp, inp2] if with_kf else inp, out)
    return model


def test_budget_config_loads():
    b = tr.load_budget()
    assert b["max_epochs"] == 100 and b["early_stopping"]["patience"] == 10
    assert b["early_stopping"]["monitor"] == MONITOR_KEY
    assert b["optimizer"] == "Adam" and b["grad_clip_norm"] == 1.0
    assert b["hpo"]["budget_caps"]["mlpq"] == 5
    assert b["seeds"]["confirmation_seed_indices"] == [0, 1, 2]


def test_train_smoke_monitor_key_in_history():
    """合成小模型 2 epoch：val_sgmacro_pinball 进 logs（与主 HPO 目标同口径）。"""
    tf = pytest.importorskip("tensorflow")
    budget = dict(tr.load_budget())
    budget["max_epochs"] = 2
    budget["es_patience"] = 10
    data = _tiny_data()
    model = _tiny_model(with_kf=True)
    out = tr.train_candidate(model, data, data, lr=1e-3, batch_size=16,
                             shuffle_seed=42, budget=budget, verbose=0)
    assert MONITOR_KEY in out["history"]
    vals = out["history"][MONITOR_KEY]
    assert len(vals) >= 1 and all(np.isfinite(v) for v in vals)
    assert out["best_val_sgmacro_pinball"] == min(vals)
    # 输出面契约：[B,30,3]
    pred = model.predict({"hist_input": data["hist"],
                          "kf_input": data["kf"]}, verbose=0)
    assert pred.shape == (64, 30, 3)

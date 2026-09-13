# -*- coding: utf-8 -*-
"""train.py — 统一训练循环/回调/预算治理（T-005，DGN-E-09；FR-597）。

冻结数值（training_budget.json 同源）：
- max_epochs=100；EarlyStopping(monitor=val split_group_macro pinball，
  patience=10，restore_best_weights=True)；ReduceLROnPlateau(factor=0.5，
  patience=5，min_lr=1e-5)；optimizer=Adam；gradient clip global_norm=1.0；
- 数据管道：tf.data.from_tensor_slices → shuffle(buffer=50000，per-run 派生种子)
  → batch → prefetch；full float32（不开 mixed precision）；
- 训练目标 kW 域 pinball（DGN-E-06）；
- val 监控 = val split_group_macro pinball（与主 HPO 目标同口径，FR-595——
  不得静默优化 window-pooled 口径）；
- lr/batch 一律由 DGN-E-08 冻结空间给出（无空间外默认值）。

本模块不实现具体模型（E1/E2 职责）；不执行任何训练（T-005 Non-scope）。
"""
from __future__ import annotations

import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
BUDGET_PATH = os.path.join(MODELE_ROOT, "config", "predictor_v3",
                           "training_budget.json")

MONITOR_KEY = "val_sgmacro_pinball"
SHUFFLE_BUFFER = 50000
PREFETCH = 4


def load_budget(path: str = BUDGET_PATH) -> dict:
    import json
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def make_dataset(hist: np.ndarray, kf: np.ndarray | None, y: np.ndarray,
                 batch_size: int, shuffle_seed: int) -> "tf.data.Dataset":
    """窗口感知训练数据管道（window-uniform；DGN-E-09 冻结管道形态）。"""
    import tensorflow as tf
    xs = {"hist_input": hist.astype(np.float32)}
    if kf is not None:
        xs["kf_input"] = kf.astype(np.float32)
    ds = tf.data.Dataset.from_tensor_slices((xs, y.astype(np.float32)))
    ds = ds.shuffle(buffer_size=SHUFFLE_BUFFER,
                    seed=shuffle_seed & 0x7FFFFFFF,
                    reshuffle_each_iteration=True)
    ds = ds.batch(batch_size).prefetch(PREFETCH)
    return ds


def build_callbacks(val_data: dict, split_group_of: dict,
                    budget: dict | None = None) -> list:
    """构造冻结回调链：自定义 sg_macro pinball 监控 → EarlyStopping → ReduceLR。

    val_data : {"hist": [N,300,11], "kf": [N,30,6]|None, "y": [N,30],
                "voyage_group": [N] 每窗组 id 字符串数组}
    split_group_of : 未直接使用（voyage_group 已含每窗组 id），保留签名兼容。
    返回 callbacks 列表（自定义监控必须在 EarlyStopping 之前——logs 注入顺序）。
    """
    import tensorflow as tf

    budget = budget or load_budget()

    class _ValSGMacroPinball(tf.keras.callbacks.Callback):
        """每 epoch 计算 val split_group_macro pinball 并注入 logs。"""

        def __init__(self, val_data):
            super().__init__()
            self._hist = np.asarray(val_data["hist"], dtype=np.float32)
            self._kf = (np.asarray(val_data["kf"], dtype=np.float32)
                        if val_data.get("kf") is not None else None)
            self._y = np.asarray(val_data["y"], dtype=np.float32)
            self._groups = np.asarray(val_data["voyage_group"])
            self._uniq = list(dict.fromkeys(self._groups.tolist()))

        def on_epoch_end(self, epoch, logs=None):
            logs = logs or {}
            inputs = {"hist_input": self._hist}
            if self._kf is not None:
                inputs["kf_input"] = self._kf
            pred = self.model.predict(inputs, verbose=0)
            # per-group pinball（组内窗口均值）→ 组均值 = sg_macro
            from predictor_v3.quantile import pinball_numpy
            pb = pinball_numpy(self._y, np.asarray(pred, dtype=np.float64))
            per_group = [float(pb[self._groups == g].mean())
                         for g in self._uniq]
            logs[MONITOR_KEY] = float(np.mean(per_group))

    cb = [_ValSGMacroPinball(val_data)]
    cb.append(tf.keras.callbacks.EarlyStopping(
        monitor=MONITOR_KEY, mode="min",
        patience=budget["early_stopping"]["patience"],
        restore_best_weights=budget["early_stopping"]["restore_best_weights"]))
    cb.append(tf.keras.callbacks.ReduceLROnPlateau(
        monitor=MONITOR_KEY, mode="min",
        factor=budget["reduce_lr_on_plateau"]["factor"],
        patience=budget["reduce_lr_on_plateau"]["patience"],
        min_lr=budget["reduce_lr_on_plateau"]["min_lr"]))
    return cb


def compile_model(model, lr: float, budget: dict | None = None):
    """Adam（global_clipnorm 属 optimizer 参数）+ kW 域 pinball（DGN-E-09）。"""
    import tensorflow as tf
    from predictor_v3.quantile import pinball_loss
    budget = budget or load_budget()
    assert budget["optimizer"] == "Adam"
    optimizer = tf.keras.optimizers.Adam(
        learning_rate=lr, global_clipnorm=budget["grad_clip_norm"])
    model.compile(optimizer=optimizer, loss=pinball_loss)
    return model


def train_candidate(model, train_data: dict, val_data: dict, *,
                    lr: float, batch_size: int, shuffle_seed: int,
                    budget: dict | None = None, verbose: int = 0) -> dict:
    """通用训练入口（模型由 E1/E2 注入；本函数只治理循环/回调/管道）。

    train_data/val_data: {"hist","kf","y","voyage_group"}（raw transform 后特征）。
    返回 {"history": ..., "best_val_sgmacro_pinball": ...}。
    """
    import tensorflow as tf
    budget = budget or load_budget()
    compile_model(model, lr, budget)
    ds = make_dataset(train_data["hist"],
                      train_data.get("kf"),
                      train_data["y"], batch_size, shuffle_seed)
    callbacks = build_callbacks(val_data, {}, budget)
    hist = model.fit(ds, epochs=budget["max_epochs"], verbose=verbose,
                     callbacks=callbacks)
    return {"history": {k: [float(v) for v in vs]
                        for k, vs in hist.history.items()},
            "best_val_sgmacro_pinball":
                min(hist.history.get(MONITOR_KEY, [float("inf")]))}


def _tf_unused():
    """避免顶部 import tensorflow（仅 train 函数需要）；本函数不应被调用。"""
    raise NotImplementedError

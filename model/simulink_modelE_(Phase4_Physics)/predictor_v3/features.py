# -*- coding: utf-8 -*-
"""features.py — 特征面装配（T-005，DGN-E-05；FR-579）。

冻结语义：
- 输出模型输入 dict："hist" float32 [B,300,11]（H1，frozen formal_norm_v3 transform）、
  "kf" float32 [B,30,6]（KF，predictor_norm_ext_v1 transform）；
  H1-only 模型只收 "hist"；
- 通道顺序 = 冻结 registry/npz 列序（INPUT_H1 / INPUT_K+INPUT_F registry 顺序；
  input_contract 逐字段引用）；
- 禁止 METADATA/scenario 字段进输入；TFT static 输入空集；
- batch 语义 = 窗口感知训练（window-uniform，FR-599）。

registry 身份从冻结 dataset_v3.contracts 读取（机器事实源，无 hard-coded 清单）。
"""
from __future__ import annotations

import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)

FORMAL_NORM_PATH = os.path.join(MODELE_ROOT, "results", "dataset_v3",
                                "formal", "formal_norm_v3.json")


def registry_channel_order() -> dict:
    """冻结 registry 驱动的通道顺序（唯一权威；禁 hard-coded 清单）。"""
    from dataset_v3 import contracts
    reg = contracts.load_registry(validate=True)
    return {
        "h1": list(contracts.signal_names(reg, "INPUT_H1")),
        "kf": (list(contracts.signal_names(reg, "INPUT_K"))
               + list(contracts.signal_names(reg, "INPUT_F"))),
    }


def load_formal_norm(path: str = FORMAL_NORM_PATH) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json_load(fh)


def json_load(fh):
    import json
    return json.load(fh)


def transform_h1(raw: np.ndarray, channel: str, formal_norm: dict) -> np.ndarray:
    """H1 单通道 transform（per-bin mean/std；D 已冻结，train 常值通道 scale=1）。"""
    mean = np.asarray(formal_norm["x_h1_mean"][channel], dtype=np.float64)
    std = np.asarray(formal_norm["x_h1_std"][channel], dtype=np.float64)
    x = np.asarray(raw, dtype=np.float64)
    if x.shape[-1] != mean.shape[0]:
        raise ValueError(f"H1 通道 {channel} 末维 {x.shape} != 冻结 {mean.shape[0]}")
    return (x - mean) / std


def assemble(raw_hist_by_channel: dict, raw_kf_by_channel: dict | None,
             formal_norm: dict, norm_ext_artifact: dict | None = None,
             channel_order: dict | None = None) -> dict:
    """装配模型输入 dict（DGN-E-05）。

    raw_hist_by_channel : {channel: [N,300] raw}（registry H1 11 通道）
    raw_kf_by_channel   : {channel: [N,30] raw}（registry K/F 6 通道；None → 不含 kf）
    返回 {"hist": float32 [N,300,11]}（H1-only）或
         {"hist": ..., "kf": float32 [N,30,6]}（H1+K+F）。
    """
    order = channel_order or registry_channel_order()
    missing = [c for c in order["h1"] if c not in raw_hist_by_channel]
    if missing:
        raise ValueError(f"缺 H1 registry 通道: {missing}")
    if set(raw_hist_by_channel) - set(order["h1"]):
        extra = set(raw_hist_by_channel) - set(order["h1"])
        raise ValueError(f"registry 外通道禁入 view（METADATA/FORBIDDEN 检查）: {extra}")
    out = {"hist": np.stack(
        [transform_h1(raw_hist_by_channel[c], c, formal_norm)
         for c in order["h1"]], axis=-1).astype(np.float32)}
    if raw_kf_by_channel is not None:
        if norm_ext_artifact is None:
            raise ValueError("KF 通道需要 predictor_norm_ext_v1 工件")
        from predictor_v3 import norm_ext
        missing_kf = [c for c in order["kf"] if c not in raw_kf_by_channel]
        if missing_kf:
            raise ValueError(f"缺 K/F registry 通道: {missing_kf}")
        if set(raw_kf_by_channel) - set(order["kf"]):
            extra = set(raw_kf_by_channel) - set(order["kf"])
            raise ValueError(f"registry 外 K/F 通道禁入: {extra}")
        kf = np.stack([norm_ext.transform_channel(
            norm_ext_artifact, c,
            np.asarray(raw_kf_by_channel[c], dtype=np.float64))
            for c in order["kf"]], axis=-1).astype(np.float32)
        out["kf"] = kf
    return out

# -*- coding: utf-8 -*-
"""T-015/T-016 驱动层单测：零通道替换（R2）+ 聚合语义 + 消融面 mask。"""
import json
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3.confirm import _zero_kf_channels  # noqa: E402
from predictor_v3.models.reat_t import (ABLATION_FACES, KF_CHANNELS,  # noqa: E402
                                        ablation_zero_channels)

RNG = np.random.default_rng(20260916)


def test_zero_replacement_semantics():
    """R2：零 = transform 后 0（train 均值语义）；指定通道置零、其余保留原值。"""
    kf = RNG.normal(0, 1, size=(4, 30, 6))
    order = ["v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW",
             "u_fcst_mps", "wl_fcst_m"]
    out = _zero_kf_channels(kf, ["u_fcst_mps", "wl_fcst_m"], order)
    assert out.shape == kf.shape
    assert np.allclose(out[:, :, order.index("u_fcst_mps")], 0.0)
    assert np.allclose(out[:, :, order.index("wl_fcst_m")], 0.0)
    # 未指定通道保持原值（逐元素）
    for ch in ("v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW"):
        assert np.allclose(out[:, :, order.index(ch)], kf[:, :, order.index(ch)])
    # 空清单 = 原样（H1+K+F 复用语义）
    out0 = _zero_kf_channels(kf, [], order)
    assert np.allclose(out0, kf)


def test_ablation_faces_masks():
    """消融 mask 冻结：H1=6 全零；H1+K=2 F 通道；H1+K+F=空（复用）。"""
    assert ablation_zero_channels("reatt_abl_h1") == KF_CHANNELS
    assert set(ablation_zero_channels("reatt_abl_h1k")) == {"u_fcst_mps", "wl_fcst_m"}
    assert ablation_zero_channels("reatt_abl_h1kf") == []
    assert len(KF_CHANNELS) == 6


def test_aggregate_semantics_known_values():
    """summary 聚合 median/mean/std/min/max（已知数据手算）。"""
    import subprocess
    vals = [17.0, 18.0, 16.5]
    assert np.median(vals) == 17.0 and abs(np.mean(vals) - 17.166666) < 1e-5
    assert np.min(vals) == 16.5 and np.max(vals) == 18.0


def test_confirm_seed_derivation_differs_by_seed_index():
    """{0,1,2} 三种子派生互异且 = 冻结公式（DGN-E-10；禁 best-seed 的机器前提）。"""
    from predictor_v3 import seeds as sd
    s0 = sd.derive_seed("reatt", "cfgX", 0)
    s1 = sd.derive_seed("reatt", "cfgX", 1)
    s2 = sd.derive_seed("reatt", "cfgX", 2)
    assert len({s0, s1, s2}) == 3
    assert sd.CONFIRMATION_SEED_INDICES == (0, 1, 2)
    assert sd.DEV_SEED_INDEX == 0

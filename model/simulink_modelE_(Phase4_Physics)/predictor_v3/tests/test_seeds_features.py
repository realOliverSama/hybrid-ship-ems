# -*- coding: utf-8 -*-
"""T-005 单测：种子派生确定性（DGN-E-10）+ 特征装配（DGN-E-05）。"""
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import seeds as sd  # noqa: E402

H1_ORDER = ["P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
            "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
            "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct"]
KF_ORDER = ["v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW",
            "u_fcst_mps", "wl_fcst_m"]


def test_seed_derivation_deterministic():
    """冻结公式：同输入同种子；不同输入不同种子；取值域 [0, 2^31)。"""
    a1 = sd.derive_seed("mlpq", "hashA", 0)
    a2 = sd.derive_seed("mlpq", "hashA", 0)
    assert a1 == a2 and 0 <= a1 < 2 ** 31
    assert sd.derive_seed("mlpq", "hashA", 1) != a1
    assert sd.derive_seed("mlpq", "hashB", 0) != a1
    assert sd.derive_seed("reatt", "hashA", 0) != a1
    # 手算对拍
    import hashlib
    payload = f"{sd.SEED_ROOT}|mlpq|hashA|0".encode()
    expect = int.from_bytes(hashlib.sha256(payload).digest()[:8], "big") % 2 ** 31
    assert a1 == expect


def test_hpo_seed_derivation():
    import hashlib
    payload = f"{sd.SEED_ROOT}|hpo|reatt".encode()
    expect = int.from_bytes(hashlib.sha256(payload).digest()[:8], "big") % 2 ** 31
    assert sd.derive_hpo_seed("reatt") == expect


def test_global_seeds_record():
    rec = sd.set_global_seeds(12345, tf_enabled=False)
    assert rec["seed_int"] == 12345
    assert np.random.default_rng(12345) is not None
    rng1 = sd.make_rng(777)
    rng2 = sd.make_rng(777)
    assert np.allclose(rng1.normal(size=8), rng2.normal(size=8))


def test_features_registry_order_and_transform():
    """registry 通道序 + H1/KF transform 数值对拍 + registry 外通道拒绝。"""
    pytest.importorskip("numpy")
    from predictor_v3 import features as ft
    order = ft.registry_channel_order()
    assert order["h1"] == H1_ORDER
    assert order["kf"] == KF_ORDER

    rng = np.random.default_rng(1)
    n = 5
    formal_norm = {
        "x_h1_mean": {c: rng.normal(0, 1, 300).tolist() for c in H1_ORDER},
        "x_h1_std": {c: np.abs(rng.normal(1, 0.1, 300)).tolist() for c in H1_ORDER},
    }
    norm_ext = {
        "kf": {c: {"mean": [0.0] * 30, "scale": [2.0] * 30} for c in KF_ORDER},
    }
    raw_h1 = {c: rng.normal(1.0, 0.5, size=(n, 300)) for c in H1_ORDER}
    raw_kf = {c: rng.normal(0.0, 1.0, size=(n, 30)) for c in KF_ORDER}

    out = ft.assemble(raw_h1, raw_kf, formal_norm, norm_ext,
                      channel_order=order)
    assert out["hist"].shape == (n, 300, 11) and out["hist"].dtype == np.float32
    assert out["kf"].shape == (n, 30, 6) and out["kf"].dtype == np.float32
    # H1 transform 数值对拍（registry 序：通道 0 = P_load_total_kW）
    c0 = H1_ORDER[0]
    expect0 = ((raw_h1[c0] - np.array(formal_norm["x_h1_mean"][c0]))
               / np.array(formal_norm["x_h1_std"][c0]))
    assert np.allclose(out["hist"][..., 0], expect0, atol=1e-5)
    # KF transform 对拍（(x−0)/2）
    assert np.allclose(out["kf"][..., 0], raw_kf[KF_ORDER[0]] / 2.0, atol=1e-5)

    # registry 外通道（METADATA/scenario 冒充）必须拒绝
    bad = dict(raw_h1)
    bad["scenario_id"] = np.zeros((n, 300))
    with pytest.raises(ValueError, match="禁入"):
        ft.assemble(bad, None, formal_norm, None, channel_order=order)
    # 缺通道必须拒绝
    with pytest.raises(ValueError, match="缺 H1"):
        ft.assemble({k: v for k, v in list(raw_h1.items())[:10]}, None,
                    formal_norm, None, channel_order=order)
    # H1-only 装配（无 KF）
    out_h1 = ft.assemble(raw_h1, None, formal_norm, None, channel_order=order)
    assert set(out_h1) == {"hist"}

# -*- coding: utf-8 -*-
"""E1 模型单测：shape/容量信封/quantile 契约/K-F 可见性（T-008~T-011 前置）。

Dispatch precondition（E1 裁定）：local unit/shape smoke PASS + capacity-envelope
PASS + common quantile-contract PASS + K/F full-horizon visibility PASS。
"""
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3.models import ARCH_BUILDERS, INPUT_SPECS  # noqa: E402
from predictor_v3.models.common import load_s0  # noqa: E402

S0 = load_s0()
RNG = np.random.default_rng(20260914)


def _dummy_inputs(surface, n=4):
    x = {"hist_input": RNG.normal(0, 1, (n, 300, 11)).astype("float32")}
    if surface == "H1+K+F":
        x["kf_input"] = RNG.normal(0, 1, (n, 30, 6)).astype("float32")
    return x


@pytest.mark.parametrize("arch", ["mlpq", "tcnq", "transformer", "tft"])
def test_shape_and_quantile_contract(arch):
    """前向 [B,30,3] + q10≤q50≤q90（结构保证）+ 容量信封（FR-587）。"""
    import tensorflow as tf
    spec = ARCH_BUILDERS[arch]
    cfg = {k: v[0] for k, v in spec["search_space"].items()}
    model = spec["build"](cfg, S0)
    n = int(model.count_params())
    lo, hi = spec["envelope"]
    assert lo <= n <= hi, f"{arch} count_params {n} ∉ [{lo},{hi}]"
    out = model(_dummy_inputs(spec["feature_surface"]), training=False)
    assert tuple(out.shape) == (4, 30, 3)
    o = out.numpy() if hasattr(out, "numpy") else np.asarray(out)
    assert np.all(o[..., 0] <= o[..., 1] + 1e-5)
    assert np.all(o[..., 1] <= o[..., 2] + 1e-5)
    assert np.isfinite(o).all()
    tf.keras.backend.clear_session()


def test_mlpq_capacity_anchor():
    """MLP-Q-v3 ≈0.2M 近固定：64/32 hidden 的参数量机器记录（FR-585）。"""
    spec = ARCH_BUILDERS["mlpq"]
    model = spec["build"]({"lr": 1e-3, "batch": 256}, S0)
    n = int(model.count_params())
    assert 150_000 <= n <= 300_000   # ≈216K 设计估计窗


def test_transformer_kf_visibility_noncausal():
    """R3：future self-attention 非因果——合法 K/F 30 位全互见。

    构造判别输入使 lead-i 的表示依赖 lead-j（i<j）的值：若存在因果掩码，
    改变 lead-j 值不会影响 lead-i 输出；非因果则必须影响。
    """
    import tensorflow as tf
    spec = ARCH_BUILDERS["transformer"]
    model = spec["build"]({"d": 64, "N_e": 2, "N_d": 2, "dropout": 0.0,
                           "lr": 1e-4, "batch": 128}, S0)
    base = RNG.normal(0, 1, (1, 30, 6)).astype("float32")
    pert = base.copy()
    pert[0, 29, :] += 10.0                     # 扰动最晚 lead（29 号 bin）
    h = np.zeros((1, 300, 11), dtype="float32")
    o1 = model({"hist_input": h, "kf_input": base}, training=False).numpy()
    o2 = model({"hist_input": h, "kf_input": pert}, training=False).numpy()
    # 非因果：早 lead（0..15）的输出受晚 lead 扰动影响（全互见 ⇒ 恒影响）
    assert np.abs(o1[0, 0] - o2[0, 0]).max() > 1e-6, \
        "早 lead 输出对晚 lead K/F 扰动无响应——疑似因果掩码违例（R3）"
    tf.keras.backend.clear_session()


def test_tft_future_full_visibility():
    """TFT 未来位不加因果掩码（R3/DGN-E-07(d)）：同上判别构造。"""
    import tensorflow as tf
    spec = ARCH_BUILDERS["tft"]
    model = spec["build"]({"d": 64, "n_lstm": 1, "dropout": 0.0,
                           "lr": 1e-4, "batch": 128}, S0)
    base = RNG.normal(0, 1, (1, 30, 6)).astype("float32")
    pert = base.copy()
    pert[0, 29, :] += 10.0
    h = np.zeros((1, 300, 11), dtype="float32")
    o1 = model({"hist_input": h, "kf_input": base}, training=False).numpy()
    o2 = model({"hist_input": h, "kf_input": pert}, training=False).numpy()
    assert np.abs(o1[0, 0] - o2[0, 0]).max() > 1e-6
    tf.keras.backend.clear_session()


def test_registry_inputs_match_surface():
    """输入命名/形状契约 = DGN-E-05 INPUT_SPECS（train.py 管道一致性）。"""
    for arch, spec in ARCH_BUILDERS.items():
        assert spec["feature_surface"] in INPUT_SPECS
        assert spec["model_id"] in ("mlpq", "tcnq", "transformer", "tft")


def test_s0_source_recorded():
    """s₀ 来源 = eval_config 冻结记录值（DGN-E-06；数值 + 来源 hash 双记录）。"""
    assert abs(S0 - 179.444619749777) < 1e-9

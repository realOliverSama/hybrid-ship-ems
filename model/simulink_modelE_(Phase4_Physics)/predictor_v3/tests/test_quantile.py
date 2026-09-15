# -*- coding: utf-8 -*-
"""T-005 单测：quantile 头（DGN-E-06）。

- pinball 对拍参考实现（numpy 手算）；
- MonotonicQuantileTransform 单调性（随机 (m,a,b) 输入 q10≤q50≤q90 恒成立）
  + 模块内无二次投影断言（层无可训练权重、输出由 raw 唯一决定）；
- 稳定 softplus 逆（大 s0 溢出安全；naive 形式对比）；
- crossing rate 审计（单调参数化下恒 0）。
"""
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import quantile as qt  # noqa: E402

RNG = np.random.default_rng(20260913)


def test_pinball_reference():
    """pinball 对拍：numpy 手算 vs 模块实现（逐窗等价）。"""
    y = RNG.normal(300.0, 50.0, size=(16, 30))
    q = np.stack([y - np.abs(RNG.normal(0, 10, y.shape)),
                  y + RNG.normal(0, 1, y.shape),
                  y + np.abs(RNG.normal(0, 10, y.shape))], axis=-1)
    got = qt.pinball_numpy(y, q)
    manual = np.zeros(y.shape[0])
    for ti, tau in enumerate(qt.TAUS):
        u = y - q[..., ti]
        rho = np.maximum(tau * u, (tau - 1.0) * u)
        manual += rho.sum(axis=(1,))
    manual /= 30 * 3
    assert np.allclose(got, manual, rtol=1e-12)
    # TF 侧同值（1e-6 容差，float32 编译）
    tf = pytest.importorskip("tensorflow")
    loss = qt.pinball_loss(y.astype("float32"), q.astype("float32"))
    assert abs(float(loss) - got.mean()) < 1e-4


def test_transform_monotonic_and_no_projection():
    """随机 (m,a,b)：q10 ≤ q50 ≤ q90 恒成立；层无参数（禁二次投影）。"""
    tf = pytest.importorskip("tensorflow")
    tr = qt.build_monotonic_quantile_transform()
    assert len(tr.trainable_weights) == 0, "共用变换模块不得含可训练投影"
    raw = np.random.default_rng(7).normal(size=(64, 30, 3)).astype("float32") * 100
    out = tr(raw).numpy()
    assert out.shape == (64, 30, 3)
    assert np.all(out[..., 0] <= out[..., 1] + 1e-5)
    assert np.all(out[..., 1] <= out[..., 2] + 1e-5)
    # 语义：q50 = m；q10 = m − softplus(a)；q90 = m + softplus(b)
    m, a, b = raw[..., 0], raw[..., 1], raw[..., 2]
    assert np.allclose(out[..., 1], m, atol=1e-5)
    assert np.allclose(out[..., 0], m - np.logaddexp(0, a), atol=1e-3)
    assert np.allclose(out[..., 2], m + np.logaddexp(0, b), atol=1e-3)


def test_softplus_inv_stable():
    """稳定 softplus 逆：大 s0 直通不溢出；小 s0 与 naive 一致。"""
    assert qt.softplus_inv(25.0) == 25.0
    s = 0.5
    expect = float(np.log(np.exp(s) - 1.0))
    assert abs(qt.softplus_inv(s) - expect) < 1e-12
    with pytest.raises(ValueError):
        qt.softplus_inv(0.0)
    bias = qt.quantile_bias_init(50.0)
    assert bias.shape == (3,) and bias[1] == bias[2] == 50.0 and bias[0] == 0.0


def test_crossing_rate_zero_for_monotonic():
    raw = np.random.default_rng(3).normal(size=(32, 30, 3)).astype("float32")
    tf = pytest.importorskip("tensorflow")
    out = qt.build_monotonic_quantile_transform()(raw).numpy()
    assert qt.crossing_rate(out) == 0.0
    # 人为构造 crossing → 检出
    q = np.stack([np.ones((4, 30)), np.zeros((4, 30)), np.full((4, 30), -1.0)],
                 axis=-1)
    assert qt.crossing_rate(q) == 1.0


def test_keras_serialize_roundtrip(tmp_path):
    """序列化回归（T-007 事故）：保存 → 新进程语义 load_model → 输出一致。

    custom_objects 必须传类本身（工厂函数会被 Keras 3 拒绝）。
    """
    import json
    import subprocess
    import sys
    import numpy as np

    raw = np.random.default_rng(1).normal(size=(3, 30, 3)).astype("float32") * 5
    tf = pytest.importorskip("tensorflow")
    from predictor_v3.quantile import (MonotonicQuantileTransform,
                                       build_monotonic_quantile_transform)
    m = tf.keras.Sequential([tf.keras.layers.Input(shape=(30, 3)),
                             build_monotonic_quantile_transform()])
    ref = m(raw).numpy()
    p = tmp_path / "m.keras"
    m.save(p)
    # 新进程 load（custom_objects = 类）+ 输出 bitwise 对拍
    code = (
        "import sys, json, numpy as np, tensorflow as tf\n"
        f"m = tf.keras.models.load_model(r'{p}', compile=False,\n"
        "    custom_objects={'MonotonicQuantileTransform': "
        "__import__('predictor_v3.quantile', fromlist=['MonotonicQuantileTransform']).MonotonicQuantileTransform})\n"
        f"raw = np.load(r'{tmp_path / 'raw.npy'}')\n"
        "out = m(raw).numpy()\n"
        "print(json.dumps({'bitwise': out.tobytes() == np.load(r'"
        + str(tmp_path / "ref.npy") + "').tobytes()}))\n"
    )
    np.save(tmp_path / "raw.npy", raw)
    np.save(tmp_path / "ref.npy", ref)
    env = dict(os.environ, PYTHONPATH=MODELE_ROOT)
    res = subprocess.run([sys.executable, "-c", code], capture_output=True,
                         text=True, env=env)
    assert res.returncode == 0, res.stderr[-500:]
    assert json.loads(res.stdout.strip().splitlines()[-1])["bitwise"] is True

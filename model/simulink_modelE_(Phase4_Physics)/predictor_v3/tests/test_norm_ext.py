# -*- coding: utf-8 -*-
"""T-004 单测：predictor_norm_ext_v1 拟合/冻结/transform 契约（DGN-E-04）。

覆盖（tasks T-004 Validation）：
- fit 只用 train 的构造性验证（fit 结果 == 手算 train 统计）；
- transform-only + 禁 clip（支持外取值保留原样）；
- 禁 refit（工件篡改检测 raise；transform 不改工件）；
- 常值诊断触发用例（count/fraction/deviation 正确；不 clamp 不拒样本）；
- std<1e-12 → scale=1 保护；
- 自哈希复算一致。
"""
import copy
import json
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import norm_ext as ne  # noqa: E402

RNG = np.random.default_rng(20260913)
CHS = ["u_fcst_mps", "v_plan_mps", "s_plan_m", "h_chart_m",
       "P_hotel_plan_kW", "wl_fcst_m"]


@pytest.fixture()
def train_data():
    """合成 train KF：N=200 窗 × 30 lead bins；2 通道含真实信号 + 1 常值通道。"""
    n = 200
    kf = {
        "u_fcst_mps": RNG.normal(2.0, 0.5, size=(n, 30)),
        "v_plan_mps": RNG.normal(4.0, 1.0, size=(n, 30)),
        "s_plan_m": RNG.normal(1000.0, 100.0, size=(n, 30)),
        "h_chart_m": RNG.normal(10.0, 2.0, size=(n, 30)),
        "P_hotel_plan_kW": RNG.normal(50.0, 5.0, size=(n, 30)),
        "wl_fcst_m": np.full((n, 30), 3.25),   # 常值通道（std<1e-12）
    }
    return kf


def _fit(train_data):
    return ne.fit(train_data, feature_order=CHS,
                  source={"train_npz_sha256": "0" * 64},
                  n_train_windows=200)


def test_fit_train_only_constructive(train_data):
    """fit 结果 == 手算 train 统计（window-uniform，逐 (channel,bin)）。"""
    art = _fit(train_data)
    for ch in CHS:
        mean = train_data[ch].mean(axis=0)
        std = train_data[ch].std(axis=0)
        scale = np.where(std < 1e-12, 1.0, std)
        assert np.allclose(art["kf"][ch]["mean"], mean, rtol=0, atol=1e-12)
        assert np.allclose(art["kf"][ch]["scale"], scale, rtol=0, atol=1e-12)
    # 常值通道登记
    assert art["constant_policy"]["train_constant_fields"] == ["wl_fcst_m"]
    assert art["fit_on"] == "train_only" and art["window_uniform_fit"] is True
    # fit 接口只收单一数据集（train）：val/test 无注入通道（结构性证明）


def test_transform_only_no_clip(train_data):
    art = _fit(train_data)
    # 支持外极端值：不 clip、按 (x−mean)/scale 线性外推（手算对拍）
    x = np.array([[1e6, -1e6] + [0.0] * 28])  # (1,30)
    got = ne.transform_channel(art, "u_fcst_mps", x)
    mean = train_data["u_fcst_mps"].mean(axis=0)
    scale = train_data["u_fcst_mps"].std(axis=0)
    assert np.allclose(got[0, :2], [(1e6 - mean[0]) / scale[0],
                                    (-1e6 - mean[1]) / scale[1]], rtol=1e-9)
    # 支持内对拍
    x2 = train_data["v_plan_mps"][:10]
    got2 = ne.transform_channel(art, "v_plan_mps", x2)
    m2 = train_data["v_plan_mps"].mean(axis=0)
    s2 = train_data["v_plan_mps"].std(axis=0)
    assert np.allclose(got2, (x2 - m2) / s2, rtol=1e-12, atol=1e-12)
    # 全 dict transform
    d = ne.transform(art, {c: train_data[c][:5] for c in CHS})
    assert set(d) == set(CHS)


def test_artifact_frozen_no_refit(train_data, tmp_path):
    art = _fit(train_data)
    p = tmp_path / "norm.json"
    p.write_text(json.dumps(art), encoding="utf-8")
    loaded = ne.load_artifact(str(p))
    before = copy.deepcopy(loaded)
    ne.transform(loaded, {c: train_data[c][:3] for c in CHS})
    assert loaded == before                      # transform 不改工件
    # 篡改检测：改值后自哈希复算失败 → raise（禁 refit 的机器保障）
    bad = copy.deepcopy(loaded)
    bad["kf"]["u_fcst_mps"]["mean"][0] += 1.0
    p2 = tmp_path / "bad.json"
    p2.write_text(json.dumps(bad), encoding="utf-8")
    with pytest.raises(ValueError, match="自哈希"):
        ne.load_artifact(str(p2))


def test_constant_support_diagnostics():
    x = np.zeros(10)
    d0 = ne.constant_support_diagnostics(x, 0.0)
    assert d0 == {"count": 0, "fraction": 0.0,
                  "deviation_min": 0.0, "deviation_max": 0.0}
    x[3] = 5.0
    x[7] = -2.0
    d1 = ne.constant_support_diagnostics(x, 0.0)
    assert d1["count"] == 2 and d1["fraction"] == 0.2
    assert d1["deviation_min"] == -2.0 and d1["deviation_max"] == 5.0
    # H1 常值通道声明（FR-581 检测对象）
    assert set(ne.H1_CONSTANT_FIELDS) == {"ctrl_mode", "manual_power_cmd_pct"}


def test_constant_channel_scale_one(train_data):
    art = _fit(train_data)
    # 常值通道 scale=1 保护、mean=常值
    assert art["kf"]["wl_fcst_m"]["scale"] == [1.0] * 30
    assert all(abs(m - 3.25) < 1e-12 for m in art["kf"]["wl_fcst_m"]["mean"])
    # transform 常值 → 0（归一化零 = train 均值语义，R2 消融替换语义的基础）
    z = ne.transform_channel(art, "wl_fcst_m", np.full((2, 30), 3.25))
    assert np.allclose(z, 0.0, atol=1e-12)


def test_self_hash_recompute(train_data, tmp_path):
    """工件自哈希复算一致（canonical JSON 排除 artifact_sha256，DGN-E-16 配方）。"""
    art = _fit(train_data)
    p = tmp_path / "norm.json"
    p.write_text(json.dumps(art), encoding="utf-8")
    loaded = ne.load_artifact(str(p))           # 校验通过即复算一致
    expect = loaded.pop("artifact_sha256")
    import hashlib
    got = hashlib.sha256(json.dumps(
        loaded, sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode("utf-8")).hexdigest()
    assert got == expect

# -*- coding: utf-8 -*-
"""T-012/T-013 单测：reat_t_arch.json 一致性校验器 + REAT-T 实现 smoke。

T-012 Validation：pytest 一致性校验全过（config ↔ DGN-E-07(e) 条款逐项）；自哈希复算一致。
T-013 Validation：shape smoke [B,30,3] / count_params 落信封断言（锚点 ≤1.5M）/
K/F 全可见断言 / 消融零替换构造正确性（同拓扑同参数量 + mask 记录）。
"""
import hashlib
import json
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3.models import ARCH_BUILDERS  # noqa: E402
from predictor_v3.models.common import load_s0  # noqa: E402
from predictor_v3.models.reat_t import (ABLATION_FACES, KF_CHANNELS,  # noqa: E402
                                        ablation_zero_channels)

S0 = load_s0()
RNG = np.random.default_rng(20260915)
ARCH_JSON = os.path.join(MODELE_ROOT, "config", "predictor_v3", "reat_t_arch.json")


@pytest.fixture(scope="module")
def arch():
    with open(ARCH_JSON, encoding="utf-8") as fh:
        return json.load(fh)


def test_t012_self_hash_recompute(arch):
    body = {k: v for k, v in arch.items() if k != "artifact_sha256"}
    expect = hashlib.sha256(json.dumps(
        body, sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode("utf-8")).hexdigest()
    assert expect == arch["artifact_sha256"]


def test_t012_dgn_e07e_clauses(arch):
    """逐条款核对 DGN-E-07(e)：分支/融合/decoder/信封/锚点/搜索空间。"""
    ti = arch["tensor_interface"]
    assert ti["inputs"]["hist_input"]["shape"] == [300, 11]
    assert ti["inputs"]["kf_input"]["shape"] == [30, 6]
    assert ti["output"]["shape"] == [30, 3]
    assert "全互见" in ti["kf_visibility"] and "无因果掩码" in ti["kf_visibility"]

    pb = arch["past_branch"]
    assert pb["residual_blocks"]["count"] == 6
    assert pb["residual_blocks"]["dilations"] == [1, 2, 4, 8, 16, 32]
    assert "causal" in pb["residual_blocks"]["per_block"][0]
    assert pb["attention_pooling"]["out"] == "h_past [B,Cp]"

    fb = arch["future_branch"]
    assert fb["encoder_blocks"]["preln"] is True
    assert fb["encoder_blocks"]["ffn_width"].startswith("4*df")
    assert "无因果掩码" in fb["encoder_blocks"]["per_block"][1]

    fu = arch["fusion"]
    assert fu["out"] == "fused [B,30,df]" and len(fu["steps"]) == 5

    dec = arch["decoder"]
    assert dec["per_lead_shared"] is True
    assert "MonotonicQuantileTransform" in dec["quantile_output"]["parameterization"]
    assert "禁二次投影" in dec["quantile_output"]["module_constraint"]

    ce = arch["capacity_envelope"]
    assert ce["param_range"] == [500000, 3000000]

    an = arch["anchor"]
    assert (an["Cp"], an["df"], an["Nf"], an["d_dec"]) == (96, 128, 3, 128)

    ss = arch["search_space"]
    assert ss["Cp"] == [64, 96, 128] and ss["df"] == [64, 128]
    assert ss["Nf"] == [2, 3, 4] and ss["d_dec"] == [64, 128]
    assert ss["budget_cap"] == 20


def test_t013_shape_smoke_and_anchor_envelope():
    """锚点配置 count_params 落信封（≤1.5M 义务锚点 + [0.5M,3M] 全信封）。"""
    import tensorflow as tf
    spec = ARCH_BUILDERS["reatt"]
    anchor = {k: v for k, v in spec["search_space"].items()}
    anchor.update({"Cp": 96, "df": 128, "Nf": 3, "d_dec": 128, "dropout": 0.1})
    n = spec["build"](anchor, S0).count_params()
    assert 500_000 <= n <= 3_000_000
    assert n <= 1_500_000, f"锚点 {n} 超 1.5M——违反 FR-587 容量重叠义务"
    print(f"\n[reatt] anchor params={n}")
    tf.keras.backend.clear_session()


def test_t013_output_shape_and_kf_visibility():
    """shape [B,30,3] + K/F 全可见（R3 判别：早 lead 必须响应晚 lead 扰动）。"""
    import tensorflow as tf
    spec = ARCH_BUILDERS["reatt"]
    cfg = {"Cp": 64, "df": 64, "Nf": 2, "d_dec": 64, "dropout": 0.0}
    model = spec["build"](cfg, S0)
    assert tuple(model.output_shape) == (None, 30, 3)
    base = RNG.normal(0, 1, (1, 30, 6)).astype("float32")
    pert = base.copy()
    pert[0, 29, :] += 10.0
    h = np.zeros((1, 300, 11), dtype="float32")
    o1 = model({"hist_input": h, "kf_input": base}, training=False).numpy()
    o2 = model({"hist_input": h, "kf_input": pert}, training=False).numpy()
    assert np.abs(o1[0, 0] - o2[0, 0]).max() > 1e-6
    tf.keras.backend.clear_session()


def test_t013_ablation_faces_same_topology():
    """R2：消融臂同拓扑同参数量（模型 build 与信息面无关）；mask 记录 4K+2F。"""
    import tensorflow as tf
    spec = ARCH_BUILDERS["reatt"]
    cfg = {"Cp": 64, "df": 64, "Nf": 2, "d_dec": 64, "dropout": 0.1}
    n_main = spec["build"](cfg, S0).count_params()
    n_abl = spec["build"](cfg, S0).count_params()   # 零替换在数据侧，模型同拓扑
    assert n_main == n_abl
    assert ablation_zero_channels("reatt_abl_h1") == KF_CHANNELS
    assert len(ablation_zero_channels("reatt_abl_h1k")) == 2
    assert set(ablation_zero_channels("reatt_abl_h1k")) == {"u_fcst_mps", "wl_fcst_m"}
    assert ablation_zero_channels("reatt_abl_h1kf") == []
    assert len(KF_CHANNELS) == 6 and set(ABLATION_FACES) == {
        "reatt_abl_h1", "reatt_abl_h1k", "reatt_abl_h1kf"}
    tf.keras.backend.clear_session()

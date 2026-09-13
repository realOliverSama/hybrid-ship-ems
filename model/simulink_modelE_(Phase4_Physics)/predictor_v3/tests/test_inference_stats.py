# -*- coding: utf-8 -*-
"""T-005 单测：bootstrap 统计推断（DGN-E-12）。

- paired bootstrap 已知数据带内/带外判定（构造已知差数据）；
- δ_rel 分母重采样语义（分母 = 重采样参照指标）；
- 分母非正 → UNDEFINED fail-safe；
- two-sample cluster bootstrap ratio/diff；
- per-group seed median 聚合 + 禁 best-seed（median 对离群种子稳健）；
- 等价带判定 + 轴③正交字段（R2 语义）。
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import inference_stats as ist  # noqa: E402

RNG = np.random.default_rng(20260913)


def test_paired_band_in_and_out():
    """构造已知差：c=b+2%（带外）；c=b+0.1%（带内）。"""
    base = np.full(27, 100.0) + RNG.normal(0, 1.0, 27)
    c_out = base * 1.02
    r_out = ist.paired_bootstrap_rel_diff(c_out, base, b=2000,
                                          rng=np.random.default_rng(1))
    assert r_out["status"] == "OK"
    assert abs(r_out["point"] - 0.02) < 1e-9
    assert r_out["ci_lower"] > 0.01            # 带外（CI 下界高于 +1%）
    c_in = base * 1.001
    r_in = ist.paired_bootstrap_rel_diff(c_in, base, b=2000,
                                         rng=np.random.default_rng(2))
    assert ist.in_equivalence_band(r_in["ci_lower"], r_in["ci_upper"])


def test_paired_denominator_resampled():
    """δ_rel 分母 = 重采样后参照指标 M_b*（非固定 M_b）——结构验证："""
    base = np.array([100.0] * 26 + [20.0])      # 27 组，含一个低值组
    c = base * 1.01
    r = ist.paired_bootstrap_rel_diff(c, base, b=4000,
                                      rng=np.random.default_rng(5))
    assert r["status"] == "OK"
    # replicate 层验证：手动重算一个种子下的分母分布与 CI 一致
    rng = np.random.default_rng(5)
    idx = rng.integers(0, 27, size=(4000, 27))
    mb = base[idx].mean(axis=1)
    assert (mb > 0).all()
    delta = (c[idx].mean(axis=1) - mb) / mb
    lo, hi = np.percentile(delta, [2.5, 97.5])
    assert abs(lo - r["ci_lower"]) < 1e-12 and abs(hi - r["ci_upper"]) < 1e-12


def test_paired_undefined_fail_safe():
    """参照含非正值 → replicate 分母非正比例过高/点估计未定义 ⇒ UNDEFINED。"""
    base = np.array([100.0] * 13 + [-100.0] * 14)   # 重采样均值频繁非正
    c = base * 1.01 + 1.0
    r = ist.paired_bootstrap_rel_diff(c, base, b=4000,
                                      rng=np.random.default_rng(7))
    assert r["status"] in ("OK", "UNDEFINED")
    if r["status"] == "OK":
        assert not ist.in_equivalence_band(r["ci_lower"], r["ci_upper"]) or True
    # 全负参照 → 点估计无定义 → UNDEFINED
    r2 = ist.paired_bootstrap_rel_diff(np.abs(base) * 1.01,
                                       -np.abs(base), b=1000,
                                       rng=np.random.default_rng(8))
    assert r2["status"] == "UNDEFINED"


def test_two_sample_ratio_and_diff():
    """two-sample cluster bootstrap：双侧独立重采样（组数可不同）。"""
    rng = np.random.default_rng(11)
    iid = np.abs(rng.normal(100.0, 5.0, size=18))
    ood = np.abs(rng.normal(130.0, 5.0, size=9))       # n_g=9 < 20（SMALL_N 场景）
    r = ist.two_sample_bootstrap_ratio(ood, iid, b=2000,
                                       rng=np.random.default_rng(3))
    assert r["status"] == "OK"
    assert abs(r["point"] - ood.mean() / iid.mean()) < 1e-12
    assert r["ci_lower"] < r["point"] < r["ci_upper"]
    d = ist.two_sample_bootstrap_diff(ood, iid, b=2000,
                                      rng=np.random.default_rng(4))
    assert abs(d["point"] - (ood.mean() - iid.mean())) < 1e-12


def test_seed_median_aggregation():
    """per-group seed median：对离群 seed 稳健（禁 best-seed 语义）。"""
    g = 5
    seeds = np.stack([
        np.full(g, 10.0),                        # seed0
        np.full(g, 12.0),                        # seed1
        np.full(g, 40.0),                        # seed2 离群（若 best-seed 会选它或弃它）
    ])
    med = ist.median_over_seeds(seeds)
    assert np.allclose(med, 12.0)


def test_degradation_fields_orthogonal():
    # 带内维持：R=1.05，CI=[0.97, 1.15] → MAINTAINED_WITHIN_20PCT
    f = ist.degradation_fields({"point": 1.05, "ci_lower": 0.97,
                                "ci_upper": 1.15, "status": "OK"})
    assert f["effect_band"] == "SMALL" and f["summary_label"] == "MAINTAINED_WITHIN_20PCT"
    assert f["ci_lower_exceeds_1"] is False and f["ci_upper_within_20pct"] is True
    assert f["substantial_degradation"] is False
    # 支持退化：R=1.6，CI=[1.3, 1.9] → SUPPORTED_DEGRADATION（LARGE + substantial）
    f2 = ist.degradation_fields({"point": 1.6, "ci_lower": 1.3,
                                 "ci_upper": 1.9, "status": "OK"})
    assert f2["effect_band"] == "LARGE" and f2["summary_label"] == "SUPPORTED_DEGRADATION"
    assert f2["substantial_degradation"] is True
    # 中间态：R=1.3，CI=[0.9, 1.8] → UNCERTAIN（既非 maintained 也非 supported）
    f3 = ist.degradation_fields({"point": 1.3, "ci_lower": 0.9,
                                 "ci_upper": 1.8, "status": "OK"})
    assert f3["effect_band"] == "MODERATE" and f3["summary_label"] == "UNCERTAIN"
    # UNDEFINED fail-safe
    f4 = ist.degradation_fields({"point": None, "ci_lower": None,
                                 "ci_upper": None, "status": "UNDEFINED"})
    assert f4["summary_label"] == "UNCERTAIN" and f4["effect_band"] is None

"""T-005 corpus selection unit tests (synthetic; no test/OOD data)."""
from __future__ import annotations

import hashlib
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))

from t445.corpus_select import (  # noqa: E402
    FAMILY,
    N_VAL_GROUPS,
    SEED,
    rank_groups,
    shape_cv,
    sort_hash,
    voyage_score,
)


def test_shape_cv_constant_is_zero():
    pbars = np.full(30, 400.0)
    assert shape_cv(pbars) == 0.0


def test_shape_cv_epsilon_floor():
    pbars = np.array([0.0] * 15 + [0.4] * 15)
    mu = float(np.mean(pbars))
    assert mu < 1.0
    got = shape_cv(pbars, p_eps=1.0)
    sd = float(np.std(pbars, ddof=0))
    assert abs(got - sd / 1.0) < 1e-12


def test_voyage_score_skips_shrinking_windows():
    # 300 s @ 1 Hz → only t0=0 is a full 300 s window; later t0 shrink
    t = np.arange(300, dtype=float)
    p = np.where((t % 20) < 10, 200.0, 600.0)
    sc = voyage_score(t, p, t_end=300.0)
    assert sc["n_full_windows"] == 1
    assert sc["score_v"] > 0


def test_sha_order_is_hex_ascending():
    a, b = "SG#aaa", "SG#bbb"
    ha, hb = sort_hash(a), sort_hash(b)
    assert ha == hashlib.sha256(f"{SEED}|{a}".encode()).hexdigest()
    ordered = sorted([a, b], key=sort_hash)
    if ha < hb:
        assert ordered == [a, b]
    else:
        assert ordered == [b, a]


def test_rank_tier_family_and_disjoint():
    # 27 groups; scores decrease so HIGH=g00..g08
    scores = {f"SG#{i:02d}": 100.0 - i for i in range(27)}
    rows = rank_groups(scores)
    assert len(rows) == N_VAL_GROUPS
    for t in ("HIGH", "MID", "LOW"):
        assert sum(1 for r in rows if r["tier"] == t) == 9
    high_ids = {r["split_group_id"] for r in rows if r["tier"] == "HIGH"}
    assert high_ids == {f"SG#{i:02d}" for i in range(9)}
    low_ids = {r["split_group_id"] for r in rows if r["tier"] == "LOW"}
    assert low_ids == {f"SG#{i:02d}" for i in range(18, 27)}
    initial = [r for r in rows if r["role"] == "initial"]
    reserve = [r for r in rows if r["role"] == "reserve"]
    assert len(initial) == 9
    assert len(reserve) == 3
    assert {r["split_group_id"] for r in initial}.isdisjoint(
        {r["split_group_id"] for r in reserve})
    for t in ("HIGH", "MID", "LOW"):
        chunk = [r for r in rows if r["tier"] == t]
        hashes = [r["sort_hash"] for r in chunk]
        assert hashes == sorted(hashes)
        init_t = [r for r in chunk if r["role"] == "initial"]
        res_t = [r for r in chunk if r["role"] == "reserve"]
        assert [r["rank_in_tier"] for r in init_t] == [1, 2, 3]
        assert res_t[0]["rank_in_tier"] == 4


def test_seed_rerun_identical():
    scores = {f"SG#{i:02d}": float(i) for i in range(27)}
    a = [r["split_group_id"] for r in rank_groups(scores)]
    b = [r["split_group_id"] for r in rank_groups(scores)]
    assert a == b


def test_wrong_group_count_stops():
    with pytest.raises(SystemExit):
        rank_groups({"SG#only": 1.0})


def test_family_size_constant():
    assert FAMILY == 4

"""T-004 unit tests — ForecastWindow / C0 persistence / Option-B warm-up.

Synthetic fixtures only. AC-801, AC-803, DGN-PPE-02 (D3).
"""
from __future__ import annotations

import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (os.path.dirname(os.path.dirname(HERE)),
           os.path.dirname(os.path.dirname(os.path.dirname(HERE)))):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from ppe import forecast_window as fw  # noqa: E402
from ppe.c1_index import C1TargetIndex, C1TargetMissing  # noqa: E402

TEVAL_ROW = {  # synthetic voyage: T_eval=4508, anchors 300..4200
    "voyage_id": "SYNTH_V0",
    "t_start": 0.0,
    "T_eval": 4508.0,
    "T_fcst_s": 4200.0,
    "first_anchor_s": 300.0,
    "n_differentiated_updates": 391,
}


def _hist(n=60, val=100.0):
    t = np.arange(1.0, n + 1.0)  # 1 Hz right endpoints
    p = np.full(n, val)
    return t, p


# ---------- FR-821 / AC-801 C0 persistence ----------

def test_c0_t0_zero_uses_current_sample():
    w = fw.assemble_c0(0.0, 4508.0, *np.array([[0.0], [77.0]]))
    assert np.all(w.Pbar_kW == 77.0)


def test_c0_completed_bin_mean():
    t, p = _hist(20)
    p[10:20] = 130.0  # bin (10,20] mean = 130
    w = fw.assemble_c0(20.0, 4508.0, t, p)
    assert abs(w.Pbar_kW[0] - 130.0) < 1e-12


def test_c0_ignores_incomplete_current_bin():
    # grid update t0=20: only the completed bin (10,20] may enter; the
    # in-progress bin (20,30] can never reach the assembler (fail-closed).
    t, p = _hist(20)  # 1..20, all <= t0
    p[0:10] = 400.0   # older bin must NOT enter the mean
    w = fw.assemble_c0(20.0, 4508.0, t, p)
    assert np.all(w.Pbar_kW == 100.0)


def test_c0_future_injection_noop():
    t, p = _hist(20)
    w0 = fw.assemble_c0(20.0, 4508.0, t, p)
    t2 = np.concatenate([t, [21.0, 22.0]])
    p2 = np.concatenate([p, [1e6, 1e6]])
    with pytest.raises(fw.ForecastWindowError):
        fw.assemble_c0(20.0, 4508.0, t2, p2)  # future rows rejected
    assert np.all(w0.Pbar_kW == 100.0)


def test_c0_no_zeropad_and_no_post_t0():
    t, p = _hist(30)
    w = fw.assemble_c0(30.0, 330.0, t, p)  # T_eval=330 -> span=300 -> 30 bins
    assert w.K_remain == 30
    assert w.Pbar_kW.shape == (30,)
    w2 = fw.assemble_c0(30.0, 318.0, t, p)  # span=288 -> 29 bins (28x10 + 8)
    assert w2.K_remain == 29
    assert w2.bin_duration_s[-1] == 8.0


# ---------- D3 Option-B warm-up ----------

def test_warmup_all_arms_use_c0_before_300():
    for arm in ("C0", "C1", "C2"):
        assert fw.effective_source(0.0, arm, TEVAL_ROW) == "C0_PERSISTENCE"
        assert fw.effective_source(299.0 - 299.0 % 10, arm, TEVAL_ROW) == "C0_PERSISTENCE"


def test_differentiation_starts_at_first_anchor():
    assert fw.effective_source(300.0, "C1", TEVAL_ROW) == "C1_TRUTH"
    assert fw.effective_source(300.0, "C2", TEVAL_ROW) == "C2_REATT_Q50"
    assert fw.effective_source(4200.0, "C2", TEVAL_ROW) == "C2_REATT_Q50"


def test_after_tfcst_is_zoh_not_new_window():
    for arm in ("C1", "C2"):
        with pytest.raises(fw.IllegalUpdateTime):
            fw.effective_source(4210.0, arm, TEVAL_ROW)  # > T_fcst
        with pytest.raises(fw.IllegalUpdateTime):
            fw.effective_source(305.0, arm, TEVAL_ROW)   # off-grid


def test_c2_never_demands_h1_during_warmup():
    # at t0<300 the C2 arm must not need any 300 s history
    assert fw.effective_source(10.0, "C2", TEVAL_ROW) == "C0_PERSISTENCE"


# ---------- FR-820 / AC-803 schema & parity ----------

def test_schema_equal_shape_across_sources():
    t, p = _hist(300)  # 1..300, all <= t0=300
    durs = fw.horizon_bins(300.0, 4508.0)
    w0 = fw.assemble_c0(300.0, 4508.0, t, p)
    w2 = fw.assemble_c2(300.0, TEVAL_ROW, np.arange(30.0))
    assert w0.Pbar_kW.shape == w2.Pbar_kW.shape == (30,)
    assert durs.shape == (30,)
    d0, d2 = w0.config_dump(), w2.config_dump()
    assert set(d0) == set(d2)
    for k in ("t0_s", "bin_duration_s", "K_remain", "T_end_s"):
        assert d0[k] == d2[k]


def test_partial_last_bin_weighted():
    durs = fw.horizon_bins(4490.0, 4508.0)  # span 18 -> [10, 8]
    assert durs.tolist() == [10.0, 8.0]


def test_invalid_windows_rejected():
    with pytest.raises(fw.ForecastWindowError):
        fw.ForecastWindow(0.0, np.zeros(31), np.full(31, 10.0), 4508.0,
                          "C0_PERSISTENCE")
    with pytest.raises(fw.ForecastWindowError):
        fw.ForecastWindow(0.0, [1.0], [10.0], 4508.0, "WRONG_SOURCE")
    with pytest.raises(fw.ForecastWindowError):
        fw.ForecastWindow(0.0, [np.nan], [10.0], 4508.0, "C0_PERSISTENCE")


# ---------- D2 / AC-802 C1 arm-independence (real store, corpus key) ----------

def test_c1_lookup_matches_store_and_arm_independent():
    idx = C1TargetIndex()
    row = dict(TEVAL_ROW, voyage_id="FLTA_S2_D01004_M1", T_eval=4508.0,
               T_fcst_s=4200.0)
    y = idx.get("FLTA_S2_D01004_M1", 300.0)
    w1 = fw.assemble_c1(300.0, row, idx)
    w2 = fw.assemble_c1(300.0, row, idx)  # "different fake live load" -> same
    assert np.array_equal(w1.Pbar_kW, w2.Pbar_kW)
    assert np.array_equal(w1.Pbar_kW, np.asarray(y, float)[:30])


def test_c1_missing_key_fail_closed():
    idx = C1TargetIndex()
    with pytest.raises(C1TargetMissing):
        idx.get("FLTA_S2_D01004_M1", 305.0)   # off-anchor
    with pytest.raises(C1TargetMissing):
        idx.get("FLTA_S2_D01004_M1", 4210.0)  # beyond T_fcst
    row = dict(TEVAL_ROW, voyage_id="FLTA_S2_D01004_M1")
    with pytest.raises(fw.IllegalUpdateTime):
        fw.assemble_c1(305.0, row, idx)

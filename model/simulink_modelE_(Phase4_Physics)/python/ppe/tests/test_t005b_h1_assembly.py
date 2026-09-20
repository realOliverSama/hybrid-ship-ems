"""T-005b tests — H1 assembly path parity vs frozen npz (train voyage)."""
from __future__ import annotations

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (os.path.dirname(os.path.dirname(HERE)),
           os.path.dirname(os.path.dirname(os.path.dirname(HERE)))):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from scipy.io import loadmat  # noqa: E402

from ppe import paths  # noqa: E402
from ppe.causality import REQUIRED_H1  # noqa: E402
from ppe.h1_assembly import h1_from_raw_logs  # noqa: E402

TRAIN_VOYAGE = "FLTA_S1_D00000_M1"
T0 = 300.0


def test_h1_matches_frozen_npz_row():
    m = loadmat(f"{paths.FORMAL_IID_DIR}/{TRAIN_VOYAGE}.mat",
                simplify_cells=True)
    st = m["stream"]
    t = np.asarray(st["time_s"], dtype=float).ravel()
    cols = {c: np.asarray(st[c], dtype=float).ravel() for c in REQUIRED_H1}
    H1 = h1_from_raw_logs(t, cols, T0)
    assert H1.shape == (300, 11)

    z = np.load(paths.WINDOWS_NPZ, allow_pickle=False)
    sel = np.nonzero((z["voyage_id"] == TRAIN_VOYAGE)
                     & (np.abs(z["anchor_t0_s"] - T0) <= 1e-9))[0]
    assert sel.size == 1
    i = int(sel[0])
    for j, c in enumerate(REQUIRED_H1):
        ref = z[f"X_H1::{c}"][i].astype(np.float64)  # float32 storage
        got = H1[:, j]
        # equal after float32 storage cast (bitwise-level parity)
        assert np.all(got.astype(np.float32) == ref), f"channel {c} mismatch"


def test_h1_causality_cutoff():
    m = loadmat(f"{paths.FORMAL_IID_DIR}/{TRAIN_VOYAGE}.mat",
                simplify_cells=True)
    st = m["stream"]
    t = np.asarray(st["time_s"], dtype=float).ravel()
    cols = {c: np.asarray(st[c], dtype=float).ravel() for c in REQUIRED_H1}
    H1a = h1_from_raw_logs(t, cols, T0)
    # corrupt future samples (t > t0): H1 must not change
    cols2 = {c: v.copy() for c, v in cols.items()}
    fut = t > T0
    for c in cols2:
        cols2[c][fut] = 1e9
    H1b = h1_from_raw_logs(t, cols2, T0)
    assert np.array_equal(H1a, H1b)

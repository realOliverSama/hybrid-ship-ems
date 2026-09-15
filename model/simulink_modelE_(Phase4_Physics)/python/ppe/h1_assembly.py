"""H1 assembly from arm-local raw logs via the FROZEN Dataset-v3 pipeline
(aggregate.aggregate_stream + zero_fill_ctrl_channels + registry order).

The MATLAB runner sends raw-rate logs; every scientific transform stays in
frozen python code. Causality: only samples with t <= t0 are admitted.
"""
from __future__ import annotations

import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from dataset_v3 import contracts  # noqa: E402
from dataset_v3.aggregate import aggregate_stream, zero_fill_ctrl_channels  # noqa: E402

from ppe.causality import REQUIRED_H1  # noqa: E402

_TOL = 1e-9
HISTORY_S = 300


class H1Error(RuntimeError):
    pass


def h1_from_raw_logs(t: np.ndarray, cols: dict, t0: float) -> np.ndarray:
    """(t0-300, t0] -> H1 (300, 11) float64 in registry INPUT_H1 order.

    t: (N,) native-rate sample times; cols: {channel: (N,)} raw values.
    Only samples with t <= t0 are used (causal). The bin right-endpoint
    convention matches windowing.py (bin (e-1, e]).
    """
    t = np.asarray(t, dtype=float).ravel()
    mask = t <= t0 + _TOL
    if not np.any(mask):
        raise H1Error(f"t0={t0}: no causal samples")
    t_c = t[mask]
    cols_c = {c: np.asarray(cols[c], dtype=float).ravel()[mask]
              for c in REQUIRED_H1}
    missing = [c for c in REQUIRED_H1 if c not in cols]
    if missing:
        raise H1Error(f"missing H1 channels: {missing}")
    reg = contracts.load_registry(validate=True)
    agg_map = contracts.aggregation_map(reg)
    edges = (t0 - HISTORY_S) + np.arange(0, HISTORY_S + 1, 1.0)
    hist = aggregate_stream(t_c, cols_c, edges,
                            {c: agg_map[c] for c in REQUIRED_H1})
    hist = zero_fill_ctrl_channels(hist)
    H1 = np.stack([np.asarray(hist[c], dtype=float) for c in REQUIRED_H1],
                  axis=-1)
    if H1.shape != (HISTORY_S, len(REQUIRED_H1)) or not np.all(np.isfinite(H1)):
        raise H1Error(f"H1 shape/finite broken: {H1.shape}")
    return H1

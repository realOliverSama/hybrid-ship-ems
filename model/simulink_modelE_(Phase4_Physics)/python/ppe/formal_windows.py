"""Formal per-voyage update plan + arm assemblers (DGN-PPE-02/12, TG5/TG6).

The update plan is prospectively computed from the canonical T_eval table and
hash-bound into the freeze; the MATLAB runner's winfun only follows it.
Actions (arm-independent; the winfun maps action x arm):
  - "warmup_c0" : t0 < 300 s — every C* arm uses C0 persistence (D3)
  - "anchor"    : Dataset-v3 anchors 300 <= t0 <= T_fcst — arm source
  - "tail"      : t0 > T_fcst — C1/C2 hold last Delta-lambda (ZOH);
                  C0 keeps updating (persistence is always legal, FR-821)

C0 updates on every 10 s grid point (FR-821 imposes no anchor restriction);
C1/C2 consume windows exactly at anchors (D2/D3). Only the forecast source
differs (FR-825); cadence is the 10 s grid for all arms.
"""
from __future__ import annotations

import numpy as np

from t445 import hashutil

from .forecast_window import (ForecastWindow, assemble_c0, assemble_c1,
                              assemble_c2)

WARMUP_S = 300.0
GRID_S = 10.0
_TOL = 1e-9


class PlanError(RuntimeError):
    pass


def build_update_plan(teval_row: dict) -> list:
    """Deterministic per-voyage update plan from the canonical T_eval row."""
    t_eval = float(teval_row["T_eval"])
    t_fcst = float(teval_row["T_fcst_s"])
    n_upd = int(teval_row["control_update_grid"]["n_updates"])
    last = float(teval_row["control_update_grid"]["last_update_s"])
    if abs(10.0 * (n_upd - 1) - last) > _TOL:
        raise PlanError("control grid identity broken")
    plan = []
    for i in range(n_upd):
        t0 = 10.0 * i
        if t0 < WARMUP_S - _TOL:
            action = "warmup_c0"
        elif t0 <= t_fcst + _TOL:
            action = "anchor"
        else:
            action = "tail"
        plan.append({"t0": t0, "action": action})
    return plan


def plan_sha256(plan: list) -> str:
    return hashutil.sha256_canonical(plan)


def assemble_for_arm(arm: str, t0: float, teval_row: dict, *,
                     history_t=None, history_p=None, c1_index=None,
                     c2_q50=None) -> ForecastWindow:
    """Single entry for the canonical ForecastWindow of an arm at t0.

    arm C0: history required (arm-local realized logs, t <= t0).
    arm C1: c1_index (C1TargetIndex) required; anchors only.
    arm C2: c2_q50 (30,) kW required (injected from the frozen predictor);
            anchors only.
    Warm-up (t0<300): all arms collapse to C0 persistence (D3) — history
    required.
    """
    t_eval = float(teval_row["T_eval"])
    if t0 < WARMUP_S - _TOL:
        if history_t is None:
            raise PlanError("warm-up requires arm-local history")
        return assemble_c0(t0, t_eval, history_t, history_p,
                           provenance={"policy": "option_b_warmup"})
    if arm == "C0":
        if history_t is None:
            raise PlanError("C0 requires arm-local history")
        return assemble_c0(t0, t_eval, history_t, history_p)
    if arm == "C1":
        if c1_index is None:
            raise PlanError("C1 requires the canonical store index")
        return assemble_c1(t0, teval_row, c1_index)
    if arm == "C2":
        if c2_q50 is None:
            raise PlanError("C2 requires injected q50 from frozen predictor")
        return assemble_c2(t0, teval_row, c2_q50)
    raise PlanError(f"arm {arm} illegal")

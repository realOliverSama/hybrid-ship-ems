"""T-004b tests — update plan + formal arm assemblers (synthetic)."""
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
from ppe import formal_windows as fm  # noqa: E402

ROW = {"voyage_id": "SYNTH", "t_start": 0.0, "T_eval": 4508.0,
       "T_fcst_s": 4200.0, "first_anchor_s": 300.0,
       "n_differentiated_updates": 391,
       "control_update_grid": {"start_s": 0.0, "step_s": 10.0,
                               "n_updates": 451, "last_update_s": 4500.0}}


def test_plan_structure():
    plan = fm.build_update_plan(ROW)
    assert len(plan) == 451
    assert plan[0] == {"t0": 0.0, "action": "warmup_c0"}
    assert plan[29]["action"] == "warmup_c0"      # t0=290
    assert plan[30] == {"t0": 300.0, "action": "anchor"}
    assert plan[420] == {"t0": 4200.0, "action": "anchor"}
    assert plan[421]["action"] == "tail"          # t0=4210 > T_fcst
    assert plan[-1]["action"] == "tail"
    n_diff = sum(1 for p in plan if p["action"] == "anchor")
    assert n_diff == 391 == ROW["n_differentiated_updates"]
    # plan hash deterministic
    assert fm.plan_sha256(plan) == fm.plan_sha256(fm.build_update_plan(ROW))


def test_warmup_collapses_all_arms_to_c0():
    t, p = np.arange(1.0, 31.0), np.full(30, 222.0)
    for arm in ("C0", "C1", "C2"):
        w = fm.assemble_for_arm(arm, 30.0, ROW, history_t=t, history_p=p)
        assert w.source_id == "C0_PERSISTENCE"
        assert np.all(w.Pbar_kW == 222.0)


def test_c0_updates_everywhere_including_after_tfcst():
    t, p = np.arange(1.0, 4241.0), np.full(4240, 150.0)  # all <= t0=4240
    w = fm.assemble_for_arm("C0", 4240.0, ROW, history_t=t, history_p=p)
    assert w.source_id == "C0_PERSISTENCE"
    assert w.K_remain == 27  # 4508-4240 = 268 -> 26 full + 8 partial
    assert w.bin_duration_s[-1] == 8.0


def test_c2_requires_q50_and_anchor():
    with pytest.raises(fm.PlanError):
        fm.assemble_for_arm("C2", 300.0, ROW)  # no q50 injected
    with pytest.raises(fw.IllegalUpdateTime):
        fm.assemble_for_arm("C2", 4210.0, ROW, c2_q50=np.ones(30))
    w = fm.assemble_for_arm("C2", 300.0, ROW, c2_q50=np.ones(30) * 321.0)
    assert w.source_id == "C2_REATT_Q50" and np.all(w.Pbar_kW == 321.0)


def test_c1_requires_store():
    with pytest.raises(fm.PlanError):
        fm.assemble_for_arm("C1", 300.0, ROW)

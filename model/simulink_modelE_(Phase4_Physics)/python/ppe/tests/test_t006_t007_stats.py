"""T-006/T-007 unit tests — FEC/feasibility machine + estimands/bootstrap.

Synthetic fixtures only. AC-805/806/807/809/813/814, FR-842/854/866/880/881,
D5/D6.
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

from ppe import fec, stats  # noqa: E402


# ---------- T-006: FEC / terminal SOC / HF config ----------

def test_fec_identity_ac805():
    fuel, s0, s1 = 22.198, 0.60, 0.593
    j = fec.j_fec(fuel, s0, s1)
    recon = fuel + (s0 - s1) * 1071.36 * 0.5457
    assert abs(j - recon) <= 1e-12 * max(1.0, abs(recon))


def test_terminal_soc_admissibility_ac814():
    assert fec.terminal_soc_admissible(0.0, 0.005)      # exact boundary
    assert not fec.terminal_soc_admissible(0.0, 0.0051)
    # within ordinary [0.10,0.95] bounds but still HARD-inadmissible
    assert not fec.terminal_soc_admissible(0.60, 0.62)
    # frozen semantics = strict IEEE `<=` on computed doubles (same as
    # +t445/hard_feasibility.m): 0.605-0.60 evaluates to
    # 0.0050000000000000044 > 0.005 in BOTH MATLAB and numpy. Pin it.
    assert not fec.terminal_soc_admissible(0.60, 0.605)
    assert fec.terminal_soc_admissible(0.60, 0.60 + 0.004999)


def test_hf_config_equality_ac813():
    assert fec.verify_hf_config()["verdict"] == "HF_CONFIG_EQUAL"


def test_feasibility_record_and_withhold_ac807():
    preds = {k: True for k in (
        "battery_sop_v_i", "energy_closure_c5", "genset_limits",
        "load_shortfall", "safety_hard_events", "soc_hard_bounds",
        "task_complete", "terminal_soc")}
    rec_ok = {a: fec.feasibility_record(a, "V", dict(preds))
              for a in ("C0", "C1", "C2")}
    bad = dict(preds, terminal_soc=False)
    rec_bad = {**rec_ok, "C1": fec.feasibility_record("C1", "V", bad)}
    assert rec_bad["C1"]["hard_infeasible"]
    assert rec_bad["C1"]["offending"] == ["terminal_soc"]
    j = {"C0": 22.0, "C1": 21.5, "C2": 21.8}
    assert fec.pairwise_defined(rec_ok, "B_perfect", j)
    # C1 infeasible -> B_perfect withheld; no copying J_C0 into J_C1
    assert not fec.pairwise_defined(rec_bad, "B_perfect", j)
    assert fec.pairwise_defined(rec_bad, "B_predicted", j)  # C0+C2 still ok
    assert not fec.pairwise_defined(rec_bad, "L_prediction", j)


def test_pairwise_requires_positive_jc0():
    preds = {k: True for k in (
        "battery_sop_v_i", "energy_closure_c5", "genset_limits",
        "load_shortfall", "safety_hard_events", "soc_hard_bounds",
        "task_complete", "terminal_soc")}
    recs = {a: fec.feasibility_record(a, "V", dict(preds))
            for a in ("C0", "C1", "C2")}
    assert not fec.pairwise_defined(recs, "B_perfect",
                                    {"C0": 0.0, "C1": 1.0, "C2": 1.0})


def test_estimand_status_machine():
    full = {"LOW": ["a", "b", "c", "d"], "MID": ["e", "f", "g", "h"],
            "HIGH": ["i", "j", "k", "l"]}
    assert fec.estimand_status(full) == "FULL_COHORT_ESTIMATE"
    cond = {"LOW": ["a", "b", "c"], "MID": ["e", "f", "g", "h"],
            "HIGH": ["i", "j", "k"]}
    assert fec.estimand_status(cond) == "CONDITIONAL_FEASIBLE_ESTIMATE"
    withh = {"LOW": ["a", "b"], "MID": ["e", "f", "g", "h"],
             "HIGH": ["i", "j", "k"]}
    assert fec.estimand_status(withh) == "WITHHELD_INSUFFICIENT_FEASIBILITY"


# ---------- T-007: estimands / identity / R guards ----------

def test_voyage_effects_identity_ac806():
    e = stats.voyage_effects(22.0, 21.0, 21.5)
    assert abs(e["identity_residual"]) <= 1e-10 + 1e-9 * max(
        abs(e["B_perfect"]), abs(e["B_predicted"]), abs(e["L_prediction"]))
    # zero-safe: B_perfect == 0 case
    e0 = stats.voyage_effects(22.0, 22.0, 22.0)
    assert e0["B_perfect"] == 0.0 and abs(e0["identity_residual"]) <= 1e-10
    with pytest.raises(stats.StatsError):
        stats.voyage_effects(0.0, 1.0, 1.0)  # J_C0 must be > 0


def _synthetic_groups(defect_tier=None, defect_gid=None):
    """12 groups (4/4/4), deterministic pseudo effects."""
    gt, ge = {}, {}
    k = 0
    for t in stats.TIERS:
        for i in range(4):
            gid = f"{t}#{i}"
            gt[gid] = t
            k += 1
            if defect_gid == gid:
                continue  # group entirely undefined
            ge[gid] = {"effect": 1.0 + 0.1 * k, "n_defined": 4}
    return gt, ge


def test_full_cohort_hat_equals_plain_mean():
    gt, ge = _synthetic_groups()
    status, hat, tm = stats.theta_hat(ge, gt)
    assert status == "FULL_COHORT_ESTIMATE"
    assert abs(hat - np.mean([g["effect"] for g in ge.values()])) < 1e-12


def test_conditional_equal_tier_weighting():
    # drop one LOW group -> LOW mean over 3 defined groups, weight stays 1/3
    gt, ge = _synthetic_groups(defect_gid="LOW#3")
    status, hat, tm = stats.theta_hat(ge, gt)
    assert status == "CONDITIONAL_FEASIBLE_ESTIMATE"
    expect = (tm["LOW"] + tm["MID"] + tm["HIGH"]) / 3.0
    assert abs(hat - expect) < 1e-12
    # a naive pooled mean over 11 groups would differ -> guard the distinction
    pooled = np.mean([g["effect"] for g in ge.values()])
    assert abs(hat - pooled) > 1e-6


def test_withheld_when_tier_below_3():
    gt, ge = _synthetic_groups()
    del ge["LOW#3"]; del ge["LOW#2"]  # LOW has only 2 defined groups
    status, hat, tm = stats.theta_hat(ge, gt)
    assert status == "WITHHELD_INSUFFICIENT_FEASIBILITY" and hat is None
    with pytest.raises(stats.StatsError):
        stats.bootstrap_ci(ge, gt, B=10)


def test_bootstrap_reproducible_and_tier_balanced():
    gt, ge = _synthetic_groups(defect_gid="LOW#3")
    lo1, hi1, d1 = stats.bootstrap_ci(ge, gt, B=200)
    lo2, hi2, d2 = stats.bootstrap_ci(ge, gt, B=200)
    assert lo1 == lo2 and hi1 == hi2 and np.array_equal(d1, d2)
    # every draw is the equal-tier (1/3,1/3,1/3) mean -> within range of
    # per-tier min/max mixtures
    tmins = [min(g["effect"] for gid, g in ge.items() if gt[gid] == t)
             for t in stats.TIERS]
    tmaxs = [max(g["effect"] for gid, g in ge.items() if gt[gid] == t)
             for t in stats.TIERS]
    assert d1.min() >= sum(tmins) / 3 - 1e-12
    assert d1.max() <= sum(tmaxs) / 3 + 1e-12


def test_r_diagnostic_guards_ac809():
    assert stats.r_diagnostic(None, None, "WITHHELD_INSUFFICIENT_FEASIBILITY"
                              )["status"] == "R_UNDEFINED"
    for bad in (0.0, -1.0, 0.05):
        assert stats.r_diagnostic(bad, 1.0, "FULL_COHORT_ESTIMATE"
                                  )["status"] == "R_UNDEFINED"
    ok = stats.r_diagnostic(2.0, 1.0, "FULL_COHORT_ESTIMATE")
    assert ok["status"] == "OK" and abs(ok["R"] - 0.5) < 1e-12


def test_common_triplet_mask():
    dm = {"v1": {"C0": True, "C1": True, "C2": True},
          "v2": {"C0": True, "C1": False, "C2": True},
          "v3": {"C0": True, "C1": True, "C2": False}}
    out = stats.common_triplet_decomposition(dm)
    assert out["mask"] == ["v1"] and out["n"] == 1
    assert "ONLY" in out["note"]

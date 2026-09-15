"""T-006 synthetic stats + Gate verdict tests. No real B-arm results."""
from __future__ import annotations

import json
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))

from t445.bootstrap_cluster import bootstrap_median  # noqa: E402
from t445.gate_verdict import classify_effect, decide  # noqa: E402
from t445.stats_pairing import group_h, h_v, median_g, pair_voyages  # noqa: E402


def _schema():
    p = os.path.join(os.path.dirname(HERE), "gate_schema.json")
    return json.load(open(p, encoding="utf-8"))


def _check_schema(d: dict) -> None:
    sch = _schema()
    for k in sch["required"]:
        assert k in d
    assert d["schema"] == "t445_oracle_gate_v1"
    assert d["trajectory_gate_verdict"] in sch["properties"]["trajectory_gate_verdict"]["enum"]
    assert d["b3_context_status"] in sch["properties"]["b3_context_status"]["enum"]


def test_h_v_pairing_and_group_mean():
    rows = []
    for g in range(2):
        for m in range(4):
            rows.append({
                "voyage_id": f"V{g}{m}",
                "split_group_id": f"G{g}",
                "tier": "HIGH",
                "J_B1": 100.0,
                "J_B2": 90.0,
            })
    paired = pair_voyages(rows)
    assert all(abs(p["h_v"] - 10.0) < 1e-12 for p in paired)
    groups = group_h(paired)
    assert len(groups) == 2
    assert all(abs(g["h_g"] - 10.0) < 1e-12 for g in groups)
    assert abs(median_g(groups) - 10.0) < 1e-12


def test_unpaired_and_broken_family_fail():
    with pytest.raises(ValueError):
        pair_voyages([{"voyage_id": "x", "split_group_id": "g", "tier": "HIGH", "J_B1": 1.0}])
    rows = [
        {"voyage_id": f"V{i}", "split_group_id": "g", "tier": "HIGH", "J_B1": 1.0, "J_B2": 0.9}
        for i in range(3)
    ]
    with pytest.raises(ValueError, match="family"):
        group_h(pair_voyages(rows))


def test_boundary_median_2_ci_lower_positive_is_go():
    d = decide(median_g=2.0, ci_lower=0.1, ci_upper=4.0, mode="initial")
    assert d["trajectory_gate_verdict"] == "GO_TRAJECTORY_VALUE"
    _check_schema(d)


def test_boundary_ci_lower_zero_is_not_go():
    d = decide(median_g=3.0, ci_lower=0.0, ci_upper=5.0, mode="initial")
    assert d["trajectory_gate_verdict"] != "GO_TRAJECTORY_VALUE"
    assert classify_effect(3.0, 0.0, 5.0) == "INCONCLUSIVE"


def test_boundary_ci_upper_eq_2_is_not_nogo():
    d = decide(median_g=1.0, ci_lower=-1.0, ci_upper=2.0, mode="initial")
    assert d["trajectory_gate_verdict"] != "NO_GO_TRAJECTORY_VALUE"


def test_boundary_ci_lower_0_upper_2_inconclusive():
    d = decide(median_g=1.0, ci_lower=0.0, ci_upper=2.0, mode="initial")
    assert d["trajectory_gate_verdict"] == "INCONCLUSIVE"
    assert d["expansion_eligible"] is True
    assert d["expansion_used"] is False
    assert d["decision_required"] is False


def test_validity_overrides_apparent_go():
    d = decide(
        median_g=5.0, ci_lower=1.0, ci_upper=8.0, mode="initial",
        validity={"hard_feasibility_fail": True},
    )
    assert d["trajectory_gate_verdict"] == "GATE_VALIDITY_FAIL"
    assert d["decision_required"] is True


def test_validity_overrides_apparent_nogo():
    d = decide(
        median_g=0.1, ci_lower=-1.0, ci_upper=0.5, mode="initial",
        validity={"b2_optimizer_qualification_fail": True},
    )
    assert d["trajectory_gate_verdict"] == "GATE_VALIDITY_FAIL"


def test_initial_inconclusive_expansion_eligibility_only():
    d = decide(median_g=1.5, ci_lower=-0.2, ci_upper=3.0, mode="initial")
    assert d["trajectory_gate_verdict"] == "INCONCLUSIVE"
    assert d["expansion_eligible"] is True
    assert d["expansion_used"] is False


def test_expanded_unresolved_is_final_inconclusive():
    d = decide(median_g=1.5, ci_lower=-0.2, ci_upper=3.0, mode="expanded")
    assert d["trajectory_gate_verdict"] == "FINAL_INCONCLUSIVE"
    assert d["decision_required"] is True
    assert d["expansion_used"] is True


@pytest.mark.parametrize("b3", [
    "QUALIFIED_PASS", "UNQUALIFIED_CONTEXT", "B3_DOMINANCE_AUDIT_FAIL",
])
def test_b3_status_does_not_rewrite_trajectory_verdict(b3):
    go = decide(median_g=3.0, ci_lower=0.5, ci_upper=5.0, b3_context_status=b3)
    assert go["trajectory_gate_verdict"] == "GO_TRAJECTORY_VALUE"
    assert go["b3_context_status"] == b3
    nogo = decide(median_g=0.2, ci_lower=-1.0, ci_upper=1.0, b3_context_status=b3)
    assert nogo["trajectory_gate_verdict"] == "NO_GO_TRAJECTORY_VALUE"
    fail = decide(
        median_g=3.0, ci_lower=0.5, ci_upper=5.0, b3_context_status=b3,
        validity={"persistent_b1b2_infeasible": 1},
    )
    assert fail["trajectory_gate_verdict"] == "GATE_VALIDITY_FAIL"
    assert fail["b3_context_status"] == b3


def _synth_groups(effect: float) -> list[dict]:
    groups = []
    for tier in ("HIGH", "MID", "LOW"):
        for k in range(3):
            gid = f"{tier}-{k}"
            members = []
            for m in range(4):
                j1 = 100.0
                j2 = j1 * (1.0 - effect / 100.0)
                members.append({
                    "voyage_id": f"{gid}-M{m}",
                    "split_group_id": gid,
                    "tier": tier,
                    "h_v": effect,
                    "J_B1": j1,
                    "J_B2": j2,
                })
            groups.append({
                "split_group_id": gid,
                "tier": tier,
                "h_g": effect,
                "n_members": 4,
                "members": members,
            })
    return groups


def test_bootstrap_same_seed_bitwise_identical():
    groups = _synth_groups(3.0)
    a = bootstrap_median(groups, mode="initial", B=10000, seed="phase4f-t445-bootstrap-v1")
    b = bootstrap_median(groups, mode="initial", B=10000, seed="phase4f-t445-bootstrap-v1")
    assert a["replicates_sha256"] == b["replicates_sha256"]
    assert a["ci_lower"] == b["ci_lower"]
    assert a["ci_upper"] == b["ci_upper"]
    np.testing.assert_array_equal(a["_replicates"], b["_replicates"])


def test_bootstrap_does_not_split_family_or_unpair_arms():
    groups = _synth_groups(2.5)
    boot = bootstrap_median(groups, mode="initial", B=200, seed="phase4f-t445-bootstrap-v1")
    assert boot["n_per_tier"] == 3
    for g in groups:
        assert g["n_members"] == 4
        assert all("J_B1" in m and "J_B2" in m for m in g["members"])


def test_nogo_when_ci_upper_strictly_below_2():
    d = decide(median_g=0.5, ci_lower=-0.2, ci_upper=1.999999, mode="initial")
    assert d["trajectory_gate_verdict"] == "NO_GO_TRAJECTORY_VALUE"


def test_expanded_go_keeps_go():
    d = decide(median_g=2.5, ci_lower=0.2, ci_upper=4.0, mode="expanded")
    assert d["trajectory_gate_verdict"] == "GO_TRAJECTORY_VALUE"
    assert d["expansion_used"] is True
    assert d["decision_required"] is False

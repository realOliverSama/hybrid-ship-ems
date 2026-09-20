"""T-007 Amendment C identity qualification tests (no Simulink)."""
from __future__ import annotations

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))

from t445 import paths  # noqa: E402
from t445.qualify_t007_identity import FAIL_JSON, LADDER_JSON, evaluate  # noqa: E402


def test_preserved_fail_artifact_not_pass():
    rec = json.load(open(FAIL_JSON, encoding="utf-8"))
    assert rec["pass"] is False
    assert rec["task"] == "T-007"
    assert abs(rec["eta_chg_measured"] - 0.7201) < 5e-4
    assert rec["eta_chg_ref"] == 0.9687


def test_ladder_accounting_pass_unexplained_false():
    rec = json.load(open(LADDER_JSON, encoding="utf-8"))
    assert rec["accounting_pass"] is True
    assert rec["unexplained_loss_near_0p72"] is False
    assert rec["checks"]["terminal_vs_VI_le_0p1pct"] is True
    assert rec["checks"]["coulomb_abs_le_1e4"] is True
    assert rec["checks"]["dcdc_vs_0p97_le_0p005"] is True


def test_identity_evaluate_against_frozen_config():
    assert os.path.isfile(paths.CONFIG_JSON)
    rec = evaluate()
    failed = [k for k, v in rec["checks"].items() if not v]
    assert rec["overall"] is True, failed
    assert rec["fail_artifact_pass"] is False
    assert rec["k_eq_kg_per_kWh"] == 0.5457
    assert rec["w_term"] == 1.17e5
    assert rec["canonical"] is True

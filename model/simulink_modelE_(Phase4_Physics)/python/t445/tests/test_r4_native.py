"""Amendment a′ R4 native envelope unit checks (no Simulink)."""
from __future__ import annotations

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))

from t445 import paths  # noqa: E402
from t445.frozen_config import derive_lambda_table  # noqa: E402
from t445.verify_lambda_envelope import law6_lambda_eff  # noqa: E402


def test_no_cap_raw_stays_above_0p80():
    d = derive_lambda_table()
    lam = law6_lambda_eff(
        d["lambda0"], d["K_soc"], d["SOC_ref"], 1 / 30, d["SOC_DP_min"],
        d["delta_lambda_max"], d["lambda_floor"])
    assert d["lambda_table_max"] == 15.525
    assert lam == 15.525
    assert lam > 0.80


def test_floor_still_applies():
    lam = law6_lambda_eff(0.425, 30.0, 0.60, 0.01, 0.90, -10.0, 0.43)
    assert lam == 0.43


def test_provenance_pipe_join_matches_frozen_sha():
    """Original design-session serialization: '|'.join(sorted voyage:sha)."""
    import hashlib

    split = json.load(open(os.path.join(
        paths.MODELE_ROOT, "results", "dataset_v3", "formal",
        "split_assignment_v3.json"), encoding="utf-8"))
    led = json.load(open(os.path.join(
        paths.MODELE_ROOT, "results", "dataset_v3", "formal",
        "pass1_iid_ledger.json"), encoding="utf-8"))
    assign = split["assignment"]
    sha_list = [
        (r["voyage_id"], r["artifact_sha256"])
        for r in led["voyages"]
        if assign[r["split_group_id"]]["split"] == "train"
    ]
    assert len(sha_list) == 396
    assert len(set(sha_list)) == 396
    prov = hashlib.sha256(
        "|".join(f"{v}:{h}" for v, h in sorted(sha_list)).encode("utf-8")
    ).hexdigest()
    from t445.frozen_config import PROVENANCE_B1_NORM
    assert prov == PROVENANCE_B1_NORM


def test_native_json_and_old_stop_retained():
    native = os.path.join(paths.RESULTS_DIR, "lambda_envelope_r4_native.json")
    old = os.path.join(paths.RESULTS_DIR, "lambda_envelope_r4.json")
    assert os.path.isfile(old), "prior STOP artifact must remain"
    with open(native, encoding="utf-8") as fh:
        rec = json.load(fh)
    assert rec["status"] == "LAW6_NATIVE_ENVELOPE_CONFIRMED"
    assert rec["overall"] is True
    assert rec["checks"]["slx_checksum_unchanged"] is True
    with open(old, encoding="utf-8") as fh:
        stop = json.load(fh)
    assert stop["stop_code"] == "R4_CAP_NOT_LEGALLY_IMPLEMENTABLE"

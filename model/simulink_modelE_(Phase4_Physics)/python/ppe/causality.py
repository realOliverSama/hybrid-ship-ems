"""T-003 — C2 K/F + H1 channel-level causality verification (DGN-PPE-04/D7).

Machine-checks the frozen causality ledger against registry / producer
schema / validate_contracts / formal-voyage KF provenance *metadata*.
Consumes no controller outcomes.

Run: python -m ppe.causality   (requires numpy+scipy env, e.g. .venv_tf)
"""
from __future__ import annotations

import json
import os
import re
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from scipy.io import loadmat  # noqa: E402

from t445 import hashutil  # noqa: E402

from ppe import ledger as access_ledger, paths  # noqa: E402
from ppe.teval import load_corpus  # noqa: E402

REGISTRY = os.path.join(paths.CONFIG_DIR, "dataset_v3_signal_registry.json")
PRODUCER_SCHEMA = os.path.join(paths.CONFIG_DIR, "dataset_v3_producer_schema.json")
VALIDATE_CONTRACTS = os.path.join(_MODELE, "dataset_v3", "validate_contracts.py")
FORECAST_ISSUE = os.path.join(paths.SCRIPTS_DIR, "+mission_v3", "forecast_issue.m")
FORECAST_PROFILES = os.path.join(paths.CONFIG_DIR, "mission_v3",
                                 "forecast_quality_profiles.json")
PASS2_LEDGER = os.path.join(paths.MODELE_ROOT, "results", "dataset_v3",
                            "formal", "pass2_iid_ledger.json")

REQUIRED_H1 = [
    "P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
    "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
    "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct",
]
REQUIRED_K = ["v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW"]
REQUIRED_F = ["u_fcst_mps", "wl_fcst_m"]
ORACLE_COUNTERPART = {"u_fcst_mps": "u_actual_future_mps",
                      "wl_fcst_m": "wl_actual_future_m"}


class CausalityStop(RuntimeError):
    """Fail-closed: a required K/F channel cannot be defended."""


def _checks():
    return []

def _add(checks, name, ok, detail=""):
    checks.append({"name": name, "pass": bool(ok), "detail": detail})
    return bool(ok)


def _registry_index(reg: dict) -> dict:
    out = {}
    def walk(o):
        if isinstance(o, dict):
            if "name" in o and "role" in o and isinstance(o["name"], str):
                out.setdefault((o["name"], o["role"]), o)
            for v in o.values():
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)
    walk(reg)
    return out


def run() -> dict:
    os.makedirs(paths.QUAL_DIR, exist_ok=True)
    checks = _checks()

    led = json.load(open(paths.CAUSALITY_LEDGER_JSON, encoding="utf-8"))
    reg = json.load(open(REGISTRY, encoding="utf-8"))
    ridx = _registry_index(reg)

    # 1. ledger verdict / stop flag
    _add(checks, "ledger_verdict", led.get("verdict")
         == "K_F_CAUSALITY_DEFENDED_FROM_FROZEN_CONTRACTS", led.get("verdict"))
    _add(checks, "ledger_stop_triggered_false", led.get("stop_triggered") is False)

    # 2. channel completeness + registry match
    by_role = {}
    for c in led["channels"]:
        by_role.setdefault(c["role"], []).append(c["channel"])
    _add(checks, "H1_set_matches_registry",
         sorted(by_role.get("INPUT_H1", [])) == sorted(REQUIRED_H1),
         str(by_role.get("INPUT_H1")))
    _add(checks, "K_set", sorted(by_role.get("INPUT_K", [])) == sorted(REQUIRED_K))
    _add(checks, "F_set", sorted(by_role.get("INPUT_F", [])) == sorted(REQUIRED_F))
    _add(checks, "TARGET_present", "TARGET" in by_role)

    mism = []
    for c in led["channels"]:
        r = ridx.get((c["channel"], c["role"]))
        if r is None:
            mism.append(f"{c['channel']}:{c['role']} absent in registry")
            continue
        for f in ("unit", "source_signal", "aggregation",
                  "issue_time_semantics", "temporal_domain"):
            if str(r.get(f)) != str(c.get(f)):
                mism.append(f"{c['channel']}:{c['role']} field {f} "
                            f"ledger={c.get(f)} registry={r.get(f)}")
    _add(checks, "ledger_vs_registry_fields", not mism, "; ".join(mism[:5]))

    # 3. F channels: causality class + ORACLE counterpart + forbidden class
    for fc in REQUIRED_F:
        c = [x for x in led["channels"] if x["channel"] == fc][0]
        _add(checks, f"{fc}_class", c["causality_class"]
             == "legitimately_known_future_exogenous_plan")
        _add(checks, f"{fc}_oracle_forbidden",
             c.get("oracle_derived_forbidden") is True)
        oc = ORACLE_COUNTERPART[fc]
        r = ridx.get((oc, "ORACLE"))
        _add(checks, f"{fc}_oracle_counterpart",
             r is not None and r.get("predictor_access") == "oracle_only",
             f"{oc} predictor_access={None if r is None else r.get('predictor_access')}")
        _add(checks, f"{fc}_formal_class",
             c.get("formal_provenance_class_required") == "CAUSAL_GENERATOR")

    # 4. validate_contracts: ORACLE_DERIVED forbidden (static + semantic)
    src = open(VALIDATE_CONTRACTS, encoding="utf-8").read()
    _add(checks, "validate_contracts_forbids_oracle_derived",
         'PROVENANCE_CLASS_FORBIDDEN = ("ORACLE_DERIVED",)' in src)
    _add(checks, "validate_contracts_cutoff_invariant",
         "source_cutoff_time_s" in src and "issue_time_s" in src
         and "> issue_time_s" in src.replace(" ", "")
         or bool(re.search(r"source_cutoff_time_s\(.{0,40}> issue_time_s", src)))

    # 5. producer schema: cutoff field + invariant text
    ps = json.load(open(PRODUCER_SCHEMA, encoding="utf-8"))
    blob = json.dumps(ps, ensure_ascii=False)
    _add(checks, "producer_schema_cutoff_field", "source_cutoff_time_s" in blob)
    _add(checks, "producer_schema_cutoff_invariant",
         "source_cutoff_time_s ≤ issue_time_s" in blob
         or "source_cutoff_time_s <= issue_time_s" in blob)

    # 6. forecast generator identity: ledger hash == forecast_issue.m bytes
    fih = hashutil.sha256_file(FORECAST_ISSUE)
    prof = json.load(open(FORECAST_PROFILES, encoding="utf-8"))
    good = prof["profiles"]["GOOD_V1"]
    dg = prof["profiles"]["DEGRADED_V1"]
    _add(checks, "GOOD_V1_u_obs_observed",
         good["u_forecast"]["u_obs_source_class"] == "OBSERVED")
    _add(checks, "GOOD_V1_cutoff_lag_zero",
         good.get("source_cutoff_lag_s") == 0)
    _add(checks, "DEGRADED_V1_lag_1800",
         dg.get("delta_lag_s") == 1800 and dg.get("source_cutoff_lag_s") == 1800)
    _add(checks, "profiles_formal_class",
         prof["provenance"]["provenance_class_formal"] == "CAUSAL_GENERATOR")
    _add(checks, "profiles_oracle_forbidden_listed",
         "ORACLE_STATE" in prof["observability_forbidden"]
         and "FUTURE_TRUTH" in prof["observability_forbidden"])

    # 7. per-voyage formal KF provenance metadata (48 corpus voyages)
    corpus = load_corpus()
    pass2 = json.load(open(PASS2_LEDGER, encoding="utf-8"))
    arms = {v["voyage_id"]: v.get("arms_forecast") for v in pass2["voyages"]}
    n_prov_ok = 0
    gen_hash_seen = set()
    profile_dist = {}
    for r in corpus["rows"]:
        vid = r["voyage_id"]
        prof_id = arms.get(vid)
        if prof_id not in ("GOOD_V1", "DEGRADED_V1"):
            raise CausalityStop(f"{vid}: arms_forecast={prof_id}")
        profile_dist[prof_id] = profile_dist.get(prof_id, 0) + 1
        lag = 0.0 if prof_id == "GOOD_V1" else 1800.0
        m = loadmat(f"{paths.FORMAL_IID_DIR}/{vid}.mat", simplify_cells=True)
        fp = m["meta"]["forecast_provenance"]
        cut = float(fp["source_cutoff_time_s"])
        iss = float(fp["issue_time_s"])
        if not (cut <= iss):
            raise CausalityStop(f"{vid}: source_cutoff {cut} > issue {iss}")
        if abs(cut - (iss - lag)) > 1e-6:
            raise CausalityStop(
                f"{vid}: cutoff {cut} inconsistent with {prof_id} lag {lag} "
                f"at issue {iss}")
        if str(fp["forecast_provenance_class"]) != "CAUSAL_GENERATOR":
            raise CausalityStop(f"{vid}: provenance class "
                                f"{fp['forecast_provenance_class']}")
        gen_hash_seen.add(str(fp["forecast_generator_hash"]))
        access_ledger.log_access(
            artifact=f"datasets_v3/formal/iid/{vid}.mat",
            purpose="QUALIFICATION",
            split_contact="val-kf-provenance-metadata-only",
            note="T-003 KF provenance invariant check; no controller outcome",
        )
        n_prov_ok += 1
    _add(checks, "corpus_kf_provenance_48", n_prov_ok == 48,
         f"{n_prov_ok}/48")
    _add(checks, "corpus_profile_mixture_24_24",
         profile_dist == {"GOOD_V1": 24, "DEGRADED_V1": 24},
         str(profile_dist))
    _add(checks, "generator_hash_single_identity", len(gen_hash_seen) == 1,
         str(gen_hash_seen))
    _add(checks, "generator_hash_matches_file",
         gen_hash_seen == {fih}, f"file={fih} seen={gen_hash_seen}")

    overall = all(c["pass"] for c in checks)
    out = {
        "schema": "ppe_c2_causality_verification_v1",
        "task": "T-003",
        "overall": overall,
        "verdict": "PASS" if overall else "FAIL",
        "checks": checks,
        "forecast_issue_m_sha256": fih,
        "corpus_forecast_profile_distribution": profile_dist,
        "ledger_sha256": hashutil.sha256_file(paths.CAUSALITY_LEDGER_JSON),
    }
    hashutil.dump_json(
        f"{paths.QUAL_DIR}/c2_causality_verification.json", out)
    if not overall:
        raise CausalityStop("T-003 verification FAIL — escalate, do not zero")
    return out


if __name__ == "__main__":
    out = run()
    print("T-003 overall:", out["overall"])
    for c in out["checks"]:
        print(("PASS" if c["pass"] else "FAIL"), c["name"], c["detail"][:80])

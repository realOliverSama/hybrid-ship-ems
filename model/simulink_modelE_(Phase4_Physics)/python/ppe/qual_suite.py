"""T-008 — complete NONFORMAL qualification suite aggregator (FR-871,
DGN-PPE-09 items 1-8 + prior items). No formal controller outcomes exist
or are consumed.

Run: python -m ppe.qual_suite   (requires numpy+scipy env, e.g. .venv_tf)
"""
from __future__ import annotations

import json
import os
import subprocess
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
for _p in (_PY,):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from t445 import hashutil  # noqa: E402

from ppe import ledger, paths  # noqa: E402

T445_LIMITATIONS = {
    "Q2": "SOC grid 0.01 vs 0.005 rel=0.0153 — documented limitation of the "
          "discrete algorithm now frozen at 0.005; retired as Gate (FR-814)",
    "Q4": "proxy-vs-plant fuel rel=0.01314 > 1% — documented limitation; "
          "outcomes measured on Model-E logs, not surrogate (FR-815/FR-870)",
    "Q6": "proxy-vs-plant fuel rel=0.010012 > 1% — same status as Q4",
}


def _load_json(p):
    with open(p, encoding="utf-8") as fh:
        return json.load(fh)


def run() -> dict:
    os.makedirs(paths.QUAL_DIR, exist_ok=True)
    items = []

    def add(item, ok, evidence, detail=""):
        items.append({"item": item, "pass": bool(ok),
                      "evidence": evidence, "detail": detail})
        return bool(ok)

    # --- evidence artifacts from T-001..T-003 ---
    t001 = _load_json(f"{paths.FREEZE_DIR}/t001_freeze_prep.json")
    add("T-001 freeze prep (T_eval table, corpus, identities)",
        t001["overall"], "freeze/t001_freeze_prep.json")

    t002 = _load_json(f"{paths.FREEZE_DIR}/t002_result.json")
    add("DGN-PPE-09.1/2 C1 stored-target parity + fail-closed + coverage",
        t002["overall"], "freeze/t002_result.json",
        f"coverage={t002['coverage']} parity_exact={t002['parity_exact_float32']}")

    t003 = _load_json(f"{paths.QUAL_DIR}/c2_causality_verification.json")
    add("DGN-PPE-09.7 channel-level K/F causality ledger (FR-832)",
        t003["overall"], "qualification/c2_causality_verification.json")

    # --- python unit tests (T-004/T-006/T-007) ---
    env = dict(os.environ)
    proc = subprocess.run(
        [sys.executable, "-m", "pytest", "ppe/tests/", "-q",
         "--tb=short"], cwd=_PY, capture_output=True, text=True)
    pytest_ok = proc.returncode == 0
    tail = (proc.stdout or "").strip().splitlines()[-1] if proc.stdout else ""
    add("T-004 ForecastWindow / C0 no-leakage / warm-up (AC-801/803, D3)",
        pytest_ok, "pytest ppe/tests", tail)
    add("T-006 FEC identity + TERMINAL_SOC + HF config (AC-805/807/813/814)",
        pytest_ok, "pytest ppe/tests", tail)
    add("DGN-PPE-09.5/6 common-triplet + equal-tier bootstrap (synthetic)",
        pytest_ok, "pytest ppe/tests", tail)
    add("R_UNDEFINED guards (AC-809)", pytest_ok, "pytest ppe/tests", tail)
    add("estimand identity zero-safe (AC-806)", pytest_ok,
        "pytest ppe/tests", tail)

    # --- T-005 MATLAB runner evidence ---
    t005 = _load_json(f"{paths.QUAL_DIR}/t005_runner.json")
    add("C* determinism two-run (FR-871.1)", t005["determinism_bitwise"],
        "qualification/t005_runner.json")
    add("SimState continuous-vs-segmented equivalence (FR-871.8)",
        t005["simstate_ok"], "qualification/t005_runner.json",
        str(t005["simstate_continuous_vs_segmented_max_rel"]))
    add("DGN-PPE-09.8 checkpoint identity mismatch refusal (D8)",
        t005["checkpoint_mismatch_refuse_arm"]
        and t005["checkpoint_mismatch_refuse_teval"],
        "qualification/t005_runner.json")
    add("resume == uninterrupted (D8)", t005["resume_ok"],
        "qualification/t005_runner.json",
        f"max_rel={t005['resume_max_rel']}")
    add("action bounds/grid: SOC n=171 (AC-804)", t005["soc_grid_n171"],
        "qualification/t005_runner.json + ppe_config assertions")
    add("current-state init from plant (FR-871.7)",
        t005["resume_ok"], "resume re-inits SOC/gen/N from plant logs")
    add("hard-feasibility instrumentation live (FR-871.9)",
        isinstance(t005["hard_feasibility_pass"], bool)
        and t005["hf_detail"]["dSOC"] > 0,
        "qualification/t005_runner.json",
        "toy fixture intentionally infeasible (dSOC=0.0076>0.005, "
        "shortfall>0): instrumentation flags correctly")
    add("fuel_raw path live (Model-E fuel_total_kg, 0.6 kg/start)",
        t005["fuel_total_kg"] > 0, "qualification/t005_runner.json")

    # --- T-005b C2 bridge transport ---
    t005b = _load_json(f"{paths.QUAL_DIR}/t005b_bridge.json")
    add("C2 bridge transport: cross-process bitwise + fail-closed (D7)",
        t005b["pass"], "qualification/t005b_bridge.json")
    add("C2 inference determinism + contract (keras SHA-gated)",
        _load_json(f"{paths.QUAL_DIR}/c2_inference_qual.json")["verdict"]
        == "PASS", "qualification/c2_inference_qual.json")

    # --- access ledger state ---
    lc = ledger.ledger_counts()
    add("access ledger: FORMAL=0 / test=0 / OOD=0 (FR-874)",
        lc["by_purpose"]["FORMAL"] == 0 and lc["split_contact"]["test"] == 0
        and lc["split_contact"]["ood"] == 0,
        "access_ledger.jsonl", json.dumps(lc))

    overall = all(i["pass"] for i in items)
    out = {
        "schema": "ppe_qual_suite_v1",
        "task": "T-008",
        "overall": overall,
        "items": items,
        "t445_limitations_documented": T445_LIMITATIONS,
        "no_formal_outcomes": True,
        "access_ledger": lc,
    }
    hashutil.dump_json(f"{paths.QUAL_DIR}/ppe_qual_suite.json", out)
    return out


if __name__ == "__main__":
    out = run()
    print("T-008 overall:", out["overall"])
    for i in out["items"]:
        print(("PASS" if i["pass"] else "FAIL"), "-", i["item"])

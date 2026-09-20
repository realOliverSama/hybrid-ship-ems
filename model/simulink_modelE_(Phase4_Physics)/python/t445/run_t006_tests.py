#!/usr/bin/env python3
"""Write T-006 synthetic-test evidence. Does not consume real B-arm runs."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from datetime import datetime, timezone

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
else:
    from . import hashutil, paths


def main() -> int:
    cfg = json.load(open(paths.CONFIG_JSON, encoding="utf-8"))
    test_file = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "tests", "test_t006_gate.py")
    proc = subprocess.run(
        [sys.executable, "-m", "pytest", test_file, "-q"],
        cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        capture_output=True, text=True,
    )
    ok = proc.returncode == 0
    rec = {
        "schema": "t445_t006_evidence_v1",
        "task": "T-006",
        "purpose": "SYNTHETIC_SMOKE",
        "config_sha256": cfg["config_sha256"],
        "overall": ok,
        "pytest_stdout": proc.stdout.strip(),
        "pytest_returncode": proc.returncode,
        "wrote_formal_gate_result": False,
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    rec["record_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in rec.items() if k != "record_sha256"}
    )
    os.makedirs(paths.RESULTS_DIR, exist_ok=True)
    hashutil.dump_json(os.path.join(paths.RESULTS_DIR, "t006_unit_test_result.json"), rec)
    with open(paths.ACCESS_LEDGER, "a", encoding="utf-8") as fh:
        fh.write(json.dumps({
            "purpose": "SYNTHETIC_SMOKE",
            "task": "T-006",
            "artifact": "t006_unit_test_result.json",
            "utc": rec["utc"],
        }, sort_keys=True) + "\n")
    print(proc.stdout)
    if proc.stderr:
        print(proc.stderr, file=sys.stderr)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())

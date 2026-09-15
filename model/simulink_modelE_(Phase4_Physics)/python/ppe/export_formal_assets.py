"""Export formal per-voyage assets (update plans + C1 stored windows).

Deterministic, fail-closed, bound to the frozen T_eval table + C1 coverage
index. Reads the canonical store (FREEZE_PREP; the formal runs then consume
the exported per-voyage files whose SHAs are recorded here).

Run: python -m ppe.export_formal_assets   (requires numpy+scipy env)
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from scipy.io import savemat  # noqa: E402

from t445 import hashutil  # noqa: E402

from ppe import ledger, paths  # noqa: E402
from ppe.c1_index import C1TargetIndex  # noqa: E402
from ppe.formal_windows import build_update_plan, plan_sha256  # noqa: E402
from ppe.teval import WINDOWS_NPZ_SHA256  # noqa: E402

OUT_BASE = os.path.join(paths.MODELE_ROOT, "artifacts", "ppe_formal")


def run() -> dict:
    plans_dir = os.path.join(OUT_BASE, "plans")
    c1_dir = os.path.join(OUT_BASE, "c1_windows")
    os.makedirs(plans_dir, exist_ok=True)
    os.makedirs(c1_dir, exist_ok=True)

    teval = json.load(open(paths.TEVAL_TABLE_JSON, encoding="utf-8"))
    idx = C1TargetIndex()
    cov = json.load(open(f"{paths.FREEZE_DIR}/c1_coverage.json",
                         encoding="utf-8"))

    per_voyage = {}
    for row in teval["voyages"]:
        vid = row["voyage_id"]
        plan = build_update_plan(row)
        psha = plan_sha256(plan)
        plan_doc = {
            "schema": "ppe_update_plan_v1",
            "voyage_id": vid,
            "teval_row_sha256": hashutil.sha256_canonical(row),
            "plan": plan,
        }
        plan_doc["plan_sha256"] = psha
        hashutil.dump_json(os.path.join(plans_dir, f"{vid}.json"), plan_doc)

        anchors = idx.available_keys(vid)
        Y = np.stack([idx.get(vid, float(a)) for a in anchors], axis=0)
        c1f = os.path.join(c1_dir, f"{vid}.mat")
        savemat(c1f, {"anchors": anchors.reshape(-1, 1), "Y": Y,
                      "store_sha256": WINDOWS_NPZ_SHA256,
                      "voyage_id": vid})
        per_voyage[vid] = {
            "plan_sha256": psha,
            "c1_windows_sha256": hashutil.sha256_file(c1f),
            "n_anchors": int(anchors.size),
            "n_updates": len(plan),
        }

    out = {
        "schema": "ppe_formal_assets_v1",
        "teval_table_sha256": teval["teval_table_sha256"],
        "c1_store_sha256": cov["store_sha256"],
        "c1_coverage_index_sha256": cov["teval_table_sha256"],
        "n_voyages": len(per_voyage),
        "per_voyage": per_voyage,
    }
    out["assets_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in out.items() if k != "assets_sha256"})
    ledger.log_access(
        artifact="artifacts/ppe_formal/", purpose="FREEZE_PREP",
        split_contact="val-target-store-export",
        note="formal plan + C1 windows export (deterministic from frozen store)")
    hashutil.dump_json(f"{paths.FREEZE_DIR}/formal_assets.json", out)
    return out


if __name__ == "__main__":
    out = run()
    print("formal assets:", out["n_voyages"], "voyages;",
          "assets sha:", out["assets_sha256"][:16])

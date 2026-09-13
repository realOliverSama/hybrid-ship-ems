#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""freeze_trial_lists.py — E1 四架构 trial list 冻结驱动（T-008~T-011 前置）。

本地执行（非 result-bearing）：DGN-E-08 七步算法（枚举→canonicalize→容量过滤
→audit→锚点(E1 无)→HPO 种子无放回抽样→冻结 JSON）。预算上限 = training_budget.json
hpo.budget_caps。产物：results/predictor_v3/hpo/<arch>_trial_list.json +
<arch>_trial_generation_audit.json（入库）。
"""
import json
import os
import sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import hpo  # noqa: E402
from predictor_v3.models.common import load_s0  # noqa: E402


def main() -> int:
    budget = json.load(open(os.path.join(
        MODELE_ROOT, "config", "predictor_v3", "training_budget.json"),
        encoding="utf-8"))
    caps = budget["hpo"]["budget_caps"]
    s0 = load_s0()
    print(f"[trial_lists] s0={s0} caps={caps}", flush=True)
    summary = {}
    for arch in ("mlpq", "tcnq", "transformer", "tft"):
        doc, audit = hpo.generate_trial_list(arch, caps[arch], s0)
        summary[arch] = {
            "enumerated": audit["n_enumerated"],
            "capacity_valid": audit["n_capacity_valid"],
            "rejected": audit["n_rejected"],
            "subsampled_out": audit["n_subsampled_out"],
            "frozen": doc["n_trials"],
            "trial_list_sha256": hpo.sha256_file(os.path.join(
                hpo.HPO_DIR, f"{arch}_trial_list.json")),
        }
        print(f"  {arch}: {summary[arch]}", flush=True)
    rec = {"schema": "predictor_v3_e1_trial_list_summary_v1",
           "utc": datetime.now(timezone.utc).isoformat(), "archs": summary}
    rec["artifact_sha256"] = hpo.__dict__ and __import__("hashlib").sha256(
        json.dumps(rec, sort_keys=True, separators=(",", ":"),
                   ensure_ascii=False).encode()).hexdigest()
    with open(os.path.join(hpo.HPO_DIR, "trial_list_summary.json"), "w",
              encoding="utf-8") as fh:
        json.dump(rec, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    print("[trial_lists] frozen OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())

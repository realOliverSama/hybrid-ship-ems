#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""compare_two_run.py — Stage B two-pass payload-hash determinism gate.

Reads results/dataset_v3/formal/pass{1,2}_{iid,ood}_ledger.json and
verifies per-voyage payload_hash identity across the two passes
(FR-537/AC-438). Exit 0 = PASS, 1 = FAIL (machine gate semantics).

Used by both drivers (overnight_stage_b.sh heredoc equivalent /
overnight_stage_b.ps1). Run from the MODELE root or pass --res.
"""

import argparse
import json
import os
import sys

EXPECT = (("iid", 576), ("ood", 200))


def rows(path, key):
    with open(path, encoding="utf-8") as f:
        d = json.load(f)
    return {r["voyage_id"]: r["payload_hash"] for r in d["voyages"]}


def main(argv=None) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser(description="Stage B two-run determinism gate")
    ap.add_argument("--res", default=os.path.join(root, "results", "dataset_v3", "formal"))
    args = ap.parse_args(argv)

    bad = []
    for key, n in EXPECT:
        a = rows(os.path.join(args.res, f"pass1_{key}_ledger.json"), key)
        b = rows(os.path.join(args.res, f"pass2_{key}_ledger.json"), key)
        assert len(a) == n and len(b) == n, f"{key} count {len(a)}/{len(b)} != {n}"
        diff = [v for v in a if a[v] != b.get(v)]
        print(f"{key}: {n} voyages, payload-hash identical {n - len(diff)}/{n}")
        bad += [f"{key}:{v}" for v in diff]
    if bad:
        print("DETERMINISM FAIL:", bad[:10])
        return 1
    print("DETERMINISM PASS: payload hashes identical across passes")
    return 0


if __name__ == "__main__":
    sys.exit(main())

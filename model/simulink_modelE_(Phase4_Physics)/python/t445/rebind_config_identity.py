#!/usr/bin/env python3
"""Rebind T-002/T-003/T-005 evidence to the post-Amendment-C config SHA.

Does not change scientific payloads. Does not rewrite T-007 FAIL/ladder
artifacts. Corpus selection identity is asserted unchanged.
"""
from __future__ import annotations

import json
import os
import sys
from datetime import datetime, timezone

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
    from t445.corpus_select import corpus_identity_sha256  # type: ignore
    from t445.frozen_config import SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C  # type: ignore
else:
    from . import hashutil, paths
    from .corpus_select import corpus_identity_sha256
    from .frozen_config import SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C

REBIND_NOTE = (
    "Scientific evidence not rerun. Evidence rebound to post-Amendment-C "
    "config identity (nominal-SOC FEC k_eq + matching w_term). Old SHA retained "
    "for provenance."
)

EVIDENCE_FILES = [
    "t002_unit_test_result.json",
    "t003_complete.json",
    "t003_b0_regression.json",
    "t003_b1_core.json",
    "t003_10b_smoke.json",
    "t003_control_gate_quick.json",
    "b1_norm_repro.json",
    "lambda_envelope_r4_native.json",
]


def _load(path: str) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def rebind_file(path: str, new_sha: str) -> dict:
    rec = _load(path)
    old = rec.get("config_sha256")
    rec["superseded_config_sha256"] = rec.get(
        "superseded_config_sha256", SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C)
    rec["config_sha256"] = new_sha
    rec["rebind_note"] = REBIND_NOTE
    rec["rebind_utc"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    rec["science"] = rec.get("science", "UNAFFECTED_BY_AMENDMENT_C")
    hashutil.dump_json(path, rec)
    return {"path": paths.rel(path), "old": old, "new": new_sha}


def rebind_corpus(new_sha: str) -> dict:
    path = os.path.join(paths.RESULTS_DIR, "corpus_manifest.json")
    man = _load(path)
    before = corpus_identity_sha256(man)
    old_cfg = man.get("config_sha256")
    man["config_sha256"] = new_sha
    man["superseded_config_sha256"] = SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C
    man["rebind_note"] = REBIND_NOTE
    man["corpus_identity_sha256"] = before
    man["science"] = "UNAFFECTED_BY_AMENDMENT_C"
    man["manifest_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in man.items() if k != "manifest_sha256"}
    )
    after = corpus_identity_sha256(man)
    if before != after:
        raise SystemExit(
            f"corpus identity changed: {before} -> {after}")
    hashutil.dump_json(path, man)
    return {
        "path": paths.rel(path),
        "old_config": old_cfg,
        "new_config": new_sha,
        "corpus_identity_sha256_before": before,
        "corpus_identity_sha256_after": after,
        "identity_unchanged": before == after,
    }


def main() -> int:
    cfg = _load(paths.CONFIG_JSON)
    new_sha = cfg["config_sha256"]
    if new_sha == SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C:
        raise SystemExit("config SHA has not changed; freeze T-001 first")
    rows = [rebind_file(os.path.join(paths.RESULTS_DIR, name), new_sha)
            for name in EVIDENCE_FILES]
    corpus = rebind_corpus(new_sha)
    rec = {
        "schema": "t445_amendment_c_rebind_v1",
        "task": "T-001",
        "purpose": "QUALIFICATION",
        "config_sha256": new_sha,
        "superseded_config_sha256": SUPERSEDED_CONFIG_SHA256_PRE_AMENDMENT_C,
        "rebound": rows,
        "corpus": corpus,
        "preserved_unrebound": [
            "qualification/t007_keq_charge_restoration.json",
            "qualification/t007_energy_ladder.json",
            "lambda_envelope_r4.json",
        ],
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    rec["record_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in rec.items() if k != "record_sha256"}
    )
    out = os.path.join(paths.RESULTS_DIR, "amendment_c_rebind.json")
    hashutil.dump_json(out, rec)
    print(json.dumps({
        "config_sha256": new_sha,
        "n_rebound": len(rows),
        "corpus_identity_unchanged": corpus["identity_unchanged"],
        "corpus_identity_sha256": corpus["corpus_identity_sha256_after"],
        "wrote": paths.rel(out),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

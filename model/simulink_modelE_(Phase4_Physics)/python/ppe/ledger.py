"""PPE access ledger (FR-874): append-only JSONL with purpose tags.

Purpose tags: FREEZE_PREP / QUALIFICATION / SYNTHETIC_SMOKE / FORMAL.
FORMAL entries are forbidden before T-009 freeze overall=true (AC-810).
"""
from __future__ import annotations

import json
import os
from datetime import datetime, timezone

from . import paths

PURPOSES = ("FREEZE_PREP", "QUALIFICATION", "SYNTHETIC_SMOKE", "FORMAL")


def log_access(artifact: str, purpose: str, split_contact: str, note: str) -> dict:
    if purpose not in PURPOSES:
        raise ValueError(f"purpose 必须属于 {PURPOSES}，实际 {purpose}")
    if purpose == "FORMAL":
        raise PermissionError(
            "FORMAL access before PRE_EXECUTION_FREEZE_GATE is forbidden (AC-810)")
    if os.environ.get("PPE_LEDGER_QUIET") == "1":
        return {"skipped": "quiet"}   # freeze-internal pytest fan-out
    entry = {
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "actor": "ppe-main-agent",
        "artifact": artifact,
        "purpose": purpose,
        "split_contact": split_contact,
        "note": note,
    }
    os.makedirs(os.path.dirname(paths.ACCESS_LEDGER), exist_ok=True)
    with open(paths.ACCESS_LEDGER, "a", encoding="utf-8") as fh:
        fh.write(json.dumps(entry, ensure_ascii=False, sort_keys=True) + "\n")
    return entry


def ledger_counts() -> dict:
    counts = {p: 0 for p in PURPOSES}
    split = {"test": 0, "ood": 0}
    n = 0
    if os.path.exists(paths.ACCESS_LEDGER):
        with open(paths.ACCESS_LEDGER, encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                e = json.loads(line)
                counts[e["purpose"]] = counts.get(e["purpose"], 0) + 1
                sc = str(e.get("split_contact", "")).lower()
                if "test" in sc:
                    split["test"] += 1
                if "ood" in sc:
                    split["ood"] += 1
                n += 1
    return {"n_entries": n, "by_purpose": counts, "split_contact": split}

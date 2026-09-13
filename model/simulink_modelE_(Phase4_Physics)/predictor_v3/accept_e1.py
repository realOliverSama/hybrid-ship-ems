#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""accept_e1.py — E1 自动验收证据生成（T-008~T-011；主 Agent 验收的机器核对层）。

逐架构核对（裁定 Closure Checklist 的机器可查部分）：
 1. executed trial_id 集合 ⊆ 冻结 trial list；
 2. budget_consumed（唯一 trial_id 去重）≤ budget_cap；
 3. eligible_trial_count ≥ 1（==0 即 STOP 标记）；
 4. selected trial = 冻结目标（val sg_macro pinball）最优 eligible；
 5. selected_config.json 自哈希复算 + trial_list/registry hash 对照；
 6. registry 中无 test/OOD 科学访问（访问日志审计——dev/campaign 分片仅 train/val
    或带 terminal_six_checks_passed 的条目）。
产物 results/predictor_v3/e1_auto_acceptance.json（self-hash）。
最终裁定（spec_task_check + 任一 STOP 触发处置）仍属主 Agent。
"""
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
HPO_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "hpo")
CAND_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "candidates")
DATA_ACCESS = os.path.join(MODELE_ROOT, "results", "predictor_v3", "data_access")

CAPS = {"mlpq": 4, "tcnq": 20, "transformer": 20, "tft": 18}


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def audit_access_logs() -> dict:
    """访问日志审计：dev 分片只允许 train/val；terminal 条目须带六核验通过标记。"""
    bad = []
    if os.path.isdir(DATA_ACCESS):
        for root, _dirs, files in os.walk(DATA_ACCESS):
            for fn in files:
                if not fn.endswith(".jsonl"):
                    continue
                p = os.path.join(root, fn)
                with open(p, encoding="utf-8") as fh:
                    for line in fh:
                        try:
                            e = json.loads(line)
                        except json.JSONDecodeError:
                            bad.append({"file": os.path.relpath(p, MODELE_ROOT),
                                        "issue": "unparsable"})
                            continue
                        sp = e.get("split", "")
                        if sp in ("test", "ood") and \
                                e.get("note") != "terminal_six_checks_passed":
                            bad.append({"file": os.path.relpath(p, MODELE_ROOT),
                                        "entry": e, "issue": "unsealed_access"})
    return {"unsealed_or_suspicious": bad, "count": len(bad)}


def audit_arch(arch: str) -> dict:
    checks, ok = [], lambda name, passed, detail="": checks.append(
        {"name": name, "pass": bool(passed), "detail": str(detail)[:200]})

    list_path = os.path.join(HPO_DIR, f"{arch}_trial_list.json")
    reg_path = os.path.join(HPO_DIR, f"{arch}_trials.jsonl")
    ok("trial_list_exists", os.path.isfile(list_path), list_path)
    ok("registry_exists", os.path.isfile(reg_path), reg_path)
    if not (os.path.isfile(list_path) and os.path.isfile(reg_path)):
        return {"arch": arch, "checks": checks, "overall": False}

    with open(list_path, encoding="utf-8") as fh:
        tl = json.load(fh)
    frozen_ids = {t["trial_id"] for t in tl["trials"]}
    rows = []
    with open(reg_path, encoding="utf-8") as fh:
        for line in fh:
            if line.strip():
                rows.append(json.loads(line))
    executed = {r["trial_id"] for r in rows}
    unknown = executed - frozen_ids
    ok("executed_subset_of_frozen_list", not unknown, f"unknown={sorted(unknown)[:5]}")

    consumed_ids = {r["trial_id"] for r in rows
                    if r["status"] in ("COMPLETED_ELIGIBLE",
                                       "COMPLETED_INELIGIBLE", "FAILED")}
    ok("budget_within_cap", len(consumed_ids) <= CAPS[arch],
       f"consumed={len(consumed_ids)} cap={CAPS[arch]}")

    eligible = [r for r in rows if r["status"] == "COMPLETED_ELIGIBLE"
                and r.get("val", {}).get("pinball_sgmacro") is not None
                and str(r["val"]["pinball_sgmacro"]).lower() not in ("nan", "inf")]
    ok("eligible_ge_1", len(eligible) >= 1, f"eligible={len(eligible)}"
       + ("（==0 ⇒ STOP 升级）" if not eligible else ""))

    sel_path = os.path.join(CAND_DIR, arch, "selected_config.json")
    ok("selected_config_exists", os.path.isfile(sel_path), sel_path)
    if os.path.isfile(sel_path) and eligible:
        with open(sel_path, encoding="utf-8") as fh:
            sel = json.load(fh)
        body = {k: v for k, v in sel.items() if k != "artifact_sha256"}
        ok("selected_config_self_hash",
           hashlib.sha256(canonical_json(body)).hexdigest() == sel.get("artifact_sha256"))
        best = min(eligible, key=lambda r: r["val"]["pinball_sgmacro"])
        ok("selected_is_frozen_objective_best",
           sel.get("selected_trial_id") == best["trial_id"],
           f"selected={sel.get('selected_trial_id')} best={best['trial_id']}")
        ok("selected_within_cap",
           int(sel.get("budget_consumed", 10 ** 9)) <= CAPS[arch],
           f"consumed={sel.get('budget_consumed')}")
        ok("trial_list_hash_recorded",
           sel.get("trial_list_sha256") == sha256_file(list_path))

    overall = all(c["pass"] for c in checks)
    return {"arch": arch, "checks": checks, "overall": overall}


def main() -> int:
    archs = {}
    for arch in CAPS:
        archs[arch] = audit_arch(arch)
    access = audit_access_logs()
    overall = all(a["overall"] for a in archs.values()) and access["count"] == 0
    doc = {
        "schema": "predictor_v3_e1_auto_acceptance_v1",
        "utc": datetime.now(timezone.utc).isoformat(),
        "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
        "archs": archs,
        "access_log_audit": access,
        "overall": overall,
        "note": "机器核对层；最终裁定（spec_task_check/STOP 处置）属主 Agent。",
    }
    doc["artifact_sha256"] = hashlib.sha256(
        canonical_json({k: v for k, v in doc.items()
                        if k != "artifact_sha256"})).hexdigest()
    out = os.path.join(MODELE_ROOT, "results", "predictor_v3",
                       "e1_auto_acceptance.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    print(f"[accept_e1] overall={overall} → {os.path.relpath(out, MODELE_ROOT)}")
    for arch, a in archs.items():
        for c in a["checks"]:
            if not c["pass"]:
                print(f"  FAIL {arch}: {c['name']} {c['detail']}")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())

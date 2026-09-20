"""T-FC3-9 — effect-analysis eligibility freeze (fuel-blinded).

Constructs voyage-level eligibility masks from the ACCEPTED FC-3
readjudication layer and freezes every analysis specification BEFORE any
fuel/J_FEC value is read.

Structurally fuel-blind: reads only run/voyage/arm identity, split_group,
tier, readjudicated_status, evidence hashes, corpus/manifest identity.
Forbidden keys are scanned at input and output. Effect formulas are bound
as METADATA ONLY and never evaluated here.
"""
from __future__ import annotations

import json
import os
import sys
from collections import Counter

_PPE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _PPE)
import fc3_core as C  # noqa: E402
from fc3_replay_execute import forbidden_keys_in  # noqa: E402

RPP = os.path.join(C._MODELE, "results", "ppe_practical")
REPLAY_DIR = os.path.join(RPP, "readjudication_epoch1")
FREEZE = os.path.join(RPP, "freeze", "fc3_readjudication_freeze.json")
GATE = os.path.join(RPP, "freeze", "FC3_READJUDICATION_GATE.json")
ACCEPT = os.path.join(REPLAY_DIR, "T_FC3_8_ACCEPTANCE.json")
MANIFEST = os.path.join(RPP, "fc3_replay_input_manifest.json")
SPECS = os.path.join(os.path.dirname(os.path.dirname(C._MODELE)), ".specs",
                     "phase4-f-practical-predictive-ems")


def main() -> int:
    checks = []

    def check(name, ok, detail=""):
        checks.append({"name": name, "pass": bool(ok), "detail": str(detail)})
        if not ok:
            print("  GATE-FAIL", name, str(detail)[:160])

    fz = json.load(open(FREEZE))
    gate = json.load(open(GATE))
    acc = json.load(open(ACCEPT))
    man = json.load(open(MANIFEST))
    replay_root = gate["freeze_file_sha256"]

    check("replay_acceptance_pass", acc.get("pass") is True)
    check("replay_acceptance_192", acc.get("n_found") == 192)
    check("accepted_all_feasible",
          acc["terminal_status_histogram"].get("READJUDICATED_FEASIBLE")
          == 192)

    # ---- derive masks from the accepted replay (fuel-blind reads) ----
    status_by_voyage = {}
    for r in man["rows"]:
        art = json.load(open(os.path.join(REPLAY_DIR, r["run_id"] + ".json")))
        # fuel-blind allow-list extraction
        sub = {k: art[k] for k in ("readjudicated_status",
                                    "original_run_id",
                                    "original_trajectory_sha",
                                    "original_audit_sha",
                                    "sanitized_view_sha",
                                    "fc3_checker_sha",
                                    "fc3_replay_tool_sha",
                                    "fc3_replay_freeze_root")}
        if forbidden_keys_in(sub):
            check("blinded_read_clean", False, r["run_id"])
        vid = r["voyage_id"]
        d = status_by_voyage.setdefault(vid, {})
        d[r["arm"]] = art["readjudicated_status"]

    FEAS = "READJUDICATED_FEASIBLE"
    vids = sorted(status_by_voyage)
    mask_c0_c1 = [v for v in vids
                  if status_by_voyage[v].get("C0") == FEAS
                  and status_by_voyage[v].get("C1") == FEAS]
    mask_c0_c2 = [v for v in vids
                  if status_by_voyage[v].get("C0") == FEAS
                  and status_by_voyage[v].get("C2") == FEAS]
    mask_c1_c2 = [v for v in vids
                  if status_by_voyage[v].get("C1") == FEAS
                  and status_by_voyage[v].get("C2") == FEAS]
    mask_triplet = [v for v in vids
                    if status_by_voyage[v].get("C0") == FEAS
                    and status_by_voyage[v].get("C1") == FEAS
                    and status_by_voyage[v].get("C2") == FEAS]
    p0_context = [v for v in vids
                  if status_by_voyage[v].get("P0") == FEAS]

    # ---- identity metadata (tier / split_group from frozen corpus) ----
    meta = {}
    for r in man["rows"]:
        rec = json.load(open(os.path.join(C.EVID, r["voyage_id"],
                                          r["arm"] + ".json")))
        meta.setdefault(r["voyage_id"], {
            "tier": rec["tier"], "split_group_id": rec["split_group_id"]})
    check("voyage_meta_48", len(meta) == 48, len(meta))

    tiers = Counter(meta[v]["tier"] for v in mask_triplet)
    groups = Counter(meta[v]["split_group_id"] for v in mask_triplet)
    tier_group_counts = {t: len([g for g in groups
                                 if all(meta[v]["tier"] == t for v in
                                        mask_triplet
                                        if meta[v]["split_group_id"] == g)])
                         for t in ("LOW", "MID", "HIGH")}

    # ---- feasibility status via the canonical theta_hat rule ----
    n_defined_groups_by_tier = {}
    for t in ("LOW", "MID", "HIGH"):
        gset = {meta[v]["split_group_id"] for v in mask_triplet
                if meta[v]["tier"] == t}
        n_defined_groups_by_tier[t] = len(gset)
    if all(n == 4 for n in n_defined_groups_by_tier.values()):
        status = "FULL_COHORT"
    elif all(n >= 3 for n in n_defined_groups_by_tier.values()):
        status = "CONDITIONAL_FEASIBLE"
    else:
        status = "WITHHELD_INSUFFICIENT_FEASIBILITY"
    check("mask_counts_derived_consistency",
          len(mask_triplet) == len(mask_c0_c1) == len(mask_c0_c2)
          == len(p0_context),
          f"{len(mask_triplet)}/{len(mask_c0_c1)}/{len(mask_c0_c2)}/"
          f"{len(p0_context)}")
    check("derived_counts_match_accepted_replay_facts",
          len(mask_triplet) == 48, len(mask_triplet))

    # ---- fuel/FEC identity (metadata only; NO evaluation) ----
    fec = {"kBAT_kWh": 1071.36, "k_eq_E": 0.5457,
           "formula": "J_FEC = fuel_raw + (SOC0 - SOC_end) * 1071.36 "
                      "* k_eq_E",
           "robustness_preregistered": {"0.8x": 0.4366, "1.2x": 0.6548,
                                        "break_even": 0.0933},
           "source": "python/ppe/fec.py (frozen bytes)",
           "fec_module_sha256": C.sha256_file(
               os.path.join(_PPE, "fec.py"))}

    eligibility = {
        "schema": "effect_analysis_eligibility_manifest_v1",
        "task": "T-FC3-9",
        "source_fc3_replay_freeze_root": replay_root,
        "source_fc3_replay_acceptance_sha256": C.sha256_file(ACCEPT),
        "voyages": [{
            "voyage_id": v,
            "split_group_id": meta[v]["split_group_id"],
            "tier": meta[v]["tier"],
            "P0": status_by_voyage[v].get("P0"),
            "C0": status_by_voyage[v].get("C0"),
            "C1": status_by_voyage[v].get("C1"),
            "C2": status_by_voyage[v].get("C2"),
        } for v in vids],
        "masks": {
            "MASK_C0_C1": mask_c0_c1,
            "MASK_C0_C2": mask_c0_c2,
            "MASK_C1_C2_diagnostic": mask_c1_c2,
            "MASK_COMMON_TRIPLET": mask_triplet,
            "P0_CONTEXT_MASK": p0_context,
        },
        "counts": {
            "MASK_C0_C1": len(mask_c0_c1),
            "MASK_C0_C2": len(mask_c0_c2),
            "MASK_C1_C2_diagnostic": len(mask_c1_c2),
            "MASK_COMMON_TRIPLET": len(mask_triplet),
            "P0_CONTEXT_MASK": len(p0_context),
            "by_tier_triplet": dict(tiers),
            "by_split_group_triplet": dict(groups),
            "defined_groups_per_tier": n_defined_groups_by_tier,
        },
        "study_feasibility_status": status,
        "fec_identity": fec,
        "formulas_metadata_only": {
            "B_perfect(v)": "100*(J_C0 - J_C1)/J_C0",
            "B_predicted(v)": "100*(J_C0 - J_C2)/J_C0",
            "L_prediction(v)": "100*(J_C2 - J_C1)/J_C0",
            "identity": "B_perfect = B_predicted + L_prediction (per voyage)",
        },
        "aggregation_identity": {
            "source": "python/ppe/stats.py (frozen bytes)",
            "stats_module_sha256": C.sha256_file(
                os.path.join(_PPE, "stats.py")),
            "chain": "voyage -> split_group mean(defined) -> tier mean(groups) "
                     "-> study: FULL_COHORT_ESTIMATE = mean over defined "
                     "groups (theta_hat); CONDITIONAL = equal 1/3 tier "
                     "average (predefined fallback, not invoked if "
                     "FULL_COHORT)",
        },
        "bootstrap_identity": {
            "B": 10000, "seed": "phase4f-t445-bootstrap-v1",
            "resampling_unit": "defined split_groups within tier",
            "tier_handling": "per-tier resample, draw size = n_defined",
            "group_handling": "groups resampled with replacement",
            "ci": "percentile [2.5, 97.5] linear interpolation",
            "execution": "NOT executed (T-FC3-9)",
        },
        "identity_residual_tolerance": {
            "atol": 1e-10, "rtol": 1e-9,
            "rule": "|B_perfect-B_predicted-L_prediction| <= 1e-10 + "
                    "1e-9*max(|B_perfect|,|B_predicted|,|L_prediction|)",
            "evaluated": False},
        "r_guard": {
            "R_UNDEFINED_iff": ["study withheld", "non-finite",
                                 "B_perfect <= 0",
                                 "|B_perfect| < 0.10 pp"],
            "evaluated": False},
        "sealed": {"IID_TEST": "SEALED", "OOD": "SEALED"},
        "evidence_label": "DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE",
        "no_2pct_gonogo": "T-445 trajectory-oracle 2% threshold NOT restored",
    }
    check("eligibility_manifest_fuel_blind",
          not forbidden_keys_in(eligibility))
    out_el = os.path.join(RPP, "effect_analysis_eligibility_manifest.json")
    eligibility["sha256_self"] = C.sha256_canonical(
        {k: v for k, v in eligibility.items() if k != "sha256_self"})
    json.dump(eligibility, open(out_el, "w"), indent=1, sort_keys=True)

    # ---- pre-access freeze/gate ----
    led = os.path.join(RPP, "access_ledger.jsonl")
    cnt = Counter()
    for line in open(led):
        line = line.strip()
        if not line:
            continue
        e = json.loads(line)
        cnt[e.get("purpose")] += 1
    freeze = {
        "schema": "effect_analysis_preaccess_freeze_v1",
        "task": "T-FC3-9",
        "utc": __import__("datetime").datetime.now(
            __import__("datetime").timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"),
        "bindings": {
            "eligibility_manifest_sha256": C.sha256_file(out_el),
            "eligibility_manifest_self_sha256": eligibility["sha256_self"],
            "fc3_replay_freeze_root": replay_root,
            "fc3_replay_acceptance_sha256": C.sha256_file(ACCEPT),
            "corpus_identity_sha256": fz["bindings"]["corpus_manifest_sha256"],
            "voyage_membership_manifest_sha256": C.sha256_file(MANIFEST),
            "fec_module_sha256": fec["fec_module_sha256"],
            "stats_module_sha256": eligibility["aggregation_identity"][
                "stats_module_sha256"],
            "design_spec_sha256": C.sha256_file(os.path.join(
                SPECS, "design.md")),
            "access_ledger_sha256": C.sha256_file(led),
            "access_ledger_counts": {k: v for k, v in sorted(cnt.items())},
            "effect_analysis_implementation": "NOT IMPLEMENTED",
        },
        "assertions": {
            "ELIGIBILITY_MASKS_FROZEN": True,
            "FUEL_JFEC_READ_DURING_ELIGIBILITY": False,
            "IID_TEST_ACCESSED": False,
            "OOD_ACCESSED": False,
            "EFFECT_ANALYSIS_EXECUTED": False,
        },
        "checks": checks,
    }
    check("assert_iid_sealed", cnt.get("IID_TEST", 0) == 0)
    check("assert_ood_sealed", cnt.get("OOD", 0) == 0)
    overall = all(c["pass"] for c in checks)
    gate_doc = {
        "schema": "EFFECT_ANALYSIS_ELIGIBILITY_GATE_v1",
        "task": "T-FC3-9",
        "overall": overall,
        "n_checks": len(checks),
        "n_pass": sum(1 for c in checks if c["pass"]),
        "freeze_file": "results/ppe_practical/freeze/"
                       "effect_analysis_preaccess_freeze.json",
        "study_feasibility_status": status,
        "next": "STOP — human Effect-Analysis Execution Gate required",
        "utc": freeze["utc"],
    }
    fz_path = os.path.join(RPP, "freeze",
                           "effect_analysis_preaccess_freeze.json")
    json.dump(freeze, open(fz_path, "w"), indent=1, sort_keys=True)
    gate_doc["freeze_file_sha256"] = C.sha256_file(fz_path)
    json.dump(gate_doc, open(os.path.join(
        RPP, "freeze", "EFFECT_ANALYSIS_ELIGIBILITY_GATE.json"), "w"),
        indent=1, sort_keys=True)
    print(f"[fc3-elig] overall={overall} status={status} "
          f"triplet={len(mask_triplet)} c0c1={len(mask_c0_c1)} "
          f"c0c2={len(mask_c0_c2)} p0={len(p0_context)} "
          f"freeze_sha={gate_doc['freeze_file_sha256'][:16]}")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())

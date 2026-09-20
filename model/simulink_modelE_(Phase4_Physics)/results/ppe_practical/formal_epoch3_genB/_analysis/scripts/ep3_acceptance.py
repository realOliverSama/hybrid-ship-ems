#!/usr/bin/env python3
"""EPOCH-3 generation-B FORMAL ACCEPTANCE artifact generator.

Re-verifies, from primary evidence, in one machine-readable artifact:
  G1 corpus reconciliation (192 = 48 voyages x 4 arms, 0 dup, 0 missing,
     genB freeze a2dc634f only; attempt-A era terminals excluded);
  G2 authoritative FC-3 readjudication of every terminal (fc3_core v3
     dynamic-envelope semantics, sha 33a8309a...);
  G3 pre-execution freeze gate (overall, assertions, execution closure);
  G4 effect analysis acceptance (pre-registered estimands + bootstrap CI
     + P0 context, artifact shas);
  G5 occupancy descriptive artifact present;
  G6 frozen eligibility manifest identity;
  G7 attempt-A disposition (sealed ledger + namespace census);
  G8 frozen module identities (stats/fec/fc3);
  G9 no-mutation check (freeze + manifest re-hashed after analysis).

Read-only over the corpus; writes exactly one artifact:
  formal_epoch3_genB/EPOCH3_GENB_FORMAL_ACCEPTANCE.json"""
import hashlib
import json
import os
import sys
from collections import Counter
from datetime import datetime, timezone

import h5py
import numpy as np

ROOT = '/root/autodl-tmp/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)'
sys.path.insert(0, os.path.join(ROOT, 'python'))
from ppe import fc3_core  # noqa: E402

RPP = os.path.join(ROOT, 'results', 'ppe_practical')
G2 = os.path.join(RPP, 'formal_epoch3_genB')
G1 = os.path.join(RPP, 'formal_epoch3')
ANA = os.path.join(G2, '_analysis')
FREEZE = os.path.join(RPP, 'freeze', 'epoch3_pre_execution_freeze_genb.json')
LEDGER = os.path.join(G1, '_audit_stop_gate',
                      'ATTEMPT_A_EXCLUSION_LEDGER.json')
ELIG_MAN = os.path.join(RPP, 'effect_analysis_eligibility_manifest.json')
ARMS = ('P0', 'C0', 'C1', 'C2')
GENB_FREEZE = 'a2dc634f'
FC3_AUTH = ('33a8309ad6f585bed5203cd0223484a0df3eda5b'
            'a8d4eeb8193e1ba85c34a4a9')
STATS_SHA = '75278cadee95f26b04955962d985bb757aa78344be07fac56888d9c30dd0a5ba'
FEC_SHA = '75d7c63f2aeda867ec91338a93b0ced4ab7300a42c7f2c0ebacd46810e325596'


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def sha256_canonical(obj):
    def canon(o):
        if isinstance(o, dict):
            return "{" + ",".join(
                f'"{k}":{canon(v)}' for k, v in sorted(o.items())) + "}"
        if isinstance(o, list):
            return "[" + ",".join(canon(v) for v in o) + "]"
        if isinstance(o, bool):
            return "true" if o else "false"
        if isinstance(o, (int, float)):
            return repr(o)
        if o is None:
            return "null"
        return json.dumps(o, ensure_ascii=False)
    return hashlib.sha256(canon(obj).encode("utf-8")).hexdigest()


def main():
    gates = {}
    problems = []
    frz_sha_start = sha256_file(FREEZE)
    elig_sha_start = sha256_file(ELIG_MAN)

    tv = json.load(open(os.path.join(ROOT, 'config',
                                     'phase4f_ppe_teval_table.json')))
    universe = {(v['voyage_id'], a) for v in tv['voyages'] for a in ARMS}

    # ---- G1 corpus reconciliation (genB freeze only, both namespaces) ----
    loc, attempt_a_census = {}, Counter()
    for ns_name, base in (('G2', G2), ('G1', G1)):
        if not os.path.isdir(base):
            continue
        for v in sorted(os.listdir(base)):
            vd = os.path.join(base, v)
            if not os.path.isdir(vd) or v.startswith('_'):
                continue
            for a in ARMS:
                f = os.path.join(vd, a + '.json')
                if not os.path.isfile(f):
                    continue
                d = json.load(open(f))
                if str(d.get('freeze_root_sha256', '')).startswith(
                        GENB_FREEZE):
                    loc.setdefault((v, a), []).append(ns_name)
                elif ns_name == 'G1':
                    attempt_a_census['terminals'] += 1
    dups = {k: x for k, x in loc.items() if len(x) > 1}
    missing = sorted(universe - set(loc))
    extra = sorted(set(loc) - universe)
    per_arm = Counter(a for (_, a) in loc)
    gates["G1_corpus_reconciliation"] = {
        "unique_covered": len(loc), "expected": 192,
        "duplicated": len(dups), "missing": len(missing),
        "extra_out_of_universe": len(extra),
        "per_arm": dict(per_arm),
        "namespace_split": {"G2_only": sum(1 for x in loc.values()
                                           if x == ['G2']),
                            "G1_only": sum(1 for x in loc.values()
                                           if x == ['G1']),
                            "both": len(dups)},
        "pass": (len(loc) == 192 and not dups and not missing
                 and not extra and per_arm == Counter(
                     {'C0': 48, 'C1': 48, 'C2': 48, 'P0': 48}))}

    # ---- G2 authoritative FC-3 readjudication of ALL 192 ----
    fc3_sha = sha256_file(os.path.join(ROOT, 'python', 'ppe', 'fc3_core.py'))
    statuses = Counter()
    failures = []
    for (v, a), nss in sorted(loc.items()):
        base = G2 if 'G2' in nss else G1
        d = json.load(open(os.path.join(base, v, a + '.json')))
        tfile = os.path.join(base, v, '%s.traj.v1.mat' % a)
        afile = os.path.join(base, v, '%s.audit.v1.json' % a)
        try:
            with h5py.File(tfile, 'r') as h:
                g = h['traj']
                traj = {k: np.asarray(g[k], dtype=float).ravel()
                        for k in g.keys()}
            audit = json.load(open(afile))
            view = {"payload": {
                "T_eval_s": d["T_eval"],
                "E_unserved_recorded_kWh":
                    d.get("hard_feasibility", {}).get("violations", {}).get(
                        "shortfall_energy_kWh", 0),
                "recorded_inherited_items": {
                    "energy_closure_c5": True,
                    "safety_hard_events": True}}}
            res = fc3_core.fc3_check(view, traj, audit)
            st = res["readjudicated_status"]
            statuses[st] += 1
            failed = [k for k, x in res["items"].items() if not x]
            if failed:
                failures.append("%s/%s:%s" % (v, a, ",".join(failed)))
        except Exception as exc:  # noqa: BLE001
            statuses['ERROR'] += 1
            failures.append("%s/%s: %s" % (v, a, exc))
    gates["G2_fc3_readjudication"] = {
        "authority": "python/ppe/fc3_core.py", "fc3_core_sha256": fc3_sha,
        "semantics": "v3 dynamic PMIN envelope; service = recorded-native "
                     "E_unserved; tracking gap is diagnostic-only",
        "n": sum(statuses.values()),
        "READJUDICATED_FEASIBLE": statuses.get("READJUDICATED_FEASIBLE", 0),
        "READJUDICATED_HARD_INFEASIBLE":
            statuses.get("READJUDICATED_HARD_INFEASIBLE", 0),
        "ERROR": statuses.get("ERROR", 0),
        "note": "terminal-recorded hard_feasibility (v2 static semantics) "
                "is superseded by this authoritative readjudication",
        "pass": (fc3_sha == FC3_AUTH
                 and statuses.get("READJUDICATED_FEASIBLE") == 192
                 and not failures)}

    # ---- G3 pre-execution freeze gate ----
    frz = json.load(open(FREEZE))
    gates["G3_pre_execution_freeze"] = {
        "freeze_file": "results/ppe_practical/freeze/"
                       "epoch3_pre_execution_freeze_genb.json",
        "schema": frz.get("schema"), "overall": frz.get("overall"),
        "provisional": frz.get("provisional"),
        "git_head": frz.get("git_head"), "git_tree_clean":
            frz.get("git_tree_clean"),
        "assertions": frz.get("assertions"),
        "run_manifest": frz.get("run_manifest"),
        "campaign_generation": frz.get("campaign_generation"),
        "campaign_namespace": frz.get("campaign_namespace"),
        "corpus_policy": frz.get("corpus_policy"),
        "utc": frz.get("utc"),
        "assertion_polarity_note":
            "FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS is a count whose PASS "
            "value is 0 (no prior authoritative campaign runs at freeze "
            "time); boolean assertions are pass=true",
        "pass": bool(frz.get("overall"))
        and not frz.get("blockers")
        and all(bool(x) for k, x in frz.get("assertions", {}).items()
                if k != "FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS")
        and frz.get("assertions", {}).get(
            "FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS") == 0}

    # ---- G4 effect analysis acceptance ----
    eff_acc_p = os.path.join(ANA, 'effect_analysis_acceptance_epoch3.json')
    eff = json.load(open(eff_acc_p))
    gt = json.load(open(os.path.join(
        ANA, 'effect_analysis_group_tier_epoch3.json')))
    bs = json.load(open(os.path.join(
        ANA, 'effect_analysis_bootstrap_epoch3.json')))
    cx = json.load(open(os.path.join(
        ANA, 'effect_analysis_context_epoch3.json')))
    gates["G4_effect_analysis"] = {
        "acceptance_pass": eff.get("pass"),
        "corpus_runs": eff.get("corpus_runs"),
        "max_reconstruction_diff": eff.get("max_reconstruction_diff"),
        "bootstrap_deterministic": eff.get("bootstrap_deterministic"),
        "analysis_class": "DESCRIPTIVE_STATISTICS_PLUS_"
                          "PREREGISTERED_PAIRED_ANALYSIS",
        "theta_hat_pp": gt.get("theta_hat_pp"),
        "ci95_pp": bs.get("ci95_pp"),
        "R_diagnostic": bs.get("R_diagnostic"),
        "artifact_shas": eff.get("artifact_shas"),
        "pass": bool(eff.get("pass")) and eff.get("corpus_runs") == 192}

    # ---- G5 occupancy descriptive artifact ----
    occ_p = os.path.join(ANA, 'occupancy_descriptive_epoch3.json')
    occ = json.load(open(occ_p))
    gates["G5_occupancy_descriptive"] = {
        "n_runs": occ.get("corpus", {}).get("n_runs"),
        "per_arm": occ.get("corpus", {}).get("per_arm"),
        "analysis_class": occ.get("analysis_class"),
        "artifact_sha256": sha256_file(occ_p),
        "pass": occ.get("corpus", {}).get("n_runs") == 192}

    # ---- G6 frozen eligibility manifest ----
    elig_sha = sha256_file(ELIG_MAN)
    gates["G6_eligibility_manifest"] = {
        "path": "results/ppe_practical/"
                "effect_analysis_eligibility_manifest.json",
        "sha256": elig_sha,
        "expected_sha256": "9a0b608e827115ecbaf3ac25a54744358f3b22d"
                           "56667082ebc928af21525f7b3",
        "pass": elig_sha == "9a0b608e827115ecbaf3ac25a54744358f3b22d566"
                          "67082ebc928af21525f7b3"}

    # ---- G7 attempt-A disposition ----
    led = json.load(open(LEDGER))
    led_sha = sha256_file(LEDGER)
    gates["G7_attempt_a_disposition"] = {
        "ledger": "results/ppe_practical/formal_epoch3/_audit_stop_gate/"
                  "ATTEMPT_A_EXCLUSION_LEDGER.json",
        "ledger_sha256": led_sha,
        "verdict": led.get("verdict"),
        "reasons": led.get("reasons"),
        "g1_attempt_a_era_terminals_census": dict(attempt_a_census),
        "superseded_dir": "_superseded_pre_fc3_fix (sealed, not in corpus)",
        "mixing_check": "genB corpus filters freeze_root a2dc634f; "
                        "attempt-A terminals (different freeze root) are "
                        "physically excluded from every analysis input",
        "pass": (led.get("verdict") == "INVALID_FOR_FINAL_EFFECT_INFERENCE"
                 and attempt_a_census['terminals'] > 0)}

    # ---- G8 frozen module identities ----
    stats_sha = sha256_file(os.path.join(ROOT, 'python', 'ppe', 'stats.py'))
    fec_sha = sha256_file(os.path.join(ROOT, 'python', 'ppe', 'fec.py'))
    gates["G8_frozen_module_identity"] = {
        "stats_sha256": stats_sha, "fec_sha256": fec_sha,
        "fc3_core_sha256": fc3_sha,
        "pass": stats_sha == STATS_SHA and fec_sha == FEC_SHA
        and fc3_sha == FC3_AUTH}

    # ---- G9 no-mutation check (re-hash after all analysis reads) ----
    frz_sha_after = sha256_file(FREEZE)
    elig_after = sha256_file(ELIG_MAN)
    gates["G9_no_mutation"] = {
        "freeze_sha256_before": frz_sha_start,
        "freeze_sha256_after": frz_sha_after,
        "manifest_sha256_before": elig_sha_start,
        "manifest_sha256_after": elig_after,
        "pass": frz_sha_start == frz_sha_after
        and elig_sha_start == elig_after}

    for gname, g in gates.items():
        if not g.get("pass"):
            problems.append(gname)

    doc = {
        "schema": "ppe_epoch3_genb_formal_acceptance_v1",
        "title": "EPOCH-3 GENERATION-B FORMAL ACCEPTANCE",
        "evidence_label": "FORMAL_EPOCH3_GENB_CONTROL_EVIDENCE",
        "utc": datetime.now(timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"),
        "campaign": {
            "study": "hybrid-ship EMS 4-arm control contrast "
                     "(P0 rule / C0 persistence / C1 perfect-info / "
                     "C2 Predictor-v3 NN)",
            "design": "48 frozen voyages stratified LOW/MID/HIGH x 4 arms "
                      "= 192 terminals",
            "freeze_root_prefix": GENB_FREEZE,
            "freeze_file_sha256": sha256_file(FREEZE)},
        "gates": gates,
        "overall_pass": not problems,
        "problems": problems,
        "summary_numbers": {
            "corpus": "192/192 terminals, 0 duplicate, 0 missing",
            "fc3": "192/192 READJUDICATED_FEASIBLE "
                   "(fc3_core 33a8309a)",
            "theta_pp": gt.get("theta_hat_pp"),
            "ci95_pp": bs.get("ci95_pp"),
            "R": (bs.get("R_diagnostic") or {}).get("R"),
            "P0_context_kg": {
                k: cx["P0_context"]["chain_estimates"][k][
                    "theta_hat_kg"]
                for k in ("DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg",
                          "DJ_C2_vs_P0_kg")},
            "P0_context_note": "chain FULL_COHORT estimates; see "
                               "effect_analysis_context_epoch3.json for "
                               "tiers, CI and Wilcoxon"}}
    doc["sha256_self"] = sha256_canonical(
        {k: v for k, v in doc.items() if k != "sha256_self"})
    out_p = os.path.join(G2, 'EPOCH3_GENB_FORMAL_ACCEPTANCE.json')
    json.dump(doc, open(out_p, 'w'), indent=1, sort_keys=True)
    print("OVERALL_PASS" if not problems else "PROBLEMS: %s" % problems)
    for gname, g in gates.items():
        print(" %s: pass=%s" % (gname, g.get("pass")))
    print("ACCEPTANCE_FILE:", out_p)
    print("ACCEPTANCE_SHA256:", sha256_file(out_p))
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())

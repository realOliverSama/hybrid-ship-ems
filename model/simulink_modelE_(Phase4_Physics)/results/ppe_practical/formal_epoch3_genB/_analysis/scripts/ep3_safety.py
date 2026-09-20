#!/usr/bin/env python3
"""EPOCH-3 genB safety-stability DESCRIPTIVE comparison (read-only).

Post-hoc descriptive supplement (NOT pre-registered): per-arm comparison of
  - DC-bus voltage stability: V_dc std / range / p99 |dV| per second /
    minimum margin below the propulsion derate line (675 V, full-power
    threshold of clamp((V-600)/75)); seconds below 675 V;
  - propulsion tracking (ship-speed stability proxy): energy gap
    integral max(demand-actual,0), max instantaneous gap, symmetric error.
Data: traj.v1.mat channels V_dc_V / P_prop_demand_kW / P_prop_actual_kW,
genB corpus only (freeze a2dc634f), per-(voyage,arm) resolution, 192 runs.
Paired within-voyage contrasts vs P0 with Wilcoxon supplement (Holm within
family). Analysis class: DESCRIPTIVE_STATISTICS_ONLY."""
import hashlib
import json
import os
import sys
from collections import Counter

import h5py
import numpy as np
from scipy.stats import wilcoxon

ROOT = '/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)'
RPP = os.path.join(ROOT, 'results', 'ppe_practical')
G2 = os.path.join(RPP, 'formal_epoch3_genB')
G1 = os.path.join(RPP, 'formal_epoch3')
OUT = os.path.join(G2, '_analysis')
GENB_FREEZE = 'a2dc634f'
ARMS = ('P0', 'C0', 'C1', 'C2')
DERATE_FULL_V = 675.0   # full-power threshold: clamp((V-600)/75,0,1)
NOMINAL_V = 600.0


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


def desc(xs):
    a = np.asarray(xs, dtype=float)
    return {"n": int(a.size), "mean": float(a.mean()),
            "median": float(np.median(a)), "min": float(a.min()),
            "max": float(a.max()), "std": float(a.std(ddof=1))}


def holm(ps):
    order = np.argsort(ps)
    m = len(ps)
    adj = np.empty(m)
    running = 0.0
    for rank, idx in enumerate(order):
        running = max(running, (m - rank) * ps[idx])
        adj[idx] = min(running, 1.0)
    return adj


def wilcoxon_supplement(diffs):
    d = np.asarray(diffs, dtype=float)
    nz = d[d != 0]
    out = {"n": int(d.size), "n_nonzero": int(nz.size)}
    if nz.size == 0:
        out.update({"note": "all paired differences zero"})
        return out
    w = wilcoxon(d)
    out["p_two_sided"] = float(w.pvalue)
    return out


def main():
    tv = json.load(open(os.path.join(ROOT, 'config',
                                     'phase4f_ppe_teval_table.json')))
    meta = {v['voyage_id']: v for v in tv['voyages']}
    loc = {}
    for ns_name, base in (('G2', G2), ('G1', G1)):
        if not os.path.isdir(base):
            continue
        for v in sorted(os.listdir(base)):
            vd = os.path.join(base, v)
            if not os.path.isdir(vd) or v.startswith('_') or v not in meta:
                continue
            for a in ARMS:
                f = os.path.join(vd, a + '.json')
                if not os.path.isfile(f):
                    continue
                d = json.load(open(f))
                if str(d.get('freeze_root_sha256', '')).startswith(
                        GENB_FREEZE):
                    loc[(v, a)] = (ns_name, vd)
    if Counter(a for _, a in loc) != {'P0': 48, 'C0': 48, 'C1': 48,
                                      'C2': 48}:
        print("ABORT: coverage", Counter(a for _, a in loc))
        return 3

    rows = []
    for (v, a) in sorted(loc):
        ns_name, vd = loc[(v, a)]
        with h5py.File(os.path.join(vd, '%s.traj.v1.mat' % a), 'r') as h:
            tr = h['traj']
            vdc = np.asarray(tr['V_dc_V'], dtype=float).ravel()
            dem = np.asarray(tr['P_prop_demand_kW'], dtype=float).ravel()
            act = np.asarray(tr['P_prop_actual_kW'], dtype=float).ravel()
            t = np.asarray(tr['time'], dtype=float).ravel()
        dt = np.diff(t)
        dv = np.abs(np.diff(vdc))
        gap = np.maximum(dem - act, 0.0)
        rows.append({
            "voyage_id": v, "arm": a, "tier": meta[v]["tier"], "ns": ns_name,
            "vdc_mean_V": float(vdc.mean()),
            "vdc_std_V": float(vdc.std(ddof=1)),
            "vdc_min_V": float(vdc.min()),
            "vdc_max_V": float(vdc.max()),
            "vdc_range_V": float(vdc.max() - vdc.min()),
            "vdc_p99_abs_dV_per_s_V": float(np.percentile(dv / np.maximum(
                dt, 1e-6), 99)),
            "vdc_margin_below_675_min_V": float(DERATE_FULL_V - vdc.min()),
            "vdc_seconds_below_675": float(np.sum(vdc < DERATE_FULL_V)),
            "track_gap_kWh": float(np.sum(gap) / 3600.0),
            "track_max_gap_kW": float(gap.max()),
            "track_frac_gap_gt_1kW": float(np.mean(gap > 1.0)),
            "track_sym_err_kWh": float(np.sum(np.abs(dem - act)) / 3600.0)})

    METRICS = ["vdc_mean_V", "vdc_min_V", "vdc_std_V", "vdc_range_V", "vdc_p99_abs_dV_per_s_V",
               "vdc_margin_below_675_min_V", "vdc_seconds_below_675",
               "track_gap_kWh", "track_max_gap_kW",
               "track_frac_gap_gt_1kW", "track_sym_err_kWh"]

    def agg(field, arm=None, tier=None):
        xs = [r[field] for r in rows
              if (arm is None or r["arm"] == arm)
              and (tier is None or r["tier"] == tier)]
        return desc(xs)

    per_arm = {a: {m: agg(m, arm=a) for m in METRICS} for a in ARMS}
    per_arm_tier = {a: {t: {m: agg(m, arm=a, tier=t) for m in METRICS}
                        for t in ("LOW", "MID", "HIGH")} for a in ARMS}

    # paired contrasts vs P0 (within voyage)
    byva = {(r["voyage_id"], r["arm"]): r for r in rows}
    contrasts = {}
    for ca in ("C0", "C1", "C2"):
        contrasts[ca] = {}
        for m in METRICS:
            diffs = [byva[(v, ca)][m] - byva[(v, 'P0')][m]
                     for v in sorted({r["voyage_id"] for r in rows})]
            entry = {"diff_descriptives": desc(diffs)}
            entry.update(wilcoxon_supplement(diffs))
            contrasts[ca][m] = entry
        fam = [contrasts[ca][m].get("p_two_sided") for m in
               ("vdc_std_V", "vdc_margin_below_675_min_V", "track_gap_kWh")]
        if all(p is not None for p in fam):
            adj = holm(fam)
            for m, p in zip(("vdc_std_V", "vdc_margin_below_675_min_V",
                             "track_gap_kWh"), adj):
                contrasts[ca][m]["p_holm_two_sided"] = float(p)

    out = {
        "schema": "ppe_epoch3_genb_safety_descriptive_v1",
        "analysis_class": "DESCRIPTIVE_STATISTICS_ONLY",
        "evidence_label": "FORMAL_EPOCH3_GENB_CONTROL_EVIDENCE",
        "purpose": "post-hoc descriptive safety/stability comparison to "
                   "support thesis claims; NOT pre-registered",
        "definitions": {
            "derate_full_power_V": DERATE_FULL_V,
            "derate_formula": "P_prop_limit = Mot_P_nom * "
                              "clamp((V_dc-600)/75, 0, 1)",
            "vdc_margin_below_675_min_V": "675 - min(V_dc); positive = "
                                          "never entered derate",
            "track_gap_kWh": "integral of max(P_prop_demand-P_prop_actual,"
                             "0) — ship-speed stability proxy"},
        "corpus": {"n_runs": len(rows),
                   "per_arm": dict(Counter(r["arm"] for r in rows))},
        "per_arm": per_arm, "per_arm_tier": per_arm_tier,
        "paired_contrasts_vs_P0": contrasts,
        "note": "V_dc stability and tracking metrics read only from frozen "
                "traj.v1.mat channels; no fuel fields touched",
        "per_run": rows}
    out["bindings"] = {
        "freeze_file_sha256": sha256_file(os.path.join(
            RPP, 'freeze', 'epoch3_pre_execution_freeze_genb.json')),
        "analysis_script_sha256": sha256_file(os.path.abspath(__file__))}
    out["sha256_self"] = sha256_canonical(
        {k: v for k, v in out.items() if k != "sha256_self"})
    os.makedirs(OUT, exist_ok=True)
    p = os.path.join(OUT, 'safety_descriptive_epoch3.json')
    json.dump(out, open(p, 'w'), indent=1, sort_keys=True)
    print("SAFETY_DONE sha256=%s" % sha256_file(p))
    for a in ARMS:
        d = per_arm[a]
        print("%s: vdc_std=%.3fV range=%.2fV min=%.1fV margin675=%.1fV "
              "t<675=%.1fs p99dV=%.2f gap=%.4fkWh maxgap=%.1fkW"
              % (a, d["vdc_std_V"]["mean"], d["vdc_range_V"]["mean"],
                 d["vdc_min_V"]["mean"],
                 d["vdc_margin_below_675_min_V"]["mean"],
                 d["vdc_seconds_below_675"]["mean"],
                 d["vdc_p99_abs_dV_per_s_V"]["mean"],
                 d["track_gap_kWh"]["mean"], d["track_max_gap_kW"]["mean"]))
    for ca in ("C0", "C1", "C2"):
        for m in ("vdc_std_V", "vdc_margin_below_675_min_V",
                  "track_gap_kWh"):
            c = contrasts[ca][m]
            print("%s dP0 %-28s mean=%+.4f p=%s holm=%s"
                  % (ca, m, c["diff_descriptives"]["mean"],
                     None if "p_two_sided" not in c
                     else "%.3g" % c["p_two_sided"],
                     c.get("p_holm_two_sided")))
    return 0


if __name__ == "__main__":
    sys.exit(main())

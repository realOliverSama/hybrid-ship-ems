#!/usr/bin/env python3
"""EPOCH-3 genB occupancy descriptive statistics (read-only).

Per (voyage, arm) terminal, resolved across both genB namespaces:
  - P_gen_ref occupancy at the frozen P_min/P_max band edges (125/500 kW,
    from the FC-3 frozen thresholds), interior share, and N_dispatch
    (1 vs 2 committed gensets) occupancy — from sourcesel.v1.mat telemetry;
  - delta_lambda distribution over the frozen 41-level grid [-0.10,+0.10]
    step 0.005 and forecast-window churn — from ctrace.v1.json;
  - selector/fallback/clamp activity rates.

Aggregation: mean of per-run fractions, reported per arm and per arm x tier.
P0 carries sourcesel telemetry; ctrace applies=False for P0 by design."""
import hashlib
import json
import os
import sys
from collections import Counter

import h5py
import numpy as np

ROOT = '/root/autodl-tmp/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)'
RPP = os.path.join(ROOT, 'results', 'ppe_practical')
G2 = os.path.join(RPP, 'formal_epoch3_genB')
G1 = os.path.join(RPP, 'formal_epoch3')
OUT = os.path.join(G2, '_analysis')
GENB_FREEZE = 'a2dc634f'
ARMS = ('P0', 'C0', 'C1', 'C2')
P_MIN, P_MAX = 125.0, 500.0
DL_MIN, DL_MAX, DL_STEP = -0.10, 0.10, 0.005


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
        print("ABORT: per-arm coverage != 48/48/48/48:",
              Counter(a for _, a in loc))
        return 3

    runs = []
    dl_levels = {a: set() for a in ('C0', 'C1', 'C2')}
    for (v, a) in sorted(loc):
        ns_name, vd = loc[(v, a)]
        row = {"voyage_id": v, "arm": a, "tier": meta[v]["tier"], "ns": ns_name}
        sf = os.path.join(vd, '%s.sourcesel.v1.mat' % a)
        with h5py.File(sf, 'r') as h:
            tel = h['tel']
            ref = np.asarray(tel['P_gen_ref_kW_end'], dtype=float).ravel()
            row["n_samples"] = int(ref.size)
            row["pmin_band_frac"] = float(np.mean(ref <= P_MIN + 1e-6))
            row["pmax_band_frac"] = float(np.mean(ref >= P_MAX - 1e-6))
            row["interior_frac"] = float(np.mean(
                (ref > P_MIN + 1e-6) & (ref < P_MAX - 1e-6)))
            row["ref_min_kW"] = float(ref.min())
            row["ref_max_kW"] = float(ref.max())
            row["ref_mean_kW"] = float(ref.mean())
            nd = np.asarray(tel['N_dispatch_end'], dtype=float).ravel()
            row["ndispatch_1_frac"] = float(np.mean(nd <= 0.5))
            row["ndispatch_2_frac"] = float(np.mean(nd > 0.5))
            for key, out in (("selector_baseline_active_end",
                              "selector_baseline_active_frac"),
                             ("dispatch_min_clamp_active_end",
                              "dispatch_min_clamp_frac"),
                             ("baseline_fallback_active_end",
                              "baseline_fallback_frac")):
                if key in tel:
                    row[out] = float(np.mean(
                        np.asarray(tel[key], dtype=float).ravel() > 0.5))
            eh = np.asarray(tel['ems_health_ok_end'], dtype=float).ravel()
            row["ems_health_ok_min"] = float(eh.min())
        cf = os.path.join(vd, '%s.ctrace.v1.json' % a)
        ct = json.load(open(cf))
        if ct.get("applies"):
            dl = np.asarray(ct["delta_lambda"], dtype=float)
            row["dl_min"] = float(dl.min())
            row["dl_max"] = float(dl.max())
            row["dl_mean"] = float(dl.mean())
            row["dl_std"] = float(dl.std(ddof=1))
            row["dl_distinct"] = int(np.unique(np.round(dl, 6)).size)
            row["dl_at_lo_bound_frac"] = float(np.mean(dl <= DL_MIN + 1e-9))
            row["dl_at_hi_bound_frac"] = float(np.mean(dl >= DL_MAX - 1e-9))
            row["dl_zero_frac"] = float(np.mean(np.abs(dl) < 1e-9))
            row["n_decisions"] = int(dl.size)
            dl_levels[a].update(np.round(dl, 6).tolist())
            ws = [str(x) for x in ct.get("forecast_window_sha256", [])]
            row["forecast_windows_distinct"] = len(set(ws))
            if len(ws) > 1:
                row["forecast_window_churn_rate"] = float(np.mean(
                    [x != y for x, y in zip(ws[:-1], ws[1:])]))
        runs.append(row)

    def agg_arm(field, arm=None, tier=None):
        xs = [r[field] for r in runs
              if (arm is None or r["arm"] == arm)
              and (tier is None or r["tier"] == tier) and field in r]
        return desc(xs) if xs else None

    FIELDS_REF = ["pmin_band_frac", "pmax_band_frac", "interior_frac",
                  "ref_min_kW", "ref_max_kW", "ref_mean_kW",
                  "ndispatch_1_frac", "ndispatch_2_frac",
                  "selector_baseline_active_frac", "dispatch_min_clamp_frac",
                  "baseline_fallback_frac"]
    FIELDS_DL = ["dl_min", "dl_max", "dl_mean", "dl_std", "dl_distinct",
                 "dl_at_lo_bound_frac", "dl_at_hi_bound_frac",
                 "dl_zero_frac", "forecast_windows_distinct",
                 "forecast_window_churn_rate"]

    per_arm = {}
    for a in ARMS:
        per_arm[a] = {f: agg_arm(f, arm=a) for f in FIELDS_REF}
        per_arm[a]["n_runs"] = sum(1 for r in runs if r["arm"] == a)
        if a != 'P0':
            per_arm[a].update({f: agg_arm(f, arm=a) for f in FIELDS_DL})
    per_arm_tier = {}
    for a in ARMS:
        per_arm_tier[a] = {}
        for t in ("LOW", "MID", "HIGH"):
            per_arm_tier[a][t] = {f: agg_arm(f, arm=a, tier=t)
                                  for f in FIELDS_REF}
            if a != 'P0':
                per_arm_tier[a][t].update(
                    {f: agg_arm(f, arm=a, tier=t) for f in FIELDS_DL})

    out = {
        "schema": "ppe_epoch3_genb_occupancy_descriptive_v1",
        "analysis_class": "DESCRIPTIVE_STATISTICS_ONLY",
        "evidence_label": "FORMAL_EPOCH3_GENB_CONTROL_EVIDENCE",
        "frozen_thresholds": {"P_min_kW": P_MIN, "P_max_kW": P_MAX,
                              "delta_lambda_grid": {"min": DL_MIN,
                                                    "max": DL_MAX,
                                                    "step": DL_STEP,
                                                    "n_levels": 41}},
        "corpus": {"n_runs": len(runs),
                   "per_arm": dict(Counter(r["arm"] for r in runs)),
                   "freeze_root_prefix": GENB_FREEZE,
                   "per_run_resolution": "per (voyage,arm) across "
                                         "formal_epoch3_genB + formal_epoch3"},
        "per_arm": per_arm, "per_arm_tier": per_arm_tier,
        "distinct_delta_lambda_levels_used": {
            a: {"n": len(s), "min": min(s) if s else None,
                "max": max(s) if s else None}
            for a, s in dl_levels.items()},
        "note": "mean of per-run fractions; P0 has no forecast trace "
                "(ctrace applies=false by design)",
        "per_run": runs}
    out["bindings"] = {
        "freeze_file_sha256": sha256_file(os.path.join(
            RPP, 'freeze', 'epoch3_pre_execution_freeze_genb.json')),
        "analysis_script_sha256": sha256_file(os.path.abspath(__file__))}
    out["sha256_self"] = sha256_canonical(
        {k: v for k, v in out.items() if k != "sha256_self"})
    os.makedirs(OUT, exist_ok=True)
    p = os.path.join(OUT, 'occupancy_descriptive_epoch3.json')
    json.dump(out, open(p, 'w'), indent=1, sort_keys=True)
    print("OCCUPANCY_DONE sha256=%s" % sha256_file(p))
    for a in ARMS:
        d = per_arm[a]
        print("%s: pmin_band=%.3f pmax_band=%.3f interior=%.3f "
              "N1=%.3f N2=%.3f sel_bl=%.4f clamp=%.4f"
              % (a, d["pmin_band_frac"]["mean"],
                 d["pmax_band_frac"]["mean"], d["interior_frac"]["mean"],
                 d["ndispatch_1_frac"]["mean"], d["ndispatch_2_frac"]["mean"],
                 (d.get("selector_baseline_active_frac") or
                  {"mean": float('nan')})["mean"],
                 (d.get("dispatch_min_clamp_frac") or
                  {"mean": float('nan')})["mean"]))
    for a in ('C0', 'C1', 'C2'):
        d = per_arm[a]
        print("%s dl: [%.3f, %.3f] mean=%+.4f distinct=%.0f "
              "lo_bound=%.3f hi_bound=%.3f zero=%.3f windows/run=%.0f"
              % (a, d["dl_min"]["mean"], d["dl_max"]["mean"],
                 d["dl_mean"]["mean"], d["dl_distinct"]["mean"],
                 d["dl_at_lo_bound_frac"]["mean"],
                 d["dl_at_hi_bound_frac"]["mean"], d["dl_zero_frac"]["mean"],
                 d["forecast_windows_distinct"]["mean"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())

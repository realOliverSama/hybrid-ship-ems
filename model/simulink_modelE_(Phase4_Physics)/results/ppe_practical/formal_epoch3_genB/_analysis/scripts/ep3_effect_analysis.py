#!/usr/bin/env python3
"""EPOCH-3 generation-B FORMAL effect analysis (descriptive + pre-registered
paired statistics).

Corpus: genB terminals only (freeze_root a2dc634f*), merged across namespaces
formal_epoch3_genB (G2) and formal_epoch3 (G1), resolved per (voyage, arm).
Statistical identity: the FROZEN ppe.stats module (aggregation chain
voyage -> split_group mean -> tier mean -> FULL_COHORT_ESTIMATE, frozen-seed
bootstrap, R guard) and the FROZEN ppe.fec module (J_FEC).  Module bytes are
sha-verified before any fuel field is read; the script aborts on mismatch.

Pre-registered estimands (ppe.stats.voyage_effects, frozen):
  B_perfect   = 100*(J_C0 - J_C1)/J_C0   (persistence -> perfect info)
  B_predicted = 100*(J_C0 - J_C2)/J_C0   (persistence -> NN predictor v3)
  L_prediction= 100*(J_C2  - J_C1)/J_C0  (prediction-error cost)
P0 is a CONTEXT arm (P0_CONTEXT_MASK): paired contrasts vs P0 are reported
as descriptive statistics with Wilcoxon signed-rank supplement; they are not
the pre-registered primary estimands and enter no algebraic identity.

k_eq sensitivity sides 0.8x/1.2x are the pre-registered FEC robustness
sides (qualification/fec_robustness_prereg.json)."""
import hashlib
import json
import os
import sys
from collections import Counter

import numpy as np
from scipy.stats import pearsonr, spearmanr, wilcoxon

ROOT = '/root/autodl-tmp/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)'
sys.path.insert(0, os.path.join(ROOT, 'python'))
from ppe import fec as F  # noqa: E402
from ppe import stats as S  # noqa: E402

RPP = os.path.join(ROOT, 'results', 'ppe_practical')
G2 = os.path.join(RPP, 'formal_epoch3_genB')
G1 = os.path.join(RPP, 'formal_epoch3')
OUT = os.path.join(G2, '_analysis')
ELIG_MAN = os.path.join(RPP, 'effect_analysis_eligibility_manifest.json')
FREEZE_FILE = os.path.join(RPP, 'freeze', 'epoch3_pre_execution_freeze_genb.json')
GENB_FREEZE = 'a2dc634f'
ARMS = ('P0', 'C0', 'C1', 'C2')
K_CANON = 0.5457
BAT_CAP = 1071.36
STATS_SHA = '75278cadee95f26b04955962d985bb757aa78344be07fac56888d9c30dd0a5ba'
FEC_SHA = '75d7c63f2aeda867ec91338a93b0ced4ab7300a42c7f2c0ebacd46810e325596'
FC3_SHA = '33a8309ad6f585bed5203cd0223484a0df3eda5ba8d4eeb8193e1ba85c34a4a9'
EVIDENCE_LABEL = 'FORMAL_EPOCH3_GENB_CONTROL_EVIDENCE'


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
            "max": float(a.max()), "std": float(a.std(ddof=1)),
            "positive": int((a > 0).sum()), "zero": int((a == 0).sum()),
            "negative": int((a < 0).sum())}


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
        out.update({"note": "all paired differences zero", "W": None,
                    "p_two_sided": None, "p_one_sided_neg": None,
                    "p_one_sided_pos": None})
        return out
    w = wilcoxon(d)  # zero_method 'wilcox' (default): zeros dropped
    out["W"] = float(w.statistic)
    out["p_two_sided"] = float(w.pvalue)
    out["p_one_sided_neg"] = float(
        wilcoxon(d, alternative='less').pvalue)
    out["p_one_sided_pos"] = float(
        wilcoxon(d, alternative='greater').pvalue)
    return out


def jk(fuel, soc0, soc_end, k):
    return float(fuel) + (float(soc0) - float(soc_end)) * BAT_CAP * k


def main():
    problems = []
    # ---- frozen module identity (abort on mismatch, before any fuel read) ----
    stats_sha = sha256_file(os.path.join(ROOT, 'python', 'ppe', 'stats.py'))
    fec_sha = sha256_file(os.path.join(ROOT, 'python', 'ppe', 'fec.py'))
    fc3_sha = sha256_file(os.path.join(ROOT, 'python', 'ppe', 'fc3_core.py'))
    if stats_sha != STATS_SHA or fec_sha != FEC_SHA or fc3_sha != FC3_SHA:
        print("ABORT: frozen module sha mismatch",
              stats_sha[:8], fec_sha[:8], fc3_sha[:8])
        return 3

    elig = json.load(open(ELIG_MAN))
    elig_sha = sha256_file(ELIG_MAN)
    triplet = elig["masks"]["MASK_COMMON_TRIPLET"]
    p0_mask = set(elig["masks"]["P0_CONTEXT_MASK"])
    meta = {v["voyage_id"]: v for v in elig["voyages"]}
    if len(triplet) != 48 or not p0_mask >= set(triplet):
        problems.append("frozen mask shape unexpected")

    tv = json.load(open(os.path.join(ROOT, 'config',
                                     'phase4f_ppe_teval_table.json')))
    universe = sorted(v['voyage_id'] for v in tv['voyages'])
    if sorted(triplet) != universe:
        problems.append("triplet mask != 48-voyage universe")

    # ---- corpus: per-(voyage, arm) namespace resolution, genB freeze only ----
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
                if not str(d.get('freeze_root_sha256', '')).startswith(
                        GENB_FREEZE):
                    continue
                loc.setdefault((v, a), []).append((ns_name, f, d))
    dups = {k: x for k, x in loc.items() if len(x) > 1}
    if dups:
        problems.append("duplicate (voyage,arm): %s" % sorted(dups))
    missing = sorted(set(meta) * set(ARMS) - set(loc)) if False else sorted(
        {(v, a) for v in meta for a in ARMS} - set(loc))
    if missing:
        problems.append("missing (voyage,arm): %s" % missing)

    vals = {}
    max_recon = 0.0
    freeze_roots = set()
    for (v, a), x in sorted(loc.items()):
        ns_name, f, d = x[0]
        j = F.j_fec(d['fuel_raw_kg'], d['SOC0'], d['SOC_end'])
        max_recon = max(max_recon, abs(j - d['J_FEC_kg']))
        freeze_roots.add(d['freeze_root_sha256'])
        vals[(v, a)] = {
            "fuel": float(d['fuel_raw_kg']), "soc0": float(d['SOC0']),
            "soc_end": float(d['SOC_end']), "J": j,
            "run_id": d.get('run_id'), "ns": ns_name}
    if max_recon > 1e-9:
        problems.append("J reconstruction diff %.3e" % max_recon)
    if len(freeze_roots) != 1:
        problems.append("freeze root not unique across corpus")
    if Counter(a for (_, a) in vals) != {'P0': 48, 'C0': 48,
                                         'C1': 48, 'C2': 48}:
        problems.append("per-arm coverage != 48/48/48/48")

    corpus_identity = sha256_canonical(
        [{"voyage": v, "arm": a, "run_id": vals[(v, a)]["run_id"],
          "J_FEC_kg": vals[(v, a)]["J"]}
         for (v, a) in sorted(vals)])

    g2t = {}
    for v in triplet:
        g2t[meta[v]["split_group_id"]] = meta[v]["tier"]

    # ---- primary voyage effects (frozen implementation) ----
    prim_rows = []
    for v in triplet:
        e = S.voyage_effects(vals[(v, 'C0')]["J"], vals[(v, 'C1')]["J"],
                             vals[(v, 'C2')]["J"])
        g = meta[v]["split_group_id"]
        g2t[g] = meta[v]["tier"]
        prim_rows.append({
            "voyage_id": v, "split_group_id": g, "tier": meta[v]["tier"],
            "J_C0": vals[(v, 'C0')]["J"], "J_C1": vals[(v, 'C1')]["J"],
            "J_C2": vals[(v, 'C2')]["J"],
            "B_perfect": e["B_perfect"], "B_predicted": e["B_predicted"],
            "L_prediction": e["L_prediction"],
            "DJ_perfect_kg": e["DJ_perfect_kg"],
            "DJ_predicted_kg": e["DJ_predicted_kg"],
            "DJ_gap_kg": e["DJ_gap_kg"],
            "identity_residual": e["identity_residual"]})
    if max(abs(r["identity_residual"]) for r in prim_rows) > 1e-9:
        problems.append("identity residual")

    def agg(rows, key):
        gv = {}
        for r in rows:
            gv.setdefault(r["split_group_id"], []).append(r[key])
        ge = S.group_effects(gv)
        st, hat, tm = S.theta_hat(ge, g2t)
        return ge, st, hat, tm

    theta, est_status, tier_vals, group_vals = {}, {}, {}, {}
    for m in ("B_perfect", "B_predicted", "L_prediction"):
        ge, st, hat, tm = agg(prim_rows, m)
        theta[m], est_status[m], tier_vals[m] = hat, st, tm
        group_vals[m] = {g: x["effect"] for g, x in ge.items()}
        if st != "FULL_COHORT_ESTIMATE":
            problems.append("estimator status %s=%s" % (m, st))

    def boot_ci(rows, key):
        ge = agg(rows, key)[0]
        lo, hi, _ = S.bootstrap_ci(ge, g2t)
        return lo, hi

    ci = {m: boot_ci(prim_rows, m) for m in
          ("B_perfect", "B_predicted", "L_prediction")}
    ci_rep = {m: boot_ci(prim_rows, m) for m in
              ("B_perfect", "B_predicted", "L_prediction")}
    if ci != ci_rep:
        problems.append("bootstrap non-deterministic")
    R = S.r_diagnostic(theta["B_perfect"], theta["B_predicted"],
                       est_status["B_perfect"])

    descs = {m: desc([r[m] for r in prim_rows]) for m in
             ("B_perfect", "B_predicted", "L_prediction")}

    # Wilcoxon supplement on the primary estimands (voyage level, n=48)
    wilc_primary = {m: wilcoxon_supplement([r[m] for r in prim_rows])
                    for m in ("B_perfect", "B_predicted", "L_prediction")}
    fam_p = [wilc_primary[m]["p_two_sided"] for m in
             ("B_perfect", "B_predicted", "L_prediction")]
    if all(p is not None for p in fam_p):
        adj = holm(fam_p)
        for m, a_ in zip(("B_perfect", "B_predicted", "L_prediction"), adj):
            wilc_primary[m]["p_holm_two_sided"] = float(a_)

    # ---- P0 context contrasts (descriptive paired, per voyage) ----
    p0_rows = []
    for v in triplet:
        jp = vals[(v, 'P0')]["J"]
        row = {"voyage_id": v, "tier": meta[v]["tier"],
               "split_group_id": meta[v]["split_group_id"], "J_P0": jp}
        for a in ("C0", "C1", "C2"):
            ja = vals[(v, a)]["J"]
            row["DJ_%s_vs_P0_kg" % a] = ja - jp
            row["DPCT_%s_vs_P0" % a] = 100.0 * (ja - jp) / jp
        p0_rows.append(row)

    def tier_table(key):
        out = {}
        for t in S.TIERS:
            xs = [r[key] for r in p0_rows if r["tier"] == t]
            out[t] = desc(xs)
        out["ALL"] = desc([r[key] for r in p0_rows])
        return out

    p0_tier = {k: tier_table(k) for k in
               ("DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg", "DJ_C2_vs_P0_kg",
                "DPCT_C2_vs_P0", "DPCT_C1_vs_P0", "DPCT_C0_vs_P0")}

    # chain estimate (group -> tier -> study) for the P0 contrasts
    p0_chain = {}
    for key in ("DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg", "DJ_C2_vs_P0_kg"):
        ge, st, hat, tm = agg(p0_rows, key)
        lo, hi, _ = S.bootstrap_ci(ge, g2t)
        p0_chain[key] = {"theta_hat_kg": hat, "status": st,
                         "tier_means_kg": tm, "ci95_kg": [lo, hi]}

    wilc_p0 = {k: wilcoxon_supplement([r[k] for r in p0_rows]) for k in
               ("DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg", "DJ_C2_vs_P0_kg",
                "DPCT_C0_vs_P0", "DPCT_C1_vs_P0", "DPCT_C2_vs_P0")}
    fam_p0 = [wilc_p0[k]["p_two_sided"] for k in
              ("DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg", "DJ_C2_vs_P0_kg")]
    if all(p is not None for p in fam_p0):
        adj = holm(fam_p0)
        for k, a_ in zip(("DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg",
                          "DJ_C2_vs_P0_kg"), adj):
            wilc_p0[k]["p_holm_two_sided"] = float(a_)

    # ---- k_eq sensitivity (pre-registered sides 0.8x / 1.2x) ----
    def effects_with_k(k):
        rows = []
        for v in triplet:
            j = {a: jk(vals[(v, a)]["fuel"], vals[(v, a)]["soc0"],
                       vals[(v, a)]["soc_end"], k) for a in ARMS}
            rows.append({
                "voyage_id": v, "split_group_id": meta[v]["split_group_id"],
                "tier": meta[v]["tier"], "J": j,
                "B_perfect": 100 * (j["C0"] - j["C1"]) / j["C0"],
                "B_predicted": 100 * (j["C0"] - j["C2"]) / j["C0"],
                "L_prediction": 100 * (j["C2"] - j["C1"]) / j["C0"],
                "DJ_C0_vs_P0_kg": j["C0"] - j["P0"],
                "DJ_C1_vs_P0_kg": j["C1"] - j["P0"],
                "DJ_C2_vs_P0_kg": j["C2"] - j["P0"]})
        return rows

    sens_metrics = ("B_perfect", "B_predicted", "L_prediction",
                    "DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg", "DJ_C2_vs_P0_kg")
    sens = {}
    for label, k in (("canonical", K_CANON), ("0.8x", 0.8 * K_CANON),
                     ("1.2x", 1.2 * K_CANON)):
        rows = effects_with_k(k)
        sens[label] = {"k_eq": k, "theta": {}}
        for m in sens_metrics:
            ge, st, hat, tm = agg(rows, m)
            sens[label]["theta"][m] = {"hat": hat, "tier": tm}

    def sgn(x):
        return 0 if x is None or abs(x) < 1e-12 else (1 if x > 0 else -1)

    sign_changes = {}
    for m in sens_metrics:
        c0 = sgn(sens["canonical"]["theta"][m]["hat"])
        sign_changes[m] = {
            "canonical_sign": c0,
            "0.8x_sign": sgn(sens["0.8x"]["theta"][m]["hat"]),
            "1.2x_sign": sgn(sens["1.2x"]["theta"][m]["hat"]),
            "0.8x_sign_change": sgn(sens["0.8x"]["theta"][m]["hat"]) != c0,
            "1.2x_sign_change": sgn(sens["1.2x"]["theta"][m]["hat"]) != c0}

    # per-voyage break-even and band sign-flip audit (C2/C1/C0 vs P0)
    def be_voyage(la, ra):
        out = []
        for v in triplet:
            df = vals[(v, la)]["fuel"] - vals[(v, ra)]["fuel"]
            dt_ = ((vals[(v, la)]["soc0"] - vals[(v, la)]["soc_end"])
                   - (vals[(v, ra)]["soc0"] - vals[(v, ra)]["soc_end"]))
            denom = dt_ * BAT_CAP
            k_be = None if abs(denom) < 1e-12 else -df / denom
            d_lo = df + denom * 0.8 * K_CANON
            d_hi = df + denom * 1.2 * K_CANON
            d_ca = df + denom * K_CANON
            out.append({"voyage_id": v, "tier": meta[v]["tier"],
                        "k_break_even": k_be,
                        "dJ_0.8x": d_lo, "dJ_canonical": d_ca,
                        "dJ_1.2x": d_hi,
                        "sign_flip_within_band":
                            sgn(d_lo) != sgn(d_hi) or sgn(d_ca) != sgn(d_lo)
                            or sgn(d_ca) != sgn(d_hi)})
        return out

    be = {}
    for name, (la, ra) in (("C2_vs_P0", ("C2", "P0")),
                           ("C1_vs_P0", ("C1", "P0")),
                           ("C0_vs_P0", ("C0", "P0")),
                           ("C2_vs_C1", ("C2", "C1")),
                           ("C0_vs_C1", ("C0", "C1"))):
        rows = be_voyage(la, ra)
        flips = [r for r in rows if r["sign_flip_within_band"]]
        # study-level break-even via linear-parts chain aggregation
        gv_f, gv_t = {}, {}
        for r in rows:
            pass
        for v in triplet:
            df = vals[(v, la)]["fuel"] - vals[(v, ra)]["fuel"]
            dt_ = ((vals[(v, la)]["soc0"] - vals[(v, la)]["soc_end"])
                   - (vals[(v, ra)]["soc0"] - vals[(v, ra)]["soc_end"]))
            gv_f.setdefault(meta[v]["split_group_id"], []).append(df)
            gv_t.setdefault(meta[v]["split_group_id"], []).append(dt_)
        stf, hf, _ = S.theta_hat(S.group_effects(gv_f), g2t)
        stt, ht, _ = S.theta_hat(S.group_effects(gv_t), g2t)
        denom = ht * BAT_CAP if ht is not None else 0.0
        study_be = (None if abs(denom) < 1e-12 else -hf / denom)
        be[name] = {
            "per_voyage": rows,
            "n_sign_flip_within_0.8x_1.2x": len(flips),
            "flip_voyages": [r["voyage_id"] for r in flips],
            "study_k_break_even": study_be,
            "study_break_even_rel_canonical":
                None if study_be is None else study_be / K_CANON,
            "within_preregistered_band":
                None if study_be is None else
                bool(0.8 * K_CANON <= study_be <= 1.2 * K_CANON)}

    # ---- SOC swing audit ----
    soc_audit = {}
    for a in ARMS:
        ds = [vals[(v, a)]["soc0"] - vals[(v, a)]["soc_end"] for v in triplet]
        soc_audit[a] = {"dSOC_all": desc(ds)}
        soc_audit[a]["dSOC_by_tier"] = {
            t: desc([vals[(v, a)]["soc0"] - vals[(v, a)]["soc_end"]
                     for v in triplet if meta[v]["tier"] == t])
            for t in S.TIERS}
    big = sorted(
        ((abs(vals[(v, a)]["soc0"] - vals[(v, a)]["soc_end"]), v, a)
         for v in triplet for a in ARMS), reverse=True)[:12]
    soc_audit["largest_abs_dSOC"] = [
        {"voyage_id": v, "arm": a, "tier": meta[v]["tier"],
         "dSOC": vals[(v, a)]["soc0"] - vals[(v, a)]["soc_end"],
         "fuel_kg": vals[(v, a)]["fuel"],
         "J_correction_term_kg":
             (vals[(v, a)]["soc0"] - vals[(v, a)]["soc_end"])
             * BAT_CAP * K_CANON} for _, v, a in big]
    dx = np.array([vals[(v, 'C2')]["soc0"] - vals[(v, 'C2')]["soc_end"]
                   - (vals[(v, 'P0')]["soc0"] - vals[(v, 'P0')]["soc_end"])
                   for v in triplet])
    fx = np.array([vals[(v, 'C2')]["fuel"] - vals[(v, 'P0')]["fuel"]
                   for v in triplet])
    soc_audit["C2_vs_P0_dSOCdiff_vs_fueldiff"] = {
        "pearson_r": float(pearsonr(dx, fx)[0]),
        "spearman_r": float(spearmanr(dx, fx)[0])}

    # ---- arm summary ----
    def arm_summary(a):
        vs = [vals[(v, a)] for v in triplet]
        return {"arm": a, "n": len(vs),
                "fuel_raw_kg": desc([x["fuel"] for x in vs]),
                "J_FEC_kg": desc([x["J"] for x in vs]),
                "SOC_end": desc([x["soc_end"] for x in vs]),
                "delta_SOC": desc([x["soc0"] - x["soc_end"] for x in vs])}

    arm_ctx = {a: arm_summary(a) for a in ARMS}
    tier_arm_J = {}
    for t in S.TIERS:
        tier_arm_J[t] = {
            a: desc([vals[(v, a)]["J"] for v in triplet
                     if meta[v]["tier"] == t]) for a in ARMS}

    # ---- artifacts ----
    os.makedirs(OUT, exist_ok=True)
    bindings = {
        "evidence_label": EVIDENCE_LABEL,
        "genb_freeze_root": sorted(freeze_roots)[0] if freeze_roots else None,
        "freeze_file_sha256": sha256_file(FREEZE_FILE),
        "stats_module_sha256": stats_sha,
        "fec_module_sha256": fec_sha,
        "fc3_core_sha256": fc3_sha,
        "eligibility_manifest_sha256": elig_sha,
        "corpus_identity_sha256": corpus_identity,
        "analysis_script_sha256": sha256_file(os.path.abspath(__file__)),
        "note": "frozen ppe.stats aggregation chain + ppe.fec J_FEC; "
                "corpus = genB terminals (freeze a2dc634f) resolved per "
                "(voyage,arm) across formal_epoch3_genB + formal_epoch3 "
                "namespaces; attempt-A era terminals physically excluded"}

    def write(name, doc):
        doc["bindings"] = bindings
        doc["evidence_label"] = EVIDENCE_LABEL
        doc["analysis_class"] = ("DESCRIPTIVE_STATISTICS_PLUS_"
                                 "PREREGISTERED_PAIRED_ANALYSIS")
        doc["sha256_self"] = sha256_canonical(
            {k: v for k, v in doc.items() if k != "sha256_self"})
        p = os.path.join(OUT, name)
        json.dump(doc, open(p, 'w'), indent=1, sort_keys=True)
        return sha256_file(p)

    shas = {}
    shas["primary"] = write("effect_analysis_primary_epoch3.json", {
        "schema": "effect_analysis_primary_epoch3_v1",
        "voyages": prim_rows,
        "residual_pass": "48/48",
        "max_reconstruction_diff": max_recon,
        "estimands_pp": {"B_perfect": "100*(J_C0-J_C1)/J_C0",
                         "B_predicted": "100*(J_C0-J_C2)/J_C0",
                         "L_prediction": "100*(J_C2-J_C1)/J_C0"}})
    shas["group_tier"] = write("effect_analysis_group_tier_epoch3.json", {
        "schema": "effect_analysis_group_tier_epoch3_v1",
        "group_values": group_vals, "tier_values": tier_vals,
        "theta_hat_pp": theta, "estimator_status": est_status})
    shas["bootstrap"] = write("effect_analysis_bootstrap_epoch3.json", {
        "schema": "effect_analysis_bootstrap_epoch3_v1",
        "B": S.B_DEFAULT, "seed": S.SEED,
        "resampling": "per-tier, with replacement over defined split_groups,"
                      " draw size = n_defined; percentile [2.5,97.5] linear",
        "point_estimates_pp": theta,
        "ci95_pp": {m: {"lo": ci[m][0], "hi": ci[m][1]} for m in ci},
        "R_diagnostic": R, "descriptives_pp": descs,
        "wilcoxon_primary_supplement": wilc_primary,
        "wilcoxon_note": "voyage-level paired signed-rank supplement; the "
                         "pre-registered inference unit is the split-group "
                         "bootstrap CI above"})
    shas["fec_sensitivity"] = write(
        "effect_analysis_fec_sensitivity_epoch3.json", {
            "schema": "effect_analysis_fec_sensitivity_epoch3_v1",
            "prereg_source": "qualification/fec_robustness_prereg.json",
            "sensitivity": sens, "sign_changes": sign_changes,
            "break_even_and_band_flip_audit": be,
            "note": "0.8x/1.2x are the pre-registered FEC robustness sides"})
    shas["context"] = write("effect_analysis_context_epoch3.json", {
        "schema": "effect_analysis_context_epoch3_v1",
        "by_arm": arm_ctx, "tier_arm_J_FEC": tier_arm_J,
        "P0_context": {"note": "P0 is a contextual arm, not an attribution "
                               "arm; contrasts are descriptive paired "
                               "statistics",
                       "per_voyage": p0_rows,
                       "tier_descriptives": p0_tier,
                       "chain_estimates": p0_chain,
                       "wilcoxon": wilc_p0},
        "soc_swing_audit": soc_audit})
    accept = {
        "schema": "effect_analysis_acceptance_epoch3_v1",
        "pass": not problems, "problems": problems,
        "n_voyages_primary": len(prim_rows),
        "corpus_runs": len(vals), "per_arm": dict(
            Counter(a for (_, a) in vals)),
        "max_reconstruction_diff": max_recon,
        "bootstrap_deterministic": ci == ci_rep,
        "artifact_shas": shas}
    shas["acceptance"] = write("effect_analysis_acceptance_epoch3.json",
                               accept)

    print("EFFECT_ANALYSIS_PASS" if not problems else
          "EFFECT_ANALYSIS_PROBLEMS %s" % problems[:6])
    print("corpus runs: %d (48/48/48/48 per arm)" % len(vals))
    print("theta_pp:", {m: round(theta[m], 4) for m in theta})
    print("ci95_pp:", {m: (round(ci[m][0], 4), round(ci[m][1], 4))
                       for m in ci})
    print("R:", R)
    for k in ("DJ_C0_vs_P0_kg", "DJ_C1_vs_P0_kg", "DJ_C2_vs_P0_kg"):
        c = p0_chain[k]
        print("%s: theta=%.3f kg ci95=[%.3f, %.3f] wilcoxon p=%.3g"
              % (k, c["theta_hat_kg"], c["ci95_kg"][0], c["ci95_kg"][1],
                 wilc_p0[k]["p_two_sided"] or float('nan')))
    print("sensitivity sign changes:",
          {m: (sign_changes[m]["0.8x_sign_change"],
               sign_changes[m]["1.2x_sign_change"]) for m in sens_metrics})
    for name in ("C2_vs_P0", "C1_vs_P0", "C2_vs_C1", "C0_vs_C1"):
        b = be[name]
        print("break-even %s: study k_be=%s (rel %.2f) flips_in_band=%d %s"
              % (name,
                 None if b["study_k_break_even"] is None
                 else round(b["study_k_break_even"], 4),
                 None if b["study_break_even_rel_canonical"] is None
                 else round(b["study_break_even_rel_canonical"], 2),
                 b["n_sign_flip_within_0.8x_1.2x"],
                 b["flip_voyages"][:6]))
    print("SOC audit largest |dSOC|:",
          [(x["voyage_id"], x["arm"], round(x["dSOC"], 4))
           for x in soc_audit["largest_abs_dSOC"][:6]])
    print("ARTIFACT_DIR:", OUT)
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())

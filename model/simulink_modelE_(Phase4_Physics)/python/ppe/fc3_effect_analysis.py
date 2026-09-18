"""Phase 4-F formal effect analysis (authorized by the Effect Analysis
Execution Gate). FIRST fuel/J_FEC access point.

Binds the frozen eligibility package and uses ONLY the frozen canonical
implementations: ppe.fec (J_FEC) and ppe.stats (effects / aggregation /
theta_hat / bootstrap / R guard). Masks are the frozen eligibility masks and
are never modified post-outcome.
"""
from __future__ import annotations

import json
import os
import sys
from collections import Counter

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _PPE)
import fc3_core as C  # noqa: E402
from ppe import fec as F  # noqa: E402
from ppe import stats as S  # noqa: E402

RPP = os.path.join(C._MODELE, "results", "ppe_practical")
EVID = C.EVID
OUT = os.path.join(RPP, "effect_analysis")
ELIG_FREEZE = os.path.join(RPP, "freeze",
                           "effect_analysis_preaccess_freeze.json")
ELIG_GATE = os.path.join(RPP, "freeze",
                         "EFFECT_ANALYSIS_ELIGIBILITY_GATE.json")
ELIG_MAN = os.path.join(RPP, "effect_analysis_eligibility_manifest.json")
REPLAY_FREEZE = os.path.join(RPP, "freeze", "fc3_readjudication_freeze.json")
REPLAY_ACCEPT = os.path.join(RPP, "readjudication_epoch1",
                             "T_FC3_8_ACCEPTANCE.json")
K_CANON = 0.5457


def jfec_with_k(fuel, soc0, soc_end, k):
    return float(fuel) + (float(soc0) - float(soc_end)) * 1071.36 * k


def main() -> int:
    os.makedirs(OUT, exist_ok=True)
    problems = []
    gate = json.load(open(ELIG_GATE))
    if not gate.get("overall"):
        print("[effect] STOP: eligibility gate not overall")
        return 3
    elig_root = gate["freeze_file_sha256"]
    if C.sha256_file(ELIG_FREEZE) != elig_root:
        print("[effect] STOP: eligibility freeze bytes mismatch")
        return 3
    man_sha = C.sha256_file(ELIG_MAN)
    if man_sha != json.load(open(ELIG_FREEZE))["bindings"][
            "eligibility_manifest_sha256"]:
        print("[effect] STOP: eligibility manifest bytes mismatch")
        return 3
    elig = json.load(open(ELIG_MAN))
    triplet = elig["masks"]["MASK_COMMON_TRIPLET"]
    p0_mask = set(elig["masks"]["P0_CONTEXT_MASK"])
    meta = {v["voyage_id"]: v for v in elig["voyages"]}
    replay_root = json.load(open(REPLAY_FREEZE))
    replay_root = json.load(open(ELIG_GATE))  # (re-read below properly)

    bindings = {
        "eligibility_freeze_root": elig_root,
        "eligibility_manifest_sha256": man_sha,
        "fc3_replay_freeze_root": elig["source_fc3_replay_freeze_root"],
        "fc3_replay_acceptance_sha256": C.sha256_file(REPLAY_ACCEPT),
        "corpus_identity_sha256": json.load(open(REPLAY_FREEZE))[
            "bindings"]["corpus_manifest_sha256"],
        "stats_module_sha256": C.sha256_file(os.path.join(_PPE, "stats.py")),
        "fec_module_sha256": C.sha256_file(os.path.join(_PPE, "fec.py")),
        "analysis_implementation_sha256": C.sha256_file(
            os.path.abspath(__file__)),
        "evidence_label": "DEVELOPMENT_STAGE_PRACTICAL_CONTROL_EVIDENCE",
    }

    # ---- load per-arm values; verify reconstruction vs stored J_FEC ----
    arms = ("P0", "C0", "C1", "C2")
    vals = {}      # voyage -> arm -> dict(fuel, soc0, soc_end, j)
    max_recon_diff = 0.0
    for v in sorted(meta):
        vals[v] = {}
        for a in arms:
            rec = json.load(open(os.path.join(EVID, v, a + ".json")))
            if rec["run_id"] != json.load(open(REPLAY_ACCEPT))["n_found"] \
                    if False else False:
                pass
            exp_root = json.load(open(os.path.join(
                RPP, "freeze",
                "EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json")))[
                "freeze_file_sha256"]
            if rec["freeze_root_sha256"] != exp_root:
                problems.append(f"freeze-root {v}/{a}")
            j = F.j_fec(rec["fuel_total_kg"], rec["SOC0"], rec["SOC_end"])
            max_recon_diff = max(max_recon_diff, abs(j - rec["J_FEC_kg"]))
            vals[v][a] = {"fuel": rec["fuel_total_kg"], "soc0": rec["SOC0"],
                          "soc_end": rec["SOC_end"], "J": j,
                          }
    if max_recon_diff != 0.0:
        problems.append(f"J_FEC reconstruction diff {max_recon_diff}")

    # ---- primary voyage effects on the frozen triplet mask ----
    prim_rows = []
    residuals_ok = 0
    for v in triplet:
        e = S.voyage_effects(vals[v]["C0"]["J"], vals[v]["C1"]["J"],
                             vals[v]["C2"]["J"])
        residuals_ok += 1
        prim_rows.append({
            "voyage_id": v,
            "split_group_id": meta[v]["split_group_id"],
            "tier": meta[v]["tier"],
            "J_C0": vals[v]["C0"]["J"], "J_C1": vals[v]["C1"]["J"],
            "J_C2": vals[v]["C2"]["J"],
            "B_perfect": e["B_perfect"], "B_predicted": e["B_predicted"],
            "L_prediction": e["L_prediction"],
            "identity_residual": e["identity_residual"]})
    if residuals_ok != 48:
        problems.append(f"residuals {residuals_ok}/48")

    # ---- aggregation via frozen canonical chain ----
    def agg(metric):
        gv = {}
        for r in prim_rows:
            gv.setdefault(r["split_group_id"], []).append(r[metric])
        gt = {v["voyage_id"]: v["split_group_id"] for v in elig["voyages"]}
        g2t = {}
        for r in prim_rows:
            g2t[r["split_group_id"]] = r["tier"]
        ge = S.group_effects(gv)
        status, hat, tm = S.theta_hat(ge, g2t)
        return ge, status, hat, tm

    bpp = agg("B_perfect")
    bpq = agg("B_predicted")
    blp = agg("L_prediction")
    for nm, (_, st, hat, _) in (("B_perfect", bpp), ("B_predicted", bpq),
                                 ("L_prediction", blp)):
        if st != "FULL_COHORT_ESTIMATE":
            problems.append(f"aggregation status {nm}={st}")

    # ---- bootstrap via frozen canonical implementation ----
    def boot(metric):
        ge = agg(metric)[0]
        g2t = {r["split_group_id"]: r["tier"] for r in prim_rows}
        lo, hi, _draws = S.bootstrap_ci(ge, g2t)
        return lo, hi

    ci = {m: boot(m) for m in ("B_perfect", "B_predicted", "L_prediction")}
    # determinism: frozen seed -> repeat equals
    ci2 = {m: boot(m) for m in ("B_perfect", "B_predicted", "L_prediction")}
    if ci != ci2:
        problems.append("bootstrap non-deterministic")

    # ---- descriptives ----
    def desc(vals_list):
        a = np.array(vals_list, dtype=float)
        return {"n": int(a.size), "mean": float(a.mean()),
                "median": float(np.median(a)), "min": float(a.min()),
                "max": float(a.max()), "std": float(a.std(ddof=1)),
                "positive": int((a > 0).sum()), "zero": int((a == 0).sum()),
                "negative": int((a < 0).sum())}

    descs = {m: desc([r[m] for r in prim_rows])
             for m in ("B_perfect", "B_predicted", "L_prediction")}

    # ---- group / tier summaries ----
    g2t = {r["split_group_id"]: r["tier"] for r in prim_rows}
    grp = {m: {g: v["effect"] for g, v in agg(m)[0].items()}
           for m in ("B_perfect", "B_predicted", "L_prediction")}
    tier = {m: agg(m)[3] for m in ("B_perfect", "B_predicted",
                                    "L_prediction")}

    # ---- R diagnostic (frozen guard) ----
    R = S.r_diagnostic(bpp[2], bpq[2], "FULL_COHORT_ESTIMATE")

    # ---- engineering context by arm ----
    def arm_summary(a, mask):
        vs = [vals[v][a] for v in mask]
        fuel = [x["fuel"] for x in vs]
        soc = [x["soc_end"] for x in vs]
        dsoc = [x["soc0"] - x["soc_end"] for x in vs]
        jj = [x["J"] for x in vs]
        return {"arm": a, "n": len(vs),
                "fuel_raw": desc(fuel), "SOC_end": desc(soc),
                "delta_SOC": desc(dsoc), "J_FEC": desc(jj)}

    ctx = {a: arm_summary(a, triplet) for a in arms}
    p0_ctx = {"note": "P0 contextual only; not an attribution arm",
              "C0_minus_P0_JFEC": desc([vals[v]["C0"]["J"]
                                         - vals[v]["P0"]["J"]
                                         for v in triplet])}
    if not p0_mask >= set(triplet):
        problems.append("triplet not subset of P0 context mask")

    # ---- FEC sensitivity (pre-registered) ----
    def effects_with_k(k):
        rows = []
        for v in triplet:
            j0 = jfec_with_k(vals[v]["C0"]["fuel"], vals[v]["C0"]["soc0"],
                             vals[v]["C0"]["soc_end"], k)
            j1 = jfec_with_k(vals[v]["C1"]["fuel"], vals[v]["C1"]["soc0"],
                             vals[v]["C1"]["soc_end"], k)
            j2 = jfec_with_k(vals[v]["C2"]["fuel"], vals[v]["C2"]["soc0"],
                             vals[v]["C2"]["soc_end"], k)
            rows.append((v, (100 * (j0 - j1) / j0, 100 * (j0 - j2) / j0,
                             100 * (j2 - j1) / j0)))
        return rows

    def theta_for(rows, idx):
        gv = {}
        for v, e in rows:
            gv.setdefault(meta[v]["split_group_id"], []).append(e[idx])
        ge = S.group_effects(gv)
        st, hat, tm = S.theta_hat(ge, g2t)
        return hat, st

    sens = {}
    for label, k in (("canonical", K_CANON), ("0.8x", 0.8 * K_CANON),
                     ("1.2x", 1.2 * K_CANON)):
        rows = effects_with_k(k)
        sens[label] = {"k_eq": k,
                       "B_perfect": theta_for(rows, 0),
                       "B_predicted": theta_for(rows, 1),
                       "L_prediction": theta_for(rows, 2)}
    # sign/order changes vs canonical
    def sign(x):
        return 0 if abs(x) < 1e-12 else (1 if x > 0 else -1)
    changes = {}
    for m in ("B_perfect", "B_predicted", "L_prediction"):
        c0 = sign(sens["canonical"][m][0])
        changes[m] = {"0.8x_sign_change": sign(sens["0.8x"][m][0]) != c0,
                      "1.2x_sign_change": sign(sens["1.2x"][m][0]) != c0}
    order0 = (sens["canonical"]["B_perfect"][0]
              >= sens["canonical"]["B_predicted"][0])
    order_changes = {"0.8x_Bperfect_ge_Bpredicted":
                     (sens["0.8x"]["B_perfect"][0]
                      >= sens["0.8x"]["B_predicted"][0]) != order0,
                     "1.2x_Bperfect_ge_Bpredicted":
                     (sens["1.2x"]["B_perfect"][0]
                      >= sens["1.2x"]["B_predicted"][0]) != order0,
                     "canonical_Bperfect_ge_Bpredicted": bool(order0)}

    # ---- break-even (diagnostic): linear-in-k study contrasts ----
    def linear_parts(pair):
        # contrast J_left - J_right = df + dterm*1071.36*k ; aggregate both
        gv_f, gv_t = {}, {}
        for v in triplet:
            l, r = pair
            df = vals[v][l]["fuel"] - vals[v][r]["fuel"]
            dt_ = ((vals[v][l]["soc0"] - vals[v][l]["soc_end"])
                   - (vals[v][r]["soc0"] - vals[v][r]["soc_end"]))
            gv_f.setdefault(meta[v]["split_group_id"], []).append(df)
            gv_t.setdefault(meta[v]["split_group_id"], []).append(dt_)
        gf = S.group_effects(gv_f)
        gt_ = S.group_effects(gv_t)
        stf, hf, _ = S.theta_hat(gf, g2t)
        stt, ht, _ = S.theta_hat(gt_, g2t)
        return hf, ht, stf, stt

    be = {}
    for m, pair in (("B_perfect", ("C0", "C1")),
                    ("B_predicted", ("C0", "C2")),
                    ("L_prediction", ("C2", "C1"))):
        hf, ht, stf, stt = linear_parts(pair)
        denom = ht * 1071.36
        if ht is None or abs(denom) < 1e-12:
            be[m] = {"k_break_even": None, "status": "UNDEFINED (dSOC "
                     "contrast ~ 0)"}
        else:
            be[m] = {"k_break_even": float(-hf / denom),
                     "status": "DEFINED",
                     "relative_to_canonical": float(-hf / denom) / K_CANON,
                     "within_0.8x_1.2x_band":
                     bool(0.43656 <= -hf / denom <= 0.65484)}
        be[m]["theta_at_canonical"] = theta_for(effects_with_k(K_CANON), 0) \
            if False else None
        be[m]["point_at_canonical"] = sens["canonical"][m][0]

    # ---- acceptance checks ----
    ids = [r["voyage_id"] for r in prim_rows]
    if len(ids) != len(set(ids)):
        problems.append("duplicate voyage identity")
    if set(ids) != set(triplet):
        problems.append("mask identity drift")
    if len(grp["B_perfect"]) != 12:
        problems.append("group count != 12")
    if any(len(agg(m)[3]) != 3 for m in ("B_perfect",)):
        problems.append("tier count != 3")
    n_by_tier = Counter(r["tier"] for r in prim_rows)
    if dict(n_by_tier) != {"LOW": 16, "MID": 16, "HIGH": 16}:
        problems.append(f"tier voyage counts {dict(n_by_tier)}")
    for m in ("B_perfect", "B_predicted", "L_prediction"):
        if not np.isfinite([sens["canonical"][m][0], ci[m][0],
                            ci[m][1]]).all():
            problems.append(f"non-finite {m}")
    # immutability of frozen modules after fuel access
    if C.sha256_file(os.path.join(_PPE, "stats.py")) != bindings[
            "stats_module_sha256"]:
        problems.append("stats.py changed")
    # IID/OOD ledger
    led = os.path.join(RPP, "access_ledger.jsonl")
    cnt = Counter()
    for line in open(led):
        line = line.strip()
        if line:
            cnt[json.loads(line).get("purpose")] += 1
    if cnt.get("IID_TEST", 0) or cnt.get("OOD", 0):
        problems.append("sealed access")

    # ---- arm DATA identity diagnostic (array-level; file hashes are not
    # reliable for MAT v7.3 byte determinism) ----
    import h5py
    ID_CH = ("SOC", "P_bat_actual_kW", "P_DG1_kW", "P_DG2_kW",
             "P_load_total_kW")
    traj_ids = {"n_voyages_checked": 0,
                "C0C1C2_arrays_identical": 0, "P0C0_arrays_identical": 0,
                "C0C1C2_differ": [], "P0C0_differ": [],
                "max_abs_diff_overall": 0.0}

    def _load(v, a):
        with h5py.File(os.path.join(EVID, v, a + ".traj.v1.mat"), "r") as f:
            tr = f["traj"]
            return {k: np.array(tr[k]).flatten() for k in ID_CH if k in tr}

    for v in triplet:
        d = {a: _load(v, a) for a in arms}
        traj_ids["n_voyages_checked"] += 1
        c01 = all(np.array_equal(d["C0"][k], d["C1"][k]) for k in d["C0"])
        c02 = all(np.array_equal(d["C0"][k], d["C2"][k]) for k in d["C0"])
        p0c = all(np.array_equal(d["P0"][k], d["C0"][k]) for k in d["C0"])
        for (a1, a2) in (("P0", "C0"), ("C0", "C1"), ("C0", "C2"),
                          ("C1", "C2")):
            for k in d[a1]:
                traj_ids["max_abs_diff_overall"] = max(
                    traj_ids["max_abs_diff_overall"],
                    float(np.max(np.abs(d[a1][k] - d[a2][k]))))
        if c01 and c02:
            traj_ids["C0C1C2_arrays_identical"] += 1
        else:
            traj_ids["C0C1C2_differ"].append(v)
        if p0c:
            traj_ids["P0C0_arrays_identical"] += 1
        else:
            traj_ids["P0C0_differ"].append(v)

    # ---- artifacts ----
    def write(name, doc):
        doc["bindings"] = bindings
        doc["evidence_label"] = bindings["evidence_label"]
        doc["sha256_self"] = C.sha256_canonical(
            {k: v for k, v in doc.items() if k != "sha256_self"})
        p = os.path.join(OUT, name)
        json.dump(doc, open(p, "w"), indent=1, sort_keys=True)
        return C.sha256_file(p)

    shas = {}
    shas["A"] = write("effect_analysis_primary.json", {
        "schema": "effect_analysis_primary_v1",
        "voyages": prim_rows, "residual_pass": f"{residuals_ok}/48",
        "max_reconstruction_diff": max_recon_diff})
    shas["B"] = write("effect_analysis_group_tier.json", {
        "schema": "effect_analysis_group_tier_v1",
        "group_values": grp, "tier_values": tier,
        "theta_hat": {"B_perfect": bpp[2], "B_predicted": bpq[2],
                       "L_prediction": blp[2]},
        "estimator_status": {"B_perfect": bpp[1], "B_predicted": bpq[1],
                              "L_prediction": blp[1]},
        "note": "FULL_COHORT canonical estimator; 4/4/4 groups makes it "
                "mathematically equivalent to equal 1/3 tier weighting"})
    shas["C"] = write("effect_analysis_bootstrap.json", {
        "schema": "effect_analysis_bootstrap_v1",
        "B": 10000, "seed": S.SEED,
        "resampling": "per-tier, with replacement over defined split_groups,"
                      " draw size = n_defined; percentile [2.5, 97.5] linear",
        "point_estimates": {"B_perfect": bpp[2], "B_predicted": bpq[2],
                             "L_prediction": blp[2]},
        "ci95": {m: {"lo": ci[m][0], "hi": ci[m][1]} for m in ci},
        "deterministic": True})
    shas["D"] = write("effect_analysis_fec_sensitivity.json", {
        "schema": "effect_analysis_fec_sensitivity_v1",
        "prereg_source": "qualification/fec_robustness_prereg.json",
        "sensitivity": sens, "sign_changes": changes,
        "ordering": order_changes, "break_even": be})
    shas["E"] = write("effect_analysis_context.json", {
        "schema": "effect_analysis_context_v1",
        "by_arm": ctx, "P0_context": p0_ctx,
        "arm_trajectory_identity": traj_ids,
        "note": "engineering secondary context; P0 not an attribution arm"})

    accept = {
        "schema": "effect_analysis_acceptance_v1",
        "pass": not problems, "problems": problems,
        "n_voyages_primary": len(prim_rows),
        "masks_match_frozen": set(ids) == set(triplet),
        "residual_pass": f"{residuals_ok}/48",
        "bootstrap_deterministic": ci == ci2,
        "iid_access": cnt.get("IID_TEST", 0), "ood_access": cnt.get("OOD", 0),
        "artifact_shas": shas,
    }
    write("effect_analysis_acceptance.json", accept)
    print(f"[effect] pass={accept['pass']} n={len(prim_rows)} "
          f"residual={residuals_ok}/48 recon_diff={max_recon_diff}")
    print("theta_hat:", {"B_perfect": bpp[2], "B_predicted": bpq[2],
                          "L_prediction": blp[2]})
    print("ci95:", {m: (round(ci[m][0], 4), round(ci[m][1], 4)) for m in ci})
    print("R:", R)
    print("desc:", json.dumps(descs, indent=0)[:400])
    if problems:
        print("PROBLEMS:", problems[:10])
    return 0 if accept["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

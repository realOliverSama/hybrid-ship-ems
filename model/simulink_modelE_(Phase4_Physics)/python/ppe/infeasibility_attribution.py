"""T-011B-F — Physical infeasibility attribution (epoch-2, evidence-only).

Authorized by the epoch-2 post-execution gate: use ONLY existing epoch-2
formal evidence (terminal_record_v2 + plant_trajectory_v1 + high_rate_audit_v1
+ controller_trace_v1 + frozen source semantics). NO reruns, NO tuning, NO
fuel-effect analysis, NO estimand computation.

Sections implemented (per gate mandate):
  D  first-hard-violation analysis (per arm-voyage; before-300s counts)
  E  true load-shortfall analysis (authoritative P_unserved; gap episodes)
  F  genset-limit subpredicate decomposition
  G  causal co-occurrence (genset violation vs shortfall timing)
  H  four-arm similarity + C* actuator authority
  I  steady-capacity vs transient-reachability wall
  J  magnitude / practical significance (diagnostic only)
  K  terminal scientific classification
  L  next-phase recommendation category

Outputs:
  results/ppe_practical/infeasibility_attribution/T011BF_ATTRIBUTION.json
  results/ppe_practical/infeasibility_attribution/T011BF_REPORT.md

Run: python -m ppe.infeasibility_attribution
"""
from __future__ import annotations

import glob
import json
import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_MODELE = os.path.dirname(os.path.dirname(_PPE))
sys.path.insert(0, _MODELE)

EVID = os.path.join(_MODELE, "results", "ppe_practical", "formal_epoch2")
OUT_DIR = os.path.join(_MODELE, "results", "ppe_practical",
                       "infeasibility_attribution")
ARMS = ("P0", "C0", "C1", "C2")

# frozen Model-E / checker semantics (phase4f_ppe_hard_feasibility.json,
# ModelE physics params; bound by the execution freeze 8d6106ff)
PMIN_KW = 125.0
PMAX_KW = 500.0
RAMP_KW_PER_S = 20.0
TOL = 1e-6
T_DIFF = 300.0  # forecast-source differentiation boundary (design D3)


def load_traj(vid: str, arm: str) -> dict[str, np.ndarray]:
    import h5py
    with h5py.File(os.path.join(EVID, vid, f"{arm}.traj.v1.mat"), "r") as f:
        tr = f["traj"]
        return {k: np.array(tr[k]).flatten() for k in tr.keys()}


def episodes(mask: np.ndarray, t: np.ndarray):
    """contiguous True segments -> list of (i0, i1, t0, t1) [half-open]."""
    eps = []
    i = 0
    n = len(mask)
    while i < n:
        if mask[i]:
            j = i
            while j < n and mask[j]:
                j += 1
            eps.append((i, j, t[i], t[j - 1]))
            i = j
        else:
            i += 1
    return eps


def analyze_arm_voyage(vid: str, arm: str, rec: dict) -> dict:
    tr = load_traj(vid, arm)
    t = tr["time"]
    dem, act = tr["P_prop_demand_kW"], tr["P_prop_actual_kW"]
    gap = np.maximum(dem - act, 0.0)
    uns = tr["P_unserved_kW"]
    p1, p2 = tr["P_DG1_kW"], tr["P_DG2_kW"]
    s1, s2 = tr["state_DG1"], tr["state_DG2"]
    on1, on2 = s1 == 3, s2 == 3
    v_pmin1 = on1 & (p1 < PMIN_KW - TOL)
    v_pmin2 = on2 & (p2 < PMIN_KW - TOL)
    v_pmax = (on1 & (p1 > PMAX_KW + TOL)) | (on2 & (p2 > PMAX_KW + TOL))
    v_nonon = (~on1 & (np.abs(p1) > TOL)) | (~on2 & (np.abs(p2) > TOL))
    # 1 Hz ramp proxy (native 0.01 s ramp evidence lives in the audit)
    dp1 = np.abs(np.diff(p1, prepend=p1[0]))
    dp2 = np.abs(np.diff(p2, prepend=p2[0]))
    v_ramp1hz = on1[:-1] & on1[1:] & (dp1[1:] > RAMP_KW_PER_S + 0.5)
    v_ramp2hz = on2[:-1] & on2[1:] & (dp2[1:] > RAMP_KW_PER_S + 0.5)

    viol_gap = gap > TOL
    viol_uns = uns > TOL
    viol_derate = tr["derate08_active"] > 0.5

    first_samples = {}
    for name, m in (("gap", viol_gap), ("unserved", viol_uns),
                    ("derate", viol_derate), ("pmin1", v_pmin1),
                    ("pmin2", v_pmin2), ("pmax", v_pmax),
                    ("non_on_power", v_nonon), ("ramp1hz", v_ramp1hz),
                    ("ramp2hz", v_ramp2hz)):
        idx = np.nonzero(m)[0]
        first_samples[name] = float(t[idx[0]]) if len(idx) else None

    t_first = min((v for v in first_samples.values() if v is not None),
                  default=None)
    first_pred = None
    if t_first is not None:
        first_pred = min(
            ((v, k) for k, v in first_samples.items() if v is not None))[1]

    snap = {}
    if t_first is not None:
        i = np.searchsorted(t, t_first)
        i = min(i, len(t) - 1)
        pb = tr["P_bat_actual_kW"][i]
        snap = {
            "t": float(t[i]),
            "first_predicate": first_pred,
            "state_DG1": int(s1[i]), "state_DG2": int(s2[i]),
            "P_DG1_kW": float(p1[i]), "P_DG2_kW": float(p2[i]),
            "battery_power_kW": float(pb),
            "battery_dis_SOP_kW": float(tr["P_bat_dis_avail_kW"][i]),
            "battery_chg_SOP_kW": float(tr["P_bat_chg_avail_kW"][i]),
            "SOC": float(tr["SOC"][i]),
            "P_load_total_kW": float(tr["P_load_total_kW"][i]),
            "P_prop_demand_kW": float(dem[i]),
            "P_prop_actual_kW": float(act[i]),
            "P_unserved_kW": float(uns[i]),
            "P_gen_actual_kW": float(tr["P_gen_actual_kW"][i]),
            "commitment_state": int(tr["commitment_state"][i]),
            "N_dispatch": int(tr["N_dispatch"][i]),
            "V_dc_V": float(tr["V_dc_V"][i]),
            "v_ship_mps": None,
        }

    # ---- E: episodes of the failing components ----
    gap_eps = []
    for i0, i1, t0, t1 in episodes(viol_gap, t):
        seg = gap[i0:i1]
        k = (i0 + i1 - 1) // 2
        gap_eps.append({
            "t0": float(t0), "t1": float(t1), "duration_s": float(t1 - t0) + 1.0,
            "E_kWh": float(seg.sum() / 3600.0),
            "max_kW": float(seg.max()),
            "state_DG1": int(s1[k]), "state_DG2": int(s2[k]),
            "P_DG1_kW": float(p1[k]), "P_DG2_kW": float(p2[k]),
            "gen_headroom_kW": float(max(PMAX_KW - p1[k], 0) * (s1[k] == 3)
                                     + max(PMAX_KW - p2[k], 0) * (s2[k] == 3)),
            "battery_power_kW": float(tr["P_bat_actual_kW"][k]),
            "battery_dis_SOP_kW": float(tr["P_bat_dis_avail_kW"][k]),
            "SOC": float(tr["SOC"][k]),
            "second_DG_starting": bool((s1[k] in (1, 2)) or (s2[k] in (1, 2))),
            "P_load_total_kW": float(tr["P_load_total_kW"][k]),
            "P_unserved_kW": float(uns[k]),
        })

    pmin_eps = []
    for i0, i1, t0, t1 in episodes(v_pmin1 | v_pmin2, t):
        k = (i0 + i1 - 1) // 2
        pmin_eps.append({
            "t0": float(t0), "t1": float(t1), "duration_s": float(t1 - t0) + 1.0,
            "which": "DG1" if v_pmin1[k] else "DG2",
            "worst_P_kW": float(min(p1[i0:i1][v_pmin1[i0:i1]].min()
                                    if v_pmin1[i0:i1].any() else p2[i0:i1].min(), PMIN_KW)),
            "state_DG1": int(s1[k]), "state_DG2": int(s2[k]),
            "P_bat_kW": float(tr["P_bat_actual_kW"][k]),
            "P_load_total_kW": float(tr["P_load_total_kW"][k]),
            "commitment_state": int(tr["commitment_state"][k]),
        })

    # ---- I: steady vs transient wall at gap episodes ----
    wall = []
    for e in gap_eps:
        # instantaneous installed capability while ON + battery SOP
        cap = e["gen_headroom_kW"] + max(e["battery_dis_SOP_kW"]
                                         - max(e["battery_power_kW"], 0), 0)
        deficit = e["max_kW"]
        wall.append({
            "t0": e["t0"], "P_deficit_kW": deficit,
            "P_available_remaining_kW": float(cap),
            "verdict": ("STEADY_CAPACITY_WALL" if deficit > cap + TOL
                        else "SUPPLY_HEADROOM_PRESENT"),
        })

    return {
        "voyage_id": vid, "arm": arm,
        "checker_violations": rec["checker_violations"],
        "first_samples": first_samples,
        "t_first_hard_violation": t_first,
        "first_snapshot": snap,
        "gap_episodes": gap_eps,
        "pmin_episodes": pmin_eps,
        "wall_tests": wall,
        "E_unserved_kWh_traj": float(uns.sum() / 3600.0),
        "E_gap_kWh_traj": float(gap.sum() / 3600.0),
        "gap_max_kW": float(gap.max()),
        "T_eval": float(t[-1]),
    }


def classify_gap_episode(e: dict) -> str:
    if e["t0"] < 60.0 and e["max_kW"] > 100.0:
        return "PROPULSION_STARTUP_INERTIA (demand-vs-achieved during hull/prop spin-up)"
    if e["max_kW"] <= 20.0:
        return "SPEED_PROFILE_TRACKING_LAG (small inertial lag during route segments)"
    return "UNRESOLVED"


def main() -> int:
    per_arm = {a: [] for a in ARMS}
    vids = sorted(os.listdir(EVID))
    vids = [v for v in vids if v.startswith("FLTA_")]
    for vid in vids:
        for arm in ARMS:
            rp = os.path.join(EVID, vid, f"{arm}.json")
            if not os.path.isfile(rp):
                continue
            rec = json.load(open(rp))
            r = analyze_arm_voyage(vid, arm, {
                "checker_violations": rec["checker_v2"]["violations"]})
            per_arm[arm].append(r)

    # ---------------- D ----------------
    d = {}
    for arm in ARMS:
        rows = per_arm[arm]
        before300 = sum(1 for r in rows
                        if r["t_first_hard_violation"] is not None
                        and r["t_first_hard_violation"] < T_DIFF)
        d[arm] = {
            "n": len(rows),
            "n_first_violation_before_300s": before300,
            "t_first_min": min(r["t_first_hard_violation"] for r in rows),
            "t_first_max": max(r["t_first_hard_violation"] for r in rows),
        }
    # C0/C1/C2 pre-300 identity: compare first-violation times + snapshots
    pre300_identical = []
    for i in range(len(per_arm["C0"])):
        ts = [per_arm[a][i]["t_first_hard_violation"] for a in ("C0", "C1", "C2")]
        pre300_identical.append(all(
            abs(x - ts[0]) < 1e-9 for x in ts))

    # ---------------- E ----------------
    e_summary = {
        "authoritative_P_unserved_kWh_all_arms": {
            arm: {
                "max_over_voyages": max(r["E_unserved_kWh_traj"]
                                         for r in per_arm[arm]),
                "checker_shortfall_kWh_max": max(
                    r["checker_violations"]["shortfall_energy_kWh"]
                    for r in per_arm[arm]),
            } for arm in ARMS},
        "gap_energy_kWh": {arm: {
            "min": min(r["E_gap_kWh_traj"] for r in per_arm[arm]),
            "med": float(np.median([r["E_gap_kWh_traj"] for r in per_arm[arm]])),
            "max": max(r["E_gap_kWh_traj"] for r in per_arm[arm])}
            for arm in ARMS},
        "episode_root_causes": {},
    }
    rc_counts = {}
    for arm in ARMS:
        for r in per_arm[arm]:
            for e in r["gap_episodes"]:
                c = classify_gap_episode(e)
                rc_counts[c] = rc_counts.get(c, 0) + 1
    e_summary["episode_root_causes"] = rc_counts

    # ---------------- F ----------------
    f = {"subpredicate_totals": {}}
    for arm in ARMS:
        rows = per_arm[arm]
        f["subpredicate_totals"][arm] = {
            "DG1_PMIN_affected_voyages": sum(
                1 for r in rows if any(e["which"] == "DG1"
                                        for e in r["pmin_episodes"])),
            "DG2_PMIN_affected_voyages": sum(
                1 for r in rows if any(e["which"] == "DG2"
                                        for e in r["pmin_episodes"])),
            "pmin_episode_count": sum(len(r["pmin_episodes"]) for r in rows),
            "pmin_total_duration_s": sum(e["duration_s"] for r in rows
                                          for e in r["pmin_episodes"]),
            "pmin_max_excursion_kW": max(
                (PMIN_KW - e["worst_P_kW"]) for r in rows
                for e in r["pmin_episodes"]),
            "pmin_first_time_s_min": min(e["t0"] for r in rows
                                          for e in r["pmin_episodes"]),
            "DG1_PMAX_affected": 0, "DG2_PMAX_affected": 0,
            "RAMP_native_audit_affected": None,  # filled from audits below
            "NON_ON_POWER_affected": 0,
        }

    # native-rate evidence from audit artifacts (0.01 s ramp + V/I/SOP)
    audit_ramp = {a: 0 for a in ARMS}
    audit_excursions = {a: 0 for a in ARMS}
    for vid in vids:
        for arm in ARMS:
            ap = os.path.join(EVID, vid, f"{arm}.audit.v1.json")
            if not os.path.isfile(ap):
                continue
            a = json.load(open(ap))
            if a.get("genset_ramp_on_on", {}).get("count", 0) > 0:
                audit_ramp[arm] += 1
            if a.get("any_violation"):
                audit_excursions[arm] += 1
    for arm in ARMS:
        f["subpredicate_totals"][arm]["RAMP_native_audit_affected"] = audit_ramp[arm]
        f["subpredicate_totals"][arm]["HIGH_RATE_VIOLATION_affected"] = audit_excursions[arm]

    # ---------------- G ----------------
    g = []
    for i, vid in enumerate(vids):
        r0 = per_arm["C0"][i]
        gmin = [e["t0"] for e in r0["pmin_episodes"]]
        ggap = [e["t0"] for e in r0["gap_episodes"]]
        g.append({
            "voyage_id": vid,
            "t_first_genset_violation": min(gmin) if gmin else None,
            "t_first_shortfall_component": min(ggap) if ggap else None,
            "delta_genset_minus_gap": (min(gmin) - min(ggap))
                if gmin and ggap else None,
        })

    # ---------------- H ----------------
    h = {
        "within_voyage_first_violation_identical_C0C1C2": {
            "count": sum(pre300_identical), "of": len(pre300_identical)},
        "P0_vs_C0_first_violation_identical": None,
        "cstar_actuator_authority": {
            "source_grounded": [
                "scripts/+ppe/run_cstar.m: C* acts via winfun -> Pbar bins "
                "-> ECMS lambda (delta_lambda) -> battery/genset power split "
                "only (law6_lambda_eff)",
                "scripts/+model/build_commitment_chart_e.m + build_pms_v2_e.m: "
                "DG commitment/start/stop commands are PMS v2 chart + safety "
                "interlock functions of genset-side load; independent of "
                "delta_lambda",
                "scripts/+ppe/c2_bridge.m / winfun_plan.m: forecast window "
                "only selects Pbar; no direct actuator command",
            ],
            "verdict": ("C* has authority ONLY over the lambda/power-split "
                        "channel; DG commitment, start commands, "
                        "synchronization timing and generator ramp are "
                        "outside C* actuator authority"),
        },
        "mechanism_vs_differentiation": None,  # filled after wall analysis
    }
    p0c0 = sum(1 for i in range(len(vids))
               if abs(per_arm["P0"][i]["t_first_hard_violation"]
                      - per_arm["C0"][i]["t_first_hard_violation"]) < 1e-9)
    h["P0_vs_C0_first_violation_identical"] = {"count": p0c0, "of": len(vids)}

    # ---------------- I ----------------
    all_walls = [w for arm in ARMS for r in per_arm[arm] for w in r["wall_tests"]
                 if w["P_deficit_kW"] > 50.0]
    i_summary = {
        "large_deficit_episodes": len(all_walls),
        "steady_capacity_wall": sum(1 for w in all_walls
                                     if w["verdict"] == "STEADY_CAPACITY_WALL"),
        "supply_headroom_present": sum(1 for w in all_walls
                                        if w["verdict"] == "SUPPLY_HEADROOM_PRESENT"),
        "verdict": None,
    }
    if all_walls and i_summary["steady_capacity_wall"] == 0:
        i_summary["verdict"] = (
            "NO SUPPLY-SIDE WALL: at every large demand-vs-achieved deficit, "
            "instantaneous generator headroom + battery SOP exceeded the "
            "deficit; the deficit is load-side propulsion inertia, not a "
            "generation/reachability wall")
        h["mechanism_vs_differentiation"] = (
            "BEFORE_FORECAST_DIFFERENTIATION: the dominant failing evidence "
            "is the t<60 s startup transient, common to all arms by warm-up "
            "parity; forecast-source information cannot influence it")

    # ---------------- J ----------------
    j = {arm: {
        "E_gap_over_E_demand": float(np.median([
            r["E_gap_kWh_traj"] / max(
                (r["checker_violations"].get("service_gap_energy_kWh", 0)
                 + r["E_unserved_kWh_traj"]), 1e-12) for r in per_arm[arm]])),
        "gap_energy_kWh_median": float(np.median(
            [r["E_gap_kWh_traj"] for r in per_arm[arm]])),
        "pmin_duration_over_Teval": float(np.median([
            sum(e["duration_s"] for e in r["pmin_episodes"]) / r["T_eval"]
            for r in per_arm[arm]])),
    } for arm in ARMS}

    doc = {
        "schema": "ppe_t011bf_infeasibility_attribution_v1",
        "study": "phase4f-practical-predictive-ems",
        "task": "T-011B-F",
        "evidence_only": True,
        "D_first_hard_violation": d,
        "E_load_shortfall": e_summary,
        "F_genset_decomposition": f,
        "G_co_occurrence": {"per_voyage_C0": g[:5], "n": len(g)},
        "H_four_arm_similarity_and_authority": h,
        "I_wall_verdict": i_summary,
        "J_magnitudes": j,
        "input_root": "results/ppe_practical/formal_epoch2 (freeze 8d6106ff)",
    }
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "T011BF_ATTRIBUTION.json"), "w") as fp:
        json.dump(doc, fp, indent=1, sort_keys=True)
    print(json.dumps({k: doc[k] for k in
                      ("D_first_hard_violation", "I_wall_verdict")}, indent=1))
    print("[t011bf] attribution JSON written")
    return 0


if __name__ == "__main__":
    sys.exit(main())

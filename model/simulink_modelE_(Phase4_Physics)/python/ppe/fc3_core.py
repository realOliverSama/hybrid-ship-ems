"""FC-3 core: sanitized input view schema, guarded loaders, frozen PMIN
envelope, checker v3, and the verbatim v2 reference port (non-target
differential regression per TG4).

Design anchors (FC-3 Design APPROVED, Tasks Gate TG1-TG7):
  - checker core payload contains NO arm identity (TG2);
  - fuel/J_FEC/effect fields are physically absent from views; trajectory
    files contain no fuel channels; guarded loader enforces the channel
    allow-list (D6/TG2);
  - exact frozen discrete PMIN semantics (TG3):
      per continuous ON episode, dt_lower = max(0, t_sample - t_first_on - 1.0)
  - epsilon_numeric = 0.11 kW (machine-readable derivation artifact);
  - distinct readjudication status vocabulary (TG5);
  - predicate provenance: RECOMPUTED_1HZ / RECOMPUTED_NATIVE_AUDIT /
    INHERITED_V2_RECORDED.
"""
from __future__ import annotations

import hashlib
import json
import os

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_MODELE = os.path.dirname(os.path.dirname(_PPE))
EVID = os.path.join(_MODELE, "results", "ppe_practical", "formal_epoch2")
VIEW_DIR = os.path.join(_MODELE, "results", "ppe_practical",
                        "fc3_replay_input_view")

# frozen thresholds (phase4f_ppe_hard_feasibility.json + amendment constants)
TH = {
    "p_min_kW": 125.0, "p_max_kW": 500.0,
    "ramp_kW_per_s": 20.0, "ramp_tol_kW_per_s": 1e-6,
    "soc_min": 0.1, "soc_max": 0.95,
    "v_min_V": 360.0, "v_max_V": 657.0, "i_abs_max_A": 1823.0,
    "abs_t_end_tol_s": 1.0,
    "terminal_band": (0.25, 0.90),
    "P_reserve_dis_kW": 543.8, "P_reserve_chg_kW": 819.8,
    "E_reserve_dis_kWh": 6.839, "E_nom_kWh": 1071.36,
    "tol_hard_kWh": 1e-6, "tol_kW": 1e-6,
    "epsilon_numeric_kW": 0.11,
    "tau_s": 5.0,
}

FORBIDDEN_VIEW_FIELDS = (
    "fuel_raw", "fuel_total", "fuel_total_kg", "J_FEC", "J_FEC_kg",
    "B_perfect", "B_predicted", "L_prediction", "R", "bootstrap",
    "k_eq", "fuel_saving", "arm_savings",
)

# trajectory channels the FC-3 layer may read (physically fuel-free class)
TRAJ_ALLOWLIST = (
    "time", "P_unserved_kW", "P_prop_demand_kW", "P_prop_actual_kW",
    "P_DG1_kW", "P_DG2_kW", "state_DG1", "state_DG2",
    "commitment_state", "N_dispatch", "P_bat_actual_kW",
    "P_bat_dis_avail_kW", "P_bat_chg_avail_kW", "SOC", "V_bat_V",
    "I_bat_A", "V_dc_V", "derate08_active", "alarm_steady",
    "alarm_transient", "flag_vdc_low", "flag_soc_low",
    "safety_baseline_fallback_active", "P_gen_actual_kW",
    # NOTE: P_bus_residual_kW is a FC-5A derivation INPUT consumed by the
    # writer (stored output = derived P_bat_bus_kW); it is intentionally NOT
    # part of the frozen trajectory channel set and NOT an FC-3 input
    # (energy_closure_c5 is INHERITED_V2_RECORDED).
)

# record fields allowed into the sanitized view (identity/provenance only —
# no arm, no fuel, no performance)
RECORD_ALLOWLIST = (
    "schema", "run_id", "voyage_id", "formal_epoch", "freeze_root_sha256",
    "T_eval", "trajectory_sha256", "audit_sha256",
    "terminal_status", "classification_authority", "evidence_contract",
)

VIEW_SCHEMA = {
    "schema": "fc3_replay_input_view_v1",
    "identity": ["run_id", "voyage_id", "formal_epoch",
                  "original_epoch2_freeze_root", "source_run_sha",
                  "source_trajectory_sha", "source_audit_sha"],
    "payload": ["T_eval_s", "E_unserved_recorded_kWh",
                 "recorded_inherited_items"],
    "guards": ["view generated exclusively via FC3_REPLAY_INPUT_VIEW "
               "allow-list semantics; forbidden fields physically absent"],
}


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_canonical(obj) -> str:
    """same canonical-json algorithm as t445.hashutil (sorted, :, separators)"""
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


class ForbiddenAccess(RuntimeError):
    pass


def load_traj_guarded(vid: str, run_id: str, traj_sha: str) -> dict:
    """open the frozen trajectory and expose ONLY allow-listed channels."""
    import h5py
    p = os.path.join(EVID, vid, run_id + ".traj.v1.mat")
    if sha256_file(p) != traj_sha:
        raise ForbiddenAccess(f"trajectory hash mismatch: {p}")
    out = {}
    with h5py.File(p, "r") as f:
        tr = f["traj"]
        present = set(tr.keys())
        req = set(TRAJ_ALLOWLIST) - {"time"}
        missing = req - present
        if missing:
            raise ForbiddenAccess(f"channels missing: {sorted(missing)}")
        for k in TRAJ_ALLOWLIST:
            if k == "time" and "time" not in present:
                continue
            out[k] = np.array(tr[k]).flatten()
    return out


def load_audit_guarded(vid: str, run_id: str, audit_sha: str) -> dict:
    p = os.path.join(EVID, vid, run_id + ".audit.v1.json")
    if sha256_file(p) != audit_sha:
        raise ForbiddenAccess(f"audit hash mismatch: {p}")
    a = json.load(open(p))
    allow = ("schema", "voyage_id", "run_id", "n_samples", "t_first",
             "t_last", "battery_voltage", "battery_current",
             "battery_power_over_avail", "genset_ramp_on_on",
             "extremes", "any_violation")
    return {k: a[k] for k in allow if k in a}


# ---------------------------------------------------------------- envelope
def p_floor(dt: float) -> float:
    """frozen source-derived dynamic minimum-power admissibility envelope
    (D1; derivation artifact fc3_pmin_envelope_derivation.json)."""
    r, tau, pmin = TH["ramp_kW_per_s"], TH["tau_s"], TH["p_min_kW"]
    t_r = pmin / r
    if dt <= 0:
        return 0.0
    if dt <= t_r:
        return r * (dt - tau * (1.0 - np.exp(-dt / tau)))
    p_r = r * (t_r - tau * (1.0 - np.exp(-t_r / tau)))
    return pmin + (p_r - pmin) * np.exp(-(dt - t_r) / tau)


P_FLOOR_VEC = np.vectorize(p_floor)

# TG3 frozen canonical discrete adjudication policy (single source of
# timing resolution; no 1 Hz / high-rate mixing per voyage):
DISCRETE_POLICY = (
    "per continuous ON(3) episode on the 1 Hz state trace: "
    "dt_lower = max(0, t_sample - t_first_observed_ON - 1.0); "
    "canonical resolution = 1 Hz for every voyage; "
    "ON entry claimed at 1 Hz only (native audit records violation "
    "events, not transition timestamps); "
    "epsilon_numeric = 0.11 kW with machine-readable derivation"
)


def on_episodes(state: np.ndarray):
    """continuous ON(3) episodes -> list of (i0, i1) half-open on the
    sample grid (TG3: every episode gets its own loading-time origin)."""
    eps, i, n = [], 0, len(state)
    while i < n:
        if state[i] == 3:
            j = i
            while j < n and state[j] == 3:
                j += 1
            eps.append((i, j))
            i = j
        else:
            i += 1
    return eps


# ---------------------------------------------------------------- checker v3
def fc3_check(view: dict, traj: dict, audit: dict) -> dict:
    """FC-3 adjudication core. Receives NO arm identity (TG2).

    Returns the predicate vector with provenance and the distinct
    readjudicated status (TG5). Never touches fuel/J_FEC (structural).
    """
    t = traj["time"]
    p1, p2 = traj["P_DG1_kW"], traj["P_DG2_kW"]
    s1, s2 = traj["state_DG1"], traj["state_DG2"]
    soc = traj["SOC"]
    pb = traj["P_bat_actual_kW"]
    pav = np.where(pb < 0, traj["P_bat_chg_avail_kW"],
                   traj["P_bat_dis_avail_kW"])

    items = {}
    prov = {}

    # -- task_complete (RECOMPUTED_1HZ; frozen tol 1.0 s)
    items["task_complete"] = bool(
        abs(float(t[-1]) - float(view["payload"]["T_eval_s"]))
        <= TH["abs_t_end_tol_s"])
    prov["task_complete"] = "RECOMPUTED_1HZ"

    # -- service (D2): hard iff recorded native E_unserved > tol_hard;
    #    derate08 is a cause/context diagnostic only
    e_uns = float(view["payload"]["E_unserved_recorded_kWh"])
    e_uns_1hz = float(np.sum(traj["P_unserved_kW"]) / 3600.0)
    items["service_curtailment"] = bool(e_uns > TH["tol_hard_kWh"])
    prov["service_curtailment"] = "RECOMPUTED_1HZ(recorded-native-integral)"
    diagnostics = {
        "E_unserved_kWh": e_uns,
        "E_unserved_1hz_crosscheck_kWh": e_uns_1hz,
        "derate08_active_s_1hz": float(
            np.sum(traj["derate08_active"] > 0.5)),
        "tracking_gap_kWh_1hz": float(np.sum(
            np.maximum(traj["P_prop_demand_kW"]
                       - traj["P_prop_actual_kW"], 0.0)) / 3600.0),
    }

    # -- genset (D1 dynamic PMIN envelope + unchanged pmax/non-on;
    #    ramp from native audit)
    eps_kW = TH["epsilon_numeric_kW"]
    pmin_viol_samples = 0
    pmin_worst = 0.0
    pmin_detail = []
    for pi, si in ((p1, s1), (p2, s2)):
        for i0, i1 in on_episodes(si):
            # TG3 frozen discrete semantics (conservative lower elapsed time)
            dt = np.maximum(t[i0:i1] - t[i0] - 1.0, 0.0)
            floor = P_FLOOR_VEC(dt)
            below = pi[i0:i1] < (floor - eps_kW)
            if below.any():
                pmin_viol_samples += int(below.sum())
                exc = float(np.min(pi[i0:i1][below]
                                   - floor[below] + eps_kW))
                pmin_worst = min(pmin_worst, exc) if pmin_worst else exc
                pmin_detail.append({"episode_t0": float(t[i0]),
                                     "n_violation_samples": int(below.sum())})
    on = (s1 == 3) | (s2 == 3)
    pmax_viol = bool(np.any((p1 > TH["p_max_kW"] + TH["tol_kW"])
                            | (p2 > TH["p_max_kW"] + TH["tol_kW"])))
    nonon_viol = bool(np.any((~(s1 == 3)) & (np.abs(p1) > TH["tol_kW"]))
                      | np.any((~(s2 == 3)) & (np.abs(p2) > TH["tol_kW"])))
    ramp_viol = bool(audit.get("genset_ramp_on_on", {})
                     .get("count", 0) > 0)
    items["genset_limits_fc3"] = bool(pmin_viol_samples == 0
                                       and not pmax_viol
                                       and not nonon_viol
                                       and not ramp_viol)
    prov["genset_limits_fc3"] = ("RECOMPUTED_1HZ(PMIN envelope+PMAX+non-ON)"
                                  "+RECOMPUTED_NATIVE_AUDIT(ramp)")
    genset_diag = {
        "pmin_envelope_violation_samples": pmin_viol_samples,
        "pmin_worst_excursion_kW": pmin_worst,
        "pmin_violation_episodes": pmin_detail[:8],
        "pmax_violation": pmax_viol, "non_on_power_violation": nonon_viol,
        "ramp_violation_native": ramp_viol,
        "loading_registered_transition_samples": int(np.sum(
            on & ((p1 < TH["p_min_kW"] - TH["tol_kW"])
                  | (p2 < TH["p_min_kW"] - TH["tol_kW"])))),
    }

    # -- SOC hard bounds (1 Hz)
    soc_out = np.any((soc < TH["soc_min"] - 1e-12)
                     | (soc > TH["soc_max"] + 1e-12))
    items["soc_hard_bounds"] = not bool(soc_out)
    prov["soc_hard_bounds"] = "RECOMPUTED_1HZ"

    # -- terminal band + reserve (amendment constants)
    soc_end = float(soc[-1])
    band = TH["terminal_band"]
    items["terminal_soc_band"] = bool(band[0] - 1e-12 <= soc_end
                                       <= band[1] + 1e-12)
    sop_ok = (float(traj["P_bat_dis_avail_kW"][-1])
              >= TH["P_reserve_dis_kW"] - TH["tol_kW"]
              and float(traj["P_bat_chg_avail_kW"][-1])
              >= TH["P_reserve_chg_kW"] - TH["tol_kW"]
              and (soc_end - band[0]) * TH["E_nom_kWh"]
              >= TH["E_reserve_dis_kWh"] - 1e-9)
    items["terminal_sop_reserve"] = bool(sop_ok)
    prov["terminal_soc_band"] = prov["terminal_sop_reserve"] = "RECOMPUTED_1HZ"

    # -- battery V/I from native audit; P-over-SOP at 1 Hz
    bv = audit.get("battery_voltage", {}).get("count", 0)
    bi = audit.get("battery_current", {}).get("count", 0)
    p_over = bool(np.any(np.abs(pb) > pav + 1e-9))
    items["battery_sop_v_i"] = not (bool(bv) or bool(bi) or p_over)
    prov["battery_sop_v_i"] = ("RECOMPUTED_NATIVE_AUDIT(V,I)"
                                "+RECOMPUTED_1HZ(P-over-SOP)")

    # -- inherited (inputs not reconstructible at replay resolution)
    inh = view["payload"]["recorded_inherited_items"]
    items["energy_closure_c5"] = bool(inh["energy_closure_c5"])
    items["safety_hard_events"] = bool(inh["safety_hard_events"])
    prov["energy_closure_c5"] = prov["safety_hard_events"] = \
        "INHERITED_V2_RECORDED"

    ok = all(items.values())
    status = ("READJUDICATED_FEASIBLE" if ok
              else "READJUDICATED_HARD_INFEASIBLE")
    return {
        "items": items, "provenance": prov,
        "readjudicated_status": status,
        "diagnostics": diagnostics, "genset_diagnostics": genset_diag,
    }


# ------------------------------------------------------- v2 reference port
def v2_reference(traj: dict, t_eval: float, gap_energy_kwh: float,
                 e_unserved_kwh: float, derate_s: float) -> dict:
    """Verbatim port of ppe.feasibility_contract_v2 (non-target identical;
    used ONLY for synthetic differential regression per TG4)."""
    t = traj["time"]
    p1, p2 = traj["P_DG1_kW"], traj["P_DG2_kW"]
    s1, s2 = traj["state_DG1"], traj["state_DG2"]
    on1, on2 = s1 == 3, s2 == 3
    TOL = 1e-6
    dt = np.ones_like(t)  # 1 Hz fixture convention

    # FC-1 genset (state-aware, whole-ON Pmin — v2 semantics)
    pmin_v = np.any(on1 & (p1 < TH["p_min_kW"] - TOL)) or \
        np.any(on2 & (p2 < TH["p_min_kW"] - TOL))
    pmax_v = np.any(on1 & (p1 > TH["p_max_kW"] + TOL)) or \
        np.any(on2 & (p2 > TH["p_max_kW"] + TOL))
    rate1 = np.abs(np.diff(p1)) / np.maximum(np.diff(t), 1e-12)
    rate2 = np.abs(np.diff(p2)) / np.maximum(np.diff(t), 1e-12)
    iv = on1[:-1] & on1[1:] & on2[:-1] & on2[1:]
    ramp_v = bool(np.any(iv & ((rate1 > TH["ramp_kW_per_s"]
                                + TH["ramp_tol_kW_per_s"])
                               | (rate2 > TH["ramp_kW_per_s"]
                                  + TH["ramp_tol_kW_per_s"]))))
    nonon_v = bool(np.any((~on1) & (np.abs(p1) > TOL))
                   or np.any((~on2) & (np.abs(p2) > TOL)))
    genset_ok = not (pmin_v or pmax_v or ramp_v or nonon_v)

    # FC-2 load shortfall (v2: gap included hard — the semantics FC-3 fixes)
    load_ok = (e_unserved_kwh <= TOL) and (derate_s <= 0) \
        and (gap_energy_kwh <= TOL)

    soc = traj["SOC"]
    soc_out = np.any((soc < TH["soc_min"] - 1e-12)
                     | (soc > TH["soc_max"] + 1e-12))
    soc_end = float(soc[-1])
    band = TH["terminal_band"]
    term_ok = (band[0] - 1e-12 <= soc_end <= band[1] + 1e-12) and \
        float(traj["P_bat_dis_avail_kW"][-1]) >= TH["P_reserve_dis_kW"] - TOL \
        and float(traj["P_bat_chg_avail_kW"][-1]) >= TH["P_reserve_chg_kW"] - TOL \
        and (soc_end - band[0]) * TH["E_nom_kWh"] >= TH["E_reserve_dis_kWh"] - 1e-9

    pb = traj["P_bat_actual_kW"]
    pav = np.where(pb < 0, traj["P_bat_chg_avail_kW"],
                   traj["P_bat_dis_avail_kW"])
    p_over = float(np.sum(dt[np.abs(pb) > pav + 1e-9]))
    v_out = float(np.sum(dt[(traj["V_bat_V"] < TH["v_min_V"] - 1e-12)
                            | (traj["V_bat_V"] > TH["v_max_V"] + 1e-12)]))
    i_out = float(np.sum(dt[np.abs(traj["I_bat_A"])
                            > TH["i_abs_max_A"] + 1e-12]))
    bat_ok = (p_over <= 0) and (v_out <= 0) and (i_out <= 0)
    return {
        "task_complete": bool(abs(float(t[-1]) - t_eval)
                              <= TH["abs_t_end_tol_s"]),
        "load_shortfall": bool(load_ok),
        "soc_hard_bounds": not bool(soc_out),
        "terminal_soc_band": bool(band[0] - 1e-12 <= soc_end
                                   <= band[1] + 1e-12),
        "terminal_sop_reserve": bool(term_ok),
        "battery_sop_v_i": bool(bat_ok),
        "genset_limits_state_aware": bool(genset_ok),
        "energy_closure_c5": True,   # caller-injected for fixtures
        "safety_hard_events": True,  # caller-injected for fixtures
    }

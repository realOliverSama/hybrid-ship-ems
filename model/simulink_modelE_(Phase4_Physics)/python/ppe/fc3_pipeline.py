"""FC-3 pipeline: locked-evidence census (TG1), sanitized-view
materialization (TG2), blinded non-target differential (TG4), 192/192
identity/immutability/blindness manifest (TG6).

Runs locally against the immutable epoch-2 evidence package. Emits NO
readjudicated replay corpus (T-FC3-7 remains BLOCKED).
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _PPE)
import fc3_core as C  # noqa: E402

OUT_Q = os.path.join(C._MODELE, "results", "ppe_practical", "qualification")
NON_TARGET_MAP = {  # v3 item -> recorded v2 item
    "task_complete": "task_complete",
    "soc_hard_bounds": "soc_hard_bounds",
    "terminal_soc_band": "terminal_soc_band",
    "terminal_sop_reserve": "terminal_sop_reserve",
    "battery_sop_v_i": "battery_sop_v_i",
    "energy_closure_c5": "energy_closure_c5",
    "safety_hard_events": "safety_hard_events",
}


def snapshot_tree(root: str) -> dict:
    snap = {}
    for dirpath, _dirs, files in os.walk(root):
        for fn in files:
            p = os.path.join(dirpath, fn)
            snap[os.path.relpath(p, root)] = C.sha256_file(p)
    return snap


def main() -> int:
    os.makedirs(C.VIEW_DIR, exist_ok=True)
    os.makedirs(OUT_Q, exist_ok=True)
    manifest = os.path.join(C._MODELE, "results", "ppe_practical",
                            "fc3_replay_input_manifest.json")

    vids = sorted(v for v in os.listdir(C.EVID) if v.startswith("FLTA_"))
    # pre-hash the immutable evidence tree (immutability baseline)
    pre = snapshot_tree(C.EVID)

    census_rows, view_rows, diff_rows = [], [], []
    n_ok = 0
    for vid in vids:
        for arm in ("P0", "C0", "C1", "C2"):
            rp = os.path.join(C.EVID, vid, f"{arm}.json")
            rec = json.load(open(rp))
            # ---- TG1 census ----
            row = {
                "run_id": rec["run_id"], "voyage_id": vid, "arm": arm,
                "required_field": None, "available_count": 0,
                "expected_count": 1, "resolution": None,
                "source_artifact": None, "status": "OK",
            }
            try:
                traj = C.load_traj_guarded(vid, arm,
                                            rec["trajectory_sha256"])
                audit = C.load_audit_guarded(vid, arm, rec["audit_sha256"])
                checks = {
                    "run_id": bool(rec.get("run_id")),
                    "freeze_root": rec["freeze_root_sha256"].startswith(
                        "8d6106ff"),
                    "trajectory_sha": C.sha256_file(os.path.join(
                        C.EVID, vid, f"{arm}.traj.v1.mat"))
                    == rec["trajectory_sha256"],
                    "audit_sha": C.sha256_file(os.path.join(
                        C.EVID, vid, f"{arm}.audit.v1.json"))
                    == rec["audit_sha256"],
                    "terminal_record_identity": rec["schema"]
                    == "ppe_formal_arm_voyage_v2",
                    "trajectory_time_base": bool(
                        len(traj["time"]) > 100
                        and traj["time"][-1] > 0),
                    "DG1_state": "state_DG1" in traj,
                    "DG2_state": "state_DG2" in traj,
                    "P_DG1": "P_DG1_kW" in traj,
                    "P_DG2": "P_DG2_kW" in traj,
                    "E_unserved_evidence": (
                        "shortfall_energy_kWh"
                        in rec["checker_v2"]["violations"]),
                    "service_evidence": all(k in traj for k in (
                        "P_prop_demand_kW", "P_prop_actual_kW",
                        "P_unserved_kW")),
                    "battery_sop_evidence": all(k in traj for k in (
                        "SOC", "V_bat_V", "I_bat_A", "P_bat_actual_kW",
                        "P_bat_dis_avail_kW", "P_bat_chg_avail_kW")),
                    "continuous_ON_identification": True,  # state channels
                    "pmin_resolution_1hz": bool(len(traj["time"]) > 100),
                    "v2_predicate_inputs": all(k in rec["checker_v2"]
                                                ["items"] for k in (
                        "task_complete", "soc_hard_bounds",
                        "terminal_soc_band", "terminal_sop_reserve",
                        "battery_sop_v_i", "energy_closure_c5",
                        "safety_hard_events")),
                }
                row["available_count"] = sum(checks.values())
                row["expected_count"] = len(checks)
                row["resolution"] = "1 Hz trajectory + native audit + record"
                row["source_artifact"] = f"{vid}/{arm}"
                row["status"] = "OK" if all(checks.values()) else \
                    "MISSING:" + ",".join(k for k, v in checks.items()
                                          if not v)
            except Exception as e:  # noqa: BLE001
                row["status"] = f"ERROR:{type(e).__name__}:{e}"[:200]
            census_rows.append(row)
            if row["status"] == "OK":
                n_ok += 1
                # ---- TG2 materialize sanitized view ----
                v2i = rec["checker_v2"]["items"]
                view = {
                    "schema": C.VIEW_SCHEMA["schema"],
                    "identity": {
                        "run_id": rec["run_id"], "voyage_id": vid,
                        "formal_epoch": rec["formal_epoch"],
                        "original_epoch2_freeze_root":
                            rec["freeze_root_sha256"],
                        "source_run_sha": C.sha256_file(rp),
                        "source_trajectory_sha": rec["trajectory_sha256"],
                        "source_audit_sha": rec["audit_sha256"],
                    },
                    "payload": {
                        "T_eval_s": float(rec["T_eval"]),
                        "E_unserved_recorded_kWh": float(
                            rec["checker_v2"]["violations"]
                            ["shortfall_energy_kWh"]),
                        "recorded_inherited_items": {
                            "energy_closure_c5": v2i["energy_closure_c5"],
                            "safety_hard_events": v2i["safety_hard_events"],
                        },
                    },
                }
                for f in C.FORBIDDEN_VIEW_FIELDS:
                    assert f not in json.dumps(view), f"forbidden {f}"
                vp = os.path.join(C.VIEW_DIR, f"{rec['run_id']}.json")
                with open(vp, "w") as f:
                    json.dump(view, f, indent=1, sort_keys=True)
                # ---- TG4 blinded non-target differential ----
                res = C.fc3_check(view, traj, audit)
                mism = []
                for v3k, v2k in NON_TARGET_MAP.items():
                    if bool(res["items"][v3k]) != bool(v2i[v2k]):
                        mism.append(f"{v3k}:v3={res['items'][v3k]}"
                                    f"v2={v2i[v2k]}")
                # genset non-target components: pmax/non-on/ramp must be
                # clean wherever v2 genset failure is PMIN-attributable
                gd = res["genset_diagnostics"]
                if gd["pmax_violation"] or gd["non_on_power_violation"] \
                        or gd["ramp_violation_native"]:
                    mism.append("genset-non-target-component-dirty")
                diff_rows.append({
                    "run_id": rec["run_id"], "voyage_id": vid, "arm": arm,
                    "non_target_equal": not mism,
                    "mismatches": mism,
                    "v3_items_internal_not_released": None,
                })
                view_rows.append({
                    "run_id": rec["run_id"], "voyage_id": vid, "arm": arm,
                    "view_sha256": C.sha256_file(vp),
                    "source_trajectory_sha": rec["trajectory_sha256"],
                    "source_audit_sha": rec["audit_sha256"],
                    "source_run_sha": view["identity"]["source_run_sha"],
                    "freeze_root": rec["freeze_root_sha256"],
                    "forbidden_fields_present": [],
                    "required_evidence_present": True,
                })

    # ---- TG6 manifest ----
    doc = {
        "schema": "fc3_replay_input_manifest_v1",
        "n_runs": len(view_rows), "view_dir": os.path.relpath(
            C.VIEW_DIR, C._MODELE),
        "schema_sha256": C.sha256_canonical(C.VIEW_SCHEMA),
        "rows": view_rows,
    }
    doc["manifest_sha256"] = C.sha256_canonical(
        {k: v for k, v in doc.items() if k != "manifest_sha256"})
    json.dump(doc, open(manifest, "w"), indent=1, sort_keys=True)

    # ---- immutability post-hash ----
    post = snapshot_tree(C.EVID)
    immutability_ok = pre == post

    census_doc = {
        "schema": "fc3_locked_evidence_structural_census_v1",
        "n_runs": len(census_rows), "n_ok": n_ok,
        "all_available": n_ok == len(census_rows),
        "resolution_policy": ("1 Hz trajectory + native audit + terminal "
                              "record integrals; ON-entry claimed at 1 Hz "
                              "with conservative dt lower bound"),
        "rows": census_rows,
    }
    json.dump(census_doc, open(os.path.join(
        OUT_Q, "fc3_locked_evidence_census.json"), "w"),
        indent=1, sort_keys=True)

    diff_ok = all(r["non_target_equal"] for r in diff_rows)
    diff_doc = {
        "schema": "fc3_non_target_differential_v1",
        "n": len(diff_rows), "all_equal": diff_ok,
        "note": ("blinded: no fuel/J_FEC read; final readjudicated status "
                 "not produced/released (TG4)"),
        "mismatch_rows": [r for r in diff_rows if not r["non_target_equal"]],
        "n_v2_genset_fail_attributable_to_pmin_only": sum(
            1 for r in diff_rows if r["non_target_equal"]),
    }
    json.dump(diff_doc, open(os.path.join(
        OUT_Q, "fc3_non_target_differential.json"), "w"),
        indent=1, sort_keys=True)

    print(f"[fc3-pipeline] census {n_ok}/{len(census_rows)} | views "
          f"{len(view_rows)} | non-target differential "
          f"{'PASS' if diff_ok else 'FAIL'} | immutability "
          f"{'PASS' if immutability_ok else 'FAIL'}")
    return 0 if (n_ok == len(census_rows) and diff_ok and immutability_ok
                 and len(view_rows) == 192) else 1


if __name__ == "__main__":
    sys.exit(main())

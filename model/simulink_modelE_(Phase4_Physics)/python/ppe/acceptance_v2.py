"""T-A9 epoch-2 acceptance v2 (recovery authorization §6).

Acceptance for the epoch-2 formal execution. MUST NOT produce a false PASS
from epoch-1 outputs:

- the formal directory MUST be the epoch-2 namespace (formal_epoch2);
- every terminal record MUST carry formal_epoch=2 and the CURRENT epoch-2
  freeze byte-root;
- every terminal record MUST carry checker-v2 identity, a canonical
  trajectory hash and a high-rate audit hash, each resolving to a real,
  hash-valid artifact;
- the expected identity set is exactly the 192 (voyage x arm) pairs of the
  frozen formal execution manifest; duplicates and extras are problems;
- warm-up parity artifacts (48/48, all PASS) are required;
- epoch-1 evidence (results/ppe_practical/formal) is ignored entirely: a
  mere file count in a shared directory is NOT a completion criterion.

Stdlib only. Exit 0 iff pass=true.

Run: python -m ppe.acceptance_v2 [--modele-root DIR] [--formal-dir DIR]
                                 [--freeze PATH] [--manifest PATH] [--out PATH]
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_DEFAULT_ROOT = os.path.dirname(_PY)


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--modele-root", default=_DEFAULT_ROOT)
    ap.add_argument("--formal-dir", default="")
    ap.add_argument("--freeze", default="")
    ap.add_argument("--manifest", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--allow-nonproduction", action="store_true",
                    help="sandbox mode: do not require the formal-192/48 "
                         "manifest shape (production PE3-1 never passes this)")
    args = ap.parse_args(argv)

    root = os.path.abspath(args.modele_root)
    rpp = os.path.join(root, "results", "ppe_practical")
    formal_dir = os.path.abspath(
        args.formal_dir or os.path.join(rpp, "formal_epoch2"))
    freeze_path = os.path.abspath(
        args.freeze or os.path.join(rpp, "freeze",
                                    "epoch2_pre_execution_freeze_v2.json"))
    manifest_path = os.path.abspath(
        args.manifest or os.path.join(rpp, "freeze",
                                      "formal_execution_manifest.json"))
    out_path = os.path.abspath(
        args.out or os.path.join(formal_dir, "T010_ACCEPTANCE_V2.json"))

    problems: list[str] = []
    checks: dict[str, object] = {}

    # --- namespace guard: epoch-1 evidence must be ignored entirely -------
    ep1 = os.path.join(rpp, "formal")
    if os.path.realpath(formal_dir) == os.path.realpath(ep1):
        problems.append(
            "namespace:formal-dir-is-epoch1 (epoch-1 evidence is not an "
            "epoch-2 completion criterion)")
    checks["formal_dir"] = formal_dir

    # --- freeze + gate -----------------------------------------------------
    freeze_bytes_sha = sha256_file(freeze_path)
    with open(freeze_path, encoding="utf-8") as f:
        fz = json.load(f)
    if fz.get("schema") != "ppe_epoch2_pre_execution_freeze_v3":
        problems.append(f"freeze:schema {fz.get('schema')}")
    if fz.get("formal_epoch") != 2:
        problems.append("freeze:formal_epoch!=2")
    if fz.get("epoch2_result_bearing_runs", 0) < 1:
        problems.append("freeze:epoch2_result_bearing_runs==0 (nothing to accept)")
    gate_path = os.path.join(os.path.dirname(freeze_path),
                             "EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json")
    try:
        with open(gate_path, encoding="utf-8") as f:
            gate = json.load(f)
        if not gate.get("overall"):
            problems.append("gate:overall=false")
        if gate.get("freeze_file_sha256") != freeze_bytes_sha:
            problems.append("gate:freeze-root-drift (gate != freeze bytes)")
    except FileNotFoundError:
        problems.append("gate:missing (EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2)")
    checks["freeze_root_sha256"] = freeze_bytes_sha
    bd = fz["bindings"]

    # --- expected identities from the frozen manifest ----------------------
    manifest_file_sha = sha256_file(manifest_path)
    if manifest_file_sha != bd.get("formal_execution_manifest_file_sha256"):
        problems.append("manifest:file-sha!=freeze-binding")
    with open(manifest_path, encoding="utf-8") as f:
        man = json.load(f)
    if man.get("teval_table_sha256") != bd.get("teval_table_sha256"):
        problems.append("manifest:teval!=freeze-binding")
    expected = {}
    want_root = freeze_bytes_sha
    seen_run_ids: set[str] = set()
    for run in man["runs"]:
        key = (run["voyage_id"], run["arm"])
        if key in expected:
            problems.append(f"manifest:duplicate-identity {key}")
        expected[key] = run
        if run["run_id"] in seen_run_ids:
            problems.append(f"manifest:duplicate-run-id {run['run_id']}")
        seen_run_ids.add(run["run_id"])
    if len(expected) != man.get("n_runs", len(expected)):
        problems.append(
            f"manifest:n_runs-mismatch ({len(expected)} vs {man.get('n_runs')})")
    production = manifest_file_sha == bd.get(
        "formal_execution_manifest_file_sha256")
    checks["production_manifest"] = production
    if not args.allow_nonproduction and (
            len(expected) != 192 or man.get("n_voyages") != 48):
        problems.append(
            "manifest:not-the-formal-192/48 "
            f"(n={len(expected)}, voyages={man.get('n_voyages')})")
    checks["n_expected_identities"] = len(expected)

    # --- per-identity terminal verification --------------------------------
    status_hist: dict[str, int] = {}
    n_found = 0
    for (vid, arm), run in sorted(expected.items()):
        vdir = os.path.join(formal_dir, vid)
        rec_path = os.path.join(vdir, f"{arm}.json")
        tag = f"{vid}/{arm}"
        if not os.path.isfile(rec_path):
            problems.append(f"missing:terminal {tag}")
            continue
        try:
            with open(rec_path, encoding="utf-8") as f:
                rec = json.load(f)
        except Exception as e:  # noqa: BLE001
            problems.append(f"unparsable:terminal {tag} ({e})")
            continue
        n_found += 1
        if rec.get("schema") != "ppe_formal_arm_voyage_v2":
            problems.append(f"record:schema {tag} {rec.get('schema')}")
        if rec.get("formal_epoch") != 2:
            problems.append(f"record:formal_epoch {tag}")
        if rec.get("freeze_root_sha256") != want_root:
            problems.append(f"record:freeze-root {tag}")
        if rec.get("run_id") != run["run_id"]:
            problems.append(f"record:run-id {tag}")
        if rec.get("voyage_id") != vid or rec.get("arm") != arm:
            problems.append(f"record:identity {tag}")
        if rec.get("teval_identity_sha256") != bd.get("teval_table_sha256"):
            problems.append(f"record:teval {tag}")
        if rec.get("cstar_impl_sha256") != bd.get("cstar_composite_sha256"):
            problems.append(f"record:cstar {tag}")
        if rec.get("slx_sha256") != bd.get("modele_slx_sha256"):
            problems.append(f"record:slx {tag}")
        cv = rec.get("checker_v2") or {}
        if cv.get("sha256") != bd.get("checker_v2_sha256"):
            problems.append(f"record:checker-v2-identity {tag}")
        if "items" not in cv or "pass" not in cv:
            problems.append(f"record:checker-v2-vector {tag}")
        st = rec.get("terminal_status", "MISSING")
        if not str(st).startswith("COMPLETE"):
            problems.append(f"record:terminal-status {tag} {st}")
        status_hist[st] = status_hist.get(st, 0) + 1
        # trajectory artifact: field -> file -> bytes
        tsha = str(rec.get("trajectory_sha256", ""))
        if not tsha:
            problems.append(f"traj:field-missing {tag}")
        else:
            tpath = os.path.join(vdir, f"{arm}.traj.v1.mat")
            if not os.path.isfile(tpath):
                problems.append(f"traj:artifact-missing {tag}")
            elif sha256_file(tpath) != tsha:
                problems.append(f"traj:hash-mismatch {tag}")
            side = os.path.join(vdir, f"{arm}.traj.v1.sidecar.json")
            if not os.path.isfile(side):
                problems.append(f"traj:sidecar-missing {tag}")
            else:
                try:
                    with open(side, encoding="utf-8") as f:
                        s = json.load(f)
                    if s.get("sha256") != tsha or s.get("run_id") != rec.get(
                            "run_id"):
                        problems.append(f"traj:sidecar-mismatch {tag}")
                except Exception as e:  # noqa: BLE001
                    problems.append(f"traj:sidecar-unparsable {tag} ({e})")
        # audit artifact
        asha = str(rec.get("audit_sha256", ""))
        if not asha:
            problems.append(f"audit:field-missing {tag}")
        else:
            apath = os.path.join(vdir, f"{arm}.audit.v1.json")
            if not os.path.isfile(apath):
                problems.append(f"audit:artifact-missing {tag}")
            elif sha256_file(apath) != asha:
                problems.append(f"audit:hash-mismatch {tag}")
        # controller trace artifact (FC-5A: required for every arm; P0
        # carries an applies=false record)
        csha = str(rec.get("controller_trace_sha256", ""))
        if not csha:
            problems.append(f"ctrace:field-missing {tag}")
        else:
            cpath = os.path.join(vdir, f"{arm}.ctrace.v1.json")
            if not os.path.isfile(cpath):
                problems.append(f"ctrace:artifact-missing {tag}")
            elif sha256_file(cpath) != csha:
                problems.append(f"ctrace:hash-mismatch {tag}")
            else:
                try:
                    with open(cpath, encoding="utf-8") as f:
                        cd = json.load(f)
                    if cd.get("schema") != "ppe_controller_trace_v1":
                        problems.append(f"ctrace:schema {tag}")
                    if arm == "P0" and cd.get("applies") is not False:
                        problems.append(f"ctrace:p0-must-not-apply {tag}")
                    if arm != "P0":
                        if cd.get("applies") is not True:
                            problems.append(f"ctrace:cstar-must-apply {tag}")
                        elif not cd.get("forecast_source_id") or not cd.get(
                                "forecast_window_sha256"):
                            problems.append(f"ctrace:provenance-missing {tag}")
                except Exception as e:  # noqa: BLE001
                    problems.append(f"ctrace:unparsable {tag} ({e})")
        # trajectory/audit artifacts must NOT point into epoch-1 namespace
        for k in ("trajectory_artifact", "audit_artifact"):
            v = str(rec.get(k, ""))
            if ep1 in v:
                problems.append(f"record:{k}-in-epoch1-namespace {tag}")

    # extras: terminal records in the epoch-2 namespace that the manifest
    # does not declare (misrouted or stale output). Only files shaped like a
    # terminal record (<declared-arm>.json) count; trajectory sidecars and
    # audit artifacts are legitimate residents of a voyage directory.
    manifest_arms = {a for _, a in expected}
    if os.path.isdir(formal_dir):
        for vid in sorted(os.listdir(formal_dir)):
            vd = os.path.join(formal_dir, vid)
            if not os.path.isdir(vd) or vid.startswith("_"):
                continue
            for fn in sorted(os.listdir(vd)):
                if fn.endswith(".json") and not fn.startswith("warmup"):
                    arm = fn[:-5]
                    if arm in manifest_arms and (vid, arm) not in expected:
                        problems.append(f"extra:terminal {vid}/{arm}")

    # --- warm-up parity 48/48 ----------------------------------------------
    n_parity = 0
    for vid in sorted({v for v, _ in expected}):
        wp = os.path.join(formal_dir, vid, "warmup_parity.json")
        if not os.path.isfile(wp):
            problems.append(f"parity:missing {vid}")
            continue
        try:
            with open(wp, encoding="utf-8") as f:
                v = json.load(f)
        except Exception as e:  # noqa: BLE001
            problems.append(f"parity:unparsable {vid} ({e})")
            continue
        if v == "PASS":
            n_parity += 1
        else:
            problems.append(f"parity:not-pass {vid} ({v})")
    if n_parity != len({v for v, _ in expected}):
        problems.append(
            f"parity:count!={len({v for v, _ in expected})} ({n_parity})")
    checks["n_warmup_parity_pass"] = n_parity
    checks["n_found_terminals"] = n_found
    checks["terminal_status_histogram"] = status_hist
    checks["n_problems"] = len(problems)

    doc = {
        "schema": "ppe_epoch2_acceptance_v2",
        "study": "phase4f-practical-predictive-ems",
        "formal_epoch": 2,
        "freeze_root_sha256": freeze_bytes_sha,
        "n_expected_identities": len(expected),
        "n_found_terminals": n_found,
        "terminal_status_histogram": status_hist,
        "n_warmup_parity_pass": n_parity,
        "checks": checks,
        "problems": problems,
        "pass": not problems,
        "utc": datetime.datetime.now(datetime.timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"),
    }
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1, sort_keys=True)
    print(f"[acceptance-v2] pass={doc['pass']} problems={len(problems)} "
          f"terminals={n_found}/192 parity={n_parity}/48 -> {out_path}")
    if problems:
        for p in problems[:20]:
            print(f"  PROBLEM {p}")
        if len(problems) > 20:
            print(f"  ... and {len(problems) - 20} more")
    return 0 if doc["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

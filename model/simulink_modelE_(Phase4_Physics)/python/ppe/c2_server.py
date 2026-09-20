"""C2 inference file-bridge server (formal runner transport, TG7-bound).

Protocol (deterministic, fail-closed):
- MATLAB writes `<reqdir>/req_<token>_<t0>.mat` (v7) with fields:
  voyage_id (char), t0 (double), H1 (300x11 double, registry channel order),
  and deletes nothing.
- Server (this process) loads keras ONCE, polls reqdir, for each request:
  reads the frozen voyage .mat kf_snapshots row issue_time_s==t0 itself
  (dataset input contract), runs the frozen model, writes
  `<respdir>/resp_<token>_<t0>.mat` with Pbar_q50 (30x1) and provenance.
- Each response records keras/norm SHAs; MATLAB side re-verifies identity.

Security/scope: only reads Dataset-v3 formal iid dir + npz; never test/OOD.
"""
from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from scipy.io import loadmat, savemat  # noqa: E402

from t445 import hashutil  # noqa: E402

from ppe import ledger, paths  # noqa: E402
from ppe.c2_inference import C2Predictor, FR836  # noqa: E402
from ppe.causality import REQUIRED_F, REQUIRED_H1, REQUIRED_K  # noqa: E402

KF_ORDER = REQUIRED_K + REQUIRED_F
H1_ORDER = REQUIRED_H1
_TOL = 1e-9


class BridgeError(RuntimeError):
    pass


def kf_row_for(voyage_id: str, t0: float) -> dict:
    """Frozen Dataset-v3 kf_snapshots row at issue_time_s == t0 (D7)."""
    m = loadmat(f"{paths.FORMAL_IID_DIR}/{voyage_id}.mat", simplify_cells=True)
    kf = m["kf"]
    issue = np.asarray(kf["issue_time_s"], dtype=float).ravel()
    hit = np.nonzero(np.abs(issue - float(t0)) <= _TOL)[0]
    if hit.size != 1:
        raise BridgeError(f"{voyage_id}@{t0}: kf vintage hits={hit.size}")
    row = int(hit[0])
    leads = np.asarray(kf["lead_s"], dtype=float)[row]
    order = np.argsort(leads, kind="stable")
    out = {}
    for c in KF_ORDER:
        out[c] = np.asarray(kf[c], dtype=float)[row][order]
    return out


def serve_once(req_path: str, resp_path: str, pred: C2Predictor) -> dict:
    req = loadmat(req_path, simplify_cells=True)
    vid = str(req["voyage_id"])
    t0 = float(req["t0"])
    t = np.asarray(req["t"], dtype=float).ravel()
    cols = {}
    for c in H1_ORDER:
        key = f"H1_{c}"
        if key not in req:
            raise BridgeError(f"request missing {key}")
        cols[c] = np.asarray(req[key], dtype=float).ravel()
    from ppe.h1_assembly import h1_from_raw_logs
    H1 = h1_from_raw_logs(t, cols, t0)
    hist = {c: H1[:, j] for j, c in enumerate(H1_ORDER)}
    kf = kf_row_for(vid, t0)
    q50 = pred.predict_q50(hist, kf)
    prov = {
        "keras_sha256": FR836["keras_sha256"],
        "formal_norm_v3_sha256": FR836["formal_norm_v3_sha256"],
        "predictor_norm_ext_v1_sha256": FR836["predictor_norm_ext_v1_sha256"],
        "voyage_id": vid, "t0": t0,
        "h1_sha256": hashutil.sha256_bytes(H1.tobytes()),
    }
    savemat(resp_path, {"Pbar_q50": q50.reshape(-1, 1), "provenance": prov})
    return {"voyage_id": vid, "t0": t0, "q50_head": float(q50[0])}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reqdir", required=True)
    ap.add_argument("--respdir", required=True)
    ap.add_argument("--stop-file", default="")
    ap.add_argument("--poll-s", type=float, default=0.2)
    ap.add_argument("--max-requests", type=int, default=0)
    args = ap.parse_args(argv)

    os.makedirs(args.respdir, exist_ok=True)
    pred = C2Predictor.instance()  # fail-closed identity at startup
    ledger.log_access(
        artifact="c2_server", purpose="QUALIFICATION",
        split_contact="none",
        note=f"C2 bridge server start reqdir={os.path.basename(args.reqdir)}")
    done = set()
    n = 0
    while True:
        if args.stop_file and os.path.exists(args.stop_file):
            return 0
        for fn in sorted(os.listdir(args.reqdir)):
            if not fn.startswith("req_") or not fn.endswith(".mat") or fn in done:
                continue
            req = os.path.join(args.reqdir, fn)
            resp = os.path.join(args.respdir, "resp_" + fn[4:])
            try:
                serve_once(req, resp, pred)
            except Exception as exc:  # noqa: BLE001 — fail-closed w/ record
                savemat(resp, {"error": f"{type(exc).__name__}: {exc}"})
            done.add(fn)
            n += 1
            if args.max_requests and n >= args.max_requests:
                return 0
        time.sleep(args.poll_s)


if __name__ == "__main__":
    raise SystemExit(main())

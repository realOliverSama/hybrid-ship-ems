"""T-002 — C1 canonical target index / 100% coverage (TG5) / parity.

Formal C1 = stored npz lookup ONLY (DGN-PPE-01b). Runtime reconstruction is
forbidden. targets.py reconstruction appears here QUALIFICATION-only, on a
train-split (non-corpus) native stream, to prove stored == recomputed parity.

Run: python -m ppe.c1_index   (requires numpy+scipy env, e.g. .venv_tf)
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from scipy.io import loadmat  # noqa: E402

from t445 import hashutil  # noqa: E402

from dataset_v3.targets import y_trajectory  # noqa: E402

from ppe import ledger, paths  # noqa: E402
from ppe.teval import WINDOWS_NPZ_SHA256  # noqa: E402

_TOL = 1e-9
PARITY_VOYAGE = "FLTA_S1_D00000_M1"  # first train-split voyage (non-corpus)


class C1TargetMissing(KeyError):
    """Fail-closed: required (voyage_id, t0) absent from canonical store."""


class C1CoverageStop(RuntimeError):
    """Fail-closed: canonical store coverage < 100% of required keys."""


def _load_teval() -> dict:
    with open(paths.TEVAL_TABLE_JSON, encoding="utf-8") as fh:
        return json.load(fh)


def required_keys(teval_row: dict) -> np.ndarray:
    """Legal C1-differentiated issue times = Dataset-v3 anchors (TG5)."""
    n = int(teval_row["n_differentiated_updates"])
    first = float(teval_row["first_anchor_s"])
    return first + 10.0 * np.arange(n, dtype=float)


class C1TargetIndex:
    """Read-only lookup into the frozen canonical target store."""

    def __init__(self, npz_path: str | None = None):
        self.npz_path = npz_path or paths.WINDOWS_NPZ
        sha = hashutil.sha256_file(self.npz_path)
        if sha != WINDOWS_NPZ_SHA256:
            raise C1CoverageStop(f"C1 store SHA mismatch: {sha}")
        z = np.load(self.npz_path, allow_pickle=False)
        self._vid = z["voyage_id"]
        self._t0 = z["anchor_t0_s"].astype(float)
        self._Y = z["Y"]
        self._splits = z["splits"]
        ledger.log_access(
            artifact=paths.rel(self.npz_path), purpose="FREEZE_PREP",
            split_contact="val-target-store-index-only",
            note="C1 canonical target index build; no controller outcome",
        )

    def get(self, voyage_id: str, t0: float) -> np.ndarray:
        """Stored y_k[1:30] for (voyage_id, t0); fail-closed on miss."""
        sel = np.nonzero(
            (self._vid == voyage_id)
            & (np.abs(self._t0 - float(t0)) <= _TOL))[0]
        if sel.size != 1:
            raise C1TargetMissing(
                f"(voyage={voyage_id}, t0={t0}) hits={sel.size} — FAIL CLOSED")
        y = np.asarray(self._Y[sel[0]], dtype=float)
        if not np.all(np.isfinite(y)):
            raise C1TargetMissing(f"({voyage_id}, {t0}) non-finite — FAIL CLOSED")
        return y

    def store_sha256(self) -> str:
        return WINDOWS_NPZ_SHA256

    def available_keys(self, voyage_id: str) -> np.ndarray:
        return np.sort(self._t0[self._vid == voyage_id].astype(float))


def build_coverage() -> dict:
    teval = _load_teval()
    idx = C1TargetIndex()
    per_voyage = {}
    n_req_total = 0
    n_hit_total = 0
    for row in teval["voyages"]:
        vid = row["voyage_id"]
        req = required_keys(row)
        got = idx.available_keys(vid)
        n_req_total += req.size
        if got.shape != req.shape or not np.allclose(got, req, rtol=0, atol=_TOL):
            missing = [float(t) for t in req
                       if not np.any(np.abs(got - t) <= _TOL)]
            raise C1CoverageStop(
                f"{vid}: coverage {req.size - len(missing)}/{req.size}; "
                f"missing {missing[:5]}")
        # spot full lookup on every required key (fail-closed proof)
        for t0 in req:
            idx.get(vid, float(t0))
        n_hit_total += int(req.size)
        per_voyage[vid] = {"n_required": int(req.size), "n_present": int(req.size)}
    coverage = {
        "schema": "ppe_c1_target_coverage_v1",
        "store": paths.rel(paths.WINDOWS_NPZ),
        "store_sha256": WINDOWS_NPZ_SHA256,
        "teval_table_sha256": teval["teval_table_sha256"],
        "n_voyages": len(per_voyage),
        "n_required_keys": n_req_total,
        "n_present_keys": n_hit_total,
        "coverage": n_hit_total / n_req_total if n_req_total else 0.0,
        "per_voyage": per_voyage,
        "verdict": "COVERAGE_100_PERCENT" if n_hit_total == n_req_total
                   else "COVERAGE_INCOMPLETE",
    }
    if coverage["verdict"] != "COVERAGE_100_PERCENT":
        raise C1CoverageStop("C1 canonical coverage < 100%")
    return coverage


def build_index_artifact(coverage: dict) -> dict:
    idx = C1TargetIndex()
    per_voyage = {}
    for vid in coverage["per_voyage"]:
        keys = idx.available_keys(vid)
        rows = []
        for t0 in keys:
            sel = np.nonzero(
                (idx._vid == vid) & (np.abs(idx._t0 - t0) <= _TOL))[0]
            rows.append(int(sel[0]))
        per_voyage[vid] = {
            "anchor_t0_s": [float(x) for x in keys],
            "npz_row_index": rows,
        }
    art = {
        "schema": "ppe_c1_target_index_v1",
        "store": paths.rel(paths.WINDOWS_NPZ),
        "store_sha256": WINDOWS_NPZ_SHA256,
        "teval_table_sha256": coverage["teval_table_sha256"],
        "per_voyage": per_voyage,
    }
    art["index_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in art.items() if k != "index_sha256"})
    return art


def parity_qualification() -> dict:
    """Stored vs targets.py reconstruction on the train-split fixture."""
    idx = C1TargetIndex()
    path = f"{paths.FORMAL_IID_DIR}/{PARITY_VOYAGE}.mat"
    m = loadmat(path, simplify_cells=True)
    st = m["stream"]
    t = np.asarray(st["time_s"], dtype=float).ravel()
    p = np.asarray(st["P_load_total_kW"], dtype=float).ravel()
    ledger.log_access(
        artifact=f"datasets_v3/formal/iid/{PARITY_VOYAGE}.mat",
        purpose="QUALIFICATION",
        split_contact="train-native-stream",
        note="C1 stored-vs-targets.py parity (non-corpus train voyage)",
    )
    worst = 0.0
    anchors = idx.available_keys(PARITY_VOYAGE)
    if anchors.size == 0:
        raise C1CoverageStop(f"{PARITY_VOYAGE}: no anchors for parity")
    n_exact = 0
    for t0 in anchors:
        y_stored = idx.get(PARITY_VOYAGE, float(t0))
        y_recomp = y_trajectory(t, p, float(t0))
        # storage-dtype-exact parity: npz stores float32; stored bytes must
        # equal float32(frozen float64 reconstruction) elementwise.
        if not np.all(y_stored == y_recomp.astype(np.float32)):
            raise C1CoverageStop(
                f"{PARITY_VOYAGE}@{t0}: stored != float32(recomputed)")
        n_exact += 1
        scale = max(float(np.max(np.abs(y_recomp))), 1.0)
        rel = float(np.max(np.abs(y_stored - y_recomp)) / scale)
        worst = max(worst, rel)
    return {
        "schema": "ppe_c1_parity_qualification_v1",
        "parity_voyage": PARITY_VOYAGE,
        "parity_split": "train (non-corpus)",
        "n_anchors_checked": int(anchors.size),
        "n_exact_float32": n_exact,
        "storage_dtype": "float32",
        "float64_space_max_rel_err": worst,
        "float64_space_note": "npz float32 storage quantization only; "
                              "float32 eps ~ 1.2e-7",
        "verdict": "PASS" if n_exact == anchors.size else "FAIL",
    }


def run() -> dict:
    os.makedirs(paths.FREEZE_DIR, exist_ok=True)
    os.makedirs(paths.QUAL_DIR, exist_ok=True)
    coverage = build_coverage()
    index = build_index_artifact(coverage)
    parity = parity_qualification()

    # fail-closed demo: missing key must raise
    idx = C1TargetIndex()
    try:
        idx.get("NO_SUCH_VOYAGE", 300.0)
        failclosed = False
    except C1TargetMissing:
        failclosed = True
    try:
        idx.get("FLTA_S1_D00000_M1", 123.0)  # off-grid t0
        failclosed_offgrid = False
    except C1TargetMissing:
        failclosed_offgrid = True

    hashutil.dump_json(f"{paths.FREEZE_DIR}/c1_target_index.json", index)
    hashutil.dump_json(f"{paths.FREEZE_DIR}/c1_coverage.json", coverage)
    hashutil.dump_json(f"{paths.QUAL_DIR}/c1_parity.json", parity)
    out = {
        "schema": "ppe_t002_result_v1",
        "task": "T-002",
        "overall": bool(
            coverage["verdict"] == "COVERAGE_100_PERCENT"
            and parity["verdict"] == "PASS"
            and failclosed and failclosed_offgrid),
        "coverage": coverage["coverage"],
        "n_required_keys": coverage["n_required_keys"],
        "parity_exact_float32": parity["n_exact_float32"],
        "parity_float64_space_max_rel_err": parity["float64_space_max_rel_err"],
        "failclosed_missing_voyage": failclosed,
        "failclosed_offgrid_t0": failclosed_offgrid,
        "index_sha256": index["index_sha256"],
    }
    hashutil.dump_json(f"{paths.FREEZE_DIR}/t002_result.json", out)
    return out


if __name__ == "__main__":
    out = run()
    print("T-002 overall:", out["overall"])
    print("coverage:", out["coverage"], "keys:", out["n_required_keys"])
    print("parity exact float32:", out["parity_exact_float32"],
          "float64-space rel:", out["parity_float64_space_max_rel_err"])
    print("fail-closed demos:", out["failclosed_missing_voyage"],
          out["failclosed_offgrid_t0"])

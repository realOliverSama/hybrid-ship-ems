"""C2 frozen REAT-T inference wrapper (FR-830–836, DGN-PPE-04).

- keras SHA verified at construction (fail-closed);
- frozen load protocol: load_model(compile=False, custom_objects=
  predictor_v3.hpo._custom_objects()) (Phase-4E packages.py reload protocol);
- inputs hist_input [B,300,11] / kf_input [B,30,6]; output [B,30,3];
- q50 = out[:, :, 1], kW domain (training target is kW-domain pinball;
  y_mean/y_std are metric denominators only, FR-823 "de-normalized" = the
  raw kW output).
"""
from __future__ import annotations

import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from t445 import hashutil  # noqa: E402

from ppe import ledger, paths  # noqa: E402
from ppe.teval import FR836  # noqa: E402


class C2IdentityError(RuntimeError):
    """Fail-closed: predictor artifact identity mismatch."""


class C2Predictor:
    _instance = None

    def __init__(self):
        for label, p, want in (
                ("keras", paths.KERAS_S0, FR836["keras_sha256"]),
                ("formal_norm_v3", paths.FORMAL_NORM_V3,
                 FR836["formal_norm_v3_sha256"]),
                ("predictor_norm_ext_v1", paths.PREDICTOR_NORM_EXT,
                 FR836["predictor_norm_ext_v1_sha256"]),
                ("selected_config", paths.SELECTED_CONFIG_JSON,
                 FR836["selected_config_sha256"]),
                ("eval_config", paths.EVAL_CONFIG, FR836["eval_config_sha256"]),
                ("reat_t_arch", paths.REAT_T_ARCH, FR836["reat_t_arch_sha256"])):
            if not os.path.exists(p):
                raise C2IdentityError(f"{label} missing: {paths.rel(p)}")
            got = hashutil.sha256_file(p)
            if got != want:
                raise C2IdentityError(f"{label} SHA mismatch: {got}")
        import json
        with open(paths.WINNER_JSON, encoding="utf-8") as fh:
            winner = json.load(fh)
        if winner.get("winner_identity_sha256") != FR836["winner_identity_sha256"]:
            raise C2IdentityError("winner identity mismatch")

        os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
        import tensorflow as tf
        from predictor_v3.hpo import _custom_objects
        self._model = tf.keras.models.load_model(
            paths.KERAS_S0, compile=False, custom_objects=_custom_objects())
        from predictor_v3 import features, norm_ext
        self._features = features
        self._formal_norm = features.load_formal_norm()
        self._norm_ext = norm_ext.load_artifact()
        self._channel_order = features.registry_channel_order()

    @classmethod
    def instance(cls) -> "C2Predictor":
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance

    def predict_q50(self, hist_raw: dict, kf_raw: dict) -> np.ndarray:
        """One window. hist_raw: {ch: (300,)} raw; kf_raw: {ch: (30,)} raw.
        Returns q50 (30,) float64 kW."""
        feats = self._features.assemble(
            {c: np.asarray(hist_raw[c], dtype=np.float64)[None, :]
             for c in self._channel_order["h1"]},
            {c: np.asarray(kf_raw[c], dtype=np.float64)[None, :]
             for c in self._channel_order["kf"]},
            self._formal_norm, self._norm_ext, self._channel_order)
        out = self._model({"hist_input": feats["hist"],
                           "kf_input": feats["kf"]}, training=False).numpy()
        q = np.asarray(out, dtype=np.float64)[0, :, 1]
        if q.shape != (30,) or not np.all(np.isfinite(q)):
            raise C2IdentityError("C2 output contract broken")
        return q


def self_qualification() -> dict:
    """Nonformal: determinism + contract on synthetic tensors and one
    train-split npz window sanity (train is non-corpus, non-sealed)."""
    pred = C2Predictor.instance()
    rng = np.random.default_rng(20260915)
    hist = {c: rng.normal(size=300) for c in pred._channel_order["h1"]}
    kf = {c: rng.normal(size=30) for c in pred._channel_order["kf"]}
    q1 = pred.predict_q50(hist, kf)
    q2 = pred.predict_q50(hist, kf)
    bitwise = bool(np.array_equal(q1, q2))

    # train-window sanity: assemble from the frozen npz (train row 0)
    z = np.load(paths.WINDOWS_NPZ, allow_pickle=False)
    ledger.log_access(
        artifact=paths.rel(paths.WINDOWS_NPZ), purpose="QUALIFICATION",
        split_contact="train-window",
        note="C2 inference smoke on one train window (non-corpus)",
    )
    mask = np.nonzero(z["splits"] == "train")[0]
    i = int(mask[0])
    hist_r = {c: z[f"X_H1::{c}"][i].astype(np.float64)
              for c in pred._channel_order["h1"]}
    kf_r = {c: z[f"X_KF::{c}"][i].astype(np.float64)
            for c in pred._channel_order["kf"]}
    qt = pred.predict_q50(hist_r, kf_r)
    y_true = z["Y"][i].astype(np.float64)
    mae = float(np.mean(np.abs(qt - y_true)))
    return {
        "schema": "ppe_c2_inference_qual_v1",
        "keras_sha256": FR836["keras_sha256"],
        "determinism_bitwise": bitwise,
        "synthetic_finite": bool(np.all(np.isfinite(q1))),
        "train_window_index": i,
        "train_window_mae_kw": mae,
        "train_window_sanity_plausible": bool(mae < 200.0),
        "note": "train-split sanity is a NONFORMAL smoke, not a metric claim",
        "verdict": "PASS" if (bitwise and np.all(np.isfinite(q1))
                              and mae < 200.0) else "FAIL",
    }


if __name__ == "__main__":
    import json
    out = self_qualification()
    print(json.dumps(out, indent=1))

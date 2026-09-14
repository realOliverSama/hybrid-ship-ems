#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_t017.py — T-017 执行驱动：补算 MAE/CE80 per-group → winner 裁定 →
PRETERMINAL 包 + reload 探针（canonical 环境；只消费 val）。

在 confirm 证据基础上补 per_group_mae/ce80（winner 层级②③所需），
用冻结 checkpoint 在 val 上推理。全程不触 test/OOD。
"""
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import data_access, winner as wn  # noqa: E402
from predictor_v3 import registry as reg  # noqa: E402
from predictor_v3.metrics import PICP_LEVEL  # noqa: E402
from predictor_v3.quantile import pinball_numpy  # noqa: E402

ARCHS = ("mlpq", "tcnq", "transformer", "tft", "reatt")
CONFIRM = os.path.join(MODELE_ROOT, "results", "predictor_v3", "confirm")


def main() -> int:
    import tensorflow as tf
    for g in tf.config.list_physical_devices("GPU"):
        tf.config.experimental.set_memory_growth(g, True)

    val = data_access.load_split("val", log=True)
    raw = val.raw_arrays()
    y = raw["y_raw"]
    voyage_ids = val.voyage_ids
    gmap = json.load(open(os.path.join(
        MODELE_ROOT, "results", "predictor_v3", "iid_voyage_group_map.json")))["mapping"]
    groups_arr = np.array([gmap.get(v, f"__{v}") for v in voyage_ids])
    uniq_groups = sorted(set(groups_arr.tolist()))

    formal_norm = json.load(open(os.path.join(
        MODELE_ROOT, "results", "dataset_v3", "formal", "formal_norm_v3.json")))
    norm_ext = json.load(open(os.path.join(
        MODELE_ROOT, "config", "predictor_v3", "predictor_norm_ext_v1.json")))

    from predictor_v3.features import assemble
    hist_by_ch = {ch: raw["hist_raw"][:, :, i] for i, ch in enumerate(val.h1_channels)}
    kf_by_ch = {ch: raw["kf_raw"][:, :, i] for i, ch in enumerate(val.kf_channels)}
    feats = assemble(hist_by_ch, kf_by_ch, formal_norm, norm_ext)
    inputs = {"hist_input": feats["hist"]}
    if "kf" in feats:
        inputs["kf_input"] = feats["kf"]

    from predictor_v3.hpo import _custom_objects
    candidates = {}
    params = {}
    artifact_dir_map = {}
    for arch in ARCHS:
        per_group = {m: {} for m in ("pinball", "mae", "ce80")}
        for s in (0, 1, 2):
            ev = json.load(open(os.path.join(CONFIRM, arch, f"s{s}_evidence.json")))
            model = tf.keras.models.load_model(
                os.path.join(MODELE_ROOT, ev["artifact"]),
                compile=False, custom_objects=_custom_objects())
            if s == 0:
                params[arch] = int(model.count_params())
            pred = np.asarray(model.predict(inputs, verbose=0, batch_size=1024),
                              dtype=np.float64)
            pb = pinball_numpy(y, pred)
            err = pred[..., 1] - y
            q10, q90 = pred[..., 0], pred[..., 2]
            ce = (1.0 - ((y >= q10) & (y <= q90)).mean(axis=1)) - 0.20  # CE80 per 窗
            for g in uniq_groups:
                m = groups_arr == g
                per_group["pinball"].setdefault(g, []).append(float(pb[m].mean()))
                per_group["mae"].setdefault(g, []).append(float(np.abs(err[m]).mean()))
                per_group["ce80"].setdefault(g, []).append(float(ce[m].mean()))
            del model
            tf.keras.backend.clear_session()
        # seed-median per group（obligation 9）
        candidates[arch] = {
            m: [float(np.median(per_group[m][g])) for g in uniq_groups]
            for m in ("pinball", "mae", "ce80")}
        for m in ("pinball", "mae", "ce80"):
            with open(os.path.join(CONFIRM, arch, f"s_all_per_group_{m}.json"),
                      "w", encoding="utf-8") as fh:
                json.dump({g: [per_group[m][g][i] for i in range(3)]
                           for g in uniq_groups}, fh, ensure_ascii=False,
                          indent=2, sort_keys=True)
        artifact_dir_map[arch] = os.path.relpath(
            os.path.join(CONFIRM, arch), MODELE_ROOT)
        print(f"[t017] {arch}: pinball_sgmacro(seed-median)="
              f"{float(np.mean(candidates[arch]['pinball'])):.4f}")

    result = wn.select_winner(candidates, params, b=10000,
                              rng=np.random.default_rng(20260915))
    print(f"[t017] WINNER={result['winner']} stage={result['stage']}")
    print(f"       S1={result['S1']} S2={result['S2']} S3={result['S3']}")

    env_manifest = os.path.join(MODELE_ROOT, "results", "predictor_v3",
                                "env_manifest_cuda.json")
    doc = wn.write_preterminal(result, candidates, params,
                               artifact_dir_map=artifact_dir_map,
                               env_manifest_sha256=wn.sha256_file(env_manifest))
    print(f"[t017] PRETERMINAL written: winner={doc['winner']['model_id']} "
          f"identity={doc['winner_identity_sha256'][:16]}…")
    return 0


if __name__ == "__main__":
    sys.exit(main())

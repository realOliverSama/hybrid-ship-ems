#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""rehearse_t018.py — T-018 UNSEAL 前置：完整评估器在 val 上预演（FR-606）。

用 winner checkpoint（seed0 canonical）在 val split 上跑通
evaluate_model_on_view 全链路，产物 = val 预演证据（hash 供 campaign 引用）。
不触 test/OOD。
"""
import hashlib
import json
import os
import sys

MODELE = "/root/autodl-tmp/hybrid-ship-ems/model/modele_link"
sys.path.insert(0, MODELE)

from datetime import datetime, timezone

import numpy as np
import tensorflow as tf

from predictor_v3 import data_access, terminal
from predictor_v3.hpo import _custom_objects

RES = os.path.join(MODELE, "results/predictor_v3")


def main():
    for g in tf.config.list_physical_devices("GPU"):
        tf.config.experimental.set_memory_growth(g, True)

    pre = json.load(open(os.path.join(
        RES, "winner/WINNER_SELECTED_PRETERMINAL.json")))
    winner_arch = pre["winner"]["arch"]
    ev = json.load(open(os.path.join(
        RES, "confirm", winner_arch, "s0_evidence.json")))
    model_path = os.path.join(MODELE, ev["artifact"])

    model = tf.keras.models.load_model(
        model_path, compile=False, custom_objects=_custom_objects())
    print(f"[rehearse] winner model loaded: {winner_arch} s0")

    formal_norm = json.load(open(os.path.join(
        MODELE, "results/dataset_v3/formal/formal_norm_v3.json")))
    norm_ext = json.load(open(os.path.join(
        MODELE, "config/predictor_v3/predictor_norm_ext_v1.json")))

    val = data_access.load_split("val", log=True)
    report = terminal.evaluate_model_on_view(
        model, val, norm_ext=norm_ext, formal_norm=formal_norm)

    ok = (
        report["n_windows"] > 0
        and np.isfinite(report.get("crossing_rate", -1))
        and report["views"]["split_group_macro"].get("pinball") is not None
    )
    evidence = {
        "schema": "predictor_v3_val_rehearsal_v1",
        "utc": datetime.now(timezone.utc).isoformat(),
        "winner_arch": winner_arch,
        "model_path": ev["artifact"],
        "n_windows": report["n_windows"],
        "n_split_groups": report["views"].get("n_split_groups"),
        "sgmacro_pinball": report["views"]["split_group_macro"]["pinball"],
        "crossing_rate": report["crossing_rate"],
        "pass": bool(ok),
    }
    evidence["artifact_sha256"] = hashlib.sha256(
        json.dumps({k: v for k, v in evidence.items()
                    if k != "artifact_sha256"}, sort_keys=True,
                   separators=(",", ":")).encode()).hexdigest()
    out = os.path.join(RES, "val_rehearsal_evidence.json")
    json.dump(evidence, open(out, "w"), indent=2, sort_keys=True)
    print(f"[rehearse] pass={ok} windows={report['n_windows']} "
          f"pinball={evidence['sgmacro_pinball']:.4f}")


if __name__ == "__main__":
    main()

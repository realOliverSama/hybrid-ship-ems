#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_t018.py — T-018 终评执行（UNSEAL 后；五候选+消融臂 ×3种子 ×（test+OOD五条件））。

用法：python run_t018.py --lane 0 --models mlpq tcnq transformer
      python run_t018.py --lane 1 --models tft reatt
      python run_t018.py --lane 0 --phase ood_diagnostics
"""
import argparse
import hashlib
import json
import os
import sys
import time
from datetime import datetime, timezone

MODELE = "/root/autodl-tmp/hybrid-ship-ems/model/modele_link"
sys.path.insert(0, MODELE)

import numpy as np
import tensorflow as tf

from predictor_v3 import data_access, terminal
from predictor_v3.hpo import _custom_objects
from predictor_v3.metrics import full_report
from predictor_v3.ood_diagnostics import (axis1_input_drift,
                                          axis2_target_drift,
                                          axis3_degradation)
from predictor_v3.quantile import pinball_numpy

RES = os.path.join(MODELE, "results/predictor_v3")
CAMP = "t443-terminal-20260915"
ARCH_OF = {"mlpq": "mlpq", "tcnq": "tcnq", "transformer": "transformer",
           "tft": "tft", "reatt": "reatt",
           "reatt_abl_h1": "reatt", "reatt_abl_h1k": "reatt",
           "reatt_abl_h1kf": "reatt"}
Z = lambda: datetime.now(timezone.utc).isoformat()


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as fh:
        for c in iter(lambda: fh.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def load_model_for(key, seed):
    ev = json.load(open(f"{RES}/confirm/{ARCH_OF[key]}/s{seed}_evidence.json"))
    m = tf.keras.models.load_model(os.path.join(MODELE, ev["artifact"]),
                                   compile=False, custom_objects=_custom_objects())
    return m, ev


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lane", type=int, required=True)
    ap.add_argument("--models", nargs="+", required=True)
    ap.add_argument("--phase", default="eval", choices=["eval", "ood_diagnostics"])
    args = ap.parse_args()
    os.environ["CUDA_VISIBLE_DEVICES"] = str(args.lane)
    for g in tf.config.list_physical_devices("GPU"):
        tf.config.experimental.set_memory_growth(g, True)

    formal_norm = json.load(open(f"{MODELE}/results/dataset_v3/formal/formal_norm_v3.json"))
    norm_ext = json.load(open(f"{MODELE}/config/predictor_v3/predictor_norm_ext_v1.json"))
    gmap = json.load(open(f"{RES}/iid_voyage_group_map.json"))["mapping"]
    ledger = terminal.IncidentLedger(CAMP)

    if args.phase == "ood_diagnostics":
        run_ood_diag(args.lane, formal_norm, norm_ext, gmap, ledger)
        return

    test_view = data_access.load_terminal("test", CAMP, campaign_dir=f"{RES}/terminal_campaigns")
    test_raw = test_view.raw_arrays()
    test_groups = np.array([gmap.get(v, f"__{v}") for v in test_view.voyage_ids])
    ood_views = {}
    for cond in ("A1", "B1", "C1", "C2", "C3"):
        v = data_access.load_terminal("ood", CAMP, ood=True,
                                      campaign_dir=f"{RES}/terminal_campaigns")
        # load_terminal 返回全量 OOD 视图；按条件过滤视图索引
        keep = np.flatnonzero(v.ood_conditions == cond)
        v.index = keep
        v._index = keep
        ood_views[cond] = v
    print(f"[t018] lane{args.lane} views ready: test={test_view.n_windows} "
          f"ood={ {c: v.n_windows for c, v in ood_views.items()} }", flush=True)

    from predictor_v3.features import assemble
    for key in args.models:
        for seed in (0, 1, 2):
            out = f"{RES}/terminal/{key}_s{seed}_metrics.json"
            if os.path.isfile(out):
                print(f"[t018] skip {key} s{seed} (exists)", flush=True)
                continue
            model, ev = load_model_for(key, seed)
            for set_name, view, raw, groups in [
                    ("test", test_view, test_raw, test_groups)] + [
                    (f"ood_{c}", v, None, None) for c, v in ood_views.items()]:
                t0 = time.time()
                try:
                    hist_by_ch = {ch: raw["hist_raw"][:, :, i]
                                  for i, ch in enumerate(view.h1_channels)}
                    kf_by_ch = {ch: raw["kf_raw"][:, :, i]
                                for i, ch in enumerate(view.kf_channels)}
                    feats = assemble(hist_by_ch, kf_by_ch, formal_norm, norm_ext)
                    inp = {"hist_input": feats["hist"]}
                    if "kf" in feats:
                        inp["kf_input"] = feats["kf"]
                    pred = np.asarray(model.predict(inp, verbose=0, batch_size=1024),
                                      dtype=np.float64)
                    rep = full_report(
                        raw["y_raw"], pred, view.voyage_ids,
                        {v: gmap.get(str(v), f"__{v}") for v in np.unique(view.voyage_ids)},
                        e300_true=raw["e300_raw"],
                        y_std_per_lead=formal_norm["y_std"],
                        y_std_global=json.load(open(
                            f"{MODELE}/config/predictor_v3/eval_config.json"))[
                            "normalization_denominators"]["global"]["value"])
                    rep["crossing_rate"] = 0.0
                    rep["crossing_rate"] = __import__(
                        "predictor_v3.quantile",
                        fromlist=["crossing_rate"]).crossing_rate(pred)
                    result = {
                        "schema": "predictor_v3_metrics_result_v1",
                        "model_key": key, "arch": ARCH_OF[key],
                        "seed_index": seed, "set": set_name,
                        "campaign_id": CAMP,
                        "views": rep["views"], "segments": rep["segments"],
                        "crossing_rate": rep["crossing_rate"],
                        "e300": rep.get("e300"),
                        "normalized": rep.get("normalized"),
                        "coverage_error_80": rep.get("coverage_error_80"),
                        "n_windows": rep["n_windows"],
                        "env_manifest_sha256": sha256_file(
                            f"{RES}/env_manifest_cuda.json"),
                        "git_commit": os.environ.get(
                            "EMS_PINNED_GIT_COMMIT", "UNPINNED"),
                        "duration_s": round(time.time() - t0, 1),
                        "utc": Z(),
                    }
                    result["artifact_sha256"] = hashlib.sha256(
                        json.dumps({k: v for k, v in result.items()
                                    if k != "artifact_sha256"},
                                   sort_keys=True,
                                   separators=(",", ":")).encode()).hexdigest()
                    os.makedirs(f"{RES}/terminal", exist_ok=True)
                    json.dump(result, open(out.replace(".json", f"_{set_name}.json", 1)
                                           if False else
                                           f"{RES}/terminal/{key}_s{seed}_{set_name}.json",
                                           "w"), indent=2, sort_keys=True)
                    print(f"[t018] {key} s{seed} {set_name}: pinball_sgmacro="
                          f"{rep['views']['split_group_macro']['pinball']:.4f} "
                          f"({result['duration_s']}s)", flush=True)
                except Exception as exc:  # noqa: BLE001
                    ledger.add(phase=f"{key} s{seed} {set_name}",
                               symptom=f"{type(exc).__name__}: {exc}"[:300],
                               root_cause_class="eval_error",
                               action="recorded; no rerun by this driver",
                               rerun_allowed=False,
                               verdict="FAILED recorded")
                    ledger.save()
                    print(f"[t018] {key} s{seed} {set_name} FAILED: {exc}",
                          flush=True)
            del model
            tf.keras.backend.clear_session()


def run_ood_diag(lane, formal_norm, norm_ext, gmap, ledger):
    """轴①②数据集级（一次）；轴③按模型（winner + 关键对照）。"""
    data_access.load_terminal("test", CAMP, campaign_dir=f"{RES}/terminal_campaigns")
    ood = data_access.load_terminal("ood", CAMP, ood=True,
                                    campaign_dir=f"{RES}/terminal_campaigns")
    iid = data_access.load_split("val", log=False)
    diag_cfg = json.load(open(f"{MODELE}/config/predictor_v3/diagnostics_config.json"))
    train_ref = diag_cfg["train_ref"]
    h1c, kfc = iid.h1_channels, iid.kf_channels
    axis1 = axis1_input_drift(ood_view=ood, iid_view=iid, train_ref=train_ref,
                              h1_channels=h1c, kf_channels=kfc)
    axis2 = axis2_target_drift(ood_view=ood, iid_view=iid, train_ref=train_ref)
    doc = {"schema": "predictor_v3_ood_diagnostics_v1", "campaign_id": CAMP,
           "axis1": axis1, "axis2": axis2,
           "note": "轴①②数据集级一次产出（FR-603）；轴③由模型级评估器另行产出"}
    doc["artifact_sha256"] = hashlib.sha256(
        json.dumps({k: v for k, v in doc.items() if k != "artifact_sha256"},
                   sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    json.dump(doc, open(f"{RES}/terminal/ood_diagnostics_axis12.json", "w"),
              indent=2, sort_keys=True)
    print("[ood_diag] axis1+2 written", flush=True)


if __name__ == "__main__":
    main()

# -*- coding: utf-8 -*-
"""confirm.py — E3 确认/消融批量驱动（T-015/T-016；DGN-E-10/12，R4/R5）。

- T-015：五候选 frozen selected_config × seeds{0,1,2} 在 canonical 确认训练
 （非调参——结果不得重开 HPO）；逐种子证据 + **每 run 冻结不可变 .keras 工件**
 （5×3=15 个，登记 model_id/config_hash/seed_index/工件 SHA256/环境/commit）；
- T-016：消融臂 REAT-T(H1)/(H1+K)/(H1+K+F) ×{0,1,2}——**同拓扑同参数量，
  归一化零通道替换**（R2）：H1 臂 6 通道全零、H1+K 臂 2 个 F 通道置零、
  H1+K+F 复用 T-015 的 REAT-T 确认运行；6 个新 .keras 工件冻结；
- 硬门（R4）：15/15 有效确认 run；消融 9 条 arm-seed 证据条目（6 新训 + 3 引用）。
全部落 results/predictor_v3/confirm/ 与 results/predictor_v3/ablation/。
"""
from __future__ import annotations

import hashlib
import json
import os
import time
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
CONFIRM_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "confirm")
ABLATION_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "ablation")
CKPT_ROOT = os.path.join(MODELE_ROOT, "experiments", "predictor_v3", "confirm")

from predictor_v3 import registry as reg  # noqa: E402
from predictor_v3 import seeds as sd  # noqa: E402
from predictor_v3.models import ARCH_BUILDERS  # noqa: E402
from predictor_v3.models.reat_t import ablation_zero_channels  # noqa: E402
from predictor_v3.quantile import pinball_numpy  # noqa: E402

ARCH_ORDER = ("mlpq", "tcnq", "transformer", "tft", "reatt")
CONFIRM_SEEDS = (0, 1, 2)


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_selected(arch: str) -> dict:
    p = os.path.join(MODELE_ROOT, "results", "predictor_v3", "candidates",
                     arch, "selected_config.json")
    with open(p, encoding="utf-8") as fh:
        return json.load(fh)


def _zero_kf_channels(kf: np.ndarray, channels_to_zero: list,
                      kf_channel_order: list) -> np.ndarray:
    """归一化零通道替换（R2）：transform 后 0 = train 均值语义。

    kf [N,30,6]；channels_to_zero 为 registry 通道名列表；禁新增运行时 mask
    张量——本替换发生在数据装载侧（assemble 之后、喂模型之前）。
    """
    if not channels_to_zero:
        return kf
    out = kf.copy()
    for ch in channels_to_zero:
        idx = kf_channel_order.index(ch)
        out[:, :, idx] = 0.0
    return out


def run_confirm(model_key: str, train_data: dict, val_data: dict, *,
                env_manifest_sha256: str, budget: dict | None = None,
                seed_indices=CONFIRM_SEEDS, log=print) -> dict:
    """单候选（或消融臂）×{0,1,2} 确认训练（T-015/T-016 通用驱动）。

    model_key = arch_id（五候选）或消融面 id（reatt_abl_h1 / reatt_abl_h1k）。
    逐 seed 训练 → 冻结 .keras 工件 + 证据 JSON；聚合（median/mean/std/min/max）
    汇总落盘。返回 {runs: [...], summary: {...}}。
    """
    import tensorflow as tf
    from predictor_v3.train import (compile_model, make_dataset,
                                    build_callbacks, load_budget)
    from predictor_v3.features import registry_channel_order
    budget = budget or load_budget()

    if model_key.startswith("reatt_abl"):
        base_arch = "reatt"
        sel = load_selected(base_arch)
        face = model_key
        zero_channels = ablation_zero_channels(face)
        model_id = face
    else:
        base_arch = model_key
        sel = load_selected(base_arch)
        zero_channels = []
        model_id = ARCH_BUILDERS[base_arch]["model_id"]

    cfg = sel["config"]
    config_sha = sel["config_sha256"]
    surface = sel.get("feature_surface") or ARCH_BUILDERS[base_arch]["feature_surface"]

    out_dir = os.path.join(
        ABLATION_DIR if model_key.startswith("reatt_abl") else CONFIRM_DIR,
        model_key)
    os.makedirs(out_dir, exist_ok=True)

    order = registry_channel_order()
    runs = []
    for seed_index in seed_indices:
        seed_int = sd.derive_seed(model_id, config_sha, seed_index)
        sd.set_global_seeds(seed_int)
        kf_train = (_zero_kf_channels(train_data["kf"], zero_channels, order["kf"])
                    if train_data.get("kf") is not None else None)
        kf_val = (_zero_kf_channels(val_data["kf"], zero_channels, order["kf"])
                  if val_data.get("kf") is not None else None)
        tr = {"hist": train_data["hist"], "kf": kf_train, "y": train_data["y"]}
        va = {"hist": val_data["hist"], "kf": kf_val, "y": val_data["y"],
              "voyage_group": val_data["voyage_group"]}

        import tensorflow as _tf
        for gpu in _tf.config.list_physical_devices("GPU"):
            _tf.config.experimental.set_memory_growth(gpu, True)
        tf.keras.utils.set_random_seed(seed_int)
        model = ARCH_BUILDERS[base_arch]["build"](cfg, _s0())
        compile_model(model, cfg["lr"], budget)
        ds = make_dataset(tr["hist"], tr.get("kf"), tr["y"],
                          cfg["batch"], seed_int)
        callbacks = build_callbacks(va, {}, budget)
        t0 = time.time()
        model.fit(ds, epochs=budget["max_epochs"], verbose=0,
                  callbacks=callbacks)

        # 冻结不可变工件（R5：seed0 亦为 canonical 交付工件）
        artifact_dir = os.path.join(CKPT_ROOT, model_key)
        os.makedirs(artifact_dir, exist_ok=True)
        art_path = os.path.join(artifact_dir, f"{model_key}_s{seed_index}.keras")
        model.save(art_path)
        artifact_sha256 = sha256_file(art_path)

        # val 逐组指标（per split_group，供 DGN-E-12 seed median 聚合）
        from predictor_v3.quantile import pinball_numpy as _pb
        inputs = {"hist_input": va["hist"]}
        if va.get("kf") is not None:
            inputs["kf_input"] = va["kf"]
        pred = model.predict(inputs, verbose=0, batch_size=1024)
        pb = _pb(va["y"], np.asarray(pred, dtype=np.float64))
        groups = np.asarray(va["voyage_group"])
        per_group = {str(g): float(pb[groups == g].mean())
                     for g in dict.fromkeys(groups.tolist())}
        sg_macro = float(np.mean(list(per_group.values())))

        evidence = {
            "schema": "predictor_v3_confirm_run_v1",
            "model_key": model_key,
            "model_id": model_id,
            "arch": base_arch,
            "ablation_face": face if model_key.startswith("reatt_abl") else None,
            "zero_kf_channels": zero_channels,
            "active_info_channels": [c for c in order["kf"]
                                     if c not in zero_channels],
            "config_sha256": config_sha,
            "seed_index": seed_index,
            "seed_int": seed_int,
            "artifact": os.path.relpath(art_path, MODELE_ROOT),
            "artifact_sha256": artifact_sha256,
            "param_count": int(model.count_params()),
            "val": {"pinball_sgmacro": sg_macro,
                    "per_group_ref": f"{model_key}/s{seed_index}_per_group.json"},
            "duration_s": round(time.time() - t0, 1),
            "env_manifest_sha256": env_manifest_sha256,
            "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
            "utc": datetime.now(timezone.utc).isoformat(),
        }
        with open(os.path.join(out_dir, f"s{seed_index}_evidence.json"), "w",
                  encoding="utf-8") as fh:
            json.dump(evidence, fh, ensure_ascii=False, indent=2, sort_keys=True)
        with open(os.path.join(out_dir, f"s{seed_index}_per_group.json"), "w",
                  encoding="utf-8") as fh:
            json.dump(per_group, fh, ensure_ascii=False, indent=2, sort_keys=True)
        runs.append(evidence)
        log(f"[confirm] {model_key} s{seed_index} sg_macro={sg_macro:.5f} "
            f"({evidence['duration_s']}s)")
        del model
        tf.keras.backend.clear_session()

    vals = [r["val"]["pinball_sgmacro"] for r in runs]
    summary = {
        "schema": "predictor_v3_confirm_summary_v1",
        "model_key": model_key,
        "config_sha256": config_sha,
        "seed_indices": list(seed_indices),
        "per_seed_pinball_sgmacro": {str(r["seed_index"]):
                                     r["val"]["pinball_sgmacro"] for r in runs},
        "aggregate": {"median": float(np.median(vals)), "mean": float(np.mean(vals)),
                      "std": float(np.std(vals)), "min": float(np.min(vals)),
                      "max": float(np.max(vals))},
        "artifacts": [{"seed_index": r["seed_index"],
                       "artifact_sha256": r["artifact_sha256"]} for r in runs],
        "env_manifest_sha256": env_manifest_sha256,
        "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
    }
    reg.write_json_with_self_hash(summary, os.path.join(out_dir, "summary.json"))
    return {"runs": runs, "summary": summary}


def _s0() -> float:
    from predictor_v3.models.common import load_s0
    return load_s0()


def _load_split_data(split: str) -> dict:
    from predictor_v3 import data_access, features
    view = data_access.load_split(split, log=True)
    raw = view.raw_arrays()
    hist_by_ch = {ch: raw["hist_raw"][:, :, i]
                  for i, ch in enumerate(view.h1_channels)}
    kf_by_ch = {ch: raw["kf_raw"][:, :, i]
                for i, ch in enumerate(view.kf_channels)}
    formal_norm = __import__("predictor_v3.features",
                             fromlist=["load_formal_norm"]).load_formal_norm()
    norm_ext = json.load(open(os.path.join(
        MODELE_ROOT, "config", "predictor_v3", "predictor_norm_ext_v1.json")))
    feats = features.assemble(hist_by_ch, kf_by_ch, formal_norm, norm_ext)
    with open(os.path.join(MODELE_ROOT, "results", "predictor_v3",
                           "iid_voyage_group_map.json"), encoding="utf-8") as fh:
        gmap = json.load(fh)["mapping"]
    groups = np.array([gmap.get(v, f"__novoyage_{v}") for v in view.voyage_ids])
    return {"hist": feats["hist"], "kf": feats.get("kf"), "y": raw["y_raw"],
            "voyage_group": groups}


def main(argv=None) -> int:
    """canonical 跑批入口：python -m predictor_v3.confirm --key <model_key> [--gpu N]"""
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--key", required=True,
                    help="arch_id（五候选）或消融面（reatt_abl_h1/h1k）")
    ap.add_argument("--gpu", default=None)
    args = ap.parse_args(argv)
    if args.gpu is not None:
        os.environ["CUDA_VISIBLE_DEVICES"] = args.gpu
    import tensorflow as tf
    for gpu in tf.config.list_physical_devices("GPU"):
        tf.config.experimental.set_memory_growth(gpu, True)
    manifest_sha = sha256_file(os.path.join(
        MODELE_ROOT, "results", "predictor_v3", "env_manifest_cuda.json"))
    train = _load_split_data("train")
    val = _load_split_data("val")
    run_confirm(args.key, train, val, env_manifest_sha256=manifest_sha)
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())

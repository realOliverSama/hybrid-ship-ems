# -*- coding: utf-8 -*-
"""hpo.py — HPO 引擎（T-008~T-011/T-014；DGN-E-08 + R5 预算记账）。

试验清单生成七步（R5 冻结）：
 ① 枚举离散笛卡尔配置全集；
 ② canonicalize 配置；
 ③ model-build/count_params 容量过滤（先于任何 result-bearing 训练；
    envelope per FR-587）；
 ④ 被拒配置记入 trial_generation_audit；
 ⑤ 注入强制锚点配置（REAT-T 必含锚点；过滤后全集须仍含 ≥1 个 ≤1.5M
    有效配置——REAT-T 属 T-014）；
 ⑥ 用派生 HPO 种子 SHA256(seed_root‖"hpo"‖arch_id) 对剩余有效配置均匀无放回
    抽样至预算数；
 ⑦ HPO 启动前冻结最终 trial list JSON（每 trial = 完整 config + config 自哈希 +
    trial_id），落 results/predictor_v3/hpo/<arch>_trial_list.json。

trial 执行（canonical 环境）：seed_index=0（seeds.derive_seed）；train/val only；
主目标 = val split_group_macro pinball（与 ES/checkpoint 同口径）；DGN-E-09 预算治理；
registry append-only（COMPLETED_ELIGIBLE / COMPLETED_INELIGIBLE / FAILED /
INFRA_INTERRUPTED）；已开始 result-bearing 训练的 trial 失败也耗预算位。

selected_config（FR-596/DGN-E-16(3)）：argmin eligible 的 val pinball_sgmacro。
"""
from __future__ import annotations

import hashlib
import itertools
import json
import os
import time
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
HPO_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "hpo")
CKPT_ROOT = os.path.join(MODELE_ROOT, "experiments", "predictor_v3", "hpo")

from predictor_v3 import registry as reg  # noqa: E402
from predictor_v3 import seeds as sd  # noqa: E402
from predictor_v3.models import ARCH_BUILDERS  # noqa: E402


# ------------------------------------------------------------ 七步清单生成

def canonicalize(cfg: dict) -> dict:
    """② canonicalize：键排序 + float 归一（json round-trip）。"""
    return json.loads(json.dumps(cfg, sort_keys=True))


def enumerate_configs(arch_id: str) -> list:
    """① 笛卡尔配置全集（registry SEARCH_SPACE 驱动）。"""
    space = ARCH_BUILDERS[arch_id]["search_space"]
    keys = sorted(space)
    return [dict(zip(keys, vals)) for vals in itertools.product(*(space[k] for k in keys))]


def count_params_for(arch_id: str, cfg: dict, s0: float) -> int:
    """③ 容量测量（model-build/count_params；先于任何 result-bearing 训练）。"""
    import tensorflow as tf
    model = ARCH_BUILDERS[arch_id]["build"](cfg, s0)
    n = int(model.count_params())
    del model
    tf.keras.backend.clear_session()
    return n


def generate_trial_list(arch_id: str, budget_cap: int, s0: float,
                        out_dir: str | None = None,
                        anchor_cfg: dict | None = None) -> dict:
    """①~⑦ 全链（本地执行，非 result-bearing）。返回 {trial_list 文档, audit}。"""
    if arch_id not in ARCH_BUILDERS:
        raise KeyError(f"未知架构 {arch_id}（REAT-T 属 T-014 另行注册）")
    envelope = ARCH_BUILDERS[arch_id]["envelope"]
    lo, hi = envelope

    # ①~④ 枚举 + canonicalize + 容量过滤 + audit
    accepted, rejected = [], []
    for cfg in enumerate_configs(arch_id):
        cfg = canonicalize(cfg)
        n = count_params_for(arch_id, cfg, s0)
        if lo <= n <= hi:
            accepted.append((cfg, n))
        else:
            rejected.append({"config": cfg, "param_count": n,
                             "reason": f"count_params {n} ∉ [{lo}, {hi}]"})
    if not accepted:
        raise ValueError(f"{arch_id}: 容量过滤后零有效配置（envelope [{lo},{hi}]）")

    # ⑤ 锚点注入（REAT-T；E1 架构无锚点或锚点已在网格内）
    if anchor_cfg is not None:
        anchor_cfg = canonicalize(anchor_cfg)
        if anchor_cfg not in [c for c, _ in accepted]:
            n = count_params_for(arch_id, anchor_cfg, s0)
            if lo <= n <= hi:
                accepted.append((anchor_cfg, n))
            else:
                raise ValueError("锚点配置超容量信封")
        lo_min = min(n for _, n in accepted)
        if lo_min > 1_500_000:
            raise ValueError("过滤后全集无 ≤1.5M 有效配置（FR-587 REAT-T 义务）")

    # ⑥ 派生 HPO 种子均匀无放回抽样至预算
    hpo_seed = sd.derive_hpo_seed(arch_id)
    rng = np.random.default_rng(hpo_seed)
    pool = sorted(accepted, key=lambda t: json.dumps(t[0], sort_keys=True))
    if len(pool) > budget_cap:
        idx = np.sort(rng.choice(len(pool), size=budget_cap, replace=False))
        chosen = [pool[i] for i in idx]
    else:
        chosen = pool
    subsampled = [c for c in pool if c not in chosen]

    # ⑦ 冻结 trial list JSON
    trials = []
    for i, (cfg, n) in enumerate(chosen):
        full_cfg = {**cfg, "seed_index": 0}
        trials.append({"trial_id": f"{arch_id}-t{i:03d}",
                       "arch": arch_id,
                       "config": cfg,
                       "param_count": n,
                       "config_sha256": reg.config_hash(full_cfg)})
    doc = {
        "schema": "predictor_v3_hpo_trial_list_v1",
        "arch": arch_id,
        "budget_cap": budget_cap,
        "n_trials": len(trials),
        "hpo_seed": hpo_seed,
        "sampling": "uniform without replacement over capacity-valid configs"
                    "（numpy default_rng，sorted canonical key 序）",
        "trials": trials,
        "subsampled_out": [{"config": c, "param_count": n} for c, n in subsampled],
        "s0": s0,
    }
    doc["artifact_sha256"] = hashlib.sha256(
        reg.canonical_json({k: v for k, v in doc.items()
                            if k != "artifact_sha256"})).hexdigest()
    audit = {
        "schema": "predictor_v3_trial_generation_audit_v1",
        "arch": arch_id,
        "n_enumerated": len(accepted) + len(rejected),
        "n_capacity_valid": len(accepted),
        "n_rejected": len(rejected),
        "rejected": rejected,
        "n_subsampled_out": len(subsampled),
        "hpo_seed": hpo_seed,
        "budget_cap": budget_cap,
    }
    out_dir = out_dir or HPO_DIR
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, f"{arch_id}_trial_list.json"), "w",
              encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    with open(os.path.join(out_dir, f"{arch_id}_trial_generation_audit.json"),
              "w", encoding="utf-8") as fh:
        json.dump(audit, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    return doc, audit


# ------------------------------------------------------------ trial 执行（canonical）

def _val_group_pinball(model, val_data: dict) -> tuple:
    """val sg_macro pinball + per-group 值（主目标口径，FR-595/DGN-E-12）。"""
    from predictor_v3.quantile import pinball_numpy
    inputs = {"hist_input": val_data["hist"]}
    if val_data.get("kf") is not None:
        inputs["kf_input"] = val_data["kf"]
    pred = model.predict(inputs, verbose=0)
    pb = pinball_numpy(val_data["y"], np.asarray(pred, dtype=np.float64))
    groups = np.asarray(val_data["voyage_group"])
    per_group = {str(g): float(pb[groups == g].mean())
                 for g in dict.fromkeys(groups.tolist())}
    sg_macro = float(np.mean(list(per_group.values())))
    return sg_macro, per_group


def run_trial(trial: dict, train_data: dict, val_data: dict,
              *, arch_id: str, env_manifest_sha256: str,
              budget: dict | None = None, ckpt_root: str | None = None,
              log=print) -> dict:
    """单 trial 正式执行（canonical 环境；seed_index=0；DGN-E-09 全治理）。

    结果（COMPLETED_ELIGIBLE / FAILED）追加 registry；INFRA_INTERRUPTED 不占预算位。
    本函数须在 canonical 环境运行（本地仅 smoke）。
    """
    import tensorflow as tf
    budget = budget or __import__("predictor_v3.train", fromlist=["load_budget"]).load_budget()
    arch = ARCH_BUILDERS[arch_id]
    ckpt_root = ckpt_root or CKPT_ROOT
    cfg = trial["config"]
    full_cfg = {**cfg, "seed_index": 0}
    config_sha = reg.config_hash(full_cfg)
    model_id = arch["model_id"]
    seed_int = sd.derive_seed(model_id, config_sha, 0)
    seed_rec = sd.set_global_seeds(seed_int)

    trial_reg = reg.TrialRegistry(arch_id)
    existing = [r for r in trial_reg.rows() if r["trial_id"] == trial["trial_id"]]
    prior = next((r for r in existing
                  if r["status"] == "INFRA_INTERRUPTED"), None)

    ckpt_dir = os.path.join(ckpt_root, arch_id, trial["trial_id"])
    os.makedirs(ckpt_dir, exist_ok=True)
    ckpt_path = os.path.join(ckpt_dir, "model.keras")
    t0 = time.time()
    try:
        model = arch["build"](cfg, _s0())
        from predictor_v3.train import compile_model, make_dataset
        compile_model(model, cfg["lr"], budget)
        ds = make_dataset(train_data["hist"], train_data.get("kf"),
                          train_data["y"], cfg["batch"], seed_int)
        callbacks = __import__("predictor_v3.train", fromlist=["build_callbacks"]).build_callbacks(
            val_data, {}, budget)
        callbacks.append(tf.keras.callbacks.ModelCheckpoint(
            ckpt_path, monitor="val_sgmacro_pinball", mode="min",
            save_best_only=True, verbose=0))
        model.fit(ds, epochs=budget["max_epochs"], verbose=0,
                  callbacks=callbacks)
        # 复载最佳权重评估（与 ES restore_best_weights 同口径的显式 checkpoint）
        best = tf.keras.models.load_model(
            ckpt_path, compile=False,
            custom_objects=_custom_objects())
        sg_macro, per_group = _val_group_pinball(best, val_data)
        if not np.isfinite(sg_macro):
            raise FloatingPointError(f"val pinball 非有限: {sg_macro}")
        row = {"trial_id": trial["trial_id"], "arch": arch_id,
               "config_sha256": config_sha, "seed_index": 0,
               "status": "COMPLETED_ELIGIBLE",
               "val": {"pinball_sgmacro": sg_macro,
                       "per_group_ref": f"{trial['trial_id']}/val_per_group.json"},
               "duration_s": round(time.time() - t0, 1),
               "env_manifest_sha256": env_manifest_sha256,
               "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
               "seed_int": seed_int,
               "seed_record": seed_rec,
               "checkpoint": os.path.relpath(ckpt_path, MODELE_ROOT)}
        trial_reg.append(row)
        with open(os.path.join(ckpt_dir, "val_per_group.json"), "w",
                  encoding="utf-8") as fh:
            json.dump({"trial_id": trial["trial_id"],
                       "per_group_pinball": per_group,
                       "sg_macro": sg_macro}, fh, ensure_ascii=False,
                      indent=2, sort_keys=True)
        log(f"[hpo] {trial['trial_id']} ELIGIBLE pinball_sgmacro={sg_macro:.5f} "
            f"({row['duration_s']}s)")
        return row
    except KeyboardInterrupt:
        raise
    except Exception as exc:  # noqa: BLE001
        row = {"trial_id": trial["trial_id"], "arch": arch_id,
               "config_sha256": config_sha, "seed_index": 0,
               "status": "FAILED",
               "error": f"{type(exc).__name__}: {exc}",
               "duration_s": round(time.time() - t0, 1),
               "env_manifest_sha256": env_manifest_sha256,
               "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED")}
        trial_reg.append(row)
        log(f"[hpo] {trial['trial_id']} FAILED: {row['error'][:200]}")
        return row


def _s0() -> float:
    from predictor_v3.models.common import load_s0
    return load_s0()


def _custom_objects() -> dict:
    from predictor_v3.models.common import (GLU, GRN, InterpretableMHA,
                                            LearnedPositionalEncoding,
                                            LearnedQueryAttentionPooling,
                                            ChannelSlice, TakeFuture)
    from predictor_v3.models.reat_t import TilePast
    from predictor_v3.quantile import MonotonicQuantileTransform
    return {"GLU": GLU, "GRN": GRN, "InterpretableMHA": InterpretableMHA,
            "LearnedQueryAttentionPooling": LearnedQueryAttentionPooling,
            "LearnedPositionalEncoding": LearnedPositionalEncoding,
            "ChannelSlice": ChannelSlice, "TakeFuture": TakeFuture,
            "TilePast": TilePast,
            "MonotonicQuantileTransform": MonotonicQuantileTransform}


# ------------------------------------------------------------ selected_config

def select_config(arch_id: str, *, s0: float, env_manifest_sha256: str,
                  out_dir: str | None = None, log=print) -> dict:
    """budget 用尽即选最优 eligible（FR-596；eligible==0 ⇒ STOP 升级，禁扩预算）。"""
    trial_reg = reg.TrialRegistry(arch_id)
    eligible = trial_reg.eligible_trials()
    list_path = os.path.join(HPO_DIR, f"{arch_id}_trial_list.json")
    with open(list_path, encoding="utf-8") as fh:
        trial_list = json.load(fh)
    if not eligible:
        raise RuntimeError(
            f"{arch_id}: eligible_trial_count == 0 ⇒ 任务 FAIL/STOP（R3；禁扩预算，升级用户）")
    best = min(eligible, key=lambda r: r["val"]["pinball_sgmacro"])
    trial_def = next(t for t in trial_list["trials"]
                     if t["trial_id"] == best["trial_id"])
    full_cfg = {**trial_def["config"], "seed_index": 0}
    sel = {
        "schema": "predictor_v3_selected_config_v1",
        "arch": arch_id,
        "model_id": ARCH_BUILDERS[arch_id]["model_id"],
        "feature_surface": ARCH_BUILDERS[arch_id]["feature_surface"],
        "param_count": trial_def["param_count"],
        "search_space": ARCH_BUILDERS[arch_id]["search_space"],
        "trial_list_sha256": sha256_file(list_path),
        "trial_registry_sha256": sha256_file(trial_reg.path),
        "budget_cap": trial_list["budget_cap"],
        "budget_consumed": trial_reg.budget_consumed(),
        "n_eligible": len(eligible),
        "selected_trial_id": best["trial_id"],
        "config": trial_def["config"],
        "config_sha256": reg.config_hash(full_cfg),
        "seed0_val_evidence": {
            "pinball_sgmacro": best["val"]["pinball_sgmacro"],
            "per_group_ref": best["val"]["per_group_ref"],
            "checkpoint": best["checkpoint"],
            "duration_s": best["duration_s"],
        },
        "env_manifest_sha256": env_manifest_sha256,
        "selected_utc": datetime.now(timezone.utc).isoformat(),
        "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
        "selection_rule": "argmin val split_group_macro pinball over COMPLETED_ELIGIBLE（R3）",
    }
    out_dir = out_dir or os.path.join(MODELE_ROOT, "results", "predictor_v3",
                                      "candidates", arch_id)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "selected_config.json")
    reg.write_json_with_self_hash(sel, path)
    log(f"[hpo] {arch_id} selected {best['trial_id']} "
        f"pinball_sgmacro={best['val']['pinball_sgmacro']:.5f} "
        f"(eligible={len(eligible)}/{trial_list['budget_cap']})")
    return sel


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ------------------------------------------------------------ CLI（canonical 跑批）

def main(argv=None) -> int:
    """canonical 环境跑批入口（远端 nohup）：

    python -m predictor_v3.hpo --arch mlpq --phase run
    python -m predictor_v3.hpo --arch mlpq --phase select
    （清单已在本地生成冻结并随代码树传输）
    """
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--arch", required=True)
    ap.add_argument("--phase", required=True, choices=["run", "select"])
    ap.add_argument("--filter", default="all", choices=["all", "even", "odd"],
                    help="trial_id 尾号奇偶分半（多卡并行用；清单不变，"
                         "registry/预算语义不变）")
    args = ap.parse_args(argv)
    if args.phase == "run":
        # 用户裁定 2026-09-13（选项 A）：小架构（实测 GPU<30%）允许 2 路并发——
        # 显存按需增长（默认 TF 预分配全卡会堵死并发）；大架构并发前须实测 util。
        import tensorflow as tf
        for _gpu in tf.config.list_physical_devices("GPU"):
            tf.config.experimental.set_memory_growth(_gpu, True)

    manifest_path = os.path.join(MODELE_ROOT, "results", "predictor_v3",
                                 "env_manifest_cuda.json")
    env_manifest_sha = sha256_file(manifest_path)
    if args.phase == "run":
        with open(os.path.join(HPO_DIR, f"{args.arch}_trial_list.json"),
                  encoding="utf-8") as fh:
            trial_list = json.load(fh)
        train = _load_split_data("train")
        val = _load_split_data("val")
        budget = __import__("predictor_v3.train", fromlist=["load_budget"]).load_budget()
        trial_reg = reg.TrialRegistry(args.arch)
        done_ids = {r["trial_id"] for r in trial_reg.rows()
                    if r["status"] == "COMPLETED_ELIGIBLE"}
        skipped = 0
        for trial in trial_list["trials"]:
            if trial["trial_id"] in done_ids:
                skipped += 1
                continue
            tail = int(trial["trial_id"].rsplit("t", 1)[1])
            if args.filter == "even" and tail % 2 != 0:
                continue
            if args.filter == "odd" and tail % 2 != 1:
                continue
            run_trial(trial, train, val, arch_id=args.arch,
                      env_manifest_sha256=env_manifest_sha, budget=budget)
        print(f"[hpo] {args.arch}: {skipped} already-eligible trials skipped "
              f"(resume-safe)")
        return 0
    sel = select_config(args.arch, s0=_s0(), env_manifest_sha256=env_manifest_sha)
    return 0


def _load_split_data(split: str) -> dict:
    """canonical 数据装载（fail-closed load_split；H1/KF raw → transform → 特征）。"""
    from predictor_v3 import data_access, features
    from predictor_v3.models import ARCH_BUILDERS  # noqa: F401  (surface 检查在调用方)
    view = data_access.load_split(split, log=True)
    raw = view.raw_arrays()
    hist_by_ch = {ch: raw["hist_raw"][:, :, i]
                  for i, ch in enumerate(view.h1_channels)}
    kf_by_ch = {ch: raw["kf_raw"][:, :, i]
                for i, ch in enumerate(view.kf_channels)}
    formal_norm = features.load_formal_norm()
    norm_ext = __import__("json").load(open(os.path.join(
        MODELE_ROOT, "config", "predictor_v3", "predictor_norm_ext_v1.json")))
    feats = features.assemble(hist_by_ch, kf_by_ch, formal_norm, norm_ext)
    # split_group：voyage → group 映射来自 split_assignment（IID 语义在 D 侧冻结）
    grp_of = _voyage_group_map()
    groups = np.array([grp_of.get(v, f"__novoyage_{v}") for v in view.voyage_ids])
    return {"hist": feats["hist"], "kf": feats.get("kf"), "y": raw["y_raw"],
            "voyage_group": groups, "voyage_ids": view.voyage_ids}


def _voyage_group_map() -> dict:
    """voyage→split_group（E 侧元数据工件 iid_voyage_group_map.json，DGV-E-02）。"""
    p = os.path.join(MODELE_ROOT, "results", "predictor_v3",
                     "iid_voyage_group_map.json")
    with open(p, encoding="utf-8") as fh:
        doc = json.load(fh)
    expect = doc.get("artifact_sha256")
    if not expect or hashlib.sha256(
            reg.canonical_json({k: v for k, v in doc.items()
                                if k != "artifact_sha256"})).hexdigest() != expect:
        raise ValueError("iid_voyage_group_map 自哈希不一致")
    return dict(doc["mapping"])


if __name__ == "__main__":
    import sys
    sys.exit(main())

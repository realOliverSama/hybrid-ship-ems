#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_pipeline.py — Dataset v3 pilot E2E 编排（T-015，FR-489/AC-426）.

链路：voyage artifacts（70 号 .mat）→ 1 Hz channel-aware 聚合 → 窗口化（vintage 对齐）
→ target/E300 → split（deterministic group-aware）→ 三级泄漏审计 + OOD 机制检查
→ train-only 归一化 → manifest（deterministic payload hash）→ 机器可读 gate 结果。

纪律：
- 全部白名单/聚合规则经 contracts 消费 registry（FR-465，无 hard-coded 清单）；
- pilot 仅证明管线正确性，统计不得晋升科学基线（FR-489）；全部产物标 PILOT；
- 归一化仅在 train split 拟合（FR-483）；manifest 经 manifest.validate_manifest；
- n_invalid_kf > 0 ⇒ FAIL（DGN-07 阈值 0）；L1 任一触发 ⇒ FAIL（DGN-10 Hard FAIL）；
- 禁绝对路径（全部 __file__ 推导）；stage=pilot 时 voyage fixture_kind 必须 PILOT_ONLY。

用法（MODELE_ROOT 下）：
  python -m dataset_v3.run_pipeline --voyage-dir datasets_v3/pilot \
      --out-dir results/dataset_v3/pilot --npz datasets_v3/pilot/windows_v3_pilot.npz
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import platform
import sys

import numpy as np
from scipy.io import loadmat

from . import contracts
from .aggregate import aggregate_stream, zero_fill_ctrl_channels
from .audit_leakage import run_l1, run_l2, run_l3, window_content_hash
from .build_dataset_v3 import build_views, assert_no_quantile_labels
from .manifest import build_manifest, validate_manifest
from .split import assign_splits, compute_artifact_hash, verify_partition, write_artifact
from .targets import crosscheck_vs_aggregate, e300_of, y_trajectory
from .validate_contracts import (REGISTRY_PATH, PRODUCER_SCHEMA_PATH,
                                 validate_forecast_provenance)
from .windowing import build_windows

SPLIT_SEED_PILOT = "pilot-split-seed-v1"   # PILOT_ONLY：管线合格化参数，非科学基线
HISTORY_BINS = 300
FUTURE_BINS = 30
STRIDE_S = 10.0


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def load_voyage(path: str) -> dict:
    """读取 70 号 voyage artifact（v7 .mat：stream/kf/meta 三 struct）。"""
    m = loadmat(path, simplify_cells=True)
    for key in ("stream", "kf", "meta"):
        if key not in m:
            raise ValueError(f"{os.path.basename(path)} 缺 {key} 表")
    return {"stream": m["stream"], "kf": m["kf"], "meta": m["meta"]}


def build_pilot_dataset(voyage_dir: str, out_dir: str, npz_path: str) -> dict:
    """全链构建，返回机器可读 gate 结果 dict（overall=true/false）。"""
    checks = []

    def add_check(name, passed, detail=""):
        checks.append({"name": name, "pass": bool(passed), "detail": detail})
        print(f"[pipeline] {'PASS' if passed else 'FAIL'} {name} {detail}")
        return bool(passed)

    reg = contracts.load_registry(validate=True)
    hist_cols = contracts.signal_names(reg, "INPUT_H1") + contracts.signal_names(reg, "INPUT_H2")
    kf_cols = contracts.signal_names(reg, "INPUT_K") + contracts.signal_names(reg, "INPUT_F")
    agg_map_hist = {c: contracts.aggregation_map(reg)[c] for c in hist_cols}
    tgt_src = contracts.signal_names(reg, "TARGET")[0]   # registry TARGET role 驱动（R3-m7）
    tgt_agg = contracts.aggregation_map(reg)[tgt_src]

    files = sorted(f for f in os.listdir(voyage_dir) if f.endswith(".mat"))
    if not files:
        raise SystemExit(f"voyage 目录无 .mat: {voyage_dir}")

    voyages = {}
    for fn in files:
        vid = fn[:-4]
        v = load_voyage(os.path.join(voyage_dir, fn))
        meta = v["meta"]
        if str(meta.get("fixture_kind", "")) != "PILOT_ONLY":
            add_check(f"fixture_kind[{vid}]", False, "pilot 航次必须标 PILOT_ONLY")
            continue
        prov = meta.get("forecast_provenance", {})
        prov_errs = validate_forecast_provenance(
            {"forecast_generator_id": str(prov.get("forecast_generator_id", "")),
             "forecast_generator_version": str(prov.get("forecast_generator_version", "")),
             "forecast_generator_hash": str(prov.get("forecast_generator_hash", "")),
             "issue_time_s": float(prov.get("issue_time_s", 0.0)),
             "source_cutoff_time_s": float(prov.get("source_cutoff_time_s", 0.0)),
             "forecast_provenance_class": str(prov.get("forecast_provenance_class", ""))},
            stage="pilot")
        if prov_errs:
            add_check(f"provenance[{vid}]", False, "; ".join(prov_errs))
            continue
        voyages[vid] = v
    add_check("voyage_fixture_and_provenance", len(voyages) > 0,
              f"{len(voyages)} 航次通过 PILOT_ONLY/provenance 门")

    # ---- 逐航次：1 Hz 聚合 → 窗口化 → target ----
    win_X_H, win_X_KF, win_Y, win_E300 = [], [], [], []
    win_meta = []           # voyage_id / anchor_t0_s / window_index_in_voyage
    win_hashes = {}         # 窗口 content hash（L1）
    l2_series = {}          # 1 Hz time_mean P_load（L2/L3 表征）
    provenance = {}
    total_invalid_kf = 0
    xcheck_max = 0.0
    for vid in sorted(voyages):
        v = voyages[vid]
        st, kf, meta = v["stream"], v["kf"], v["meta"]
        t = np.asarray(st["time_s"], dtype=float).ravel()
        cols = {c: np.asarray(st[c], dtype=float).ravel() for c in hist_cols if c in st}
        cols[tgt_src] = np.asarray(st[tgt_src], dtype=float).ravel()
        missing = [c for c in hist_cols if c not in cols]
        if missing:
            add_check(f"stream_cols[{vid}]", False, f"缺列 {missing}")
            continue
        # 1 Hz 聚合（右端点时间基：bin (e_i, e_{i+1}] 的聚合值置于 e_{i+1}）；
        # edges 取 floor(t_end)——每个 bin 完整落在数据覆盖内（禁静默外推）
        edges = np.arange(0.0, float(np.floor(t[-1])) + 1.0, 1.0)
        agg = aggregate_stream(t, cols, edges, {**agg_map_hist, tgt_src: tgt_agg})
        t_1hz = edges[1:]
        hist = zero_fill_ctrl_channels({k: agg[k] for k in hist_cols if k in agg})
        # target（只消费 native stream）
        p = cols[tgt_src]
        kf_in = {"issue_time_s": np.asarray(kf["issue_time_s"], dtype=float).ravel(),
                 "lead_s": np.asarray(kf["lead_s"], dtype=float)}
        for c in kf_cols:
            kf_in[c] = np.asarray(kf[c], dtype=float)
        wins = build_windows(t_1hz, hist, kf_in, stride_s=STRIDE_S)
        total_invalid_kf += int(wins["n_invalid_kf"])
        for i, t0 in enumerate(wins["anchors"]):
            y = y_trajectory(t, p, float(t0))
            win_X_H.append({c: wins["X_H"][c][i] for c in hist_cols})
            win_X_KF.append({c: wins["X_KF"][c][i] for c in kf_cols})
            win_Y.append(y)
            win_E300.append(e300_of(y))
            win_meta.append({"voyage_id": vid, "anchor_t0_s": float(t0),
                             "window_index_in_voyage": int(wins["window_index"][i]),
                             "split_group_id": str(meta["split_group_id"]),
                             "scenario_stratum": str(meta["scenario_stratum"])})
            win_hashes[f"{vid}#{i}"] = window_content_hash(
                np.stack([wins["X_H"][c][i] for c in hist_cols], axis=1),
                np.stack([wins["X_KF"][c][i] for c in kf_cols], axis=1), y)
        # 聚合-target 交叉验证（同定义等价，1e-9）
        if wins["anchors"].size:
            xc = crosscheck_vs_aggregate(t, p, float(wins["anchors"][0]),
                                         np.arange(float(wins["anchors"][0]) - 300.0,
                                                   float(wins["anchors"][0]) + 301.0, 1.0))
            xcheck_max = max(xcheck_max, abs(xc))
        l2_series[vid] = hist[tgt_src]
        provenance[vid] = {"generation_seed": str(meta.get("generation_seed", "")),
                           "route_template_id": str(meta.get("route_template_id", ""))}
        print(f"[pipeline] {vid}: windows={wins['anchors'].size} dur={t[-1]:.0f}s")

    add_check("energy_crosscheck_1e-9", xcheck_max < 1e-9, f"max_rel={xcheck_max:.3e}")
    add_check("kf_vintage_zero_invalid", total_invalid_kf == 0,
              f"invalid={total_invalid_kf}（阈值 0，DGN-07）")

    if not win_Y:
        return {"schema": "dataset_v3_pilot_gate_result_v1", "stage": "PILOT",
                "overall": False, "checks": checks,
                "reason": "零有效窗口"}

    # ---- split（deterministic group-aware；覆盖全部航次含零窗航次）----
    voyage_grp = {vid: str(v["meta"]["split_group_id"]) for vid, v in voyages.items()}
    voyage_stratum = {vid: str(v["meta"]["scenario_stratum"]) for vid, v in voyages.items()}
    groups = sorted(voyage_grp.values())
    grp_stratum = {}
    for vid in sorted(voyages):
        g = voyage_grp[vid]
        s = voyage_stratum[vid]
        if grp_stratum.setdefault(g, s) != s:
            add_check("strata_unique", False, f"group {g} 映射多 stratum")
    strata: dict = {}
    for g in groups:
        strata.setdefault(grp_stratum[g], []).append(g)
    assignment = assign_splits(groups, strata, SPLIT_SEED_PILOT)
    part_errs = verify_partition(assignment)
    add_check("split_partition_invariants", not part_errs, "; ".join(part_errs[:3]))
    grp_split = {g: rec["split"] for g, rec in assignment["assignment"].items()}
    voyage_split = {vid: grp_split[voyage_grp[vid]] for vid in voyages}
    for m in win_meta:
        m["split"] = grp_split[m["split_group_id"]]
    os.makedirs(out_dir, exist_ok=True)
    art_path = os.path.join(out_dir, "split_assignment_v3.json")
    write_artifact(assignment, art_path)
    art_sha = compute_artifact_hash(assignment)

    # ---- 三级泄漏审计 ----
    artifact_hashes = {vid: sha256_file(os.path.join(voyage_dir, vid + ".mat"))
                       for vid in voyages}
    l1 = run_l1({f"{g}#voyage": grp_split[g] for g in groups}, artifact_hashes, win_hashes)
    add_check("L1_hard_fail_zero", l1["overall_l1"], json.dumps(l1["violations"][:3], ensure_ascii=False))
    l2 = run_l2(l2_series, provenance)
    l3 = run_l3(l2_series, voyage_split, None, provenance=provenance,
                require_train_stats=False)
    add_check("L2_provenance_gated", True,
              f"candidates={l2['counts']['l2_candidate']} "
              f"confirmed={l2['counts']['CONFIRMED_DUPLICATE']}（禁自动剔除，report-only）")
    add_check("L3_sbd_suspect_flag", True,
              f"suspects={l3['n_suspects']}（similarity≠leakage，永不 FAIL）")

    # ---- 组装 views + train-only 归一化 ----
    import numpy as _np
    Y = _np.stack(win_Y).astype(np.float64)
    E300 = _np.asarray(win_E300, dtype=np.float64)
    X_H = {c: _np.stack([w[c] for w in win_X_H]) for c in hist_cols}
    X_KF = {c: _np.stack([w[c] for w in win_X_KF]) for c in kf_cols}
    views = build_views(X_H, X_KF, Y, E300, reg)
    assert_no_quantile_labels(views)
    splits = _np.array([m["split"] for m in win_meta])
    tr = splits == "train"
    if not tr.any():
        add_check("train_nonempty", False)
    else:
        add_check("train_nonempty", True, f"train windows={int(tr.sum())}")
    # train-only 归一化（FR-483）；pilot 常量物理通道（ctrl_mode/water_depth/v_ref 等
    # 在 pilot 场景族内为常量）train std=0 → 记录并以 1.0 恒等变换（机制级；
    # pilot 统计非科学基线，FR-489）；目标 y/E300 必须非零方差
    norm = {"fit_on": "train_only", "constant_channels_transform_std_1": []}
    for c in views["h1_view"]:
        sd = X_H[c][tr].std(axis=0)
        const_ch = int((sd <= 0).sum())
        if const_ch:
            norm["constant_channels_transform_std_1"].append({"col": c, "n_bins": const_ch})
        if sd.size and not (sd > 0).any():   # 整通道零方差才 FAIL（R3-m1：原 is False 为死分支）
            add_check("norm_std_positive", False, f"{c} 全通道零方差（train）")
    norm.update({
        "x_h1_mean": {c: X_H[c][tr].mean(axis=0).tolist() for c in views["h1_view"]},
        "x_h1_std": {c: np.where(X_H[c][tr].std(axis=0) > 0, X_H[c][tr].std(axis=0), 1.0).tolist()
                     for c in views["h1_view"]},
        "y_mean": Y[tr].mean(axis=0).tolist(), "y_std": Y[tr].std(axis=0).tolist(),
        "e300_mean": float(E300[tr].mean()), "e300_std": float(E300[tr].std())})
    if Y[tr].std() <= 0 or float(E300[tr].std()) <= 0:
        add_check("target_std_positive", False, "train 目标零方差")
    else:
        add_check("target_std_positive", True)
    add_check("normalization_fit_train_only", True, "val/test/OOD 仅 transform（FR-483）")

    # ---- npz 落盘（本地不入库）----
    os.makedirs(os.path.dirname(npz_path) or ".", exist_ok=True)
    arr = {"Y": Y.astype(np.float32), "E300": E300.astype(np.float32),
           "splits": splits.astype(str), "voyage_id": _np.array([m["voyage_id"] for m in win_meta]),
           "anchor_t0_s": _np.array([m["anchor_t0_s"] for m in win_meta], dtype=np.float64),
           "window_index_in_voyage": _np.array([m["window_index_in_voyage"] for m in win_meta], dtype=np.int32)}
    for c in views["h1_view"]:
        arr[f"X_H1::{c}"] = views["h1_view"][c].astype(np.float32)
    for c in views["h2_view"]:
        if c not in views["h1_view"]:
            arr[f"X_H2::{c}"] = views["h2_view"][c].astype(np.float32)
    for c in kf_cols:
        arr[f"X_KF::{c}"] = X_KF[c].astype(np.float32)
    _np.savez_compressed(npz_path, **arr)
    norm_path = os.path.join(out_dir, "pilot_norm_v3.json")
    with open(norm_path, "w", encoding="utf-8") as f:
        json.dump(norm, f, ensure_ascii=False, sort_keys=True, indent=2)

    # ---- manifest（FR-488 + DGN-12，payload/run 分离）----
    counts = {s: int((splits == s).sum()) for s in ("train", "val", "test")}
    payload = {
        "split_policy_version": "dgn09-v1", "split_seed": SPLIT_SEED_PILOT,
        "split_group_key": "voyage_id（pilot）", "split_assignment_artifact_sha256": art_sha,
        "train_val_test_voyage_counts": {
            s: int(sum(1 for g in groups if grp_split[g] == s)) for s in ("train", "val", "test")},
        "normalization_source": "train_only",
        "preprocessing_artifact_sha256": sha256_file(norm_path),
        "ood_definition_version": "DGN-11-v1", "ood_generation_seed": "UNDEFINED_IN_PILOT",
        "provenance_duplicate_count": int(l2["counts"]["CONFIRMED_DUPLICATE"]),
        "exact_duplicate_count": int(sum(len(v.get("windows", [])) - 1
                                         for v in l1["violations"]
                                         if v.get("type") == "duplicate_window_content")),
        "sbd_threshold_policy": "T_SBD=Q0.01(train-vs-train NN SBD)（policy 冻结，pilot 数值非基线）",
        "sbd_suspect_count_disposition": f"{l3['n_suspects']}:unresolved",
        "test_sealed": True, "ood_sealed": True,
        "dataset_anchor_stride_s": 10, "predictor_runtime_period_s": "UNDEFINED_IN_D",
        "window_geometry": {"L_H": 300, "H_F": 300, "K": 30, "delta_F": 10, "stride": 10},
        "registry_sha256": json.loads(open(REGISTRY_PATH, encoding="utf-8").read())["self_hash"]["value"],
        "producer_schema_sha256": json.loads(open(PRODUCER_SCHEMA_PATH, encoding="utf-8").read())["self_hash"]["value"],
        "aggregation_table_version": "dgn05-r1", "target_definition": "y_k=E_k/10 kW（native trapezoidal 能量差分）；E300=ΣE_k/3600 kWh",
        "stage": "PILOT", "fixture_policy": "PILOT_ONLY 禁入 formal（FR-500）",
    }
    run_metadata = {"build_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
                    "python": sys.version.split()[0], "numpy": np.__version__,
                    "platform": platform.platform(), "pipeline": "dataset_v3.run_pipeline",
                    "generation_script_path": "dataset_v3/run_pipeline.py",
                    "generation_script_sha256": sha256_file(os.path.abspath(__file__)),
                    "matlab_version": "R2026a（70 号 producer 侧记录）",
                    "python_version": sys.version.split()[0]}
    man = build_manifest(payload, run_metadata)
    man_errs = validate_manifest(man)
    add_check("manifest_validate", not man_errs, "; ".join(man_errs[:3]))
    man_path = os.path.join(out_dir, "dataset_v3_manifest.json")
    with open(man_path, "w", encoding="utf-8") as f:
        json.dump(man, f, ensure_ascii=False, indent=2)

    overall = all(c["pass"] for c in checks)
    return {"schema": "dataset_v3_pilot_gate_result_v1", "stage": "PILOT",
            "generated_utc": run_metadata["build_utc"], "overall": overall,
            "n_windows": int(len(Y)), "window_counts": counts, "n_voyages": len(voyages),
            "audits": {"L1": l1, "L2": l2, "L3": l3},
            "outputs": {"npz": npz_path, "manifest": man_path, "split_artifact": art_path,
                        "norm": norm_path},
            "checks": checks}


def main(argv=None) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser(description="Dataset v3 pilot E2E（T-015）")
    ap.add_argument("--voyage-dir", default=os.path.join("datasets_v3", "pilot"))
    ap.add_argument("--out-dir", default=os.path.join("results", "dataset_v3", "pilot"))
    ap.add_argument("--npz", default=os.path.join("datasets_v3", "pilot", "windows_v3_pilot.npz"))
    ap.add_argument("--result", default=os.path.join("results", "dataset_v3", "pilot",
                                                     "pilot_gate_result.json"))
    args = ap.parse_args(argv)
    vd = args.voyage_dir if os.path.isabs(args.voyage_dir) else os.path.join(root, args.voyage_dir)
    od = args.out_dir if os.path.isabs(args.out_dir) else os.path.join(root, args.out_dir)
    nz = args.npz if os.path.isabs(args.npz) else os.path.join(root, args.npz)
    rs = args.result if os.path.isabs(args.result) else os.path.join(root, args.result)
    res = build_pilot_dataset(vd, od, nz)
    os.makedirs(os.path.dirname(rs), exist_ok=True)
    with open(rs, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=2)
    print(f"[pipeline] pilot_gate_result: overall={res['overall']} windows={res.get('n_windows')}")
    return 0 if res["overall"] else 1


if __name__ == "__main__":
    sys.exit(main())

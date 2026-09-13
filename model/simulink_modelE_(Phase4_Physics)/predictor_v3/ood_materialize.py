#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ood_materialize.py — OOD 窗口物化薄驱动（T-003，DGN-E-03，SEALED_PREPROCESS_ONLY）。

纪律（tasks gate R1：T-018 前唯一获准的 OOD 预处理例外）：
- 只读调用冻结 dataset_v3 管线实现（load_voyage → aggregate → build_windows →
  targets → build_views 白名单），禁复制/改写冻结逻辑；
- 只读消费 datasets_v3/formal/ood/ 的 200 条 raw 航次，不写 datasets_v3/ 任何字节；
- 产物写 E-owned 本地命名空间 artifacts/predictor_v3/data/（不入库）+
  integrity JSON（results/predictor_v3/，入库）；
- **只产 schema/计数/hash/完整性元数据——禁产科学分布/target·输入摘要/预测指标/
  图/样本值**；产物保持 sealed，T-018 前不得经正常 DatasetView/模型/评估器 API 暴露；
- realized exclusive 映射来自 formal_freeze_result.json.ood_evidence.voyage_evidence；
  映射缺失任一航次即 FAIL；不写 splits 列（OOD 无 train/val/test 语义）；
- 双 hash：npz 文件 sha256（存储/完整性）+ canonical scientific payload sha256
  （确定性以 payload hash 一致为门，文件 sha256 逐位一致仅另报）。

偏差登记（design.md 偏差区同步）：npz 在 DGN-E-03 标签清单之外附加
``split_group_id`` 元数据列——FR-577 per-condition split_group 计数与 FR-602/
DGN-E-12 two-sample cluster bootstrap（重采样单位 = split_group）的必需元数据；
非科学分布值，不泄露窗口内容。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import sys
import zipfile
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from dataset_v3 import contracts                                    # noqa: E402
from dataset_v3.aggregate import aggregate_stream, zero_fill_ctrl_channels  # noqa: E402
from dataset_v3.build_dataset_v3 import assert_no_quantile_labels, build_views  # noqa: E402
from dataset_v3.run_formal_freeze import formal_voyage_gate, provenance_record  # noqa: E402
from dataset_v3.run_pipeline import (STRIDE_S, load_voyage,          # noqa: E402
                                     sha256_file)
from dataset_v3.targets import crosscheck_vs_aggregate, e300_of, y_trajectory  # noqa: E402
from dataset_v3.windowing import build_windows                      # noqa: E402

FREEZE_RESULT_PATH = os.path.join(
    MODELE_ROOT, "results", "dataset_v3", "formal", "formal_freeze_result.json")
OOD_DIR = os.path.join(MODELE_ROOT, "datasets_v3", "formal", "ood")
OUT_NPZ = os.path.join(MODELE_ROOT, "artifacts", "predictor_v3", "data",
                       "ood_windows_v3_formal.npz")
OUT_INTEGRITY = os.path.join(MODELE_ROOT, "results", "predictor_v3",
                             "ood_windows_integrity.json")

_EXPECTED_CONDITIONS = {"A1": (36, 9), "B1": (40, 10), "C1": (44, 11),
                        "C2": (40, 10), "C3": (40, 10)}  # FR-577 (voyages, groups)


# ---------------------------------------------------------------- hash 工具

def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def scientific_payload_hash(npz_path: str) -> str:
    """canonical scientific payload sha256：逐 array（解压后 .npy 原始字节）
    的 sha256 + name/dtype/shape 描述的 canonical JSON 再 hash（DGN-E-03 R1）。

    确定性门：同输入两跑 payload hash 必须一致；zip 层时间戳差异不影响本 hash。
    """
    import io

    import numpy as _np
    entries = []
    with zipfile.ZipFile(npz_path, "r") as zf:
        for name in sorted(zf.namelist()):
            if not name.endswith(".npy"):
                continue
            raw = zf.read(name)  # zipfile 透明解压 → 原始 .npy 字节
            fh = io.BytesIO(raw)
            ver = _np.lib.format.read_magic(fh)
            if ver == (1, 0):
                shape, fortran, dtype = _np.lib.format.read_array_header_1_0(fh)
            elif ver == (2, 0):
                shape, fortran, dtype = _np.lib.format.read_array_header_2_0(fh)
            else:
                raise ValueError(f"未知 npy 版本 {ver}: {name}")
            entries.append({
                "name": name[:-len(".npy")],
                "dtype": str(dtype),
                "shape": list(shape),
                "fortran_order": bool(fortran),
                "npy_sha256": hashlib.sha256(raw).hexdigest(),
            })
    return hashlib.sha256(canonical_json(entries)).hexdigest()


def dataset_v3_code_hash() -> str:
    """冻结 dataset_v3 包代码 hash（文件集 canonical hash，排除 __pycache__）。"""
    pkg_dir = os.path.join(MODELE_ROOT, "dataset_v3")
    files = {}
    for root, dirs, names in os.walk(pkg_dir):
        dirs[:] = [d for d in dirs if d != "__pycache__"]
        for n in sorted(names):
            if not n.endswith((".py",)):
                continue
            p = os.path.join(root, n)
            files[os.path.relpath(p, pkg_dir)] = sha256_file(p)
    return hashlib.sha256(canonical_json(files)).hexdigest()


# ---------------------------------------------------------------- 映射与门

def load_realized_mapping(freeze_result_path: str = FREEZE_RESULT_PATH) -> dict:
    """voyage_id → ood_condition_id（realized exclusive；映射缺失/非 OOD 即 FAIL）。"""
    with open(freeze_result_path, encoding="utf-8") as fh:
        freeze = json.load(fh)
    evidence = freeze["ood_evidence"]["voyage_evidence"]
    mapping = {}
    for rec in evidence:
        vid = str(rec["voyage_id"])
        if rec.get("verdict") != "OOD" or rec.get("dataset_domain") != "OOD":
            raise ValueError(f"voyage_evidence 非 realized OOD: {vid}")
        if rec.get("ood_condition_id") not in _EXPECTED_CONDITIONS:
            raise ValueError(f"voyage {vid} realized 条目非法: "
                             f"{rec.get('ood_condition_id')}")
        if vid in mapping:
            raise ValueError(f"voyage_evidence 重复航次: {vid}")
        mapping[vid] = str(rec["ood_condition_id"])
    return mapping


# ---------------------------------------------------------------- 主物化

def materialize_ood(ood_dir: str = OOD_DIR, out_npz: str = OUT_NPZ,
                    freeze_result_path: str = FREEZE_RESULT_PATH,
                    *, log=print) -> dict:
    """单次物化：读 raw OOD 航次 → 冻结管线 → 写 OOD npz；返回统计+hash 摘要。"""
    mapping = load_realized_mapping(freeze_result_path)

    reg = contracts.load_registry(validate=True)
    hist_cols = (contracts.signal_names(reg, "INPUT_H1")
                 + contracts.signal_names(reg, "INPUT_H2"))
    kf_cols = (contracts.signal_names(reg, "INPUT_K")
               + contracts.signal_names(reg, "INPUT_F"))
    agg_map_hist = {c: contracts.aggregation_map(reg)[c] for c in hist_cols}
    tgt_src = contracts.signal_names(reg, "TARGET")[0]
    tgt_agg = contracts.aggregation_map(reg)[tgt_src]

    files = sorted(f for f in os.listdir(ood_dir) if f.endswith(".mat"))
    missing = [fn[:-4] for fn in map(os.path.basename, files)
               if fn[:-4] not in mapping]
    # 文件集与 realized 映射双向一致：多/少任一即 FAIL（DGN-E-03）
    extra = sorted(set(mapping) - {os.path.basename(f)[:-4] for f in files})
    if missing or extra:
        raise ValueError(
            f"realized 映射与语料文件集不一致: 缺映射={missing[:5]} 多余={extra[:5]}")

    win_X_H, win_X_KF, win_Y, win_E300, win_meta = [], [], [], [], []
    total_invalid_kf, xcheck_max = 0, 0.0
    cond_voyages: dict = {}
    for fn in files:
        vid = os.path.basename(fn)[:-4]
        v = load_voyage(os.path.join(ood_dir, fn))
        st, kf, meta = v["stream"], v["kf"], v["meta"]
        errs = formal_voyage_gate(meta, provenance_record(meta))
        if errs:
            raise ValueError(f"formal 门拒绝 {vid}: {'; '.join(errs)}")
        cond = mapping[vid]
        cond_voyages.setdefault(cond, set()).add(vid)

        t = np.asarray(st["time_s"], dtype=float).ravel()
        cols = {c: np.asarray(st[c], dtype=float).ravel()
                for c in hist_cols if c in st}
        cols[tgt_src] = np.asarray(st[tgt_src], dtype=float).ravel()
        missing_cols = [c for c in hist_cols if c not in cols]
        if missing_cols:
            raise ValueError(f"{vid} 缺 stream 列 {missing_cols}")
        edges = np.arange(0.0, float(np.floor(t[-1])) + 1.0, 1.0)
        agg = aggregate_stream(t, cols, edges,
                               {**agg_map_hist, tgt_src: tgt_agg})
        t_1hz = edges[1:]
        hist = zero_fill_ctrl_channels({k: agg[k] for k in hist_cols if k in agg})
        p = cols[tgt_src]
        kf_in = {"issue_time_s": np.asarray(kf["issue_time_s"], dtype=float).ravel(),
                 "lead_s": np.asarray(kf["lead_s"], dtype=float)}
        for c in kf_cols:
            kf_in[c] = np.asarray(kf[c], dtype=float)
        wins = build_windows(t_1hz, hist, kf_in, stride_s=STRIDE_S)
        total_invalid_kf += int(wins["n_invalid_kf"])
        grp = str(meta["split_group_id"])
        for i, t0 in enumerate(wins["anchors"]):
            y = y_trajectory(t, p, float(t0))
            win_X_H.append({c: wins["X_H"][c][i] for c in hist_cols})
            win_X_KF.append({c: wins["X_KF"][c][i] for c in kf_cols})
            win_Y.append(y)
            win_E300.append(e300_of(y))
            win_meta.append({
                "voyage_id": vid, "anchor_t0_s": float(t0),
                "window_index_in_voyage": int(wins["window_index"][i]),
                "ood_condition": cond, "split_group_id": grp})
        if wins["anchors"].size:
            xc = crosscheck_vs_aggregate(
                t, p, float(wins["anchors"][0]),
                np.arange(float(wins["anchors"][0]) - 300.0,
                          float(wins["anchors"][0]) + 301.0, 1.0))
            xcheck_max = max(xcheck_max, abs(xc))
        log(f"[ood_materialize] {vid} cond={cond} windows={wins['anchors'].size}")

    if total_invalid_kf != 0:
        raise ValueError(f"KF vintage invalid 窗口 {total_invalid_kf} != 0"
                         "（DGN-07 阈值 0，禁静默修补）")
    if xcheck_max >= 1e-9:
        raise ValueError(f"能量交叉验证 max_rel={xcheck_max:.3e} ≥ 1e-9")
    if not win_Y:
        raise ValueError("零有效窗口")

    Y = np.stack(win_Y).astype(np.float64)
    E300 = np.asarray(win_E300, dtype=np.float64)
    X_H = {c: np.stack([w[c] for w in win_X_H]) for c in hist_cols}
    X_KF = {c: np.stack([w[c] for w in win_X_KF]) for c in kf_cols}
    views = build_views(X_H, X_KF, Y, E300, reg)   # 白名单（含 E300 恒等 1e-12）
    assert_no_quantile_labels(views)

    arr = {
        "Y": Y.astype(np.float32),
        "E300": E300.astype(np.float32),
        "voyage_id": np.array([m["voyage_id"] for m in win_meta]),
        "anchor_t0_s": np.array([m["anchor_t0_s"] for m in win_meta],
                                dtype=np.float64),
        "window_index_in_voyage": np.array(
            [m["window_index_in_voyage"] for m in win_meta], dtype=np.int32),
        "ood_condition": np.array([m["ood_condition"] for m in win_meta]),
        "split_group_id": np.array([m["split_group_id"] for m in win_meta]),
    }
    for c in views["h1_view"]:
        arr[f"X_H1::{c}"] = views["h1_view"][c].astype(np.float32)
    for c in views["h2_view"]:
        if c not in views["h1_view"]:
            arr[f"X_H2::{c}"] = views["h2_view"][c].astype(np.float32)
    for c in kf_cols:
        arr[f"X_KF::{c}"] = X_KF[c].astype(np.float32)
    # 注意：不写 splits 列（OOD 无 train/val/test 语义，DGN-E-03）

    os.makedirs(os.path.dirname(out_npz), exist_ok=True)
    np.savez_compressed(out_npz, **arr)

    per_condition = {}
    for cond, vids in sorted(cond_voyages.items()):
        n_win = int(sum(1 for m in win_meta if m["ood_condition"] == cond))
        n_grp = len({m["split_group_id"] for m in win_meta
                     if m["ood_condition"] == cond})
        per_condition[cond] = {"voyages": len(vids), "split_groups": n_grp,
                               "windows": n_win}
        if n_win == 0:
            raise ValueError(f"per-condition 窗口数为 0: {cond}（FAIL，DGN-E-03）")
    return {
        "n_windows": int(len(Y)),
        "n_voyages": len(files),
        "per_condition": per_condition,
        "npz_path": out_npz,
    }


# ---------------------------------------------------------------- integrity

def _corpus_manifest_hash(ood_dir: str) -> tuple:
    files = sorted(f for f in os.listdir(ood_dir) if f.endswith(".mat"))
    listing = [{"file": f, "sha256": sha256_file(os.path.join(ood_dir, f))}
               for f in files]
    return hashlib.sha256(canonical_json(listing)).hexdigest(), len(listing)


def build_integrity(stats: dict, cmd: str, second_payload_sha256: str,
                    freeze_result_path: str = FREEZE_RESULT_PATH,
                    ood_dir: str = OOD_DIR, out_npz: str = OUT_NPZ) -> dict:
    """integrity JSON（只含 schema/计数/hash/完整性元数据——SEALED_PREPROCESS_ONLY）。"""
    npz_path = stats["npz_path"]
    mapping = load_realized_mapping(freeze_result_path)
    map_canon = hashlib.sha256(canonical_json(mapping)).hexdigest()
    corpus_hash, corpus_n = _corpus_manifest_hash(ood_dir)
    doc = {
        "schema": "predictor_v3_ood_windows_integrity_v1",
        "task": "T-003",
        "discipline": "SEALED_PREPROCESS_ONLY",
        "utc": datetime.now(timezone.utc).isoformat(),
        "python": sys.version.split()[0],
        "numpy": np.__version__,
        "platform": platform.platform(),
        "npz": {
            "path": os.path.relpath(npz_path, MODELE_ROOT),
            "size_bytes": os.path.getsize(npz_path),
            "file_sha256": sha256_file(npz_path),
            "scientific_payload_sha256": scientific_payload_hash(npz_path),
        },
        "determinism": {
            "n_runs": 2,
            "run2_scientific_payload_sha256": second_payload_sha256,
            "payload_hash_equal": (
                scientific_payload_hash(npz_path) == second_payload_sha256),
            "note": "确定性门 = canonical payload hash 一致；文件 sha256 逐位一致"
                    "另报不作为门（zip 时间戳差异不构成科学不确定性）",
        },
        "counts": {
            "n_windows": stats["n_windows"],
            "n_voyages": stats["n_voyages"],
            "per_condition": stats["per_condition"],
            "expected_per_condition_voyages_groups": {
                k: {"voyages": v[0], "split_groups": v[1]}
                for k, v in _EXPECTED_CONDITIONS.items()},
        },
        "provenance": {
            "frozen_pipeline": "dataset_v3（只读调用，零改动）",
            "dataset_v3_code_hash": dataset_v3_code_hash(),
            "driver": "predictor_v3/ood_materialize.py",
            "driver_sha256": sha256_file(os.path.abspath(__file__)),
            "command": cmd,
            "freeze_result_sha256": sha256_file(freeze_result_path),
            "realized_mapping_sha256": map_canon,
            "ood_corpus_manifest_sha256": corpus_hash,
            "ood_corpus_n_files": corpus_n,
        },
        "sealed_note": "产物保持 sealed；T-018 campaign 授权前禁止科学性访问"
                       "（预测器推理/评估/分布诊断/指标/绘图/样本值查看）。",
    }
    doc["artifact_sha256"] = hashlib.sha256(
        canonical_json({k: v for k, v in doc.items()
                        if k != "artifact_sha256"})).hexdigest()
    return doc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="T-003 OOD 窗口物化（SEALED_PREPROCESS_ONLY）")
    ap.add_argument("--ood-dir", default=OOD_DIR)
    ap.add_argument("--out-npz", default=OUT_NPZ)
    ap.add_argument("--out-integrity", default=OUT_INTEGRITY)
    ap.add_argument("--freeze-result", default=FREEZE_RESULT_PATH)
    args = ap.parse_args(argv)

    cmd = ("python -m predictor_v3.ood_materialize --ood-dir "
           + os.path.relpath(args.ood_dir, MODELE_ROOT)
           + " --out-npz " + os.path.relpath(args.out_npz, MODELE_ROOT))
    log = lambda m: print(m, flush=True)
    stats = materialize_ood(args.ood_dir, args.out_npz, args.freeze_result, log=log)
    log(f"[ood_materialize] run1 windows={stats['n_windows']} "
        f"payload={scientific_payload_hash(stats['npz_path'])[:16]}…")

    # 双跑确定性验证（run2 写独立临时 npz，比较后删除）
    tmp2 = args.out_npz + ".run2.npz"
    stats2 = materialize_ood(args.ood_dir, tmp2, args.freeze_result, log=lambda m: None)
    payload2 = scientific_payload_hash(tmp2)
    os.remove(tmp2)
    if stats2["n_windows"] != stats["n_windows"] or \
            stats2["per_condition"] != stats["per_condition"]:
        raise ValueError("双跑计数不一致")
    log(f"[ood_materialize] run2 payload={payload2[:16]}… "
        f"equal={payload2 == scientific_payload_hash(stats['npz_path'])}")

    integrity = build_integrity(stats, cmd, payload2, args.freeze_result,
                                args.ood_dir, args.out_npz)
    os.makedirs(os.path.dirname(args.out_integrity), exist_ok=True)
    with open(args.out_integrity, "w", encoding="utf-8") as fh:
        json.dump(integrity, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    if not integrity["determinism"]["payload_hash_equal"]:
        log("[ood_materialize] FAIL: 双跑 payload hash 不一致")
        return 1
    log(f"[ood_materialize] integrity → {args.out_integrity}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

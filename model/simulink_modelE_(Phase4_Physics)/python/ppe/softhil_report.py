#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""softhil_report.py — phase4-g soft-HIL 配对分析与报告（T-009）。

输入（全部只读）：
  - results/ppe_practical/soft_hil/<voyage>/C2.json     板后端 run 记录（本 phase 新增）
  - results/ppe_practical/formal_epoch3_genB/<v>/C2.json 冻结 host-keras 基线（配对）
  - results/ppe_practical/formal_epoch3_genB/<v>/P0.json 冻结规则基线（上下文，可选）
  - artifacts/ppe_softhil/<token>/board_queries.jsonl    板查询全量日志
    （hist/kf 原始字节 + 板 q50；供 keras fp32 逐锚点重放对拍）

输出：
  - results/ppe_practical/soft_hil/_analysis/softhil_report.json
  - results/ppe_practical/soft_hil/_analysis/softhil_summary.md

口径：
  - 配对差 = soft-HIL C2-board − 冻结 C2-host（同 voyage 同 arm 同冻结链，
    唯一差异 = 推理后端 int8-board vs keras-fp32-host）；
  - q50 逐锚点重放：keras fp32（C2Predictor 冻结身份）对日志窗推理，
    与板 q50 逐锚点 RMSE/MAE（需 .venv_tf，--no-replay 可跳过）；
  - 全部描述性呈现（非正式证据；不作 formality 声明）。

用法：
    .venv_tf/bin/python -m ppe.softhil_report [--token TOKEN] [--no-replay]
"""
from __future__ import annotations

import argparse
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

from ppe import paths  # noqa: E402
from ppe.causality import REQUIRED_F, REQUIRED_H1, REQUIRED_K  # noqa: E402

SOFT_HIL = os.path.join(paths.RESULTS_DIR, "soft_hil")
GENB = os.path.join(paths.RESULTS_DIR, "formal_epoch3_genB")
OUT_DIR = os.path.join(SOFT_HIL, "_analysis")
KF_ORDER = REQUIRED_K + REQUIRED_F


def load(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def pair_analysis(voyages):
    rows = []
    for vid in voyages:
        sh = load(os.path.join(SOFT_HIL, vid, "C2.json"))
        c2 = load(os.path.join(GENB, vid, "C2.json"))
        p0_path = os.path.join(GENB, vid, "P0.json")
        row = {
            "voyage_id": vid,
            "fuel_board_kg": sh["fuel_raw_kg"],
            "fuel_host_kg": c2["fuel_raw_kg"],
            "d_fuel_kg": sh["fuel_raw_kg"] - c2["fuel_raw_kg"],
            "d_fuel_pct": 100.0 * (sh["fuel_raw_kg"] - c2["fuel_raw_kg"])
                          / c2["fuel_raw_kg"],
            "SOC_end_board": sh["SOC_end"],
            "SOC_end_host": c2["SOC_end"],
            "d_SOC_end": sh["SOC_end"] - c2["SOC_end"],
            "J_FEC_board_kg": sh["J_FEC_kg"],
            "J_FEC_host_kg": c2["J_FEC_kg"],
            "d_J_FEC_kg": sh["J_FEC_kg"] - c2["J_FEC_kg"],
            "n_segments_board": sh.get("n_segments"),
            "n_segments_host": c2.get("n_segments"),
            "freeze_root_match": (sh.get("freeze_root_sha256")
                                  == c2.get("freeze_root_sha256")),
            "slx_match": (sh.get("slx_sha256") == c2.get("slx_sha256")),
        }
        if os.path.isfile(p0_path):
            p0 = load(p0_path)
            row["fuel_p0_kg"] = p0["fuel_raw_kg"]
            row["board_vs_p0_kg"] = sh["fuel_raw_kg"] - p0["fuel_raw_kg"]
            row["host_vs_p0_kg"] = c2["fuel_raw_kg"] - p0["fuel_raw_kg"]
        rows.append(row)
    return rows


def replay_q50(token):
    """keras fp32 重放日志窗（需 TF）→ 逐锚点 board vs fp32 统计。"""
    from ppe.c2_inference import C2Predictor
    pred = C2Predictor.instance()
    qlp = os.path.join(paths.MODELE_ROOT, "artifacts", "ppe_softhil", token,
                       "board_queries.jsonl")
    stats = {"n_anchors": 0, "rmse_kW": [], "mae_kW": [], "infer_us": [],
             "roundtrip_ms": []}
    per_anchor = []
    with open(qlp, encoding="utf-8") as fh:
        for line in fh:
            e = json.loads(line)
            if e.get("backend", "").startswith("mock"):
                continue  # MOCK 行不作板级证据
            hist = np.frombuffer(bytes.fromhex(e["hist_raw_hex"]),
                                 dtype="<f4").reshape(300, 11).astype(float)
            kf = np.frombuffer(bytes.fromhex(e["kf_raw_hex"]),
                               dtype="<f4").reshape(30, 6).astype(float)
            hist_d = {c: hist[:, j] for j, c in enumerate(REQUIRED_H1)}
            kf_d = {c: kf[:, j] for j, c in enumerate(KF_ORDER)}
            q50_fp32 = pred.predict_q50(hist_d, kf_d)
            q50_board = np.asarray(e["q50"], dtype=float)
            d = np.abs(q50_board - q50_fp32)
            stats["n_anchors"] += 1
            stats["rmse_kW"].append(float(np.sqrt((d ** 2).mean())))
            stats["mae_kW"].append(float(d.mean()))
            stats["infer_us"].append(int(e["board_infer_us"]))
            stats["roundtrip_ms"].append(float(e["roundtrip_ms"]))
            per_anchor.append({"voyage_id": e["voyage_id"], "t0": e["t0"],
                               "mae_kW": float(d.mean()),
                               "rmse_kW": float(np.sqrt((d ** 2).mean()))})
    agg = {}
    for k in ("rmse_kW", "mae_kW", "infer_us", "roundtrip_ms"):
        a = np.asarray(stats[k], dtype=float)
        agg[k] = {"mean": float(a.mean()) if a.size else None,
                  "p50": float(np.percentile(a, 50)) if a.size else None,
                  "p99": float(np.percentile(a, 99)) if a.size else None,
                  "max": float(a.max()) if a.size else None}
    return {"n_anchors": stats["n_anchors"], "agg": agg}


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--token", default="")
    ap.add_argument("--no-replay", action="store_true")
    args = ap.parse_args(argv)

    voyages = sorted(d for d in os.listdir(SOFT_HIL)
                     if os.path.isdir(os.path.join(SOFT_HIL, d))
                     and not d.startswith("_"))
    if not voyages:
        print("无 soft-HIL run 记录（先跑 run_softhil_voyage）")
        return 1
    rows = pair_analysis(voyages)

    replay = None
    if not args.no_replay:
        token = args.token or _guess_token()
        if token:
            replay = replay_q50(token)

    report = {
        "schema": "phase4g_softhil_report_v1",
        "nonformal": True,
        "pairing": "soft-HIL C2-board − frozen genB C2-host (same voyage/arm/freeze chain)",
        "voyages": rows,
        "q50_replay": replay,
        "identity": {
            "freeze_root_match_all": all(r["freeze_root_match"] for r in rows),
            "slx_match_all": all(r["slx_match"] for r in rows),
        },
    }
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump(report, open(os.path.join(OUT_DIR, "softhil_report.json"), "w"),
              indent=2, ensure_ascii=False)

    lines = ["# phase4-g soft-HIL 配对摘要（非正式）", "",
             "基线：冻结 genB C2 host keras fp32；差 = board − host。", "",
             "| voyage | fuel_board | fuel_host | Δfuel kg | Δfuel % | ΔSOC_end | ΔJ_FEC |",
             "|---|---|---|---|---|---|---|"]
    for r in rows:
        lines.append(
            f"| {r['voyage_id']} | {r['fuel_board_kg']:.2f} | "
            f"{r['fuel_host_kg']:.2f} | {r['d_fuel_kg']:+.2f} | "
            f"{r['d_fuel_pct']:+.3f}% | {r['d_SOC_end']:+.5f} | "
            f"{r['d_J_FEC_kg']:+.2f} |")
    if replay:
        a = replay["agg"]
        lines += ["", f"q50 逐锚点（板 vs keras fp32 重放，n={replay['n_anchors']}）："
                  f" RMSE mean {a['rmse_kW']['mean']:.2f} kW / p99 {a['rmse_kW']['p99']:.2f}；"
                  f"板推理 {a['infer_us']['mean']:.0f} µs mean（p99 {a['infer_us']['p99']:.0f}）；"
                  f"往返 {a['roundtrip_ms']['mean']:.0f} ms mean。"]
    open(os.path.join(OUT_DIR, "softhil_summary.md"), "w",
         encoding="utf-8").write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0


def _guess_token():
    base = os.path.join(paths.MODELE_ROOT, "artifacts", "ppe_softhil")
    if not os.path.isdir(base):
        return ""
    toks = [d for d in sorted(os.listdir(base))
            if os.path.isfile(os.path.join(base, d, "board_queries.jsonl"))]
    return toks[-1] if toks else ""


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""remote_snapshot.py — 在远端执行：输出 E1/HPO 进度 REG 行（watch_e.sh 数据源）。"""
import json
import os
import subprocess

BASE = "/root/autodl-tmp/hybrid-ship-ems/model/modele_link/results/predictor_v3/hpo"
CAPS = {"mlpq": 4, "tcnq": 20, "transformer": 20, "tft": 18, "reatt": 20}


def main():
    for arch, cap in CAPS.items():
        path = os.path.join(BASE, f"{arch}_trials.jsonl")
        if not os.path.isfile(path):
            print(f"REG {arch} 0 {cap} 0 0 0 - -")
            continue
        rows = [json.loads(l) for l in open(path) if l.strip()]
        adjudicated = {r["trial_id"] for r in rows
                       if r["status"] == "INFRA_INTERRUPTED"
                       and "rerun allowed" in r.get("error", "")}
        elig = {r["trial_id"] for r in rows
                if r["status"] == "COMPLETED_ELIGIBLE"}
        real_fail = sum(1 for r in rows if r["status"] == "FAILED"
                        and r["trial_id"] not in adjudicated)
        infra = sum(1 for r in rows if r["status"] == "INFRA_INTERRUPTED"
                    and "rerun allowed" not in r.get("error", ""))
        done = len(elig | adjudicated)
        vb, bid = None, "-"
        for r in rows:
            if r["status"] == "COMPLETED_ELIGIBLE":
                v = r["val"]["pinball_sgmacro"]
                if vb is None or v < vb:
                    vb, bid = v, r["trial_id"]
        best = f"{vb:.5f}" if vb is not None else "-"
        print(f"REG {arch} {done} {cap} {len(elig)} {real_fail} {infra} {best} {bid}")

    out = subprocess.run(
        ["ps", "aux"], capture_output=True, text=True).stdout
    lanes = {}
    for line in out.splitlines():
        if "predictor_v3.hpo" in line and "--arch" in line:
            parts = line.split()
            try:
                a = parts[parts.index("--arch") + 1]
                f = parts[parts.index("--filter") + 1]
                lanes[f"{a}/{f}"] = lanes.get(f"{a}/{f}", 0) + 1
            except (ValueError, IndexError):
                pass
    print("LANES " + "; ".join(f"{k}x{v}" for k, v in sorted(lanes.items())) or "LANES idle")
    gpu = subprocess.run(
        ["nvidia-smi", "--query-gpu=index,utilization.gpu,memory.used",
         "--format=csv,noheader"], capture_output=True, text=True).stdout
    print("GPU " + gpu.replace("\n", " | ").strip())
    tail = ""
    for f in ("hpo_tft_a", "hpo_tft_b", "hpo_reatt_a", "hpo_reatt_b",
              "hpo_transformer_a"):
        p = f"/root/autodl-tmp/{f}.log"
        if os.path.isfile(p):
            for line in reversed(open(p, errors="ignore").read().splitlines()):
                if line.startswith("[hpo]"):
                    tail = line
                    break
            if tail:
                break
    print("TAIL " + (tail or "(no [hpo] line yet)"))


if __name__ == "__main__":
    main()

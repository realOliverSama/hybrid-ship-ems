#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""wc_test_client.py — workclf 板上类别一致率客户（CMD_WC_TEST 0x08 / RSP 0x09）。

用途（phase4-g NPU 线判别实验）：workclf = 本平台历史上**可正常执行**的简单
NPU 图（小 MLP 分类器，权重在 NOR 0x70400000）。REATT 类图（整图 NPU 形态、
拆分后 N2 段）在板上输出退化后，本工具用于判定：
  workclf 正常 ⇒ NPU/NOR/运行时集成可用，退化是 REATT 类图自身的问题（继承
                2026-09-03 TCN64 NO-GO 定性）；
  workclf 退化 ⇒ 共享集成（NOR 权重通路 / 缓存契约 / EC 运行时）在本 appli 里
                已坏，属可修根因。

协议（ems_proto.h）：CMD_WC_TEST 载荷 [seq u32][302×f32]（Pdec30 = f32[0::10]、
旁路 [v,a] = f32[300..301]，固件内取）；RSP_WC_TEST [seq u32][cls u8][3B][logits 7×f32][wc_us u32]。

输入：Host/reference/vectors_raw.csv（200×302 无表头，与 reference_int8_val200.csv
逐行对应）；参考：cls_norm 列（归一化口径，= 固件口径）。

用法（仓库根 .venv_tf）：
  python3 <本文件> --port /dev/cu.usbserial-1120 [--n 200]
产物：Host/results/reatt_board/wc_test_board.csv + wc_test_report.json
"""
from __future__ import annotations

import argparse
import csv
import importlib.util
import json
import os
import struct
import sys
import time

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CMD_WC_TEST, RSP_WC_TEST = 0x08, 0x09


def _load_client(path):
    spec = importlib.util.spec_from_file_location("reatt_board_client", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--n", type=int, default=200)
    ap.add_argument("--tools", default=SCRIPT_DIR)
    args = ap.parse_args()

    cli = _load_client(os.path.join(args.tools, "reatt_board_client.py"))
    ref_dir = os.path.join(cli.PROJ, "Host", "reference")
    res_dir = os.path.join(cli.PROJ, "Host", "results", "reatt_board")
    vec = np.loadtxt(os.path.join(ref_dir, "vectors_raw.csv"), delimiter=",", dtype=np.float32)
    refs = list(csv.DictReader(open(os.path.join(ref_dir, "reference_int8_val200.csv"))))
    n = min(args.n, vec.shape[0], len(refs))

    ser = cli.Board(args.port, args.baud)
    print("PING:", "OK" if ser.ping() else "FAIL")

    rows = []
    t_all = time.time()
    for i in range(n):
        seq = 0x7000 + i
        payload = struct.pack("<I", seq) + vec[i].astype("<f4").tobytes()
        ser.ser.write(cli.enc_frame(CMD_WC_TEST, payload))
        while True:
            t, p = ser._read_frame()
            if t == cli.NACK:
                print(f"[{i}] NACK err={p[1]:#x}")
                rows.append({"i": i, "cls": -1, "ref": int(refs[i]["cls_norm"]),
                             "match": False, "wc_us": 0})
                break
            if t == RSP_WC_TEST and len(p) == 4 + 4 + 7 * 4 + 4:
                rseq, = struct.unpack("<I", p[:4])
                if rseq != seq:
                    continue
                cls = p[4]
                logits = np.frombuffer(p[8:8 + 28], dtype="<f4")
                us, = struct.unpack("<I", p[8 + 28:8 + 32])
                r = int(refs[i]["cls_norm"])
                rows.append({"i": i, "cls": int(cls), "ref": r,
                             "match": bool(int(cls) == r), "wc_us": int(us),
                             "logit_max": float(logits.max())})
                break
    ser.close()

    acc = float(np.mean([r["match"] for r in rows]))
    us = np.array([r["wc_us"] for r in rows], dtype=float)
    report = {
        "schema": "phase4g_workclf_board_wctest_v1",
        "n": len(rows), "cls_match_rate": acc,
        "reference": "reference_int8_val200.csv cls_norm（固件口径）",
        "wc_us_median": float(np.median(us)), "wc_us_max": float(us.max()),
        "total_wall_s": time.time() - t_all,
        "verdict": ("NPU/NOR/运行时集成可用（workclf 正常）" if acc >= 0.9
                    else "workclf 亦退化 ⇒ 共享集成可疑"),
    }
    os.makedirs(res_dir, exist_ok=True)
    with open(os.path.join(res_dir, "wc_test_board.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    with open(os.path.join(res_dir, "wc_test_report.json"), "w") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())

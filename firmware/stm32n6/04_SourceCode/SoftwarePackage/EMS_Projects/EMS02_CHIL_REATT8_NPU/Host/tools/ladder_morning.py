#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ladder_morning.py — phase4-g 晨间判别实验一键执行（flash boot + 冷启动后跑）。

流程：PING → 发 1 个 REATT 窗（触发全部板上诊断）→ 收集并判决：
  RS-NOR   appli 读 NOR 权重三区校验和（期望 n2=5d079aec sf=6ff8c563 wc=f916d5f5）
  RS-SELF  NPU 平凡自检图（期望 out == in）
  RS-L1    kf → fut_proj+位置        （NPU 子图 vs ORT golden）
  RS-L2    单块 attention            （Q/K/V、QKᵀ、Softmax、AV）
  RS-L3    h_past → Tile             （头号嫌疑：30 lead 全同指纹的来源）
  RS-L4    fusion+decoder
  RS-F1    整图 --Ono-clone-dma 消融
  RS-N1/N2/T  拆分服务本体的健康指纹

判决树（脚本自动给出）：
  F1 PASS                                → clone_dma 即根因：服务 N2 重编译加 --Ono-clone-dma
  L1..L4 有 FAIL → 首个 FAIL 级=首个坏段 → 该段退回 CPU 形态，其余留 NPU
  L1..L4 全 PASS 且 F1 FAIL              → 深挖编译选项（第二轮消融）
  RS-SELF FAIL                           → NPU 通路回退（与已知证据矛盾，需复查）

用法（仓库根 .venv_tf）：
  python3 <本文件> --port /dev/cu.usbserial-1120
产物：Host/results/reatt_board/ladder_morning.log + 判决表
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import struct
import sys
import time

import numpy as np


def find_repo_root(start_dir):
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("ERROR: 无法定位仓库根")
        d = parent


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    REPO = find_repo_root(os.path.abspath(__file__))
    PROJ = os.path.join(REPO, "firmware", "stm32n6", "04_SourceCode",
                        "SoftwarePackage", "EMS_Projects", "EMS02_CHIL_REATT8_NPU")
    spec = importlib.util.spec_from_file_location(
        "c", os.path.join(PROJ, "Host", "tools", "reatt_board_client.py"))
    c = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(c)

    import serial
    ser = serial.Serial(args.port, args.baud, timeout=0.3)
    time.sleep(0.4)
    ser.reset_input_buffer()

    def cap(seconds):
        buf = bytearray()
        t0 = time.time()
        while time.time() - t0 < seconds:
            d = ser.read(4096)
            if d:
                buf.extend(d)
        return bytes(buf)

    # PING（帧级）
    b = c.Board(args.port, args.baud)
    ok = b.ping()
    print(f"PING: {'OK' if ok else 'FAIL'}")
    if not ok:
        print("板未响应：确认已切 Flash boot 并完整断电重上电。")
        return 1
    ser = b.ser
    ser.reset_input_buffer()

    # REATT 首窗（触发全部诊断）
    z = np.load(os.path.join(PROJ, "Host", "reference", "reatt",
                             "reatt_ref_vectors_split.npz"))
    payload = (struct.pack("<I", 0xC001)
               + z["hist_raw"][0].astype("<f4").tobytes()
               + z["kf_raw"][0].astype("<f4").tobytes())
    ser.write(c.enc_frame(c.CMD_REATT, payload))
    raw = cap(40.0)
    b.close()

    os.makedirs(os.path.join(PROJ, "Host", "results", "reatt_board"), exist_ok=True)
    log = os.path.join(PROJ, "Host", "results", "reatt_board", "ladder_morning.log")
    with open(log, "wb") as fh:
        fh.write(raw)

    txt = "".join(ch if 32 <= ord(ch) < 127 or ch in "\r\n\t" else "."
                  for ch in raw.decode("latin1"))
    lines = [l.strip() for l in txt.splitlines() if l.strip()]
    keys = ("RS-NOR", "RS-SELF", "RS-L1", "RS-L2", "RS-L3", "RS-L4", "RS-F1",
            "RS-N1", "RS-N2", "RS-T", "FAIL")
    hits = [l for l in lines if any(k in l for k in keys)]
    print("---- 板上诊断 ----")
    for l in hits:
        print("   ", l[:150])

    print("---- 判决表 ----")
    verdicts = {}
    for l in hits:
        for tag in ("L1", "L2", "L3", "L4", "F1"):
            if l.startswith(f"RS-{tag} "):
                verdicts[tag] = ("PASS" if "PASS" in l.split() else "DIFF")
    if any("out=[-128 -100 -1 0 1 50 100 127]" in l for l in hits):
        verdicts["SELF"] = "PASS"
    else:
        verdicts["SELF"] = "FAIL"
    nor_ok = ("n2=5d079aec" in txt and "sf=6ff8c563" in txt
              and "wc=f916d5f5" in txt)
    verdicts["NOR"] = "PASS" if nor_ok else "FAIL"
    for k in ("NOR", "SELF", "L1", "L2", "L3", "L4", "F1"):
        print(f"    {k:4s}: {verdicts.get(k, '（无输出）')}")

    print("---- 结论 ----")
    if verdicts.get("NOR") != "PASS" or verdicts.get("SELF") != "PASS":
        print("  前置门未过（NOR/SELF），先查集成前提。")
    elif verdicts.get("F1") == "PASS":
        print("  F1 PASS ⇒ --Ono-clone-dma 即根因：服务 N2 重编译加该选项即可修复。")
    else:
        bad = [t for t in ("L1", "L2", "L3", "L4") if verdicts.get(t) == "DIFF"]
        if bad:
            print(f"  首个坏段 = {bad[0]}（{bad}）⇒ 该段退回 CPU 形态，其余留 NPU。")
            why = {"L1": "kf 输入通路（Transpose/DQ/Dense/位置）",
                   "L2": "attention 块（QKᵀ/Softmax/AV）",
                   "L3": "h_past Tile 路径（30-lead 全同指纹来源）",
                   "L4": "fusion+decoder"}
            print(f"    坏段语义：{why[bad[0]]}")
        else:
            print("  L1..L4 全 PASS 且 F1 FAIL ⇒ 图结构无错，深挖编译选项（第二轮消融）。")
    print(f"原始日志: {log}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

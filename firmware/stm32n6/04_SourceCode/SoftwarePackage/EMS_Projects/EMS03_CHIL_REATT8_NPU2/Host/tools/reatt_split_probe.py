#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""reatt_split_probe.py — 拆分形态首次板上探针（诊断捕获 + 单窗对拍）。

用途：烧录后第一次上电自检。功能：
  1. PING 链路检查；
  2. 发 1 个 REATT 窗（取拆分参考向量第 0 条），期间把串口全部原始字节落盘，
     以便读取固件诊断行（RS-Q / RS-N1 / RS-N2 / RS-T / RS-FAIL）；
  3. 打印该窗板上 q 与 host 拆分链参考（q_split）的逐档差。

用法（仓库根 .venv_tf）：
  python3 <本文件> --port /dev/cu.usbserial-1120 [--idx 0]
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import sys
import time

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def _load_client(path):
    spec = importlib.util.spec_from_file_location("reatt_board_client", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--idx", type=int, default=0)
    ap.add_argument("--tools", default=None, help="Host/tools 目录（缺省按 __file__ 推导）")
    ap.add_argument("--raw-out", default="/tmp/reatt_split_probe_raw.log")
    args = ap.parse_args()

    tools = args.tools or SCRIPT_DIR
    cli = _load_client(os.path.join(tools, "reatt_board_client.py"))

    z = np.load(os.path.join(cli.PROJ, "Host", "reference", "reatt",
                             "reatt_ref_vectors_split.npz"))
    hist_raw, kf_raw, q_ref = z["hist_raw"], z["kf_raw"], z["q_split"]
    i = args.idx

    b = cli.Board(args.port, args.baud)
    ok = b.ping()
    print("PING:", "OK" if ok else "FAIL")
    if not ok:
        b.close()
        return 1

    b.ser.reset_input_buffer()
    b.rxbuf.clear()
    q, us = b.reatt(hist_raw[i].reshape(300, 11), kf_raw[i].reshape(30, 6), 0x2000 + i)
    time.sleep(0.5)
    raw = bytes(b.rxbuf) + (b.ser.read(65536) or b"")
    b.close()

    with open(args.raw_out, "wb") as fh:
        fh.write(raw)
    txt = "".join(ch if 32 <= ord(ch) < 127 or ch in "\r\n\t" else "." for ch in raw.decode("latin1"))
    print("---- 串口原始输出（诊断行）----")
    for line in txt.splitlines():
        if line.strip():
            print("  ", line.strip())

    cmp_ref = q_ref[i]
    d = np.abs(q - cmp_ref)
    print("---- 单窗对拍（窗 %d）----" % i)
    print("  infer_us = %d" % us)
    print("  q50 板上 :", np.round(q[::6, 1], 3), "...")
    print("  q50 参考 :", np.round(cmp_ref[::6, 1], 3), "...")
    print("  q10/q50/q90 MAE vs 拆分参考: %.4f / %.4f / %.4f kW"
          % (d[:, 0].mean(), d[:, 1].mean(), d[:, 2].mean()))
    print("  q50 max diff: %.4f kW   finite=%s" % (d[:, 1].max(), bool(np.isfinite(q).all())))
    return 0


if __name__ == "__main__":
    sys.exit(main())

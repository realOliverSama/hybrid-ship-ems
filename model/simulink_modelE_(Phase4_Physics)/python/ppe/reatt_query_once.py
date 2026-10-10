#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""reatt_query_once.py — 单次 REAT-T 板查询 worker（phase4-g soft-HIL）。

架构（2026-09-22 bring-up 裁决）：**每请求独立进程 + 新开端口**——板上实测
"持口服务器"在 macOS/CH340 上不可靠（MATLAB 冷启动窗口争用、长空闲后设备
静默），而"新开端口→ping→查询→关闭"模式在全部验证中 100% 可靠（板级
L1 对拍 64/64 即此模式）。

输入：一个 .npz 文件（hist_raw [300,11] f32、kf_raw [30,6] f32）。
输出：stdout 单行 JSON：{"ok":true,"q50":[...30],"q":[[q10,q50,q90]x30],
"infer_us":N,"rtt_ms":F} 或 {"ok":false,"error":"..."}。

用法：python -m ppe.reatt_query_once --port DEV --baud 115200 --npz FILE.npz
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
import time

import numpy as np

SYNC0, SYNC1 = 0xAA, 0x55
CMD_PING, CMD_REATT, RSP_REATT = 0x06, 0x0A, 0x0B
ACK, NACK = 0x7F, 0x7E


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def enc_frame(ftype: int, payload: bytes) -> bytes:
    body = bytes([ftype]) + struct.pack("<H", len(payload)) + payload
    return bytes([SYNC0, SYNC1]) + body + struct.pack("<H", crc16_ccitt(body))


def read_frame(ser, rxbuf: bytearray, timeout_s=90.0):
    t0 = time.time()
    while time.time() - t0 < timeout_s:
        chunk = ser.read(4096)
        if chunk:
            rxbuf.extend(chunk)
        while len(rxbuf) >= 5:
            if rxbuf[0] != SYNC0 or rxbuf[1] != SYNC1:
                rxbuf.pop(0)
                continue
            ln = struct.unpack("<H", bytes(rxbuf[3:5]))[0]
            if len(rxbuf) < 5 + ln + 2:
                break
            body = bytes(rxbuf[2:5 + ln])
            crc_rx = struct.unpack("<H", bytes(rxbuf[5 + ln:7 + ln]))[0]
            del rxbuf[:7 + ln]
            if crc16_ccitt(body) != crc_rx:
                continue
            return body[0], body[3:]
    raise TimeoutError("板响应超时")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="设备路径或 auto（glob /dev/cu.usbserial*）")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--npz", default="")
    ap.add_argument("--ping-only", action="store_true")
    args = ap.parse_args()

    import glob as _glob
    import serial

    def resolve_port(p):
        if p != "auto":
            return p
        cands = sorted(_glob.glob("/dev/cu.usbserial*"))
        if not cands:
            raise RuntimeError("无 /dev/cu.usbserial* 设备")
        return cands[0]

    args.port = resolve_port(args.port)
    out = {"ok": False, "error": "init"}
    try:
        if args.ping_only:
            ser = serial.Serial(args.port, args.baud, timeout=0.5)
            rx = bytearray()
            ser.write(enc_frame(CMD_PING, b""))
            while True:
                t, p = read_frame(ser, rx, timeout_s=20.0)
                if t == ACK and p and p[0] == CMD_PING:
                    break
            ser.close()
            out = {"ok": True, "pong": True}
            print(json.dumps(out))
            return 0
        z = np.load(args.npz)
        hist = np.ascontiguousarray(z["hist_raw"], dtype="<f4")  # [300,11]
        kf = np.ascontiguousarray(z["kf_raw"], dtype="<f4")      # [30,6]
        ser = serial.Serial(args.port, args.baud, timeout=0.5)
        rx = bytearray()
        t_ping = time.time()
        ser.write(enc_frame(CMD_PING, b""))
        while True:
            t, p = read_frame(ser, rx, timeout_s=20.0)
            if t == ACK and p and p[0] == CMD_PING:
                break
        t0 = time.time()
        payload = struct.pack("<I", 0) + hist.tobytes() + kf.tobytes()
        ser.write(enc_frame(CMD_REATT, payload))
        while True:
            t, p = read_frame(ser, rx, timeout_s=90.0)
            if t == NACK:
                raise RuntimeError(f"board NACK err={p[1]:#x}")
            if t == RSP_REATT and len(p) == 4 + 90 * 4 + 4:
                (rseq,) = struct.unpack("<I", p[:4])
                q = np.frombuffer(p[4:4 + 90 * 4], dtype="<f4").copy()
                (us,) = struct.unpack("<I", p[4 + 90 * 4:])
                q = q.reshape(30, 3)
                out = {"ok": True, "q": q.astype(float).tolist(),
                       "q50": q[:, 1].astype(float).tolist(),
                       "infer_us": int(us),
                       "rtt_ms": round((time.time() - t0) * 1000.0, 1),
                       "ping_ms": round((t0 - t_ping) * 1000.0, 1)}
                break
        ser.close()
    except Exception as exc:  # noqa: BLE001 — fail-closed 单行 JSON
        out = {"ok": False, "error": f"{type(exc).__name__}: {exc}"}
    print(json.dumps(out))
    return 0 if out["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

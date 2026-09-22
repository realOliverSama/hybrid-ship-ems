#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""reatt_board_client.py — REAT-T 板上服务客户端（phase4-g，T-007）。

CMD_REATT/RSP_REATT（ems_proto 0x0A/0x0B，帧布局 [AA 55][TYPE][LEN LE16][PAYLOAD]
[CRC16-CCITT LE16]，CRC 作用域 TYPE..PAYLOAD——与 ems_inject 同协议，独立实现
避免改动既有工具）。

功能：
  --ping                 链路检查（CMD_PING→ACK）
  --parity [N]           对拍：Host/reference/reatt/reatt_ref_vectors.npz 前 N 条
                         （缺省全部）原始域向量 → 板 → 与 host int8（L1）/keras
                         fp32（L2）参考比较；产物 Host/results/reatt_board/
                         board_parity.csv + board_parity_report.json（含时延分布）
  --latency N            纯时延测量（重复发同向量 N 次，统计 infer_us 与往返）

用法：
    python3 Host/tools/reatt_board_client.py --port /dev/cu.xxx --ping
    python3 Host/tools/reatt_board_client.py --port /dev/cu.xxx --parity
    python3 Host/tools/reatt_board_client.py --port /dev/cu.xxx --latency 50

依赖：pyserial（协议自实现，无 TF 依赖）。
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import struct
import sys
import time

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


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


REPO_ROOT = find_repo_root(SCRIPT_DIR)
PROJ = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
REF = os.path.join(PROJ, "Host", "reference", "reatt", "reatt_ref_vectors.npz")
RES = os.path.join(PROJ, "Host", "results", "reatt_board")

SYNC0, SYNC1 = 0xAA, 0x55
CMD_PING, CMD_REATT, RSP_REATT = 0x06, 0x0A, 0x0B
ACK, NACK = 0x7F, 0x7E
HIST_FLT, KF_FLT, Q_FLT = 3300, 180, 90


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


class Board:
    def __init__(self, port: str, baud: int = 921600):
        import serial
        self.ser = serial.Serial(port, baud, timeout=5.0)
        self.rxbuf = bytearray()

    def close(self):
        self.ser.close()

    def _read_frame(self, timeout_s: float = 30.0):
        """返回 (type, payload)；超时抛 TimeoutError。"""
        t0 = time.time()
        while time.time() - t0 < timeout_s:
            chunk = self.ser.read(4096)
            if chunk:
                self.rxbuf.extend(chunk)
            while len(self.rxbuf) >= 5:  # 同步2+TYPE1+LEN2
                if self.rxbuf[0] != SYNC0 or self.rxbuf[1] != SYNC1:
                    self.rxbuf.pop(0)
                    continue
                ln = struct.unpack("<H", self.rxbuf[3:5])[0]  # [AA 55][TYPE][LEN LE16]
                if len(self.rxbuf) < 5 + ln + 2:
                    break
                body = bytes(self.rxbuf[2:5 + ln])  # TYPE+LEN+PAYLOAD（CRC 作用域 TYPE..PAYLOAD）
                crc_rx = struct.unpack("<H", self.rxbuf[5 + ln:7 + ln])[0]
                del self.rxbuf[:7 + ln]
                if crc16_ccitt(body) != crc_rx:
                    continue  # CRC 错帧丢弃（与固件 NACK+事件环语义互补）
                return body[0], body[3:]  # payload（剥 TYPE+LEN）
        raise TimeoutError("板响应超时")

    def ping(self) -> bool:
        self.ser.reset_input_buffer()
        self.rxbuf.clear()
        self.ser.write(enc_frame(CMD_PING, b""))
        t, p = self._read_frame()
        return t == ACK and p[0] == CMD_PING

    def reatt(self, hist_raw, kf_raw, seq: int):
        """返回 (q[90] np.float32, infer_us)。"""
        payload = struct.pack("<I", seq) + hist_raw.astype("<f4").tobytes() \
            + kf_raw.astype("<f4").tobytes()
        assert len(payload) == 4 + (HIST_FLT + KF_FLT) * 4
        self.ser.write(enc_frame(CMD_REATT, payload))
        while True:
            t, p = self._read_frame()
            if t == NACK:
                raise RuntimeError(f"板 NACK err={p[1]:#x}")
            if t == RSP_REATT and len(p) == 4 + Q_FLT * 4 + 4:
                rseq, = struct.unpack("<I", p[:4])
                if rseq != seq:
                    continue  # 陈旧帧跳过
                q = np.frombuffer(p[4:4 + Q_FLT * 4], dtype="<f4").copy()
                us, = struct.unpack("<I", p[4 + Q_FLT * 4:])
                return q.reshape(30, 3), us


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--ping", action="store_true")
    ap.add_argument("--parity", nargs="?", type=int, const=0,
                    help="对拍（可给定向量数；0=全部）")
    ap.add_argument("--latency", type=int, default=0)
    args = ap.parse_args()

    os.makedirs(RES, exist_ok=True)
    b = Board(args.port, args.baud)

    if args.ping:
        ok = b.ping()
        print("PING:", "OK" if ok else "FAIL")
        b.close()
        return 0 if ok else 1

    z = np.load(REF)
    hist_raw, kf_raw = z["hist_raw"], z["kf_raw"]
    q_int8, q_keras = z["q_int8"], z["q_keras"]
    n = len(hist_raw) if not args.parity else args.parity

    rows = []
    us_list, rtt_list = [], []
    t_all = time.time()
    for i in range(n):
        seq = 0x1000 + i
        t0 = time.time()
        q, us = b.reatt(hist_raw[i].reshape(300, 11), kf_raw[i].reshape(30, 6), seq)
        rtt = (time.time() - t0) * 1000.0
        us_list.append(us)
        rtt_list.append(rtt)
        d_int8 = np.abs(q - q_int8[i])
        d_keras = np.abs(q - q_keras[i])
        rows.append({
            "i": i, "seq": seq, "infer_us": us, "rtt_ms": round(rtt, 2),
            "q50_mae_vs_hostint8_kW": float(d_int8[:, 1].mean()),
            "q50_max_vs_hostint8_kW": float(d_int8[:, 1].max()),
            "q50_mae_vs_keras_kW": float(d_keras[:, 1].mean()),
            "q50_max_vs_keras_kW": float(d_keras[:, 1].max()),
            "finite": bool(np.isfinite(q).all()),
        })
        print(f"[{i+1}/{n}] infer_us={us} rtt={rtt:.0f}ms "
              f"L1(q50)mae={rows[-1]['q50_mae_vs_hostint8_kW']:.4f} "
              f"L2(q50)mae={rows[-1]['q50_mae_vs_keras_kW']:.3f}")
    b.close()

    l1_mae = [r["q50_mae_vs_hostint8_kW"] for r in rows]
    l1_max = max(r["q50_max_vs_hostint8_kW"] for r in rows)
    l2_mae = [r["q50_mae_vs_keras_kW"] for r in rows]
    us = np.array(us_list)
    report = {
        "schema": "phase4g_reatt_board_parity_v1",
        "n": n, "baud": args.baud,
        "l1_board_vs_hostint8": {
            "q50_mae_kW_mean": float(np.mean(l1_mae)),
            "q50_max_kW": float(l1_max),
            "gate": {"q50_mae_mean_max": 0.5, "q50_max_max": 5.0},
            "pass": bool(np.mean(l1_mae) <= 0.5 and l1_max <= 5.0),
        },
        "l2_board_vs_keras_report": {
            "q50_mae_kW_mean": float(np.mean(l2_mae)),
            "q50_max_kW": float(max(r["q50_max_vs_keras_kW"] for r in rows)),
        },
        "latency": {
            "infer_us_mean": float(us.mean()),
            "infer_us_p50": float(np.percentile(us, 50)),
            "infer_us_max": float(us.max()),
            "rtt_ms_mean": float(np.mean(rtt_list)),
            "rtt_ms_max": float(np.max(rtt_list)),
        },
        "finite_rate": float(np.mean([r["finite"] for r in rows])),
        "total_wall_s": time.time() - t_all,
    }
    csv_path = os.path.join(RES, "board_parity.csv")
    with open(csv_path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    json.dump(report, open(os.path.join(RES, "board_parity_report.json"), "w"),
              indent=2, ensure_ascii=False)
    print(json.dumps(report, indent=2))
    return 0 if report["l1_board_vs_hostint8"]["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

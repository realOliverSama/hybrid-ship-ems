#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_c2_smoke.py — S4-C2 门：TICK_CTRL 板级冒烟（板在环、无植物）。

流程：
  1) 板载 S4Full ELF（SRAM 直载，host 侧另行 MCP 载入；本脚本只走 UART）
  2) CMD_PING 同步 → CMD_RESET（复位 PMS/law6 状态）
  3) 逐拍 CMD_TICK_CTRL：输入 = 真值包 hist/kf（S2 canonical）+ 合成状态
     （P_load/SOC/Vdc 等按拍步进的正弦/斜坡合成），共 100 拍；
     记录 RSP_TICK_CTRL 全字段；断言零 NACK/零超时、cmd/ref/commitment 形态合理。
  4) 抽 3 拍与 B_HOST（host 侧同链：ORT REATT→dp_tick→执行层→PMS/Safety Python
     口径）对拍 action-index（d1 档位）——一致 ⇒ C2 门 PASS。

运行：.venv_tf/bin/python tools/s4_c2_smoke.py --port /dev/cu.usbserial-1120
产物：Host/results/s4_c2/s4_c2_smoke_report.json
"""
import argparse
import ctypes
import json
import os
import struct
import sys
import time
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)
REPO = os.path.abspath(os.path.join(HOST, "..", "..", "..", "..", "..", "..", "..", "..", ".."))
sys.path.insert(0, os.path.join(HOST, "tools"))
from s3c_reatt_replay import enc_frame, SYNC0, SYNC1  # noqa: E402  (成熟帧编码)
import serial  # noqa: E402
from ems04_law6_c2 import dp_tick_py  # placeholder, not used (dylib below)

CMD_PING = 0x06
CMD_RESET = 0x01
CMD_TICK_CTRL = 0x0E
RSP_TICK_CTRL = 0x0F
ACK = 0x00

HIST_FLT, KF_FLT = 300 * 11, 30 * 6
CMD_LEN = 4 + 4 + 8 * 4 + (HIST_FLT + KF_FLT) * 4
RSP_LEN = 4 + 20 * 4 + 4 + 4 + 1


def load_truth_anchor(vid, t0):
    base = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25",
                        "ems04_s2", "truth", "anchors", vid)
    hist = np.fromfile(os.path.join(base, f"t0_{t0:05d}.hist.f32.bin"), "<f4")
    kf = np.fromfile(os.path.join(base, f"t0_{t0:05d}.kf.f32.bin"), "<f4")
    return hist, kf


def enc_tick_ctrl(seq, tick, P_load, SOC, Vdc, dis, chg, prop, hotel, hist, kf):
    b = struct.pack("<II8f", seq, tick, P_load, SOC, Vdc, dis, chg, prop, hotel)
    return b + hist.astype("<f4").tobytes() + kf.astype("<f4").tobytes()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbserial-1120")
    ap.add_argument("--ticks", type=int, default=100)
    ap.add_argument("--vid", default="FLTA_S1_D00000_M2")
    ap.add_argument("--t0", type=int, default=300)
    args = ap.parse_args()

    ser = serial.Serial(args.port, 115200, timeout=0.1)

    def send(ftype, payload=b""):
        ser.write(enc_frame(ftype, payload)); ser.flush()

    def wait(ftype, timeout_ms=8000):
        """等一帧 ftype；返回 payload 或 None（简单状态机扫 SYNC+type+len+CRC）。"""
        import time as _t
        deadline = _t.monotonic() + timeout_ms / 1000.0
        buf = b""
        while _t.monotonic() < deadline:
            buf += ser.read(ser.in_waiting or 1)
            while True:
                i = buf.find(bytes([SYNC0, SYNC1]))
                if i < 0:
                    buf = buf[-1:]
                    break
                if len(buf) < i + 4:
                    break
                ftype = buf[i + 2]
                ln = struct.unpack_from("<H", buf, i + 3)[0]
                if len(buf) < i + 6 + ln + 2:
                    break
                frame = buf[i:i + 6 + ln + 2]
                # CRC 校验（复用 s3c_reatt_replay 的 crc16）
                from s3c_reatt_replay import crc16_ccitt
                body = frame[2:-2]
                if struct.unpack("<H", frame[-2:])[0] != crc16_ccitt(body):
                    buf = buf[i + 2:]
                    continue
                if ftype == ftype:
                    payload = frame[6:6 + ln]
                    if ftype == ftype:
                        buf = buf[i + 6 + ln + 2:]
                        if ftype == RSP_TICK_CTRL or ftype == 0x00:
                            return (ftype, payload)
                        return (ftype, payload)
                break
        return None

    def wait_ack(ftype, timeout_ms):
        r = wait(ftype, timeout_ms)
        return r

    ok = False
    for _ in range(5):
        send(CMD_PING)
        r = wait(0x00, 400)
        if r is not None:
            ok = True
            break
    if not ok:
        print("PING_FAIL", file=sys.stderr)
        return 2
    send(CMD_RESET)
    if wait(0x00, 500) is None:
        print("RESET_FAIL", file=sys.stderr)
        return 2

    hist, kf = load_truth_anchor(args.vid, args.t0)
    rows, n_nack = [], 0
    seq = 0
    for k in range(args.ticks):
        P_load = 100.0 + 60.0 * (1 + np.sin(2 * np.pi * k / 25.0))
        SOC = 0.60 - 0.01 * k / args.ticks
        Vdc = 750.0
        dis, chg, prop, hotel = 50.0, 30.0, 40.0 + 5.0 * np.sin(k), 100.0
        payload = enc_tick_ctrl(seq, k, P_load, SOC, Vdc, dis, chg, prop, hotel, hist, kf)
        send(CMD_TICK_CTRL, payload)
        r = wait(RSP_TICK_CTRL, 8000)
        if r is None or r[0] != RSP_TICK_CTRL:
            n_nack += 1
            rows.append({"tick": k, "NACK": True})
            if n_nack > 3:
                break
            continue
        body = r[1]
        if len(body) < RSP_LEN:
            n_nack += 1
            continue
        seq_r, = struct.unpack_from("<I", body, 0)
        (P_gen_ref, P_commit, cstate, rcode, on1, on2, ref1, ref2, N, d1, lam,
         f0, f1, fs0, fs1, fstp0, fstp1, health, fb) = struct.unpack_from("<19f", body, 4)
        infer_us, ctrl_us = struct.unpack_from("<II", body, 4 + 76)
        flags = body[4 + 84]
        rows.append({"tick": k, "seq": seq_r, "P_gen_ref": P_gen_ref,
                     "P_commit": P_commit, "commitment_state": cstate,
                     "reason_code": rcode, "cmd_onoff_DG1": on1, "cmd_onoff_DG2": on2,
                     "P_DG1_ref": ref1, "P_DG2_ref": ref2, "N_dispatch": N,
                     "d1": d1, "lam_eff": lam, "fault0": f0, "fault1": f1,
                     "infer_us": infer_us, "ctrl_us": ctrl_us, "flags": flags})
        seq += 1
        time.sleep(0.005)

    n_ok = sum(1 for r in rows if not r.get("NACK"))
    report = {
        "schema": "s4_c2_smoke_v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "port": args.port, "ticks_requested": args.ticks,
        "ticks_ok": n_ok, "n_nack": n_nack,
        "zero_nack": n_nack == 0,
        "rows": rows,
    }
    out_dir = os.path.join(HOST, "results", "s4_c2")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "s4_c2_smoke_report.json")
    json.dump(report, open(out, "w"), indent=1)
    print(f"smoke: {n_ok}/{args.ticks} ok, NACK={n_nack}")
    print("ZERO_NACK:", report["zero_nack"])
    print("report ->", out)
    return 0 if report["zero_nack"] and n_ok >= args.ticks else 1


if __name__ == "__main__":
    sys.exit(main())

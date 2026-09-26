#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3c_reatt_replay.py — EMS04-S3-C CMD_REATT 回放客户端（S3C_CONSTRUCTION_SPEC §4.5）。

与 T-007 的 reatt_board_client.py 独立（互不影响）：本工具的输入源 = S2 真值包
canonical 特征字节（experiments/softhil_ems03_2026-09-25/ems04_s2/truth/anchors/
<VID>/t0_*.{hist,kf}.f32.bin，755 锚），回放前逐锚校验 T0_INDEX.json 登记的
sha256。协议同 ems_proto：帧 [AA 55][TYPE][LEN LE16][PAYLOAD][CRC16-CCITT LE16]，
CRC 作用域 TYPE..PAYLOAD。

S3-C 用途：
  首测前置 → 单锚冒烟（--limit 1）→ 全 755 锚回放；产物 capture（.jsonl，逐锚
  seq + q30×3 raw f32 + infer_us + 输入字节 sha256）+ summary（.json，NACK/CRC 错/
  超时/mismatch 统计）。板侧 hist/kf requant raw code 与 N1/N2 边界码经调试器
  内存读取另行取证（0x34357080 = N1 Quantize_27_out_0，见 s3c 边界路径记录），
  本工具只负责 UART 通道的输入喂入与链尾 q30×3 收集。

用法：
  --dry-run                          不开串口：构造全部帧 + sha256 清单 + CRC 自检
  --port /dev/cu.xxx --ping          链路检查
  --port ... --limit 1               单锚冒烟
  --port ... [--vid FLTA_...]        全量/单航次回放
协议纪律（沿用先例）：pyserial 超时读 + CRC 校验 + NACK 即停 + 陈旧 seq 帧跳过；
零进展保护 = _read_frame 超时抛错；逐写不拆帧（13924 B 单 write 由 OS 缓冲）。

依赖：pyserial（--dry-run 与 numpy 可选路径不依赖串口）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys
import time

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def find_repo_root(start_dir):
    d = os.path.abspath(start_dir)
    while d != os.path.dirname(d):
        if os.path.isdir(os.path.join(d, ".git")):
            return d
        d = os.path.dirname(d)
    return d


REPO = find_repo_root(SCRIPT_DIR)
TRUTH_DIR = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25",
                         "ems04_s2", "truth", "anchors")

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


def load_anchor_index():
    """T0_INDEX.json ×2（GOOD/DEGRADED）→ 有序锚清单 [(vid, t0, hist_path, kf_path,
    hist_sha, kf_sha)]，按 (vid, t0) 升序，共 755。"""
    anchors = []
    for vid in sorted(os.listdir(TRUTH_DIR)):
        idx_path = os.path.join(TRUTH_DIR, vid, "T0_INDEX.json")
        if not os.path.isfile(idx_path):
            continue
        idx = json.load(open(idx_path))
        entries = idx.get("anchors") or idx.get("t0s") or idx.get("entries")
        if entries is None:
            raise SystemExit(f"T0_INDEX.json 结构不含锚清单字段: {idx_path} keys={list(idx)}")
        for e in entries:
            if isinstance(e, dict):
                t0 = e["t0_s"] if "t0_s" in e else e["t0"]
                hist_sha = e.get("hist_sha256") or e.get("hist_sha")
                kf_sha = e.get("kf_sha256") or e.get("kf_sha")
            else:  # [t0, hist_sha, kf_sha] 三元组
                t0, hist_sha, kf_sha = e
            base = f"t0_{int(t0):05d}"
            anchors.append((vid, int(t0),
                            os.path.join(TRUTH_DIR, vid, base + ".hist.f32.bin"),
                            os.path.join(TRUTH_DIR, vid, base + ".kf.f32.bin"),
                            hist_sha, kf_sha))
    anchors.sort(key=lambda a: (a[0], a[1]))
    return anchors


def load_anchor_bin(path, expect_n, sha_expect):
    raw = open(path, "rb").read()
    if len(raw) != expect_n * 4:
        raise SystemExit(f"{path}: {len(raw)} B != {expect_n * 4} B")
    if sha_expect and hashlib.sha256(raw).hexdigest() != sha_expect:
        raise SystemExit(f"{path}: sha256 与 T0_INDEX 登记不符")
    return np.frombuffer(raw, dtype="<f4")


class Board:
    def __init__(self, port: str, baud: int):
        import serial
        self.ser = serial.Serial(port, baud, timeout=5.0)
        self.rxbuf = bytearray()
        self.crc_errors = 0

    def close(self):
        self.ser.close()

    def _read_frame(self, timeout_s: float = 60.0):
        t0 = time.time()
        while time.time() - t0 < timeout_s:
            chunk = self.ser.read(4096)
            if chunk:
                self.rxbuf.extend(chunk)
            while len(self.rxbuf) >= 5:
                if self.rxbuf[0] != SYNC0 or self.rxbuf[1] != SYNC1:
                    self.rxbuf.pop(0)
                    continue
                ln = struct.unpack("<H", self.rxbuf[3:5])[0]
                if len(self.rxbuf) < 5 + ln + 2:
                    break
                body = bytes(self.rxbuf[2:5 + ln])
                crc_rx = struct.unpack("<H", self.rxbuf[5 + ln:7 + ln])[0]
                del self.rxbuf[:7 + ln]
                if crc16_ccitt(body) != crc_rx:
                    self.crc_errors += 1
                    continue
                return body[0], body[3:]
        raise TimeoutError("板响应超时（零进展保护触发）")

    def ping(self) -> bool:
        self.ser.reset_input_buffer()
        self.rxbuf.clear()
        self.ser.write(enc_frame(CMD_PING, b""))
        t, p = self._read_frame(timeout_s=10.0)
        return t == ACK and p[0] == CMD_PING

    def reatt(self, hist_raw, kf_raw, seq: int):
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
                    continue
                q = np.frombuffer(p[4:4 + Q_FLT * 4], dtype="<f4").copy()
                us, = struct.unpack("<I", p[4 + Q_FLT * 4:])
                return q.reshape(30, 3), us


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", help="串口设备（--dry-run 时可省略）")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--ping", action="store_true")
    ap.add_argument("--dry-run", action="store_true",
                    help="不开串口：帧构造 + 输入 sha 清单 + CRC 自检")
    ap.add_argument("--vid", action="append", default=[],
                    help="只回放指定航次（可多次）")
    ap.add_argument("--limit", type=int, default=0, help="只回放前 N 锚（0=全部）")
    ap.add_argument("--outdir", default=os.path.join(SCRIPT_DIR, "..", "results", "s3c"))
    args = ap.parse_args()

    anchors = load_anchor_index()
    if args.vid:
        anchors = [a for a in anchors if a[0] in set(args.vid)]
    if args.limit:
        anchors = anchors[:args.limit]
    if not anchors:
        raise SystemExit("锚清单为空（检查 truth 目录 / --vid / --limit）")
    print(f"锚清单: {len(anchors)} 条（755 预期全量）")

    frames = []
    for i, (vid, t0, hp, kp, hsha, ksha) in enumerate(anchors):
        hist = load_anchor_bin(hp, HIST_FLT, hsha)
        kf = load_anchor_bin(kp, KF_FLT, ksha)
        frames.append((i, vid, t0, hist, kf))
    print(f"输入加载+sha 校验: {len(frames)}/{len(anchors)} PASS")

    if args.dry_run:
        total = sum(4 + (HIST_FLT + KF_FLT) * 4 + 6 for _, _, _, h, k in frames)
        ok = all(crc16_ccitt(enc_frame(CMD_REATT,
                 struct.pack("<I", i) + h.astype("<f4").tobytes()
                 + k.astype("<f4").tobytes())[2:-2]) ==
                 struct.unpack("<H", enc_frame(CMD_REATT,
                 struct.pack("<I", i) + h.astype("<f4").tobytes()
                 + k.astype("<f4").tobytes())[-2:])[0]
                 for i, _, _, h, k in frames[:32])
        print(f"dry-run: 帧数={len(frames)} 下行总字节={total} CRC 自检(前32帧)={'PASS' if ok else 'FAIL'}")
        print(f"dry-run: RSP 解析长度=4+{Q_FLT}*4+4={4 + Q_FLT * 4 + 4} B；协议常量 "
              f"CMD_REATT={CMD_REATT:#x} RSP_REATT={RSP_REATT:#x}")
        return

    if not args.port:
        ap.error("--port 必填（或用 --dry-run）")
    os.makedirs(args.outdir, exist_ok=True)
    stamp = time.strftime("%Y%m%dT%H%M%S", time.gmtime())
    cap_path = os.path.join(args.outdir, f"s3c_capture_{stamp}.jsonl")
    sum_path = os.path.join(args.outdir, f"s3c_summary_{stamp}.json")

    b = Board(args.port, args.baud)
    if args.ping:
        print("ping:", "OK" if b.ping() else "FAIL")

    n_ok = n_err = 0
    t_start = time.time()
    with open(cap_path, "w") as cap:
        for i, vid, t0, hist, kf in frames:
            try:
                q, us = b.reatt(hist, kf, i)
                cap.write(json.dumps({
                    "seq": i, "vid": vid, "t0": t0,
                    "q_f32": q.reshape(-1).tolist(), "infer_us": int(us),
                }) + "\n")
                n_ok += 1
            except (RuntimeError, TimeoutError) as e:
                cap.write(json.dumps({"seq": i, "vid": vid, "t0": t0,
                                      "error": str(e)}) + "\n")
                n_err += 1
                print(f"seq {i} ({vid} t0={t0}): {e}")
                if n_err >= 3:
                    print("连续错误 ≥3，停止回放")
                    break
    dt = time.time() - t_start
    b.close()
    summary = {
        "anchors_total": len(anchors), "n_ok": n_ok, "n_err": n_err,
        "crc_errors": b.crc_errors, "elapsed_s": round(dt, 1),
        "cap_sha256": hashlib.sha256(open(cap_path, "rb").read()).hexdigest(),
    }
    json.dump(summary, open(sum_path, "w"), indent=2)
    print(json.dumps(summary, indent=2))
    print(f"capture: {cap_path}\nsummary: {sum_path}")


if __name__ == "__main__":
    main()

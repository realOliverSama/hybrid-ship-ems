#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3a2_chunk_to_frame.py — S3-A2 诊断传输适配器（0x27 分块流 → 0x26 单帧）。

背景（用户裁决 2026-09-26 §14 "Repair only the diagnostic transport"）：
RSP_FP_DUMP(0x26) 单帧 13939 B 在 115200 UART 长流中丢帧头，板上改由 TYPE 0x27
分块直发（ems04_fp_dumpchunk.c，EMS04_DUMP_CHUNK=1）。载荷本身逐字节不变——本
工具把板侧 capture 里的 0x27 分块流**原位替换**为一枚 0x26 单帧（载荷 = 重组
后的 13932 B 板上原始字节），使按 0x26 语义编写的离线核验器（s3a2_check.py）
可以直接消费板侧真实字节，无需改动其任何断言。

严格边界（fail-closed，任一不满足即非零退出且不产出输出文件）：
  * 0x27 载荷契约 [u32 seq][u32 chunk_idx][u32 total_chunks][u32 payload_len]
    [u8 last_flag][u8 rsv][payload ≤494 B]；同一 DUMP 全部帧同 seq；
  * chunk_idx 必须 0..total_chunks-1 连续且无重复；last_flag 仅末块为 1；
  * 重组长度必须等于 --expect-len（默认 13932 = EMS04_FP_RSP_DUMP_LEN）；
  * 0x27 帧 CRC 必须全部通过；出现多个 seq 组即报错（本流程单 DUMP）；
  * 非 0x27 帧逐字节原样保留（含 ACK/NACK/RSP_QUERY），垃圾字节同样保留
    （核验器自带重同步与垃圾字节断言，不得由本工具静默清洗）。

用法（仓库根 .venv_tf python）：
    python3 s3a2_chunk_to_frame.py --capture board.capture.bin --out adapted.bin
    python3 s3a2_chunk_to_frame.py --capture board.capture.bin --out adapted.bin \
        --expect-len 13932 [--report report.json]

退出码：0 成功（含"无 0x27 帧、原样透传"情形，report 中标 no_chunk=true）；
        2 参数/IO 错；5 契约违反（连续性/长度/CRC/多 seq）。
纪律：AGENTS.md §20.2——所有路径由调用方给出或 __file__ 相对推导，无绝对路径。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys

SYNC0, SYNC1 = 0xAA, 0x55
TYPE_DUMP_CHUNK = 0x27
TYPE_RSP_DUMP = 0x26
CHUNK_HDR_LEN = 18
CHUNK_DATA_MAX = 494
EXPECT_DUMP_LEN = 13932        # EMS04_FP_RSP_DUMP_LEN = 8 + 4 + 13200 + 720
CHUNK_HDR_FMT = "<IIIIBB"


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def enc_frame(ftype: int, payload: bytes) -> bytes:
    body = bytes([ftype]) + struct.pack("<H", len(payload)) + payload
    return bytes([SYNC0, SYNC1]) + body + struct.pack("<H", crc16_ccitt_false(body))


def walk_frames(buf: bytes):
    """逐字节重同步扫描：返回 (items, stats)。item = (offset, raw_frame, ftype, payload)。
    无法构成合法帧的字节计入 junk，原样保留在输出中。"""
    items = []
    junk = 0
    crc_bad = 0
    i = 0
    n = len(buf)
    while i < n:
        if buf[i] != SYNC0 or i + 1 >= n or buf[i + 1] != SYNC1:
            junk += 1
            i += 1
            continue
        if i + 5 > n:
            junk += 1
            i += 1
            continue
        ln = struct.unpack_from("<H", buf, i + 3)[0]
        end = i + 5 + ln + 2
        if end > n:
            junk += 1
            i += 1
            continue
        body = buf[i + 2:i + 5 + ln]
        crc_rx = struct.unpack_from("<H", buf, i + 5 + ln)[0]
        if crc16_ccitt_false(body) != crc_rx:
            crc_bad += 1
            i += 1
            continue
        items.append((i, buf[i:end], body[0], body[3:]))
        i = end
    return items, {"junk_bytes": junk, "crc_bad": crc_bad, "total_bytes": n}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="S3-A2 0x27 分块流 → 0x26 单帧 传输适配器")
    ap.add_argument("--capture", required=True, help="板侧响应字节流（fp_replay_client capture）")
    ap.add_argument("--out", required=True, help="适配后 capture 输出路径")
    ap.add_argument("--expect-len", type=int, default=EXPECT_DUMP_LEN,
                    help="期望重组 DUMP 载荷长度（默认 %d）" % EXPECT_DUMP_LEN)
    ap.add_argument("--report", help="可选：核验报告 JSON 落盘路径")
    args = ap.parse_args(argv)

    try:
        buf = open(args.capture, "rb").read()
    except OSError as exc:
        print("ERROR: 无法读取 capture: %s" % exc, file=sys.stderr)
        return 2

    items, stats = walk_frames(buf)
    chunks = [it for it in items if it[2] == TYPE_DUMP_CHUNK]

    report = {
        "capture": os.path.basename(args.capture),
        "capture_sha256": hashlib.sha256(buf).hexdigest(),
        "total_bytes": stats["total_bytes"],
        "frames_total": len(items),
        "junk_bytes": stats["junk_bytes"],
        "crc_bad_frames": stats["crc_bad"],
    }

    if not chunks:
        # 无分块帧：原样透传（非分块形态 capture，例如 0x26 形态或模拟 capture）
        report.update({"no_chunk": True, "adapted_sha256": report["capture_sha256"],
                       "note": "capture 中无 0x27 帧，逐字节原样输出"})
        open(args.out, "wb").write(buf)
        if args.report:
            json.dump(report, open(args.report, "w"), indent=2)
        print("[adapter] 无 0x27 帧：逐字节透传 %d B → %s" % (len(buf), args.out))
        return 0

    if stats["crc_bad"]:
        print("ERROR: capture 含 %d 条 CRC 错帧（契约要求 0）" % stats["crc_bad"], file=sys.stderr)
        return 5

    # 解析块头 + 按 seq 分组
    by_seq = {}
    order = []
    for off, raw, _t, pay in chunks:
        if len(pay) < CHUNK_HDR_LEN:
            print("ERROR: 0x27 帧载荷 %d B < 块头 %d B" % (len(pay), CHUNK_HDR_LEN), file=sys.stderr)
            return 5
        seq, idx, total, plen, last, _rsv = struct.unpack_from(CHUNK_HDR_FMT, pay, 0)
        data = pay[CHUNK_HDR_LEN:CHUNK_HDR_LEN + plen]
        if len(data) != plen:
            print("ERROR: 帧 payload_len=%d 与实载不符" % plen, file=sys.stderr)
            return 5
        if plen > CHUNK_DATA_MAX:
            print("ERROR: 帧数据 %d B > %d B" % (plen, CHUNK_DATA_MAX), file=sys.stderr)
            return 5
        by_seq.setdefault(seq, []).append((idx, total, plen, last, data, off))
        order.append(seq)

    if len(by_seq) != 1:
        print("ERROR: capture 含 %d 个 seq 组（期望 1 个 DUMP）" % len(by_seq), file=sys.stderr)
        return 5
    seq, group = next(iter(by_seq.items()))
    totals = {g[1] for g in group}
    if len(totals) != 1:
        print("ERROR: total_chunks 不一致: %s" % sorted(totals), file=sys.stderr)
        return 5
    total = totals.pop()
    if len(group) != total:
        print("ERROR: 收到 %d 块 != total_chunks %d（丢块）" % (len(group), total), file=sys.stderr)
        return 5
    group.sort(key=lambda g: g[0])
    if [g[0] for g in group] != list(range(total)):
        print("ERROR: chunk_idx 不连续/重复", file=sys.stderr)
        return 5
    for k, g in enumerate(group):
        want_last = 1 if k == total - 1 else 0
        if g[3] != want_last:
            print("ERROR: chunk#%d last_flag=%d != %d" % (k, g[3], want_last), file=sys.stderr)
            return 5

    payload = b"".join(g[4] for g in group)
    if len(payload) != args.expect_len:
        print("ERROR: 重组 %d B != 期望 %d B" % (len(payload), args.expect_len), file=sys.stderr)
        return 5

    # 原位替换：首个 0x27 帧位置写入 0x26 单帧，其余 0x27 帧移除；其余字节逐字节保留
    chunk_raws = sorted((it[0], it[1]) for it in chunks)
    first_off = chunk_raws[0][0]
    out = bytearray()
    cursor = 0
    for off, raw in chunk_raws:
        out += buf[cursor:off]
        if off == first_off:
            out += enc_frame(TYPE_RSP_DUMP, payload)
        cursor = off + len(raw)
    out += buf[cursor:]

    open(args.out, "wb").write(bytes(out))
    report.update({
        "no_chunk": False,
        "chunk_seq": seq,
        "chunks": total,
        "chunk_len_min": min(g[2] for g in group),
        "chunk_len_max": max(g[2] for g in group),
        "reassembled_len": len(payload),
        "reassembled_sha256": hashlib.sha256(payload).hexdigest(),
        "adapted_sha256": hashlib.sha256(bytes(out)).hexdigest(),
        "adapted_bytes": len(out),
    })
    if args.report:
        json.dump(report, open(args.report, "w"), indent=2)
    print("[adapter] seq=%d 块=%d 重组=%d B（sha256=%s…）→ 单帧 0x26 适配 capture %d B → %s"
          % (seq, total, len(payload), report["reassembled_sha256"][:16], len(out), args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())

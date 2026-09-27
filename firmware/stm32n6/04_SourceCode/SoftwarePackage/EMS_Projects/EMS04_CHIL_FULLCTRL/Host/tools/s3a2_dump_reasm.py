#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3a2_dump_reasm.py — EMS04-S3-A2 RSP_FP_DUMP_CHUNK(0x27) capture 重组工具。

用途（S3-A2 分块传输修复的主机侧配套，仅诊断，不改特征/预测语义）：
  板上 EMS04_DUMP_CHUNK=1 形态把 RSP_FP_DUMP(0x26) 载荷切成 TYPE=0x27 分块帧
  直发（格式见 Appli/App/ems04_fp_dumpchunk.h 文件头注释）。本工具读取
  `fp_replay_client.py capture` 同款 **U→H 原始字节流**（串口长流，可含文本
  日志行/其他 TYPE 帧/噪声），提取全部 0x27 chunk 帧、按 seq 分组重组出完整
  DUMP 载荷（默认契约 13932 B）写文件，并打印 chunk/CRC 完整性摘要。

线格式（与固件 ems04_fp_proto 冻结定义逐字节同构）：
  帧容器  [AA 55][TYPE u8][LEN u16 LE][PAYLOAD LEN B][CRC16 u16 LE]
          CRC16-CCITT-FALSE（poly 0x1021 / init 0xFFFF），作用域 TYPE..PAYLOAD
  0x27 载荷 [u32 seq][u32 chunk_idx][u32 total_chunks][u32 payload_len]
            [u8 last_flag][u8 rsv][payload payload_len B]
          单帧载荷 ≤ 512 B（块头 18 B → 数据 ≤ 494 B/帧），LEN ≤ 512。

fail-closed 判据（任一不满足即非零退出，不产出"看似完整"的文件）：
  1) 选中 seq 的 chunk_idx 集合 = {0..total_chunks-1}（无缺口/无重复）；
  2) last_flag 仅出现在 idx = total_chunks-1 且 =1；
  3) rsv 恒 0；每帧 payload_len == LEN-18；
  4) 重组长度 == --expect-len（默认 13932 = PROTOCOL_FP §5 DUMP 载荷契约）。

用法：
  python3 Host/tools/s3a2_dump_reasm.py --capture CAP.bin --out dump.bin
      [--seq N] [--expect-len 13932] [--stats-only]
退出码：0 成功；4 chunk 缺口/重复/标志非法；5 长度 != --expect-len；
        6 capture 中无 0x27 帧；7 多 seq 未指定 --seq；8 参数/IO 错误。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys

SYNC0, SYNC1 = 0xAA, 0x55
HDR_LEN, CRC_LEN = 5, 2                      # AA55/TYPE/LEN + CRC16
TYPE_DUMP_CHUNK = 0x27
CHUNK_HDR_LEN = 18                            # 4×u32 + last_flag + rsv
FRAME_PAYLOAD_MAX = 512                       # 单帧载荷上限（18 + 494）
DEFAULT_EXPECT_LEN = 13932                    # [f64 t0][status/rsv][hist 13200][kf 720]


def crc16_ccitt_false(data: bytes) -> int:
    """CRC16-CCITT-FALSE：poly 0x1021 / init 0xFFFF / 不反射 / 无输出异或。"""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def parse_stream(buf: bytes):
    """扫描原始字节流：返回 (frames_0x27, stats)。

    解析策略：AA55 同步搜索 → TYPE/LEN 候选 → CRC 校验（TYPE..PAYLOAD）。
    CRC 过 = 消费整帧；CRC 不过 = 自 sync0 起前进 1 字节重同步（长流容错）。
    非 0x27 的合法帧同样消费跳过（capture 可混有 ACK/RSP_TICK/QUERY 等流量）。
    LEN 候选上限 = 线上限 65535，由 CRC 把关误同步。
    """
    stats = {
        "bytes": len(buf),
        "frames_ok": 0,           # CRC 通过的帧（全 TYPE）
        "frames_0x27": 0,
        "crc_errors": 0,          # CRC 失败的 AA55 候选（含噪声/文本行内 0xAA）
        "resyncs": 0,
        "types": {},
    }
    frames = []                   # [(seq, idx, total, payload_len, last, rsv, data)]
    i, n = 0, len(buf)
    while i + HDR_LEN <= n:
        if buf[i] != SYNC0 or buf[i + 1] != SYNC1:
            i += 1
            continue
        ftype = buf[i + 2]
        (flen,) = struct.unpack_from("<H", buf, i + 3)
        if i + HDR_LEN + flen + CRC_LEN > n:
            stats["resyncs"] += 1
            i += 1                # 截断帧：前进重同步
            continue
        body = buf[i + 2: i + HDR_LEN + flen]
        (crc_rx,) = struct.unpack_from("<H", buf, i + HDR_LEN + flen)
        if crc16_ccitt_false(body) != crc_rx:
            stats["crc_errors"] += 1
            stats["resyncs"] += 1
            i += 1
            continue
        stats["frames_ok"] += 1
        stats["types"][ftype] = stats["types"].get(ftype, 0) + 1
        if ftype == TYPE_DUMP_CHUNK:
            if flen < CHUNK_HDR_LEN or flen > FRAME_PAYLOAD_MAX:
                stats["crc_errors"] += 1   # 结构越界按坏帧计（CRC 过但 LEN 非法）
            else:
                p = i + HDR_LEN  # 载荷起点（AA55/TYPE/LEN16 之后）
                seq, idx, total, plen = struct.unpack_from("<IIII", buf, p)
                last = buf[p + 16]
                rsv = buf[p + 17]
                data = bytes(buf[p + CHUNK_HDR_LEN: p + flen])
                frames.append((seq, idx, total, plen, last, rsv, data))
                stats["frames_0x27"] += 1
        i += HDR_LEN + flen + CRC_LEN
    return frames, stats


def reassemble(frames, seq_sel):
    """按 seq 分组 → 校验 → 重组。返回 (payload|None, problems[list[str]], info)。"""
    groups: dict[int, list] = {}
    for f in frames:
        groups.setdefault(f[0], []).append(f)
    problems: list[str] = []
    seqs = sorted(groups)
    if seq_sel is None:
        if len(seqs) != 1:
            return None, problems, {"seqs": seqs}
        seq_sel = seqs[0]
    if seq_sel not in groups:
        return None, [f"seq {seq_sel} 不存在于 capture（可用 seqs={seqs}）"], {"seqs": seqs}

    g = sorted(groups[seq_sel], key=lambda f: f[1])
    total = g[0][2]
    info = {
        "seqs": seqs,
        "seq": seq_sel,
        "total_chunks": total,
        "chunks_rx": len(g),
        "idx_seen": [f[1] for f in g],
        "crc_ok_frames": True,    # 能进入本函数的 0x27 帧 CRC 已全部通过
    }
    if any(f[2] != total for f in g):
        problems.append("total_chunks 帧间不一致")
    idx_seen = [f[1] for f in g]
    if len(set(idx_seen)) != len(idx_seen):
        problems.append(f"chunk_idx 重复：{sorted(idx_seen)}")
    missing = sorted(set(range(total)) - set(idx_seen))
    if missing:
        problems.append(f"chunk 缺口 idx={missing}")
    out = bytearray(total * (FRAME_PAYLOAD_MAX - CHUNK_HDR_LEN))
    covered = bytearray(total)
    last_end = 0
    for (seq, idx, tot, plen, last, rsv, data) in g:
        if plen != len(data):
            problems.append(f"idx={idx} payload_len={plen} != 实数据 {len(data)}")
        expect_last = 1 if idx == tot - 1 else 0
        if last != expect_last:
            problems.append(f"idx={idx} last_flag={last} 期望 {expect_last}")
        if rsv != 0:
            problems.append(f"idx={idx} rsv={rsv} != 0")
        start = idx * (FRAME_PAYLOAD_MAX - CHUNK_HDR_LEN)
        out[start: start + len(data)] = data
        covered[idx] = 1
        if idx == tot - 1:
            last_end = start + len(data)  # 重组长度由末帧数据截止（非整 494 倍数时）
    if not all(covered):
        problems.append("重组存在未覆盖块（缺口）")
    if problems:
        payload = None
    else:
        payload = bytes(out[:last_end])
    info["reassembled_len"] = last_end
    info["sha256"] = hashlib.sha256(out).hexdigest()
    return payload, problems, info


def main() -> int:
    ap = argparse.ArgumentParser(description="EMS04 S3-A2 0x27 DUMP chunk capture reassembler")
    ap.add_argument("--capture", required=True, help="U→H 原始字节流 capture 文件")
    ap.add_argument("--out", help="重组 DUMP 载荷输出文件（--stats-only 时可省）")
    ap.add_argument("--seq", type=lambda v: int(v, 0), default=None,
                    help="选择重组的 seq（capture 含多个 seq 时必填）")
    ap.add_argument("--expect-len", type=int, default=DEFAULT_EXPECT_LEN,
                    help=f"期望重组长度（默认 {DEFAULT_EXPECT_LEN}）")
    ap.add_argument("--stats-only", action="store_true", help="只打印摘要，不写文件")
    args = ap.parse_args()

    if not args.stats_only and not args.out:
        ap.error("--out 必填（或 --stats-only）")
    if not os.path.isfile(args.capture):
        print(f"[s3a2] ERROR: capture 不存在: {args.capture}", file=sys.stderr)
        return 8

    with open(args.capture, "rb") as fh:
        buf = fh.read()
    frames, stats = parse_stream(buf)
    if not frames:
        print("[s3a2] FAIL: capture 中无 0x27 chunk 帧")
        print("[s3a2] stats: " + json.dumps(stats, ensure_ascii=False, sort_keys=True))
        return 6

    payload, problems, info = reassemble(frames, args.seq)
    if len(info.get("seqs", [])) > 1 and args.seq is None:
        print(f"[s3a2] FAIL: capture 含多个 seq {info['seqs']}，请用 --seq 选择")
        print("[s3a2] stats: " + json.dumps(stats, ensure_ascii=False, sort_keys=True))
        return 7

    print("[s3a2] ---- capture 扫描摘要 ----")
    print(f"[s3a2] bytes={stats['bytes']} frames_ok={stats['frames_ok']} "
          f"types={{{', '.join(f'0x{k:02X}:{v}' for k, v in sorted(stats['types'].items()))}}} "
          f"frames_0x27={stats['frames_0x27']} crc_errors={stats['crc_errors']} "
          f"resyncs={stats['resyncs']}")
    print(f"[s3a2] seqs={info.get('seqs')} selected_seq={info.get('seq')} "
          f"total_chunks={info.get('total_chunks')} chunks_rx={info.get('chunks_rx')} "
          f"idx_seen={info.get('idx_seen')}")

    if problems:
        for p in problems:
            print(f"[s3a2] PROBLEM: {p}")
        print("[s3a2] FAIL: chunk/CRC 完整性校验未通过（fail-closed，不写输出）")
        print("[s3a2] stats: " + json.dumps(stats, ensure_ascii=False, sort_keys=True))
        return 4

    assert payload is not None
    if args.expect_len is not None and len(payload) != args.expect_len:
        print(f"[s3a2] PROBLEM: 重组长度 {len(payload)} != expect-len {args.expect_len}")
        print("[s3a2] FAIL")
        return 5

    if not args.stats_only:
        with open(args.out, "wb") as fh:
            fh.write(payload)
    print(f"[s3a2] reassembled_len={len(payload)} sha256={info['sha256']}")
    print(f"[s3a2] out={'(stats-only)' if args.stats_only else args.out}")
    print("[s3a2] PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

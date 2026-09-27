#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3a2_board_test.py — EMS04-S3-A2 分块 DUMP 板上实测驱动脚本（单文件）。

驱动 EMS04_DUMP_CHUNK=1 固件形态（ems04_fp_dumpchunk.c，挂载点 ems_app.c 受控块）
在板上完整跑一遍 S3A2_SYNTH 会话，并收齐 RSP_FP_DUMP_CHUNK(0x27) 分块帧流、
重组出完整 DUMP 载荷（13932 B）落盘。

板上流程（PROTOCOL_FP，帧容器 [AA 55][TYPE][LEN LE16][PAYLOAD][CRC16-CCITT LE]，
CRC 作用域 TYPE..PAYLOAD）：
    VOYAGE_INIT(0x20, 116 B)  → ACK(0x7F, echo 0x20)
    KF_CHUNK(0x21) × 29       → ACK echo 0x21（逐帧握手，板 RX 环 1024 B）
    TICK2(0x22) × 301         → ACK echo 0x22（tick 0..300，sentinel 锚 299/300）
    QUERY(0x23, t0=300)       → RSP_FP_QUERY(0x24, 76 B：t0/status/hist_sha/kf_sha)
    DUMP(0x25, t0=300)        → 0x27 分块帧流 ×29（板上重组期连续直发，无 ACK；
                                客户端发完 DUMP 立即进入持续排水读循环直到
                                收齐 total_chunks 或超时）
0x27 载荷：[u32 seq][u32 chunk_idx][u32 total_chunks][u32 payload_len]
           [u8 last_flag][u8 rsv][payload ≤494 B]；块头 18 B；同一 DUMP 全部
           分块共用同一 seq（板静态自增计数器）。13932 B → 29 帧（28×494+100）。

会话构造内嵌：import experiments/softhil_ems03_2026-09-25/ems04_s3/tools/
s3a2_synth.py 的 synth_tick/kf_row_payload 生成同款合成会话（voyage "S3A2_SYNTH"、
tick 0..300、sentinel 299/300）；如存在同目录 out/S3A2_SYNTH/asserts.json 则读取
做 t0/mode 交叉核对（可选，缺失不阻塞）。协议原语 crc16/enc_frame/串口 Link
复用 Host/tools/fp_replay_client.py（import，零重实现）。

用法（仓库根 .venv_tf python）：
    --dry-run                        不开串口：构造会话并打印帧统计（验证①）
    --selftest                       host 自环：构造含噪声的假 0x27 帧流喂本文件
                                     重组函数并验证输出（验证②）
    --port /dev/cu.xxx [--baud 115200] [--ping]
                                     板上实测全流程；产物 = <out前缀>.dump.bin +
                                     .meta.json + .capture.bin
    --out PREFIX                     产物前缀（默认 Host/results/s3a2_board_<UTC>）

退出码：0 PASS；2 用法/环境错；3 串口打开失败；4 超时/零进展；
        5 协议不符/断言 FAIL；6 写失败。
纪律：路径全部由 __file__ 相对推导（AGENTS.md §20.2）；fail-closed，任何完整性
问题不产出"看似完整"的 .dump.bin。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from fp_replay_client import Link, crc16_ccitt, enc_frame  # 协议原语/串口链路复用

# ---- 仓库相对定位（AGENTS.md §20.2：禁绝对路径） ----
def find_repo_root(start_dir: str) -> str:
    d = os.path.abspath(start_dir)
    while d != os.path.dirname(d):
        if os.path.isdir(os.path.join(d, ".git")):
            return d
        d = os.path.dirname(d)
    return d


REPO = find_repo_root(SCRIPT_DIR)
S3A2_TOOLS_DIR = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25",
                              "ems04_s3", "tools")
S3A2_ASSERTS_JSON = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25",
                                 "ems04_s3", "out", "S3A2_SYNTH", "asserts.json")

# ---- 协议常量（权威：Appli/App/ems04_fp_proto.h + ems04_fp_dumpchunk.h +
#      experiments/softhil_ems03_2026-09-25/ems04_s2/b/PROTOCOL_FP.md，冻结） ----
ACK, NACK = 0x7F, 0x7E
FP_VI, FP_KFC, FP_TICK2, FP_QUERY, FP_DUMP = 0x20, 0x21, 0x22, 0x23, 0x25
RSP_FP_QUERY, RSP_FP_DUMP = 0x24, 0x26
TYPE_DUMP_CHUNK = 0x27
CMD_PING = 0x06
VOYAGE_ID = "S3A2_SYNTH"
VI_PAYLOAD_LEN = 116          # EMS04_FP_VOYAGE_INIT_LEN
TICK2_PAYLOAD_LEN = 92        # EMS04_FP_TICK2_LEN
RSP_QUERY_PAYLOAD_LEN = 76    # 8 + 4 + 32 + 32
EXPECT_DUMP_LEN = 13932       # EMS04_FP_RSP_DUMP_LEN = 8+4+13200+720
CHUNK_HDR_LEN = 18            # 4×u32 + last_flag + rsv
CHUNK_DATA_MAX = 494          # 512 - 18
N_TICKS = 301                 # tick 0..300（窗满）
KF_CHUNK_DATA_LEN = 1024      # 会话切表块长（s3a2_synth 同款）
MODE = "sentinel"             # 覆盖 ramp + t0-1/t0 sentinel 锚


def sha256b(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------- 会话构造
def load_synth():
    """延迟 import s3a2_synth（其模块级 import 链带 numpy/gen_ref_split_v2）。"""
    if not os.path.isdir(S3A2_TOOLS_DIR):
        raise SystemExit("ERROR: s3a2 合成工具目录不存在: %s（检查仓库完整性）"
                         % os.path.relpath(S3A2_TOOLS_DIR, REPO))
    if S3A2_TOOLS_DIR not in sys.path:
        sys.path.insert(0, S3A2_TOOLS_DIR)
    import s3a2_synth  # noqa: PLC0415（延迟导入）
    return s3a2_synth


def load_asserts_json() -> dict | None:
    """读取 S3A2_SYNTH 断言清单做交叉核对（可选，缺失返回 None）。"""
    if not os.path.isfile(S3A2_ASSERTS_JSON):
        return None
    with open(S3A2_ASSERTS_JSON, "r", encoding="utf-8") as fh:
        return json.load(fh)


def build_session() -> dict:
    """构造 S3A2_SYNTH 板上会话（与 s3a2_synth.main 逐字节同款，正路径）。

    返回 {"vi_payload", "kf_chunks", "tick_payloads", "t0_payload",
          "kf_sha256", "synth", "asserts_json"}；
    帧编码统一由 enc_frame 在发送/统计阶段完成（单一编码面）。
    """
    s = load_synth()
    t0 = int(s.T0)
    if t0 != 300 or int(s.KF_ROWS) != 40:
        raise SystemExit("ERROR: s3a2_synth 常量漂移（T0=%r KF_ROWS=%r），"
                         "与 S3-A2 板测契约不符" % (s.T0, s.KF_ROWS))

    kf_rows = b"".join(s.kf_row_payload(k) for k in range(int(s.KF_ROWS)))
    kf_sha = hashlib.sha256(kf_rows).digest()
    vi = struct.pack("<II", 1, 0x00010000) + VOYAGE_ID.ljust(32, "\0").encode("ascii") \
        + struct.pack("<d", 50.0) + bytes([0, 0]) + struct.pack("<H", 0) \
        + struct.pack("<III", 0, t0, t0 + 1) + struct.pack("<I", s.KF_ROWS) \
        + struct.pack("<dd", s.KF_FIRST, s.KF_STEP) + kf_sha
    if len(vi) != VI_PAYLOAD_LEN:
        raise SystemExit("ERROR: VOYAGE_INIT 载荷 %d B != 契约 %d B"
                         % (len(vi), VI_PAYLOAD_LEN))

    chunks = [kf_rows[i:i + KF_CHUNK_DATA_LEN]
              for i in range(0, len(kf_rows), KF_CHUNK_DATA_LEN)]
    kf_chunks = [struct.pack("<IIH", ci, len(chunks), len(ch)) + ch
                 for ci, ch in enumerate(chunks)]

    ticks = [struct.pack("<I", t)
             + struct.pack("<11d", *s.synth_tick(t, MODE))
             for t in range(t0 + 1)]
    for t, pay in enumerate(ticks):
        if len(pay) != TICK2_PAYLOAD_LEN:
            raise SystemExit("ERROR: TICK2(t=%d) 载荷 %d B != 契约 %d B"
                             % (t, len(pay), TICK2_PAYLOAD_LEN))

    t0_payload = struct.pack("<d", float(t0))

    asserts_json = load_asserts_json()
    if asserts_json is not None:
        if int(asserts_json.get("t0", t0)) != t0:
            raise SystemExit("ERROR: asserts.json t0=%r 与会话 t0=%d 不一致"
                             % (asserts_json.get("t0"), t0))
        if str(asserts_json.get("mode", MODE)) != MODE:
            raise SystemExit("ERROR: asserts.json mode=%r 与会话 mode=%r 不一致"
                             % (asserts_json.get("mode"), MODE))

    return {
        "voyage_id": VOYAGE_ID, "mode": MODE, "t0": t0,
        "vi_payload": vi, "kf_chunks": kf_chunks, "tick_payloads": ticks,
        "t0_payload": t0_payload, "kf_sha256": kf_sha.hex(),
        "asserts_json": asserts_json, "synth": s,
    }


# ---------------------------------------------------- 0x27 分块收集与重组
def build_chunk_frames(dump_payload: bytes, seq: int) -> list:
    """按固件 ems04_fp_dumpchunk_emit 同款切分编码（自环单测用帧源）。"""
    total = (len(dump_payload) + CHUNK_DATA_MAX - 1) // CHUNK_DATA_MAX or 1
    frames = []
    for idx in range(total):
        data = dump_payload[idx * CHUNK_DATA_MAX:(idx + 1) * CHUNK_DATA_MAX]
        hdr = struct.pack("<IIII", seq, idx, total, len(data)) \
            + bytes([1 if idx == total - 1 else 0, 0])
        frames.append(enc_frame(TYPE_DUMP_CHUNK, hdr + data))
    return frames


class ChunkCollector:
    """增量收集 0x27 分块帧（噪声容错解析由 fp_replay_client.Link 承担，
    此处只消费已过 CRC 的 (ftype, payload)）。按 seq 分组缓存。"""

    def __init__(self):
        self.groups = {}          # seq -> {idx: (total, last, rsv, plen, data)}
        self.crc_errors_before = 0  # 由调用方用 Link.crc_bad 差值回填
        self.other_types = {}

    def add_frame(self, ftype: int, payload: bytes) -> None:
        if ftype != TYPE_DUMP_CHUNK:
            self.other_types[ftype] = self.other_types.get(ftype, 0) + 1
            return
        if len(payload) < CHUNK_HDR_LEN:
            self.other_types["0x27_short"] = \
                self.other_types.get("0x27_short", 0) + 1
            return
        seq, idx, total, plen = struct.unpack_from("<IIII", payload, 0)
        last = payload[16]
        rsv = payload[17]
        data = payload[CHUNK_HDR_LEN:]
        self.groups.setdefault(seq, {})[idx] = (total, last, rsv, plen, data)

    def completed_seq(self) -> int | None:
        """最近一个已收齐（idx 连续覆盖 0..total-1）的 seq；无则 None。"""
        done = []
        for seq, g in self.groups.items():
            if not g:
                continue
            totals = {v[0] for v in g.values()}
            if len(totals) != 1:
                continue
            total = totals.pop()
            if len(g) >= total and (total - 1) in g:
                done.append(seq)
        return max(done) if done else None

    def seqs(self) -> list:
        return sorted(self.groups)


def reassemble_seq(chunks: list, seq_sel: int | None,
                   expect_len: int | None = None):
    """按 seq 分组校验并重组 0x27 分块流。

    chunks 元素 = (seq, idx, total, last, rsv, plen, data)。
    返回 (payload|None, problems[list[str]], info[dict])。
    fail-closed 判据：idx 集合 == {0..total-1}（无缺口/无重复）；last_flag
    仅末块为 1；rsv 恒 0；plen == len(data) ≤ 494；重组长度 == expect_len。
    """
    groups: dict = {}
    for (seq, idx, total, last, rsv, plen, data) in chunks:
        groups.setdefault(seq, []).append((idx, total, last, rsv, plen, data))
    problems: list = []
    seqs = sorted(groups)
    if seq_sel is None:
        if len(seqs) != 1:
            return None, ["seq 不唯一：%r（需显式选择）" % seqs], {"seqs": seqs}
        seq_sel = seqs[0]
    if seq_sel not in groups:
        return None, ["seq %r 不存在（可用 %r）" % (seq_sel, seqs)], {"seqs": seqs}

    g = sorted(groups[seq_sel], key=lambda c: c[0])
    totals = {c[1] for c in g}
    total = g[0][1]
    info = {"seq": seq_sel, "seqs": seqs, "total_chunks": total,
            "chunks_rx": len(g), "idx_seen": [c[0] for c in g]}
    if len(totals) != 1:
        problems.append("total_chunks 帧间不一致: %r" % sorted(totals))
    idx_seen = [c[0] for c in g]
    dups = sorted({i for i in idx_seen if idx_seen.count(i) > 1})
    if dups:
        problems.append("chunk_idx 重复: %r" % dups)
    missing = sorted(set(range(total)) - set(idx_seen))
    if missing:
        problems.append("chunk 缺口 idx=%r" % missing)

    out = bytearray(total * CHUNK_DATA_MAX)
    last_end = 0
    for (idx, tot, last, rsv, plen, data) in g:
        if plen != len(data):
            problems.append("idx=%d payload_len=%d != 实数据 %d B"
                            % (idx, plen, len(data)))
        if plen > CHUNK_DATA_MAX:
            problems.append("idx=%d payload_len=%d > 块数据上限 %d"
                            % (idx, plen, CHUNK_DATA_MAX))
        expect_last = 1 if idx == tot - 1 else 0
        if last != expect_last:
            problems.append("idx=%d last_flag=%d 期望 %d" % (idx, last, expect_last))
        if rsv != 0:
            problems.append("idx=%d rsv=%d != 0" % (idx, rsv))
        start = idx * CHUNK_DATA_MAX
        out[start:start + len(data)] = data
        if idx == tot - 1:
            last_end = start + len(data)
    info["reassembled_len"] = last_end
    if problems:
        return None, problems, info
    if expect_len is not None and last_end != expect_len:
        problems.append("重组长度 %d != 期望 %d" % (last_end, expect_len))
        return None, problems, info
    return bytes(out[:last_end]), problems, info


def collector_chunks(collector: ChunkCollector, seq_sel: int | None) -> list:
    """收集器缓存 → reassemble_seq 输入元组列表（可选 seq 过滤）。"""
    out = []
    for seq, g in collector.groups.items():
        if seq_sel is not None and seq != seq_sel:
            continue
        for idx, (total, last, rsv, plen, data) in sorted(g.items()):
            out.append((seq, idx, total, last, rsv, plen, data))
    return out


# ---------------------------------------------------------------- 自环单测
def selftest() -> int:
    """验证②：构造假 0x27 帧流（含噪声字节）喂重组函数，逐场景验证输出。"""
    print("[selftest] ---- 场景 1：完整 29 块流 + 噪声/坏帧/分片喂入 ----")
    ref = bytearray(EXPECT_DUMP_LEN)
    for i in range(EXPECT_DUMP_LEN):
        ref[i] = (i * 7 + 3) & 0xFF
    for off in (100, 5000, 9000, 13000, 13920):  # 数据内嵌帧同步样式字节对（加压）
        ref[off:off + 2] = b"\xAA\x55"
    ref = bytes(ref)
    t0d = struct.pack("<d", 300.0)
    payload = t0d + bytes(4) + ref[12:]           # 模拟 [f64 t0][status/rsv][hist][kf]
    if len(payload) != EXPECT_DUMP_LEN:
        raise SystemExit("selftest 内部错：payload 长度 %d" % len(payload))

    frames = build_chunk_frames(payload, seq=0)
    bad = bytearray(frames[0])
    bad[-2:] = b"\x00\x00"                         # CRC 必错的真尺寸 0x27 帧
    fs = bytearray(enc_frame(TYPE_DUMP_CHUNK, b"\xDE"))
    fs[-2:] = b"\xAD\xDE"                          # 假同步小帧（CRC 错 → 重同步）
    stream = bytearray()
    stream += b"EMS04 board boot log line 0.1\n\x00"   # 文本噪声（无 AA55 对）
    stream += fs                                       # 假同步（CRC 错 → 重同步）
    stream += bad                                      # CRC 错的真尺寸 0x27 帧
    for i, f in enumerate(frames):
        stream += f
        if i in (3, 10, 27):
            stream += b"log: chunk %d ok\n" % i        # 帧间文本噪声
    stream += bytes([0xAA])                            # 尾部孤立半同步字节

    collector = ChunkCollector()
    rxbuf = bytearray()
    pos, step, sim_crc_bad = 0, 0, 0
    while pos < len(stream):
        k = (1, 2, 3, 5, 7, 11, 13)[step % 7]          # 不规则分片喂入（流式仿真）
        rxbuf.extend(stream[pos:pos + k])
        pos += k
        step += 1
        # 增量 CRC 解析（与 fp_replay_client.Link._parse_frames 同构的最小内联）
        while len(rxbuf) >= 7:
            if rxbuf[0] != 0xAA or rxbuf[1] != 0x55:
                del rxbuf[0]
                continue
            (ln,) = struct.unpack_from("<H", rxbuf, 3)
            if len(rxbuf) < 7 + ln:
                break
            body = bytes(rxbuf[2:5 + ln])
            (crx,) = struct.unpack_from("<H", rxbuf, 5 + ln)
            del rxbuf[:7 + ln]
            if crc16_ccitt(body) != crx:
                sim_crc_bad += 1
                continue
            collector.add_frame(body[0], body[3:])

    seq = collector.completed_seq()
    print("[selftest] completed_seq=%r seqs=%r 0x27 帧=%d 模拟CRC错=%d"
          % (seq, collector.seqs(), len(collector.groups.get(0, {})),
             sim_crc_bad))
    ok1 = True
    if seq != 0:
        print("[selftest] FAIL: completed_seq 期望 0")
        ok1 = False
    chunks = collector_chunks(collector, seq)
    pay, problems, info = reassemble_seq(chunks, seq, expect_len=EXPECT_DUMP_LEN)
    if problems or pay != payload:
        print("[selftest] FAIL: problems=%r 或重组不等于参考" % problems)
        ok1 = False
    if pay is not None:
        print("[selftest] 重组 %d B sha256=%s" % (len(pay), sha256b(pay)[:16] + "…"))
        print("[selftest] 位级一致(含数据内嵌 AA55 噪声加压): %s"
              % ("PASS" if pay == payload else "FAIL"))
        ok1 = ok1 and pay == payload
    print("[selftest] 场景1: %s" % ("PASS" if ok1 else "FAIL"))

    print("[selftest] ---- 场景 2：丢块（idx=5）→ fail-closed 不产出 ----")
    frames2 = [f for i, f in enumerate(frames) if i != 5]
    ch2 = _frames_to_chunks(frames2)
    pay2, problems2, _ = reassemble_seq(ch2, 0, expect_len=EXPECT_DUMP_LEN)
    ok2 = pay2 is None and any("缺口" in p for p in problems2)
    print("[selftest] problems=%r" % problems2)
    print("[selftest] 场景2: %s" % ("PASS" if ok2 else "FAIL"))

    print("[selftest] ---- 场景 3：重复块（idx=3 两次）→ 检出重复 ----")
    ch3 = _frames_to_chunks(frames) + [_frames_to_chunks(frames)[3]]
    pay3, problems3, _ = reassemble_seq(ch3, 0, expect_len=EXPECT_DUMP_LEN)
    ok3 = pay3 is None and any("重复" in p for p in problems3)
    print("[selftest] problems=%r" % problems3)
    print("[selftest] 场景3: %s" % ("PASS" if ok3 else "FAIL"))

    print("[selftest] ---- 场景 4：last_flag 违例（idx=0 置 1）→ 检出 ----")
    ch4 = list(_frames_to_chunks(frames))
    s0, i0, t0c, l0, r0, p0, d0 = ch4[0]
    ch4[0] = (s0, i0, t0c, 1, r0, p0, d0)
    pay4, problems4, _ = reassemble_seq(ch4, 0, expect_len=EXPECT_DUMP_LEN)
    ok4 = pay4 is None and any("last_flag" in p for p in problems4)
    print("[selftest] problems=%r" % problems4)
    print("[selftest] 场景4: %s" % ("PASS" if ok4 else "FAIL"))

    print("[selftest] ---- 场景 5：双 seq 分组 + 显式选择 ----")
    ch5 = _frames_to_chunks(frames) \
        + _frames_to_chunks(build_chunk_frames(payload[:100], seq=7))
    pay5a, prob5a, info5a = reassemble_seq(ch5, 0, expect_len=EXPECT_DUMP_LEN)
    pay5b, prob5b, info5b = reassemble_seq(ch5, 7, expect_len=100)
    ok5 = (pay5a == payload and not prob5a
           and pay5b == payload[:100] and not prob5b
           and info5a["seqs"] == [0, 7])
    print("[selftest] seqs=%r sel0=%dB sel7=%dB" % (info5a["seqs"],
          len(pay5a or b""), len(pay5b or b"")))
    print("[selftest] 场景5: %s" % ("PASS" if ok5 else "FAIL"))

    all_ok = all([ok1, ok2, ok3, ok4, ok5])
    print("[selftest] VERDICT=%s" % ("PASS" if all_ok else "FAIL"))
    return 0 if all_ok else 5


def _frames_to_chunks(frames: list, seq: int | None = None) -> list:
    """整帧列表 → reassemble_seq 输入元组（解码 0x27 载荷头）。"""
    out = []
    for f in frames:
        plen_f = len(f) - 7
        payload = f[5:5 + plen_f]
        s, idx, total, plen = struct.unpack_from("<IIII", payload, 0)
        if seq is not None:
            s = seq
        out.append((s, idx, total, payload[16], payload[17], plen,
                    payload[CHUNK_HDR_LEN:]))
    return out


# ---------------------------------------------------------------- dry-run
def dry_run() -> int:
    """验证①：不开串口，构造会话并打印帧统计 + CRC 自检。"""
    ses = build_session()
    plan = [
        ("VOYAGE_INIT", FP_VI, [ses["vi_payload"]], "ACK echo 0x20"),
        ("KF_CHUNK", FP_KFC, ses["kf_chunks"], "ACK echo 0x21 ×%d" % len(ses["kf_chunks"])),
        ("TICK2", FP_TICK2, ses["tick_payloads"], "ACK echo 0x22 ×%d" % len(ses["tick_payloads"])),
        ("FP_QUERY", FP_QUERY, [ses["t0_payload"]], "RSP_FP_QUERY(0x24, 76 B)"),
        ("FP_DUMP", FP_DUMP, [ses["t0_payload"]], "RSP_FP_DUMP_CHUNK(0x27) ×29"),
    ]
    total_tx = 0
    total_frames = 0
    crc_bad = 0
    print("[dry-run] 会话 voyage=%s mode=%s t0=%d ticks=%d kf_rows=%d"
          % (ses["voyage_id"], ses["mode"], ses["t0"],
             len(ses["tick_payloads"]), int(ses["synth"].KF_ROWS)))
    print("[dry-run] kf row payload sha256=%s" % ses["kf_sha256"])
    if ses["asserts_json"] is not None:
        print("[dry-run] asserts.json 交叉核对: t0=%s mode=%s PASS"
              % (ses["asserts_json"].get("t0"), ses["asserts_json"].get("mode")))
    else:
        print("[dry-run] asserts.json 不存在（可选交叉核对跳过）")
    for name, ftype, payloads, expect in plan:
        n = len(payloads)
        pay_bytes = sum(len(p) for p in payloads)
        frm_bytes = sum(len(enc_frame(ftype, p)) for p in payloads)
        for p in payloads:
            f = enc_frame(ftype, p)
            if crc16_ccitt(f[2:-2]) != struct.unpack("<H", f[-2:])[0]:
                crc_bad += 1
        total_tx += frm_bytes
        total_frames += n
        print("[dry-run]   %-13s TYPE=0x%02X 帧=%-4d 载荷=%-6d 帧=%-7d 期望响应: %s"
              % (name, ftype, n, pay_bytes, frm_bytes, expect))
    exp_chunks = (EXPECT_DUMP_LEN + CHUNK_DATA_MAX - 1) // CHUNK_DATA_MAX
    print("[dry-run] 合计: 帧=%d TX=%d B（115200 ≈ %.1f s 线路时间）CRC坏=%d"
          % (total_frames, total_tx, total_tx * 10 / 115200.0, crc_bad))
    print("[dry-run] DUMP 重组期望: %d B = %d 块（%d×%d + %d）"
          % (EXPECT_DUMP_LEN, exp_chunks, exp_chunks - 1, CHUNK_DATA_MAX,
             EXPECT_DUMP_LEN - (exp_chunks - 1) * CHUNK_DATA_MAX))
    if crc_bad:
        print("DRY-RUN: FAIL（CRC 自检 %d 帧错）" % crc_bad)
        return 5
    print("DRY-RUN: PASS（会话构造/帧边界/CRC 自检如上）")
    return 0


# ---------------------------------------------------------------- 板上流程
def _wait_frame(link: Link, start: int, pred, stall_s: float, gate_s: float,
                what: str):
    """排水等待：从 link.frames[start:] 中找到首个满足 pred 的帧。
    返回 (帧在 frames 中的下标, ftype, payload) 或 (None, None, None)。"""
    t0 = time.time()
    last = t0
    while True:
        got = link.drain()
        now = time.time()
        for i in range(start, len(link.frames)):
            ftype, payload = link.frames[i]
            if pred(ftype, payload):
                return i, ftype, payload
        if got:
            last = now
            continue
        if now - last > stall_s:
            print("ERROR: 等待 %s 零进展（%.1fs 无字节）" % (what, stall_s),
                  file=sys.stderr)
            return None, None, None
        if now - t0 > gate_s:
            print("ERROR: 等待 %s 超时（gate %.1fs）" % (what, gate_s),
                  file=sys.stderr)
            return None, None, None
        time.sleep(0.002)


def _expect_ack(link: Link, start: int, cmd: int, stall_s: float,
                gate_s: float, what: str) -> int:
    """等待某命令的 ACK（echo=cmd）；期间出现 NACK 即失败。返回新消费下标。"""
    def pred(ftype, payload):
        return ((ftype == ACK and payload and payload[0] == cmd)
                or ftype == NACK)
    i, ftype, payload = _wait_frame(link, start, pred, stall_s, gate_s, what)
    if i is None:
        raise TimeoutError("等待 %s ACK 超时/零进展" % what)
    if ftype == NACK:
        raise RuntimeError("%s → NACK err=0x%02X（会话锁存，仅 VOYAGE_INIT 可清）"
                           % (what, payload[1] if len(payload) > 1 else 0))
    return i + 1


def run_board(args) -> int:
    ses = build_session()
    out_prefix = args.out
    os.makedirs(os.path.dirname(os.path.abspath(out_prefix)), exist_ok=True)
    capture_path = out_prefix + ".capture.bin"
    dump_path = out_prefix + ".dump.bin"
    meta_path = out_prefix + ".meta.json"

    meta = {
        "schema": "ems04_s3a2_board_test_v1",
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "port": args.port, "baud": args.baud,
        "session": {"voyage_id": ses["voyage_id"], "mode": ses["mode"],
                    "t0": ses["t0"], "n_ticks": len(ses["tick_payloads"]),
                    "kf_rows": int(ses["synth"].KF_ROWS),
                    "kf_sha256": ses["kf_sha256"]},
        "asserts_json_echo": (None if ses["asserts_json"] is None
                              else {"t0": ses["asserts_json"].get("t0"),
                                    "mode": ses["asserts_json"].get("mode")}),
        "expect_dump_len": EXPECT_DUMP_LEN,
    }

    try:
        link = Link(args.port, args.baud, capture_path)
    except Exception as exc:  # noqa: BLE001
        print("ERROR: 打开串口失败: %s" % exc, file=sys.stderr)
        return 3
    print("[board] 串口 %s @%d capture=%s" % (args.port, args.baud, capture_path))

    verdict = "FAIL"
    exit_code = 0
    dump_meta = {}
    assert_results = {}
    try:
        if args.settle > 0:
            discarded = 0
            t_settle = time.time()
            while time.time() - t_settle < args.settle:
                n = link.ser.in_waiting
                if n:
                    discarded += len(link.ser.read(n))
                else:
                    time.sleep(0.05)
            print("[board] settle %.1fs：丢弃上电字节 %d" % (args.settle, discarded))
        link.ser.reset_input_buffer()

        if args.ping:
            link.write(enc_frame(CMD_PING, b""))
            _, ftype, payload = _wait_frame(
                link, 0, lambda t, p: t in (ACK, NACK),
                args.stall, args.cmd_gate, "PING")
            if ftype != ACK:
                print("ERROR: PING 失败（链路不通）", file=sys.stderr)
                return 5
            print("[board] PING OK")

        # ---- VOYAGE_INIT ----
        consumed = len(link.frames)
        link.write(enc_frame(FP_VI, ses["vi_payload"]))
        consumed = _expect_ack(link, consumed, FP_VI, args.stall,
                               args.cmd_gate, "VOYAGE_INIT")
        print("[board] VOYAGE_INIT ACK")

        # ---- KF_CHUNK ×29 ----
        t0 = time.time()
        for ci, pay in enumerate(ses["kf_chunks"]):
            link.write(enc_frame(FP_KFC, pay))
            consumed = _expect_ack(link, consumed, FP_KFC, args.stall,
                                   args.cmd_gate, "KF_CHUNK[%d]" % ci)
            if (ci + 1) % 10 == 0 or ci + 1 == len(ses["kf_chunks"]):
                print("[board] KF_CHUNK %d/%d（%.1fs）"
                      % (ci + 1, len(ses["kf_chunks"]), time.time() - t0))

        # ---- TICK2 ×301 ----
        t0 = time.time()
        for t, pay in enumerate(ses["tick_payloads"]):
            link.write(enc_frame(FP_TICK2, pay))
            consumed = _expect_ack(link, consumed, FP_TICK2, args.stall,
                                   args.cmd_gate, "TICK2[%d]" % t)
            if (t + 1) % 50 == 0 or t + 1 == len(ses["tick_payloads"]):
                print("[board] TICK2 %d/%d（%.1fs）"
                      % (t + 1, len(ses["tick_payloads"]), time.time() - t0))

        # ---- QUERY ----
        link.write(enc_frame(FP_QUERY, ses["t0_payload"]))
        i, ftype, payload = _wait_frame(
            link, consumed,
            lambda t, p: t in (RSP_FP_QUERY, NACK),
            args.stall, args.cmd_gate, "RSP_FP_QUERY")
        if i is None:
            raise TimeoutError("等待 RSP_FP_QUERY 超时/零进展")
        consumed = i + 1
        if ftype == NACK:
            raise RuntimeError("QUERY → NACK err=0x%02X"
                               % (payload[1] if len(payload) > 1 else 0))
        if len(payload) != RSP_QUERY_PAYLOAD_LEN:
            raise RuntimeError("RSP_FP_QUERY 载荷 %d B != 契约 %d B"
                               % (len(payload), RSP_QUERY_PAYLOAD_LEN))
        q_t0 = struct.unpack_from("<d", payload, 0)[0]
        q_status = payload[8]
        q = {"t0": q_t0, "status": q_status,
             "hist_sha256": payload[12:44].hex(),
             "kf_sha256": payload[44:76].hex()}
        meta["query"] = q
        if struct.pack("<d", q_t0) != ses["t0_payload"]:
            raise RuntimeError("RSP_FP_QUERY t0=%r 位型 != 会话 t0" % q_t0)
        if q_status != 0:
            raise RuntimeError("RSP_FP_QUERY status=%d != 0" % q_status)
        print("[board] QUERY OK：t0=%.1f status=0 hist_sha=%s… kf_sha=%s…"
              % (q_t0, q["hist_sha256"][:16], q["kf_sha256"][:16]))

        # ---- DUMP → 0x27 分块流：发完立即持续排水直到收齐 total_chunks ----
        collector = ChunkCollector()
        crc_before = link.crc_bad
        junk_before = link.framing_bad
        link.write(enc_frame(FP_DUMP, ses["t0_payload"]))
        print("[board] DUMP 已发送，进入持续排水（板连续直发 0x27 流）…")
        t_dump = time.time()
        last_bytes = time.time()
        done_seq = None
        nack_seen = None
        while done_seq is None:
            got = link.drain()
            for i in range(consumed, len(link.frames)):
                ftype, payload = link.frames[i]
                if ftype == NACK and nack_seen is None:
                    nack_seen = payload[1] if len(payload) > 1 else 0
                collector.add_frame(ftype, payload)
            consumed = len(link.frames)
            if got:
                last_bytes = time.time()
            done_seq = collector.completed_seq()
            n_rx = sum(len(g) for g in collector.groups.values())
            if done_seq is None:
                now = time.time()
                if now - last_bytes > args.stall:
                    raise TimeoutError(
                        "DUMP 流零进展（%.1fs 无字节），已收 %d 块，其他 TYPE=%r"
                        % (args.stall, n_rx, collector.other_types))
                if now - t_dump > args.dump_gate:
                    raise TimeoutError(
                        "DUMP 流超时（gate %.1fs），已收 %d 块，其他 TYPE=%r"
                        "（若含 0x26 提示：板可能非 EMS04_DUMP_CHUNK 形态）"
                        % (args.dump_gate, n_rx, collector.other_types))
                time.sleep(0.002)
        print("[board] 0x27 流收齐：seq=%s 块=%d（%.1fs）其他 TYPE=%r"
              % (done_seq, len(collector.groups[done_seq]),
                 time.time() - t_dump, collector.other_types))

        dump_crc_errors = link.crc_bad - crc_before
        dump_junk = link.framing_bad - junk_before
        chunks = collector_chunks(collector, done_seq)
        pay, problems, info = reassemble_seq(chunks, done_seq,
                                             expect_len=args.expect_len)
        dump_meta = dict(info)
        dump_meta.update({
            "crc_errors_in_phase": dump_crc_errors,
            "junk_bytes_in_phase": dump_junk,
            "nack_in_phase": nack_seen,
            "other_types": {str(k): v for k, v in collector.other_types.items()},
            "expect_len": args.expect_len,
            "dump_elapsed_s": round(time.time() - t_dump, 3),
        })
        meta["dump"] = dump_meta

        if nack_seen is not None:
            problems = problems + ["DUMP 阶段收到 NACK err=0x%02X" % nack_seen]

        # ---- 断言与落盘（fail-closed：有问题不写 .dump.bin） ----
        idx_seen = info.get("idx_seen", [])
        total = info.get("total_chunks", 0)
        cont = (pay is not None and not problems
                and sorted(idx_seen) == list(range(total)))
        last_ok = pay is not None and all(
            c[3] == (1 if c[1] == total - 1 else 0) for c in chunks)
        len_ok = pay is not None and len(pay) == args.expect_len

        assert_results = {
            "chunk_seq_continuous": (cont, "块 %d/%d，idx=%s"
                                     % (len(idx_seen), total,
                                        "0..%d 连续" % (total - 1) if cont
                                        else str(idx_seen))),
            "last_flag_last_only": (last_ok, "仅末块 idx=%d 为 1"
                                    % (total - 1) if last_ok else "违例"),
            "reassembled_len_%d" % args.expect_len: (
                len_ok, "%s B" % (len(pay) if pay is not None else "∅")),
            "no_crc_error_in_dump_phase": (dump_crc_errors == 0,
                                           "%d 个坏帧" % dump_crc_errors),
            "no_nack": (nack_seen is None, "err=%r" % nack_seen),
        }
        if problems:
            print("[board] PROBLEM:")
            for p in problems:
                print("[board]   - %s" % p)
        all_pass = pay is not None and not problems and cont and last_ok and len_ok
        if all_pass:
            try:
                with open(dump_path, "wb") as fh:
                    fh.write(pay)
            except OSError as exc:
                print("ERROR: 写 %s 失败: %s" % (dump_path, exc), file=sys.stderr)
                return 6
            dump_meta["sha256"] = sha256b(pay)
            print("[board] DUMP 载荷 %d B → %s（sha256=%s）"
                  % (len(pay), dump_path, dump_meta["sha256"]))
        else:
            print("[board] fail-closed：完整性未通过，不写 .dump.bin",
                  file=sys.stderr)
    except (TimeoutError, RuntimeError, IOError, OSError) as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        exit_code = 4 if isinstance(exc, TimeoutError) else 5
    finally:
        meta["dump"] = meta.get("dump") or dump_meta or None
        meta["bytes_written"] = link.bytes_written
        meta["bytes_read"] = link.bytes_read
        meta["capture"] = capture_path
        meta["capture_sha256"] = (sha256b(open(capture_path, "rb").read())
                                  if os.path.isfile(capture_path) else None)
        link.close()

    meta["asserts"] = {k: {"pass": bool(v[0]), "detail": v[1]}
                       for k, v in assert_results.items()} or None
    meta["verdict"] = ("PASS" if exit_code == 0 and
                       all(v[0] for v in assert_results.values()) else "FAIL")
    try:
        with open(meta_path, "w", encoding="utf-8") as fh:
            json.dump(meta, fh, indent=2, ensure_ascii=False)
        print("[board] 元数据 → %s" % meta_path)
    except OSError as exc:
        print("ERROR: 写 %s 失败: %s" % (meta_path, exc), file=sys.stderr)
        return 6

    print("[board] ---- 断言清单 ----")
    for k, v in assert_results.items():
        print("[board]   %-28s %s  %s" % (k, "PASS" if v[0] else "FAIL", v[1]))
    if meta["verdict"] == "PASS":
        print("VERDICT=PASS")
        return 0
    print("VERDICT=FAIL")
    return 5 if exit_code == 0 else exit_code


# ---------------------------------------------------------------- 入口
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="EMS04-S3-A2 分块 DUMP 板上实测驱动（0x27 收集+重组）")
    ap.add_argument("--port", help="串口设备（--dry-run/--selftest 时可省略）")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", help="产物前缀：<前缀>.dump.bin/.meta.json/.capture.bin"
                                  "（默认 Host/results/s3a2_board_<UTC>）")
    ap.add_argument("--ping", action="store_true", help="链检 CMD_PING(0x06)")
    ap.add_argument("--dry-run", action="store_true",
                    help="不开串口：构造会话并打印帧统计")
    ap.add_argument("--selftest", action="store_true",
                    help="host 自环：假 0x27 帧流（含噪声）喂重组函数")
    ap.add_argument("--stall", type=float, default=5.0,
                    help="秒：无新字节判零进展（默认 5）")
    ap.add_argument("--cmd-gate", type=float, default=10.0,
                    help="秒：单命令响应等待上限（默认 10）")
    ap.add_argument("--dump-gate", type=float, default=30.0,
                    help="秒：DUMP 分块流总上限（默认 30）")
    ap.add_argument("--settle", type=float, default=0.5,
                    help="秒：开抓前静置丢弃上电文本（默认 0.5）")
    ap.add_argument("--expect-len", type=int, default=EXPECT_DUMP_LEN,
                    help="期望重组长度（默认 %d）" % EXPECT_DUMP_LEN)
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()
    if args.dry_run:
        return dry_run()
    if not args.port:
        ap.error("--port 必填（或用 --dry-run / --selftest）")
    if args.out is None:
        stamp = time.strftime("%Y%m%dT%H%M%S", time.gmtime())
        args.out = os.path.join(SCRIPT_DIR, "..", "results",
                                "s3a2_board_" + stamp)
    args.out = os.path.normpath(args.out)
    return run_board(args)


if __name__ == "__main__":
    sys.exit(main())

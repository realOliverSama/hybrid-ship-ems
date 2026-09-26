#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fp_replay_client.py — EMS04-S2 C 阶段（C-fw）串口回放客户端。

用途：把 B2 阶段（`experiments/softhil_ems03_2026-09-25/ems04_s2/b/`）生成的
**同款回放脚本束**经 UART 115200 全量喂给 EMS04_CHIL_FULLCTRL 的 `EMS04_FP_PARITY`
固件形态，并把板侧全部响应帧字节落盘（capture），供离线核验器
`../../c/tools/verify_capture.py` 逐位对拍 canonical 真值。

线格式权威：`../PROTOCOL_FP.md`（帧容器 [AA 55][TYPE][LEN LE16][PAYLOAD][CRC16 LE]，
CRC16-CCITT-FALSE 作用域 TYPE..PAYLOAD）+ 既有 `ems_proto` 命令段。
本客户端**只做传输**：不重编码、不插帧、不改字节；脚本里的帧按原字节透传。

脚本束格式（B2V1，b/gen_b2_replay.py 产出）：`[op u8][载荷]...`
    op 0x01 FEED   `[u32 len][整帧字节]`  → 送板（1 FEED = 1 整帧）
    op 0x02 EXPECT `[kind u8][载荷]`      → 板侧响应的期望（本客户端据此做流控与校验）
    op 0x03 NOTE   `[u32 len][UTF-8]`     → 注释，跳过
非 B2V1 文件按**裸帧字节流**透传（无期望信息，退化流控见 `--raw-pacing`）。

流控（关键）：板侧 RX 环仅 1024 B，而 RSP_FP_DUMP 一帧 13939 B（115200 下 ~1.21 s
阻塞发送）。若按「写满批次就走」会必然溢出丢字节（帧同步破坏 → NACK → 会话锁存）。
故每批写入后，客户端**等到该批全部期望响应收齐**再继续（脚本自带 EXPECT 信息，
零猜测）；等待期间持续排水、追加 capture。

UART 纪律：用 `in_waiting` 判读，禁止盲目 `read(n)` 阻塞；`write()` 返回值逐一校验；
异常重试上限明确；超时/零进展保护；异常退出时打印已发送字节数（capture 已增量落盘）。

用法（仓库根 .venv_tf python）：
    python3 Host/tools/fp_replay_client.py --list-ports
    python3 Host/tools/fp_replay_client.py ping --port /dev/cu.usbserial-XXXX
    python3 Host/tools/fp_replay_client.py replay \
        --scripts ../../../../../experiments/softhil_ems03_2026-09-25/ems04_s2/b/vectors_b2/script_B2_FLTA_S2_D01009_M1.bin \
                   <...M2.bin> \
        --capture /tmp/fp_capture.bin --port /dev/cu.usbserial-XXXX --baud 115200 \
        --batch 512 --progress 50
    python3 Host/tools/fp_replay_client.py replay --scripts <...> --dry-run   # 不连板自检

退出码：0 正常完成；2 用法/文件错；3 串口打开失败；4 超时/零进展；5 响应不符
（NACK/CRC 错/期望类型不符，可用 --on-mismatch continue 仅记录）；6 写失败。
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

SYNC0, SYNC1 = 0xAA, 0x55
HDR = 5   # AA 55 TYPE LEN(2)
CRC_LEN = 2

# ems_proto 命令/响应码（PROTOCOL_FP §0；既有段 0x01..0x0B 不动）
CMD_PING = 0x06
ACK, NACK = 0x7F, 0x7E
FP_CMD_NAMES = {
    0x20: "VOYAGE_INIT", 0x21: "KF_CHUNK", 0x22: "TICK2",
    0x23: "FP_QUERY", 0x24: "RSP_FP_QUERY", 0x25: "FP_DUMP", 0x26: "RSP_FP_DUMP",
}
FP_RSP_NAMES = {ACK: "ACK", NACK: "NACK", 0x24: "RSP_FP_QUERY", 0x26: "RSP_FP_DUMP"}

# B2V1 ops / EXPECT kinds（b/gen_b2_replay.py）
OP_FEED, OP_EXPECT, OP_NOTE = 0x01, 0x02, 0x03
EK_ACK, EK_NACK, EK_TYPE = 0x01, 0x02, 0x03
EK_QUERY, EK_DUMP, EK_QSHA = 0x04, 0x05, 0x06
EK_COUNTER, EK_ERRCOUNT, EK_LATCH, EK_STATE, EK_SINK = 0x07, 0x08, 0x09, 0x0A, 0x0B


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


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ----------------------------------------------------------------- 脚本解析
class Script:
    """B2V1 脚本束解析：FEED 帧（原字节）+ 其后的 EXPECT 响应期望。"""

    def __init__(self, path: str):
        self.path = path
        self.raw_mode = False
        self.units = []          # [(frame_bytes, [expectation, ...]), ...]
        self.frames = []         # 仅帧字节（顺序）= units 的投影
        self.note_count = 0
        self.sid = None
        self.n_ops = 0
        self.type_counts = {}
        self.exp_counts = {}
        self.framing_bad = 0
        self.crc_bad = 0
        self.n_expected_rsp = 0
        self.parse()

    def parse(self) -> None:
        blob = open(self.path, "rb").read()
        if blob[:4] != b"B2V1":
            # 裸帧字节流：整体透传（无期望信息）
            self.raw_mode = True
            self.raw = blob
            off = 0
            while off < len(blob):
                if off + HDR > len(blob):
                    self.framing_bad += 1
                    break
                if blob[off] != SYNC0 or blob[off + 1] != SYNC1:
                    self.framing_bad += 1
                    break
                ln = struct.unpack_from("<H", blob, off + 3)[0]
                end = off + HDR + ln + CRC_LEN
                if end > len(blob):
                    self.framing_bad += 1
                    break
                self.frames.append(blob[off:end])
                self.type_counts[blob[off + 2]] = self.type_counts.get(blob[off + 2], 0) + 1
                if crc16_ccitt(blob[off + 2:off + HDR + ln]) != struct.unpack_from(
                        "<H", blob, off + HDR + ln)[0]:
                    self.crc_bad += 1
                off = end
            return

        _, n_ops, sid_len = struct.unpack_from("<III", blob, 4)
        self.n_ops = n_ops
        self.sid = blob[16:16 + sid_len].decode("ascii", "replace")
        p = 16 + sid_len
        pend = len(blob)
        cur = None
        for _ in range(n_ops):
            if p >= pend:
                self.framing_bad += 1
                break
            op = blob[p]
            p += 1
            if op == OP_NOTE:
                ln = struct.unpack_from("<I", blob, p)[0]
                p += 4 + ln
                self.note_count += 1
                continue
            if op == OP_FEED:
                ln = struct.unpack_from("<I", blob, p)[0]
                p += 4
                frame = blob[p:p + ln]
                p += ln
                if len(frame) < HDR + CRC_LEN or frame[0] != SYNC0 or frame[1] != SYNC1:
                    self.framing_bad += 1
                    continue
                flen = struct.unpack_from("<H", frame, 3)[0]
                if flen != len(frame) - HDR - CRC_LEN:
                    self.framing_bad += 1
                elif crc16_ccitt(frame[2:HDR + flen]) != struct.unpack_from("<H", frame, HDR + flen)[0]:
                    self.crc_bad += 1
                self.type_counts[frame[2]] = self.type_counts.get(frame[2], 0) + 1
                cur = (frame, [])
                self.units.append(cur)
                self.frames.append(frame)
                continue
            if op == OP_EXPECT:
                kind = blob[p]
                p += 1
                self.exp_counts[kind] = self.exp_counts.get(kind, 0) + 1
                exp = {"kind": kind}
                if kind in (EK_ACK, EK_NACK, EK_TYPE):
                    exp["arg"] = blob[p]
                    p += 1
                elif kind in (EK_QUERY, EK_DUMP):
                    exp["t0"] = struct.unpack_from("<d", blob, p)[0]
                    p += 8
                    if kind == EK_DUMP:  # 真值文件路径（板侧不消费）
                        n1 = struct.unpack_from("<I", blob, p)[0]
                        p += 4 + n1
                        n2 = struct.unpack_from("<I", blob, p)[0]
                        p += 4 + n2
                elif kind == EK_QSHA:
                    p += 128
                elif kind == EK_COUNTER:
                    p += 9
                elif kind == EK_ERRCOUNT:
                    p += 5
                elif kind in (EK_LATCH, EK_STATE):
                    p += 1
                elif kind == EK_SINK:
                    p += 40
                else:
                    raise SystemExit("ERROR: %s 未知 EXPECT kind 0x%02X" % (self.path, kind))
                if kind in (EK_ACK, EK_NACK, EK_TYPE, EK_QUERY, EK_DUMP):
                    self.n_expected_rsp += 1
                    if cur is not None:
                        cur[1].append(exp)
                continue
            raise SystemExit("ERROR: %s 未知 op 0x%02X @%d" % (self.path, op, p - 1))

    def summary(self) -> dict:
        return {
            "path": self.path,
            "sid": self.sid,
            "raw_mode": self.raw_mode,
            "bytes": os.path.getsize(self.path),
            "sha256": sha256_file(self.path),
            "n_frames": len(self.frames),
            "n_expected_rsp": self.n_expected_rsp,
            "type_counts": {("0x%02X %s" % (k, FP_CMD_NAMES.get(k, "?"))): v
                            for k, v in sorted(self.type_counts.items())},
            "expect_counts": {"0x%02X" % k: v for k, v in sorted(self.exp_counts.items())},
            "framing_bad": self.framing_bad,
            "crc_bad": self.crc_bad,
        }


# ----------------------------------------------------------------- 串口链接
class Link:
    """串口读写 + U→H 帧解析（含 CRC 校验）+ 增量 capture。"""

    def __init__(self, port: str, baud: int, capture_path: str | None,
                 write_retries: int = 3):
        import serial  # 延迟导入：--dry-run 不需要 pyserial
        self.serial_mod = serial
        self.ser = serial.Serial(port, baud, timeout=0)
        self.write_retries = write_retries
        self.rxbuf = bytearray()
        self.capture = open(capture_path, "wb") if capture_path else None
        self.bytes_written = 0
        self.bytes_read = 0
        self.frames = []              # [(type, payload)]
        self.crc_bad = 0
        self.framing_bad = 0
        self.nack = 0
        self.rsp_counts = {}
        self.ack = 0

    def close(self):
        try:
            self.ser.close()
        finally:
            if self.capture is not None:
                self.capture.close()
                self.capture = None

    # ---- 写：返回值校验 + 有限重试
    def write(self, data: bytes) -> None:
        off = 0
        attempts = 0
        while off < len(data):
            n = self.ser.write(data[off:])
            if n is None:
                n = 0
            if n <= 0:
                attempts += 1
                if attempts > self.write_retries:
                    raise IOError("串口写入失败（已写 %d/%d B，重试 %d 次）"
                                  % (self.bytes_written + off, len(data), attempts))
                time.sleep(0.05)
                continue
            off += n
            attempts = 0
        self.bytes_written += len(data)

    # ---- 读：只用 in_waiting 判读（禁止盲目 read(n) 阻塞）
    def drain(self) -> int:
        n = self.ser.in_waiting
        if not n:
            return 0
        data = self.ser.read(n)
        if not data:
            return 0
        self.bytes_read += len(data)
        if self.capture is not None:
            self.capture.write(data)
            self.capture.flush()
        self.rxbuf.extend(data)
        self._parse_frames()
        return len(data)

    def _parse_frames(self) -> None:
        while True:
            buf = self.rxbuf
            if len(buf) < HDR + CRC_LEN:
                return
            if buf[0] != SYNC0 or buf[1] != SYNC1:
                # 非帧字节（如固件文本日志行）：计数并逐字节重同步
                self.framing_bad += 1
                del buf[0]
                continue
            ln = struct.unpack_from("<H", buf, 3)[0]
            total = HDR + ln + CRC_LEN
            if len(buf) < total:
                return
            body = bytes(buf[2:HDR + ln])
            crx = struct.unpack_from("<H", buf, HDR + ln)[0]
            del buf[:total]
            if crc16_ccitt(body) != crx:
                self.crc_bad += 1
                continue
            ftype, payload = body[0], body[3:]
            self.frames.append((ftype, payload))
            self.rsp_counts[ftype] = self.rsp_counts.get(ftype, 0) + 1
            if ftype == ACK:
                self.ack += 1
            elif ftype == NACK:
                self.nack += 1

    def wait_frames(self, want_frames: int, stall_s: float, gate_s: float,
                    progress=None) -> bool:
        """等到已解析帧数 ≥ want_frames；期间持续排水。返回 False = 超时/零进展。"""
        t0 = time.time()
        last = t0
        while len(self.frames) < want_frames:
            got = self.drain()
            now = time.time()
            if got:
                last = now
                if progress is not None:
                    progress()
                continue
            if now - last > stall_s:
                return False
            if now - t0 > gate_s:
                return False
            time.sleep(0.002)
        return True

    def drain_idle(self, idle_s: float = 0.05, max_s: float = 1.0) -> int:
        total = 0
        t0 = time.time()
        while time.time() - t0 < max_s:
            n = self.drain()
            total += n
            if n == 0:
                time.sleep(idle_s)
                if self.drain() == 0:
                    break
        return total


# ----------------------------------------------------------------- 子命令
def cmd_list_ports() -> int:
    try:
        from serial.tools import list_ports
    except ImportError:
        print("ERROR: 需要 pyserial（仓库根 .venv_tf）", file=sys.stderr)
        return 2
    ports = list(list_ports.comports())
    if not ports:
        print("(无串口设备)")
        return 0
    for p in ports:
        print("%-28s %-40s %s" % (p.device, p.description, p.hwid))
    return 0


def cmd_ping(args) -> int:
    import serial
    try:
        ser = serial.Serial(args.port, args.baud, timeout=args.timeout)
    except Exception as exc:  # noqa: BLE001
        print("ERROR: 打开串口失败: %s" % exc, file=sys.stderr)
        return 3
    try:
        ser.reset_input_buffer()
        ser.write(enc_frame(CMD_PING, b""))
        t0 = time.time()
        rx = bytearray()
        while time.time() - t0 < args.timeout:
            n = ser.in_waiting
            if n:
                rx.extend(ser.read(n))
                # 尝试解一帧
                while len(rx) >= HDR + CRC_LEN:
                    if rx[0] != SYNC0 or rx[1] != SYNC1:
                        del rx[0]
                        continue
                    ln = struct.unpack_from("<H", rx, 3)[0]
                    if len(rx) < HDR + ln + CRC_LEN:
                        break
                    body = bytes(rx[2:HDR + ln])
                    crx = struct.unpack_from("<H", rx, HDR + ln)[0]
                    del rx[:HDR + ln + CRC_LEN]
                    if crc16_ccitt(body) != crx:
                        continue
                    ftype, payload = body[0], body[3:]
                    if ftype == ACK and payload and payload[0] == CMD_PING:
                        print("PING: OK (ACK echo=0x%02X)" % payload[0])
                        return 0
                    if ftype == NACK:
                        print("PING: FAIL (NACK err=0x%02X)" % (payload[1] if len(payload) > 1 else 0))
                        return 5
            else:
                time.sleep(0.005)
        print("PING: FAIL (超时 %.1fs)" % args.timeout, file=sys.stderr)
        return 4
    finally:
        ser.close()


def _dry_run(scripts) -> int:
    bad = 0
    for sc in scripts:
        s = sc.summary()
        print("--- %s ---" % s["path"])
        print("    sid=%s raw_mode=%s frames=%d expected_rsp=%d bytes=%d sha256=%s"
              % (s["sid"], s["raw_mode"], s["n_frames"], s["n_expected_rsp"],
                 s["bytes"], s["sha256"][:16] + "…"))
        for k, v in s["type_counts"].items():
            print("    TYPE %-22s x%d" % (k, v))
        for k, v in s["expect_counts"].items():
            print("    EXPECT %s x%d" % (k, v))
        if s["framing_bad"] or s["crc_bad"]:
            print("    FAIL framing_bad=%d crc_bad=%d" % (s["framing_bad"], s["crc_bad"]))
            bad += 1
    if bad:
        print("DRY-RUN: FAIL（%d 个脚本线格式异常）" % bad)
        return 5
    print("DRY-RUN: PASS（帧边界/CRC/TYPE 计数如上）")
    return 0


def _verify_expectation(exp: dict, ftype: int, payload: bytes) -> str | None:
    """返回 None = 符合；否则返回不符描述。"""
    k = exp["kind"]
    if k == EK_ACK:
        if ftype != ACK:
            return "期望 ACK(0x%02X) 得 TYPE=0x%02X" % (exp["arg"], ftype)
        if not payload or payload[0] != exp["arg"]:
            return "ACK echo=%s 期望 0x%02X" % (
                ("0x%02X" % payload[0]) if payload else "∅", exp["arg"])
        return None
    if k == EK_NACK:
        return None if ftype == NACK else "期望 NACK 得 TYPE=0x%02X" % ftype
    if k == EK_TYPE:
        return None if ftype == exp["arg"] else "期望 TYPE=0x%02X 得 0x%02X" % (exp["arg"], ftype)
    if k in (EK_QUERY, EK_DUMP):
        want = 0x24 if k == EK_QUERY else 0x26
        if ftype != want:
            return "期望 TYPE=0x%02X 得 0x%02X" % (want, ftype)
        if len(payload) < 8:
            return "响应载荷过短（%d B）" % len(payload)
        got = struct.unpack_from("<d", payload, 0)[0]
        if struct.pack("<d", got) != struct.pack("<d", exp["t0"]):
            return "t0 位型不符 got=%r want=%r" % (got, exp["t0"])
        return None
    return None


def cmd_replay(args) -> int:
    scripts = [Script(p) for p in args.scripts]

    if args.dry_run:
        return _dry_run(scripts)

    if not args.port:
        print("ERROR: replay 需要 --port（或用 --dry-run 不连板自检）", file=sys.stderr)
        return 2

    total_frames = sum(len(s.frames) for s in scripts)
    total_expected = sum(s.n_expected_rsp for s in scripts)
    print("[replay] 脚本 %d 个 / 帧 %d / 期望响应 %d / capture=%s"
          % (len(scripts), total_frames, total_expected, args.capture))

    link = Link(args.port, args.baud, args.capture, write_retries=args.write_retries)
    t_start = time.time()
    if args.settle > 0:
        # 上电文本日志（printf）在 capture 之前排空并丢弃，避免其进入证据流
        discarded = 0
        t_settle = time.time()
        while time.time() - t_settle < args.settle:
            n = link.ser.in_waiting
            if n:
                discarded += len(link.ser.read(n))
            else:
                time.sleep(0.05)
        print("[replay] settle %.1fs：丢弃上电字节 %d" % (args.settle, discarded))
    expect_queue: list[tuple[int, dict]] = []   # (全局帧序, 期望) 待校验
    mismatches: list[dict] = []
    sent_frames = 0
    dumps_seen = 0
    last_progress = time.time()

    def progress_report(final: bool = False) -> None:
        nonlocal last_progress
        last_progress = time.time()
        nonlocal dumps_seen
        n = link.rsp_counts.get(0x26, 0)
        if not final and n == dumps_seen:
            return
        dumps_seen = n
        if final or (args.progress > 0 and n % args.progress == 0):
            print("[replay] anchors=%d/%d frames_sent=%d rsp=%d (ack=%d nack=%d crc_bad=%d) "
                  "in=%d out=%d"
                  % (n, args.expected_anchors if args.expected_anchors else 0, sent_frames,
                     len(link.frames), link.ack, link.nack, link.crc_bad,
                     link.bytes_written, link.bytes_read))

    try:
        for sc in scripts:
            if sc.framing_bad or sc.crc_bad:
                print("ERROR: %s 脚本线格式异常（framing_bad=%d crc_bad=%d）"
                      % (sc.path, sc.framing_bad, sc.crc_bad), file=sys.stderr)
                return 2
            units = sc.units if not sc.raw_mode else [(f, []) for f in sc.frames]
            batch = bytearray()
            batch_rsp = 0
            for frame, exps in units:
                batch += frame
                sent_frames += 1
                for e in exps:
                    expect_queue.append((sent_frames, e))
                    batch_rsp += 1
                if len(batch) >= args.batch:
                    link.write(bytes(batch))
                    batch.clear()
                    if sc.raw_mode:
                        # 裸帧模式（无 EXPECT 信息）：以线路静默判批完成
                        link.drain_idle(idle_s=0.05, max_s=args.gate_timeout)
                        progress_report()
                        continue
                    want = len(link.frames) + batch_rsp
                    if not link.wait_frames(want, args.stall, args.gate_timeout,
                                            progress=progress_report):
                        raise TimeoutError(
                            "等待响应超时/零进展：已发 %d B / %d 帧，已收 %d 帧"
                            % (link.bytes_written, sent_frames, len(link.frames)))
                    batch_rsp = 0
                    # 逐条校验期望
                    while expect_queue:
                        idx, e = expect_queue.pop(0)
                        ftype, payload = link.frames[idx - 1] if idx - 1 < len(link.frames) else (None, b"")
                        desc = _verify_expectation(e, ftype, payload)
                        if desc:
                            mismatches.append({"frame": idx, "expect_kind": e["kind"],
                                               "detail": desc})
                            if args.on_mismatch == "stop":
                                raise ValueError("响应与脚本期望不符：%s（帧 #%d）" % (desc, idx))
            if batch:
                link.write(bytes(batch))
                if sc.raw_mode:
                    link.drain_idle(idle_s=0.05, max_s=args.gate_timeout)
                    continue
                wanted = len(link.frames) + batch_rsp
                if not link.wait_frames(wanted, args.stall, args.gate_timeout,
                                        progress=progress_report):
                    raise TimeoutError("等待响应超时/零进展（批尾）：已发 %d B / %d 帧，已收 %d 帧"
                                       % (link.bytes_written, sent_frames, len(link.frames)))
                while expect_queue:
                    idx, e = expect_queue.pop(0)
                    ftype, payload = link.frames[idx - 1] if idx - 1 < len(link.frames) else (None, b"")
                    desc = _verify_expectation(e, ftype, payload)
                    if desc:
                        mismatches.append({"frame": idx, "expect_kind": e["kind"], "detail": desc})
                        if args.on_mismatch == "stop":
                            raise ValueError("响应与脚本期望不符：%s（帧 #%d）" % (desc, idx))
            print("[replay] 脚本 %s 完成：累计帧 %d / 响应 %d" % (sc.sid or sc.path, sent_frames, len(link.frames)))
    except (TimeoutError, ValueError, IOError, OSError) as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        print("ERROR: 已发送 %d B / %d 帧；已收 %d B / %d 帧；capture=%s（已增量落盘）"
              % (link.bytes_written, sent_frames, link.bytes_read, len(link.frames), args.capture),
              file=sys.stderr)
        _write_summary(args, scripts, link, sent_frames, mismatches, total_expected,
                       time.time() - t_start, ok=False)
        link.close()
        return 4 if isinstance(exc, TimeoutError) else 5

    link.drain_idle()
    elapsed = time.time() - t_start
    progress_report(final=True)
    ok = (link.nack == 0 and link.crc_bad == 0 and not mismatches
          and len(link.frames) >= total_expected)
    _write_summary(args, scripts, link, sent_frames, mismatches, total_expected, elapsed, ok)
    print("[replay] 完成：帧 %d / 响应 %d（ACK %d / NACK %d / QUERY %d / DUMP %d）"
          " CRC 错 %d / 期望不符 %d / 耗时 %.1fs"
          % (sent_frames, len(link.frames), link.ack, link.nack,
             link.rsp_counts.get(0x24, 0), link.rsp_counts.get(0x26, 0),
             link.crc_bad, len(mismatches), elapsed))
    link.close()
    if not ok:
        print("VERDICT=FAIL（详见 %s）" % (args.summary or "<capture>.summary.json"),
              file=sys.stderr)
        return 5
    print("VERDICT=PASS")
    return 0


def _write_summary(args, scripts, link, sent_frames, mismatches, total_expected,
                   elapsed, ok) -> None:
    path = args.summary or (args.capture + ".summary.json" if args.capture else None)
    if not path:
        return
    data = {
        "schema": "ems04_s2_c_fw_replay_client_v1",
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "port": args.port, "baud": args.baud, "batch": args.batch,
        "capture": args.capture,
        "capture_sha256": sha256_file(args.capture) if (args.capture and os.path.exists(args.capture)) else None,
        "scripts": [s.summary() for s in scripts],
        "counts": {
            "frames_sent": sent_frames,
            "expected_responses": total_expected,
            "responses": len(link.frames),
            "ack": link.ack, "nack": link.nack,
            "rsp_fp_query": link.rsp_counts.get(0x24, 0),
            "rsp_fp_dump": link.rsp_counts.get(0x26, 0),
            "crc_bad": link.crc_bad,
            "junk_bytes": link.framing_bad,
            "mismatches": len(mismatches),
        },
        "bytes_written": link.bytes_written,
        "bytes_read": link.bytes_read,
        "elapsed_s": round(elapsed, 3),
        "mismatch_detail": mismatches[:64],
        "status": "PASS" if ok else "FAIL",
    }
    with open(path, "w") as fh:
        json.dump(data, fh, indent=2, ensure_ascii=False)
    print("[replay] summary → %s（status=%s）" % (path, data["status"]))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="EMS04-S2 C-fw 串口回放客户端")
    sub = ap.add_subparsers(dest="cmd")

    ap.add_argument("--list-ports", action="store_true", help="列出串口设备后退出")

    p_ping = sub.add_parser("ping", help="CMD_PING 链检")
    p_ping.add_argument("--port", required=True)
    p_ping.add_argument("--baud", type=int, default=115200)
    p_ping.add_argument("--timeout", type=float, default=3.0)

    p_rep = sub.add_parser("replay", help="回放脚本束并落 capture")
    p_rep.add_argument("--scripts", nargs="+", required=True,
                       help="B2V1 脚本束（b/vectors_b2/script_B2_*.bin），按回放顺序给出")
    p_rep.add_argument("--capture", help="响应字节落盘路径（板级会话必填）")
    p_rep.add_argument("--port")
    p_rep.add_argument("--baud", type=int, default=115200)
    p_rep.add_argument("--batch", type=int, default=512, help="单次 write 上限字节")
    p_rep.add_argument("--progress", type=int, default=50, help="每 N 个 DUMP 打一行进度（0=只在末尾）")
    p_rep.add_argument("--summary", help="summary JSON 路径（默认 <capture>.summary.json）")
    p_rep.add_argument("--dry-run", action="store_true", help="不连板：只解析脚本并打印摘要")
    p_rep.add_argument("--stall", type=float, default=5.0, help="秒：无新字节即判零进展")
    p_rep.add_argument("--settle", type=float, default=0.5,
                       help="秒：开抓前静置并丢弃上电文本字节")
    p_rep.add_argument("--gate-timeout", type=float, default=120.0, help="秒：单批等待响应上限")
    p_rep.add_argument("--write-retries", type=int, default=3)
    p_rep.add_argument("--on-mismatch", choices=("stop", "continue"), default="stop")
    p_rep.add_argument("--expected-anchors", type=int, default=0,
                       help="仅用于进度显示（默认 0=不显示分母）")

    args = ap.parse_args(argv)
    if args.list_ports:
        return cmd_list_ports()
    if args.cmd == "ping":
        return cmd_ping(args)
    if args.cmd == "replay":
        if not args.dry_run and not args.capture:
            print("ERROR: replay 需 --capture（或用 --dry-run）", file=sys.stderr)
            return 2
        return cmd_replay(args)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())

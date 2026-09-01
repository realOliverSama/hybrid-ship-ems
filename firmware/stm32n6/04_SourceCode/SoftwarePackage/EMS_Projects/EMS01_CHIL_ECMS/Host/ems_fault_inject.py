#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ems_fault_inject.py — T-508：EMS01 CHIL ECMS FR-508 故障注入 CLI（Host 侧）。

spec ems01-chil-ecms-v1 T-508 / design §Error Handling。四场景各自独立产物
（先 CSV 后 JSON），每场景结束 CMD_DUMP(0x07) 导出事件环文本；本脚本只记录
观察数据，不做门禁判定——结果解释归主 Agent：
  F1 fault_nan_ticks        NaN 输入拍（P_load_kW=NaN）→ 观察 FALLBACK / ê=0
  F2 fault_stall_ticks      停发 --stall-s（>2s）→ 观察 STALE|HELD + P 保持前拍
  F3 fault_garbage_frames   无同步头垃圾 + 坏 CRC 帧 → 观察 NACK/静默 + CRC_ERR 事件
  F4 fault_reset_recovery   NaN 前置态 → CMD_RESET → 观察 INIT 平 fill + 事件环清空

协议实现单一来源（冻结，禁止复制实现）：帧构造/CRC/载荷编解码 import 自
Host/tests/test_proto_loopback.py；SerialSession/_read_some/wait_reply/
parse_stream/load_trace/GRID_STEP_KW/TRACE_COLS import 自 Host/ems_inject.py
（import ems_inject 自身会把 tests 目录插进 sys.path，此处再显式插一次以稳妥）。
flags 位（RSP_TICK payload 末字节，ems_proto.h 镜像）：
  STALE=0x01 / FALLBACK=0x02 / CLS_BAD=0x04 / HELD=0x08。
帧契约（design §UART）：CMD_DUMP(0x07) 响应 = 事件环文本行 + ACK(echo 0x07)；
NACK(0x7E) = echo TYPE + err u8（ERR_CRC=0x01/ERR_TYPE=0x02/ERR_LEN=0x03/
ERR_INFER=0x04）。
运行环境：仓库 model/simulink/.venv_tf（内含 pyserial）；仅 pyserial+stdlib。
路径纪律：全部由 __file__ 相对推导；禁止硬编码绝对路径（注释示例同禁）。
"""
from __future__ import annotations

import argparse
import csv
import itertools
import json
import math
import os
import struct
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))            # .../EMS01/Host
TESTS_DIR = os.path.join(HERE, "tests")
sys.path.insert(0, TESTS_DIR)
sys.path.insert(0, HERE)

from ems_inject import (  # noqa: E402
    SerialSession, parse_stream, load_trace, GRID_STEP_KW, TRACE_COLS)
from test_proto_loopback import (  # noqa: E402
    CMD_RESET, CMD_TICK, RSP_TICK, CMD_PING, CMD_DUMP, ACK, NACK, ERR_CRC,
    build_frame, enc_cmd_tick, dec_rsp_tick, enc_rsp_tick, crc16_ref)

# ems_proto.h 镜像（冻结源仅含 ERR_CRC；余者按 ems_inject.MOCK_ERR_LEN 前例本地镜像）
ERR_TYPE, ERR_LEN, ERR_INFER = 0x02, 0x03, 0x04
ERR_NAMES = {ERR_CRC: "ERR_CRC", ERR_TYPE: "ERR_TYPE",
             ERR_LEN: "ERR_LEN", ERR_INFER: "ERR_INFER"}
FL_STALE, FL_FALLBACK, FL_CLS_BAD, FL_HELD = 0x01, 0x02, 0x04, 0x08
MOCK_P_SCALE = 2.0        # mock 固件：P_gen_raw = P_load * 2.0（确定性）
MOCK_STALE_GAP_S = 0.05   # mock STALE 判定阈（真实固件 2s；--selftest 加速）

CSV_COLS = ["phase", "tick", "t", "flags", "stale", "fallback", "cls_bad",
            "held", "P_gen_kW", "e_hat", "K", "cls_eff", "err"]


def _text_lines(seg):
    """非帧字节段 → 文本行（按 \n 拆分、去 \r 与首尾空白、丢空行）。"""
    out = []
    for ln in seg.decode("ascii", "replace").split("\n"):
        ln = ln.strip("\r").strip()
        if ln:
            out.append(ln)
    return out


def tick_record(dec, phase, extra=None):
    """统一行结构（spec T-508）：dec=RSP_TICK 解码（None=无响应/超时行）。"""
    extra = extra or {}
    if dec is None:
        rec = {c: "" for c in CSV_COLS}
        rec.update({"phase": phase, "tick": extra.get("tick", ""),
                    "t": extra.get("t", ""),
                    "err": extra.get("err", "no_reply")})
        return rec
    f = dec["flags"]
    return {"phase": phase, "tick": dec["idx"], "t": extra.get("t", ""),
            "flags": f,
            "stale": int(bool(f & FL_STALE)),
            "fallback": int(bool(f & FL_FALLBACK)),
            "cls_bad": int(bool(f & FL_CLS_BAD)),
            "held": int(bool(f & FL_HELD)),
            "P_gen_kW": dec["P_gen_ref"], "e_hat": dec["e_hat"],
            "K": dec["K"], "cls_eff": dec["cls"],
            "err": extra.get("err", "")}


def do_tick(sess, row, phase, rsp_ms, recs, p_load=None):
    """发 1 拍 CMD_TICK（7 信号按 TRACE_COLS 顺序）→ 逐拍等 RSP_TICK → 记录。"""
    sig = tuple(row[c] for c in TRACE_COLS)
    if p_load is not None:
        sig = (sig[0], p_load) + sig[2:]
    sess.send(CMD_TICK, enc_cmd_tick(row["tick"], sig))
    pay = sess.wait_reply(RSP_TICK, rsp_ms)
    rec = tick_record(dec_rsp_tick(pay) if pay is not None else None, phase,
                      {"tick": row["tick"], "t": row["t"]})
    recs.append(rec)
    return rec


def dump_events(sess, timeout_ms=2000):
    """CMD_DUMP(0x07)：循环 _read_some 累积 buffer + parse_stream 提帧；帧外
    字节段（parser 跳过的非帧字节）按段内最后一个 \n 截断收集为文本行（未完
    行留待续收）；收到 ACK(payload[0]==CMD_DUMP) 终止。
    返回 (text_lines, raw_bytes)，raw = DUMP 窗口内全部收字节。"""
    buf = bytearray(sess.rx)
    del sess.rx[:]
    sess.send(CMD_DUMP)
    raw = bytearray()
    lines = []
    deadline = time.monotonic() + timeout_ms / 1000.0
    done = False
    while not done and time.monotonic() < deadline:
        chunk = sess._read_some()
        if chunk:
            buf.extend(chunk)
            raw.extend(chunk)
        j = buf.find(b"\xAA\x55")            # 首个帧同步头前的非帧字节段
        nl = buf.rfind(b"\n", 0, len(buf) if j < 0 else j)
        if nl >= 0:                          # 只吐完整行，未完行留待续收
            lines.extend(_text_lines(bytes(buf[:nl + 1])))
            del buf[:nl + 1]
        frames, used = parse_stream(bytes(buf))
        if not frames:
            continue
        pos = 0
        for f in frames:
            k = buf.find(b"\xAA\x55", pos, used)
            if k < 0:
                break
            lines.extend(_text_lines(bytes(buf[pos:k])))   # 帧间非帧字节段
            ln = buf[k + 3] | (buf[k + 4] << 8)
            pos = k + 5 + ln + 2           # 坏 CRC 占位帧同按头部 LEN 消费整帧
            if f["type"] == ACK and f["payload"][:1] == bytes([CMD_DUMP]):
                done = True
            elif f["type"] == NACK and f["payload"]:
                lines.append("NACK echo=0x%02X err=0x%02X" % (
                    f["payload"][0],
                    f["payload"][1] if len(f["payload"]) > 1 else 0))
        del buf[:max(pos, used)]
    return lines, bytes(raw)

# ------------------------------------------------------- 会话 / 场景公共
def open_session(port, rsp_ms=500):
    """SerialSession(dev=port) + PING 预同步（≤5 次，复用 ems_inject 模式语义）。"""
    sess = SerialSession(dev=port, timeout_ms=rsp_ms)
    for _ in range(5):
        sess.send(CMD_PING, b"")
        if sess.wait_reply(ACK, 400) is not None:
            return sess
        time.sleep(0.2)
    sess.close()
    raise SystemExit("PING 预同步失败（5 次无 ACK，链路未就绪）")


def warmup(sess, rows, n, log, rsp_ms=500):
    """CMD_RESET → wait_reply(ACK)（payload[0]==CMD_RESET 否则 exit 2）→ 连续
    n 拍正常 CMD_TICK（逐拍 RSP_TICK），逐拍记录经 log 回调；返回记录列表。"""
    sess.send(CMD_RESET)
    b = sess.wait_reply(ACK, rsp_ms)
    if b is None or not b or b[0] != CMD_RESET:
        print("RESET 未确认（payload=%r）" % (b,))
        sys.exit(2)
    out = []
    for i in range(n):
        do_tick(sess, rows[i % len(rows)], "warmup", rsp_ms, out)
        if log:
            log(out[-1])
    return out


def _rows_from(start, rows):
    """行游标：从 start 起循环取 trace 行（warmup 已消费 0..start-1）。"""
    c = itertools.count(start)
    return lambda: rows[next(c) % len(rows)]


def _pick(rec):
    """观察要点子集（非有限浮点转字符串，保证 JSON 严格可序列化）。"""
    out = {}
    for k in ("phase", "tick", "flags", "P_gen_kW", "e_hat", "K",
              "cls_eff", "err"):
        v = rec.get(k)
        if isinstance(v, float) and not math.isfinite(v):
            v = repr(v)
        out[k] = v
    return out


def _write_outputs(out_prefix, recs, js):
    """先写 CSV 再写 JSON（场景结束立即落盘）。"""
    os.makedirs(os.path.dirname(out_prefix) or ".", exist_ok=True)
    with open(out_prefix + ".csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_COLS)
        w.writeheader()
        for r in recs:
            w.writerow({c: r.get(c, "") for c in CSV_COLS})
    with open(out_prefix + ".json", "w") as f:
        json.dump(js, f, ensure_ascii=False, indent=1)


def _dump_json(name, a, recs, lines, raw, observations, n_nack=0):
    """场景 JSON：scenario/参数快照/计数/dump 文本原文/观察要点（仅事实，无判定）。"""
    return {"scenario": name,
            "params": {"port": a.port,
                       "trace": os.path.basename(a.trace) if a.trace
                       else "(selftest rows)",
                       "rsp_timeout_ms": a.rsp_timeout_ms,
                       "stall_s": a.stall_s, "warmup": a.warmup,
                       "post": a.post},
            "counts": {"n_rows": len(recs), "n_nack": n_nack,
                       "n_dump_lines": len(lines)},
            "dump_text": lines, "dump_raw_hex": raw.hex(),
            "observations": observations}


# ----------------------------------------------------------------- 四场景
def scenario_nan(sess, rows, a, out_prefix):
    """F1：warmup → 基线 1 拍 → 1 拍 P_load=NaN → --post 拍正常 → DUMP。"""
    recs = []
    warmup(sess, rows, a.warmup, recs.append, a.rsp_timeout_ms)
    nxt = _rows_from(a.warmup, rows)
    base = do_tick(sess, nxt(), "baseline", a.rsp_timeout_ms, recs)
    nan_r = do_tick(sess, nxt(), "nan_tick", a.rsp_timeout_ms, recs,
                    p_load=float("nan"))
    post = [do_tick(sess, nxt(), "post", a.rsp_timeout_ms, recs)
            for _ in range(a.post)]
    lines, raw = dump_events(sess)
    js = _dump_json("F1_fault_nan_ticks", a, recs, lines, raw,
                    {"baseline": _pick(base), "nan_tick": _pick(nan_r),
                     "post_flags": [r["flags"] for r in post]})
    _write_outputs(out_prefix, recs, js)
    return js


def scenario_stall(sess, rows, a, out_prefix):
    """F2：warmup → tick A → sleep(--stall-s) → tick B（预期观察 STALE|HELD 且
    P==tick A 的 P）→ --post 拍（预期 STALE 清）→ DUMP。"""
    recs = []
    warmup(sess, rows, a.warmup, recs.append, a.rsp_timeout_ms)
    nxt = _rows_from(a.warmup, rows)
    pre = do_tick(sess, nxt(), "pre_stall", a.rsp_timeout_ms, recs)
    time.sleep(a.stall_s)
    stall = do_tick(sess, nxt(), "stall_tick", a.rsp_timeout_ms, recs)
    post = [do_tick(sess, nxt(), "post", a.rsp_timeout_ms, recs)
            for _ in range(a.post)]
    lines, raw = dump_events(sess)
    js = _dump_json("F2_fault_stall_ticks", a, recs, lines, raw,
                    {"pre_stall": _pick(pre), "stall_tick": _pick(stall),
                     "stall_P_equals_pre":
                         stall["P_gen_kW"] == pre["P_gen_kW"],
                     "post_flags": [r["flags"] for r in post]})
    _write_outputs(out_prefix, recs, js)
    return js

def scenario_garbage(sess, rows, a, out_prefix):
    """F3：warmup → 32B 无同步头垃圾（预期静默，NACK 等 300ms 落空）→ 坏 CRC
    PING 帧 → NACK(ERR_CRC)（预期捕获，记录 err 码）→ 1 拍正常 tick（会话存活）
    → DUMP（预期含 CRC_ERR 事件）。"""
    recs = []
    warmup(sess, rows, a.warmup, recs.append, a.rsp_timeout_ms)
    nxt = _rows_from(a.warmup, rows)
    sess.ser.write(b"\x00\xDE\xAD\xBE\xEF" * 8)   # 无同步头：固件/解析器静默丢
    g = sess.wait_reply(NACK, 300)
    bad = bytearray(build_frame(CMD_PING, b""))
    bad[-1] ^= 0xFF                                  # 翻转末字节 → 坏 CRC
    sess.ser.write(bytes(bad))
    n = sess.wait_reply(NACK, 500)
    alive = do_tick(sess, nxt(), "post_garbage", a.rsp_timeout_ms, recs)
    lines, raw = dump_events(sess)
    nack = None
    if n is not None:
        err = n[1] if len(n) > 1 else None
        nack = {"echo_type": "0x%02X" % n[0], "err": err,
                "err_name": ERR_NAMES.get(err, "?")}
    js = _dump_json("F3_fault_garbage_frames", a, recs, lines, raw,
                    {"garbage_nack": None if g is None else g.hex(),
                     "garbage_nack_silent": g is None,
                     "bad_crc_nack": nack,
                     "alive_after_tick": _pick(alive),
                     "dump_lines_head": lines[:3]},
                    n_nack=(0 if g is None else 1) + (0 if n is None else 1))
    _write_outputs(out_prefix, recs, js)
    return js


def scenario_reset(sess, rows, a, out_prefix):
    """F4：warmup → 1 拍 NaN（FALLBACK 前置态）→ CMD_RESET → ACK → --post 拍
    正常（首拍预期 INIT 平 fill，逐拍记 flags/e_hat）→ DUMP（预期事件环已被
    CMD_RESET 清空或仅含复位后事件）。"""
    recs = []
    warmup(sess, rows, a.warmup, recs.append, a.rsp_timeout_ms)
    nxt = _rows_from(a.warmup, rows)
    nan_r = do_tick(sess, nxt(), "pre_reset_nan", a.rsp_timeout_ms, recs,
                    p_load=float("nan"))
    sess.send(CMD_RESET)
    ack = sess.wait_reply(ACK, a.rsp_timeout_ms)
    if ack is None or not ack or ack[0] != CMD_RESET:
        print("F4 CMD_RESET 未确认（payload=%r）" % (ack,))
        sys.exit(2)
    post = [do_tick(sess, nxt(), "post_reset", a.rsp_timeout_ms, recs)
            for _ in range(a.post)]
    lines, raw = dump_events(sess)
    js = _dump_json("F4_fault_reset_recovery", a, recs, lines, raw,
                    {"pre_reset_nan": _pick(nan_r),
                     "reset_ack_payload_hex": ack.hex(),
                     "post_reset_flags": [r["flags"] for r in post],
                     "post_reset_e_hat": [r["e_hat"] for r in post]})
    _write_outputs(out_prefix, recs, js)
    return js


SCENARIOS = {"nan": ("F1_fault_nan_ticks", scenario_nan),
             "stall": ("F2_fault_stall_ticks", scenario_stall),
             "garbage": ("F3_fault_garbage_frames", scenario_garbage),
             "reset": ("F4_fault_reset_recovery", scenario_reset)}


def run_scenarios(a, rows):
    """逐场景执行：每场景结束立即落盘 CSV/JSON 并 print 产物路径。"""
    os.makedirs(a.out, exist_ok=True)
    keys = [a.only] if a.only else list(SCENARIOS)
    results = {}
    sess = open_session(a.port, a.rsp_timeout_ms)
    try:
        for key in keys:
            name, fn = SCENARIOS[key]
            prefix = os.path.join(a.out, name)
            results[key] = fn(sess, rows, a, prefix)
            print("%s -> %s.csv|.json" % (name, prefix))
    finally:
        sess.close()
    return results


# ------------------------------------------------------- --selftest mock 固件
class _MockFW:
    """pty 侧最小 mock 固件（--selftest 专用，与冻结源帧语义一致）：
    RESET/PING → ACK(echo)；TICK → RSP_TICK（idx 回显、P 网格化；NaN →
    FALLBACK+ê=0+保持前拍 P+INFER_FAIL 事件；tick 间隔 > MOCK_STALE_GAP_S →
    STALE|HELD + P 保持前拍 + STALE 事件；正常拍尾随 FALLBACK 一拍）；坏 CRC
    帧 → NACK(echo,ERR_CRC) + CRC_ERR 事件；DUMP → 首事件/空环 1 行 + 汇总行
    + ACK(echo 0x07)。仅服务离线链路验证，不代表固件完整语义。"""

    def __init__(self, fd):
        self.fd = fd
        self.buf = bytearray()
        self.last_p = 0.0
        self.last_t = None
        self.fb_pending = False
        self.ring = []          # (seq, type, aux, tick)
        self.ev_seq = 0

    def _emit(self, b):
        os.write(self.fd, b)

    def _evt(self, etype, aux, tick):
        self.ring.append((self.ev_seq, etype, aux, tick))
        self.ev_seq += 1

    def _tick(self, pay, now):
        idx = struct.unpack_from("<I", pay, 0)[0]
        p_load = struct.unpack_from("<f", pay, 8)[0]
        flags = 0
        if math.isnan(p_load) or math.isinf(p_load):
            p_gen, e_hat, k, cls = self.last_p, 0.0, 1.0, 0
            flags |= FL_FALLBACK
            self.fb_pending = True
            self._evt(4, 0, idx)                    # INFER_FAIL
        elif self.last_t is not None and (now - self.last_t) > MOCK_STALE_GAP_S:
            p_gen, k, cls = self.last_p, 0.75, 3
            e_hat = round(self.last_p * 0.05, 3)
            flags |= FL_STALE | FL_HELD
            self._evt(3, 0, idx)                    # STALE
        else:
            p_gen = math.floor(p_load * MOCK_P_SCALE / GRID_STEP_KW) * GRID_STEP_KW
            e_hat, k, cls = round(p_gen * 0.05, 3), 0.75, 3
            if self.fb_pending:
                flags |= FL_FALLBACK                # 上一控制拍 ok=0 尾随一拍
            self.fb_pending = False
            self.last_p = p_gen
        self.last_t = now
        self._emit(build_frame(RSP_TICK, enc_rsp_tick(
            idx, p_gen, e_hat, k, cls, 1000 + idx % 997, 2000 + idx % 883,
            flags)))

    def feed(self, chunk):
        self.buf.extend(chunk)
        while True:
            i = self.buf.find(b"\xAA\x55")
            if i < 0:                               # 无同步头：静默丢
                if self.buf[-1:] == b"\xAA":        # 末字节可能是拆裂的帧头
                    del self.buf[:-1]
                else:
                    self.buf.clear()
                break
            if i:
                del self.buf[:i]
            if len(self.buf) < 5:
                break
            ttype = self.buf[2]
            ln = self.buf[3] | (self.buf[4] << 8)
            total = 5 + ln + 2
            if len(self.buf) < total:
                break
            pay = bytes(self.buf[5:5 + ln])
            frame = bytes(self.buf[:total])
            del self.buf[:total]
            crc_rx = frame[5 + ln] | (frame[5 + ln + 1] << 8)
            hdr = bytes([ttype, ln & 0xFF, (ln >> 8) & 0xFF])
            if crc16_ref(hdr + pay) != crc_rx:      # 坏 CRC → NACK + 事件
                self._evt(1, ttype, 0)              # CRC_ERR（aux=线上 TYPE）
                self._emit(build_frame(NACK, bytes([ttype, ERR_CRC])))
                continue
            now = time.monotonic()
            if ttype == CMD_RESET:
                self.last_p, self.last_t = 0.0, None
                self.fb_pending = False
                self.ring = []
                self._emit(build_frame(ACK, bytes([CMD_RESET])))
            elif ttype == CMD_PING:
                self._emit(build_frame(ACK, bytes([CMD_PING])))
            elif ttype == CMD_DUMP:
                if self.ring:
                    s, ty, aux, tk = self.ring[0]
                    self._emit(("EVT seq=%d type=%d aux=%d tick=%d ms=0"
                                % (s, ty, aux, tk)).encode() + b"\r\n")
                else:
                    self._emit(b"EVT ring=empty\r\n")
                self._emit(("EVT total=%d kept=%d cap=32"
                            % (len(self.ring),
                               min(len(self.ring), 32))).encode() + b"\r\n")
                self._emit(build_frame(ACK, bytes([CMD_DUMP])))
            elif ttype == CMD_TICK and ln == 32:
                self._tick(pay, now)
            else:
                self._emit(build_frame(NACK, bytes([ttype, ERR_TYPE])))


def _mock_worker(fd, stop):
    m = _MockFW(fd)
    while not stop.is_set():
        try:
            chunk = os.read(fd, 512)
        except OSError:
            stop.wait(0.005)
            continue
        if chunk:
            m.feed(chunk)
        else:
            stop.wait(0.005)


def selftest_main():
    """--selftest：pty + mock 固件（--port 指向 pty slave 名；stall-s=0.1 放宽），
    四场景全跑；断言：无异常退出、四组（8 个）文件生成、F3 捕获 mock NACK
    （ERR_CRC）、F1 FALLBACK / F2 STALE|HELD 链路到位。"""
    import pty
    import tempfile
    import tty
    ma_fd, s_fd = pty.openpty()
    tty.setraw(s_fd)   # pty 默认 cooked 模式，二进制帧必须 RAW
    stop = threading.Event()
    threading.Thread(target=_mock_worker, args=(ma_fd, stop), daemon=True).start()
    out_dir = tempfile.mkdtemp(prefix="ems_fault_selftest_")
    a = argparse.Namespace(port=os.ttyname(s_fd), trace=None, out=out_dir,
                           rsp_timeout_ms=500, stall_s=0.1, warmup=3, post=3,
                           only=None, selftest=True)
    rows = [{"tick": i, "t": float(i), "P_load_kW": 100.0 + 20.0 * i,
             "SOC": 0.6, "v_cmd_mps": 3.0, "v_ship_mps": 3.0,
             "accel_mps2": 0.0, "x_ship_m": 100.0 * i} for i in range(8)]
    try:
        results = run_scenarios(a, rows)
    finally:
        stop.set()
        os.close(s_fd)
        os.close(ma_fd)
    ok = True
    for key, (name, _fn) in SCENARIOS.items():
        have = all(os.path.isfile(os.path.join(out_dir, name + ext))
                   for ext in (".csv", ".json"))
        print("selftest %s: files=%s" % (name, have))
        ok &= have
    f1 = results.get("nan", {}).get("observations", {})
    nf = f1.get("nan_tick", {}).get("flags") or 0
    print("selftest F1 nan_tick flags=0x%02X (FALLBACK=%s)" % (nf,
          bool(nf & FL_FALLBACK)))
    ok &= bool(nf & FL_FALLBACK)
    f2 = results.get("stall", {}).get("observations", {})
    sf = f2.get("stall_tick", {}).get("flags") or 0
    print("selftest F2 stall flags=0x%02X (STALE=%s) P==pre:%s"
          % (sf, bool(sf & FL_STALE), f2.get("stall_P_equals_pre")))
    ok &= bool(sf & FL_STALE) and f2.get("stall_P_equals_pre") is True
    n = results.get("garbage", {}).get("observations", {}).get("bad_crc_nack")
    print("selftest F3 bad_crc_nack=%s" % (n,))
    ok &= bool(n) and n.get("err") == ERR_CRC
    print("selftest artifacts: %s" % out_dir)
    print("EMS_FAULT_INJECT SELFTEST %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(
        description="T-508: EMS01 CHIL ECMS FR-508 故障注入 CLI（四场景观察记录，"
                    "无门禁判定；协议复用 ems_inject/test_proto_loopback 冻结源）")
    ap.add_argument("--port", help="串口设备（--selftest 时可省）")
    ap.add_argument("--trace", metavar="CSV",
                    help="暖机 tick 源 CSV（ref_G2_*.csv 同款 7 信号全列，"
                         "ems_inject.load_trace 读）")
    ap.add_argument("--out", default=os.path.join(HERE, "fault_out"),
                    help="产物目录（默认 <Host>/fault_out）")
    ap.add_argument("--rsp-timeout-ms", type=int, default=500,
                    help="RSP/ACK 等待超时 [ms]（默认 500）")
    ap.add_argument("--stall-s", type=float, default=3.0,
                    help="F2 停发时长 [s]（默认 3.0；须 >2.0；--selftest 豁免）")
    ap.add_argument("--warmup", type=int, default=12,
                    help="暖机正常 tick 数（默认 12）")
    ap.add_argument("--post", type=int, default=12,
                    help="每场景后置正常 tick 数（默认 12）")
    ap.add_argument("--only", choices=list(SCENARIOS), metavar="nan|stall|garbage|reset",
                    help="只跑一个场景（默认四个全跑）")
    ap.add_argument("--selftest", action="store_true",
                    help="无硬件离线自测（pty + mock 固件；stall 阈值检查豁免）")
    a = ap.parse_args()
    if a.selftest:
        return selftest_main()
    if not a.port:
        ap.error("--port 必填（--selftest 时可省）")
    if not a.trace:
        ap.error("--trace CSV 必填（暖机 tick 源）")
    if a.stall_s <= 2.0:
        ap.error("--stall-s 必须 > 2.0 s（STALE 注入阈 = 2s 严格大于）")
    rows = load_trace(a.trace)
    if not rows:
        print("trace 无行：%s" % a.trace)
        return 1
    results = run_scenarios(a, rows)
    print("scenarios=%d out=%s" % (len(results), os.path.abspath(a.out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
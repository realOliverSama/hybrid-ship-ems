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

T-609 扩展（spec ems01-chil-v3-closedloop，FR-608/AC-604）：--closed-loop /
--selftest-closedloop 闭环注入模式（详见文件尾【T-609 闭环注入】区段头注释：
注入层=包装会话候选②；四场景闭环化；FALLBACK 段经 flags 界定并统计段内
时长/Δfuel/ΔSOC/恢复正常拍数；v1 行为/CLI/退出码零破坏——--closed-loop
缺省 off）。
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




# =========================================================== T-609 闭环注入
# 【注入层设计选择（候选②，最小侵入）】不改 ems_closedloop.py——经
# ems_closedloop.SerialSession 模块名运行期替换，把包装会话注入 harness 的
# board 模式：包装器在事务层（send(ttype, payload) 类型化入口）按场景策略
# 插入异常（F1 改写 P_load=NaN 后重编码合法帧 / F2 延迟转发触发 STALE /
# F3 向 mock 注垃圾+坏 CRC / F4 事务间隙注入 CMD_RESET），反向 RSP_TICK
# 解码 flags 做 FALLBACK 段界定。候选①（pty 代理线程）需要双 pty+字节级
# 帧重组，移动部件更多而效果相同，故弃用。设计 §6.4：回退=安全降级继续
# 控制（回路不解体）；回退段以 flags 位界定并统计段内时长/Δfuel/ΔSOC。
CL_FL_NAMES = {"STALE": FL_STALE, "FALLBACK": FL_FALLBACK,
               "CLS_BAD": FL_CLS_BAD, "HELD": FL_HELD}


class _CLPolicy(object):
    """闭环注入策略（单场景）。tick 点由 payload 头 u32（tick_idx）判定，
    不依赖发送计数。FALLBACK 段由 on_rsp 的 flags 位界定。"""

    def __init__(self, scenario, fault_tick, stall_s):
        self.scenario = scenario
        self.fault_tick = int(fault_tick)
        self.stall_s = float(stall_s)
        self.forward = True          # False = 本次 send 被策略吞掉
        self.events = []             # 策略侧事件（注入动作流水）
        self.fb_segs = []            # FALLBACK 段（on_rsp 界定）
        self._fb_open = None         # {"start_tick":..,"start_i":..}
        self._rsp_i = 0
        self.reset_injected = False
        self._garbage_now = False
        self._pending_reset_now = False

    # ---- 发送侧（harness → mock）：仅记流水/置标记；改写走 mutate_payload，
    #      垃圾与 RESET 注入走 pre_forward（转发前直写底层会话） ----
    def on_send(self, ttype, payload):
        if ttype != CMD_TICK:
            return
        idx = struct.unpack_from("<I", payload, 0)[0]
        if self.scenario == "stall" and idx == self.fault_tick:
            time.sleep(self.stall_s)   # > MOCK_STALE_GAP_S → mock 判 STALE|HELD
            self.events.append({"action": "stall", "tick": idx,
                                "stall_s": self.stall_s})
        elif self.scenario == "garbage" and idx == self.fault_tick:
            self._garbage_now = True   # 标记：转发前先注垃圾（见 pre_forward）
            self.events.append({"action": "garbage", "tick": idx})
        elif self.scenario == "reset" and idx == self.fault_tick:
            # 前一拍（fault_tick-1）已由 mutate_payload 注 NaN（FALLBACK 前置态）；
            # 本拍转发前注入 CMD_RESET（INIT 平 fill 恢复）
            self._pending_reset_now = True
            self.events.append({"action": "inject_reset_before_tick",
                                "tick": idx})

    def mutate_payload(self, ttype, payload):
        """F1/F4(前置)：P_load=NaN 改写（重编码合法帧——固件 EML 见 NaN 输入）。"""
        if ttype != CMD_TICK or self.scenario not in ("nan", "reset"):
            return payload
        idx = struct.unpack_from("<I", payload, 0)[0]
        hit = (idx == self.fault_tick if self.scenario == "nan"
               else idx == self.fault_tick - 1)
        if hit:
            self.events.append({"action": "mutate_pload_nan", "tick": idx})
            pay = bytearray(payload)
            pay[8:12] = struct.pack("<f", float("nan"))
            return bytes(pay)
        return payload

    def pre_forward(self, ttype, inner):
        """F3/F4：在转发本拍 CMD_TICK 前注入的额外流量（直接写底层会话）。"""
        if ttype != CMD_TICK:
            return
        if getattr(self, "_garbage_now", False):
            self._garbage_now = False
            inner.ser.write(b"\x00\xDE\xAD\xBE\xEF" * 8)     # 无同步头：静默丢
            bad = bytearray(build_frame(CMD_PING, b""))
            bad[-1] ^= 0xFF                                        # 坏 CRC
            inner.ser.write(bytes(bad))
        if getattr(self, "_pending_reset_now", False):
            self._pending_reset_now = False
            inner.send(CMD_RESET, b"")
            ack = inner.wait_reply(ACK, 500)
            self.reset_injected = bool(ack is not None and ack[:1] ==
                                       bytes([CMD_RESET]))
            self.events.append({"action": "reset_acked", "ok": self.reset_injected})

    def on_rsp(self, dec):
        """反向 RSP_TICK：flags 位界定 FALLBACK 段。"""
        self._rsp_i += 1
        idx = dec["idx"]
        fb = bool(dec["flags"] & FL_FALLBACK)
        if fb and self._fb_open is None:
            self._fb_open = {"start_tick": idx, "start_rsp_i": self._rsp_i}
        elif not fb and self._fb_open is not None:
            self._fb_open["end_tick"] = idx
            self.fb_segs.append(self._fb_open)
            self._fb_open = None


class _CLFaultSession(object):
    """包装 harness 的 SerialSession 实例（仅转发 send/wait_reply/close 与
    harness 实际使用的少数属性；不实现 ContextManager——harness 未用）。"""

    def __init__(self, inner, policy):
        self._inner = inner
        self._pol = policy

    def send(self, ttype, payload=b""):
        self._pol.on_send(ttype, payload)
        self._pol.pre_forward(ttype, self._inner)
        pay = self._pol.mutate_payload(ttype, payload)
        return self._inner.send(ttype, pay)

    def wait_reply(self, want_type, timeout_ms=2000, want_seq=None):
        pay = self._inner.wait_reply(want_type, timeout_ms, want_seq)
        if pay is not None and want_type == RSP_TICK and len(pay) == 26:
            self._pol.on_rsp(dec_rsp_tick(pay))
        return pay

    def close(self):
        self._inner.close()

    @property
    def ser(self):
        return self._inner.ser

    def _read_some(self):
        return self._inner._read_some()

    @property
    def rx(self):
        return self._inner.rx


def _cl_fallback_segments(csv_path):
    """从 harness 轨迹 CSV 解析 FALLBACK 段（design §6.4：段内时长/Δfuel/ΔSOC
    + 退出后恢复正常拍数）。段 = flags&0x02 的连续拍。"""
    segs = []
    cur = None
    prev = None
    with open(csv_path, newline="") as f:
        for r in csv.DictReader(f):
            tick = int(float(r["tick"]))
            fl = int(float(r["flags"])) if r["flags"] not in ("", "nan") else 0
            fuel = float(r["fuel_total_kg"])
            soc = float(r["SOC"])
            fb = bool(fl & FL_FALLBACK)
            if fb and cur is None:
                cur = {"start_tick": tick, "start_fuel": prev["fuel_total_kg"]
                       if prev else fuel, "start_soc": prev["SOC"] if prev
                       else soc, "end_tick": tick, "end_fuel": fuel,
                       "end_soc": soc}
            elif fb and cur is not None:
                cur["end_tick"] = tick
                cur["end_fuel"] = fuel
                cur["end_soc"] = soc
            elif not fb and cur is not None:
                cur["recovery_ticks"] = tick - cur["end_tick"] - 1
                segs.append(cur)
                cur = None
            prev = {"tick": tick, "fuel_total_kg": fuel, "SOC": soc}
    if cur is not None:
        cur["recovery_ticks"] = 0
        segs.append(cur)
    for g in segs:
        g["n_ticks"] = g["end_tick"] - g["start_tick"] + 1
        g["d_fuel_kg"] = g["end_fuel"] - g["start_fuel"]
        g["d_soc"] = g["end_soc"] - g["start_soc"]
    return segs


def _cl_run_one(scenario, a):
    """单场景闭环注入：pty mock（独立 pty，状态干净）+ 包装会话注入
    ems_closedloop.board 模式（stub 推进器，无 dylib 依赖）→ 轨迹 CSV 解析
    FALLBACK 段 → 事后 DUMP 事件环 → 产物落 <out>/closedloop/。"""
    import pty as _pty
    import tty as _tty
    import tempfile as _tempfile
    import ems_closedloop as cl

    ma_fd, s_fd = _pty.openpty()
    _tty.setraw(s_fd)
    stop = threading.Event()
    threading.Thread(target=_mock_worker, args=(ma_fd, stop),
                     daemon=True).start()
    orig_sess = None
    try:
        cl_dir = os.path.join(a.out, "closedloop")
        os.makedirs(cl_dir, exist_ok=True)
        h_out = _tempfile.mkdtemp(prefix="ems_cl_fault_harness_")
        policy = _CLPolicy(scenario, a.cl_fault_tick, a.cl_stall_s)
        orig_sess = cl.SerialSession

        def _factory(dev=None, timeout_ms=500, io=None):
            inner = orig_sess(dev=dev, timeout_ms=timeout_ms, io=io)
            return _CLFaultSession(inner, policy)

        cl.SerialSession = _factory
        args = argparse.Namespace(
            scenario="S20", port=os.ttyname(s_fd), image=a.image,
            advancer="stub", out=h_out, runs_dir=None,
            rsp_timeout_ms=a.rsp_timeout_ms, max_ticks=a.cl_ticks)
        rc = cl.main(["--scenario", "S20", "--port", os.ttyname(s_fd),
                      "--image", a.image, "--advancer", "stub",
                      "--out", h_out, "--rsp-timeout-ms",
                      str(a.rsp_timeout_ms), "--max-ticks", str(a.cl_ticks)])
        cl.SerialSession = orig_sess
        orig_sess = None

        # harness 轨迹（board 模式产物在 h_out）
        csvs = [os.path.join(h_out, f) for f in os.listdir(h_out)
                if f.endswith(".csv")]
        assert len(csvs) == 1, "harness 产物 CSV 数 %d != 1" % len(csvs)
        segs = _cl_fallback_segments(csvs[0])

        # 事后 DUMP（新开会话；mock 线程仍在）
        sess2 = open_session(os.ttyname(s_fd), a.rsp_timeout_ms)
        lines, raw = dump_events(sess2, 2000)
        sess2.close()

        js = {"scenario": "CL_" + scenario.upper(),
              "v1_scenario_ref": scenario,
              "harness_csv": csvs[0],
              "params": {"cl_fault_tick": a.cl_fault_tick,
                         "cl_stall_s": a.cl_stall_s,
                         "cl_ticks": a.cl_ticks,
                         "rsp_timeout_ms": a.rsp_timeout_ms,
                         "advancer": "stub", "image": a.image,
                         "mock": "pty+_MockFW（STALE 阈加速 0.05s）"},
              "counts": {"n_fallback_segments": len(segs),
                         "n_policy_events": len(policy.events),
                         "n_dump_lines": len(lines)},
              "policy_events": policy.events,
              "fallback_segments": segs,
              "dump_text": lines,
              "observations": {}}
        prefix = os.path.join(cl_dir, "CL_" + scenario.upper())
        with open(prefix + ".json", "w") as f:
            json.dump(js, f, ensure_ascii=False, indent=1)
        for f_ in os.listdir(h_out):
            import shutil
            shutil.copy2(os.path.join(h_out, f_),
                         os.path.join(cl_dir, scenario + "_" + f_))
        print("CL %s -> %s.json（+harness 轨迹副本）" % (scenario, prefix))
        return js, rc
    finally:
        if orig_sess is not None:
            cl.SerialSession = orig_sess
        stop.set()
        os.close(s_fd)
        os.close(ma_fd)


def _cl_csv_flags(csv_path):
    """harness 轨迹 CSV → {tick: (flags, P_gen)}（int/float）。"""
    out = {}
    if not csv_path or not os.path.isfile(csv_path):
        return out
    with open(csv_path, newline="") as f:
        for r in csv.DictReader(f):
            tick = int(float(r["tick"]))
            fl = int(float(r["flags"])) if r["flags"] not in ("", "nan") else 0
            out[tick] = (fl, float(r["P_gen_kW"]))
    return out


def _cl_assert_one(scenario, js, rc, fault_tick=None):
    """单场景闭环注入断言（机制级；返回 (name, ok, detail) 列表）。"""
    out = []
    out.append(("harness 退出码 0", rc == 0, ""))
    segs = js["fallback_segments"]
    evs = js["policy_events"]
    dl = js["dump_text"]
    ft = fault_tick if fault_tick is not None else js.get(
        "params", {}).get("cl_fault_tick", 30)
    flmap = _cl_csv_flags(js.get("harness_csv", ""))
    fl_ft, pg_ft = flmap.get(ft, (0, float("nan")))
    fl_pre, pg_pre = flmap.get(ft - 1, (0, float("nan")))
    fl_nxt = flmap.get(ft + 1, (0,))[0]
    if scenario == "nan":
        fb_n = sum(1 for g in segs if g["n_ticks"] >= 1)
        has_ev = (any("INFER_FAIL" in ln for ln in dl)
                  or any(e.get("action") == "mutate_pload_nan" for e in evs))
        out.append(("F1 FALLBACK 段 ≥1（注入拍反应）", fb_n >= 1, ""))
        out.append(("F1 注入动作/事件环证据", has_ev, ""))
        out.append(("F1 注入拍 flags 含 FALLBACK",
                    bool(fl_ft & FL_FALLBACK), ""))
        out.append(("F1 尾随拍 FALLBACK（fb_pending 语义）",
                    bool(fl_nxt & FL_FALLBACK), ""))
        out.append(("F1 段统计字段齐",
                    all(("n_ticks" in g and "d_fuel_kg" in g and
                         "d_soc" in g and "recovery_ticks" in g)
                        for g in segs), ""))
    elif scenario == "stall":
        has_stall = any(e.get("action") == "stall" for e in evs)
        out.append(("F2 注入动作流水", has_stall, ""))
        out.append(("F2 注入拍 flags 含 STALE|HELD",
                    bool(fl_ft & FL_STALE) and bool(fl_ft & FL_HELD), ""))
        out.append(("F2 Pheld（P_gen == 前拍）", pg_ft == pg_pre, ""))
        out.append(("F2 STALE 事件环证据",
                    any("type=3" in ln for ln in dl) or has_stall, ""))
        out.append(("F2 段/事件字段齐", isinstance(segs, list), ""))
    elif scenario == "garbage":
        # mock 事件环行格式 "EVT seq=.. type=<码> aux=<线上TYPE>.."；
        # CRC_ERR=0x01（ems_proto.h 镜像），aux=引发帧的线上 TYPE
        out.append(("F3 CRC_ERR 事件环证据",
                    any("type=1 " in ln and "aux=6" in ln for ln in dl), ""))
        out.append(("F3 垃圾注入动作流水",
                    any(e.get("action") == "garbage" for e in evs), ""))
        out.append(("F3 harness 会话存活（退出码 0）", rc == 0, ""))
    elif scenario == "reset":
        rst = any(e.get("action") == "inject_reset_before_tick" for e in evs)
        acked = any(e.get("action") == "reset_acked" and e.get("ok")
                    for e in evs)
        out.append(("F4 RESET 注入且 ACK", rst and acked, ""))
        out.append(("F4 前置拍（NaN）FALLBACK 置位",
                    bool(fl_pre & FL_FALLBACK), ""))
        out.append(("F4 RESET 拍 FALLBACK 清除",
                    not (fl_ft & FL_FALLBACK), ""))
        out.append(("F4 前置 FALLBACK 段存在", len(segs) >= 1, ""))
    return out


def closedloop_main(a):
    """--closed-loop：闭环注入 CLI（真实串口或 pty；四场景或 --only 单场景）。
    注入宿主 = ems_closedloop board 模式（stub/ctypes 由 --cl-ticks/--advancer
    决定，CLI 缺省 ctypes 真实 dylib）；轨迹与 FALLBACK 段统计落 <out>/closedloop/。"""
    import ems_closedloop as cl
    keys = [a.only] if a.only else list(SCENARIOS)
    if a.scenario is None:
        ap_err = "--closed-loop 需要 --scenario（S20 | voyage002）"
        print("ERROR: " + ap_err, file=sys.stderr)
        return 2
    if a.port is None:
        print("ERROR: --closed-loop 需要 --port（注入宿主 = board 模式串口）",
              file=sys.stderr)
        return 2
    overall = True
    for key in keys:
        ma_fd, s_fd = None, None
        try:
            import pty as _pty
            import tty as _tty
            ma_fd, s_fd = _pty.openpty()
        except Exception as exc:  # noqa: BLE001
            print("ERROR: pty 不可用（%s）——闭环注入需 pty mock 板" % exc,
                  file=sys.stderr)
            return 2
        try:
            js, rc = _cl_run_one(key, a)
            for nm, okk, detail in _cl_assert_one(key, js, rc):
                print("  [%s] %s %s" % ("OK" if okk else "FAIL", key, nm))
            overall &= all(okk for _, okk, _ in _cl_assert_one(key, js, rc))
        finally:
            if ma_fd is not None:
                os.close(ma_fd)
            if s_fd is not None:
                os.close(s_fd)
    print("CLOSEDLOOP FAULT INJECT %s" % ("PASS" if overall else "FAIL"))
    return 0 if overall else 1


def closedloop_selftest_main():
    """--selftest-closedloop：pty mock + stub 推进器四场景闭环注入 + v1 回归。
    机器可读：CLOSEDLOOP FAULT INJECT SELFTEST PASS|FAIL；退出码 0/1。"""
    import tempfile as _tempfile
    print("== [1/2] 闭环注入四场景（pty mock + stub 推进器） ==")
    a = argparse.Namespace(
        out=_tempfile.mkdtemp(prefix="ems_cl_fault_selftest_"),
        rsp_timeout_ms=500, cl_fault_tick=30, cl_stall_s=0.1, cl_ticks=60,
        only=None, scenario="S20", image="ref", port=None, trace=None)
    checks = []
    for key in ("nan", "stall", "garbage", "reset"):
        try:
            js, rc = _cl_run_one(key, a)
            for nm, okk, _ in _cl_assert_one(key, js, rc):
                print("  [%s] %s: %s" % ("OK" if okk else "FAIL", key, nm))
            checks.append((key, all(okk for _, okk, _ in
                                    _cl_assert_one(key, js, rc)) and rc == 0))
        except Exception as exc:  # noqa: BLE001
            print("  [FAIL] %s: 异常 %s" % (key, exc))
            checks.append((key, False))
    print("== [2/2] v1 四场景回归（--selftest 原路径） ==")
    v1_rc = selftest_main()
    checks.append(("v1_regression", v1_rc == 0))
    ok = all(okk for _, okk in checks)
    for key, okk in checks:
        print("  [%s] %s" % ("OK" if okk else "FAIL", key))
    print("CLOSEDLOOP FAULT INJECT SELFTEST %s" % ("PASS" if ok else "FAIL"))
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
    # ---- T-609 闭环注入增量（既有参数零改动；--closed-loop 缺省 off = v1 行为） ----
    ap.add_argument("--closed-loop", action="store_true",
                    help="T-609：闭环注入模式（注入宿主 = ems_closedloop board "
                         "模式 + pty mock 板；缺省 off = v1 行为完全不变）")
    ap.add_argument("--scenario", default=None, choices=("S20", "voyage002"),
                    help="闭环模式：harness 场景（--closed-loop 时用）")
    ap.add_argument("--image", default="ref", choices=("npu", "ref"),
                    help="闭环模式：镜像标识（meta 原样记录）")
    ap.add_argument("--selftest-closedloop", action="store_true",
                    help="T-609：闭环注入 selftest（pty mock + stub 推进器，"
                         "四场景 + v1 回归；无 dylib 依赖）")
    ap.add_argument("--cl-fault-tick", type=int, default=30,
                    help="闭环模式：故障注入 tick 点（默认 30）")
    ap.add_argument("--cl-stall-s", type=float, default=0.1,
                    help="闭环模式：F2 停发时长 [s]（默认 0.1；须 > mock 加速阈 "
                         "0.05 且 < rsp-timeout）")
    ap.add_argument("--cl-ticks", type=int, default=60,
                    help="闭环模式 selftest：harness 总拍数（默认 60）")
    a = ap.parse_args()
    if a.selftest:
        return selftest_main()
    if a.selftest_closedloop:
        return closedloop_selftest_main()
    if a.closed_loop:
        return closedloop_main(a)
    if not a.port:
        ap.error("--port 必填（--selftest/--closed-loop 时可省）")
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
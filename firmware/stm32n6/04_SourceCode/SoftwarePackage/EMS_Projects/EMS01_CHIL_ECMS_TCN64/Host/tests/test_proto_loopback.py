#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_proto_loopback.py — ems_proto 主机环回测试（T-506，spec ems01-chil-ecms-v1）。

被测对象：Appli/App/ems_proto.c（固件与主机共用的协议层，design §UART FR-510 +
§Error Handling）。手段参照 test_ctrl_diff.py / test_driver.c（T-503 前例）：
host gcc -std=c11 -Wall -Wextra -Wpedantic 零警告编译 ems_proto.c 与本目录
test_proto_driver.c（stdin 行协议驱动），逐用例喂入字节流并精确断言 stdout
（FRAME 捕获 / STAT 计数 / TX 回显 / 事件环导出）。

用例（spec T-506 验证 1 覆盖；2026-09-01 B2 起载荷级 seq + WC 帧语义）：
  t01 CRC 已知检验值 + 反例                 t02 迁移契约（新旧实现/分段续算）
  t03 帧编解码往返（七类帧）               t04 INFER_TEST(1212B,seq前缀) + TICK 解码
  t05 RSP_TICK 字段序/往返                  t06 CRC 错→NACK+事件+计数
  t07 截断帧重同步 + 垃圾/文本分流          t08 STALE 置位/清除/事件环/RESETS
  t09 事件环满覆盖 + DUMP 文本契约          t10 随机流整段 vs 逐字节喂入等价
  t11 LEN 守卫 + 文本分流                   t12 随机流分段等价
  t13 RSP_INFER/WC seq 布局（20B/40B）      t14 CMD/RSP_WC_TEST 构造+回环逐字段

载荷级 seq 契约（2026-09-01 用户裁决，design §UART 变更记录）：
  CMD_INFER_TEST(0x04)/CMD_WC_TEST(0x08) 载荷 = [seq u32 LE][302×f32]（1212 B），
  对应 RSP(0x05/0x09) 载荷头原样回显 seq（主机按 seq 归因，级联错位免疫）；
  帧格式（AA 55/TYPE/LEN/CRC 作用域）不变。RSP_INFER_TEST 20 B（+4B seq）、
  RSP_WC_TEST 40 B = [seq u32][cls u8][3B 0][logits 7×f32][wc_us u32]。
  本文件是 ems_inject.py 的帧语义冻结源（单一来源，勿双实现）。

应用层（ems_app.c 的 TYPE 分发控制语义）由固件 Debug 构建 + T-507/T-508 板级
会话验证；本文件为协议层（编解码/CRC/解析/会话/事件环）+ CRC 迁移契约。
"""
from __future__ import annotations

import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
P_DIR = os.path.abspath(os.path.join(HERE, "..", ".."))  # .../EMS01_CHIL_ECMS
PROTO_C = os.path.join(P_DIR, "Appli", "App", "ems_proto.c")
DRIVER_C = os.path.join(P_DIR, "Host", "tests", "test_proto_driver.c")
if not (os.path.isfile(PROTO_C) and os.path.isfile(DRIVER_C)):
    raise SystemExit("FAIL: ems_proto.c / test_proto_driver.c 未找到（路径推导失败）")

# ---- 协议镜像常量（ems_proto.h 数值契约，禁改语义） ----
CMD_RESET, CMD_TICK, RSP_TICK = 0x01, 0x02, 0x03
CMD_INFER_TEST, RSP_INFER_TEST, CMD_PING, CMD_DUMP = 0x04, 0x05, 0x06, 0x07
CMD_WC_TEST, RSP_WC_TEST = 0x08, 0x09
ACK, NACK = 0x7F, 0x7E
ERR_CRC = 0x01
TICK_NONE = 0xFFFFFFFF

# ---- 载荷长度契约（ems_proto.h 2026-09-01；禁改语义） ----
FEAT_N = 302                            # INFER/WC_TEST 特征数（302×f32）
CMD_TEST_LEN = 4 + FEAT_N * 4           # 1212：[seq u32][302×f32]（04/08 共用）
RSP_INFER_LEN = 20                      # [seq u32] + 3×f32 + u32（2026-09-01 +4B）
RSP_WC_LEN = 40                         # 4 seq + 1 cls + 3 保留 + 28 logits + 4 us
MAX_PAYLOAD = CMD_TEST_LEN              # ems_proto.h EMS_PROTO_MAX_PAYLOAD


def crc16_ref(data: bytes) -> int:
    """CCITT-FALSE 独立参考（poly 0x1021, init 0xFFFF），与 ems_proto.c 双写互证。"""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_frame(t: int, payload: bytes = b"") -> bytes:
    """组帧镜像：CRC 作用域 TYPE..PAYLOAD，线上小端。"""
    hdr = bytes([t & 0xFF, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF])
    crc = crc16_ref(hdr + payload) if payload else crc16_ref(hdr)
    return bytes([0xAA, 0x55]) + hdr + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def enc_cmd_tick(idx: int, sig7) -> bytes:
    """CMD_TICK payload：tick_idx u32 + 7×f32（t,P_load,SOC,v_cmd,v_ship,accel,x_ship）。"""
    return struct.pack("<I7f", idx, *sig7)


def enc_rsp_tick(idx, p, e, k, cls, iu, cu, flags) -> bytes:
    """RSP_TICK payload（26 B）：tick u32 + f32×3 + cls u8 + us u32×2 + flags u8。"""
    return struct.pack("<I3fB", idx, p, e, k, cls) + struct.pack("<II", iu, cu) \
        + bytes([flags])


def dec_rsp_tick(pay: bytes):
    """RSP_TICK 26B 解码（冻结布局：idx@0 P@4 e@8 K@12 cls@16 us@17/21 f@25）。"""
    if len(pay) != 26:
        return None
    idx, pgen, ehat, k, cls = struct.unpack_from("<I3fB", pay, 0)
    iu, cu = struct.unpack("<II", pay[17:25])
    return {"idx": idx, "P_gen_ref": pgen, "e_hat": ehat, "K": k, "cls": cls,
            "us_infer": iu, "us_ctrl": cu, "flags": pay[25]}


def payload_seq(pay: bytes):
    """载荷头 seq u32 LE（2026-09-01 载荷级 seq 回显）；<4B 帧无序号返回 None。
    ems_inject.wait_reply 的期望 seq 匹配与 mock 固件回显共用此冻结实现。"""
    if len(pay) < 4:
        return None
    return struct.unpack_from("<I", pay, 0)[0]


def enc_cmd_test(seq: int, feats) -> bytes:
    """CMD_INFER_TEST(0x04)/CMD_WC_TEST(0x08) 共用载荷：[seq u32 LE][302×f32]。"""
    if len(feats) != FEAT_N:
        raise ValueError("feat n=%d != %d" % (len(feats), FEAT_N))
    return struct.pack("<I%df" % FEAT_N, seq & 0xFFFFFFFF, *feats)


def enc_rsp_infer(seq: int, q10: float, q50: float, q90: float, us: int) -> bytes:
    """RSP_INFER_TEST 20 B：[seq u32][q10/q50/q90 f32][infer_us u32]（B1 布局）。"""
    return struct.pack("<I3fI", seq & 0xFFFFFFFF, q10, q50, q90, us)


def dec_rsp_infer(pay: bytes):
    """RSP_INFER_TEST 20B 解码：seq@[0..3]、q@[4..15]、us@[16..19]（B1 布局）。"""
    if len(pay) != RSP_INFER_LEN:
        return None
    seq, q10, q50, q90, us = struct.unpack("<I3fI", pay)
    return {"seq": seq, "q10": q10, "q50": q50, "q90": q90, "us_infer": us}


def enc_rsp_wc(seq: int, cls: int, logits7, us: int) -> bytes:
    """RSP_WC_TEST 40B：[seq u32][cls u8][3B 保留=0][logits 7×f32][wc_us u32]。"""
    if len(logits7) != 7:
        raise ValueError("logits n=%d != 7" % len(logits7))
    return struct.pack("<IB3x7fI", seq & 0xFFFFFFFF, cls & 0xFF, *logits7, us)


def dec_rsp_wc(pay: bytes):
    """RSP_WC_TEST 40B 解码（冻结布局：seq@0 cls@4 保留@5..7 logits@8..35 us@36）。"""
    if len(pay) != RSP_WC_LEN:
        return None
    seq, cls = struct.unpack_from("<IB", pay, 0)
    logits = struct.unpack_from("<7f", pay, 8)
    (us,) = struct.unpack_from("<I", pay, 36)
    return {"seq": seq, "cls": cls, "logits": list(logits), "us_wc": us,
            "reserved": bytes(pay[5:8])}


def lcg_bytes(n: int, seed: int = 0x5EED2026) -> bytes:
    s = seed & 0x7FFFFFFF
    out = bytearray()
    for _ in range(n):
        s = (s * 1103515245 + 12345) & 0x7FFFFFFF
        out.append((s >> 15) & 0xFF)
    return bytes(out)


def hexs(b: bytes) -> str:
    return b.hex().upper()


# ------------------------------------------------------------------ 驱动构建


def build_driver() -> str:
    tmp = tempfile.mkdtemp(prefix="ems_proto_loopback_")
    exe = os.path.join(tmp, "test_proto_driver")
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-O2",
           "-I", os.path.join(P_DIR, "Appli", "App"), "-o", exe,
           DRIVER_C, PROTO_C]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if r.returncode != 0 or r.stderr.strip():
        raise SystemExit("BUILD FAIL（必须零警告）:\n" + r.stderr)
    return exe


def run_lines(driver: str, lines: list) -> list:
    inp = "\n".join(lines) + "\n"
    r = subprocess.run([driver], input=inp, capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        raise AssertionError("driver exit %d: %s" % (r.returncode, r.stderr))
    return r.stdout.splitlines()


def one(lines: list, prefix: str) -> str:
    hits = [l for l in lines if l.startswith(prefix)]
    assert len(hits) == 1, "want exactly one %r, got %r" % (prefix, hits[:6])
    return hits[0]


def many(lines: list, prefix: str) -> list:
    return [l for l in lines if l.startswith(prefix)]


def none_of(lines: list, prefix: str) -> None:
    hits = many(lines, prefix)
    assert not hits, "unexpected lines: %r" % (hits,)


def tick_frame(idx: int, sig=None) -> bytes:
    sig = sig if sig is not None else [1.0, 100.0, 0.6, 3.0, 3.0, 0.0, 0.0]
    return build_frame(CMD_TICK, enc_cmd_tick(idx, sig))


# 往返用例素材：(TYPE, payload)
RT_CASES = [
    (CMD_RESET, b""),
    (CMD_PING, bytes([CMD_PING])),
    (ACK, bytes([CMD_PING])),
    (NACK, bytes([CMD_PING, 0x02])),
    (RSP_INFER_TEST, enc_rsp_infer(4242, 1.5, 2.5, 3.5, 4242)),
    (RSP_WC_TEST, enc_rsp_wc(77, 5, [0.5, -1.25, 2.0, 3.5, -0.75, 6.0, 0.125], 4321)),
    (CMD_TICK, enc_cmd_tick(7, [10.5, 3200.0, 0.625, 4.25, 4.25, -0.125, 12000.0])),
    (RSP_TICK, enc_rsp_tick(9, 320.0, -0.5, 0.75, 0xFF, 1234, 5678, 0x05)),
]

# ---------------------------------------------------------------- 用例实现


def t01_crc_known_and_single_bit(driver):
    """CRC 已知检验值（'123456789'→0x29B1）正例；单 bit 翻转反例由 t06 覆盖。"""
    ln = run_lines(driver, ["CRC 313233343536373839", "OLDCRC 313233343536373839"])
    assert ln[0] == "CRC 29B1", ln
    assert ln[1] == "CRC 29B1", "迁移契约失配：T-505 原始实现 ≠ ems_proto_crc16"


def t02_crc_migration_chain(driver):
    """迁移契约：1211 B 随机模式新旧实现逐位一致 + 分段续算 = 整段。"""
    data = lcg_bytes(1211)
    ln = run_lines(driver, [
        "CRC " + hexs(data),
        "OLDCRC " + hexs(data),
        "CHAIN %04X %s" % (crc16_ref(data[:3]), hexs(data[3:])),
        "OLDCHAIN %04X %s" % (crc16_ref(data[:3]), hexs(data[3:])),
        "CHAIN 0000 " + hexs(data),
        "OLDCHAIN 0000 " + hexs(data),
    ])
    want = "%04X" % crc16_ref(data)
    want_line = "CRC " + want
    got = many(ln, "CRC ")
    assert got[:2] == ["CRC " + want] * 2, got
    # 分段续算：chain(crc16(hdr), payload) == crc16(hdr+payload)；种子任意时新旧两版恒等
    assert got[2] == got[3] == "CRC " + want, (got[2], want)
    assert got[4] == got[5], "chain 种子 0 时新旧实现不同"


def t03_frame_roundtrip(driver):
    """BUILD → TX 字节 == Python 镜像；FEED 回环 → FRAME 捕获与 STAT 计数。"""
    inp = ["INIT"]
    for t, pl in RT_CASES:
        inp.append("BUILD %02X %s" % (t, hexs(pl)))
        inp.append("FEED " + hexs(build_frame(t, pl)))
    ln = run_lines(driver, inp)
    tx, fr, sts = many(ln, "TX "), many(ln, "FRAME "), many(ln, "STAT")
    assert len(tx) == len(fr) == len(RT_CASES), (len(tx), len(fr))
    st = sts[1:]  # 丢弃 INIT 的首条 STAT（ok=0）
    for i, (t, pl) in enumerate(RT_CASES):
        assert tx[i] == "TX " + hexs(build_frame(t, pl)), (i, tx[i])
        assert fr[i] == "FRAME %02X %d %s" % (t, len(pl), hexs(pl)), fr[i]
        # CMD_TICK（第 7 案）起会话在 feed 内推进 → sess=1 且 STALE 计时开始
        want_sess = 0 if (t != CMD_TICK and t != RSP_TICK) else 1
        assert st[i] == "STAT ok=%d crcerr=0 stale=0 sess=%d ev=0 total=0" % (i + 1, want_sess), st[i]


def t04_tick_infer_layout(driver):
    """CMD_INFER_TEST 1208 B 全帧 + CMD_TICK 解码（字段序/小端/f32）。"""
    pl = enc_cmd_test(0x5A5A0001, [float((i * 7) % 101) - 50.0 for i in range(FEAT_N)])
    tick_pl = enc_cmd_tick(7, [10.5, 3200.0, 0.625, 4.25, 4.25, -0.125, 12000.0])
    ln = run_lines(driver, [
        "INIT",
        "BUILD 04 " + hexs(pl),
        "FEED " + hexs(build_frame(CMD_INFER_TEST, pl)),
        "DECTICK " + hexs(tick_pl),
    ])
    one(ln, "TX " + hexs(build_frame(CMD_INFER_TEST, pl)))
    one(ln, "FRAME 04 %d %s" % (CMD_TEST_LEN, hexs(pl)))
    # seq 前缀断言（B1 布局）：载荷头 4B = seq u32 LE，特征块自 +4 起
    assert struct.unpack_from("<I", pl, 0)[0] == 0x5A5A0001, pl[:8].hex()
    assert len(pl) == CMD_TEST_LEN and len(pl) - 4 == FEAT_N * 4, len(pl)
    assert many(ln, "STAT")[-1] == "STAT ok=1 crcerr=0 stale=0 sess=0 ev=0 total=0", ln


def t05_cmd_tick_decode(driver):
    """DECTICK：7 信号小端字段序（%.9g 与 Python f32 round-trip）。"""
    tp = enc_cmd_tick(7, [10.5, 3200.0, 0.625, 4.25, 4.25, -0.125, 12000.0])
    ln = run_lines(driver, ["DECTICK " + hexs(tp)])
    assert one(ln, "DECTICK ") == (
        "DECTICK idx=7 t=10.5 P=3200 SOC=0.625 vcmd=4.25 vs=4.25 a=-0.125 x=12000"), ln


def t06_rsp_tick_roundtrip(driver):
    """ENCRSP 字段序/偏移（0/4/8/12/16/19/21/22…见 design 帧表）+ DECRSP 往返。"""
    want = enc_rsp_tick(9, 320.0, -0.5, 0.75, 0xFF, 1234, 5678, 0x05)
    ln = run_lines(driver, [
        "ENCRSP 9 320 -0.5 0.75 FF 1234 5678 05",
        "DECRSP " + hexs(want),
    ])
    got = bytes.fromhex(one(ln, "ENCRSP ").split(" ", 1)[1])
    assert got == want, (got.hex(), want.hex())
    # 线上偏移抽检（design 帧表 0/4/8/12/16/17/19/21
    assert want[16] == 0xFF and want[17:21] == struct.pack("<I", 1234) \
        and want[21:25] == struct.pack("<I", 5678) and want[25] == 0x05, want.hex()


def t07_crc_negative(driver):
    """CRC 错帧 → NACK(echo,err=01) + 事件环 + stat_crc_err（Error Handling 契约）。"""
    good = tick_frame(7)
    bad = bytearray(good)
    bad[-1] ^= 0x01
    nack = build_frame(NACK, bytes([CMD_TICK, ERR_CRC]))
    ln = run_lines(driver, [
        "INIT",
        "FEED " + hexs(good),
        "FEED " + hexs(bytes(bad)),
        "EVENTS",
    ])
    assert many(ln, "STAT")[-1] == "STAT ok=1 crcerr=1 stale=0 sess=1 ev=1 total=1", ln
    tx = many(ln, "TX ")
    assert tx == ["TX " + hexs(nack)], (tx,)
    ev = one(ln, "EVENT n=0 ")
    assert re.match(r"EVENT n=0 type=1 aux=2 tick=7 ms=\d+$", ev), ev
    assert one(ln, "NEV ") == "NEV 1", ln


def t08_stale_session(driver):
    """STALE：2000 严格界；gap 2200→置位+事件（tick=前拍）；下一拍（gap 400）清除。"""
    inp = ["INIT", "LASTTICK",
           "TIME 0", "FEED " + hexs(tick_frame(1)),
           "TIME 2000", "FEED " + hexs(tick_frame(2)),
           "LASTTICK",
           "TIME 4200", "FEED " + hexs(tick_frame(3)),
           "EVENTS",
           "TIME 4600", "FEED " + hexs(tick_frame(4)),
           "RESETS", "LASTTICK", "EVENTS"]
    ln = run_lines(driver, inp)
    lts = many(ln, "LASTTICK ")
    assert lts == ["LASTTICK FFFFFFFF", "LASTTICK 00000002", "LASTTICK FFFFFFFF"], lts
    sts = many(ln, "STAT")
    assert sts[3] == "STAT ok=3 crcerr=0 stale=1 sess=1 ev=1 total=1", sts
    assert sts[4] == "STAT ok=4 crcerr=0 stale=0 sess=1 ev=1 total=1", sts
    ev = many(ln, "EVENT n=0 ")
    assert len(ev) == 1 and re.match(r"EVENT n=0 type=3 aux=0 tick=2 ms=4200$", ev[0]), ev
    assert many(ln, "NEV ") == ["NEV 1", "NEV 0"], ln  # EVENTS×2：置位后 / 复位后


def t09_stale_boundary_equal(driver):
    """gap == 2000 ms 不置 STALE（严格大于，design 注入间隔 = 2 s）。"""
    ln = run_lines(driver, ["INIT", "TIME 0", "FEED " + hexs(tick_frame(1)),
                            "TIME 2000", "FEED " + hexs(tick_frame(2)), "EVENTS"])
    assert many(ln, "STAT")[-1] == "STAT ok=2 crcerr=0 stale=0 sess=1 ev=0 total=0", ln
    assert one(ln, "NEV ") == "NEV 0", ln


def t10_ring_overflow_dump(driver):
    """事件环满覆盖：40 错 → kept=32 total=40；DUMP 行数/序号/首行/汇总行。"""
    good = build_frame(CMD_PING, bytes([CMD_PING]))
    bad = bytearray(good)
    bad[-1] ^= 0xFF
    inp = ["INIT"]
    for _ in range(40):
        inp.append("FEED " + hexs(bytes(bad)))
    inp += ["EVENTS", "DUMP"]
    ln = run_lines(driver, inp)
    assert one(ln, "NEV ") == "NEV 32", ln
    assert many(ln, "STAT")[-1].endswith("ev=32 total=40"), many(ln, "STAT")[-1]
    assert len(many(ln, "TX ")) == 40, "每个 CRC 错都应 NACK"
    txt = many(ln, "TXTEXT EVT ")
    assert len(txt) == 33, len(txt)  # 32 事件行 + 1 汇总行
    assert txt[0].startswith("TXTEXT EVT seq=9 "), txt[0]  # 最旧 seq = 40-32+1
    assert txt[-1] == "TXTEXT EVT total=40 kept=32 cap=32\\r\\n", txt[-1]


def t11_len_guard_and_text(driver):
    """LEN>1212（EMS_PROTO_MAX_PAYLOAD）静默丢弃；文本行静默；合法帧不受影响。"""
    ln = run_lines(driver, [
        "INIT",
        "FEED AA55FFFFFF04",                     # TYPE=0xFF LEN=0xFFFF > 1212 → 静默丢
        "FEED " + hexs(b"Hello\r\n"),              # 文本日志行 → 静默
        "FEED " + hexs(build_frame(CMD_PING, bytes([CMD_PING]))),
        "EVENTS",
    ])
    assert many(ln, "STAT")[-1] == "STAT ok=1 crcerr=0 stale=0 sess=0 ev=0 total=0", ln
    fr = many(ln, "FRAME ")
    assert len(fr) == 1 and fr[0] == "FRAME 06 1 06", fr
    assert not many(ln, "TX "), "文本/超长帧不应触发 NACK"


def t12_fuzz_feed_equivalence(driver):
    """同 500 B 随机流：逐字节 vs 每 8 B 分段 → 终态与捕获完全一致。"""
    rb = lcg_bytes(500)

    def inputs(step):
        ls = ["INIT"]
        for i in range(0, len(rb), step):
            ls.append("FEED " + hexs(rb[i:i + step]))
        ls.append("EVENTS")
        return ls

    def obs(lines):
        return [l for l in lines if l.startswith(("FRAME", "TX ", "NEV", "EVENT n="))]

    r1, r2 = run_lines(driver, inputs(1)), run_lines(driver, inputs(8))
    assert obs(r1) == obs(r2), (obs(r1)[-4:], obs(r2)[-4:])
    assert many(r1, "STAT")[-1] == many(r2, "STAT")[-1], "终态统计不同"


def t13_rsp_seq_layouts(driver):
    """RSP_INFER_TEST 20B / RSP_WC_TEST 40B 冻结布局：BUILD 字节级 + 逐字段解码。

    B1 布局：RSP seq@[0..3]、q@[4..15]、us@[16..19]；RSP_WC seq@0、cls@4、
    保留@5..7=0、logits@[8..35]、us@[36..39]（design §UART 帧表 2026-09-01）。"""
    rinf = enc_rsp_infer(0x01020304, 1.5, -2.5, 3.5, 98765)
    rwc = enc_rsp_wc(0xA5A5A5A5, 5, [0.5, -1.25, 2.0, 3.5, -0.75, 6.0, 0.125], 4321)
    ln = run_lines(driver, [
        "INIT",
        "BUILD 05 " + hexs(rinf),
        "BUILD 09 " + hexs(rwc),
        "FEED " + hexs(build_frame(RSP_INFER_TEST, rinf)),
        "FEED " + hexs(build_frame(RSP_WC_TEST, rwc)),
        "EVENTS",
    ])
    # 构造字节级：TX 回显 == Python 镜像组帧（布局/CRC/LEN 全链）
    one(ln, "TX " + hexs(build_frame(RSP_INFER_TEST, rinf)))
    one(ln, "TX " + hexs(build_frame(RSP_WC_TEST, rwc)))
    # FEED 回环 → FRAME 捕获 == 原载荷（解析器对 20B/40B 帧 LEN 守卫通过）
    one(ln, "FRAME 05 %d %s" % (RSP_INFER_LEN, hexs(rinf)))
    one(ln, "FRAME 09 %d %s" % (RSP_WC_LEN, hexs(rwc)))
    assert many(ln, "STAT")[-1] == "STAT ok=2 crcerr=0 stale=0 sess=0 ev=0 total=0", ln
    # 逐字段解码断言（ems_inject 复用的冻结解码器）
    d = dec_rsp_infer(rinf)
    assert d == {"seq": 0x01020304, "q10": 1.5, "q50": -2.5, "q90": 3.5,
                 "us_infer": 98765}, d
    assert struct.unpack_from("<I", rinf, 16)[0] == 98765, "us@[16..19] 偏移"
    w = dec_rsp_wc(rwc)
    assert w["seq"] == 0xA5A5A5A5 and w["cls"] == 5 and w["us_wc"] == 4321, w
    assert w["reserved"] == b"\x00\x00\x00", w
    assert w["logits"] == [0.5, -1.25, 2.0, 3.5, -0.75, 6.0, 0.125], w["logits"]
    assert dec_rsp_infer(b"\x00" * 16) is None and dec_rsp_wc(b"\x00" * 39) is None, \
        "长度不符应拒解码"


def t14_wc_test_roundtrip(driver):
    """CMD_WC_TEST/RSP_WC_TEST 构造+解析+回环：seq/cls/logits/us 逐字段断言。"""
    seq = 0x00C0FFEE
    feats = [100.0 + 0.5 * (i % 64) - 16.0 for i in range(FEAT_N)]
    cmd_pl = enc_cmd_test(seq, feats)
    logits = [float(j) * 0.25 - 0.75 for j in range(7)]
    rsp = enc_rsp_wc(seq, 6, logits, 4321)
    assert len(cmd_pl) == CMD_TEST_LEN and len(rsp) == RSP_WC_LEN
    ln = run_lines(driver, [
        "INIT",
        "BUILD 08 " + hexs(cmd_pl),
        "FEED " + hexs(build_frame(CMD_WC_TEST, cmd_pl)),
        "BUILD 09 " + hexs(rsp),
        "FEED " + hexs(build_frame(RSP_WC_TEST, rsp)),
        "EVENTS",
    ])
    one(ln, "TX " + hexs(build_frame(CMD_WC_TEST, cmd_pl)))
    one(ln, "TX " + hexs(build_frame(RSP_WC_TEST, rsp)))
    one(ln, "FRAME 08 %d %s" % (CMD_TEST_LEN, hexs(cmd_pl)))
    one(ln, "FRAME 09 %d %s" % (RSP_WC_LEN, hexs(rsp)))
    assert many(ln, "STAT")[-1] == "STAT ok=2 crcerr=0 stale=0 sess=0 ev=0 total=0", ln
    none_of(ln, "TXTEXT ")  # WC 帧不产生 NACK/文本
    # CMD 侧：seq 头 + 特征块逐字段
    assert struct.unpack_from("<I", cmd_pl, 0)[0] == seq
    assert struct.unpack_from("<%df" % FEAT_N, cmd_pl, 4) == tuple(feats)
    # RSP 侧逐字段（冻结 dec_rsp_wc）：seq 回显 + cls + 保留 0 + logits + us
    d = dec_rsp_wc(rsp)
    assert d["seq"] == seq, "RSP_WC seq 必须原样回显"
    assert d["cls"] == 6 and d["reserved"] == b"\x00\x00\x00", d
    assert d["logits"] == list(struct.unpack("<7f", struct.pack("<7f", *logits))), d
    assert d["us_wc"] == 4321, d


# ---------------------------------------------------------------- 测试脚手架

TESTS = [
    t01_crc_known_and_single_bit,
    t02_crc_migration_chain,
    t03_frame_roundtrip,
    t04_tick_infer_layout,
    t05_cmd_tick_decode,
    t06_rsp_tick_roundtrip,
    t07_crc_negative,
    t08_stale_session,
    t09_stale_boundary_equal,
    t10_ring_overflow_dump,
    t11_len_guard_and_text,
    t12_fuzz_feed_equivalence,
    t13_rsp_seq_layouts,
    t14_wc_test_roundtrip,
]


def main() -> int:
    print("== build ==")
    driver = build_driver()
    print("PASS build (gcc -Wall -Wextra -Wpedantic 零警告)")
    fails = 0
    for fn in TESTS:
        try:
            fn(driver)
            print("PASS %s" % fn.__name__)
            fails += 0
        except AssertionError as e:
            print("FAIL %s: %s" % (fn.__name__, e))
            fails += 1
        except Exception as e:  # noqa: BLE001
            print("FAIL %s (exception): %r" % (fn.__name__, e))
            fails += 1
    print("---- %d/%d passed ----" % (len(TESTS) - fails, len(TESTS)))
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    print("T-506 ems_proto loopback tests")
    sys.exit(main())


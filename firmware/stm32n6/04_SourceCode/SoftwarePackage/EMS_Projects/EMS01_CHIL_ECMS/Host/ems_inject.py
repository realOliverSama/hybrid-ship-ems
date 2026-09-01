#!/usr/bin/env python3
# ems_inject.py — T-506 交付物：EMS01 串口注入/对拍 CLI（Host 侧）
#
# 行为（design §API / §UART 帧表，FR-503/FR-504/FR-509/FR-510 · T-506/B2）：
#   1) TICK 注入（默认）：--port DEV 连接 EMS01 板 115200-8N1；CMD_RESET →
#      逐拍 CMD_TICK（--trace CSV，7 信号全列 tick,t,P_load_kW,SOC,v_cmd_mps,
#      v_ship_mps,accel_mps2,x_ship_m，同 fw_reference ref_G2_*.csv）→ 逐拍等
#      RSP_TICK（--rsp-timeout-ms，默认 500；超时记 TIMEOUT 继续）。对拍：
#      --ref-int8（gen_reference_int8 模式 B CSV）比 e_hat/K/cls_eff（层②）；
#      --ref（Simulink 参考 CSV）比 P_gen_ref（层①/③：exact-match / ≤1 网格步）。
#   2) INFER_TEST 批量（--infer-test DIR|CSV，FR-504 M1 用）：逐向量发
#      CMD_INFER_TEST（载荷 [seq u32][302×f32]，send 自动加 seq 头）→ 收
#      RSP_INFER_TEST（[seq 回显][q10/q50/q90 kWh + infer_us]）→ 与同目录
#      reference_int8_val200.csv 对拍。seq 配对：wait_reply 按 want_seq 跳过
#      陈旧帧；单向量 ≤--retry 次尝试，超时→换新 seq 重发同向量。
#   3) WC_TEST 批量（--wc-test，T-507 类别一致率）：同构 infer 批量
#      （CMD_WC_TEST [seq][302×f32] → RSP_WC_TEST [seq][cls][logits×7][us]），
#      对拍 reference cls/logits（positional 键控）→ wc_compare.csv/json。
#   4) --selftest：无硬件离线链路（pty + mock 固件线程）验证本工具收发全链
#      （mock 同步新语义：infer seq 回显 + WC 帧）。
# 帧语义（构造/CRC/载荷级编解码/seq）全部复用 Host/tests/test_proto_loopback.py
# 的冻结实现（单一来源，勿双实现）；仅 RX 流分帧（parse_stream）在本文件自实现。
# 路径纪律：本文件位置相对推导；禁止硬编码绝对路径。
# pyserial 在 .venv_tf（REPO/model/simulink/.venv_tf）内；用对应 python 运行。

import argparse
import csv
import json
import math
import os
import struct
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))          # EMS01/Host
TESTS_DIR = os.path.join(HERE, "tests")

# 复用环回测试冻结的协议实现（单一来源：帧构造/CRC/载荷编解码/seq 语义；
# 仅 RX 流分帧 parse_stream 在本文件自实现）
sys.path.insert(0, TESTS_DIR)
from test_proto_loopback import (  # noqa: E402
    CMD_RESET, CMD_TICK, RSP_TICK, CMD_INFER_TEST, RSP_INFER_TEST,
    CMD_PING, CMD_DUMP, ACK, NACK, ERR_CRC, CMD_WC_TEST, RSP_WC_TEST,
    build_frame, enc_rsp_tick, enc_cmd_tick, crc16_ref,
    enc_cmd_test, enc_rsp_infer, enc_rsp_wc,
    dec_rsp_tick, dec_rsp_infer, dec_rsp_wc, payload_seq,
    CMD_TEST_LEN, RSP_INFER_LEN, RSP_WC_LEN, FEAT_N, MAX_PAYLOAD,
)

GRID_STEP_KW = 20.0  # design 固定：P_gen 网格步 20 kW（层②/③门禁单位）
MOCK_ERR_LEN = 0x03  # ems_proto.h EMS_PROTO_ERR_LEN（mock 固件 LEN 守卫用）


def parse_stream(buf):
    """流式解帧（主机侧 RX）：返回 (frames, consumed)。

    帧 = AA 55 | TYPE u8 | LEN u16LE | PAYLOAD | CRC16LE；CRC 作用域 TYPE..PAYLOAD。
    CRC 坏帧：返回 type=-1 占位，消费整帧（上层计数 crcerr）。
    LEN>MAX_PAYLOAD(1212) 静默跳过（与固件 ems_proto.c 一致）。
    """
    frames = []
    i = 0
    n = len(buf)
    while i + 4 <= n:
        if buf[i] != 0xAA or buf[i + 1] != 0x55:
            i += 1
            continue
        if i + 5 > n:
            break
        ln = buf[i + 3] | (buf[i + 4] << 8)
        if ln > MAX_PAYLOAD:
            i += 1
            continue
        total = 5 + ln + 2
        if i + total > n:
            break
        ttype = buf[i + 2]
        pay = bytes(buf[i + 5:i + 5 + ln])
        crc_rx = buf[i + 5 + ln] | (buf[i + 5 + ln + 1] << 8)
        hdr = bytes([ttype, ln & 0xFF, (ln >> 8) & 0xFF])
        crc_calc = crc16_ref(hdr + pay)
        if crc_calc != crc_rx:
            frames.append({"type": -1, "payload": b"", "crc_err": 1})
        else:
            frames.append({"type": ttype, "payload": pay})
        i += total
    return frames, i


class FdIO:
    """--selftest 用：os fd 上的 pyserial 最小接口（read 带 select 超时）。"""

    def __init__(self, fd):
        self.fd = fd

    def write(self, data):
        return os.write(self.fd, data)

    def read(self, n):
        import select
        r, _, _ = select.select([self.fd], [], [], 0.01)
        if r:
            try:
                return os.read(self.fd, 4096)
            except OSError:
                pass
        return b""

    def close(self):
        os.close(self.fd)


class SerialSession:
    """串口会话：write 帧 + 带超时等响应（pyserial；--selftest 用 FdIO）。

    载荷级 seq（2026-09-01）：send 对 CMD_INFER_TEST/CMD_WC_TEST 自动在载荷头
    prepend seq u32 LE（自增计数器，其他类型不动）并返回本帧 seq；wait_reply
    的 want_seq 匹配：目标类型帧载荷头 seq ≠ 期望 → 视为陈旧帧跳过继续等
    （级联错位免疫的收向，design §UART 变更记录 2026-09-01）。"""

    def __init__(self, dev=None, baud=115200, timeout_ms=2000, io=None):
        if io is not None:
            self.ser = io
        else:
            import serial
            # 2026-09-01 晚（bug#5 修复 v2，用户判别实验定根因）：pyserial
            # read(size) 攒满 size 字节或吃满 timeout 才返回——固定 timeout=2.0
            # 只改长度实测：read(7)=4.0ms（7B=ACK 帧长，到齐即返）vs
            # read(64)=2006.3ms（8B 到齐但 <64 → 吃满 timeout）；in_waiting
            # 真值 first_byte=6.4ms（ACK 真实 ~6ms 到达，无任何驱动延迟）。
            # 故串口打开为非阻塞（timeout=0），收向一律走 _read_some；帧完整性
            # 由 parse_stream 保证，事务超时由 wait_reply deadline
            # （--rsp-timeout-ms）承担，与单次 read 阻塞时长彻底解耦（跨
            # macOS/Windows/Linux 行为一致）。
            self.ser = serial.Serial(dev, baud, timeout=0)
        self.rx = bytearray()
        self.tx_seq = 0        # 下一个测试命令载荷 seq（仅 0x04/0x08 消费）
        self.n_stale_skip = 0  # wait_reply 跳过的 seq 不匹配陈旧帧计数

    def _read_some(self):
        """非阻塞收字节（bug#5 v2）：in_waiting 即到即读，空转 2ms 轮询。

        禁止用大 size + 长 timeout 的阻塞 read（pyserial 攒字节语义会吃满
        timeout——bug#5 根因，2026-09-01 用户判别实验证实）；帧完整性由
        parse_stream 保证。FdIO（--selftest）无 in_waiting，走 select 短读。"""
        in_waiting = getattr(self.ser, "in_waiting", None)
        if in_waiting is not None:
            if in_waiting:
                return self.ser.read(in_waiting)
            time.sleep(0.002)
            return b""
        return self.ser.read(4096)  # FdIO：select(10ms) + os.read 即到即返

    def close(self):
        if self.ser is not None:
            self.ser.close()
            self.ser = None

    def send(self, ttype, payload=b""):
        """发帧；0x04/0x08 自动加 seq u32 LE 载荷头并返回该 seq（其他类型
        原样发送返回 None）。seq 语义复用冻结源 payload_seq/build_frame。"""
        seq = None
        if ttype in (CMD_INFER_TEST, CMD_WC_TEST):
            seq = self.tx_seq & 0xFFFFFFFF
            self.tx_seq = (self.tx_seq + 1) & 0xFFFFFFFF
            payload = struct.pack("<I", seq) + (payload or b"")
        self.ser.write(build_frame(ttype, payload))
        return seq

    def wait_reply(self, want_type, timeout_ms=2000, want_seq=None):
        """等一帧 want_type；CRC 坏帧继续等；超时返回 None。

        want_seq 非 None（载荷级 seq 配对，2026-09-01）：目标类型帧载荷头
        seq ≠ 期望 → 视为陈旧/错位帧跳过（继续等本 seq 的响应）。"""
        deadline = time.monotonic() + timeout_ms / 1000.0
        while time.monotonic() < deadline:
            frames, used = parse_stream(bytes(self.rx))
            if used:
                del self.rx[:used]
            for f in frames:
                if f["type"] != want_type:
                    continue  # 忽略其它帧（含 CRC 坏帧占位）
                if want_seq is not None and payload_seq(f["payload"]) != want_seq:
                    self.n_stale_skip += 1
                    continue  # 陈旧/错位帧：seq 不匹配，继续等本 seq
                return f["payload"]
            chunk = self._read_some()
            if chunk:
                self.rx.extend(chunk)
        return None


TRACE_COLS = ("t", "P_load_kW", "SOC", "v_cmd_mps", "v_ship_mps",
              "accel_mps2", "x_ship_m")


def load_trace(path, limit=0):
    """读注入轨迹 CSV：7 信号全列（fw_reference ref_G2_*.csv 同款）。

    列 tick?,t,P_load_kW,SOC,v_cmd_mps,v_ship_mps,accel_mps2,x_ship_m。
    v_cmd_mps 缺省回退 v_ship_mps；accel/x_ship 缺列→0.0 并警告（进预测特征窗，
    与固件/参考不一致会导致对拍系统性偏差）。tick 缺省取行序。
    """
    rows = []
    warned = {"accel_mps2": False, "x_ship_m": False}
    with open(path, newline="") as f:
        rd = csv.DictReader(f)
        for i, r in enumerate(rd):
            tick = int(r["tick"]) if r.get("tick") else i
            vs = float(r["v_ship_mps"] or 0.0)
            row = {
                "tick": tick,
                "t": float(r["t"]),
                "P_load_kW": float(r.get("P_load_kW") or 0.0),
                "SOC": float(r["SOC"]) if r.get("SOC") else 0.6,
                "v_cmd_mps": float(r["v_cmd_mps"]) if r.get("v_cmd_mps") else vs,
                "v_ship_mps": vs,
            }
            for c in ("accel_mps2", "x_ship_m"):
                if r.get(c):
                    row[c] = float(r[c])
                else:
                    row[c] = 0.0
                    if not warned[c]:
                        print("WARN: trace 缺 %s 列，按 0.0 注入" % c)
                        warned[c] = True
            rows.append(row)
            if limit and len(rows) >= limit:
                break
    return rows


def run_inject(sess, rows, rsp_ms=2000, do_reset=True):
    """RESET → CMD_TICK 流 → 收集 RSP_TICK。返回 (rows_out, errors)。"""
    errs = {"timeout": 0, "crcerr": 0, "idx_mismatch": 0}
    if do_reset:
        sess.send(CMD_RESET)
        b = sess.wait_reply(ACK, rsp_ms)
        if b is None or not b or b[0] != CMD_RESET:
            print("RESET 未确认（ACK payload=0x%02X）" % (b[0] if b else 0xFF))
            sys.exit(2)
    out = []
    for r in rows:
        sig = tuple(r[c] for c in TRACE_COLS)
        sess.send(CMD_TICK, enc_cmd_tick(r["tick"], sig))
        pay = sess.wait_reply(RSP_TICK, rsp_ms)
        if pay is None:
            errs["timeout"] += 1
            out.append({"idx": r["tick"], "err": "timeout"})
            continue
        d = dec_rsp_tick(pay)
        if d is None or d["idx"] != r["tick"]:
            errs["idx_mismatch"] += 1
            out.append({"idx": r["tick"], "err": "bad_rsp"})
            continue
        d["t"] = r["t"]
        d["P_load_kW"] = r["P_load_kW"]
        out.append(d)
    return out, errs


def run_infer_test(sess, vectors, rsp_ms=500, frame_gap_ms=300, retry=3):
    """CMD_INFER_TEST 批量（FR-504 M1）：302×f32 原始特征逐向量注入。

    vectors: list[list[float]]（302 维原始物理量，vectors_raw.csv 口径）。
    载荷 = [seq u32][302×f32]（send 自动 prepend seq，2026-09-01 载荷级回显）。
    重试：单向量最多 retry 次尝试；超时（含陈旧帧被 seq 配对跳过后的等待
    超时）→ 间隔 frame_gap_ms 换新 seq 重发同向量；retry 次全失败才记 timeout。
    frame_gap_ms: 帧间发送间隔（T-507 实测：CH340 收发换向后立即发帧会损坏
    首段字节（CRC 损→NACK/丢帧），默认 ≥300ms 稳定窗口）。
    返回 (rows_out, errors)；rows_out[i] = {idx, seq, q10, q50, q90, us_infer}。
    """
    errs = {"timeout": 0, "bad_rsp": 0, "resend": 0, "stale_skip": 0}
    out = []
    # 链路预同步（T-507 实测：开口瞬态/换向毛刺会损坏首帧 → NACK + 级联错位；
    # PING 直到 ACK 才开始批量，保证首帧在干净链路上发出）
    for _ in range(5):
        sess.send(CMD_PING, b"")
        if sess.wait_reply(ACK, 400) is not None:
            break
        time.sleep(0.2)
    else:
        raise RuntimeError("PING pre-sync failed: 链路未就绪（无 ACK）")
    for i, vec in enumerate(vectors):
        if len(vec) != FEAT_N:
            raise ValueError("向量 %d 维度 %d != %d" % (i, len(vec), FEAT_N))
        feat = struct.pack("<%df" % FEAT_N, *vec)
        pay = None
        seq = None
        for a in range(max(1, retry)):
            if a:  # 重试：间隔后换新 seq 重发同向量（send 自增 seq）
                time.sleep(frame_gap_ms / 1000.0)
                errs["resend"] += 1
            elif frame_gap_ms and i:
                time.sleep(frame_gap_ms / 1000.0)
            seq = sess.send(CMD_INFER_TEST, feat)
            pay = sess.wait_reply(RSP_INFER_TEST, rsp_ms, want_seq=seq)
            if pay is not None:
                break
        errs["stale_skip"] += sess.n_stale_skip
        sess.n_stale_skip = 0
        if pay is None:
            errs["timeout"] += 1
            out.append({"idx": i, "seq": seq, "err": "timeout"})
            continue
        d = dec_rsp_infer(pay)
        if d is None:
            errs["bad_rsp"] += 1
            out.append({"idx": i, "seq": seq, "err": "bad_rsp"})
            continue
        d["idx"] = i  # seq 由冻结解码器 dec_rsp_infer 给出（回显字段）
        out.append(d)
    return out, errs


def load_vectors_raw(path):
    """读 302 维原始特征 CSV（无表头，每行 302 列，vectors_raw.csv 口径）。"""
    vecs = []
    with open(path, newline="") as f:
        for row in csv.reader(f):
            if row:
                vecs.append([float(x) for x in row])
    return vecs


def load_ref_int8(path):
    """Host int8 参考（层②）：模式 A 目录（reference_int8_val200.csv）或
    模式 B 轨迹 CSV（tick,…,e_hat,K,…,cls_eff,…）。按 tick/idx 索引。"""
    p = path
    if os.path.isdir(p):
        p = os.path.join(p, "reference_int8_val200.csv")
    if not os.path.exists(p):
        return {}
    ref = {}
    with open(p, newline="") as f:
        for r in csv.DictReader(f):
            k = int(r["tick"]) if r.get("tick") else int(r["idx"])
            ref[k] = {
                "q50": float(r["q50_kwh"]) if r.get("q50_kwh") else None,
                "e_hat": float(r["e_hat"]) if r.get("e_hat") else None,
                "K": float(r["K"]) if r.get("K") else None,
                "cls": int(float(r.get("cls_eff") or r.get("cls_raw") or -1)),
            }
    return ref


def load_ref_sim(path):
    """Simulink 参考（层①/③）：ref_G2_*.csv → tick → P_gen_ref_kW 等。"""
    ref = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            k = int(r["tick"]) if r.get("tick") else len(ref)
            ref[k] = {"P_gen_ref": float(r["P_gen_ref_kW"]),
                      "q50": float(r["q50_kwh"]) if r.get("q50_kwh") else None,
                      "cls": int(float(r["cls_eff"])) if r.get("cls_eff") else -1}
    return ref


def load_ref_infer(path_or_dir):
    """INFER_TEST 对拍参考：reference_int8_val200.csv（idx,q10/q50/q90_kwh）。"""
    p = path_or_dir
    if os.path.isdir(p):
        p = os.path.join(p, "reference_int8_val200.csv")
    ref = {}
    with open(p, newline="") as f:
        # 按文件行序键控（0..N-1）：idx 列是原始 tick，与 vectors_raw 行序无关
        # （T-507 实测：按 idx 键控 n_cmp≈0，行序配对后与板上值精确对齐）
        for i, r in enumerate(csv.DictReader(f)):
            ref[i] = {"q10": float(r["q10_kwh"]),
                      "q50": float(r["q50_kwh"]),
                      "q90": float(r["q90_kwh"])}
    return ref


def compare_tick(out_rows, ref_int8, ref_sim):
    """TICK 对拍统计（层② vs host-int8；层①/③ vs Simulink）。"""
    st = {"int8": {}, "sim": {}}
    if ref_int8:
        de, dk, cm, nc = [], [], 0, 0
        for r in out_rows:
            q = ref_int8.get(r["idx"])
            if q is None or "err" in r:
                continue
            nc += 1
            if q["e_hat"] is not None:
                de.append(abs(r["e_hat"] - q["e_hat"]))
            if q["K"] is not None:
                dk.append(abs(r["K"] - q["K"]))
            if q["cls"] >= 1 and r["cls"] == q["cls"]:
                cm += 1
        st["int8"] = {"n_cmp": nc,
                      "max_de_hat": max(de) if de else None,
                      "max_dK": max(dk) if dk else None,
                      "cls_match_rate": (cm / nc) if nc else None}
    if ref_sim:
        exact = within1 = nc = 0
        maxdp = 0.0
        for r in out_rows:
            s = ref_sim.get(r["idx"])
            if s is None or "err" in r:
                continue
            nc += 1
            dp = abs(r["P_gen_ref"] - s["P_gen_ref"])
            maxdp = max(maxdp, dp)
            if dp < 1e-6:
                exact += 1
            if dp <= GRID_STEP_KW + 1e-9:
                within1 += 1
        st["sim"] = {"n_cmp": nc, "exact_rate": (exact / nc) if nc else None,
                     "within_1grid_rate": (within1 / nc) if nc else None,
                     "max_dP_kW": maxdp}
    return st


def compare_infer(out_rows, ref):
    """INFER_TEST 对拍（FR-504 输入）：max|Δq| 与 q50 RMSE 比所需原始量。"""
    dq50 = []
    dq10 = []
    dq90 = []
    sd50 = []
    se_b = 0.0
    se_r = 0.0
    nc = 0
    ref_q50 = []
    for r in out_rows:
        q = ref.get(r["idx"])
        if q is None or "err" in r:
            continue
        nc += 1
        dq10.append(abs(r["q10"] - q["q10"]))
        dq50.append(abs(r["q50"] - q["q50"]))
        dq90.append(abs(r["q90"] - q["q90"]))
        sd50.append(r["q50"] - q["q50"])
        ref_q50.append(q["q50"])
    if not nc:
        return {"n_cmp": 0}
    mean_ref = sum(ref_q50) / nc
    # FR-504 硬门禁（design.md 2026-09-01 修订，用户批准）：|Δq50| 以输出 LSB 计
    # （1 LSB = 冻结 mlpq 输出 scale 0.015376422 × y_std 16.495129 = 0.2536 kWh）：
    # mean ≤1.5 且 p99 ≤6 且 max ≤8（LSB），|mean| ≤0.5（无偏置），RMSE 比 ≤1.1。
    # 原值 max|Δq50|≤0.25 为无推导拍定值（< 1 LSB，字面=遂位一致），已废弃。
    LSB_KWH = 0.253636
    d50_lsb = sorted(d / LSB_KWH for d in dq50)
    mean_lsb = sum(d50_lsb) / nc
    p99_lsb = d50_lsb[min(nc - 1, int(round(0.99 * nc)) - 1)]
    max_lsb = d50_lsb[-1]
    signed_mean_lsb = (sum(sd50) / nc) / LSB_KWH
    # RMSE 比：Δ 的 RMSE 相对参考量幅值的 RMSE（真值 RMSE 在 T-507 由 val 标签补全）
    rmse_delta = (sum(d ** 2 for d in dq50) / nc) ** 0.5
    rmse_ref = (sum(q ** 2 for q in ref_q50) / nc) ** 0.5
    rmse_ratio = rmse_delta / rmse_ref if rmse_ref else float("inf")
    return {"n_cmp": nc,
            "max_dq10": max(dq10), "max_dq50": max(dq50),
            "max_dq90": max(dq90),
            "mean_ref_q50": mean_ref,
            "gate_lsb": {"mean_abs": round(mean_lsb, 3), "p99": round(p99_lsb, 3),
                         "max": round(max_lsb, 3),
                         "signed_mean": round(signed_mean_lsb, 3)},
            "hard_gate_fr504": (mean_lsb <= 1.5 and p99_lsb <= 6.0
                                and max_lsb <= 8.0 and rmse_ratio <= 1.1
                                and abs(signed_mean_lsb) <= 0.5),
            "rmse_ratio": round(rmse_ratio, 4)}


def load_ref_wc(path_or_dir):
    """WC_TEST 对拍参考（positional 键控，同 load_ref_infer 修法）：
    cls 参考 = reference_int8_val200.csv 的 cls_raw 列（行序）；
    logits 参考 = reference_int8_val200_logits.csv（gen_reference_int8 模式 A
    B2 新增：idx,logit0..logit6）。返回 (ref_cls list, ref_logits list[list])。"""
    p = path_or_dir
    if os.path.isdir(p):
        p = os.path.join(p, "reference_int8_val200.csv")
    ref_cls = []
    with open(p, newline="") as f:
        # 按文件行序键控（0..N-1）：与 vectors_raw 行序一一对应（T-507 实测同款修法）
        for r in csv.DictReader(f):
            ref_cls.append(int(float(r["cls_raw"])))
    lp = os.path.join(os.path.dirname(os.path.abspath(p)),
                      "reference_int8_val200_logits.csv")
    ref_logits = []
    if os.path.exists(lp):
        with open(lp, newline="") as f:
            for r in csv.DictReader(f):
                ref_logits.append([float(r["logit%d" % j]) for j in range(7)])
    return ref_cls, ref_logits


def compare_wc(out_rows, ref_cls, ref_logits):
    """WC_TEST 对拍统计（T-507）：cls 一致率 + logits max|Δ|（vs 模式 A logits CSV）。"""
    nc = match = 0
    dmax = None
    for r in out_rows:
        if "err" in r:
            continue
        nc += 1
        if r.get("match") == 1:
            match += 1
        d = r.get("logits_max_abs_delta")
        if d is not None:
            dmax = d if dmax is None else max(dmax, d)
    return {"n_cmp": nc, "cls_match_rate": (match / nc) if nc else None,
            "cls_match": match, "max_logit_abs_delta": dmax}


def write_wc_outputs(out_rows, errs, st, out_prefix):
    """写 wc_compare.csv + .json（行：idx, cls_board, cls_ref, match,
    logits_max_abs_delta, wc_us）。返回 csv 路径。"""
    csv_p = out_prefix + ".csv"
    cols = ["idx", "cls_board", "cls_ref", "match",
            "logits_max_abs_delta", "wc_us", "err"]
    with open(csv_p, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for r in out_rows:
            row = {c: r.get(c, "") for c in cols}
            d = r.get("logits_max_abs_delta")
            if row["logits_max_abs_delta"] != "":
                row["logits_max_abs_delta"] = "%.6f" % row["logits_max_abs_delta"]
            w.writerow(row)
    js = {"rows": len(out_rows), "errors": errs, "compare": st,
          "out_csv": os.path.basename(csv_p)}
    with open(out_prefix + ".json", "w") as f:
        json.dump(js, f, ensure_ascii=False, indent=1)
    return csv_p


def run_wc_test(sess, vectors, ref_cls, ref_logits, rsp_ms=500,
                frame_gap_ms=300, retry=3):
    """CMD_WC_TEST 批量（T-507 workclf 类别一致率）：与 run_infer_test 同构。

    发送 CMD_WC_TEST [seq][302×f32] × N → 收 RSP_WC_TEST [seq][cls u8]
    [3B 0][logits 7×f32][wc_us u32]；seq 配对/重试语义同 run_infer_test。
    对拍：reference cls（positional 键控，同 infer 修法）→ match 逐行 + 一致率；
    logits max|Δ|（参考 = gen_reference_int8 模式 A 新增 logits CSV，位置对齐）。
    返回 (rows_out, errs)；rows_out[i] = {idx, seq, cls_board, cls_ref, match,
    logits_max_abs_delta, wc_us, logits}。
    """
    errs = {"timeout": 0, "bad_rsp": 0, "resend": 0, "stale_skip": 0}
    out = []
    for _ in range(5):  # 链路预同步（同 run_infer_test）
        sess.send(CMD_PING, b"")
        if sess.wait_reply(ACK, 400) is not None:
            break
        time.sleep(0.2)
    else:
        raise RuntimeError("PING pre-sync failed: 链路未就绪（无 ACK）")
    for i, vec in enumerate(vectors):
        if len(vec) != FEAT_N:
            raise ValueError("向量 %d 维度 %d != %d" % (i, len(vec), FEAT_N))
        feat = struct.pack("<%df" % FEAT_N, *vec)
        pay = None
        seq = None
        for a in range(max(1, retry)):
            if a:  # 重试：间隔后换新 seq 重发同向量
                time.sleep(frame_gap_ms / 1000.0)
                errs["resend"] += 1
            elif frame_gap_ms and i:
                time.sleep(frame_gap_ms / 1000.0)
            seq = sess.send(CMD_WC_TEST, feat)
            pay = sess.wait_reply(RSP_WC_TEST, rsp_ms, want_seq=seq)
            if pay is not None:
                break
        errs["stale_skip"] += sess.n_stale_skip
        sess.n_stale_skip = 0
        row = {"idx": i, "seq": seq}  # err 键仅失败行存在（与 infer 行约定一致）
        if pay is None:
            errs["timeout"] += 1
            row["err"] = "timeout"
            out.append(row)
            continue
        d = dec_rsp_wc(pay)
        if d is None:
            errs["bad_rsp"] += 1
            row["err"] = "bad_rsp"
            out.append(row)
            continue
        row.update(cls_board=d["cls"], wc_us=d["us_wc"], logits=d["logits"])
        if i < len(ref_cls):
            row["cls_ref"] = ref_cls[i]
            row["match"] = 1 if d["cls"] == ref_cls[i] else 0
        if ref_logits and i < len(ref_logits):
            row["logits_max_abs_delta"] = max(
                abs(a - b) for a, b in zip(d["logits"], ref_logits[i]))
        out.append(row)
    errs["stale_skip"] += sess.n_stale_skip
    return out, errs


def write_wc_outputs(out_rows, errs, st, out_prefix):
    """写 wc_compare.csv[.json]；返回 csv 路径。"""
    csv_p = out_prefix + ".csv"
    cols = ["idx", "cls_board", "cls_ref", "match",
            "logits_max_abs_delta", "wc_us", "err"]
    with open(csv_p, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for r in out_rows:
            row = {c: r.get(c, "") for c in cols}
            if r.get("logits_max_abs_delta") is not None:
                row["logits_max_abs_delta"] = "%.6f" % r["logits_max_abs_delta"]
            w.writerow(row)
    js = {"rows": len(out_rows), "errors": errs, "compare": st,
          "out_csv": os.path.basename(csv_p)}
    with open(out_prefix + ".json", "w") as f:
        json.dump(js, f, ensure_ascii=False, indent=1)
    return csv_p


def write_outputs(out_rows, errs, st, out_prefix, extra_cols=None):
    """写 <prefix>.csv + <prefix>.json。返回 csv 路径。"""
    csv_p = out_prefix + ".csv"
    cols = ["idx", "t", "P_load_kW", "P_gen_ref", "e_hat", "K", "cls",
            "us_infer", "us_ctrl", "flags", "err"] + list(extra_cols or [])
    keys = {"idx": "idx", "t": "t", "P_load_kW": "P_load_kW",
            "P_gen_ref": "P_gen_ref", "e_hat": "e_hat", "K": "K",
            "cls": "cls", "us_infer": "us_infer", "us_ctrl": "us_ctrl",
            "flags": "flags", "err": "err"}
    with open(csv_p, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for r in out_rows:
            row = {c: r.get(k, "") for c, k in keys.items()}
            for c in (extra_cols or []):
                row[c] = r.get(c, "")
            if "err" not in r:
                row["err"] = r.get("err", "")
            w.writerow(row)
    js = {"rows": len(out_rows), "errors": errs, "compare": st,
          "out_csv": os.path.basename(csv_p)}
    with open(out_prefix + ".json", "w") as f:
        json.dump(js, f, ensure_ascii=False, indent=1)
    return csv_p


MOCK_QSCALE = 0.1  # mock 固件：P_gen_ref = P_load_kW * 0.1（确定性，可断言）


def mock_firmware_worker(fd, stop_evt):
    """--selftest：pty 侧 mock EMS01 固件线程（B2 语义同步：ACK/RSP_TICK/
    RSP_INFER seq 回显/RSP_WC seq+cls+logits+us）。"""
    buf = bytearray()
    tick_count = 0
    while not stop_evt.is_set():
        try:
            chunk = os.read(fd, 512)
        except OSError:
            time.sleep(0.005)
            continue
        if not chunk:
            time.sleep(0.005)
            continue
        buf.extend(chunk)
        while True:
            i = buf.find(b"\xAA\x55")
            if i < 0:
                buf.clear()
                break
            if i:
                del buf[:i]
            if len(buf) < 5:
                break
            ttype = buf[2]
            ln = buf[3] | (buf[4] << 8)
            total = 5 + ln + 2
            if len(buf) < total:
                break
            pay = bytes(buf[5:5 + ln])
            del buf[:total]
            if ttype == CMD_RESET:
                os.write(fd, build_frame(ACK, bytes([CMD_RESET])))
            elif ttype == CMD_PING:
                os.write(fd, build_frame(ACK, bytes([CMD_PING])))
            elif ttype == CMD_TICK and ln == 32:
                idx = struct.unpack_from("<I", pay, 0)[0]
                p_load = struct.unpack_from("<f", pay, 8)[0]
                q = p_load * MOCK_QSCALE
                rp = enc_rsp_tick(idx, q, q, q, 3, 1000 + idx, 2000 + idx, 0)
                os.write(fd, build_frame(RSP_TICK, rp))
                tick_count += 1
            elif ttype == CMD_INFER_TEST and ln == CMD_TEST_LEN:
                # B1 语义：载荷头 seq 原样回显（dec 由冻结源 payload_seq 承担）
                seq = payload_seq(pay)
                f0 = struct.unpack_from("<f", pay, 4)[0]
                q = f0 * MOCK_QSCALE
                os.write(fd, build_frame(RSP_INFER_TEST,
                                         enc_rsp_infer(seq, q * 0.9, q, q * 1.1,
                                                       1000 + (seq % 997))))
            elif ttype == CMD_WC_TEST and ln == CMD_TEST_LEN:
                seq = payload_seq(pay)
                cls = 1 + (seq % 7)
                logits = [((seq + j) % 7) * 0.25 - 0.75 for j in range(7)]
                os.write(fd, build_frame(RSP_WC_TEST,
                                         enc_rsp_wc(seq, cls, logits,
                                                    2000 + (seq % 891))))
            elif ttype in (CMD_INFER_TEST, CMD_WC_TEST):
                os.write(fd, build_frame(NACK, bytes([ttype, MOCK_ERR_LEN])))
            else:
                os.write(fd, build_frame(NACK, bytes([ERR_CRC])))
    return tick_count


def selftest_main():
    """--selftest：pty + mock 固件，离线验证 注入→RSP→对拍 全链。"""
    import pty
    import tty
    sl_fd, ma_fd = pty.openpty()
    # pty 默认 cooked 模式：ICANON 按行缓冲、OPOST/ONLCR 篡改 0x0A，
    # 二进制帧必须 RAW（macOS/Linux 同样）。
    tty.setraw(sl_fd)
    stop = threading.Event()
    th = threading.Thread(target=mock_firmware_worker, args=(sl_fd, stop),
                          daemon=True)
    th.start()
def selftest_main():
    """--selftest：pty + mock 固件，离线验证 注入→RSP→对拍 全链（B2 含 infer
    seq 回显 + wc 批量链；mock 与固件 B1 语义对齐：seq 原样回显）。"""
    import pty
    import tty
    sl_fd, ma_fd = pty.openpty()
    # pty 默认 cooked 模式：ICANON 按行缓冲、OPOST/ONLCR 篡改 0x0A，
    # 二进制帧必须 RAW（macOS/Linux 同样）。
    tty.setraw(sl_fd)
    stop = threading.Event()
    th = threading.Thread(target=mock_firmware_worker, args=(sl_fd, stop),
                          daemon=True)
    th.start()
    rows = [{"tick": i, "t": float(i), "P_load_kW": 100.0 + 10.0 * i,
             "SOC": 0.6, "v_cmd_mps": 1.0, "v_ship_mps": 1.0,
             "accel_mps2": 0.0, "x_ship_m": 100.0 * i} for i in range(10)]
    sess = SerialSession(io=FdIO(ma_fd), timeout_ms=1000)
    out, errs = run_inject(sess, rows, rsp_ms=500)
    # B2：infer 批量 seq 回显链（mock seq 从 0 起，与 sess.tx_seq 计数器对齐）
    ivec = [[100.0 + 10.0 * i] + [0.0] * (FEAT_N - 1) for i in range(6)]
    iout, ierrs = run_infer_test(sess, ivec, rsp_ms=500, frame_gap_ms=0, retry=3)
    # B2：wc 批量链（mock cls = 1+seq%7；infer 已用 seq 0..5，wc 首 seq=6，
    # 参考按同一确定性规则以 seq 对齐，验证的是回显链而非位置巧合）
    wvec = [[200.0 + 5.0 * i] + [0.0] * (FEAT_N - 1) for i in range(6)]
    wc_seq0 = len(ivec)  # infer 批量后 seq 计数器起点（=6）
    ref_cls = [1 + ((wc_seq0 + i) % 7) for i in range(6)]
    ref_log = [[((wc_seq0 + i + j) % 7) * 0.25 - 0.75 for j in range(7)]
               for i in range(6)]
    wout, werrs = run_wc_test(sess, wvec, ref_cls, ref_log,
                              rsp_ms=500, frame_gap_ms=0)
    sess.close()
    stop.set()
    exp = [r["P_load_kW"] * MOCK_QSCALE for r in rows]
    bad = [i for i, r in enumerate(out) if "err" in r]
    maxdq = max((abs(out[i]["P_gen_ref"] - exp[i]) for i in range(len(out))),
                default=9e9)
    # infer 链断言：seq 回显（seq=i）+ q50 = f0*MOCK_QSCALE（mock 规则）
    exp_q = [v[0] * MOCK_QSCALE for v in ivec]
    bad_i = [r["idx"] for r in iout if "err" in r]
    seq_i_ok = all(r.get("seq") == i for i, r in enumerate(iout)
                   if "err" not in r)
    dq = max((abs(r["q50"] - exp_q[i]) for i, r in enumerate(iout)
              if "err" not in r), default=9e9)
    # wc 链断言：cls 一致 + logits 零偏差 + seq 回显（续计数）+ us 字段在位
    wbad = [r["idx"] for r in wout if "err" in r]
    w_match = sum(1 for r in wout if r.get("match") == 1)
    wseq_ok = all(r.get("seq") == wc_seq0 + i for i, r in enumerate(wout))
    max_dlogit = max((r["logits_max_abs_delta"] for r in wout
                      if r.get("logits_max_abs_delta") is not None), default=9e9)
    ok = (len(out) == 10 and not bad and maxdq < 1e-4
          and len(iout) == 6 and not bad_i and dq < 1e-4 and seq_i_ok
          and len(wout) == 6 and not wbad and w_match == 6
          and max_dlogit < 1e-6 and wseq_ok)
    print("selftest: ticks=%d bad=%d P_gen_ref(P=100..190)=[%.2f..%.2f] us=[%s]"
          % (len(out), len(bad), min(out[0]["P_gen_ref"], out[9]["P_gen_ref"]),
             max(out[0]["P_gen_ref"], out[-1]["P_gen_ref"]),
             ",".join(str(r.get("us_infer")) for r in out[:3])))
    print("selftest: infer n=%d bad=%d seq_echo=%s maxdq50=%.2g | wc n=%d "
          "match=%d/6 maxdlogit=%.2g us=[%s]"
          % (len(iout), len(bad_i), seq_i_ok, dq,
             len(wout), w_match, max_dlogit,
             ",".join(str(r.get("wc_us")) for r in wout[:3])))
    if ok:
        print("EMS_INJECT SELFTEST PASS")
        return 0
    print("EMS_INJECT SELFTEST FAIL: bad=%s bad_infer=%s bad_wc=%s"
          % (bad, bad_i, wbad))
    return 1


def resolve_out_prefix(a):
    """--out DIR（design §API：report 目录）优先；--out-prefix 兼容。产物名按
    模式：fw_compare（TICK）/ infer_compare（INFER）/ wc_compare（WC）。"""
    if a.out:
        os.makedirs(a.out, exist_ok=True)
        if a.wc_test:
            name = "wc_compare"
        elif a.infer_test:
            name = "infer_compare"
        else:
            name = "fw_compare"
        return os.path.join(a.out, name)
    return a.out_prefix


def main():
    ap = argparse.ArgumentParser(
        description="T-506: EMS01 串口注入/对拍 CLI（协议见 ems_proto.h / "
                    "design §UART 帧表）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
模式：
  TICK 注入（默认）   --port DEV --trace ref_G2_*.csv [--ref-int8 模式B CSV]
                      [--ref Simulink ref CSV] --out REPORT_DIR
  INFER_TEST 批量     --port DEV --infer-test 模式A目录|vectors_raw.csv
                      [--out REPORT_DIR]   （FR-504 M1，×200 向量；载荷
                      [seq u32][302×f32]，seq 自动计数，RSP 按 seq 配对）
  WC_TEST 批量        --port DEV --wc-test 模式A目录|vectors_raw.csv
                      [--out REPORT_DIR]   （T-507，×200；对拍 cls_raw +
                      reference_int8_val200_logits.csv → wc_compare.csv/json）
  离线自测            --selftest（pty + mock 固件，无需硬件与串口）

轨迹 CSV 列（7 信号，语义/单位固定 design §UART）：
  tick,t,P_load_kW,SOC,v_cmd_mps,v_ship_mps,accel_mps2,x_ship_m
产物：<out>/fw_compare.csv|.json（TICK）/ infer_compare.csv|.json（INFER）/
  wc_compare.csv|.json（WC，列 idx,cls_board,cls_ref,match,
  logits_max_abs_delta,wc_us）
  TICK csv 列：idx,t,P_load_kW,P_gen_ref,e_hat,K,cls,us_infer,us_ctrl,flags,err
  汇总 json：errors{timeout,resend,stale_skip,bad_rsp,idx_mismatch} +
  compare.int8（层② max_de_hat/max_dK/cls_match_rate）+ compare.sim（层①/③
  exact_rate/within_1grid_rate/max_dP_kW）；INFER_TEST json 含 max_dq50 与
  hard_gate_max_dq50_le_0.25（FR-504 硬门禁 max|Δq50|≤0.25 kWh 输入）；
  WC_TEST json 含 cls_match_rate 与 max_logit_abs_delta（T-507）。
""")
    ap.add_argument("--port", help="串口设备（如 /dev/ttyACM0；--selftest 时可省）")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--trace", metavar="CSV",
                    help="CMD_TICK 轨迹（7 信号全列，见上）")
    ap.add_argument("--ref", metavar="CSV",
                    help="Simulink 参考（ref_G2_*.csv，层①/③ P_gen_ref 对拍）")
    ap.add_argument("--ref-int8", metavar="REF",
                    help="Host int8 参考（层②；模式 A 目录或模式 B CSV）")
    ap.add_argument("--infer-test", metavar="VECTORS",
                    help="INFER_TEST 批量：模式 A 输出目录（含 vectors_raw.csv +"
                         " reference_int8_val200.csv）或 302 列 raw CSV")
    ap.add_argument("--wc-test", metavar="VECTORS",
                    help="WC_TEST 批量（T-507 类别一致率）：模式 A 输出目录或 "
                         "302 列 raw CSV；对拍 reference cls/logits → "
                         "wc_compare.csv/json")
    ap.add_argument("--retry", type=int, default=3,
                    help="infer/wc 单向量最大尝试次数（默认 3；超时→间隔 "
                         "--frame-gap-ms 换新 seq 重发同向量，全失败才记 timeout）")
    ap.add_argument("--rsp-timeout-ms", type=int, default=500,
                    help="等 RSP 超时 [ms]（默认 500 = design §UART 注入会话口径；\n"
                         "超时记 TIMEOUT 事件继续）")
    ap.add_argument("--frame-gap-ms", type=int, default=300,
                    help="infer/wc 帧间发送间隔 [ms]（默认 300；T-507 实测 CH340\n"
                         "换向后立即发帧会损坏首段字节，0 = 不等）")
    ap.add_argument("--limit", type=int, default=0, help="最多注入 N 个 tick")
    ap.add_argument("--out", metavar="DIR",
                    help="报告目录（design §API；内产 fw_compare/infer_compare/"
                         "wc_compare .csv/.json）")
    ap.add_argument("--out-prefix",
                    default=os.path.join(HERE, "fw_compare"),
                    help=argparse.SUPPRESS if False else
                         "输出前缀（--out 未给时的兼容形式）")
    ap.add_argument("--selftest", action="store_true",
                    help="无硬件离线自测（pty + mock 固件）")
    a = ap.parse_args()
    if a.selftest:
        return selftest_main()
    if not a.port:
        ap.print_help()
        return 1
    out_prefix = resolve_out_prefix(a)
    sess = SerialSession(a.port, a.baud, a.rsp_timeout_ms)
    try:
        if a.wc_test:
            p = a.wc_test
            vec_csv = os.path.join(p, "vectors_raw.csv") if os.path.isdir(p) else p
            vectors = load_vectors_raw(vec_csv)
            if a.limit:
                vectors = vectors[:a.limit]
            ref_dir = p if os.path.isdir(p) else os.path.dirname(os.path.abspath(p))
            ref_cls, ref_logits = load_ref_wc(ref_dir)
            if not ref_cls:
                print("WC 对拍参考缺失：%s/reference_int8_val200.csv" % ref_dir)
                return 1
            out, errs = run_wc_test(sess, vectors, ref_cls, ref_logits,
                                    rsp_ms=a.rsp_timeout_ms,
                                    frame_gap_ms=a.frame_gap_ms, retry=a.retry)
            st = compare_wc(out, ref_cls, ref_logits)
            cp = write_wc_outputs(out, errs, st, out_prefix)
        elif a.infer_test:
            p = a.infer_test
            vec_csv = os.path.join(p, "vectors_raw.csv") if os.path.isdir(p) else p
            vectors = load_vectors_raw(vec_csv)
            if a.limit:
                vectors = vectors[:a.limit]
            out, errs = run_infer_test(sess, vectors, a.rsp_timeout_ms,
                                       a.frame_gap_ms, retry=a.retry)
            ref = load_ref_infer(p if os.path.isdir(p) else
                                 os.path.dirname(os.path.abspath(p)))
            st = compare_infer(out, ref)
            cp = write_outputs(out, errs, st, out_prefix,
                               extra_cols=["q10", "q50", "q90"])
        else:
            rows = load_trace(a.trace, a.limit) if a.trace else []
            if not rows:
                print("--trace CSV 缺失或无行")
                return 1
            out, errs = run_inject(sess, rows, a.rsp_timeout_ms)
            ref_int8 = load_ref_int8(a.ref_int8) if a.ref_int8 else {}
            ref_sim = load_ref_sim(a.ref) if a.ref else {}
            st = compare_tick(out, ref_int8, ref_sim)
            cp = write_outputs(out, errs, st, out_prefix)
    finally:
        sess.close()
    print("injected=%d errors=%s cmp=%s" % (len(out), errs, st))
    print("wrote %s[.json]" % cp)
    bad = errs.get("timeout", 0) + errs.get("idx_mismatch", 0) \
        + errs.get("bad_rsp", 0)
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())

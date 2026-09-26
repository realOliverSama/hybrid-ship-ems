#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3a2_check.py — EMS04-S3-A2 时序语义微测断言机（capture 离线核验器）。

输入：
  --capture   板侧响应字节流（fp_replay_client.py replay 的 capture 落盘：
              响应帧原始字节按到达顺序拼接，中间可能夹带被重同步跳过的垃圾字节）
  --synth-dir S3-A2 生成器产物目录（script_s3a2.bin + asserts.json）

断言（A1..A8，机械执行，全部字节级）：
  A1 会话帧    ：VOYAGE_INIT / KF_CHUNK×n / TICK2×301 逐一 ACK 且 echo 对应；
                 capture 帧全部被配对消费（无多余帧）、零垃圾字节、零 CRC 错。
  A2 负路径 1  ：QUERY t0=299  → NACK(cmd=0x23, err=0x45)
  A3 负路径 2  ：QUERY t0=300.5 → NACK(cmd=0x23, err=0x43)
  A4           ：QUERY t0=300  → RSP_FP_QUERY(0x24, 76 B)，t0 回显 f64 位级 == 300.0。
  A5           ：DUMP  t0=300  → RSP_FP_DUMP(0x26, 13932 B)，t0 回显 f64 位级。
  A6 窗右端点=t0（核心）：RSP_FP_DUMP hist 13200 B = (300,11) f32 LE C 序 t-major，
                 取 P_load 列：
                   6a 逐行位级复算——按契约 §6.1/ems04_h1win.construct 冻结语义：
                     row j 的 P_load = f32( (P(t0-300+j) + P(t0-299+j)) * 0.5 )，
                     P 为脚本 TICK2 帧内原始 f64 值（不硬编码 sentinel，全由脚本导出）；
                   6b 排序——row299 最大，row299 > row298 > max(row0..297)，
                     ramp 段（row0..297）单调非降；
                   6c host 归一化期望——gen_ref_split_v2.transform_h1 单点复算
                     （f64 运算 → cast f32，RNE）：norm(row299 期望) > norm(row298 期望)
                     > norm(row297 期望)，且对 capture 实测值上溯 f64 复算结果位级一致。
  A7 kf 行签名 ：RSP_FP_DUMP kf 720 B = (30,6) f32 LE；t0=300 → (300-300)/10 = row0，
                 row0 全 180 个 f32 位级 == 10.0；且与脚本 KF_CHUNK 重组表中
                 issue_time_s 位级 == 300.0 行的后 720 B 位级一致。
  A8 发布一致性：QUERY RSP 内 hist_sha256/kf_sha256 == DUMP 载荷对应段 sha256。

字节序/线格式假设（全部标注，权威 = PROTOCOL_FP.md 冻结版）：
  * 帧容器 [AA 55][TYPE u8][LEN u16 LE][PAYLOAD LEN B][CRC16 u16 LE]，
    CRC16-CCITT-FALSE（poly 0x1021, init 0xFFFF, 不反射, 无输出异或），
    作用域 TYPE..PAYLOAD；垃圾字节按 fp_replay_client.Link 同款逐字节重同步跳过。
  * 多字节整数一律 LE；f32/f64 IEEE-754 LE；sha256 为原始 32 B 摘要。
  * ACK=0x7F payload [cmd]；NACK=0x7E payload [cmd, err]。
  * RSP_FP_QUERY(0x24) payload 76 B：[f64 t0][u8 status][u8 rsv][u16 rsv]
    [hist sha256 32][kf sha256 32]。
  * RSP_FP_DUMP(0x26) payload 13932 B：[f64 t0][u8 status][u8 rsv][u16 rsv]
    [hist 13200 B][kf 720 B]。
  * B2V1 脚本束："B2V1" + [u32 ver][u32 n_ops][u32 sid_len] + sid + ops 流；
    OP_FEED=0x01 [u32 len][整帧]、OP_EXPECT=0x02 [kind u8][载荷]、OP_NOTE=0x03。

实现前提勘误（以冻结代码为准，任务文字两处假设已按权威源修正）：
  1) hist P_load 列 = index 0（非任务文字所写 index 1）。权威：
     b/ems04_h1win.c `ems04_h1_col_names`（REQUIRED_H1 冻结序）与
     model/.../dataset_v3_signal_registry.json INPUT_H1 序完全一致，
     第 0 列即 P_load_total_kW；index 1 是 v_ship_mps。
     （CH 表的 index 1 是 CMD_TICK2 载荷字段序，非 DUMP hist 列序。）
  2) DUMP hist 为**原始物理域**构造出口字节（契约：主判据域 = 原始 f32 张量字节），
     P_load 为 time_mean 中点行，故 row299 = f32((7777+8888)*0.5) = f32(8332.5)、
     row298 = f32((1298+7777)*0.5) = f32(4537.5)，而非 8888/7777 的直接归一化。
     A6 的位级复算按冻结语义实现，排序断言（max 在末行）与生成器 asserts.json 叙事一致。
  3) A2 期望 err=0x45 按任务/生成器规格实现；注意冻结固件（B1 b/ems04_fp_svc.c 与
     C 阶段 Appli/App/ems04_fp_svc.c 同源）的 QUERY 检查序为
     状态→整数→窗满→范围[300,tick_last]→锚点齐备，窗满后 QUERY(299) 命中范围门
     返回 0x43——该规格与固件实现的冲突不在本核验器内裁决（升级主 Agent）。

自测辅助（仅写调用方指定路径，不在仓库内产生文件）：
  --emit-sim-capture PATH   按 B2V1 帧序离线构造「模拟 capture」（期望响应字节流：
                            ACK/NACK/RSP 按协议拼装；hist/kf 按 §6.1 冻结构造语义
                            host 侧复算填充；负路径按脚本期望应答）。
  --sim-break-a6            与上项联用：构造时把 hist 末行 P_load 篡改为
                            f32(transform_h1(1298.0))（锁存域注入故障），
                            用于验证 A6 失败路径（first_failure=A6）。

用法（仓库根 .venv_tf python，或 experiments 层 venv）：
  python3 s3a2_check.py --synth-dir <out/S3A2_SYNTH> --dry-run
  python3 s3a2_check.py --synth-dir <out/S3A2_SYNTH> --emit-sim-capture /tmp/sim.bin
  python3 s3a2_check.py --synth-dir <out/S3A2_SYNTH> --capture <cap.bin> --out <r.json>

退出码：0 = overall pass / dry-run 通过；1 = overall fail；2 = 用法/环境/输入错误。
纪律：路径一律 __file__ 相对推导；PYTHONDONTWRITEBYTECODE=1；不写仓库内文件（--out 指定路径除外）。
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import os
import struct
import sys

sys.dont_write_bytecode = True

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- 线格式常量
SYNC0, SYNC1 = 0xAA, 0x55
HDR_LEN, CRC_LEN = 5, 2                      # AA 55 TYPE LEN(2) | CRC(2)

T_ACK, T_NACK = 0x7F, 0x7E
T_VI, T_KFC, T_TICK2 = 0x20, 0x21, 0x22      # H→F
T_QUERY, T_RSP_QUERY = 0x23, 0x24
T_DUMP, T_RSP_DUMP = 0x25, 0x26

TYPE_NAMES = {
    T_VI: "VOYAGE_INIT", T_KFC: "KF_CHUNK", T_TICK2: "TICK2",
    T_QUERY: "FP_QUERY", T_RSP_QUERY: "RSP_FP_QUERY",
    T_DUMP: "FP_DUMP", T_RSP_DUMP: "RSP_FP_DUMP",
    T_ACK: "ACK", T_NACK: "NACK",
}

OP_FEED, OP_EXPECT, OP_NOTE = 0x01, 0x02, 0x03
EK_ACK, EK_NACK, EK_TYPE = 0x01, 0x02, 0x03
EK_QUERY, EK_DUMP, EK_QSHA = 0x04, 0x05, 0x06
EK_COUNTER, EK_ERRCOUNT, EK_LATCH, EK_STATE, EK_SINK = 0x07, 0x08, 0x09, 0x0A, 0x0B
# EXPECT kind 载荷长度（kind 字节之后；与 fp_replay_client 解析表同源）
EXPECT_ARG_LEN = {EK_ACK: 1, EK_NACK: 1, EK_TYPE: 1, EK_QUERY: 8, EK_DUMP: 8 + 4 + 4,
                  EK_QSHA: 128, EK_COUNTER: 9, EK_ERRCOUNT: 5, EK_LATCH: 1,
                  EK_STATE: 1, EK_SINK: 40}
RESPONSE_EXPECT_KINDS = (EK_ACK, EK_NACK, EK_TYPE, EK_QUERY, EK_DUMP)

ERR_STATE, ERR_POLICY, ERR_ORDER = 0x40, 0x41, 0x42
ERR_TICK_T, ERR_NONFINITE, ERR_NOT_READY = 0x43, 0x44, 0x45
ERR_KF_HASH, ERR_KF_ROW, ERR_BOUNDS = 0x46, 0x47, 0x48

# hist / kf 几何（PROTOCOL_FP §5）
HIST_ROWS, HIST_COLS = 300, 11
HIST_BYTES = HIST_ROWS * HIST_COLS * 4       # 13200
KF_R, KF_C = 30, 6
KF_SEG_BYTES = KF_R * KF_C * 4               # 720
KF_ROW_PAYLOAD_BYTES = 8 + KF_SEG_BYTES      # f64 issue_time_s + 720
RSP_QUERY_LEN = 8 + 1 + 1 + 2 + 32 + 32      # 76
RSP_DUMP_LEN = 8 + 1 + 1 + 2 + HIST_BYTES + KF_SEG_BYTES  # 13932

# REQUIRED_H1 冻结列序（b/ems04_h1win.c ems04_h1_col_names = registry INPUT_H1 序）
REQUIRED_H1_COLS = ["P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
                    "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
                    "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct"]
P_LOAD_COL = REQUIRED_H1_COLS.index("P_load_total_kW")   # = 0（勘误见模块 docstring）

# A2/A3 期望（任务/生成器规格；A2 与冻结固件 gate 序的冲突见 docstring 勘误 3）
NEG1_T0, NEG1_ERR = 299.0, ERR_NOT_READY     # 0x45
NEG2_T0, NEG2_ERR = 300.5, ERR_TICK_T        # 0x43

QUERY_KIND_NEG1, QUERY_KIND_NEG2, QUERY_KIND_POS = "neg1", "neg2", "pos"


def crc16_ccitt(data: bytes) -> int:
    """CRC16-CCITT-FALSE：poly 0x1021, init 0xFFFF, 不反射, 无输出异或。"""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def enc_frame(ftype: int, payload: bytes) -> bytes:
    """按线格式拼整帧（模拟 capture 构造用）。字节序全 LE。"""
    body = bytes([ftype]) + struct.pack("<H", len(payload)) + payload
    return bytes([SYNC0, SYNC1]) + body + struct.pack("<H", crc16_ccitt(body))


def f32_bytes(x: float) -> bytes:
    """f64 → f32（IEEE RNE，与 C 出口 cast 一致）→ 4 B LE。"""
    return struct.pack("<f", x)


def to_scalar(v):
    """numpy 标量/形状 (1,) 数组 → python float（transform_h1 返回数组形态兼容）。"""
    item = getattr(v, "item", None)
    return float(item()) if item is not None else float(v)


def sha256(b: bytes) -> bytes:
    return hashlib.sha256(b).digest()


# ---------------------------------------------------------------- 路径
def find_repo_root(start_dir: str) -> str:
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("ERROR: 无法定位仓库根（自 %s 向上）" % start_dir)
        d = parent


REPO_ROOT = find_repo_root(SCRIPT_DIR)
GEN_REF_PATH = os.path.join(REPO_ROOT, "experiments", "softhil_ems03_2026-09-25",
                            "ems04_s3", "tools", "gen_ref_split_v2.py")


def load_gen_ref():
    """延迟导入 gen_ref_split_v2（transform_h1 / load_norm_artifacts）。"""
    if not os.path.isfile(GEN_REF_PATH):
        raise SystemExit("ERROR: 缺 %s（相对仓库根推导）" % GEN_REF_PATH)
    spec = importlib.util.spec_from_file_location("gen_ref_split_v2", GEN_REF_PATH)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)   # 模块内部 import numpy（需 .venv_tf）
    return mod


# ---------------------------------------------------------------- 脚本解析
class Script:
    """B2V1 脚本束解析：FEED 帧序 + 每帧随附的期望响应条数（配对计划依据）。"""

    def __init__(self, path: str):
        self.path = path
        blob = open(path, "rb").read()
        self.sha256 = hashlib.sha256(blob).hexdigest()
        if blob[:4] != b"B2V1":
            raise SystemExit("ERROR: %s 非 B2V1 脚本束" % path)
        _, self.n_ops, sid_len = struct.unpack_from("<III", blob, 4)
        self.sid = blob[16:16 + sid_len].decode("ascii", "replace")
        self.units = []          # [(frame_bytes, n_expected_rsp)]
        self.frames = []         # 仅帧字节（顺序）
        self.note_count = 0
        self.framing_bad = 0
        self.crc_bad = 0
        pos = 16 + sid_len
        cur = None
        for _ in range(self.n_ops):
            if pos >= len(blob):
                self.framing_bad += 1
                break
            op = blob[pos]
            pos += 1
            if op == OP_NOTE:
                ln = struct.unpack_from("<I", blob, pos)[0]
                pos += 4 + ln
                self.note_count += 1
            elif op == OP_FEED:
                ln = struct.unpack_from("<I", blob, pos)[0]
                pos += 4
                frame = blob[pos:pos + ln]
                pos += ln
                if len(frame) < HDR_LEN + CRC_LEN or frame[0] != SYNC0 or frame[1] != SYNC1:
                    self.framing_bad += 1
                    cur = (frame, 1)
                    self.units.append(cur)
                    self.frames.append(frame)
                    continue
                flen = struct.unpack_from("<H", frame, 3)[0]
                if flen != len(frame) - HDR_LEN - CRC_LEN:
                    self.framing_bad += 1
                elif crc16_ccitt(frame[2:HDR_LEN + flen]) != struct.unpack_from(
                        "<H", frame, HDR_LEN + flen)[0]:
                    self.crc_bad += 1
                cur = (frame, 0)     # 期望响应数由后续 EXPECT 累加；无 EXPECT 则收尾默认 1
                self.units.append(cur)
                self.frames.append(frame)
            elif op == OP_EXPECT:
                kind = blob[pos]
                pos += 1
                if kind not in EXPECT_ARG_LEN:
                    raise SystemExit("ERROR: %s 未知 EXPECT kind 0x%02X" % (path, kind))
                pos += EXPECT_ARG_LEN[kind]
                if kind in RESPONSE_EXPECT_KINDS and cur is not None:
                    cur = (cur[0], cur[1] + 1)
                    self.units[-1] = cur
            else:
                raise SystemExit("ERROR: %s 未知 op 0x%02X @%d" % (path, op, pos - 1))
        if pos != len(blob):
            self.framing_bad += 1
        # 无 EXPECT 的 FEED：命令恒有一响应，默认配对 1 条
        self.units = [(f, n if n > 0 else 1) for f, n in self.units]


# ---------------------------------------------------------------- capture 解析
class Capture:
    """板侧响应字节流解析（fp_replay_client.Link 同款重同步语义）。"""

    def __init__(self, path: str):
        blob = open(path, "rb").read()
        self.path = path
        self.frames = []         # [(type, payload, crc_ok)]
        self.junk_bytes = 0      # 非帧字节（重同步跳过）
        self.crc_bad = 0
        self.partial_tail = 0    # 末尾不完整帧字节
        off = 0
        n = len(blob)
        while off < n:
            if blob[off] != SYNC0 or blob[off + 1] != SYNC1:
                self.junk_bytes += 1
                off += 1
                continue
            if off + HDR_LEN > n:
                self.partial_tail = n - off
                break
            ln = struct.unpack_from("<H", blob, off + 3)[0]
            total = HDR_LEN + ln + CRC_LEN
            if off + total > n:
                self.partial_tail = n - off
                break
            body = blob[off + 2:off + HDR_LEN + ln]
            crx = struct.unpack_from("<H", blob, off + HDR_LEN + ln)[0]
            ok = crc16_ccitt(body) == crx
            if not ok:
                self.crc_bad += 1
            self.frames.append((body[0], body[3:], ok))
            off += total


# ---------------------------------------------------------------- 会话计划
class SessionPlan:
    """从脚本 FEED 帧导出的会话结构（不硬编码任何 sentinel 值）。"""

    def __init__(self, script: Script):
        self.script = script
        self.ticks = {}          # tick_idx -> (t_s, P_load, ...11×f64)
        self.kf_rows = []        # 重组后的 728 B 行 payload（按行序）
        self.water_depth = None
        self.ctrl_mode = None
        self.kf_n_rows = None
        self.queries = []        # [(unit_idx, t0 f64)]
        self.dumps = []          # [(unit_idx, t0 f64)]
        self.counts = {T_VI: 0, T_KFC: 0, T_TICK2: 0}
        self.chunks = {}         # chunk_idx -> payload bytes
        self.chunk_total = None

        for idx, (frame, _n) in enumerate(script.units):
            if len(frame) < HDR_LEN + CRC_LEN or frame[0] != SYNC0 or frame[1] != SYNC1:
                continue
            ftype = frame[2]
            ln = struct.unpack_from("<H", frame, 3)[0]
            pay = frame[HDR_LEN:HDR_LEN + ln]
            if ftype == T_VI:
                self.counts[T_VI] += 1
                self.water_depth = struct.unpack_from("<d", pay, 40)[0]
                self.ctrl_mode = float(pay[48])
                _tf, _tl, _nt = struct.unpack_from("<III", pay, 52)
                self.kf_n_rows = struct.unpack_from("<I", pay, 64)[0]
            elif ftype == T_KFC:
                self.counts[T_KFC] += 1
                ci, total, clen = struct.unpack_from("<IIH", pay, 0)
                self.chunk_total = total
                self.chunks[ci] = pay[10:10 + clen]
            elif ftype == T_TICK2:
                self.counts[T_TICK2] += 1
                t = struct.unpack_from("<I", pay, 0)[0]
                self.ticks[t] = struct.unpack_from("<11d", pay, 4)
            elif ftype == T_QUERY:
                self.queries.append((idx, struct.unpack_from("<d", pay, 0)[0]))
            elif ftype == T_DUMP:
                self.dumps.append((idx, struct.unpack_from("<d", pay, 0)[0]))
        # kf 表重组（chunk 乱序由连续性门检查）；行数 = 重组总字节 // 728
        if self.chunks:
            joined = b"".join(self.chunks[i] for i in range(len(self.chunks)))
            n_rows = len(joined) // KF_ROW_PAYLOAD_BYTES
            self.kf_rows = [joined[k * KF_ROW_PAYLOAD_BYTES:(k + 1) * KF_ROW_PAYLOAD_BYTES]
                            for k in range(n_rows)]

    def query_kind(self, t0: float) -> str:
        """按 t0 位级分类 QUERY：299.0 → neg1；300.5 → neg2；== 300.0 → pos。"""
        if struct.pack("<d", t0) == struct.pack("<d", NEG1_T0):
            return QUERY_KIND_NEG1
        if struct.pack("<d", t0) == struct.pack("<d", NEG2_T0):
            return QUERY_KIND_NEG2
        if struct.pack("<d", t0) == struct.pack("<d", float(self.max_tick())):
            return QUERY_KIND_POS
        return "unknown"

    def max_tick(self) -> int:
        return max(self.ticks)


# ---------------------------------------------------------------- 配对
def pair_responses(script: Script, cap: Capture):
    """FEED 帧序 × capture 响应帧序严格配对。返回 per-unit 响应列表 + 消费数。"""
    paired = []
    cursor = 0
    for frame, n_exp in script.units:
        got = []
        for _ in range(n_exp):
            if cursor < len(cap.frames):
                got.append(cap.frames[cursor])
                cursor += 1
        paired.append(got)
    return paired, cursor


def frame_fields(frame: bytes):
    """整帧 → (type, payload)；线格式非法时返回 (None, b'')。"""
    if len(frame) < HDR_LEN + CRC_LEN or frame[0] != SYNC0 or frame[1] != SYNC1:
        return None, b""
    ln = struct.unpack_from("<H", frame, 3)[0]
    if ln != len(frame) - HDR_LEN - CRC_LEN:
        return None, b""
    return frame[2], frame[HDR_LEN:HDR_LEN + ln]


# ---------------------------------------------------------------- A6 复算
def expected_pload_column(plan: SessionPlan, t0: int):
    """契约 §6.1 冻结构造：P_load 列 time_mean 中点（f64 构造），返回 300 个 f64。"""
    col = []
    for j in range(HIST_ROWS):
        v0 = plan.ticks[t0 - HIST_ROWS + j][1]      # TICK2 字段 [1] = P_load f64
        v1 = plan.ticks[t0 - HIST_ROWS + 1 + j][1]
        col.append((v0 + v1) * 0.5)                 # f64 中点，op 序冻结
    return col


def sentinel_presence(plan: SessionPlan, t0: int):
    """脚本 sentinel 在位性（防退化脚本）：P(t0) > P(t0-1) > max(P(<t0-1))。"""
    pl = {t: v[1] for t, v in plan.ticks.items()}
    last = pl[t0]
    prev = pl[t0 - 1]
    earlier_max = max(pl[t] for t in pl if t < t0 - 1)
    return last > prev > earlier_max, (earlier_max, prev, last)


# ---------------------------------------------------------------- 模拟 capture
def construct_hist_sim(plan: SessionPlan, t0: int):
    """§6.1 冻结构造的 host 侧复算（与 ems04_h1win.construct 逐通道同式）。"""
    wd = plan.water_depth
    ctrl = plan.ctrl_mode
    out = bytearray()
    for j in range(HIST_ROWS):
        va = plan.ticks[t0 - HIST_ROWS + j]
        vb = plan.ticks[t0 - HIST_ROWS + 1 + j]
        # 环通道序（TICK2_TO_RING）：P_load←v1 v_ship←v4 accel←v5 n_prop←v7
        #                            P_hotel←v8 v_current←v9 (v_ref←v3 端点列) P_prop←v10
        ra = (va[1], va[4], va[5], va[7], va[8], va[9], None, va[10])
        rb = (vb[1], vb[4], vb[5], vb[7], vb[8], vb[9], None, vb[10])
        for c in range(HIST_COLS):
            if c == 6:                       # water_depth 常量列（(w+w)*0.5 = w）
                b_ = (wd + wd) * 0.5
            elif c == 8:                     # ctrl_mode 常量列（AUTO = 0.0）
                b_ = ctrl
            elif c == 9:                     # v_ref：interval_endpoint 右端点
                b_ = vb[3]
            elif c == 10:                    # zero-fill（AUTO 分支）
                b_ = 0.0
            else:                            # time_mean：f64 中点
                b_ = (ra[c] + rb[c]) * 0.5
            if not math.isfinite(b_):
                return None                  # 出口有限性门（fail-closed）
            out += f32_bytes(b_)
    return bytes(out)


def kf_lookup_sim(plan: SessionPlan, t0: float):
    """kf 行查找：issue_time_s 与 t0 f64 位级相等。返回 720 B 段或 None。"""
    want = struct.pack("<d", t0)
    for row in plan.kf_rows:
        if len(row) == KF_ROW_PAYLOAD_BYTES and row[:8] == want:
            return row[8:]
    return None


def simulate_capture(plan: SessionPlan, break_a6: bool = False) -> bytes:
    """按脚本帧序构造期望响应字节流（喂核验器应 overall=pass）。

    负路径按 S3-A2 脚本期望应答（neg1 → 0x45）。注意：冻结固件 gate 序在窗满后对
    QUERY(299) 回 0x43（范围门）——本模拟按脚本期望语义实现，冲突已升级（docstring 勘误 3）。
    break_a6=True 时在构造后、锁存前把 hist 末行 P_load 改为
    f32(transform_h1(1298.0))（锁存域注入，QUERY sha 随之一致，A6 单点失败）。
    """
    gen = load_gen_ref() if break_a6 else None
    norm = gen.load_norm_artifacts() if break_a6 else None
    bad_val = None
    if break_a6:
        import numpy as np
        # transform_h1 为 bin 级（mean/std 为 300 元素行数组）：
        # 对常值 1298.0 列做整列复算，取末行 bin（row299）的归一化值
        col = np.full(HIST_ROWS, 1298.0, dtype=np.float64)
        bad = gen.transform_h1(col, "P_load_total_kW", norm["formal"])
        bad_val = f32_bytes(to_scalar(bad[HIST_ROWS - 1]))

    out = bytearray()
    inited = False
    table_ready = False
    latched = None            # (t0, hist_bytes, kf_seg)
    last_query_t0 = None

    def nack(cmd, err):
        out.extend(enc_frame(T_NACK, bytes([cmd, err])))

    def ack(cmd):
        out.extend(enc_frame(T_ACK, bytes([cmd])))

    for frame, _n in plan.script.units:
        ftype, pay = frame_fields(frame)
        if ftype is None:
            continue
        if ftype == T_VI:
            inited = True
            table_ready = False
            latched = None
            ack(ftype)
        elif ftype == T_KFC:
            if not inited:
                nack(ftype, ERR_STATE)
                continue
            ci, total, clen = struct.unpack_from("<IIH", pay, 0)
            if ci == total - 1:
                table_ready = len(plan.kf_rows) == plan.kf_n_rows
            ack(ftype)
        elif ftype == T_TICK2:
            if not inited:
                nack(ftype, ERR_STATE)
                continue
            ack(ftype)
        elif ftype == T_QUERY:
            t0 = struct.unpack_from("<d", pay, 0)[0]
            if not inited or not table_ready:
                nack(ftype, ERR_STATE)
            elif not math.isfinite(t0) or t0 != math.floor(t0):
                nack(ftype, ERR_TICK_T)
            elif t0 != float(plan.max_tick()):
                # S3-A2 锚点语义（PROTOCOL_FP §4 冻结补充）：非当前拍 → NOT_READY
                nack(ftype, ERR_NOT_READY)
            else:
                hist = construct_hist_sim(plan, int(t0))
                kf_seg = kf_lookup_sim(plan, t0)
                if hist is None or kf_seg is None:
                    nack(ftype, ERR_KF_ROW)
                else:
                    if break_a6 and bad_val is not None:
                        off = (HIST_ROWS - 1) * HIST_COLS * 4 + P_LOAD_COL * 4
                        hist = hist[:off] + bad_val + hist[off + 4:]
                    latched = (t0, hist, kf_seg)
                    last_query_t0 = t0
                    payload = (struct.pack("<d", t0) + b"\x00\x00\x00\x00"
                               + sha256(hist) + sha256(kf_seg))
                    out.extend(enc_frame(T_RSP_QUERY, payload))
        elif ftype == T_DUMP:
            t0 = struct.unpack_from("<d", pay, 0)[0]
            if latched is None or struct.pack("<d", t0) != struct.pack("<d", last_query_t0):
                nack(ftype, ERR_STATE)
            else:
                _t0, hist, kf_seg = latched
                payload = (struct.pack("<d", t0) + b"\x00\x00\x00\x00"
                           + hist + kf_seg)
                out.extend(enc_frame(T_RSP_DUMP, payload))
        else:
            nack(ftype, ERR_STATE)
    return bytes(out)


# ---------------------------------------------------------------- 核验主体
def run_checks(script: Script, cap: Capture, plan: SessionPlan, asserts: dict):
    results = {}
    details = {}
    paired, consumed = pair_responses(script, cap)

    def unit_rsp(idx):
        return paired[idx] if idx < len(paired) else []

    # ---- A1 会话帧：VI/KFC/TICK2 全部 ACK 对应 + capture 全消费 + 零垃圾/CRC 错
    a1_bad = []
    a1_ack = 0
    for idx, (frame, _n) in enumerate(script.units):
        ftype, _pay = frame_fields(frame)
        if ftype not in (T_VI, T_KFC, T_TICK2):
            continue
        rsp = unit_rsp(idx)
        if len(rsp) != 1 or rsp[0][0] != T_ACK or rsp[0][1] != bytes([ftype]) \
                or not rsp[0][2]:
            a1_bad.append("unit#%d(%s) 响应=%s"
                          % (idx, TYPE_NAMES.get(ftype, "?"),
                             ["%s/crc%s" % (TYPE_NAMES.get(t, hex(t)), ok) for t, _p, ok in rsp]
                             if rsp else "缺失"))
        else:
            a1_ack += 1
    extra = len(cap.frames) - consumed
    if a1_bad:
        results["A1"] = "FAIL: " + "; ".join(a1_bad[:8]) + ("（共 %d 处）" % len(a1_bad) if len(a1_bad) > 8 else "")
    elif extra != 0:
        results["A1"] = "FAIL: capture 存在 %d 条未被脚本配对的多余响应帧" % extra
    elif cap.junk_bytes or cap.partial_tail:
        results["A1"] = "FAIL: 垃圾字节 %d / 不完整尾帧 %d（重同步被触发）" % (
            cap.junk_bytes, cap.partial_tail)
    elif cap.crc_bad:
        results["A1"] = "FAIL: %d 条响应帧 CRC 错" % cap.crc_bad
    else:
        results["A1"] = "PASS (VI=%d KFC=%d TICK2=%d 全 ACK 对应；capture %d 帧全消费)" % (
            plan.counts[T_VI], plan.counts[T_KFC], plan.counts[T_TICK2], len(cap.frames))
    details["A1"] = {"ack_ok": a1_ack, "capture_frames": len(cap.frames),
                     "consumed": consumed, "extra_frames": extra,
                     "junk_bytes": cap.junk_bytes, "crc_bad": cap.crc_bad,
                     "partial_tail": cap.partial_tail}

    # ---- A2/A3 负路径
    neg_units = {QUERY_KIND_NEG1: None, QUERY_KIND_NEG2: None}
    for idx, t0 in plan.queries:
        k = plan.query_kind(t0)
        if k in neg_units:
            neg_units[k] = (idx, t0)
    for key, akey, want_err, label in (
            (QUERY_KIND_NEG1, "A2", NEG1_ERR, "QUERY t0=299"),
            (QUERY_KIND_NEG2, "A3", NEG2_ERR, "QUERY t0=300.5")):
        u = neg_units.get(key)
        if u is None:
            results[akey] = "FAIL: 脚本中未找到 %s 帧" % label
            continue
        idx, t0 = u
        rsp = unit_rsp(idx)
        if len(rsp) != 1:
            results[akey] = "FAIL: %s 响应缺失（得 %d 条）" % (label, len(rsp))
        else:
            t, p, ok = rsp[0]
            if t != T_NACK:
                results[akey] = "FAIL: %s 期望 NACK 得 TYPE=0x%02X" % (label, t)
            elif len(p) != 2 or p[0] != T_QUERY:
                results[akey] = "FAIL: %s NACK 载荷异常 %s（期望 [cmd=0x23,err]）" % (label, p.hex())
            elif p[1] != want_err:
                msg = "FAIL: %s 期望 err=0x%02X 得 0x%02X" % (label, want_err, p[1])
                if key == QUERY_KIND_NEG1 and p[1] == ERR_TICK_T:
                    msg += "（冻结固件 gate 序对窗满后 QUERY(299) 回范围门 0x43——与脚本/规格期望 0x45 冲突，需主 Agent 裁决，见模块 docstring 勘误 3）"
                results[akey] = msg
            elif not ok:
                results[akey] = "FAIL: %s NACK 帧 CRC 错" % label
            else:
                results[akey] = "PASS (NACK cmd=0x23 err=0x%02X)" % p[1]

    # ---- A4/A5 正路径响应头
    rsp_q = None
    rsp_d = None
    pos_q = [u for u in plan.queries if plan.query_kind(u[1]) == QUERY_KIND_POS]
    if not pos_q:
        results["A4"] = "FAIL: 脚本中未找到 QUERY t0=300 帧"
    else:
        idx, t0 = pos_q[0]
        rsp = unit_rsp(idx)
        if len(rsp) != 1 or rsp[0][0] != T_RSP_QUERY:
            results["A4"] = "FAIL: 期望 RSP_FP_QUERY 得 %s" % (
                ["0x%02X" % t for t, _p, _o in rsp] if rsp else "缺失")
        else:
            t, p, ok = rsp[0]
            if len(p) != RSP_QUERY_LEN:
                results["A4"] = "FAIL: RSP_FP_QUERY 载荷 %d B != %d" % (len(p), RSP_QUERY_LEN)
            elif struct.pack("<d", struct.unpack_from("<d", p, 0)[0]) != struct.pack("<d", float(plan.max_tick())):
                results["A4"] = "FAIL: t0 回显非 300.0 位级（got=%r)" % struct.unpack_from("<d", p, 0)[0]
            elif p[8] != 0:
                results["A4"] = "FAIL: status=%d != 0" % p[8]
            elif not ok:
                results["A4"] = "FAIL: RSP_FP_QUERY 帧 CRC 错"
            else:
                rsp_q = p
                results["A4"] = "PASS (t0 位级回显 300.0, status=0, 76 B)"
    if not plan.dumps:
        results["A5"] = "FAIL: 脚本中未找到 DUMP t0=300 帧"
    else:
        idx, t0 = plan.dumps[0]
        rsp = unit_rsp(idx)
        if len(rsp) != 1 or rsp[0][0] != T_RSP_DUMP:
            results["A5"] = "FAIL: 期望 RSP_FP_DUMP 得 %s" % (
                ["0x%02X" % t for t, _p, _o in rsp] if rsp else "缺失")
        else:
            t, p, ok = rsp[0]
            if len(p) != RSP_DUMP_LEN:
                results["A5"] = "FAIL: RSP_FP_DUMP 载荷 %d B != %d" % (len(p), RSP_DUMP_LEN)
            elif struct.pack("<d", struct.unpack_from("<d", p, 0)[0]) != struct.pack("<d", t0):
                results["A5"] = "FAIL: t0 回显位级不符"
            elif p[8] != 0:
                results["A5"] = "FAIL: status=%d != 0" % p[8]
            elif not ok:
                results["A5"] = "FAIL: RSP_FP_DUMP 帧 CRC 错"
            else:
                rsp_d = p
                results["A5"] = "PASS (t0 位级回显, status=0, %d B)" % len(p)

    hist_seg = rsp_d[12:12 + HIST_BYTES] if rsp_d is not None else None
    kf_seg = rsp_d[12 + HIST_BYTES:12 + HIST_BYTES + KF_SEG_BYTES] if rsp_d is not None else None

    # ---- A6 窗右端点 = t0（核心）
    if rsp_d is None:
        results["A6"] = "FAIL: 依赖 A5 的 DUMP 载荷缺失"
    else:
        t0 = plan.max_tick()
        t0_bits = struct.pack("<d", float(t0))
        dump_t0 = struct.unpack_from("<d", rsp_d, 0)[0]
        if struct.pack("<d", dump_t0) != t0_bits:
            results["A6"] = "FAIL: DUMP t0=%r 与脚本锚点 %d 不符" % (dump_t0, t0)
        else:
            ok_pres, sent_vals = sentinel_presence(plan, t0)
            if not ok_pres:
                results["A6"] = "FAIL: 脚本 sentinel 不在位 P(<t0-1)max=%r < P(t0-1)=%r < P(t0)=%r 不成立" % sent_vals
            else:
                exp_col = expected_pload_column(plan, t0)
                off0 = P_LOAD_COL * 4
                cap_col = [struct.unpack_from("<f", hist_seg, r * HIST_COLS * 4 + off0)[0]
                           for r in range(HIST_ROWS)]
                mism = [r for r in range(HIST_ROWS)
                        if f32_bytes(exp_col[r]) != struct.pack("<f", cap_col[r])]
                ramp_nondecr = all(cap_col[r] <= cap_col[r + 1] for r in range(HIST_ROWS - 3))
                ordering = (max(cap_col) == cap_col[HIST_ROWS - 1]
                            and cap_col[HIST_ROWS - 1] > cap_col[HIST_ROWS - 2]
                            and cap_col[HIST_ROWS - 2] > max(cap_col[:HIST_ROWS - 2]))
                if mism:
                    r = mism[0]
                    results["A6"] = ("FAIL: P_load 列(index %d) 位级复算 %d/%d 行不符，"
                                     "首Mismatch row%d: capture=%r(%s) expect_f32=%r"
                                     % (P_LOAD_COL, len(mism), HIST_ROWS, r,
                                        cap_col[r], struct.pack("<f", cap_col[r]).hex(),
                                        struct.unpack("<f", f32_bytes(exp_col[r]))[0]))
                elif not ramp_nondecr:
                    bad = [r for r in range(HIST_ROWS - 3) if cap_col[r] > cap_col[r + 1]]
                    results["A6"] = "FAIL: ramp 段(row0..297)非单调非降，首逆序 row%d" % bad[0]
                elif not ordering:
                    results["A6"] = ("FAIL: sentinel 排序不成立 row298=%r row299=%r "
                                     "max(row0..297)=%r" % (cap_col[HIST_ROWS - 2],
                                                            cap_col[HIST_ROWS - 1],
                                                            max(cap_col[:HIST_ROWS - 2])))
                else:
                    # 6c host 归一化期望（transform_h1 单点复算，f64 → cast f32）。
                    # transform_h1 的 mean/std 为 bin 级（300 元素行数组），故对整列
                    # 复算后取 row299/298/297 元素（等价于各行的单点复算）。
                    try:
                        gen = load_gen_ref()
                        norm = gen.load_norm_artifacts()
                        import numpy as np
                        ch = "P_load_total_kW"
                        exp_arr = np.asarray(exp_col, dtype=np.float64)
                        cap_arr = np.asarray([float(v) for v in cap_col], dtype=np.float64)
                        n_all = gen.transform_h1(exp_arr, ch, norm["formal"])
                        c_all = gen.transform_h1(cap_arr, ch, norm["formal"])
                        n299 = f32_bytes(to_scalar(n_all[HIST_ROWS - 1]))
                        n298 = f32_bytes(to_scalar(n_all[HIST_ROWS - 2]))
                        n297 = f32_bytes(to_scalar(n_all[HIST_ROWS - 3]))
                        c299 = f32_bytes(to_scalar(c_all[HIST_ROWS - 1]))
                        c298 = f32_bytes(to_scalar(c_all[HIST_ROWS - 2]))
                        f = lambda b: struct.unpack("<f", b)[0]
                        if not (f(n299) > f(n298) > f(n297)):
                            results["A6"] = ("FAIL: 归一化期望保序不成立 norm299=%s norm298=%s "
                                             "norm297=%s" % (n299.hex(), n298.hex(), n297.hex()))
                        elif c299 != n299 or c298 != n298:
                            results["A6"] = "FAIL: capture 实测值归一化复算与期望位级不符"
                        else:
                            results["A6"] = ("PASS (位级复算 300/300 行；row299=f32(%.1f) "
                                             "> row298=f32(%.1f) > max(ramp)；norm 保序位级一致)"
                                             % (exp_col[HIST_ROWS - 1], exp_col[HIST_ROWS - 2]))
                    except SystemExit as exc:
                        results["A6"] = "FAIL: host 归一化期望无法复算：%s" % exc
                details["A6"] = {"p_load_col": P_LOAD_COL, "sentinel_ticks": sent_vals,
                                 "bit_mismatch_rows": len(mism) if mism else 0,
                                 "expected_row298_f64": exp_col[HIST_ROWS - 2],
                                 "expected_row299_f64": exp_col[HIST_ROWS - 1]}

    # ---- A7 kf 行签名
    if kf_seg is None:
        results["A7"] = "FAIL: 依赖 A5 的 DUMP kf 段缺失"
    else:
        want10 = struct.pack("<f", 10.0)
        all10 = all(kf_seg[i * 4:i * 4 + 4] == want10 for i in range(KF_R * KF_C))
        row0 = kf_seg[:KF_C * 4]
        all10_row0 = all(row0[i * 4:i * 4 + 4] == want10 for i in range(KF_C))
        want_issue = struct.pack("<d", float(plan.max_tick()))
        script_row = None
        for row in plan.kf_rows:
            if len(row) == KF_ROW_PAYLOAD_BYTES and row[:8] == want_issue:
                script_row = row[8:]
                break
        if not all10:
            bad = [i for i in range(KF_R * KF_C) if kf_seg[i * 4:i * 4 + 4] != want10]
            r, c = divmod(bad[0], KF_C)
            results["A7"] = ("FAIL: kf (30,6) 存在非 10.0 值 %d 处，首处 row%d col%d = %r"
                             % (len(bad), r, c, struct.unpack_from("<f", kf_seg, bad[0] * 4)[0]))
        elif script_row is None:
            results["A7"] = "FAIL: 脚本 kf 表中未找到 issue_time_s 位级 == 300.0 的行"
        elif script_row != kf_seg:
            results["A7"] = "FAIL: DUMP kf 720 B 与脚本表 row0(issue=300.0) 段位级不一致"
        elif not all10_row0:
            results["A7"] = "FAIL: kf row0 首 6 值非全 10.0（与 (30,6) 行签名口径矛盾）"
        else:
            results["A7"] = "PASS (row0 全 180 f32 == 10.0 位级；与脚本表 issue=300.0 行 720 B 位级一致)"

    # ---- A8 发布/锁存一致性
    if rsp_q is None or hist_seg is None or kf_seg is None:
        results["A8"] = "FAIL: 依赖 A4/A5 的载荷缺失"
    else:
        q_hist_sha = rsp_q[12:12 + 32]
        q_kf_sha = rsp_q[12 + 32:12 + 64]
        d_hist_sha = sha256(hist_seg)
        d_kf_sha = sha256(kf_seg)
        if q_hist_sha != d_hist_sha and q_kf_sha != d_kf_sha:
            results["A8"] = "FAIL: hist/kf sha 均不一致"
        elif q_hist_sha != d_hist_sha:
            results["A8"] = "FAIL: hist_sha256 不一致 QUERY=%s DUMP=%s" % (
                q_hist_sha.hex()[:16], d_hist_sha.hex()[:16])
        elif q_kf_sha != d_kf_sha:
            results["A8"] = "FAIL: kf_sha256 不一致 QUERY=%s DUMP=%s" % (
                q_kf_sha.hex()[:16], d_kf_sha.hex()[:16])
        else:
            results["A8"] = "PASS (hist/kf sha256 QUERY↔DUMP 位级一致)"

    first_failure = None
    for k in ("A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8"):
        if results[k].startswith("FAIL"):
            first_failure = k
            break
    return results, first_failure, details


# ---------------------------------------------------------------- dry-run / 汇总
def dry_run(script: Script, plan: SessionPlan, asserts: dict) -> int:
    print("--- 脚本 %s ---" % script.path)
    print("    sid=%s n_ops=%d units=%d notes=%d bytes=%d sha256=%s"
          % (script.sid, script.n_ops, len(script.units), script.note_count,
             os.path.getsize(script.path), script.sha256[:16] + "…"))
    print("    framing_bad=%d crc_bad=%d" % (script.framing_bad, script.crc_bad))
    print("    帧型计数: %s" % {TYPE_NAMES.get(t, hex(t)): c for t, c in plan.counts.items()})
    print("    QUERY: %s / DUMP: %s"
          % ([(i, t0, plan.query_kind(t0)) for i, t0 in plan.queries],
             [(i, t0) for i, t0 in plan.dumps]))
    print("    VI: tick_last=%d kf_n_rows=%d water_depth=%r ctrl=%r"
          % (plan.max_tick(), plan.kf_n_rows, plan.water_depth, plan.ctrl_mode))
    print("    kf 表: chunks=%d rows=%d" % (plan.chunk_total or 0, len(plan.kf_rows)))
    print("--- 配对计划（每 FEED 1 期望响应，共 %d 单元）---" % len(script.units))
    print("    [0]      VOYAGE_INIT        -> ACK(0x20)")
    print("    [1..%d]   KF_CHUNK×%d        -> ACK(0x21)×%d"
          % (plan.counts[T_KFC], plan.counts[T_KFC], plan.counts[T_KFC]))
    print("    [..%d]    TICK2×%d          -> ACK(0x22)×%d"
          % (plan.counts[T_KFC] + plan.counts[T_TICK2], plan.counts[T_TICK2],
             plan.counts[T_TICK2]))
    print("    [..]     QUERY t0=299/300.5/300 -> NACK 0x45 / NACK 0x43 / RSP_FP_QUERY")
    print("    [末]     DUMP t0=300        -> RSP_FP_DUMP(13932 B 载荷)")
    sha_ok = asserts.get("script_sha256") == script.sha256
    print("    asserts.json script_sha256 比对: %s" % ("一致" if sha_ok else "不一致"))
    if script.framing_bad or script.crc_bad or not sha_ok:
        print("DRY-RUN: FAIL")
        return 5
    print("DRY-RUN: PASS（配对计划如上，等待 --capture 执行 A1..A8）")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="S3-A2 时序语义微测断言机（capture 离线核验器）")
    ap.add_argument("--capture", help="板侧响应字节流（fp_replay_client capture）")
    ap.add_argument("--synth-dir", help="S3-A2 产物目录（script_s3a2.bin + asserts.json）")
    ap.add_argument("--out", help="结果 JSON 落盘路径")
    ap.add_argument("--dry-run", action="store_true",
                    help="无 capture：仅解析 script + asserts 并打印配对计划")
    ap.add_argument("--emit-sim-capture",
                    help="自测：按脚本帧序构造模拟 capture 写入指定路径后退出")
    ap.add_argument("--sim-break-a6", action="store_true",
                    help="与 --emit-sim-capture 联用：锁存域注入 A6 故障（hist 末行 P_load "
                         "→ f32(transform_h1(1298.0))）")
    args = ap.parse_args(argv)

    if not args.synth_dir:
        ap.print_help()
        return 2
    script_path = os.path.join(args.synth_dir, "script_s3a2.bin")
    asserts_path = os.path.join(args.synth_dir, "asserts.json")
    if not os.path.isfile(script_path) or not os.path.isfile(asserts_path):
        print("ERROR: %s / %s 缺一" % (script_path, asserts_path), file=sys.stderr)
        return 2
    script = Script(script_path)
    with open(asserts_path, encoding="utf-8") as fh:
        asserts = json.load(fh)
    plan = SessionPlan(script)

    if args.dry_run:
        return dry_run(script, plan, asserts)

    if args.emit_sim_capture:
        blob = simulate_capture(plan, break_a6=args.sim_break_a6)
        with open(args.emit_sim_capture, "wb") as fh:
            fh.write(blob)
        n_frames = blob.count(bytes([SYNC0, SYNC1]))  # 近似计数（负载含 AA55 概率极低）
        print("[sim] 模拟 capture → %s（%d B；break_a6=%s）"
              % (args.emit_sim_capture, len(blob), args.sim_break_a6))
        return 0

    if not args.capture:
        print("ERROR: 需要 --capture（或 --dry-run / --emit-sim-capture）", file=sys.stderr)
        return 2
    cap = Capture(args.capture)
    results, first_failure, details = run_checks(script, cap, plan, asserts)

    labels = {
        "A1": "会话帧 ACK 对应", "A2": "负路径1 QUERY t0=299 → NACK 0x45",
        "A3": "负路径2 QUERY t0=300.5 → NACK 0x43", "A4": "QUERY t0=300 → RSP_QUERY t0 位级回显",
        "A5": "DUMP t0=300 → RSP_DUMP t0 回显", "A6": "窗右端点=t0（hist P_load 列位级+排序+归一化期望）",
        "A7": "kf 行签名 row0 全 10.0", "A8": "QUERY↔DUMP sha256 一致",
    }
    for k in ("A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8"):
        print("%s %-46s %s" % (k, labels[k], results[k]))
    overall = "pass" if first_failure is None else "fail"
    print("OVERALL: %s%s" % (overall.upper(),
                             ("（first_failure=%s）" % first_failure) if first_failure else ""))

    if args.out:
        payload = {"overall": overall}
        payload.update({k: results[k] for k in ("A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8")})
        payload["first_failure"] = first_failure
        payload["details"] = details
        with open(args.out, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=2)
        print("[check] 结果 JSON → %s" % args.out)
    return 0 if overall == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())

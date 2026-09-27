#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3c_pilot_batch.py — EMS04 S3-C Q27Shadow 六锚批式驱动
（S3C_BACKEND_RCA_DEVELOPMENT_PILOT，裁决 §9）。

在 Q27Shadow 诊断固件上无中断连跑 6 预声明锚：
  FLTA_S1_D00000_M2: t0 = 300 / 2180 / 4070
  FLTA_S2_D01009_M1: t0 = 300 / 2180 / 4060
逐锚经 UART CMD_REATT（复用同目录 s3c_reatt_replay.py 的 Board 类与锚加载，
输入 = S2 真值包 canonical 特征字节 + T0_INDEX.json sha256 校验），
收链尾 q30×3 raw f32 + infer_us，产出 jsonl capture。
"无中断"语义由使用者保证：本工具不复位/不重载板，仅逐锚打印
[i/6] REATT <vid> t0=... 提示外部（主 Agent）已 reload。

另支持 --shadow-dump <file> 离线模式：不经串口，直接读 Q27Shadow 影子缓冲
镜像（ems04_s3c_q27_shadow.h 布局，即 s3a2_dump_reasm.py --out 产物）：
  28 B 头（小端）: magic u32=0x51323753("Q27S") / version u16=1 /
    hdr_len u16=28 / seq u32 / src_addr u32=0x34357080 / len u32=3300 /
    crc32 u32 / valid u32，紧随 3300 B int8 码 payload。
校验 fail-closed（任一失败即非零退出）：magic / hdr_len / len / valid /
crc32（CRC-32/IEEE 反射式 poly 0xEDB88320，同固件逐位实现）。
通过则打印 Q27 码域统计（min / max / ==127 数 / ==-128 数）与 sha256
（payload 码 sha256 + 整文件 sha256）。

用法（路径均相对仓库根）：
  --list                                          打印 6 锚清单（可配 --anchors）
  --shadow-dump dump.bin                          离线校验影子缓冲镜像
  --port /dev/cu.xxx                              连跑 6 锚（默认 baud 115200）
  --port ... --out cap.jsonl                      指定 capture 输出文件
  --port ... --anchors "VID:300,VID:2180"         覆盖锚清单（vid:t0 逗号分隔）
  --port ... --stop-on-error                      首错即停（默认记录 error 继续）

依赖：numpy（pyserial 仅串口模式需要）。
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
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

# 复用 S3-C 回放客户端：Board（UART 协议）+ 锚索引/加载（sha256 校验）
from s3c_reatt_replay import (  # noqa: E402
    Board,
    HIST_FLT,
    KF_FLT,
    load_anchor_bin,
    load_anchor_index,
)

# Q27Shadow 影子缓冲镜像常量（与 Appli/App/ems04_s3c_q27_shadow.h 逐项一致）
SHADOW_MAGIC = 0x51323753      # 固件常量 "Q27S"（u32 小端存储）
SHADOW_VERSION = 1
SHADOW_HDR_SIZE = 28
SHADOW_LEN = 3300              # int8 [1,300,11]
SHADOW_SRC_ADDR = 0x34357080   # 0x34350000 + 28800（仅打印展示用）
SHADOW_HDR_FMT = "<IHHIIIII"   # magic ver hdr_len seq src_addr len crc32 valid

# 预声明 6 锚（裁决 §9，与 S3C_BACKEND_RCA_DEVELOPMENT_PILOT 一致）
DEFAULT_ANCHORS = [
    ("FLTA_S1_D00000_M2", 300),
    ("FLTA_S1_D00000_M2", 2180),
    ("FLTA_S1_D00000_M2", 4070),
    ("FLTA_S2_D01009_M1", 300),
    ("FLTA_S2_D01009_M1", 2180),
    ("FLTA_S2_D01009_M1", 4060),
]


def crc32_reflected(data: bytes) -> int:
    """CRC-32/IEEE 反射式（poly 0xEDB88320，初值/终值异或 0xFFFFFFFF），
    逐位自实现，与 ems04_s3c_q27_shadow.c 完全一致（= zlib.crc32）。"""
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xEDB88320 if (crc & 1) else (crc >> 1)
    return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF


def parse_anchors(spec: str):
    """--anchors "VID:t0,VID:t0" → [(vid, t0), ...]，保留声明顺序。"""
    out = []
    for item in spec.split(","):
        item = item.strip()
        if not item:
            continue
        if ":" not in item:
            raise SystemExit(f"--anchors 条目缺少 ':t0': {item!r}")
        vid, _, t0s = item.partition(":")
        try:
            out.append((vid.strip(), int(t0s)))
        except ValueError:
            raise SystemExit(f"--anchors t0 非整数: {item!r}")
    if not out:
        raise SystemExit("--anchors 解析结果为空")
    return out


def resolve_anchors(requested):
    """(vid, t0) 清单 × T0_INDEX → [(i, vid, t0, hist, kf)]（保留声明顺序，
    逐锚 sha256 校验）。任一锚未登记/文件缺失 → SystemExit（fail-closed）。"""
    index = {(a[0], a[1]): a for a in load_anchor_index()}
    missing = [r for r in requested if r not in index]
    if missing:
        raise SystemExit(
            "锚未在 T0_INDEX 登记: "
            + ", ".join(f"{v}:t0={t}" for v, t in missing))
    frames = []
    for i, (vid, t0) in enumerate(requested):
        _, _, hp, kp, hsha, ksha = index[(vid, t0)]
        hist = load_anchor_bin(hp, HIST_FLT, hsha)
        kf = load_anchor_bin(kp, KF_FLT, ksha)
        frames.append((i, vid, t0, hist, kf))
    return frames


def run_batch(args):
    requested = parse_anchors(args.anchors) if args.anchors else DEFAULT_ANCHORS
    frames = resolve_anchors(requested)
    n = len(frames)
    print(f"锚清单: {n} 条（预声明 6 锚; S3C_BACKEND_RCA_DEVELOPMENT_PILOT §9）")
    print(f"输入加载+sha 校验: {n}/{n} PASS")

    out_path = args.out
    if not out_path:
        stamp = time.strftime("%Y%m%dT%H%M%S", time.gmtime())
        out_path = os.path.join(SCRIPT_DIR, "..", "results", "s3c_pilot",
                                f"s3c_pilot_capture_{stamp}.jsonl")
    out_path = os.path.abspath(out_path)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    b = Board(args.port, args.baud)
    n_ok = n_err = 0
    t_start = time.time()
    with open(out_path, "w") as cap:
        for i, vid, t0, hist, kf in frames:
            print(f"[{i + 1}/{n}] REATT {vid} t0={t0}", flush=True)
            try:
                q, us = b.reatt(hist, kf, i)
                cap.write(json.dumps({
                    "seq": i, "vid": vid, "t0": t0,
                    "q_f32": q.reshape(-1).tolist(), "infer_us": int(us),
                }) + "\n")
                cap.flush()
                n_ok += 1
                print(f"    ok infer_us={int(us)}")
            except (RuntimeError, TimeoutError) as e:
                cap.write(json.dumps({
                    "seq": i, "vid": vid, "t0": t0, "error": str(e),
                }) + "\n")
                cap.flush()
                n_err += 1
                print(f"    ERROR: {e}", flush=True)
                if args.stop_on_error:
                    print("--stop-on-error: 停止批跑", flush=True)
                    break
    dt = time.time() - t_start
    b.close()
    summary = {
        "anchors_total": n, "n_ok": n_ok, "n_err": n_err,
        "elapsed_s": round(dt, 1),
        "cap_sha256": hashlib.sha256(
            open(out_path, "rb").read()).hexdigest(),
    }
    print(json.dumps(summary, indent=2))
    print(f"capture: {out_path}")
    return 0 if n_err == 0 else 1


def inspect_shadow_dump(path: str) -> int:
    """读影子缓冲镜像，fail-closed 校验后打印码域统计与 sha256。"""
    try:
        buf = open(path, "rb").read()
    except OSError as e:
        print(f"shadow-dump FAIL: 无法读取 {path}: {e}", file=sys.stderr)
        return 2

    def fail(msg):
        print(f"shadow-dump FAIL: {msg}", file=sys.stderr)
        return 2

    if len(buf) < SHADOW_HDR_SIZE:
        return fail(f"文件 {len(buf)} B < 头 {SHADOW_HDR_SIZE} B")
    (magic, ver, hdr_len, seq, src_addr, plen,
     crc32_rx, valid) = struct.unpack_from(SHADOW_HDR_FMT, buf, 0)
    if magic != SHADOW_MAGIC:
        return fail(f"magic {magic:#010x} != {SHADOW_MAGIC:#010x} (Q27S)")
    if hdr_len != SHADOW_HDR_SIZE:
        return fail(f"hdr_len {hdr_len} != {SHADOW_HDR_SIZE}")
    if ver != SHADOW_VERSION:
        return fail(f"version {ver} != {SHADOW_VERSION}")
    if plen != SHADOW_LEN:
        return fail(f"len {plen} != {SHADOW_LEN}（int8 [1,300,11]）")
    if len(buf) != SHADOW_HDR_SIZE + plen:
        return fail(f"文件 {len(buf)} B != 头 {SHADOW_HDR_SIZE} + payload {plen} B")
    if valid != 1:
        return fail(f"valid={valid}（固件 fail-closed：capture 未完成或被 reset）")
    payload = buf[SHADOW_HDR_SIZE:]
    crc32_calc = crc32_reflected(payload)
    if crc32_calc != crc32_rx:
        return fail(f"crc32 不符: hdr={crc32_rx:#010x} calc={crc32_calc:#010x} "
                    f"（payload 损坏或头/体错位）")

    codes = np.frombuffer(payload, dtype=np.int8)
    n127 = int(np.count_nonzero(codes == 127))
    nm128 = int(np.count_nonzero(codes == -128))
    print("shadow-dump PASS")
    print(f"  file      : {path}")
    print(f"  hdr       : magic=Q27S ver={ver} hdr_len={hdr_len} seq={seq} "
          f"src_addr={src_addr:#x} len={plen} valid={valid}")
    print(f"  crc32     : {crc32_rx:#010x}（反射 0xEDB88320 校验一致）")
    print(f"  q27 codes : n={codes.size} min={codes.min()} max={codes.max()} "
          f"==127: {n127}  ==-128: {nm128}")
    print(f"  sha256    : payload={hashlib.sha256(payload).hexdigest()}")
    print(f"  sha256    : file={hashlib.sha256(buf).hexdigest()}")
    return 0


def print_list(requested):
    n = len(requested)
    print(f"锚清单（{n} 条; S3C_BACKEND_RCA_DEVELOPMENT_PILOT 裁决 §9）:")
    for i, (vid, t0) in enumerate(requested):
        print(f"  [{i + 1}/{n}] {vid} t0={t0}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__.splitlines()[0],
        epilog="示例: python3 Host/tools/s3c_pilot_batch.py --list",
    )
    ap.add_argument("--port", help="串口设备（--shadow-dump / --list 时可省略）")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--anchors",
                    help="覆盖锚清单: \"VID:t0,VID:t0\"（逗号分隔，默认预声明 6 锚）")
    ap.add_argument("--out", help="capture 输出 jsonl 路径"
                                 "（默认 Host/results/s3c_pilot/s3c_pilot_capture_<utc>.jsonl）")
    ap.add_argument("--stop-on-error", action="store_true",
                    help="单锚失败即停止（默认记录 error 行并继续）")
    ap.add_argument("--shadow-dump", metavar="FILE",
                    help="离线模式：读影子缓冲镜像并 fail-closed 校验（不经串口）")
    ap.add_argument("--list", action="store_true",
                    help="打印锚清单后退出")
    args = ap.parse_args()

    if args.shadow_dump:
        return inspect_shadow_dump(args.shadow_dump)
    requested = parse_anchors(args.anchors) if args.anchors else DEFAULT_ANCHORS
    if args.list:
        print_list(requested)
        return 0
    if not args.port:
        ap.error("--port 必填（或用 --shadow-dump / --list）")
    return run_batch(args)


if __name__ == "__main__":
    sys.exit(main())

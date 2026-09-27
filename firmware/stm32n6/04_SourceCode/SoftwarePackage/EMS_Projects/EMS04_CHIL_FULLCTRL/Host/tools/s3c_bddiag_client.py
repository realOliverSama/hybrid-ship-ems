#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3c_bddiag_client.py — S3-C BD 腿批量诊断回传客户端（CMD_BDDIAG 0x0C / RSP_BDDIAG 0x0D）。

用途：对 BdDiagUart 诊断固件（Appli/build/BdDiagUart，开关 EMS04_BDDIAG_UART=1）
批量取"最近一次 CMD_REATT 的边界缓冲 + Q27 影子头"：每锚 = CMD_REATT（喂 hist/kf）
→ CMD_BDDIAG（取 h_past/n2out/n2kf/q27_crc）→ 落盘。**无需 halt/重载**（纯 UART）。

RSP_BDDIAG 载荷（666 B，小端）：
  off   0 seq u32 | off 4 q27_crc32 u32 | off 8 q27_valid u8(+3R)
  off  12 hpast 96×f32 | off 396 n2out 90×i8 | off 486 n2kf 180×i8

落盘布局（与既有 BD 腿读取口径一致）：{anchor}.672.bin =
  [0:90] n2out | [96:276] n2kf | [288:672] hpast（其余 0）
另落 {anchor}.meta.json（seq/crc/valid/q30×3/infer_us）。

用法（仓库根；.venv_tf）：
  python3 Host/tools/... 本脚本以 --anchors-dir 指向 inputs/anchors（含 <vid>/t0_*.bin）
  python3 .../s3c_bddiag_client.py --anchors-dir <dir> --outdir <dir> [--port /dev/cu.usbserial-1120]
  python3 .../s3c_bddiag_client.py --one <vid>:<t0> --anchors-dir <dir> --outdir <dir>   # 单锚（冒烟）
"""
from __future__ import annotations
import argparse, json, os, struct, sys, time
from pathlib import Path
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import s3c_reatt_replay as cl  # noqa: E402

CMD_BDDIAG, RSP_BDDIAG = 0x0C, 0x0D
RSP_LEN = 666


def bddiag(b, seq: int):
    """发 CMD_BDDIAG，返回 (q27_crc, q27_valid, hpast f32[96], n2out i8[90], n2kf i8[180])。"""
    b.ser.write(cl.enc_frame(CMD_BDDIAG, struct.pack("<I", seq)))
    t, p = b._read_frame(timeout_s=30.0)
    if t != RSP_BDDIAG:
        raise RuntimeError("RSP type 0x%02X != RSP_BDDIAG（NACK? payload=%s）" % (t, p[:4].hex()))
    if len(p) != RSP_LEN:
        raise RuntimeError("RSP_BDDIAG 长度 %d != %d" % (len(p), RSP_LEN))
    seq_e, crc, valid = struct.unpack_from("<IIB", p, 0)
    if seq_e != seq:
        raise RuntimeError("seq 回显 %d != %d" % (seq_e, seq))
    hpast = np.frombuffer(p[12:396], dtype="<f4").copy()
    n2out = np.frombuffer(p[396:486], dtype=np.int8).copy()
    n2kf = np.frombuffer(p[486:666], dtype=np.int8).copy()
    return crc, valid, hpast, n2out, n2kf


def pack672(n2out, n2kf, hpast) -> bytes:
    blk = bytearray(672)
    blk[0:90] = n2out.tobytes()
    blk[96:276] = n2kf.tobytes()
    blk[288:672] = hpast.tobytes()
    return bytes(blk)


def list_anchors(root: Path):
    out = []
    for vd in sorted(p for p in root.iterdir() if p.is_dir()):
        for h in sorted(vd.glob("t0_*.hist.f32.bin")):
            t0 = int(h.name.split(".")[0].split("_")[1])
            out.append((vd.name, t0, h, vd / ("t0_%05d.kf.f32.bin" % t0)))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--anchors-dir", required=True)
    ap.add_argument("--outdir", required=True)
    ap.add_argument("--port", default="/dev/cu.usbserial-1120")
    ap.add_argument("--one", default=None, help="VID:T0 单锚模式")
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    adir, odir = Path(args.anchors_dir), Path(args.outdir)
    odir.mkdir(parents=True, exist_ok=True)
    anchors = list_anchors(adir)
    if args.one:
        v, _, t0s = args.one.rpartition(":")
        anchors = [a for a in anchors if a[0] == v and a[1] == int(t0s)]
        assert anchors, "anchor not found"
    if args.limit:
        anchors = anchors[: args.limit]
    print("anchors:", len(anchors))

    b = cl.Board(args.port, 115200)
    b.ser.reset_input_buffer()
    ok = False
    for _ in range(12):
        try:
            if b.ping():
                ok = True
                break
        except Exception:
            pass
        time.sleep(0.5)
    if not ok:
        print("PING_FAIL"); b.close(); return 2

    lines = []
    for i, (v, t0, hp, kp) in enumerate(anchors):
        name = "%s_t0_%05d" % (v, t0)
        hist = np.fromfile(hp, dtype="<f4")
        kf = np.fromfile(kp, dtype="<f4")
        assert hist.size == 3300 and kf.size == 180
        q, us = b.reatt(hist, kf, i)
        crc, valid, hpast, n2out, n2kf = bddiag(b, i)
        (odir / (name + ".672.bin")).write_bytes(pack672(n2out, n2kf, hpast))
        json.dump({"seq": i, "vid": v, "t0": t0, "q27_crc32": "0x%08x" % crc,
                   "q27_valid": int(valid), "infer_us": int(us),
                   "q_f32": q.reshape(-1).tolist()},
                  open(odir / (name + ".meta.json"), "w"))
        lines.append(name)
        print("[%d/%d] %s q50_1=%.4f q27crc=0x%08x valid=%d us=%d"
              % (i + 1, len(anchors), name, q[0, 1], crc, valid, us), flush=True)
    b.close()
    json.dump({"n": len(lines), "anchors": lines}, open(odir / "BDLEG_MANIFEST.json", "w"), indent=1)
    print("BD_LEG_DONE", len(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())

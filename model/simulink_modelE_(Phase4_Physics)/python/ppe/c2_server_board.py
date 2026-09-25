"""C2 inference file-bridge server — BOARD backend (phase4-g soft-HIL).

与冻结件 ppe/c2_server.py 同文件协议（req/resp .mat 目录轮询，fail-closed）；
唯一差异 = 推理不在本进程（keras），而是转发到固件板的 CMD_REATT 服务（原始域
hist/kf 上板、归一化在固件内）：默认 EMS02_CHIL_REATT8_CPU（int8 全 CPU 形态）；
EMS03（N1 段链 + N2 NPU，Flash-boot XIP）形态经 --fw-project/--fw-tag 指定
（固件身份仅随 provenance 记录，协议不变）。

- 科学变换（H1 聚合 / kf 快照取行）复用冻结件只读导入：
  ppe.c2_server.kf_row_for + ppe.h1_assembly.h1_from_raw_logs（零复制零改动）；
- 每请求全量记录 board_queries.jsonl（H1/kf 原始字节 + 板 q50 + 时延），
  供事后 keras fp32 逐锚点重放对拍（L3 证据链）；
- provenance：keras_sha256 = FR836 冻结源权重身份（板 int8 的量化源），
  backend = 'board_int8'，tflite_sha256 取固件工程 quant_report.json；
- --mock：无板联调模式——以 host TFLite int8 解释器模拟板响应（仅验证
  传输链，产物标 MOCK，不作板级证据）。

Security/scope: only reads Dataset-v3 formal iid dir（kf 快照，与冻结
c2_server 相同面）；never test/OOD。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time

import numpy as np
from scipy.io import loadmat, savemat

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from ppe import ledger, paths  # noqa: E402
from ppe.c2_inference import FR836  # noqa: E402  (身份常量只读复用)
from ppe.c2_server import kf_row_for  # noqa: E402  (冻结件只读复用)
from ppe.causality import REQUIRED_F, REQUIRED_H1, REQUIRED_K  # noqa: E402
from ppe.h1_assembly import h1_from_raw_logs  # noqa: E402

KF_ORDER = REQUIRED_K + REQUIRED_F
H1_ORDER = REQUIRED_H1
FW_PROJECT = os.path.normpath(os.path.join(
    _MODELE, "..", "..", "firmware", "stm32n6", "04_SourceCode", "SoftwarePackage",
    "EMS_Projects", "EMS02_CHIL_REATT8_CPU"))
FW_QUANT_REPORT = os.path.join(
    FW_PROJECT, "Host", "results", "reatt_board", "quant_report.json")
FW_TFLITE = os.path.join(
    FW_PROJECT, "Model", "reatt8_q", "reatt_raw_int8.tflite")

SYNC0, SYNC1 = 0xAA, 0x55
CMD_PING, CMD_REATT, RSP_REATT = 0x06, 0x0A, 0x0B
ACK, NACK = 0x7F, 0x7E


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def enc_frame(ftype: int, payload: bytes) -> bytes:
    import struct
    body = bytes([ftype]) + struct.pack("<H", len(payload)) + payload
    return bytes([SYNC0, SYNC1]) + body + struct.pack("<H", crc16_ccitt(body))


class BoardLink:
    """CMD_REATT 串口客户端（协议同固件工程 Host/tools/reatt_board_client.py）。"""

    def __init__(self, port: str, baud: int = 115200):
        import serial
        self.ser = serial.Serial(port, baud, timeout=10.0)
        self.rxbuf = bytearray()

    def close(self):
        self.ser.close()

    def _read_frame(self, timeout_s: float = 60.0):
        import struct
        t0 = time.time()
        while time.time() - t0 < timeout_s:
            chunk = self.ser.read(4096)
            if chunk:
                self.rxbuf.extend(chunk)
            while len(self.rxbuf) >= 5:  # 同步2+TYPE1+LEN2
                if self.rxbuf[0] != SYNC0 or self.rxbuf[1] != SYNC1:
                    self.rxbuf.pop(0)
                    continue
                ln = struct.unpack("<H", self.rxbuf[3:5])[0]  # [AA 55][TYPE][LEN LE16]
                if len(self.rxbuf) < 5 + ln + 2:
                    break
                body = bytes(self.rxbuf[2:5 + ln])  # TYPE+LEN+PAYLOAD（CRC 作用域 TYPE..PAYLOAD）
                crc_rx = struct.unpack("<H", self.rxbuf[5 + ln:7 + ln])[0]
                del self.rxbuf[:7 + ln]
                if crc16_ccitt(body) != crc_rx:
                    continue
                return body[0], body[3:]  # payload（剥 TYPE+LEN）
        raise TimeoutError("板响应超时")

    def ping(self) -> bool:
        self.ser.reset_input_buffer()
        self.rxbuf.clear()
        self.ser.write(enc_frame(CMD_PING, b""))
        t, p = self._read_frame()
        return t == ACK and p and p[0] == CMD_PING

    def reatt(self, hist_raw, kf_raw, seq: int):
        import struct
        payload = struct.pack("<I", seq) \
            + np.ascontiguousarray(hist_raw, dtype="<f4").tobytes() \
            + np.ascontiguousarray(kf_raw, dtype="<f4").tobytes()
        self.ser.write(enc_frame(CMD_REATT, payload))
        while True:
            t, p = self._read_frame()
            if t == NACK:
                raise RuntimeError(f"board NACK err={p[1]:#x}")
            if t == RSP_REATT and len(p) == 4 + 90 * 4 + 4:
                (rseq,) = struct.unpack("<I", p[:4])
                if rseq != seq:
                    continue
                q = np.frombuffer(p[4:4 + 90 * 4], dtype="<f4").copy()
                (us,) = struct.unpack("<I", p[4 + 90 * 4:])
                return q.reshape(30, 3), us


class MockBoard:
    """host TFLite int8 板模拟器（联调用；产物必须标 MOCK）。"""

    def __init__(self, tflite_path: str = FW_TFLITE):
        import tensorflow as tf  # noqa: 延迟导入（仅 mock 模式需要）
        tfl = open(tflite_path, "rb").read()
        self.inter = tf.lite.Interpreter(model_content=tfl)
        self.inter.allocate_tensors()
        self.ind = self.inter.get_input_details()
        self.outd = self.inter.get_output_details()

    def close(self):
        pass

    def ping(self) -> bool:
        return True

    def reatt(self, hist_raw, kf_raw, seq: int):
        self.inter.set_tensor(self.ind[0]["index"], hist_raw[None].astype(np.float32))
        self.inter.set_tensor(self.ind[1]["index"], kf_raw[None].astype(np.float32))
        self.inter.invoke()
        raw = self.inter.get_tensor(self.outd[0]["index"])[0].astype(np.float64)
        # 复刻固件后处理（ems_reatt.c 稳定 softplus 单调分位变换）
        ax = np.abs(raw[:, 1:])
        sp = np.maximum(raw[:, 1:], 0.0) + np.log1p(np.exp(-ax))
        q = np.stack([raw[:, 0] - sp[:, 0], raw[:, 0], raw[:, 0] + sp[:, 1]],
                     axis=-1)
        return q.astype(np.float32), 0


def board_normalizer():
    """固件归一化的 host 参考实现（仅 mock 路径需要；真板在固件内归一化）。"""
    from predictor_v3 import features, norm_ext
    formal_norm = features.load_formal_norm()
    ne = norm_ext.load_artifact()

    def norm(hist_raw, kf_raw):
        h = np.zeros_like(hist_raw, dtype=np.float32)
        k = np.zeros_like(kf_raw, dtype=np.float32)
        for j, c in enumerate(H1_ORDER):
            m = np.asarray(formal_norm["x_h1_mean"][c], dtype=np.float64)
            s = np.asarray(formal_norm["x_h1_std"][c], dtype=np.float64)
            h[:, j] = ((hist_raw[:, j].astype(np.float64) - m) / s).astype(np.float32)
        for j, c in enumerate(KF_ORDER):
            t = norm_ext.transform_channel(ne, c, kf_raw[:, j].astype(np.float64))
            k[:, j] = np.asarray(t, dtype=np.float32)
        return h, k

    return norm


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reqdir", required=True)
    ap.add_argument("--respdir", required=True)
    ap.add_argument("--stop-file", default="")
    ap.add_argument("--port", default="")
    ap.add_argument("--baud", type=int, default=115200)  # bring-up：与固件一致（CH340 921600 不可靠）
    ap.add_argument("--poll-s", type=float, default=0.2)
    ap.add_argument("--max-requests", type=int, default=0)
    ap.add_argument("--mock", action="store_true",
                    help="无板联调：host int8 模拟板（产物标 MOCK，非证据）")
    ap.add_argument("--fw-project", default="",
                    help="固件工程目录（缺省=内置 EMS02_CHIL_REATT8_CPU 常量；"
                         "EMS03 给该工程路径，quant_report/tflite 缺失则留空继续）")
    ap.add_argument("--fw-tag", default="",
                    help="固件形态标签（自由文本，如 ems03_split_npu_flash_xip）")
    ap.add_argument("--fw-hex-sha256", default="",
                    help="烧录镜像 HEX 的 sha256（仅随 provenance 记录，不校验）")
    args = ap.parse_args(argv)

    # 固件身份（provenance）：默认 EMS02 常量；--fw-project 给定时替换推导，
    # 两个产物文件允许缺失（取不到留空继续，不阻断服务）。
    fw_project = (os.path.normpath(args.fw_project) if args.fw_project
                  else FW_PROJECT)
    fw_quant_report = os.path.join(
        fw_project, "Host", "results", "reatt_board", "quant_report.json")
    fw_tflite = os.path.join(
        fw_project, "Model", "reatt8_q", "reatt_raw_int8.tflite")
    fw_prov = {}
    if args.fw_project:
        fw_prov["fw_project"] = os.path.basename(fw_project)
    if args.fw_tag:
        fw_prov["fw_tag"] = args.fw_tag
    if args.fw_hex_sha256:
        fw_prov["fw_hex_sha256"] = args.fw_hex_sha256

    os.makedirs(args.respdir, exist_ok=True)
    if args.mock:
        board = MockBoard(fw_tflite)
        normalizer = board_normalizer()
        backend = "mock_host_int8"
    else:
        if not args.port:
            raise SystemExit("--port 必填（或用 --mock）")
        # bring-up 裁决（2026-09-22）：每请求独立 worker 子进程 + 新开端口。
        # 持口服务器在 macOS/CH340 上不可靠（MATLAB 冷启动窗口争用、长空闲
        # 设备静默）；新开端口模式在板级验证中 100% 可靠（L1 对拍 64/64）。
        board = None
        normalizer = None  # 真板：归一化在固件内
        backend = "board_int8"
        # 启动零板交互（bring-up：MATLAB 冷启动窗口串口不可用，启动期碰板必败）；
        # 板交互完全延迟到首个请求（worker 内含 ping+重试，fail-closed 每请求）。

    tfl_sha = ""
    try:  # EMS03 形态下报告可能缺失/不完整：取不到留空继续
        if os.path.isfile(fw_quant_report):
            qr = json.load(open(fw_quant_report))
            tfl_sha = qr["provenance"]["tflite_int8_sha256"]
    except (OSError, ValueError, KeyError):
        tfl_sha = ""

    fw_note = ""
    if fw_prov:
        fw_note = (f" fw_project={fw_prov.get('fw_project', '')}"
                   f" fw_tag={fw_prov.get('fw_tag', '')}"
                   f" fw_hex_sha256={fw_prov.get('fw_hex_sha256', '')[:12]}")
    ledger.log_access(
        artifact="c2_server_board", purpose="QUALIFICATION",
        split_contact="none",
        note=f"phase4-g soft-HIL board backend start "
             f"reqdir={os.path.basename(args.reqdir)} "
             f"backend={backend} tflite={tfl_sha[:12]}"
             f"{fw_note}（非正式；purpose 枚举为"
             f"冻结件 ppe/ledger.py 封闭集，借 QUALIFICATION 槽位 + note 区分）")

    # 每 token 一份查询日志：artifacts/ppe_softhil/<token>/board_queries.jsonl
    log_path = os.path.join(
        os.path.dirname(os.path.normpath(args.respdir).rstrip(os.sep)),
        "board_queries.jsonl")
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    logf = open(log_path, "a", encoding="utf-8")

    done = set()
    n = 0
    last_keepalive = time.time()
    while True:
        if args.stop_file and os.path.exists(args.stop_file):
            break
        for fn in sorted(os.listdir(args.reqdir)):
            if not fn.startswith("req_") or not fn.endswith(".mat") or fn in done:
                continue
            req_path = os.path.join(args.reqdir, fn)
            resp_path = os.path.join(args.respdir, "resp_" + fn[4:])
            try:
                req = loadmat(req_path, simplify_cells=True)
                vid = str(req["voyage_id"])
                t0 = float(req["t0"])
                t = np.asarray(req["t"], dtype=float).ravel()
                cols = {c: np.asarray(req[f"H1_{c}"], dtype=float).ravel()
                        for c in H1_ORDER}
                H1 = h1_from_raw_logs(t, cols, t0)          # 冻结聚合（科学变换留 host）
                hist_raw = np.asarray(H1, dtype=np.float32)  # [300,11] 原始域
                kf_raw = None
                kf = kf_row_for(vid, t0)                     # 冻结快照取行（数据面留 host）
                kf_raw = np.stack([np.asarray(kf[c], dtype=np.float32)
                                   for c in KF_ORDER], axis=-1)  # [30,6] 原始域
                t_q = time.time()
                if args.mock:
                    h_n, k_n = normalizer(hist_raw, kf_raw)
                    q3, us = board.reatt(h_n, k_n, 0)
                else:
                    import subprocess as _sp
                    npz_tmp = os.path.join(
                        os.path.dirname(os.path.normpath(args.respdir)), "_q_tmp.npz")
                    np.savez(npz_tmp, hist_raw=hist_raw.astype("<f4"),
                             kf_raw=kf_raw.astype("<f4"))
                    jr = {"ok": False, "error": "no attempt"}
                    for _wa in range(4):  # 请求内重试（桥超时 300s 合同内）
                        r = _sp.run([sys.executable, "-m", "ppe.reatt_query_once",
                                     "--port", args.port, "--baud", str(args.baud),
                                     "--npz", npz_tmp],
                                    capture_output=True, text=True, timeout=120)
                        lines = r.stdout.strip().splitlines()
                        jr = json.loads(lines[-1]) if lines else {"ok": False,
                                                                  "error": "no output"}
                        if jr.get("ok"):
                            break
                        time.sleep(3.0)
                    if not jr.get("ok"):
                        raise RuntimeError(f"worker: {jr.get('error')}")
                    q3 = np.asarray(jr["q"], dtype=float)
                    us = int(jr["infer_us"])
                dt_ms = (time.time() - t_q) * 1000.0
                q50 = q3[:, 1].astype(float)
                prov = {
                    "keras_sha256": FR836["keras_sha256"],   # 源权重身份（量化源）
                    "backend": backend,
                    "tflite_sha256": tfl_sha,
                    "board_infer_us": int(us),
                    "roundtrip_ms": round(dt_ms, 1),
                    "voyage_id": vid, "t0": t0,
                    "h1_sha256": hashlib.sha256(
                        np.ascontiguousarray(hist_raw, dtype="<f4").tobytes()
                    ).hexdigest(),
                    "kf_sha256": hashlib.sha256(
                        np.ascontiguousarray(kf_raw, dtype="<f4").tobytes()
                    ).hexdigest(),
                }
                prov.update(fw_prov)  # EMS03 固件身份（缺省为空 → 输出不变）
                rec = {
                    "voyage_id": vid, "t0": t0, "backend": backend,
                    "board_infer_us": int(us), "roundtrip_ms": round(dt_ms, 1),
                    "hist_raw_hex": np.ascontiguousarray(
                        hist_raw, dtype="<f4").tobytes().hex(),
                    "kf_raw_hex": np.ascontiguousarray(
                        kf_raw, dtype="<f4").tobytes().hex(),
                    "q50": q50.tolist(),
                }
                rec.update(fw_prov)
                logf.write(json.dumps(rec) + "\n")
                logf.flush()
                savemat(resp_path, {"Pbar_q50": q50.reshape(-1, 1),
                                    "provenance": prov})
            except Exception as exc:  # noqa: BLE001 — fail-closed w/ record
                savemat(resp_path, {"error": f"{type(exc).__name__}: {exc}"})
            done.add(fn)
            n += 1
            if args.max_requests and n >= args.max_requests:
                return 0
        time.sleep(args.poll_s)
    logf.close()
    if board is not None:  # 真板模式 board=None（交互延迟到 worker），防御 stop-file 退出
        board.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

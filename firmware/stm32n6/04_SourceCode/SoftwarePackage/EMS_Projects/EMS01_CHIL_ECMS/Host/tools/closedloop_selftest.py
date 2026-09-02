#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""closedloop_selftest.py — T-608：soft-HIL 闭环 harness selftest 套件（AC-603）
（spec: ems01-chil-v3-closedloop，FR-606/FR-607/AC-603；design §6.5 selftest 定案：
 pty mock 板（复用 ems_fault_inject mock 模式）+ 推进器回放模式；覆盖帧配对、
 超时语义、事件环、记录列完整性、推进器接口、终止条件；G-P1/G-P2 可无板执行，
 板会话前必须全绿）。

用例矩阵（每用例独立断言 + 独立产物子目录，互不污染）:
  A replay_stub_baseline       replay+stub 30 拍基线（CLI 子进程）：19 列表头逐字符
                               一致、meta 必备字段齐、status=completed、termination=
                               tick_count_reached、row0 与 start_ic/ref row0 同口径、
                               P_gen 列 == ref_G2 回放序列、RSP 遥测 6 列 = NaN（replay
                               语义）、退出码 0；
  B board_mockfw_pairing       pty mock 板（复用 ems_fault_inject._MockFW）+ board 模式
                               20 拍（CLI 子进程）：mode=board、RSP 遥测列全有限且等于
                               mock 确定性规则（P_gen 网格化/e_hat/K/cls/us 模式）、
                               meta events==[]（IDX_MISMATCH/TIMEOUT 零发生 = idx 配对
                               零错位）、mock 侧恰收 20 帧 CMD_TICK、status=completed；
  C rsp_timeout_abort          mock 板对 CMD_TICK 静默（PING/RESET 正常应答）→ harness
                               在 rsp-timeout-ms 后 abort：TIMEOUT 事件@tick0 + <csv>
                               .aborted 存在（0 数据行）+ meta status=aborted + 正式
                               CSV 不存在 + 退出码 1；
  D voyage_stop_termination    伪造推进器（monkeypatch make_advancer，不动 harness）第 4
                               拍置位 voyage_stop → VOYAGE_STOP 事件@tick3 + termination
                               _reason=voyage_stop + status=completed + 正式 CSV 落盘；
  E1 advancer_stub_smoke       make_advancer('stub') 3 拍接口冒烟：11 键齐/全有限/t_s 整
                               秒/fuel 递增/state() 7 键/init() 复位 IC/close()；
  E2 advancer_ctypes_smoke     make_advancer('ctypes') 3 拍接口冒烟（真实 dylib）；dylib
                               路径存在性前置检查，缺失则 SKIP（不判 FAIL，汇总标注）；
  F midrun_silent_redo         mock 板第 10 拍后停发 → 整跑重做（OQ-4/FD5）：.aborted
                               恰含 10 已收集行 + TIMEOUT 事件@tick10 + 正式 CSV 不存在
                               + 退出码 1；
  G idx_mismatch_guard         mock 板回 idx+1 → IDX_MISMATCH 事件@tick0 + aborted +
                               退出码 1（配对守卫负例）；
  H plantstep_abnormal_event   伪造推进器第 2 拍抛 PlantStepError → PLANT_STEP_ABNORMAL
                               事件@tick1 + .aborted 1 行 + 正式 CSV 不存在；
  I builtin_selftest_parity    ems_closedloop --selftest（T-607 内置自检，replay+stub
                               30 拍）rc=0 平价对照——本套件为其超集。

机器可读结果:
  - 每用例 PASS / SKIP / FAIL（逐断言 [OK]/[FAIL] 行）；
  - stdout 末行汇总 `CLOSEDLOOP SELFTEST PASS|FAIL`；退出码 0/1；
  - --json 追加结果 JSON（用例名/结果/耗时/断言详情；打印于汇总行之前，不破坏
    "stdout 末行 = 汇总"契约）。

隔离与幂等:
  - 全部产物进 tempfile.mkdtemp（每 run 一个根，每用例一个子目录）；overall 无 FAIL
    时自动清理，有 FAIL 时保留并打印路径供排查；
  - 对 Host/ 目录零写入（dylib/ref CSV 只读）；幂等可重复执行。

运行方式:
  python3 tools/closedloop_selftest.py                       # 全矩阵（A..I）
  python3 tools/closedloop_selftest.py --backend ctypes      # 仅 ctypes 推进器用例
  python3 tools/closedloop_selftest.py --backend stub        # 仅 stub 推进器用例
  python3 tools/closedloop_selftest.py --json                # 附加结果 JSON 输出
  （--backend 仅过滤 E1/E2 推进器后端用例；A..D/F..I 为机制用例，固定 stub）

环境: 仓库 model/simulink/.venv_tf（pyserial 在内——board 模式用例需要；
缺失时 board 用例 SKIP 并标注，replay/推进器用例不受影响）。纯 stdlib + pty +
本目录 import；无真实串口/真实板操作。

纪律: 协议帧实现 import 自 Host/tests/test_proto_loopback.py（冻结单一来源）；
mock 板复用 ems_fault_inject._MockFW（仅子类化注入静默/错位行为，不改冻结实现）；
本套件只 import 复用 harness（ems_closedloop.py），发现 harness 缺陷须 Escalate
上报而非修改；路径全部由 __file__ 相对推导，禁绝对路径（AGENTS §20.2）。
"""
import argparse
import contextlib
import csv
import io
import json
import math
import os
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))            # .../EMS01/Host/tools
HOST = os.path.dirname(HERE)                                 # .../EMS01/Host
TESTS_DIR = os.path.join(HOST, "tests")
EMS_CLOSEDLOOP_CLI = os.path.join(HOST, "ems_closedloop.py")

sys.path.insert(0, TESTS_DIR)
sys.path.insert(0, HOST)

# 协议帧实现（冻结单一来源）+ mock 板复用 + harness/推进器复用（勿双实现）
from test_proto_loopback import build_frame, enc_rsp_tick, RSP_TICK  # noqa: E402
from ems_inject import GRID_STEP_KW  # noqa: E402
from ems_fault_inject import (  # noqa: E402
    _MockFW, MOCK_P_SCALE, FL_STALE, FL_HELD)
from ems_closedloop import (  # noqa: E402
    RECORD_COLS, META_REQUIRED_FIELDS, REPO_ROOT, _f32, load_ref_pgen,
    run_closedloop, main as closedloop_main)
from plantstep_advancer import (  # noqa: E402
    make_advancer, PlantStepError, OUT_KEYS, STATE_KEYS,
    STUB_HOTEL_KW, PLANTSTEP_DIR)

FL_STALE_HELD = FL_STALE | FL_HELD          # mock STALE 分支 flags（pty 节拍偶发慢）
CASE_TIMEOUT_S = 120                        # 单用例（子进程）墙钟上限


# ============================================================ 小工具
def _tail(s, n=400):
    s = (s or "").strip()
    return s[-n:] if len(s) > n else s


def _finite(v):
    try:
        return math.isfinite(float(v))
    except (TypeError, ValueError):
        return False


class _CliResult(object):
    def __init__(self, returncode, stdout, stderr):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def _run_cli(argv_extra, timeout_s=CASE_TIMEOUT_S):
    """子进程跑 ems_closedloop.py CLI（真实退出码/真实产物路径；cwd=Host）。"""
    cmd = [sys.executable, EMS_CLOSEDLOOP_CLI] + list(argv_extra)
    try:
        cp = subprocess.run(cmd, cwd=HOST, capture_output=True, text=True,
                            timeout=timeout_s)
        return _CliResult(cp.returncode, cp.stdout, cp.stderr)
    except subprocess.TimeoutExpired:
        return _CliResult(-999, "", "subprocess 超时（>%ds）" % timeout_s)


def _read_csv_rows(path):
    """CSV → (header list, data rows list)。文件缺失 → (None, None)。"""
    if not os.path.isfile(path):
        return None, None
    with open(path, newline="") as f:
        rr = list(csv.reader(f))
    return (rr[0] if rr else []), rr[1:]


def _load_meta(path):
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _harness_args(out_dir, scenario="S20", image="replay", backend="stub",
                  max_ticks=30, rsp_timeout_ms=500):
    """构造 run_closedloop 所需 args 命名空间（in-process 用例 D/H）。"""
    ns = argparse.Namespace()
    ns.scenario, ns.port, ns.image, ns.advancer = scenario, None, image, backend
    ns.out, ns.runs_dir = out_dir, None
    ns.rsp_timeout_ms, ns.max_ticks = rsp_timeout_ms, max_ticks
    return ns


def _board_cli_args(port, out_dir, max_ticks, rsp_timeout_ms):
    """board 模式 CLI 参数（mock 板 + stub 推进器，机制断言与 dylib 解耦）。"""
    return ["--scenario", "S20", "--image", "npu", "--advancer", "stub",
            "--port", port, "--max-ticks", str(max_ticks),
            "--rsp-timeout-ms", str(rsp_timeout_ms), "--out", out_dir]


def _has_event(meta, etype, tick=None):
    """meta['events'] 含指定类型（可选 tick）事件 → bool。"""
    for ev in meta.get("events", []):
        if ev.get("type") == etype and (tick is None or ev.get("tick") == tick):
            return True
    return False


# ============================================================ pty mock 板机制
def _mock_board_worker(fd, board, stop):
    """pty master 收包线程（镜像 ems_fault_inject._mock_worker 模式：5ms 级轮询；
    master 设非阻塞 + select 50ms，消除阻塞读与 close 的竞态）。"""
    while not stop.is_set():
        try:
            r, _, _ = select.select([fd], [], [], 0.05)
            if not r:
                continue
            chunk = os.read(fd, 512)
        except OSError:
            stop.wait(0.005)
            continue
        if chunk:
            try:
                board.feed(chunk)
            except Exception as exc:  # noqa: BLE001 —— 线程内异常留证供诊断
                board.feed_error = board.feed_error or repr(exc)


class _MockBoard(object):
    """pty mock 板生命周期（ems_fault_inject --selftest 同款机制）：
    openpty → slave 设 RAW（cooked 模式 OPOST/ICANON 损坏二进制帧）→ master
    非阻塞 → worker 线程收 master 字节喂 board；`board.port` 供 harness
    --port 使用（pyserial 按名打开 slave）。with 语义退出时停线程并关 fd。"""

    def __init__(self, board_factory):
        self._factory = board_factory
        self.board = None
        self._ma = self._sl = None
        self._stop = None
        self._th = None

    def __enter__(self):
        import pty
        import tty
        self._ma, self._sl = pty.openpty()
        tty.setraw(self._sl)                 # pty 必须 RAW（二进制帧）
        os.set_blocking(self._ma, False)
        self.board = self._factory(self._ma)
        self.board.feed_error = None
        self._stop = threading.Event()
        self._th = threading.Thread(
            target=_mock_board_worker,
            args=(self._ma, self.board, self._stop), daemon=True)
        self._th.start()
        return self

    @property
    def port(self):
        return os.ttyname(self._sl)

    def __exit__(self, *exc):
        self._stop.set()
        self._th.join(timeout=1.0)
        for fd in (self._sl, self._ma):
            try:
                os.close(fd)
            except OSError:
                pass
        return False


class _CountingMockFW(_MockFW):
    """_MockFW 计数子类（case B）：数收到的 CMD_TICK 帧数（帧配对 mock 侧证据）。"""

    def __init__(self, fd):
        _MockFW.__init__(self, fd)
        self.n_tick_rx = 0

    def _tick(self, pay, now):
        self.n_tick_rx += 1
        _MockFW._tick(self, pay, now)


class _SilentAfterMockFW(_MockFW):
    """_MockFW 变体（case C/F）：前 reply_ticks 拍正常应答，其后 CMD_TICK 静默
    （不回 RSP_TICK → harness 事务超时）。reply_ticks=0 即全程静默。"""

    def __init__(self, fd, reply_ticks):
        _MockFW.__init__(self, fd)
        self.reply_ticks = int(reply_ticks)
        self.n_tick_rx = 0
        self.n_replied = 0

    def _tick(self, pay, now):
        self.n_tick_rx += 1
        if self.n_replied >= self.reply_ticks:
            return                           # 静默：不回 RSP_TICK
        self.n_replied += 1
        _MockFW._tick(self, pay, now)


class _WrongIdxMockFW(_MockFW):
    """_MockFW 变体（case G）：RSP_TICK idx 恒偏 +1（配对守卫负例）。"""

    def __init__(self, fd):
        _MockFW.__init__(self, fd)
        self.n_tick_rx = 0

    def _tick(self, pay, now):
        self.n_tick_rx += 1
        idx = struct.unpack_from("<I", pay, 0)[0]
        self._emit(build_frame(RSP_TICK, enc_rsp_tick(
            idx + 1, 200.0, 10.0, 0.75, 3, 1000, 2000, 0)))


# ============================================================ 用例框架
class _CaseSkip(Exception):
    """用例环境性跳过（dylib 缺失 / pyserial 缺失 / --backend 过滤），不判 FAIL。"""


def _mock_expected_pgen(p_load):
    """mock 固件确定性 P_gen 规则（镜像 ems_fault_inject._MockFW 正常分支）:
    P_gen = floor(P_load × MOCK_P_SCALE / GRID_STEP_KW) × GRID_STEP_KW。"""
    return math.floor(float(p_load) * MOCK_P_SCALE / GRID_STEP_KW) * GRID_STEP_KW


def case_a_replay_stub_baseline(ctx):
    """A：replay+stub 30 拍基线（CLI 子进程，产物完整性 + 退出码）。"""
    out = ctx["case_dir"]("a_replay_stub_baseline")
    p = _run_cli(["--scenario", "S20", "--image", "replay", "--advancer", "stub",
                  "--max-ticks", "30", "--out", out, "--rsp-timeout-ms", "500"])
    details = ["cmd: ems_closedloop --scenario S20 --image replay --advancer stub "
               "--max-ticks 30 --out <tmp>"]
    A = [("CLI 退出码 0", p.returncode == 0)]
    if p.returncode != 0:
        A.append(("stdout 尾部: %r" % _tail(p.stdout), False))
        A.append(("stderr 尾部: %r" % _tail(p.stderr), False))
        return A, details
    csv_path = os.path.join(out, "closedloop_S20_replay.csv")
    meta_path = os.path.join(out, "closedloop_S20_replay_meta.json")
    header, rows = _read_csv_rows(csv_path)
    meta = _load_meta(meta_path)
    col = {c: i for i, c in enumerate(RECORD_COLS)}
    A.append(("CSV 产物存在", header is not None))
    A.append(("meta 产物存在", meta is not None))
    if header is None or meta is None:
        return A, details
    A.append(("19 列表头逐字符一致", tuple(header) == tuple(RECORD_COLS)))
    A.append(("数据行数=30", len(rows) == 30))
    A.append(("每行 19 字段", all(len(r) == len(RECORD_COLS) for r in rows)))
    t_col = [float(r[col["t_s"]]) for r in rows]
    A.append(("t_s 单调 +1（stub 整秒语义，t[k]=k）",
              all(abs(t_col[i] - i) < 1e-9 for i in range(len(t_col)))))
    ref_pgen = load_ref_pgen(meta["ref_csv"])
    A.append(("P_gen 列 == ref_G2 回放序列（逐拍）",
              all(abs(float(rows[k][col["P_gen_kW"]]) - ref_pgen[k]) < 1e-9
                  for k in range(30))))
    A.append(("RSP 遥测 6 列 = NaN（replay 语义）",
              all(not _finite(rows[k][col[c]])
                  for k in range(30)
                  for c in ("e_hat", "K", "cls_eff", "flags",
                            "infer_us", "ctrl_us"))))
    A.append(("row0 P_load == f32(start_ic.P_load_kW)（hotel，线上量化值）",
              float(rows[0][col["P_load_kW"]])
              == _f32(meta["start_ic"]["P_load_kW"])))
    A.append(("row0 SOC == f32(start_ic.SOC)（Bat_SOC_init，f32 量化口径）",
              float(rows[0][col["SOC"]]) == _f32(meta["start_ic"]["SOC"])))
    A.append(("row0 v_ship=accel=x_ship=0",
              float(rows[0][col["v_ship_mps"]]) == 0.0
              and float(rows[0][col["accel_mps2"]]) == 0.0
              and float(rows[0][col["x_ship_m"]]) == 0.0))
    A.append(("row0 v_cmd == f32(start_ic.v_cmd_mps)（VOYAGE 表首值）",
              float(rows[0][col["v_cmd_mps"]])
              == _f32(meta["start_ic"]["v_cmd_mps"])))
    A.append(("fuel 列逐拍严格递增（推进器活着）",
              all(float(rows[k][col["fuel_total_kg"]])
                  < float(rows[k + 1][col["fuel_total_kg"]]) for k in range(29))))
    missing = [k for k in META_REQUIRED_FIELDS if k not in meta]
    A.append(("meta 必备字段齐（%d 个）" % len(META_REQUIRED_FIELDS),
              not missing))
    A.append(("status=completed", meta["status"] == "completed"))
    A.append(("termination=tick_count_reached",
              meta["termination_reason"] == "tick_count_reached"))
    A.append(("mode=replay / image=replay / backend=stub",
              meta["mode"] == "replay" and meta["image"] == "replay"
              and meta["backend"] == "stub"))
    A.append(("rows=30 / n_ticks_planned=30",
              meta["rows"] == 30 and meta["n_ticks_planned"] == 30))
    A.append(("meta events 为空（无错误事件）", meta["events"] == []))
    A.append(("meta tick_time 分布齐（mean/p50/p95/max/n）",
              all(meta["tick_time"].get(k) is not None
                  for k in ("mean", "p50", "p95", "max"))
              and meta["tick_time"].get("n") == 30))
    A.append(("start_ic 与 ref row0 同口径（P_load=100/SOC=0.6）",
              abs(meta["start_ic"]["P_load_kW"] - 100.0) < 1e-9
              and abs(meta["start_ic"]["SOC"] - 0.6) < 1e-9))
    A.append(("plantstep.backend=stub 且 dylib=None",
              meta["plantstep"]["backend"] == "stub"
              and meta["plantstep"]["dylib"] is None))
    A.append(("ref_csv 存在", os.path.isfile(meta["ref_csv"])))
    return A, details


def case_b_board_mockfw_pairing(ctx):
    """B：pty mock 板（真复用 _MockFW）+ board 模式 20 拍：RSP 遥测/idx 配对。"""
    if not ctx["serial_ok"]:
        raise _CaseSkip("pyserial 不可用（board 模式需 pyserial；请用 .venv_tf 运行）")
    out = ctx["case_dir"]("b_board_mockfw_pairing")
    with _MockBoard(_CountingMockFW) as mb:
        p = _run_cli(_board_cli_args(mb.port, out, max_ticks=20,
                                     rsp_timeout_ms=1000))
    details = ["cmd: ems_closedloop --image npu --port <pty> --max-ticks 20 "
               "--rsp-timeout-ms 1000（mock = ems_fault_inject._MockFW）"]
    A = [("CLI 退出码 0", p.returncode == 0)]
    if p.returncode != 0:
        A.append(("stdout 尾部: %r" % _tail(p.stdout), False))
        A.append(("stderr 尾部: %r" % _tail(p.stderr), False))
        return A, details
    csv_path = os.path.join(out, "closedloop_S20_npu.csv")
    meta_path = os.path.join(out, "closedloop_S20_npu_meta.json")
    header, rows = _read_csv_rows(csv_path)
    meta = _load_meta(meta_path)
    col = {c: i for i, c in enumerate(RECORD_COLS)}
    A.append(("CSV/meta 产物存在", header is not None and meta is not None))
    if header is None or meta is None:
        return A, details
    A.append(("19 列表头逐字符一致", tuple(header) == tuple(RECORD_COLS)))
    A.append(("数据行数=20", len(rows) == 20))
    A.append(("mode=board / image=npu", meta["mode"] == "board"
              and meta["image"] == "npu"))
    A.append(("status=completed / termination=tick_count_reached",
              meta["status"] == "completed"
              and meta["termination_reason"] == "tick_count_reached"))
    A.append(("meta events 为空（IDX_MISMATCH/TIMEOUT 零发生 = idx 配对零错位）",
              meta["events"] == []))
    A.append(("mock 侧恰收 20 帧 CMD_TICK（每拍一帧，无重发/漂移）",
              mb.board.n_tick_rx == 20))
    A.append(("RSP 遥测 6 列全有限（非 NaN，板上解析值被记录）",
              all(_finite(rows[k][col[c]])
                  for k in range(len(rows))
                  for c in ("e_hat", "K", "cls_eff", "flags",
                            "infer_us", "ctrl_us"))))
    exp_p = _mock_expected_pgen(100.0)       # stub: P_load≡hotel=100 → 网格化 P
    A.append(("P_gen 列 == mock 网格化规则（P=100 → %.0f kW）" % exp_p,
              all(abs(float(rows[k][col["P_gen_kW"]]) - exp_p) < 1e-9
                  for k in range(len(rows)))))
    A.append(("e_hat/K/cls == mock 确定性值（%.3g / 0.75 / 3）"
              % round(exp_p * 0.05, 3),
              all(abs(float(rows[k][col["e_hat"]]) - round(exp_p * 0.05, 3)) < 1e-9
                  and abs(float(rows[k][col["K"]]) - 0.75) < 1e-9
                  and abs(float(rows[k][col["cls_eff"]]) - 3.0) < 1e-9
                  for k in range(len(rows)))))
    A.append(("infer_us/ctrl_us == mock 模式值（1000+k / 2000+k）",
              all(abs(float(rows[k][col["infer_us"]]) - (1000 + k)) < 1e-9
                  and abs(float(rows[k][col["ctrl_us"]]) - (2000 + k)) < 1e-9
                  for k in range(len(rows)))))
    A.append(("flags 列 ∈ {0, STALE|HELD}（pty 节拍偶发 >50ms 时允许 STALE）",
              all(float(rows[k][col["flags"]]) in (0.0, float(FL_STALE_HELD))
                  for k in range(len(rows)))))
    A.append(("P_load 列 == f32(100)（stub hotel，线上量化值）",
              all(float(rows[k][col["P_load_kW"]]) == _f32(100.0)
                  for k in range(len(rows)))))
    return A, details


def case_c_rsp_timeout_abort(ctx):
    """C：mock 对 CMD_TICK 静默 → TIMEOUT 事件 + .aborted + 退出码 1。"""
    if not ctx["serial_ok"]:
        raise _CaseSkip("pyserial 不可用（board 模式需 pyserial；请用 .venv_tf 运行）")
    out = ctx["case_dir"]("c_rsp_timeout_abort")
    with _MockBoard(lambda fd: _SilentAfterMockFW(fd, reply_ticks=0)) as mb:
        p = _run_cli(_board_cli_args(mb.port, out, max_ticks=5,
                                     rsp_timeout_ms=300))
    details = ["cmd: board 模式，mock PING/RESET 应答但 CMD_TICK 全静默，"
               "rsp-timeout=300ms"]
    A = [("CLI 退出码 1（aborted 语义）", p.returncode == 1)]
    csv_path = os.path.join(out, "closedloop_S20_npu.csv")
    meta_path = os.path.join(out, "closedloop_S20_npu_meta.json")
    meta = _load_meta(meta_path)
    A.append(("meta 存在", meta is not None))
    if meta is None:
        return A, details
    _, rows_ab = _read_csv_rows(csv_path + ".aborted")
    A.append(("meta status=aborted", meta["status"] == "aborted"))
    A.append(("meta termination_reason=error",
              meta["termination_reason"] == "error"))
    A.append(("TIMEOUT 事件@tick0", _has_event(meta, "TIMEOUT", 0)))
    A.append(("meta.error 非空且含 RSP_TICK 超时",
              bool(meta["error"]) and "RSP_TICK 超时" in meta["error"]))
    A.append(("<csv>.aborted 存在（表头 + 0 数据行）",
              rows_ab is not None and len(rows_ab) == 0))
    A.append(("正式 CSV 不存在（整跑作废）", not os.path.isfile(csv_path)))
    A.append(("meta out_csv 指向 .aborted", meta["out_csv"].endswith(".aborted")))
    return A, details


class _StopAdv(object):
    """case D 伪造推进器：第 stop_at+1 拍（k=stop_at）置位 voyage_stop。"""

    def __init__(self, stop_at):
        self.stop_at = stop_at
        self.n = 0

    def step(self, p_gen_kw):
        k = self.n
        self.n += 1
        return {
            "t_s": float(k + 1), "P_load_total_kW": 100.0,
            "SOC": 0.6 - 0.001 * (k + 1), "v_cmd_mps": 3.0,
            "v_ship_mps": 2.5, "acceleration_mps2": 0.0,
            "x_ship_m": 2.5 * (k + 1), "fuel_total_kg": 0.5 * (k + 1),
            "P_bat_kW": 100.0 - float(p_gen_kw), "P_prop_kW": 150.0,
            "voyage_stop": 1.0 if k >= self.stop_at else 0.0,
        }

    def state(self):
        return {k: 0.0 for k in STATE_KEYS}

    def fuel(self):
        return 0.5 * self.n

    def voyage_stop(self):
        return self.n > self.stop_at

    def close(self):
        pass


class _BoomAdv(object):
    """case H 伪造推进器：第 2 拍 step() 抛 PlantStepError（数值健康注入）。"""

    def __init__(self):
        self.n = 0

    def step(self, p_gen_kw):
        self.n += 1
        if self.n >= 2:
            raise PlantStepError("selftest: 注入数值异常（第 2 拍）")
        return {"t_s": 1.0, "P_load_total_kW": 100.0, "SOC": 0.599,
                "v_cmd_mps": 3.0, "v_ship_mps": 2.5,
                "acceleration_mps2": 0.0, "x_ship_m": 2.5,
                "fuel_total_kg": 0.5, "P_bat_kW": 100.0 - float(p_gen_kw),
                "P_prop_kW": 150.0, "voyage_stop": 0.0}

    def state(self):
        return {}

    def fuel(self):
        return 0.0

    def voyage_stop(self):
        return False

    def close(self):
        pass


def _run_with_fake_advancer(ctx, out_dir, fake_factory, max_ticks):
    """in-process 跑 run_closedloop，monkeypatch make_advancer（finally 还原，
    不动 harness 源码）。返回 (result, 还原后 make_advancer 引用)。"""
    orig = sys.modules["ems_closedloop"].make_advancer
    sys.modules["ems_closedloop"].make_advancer = fake_factory
    try:
        r = run_closedloop(_harness_args(out_dir, max_ticks=max_ticks))
    finally:
        sys.modules["ems_closedloop"].make_advancer = orig
    return r


def case_d_voyage_stop_termination(ctx):
    """D：voyage_stop 提前终止兜底（VOYAGE_STOP 事件 + 终止原因标记）。"""
    out = ctx["case_dir"]("d_voyage_stop_termination")
    stop_at = 3                            # 第 4 拍（k=3）置位
    r = _run_with_fake_advancer(
        ctx, out, lambda backend="stub", scenario="S20": _StopAdv(stop_at),
        max_ticks=30)
    meta = r["meta"]
    csv_path = os.path.join(out, "closedloop_S20_replay.csv")
    header, rows = _read_csv_rows(csv_path)
    col = {c: i for i, c in enumerate(RECORD_COLS)}
    A = [("run status=completed（voyage_stop 为优雅终止）",
          r["status"] == "completed")]
    A.append(("termination_reason=voyage_stop",
              meta["termination_reason"] == "voyage_stop"))
    A.append(("rows=4（tick0..3，第 4 拍触发）", meta["rows"] == 4))
    A.append(("VOYAGE_STOP 事件@tick3", _has_event(meta, "VOYAGE_STOP", 3)))
    A.append(("事件 detail 含置位拍 x_ship 读数",
              all("x_ship" in ev.get("detail", "")
                  for ev in meta["events"] if ev["type"] == "VOYAGE_STOP")))
    A.append(("正式 CSV（非 .aborted）落盘", header is not None))
    A.append(("CSV 末行 voyage_stop 列=1",
              header is not None
              and float(rows[-1][col["voyage_stop"]]) == 1.0))
    A.append(("CSV 前 3 行 voyage_stop 列=0",
              header is not None
              and all(float(rows[k][col["voyage_stop"]]) == 0.0
                      for k in range(3))))
    A.append(("n_ticks_planned=30（提前终止，未达固定拍数）",
              meta["n_ticks_planned"] == 30))
    return A, ["in-process + monkeypatch make_advancer（fake: 第 %d 拍置位）"
               % (stop_at + 1)]


def case_e1_advancer_stub_smoke(ctx):
    """E1：stub 推进器接口 3 拍冒烟（11 键/有限/复位 IC）。"""
    adv = make_advancer(backend="stub", scenario="S20")
    A = []
    try:
        outs = [adv.step(200.0) for _ in range(3)]
        A.append(("每拍返回 11 键 == OUT_KEYS（键集合一致）",
                  all(tuple(d.keys()) == tuple(OUT_KEYS) for d in outs)))
        A.append(("所有返回值有限（数值健康）",
                  all(_finite(v) for d in outs for v in d.values())))
        A.append(("t_s 整秒语义（1/2/3，100×0.01s 子步）",
                  all(abs(outs[k]["t_s"] - (k + 1)) < 1e-9 for k in range(3))))
        A.append(("P_load_total_kW == STUB_HOTEL_KW",
                  all(d["P_load_total_kW"] == STUB_HOTEL_KW for d in outs)))
        A.append(("fuel 逐拍严格递增且 fuel() 与末拍一致",
                  outs[0]["fuel_total_kg"] < outs[1]["fuel_total_kg"]
                  < outs[2]["fuel_total_kg"]
                  and abs(adv.fuel() - outs[2]["fuel_total_kg"]) < 1e-12))
        A.append(("voyage_stop() is False（stub 恒 False）",
                  adv.voyage_stop() is False))
        st = adv.state()
        A.append(("state() 键 == 前 7 键（CMD_TICK 载荷序）",
                  tuple(st.keys()) == tuple(STATE_KEYS)))
        A.append(("SOC 随放电/充电变化且有限", _finite(st["SOC"])))
        adv.init("S20")                      # 再次 init = 复位到模型 IC
        st0 = adv.state()
        A.append(("re-init 复位 IC（t=0 / SOC=0.60）",
                  st0["t_s"] == 0.0 and st0["SOC"] == 0.60))
        A.append(("close() 无异常", True))
    finally:
        adv.close()
    return A, ["make_advancer('stub', 'S20') + 3×step(200.0)"]


def case_e2_advancer_ctypes_smoke(ctx):
    """E2：ctypes 推进器接口 3 拍冒烟（真实 dylib；缺失 SKIP）。"""
    dylib = os.path.join(PLANTSTEP_DIR, "S20", "libplantstep_S20.dylib")
    if not os.path.isfile(dylib):
        raise _CaseSkip("dylib 缺失: %s（先跑 codegen + build_plantstep.sh；"
                        "SKIP 不判 FAIL——design §6.5）" % os.path.basename(dylib))
    adv = make_advancer(backend="ctypes", scenario="S20")
    A = []
    try:
        outs = [adv.step(200.0) for _ in range(3)]
        A.append(("每拍返回 11 键 == OUT_KEYS（键集合一致）",
                  all(tuple(d.keys()) == tuple(OUT_KEYS) for d in outs)))
        A.append(("所有返回值有限（数值健康，接口可用）",
                  all(_finite(v) for d in outs for v in d.values())))
        A.append(("t_s 逐拍严格递增（推进器在走时）",
                  outs[0]["t_s"] < outs[1]["t_s"] < outs[2]["t_s"]))
        A.append(("fuel 逐拍严格递增且 fuel() 与末拍一致",
                  outs[0]["fuel_total_kg"] < outs[1]["fuel_total_kg"]
                  < outs[2]["fuel_total_kg"]
                  and abs(adv.fuel() - outs[2]["fuel_total_kg"]) < 1e-9))
        A.append(("voyage_stop() 返回 bool", isinstance(adv.voyage_stop(), bool)))
        st = adv.state()
        A.append(("state() 键 == 前 7 键且全有限",
                  tuple(st.keys()) == tuple(STATE_KEYS)
                  and all(_finite(v) for v in st.values())))
        A.append(("close()（dlclose）无异常", True))
    finally:
        adv.close()
    # 注：t_s 整秒性不在此断言（ERT 根 Outport 写出时机口径由 G-P2 门禁逐拍定案，
    # 见 plantstep_advancer.py 头注释）。
    return A, ["make_advancer('ctypes', 'S20') + 3×step(200.0); dylib=%s"
               % os.path.basename(dylib)]


def case_f_midrun_silent_redo(ctx):
    """F：mock 第 10 拍后停发 → 整跑重做语义（OQ-4/FD5）。"""
    if not ctx["serial_ok"]:
        raise _CaseSkip("pyserial 不可用（board 模式需 pyserial；请用 .venv_tf 运行）")
    out = ctx["case_dir"]("f_midrun_silent_redo")
    with _MockBoard(lambda fd: _SilentAfterMockFW(fd, reply_ticks=10)) as mb:
        p = _run_cli(_board_cli_args(mb.port, out, max_ticks=20,
                                     rsp_timeout_ms=300))
    details = ["cmd: board 模式，mock 应答 tick0..9 后 CMD_TICK 静默，"
               "rsp-timeout=300ms"]
    A = [("CLI 退出码 1（aborted 语义）", p.returncode == 1)]
    csv_path = os.path.join(out, "closedloop_S20_npu.csv")
    meta_path = os.path.join(out, "closedloop_S20_npu_meta.json")
    meta = _load_meta(meta_path)
    A.append(("meta 存在", meta is not None))
    if meta is None:
        return A, details
    _, rows_ab = _read_csv_rows(csv_path + ".aborted")
    A.append(("meta status=aborted", meta["status"] == "aborted"))
    A.append(("TIMEOUT 事件@tick10", _has_event(meta, "TIMEOUT", 10)))
    A.append(("<csv>.aborted 恰含 10 已收集行（tick0..9）",
              rows_ab is not None and len(rows_ab) == 10))
    A.append(("mock 侧恰收 11 帧 CMD_TICK（0..10）", mb.board.n_tick_rx == 11))
    A.append(("正式 CSV 不存在", not os.path.isfile(csv_path)))
    A.append(("meta out_csv 指向 .aborted", meta["out_csv"].endswith(".aborted")))
    return A, details


def case_g_idx_mismatch_guard(ctx):
    """G：mock 回 idx+1 → IDX_MISMATCH 事件（配对守卫负例）。"""
    if not ctx["serial_ok"]:
        raise _CaseSkip("pyserial 不可用（board 模式需 pyserial；请用 .venv_tf 运行）")
    out = ctx["case_dir"]("g_idx_mismatch_guard")
    with _MockBoard(_WrongIdxMockFW) as mb:
        p = _run_cli(_board_cli_args(mb.port, out, max_ticks=5,
                                     rsp_timeout_ms=500))
    details = ["cmd: board 模式，mock 对 CMD_TICK(k) 回 idx=k+1（错位注入）"]
    A = [("CLI 退出码 1（aborted 语义）", p.returncode == 1)]
    csv_path = os.path.join(out, "closedloop_S20_npu.csv")
    meta_path = os.path.join(out, "closedloop_S20_npu_meta.json")
    meta = _load_meta(meta_path)
    A.append(("meta 存在", meta is not None))
    if meta is None:
        return A, details
    _, rows_ab = _read_csv_rows(csv_path + ".aborted")
    A.append(("meta status=aborted", meta["status"] == "aborted"))
    A.append(("IDX_MISMATCH 事件@tick0", _has_event(meta, "IDX_MISMATCH", 0)))
    A.append(("<csv>.aborted 存在（0 数据行，首拍即错位）",
              rows_ab is not None and len(rows_ab) == 0))
    A.append(("正式 CSV 不存在", not os.path.isfile(csv_path)))
    return A, details


def case_h_plantstep_abnormal_event(ctx):
    """H：推进器数值健康异常 → PLANT_STEP_ABNORMAL 事件（事件环映射）。"""
    out = ctx["case_dir"]("h_plantstep_abnormal_event")
    r = _run_with_fake_advancer(
        ctx, out, lambda backend="stub", scenario="S20": _BoomAdv(),
        max_ticks=5)
    meta = r["meta"]
    csv_path = os.path.join(out, "closedloop_S20_replay.csv")
    _, rows_ab = _read_csv_rows(csv_path + ".aborted")
    A = [("run status=aborted", r["status"] == "aborted")]
    A.append(("meta termination_reason=error",
              meta["termination_reason"] == "error"))
    A.append(("PLANT_STEP_ABNORMAL 事件@tick1", _has_event(
        meta, "PLANT_STEP_ABNORMAL", 1)))
    A.append(("meta error 含 PLANT_STEP_ABNORMAL",
              "PLANT_STEP_ABNORMAL" in meta["error"]))
    A.append(("<csv>.aborted 恰含 1 已收集行", rows_ab is not None
              and len(rows_ab) == 1))
    A.append(("正式 CSV 不存在", not os.path.isfile(csv_path)))
    return A, ["in-process + monkeypatch make_advancer（fake 第 2 拍抛 "
               "PlantStepError）"]


def case_i_builtin_selftest_parity(ctx):
    """I：T-607 内置自检（ems_closedloop --selftest）rc=0 平价对照。"""
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = closedloop_main(["--selftest"])
    text = buf.getvalue()
    A = [("内置自检退出码 0", rc == 0)]
    A.append(("内置自检输出含 EMS_CLOSEDLOOP SELFTEST PASS",
              "EMS_CLOSEDLOOP SELFTEST PASS" in text))
    A.append(("内置自检覆盖 10 项断言（status/termination/列/meta/mode/...）",
              text.count("[OK]") >= 10))
    return A, ["in-process: ems_closedloop.main(['--selftest'])（其产物自进其 "
               "tempfile，本套件不清理）"]


CASES = (
    ("A_replay_stub_baseline", case_a_replay_stub_baseline, None),
    ("B_board_mockfw_pairing", case_b_board_mockfw_pairing, None),
    ("C_rsp_timeout_abort", case_c_rsp_timeout_abort, None),
    ("D_voyage_stop_termination", case_d_voyage_stop_termination, None),
    ("E1_advancer_stub_smoke", case_e1_advancer_stub_smoke, "stub"),
    ("E2_advancer_ctypes_smoke", case_e2_advancer_ctypes_smoke, "ctypes"),
    ("F_midrun_silent_redo", case_f_midrun_silent_redo, None),
    ("G_idx_mismatch_guard", case_g_idx_mismatch_guard, None),
    ("H_plantstep_abnormal_event", case_h_plantstep_abnormal_event, None),
    ("I_builtin_selftest_parity", case_i_builtin_selftest_parity, None),
)


def _run_case(name, fn, ctx):
    t0 = time.perf_counter()
    rec = {"name": name, "status": "PASS", "duration_s": None,
           "asserts": [], "details": [], "error": ""}
    try:
        asserts, details = fn(ctx)
        rec["asserts"] = [{"name": n, "ok": bool(ok)} for n, ok in asserts]
        rec["details"] = list(details)
        if not all(ok for _, ok in asserts):
            rec["status"] = "FAIL"
    except _CaseSkip as sk:
        rec["status"] = "SKIP"
        rec["error"] = str(sk)
    except Exception as exc:  # noqa: BLE001
        rec["status"] = "FAIL"
        rec["error"] = "%s: %s" % (type(exc).__name__, exc)
        rec["traceback"] = traceback.format_exc()
    rec["duration_s"] = round(time.perf_counter() - t0, 3)
    return rec


# ============================================================ 主流程
def main(argv=None):
    ap = argparse.ArgumentParser(
        description="T-608: 闭环 harness selftest 套件（AC-603，板会话前置条件；"
                    "design §6.5）")
    ap.add_argument("--backend", choices=("both", "stub", "ctypes"),
                    default="both",
                    help="推进器后端过滤（仅作用于 E1/E2 接口用例；默认 both 全跑）")
    ap.add_argument("--json", action="store_true",
                    help="末尾附加结果 JSON（用例名/结果/耗时/断言详情）")
    args = ap.parse_args(argv)

    try:
        import serial  # noqa: F401
        serial_ok = True
    except ImportError:
        serial_ok = False

    tmp_root = tempfile.mkdtemp(prefix="closedloop_selftest_")

    def case_dir(name):
        d = os.path.join(tmp_root, name)
        os.makedirs(d, exist_ok=True)
        return d

    ctx = {"case_dir": case_dir, "serial_ok": serial_ok,
           "backend_filter": args.backend}

    print("== closedloop_selftest (T-608) spec=ems01-chil-v3-closedloop "
          "FR-606/FR-607/AC-603")
    print("python:  %s" % sys.executable)
    print("repo:    %s" % REPO_ROOT)
    print("tmp:     %s" % tmp_root)
    print("serial:  %s（pyserial）" % ("available" if serial_ok else "MISSING"
                                       "——board 用例将 SKIP"))
    print()

    results = []
    for name, fn, backend_tag in CASES:
        if backend_tag is not None and args.backend != "both" \
                and args.backend != backend_tag:
            results.append({"name": name, "status": "SKIP", "duration_s": 0.0,
                            "asserts": [], "details": [],
                            "error": "由 --backend %s 过滤" % args.backend})
            continue
        rec = _run_case(name, fn, ctx)
        results.append(rec)

    for rec in results:
        line = "[%s] %s (%.2fs)" % (rec["status"], rec["name"],
                                    rec["duration_s"])
        print(line)
        if rec["status"] == "SKIP":
            print("    SKIP: %s" % rec["error"])
        for a in rec["asserts"]:
            print("    [%s] %s" % ("OK" if a["ok"] else "FAIL", a["name"]))
        if rec["status"] == "FAIL":
            for d in rec["details"]:
                print("    detail: %s" % d)
            if rec.get("error"):
                print("    error: %s" % rec["error"])
            if rec.get("traceback"):
                print(rec["traceback"].rstrip())

    n_pass = sum(1 for r in results if r["status"] == "PASS")
    n_skip = sum(1 for r in results if r["status"] == "SKIP")
    n_fail = sum(1 for r in results if r["status"] == "FAIL")
    overall = n_fail == 0
    print()
    print("汇总: PASS=%d SKIP=%d FAIL=%d（SKIP=环境性跳过，不计失败）"
          % (n_pass, n_skip, n_fail))

    keep = not overall
    print("artifacts: %s%s" % (tmp_root, "" if keep else "（已清理）"))
    if keep:
        print("（存在 FAIL——产物目录保留供排查: %s）" % tmp_root)

    if args.json:
        js = {
            "tool": "closedloop_selftest",
            "task": "T-608",
            "spec": "ems01-chil-v3-closedloop (FR-606/FR-607/AC-603)",
            "generated": time.strftime("%Y-%m-%d %H:%M:%S"),
            "python": sys.executable,
            "backend_filter": args.backend,
            "serial_ok": serial_ok,
            "overall": overall,
            "summary": {"pass": n_pass, "skip": n_skip, "fail": n_fail},
            "artifacts_dir": tmp_root if keep else None,
            "cases": results,
        }
        print(json.dumps(js, ensure_ascii=False, indent=1))

    print("CLOSEDLOOP SELFTEST %s" % ("PASS" if overall else "FAIL"))
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_c4_battery.py — S4-C4：冻结电池多航次闭环 runner（板在环，一次板会话顺序跑完全部航次）。

复用与差异（本文件冻结的设计决策）：
  组件全部 import 复用 C3 runner（Host/tools/s4_c3_closedloop.py，v2 定稿口径）：
  PlantE / Law6 / PmsHost / BoardClient / A2Assembler / load_kf_table / f32 / _fmt /
  _ring_v / H1WindowHost / sha256_file / build_gate 及全部常量。除"多航次编排"外，
  逐拍机制、下行 8 态口径、三链对照、G6 机械化 v2（含 2026-09-29 用户裁决：板后端
  漂移降级为 CLOSED_LOOP_BACKEND_DRIFT 档案、停机条件只对未归因不一致触发——全文
  承 Host/results/s4_c3/s4_c3_design_notes.md §5/§5.1）与 C3 完全同构；本文件不复制、
  不重定义任何 C3 语义。

多航次编排（唯一语义差异）：
  - 航次集合与顺序 = 冻结电池
    experiments/softhil_ems03_2026-09-25/results/s4_c3/S4_C4_BATTERY_FREEZE.json
    battery 顺序（S02_downstream → S08_resistance_bands → S03_upstream）；
    --voyages 可选子集/顺序（逐个校验属于冻结集合）；freeze 文件 sha256 记入全部
    产物 gate（禁按结果挑样，provenance_rule 同冻结件）。
  - 每航次独立会话态：plant 重新 init（PlantE 新建 = IC 复位）、新建 H1WindowHost
    并 begin（water_depth = 该航次 stream[0]）、A2 单实例换 stream + 清空缓冲
    （keras/TF 只初始化一次；A2Assembler 跨航次状态仅 stream + 双端缓冲，公开属性
    重置语义等价新建——TF set_visible_devices 在初始化后不可二次调用）、环 IC 样本
    重推（同 C3 tick 0 IC 口径）、TICK_CTRL seq 归零；每航次正式段前板 CMD_RESET +
    host 两 PMS ems04_pms_init（航次间零状态残留；复位位置与 C3 正式首拍前同构）。
  - 每航次：预热 WARMUP_TICKS 拍（law6 冷启动，板不参与）→ 正式拍数 = 该航次 kf 表
    行数 n（kk=30..30+n−1，t0=300..300+10×(n−1)，首拍首子步 stamp=t0 闭合 A2 窗与
    环边界——与 C3 完全同构，仅拍数由每航次 kf 表决定）。
  - 数据源：stream = model/simulink_modelE_(Phase4_Physics)/datasets_v3/pilot/
    <vid>.mat（scipy loadmat simplify_cells，只取 SCENARIO_COLS+time_s）；kf 表 =
    experiments/softhil_ems03_2026-09-25/results/s4_c3/<vid>.kf_table.bin（并行
    worker 产出；启动时统一存在性检查先于任何重初始化，缺文件 fail-closed 拒绝）。

停机语义：
  - 单航次 park（未归因停机条件：连续 ≥3 拍未归因 action 不一致 / 连续 ≥3 拍同档
    P_gen_ref 发散 / SOC 出界 / 板 flags≠0 / 植物非有限 / kf 缺行）→ 该航次 gate
    记 stop、电池合取 false，继续下一航次；
  - 板失联（PING / CMD_RESET / TICK_CTRL 重试无效）→ 全局停止（stopped_global），
    不再继续后续航次；未跑航次记入 skipped_voyages。

产物（Host/results/s4_c4/，目录自建）：
  每航次 s4_c4_<vid>.csv（列 = C3 CSV_COLS 原样）+ s4_c4_<vid>_warmup.csv +
  s4_c4_<vid>_gate.json（schema=s4_c4_voyage_gate_v1 = C3 gate v2 结构 + form.
  battery_freeze_sha256 + form.kf_table / form.s20_mat 修正为该航次实际路径）；
  电池级 s4_c4_gate.json（schema=s4_c4_battery_gate_v1：freeze 引用（path+sha256）
  + 每航次 overall/stop_reason/G6 摘要 + 电池合取 overall = 全部计划航次 overall
  ∧ 无全局停机 ∧ 非干跑；--dry 无板 overall 恒 false）。

运行（板在环）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python tools/s4_c4_battery.py \
      --port /dev/cu.usbserial-1120
  --voyages S02_downstream,S03_upstream   # 子集/顺序（默认按冻结 battery 顺序）
  --dry                                    # 无板干跑（不出正式 gate，overall 恒 false）
自检（无板无植物；import C3 模块 + 冻结件/结构校验，不初始化任何 dylib/keras/ORT）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python tools/s4_c4_battery.py --selftest

S4-P3 B1 扩展（2026-09-29，向后兼容；默认 pilot/modelE 路径行为零改动）：
  --plant-backend streamE --data-root <formal/iid 目录>
    装配 P3 formal 臂电池（冻结件 Host/results/s4_c4/S4_P3_FORMAL_ARM_FREEZE.json，
    schema s4_p3_formal_arm_freeze_v1）：植物 = streamE 流驱动 dylib
    （Host/plantstep_e_stream/<vid>/libplantstepE_stream.dylib），stream 数据源 =
    --data-root/<vid>.mat；产物目录改 Host/results/s4_p3/（不与 pilot 产物混写）。
    dylib 缺席（B2 未放行）时启动即写 PLANTSTEP_DYLIB_PENDING 门
    （results/s4_p3/s4_p3_gate.json，overall 恒 false，rc=3），不做任何重初始化。
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))

# ---- 复用 C3 runner（import 只执行其顶层：路径推导 + constants.json 加载 +
# sys.path 注入；无串口/keras 副作用）----
import s4_c3_closedloop as c3  # noqa: E402
from s4_c3_closedloop import (  # noqa: E402
    A2Assembler, BoardClient, Law6, PmsHost, PlantE,
    CSV_COLS, CMD_PING, CMD_RESET, CMD_TICK_CTRL, GRID_DLAM, GRID_DP,
    KK_FIRST, RADPS2RPM, RSP_LEN, RSP_TICK_CTRL, SCENARIO_COLS, SOC_HI, SOC_LO,
    SUB_PER_SEC, SUB_PER_TICK, TICK_S, WARMUP_TICKS,
    _fmt, _ring_v, f32, load_kf_table)

REPO = c3.REPO
FREEZE_PATH = os.path.join(c3.EXP, "results", "s4_c3", "S4_C4_BATTERY_FREEZE.json")
RESULTS = os.path.join(c3.HOST, "results", "s4_c4")
SCENARIO = c3.SCENARIO                     # PlantStepE 场景（与 C3 同一 dylib 入口）

# 冻结电池文档顺序（S4-C4 任务书冻结；selftest 钉住，防止冻结件被静默替换）
BATTERY_DOC_ORDER = ("S02_downstream", "S08_resistance_bands", "S03_upstream")
BATTERY_DOC_ROLES = ("R_low", "R_mid", "R_high")

# ---- S4-P3 B1（2026-09-29）：streamE 变体装配（默认 modelE/pilot 零改动）----
PLANT_BACKENDS = ("modelE", "streamE")
FREEZE_P3_PATH = os.path.join(c3.HOST, "results", "s4_c4",
                              "S4_P3_FORMAL_ARM_FREEZE.json")
RESULTS_P3 = os.path.join(c3.HOST, "results", "s4_p3")

# 结构键完整性契约（selftest 自检面）
BATTERY_GATE_KEYS = ("schema", "created_utc", "mode", "dry_note",
                     "battery_freeze", "voyages", "skipped_voyages",
                     "stopped_global", "overall", "overall_note")
VOYAGE_ENTRY_KEYS = ("voyage_id", "role", "scenario_id", "overall", "stop_reason",
                     "formal_done", "formal_ticks", "board_lost", "g6", "artifacts")
VOYAGE_G6_KEYS = ("g6_1_pass", "g6_3_amended_pass", "g6_4_pass", "g6_6_pass",
                  "soc_min", "soc_max", "unattributed_pgen_n",
                  "drift_action_diff_n", "undeclared_n")


def kf_table_path(vid):
    return os.path.join(c3.EXP, "results", "s4_c3", vid + ".kf_table.bin")


def stream_path(vid, data_root=None):
    """stream .mat 路径（默认 pilot 目录 = 冻结行为；streamE 模式 = --data-root，
    相对路径按仓库根解析——同 s4_c3_pack_kf_table.py --mat 惯例）。"""
    root = data_root or os.path.join(c3.MODELE, "datasets_v3", "pilot")
    if not os.path.isabs(root):
        root = os.path.normpath(os.path.join(c3.REPO, root))
    return os.path.join(root, vid + ".mat")


def stream_bin_path(vid):
    """streamE 变体流回放 bin（tools/s4_p3_pack_stream.py 产物）。"""
    return os.path.join(c3.HOST, "plantstep_e_stream", vid,
                        vid + ".stream.f64.bin")


def load_freeze():
    """冻结电池读取（fail-closed：缺文件/结构异 → 明确错误）。"""
    if not os.path.isfile(FREEZE_PATH):
        raise RuntimeError("冻结电池缺失: %s" % os.path.relpath(FREEZE_PATH, REPO))
    freeze = json.load(open(FREEZE_PATH))
    if freeze.get("schema") != "s4_c4_battery_freeze_v1":
        raise RuntimeError("冻结电池 schema 异常: %r（期望 s4_c4_battery_freeze_v1）"
                           % freeze.get("schema"))
    battery = freeze.get("battery")
    if not isinstance(battery, list) or not battery:
        raise RuntimeError("冻结电池 battery 结构异常（需非空 list）")
    for e in battery:
        for k in ("role", "voyage_id"):
            if not isinstance(e.get(k), str) or not e[k]:
                raise RuntimeError("冻结电池条目缺 %s: %r" % (k, e))
    return freeze


def load_freeze_p3(path=None):
    """P3 formal 臂冻结件读取（streamE 模式；fail-closed，schema/status 校验）。"""
    p = path or FREEZE_P3_PATH
    if not os.path.isfile(p):
        raise RuntimeError("P3 冻结件缺失: %s" % os.path.relpath(p, c3.REPO))
    freeze = json.load(open(p))
    if (freeze.get("schema") != "s4_p3_formal_arm_freeze_v1"
            or freeze.get("status") != "FROZEN"):
        raise RuntimeError(
            "P3 冻结件 schema/status 异常: %r/%r"
            % (freeze.get("schema"), freeze.get("status")))
    battery = freeze.get("battery")
    if not isinstance(battery, list) or not battery:
        raise RuntimeError("P3 冻结件 battery 结构异常（需非空 list）")
    for e in battery:
        for k in ("role", "voyage_id", "arm"):
            if not isinstance(e.get(k), str) or not e[k]:
                raise RuntimeError("P3 冻结件条目缺 %s: %r" % (k, e))
    return freeze


def resolve_voyages(args, freeze):
    """航次顺序：默认冻结 battery 顺序；--voyages 子集/顺序（校验属于冻结集合）。"""
    battery = freeze["battery"]
    ids = [e["voyage_id"] for e in battery]
    role_of = {e["voyage_id"]: e["role"] for e in battery}
    # P3 冻结件条目无 scenario_id 键 → 回退 voyage_id（mission_v3 航次自为场景）；
    # C4 冻结件条目有 scenario_id → 行为与原实现逐字段一致。
    scen_of = {e["voyage_id"]: (e.get("scenario_id") or e["voyage_id"])
               for e in battery}
    if args.voyages:
        req = [v.strip() for v in args.voyages.split(",") if v.strip()]
        unknown = [v for v in req if v not in ids]
        if unknown:
            raise RuntimeError("--voyages 含冻结集合之外的航次: %s（冻结集合=%s）"
                               % (unknown, ids))
        if len(set(req)) != len(req):
            raise RuntimeError("--voyages 含重复航次: %s" % req)
        return req, role_of, scen_of
    return ids, role_of, scen_of


def check_kf_tables(vids):
    """kf 表存在性检查（先于任何读取/重初始化；返回缺失清单）。"""
    return [kf_table_path(v) for v in vids if not os.path.isfile(kf_table_path(v))]


def load_stream(vid, data_root=None, resample_100hz=False):
    """stream 装载（只取 SCENARIO_COLS+time_s；结构 fail-closed）。
    streamE 模式 resample_100hz=True：formal stream 为 1 Hz（n=dur+1），C3 链
    （_ring_v/st8/A2 场景侧列）按 100 Hz 索引——ZOH ×100 上采样对齐（与植物
    shim 每行 ZOH 100 子步同语义）；默认 pilot 路径 100 Hz 原样（零改动）。"""
    from scipy.io import loadmat
    spath = stream_path(vid, data_root)
    if not os.path.isfile(spath):
        raise RuntimeError("pilot stream 缺失: %s" % os.path.relpath(spath, REPO))
    mat = loadmat(spath, simplify_cells=True)
    if "stream" not in mat or "meta" not in mat:
        raise RuntimeError("stream .mat 结构异常（缺 stream/meta）: %s" % spath)
    need = set(SCENARIO_COLS) | {"time_s"}
    missing = need - set(mat["stream"].keys())
    if missing:
        raise RuntimeError("stream 缺列 %s: %s" % (sorted(missing), spath))
    stream = {c: np.asarray(mat["stream"][c], dtype=np.float64).ravel()
              for c in need}
    if resample_100hz:
        stream = {c: np.repeat(v, c3.SUB_PER_SEC) for c, v in stream.items()}
    meta = {"voyage_id": str(mat["meta"]["voyage_id"]),
            "fixture_kind": str(mat["meta"]["fixture_kind"])}
    del mat
    return spath, stream, meta


class PlantEStreamE(c3.PlantE):
    """S4-P3 B1：streamE 流驱动植物（Path S″）。子步推进/读数语义与 C3 PlantE
    完全同构（step_substep/close 继承），仅 advancer 装配后端不同
    （streamE = Host/plantstep_e_stream/<vid>/libplantstepE_stream.dylib）。"""

    def __init__(self, scenario):
        from plantstep_advancer import make_advancer
        self.adv = make_advancer(backend="streamE", scenario=scenario)
        self._lib = self.adv._lib
        self.t = 0.0                          # 已推进到的模型时刻（子步末）
        self.last_out = None
        self.last_states = None


def make_plant(backend, scenario):
    """植物装配（modelE = 冻结默认路径零改动；streamE = P3 B1 扩展）。"""
    if backend == "streamE":
        return PlantEStreamE(scenario)
    if backend == "modelE":
        return c3.PlantE(scenario)
    raise RuntimeError("plant-backend 必须为 %s，实际 %r"
                       % (PLANT_BACKENDS, backend))


def finalize_voyage_gate(gate, vid, role, kf_path, freeze_sha, spath):
    """C3 gate v2 → s4_c4_voyage_gate_v1（仅 schema/署名/form 路径修正，判据不动）。"""
    gate["schema"] = "s4_c4_voyage_gate_v1"
    gate["voyage_id"] = vid
    gate["battery_role"] = role
    gate["form"]["battery_freeze_sha256"] = freeze_sha
    kt = dict(gate["form"]["kf_table"])
    kt["path"] = os.path.relpath(kf_path, REPO)   # C3 form 内 path 固定 S20，改本航次表
    gate["form"]["kf_table"] = kt
    gate["form"]["s20_mat"] = os.path.relpath(spath, REPO)
    return gate


def build_battery_gate(planned, entries, dry, stopped_global, freeze, freeze_sha):
    """电池级合取 gate（纯构造，无 I/O；selftest 亦用）。"""
    done_ids = [e["voyage_id"] for e in entries]
    skipped = [v for v in planned if v not in done_ids]
    overall = bool(not dry and stopped_global is None
                   and len(entries) == len(planned) and entries
                   and all(e["overall"] for e in entries))
    return {
        "schema": "s4_c4_battery_gate_v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "mode": "dry" if dry else "board",
        "dry_note": None if not dry else
            "干跑无板：B_BOARD 面=NaN，G6-3/G6-4 不适用，电池 overall 恒 false",
        "battery_freeze": {
            "path": os.path.relpath(FREEZE_PATH, REPO),
            "sha256": freeze_sha,
            "schema": freeze["schema"],
            "battery_order": [{"role": e["role"], "voyage_id": e["voyage_id"]}
                              for e in freeze["battery"]],
        },
        "voyages": entries,
        "skipped_voyages": skipped,
        "stopped_global": stopped_global,
        "overall": overall,
        "overall_note": "电池合取 = 全部计划航次 overall ∧ 无全局停机 ∧ 非干跑；"
                        "单航次 park 只使该航次 overall=false 并继续下一航次，"
                        "板失联（PING/RESET/TICK_CTRL 重试无效）→ 全局停止。",
    }


def build_pending_gate(planned, dry, freeze, freeze_sha, missing):
    """S4-P3 B1：PLANTSTEP_DYLIB_PENDING 门（streamE dylib 缺席，待 B2 codegen
    +build 放行）。fail-closed 记录产物：overall 恒 false，不做任何重初始化/写航次
    产物；B2 补齐 dylib 后须以正式 4/4 合取门覆盖本文件。"""
    return {
        "schema": "s4_p3_formal_battery_gate_v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "mode": "dry" if dry else "board",
        "status": "PLANTSTEP_DYLIB_PENDING",
        "status_note": "streamE dylib 尚未产出（B2 未放行）；本门为 B1 装配"
                       "就绪检查记录（freeze/stream bin/kf 表已就绪面），"
                       "overall 恒 false，非正式验收产物",
        "battery_freeze": {
            "path": os.path.relpath(FREEZE_P3_PATH, c3.REPO),
            "sha256": freeze_sha,
            "schema": freeze["schema"],
            "battery_order": [{"role": e["role"], "voyage_id": e["voyage_id"]}
                              for e in freeze["battery"]],
        },
        "planned_voyages": list(planned),
        "voyages": [],
        "skipped_voyages": list(planned),
        "stopped_global": {"reason": "PLANTSTEP_DYLIB_PENDING",
                           "at_voyage": None},
        "missing_dylib": missing,
        "overall": False,
        "overall_note": "PENDING 门：B2 补齐 dylib 后重跑覆盖；dry 语义下同样"
                        "不出正式 gate（overall 恒 false）",
    }


def run_voyage(vid, role, spath, stream, meta, a2, kft, kf_path,
               board, law, pms_bh, pms_a2, chain, dry, freeze_sha,
               plant_backend="modelE", results_dir=None):
    """单航次闭环（与 C3 main 逐拍同构；差异仅：产物路径、正式拍数=该航次 kf 行数、
    正式段前板复位失败 → 全局停机语义、航次级产物）。返回结果 dict（含 gate）。
    S4-P3 B1：plant_backend='streamE' 时植物走流驱动 dylib、场景=voyage_id；
    results_dir 缺省回落模块级 RESULTS（pilot 行为零改动）。"""
    res_dir = results_dir or RESULTS
    t_v0 = time.time()
    n_formal = kft["n"]
    print("[c4:%s] kf 表: n=%d issue=%g..%g step=%g vid=%s sha=%s…"
          % (vid, n_formal, kft["issue_first"],
             kft["issue_first"] + (n_formal - 1) * kft["issue_step"],
             kft["issue_step"], kft["voyage_id"], kft["sha256"][:12]))

    plant = make_plant(plant_backend,
                       vid if plant_backend == "streamE" else SCENARIO)
    try:
        ring = c3.H1WindowHost()
        ring.begin(0, float(stream["water_depth_m"][0]), 0.0, 0)

        # ---- 环 IC 样本（同 C3：tick_first=0，场景初值口径）----
        st_ic = {"SOC": 0.60, "x_ship_m": 0.0, "omega_prop_radps": 0.0,
                 "v_ship_mps": 0.0, "V_dc_V": 750.0}
        ic = _ring_v(0, {"P_load_total_kW": 100.0, "v_ship_mps": 0.0,
                         "acceleration_mps2": 0.0}, st_ic, stream)
        ring.tick(0, ic)

        def _run_substeps(n, pgen, st_prev):
            """同 C3：n 个子步（恒 pgen）；推 A2；整秒 stamp 推环。"""
            out = st = None
            for _ in range(n):
                stamp, out, st = plant.step_substep(pgen)
                a2.push_substep(stamp, out, st)
                if abs(stamp - round(stamp)) < 1e-9 and int(round(stamp)) > 0:
                    sec = int(round(stamp))
                    ring.tick(sec, _ring_v(sec, out, st_prev, stream))
                st_prev = st
            return out, st, st_prev

        # ---- 预热段（is_formal=0；板不参与；law6 冷启动）----
        warm_path = os.path.join(res_dir, "s4_c4_%s_warmup.csv" % vid)
        f_warm = open(warm_path, "w")
        f_warm.write("tick,t_end,pgen_cold_kw,P_load,SOC,Vdc,voyage_stop\n")
        pgen_next = 0.0
        st_prev = st_ic
        out_last = None
        print("[c4:%s] 预热 %d 拍（law6 冷启动，is_formal=0）…" % (vid, WARMUP_TICKS))
        for kk in range(WARMUP_TICKS):
            if kk == 0:
                p_load, soc = 100.0, 0.60
            else:
                p_load = out_last["P_load_total_kW"]
                soc = st_prev["SOC"]
            lam_cold = law.lam_eff(0.0, soc)
            pg = law.exec_layer(p_load, soc, lam_cold)
            out_last, _, st_prev = _run_substeps(SUB_PER_TICK, pg, st_prev)
            pgen_next = pg
            f_warm.write("%d,%g,%.9g,%.9g,%.9g,%.9g,%d\n"
                         % (kk, plant.t, pg, p_load, soc, st_prev["V_dc_V"],
                            int(out_last["voyage_stop"])))
        f_warm.close()
        print("[c4:%s] 预热完成 t=%.0f SOC=%.4f" % (vid, plant.t, st_prev["SOC"]))

        # ---- 板复位 + host PMS 同步（每航次零残留；C3 正式首拍前同位）----
        stop_reason = None
        board_lost = False
        if board is not None:
            if not board.reset():
                stop_reason = "板失联（CMD_RESET 失败，正式段前）"
                board_lost = True
            else:
                pms_bh.init()
                pms_a2.init()

        csv_path = os.path.join(res_dir, "s4_c4_%s.csv" % vid)
        f_csv = open(csv_path, "w")
        f_csv.write(",".join(CSV_COLS) + "\n")
        rows = []
        seq = 0
        consec_unattr_pgen = 0
        n_drift_action = n_unattr_pgen = n_drift_pgen = 0
        n_a2_action_diff = n_a2_pgen_gt1 = 0
        first_unattr_pgen = None
        drift_ticks = []
        voyage_stop_tick = None

        print("[c4:%s] 正式段 %d 拍（t0=%d..%d，is_formal=1）…"
              % (vid, n_formal, KK_FIRST * TICK_S,
                 (KK_FIRST + n_formal - 1) * TICK_S))
        for kk in range(KK_FIRST, KK_FIRST + n_formal):
            if stop_reason is not None:
                break
            t0 = kk * TICK_S
            if int(t0) not in kft["rows"]:
                stop_reason = "kf 表缺 issue=%.0f（fail-closed）" % t0
                break
            # 1) 本拍第 1 个子步（OUT@stamp=t0 闭合 A2 窗与环边界）
            out, _, st_prev = _run_substeps(1, pgen_next, st_prev)
            if out["voyage_stop"] > 0 and voyage_stop_tick is None:
                voyage_stop_tick = t0
            if not all(math.isfinite(x) for x in
                       (out["SOC"], out["P_load_total_kW"], st_prev["v_ship_mps"],
                        st_prev["V_dc_V"])):
                stop_reason = "STOP: 植物数值异常（非有限）"
                break

            # 2) 下行 8 态（f32 线域；host 链消费同 f32 回读值）
            soc_x, vdc_x = st_prev["SOC"], st_prev["V_dc_V"]
            i_t0 = int(round(t0 * SUB_PER_SEC))
            st8_f64 = (out["P_load_total_kW"], soc_x, vdc_x,
                       max(out["P_bat_kW"], 0.0), max(-out["P_bat_kW"], 0.0),
                       stream["P_prop_demand_kW"][i_t0], stream["P_hotel_kW"][i_t0])
            st8 = tuple(f32(v) for v in st8_f64) + (0.0,)

            # 3) B hist（板算法 host 复刻）+ kf 行
            hist_b = ring.construct(int(t0))
            kf_bytes, kf_f32 = kft["rows"][int(t0)]

            # 4) 板在环
            board_ms = float("nan")
            if board is not None:
                t_b0 = time.time()
                bb = board.tick_ctrl(seq, t0, st8, hist_b, kf_f32)
                board_ms = (time.time() - t_b0) * 1000.0
                if bb is None:
                    stop_reason = "板失联（TICK_CTRL 重试后仍无 RSP）"
                    board_lost = True
                    break
                if bb["flags"] != 0:
                    stop_reason = "STOP: 板 flags=%d（infer/DP fail）" % bb["flags"]
                    break
            else:
                bb = {k: float("nan") for k in
                      ("d1", "lam", "P_gen_ref", "P_commit", "cstate", "rcode",
                       "on1", "on2", "ref1", "ref2", "N", "fault0", "fault1",
                       "fs0", "fs1", "fstp0", "fstp1", "health", "fb",
                       "infer_us", "ctrl_us", "flags")}

            # 5) B_HOST 链（下行同字节）
            t_h0 = time.time()
            p_load_f, soc_f = st8[0], st8[1]
            dis_f, chg_f, prop_f, hotel_f = st8[3], st8[4], st8[5], st8[6]
            q_bh = np.asarray(chain.run(hist_b.reshape(300, 11),
                                        kf_f32.reshape(30, 6))["q"],
                              dtype=np.float64).reshape(30, 3)[:, 1]
            n0_bh = max(1.0, pms_bh.S.UD_N_dispatch_fb)
            d1_bh = law.dp_tick(q_bh, soc_f, n0_bh)
            lam_bh = law.lam_eff(d1_bh, soc_f)
            pg_bh = law.exec_layer(p_load_f, soc_f, lam_bh)
            pms_bh.tick1000(p_load_f, soc_f, dis_f, chg_f, prop_f, hotel_f, pg_bh)
            fc_bh = pms_bh.faces()

            # 6) A2 链（canonical 域）
            q_a2 = a2.predict(t0, kf_f32)
            n0_a2 = max(1.0, pms_a2.S.UD_N_dispatch_fb)
            d1_a2 = law.dp_tick(q_a2, soc_f, n0_a2)
            lam_a2 = law.lam_eff(d1_a2, soc_f)
            pg_a2 = law.exec_layer(p_load_f, soc_f, lam_a2)
            pms_a2.tick1000(p_load_f, soc_f, dis_f, chg_f, prop_f, hotel_f, pg_a2)
            fc_a2 = pms_a2.faces()
            host_ms = (time.time() - t_h0) * 1000.0

            # 7) 记录 + 归因判据面（v2 口径，承 design notes §5/§5.1 裁决）
            a_bh = int(round(d1_bh / GRID_DLAM))
            a_a2 = int(round(d1_a2 / GRID_DLAM))
            eq_bb_bh = pg_diff_bb_bh = None
            a_bb = float("nan")
            attr = "equal"
            if board is not None:
                a_bb = int(round(float(bb["d1"]) / GRID_DLAM))
                pg_diff_bb_bh = abs(float(bb["P_gen_ref"]) - pg_bh)
                eq_bb_bh = a_bb == a_bh
                attr, sub = c3.classify_action_face(a_bb, a_bh, a_a2)
            pg_diff_a2_bh = abs(pg_a2 - pg_bh)
            eq_a2_bh = a_a2 == a_bh
            if board is not None:
                if attr == "drift":
                    n_drift_action += 1
                    drift_ticks.append(t0)
                if pg_diff_bb_bh > GRID_DP + 1e-9:
                    if eq_bb_bh:
                        n_unattr_pgen += 1
                        consec_unattr_pgen += 1
                        if first_unattr_pgen is None:
                            first_unattr_pgen = t0
                    else:
                        n_drift_pgen += 1
                        consec_unattr_pgen = 0
                else:
                    consec_unattr_pgen = 0
            if not eq_a2_bh:
                n_a2_action_diff += 1
            if pg_diff_a2_bh > GRID_DP + 1e-9:
                n_a2_pgen_gt1 += 1

            row = ([kk, t0, seq, 1, pgen_next]
                   + [out["P_load_total_kW"], soc_x, vdc_x, out["P_bat_kW"],
                      st_prev["v_ship_mps"],
                      st_prev["omega_prop_radps"] * RADPS2RPM,
                      st_prev["x_ship_m"], out["voyage_stop"]]
                   + list(st8[:7])
                   + [float(q_a2[0]), float(q_bh[0])]
                   + [bb["d1"], a_bb, bb["lam"], bb["P_gen_ref"], bb["P_commit"],
                      bb["cstate"], bb["rcode"], bb["on1"], bb["on2"], bb["ref1"],
                      bb["ref2"], bb["N"], bb["fault0"], bb["fault1"], bb["fs0"],
                      bb["fs1"], bb["fstp0"], bb["fstp1"], bb["health"], bb["fb"],
                      bb["flags"], bb["infer_us"], bb["ctrl_us"]]
                   + [d1_bh, a_bh, lam_bh, pg_bh]
                   + [fc_bh[k] for k in PmsHost.FACE_KEYS]
                   + [d1_a2, a_a2, lam_a2, pg_a2]
                   + [fc_a2[k] for k in PmsHost.FACE_KEYS]
                   + [int(eq_bb_bh) if eq_bb_bh is not None else "nan", attr,
                      pg_diff_bb_bh if pg_diff_bb_bh is not None else "nan",
                      int(eq_a2_bh), pg_diff_a2_bh, host_ms, board_ms])
            rows.append(row)
            f_csv.write(",".join(_fmt(v) for v in row) + "\n")
            f_csv.flush()
            seq += 1

            # 8) 植物推进本拍剩余 999 子步（新板 P_gen_ref）
            new_pgen = (float(bb["P_gen_ref"]) if board is not None else pg_bh)
            out, _, st_prev = _run_substeps(SUB_PER_TICK - 1, new_pgen, st_prev)
            pgen_next = new_pgen

            # 停机条件（v2.1 硬面：同档执行层发散 / 运行性；action 差全归档）
            if consec_unattr_pgen >= 3:
                stop_reason = ("STOP: B_BOARD vs B_HOST 连续 %d 拍同档 P_gen_ref "
                               "发散" % consec_unattr_pgen)
                break
            if not (SOC_LO - 1e-9 <= soc_x <= SOC_HI + 1e-9):
                stop_reason = "STOP: SOC=%.4f 出界 [%.2f,%.2f]" % (soc_x, SOC_LO,
                                                                   SOC_HI)
                break
            if (kk - KK_FIRST) % 25 == 0:
                print("[c4:%s] t0=%.0f bb_d1=%+.3f bh_d1=%+.3f a2_d1=%+.3f "
                      "pgen bb=%.0f bh=%.0f a2=%.0f soc=%.3f (%.0fs)"
                      % (vid, t0, bb["d1"], d1_bh, d1_a2, bb["P_gen_ref"], pg_bh,
                         pg_a2, soc_x, time.time() - t_v0))

        f_csv.close()
        n_done = len(rows)

        # ---- G6 机械化（复用 C3 build_gate，v2.1 口径）----
        gate = c3.build_gate(
            rows, argparse.Namespace(formal_ticks=n_formal, dry=dry),
            kft, stop_reason, n_drift_action,
            n_unattr_pgen, n_drift_pgen, n_a2_action_diff, n_a2_pgen_gt1,
            first_unattr_pgen, drift_ticks, {}, voyage_stop_tick,
            board, seq, t_v0, meta)
        gate = finalize_voyage_gate(gate, vid, role, kf_path, freeze_sha, spath)
        gate_path = os.path.join(res_dir, "s4_c4_%s_gate.json" % vid)
        json.dump(gate, open(gate_path, "w"), indent=1, ensure_ascii=False)
        print("[c4:%s] 完成 %d/%d 拍；stop=%s" % (vid, n_done, n_formal, stop_reason))
        print("[c4:%s] G6 overall: %s -> %s" % (vid, gate["overall"],
                                                os.path.basename(gate_path)))
        return {
            "voyage_id": vid, "role": role, "scenario_id": meta["voyage_id"],
            "overall": bool(gate["overall"]), "stop_reason": gate["stop_reason"],
            "formal_done": n_done, "formal_ticks": n_formal,
            "board_lost": board_lost,
            "g6": {
                "g6_1_pass": gate["g6_1_pass"],
                "g6_3_amended_pass": gate["g6_3_amended_pass"],
                "g6_4_pass": gate["g6_4_pass"],
                "g6_6_pass": gate["g6_6_pass"],
                "soc_min": gate["runnability_G6_1"]["soc_min"],
                "soc_max": gate["runnability_G6_1"]["soc_max"],
                "unattributed_pgen_n": gate["frequency_G6_4"]["unattributed_pgen_n"],
                "drift_action_diff_n":
                    gate["backend_drift_archive"]["drift_action_diff_n"],
                "undeclared_n": gate["events_G6_6"]["undeclared_n"],
            },
            "artifacts": {
                "csv": os.path.relpath(csv_path, REPO),
                "warmup_csv": os.path.relpath(warm_path, REPO),
                "gate": os.path.relpath(gate_path, REPO),
            },
            "_gate": gate,
        }
    finally:
        plant.close()


# ---------------------------------------------------------------- 自检
def _selftest():
    """无板无植物自检：不初始化任何 dylib/keras/ORT/串口，不做任何写盘。"""
    n_pass = 0
    failures = []

    def check(name, cond):
        nonlocal n_pass
        if cond:
            n_pass += 1
            print("PASS: %s" % name)
        else:
            failures.append(name)
            print("FAIL: %s" % name)

    print("[selftest] 1) C3 模块 import + 关键符号在位")
    syms = ("PlantE", "Law6", "PmsHost", "BoardClient", "A2Assembler",
            "load_kf_table", "f32", "_fmt", "_ring_v", "sha256_file", "build_gate",
            "H1WindowHost", "COL_NAMES",
            "TICK_S", "SUB_PER_TICK", "SUB_PER_SEC", "WARMUP_TICKS", "KK_FIRST",
            "GRID_DP", "GRID_DLAM", "SOC_LO", "SOC_HI", "RADPS2RPM",
            "SCENARIO_COLS", "CSV_COLS", "RSP_LEN",
            "CMD_PING", "CMD_RESET", "CMD_TICK_CTRL", "RSP_TICK_CTRL")
    missing = [s for s in syms if not hasattr(c3, s)]
    check("c3 关键符号全部在位（缺=%s）" % missing, not missing)
    check("C3 口径常量域（TICK_S/SUB/WARMUP/KK_FIRST）",
          (c3.TICK_S, c3.SUB_PER_TICK, c3.SUB_PER_SEC, c3.WARMUP_TICKS,
           c3.KK_FIRST) == (10.0, 1000, 100, 30, 30))
    check("C3 口径常量域（SOC 界/网格）",
          (c3.SOC_LO, c3.SOC_HI, c3.GRID_DP, c3.GRID_DLAM, c3.RADPS2RPM)
          == (0.15, 0.95, 20.0, 0.005, 30.0 / math.pi))

    print("[selftest] 2) 冻结电池读取与 battery 顺序校验")
    try:
        freeze = load_freeze()
        check("冻结电池 schema + battery 结构", True)
    except RuntimeError as e:
        freeze = None
        check("冻结电池读取: %s" % e, False)
    if freeze is not None:
        ids = [e["voyage_id"] for e in freeze["battery"]]
        roles = [e["role"] for e in freeze["battery"]]
        check("battery 顺序 == 冻结文档序 %s（实际 %s）"
              % (list(BATTERY_DOC_ORDER), ids),
              tuple(ids) == BATTERY_DOC_ORDER)
        check("battery roles == %s（实际 %s）" % (list(BATTERY_DOC_ROLES), roles),
              tuple(roles) == BATTERY_DOC_ROLES)
        check("battery 航次域（duration≥600、kf_issues≥30、id 唯一）",
              all(e.get("duration_s", 0) >= 600 and e.get("kf_issues", 0) >= 30
                  for e in freeze["battery"]) and len(set(ids)) == len(ids))

    print("[selftest] 3) 3 航次 kf 表存在性（就绪检查，先于任何读取；"
          "缺失≠selftest 失败，但真跑/干跑启动时 fail-closed）")
    if freeze is not None:
        vids = [e["voyage_id"] for e in freeze["battery"]]
        kf_missing = []
        for v in vids:
            p = kf_table_path(v)
            if os.path.isfile(p):
                check("kf 表在位: %s" % os.path.relpath(p, REPO), True)
            else:
                kf_missing.append(p)
                print("WARN: kf 表尚未产出: %s（并行 worker 产出；缺文件时 runner "
                      "启动即 fail-closed 拒绝，错误信息含完整路径）"
                      % os.path.relpath(p, REPO))
        check("kf 表存在性检查机制执行完毕（缺失 %d/%d）"
              % (len(kf_missing), len(vids)), True)
        if kf_missing:
            print("[selftest] kf 表就绪状态: %d/%d 缺失 → 真跑前需产出；selftest "
                  "不因此失败（表可能尚未产出，本检查先于读取）"
                  % (len(kf_missing), len(vids)))

    print("[selftest] 4) CSV_COLS 与 C3 断言相等")
    check("CSV_COLS 逐列相等（列数=%d）" % len(CSV_COLS),
          list(CSV_COLS) == list(c3.CSV_COLS))
    check("CSV_COLS 关键列在位",
          CSV_COLS[0] == "tick" and CSV_COLS.count("bb_bh_attr") == 1
          and "bb_P_gen_ref" in CSV_COLS and "a2_P_gen_ref" in CSV_COLS
          and CSV_COLS[-1] == "board_ms")

    print("[selftest] 5) 电池级 gate 结构键完整性（构造 dict 自检，不落盘）")
    fake_entries = [
        {"voyage_id": "V%d" % i, "role": "R", "scenario_id": "S", "overall": True,
         "stop_reason": None, "formal_done": 30, "formal_ticks": 30,
         "board_lost": False,
         "g6": {k: True if k.endswith("_pass") else 0 for k in VOYAGE_G6_KEYS},
         "artifacts": {"csv": "c", "warmup_csv": "w", "gate": "g"}}
        for i in range(3)]
    fake_freeze = {"schema": "s4_c4_battery_freeze_v1",
                   "battery": [{"role": "R", "voyage_id": "V%d" % i}
                               for i in range(3)]}
    bg = build_battery_gate(["V0", "V1", "V2"], fake_entries, False, None,
                            fake_freeze, "0" * 64)
    check("电池 gate 顶层键完整（缺=%s）"
          % [k for k in BATTERY_GATE_KEYS if k not in bg],
          all(k in bg for k in BATTERY_GATE_KEYS))
    check("电池 gate 合取语义（全 PASS → true）", bg["overall"] is True)
    bg_false = build_battery_gate(
        ["V0", "V1", "V2"],
        [dict(e, overall=False) if e["voyage_id"] == "V1" else e
         for e in fake_entries], False, None, fake_freeze, "0" * 64)
    check("电池 gate 合取语义（单航次 park → false）", bg_false["overall"] is False)
    bg_stop = build_battery_gate(["V0", "V1", "V2"], fake_entries[:2], False,
                                 {"reason": "板失联", "at_voyage": "V1"},
                                 fake_freeze, "0" * 64)
    check("电池 gate 合取语义（全局停机 → false + skipped 记录）",
          bg_stop["overall"] is False and bg_stop["skipped_voyages"] == ["V2"])
    bg_dry = build_battery_gate(["V0", "V1", "V2"], fake_entries, True, None,
                                fake_freeze, "0" * 64)
    check("电池 gate 合取语义（dry → 恒 false）", bg_dry["overall"] is False)
    check("航次条目键完整（含 G6 摘要键）",
          all(k in e for e in fake_entries for k in VOYAGE_ENTRY_KEYS))
    tpl_gate = {"schema": "s4_c3_gate_v2", "overall": False, "stop_reason": None,
                "form": {"kf_table": {"path": "old", "sha256": "s", "n_rows": 1,
                                      "voyage_id": "v", "profile_code": 3,
                                      "profile_meaning": "PILOT_FIXTURE"},
                         "s20_mat": "old", "s20_mat_fixture_kind": "k"}}
    fin = finalize_voyage_gate(tpl_gate, "V0", "R_low", kf_table_path("V0"),
                               "0" * 64, stream_path("V0"))
    check("航次 gate = voyage_gate_v1 + freeze sha + 航次级 kf/s20 路径",
          fin["schema"] == "s4_c4_voyage_gate_v1"
          and fin["form"]["battery_freeze_sha256"] == "0" * 64
          and fin["form"]["kf_table"]["path"] == os.path.relpath(
              kf_table_path("V0"), REPO)
          and fin["form"]["s20_mat"] == os.path.relpath(stream_path("V0"), REPO)
          and fin["form"]["kf_table"]["sha256"] == "s")

    print("")
    if failures:
        print("SELFTEST: FAIL (%d/%d checks failed)"
              % (len(failures), n_pass + len(failures)))
        return False
    print("SELFTEST: PASS (%d checks)" % n_pass)
    return True


# ---------------------------------------------------------------- 主流程
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbserial-1120")
    ap.add_argument("--voyages", default=None,
                    help="逗号分隔 voyage_id（默认按冻结 battery 顺序）")
    ap.add_argument("--dry", action="store_true", help="无板干跑（overall 恒 false）")
    ap.add_argument("--selftest", action="store_true",
                    help="无板无植物自检（不写盘）")
    ap.add_argument("--plant-backend", choices=PLANT_BACKENDS, default="modelE",
                    help="modelE=默认冻结路径（pilot 烘焙 dylib）；streamE="
                         "S4-P3 流驱动变体（P3 冻结件 + --data-root，B1）")
    ap.add_argument("--data-root", default=None,
                    help="streamE 模式 formal .mat 目录（如 model/.../datasets_v3"
                         "/formal/iid；相对路径按仓库根解析）；modelE 默认路径忽略")
    args = ap.parse_args()

    if args.selftest:
        return 0 if _selftest() else 1

    # ---- S4-P3 B1：streamE 装配分支（默认 modelE 路径全部行为不变）----
    if args.plant_backend == "streamE":
        if not args.data_root:
            print("[p3] FAIL-CLOSED: --plant-backend streamE 需要显式 "
                  "--data-root（formal .mat 目录）")
            return 2
        results_dir = RESULTS_P3
        freeze_path = FREEZE_P3_PATH
        try:
            freeze = load_freeze_p3(freeze_path)
        except RuntimeError as e:
            print("[p3] FAIL-CLOSED: %s" % e)
            return 2
    else:
        results_dir = RESULTS
        freeze_path = FREEZE_PATH
        freeze = load_freeze()
    os.makedirs(results_dir, exist_ok=True)
    t_start_wall = time.time()

    # ---- 冻结件 + 航次顺序 + kf 表就绪（全部先于任何重初始化）----
    freeze_sha = c3.sha256_file(freeze_path)
    planned, role_of, scen_of = resolve_voyages(args, freeze)
    print("[p3/c4] plant-backend=%s 冻结件=%s sha=%s… 航次顺序=%s"
          % (args.plant_backend, os.path.relpath(freeze_path, c3.REPO),
             freeze_sha[:12], planned))
    kf_missing = check_kf_tables(planned)
    if kf_missing:
        for p in kf_missing:
            print("[p3/c4] FAIL-CLOSED: kf 表缺失: %s（并行 worker 产出后重跑）"
                  % os.path.relpath(p, c3.REPO))
        return 2
    if args.plant_backend == "streamE":
        stream_missing = [stream_bin_path(v) for v in planned
                          if not os.path.isfile(stream_bin_path(v))]
        if stream_missing:
            for p in stream_missing:
                print("[p3] FAIL-CLOSED: stream bin 缺失: %s"
                      "（tools/s4_p3_pack_stream.py 产出后重跑）"
                      % os.path.relpath(p, c3.REPO))
            return 2
        from plantstep_advancer import streamE_dylib_path
        dylib_missing = [streamE_dylib_path(v) for v in planned
                         if not os.path.isfile(streamE_dylib_path(v))]
        if dylib_missing:
            gate = build_pending_gate(planned, args.dry, freeze, freeze_sha,
                                      [os.path.relpath(p, c3.REPO)
                                       for p in dylib_missing])
            gate_path = os.path.join(results_dir, "s4_p3_gate.json")
            json.dump(gate, open(gate_path, "w"), indent=1, ensure_ascii=False)
            print("[p3] PENDING: PLANTSTEP_DYLIB_PENDING（%d/%d 航次 dylib 缺席，"
                  "待 B2 codegen+build 放行）-> %s"
                  % (len(dylib_missing), len(planned),
                     os.path.relpath(gate_path, c3.REPO)))
            return 3

    print("[c4] 初始化 law6/PMS host/B 链（law6 dylib + ORT 首载；"
          "keras 经首航次 A2 装配器载入）…")
    law = Law6()
    pms_bh = PmsHost()
    pms_a2 = PmsHost()
    from gen_ref_split_v2 import Chain
    chain = Chain()                          # 板源 OE int8 N1+N2 ORT 代理（跨航次复用）

    board = None
    if not args.dry:
        board = BoardClient(args.port)
        if not board.ping():
            board.close()
            stopped = {"reason": "PING 失败（板失联；需按加载通道重载后重跑）",
                       "at_voyage": None}
            entries = []
            gate = build_battery_gate(planned, entries, args.dry, stopped,
                                      freeze, freeze_sha)
            json.dump(gate, open(os.path.join(results_dir, "s4_c4_gate.json"),
                                 "w"),
                      indent=1, ensure_ascii=False)
            print("[c4] PARK: PING 失败（板失联）-> 电池 gate（overall=false）已写")
            return 2
        print("[c4] 板 PING OK（S4Smoke SRAM 形态在环；每航次正式段前 CMD_RESET）")

    # ---- 顺序跑完冻结电池全部航次 ----
    entries = []
    stopped_global = None
    a2 = None                                # A2 单实例（keras/TF 只初始化一次）
    for vid in planned:
        print("[c4] ===== 航次 %s（role=%s，%d/%d）====="
              % (vid, role_of[vid], len(entries) + 1, len(planned)))
        spath, stream, meta = load_stream(
            vid, args.data_root,
            resample_100hz=(args.plant_backend == "streamE"))
        if a2 is None:
            a2 = A2Assembler(stream)
        else:
            # A2Assembler 跨航次状态仅 stream + 双端缓冲（公开属性），重置语义
            # 等价新建；TF set_visible_devices 初始化后不可二次调用。
            a2.stream = stream
            a2.t_buf.clear()
            for dq in a2.v_buf.values():
                dq.clear()
        kf_path = kf_table_path(vid)
        kft = load_kf_table(kf_path)
        if kft["voyage_id"] != vid:
            print("[c4] FAIL-CLOSED: kf 表 voyage_id=%s != %s（%s）"
                  % (kft["voyage_id"], vid, kf_path))
            stopped_global = {"reason": "kf 表 voyage_id 与航次不符（fail-closed）",
                              "at_voyage": vid}
            break
        if not (kft["issue_first"] == 300.0 and kft["issue_step"] == 10.0):
            print("[c4] FAIL-CLOSED: kf 表 issue 网格异常 %g:%g（期望 300:10）"
                  % (kft["issue_first"], kft["issue_step"]))
            stopped_global = {"reason": "kf 表 issue 网格非 300:10（fail-closed）",
                              "at_voyage": vid}
            break

        res = run_voyage(vid, role_of[vid], spath, stream, meta, a2, kft, kf_path,
                         board, law, pms_bh, pms_a2, chain, args.dry, freeze_sha,
                         plant_backend=args.plant_backend,
                         results_dir=results_dir)
        res["scenario_id"] = scen_of[vid]    # 冻结件元数据（权威）覆盖 mat meta
        entries.append({k: res[k] for k in VOYAGE_ENTRY_KEYS})

        if res["board_lost"]:
            stopped_global = {"reason": res["stop_reason"], "at_voyage": vid}
            break
        # 单航次 park（未归因停机条件）→ 合取 false，继续下一航次

    if board is not None:
        board.close()

    gate = build_battery_gate(planned, entries, args.dry, stopped_global,
                              freeze, freeze_sha)
    gate_path = os.path.join(results_dir, "s4_c4_gate.json")
    json.dump(gate, open(gate_path, "w"), indent=1, ensure_ascii=False)
    print("[c4] 电池完成 %d/%d 航次；stopped_global=%s"
          % (len(entries), len(planned), stopped_global))
    print("[c4] 电池 overall:", gate["overall"], "->", gate_path)
    print("[c4] wall=%.0fs" % (time.time() - t_start_wall))
    return 0 if gate["overall"] else 1


if __name__ == "__main__":
    sys.exit(main())

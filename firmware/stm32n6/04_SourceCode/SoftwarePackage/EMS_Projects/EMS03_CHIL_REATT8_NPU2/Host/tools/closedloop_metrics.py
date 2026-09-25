#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""closedloop_metrics.py — T-615：四臂两场景统一指标汇总与 SOC 公平性判定（纯 stdlib）。

spec ems01-chil-v3-closedloop（FR-610/FR-611/AC-605，design §8.1/§8.2/§8.3）。

四臂：(a) NPU 板载闭环 / (b) REF 板载闭环 / (b′) 原生 law=7 / (c) 原生 law=5；
两场景：S20、voyage002。输入全部为既有只读轨迹 CSV / JSON（无板操作、无 MATLAB），
本脚本只做汇总计算，输出机器可读指标 JSON + 人类可读 summary MD。
退出码语义：0 = 完成（汇总任务不设门禁 FAIL 语义；仪器级①②门禁项的 pass/fail
如实记录进 JSON 的 instrument_grade_* 块，由主 Agent 判读）。

指标定义（design §8.1，逐臂逐场景，禁新造公式）：
  fuel_raw      = fuel_total_kg(N-1)                          # 末拍累计油耗
  SOC_end       = SOC(N-1)
  fuel_corr     = fuel_raw + (0.60 − SOC_end) × BatCap × K_EQ
                  BatCap = 1071.36 kWh（Bat_Cap_kWh）、K_EQ = 0.4774 kg/kWh
                  —— 同式同常数单一来源：model 仓库 scripts/21_run_v3_native_arms.m:94-95
                    （其注释溯源 64_run_matrix_v2 同式 64:239 / 66 同口径；design §8.1）
  v_track_rmse  = sqrt(mean((v_ship − v_cmd)²))               # 全任务
  ê 生效占比     = mean(K>0 ∧ 非回退)：
                  板载臂 (a)/(b)：K>0 且 flags&0x02(FALLBACK)==0（flags 位镜像
                  ems_proto.h：STALE=0x01/FALLBACK=0x02/CLS_BAD=0x04/HELD=0x08）；
                  原生 law=7 (b′)：CSV 无 flags 列，取 mean(K>0)；
                  law=5 (c)：恒 0（无 EML 预测，design §9；CSV 中 K=1 为中性值，
                  不得据 K 列误计为生效）
  临界工况       = SOC 下穿 ASSUMED_SOC_DisDerate_Start=0.25（config/Model_Parameters.m:123）
                  的事件数 / 时长；n_undervoltage 按可得列计算：
                  原生臂 CSV 无 alarm 列 → 记 null（另附 native_arms_summary.json
                  的 n_undervoltage 作交叉核对值，注明来源）；
                  板载臂 CSV 无 Vdc alarm 列 → 按 flags&0x08(HELD) 保持拍数计
                  （主 Agent 指定口径；HELD 语义为输出保持、非 Vdc 越限，
                  字段附 source 说明）。

SOC 公平性（design §8.2）：主对比（臂 X vs Y，同场景）有效条件
  |SOC_end(X) − SOC_end(Y)| ≤ 0.02（±2% 容差带）；
超带标 SOC-UNFAIR，以 fuel_corr 差值作次级结论并显式标注；
每臂同时输出 fuel_raw 与 fuel_corr + SOC_end（禁只报单一数字）。

两两对比矩阵（design §8.2/§9）：同场景内 (a)/(b)/(b′) vs (c)（NN 增益与 (b′) 复验）
+ (a) vs (b)（零混杂 NN 移植代价）；每对比给 fuel_raw 差 / fuel_corr 差 / SOC_end 差 /
公平性判定。差值符号约定：diff = 臂X − 臂Y（X 为对比前者）。

仪器级①②门禁复述（FR-610，design §5；T-613 现场已核对，此处正式落 JSON）：
  ① 闭环 (b) vs 开环参考（Host/reference/v3_gp1_<scn>_replay.csv，其 P_gen_ref_kW
     即 ref_G2 驱动序列，G-P1 T-602 gate 已证与 ref_G2 一致）；
  ② 闭环 (b) vs 原生 (b′)；
  阈值（两表同）：SOC_end |Δ| ≤ 0.005；fuel_raw 相对偏差 ≤ 1%；v_ship RMSE ≤ 0.01 m/s；
  P_gen 网格一致率（逐拍 |ΔP| ≤ 1 网格步占比）≥ 95%；数值健康（无 NaN / meta 无 error）。

层③报告性输出（FR-612/§8.3，不作门禁）：闭环 (a) 臂 vs ref_G2 同拍 P_gen 分布
（exact 率 / ±1 网格率 / max|Δ|；ref 取 gp1 replay 的 P_gen_ref_kW 列）。

FR-508 闭环代价汇总：从 Host/fault_out/closedloop_npu/closedloop/CL_*.json 汇总
四场景（F1=CL_NAN NaN 输入拍、F2=CL_STALL 停发、F3=CL_GARBAGE 垃圾帧、
F4=CL_RESET 复位恢复）：F1/F4 的 FALLBACK 段统计 + F2 STALE|HELD + F3 会话存活，
进 JSON 的 critical_section 块（STALE|HELD 逐拍位统计取同目录 harness 轨迹副本 CSV）。

输入（全部只读，相对 EMS01 工程根 / model 仓库根）：
  板载 (a)/(b)   Host/results/closedloop/<scn>/closedloop_<scn>_{npu,ref}.csv（19 列）
  原生 (b′)/(c)  <model_ws>/results/ecms_phase25/closedloop/native_arms/
                 V3_{B,C}_<scn>_native_law{7,5}.csv（22 列，P_gen 列名 P_gen_ref_kW）
  参考(仪器级①) Host/reference/v3_gp1_<scn>_replay.csv（13 列）+ v3_gp1_<scn>_gate.json
  故障注入       Host/fault_out/closedloop_npu/closedloop/CL_*.json + harness 轨迹副本 CSV
  交叉核对       <model_ws>/results/ecms_phase25/fw_reference/export_summary.json、
                 native_arms/native_arms_summary.json

输出（脚本内固定相对路径）：
  Host/results/closedloop/closedloop_metrics.json
  Host/results/closedloop/closedloop_metrics_summary.md

运行环境：Python 3 纯 stdlib；路径纪律：自 __file__ 上溯推导工程/仓库根 + pwd 兜底，
禁止硬编码绝对路径（AGENTS.md §20.2）。只读输入，不修改任何既有文件。
"""
from __future__ import annotations

import csv
import glob
import hashlib
import json
import math
import os
import sys
from datetime import datetime, timezone

# ---------------------------------------------------------------------------
# 固定常量（Fixed Decisions；单一来源，禁新造公式）
# ---------------------------------------------------------------------------
BAT_CAP_KWH = 1071.36        # Bat_Cap_kWh（21_run_v3_native_arms.m:94；64:239 同式同常数）
K_EQ_KG_PER_KWH = 0.4774     # kg/kWh（21_run_v3_native_arms.m:95；64 同口径）
SOC_REF_CHARGE = 0.60        # fuel_corr 电荷维持参考 SOC（design §8.1）
GRID_STEP_KW = 20.0          # P_gen 离散网格步（Host/ems_inject.py GRID_STEP_KW，design 固定）
GRID_EPS_KW = 1e-9           # 网格比较容差
SOC_DISDERATE_START = 0.25   # ASSUMED_SOC_DisDerate_Start（Model_Parameters.m:123）
SOC_FAIR_BAND = 0.02         # design §8.2 SOC 公平性 ±2% 容差带

# 仪器级①②阈值（design §5 两表同；先验设定，FAIL 排查不放宽）
IG_THRESHOLDS = {
    "soc_end_abs_delta_max": 0.005,
    "fuel_rel_delta_max": 0.01,
    "v_ship_rmse_max_mps": 0.01,
    "p_gen_grid_match_rate_min": 0.95,
}

SCENARIOS = ("S20", "voyage002")
ONBOARD_IMAGES = {"a_npu_onboard": "npu", "b_ref_onboard": "ref"}
NATIVE_ARM_FILES = {
    "bp_native_law7": ("V3_B_{scn}_native_law7.csv", 7),
    "c_native_law5": ("V3_C_{scn}_native_law5.csv", 5),
}
ARM_ORDER = ("a_npu_onboard", "b_ref_onboard", "bp_native_law7", "c_native_law5")
ARM_LABELS = {
    "a_npu_onboard": "(a) NPU 板载闭环",
    "b_ref_onboard": "(b) REF 板载闭环",
    "bp_native_law7": "(b′) 原生 law=7",
    "c_native_law5": "(c) 原生 law=5",
}
# model 仓库 ref_G2 冻结缓存 run_id（export_summary.json 交叉核对键）
G2_RUN_IDS = {"S20": "G2_S20_composite_voyage_matrix", "voyage002": "G2_test_voyage_002_v2"}
# 任务给定静态锚点（22 号产物 / T-602 gate：gp1 replay S20 fuel_raw）
GP1_FUEL_RAW_ANCHOR = {"S20": 328.094641944243}

# FR-508 四场景映射（ems_fault_inject.py F1~F4 ↔ closed-loop 注入 CL_*.json）
FAULT_SCENARIOS = (
    ("F1_nan_ticks", "CL_NAN.json", "F1 fault_nan_ticks：NaN 输入拍 → FALLBACK（段统计）"),
    ("F2_stall_ticks", "CL_STALL.json", "F2 fault_stall_ticks：停发 → STALE|HELD + P 保持"),
    ("F3_garbage_frames", "CL_GARBAGE.json", "F3 fault_garbage_frames：垃圾/坏 CRC 帧 → 会话存活"),
    ("F4_reset_recovery", "CL_RESET.json", "F4 fault_reset_recovery：复位 → INIT 平 fill + FALLBACK（段统计）"),
)

FLAGS_STALE, FLAGS_FALLBACK, FLAGS_CLS_BAD, FLAGS_HELD = 0x01, 0x02, 0x04, 0x08

# 运行期根目录（main() 内赋值：(ems01_root, repo_root, model_ws)）
_ROOTS = (None, None, None)


# ---------------------------------------------------------------------------
# 路径推导（AGENTS.md §20.2：__file__ 上溯 + pwd 兜底，禁绝对路径）
# ---------------------------------------------------------------------------
def _find_root(start_dir, marker_fn):
    d = os.path.abspath(start_dir)
    while True:
        if marker_fn(d):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            return None
        d = parent


def _is_ems01_root(d):
    return os.path.isdir(os.path.join(d, "Appli")) and os.path.isdir(os.path.join(d, "Host"))


def _is_repo_root(d):
    return (os.path.isfile(os.path.join(d, "AGENTS.md"))
            and os.path.isdir(os.path.join(d, "firmware"))
            and os.path.isdir(os.path.join(d, "model")))


def find_model_ws(repo_root):
    """定位 ModelD 仓库工作区（含 results/ecms_phase25/closedloop/native_arms）。"""
    model_dir = os.path.join(repo_root, "model")
    if not os.path.isdir(model_dir):
        return None
    hits = []
    for name in sorted(os.listdir(model_dir)):
        cand = os.path.join(model_dir, name)
        if os.path.isdir(cand) and os.path.isdir(
                os.path.join(cand, "results", "ecms_phase25", "closedloop", "native_arms")):
            hits.append(cand)
    return hits[0] if hits else None


def find_roots():
    """返回 (ems01_root, repo_root, model_ws_root)；推导失败返回 None 由调用方报错。"""
    here = os.path.dirname(os.path.abspath(__file__))
    ems01 = _find_root(here, _is_ems01_root)
    if ems01 is None:  # pwd 兜底（§20.2：mfilename/脚本顶层为空时降级）
        ems01 = _find_root(os.getcwd(), _is_ems01_root)
    repo = _find_root(here, _is_repo_root)
    if repo is None:
        repo = _find_root(os.getcwd(), _is_repo_root)
    model_ws = find_model_ws(repo) if repo is not None else None
    return ems01, repo, model_ws


# ---------------------------------------------------------------------------
# CSV 读取与列名映射
# ---------------------------------------------------------------------------
# 各表头 → 统一字段映射（tick/t/SOC/v_cmd/v_ship/P_gen/e_hat/K/flags/fuel）
COLMAP_ONBOARD = {   # 板载闭环 19 列（ems_closedloop.py CSV_COLS）
    "tick": "tick", "t": "t_s", "SOC": "SOC", "v_cmd": "v_cmd_mps",
    "v_ship": "v_ship_mps", "P_gen": "P_gen_kW", "e_hat": "e_hat", "K": "K",
    "flags": "flags", "fuel": "fuel_total_kg",
}
COLMAP_NATIVE = {    # 原生臂 22 列（66 的 21 列 + fuel_total_kg；P_gen 列名 P_gen_ref_kW）
    "tick": "tick", "t": "t", "SOC": "SOC", "v_cmd": "v_cmd_mps",
    "v_ship": "v_ship_mps", "P_gen": "P_gen_ref_kW", "e_hat": "e_hat", "K": "K",
    "flags": None, "fuel": "fuel_total_kg",
}
COLMAP_GP1REF = {    # gp1 开环参考 13 列（P_gen 列名 P_gen_ref_kW）
    "tick": "tick", "t": "t", "SOC": "SOC", "v_cmd": "v_cmd_mps",
    "v_ship": "v_ship_mps", "P_gen": "P_gen_ref_kW", "e_hat": None, "K": None,
    "flags": None, "fuel": "fuel_total_kg",
}


def _to_float(s):
    if s is None:
        return None
    s = s.strip()
    if s == "":
        return None
    try:
        return float(s)
    except ValueError:
        return None


def _to_flags(s):
    v = _to_float(s)
    if v is None or math.isnan(v):
        return None
    return int(v) & 0xFF


def load_track_csv(path, kind):
    """读轨迹 CSV → 按 tick 升序记录列表（统一字段），并返回实际表头。

    kind: onboard（19 列板载）/ native（22 列原生）/ gp1ref（13 列开环参考）。
    表头缺必需列时抛 ValueError（列出实际表头，供 Escalate 上报）。
    """
    colmap = {"onboard": COLMAP_ONBOARD, "native": COLMAP_NATIVE, "gp1ref": COLMAP_GP1REF}[kind]
    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        raw_fields = list(reader.fieldnames or [])
        header = [h.strip() for h in raw_fields]
        missing = [logical for logical, physical in colmap.items()
                   if physical is not None and physical not in header]
        if missing:
            raise ValueError(
                "CSV 列名与假设不符（kind=%s, path=%s）：缺列 %s；实际表头=%s"
                % (kind, path, missing, header))
        recs = []
        for row in reader:
            tick_v = _to_float(row.get(colmap["tick"]))
            if tick_v is None or math.isnan(tick_v):
                continue  # 空行/尾行防御

            def get(key):
                phys = colmap[key]
                return row.get(phys) if phys is not None else None

            rec = {
                "tick": int(tick_v),
                "t": _to_float(get("t")),
                "SOC": _to_float(get("SOC")),
                "v_cmd": _to_float(get("v_cmd")),
                "v_ship": _to_float(get("v_ship")),
                "P_gen": _to_float(get("P_gen")),
                "e_hat": _to_float(get("e_hat")),
                "K": _to_float(get("K")),
                "flags": _to_flags(get("flags")),
                "fuel": _to_float(get("fuel")),
            }
            recs.append(rec)
    if not recs:
        raise ValueError("CSV 无有效数据行：%s" % path)
    recs.sort(key=lambda r: r["tick"])
    ticks = [r["tick"] for r in recs]
    if len(set(ticks)) != len(ticks):
        raise ValueError("CSV tick 重复：%s" % path)
    return recs, header


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------------------
# §8.1 指标计算
# ---------------------------------------------------------------------------
def soc_downcross_events(recs, threshold):
    """SOC 序列下穿 threshold 的事件列表（起止 tick / 持续拍数 / 最低点）。"""
    events = []
    in_event = False
    ev = None
    for r in recs:
        soc = r["SOC"]
        below = soc is not None and not math.isnan(soc) and soc < threshold
        if below and not in_event:
            in_event = True
            ev = {"start_tick": r["tick"], "n_ticks": 1, "soc_min": soc, "end_tick": r["tick"]}
        elif below and in_event:
            ev["n_ticks"] += 1
            ev["end_tick"] = r["tick"]
            ev["soc_min"] = min(ev["soc_min"], soc)
        elif not below and in_event:
            in_event = False
            events.append(ev)
            ev = None
    if in_event and ev is not None:
        events.append(ev)
    return events


def rmse(pairs):
    """pairs: [(x, y)]，剔除任一为 NaN/None 的对；返回 (rmse, n_used)。"""
    used = [(x, y) for (x, y) in pairs
            if x is not None and y is not None
            and not math.isnan(x) and not math.isnan(y)]
    if not used:
        return None, 0
    acc = 0.0
    for x, y in used:
        acc += (x - y) * (x - y)
    return math.sqrt(acc / len(used)), len(used)


def compute_arm_metrics(arm_id, kind, csv_path, recs, meta=None):
    """单臂单场景统一指标（design §8.1）。kind: onboard / native7 / native5。"""
    n = len(recs)
    last = recs[-1]
    fuel_raw = last["fuel"]
    soc_end = last["SOC"]
    fuel_corr = fuel_raw + (SOC_REF_CHARGE - soc_end) * BAT_CAP_KWH * K_EQ_KG_PER_KWH

    v_rmse, n_v = rmse([(r["v_ship"], r["v_cmd"]) for r in recs])

    # ê 生效占比（按臂类型的定义差异，见文件头注释）
    if kind == "native5":
        e_ratio = 0.0
        e_def = "law=5 恒 0（无 EML 预测，design §9；CSV K=1 为中性值不据其计算）"
        n_e_used, n_e_invalid = n, 0
    elif kind == "native7":
        n_e_used, num, n_e_invalid = 0, 0, 0
        for r in recs:
            k = r["K"]
            if k is None or math.isnan(k):
                n_e_invalid += 1
                continue
            n_e_used += 1
            if k > 0:
                num += 1
        e_ratio = (num / n_e_used) if n_e_used else None
        e_def = "mean(K>0)（原生 CSV 无 flags 列）"
    else:  # onboard
        n_e_used, num, n_e_invalid = 0, 0, 0
        for r in recs:
            k, fl = r["K"], r["flags"]
            if k is None or math.isnan(k) or fl is None:
                n_e_invalid += 1
                continue
            n_e_used += 1
            if k > 0 and (fl & FLAGS_FALLBACK) == 0:
                num += 1
        e_ratio = (num / n_e_used) if n_e_used else None
        e_def = "mean(K>0 ∧ flags&0x02(FALLBACK)==0)"

    # 临界工况：SOC 下穿降额阈值
    events = soc_downcross_events(recs, SOC_DISDERATE_START)
    n_below = sum(ev["n_ticks"] for ev in events)
    t_first = recs[0]["t"]
    t_last = last["t"]
    dt = None
    if t_first is not None and t_last is not None and n > 1 and last["tick"] != recs[0]["tick"]:
        dt = (t_last - t_first) / (last["tick"] - recs[0]["tick"])
    soc_disderate_duration_s = n_below * dt if dt is not None else None

    m = {
        "arm": arm_id,
        "kind": kind,
        "arm_label": ARM_LABELS[arm_id],
        "source_csv": os.path.relpath(csv_path, _ROOTS[1]) if _ROOTS[1] else csv_path,
        "n_ticks": n,
        "tick_first": recs[0]["tick"],
        "tick_last": last["tick"],
        "dt_s": dt,
        "fuel_raw_kg": fuel_raw,
        "SOC_end": soc_end,
        "fuel_corr_kg": fuel_corr,
        "SOC_min": min(r["SOC"] for r in recs if r["SOC"] is not None and not math.isnan(r["SOC"])),
        "v_track_rmse_mps": v_rmse,
        "v_track_rmse_n_used": n_v,
        "e_hat_active_ratio": e_ratio,
        "e_hat_active_ratio_definition": e_def,
        "e_hat_n_invalid": n_e_invalid,
        "soc_disderate_start": SOC_DISDERATE_START,
        "n_soc_disderate_events": len(events),
        "soc_disderate_duration_s": soc_disderate_duration_s,
        "soc_disderate_events": events,
    }

    # 数值健康：关键列 NaN 检查 + P_gen 网格隶属残差
    nan_counts = {}
    for key in ("SOC", "v_ship", "v_cmd", "P_gen", "fuel"):
        nan_counts[key] = sum(1 for r in recs if r[key] is None or math.isnan(r[key]))
    grid_res = 0.0
    for r in recs:
        p = r["P_gen"]
        if p is not None and not math.isnan(p):
            grid_res = max(grid_res, abs(p - round(p / GRID_STEP_KW) * GRID_STEP_KW))
    m["numeric_health_nan_counts"] = nan_counts
    m["p_gen_grid_residual_max_kW"] = grid_res

    # 板载臂：flags 位统计 + meta 健康；原生臂：n_undervoltage 记 null（无 alarm 列）
    if kind == "onboard":
        m["n_fallback_ticks"] = sum(1 for r in recs if r["flags"] is not None
                                    and (r["flags"] & FLAGS_FALLBACK))
        m["n_stale_ticks"] = sum(1 for r in recs if r["flags"] is not None
                                 and (r["flags"] & FLAGS_STALE))
        m["n_cls_bad_ticks"] = sum(1 for r in recs if r["flags"] is not None
                                   and (r["flags"] & FLAGS_CLS_BAD))
        m["n_held_ticks"] = sum(1 for r in recs if r["flags"] is not None
                                and (r["flags"] & FLAGS_HELD))
        # n_undervoltage：板载 CSV 无 Vdc/alarm 列 → 按 HELD 保持拍数（主 Agent 指定口径）
        m["n_undervoltage"] = m["n_held_ticks"]
        m["n_undervoltage_source"] = ("onboard CSV flags&0x08(HELD) 保持拍数（板载 CSV 无 "
                                      "Vdc/alarm 列；HELD 语义为输出保持拍，非 Vdc 越限，"
                                      "解释权在主 Agent）")
    else:
        m["n_undervoltage"] = None
        m["n_undervoltage_source"] = ("native CSV 无 alarm 列 → null"
                                      "（交叉核对值见 native_summary_crosscheck）")

    if meta is not None:
        m["meta_health"] = {
            "status": meta.get("status"),
            "error": meta.get("error", ""),
            "n_events": len(meta.get("events") or []),
            "rows": meta.get("rows"),
            "rows_matches_csv": meta.get("rows") == n,
        }
    return m


# ---------------------------------------------------------------------------
# 两两对比（§8.2 SOC 公平性）
# ---------------------------------------------------------------------------
PAIRWISE_SPECS = (
    ("a_npu_onboard", "c_native_law5", "NN 增益：(a) NPU 板载 vs (c) law=5 基线"),
    ("b_ref_onboard", "c_native_law5", "NN 增益：(b) REF 板载 vs (c) law=5 基线"),
    ("bp_native_law7", "c_native_law5", "NN 增益复验：(b′) 原生 law=7 vs (c) law=5 基线"),
    ("a_npu_onboard", "b_ref_onboard", "零混杂 NN 移植代价：(a) NPU vs (b) REF"),
)


def make_comparison(armx, army, x_id, y_id, purpose):
    """臂 X vs 臂 Y 对比（diff = X − Y；SOC 公平性按 design §8.2）。"""
    soc_diff = armx["SOC_end"] - army["SOC_end"]
    fair = abs(soc_diff) <= SOC_FAIR_BAND
    fuel_raw_diff = armx["fuel_raw_kg"] - army["fuel_raw_kg"]
    fuel_corr_diff = armx["fuel_corr_kg"] - army["fuel_corr_kg"]
    return {
        "pair": "%s vs %s" % (x_id, y_id),
        "purpose": purpose,
        "fuel_raw_diff_kg": fuel_raw_diff,
        "fuel_corr_diff_kg": fuel_corr_diff,
        "SOC_end_diff": soc_diff,
        "SOC_end_x": armx["SOC_end"],
        "SOC_end_y": army["SOC_end"],
        "fairness": {
            "band_abs_soc": SOC_FAIR_BAND,
            "abs_soc_end_diff": abs(soc_diff),
            "verdict": "FAIR" if fair else "SOC-UNFAIR",
            "primary_conclusion": (
                "带内：fuel_raw 差 %+.6f kg 即主结论（SOC 公平）" % fuel_raw_diff
                if fair else
                "SOC-UNFAIR：|ΔSOC_end|=%.6f 超出 ±%.2f 容差带，fuel_raw 差不作主结论"
                % (abs(soc_diff), SOC_FAIR_BAND)),
            "secondary_conclusion_fuel_corr_diff_kg": fuel_corr_diff,
            "secondary_note": ("电荷维持修正口径（与参考链同式同常数，design §8.2）；"
                               "超带时为次级结论，带内时与 fuel_raw 差同号供参照"),
        },
    }


# ---------------------------------------------------------------------------
# 仪器级①②（design §5，FR-610 复述落 JSON）
# ---------------------------------------------------------------------------
def _grid_match_stats(pairs):
    """返回 (|ΔP|≤1 网格步占比, n_used, max|ΔP|)。"""
    used = [(x, y) for (x, y) in pairs
            if x is not None and y is not None and not math.isnan(x) and not math.isnan(y)]
    if not used:
        return None, 0, None
    n_ok = 0
    max_dp = 0.0
    for x, y in used:
        dp = abs(x - y)
        max_dp = max(max_dp, dp)
        if dp <= GRID_STEP_KW + GRID_EPS_KW:
            n_ok += 1
    return n_ok / len(used), len(used), max_dp


def instrument_grade(arm_b, ref_recs, ref_label, ref_fuel_raw, ref_soc_end, extra_health=None):
    """闭环 (b) vs 参考轨迹（gp1 replay 或原生 (b′)）的仪器级门禁复述（tick 交集对齐）。"""
    ref_by_tick = {r["tick"]: r for r in ref_recs}
    common = [r["tick"] for r in arm_b["_recs"] if r["tick"] in ref_by_tick]
    xb = {r["tick"]: r for r in arm_b["_recs"]}
    v_r, _n_v = rmse([(xb[t]["v_ship"], ref_by_tick[t]["v_ship"]) for t in common])
    rate, _n_p, max_dp = _grid_match_stats(
        [(xb[t]["P_gen"], ref_by_tick[t]["P_gen"]) for t in common])

    soc_delta = abs(arm_b["SOC_end"] - ref_soc_end)
    fuel_rel = (abs(arm_b["fuel_raw_kg"] - ref_fuel_raw) / abs(ref_fuel_raw)
                if ref_fuel_raw else None)

    items = [
        {"metric": "soc_end_abs_delta", "value": soc_delta,
         "threshold": IG_THRESHOLDS["soc_end_abs_delta_max"], "cmp": "<=",
         "pass": bool(soc_delta <= IG_THRESHOLDS["soc_end_abs_delta_max"])},
        {"metric": "fuel_raw_rel_delta", "value": fuel_rel,
         "threshold": IG_THRESHOLDS["fuel_rel_delta_max"], "cmp": "<=",
         "pass": bool(fuel_rel is not None
                      and fuel_rel <= IG_THRESHOLDS["fuel_rel_delta_max"])},
        {"metric": "v_ship_rmse_mps", "value": v_r,
         "threshold": IG_THRESHOLDS["v_ship_rmse_max_mps"], "cmp": "<=",
         "pass": bool(v_r is not None and v_r <= IG_THRESHOLDS["v_ship_rmse_max_mps"])},
        {"metric": "p_gen_grid_match_rate", "value": rate,
         "threshold": IG_THRESHOLDS["p_gen_grid_match_rate_min"], "cmp": ">=",
         "pass": bool(rate is not None and rate >= IG_THRESHOLDS["p_gen_grid_match_rate_min"])},
    ]
    # 数值健康：(b) 关键列 NaN + 网格隶属 + meta 健康（+ 参考侧健康可选）
    health = (all(v == 0 for v in arm_b["numeric_health_nan_counts"].values())
              and arm_b.get("p_gen_grid_residual_max_kW", 1e9) <= GRID_EPS_KW)
    mh = arm_b.get("meta_health")
    if mh is not None:
        health = health and mh.get("status") == "completed" and not mh.get("error") \
            and mh.get("rows_matches_csv", False)
    if extra_health is not None:
        health = health and extra_health
    items.append({"metric": "numeric_health", "value": health, "threshold": True,
                  "cmp": "==", "pass": bool(health)})
    return {
        "arm_b": "b_ref_onboard",
        "reference": ref_label,
        "n_ticks_compared": len(common),
        "n_ticks_b": arm_b["n_ticks"],
        "n_ticks_ref": len(ref_recs),
        "fuel_raw_ref_kg": ref_fuel_raw,
        "SOC_end_ref": ref_soc_end,
        "max_abs_dP_kW": max_dp,
        "thresholds": dict(IG_THRESHOLDS),
        "items": items,
        "overall": all(it["pass"] for it in items),
    }


# ---------------------------------------------------------------------------
# 层③报告性输出（FR-612/§8.3，(a) vs ref_G2 同拍 P_gen 分布，不作门禁）
# ---------------------------------------------------------------------------
def layer3_p_gen_distribution(arm_a, ref_recs):
    ref_by_tick = {r["tick"]: r for r in ref_recs}
    common = [r["tick"] for r in arm_a["_recs"] if r["tick"] in ref_by_tick]
    xa = {r["tick"]: r for r in arm_a["_recs"]}
    n_exact = 0
    n_plus1 = 0
    max_dp = 0.0
    max_dp_tick = None
    n_beyond = 0
    n_beyond_startup = 0  # 启动瞬态（tick ≤ 20）内的超 1 网格拍数
    for t in common:
        pa = xa[t]["P_gen"]
        pr = ref_by_tick[t]["P_gen"]
        if pa is None or pr is None or math.isnan(pa) or math.isnan(pr):
            continue
        dp = abs(pa - pr)
        if dp > max_dp:
            max_dp = dp
            max_dp_tick = t
        if dp > GRID_STEP_KW + GRID_EPS_KW:
            n_beyond += 1
            if t <= 20:
                n_beyond_startup += 1
        if dp <= GRID_EPS_KW:
            n_exact += 1
        if dp <= GRID_STEP_KW + GRID_EPS_KW:
            n_plus1 += 1
    n_cmp = len(common)
    return {
        "report_only": True,
        "note": ("层③报告性输出（FR-612/design §8.3），不作门禁。闭环 (a) 自 v_ship=0 "
                 "初值起动（首拍 INIT fill/HELD），开环 ref 序列无此瞬态；超 1 网格拍"
                 "集中于起动瞬态，其余为零散 λ 网格边界翻转（design §4.4 机制）"),
        "arm": "a_npu_onboard",
        "ref_source": ("Host/reference/v3_gp1_<scn>_replay.csv:P_gen_ref_kW"
                       "（≡ref_G2 P_gen 驱动序列，G-P1 T-602 gate fuel_rel=0）"),
        "n_ticks_compared": n_cmp,
        "exact_rate": (n_exact / n_cmp) if n_cmp else None,
        "plus1_grid_rate": (n_plus1 / n_cmp) if n_cmp else None,
        "max_abs_dP_kW": max_dp,
        "max_abs_dP_tick": max_dp_tick,
        "n_ticks_beyond_1grid": n_beyond,
        "n_ticks_beyond_1grid_startup_le20": n_beyond_startup,
    }


# ---------------------------------------------------------------------------
# FR-508 闭环代价（critical_section）
# ---------------------------------------------------------------------------
def _parse_dump_evt_stats(dump_lines):
    evt_total = evt_kept = None
    for line in dump_lines or []:
        if "EVT total=" in line:
            for tok in line.split():
                if tok.startswith("total="):
                    evt_total = int(tok.split("=")[1])
                elif tok.startswith("kept="):
                    evt_kept = int(tok.split("=")[1])
    return {"evt_total": evt_total, "evt_kept": evt_kept}


def fault_harness_flag_stats(csv_path, fault_tick=None):
    """harness 轨迹副本 CSV 的 flags 位统计（STALE/HELD/FALLBACK/CLS_BAD）+ P 保持核验。"""
    recs, _ = load_track_csv(csv_path, "onboard")
    by_tick = {r["tick"]: r for r in recs}
    stats = {
        "file": os.path.basename(csv_path),
        "n_ticks": len(recs),
        "n_stale_ticks": sum(1 for r in recs if r["flags"] is not None
                             and (r["flags"] & FLAGS_STALE)),
        "n_held_ticks": sum(1 for r in recs if r["flags"] is not None
                             and (r["flags"] & FLAGS_HELD)),
        "n_fallback_ticks": sum(1 for r in recs if r["flags"] is not None
                                 and (r["flags"] & FLAGS_FALLBACK)),
        "n_cls_bad_ticks": sum(1 for r in recs if r["flags"] is not None
                                and (r["flags"] & FLAGS_CLS_BAD)),
        "flags_at_fault_tick": by_tick[fault_tick]["flags"] if fault_tick in by_tick else None,
    }
    if fault_tick is not None and fault_tick in by_tick and (fault_tick - 1) in by_tick:
        p0 = by_tick[fault_tick - 1]["P_gen"]
        p1 = by_tick[fault_tick]["P_gen"]
        stats["p_gen_hold_at_fault_tick"] = (
            None if p0 is None or p1 is None or math.isnan(p0) or math.isnan(p1)
            else bool(abs(p1 - p0) <= 1e-6))
    return stats


def build_critical_section(fault_dir):
    out = {
        "source_dir": os.path.relpath(fault_dir, _ROOTS[1]) if _ROOTS[1] else fault_dir,
        "image": "npu",
        "mapping_note": ("F1=CL_NAN（NaN 输入拍）/ F2=CL_STALL（停发）/ F3=CL_GARBAGE（垃圾帧）/ "
                         "F4=CL_RESET（复位恢复）；FALLBACK 段统计取 CL_*.json，"
                         "STALE|HELD 逐拍位统计取同目录 harness 轨迹副本 CSV"),
        "mock_note": ("注入运行用 stub advancer + pty+_MockFW（STALE 阈加速 0.05s，"
                      "见各 CL_*.json params）：验证协议/flags/回退语义，段内 Δfuel/ΔSOC "
                      "为 mock 口径，不代表 plant 保真油耗"),
        "scenarios": {},
        "fallback_total": {"n_segments": 0, "n_ticks": 0, "d_fuel_kg": 0.0, "d_soc": 0.0},
    }
    for fault_id, json_name, focus in FAULT_SCENARIOS:
        jp = os.path.join(fault_dir, json_name)
        if not os.path.isfile(jp):
            out["scenarios"][fault_id] = {"error": "missing %s" % json_name}
            continue
        data = load_json(jp)
        segs = data.get("fallback_segments") or []
        seg_tot = {
            "n_segments": len(segs),
            "n_ticks": sum(s.get("n_ticks", 0) for s in segs),
            "d_fuel_kg": sum(s.get("d_fuel_kg", 0.0) for s in segs),
            "d_soc": sum(s.get("d_soc", 0.0) for s in segs),
        }
        out["fallback_total"]["n_segments"] += seg_tot["n_segments"]
        out["fallback_total"]["n_ticks"] += seg_tot["n_ticks"]
        out["fallback_total"]["d_fuel_kg"] += seg_tot["d_fuel_kg"]
        out["fallback_total"]["d_soc"] += seg_tot["d_soc"]
        params = data.get("params") or {}
        harness = None
        ref = data.get("v1_scenario_ref")
        if ref:
            hp = os.path.join(fault_dir, "%s_closedloop_S20_npu.csv" % ref)
            if os.path.isfile(hp):
                harness = fault_harness_flag_stats(hp, params.get("cl_fault_tick"))
        dump_stats = _parse_dump_evt_stats(data.get("dump_text"))
        session_alive = None
        if harness is not None and params.get("cl_ticks"):
            session_alive = bool(harness["n_ticks"] == params["cl_ticks"])
        out["scenarios"][fault_id] = {
            "json": json_name,
            "v1_scenario_ref": data.get("v1_scenario_ref"),
            "focus": focus,
            "params": params,
            "counts": data.get("counts"),
            "fallback_segments": segs,
            "fallback_total": seg_tot,
            "policy_events": data.get("policy_events"),
            "dump_text": data.get("dump_text"),
            "dump_evt_stats": dump_stats,
            "harness_csv_flags": harness,
            "session_alive": session_alive,
        }
    return out


# ---------------------------------------------------------------------------
# 交叉核对（锚点）
# ---------------------------------------------------------------------------
def build_cross_checks(model_ws, gp1_refs, arms):
    checks = []
    ref_dir = os.path.join(_ROOTS[0], "Host", "reference")
    native_summary_path = os.path.join(_ROOTS[2], "results", "ecms_phase25", "closedloop",
                                       "native_arms", "native_arms_summary.json")
    export_summary_path = os.path.join(_ROOTS[2], "results", "ecms_phase25", "fw_reference",
                                       "export_summary.json")

    def add_check(name, values, tol_abs=None, note="", tol_rel=None):
        vals = [v for v in values if v is not None]
        ok = None
        max_abs = None
        if len(vals) >= 2:
            max_abs = max(abs(vals[i] - vals[j])
                          for i in range(len(vals)) for j in range(len(vals)))
            if tol_abs is not None:
                ok = bool(max_abs <= tol_abs)
            elif tol_rel is not None:
                base = max(abs(v) for v in vals)
                ok = bool(max_abs <= tol_rel * base) if base > 0 else bool(max_abs == 0.0)
        checks.append({"check": name, "values": values, "max_abs_delta": max_abs,
                       "tolerance_abs": tol_abs, "tolerance_rel": tol_rel,
                       "pass": ok, "note": note})

    # 1) gp1 replay fuel_raw vs 静态锚点 vs T-602 gate JSON vs export_summary G2
    export = load_json(export_summary_path)
    export_by_run = {r["run_id"]: r for r in export.get("runs", [])}
    for scn in SCENARIOS:
        vals = {"gp1_replay_fuel_raw": gp1_refs[scn]["fuel_raw"]}
        if scn in GP1_FUEL_RAW_ANCHOR:
            vals["static_anchor_22hao"] = GP1_FUEL_RAW_ANCHOR[scn]
        gate_p = os.path.join(ref_dir, "v3_gp1_%s_gate.json" % scn)
        if os.path.isfile(gate_p):
            vals["t602_gate_fuel_ref"] = load_json(gate_p).get("metrics", {}).get("fuel_ref")
        g2 = export_by_run.get(G2_RUN_IDS[scn])
        if g2:
            vals["export_summary_G2_fuel_raw"] = g2.get("fuel_raw_kg")
        add_check("gp1_fuel_raw_%s_consistency" % scn, list(vals.values()), tol_abs=1e-6,
                  note="锚点来源：22 号产物 / v3_gp1_%s_gate.json / fw_reference export_summary" % scn)

    # 2) 原生臂 (b′)/(c) 指标 vs native_arms_summary.json（21 号产物）
    if os.path.isfile(native_summary_path):
        ns = load_json(native_summary_path)
        ns_by_run = {a["run_id"]: a for a in ns.get("arms", [])}
        for scn in SCENARIOS:
            for arm_id, (fname, _law) in NATIVE_ARM_FILES.items():
                run_id = fname.format(scn=scn).replace(".csv", "")
                ref_arm = ns_by_run.get(run_id)
                mine = arms[scn][arm_id]
                if ref_arm is None:
                    continue
                for metric, tol in (("fuel_raw_kg", 1e-9), ("fuel_corr_kg", 1e-9),
                                    ("SOC_end", 1e-9), ("e_hat_active_ratio", 1e-9)):
                    mv, rv = mine[metric], ref_arm.get(metric)
                    if rv is None or mv is None:
                        continue
                    checks.append({
                        "check": "native_vs_summary.%s.%s" % (run_id, metric),
                        "values": [mv, rv], "max_abs_delta": abs(mv - rv),
                        "tolerance_abs": tol, "tolerance_rel": None,
                        "pass": bool(abs(mv - rv) <= tol),
                        "note": "native_arms_summary.json（21 号产物）交叉核对",
                    })
                # v_track_rmse：summary 主口径为 0.01 s 全分辨率轴；本脚本为 1 s CSV 轴
                # → 与 v_track_rmse_1s_grid 辅助口径比对（报告性差异，不设门禁）
                rv1 = ref_arm.get("v_track_rmse_1s_grid")
                mv1 = mine.get("v_track_rmse_mps")
                if rv1 is not None and mv1 is not None:
                    checks.append({
                        "check": "native_vs_summary.%s.v_track_rmse_1s_axis" % run_id,
                        "values": [mv1, rv1],
                        "max_abs_delta": abs(mv1 - rv1),
                        "tolerance_abs": None, "tolerance_rel": None, "pass": None,
                        "note": ("报告性差异（本脚本 1 s CSV 轴 vs summary resample_1s "
                                 "辅助口径），不设门禁"),
                    })
    return checks


# ---------------------------------------------------------------------------
# 汇总 MD 输出
# ---------------------------------------------------------------------------
def _fmt(x, nd=6):
    if x is None:
        return "null"
    if isinstance(x, bool):
        return "True" if x else "False"
    if isinstance(x, float):
        if math.isnan(x):
            return "NaN"
        return ("%." + str(nd) + "f") % x
    return str(x)


def write_summary_md(path, doc):
    L = []
    L.append("# EMS01 v3 闭环四臂两场景统一指标汇总（T-615）")
    L.append("")
    L.append("- spec: %s" % doc["spec"])
    L.append("- generated_utc: %s" % doc["generated_utc"])
    L.append("- script: `%s`（纯 stdlib，相对路径，只读输入）" % doc["script"])
    L.append("- 机器可读全文：`closedloop_metrics.json`（本文件为其人类可读摘要）")
    L.append("")
    L.append("## §0 常量与来源（禁新造公式，design §8.1）")
    L.append("")
    L.append("| 常量 | 值 | 来源 |")
    L.append("|---|---|---|")
    c = doc["constants"]
    L.append("| BatCap (Bat_Cap_kWh) | %s kWh | %s |" % (c["BAT_CAP_KWH"], c["BAT_CAP_KWH_source"]))
    L.append("| K_EQ | %s kg/kWh | %s |" % (c["K_EQ_KG_PER_KWH"], c["K_EQ_KG_PER_KWH_source"]))
    L.append("| SOC 参考（fuel_corr） | %s | %s |" % (c["SOC_REF_CHARGE"], c["SOC_REF_CHARGE_source"]))
    L.append("| P_gen 网格步 | %s kW | %s |" % (c["GRID_STEP_KW"], c["GRID_STEP_KW_source"]))
    L.append("| SOC 降额起始 | %s | %s |" % (c["SOC_DISDERATE_START"], c["SOC_DISDERATE_START_source"]))
    L.append("| SOC 公平带 | ±%s | design §8.2 |" % c["SOC_FAIR_BAND"])
    L.append("")
    L.append("## §1 四臂 × 两场景核心指标（fuel_corr = fuel_raw + (0.60−SOC_end)×BatCap×K_EQ）")
    L.append("")
    for scn in doc["scenarios"]:
        L.append("### 场景 %s" % scn)
        L.append("")
        L.append("| 臂 | fuel_raw [kg] | fuel_corr [kg] | SOC_end | v_track_rmse [m/s] | "
                 "ê 生效占比 | SOC<0.25 事件/时长 | n_undervoltage |")
        L.append("|---|---|---|---|---|---|---|---|")
        for arm_id in ARM_ORDER:
            m = doc["arms"][scn][arm_id]
            dur = ("null" if m["soc_disderate_duration_s"] is None
                   else "%.1f s" % m["soc_disderate_duration_s"])
            L.append("| %s | %s | %s | %s | %s | %s | %d / %s | %s |" % (
                ARM_LABELS[arm_id], _fmt(m["fuel_raw_kg"], 6), _fmt(m["fuel_corr_kg"], 6),
                _fmt(m["SOC_end"], 8), _fmt(m["v_track_rmse_mps"], 6),
                _fmt(m["e_hat_active_ratio"], 6), m["n_soc_disderate_events"], dur,
                _fmt(m["n_undervoltage"], 0)))
        L.append("")
    L.append("> 注：n_undervoltage 口径见 JSON 各臂 `n_undervoltage_source`"
             "（板载=flags HELD 保持拍数；原生臂 CSV 无 alarm 列记 null，交叉核对值见 §6）。")
    L.append("")
    L.append("## §2 SOC 公平性判定（±0.02 带，design §8.2）")
    L.append("")
    for scn in doc["scenarios"]:
        L.append("### 场景 %s" % scn)
        L.append("")
        L.append("| 对比 | 用途 | Δfuel_raw [kg] | Δfuel_corr [kg] | ΔSOC_end | 判定 |")
        L.append("|---|---|---|---|---|---|")
        for comp in doc["pairwise_comparisons"][scn]:
            f = comp["fairness"]
            L.append("| %s | %s | %+.6f | %+.6f | %+.8f | **%s** |" % (
                comp["pair"], comp["purpose"], comp["fuel_raw_diff_kg"],
                comp["fuel_corr_diff_kg"], comp["SOC_end_diff"], f["verdict"]))
            L.append("| └ 结论 | | | | | %s；次级（fuel_corr 差）= %+.6f kg |" % (
                f["primary_conclusion"], f["secondary_conclusion_fuel_corr_diff_kg"]))
        L.append("")
    L.append("## §3 仪器级①②门禁复述（FR-610，design §5；T-613 现场已核对，此处正式落 JSON）")
    L.append("")
    L.append("阈值：SOC_end |Δ| ≤ 0.005；fuel_raw 相对偏差 ≤ 1%；v_ship RMSE ≤ 0.01 m/s；"
             "P_gen 网格一致率（|ΔP| ≤ 20 kW）≥ 95%；数值健康。")
    L.append("")
    for title, key in (
            ("仪器级①：闭环 (b) vs gp1 replay（开环参考）", "instrument_grade_1_closedloop_b_vs_gp1"),
            ("仪器级②：闭环 (b) vs 原生 (b′)", "instrument_grade_2_closedloop_b_vs_native_bprime")):
        L.append("### %s" % title)
        L.append("")
        L.append("| 场景 | 指标 | 值 | 阈值 | 判定 |")
        L.append("|---|---|---|---|---|")
        for scn in doc["scenarios"]:
            g = doc[key][scn]
            for it in g["items"]:
                L.append("| %s | %s | %s | %s %s | %s |" % (
                    scn, it["metric"], _fmt(it["value"], 8), it["cmp"], it["threshold"],
                    "PASS" if it["pass"] else "FAIL"))
            L.append("| %s | **overall** | | | **%s** |" % (scn, "PASS" if g["overall"] else "FAIL"))
        L.append("")
    L.append("## §4 层③报告性输出（FR-612/§8.3，不作门禁）：闭环 (a) vs ref_G2 同拍 P_gen 分布")
    L.append("")
    L.append("| 场景 | n_ticks | exact 率 | ±1 网格率 | max\\|ΔP\\| [kW]（@tick） | 超 1 网格拍（起动瞬态 tick≤20） |")
    L.append("|---|---|---|---|---|---|")
    for scn in doc["scenarios"]:
        l3 = doc["layer3_report_only_a_vs_refG2_p_gen"][scn]
        L.append("| %s | %d | %s | %s | %s（@%s） | %d（%d） |" % (
            scn, l3["n_ticks_compared"], _fmt(l3["exact_rate"], 6),
            _fmt(l3["plus1_grid_rate"], 6), _fmt(l3["max_abs_dP_kW"], 3),
            l3["max_abs_dP_tick"], l3["n_ticks_beyond_1grid"],
            l3["n_ticks_beyond_1grid_startup_le20"]))
    L.append("")
    L.append("> 注：闭环 (a) 自 v_ship=0 初值起动（首拍 INIT fill/HELD），开环 ref 序列无此瞬态；"
             "超 1 网格拍集中于起动段，其余为零散 λ 网格边界翻转（design §4.4 机制）。"
             "层③报告性，不作门禁。")
    L.append("")
    L.append("## §5 FR-508 闭环故障注入代价（critical_section，图像=NPU）")
    L.append("")
    L.append("| 场景 | focus | FALLBACK 段/拍 | 段内 Δfuel [kg] | 段内 ΔSOC | "
             "STALE/HELD/FB 拍 | 会话存活 |")
    L.append("|---|---|---|---|---|---|---|")
    for fault_id, sc in doc["critical_section_fr508"]["scenarios"].items():
        ft = sc.get("fallback_total") or {}
        h = sc.get("harness_csv_flags") or {}
        L.append("| %s (%s) | %s | %d / %d | %+.6f | %+.8f | %s/%s/%s | %s |" % (
            fault_id, sc.get("v1_scenario_ref"), (sc.get("focus") or "").split("：")[0],
            ft.get("n_segments", 0), ft.get("n_ticks", 0),
            ft.get("d_fuel_kg", 0.0), ft.get("d_soc", 0.0),
            h.get("n_stale_ticks"), h.get("n_held_ticks"), h.get("n_fallback_ticks"),
            sc.get("session_alive")))
    ftot = doc["critical_section_fr508"]["fallback_total"]
    L.append("")
    L.append("FALLBACK 汇总：%d 段 / %d 拍 / Δfuel=%.6f kg / ΔSOC=%.8f" % (
        ftot["n_segments"], ftot["n_ticks"], ftot["d_fuel_kg"], ftot["d_soc"]))
    L.append("")
    L.append("> 注：注入运行用 stub advancer + pty+_MockFW（STALE 阈加速 0.05s）——验证"
             "协议/flags/回退语义；段内 Δfuel 为 mock 口径，不代表 plant 保真油耗。")
    L.append("")
    L.append("## §6 交叉核对（锚点）")
    L.append("")
    L.append("| 核对项 | 值组 | max\\|Δ\\| | 容差 | 判定 |")
    L.append("|---|---|---|---|---|")
    for chk in doc["cross_checks"]:
        vals = "[" + ", ".join(_fmt(v, 9) for v in chk["values"]) + "]"
        tol = chk["tolerance_abs"] if chk["tolerance_abs"] is not None else chk["tolerance_rel"]
        L.append("| %s | %s | %s | %s | %s |" % (
            chk["check"], vals, _fmt(chk["max_abs_delta"], 12), _fmt(tol, 12),
            "PASS" if chk["pass"] else ("FAIL" if chk["pass"] is False else "n/a（记录差异）")))
    L.append("")
    L.append("## §7 口径备注")
    L.append("")
    for note in doc["notes"]:
        L.append("- %s" % note)
    L.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L))


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    global _ROOTS
    ems01_root, repo_root, model_ws = find_roots()
    if ems01_root is None or repo_root is None or model_ws is None:
        sys.stderr.write("ERROR: 无法定位 EMS01 工程根 / 仓库根 / model 仓库工作区"
                         "（__file__ 与 pwd 均上溯失败）\n")
        return 2
    _ROOTS = (ems01_root, repo_root, model_ws)

    cl_dir = os.path.join(ems01_root, "Host", "results", "closedloop")
    ref_dir = os.path.join(ems01_root, "Host", "reference")
    native_dir = os.path.join(model_ws, "results", "ecms_phase25", "closedloop", "native_arms")
    fault_dir = os.path.join(ems01_root, "Host", "fault_out", "closedloop_npu", "closedloop")

    inputs = {}

    def note_input(path):
        inputs[os.path.relpath(path, repo_root)] = {
            "sha256": sha256_of(path), "bytes": os.path.getsize(path)}

    doc = {
        "task": "T-615",
        "spec": "ems01-chil-v3-closedloop (FR-610/FR-611/AC-605)",
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "script": "Host/tools/closedloop_metrics.py",
        "exit_code_semantics": "0=完成；仪器级①② pass/fail 如实记录于 instrument_grade_*，"
                               "不作退出码门禁",
        "constants": {
            "BAT_CAP_KWH": BAT_CAP_KWH,
            "BAT_CAP_KWH_source": "Bat_Cap_kWh，model 仓库 scripts/21_run_v3_native_arms.m:94"
                                  "（64:239 同式同常数）",
            "K_EQ_KG_PER_KWH": K_EQ_KG_PER_KWH,
            "K_EQ_KG_PER_KWH_source": "model 仓库 scripts/21_run_v3_native_arms.m:95（64 同口径）",
            "SOC_REF_CHARGE": SOC_REF_CHARGE,
            "SOC_REF_CHARGE_source": "design §8.1 fuel_corr 式（64/66 同式）",
            "GRID_STEP_KW": GRID_STEP_KW,
            "GRID_STEP_KW_source": "Host/ems_inject.py GRID_STEP_KW（design 固定 20 kW）",
            "SOC_DISDERATE_START": SOC_DISDERATE_START,
            "SOC_DISDERATE_START_source": "ASSUMED_SOC_DisDerate_Start，"
                                          "config/Model_Parameters.m:123",
            "SOC_FAIR_BAND": SOC_FAIR_BAND,
            "SOC_FAIR_BAND_source": "design §8.2（±2% SOC 容差带）",
            "fuel_corr_formula": "fuel_corr = fuel_raw + (0.60 - SOC_end) * BAT_CAP_KWH * K_EQ_KG_PER_KWH",
        },
        "scenarios": list(SCENARIOS),
        "inputs_sha256": inputs,
        "arms": {},
        "pairwise_comparisons": {},
        "instrument_grade_1_closedloop_b_vs_gp1": {},
        "instrument_grade_2_closedloop_b_vs_native_bprime": {},
        "layer3_report_only_a_vs_refG2_p_gen": {},
    }

    # ---- 逐场景装载与计算 ----
    gp1_refs = {}
    for scn in SCENARIOS:
        arms = {}
        # 板载 (a)/(b)：19 列
        for arm_id, image in ONBOARD_IMAGES.items():
            p = os.path.join(cl_dir, scn, "closedloop_%s_%s.csv" % (scn, image))
            recs, _ = load_track_csv(p, "onboard")
            note_input(p)
            meta_p = os.path.join(cl_dir, scn, "closedloop_%s_%s_meta.json" % (scn, image))
            meta = None
            if os.path.isfile(meta_p):
                meta = load_json(meta_p)
                note_input(meta_p)
            m = compute_arm_metrics(arm_id, "onboard", p, recs, meta=meta)
            m["image"] = image
            m["law"] = 7  # (a)/(b) 同一模型对（MLP-Q + workclf，design §9）
            m["_recs"] = recs
            arms[arm_id] = m
        # 原生 (b′)/(c)：22 列
        for arm_id, (fname, law) in NATIVE_ARM_FILES.items():
            p = os.path.join(native_dir, fname.format(scn=scn))
            recs, _ = load_track_csv(p, "native")
            note_input(p)
            m = compute_arm_metrics(arm_id, "native7" if law == 7 else "native5", p, recs)
            m["law"] = law
            m["_recs"] = recs
            arms[arm_id] = m
        doc["arms"][scn] = arms

        # 仪器级①参考：gp1 replay（开环解释执行参考）
        gp = os.path.join(ref_dir, "v3_gp1_%s_replay.csv" % scn)
        gp_recs, _ = load_track_csv(gp, "gp1ref")
        note_input(gp)
        gp1_refs[scn] = {"recs": gp_recs, "fuel_raw": gp_recs[-1]["fuel"],
                         "SOC_end": gp_recs[-1]["SOC"]}

        # 两两对比（同场景）
        doc["pairwise_comparisons"][scn] = [
            make_comparison(arms[x], arms[y], x, y, purpose)
            for (x, y, purpose) in PAIRWISE_SPECS
        ]

        # 仪器级①：(b) vs gp1 replay
        doc["instrument_grade_1_closedloop_b_vs_gp1"][scn] = instrument_grade(
            arms["b_ref_onboard"], gp1_refs[scn]["recs"],
            "Host/reference/v3_gp1_%s_replay.csv（开环解释执行参考；"
            "P_gen_ref_kW ≡ ref_G2 驱动序列）" % scn,
            gp1_refs[scn]["fuel_raw"], gp1_refs[scn]["SOC_end"])

        # 仪器级②：(b) vs (b′)
        bp = arms["bp_native_law7"]
        health_bp = (all(v == 0 for v in bp["numeric_health_nan_counts"].values())
                     and bp["p_gen_grid_residual_max_kW"] <= GRID_EPS_KW)
        doc["instrument_grade_2_closedloop_b_vs_native_bprime"][scn] = instrument_grade(
            arms["b_ref_onboard"], bp["_recs"],
            "原生 (b′) %s（native_arms，law=7）" % bp["source_csv"],
            bp["fuel_raw_kg"], bp["SOC_end"], extra_health=health_bp)

        # 层③（报告性）
        doc["layer3_report_only_a_vs_refG2_p_gen"][scn] = layer3_p_gen_distribution(
            arms["a_npu_onboard"], gp1_refs[scn]["recs"])

    # FR-508 闭环代价
    doc["critical_section_fr508"] = build_critical_section(fault_dir)
    for jp in sorted(glob.glob(os.path.join(fault_dir, "CL_*.json"))):
        note_input(jp)

    # 原生臂交叉核对值（n_undervoltage / v_track_rmse 全分辨率口径；来源标注）
    ns_path = os.path.join(native_dir, "native_arms_summary.json")
    if os.path.isfile(ns_path):
        ns = load_json(ns_path)
        ns_by_run = {a["run_id"]: a for a in ns.get("arms", [])}
        for scn in SCENARIOS:
            for arm_id, (fname, _law) in NATIVE_ARM_FILES.items():
                run_id = fname.format(scn=scn).replace(".csv", "")
                ref = ns_by_run.get(run_id)
                if ref is not None:
                    doc["arms"][scn][arm_id]["native_summary_crosscheck"] = {
                        "source": "native_arms_summary.json（21 号产物，MATLAB logsout）",
                        "n_undervoltage": ref.get("n_undervoltage"),
                        "n_derate": ref.get("n_derate"),
                        "v_track_rmse_full_res_mps": ref.get("v_track_rmse"),
                        "v_track_rmse_1s_grid_mps": ref.get("v_track_rmse_1s_grid"),
                    }
        note_input(ns_path)
    exp_path = os.path.join(model_ws, "results", "ecms_phase25", "fw_reference",
                            "export_summary.json")
    if os.path.isfile(exp_path):
        note_input(exp_path)

    doc["cross_checks"] = build_cross_checks(model_ws, gp1_refs, doc["arms"])

    doc["notes"] = [
        "四臂口径：板载 CSV 19 列（P_gen 列名 P_gen_kW，flags 位镜像 ems_proto.h："
        "STALE=0x01/FALLBACK=0x02/CLS_BAD=0x04/HELD=0x08）；原生臂 CSV 22 列"
        "（P_gen 列名 P_gen_ref_kW，无 flags 列）；gp1 参考 CSV 13 列。均按各自表头映射。",
        "ê 生效占比：板载臂 = mean(K>0 ∧ flags&0x02==0)；原生 law=7 = mean(K>0)"
        "（与 native_arms_summary.json 逐位一致）；law=5 恒 0（CSV K=1 为中性值，不据其计算）。",
        "v_track_rmse 为全任务 1 s 轴口径（各臂自体 v_ship vs v_cmd，design §8.1 定义式）；"
        "原生臂另附 native_arms_summary.json 的 0.01 s 全分辨率主口径与 1 s 网格辅助口径交叉核对。",
        "fuel_corr 电荷维持修正：64/66 同式同常数（design §8.1 禁新造公式）；"
        "每臂同时报告 fuel_raw 与 fuel_corr + SOC_end。",
        "仪器级①参考 = gp1 replay（其 P_gen_ref_kW 为 ref_G2 驱动序列，G-P1 T-602 gate "
        "fuel_rel=0）；两场景全部算齐，pass/fail 如实记录，不作退出码门禁。",
        "层③为报告性输出（FR-612/§8.3），不作门禁；层② (a) 裁决登记归 T-616 报告。",
        "tick 对齐：全部比较按 tick 号交集对齐（板载 0..N-1 与原生/gp1 同网格）。",
        "退出码 0 = 完成；仪器级①②判定仅记录于 JSON，不改变退出码。",
    ]

    # ---- 写产物（脚本内固定相对路径）----
    out_json = os.path.join(cl_dir, "closedloop_metrics.json")
    out_md = os.path.join(cl_dir, "closedloop_metrics_summary.md")

    def strip(o):
        if isinstance(o, dict):
            return {k: strip(v) for k, v in o.items() if k != "_recs"}
        if isinstance(o, list):
            return [strip(v) for v in o]
        return o

    doc_stripped = strip(doc)
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(doc_stripped, f, ensure_ascii=False, indent=1)
        f.write("\n")
    write_summary_md(out_md, doc_stripped)

    # ---- stdout 摘要 ----
    print("closedloop_metrics.py（T-615）完成")
    print("  JSON: %s" % os.path.relpath(out_json, repo_root))
    print("  MD  : %s" % os.path.relpath(out_md, repo_root))
    print()
    for scn in SCENARIOS:
        print("场景 %s：" % scn)
        print("  %-16s %12s %12s %10s %9s %9s %12s" % (
            "臂", "fuel_raw", "fuel_corr", "SOC_end", "v_rmse", "ê占比", "SOC<0.25"))
        for arm_id in ARM_ORDER:
            m = doc["arms"][scn][arm_id]
            print("  %-16s %12.6f %12.6f %10.6f %9.4f %9.4f %4d/%6.1fs" % (
                arm_id, m["fuel_raw_kg"], m["fuel_corr_kg"], m["SOC_end"],
                m["v_track_rmse_mps"], m["e_hat_active_ratio"],
                m["n_soc_disderate_events"], m["soc_disderate_duration_s"] or 0.0))
        print("  两两对比与公平性：")
        for comp in doc["pairwise_comparisons"][scn]:
            f = comp["fairness"]
            print("    %-28s Δraw=%+.4f kg  Δcorr=%+.4f kg  ΔSOC=%+.6f  %s" % (
                comp["pair"], comp["fuel_raw_diff_kg"], comp["fuel_corr_diff_kg"],
                comp["SOC_end_diff"], f["verdict"]))
        g1 = doc["instrument_grade_1_closedloop_b_vs_gp1"][scn]
        g2 = doc["instrument_grade_2_closedloop_b_vs_native_bprime"][scn]
        print("  仪器级① (b) vs gp1 replay: overall=%s" % ("PASS" if g1["overall"] else "FAIL"))
        print("  仪器级② (b) vs (b′) native: overall=%s" % ("PASS" if g2["overall"] else "FAIL"))
        l3 = doc["layer3_report_only_a_vs_refG2_p_gen"][scn]
        print("  层③ (a) vs ref_G2 P_gen: exact=%.4f ±1grid=%.4f max|ΔP|=%.1f kW（报告性）" % (
            l3["exact_rate"], l3["plus1_grid_rate"], l3["max_abs_dP_kW"]))
        print()
    bad = [c["check"] for c in doc["cross_checks"] if c["pass"] is False]
    print("交叉核对：%d 项，%s" % (
        len(doc["cross_checks"]), "全部一致" if not bad else "不一致项=%s" % bad))
    return 0


if __name__ == "__main__":
    sys.exit(main())
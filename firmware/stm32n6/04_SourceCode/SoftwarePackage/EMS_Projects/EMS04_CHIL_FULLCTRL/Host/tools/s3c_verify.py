#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3c_verify.py — EMS04-S3-C 板侧对拍核验器（首测 / pilot / 全量共用）。

用途
----
把三个来源做整数域逐位比对，产出机器可读报告（JSON）+ stdout 摘要表：

  1) --capture <jsonl>   板侧响应 capture（由同目录 s3c_reatt_replay.py 产出），每行：
       成功行 {"seq":int, "vid":str, "t0":int, "q_f32":[90 float], "infer_us":int}
       错误行 {"seq":int, "vid":str, "t0":int, "error":str}
  2) --edges <json>      调试器内存读取的边界码（字节序 = 板侧内存序，STM32 小端）：
       {"<vid>__t0_<t0>": {
          "n1_hist_int8_code": "<3300 字节 hex>",   # (300,11) int8, C 序
          "n2_in_kf_code":     "<180 字节 hex>",    # (1,6,30) int8, ch-major C 序
          "n2_out_code":       "<90 字节 hex>",     # (30,3)   int8, C 序
          "h_past":            "<384 字节 hex>" }}  # (96,)  f32, 小端
  3) --ref-dir <dir>     host B 参考 npz 目录，文件名 "<vid>_t0_<t0:05d>.npz"，键：
       n1_hist_int8_code (300,11) int8 / n2_in_kf_code (1,6,30) int8 /
       n2_out_code (30,3) int8 / h_past (96,) f32 / q_f32 (30,3) f32

锚映射：capture 行 + edges 键同名（vid + t0）；参考文件 <vid>_t0_<t0:05d>.npz。

判据（machine gate）
--------------------
  A n1_hist_int8_code / B n2_in_kf_code / C n2_out_code：板 hex（bytes.fromhex 后
    按声明形状 C 序）vs npz 同名键逐字节相等；
  D q_f32：capture 90 float vs npz q_f32 展平，按 float32 位级（uint32 view）相等；
    NaN 一律视为不等并单独计数；--q-tolerate-ulp N（默认 0）可放宽为 ULP 距离 ≤ N。
  q50（q 通道 1，单位 kW）差值仅作二级诊断输出，不参与判据。

overall = pass 当且仅当：三源齐备的共同锚全部满足 A/B/C mismatch=0 且 D 位级全等
（含 ULP 容限）。共同锚 = capture、edges、ref npz 三者齐备的锚。
  edges 缺某锚        → 该锚记 SKIP（板侧未取）；
  capture 缺某锚      → 该锚记 MISSING；
  ref npz 缺/不可读   → 该锚记 SKIP_REF；
以上三类不参与 overall；共同锚数为 0 时 overall=fail（避免空集空过）。

已标注的实现假设（Spec 未明示处，按最合理解释，见 report["assumptions"]）：
  - h_past 双侧均有数据，但 Spec 判据仅 A/B/C/D；h_past 做位级比对并计入
    boundary totals，作为非门控诊断（"gating": false），不影响 overall；
  - first_divergence 只在门控字段（A/B/C/D）中定位，锚遍历顺序 = (vid, t0) 升序；
  - h_past 的 f32 字节序按小端（板侧 STM32 / host 均小端）解释；
  - edges 条目缺某字段或 hex 长度不符 → 该门控项记 error（判 fail），不静默跳过；
  - capture 错误行（有 error 无 q_f32）在三源齐备时 → D 记 error（判 fail）。

用法
----
  # 全量核验
  .venv_tf/bin/python s3c_verify.py \
      --capture results/s3c/s3c_capture_<stamp>.jsonl \
      --edges   results/s3c/s3c_edges_<stamp>.json \
      --ref-dir Host/reference/s3c_hostB \
      --out     results/s3c/s3c_verify_<stamp>.json
  # 比较逻辑自检（无输入，合成 2 锚数据，打印 PASS/FAIL）
  .venv_tf/bin/python s3c_verify.py --dry-run

退出码：0 = overall pass（或 dry-run 自检通过）；1 = fail；2 = 用法/IO 错误。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
import time

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# 边界字段规格：hex 字节数 / npz 形状 / dtype
FIELD_SPEC = {
    "n1_hist_int8_code": {"nbytes": 3300, "shape": (300, 11), "dtype": np.int8},
    "n2_in_kf_code": {"nbytes": 180, "shape": (1, 6, 30), "dtype": np.int8},
    "n2_out_code": {"nbytes": 90, "shape": (30, 3), "dtype": np.int8},
    "h_past": {"nbytes": 384, "shape": (96,), "dtype": np.float32},
}
INT_FIELDS = ("n1_hist_int8_code", "n2_in_kf_code", "n2_out_code")
GATING_ORDER = ("n1_hist_int8_code", "n2_in_kf_code", "n2_out_code", "q_f32")
Q_FLAT = 90  # 30 步 × 3 分位（q10/q50/q90），q50 = 通道 1
Q50_CH = 1

# 报告中登记的解释性假设（Spec 未明示处的最合理解释）
ASSUMPTIONS = [
    "h_past 位级比对仅作非门控诊断（Spec 判据为 A/B/C/D），计入 boundary totals",
    "first_divergence 仅在门控字段 A/B/C/D 中定位，锚遍历顺序 = (vid, t0) 升序",
    "h_past 的 f32 按小端（板侧 STM32 / host 均小端）位级比对",
    "edges 条目缺字段或 hex 长度不符 -> 该门控项 error（判 fail），不静默跳过",
    "capture 错误行（无 q_f32）在三源齐备时 -> D 项 error（判 fail）",
    "共同锚（三源齐备）数为 0 时 overall=fail，避免空集空过",
    "q50 = q 通道 1（30 步 x3 分位中的中位分位），单位 kW，仅作二级诊断",
]


# ---------------------------------------------------------------- 路径/加载

def find_repo_root(start_dir):
    """沿目录树向上找 .git（与同目录 s3c_reatt_replay.py 同规则），仅用于日志。"""
    d = os.path.abspath(start_dir)
    while d != os.path.dirname(d):
        if os.path.isdir(os.path.join(d, ".git")):
            return d
        d = os.path.dirname(d)
    return d


def load_capture(path):
    """capture jsonl → {(vid, t0): row}。同锚重复行保留首条并计数；坏行计数。"""
    rows, dup, bad = {}, 0, 0
    with open(path, "r", encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
                key = (str(r["vid"]), int(r["t0"]))
            except (json.JSONDecodeError, KeyError, TypeError, ValueError):
                bad += 1
                print(f"[warn] capture 第 {ln} 行无法解析（缺 vid/t0），跳过", file=sys.stderr)
                continue
            if key in rows:
                dup += 1
                continue
            r["_line"] = ln
            rows[key] = r
    return rows, {"duplicates": dup, "bad_lines": bad}


def parse_edge_key(key):
    """"<vid>__t0_<t0>" → (vid, int(t0))；t0 兼容补零/不补零写法。"""
    m = key.rsplit("__t0_", 1)
    if len(m) != 2 or not m[0] or not m[1]:
        raise ValueError(f"edges 键格式非法: {key!r}（期望 <vid>__t0_<t0>）")
    return m[0], int(m[1])


def load_edges(path):
    """edges json → {(vid, t0): entry_dict}。"""
    raw = json.load(open(path, "r", encoding="utf-8"))
    if not isinstance(raw, dict):
        raise SystemExit(f"edges 根必须是 dict: {path}")
    edges = {}
    for k, v in raw.items():
        vid, t0 = parse_edge_key(k)
        if not isinstance(v, dict):
            raise SystemExit(f"edges[{k!r}] 必须是 dict，实际 {type(v).__name__}")
        edges[(vid, t0)] = v
    return edges


REF_RE = re.compile(r"^(?P<vid>.+)_t0_(?P<t0>\d+)\.npz$")


def scan_ref_dir(ref_dir):
    """扫描参考目录 → {(vid, t0): npz 绝对路径}，无法解析的文件名计数。"""
    refs, ignored = {}, 0
    for name in sorted(os.listdir(ref_dir)):
        p = os.path.join(ref_dir, name)
        if not os.path.isfile(p) or not name.endswith(".npz"):
            ignored += 1
            continue
        m = REF_RE.match(name)
        if not m:
            ignored += 1
            continue
        refs[(m.group("vid"), int(m.group("t0")))] = p
    return refs, ignored


def load_ref_npz(path):
    """读参考 npz → {key: ndarray}；读失败返回 None（记 SKIP_REF）。"""
    try:
        with np.load(path, allow_pickle=False) as z:
            return {k: np.asarray(z[k]) for k in z.files}
    except Exception as e:  # noqa: BLE001 — 参考文件损坏按缺参考处理并在 reason 说明
        print(f"[warn] 参考 npz 读取失败 {path}: {e}", file=sys.stderr)
        return None


# ---------------------------------------------------------------- 比较

def _item(status, gating, **kw):
    d = {"status": status, "gating": gating}
    d.update(kw)
    return d


def compare_bytes(field, board_bytes, ref_arr, gating):
    """int8 边界码逐字节比较（int8 无字节序问题，按声明形状 C 序展平）。"""
    spec = FIELD_SPEC[field]
    ref = np.ascontiguousarray(ref_arr)
    it = {"board_bytes": len(board_bytes),
          "ref_shape": list(ref.shape), "ref_nelem": int(ref.size)}
    nelem = int(np.prod(spec["shape"]))
    if ref.size != nelem:
        it.update(status="error", reason="ref_size_unexpected",
                  expected_nelem=nelem)
        return _item("error", gating, **it)
    if ref.dtype != spec["dtype"]:
        ref = ref.astype(spec["dtype"])
    if len(board_bytes) != spec["nbytes"]:
        it.update(status="error", reason="board_hex_len_unexpected",
                  expected_bytes=spec["nbytes"])
        return _item("error", gating, **it)
    b = np.frombuffer(board_bytes, dtype=np.uint8)
    r = ref.reshape(-1).view(np.uint8)
    diff = b != r
    n = int(np.count_nonzero(diff))
    if n:
        off = int(np.argmax(diff))
        it.update(first_offset=off, first_board=int(b[off]), first_ref=int(r[off]))
        return _item("mismatch", gating, mismatches=n, **it)
    it.update(first_offset=None)
    return _item("match", gating, mismatches=0, **it)


def compare_hpast(board_bytes, ref_arr):
    """h_past f32 位级比较（非门控诊断）。假设双侧均小端。"""
    spec = FIELD_SPEC["h_past"]
    ref = np.ascontiguousarray(ref_arr, dtype="<f4").reshape(-1)
    if ref.size != spec["shape"][0]:
        return _item("error", False, reason="ref_size_unexpected",
                     expected_nelem=spec["shape"][0], board_bytes=len(board_bytes))
    if len(board_bytes) != spec["nbytes"]:
        return _item("error", False, reason="board_hex_len_unexpected",
                     expected_bytes=spec["nbytes"])
    b = np.frombuffer(board_bytes, dtype="<u4")
    r = ref.view("<u4")
    diff = b != r
    n = int(np.count_nonzero(diff))
    if n:
        off = int(np.argmax(diff))
        return _item("mismatch", False, mismatches=n, first_offset=off,
                     first_board_bits=int(b[off]), first_ref_bits=int(r[off]),
                     board_bytes=len(board_bytes))
    return _item("match", False, mismatches=0, first_offset=None,
                 board_bytes=len(board_bytes))


def _f32_ordered(u32):
    """uint32 位型 → 单调有序 int64 键（经典 f32-ULP 距离映射，NaN 落在端点外）。"""
    u = u32.astype(np.uint64)
    return np.where(u < np.uint64(0x80000000),
                    u + np.uint64(0x80000000), np.uint64(0xFFFFFFFF) - u)


def _f32_ulp_dist(cb, rb):
    """f32 位型的 ULP 距离（int64 有符号作差，避免 uint 回绕）。"""
    return np.abs(_f32_ordered(cb).astype(np.int64)
                  - _f32_ordered(rb).astype(np.int64))


def compare_q(cap_q_list, ref_arr, tol_ulp):
    """D：capture 90 float vs npz q_f32 的 float32 位级比较（ULP 容限可放宽）。

    NaN 一律视为不等并单独计数（JSON 往返不保 NaN payload）。q50（通道 1，kW）
    差值仅作二级诊断输出，不影响判据。
    """
    ref = np.ascontiguousarray(ref_arr, dtype=np.float32).reshape(-1)
    if ref.size != Q_FLAT:
        return _item("error", True, reason="ref_size_unexpected", expected_nelem=Q_FLAT)
    cap = np.asarray(cap_q_list, dtype=np.float32)
    if cap.size != Q_FLAT:
        return _item("error", True, reason="capture_q_len_unexpected",
                     got=int(cap.size), expected=Q_FLAT)
    cb, rb = cap.view("<u4"), ref.view("<u4")
    nan_mask = np.isnan(cap) | np.isnan(ref)
    if tol_ulp <= 0:
        eq = cb == rb
    else:
        eq = _f32_ulp_dist(cb, rb) <= np.int64(tol_ulp)
    eq &= ~nan_mask
    diff = ~eq
    n = int(np.count_nonzero(diff))
    nan_n = int(np.count_nonzero(nan_mask))
    it = {"mismatches": n, "nan_mismatches": nan_n,
          "max_ulp": None, "tolerance_ulp": int(tol_ulp)}
    if n:
        off = int(np.argmax(diff))
        ulps = _f32_ulp_dist(cb, rb)[~nan_mask]
        it.update(first_offset=off,
                  first_capture_bits=int(cb[off]), first_ref_bits=int(rb[off]),
                  first_capture=float(cap[off]), first_ref=float(ref[off]),
                  max_ulp=int(ulps.max()) if ulps.size else None)
    else:
        it.update(first_offset=None)
    # 二级诊断：q50（通道 1）kW 差值 + 全通道 max|Δ|（不参与判据）
    d = cap.astype(np.float64) - ref.astype(np.float64)
    d50 = np.abs(d.reshape(30, 3)[:, Q50_CH])
    it["q50_kw_mae"] = float(d50.mean()) if np.isfinite(d50.mean()) else None
    it["q50_kw_max"] = float(d50.max()) if np.isfinite(d50.max()) else None
    it["all_ch_kw_max_abs"] = (float(np.abs(d).max())
                               if np.isfinite(np.abs(d).max()) else None)
    return _item("mismatch" if n else "match", True, **it)


# ---------------------------------------------------------------- 单锚核验

def verify_anchor(vid, t0, cap_row, edge_entry, ref, tol_ulp):
    """对单锚做 A/B/C/D（+ h_past 诊断）。各字段独立按数据齐备情况评估。"""
    items = {}
    for field in INT_FIELDS:
        spec = FIELD_SPEC[field]
        if edge_entry is None:
            items[field] = _item("skip", True, reason="edges_missing")
        elif edge_entry.get(field) is None:
            items[field] = _item("error", True, reason="edges_field_missing")
        elif ref is None or field not in ref:
            items[field] = _item("skip", True, reason="ref_missing")
        else:
            try:
                bb = bytes.fromhex(str(edge_entry[field]).strip())
            except ValueError:
                items[field] = _item("error", True, reason="board_hex_decode_failed")
                continue
            items[field] = compare_bytes(field, bb, ref[field], gating=True)

    # h_past：非门控诊断
    if edge_entry is None:
        items["h_past"] = _item("skip", False, reason="edges_missing")
    elif edge_entry.get("h_past") is None:
        items["h_past"] = _item("error", False, reason="edges_field_missing")
    elif ref is None or "h_past" not in ref:
        items["h_past"] = _item("skip", False, reason="ref_missing")
    else:
        try:
            hb = bytes.fromhex(str(edge_entry["h_past"]).strip())
        except ValueError:
            items["h_past"] = _item("error", False, reason="board_hex_decode_failed")
        else:
            items["h_past"] = compare_hpast(hb, ref["h_past"])

    # D q_f32
    if cap_row is None:
        items["q_f32"] = _item("skip", True, reason="capture_missing")
    elif "error" in cap_row or cap_row.get("q_f32") is None:
        items["q_f32"] = _item("error", True, reason="capture_error_row",
                               capture_error=cap_row.get("error"))
    elif ref is None or "q_f32" not in ref:
        items["q_f32"] = _item("skip", True, reason="ref_missing")
    else:
        items["q_f32"] = compare_q(cap_row["q_f32"], ref["q_f32"], tol_ulp)

    gating_ok = all(not it["gating"] or it["status"] == "match"
                    for it in items.values())
    return {"vid": vid, "t0": t0, "items": items,
            "status": "pass" if gating_ok else "fail"}


def run_verify(capture, edges, ref_dir, tol_ulp, capture_stats=None,
               source_paths=None):
    """主流程：三源锚并集 → 逐锚核验 → 汇总报告（dict，可直接 json.dump）。"""
    refs, ref_ignored = scan_ref_dir(ref_dir)
    keys = set(capture) | set(edges) | set(refs)
    anchors = []
    for vid, t0 in sorted(keys):
        cap_row = capture.get((vid, t0))
        edge = edges.get((vid, t0))
        ref = load_ref_npz(refs[(vid, t0)]) if (vid, t0) in refs else None
        res = verify_anchor(vid, t0, cap_row, edge, ref, tol_ulp)
        # 锚级状态（门控只看三源齐备的共同锚）
        if cap_row is None:
            res["status"] = "missing"          # capture 缺 = MISSING
        elif edge is None:
            res["status"] = "skip"             # edges 缺 = SKIP（板侧未取）
        elif ref is None:
            res["status"] = "skip_ref"
        elif res["status"] != "fail":
            res["status"] = "pass"
        res["seq"] = cap_row.get("seq") if cap_row else None
        res["infer_us"] = cap_row.get("infer_us") if cap_row else None
        res["capture_error"] = cap_row.get("error") if cap_row else None
        anchors.append(res)

    aid = lambda a: f"{a['vid']}__t0_{a['t0']}"  # noqa: E731
    n_status = {s: 0 for s in ("pass", "fail", "skip", "missing", "skip_ref")}
    boundary = {f: {"mismatches": 0, "anchors_affected": 0} for f in
                ("n1_hist_int8_code", "n2_in_kf_code", "n2_out_code",
                 "q_f32", "h_past")}
    nan_total = 0
    anchor_reports = {}
    first_div = None
    for a in anchors:
        n_status[a["status"]] += 1
        rep = {"seq": a["seq"], "vid": a["vid"], "t0": a["t0"],
               "status": a["status"]}
        if a["capture_error"] is not None:
            rep["capture_error"] = a["capture_error"]
        rep["items"] = a["items"]
        anchor_reports[aid(a)] = rep
        for f, agg in boundary.items():
            m = a["items"][f].get("mismatches", 0)
            if m:
                agg["mismatches"] += m
                agg["anchors_affected"] += 1
        nan_total += a["items"]["q_f32"].get("nan_mismatches", 0)
        if first_div is None and a["status"] == "fail":
            for f in GATING_ORDER:
                it = a["items"][f]
                if it["status"] in ("mismatch", "error"):
                    first_div = {"anchor": aid(a), "field": f,
                                 "status": it["status"],
                                 "reason": it.get("reason"),
                                 "first_offset": it.get("first_offset"),
                                 "detail": {k: v for k, v in it.items()
                                            if k not in ("status", "gating",
                                                         "reason")}}
                    break

    common = n_status["pass"] + n_status["fail"]
    overall = "pass" if (common >= 1 and n_status["fail"] == 0) else "fail"
    overall_reason = None
    if common == 0:
        overall_reason = "no_common_anchors（三源齐备锚为 0，无可核验锚）"

    report = {
        "tool": "s3c_verify.py",
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "overall": overall,
        "overall_reason": overall_reason,
        "criterion": "A/B/C int8 逐字节相等；D q_f32 f32 位级相等（NaN 不等），"
                     f"ULP 容限 {tol_ulp}；h_past 仅诊断",
        "inputs": dict(source_paths or {}, q_tolerate_ulp=int(tol_ulp)),
        "totals": {
            "anchors_capture": len(capture), "anchors_edges": len(edges),
            "anchors_ref_npz": len(refs), "ref_files_ignored": ref_ignored,
            "common": common, "pass": n_status["pass"], "fail": n_status["fail"],
            "skip_no_edges": n_status["skip"],
            "missing_no_capture": n_status["missing"],
            "skip_ref_npz": n_status["skip_ref"],
            "q_f32_nan_mismatches": nan_total,
            "capture_duplicates": (capture_stats or {}).get("duplicates", 0),
            "capture_bad_lines": (capture_stats or {}).get("bad_lines", 0),
            "boundary_mismatches": boundary,
        },
        "first_divergence": first_div,
        "assumptions": ASSUMPTIONS,
        "anchors": anchor_reports,
    }
    return report


# ---------------------------------------------------------------- 输出

def _f(x):
    return None if x is None or not np.isfinite(x) else float(x)


def print_summary(report, list_all=False):
    t = report["totals"]
    print("=== s3c_verify 摘要 ===")
    print(f"overall: {report['overall'].upper()}"
          + (f"（{report['overall_reason']}）" if report["overall_reason"] else ""))
    print(f"anchors: capture={t['anchors_capture']} edges={t['anchors_edges']} "
          f"ref={t['anchors_ref_npz']} | common={t['common']} "
          f"pass={t['pass']} fail={t['fail']} skip(no-edges)={t['skip_no_edges']} "
          f"missing(no-capture)={t['missing_no_capture']} "
          f"skip_ref={t['skip_ref_npz']}")
    print("boundary mismatch totals（元素数 / 影响锚数）:")
    for f, agg in t["boundary_mismatches"].items():
        tag = "" if f != "h_past" else "  [非门控诊断]"
        print(f"  {f:<20}: {agg['mismatches']:>8} / {agg['anchors_affected']}"
              f"{tag}")
    print(f"q_f32 NaN mismatch 合计: {t['q_f32_nan_mismatches']}")
    fd = report["first_divergence"]
    if fd:
        print(f"first_divergence: anchor={fd['anchor']} field={fd['field']} "
              f"status={fd['status']} first_offset={fd['first_offset']}")
    else:
        print("first_divergence: none")
    print("per-anchor（FAIL/SKIP/MISSING 全列；PASS 需 --list-all）:")
    short = {"n1_hist_int8_code": "n1", "n2_in_kf_code": "kf",
             "n2_out_code": "out", "h_past": "h", "q_f32": "q"}
    shown = 0
    for key, a in report["anchors"].items():
        if a["status"] == "pass" and not list_all:
            continue
        flags = " ".join(
            f"{short[f]}={it['status']}"
            f"{'(' + str(it.get('mismatches', 0)) + '@' + str(it.get('first_offset')) + ')'}"
            if it.get("mismatches") else f"{short[f]}={it['status']}"
            for f, it in a["items"].items())
        extra = f" err={a['capture_error']}" if a.get("capture_error") else ""
        print(f"  {a['status']:<8} {key}  {flags}{extra}")
        shown += 1
    if shown == 0:
        print("  （无非 PASS 锚）")


# ---------------------------------------------------------------- dry-run

def _mk_synthetic(ref_dir, rng, vid="DRYRUN_VID"):
    """生成 2 锚合成数据（ref npz 落盘 + capture/edges 内容），返回
    (anchors_meta, capture_rows, edges_dict)。"""
    anchors = [(vid, 0), (vid, 12)]
    cap_rows, edges = [], {}
    for i, (v, t0) in enumerate(anchors):
        n1 = rng.integers(-128, 128, size=(300, 11), dtype=np.int8)
        kf = rng.integers(-128, 128, size=(1, 6, 30), dtype=np.int8)
        out = rng.integers(-128, 128, size=(30, 3), dtype=np.int8)
        hp = rng.standard_normal(96).astype(np.float32)
        q = (rng.standard_normal((30, 3)) * 50.0).astype(np.float32)
        np.savez(os.path.join(ref_dir, f"{v}_t0_{t0:05d}.npz"),
                 n1_hist_int8_code=n1, n2_in_kf_code=kf, n2_out_code=out,
                 h_past=hp, q_f32=q)
        cap_rows.append({"seq": i, "vid": v, "t0": t0,
                         "q_f32": q.reshape(-1).tolist(), "infer_us": 1000 + i})
        edges[f"{v}__t0_{t0}"] = {
            "n1_hist_int8_code": n1.tobytes().hex(),
            "n2_in_kf_code": kf.tobytes().hex(),
            "n2_out_code": out.tobytes().hex(),
            "h_past": hp.astype("<f4").tobytes().hex(),
        }
    return anchors, cap_rows, edges


def _write_inputs(tmp, cap_rows, edges):
    cap_path = os.path.join(tmp, "capture.jsonl")
    with open(cap_path, "w", encoding="utf-8") as f:
        for r in cap_rows:
            f.write(json.dumps(r) + "\n")
    edges_path = os.path.join(tmp, "edges.json")
    json.dump(edges, open(edges_path, "w", encoding="utf-8"))
    return cap_path, edges_path


def dry_run():
    """合成 2 锚自检比较逻辑：全等 → pass；1-ULP 扰动 → fail/tol1 过；
    n2_out_code 单字节翻转 → fail 且 first_divergence 定位正确。"""
    rng = np.random.default_rng(20260926)
    checks = []
    with tempfile.TemporaryDirectory(prefix="s3c_verify_dry_") as tmp:
        ref_dir = os.path.join(tmp, "ref")
        os.makedirs(ref_dir)
        anchors, cap_rows, edges = _mk_synthetic(ref_dir, rng)
        cap_path, edges_path = _write_inputs(tmp, cap_rows, edges)
        cap, cstats = load_capture(cap_path)
        eg = load_edges(edges_path)

        def run(tol=0, rows=None, ed=None):
            r = run_verify(rows if rows is not None else cap, ed if ed is not None else eg,
                           ref_dir, tol, cstats,
                           {"capture": "dry-run", "edges": "dry-run",
                            "ref_dir": "dry-run"})
            return r

        r0 = run()
        checks.append(("基线合成数据 overall=pass", r0["overall"] == "pass"))

        # q_f32 注入 1 ULP（锚 t0=0, 展平 idx=52）
        rows2 = [dict(r) for r in cap_rows]
        q = np.asarray(rows2[0]["q_f32"], dtype=np.float32).copy()
        q.view("<u4")[52] ^= np.uint32(1)  # 尾数 ±1 → 1 ULP
        rows2[0]["q_f32"] = q.tolist()
        cp2, _ = _write_inputs(tmp, rows2, edges)
        cap2, _ = load_capture(cp2)
        r1 = run(rows=cap2)
        fd = r1["first_divergence"]
        checks.append(("q 1-ULP → fail 且定位 q_f32@52",
                       r1["overall"] == "fail" and fd
                       and fd["anchor"] == f"{anchors[0][0]}__t0_{anchors[0][1]}"
                       and fd["field"] == "q_f32" and fd["first_offset"] == 52))
        r2 = run(tol=1, rows=cap2)
        checks.append(("q 1-ULP + --q-tolerate-ulp 1 → pass", r2["overall"] == "pass"))

        # 恢复 q，翻转锚 t0=12 的 n2_out_code 第 45 字节
        eb = dict(edges)
        e2 = dict(eb[f"{anchors[1][0]}__t0_{anchors[1][1]}"])
        b = bytearray(bytes.fromhex(e2["n2_out_code"]))
        b[45] ^= 0xFF
        e2["n2_out_code"] = b.hex()
        eb[f"{anchors[1][0]}__t0_{anchors[1][1]}"] = e2
        _, ep3 = _write_inputs(tmp, cap_rows, eb)
        eg3 = load_edges(ep3)
        r3 = run(ed=eg3)
        fd3 = r3["first_divergence"]
        checks.append(("n2_out_code 单字节翻转 → fail 且定位 @45",
                       r3["overall"] == "fail" and fd3
                       and fd3["anchor"] == f"{anchors[1][0]}__t0_{anchors[1][1]}"
                       and fd3["field"] == "n2_out_code"
                       and fd3["first_offset"] == 45))

    ok = all(c for _, c in checks)
    print("=== s3c_verify --dry-run 自检 ===")
    for name, c in checks:
        print(f"  [{'PASS' if c else 'FAIL'}] {name}")
    print(f"dry-run self-check: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


# ---------------------------------------------------------------- main

def main(argv=None):
    ap = argparse.ArgumentParser(
        description="EMS04-S3-C 板侧对拍核验器（capture + 边界码 vs host B 参考 npz）",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("--capture", help="板侧响应 capture jsonl（s3c_reatt_replay.py 产出）")
    ap.add_argument("--edges", help="调试器内存读取边界码 json（<vid>__t0_<t0> → hex 字段）")
    ap.add_argument("--ref-dir", help="host B 参考 npz 目录（<vid>_t0_<t0:05d>.npz）")
    ap.add_argument("--out", help="机器可读报告输出 json 路径（非 dry-run 必填）")
    ap.add_argument("--q-tolerate-ulp", type=int, default=0,
                    help="D 项 q_f32 允许的最大 ULP 距离（默认 0 = 位级全等）")
    ap.add_argument("--dry-run", action="store_true",
                    help="无输入：合成 2 锚数据自检比较逻辑并打印 PASS/FAIL")
    ap.add_argument("--list-all", action="store_true",
                    help="stdout 摘要列出全部锚（默认只列非 PASS）")
    args = ap.parse_args(argv)

    if args.dry_run:
        return dry_run()

    missing = [n for n in ("capture", "edges", "ref_dir") if not getattr(args, n)]
    if missing:
        ap.error(f"缺少必填参数: --{', --'.join(m.replace('_', '-') for m in missing)}"
                 "（或使用 --dry-run）")
    if not args.out:
        ap.error("非 dry-run 模式必须提供 --out <json>")
    if args.q_tolerate_ulp < 0:
        ap.error("--q-tolerate-ulp 必须 >= 0")
    for p in (args.capture, args.edges):
        if not os.path.isfile(p):
            ap.error(f"输入文件不存在: {p}")
    if not os.path.isdir(args.ref_dir):
        ap.error(f"参考目录不存在: {args.ref_dir}")

    capture, cstats = load_capture(args.capture)
    edges = load_edges(args.edges)
    report = run_verify(capture, edges, args.ref_dir, args.q_tolerate_ulp, cstats,
                        {"capture": os.path.relpath(args.capture, find_repo_root(SCRIPT_DIR)),
                         "edges": os.path.relpath(args.edges, find_repo_root(SCRIPT_DIR)),
                         "ref_dir": os.path.relpath(args.ref_dir, find_repo_root(SCRIPT_DIR))})
    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print_summary(report, list_all=args.list_all)
    print(f"report: {args.out}")
    return 0 if report["overall"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())

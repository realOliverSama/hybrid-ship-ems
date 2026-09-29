#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_p3_pack_stream.py — S4-P3 B1：formal 航次 stream bin 打包器（S4_P3_STREAM_PLANT_SPEC §3，格式冻结）。

上游（只读参照）：
  冻结件  Host/results/s4_c4/S4_P3_FORMAL_ARM_FREEZE.json
          （battery 4 航次 = FLTA_S2_D01006_M1/M2 + FLTA_S6_D05003_M1/M2；
           plant_source_resolution_E1 = Path S″ 流驱动植物变体）
  规格    experiments/softhil_ems03_2026-09-25/S4_P3_STREAM_PLANT_SPEC.md §3

二进制格式（冻结）：

```
offset  0    8   magic                 = b"EMS04STM"
offset  8    4   u32 format_version    = 1
offset  12   4   u32 n_rows
offset  16   4   u32 reserved          = 0
offset  20   ... n_rows × (f64 × 4)
```
行 = [t_s, P_prop_realized_kW, P_hotel_kW, v_ship_mps]，'<f8' C-order，全小端。

源列白名单（AST 静态自查 `_static_whitelist_audit` 防回归）：
  stream.time_s / stream.P_load_total_kW / stream.P_hotel_kW / stream.v_ship_mps
推导列：P_prop_realized_kW := P_load_total_kW − P_hotel_kW（规格 §3 冻结口径：
  stream 的 P_prop_demand_kW 是指令值非母线实取，直接回放会虚构母线负载）。
禁读（闭环响应列，模型闭环要重现的对象，回放会造成双重闭环）：SOC/Vdc/P_gen/P_bat
  等列 —— 本文件 AST 自查禁词表（loadmat 即便载入也不得引用）。

产物：
  Host/plantstep_e_stream/<vid>/<vid>.stream.f64.bin
  Host/plantstep_e_stream/<vid>/stream_check.json
    （bin sha256 + n_rows == duration_s+1 + 首末行数值指纹 + 派生列恒等式复算
     + 独立第二路径全行逐位比对）

用法（CWD 任意，脚本内相对推导仓库根；解释器 `.venv_tf/bin/python`）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本>                 # 打包冻结件全部 4 航次
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本> --voyage-id FLTA_S2_D01006_M1
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本> --selfcheck      # 只读复核（不重写 bin）
"""
from __future__ import annotations

import argparse
import ast
import datetime
import hashlib
import json
import os
import struct
import sys

import numpy as np
from scipy.io import loadmat

# ------------------------------------------------------------------- constants
MAGIC = b"EMS04STM"
FORMAT_VERSION = 1
HEADER_BYTES = 20
ROW_BYTES = 4 * 8
SRC_MAT_DIR = os.path.join("model", "simulink_modelE_(Phase4_Physics)",
                           "datasets_v3", "formal", "iid")
FREEZE_REL = os.path.join("firmware", "stm32n6", "04_SourceCode",
                          "SoftwarePackage", "EMS_Projects",
                          "EMS04_CHIL_FULLCTRL", "Host", "results", "s4_c4",
                          "S4_P3_FORMAL_ARM_FREEZE.json")
OUT_DIR_REL = os.path.join("plantstep_e_stream")
CHECK_SCHEMA = "s4_p3_stream_check_v1"

# 源列白名单（唯四允许引用的 stream 列 + 冻结推导列名）
WHITELIST_COLS = ("time_s", "P_load_total_kW", "P_hotel_kW", "v_ship_mps")
DERIVED_COL = "P_prop_realized_kW"
# 禁读列（闭环响应/指令值/非回放面；出现即 AST 自查 FAIL，不出表）
FORBIDDEN_STRING_KEYS = (
    "SOC", "V_dc_V", "V_bat_V", "I_bat_A",
    "P_gen_actual_kW", "P_bat_actual_kW", "P_bat_kW",
    "P_bat_dis_avail_kW", "P_bat_chg_avail_kW",
    "P_prop_demand_kW", "n_prop_rpm", "acceleration_mps2", "x_ship_m",
    "manual_power_cmd_pct", "ctrl_mode", "resistance_multiplier",
    "state_DG1", "state_DG2",
)


class StreamPackError(RuntimeError):
    pass


def find_repo_root(start: str) -> str:
    """仓库根 = 向上第一个同时含 AGENTS.md + model/ + firmware/ 的目录。"""
    cur = os.path.abspath(start)
    for _ in range(16):
        if (os.path.isfile(os.path.join(cur, "AGENTS.md"))
                and os.path.isdir(os.path.join(cur, "model"))
                and os.path.isdir(os.path.join(cur, "firmware"))):
            return cur
        nxt = os.path.dirname(cur)
        if nxt == cur:
            break
        cur = nxt
    raise SystemExit("repo root not found above %s" % start)


HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)
REPO = find_repo_root(HOST)


def rel(path: str) -> str:
    return os.path.relpath(path, REPO).replace("\\", "/")


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ------------------------------------------------------- static whitelist audit
def _static_whitelist_audit() -> dict:
    """AST 静态自查：本文件（除禁止清单自身定义处）不得出现禁读列名字符串。"""
    with open(os.path.abspath(__file__), encoding="utf-8") as fh:
        src = fh.read()
    tree = ast.parse(src)
    exempt = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(n in ("FORBIDDEN_STRING_KEYS",) for n in names):
                for sub in ast.walk(node.value):
                    if isinstance(sub, ast.Constant):
                        exempt.add(id(sub))
    bad = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            if id(node) not in exempt and node.value in FORBIDDEN_STRING_KEYS:
                bad.append(node.value)
    if bad:
        raise StreamPackError("静态白名单自查失败：禁读列引用=%s" % sorted(set(bad)))
    return {"forbidden_string_keys": list(FORBIDDEN_STRING_KEYS),
            "whitelist_cols": list(WHITELIST_COLS) + [DERIVED_COL],
            "exempt_constants": len(exempt), "violations": [], "pass": True}


# ------------------------------------------------------------- freeze + source
def load_frozen_voyages():
    """冻结电池航次清单（fail-closed：缺文件/结构异 → 明确错误）。"""
    fp = os.path.join(REPO, FREEZE_REL)
    if not os.path.isfile(fp):
        raise StreamPackError("冻结件缺失: %s" % rel(fp))
    freeze = json.load(open(fp))
    if freeze.get("schema") != "s4_p3_formal_arm_freeze_v1" \
            or freeze.get("status") != "FROZEN":
        raise StreamPackError("冻结件 schema/status 异常: %s/%s"
                              % (freeze.get("schema"), freeze.get("status")))
    battery = freeze.get("battery")
    if not isinstance(battery, list) or not battery:
        raise StreamPackError("冻结件 battery 结构异常（需非空 list）")
    for e in battery:
        for k in ("role", "voyage_id", "arm"):
            if not isinstance(e.get(k), str) or not e[k]:
                raise StreamPackError("冻结件条目缺 %s: %r" % (k, e))
    return fp, freeze, [(e["voyage_id"], e["role"], e["arm"],
                         e["arms_forecast"]) for e in battery]


def read_stream_whitelist(mat_path: str) -> dict:
    """只取材白名单 4 列（其余字段即便被 loadmat 载入也不得被引用）。"""
    m = loadmat(mat_path, simplify_cells=True)
    st = m["stream"]
    meta = m["meta"]
    t = np.asarray(st["time_s"], dtype=np.float64).ravel()
    p_load = np.asarray(st["P_load_total_kW"], dtype=np.float64).ravel()
    p_hotel = np.asarray(st["P_hotel_kW"], dtype=np.float64).ravel()
    v = np.asarray(st["v_ship_mps"], dtype=np.float64).ravel()
    return {"t": t, "p_load": p_load, "p_hotel": p_hotel, "v": v,
            "voyage_id": str(meta["voyage_id"]),
            "duration_s": int(meta["voyage_duration_s"])}


def preflight(d: dict) -> None:
    """打包前校验；任一不符即 raise。"""
    t, p_load, p_hotel, v = d["t"], d["p_load"], d["p_hotel"], d["v"]
    n = t.size
    errs = []
    if not (p_load.size == p_hotel.size == v.size == n):
        errs.append("4 列长度不一致: %s"
                    % ((t.size, p_load.size, p_hotel.size, v.size),))
    if not (n >= 2 and float(t[0]) == 0.0):
        errs.append("time_s 首值非 0: %r" % (t[:1],))
    steps = np.diff(t)
    if not (np.unique(steps).size == 1 and abs(steps[0] - 1.0) < 1e-9):
        errs.append("time_s 非唯一 1 Hz 网格: uniq_steps=%s" % np.unique(steps)[:5])
    if n != d["duration_s"] + 1:
        errs.append("n_rows(%d) != duration_s+1(%d)" % (n, d["duration_s"] + 1))
    for name, col in (("P_load_total_kW", p_load), ("P_hotel_kW", p_hotel),
                      ("v_ship_mps", v)):
        if not np.all(np.isfinite(col)):
            errs.append("%s 含非有限值" % name)
    if errs:
        raise StreamPackError("; ".join(errs))


# ------------------------------------------------------------------ pack core
def build_bin_bytes(d: dict) -> bytes:
    n = int(d["t"].size)
    prop = d["p_load"] - d["p_hotel"]          # 冻结推导列（f64 RNE 语义内为精确减法）
    rows = np.ascontiguousarray(
        np.stack([d["t"], prop, d["p_hotel"], d["v"]], axis=1),
        dtype="<f8").tobytes()
    if len(rows) != n * ROW_BYTES:
        raise StreamPackError("rows 字节数 %d != n*%d" % (len(rows), n, ROW_BYTES))
    head = struct.pack("<8sIII", MAGIC, FORMAT_VERSION, n, 0)
    if len(head) != HEADER_BYTES:
        raise StreamPackError("header 长度 %d != %d" % (len(head), HEADER_BYTES))
    return head + rows


def parse_bin(blob: bytes) -> dict:
    if len(blob) < HEADER_BYTES:
        raise StreamPackError("bin 过短: %d" % len(blob))
    magic, ver, n, reserved = struct.unpack_from("<8sIII", blob, 0)
    payload = blob[HEADER_BYTES:]
    if len(payload) != n * ROW_BYTES:
        raise StreamPackError("payload 长度 %d != n(%d)×%d"
                              % (len(payload), n, ROW_BYTES))
    arr = np.frombuffer(payload, dtype="<f8").reshape(n, 4)
    return {"magic": magic, "version": ver, "n_rows": n, "reserved": reserved,
            "t": np.asarray(arr[:, 0], dtype=np.float64),
            "prop": np.asarray(arr[:, 1], dtype=np.float64),
            "hotel": np.asarray(arr[:, 2], dtype=np.float64),
            "v": np.asarray(arr[:, 3], dtype=np.float64)}


def _fp_row(t, prop, hotel, v, i: int) -> dict:
    """第 i 行数值指纹（float.hex 逐位无损）。"""
    return {"i": int(i),
            "t_s_hex": float(t[i]).hex(),
            "P_prop_realized_kW_hex": float(prop[i]).hex(),
            "P_hotel_kW_hex": float(hotel[i]).hex(),
            "v_ship_mps_hex": float(v[i]).hex()}


# ---------------------------------------------------------------- per voyage
def pack_voyage(vid: str, role: str, arm: str, fq: str,
                freeze_path: str, freeze_sha: str, mode: str) -> dict:
    out_dir = os.path.join(HOST, OUT_DIR_REL, vid)
    bin_path = os.path.join(out_dir, "%s.stream.f64.bin" % vid)
    json_path = os.path.join(out_dir, "stream_check.json")

    mat_rel = os.path.join(SRC_MAT_DIR, vid + ".mat")
    mat_path = os.path.join(REPO, mat_rel)
    if not os.path.isfile(mat_path):
        raise StreamPackError("源 .mat 不存在: %s" % mat_rel)

    d = read_stream_whitelist(mat_path)
    if d["voyage_id"] != vid:
        raise StreamPackError("meta.voyage_id=%r 与 %r 不一致"
                              % (d["voyage_id"], vid))
    preflight(d)

    blob = build_bin_bytes(d)
    disk_identical = None
    if mode == "pack":
        os.makedirs(out_dir, exist_ok=True)
        with open(bin_path, "wb") as fh:
            fh.write(blob)
    elif not os.path.isfile(bin_path):
        raise StreamPackError("selfcheck 缺盘上 bin: %s" % rel(bin_path))
    with open(bin_path, "rb") as fh:
        disk_blob = fh.read()
    disk_identical = disk_blob == blob
    if not disk_identical:
        raise StreamPackError("%s：盘上 bin 与重算逐字节不一致（disk=%d B vs "
                              "recompute=%d B）" % (mode, len(disk_blob),
                                                    len(blob)))

    parsed = parse_bin(disk_blob)
    # 派生列恒等式（独立第二路径）：重 loadmat → 重算 → 与盘上 bin 全行逐位比对
    d2 = read_stream_whitelist(mat_path)
    prop2 = d2["p_load"] - d2["p_hotel"]
    bitwise = {
        "t_s": bool(np.array_equal(parsed["t"].astype("<f8"),
                                   d2["t"].astype("<f8"))),
        "P_prop_realized_kW": bool(np.array_equal(parsed["prop"],
                                                  prop2.astype("<f8"))),
        "P_hotel_kW": bool(np.array_equal(parsed["hotel"],
                                          d2["p_hotel"].astype("<f8"))),
        "v_ship_mps": bool(np.array_equal(parsed["v"], d2["v"].astype("<f8"))),
    }
    # 恒等式残差（信息面）：(P_load−P_hotel)+P_hotel 与 P_load 的最大偏差（~ULP）
    resid = float(np.max(np.abs((prop2 + d2["p_hotel"]) - d2["p_load"])))

    readback = {
        "magic_ok": bool(parsed["magic"] == MAGIC),
        "version_ok": bool(parsed["version"] == FORMAT_VERSION),
        "reserved_ok": bool(parsed["reserved"] == 0),
        "n_rows_ok": bool(parsed["n_rows"] == d["t"].size),
        "n_rows_eq_duration_plus_1":
            bool(parsed["n_rows"] == d["duration_s"] + 1),
        "total_bytes_ok":
            bool(len(disk_blob) == HEADER_BYTES + parsed["n_rows"] * ROW_BYTES),
        "t_grid_ok": bool(parsed["t"][0] == 0.0
                          and np.unique(np.diff(parsed["t"])).size == 1
                          and abs(np.diff(parsed["t"])[0] - 1.0) < 1e-9),
        "all_finite": bool(np.all(np.isfinite(parsed["prop"]))
                           and np.all(np.isfinite(parsed["hotel"]))
                           and np.all(np.isfinite(parsed["v"]))
                           and np.all(np.isfinite(parsed["t"]))),
        "on_disk_byte_identical_to_recomputed": bool(disk_identical),
        "bitwise_exact_vs_source": bool(all(bitwise.values())),
    }

    overall = bool(all(readback.values()))
    payload = {
        "schema": CHECK_SCHEMA,
        "created_utc": utc_now(),
        "mode": mode,
        "voyage_id": vid,
        "role": role,
        "arm": arm,
        "arms_forecast": fq,
        "freeze": {"path": FREEZE_REL.replace(os.sep, "/"),
                   "sha256": freeze_sha},
        "source_mat": {"path": mat_rel.replace(os.sep, "/"),
                       "sha256": sha256_file(mat_path)},
        "bin": {"path": rel(bin_path),
                "total_bytes": len(disk_blob),
                "sha256": hashlib.sha256(disk_blob).hexdigest(),
                "header_bytes": HEADER_BYTES,
                "row_bytes": ROW_BYTES,
                "header": {"magic": MAGIC.decode("ascii"),
                           "format_version": FORMAT_VERSION,
                           "n_rows": int(parsed["n_rows"]),
                           "reserved": int(parsed["reserved"])},
                "columns": [ "t_s", DERIVED_COL, "P_hotel_kW", "v_ship_mps"],
                "row_layout": "f64 ×4 '<f8' C-order，全小端"},
        "grid": {"n_stream": int(d["t"].size),
                 "duration_s": int(d["duration_s"]),
                 "t_first": float(d["t"][0]),
                 "t_last": float(d["t"][-1])},
        "fingerprint": {"first_row": _fp_row(parsed["t"], parsed["prop"],
                                             parsed["hotel"], parsed["v"], 0),
                        "last_row": _fp_row(parsed["t"], parsed["prop"],
                                            parsed["hotel"], parsed["v"],
                                            parsed["n_rows"] - 1)},
        "derived_identity": {
            "definition": "P_prop_realized_kW := P_load_total_kW − P_hotel_kW",
            "bitwise_exact_vs_source": bitwise,
            "max_abs_residual_kw_(prop+hotel)-load": resid},
        "checks": {"readback": readback,
                   "static_audit": _static_whitelist_audit()},
        "overall": overall,
        "errors": [],
    }
    os.makedirs(out_dir, exist_ok=True)
    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=1, ensure_ascii=False)
    print(json.dumps({"overall": overall, "voyage_id": vid, "mode": mode,
                      "n_rows": int(parsed["n_rows"]),
                      "total_bytes": len(disk_blob),
                      "bin_sha256": payload["bin"]["sha256"]}, indent=1))
    return payload


# ----------------------------------------------------------------------- main
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="EMS04 S4-P3 B1 formal 航次 stream bin 打包器"
                    "（EMS04STM v1，规格 §3 冻结格式）")
    ap.add_argument("--voyage-id", default=None,
                    help="单航次模式（须属于冻结 battery；缺省 = 冻结件全部航次）")
    ap.add_argument("--selfcheck", action="store_true",
                    help="只读复核：重算并与盘上 bin 逐字节比对（不重写 bin）")
    args = ap.parse_args(argv)
    mode = "selfcheck" if args.selfcheck else "pack"
    try:
        if _static_whitelist_audit()["pass"] is not True:
            raise StreamPackError("静态白名单自查未通过")
        freeze_path, freeze, voyages = load_frozen_voyages()
        freeze_sha = sha256_file(freeze_path)
        ids = [v[0] for v in voyages]
        if args.voyage_id:
            if args.voyage_id not in ids:
                ap.error("--voyage-id 不在冻结 battery: %s（冻结集合=%s）"
                         % (args.voyage_id, ids))
            voyages = [v for v in voyages if v[0] == args.voyage_id]
        results = [pack_voyage(vid, role, arm, fq, freeze_path, freeze_sha,
                               mode) for (vid, role, arm, fq) in voyages]
        ok = all(r["overall"] for r in results)
        print("PACK-STREAM: %s (%d/%d voyage bins %s)"
              % ("PASS" if ok else "FAIL", sum(1 for r in results
                                               if r["overall"]),
                 len(results), mode))
        return 0 if ok else 1
    except (StreamPackError, OSError) as e:
        print("FAIL: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

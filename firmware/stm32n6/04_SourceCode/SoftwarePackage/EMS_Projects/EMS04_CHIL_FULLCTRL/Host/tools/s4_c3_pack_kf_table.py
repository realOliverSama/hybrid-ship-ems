#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_c3_pack_kf_table.py — EMS04 S4-C3：S20 PILOT fixture kf 表打包器。

格式与自检方法同源（只读参照，禁改）：
  `experiments/softhil_ems03_2026-09-25/ems04_s2/a/tools/pack_kf_table.py`
二进制格式 EMS04KFT v1 冻结，全小端。

forecast_profile_code 分配登记（本脚本新增）：
  1 = GOOD_V1, 2 = DEGRADED_V1（EMS04-S2 已用）；
  3 = PILOT_FIXTURE（S4-C3 新分配，对应 meta.fixture_kind = "PILOT_ONLY"
  的 S20 合成 PILOT 航次）。

输入白名单（**只允许**读取以下字段）：
  kf.issue_time_s / kf.lead_s / kf.v_plan_mps / kf.s_plan_m / kf.h_chart_m /
  kf.P_hotel_plan_kW / kf.u_fcst_mps / kf.wl_fcst_m
  meta.voyage_id
  meta.forecast_provenance.{forecast_generator_id, forecast_generator_version,
                            forecast_generator_hash, issue_time_s,
                            source_cutoff_time_s, forecast_provenance_class}
  meta.fixture_kind
严禁引用未来真值列（实际航速 / 实际水位 / 发电机实际出力等）、预测真值层与
逐拍真值轨迹属性 —— 本文件自带 AST 静态自查（`_static_whitelist_audit`）防回归。

二进制格式（冻结；header 216 B）：

```
offset  0    8   magic                 = b"EMS04KFT"
offset  8    4   u32 format_version    = 1
offset  12   4   u32 contract_version  = 0x00010000
offset  16   4   u32 forecast_profile_code (3 = PILOT_FIXTURE)
offset  20   4   u32 n_rows
offset  24   8   f64 issue_first
offset  32   8   f64 issue_step
offset  40   32  voyage_id[32]                  ASCII, NUL 填充
offset  72   32  forecast_generator_id[32]      ASCII, NUL 填充
offset  104  16  forecast_generator_version[16] ASCII, NUL 填充
offset  120  64  forecast_generator_hash[64]    hex ASCII（64 字符）
offset  184  32  payload_sha256[32]             对全部 row payload
offset  216  ... n_rows × [ f64 issue_time_s | 720 B kf ]
```
行内 kf = (30,6) `<f4` C-order（k-major：每行一个 lead 的 6 个通道），列序冻结 =
("v_plan_mps","s_plan_m","h_chart_m","P_hotel_plan_kW","u_fcst_mps","wl_fcst_m")；
f64→f32 统一 numpy astype('<f4')（RNE）。

用法（CWD 任意，脚本内相对推导仓库根；解释器 `.venv_tf/bin/python`）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本> --pack
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本> --selfcheck
泛化用法（任意 pilot 航次；--mat 与 --voyage-id 必须同时给出）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本> --pack \
      --mat "model/simulink_modelE_(Phase4_Physics)/datasets_v3/pilot/<vid>.mat" \
      --voyage-id <vid>
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本> --selfcheck \
      --mat ... --voyage-id <vid>
S4-P3 formal 臂（2026-09-29 参数化，向后兼容扩展；缺省行为逐字节不变）：
  forecast_profile_code 1=GOOD_V1 / 2=DEGRADED_V1 / 3=PILOT_FIXTURE（登记表见
  FORECAST_PROFILE_CODES）；formal 航次示例（M1 臂→1，M2 臂→2）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python <本脚本> --pack \
      --mat "model/simulink_modelE_(Phase4_Physics)/datasets_v3/formal/iid/<vid>.mat" \
      --voyage-id <vid> --profile-code 1
泛化产物：results/s4_c3/<vid>.kf_table.bin 与
s4_c3_kf_table_selfcheck_<vid>.json（缺省两参数时 = S20 既有路径与既有文件名，
行为与历史版本逐字节一致）。
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
MAGIC = b"EMS04KFT"
FORMAT_VERSION = 1
CONTRACT_VERSION = 0x00010000
FORECAST_PROFILE_CODES = {1: "GOOD_V1", 2: "DEGRADED_V1",
                          3: "PILOT_FIXTURE"}
HEADER_BYTES = 216
ROW_BYTES = 8 + 30 * 6 * 4
KF_COLS = ("v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW",
           "u_fcst_mps", "wl_fcst_m")
WHITELIST_KF = KF_COLS + ("issue_time_s", "lead_s")
EXPECTED_LEADS = 10.0 * np.arange(1, 31)
EXPECTED_ISSUE_FIRST = 300.0
MIN_ISSUE_ROWS = 2

FORBIDDEN_STRING_KEYS = ("stream", "P_gen_actual_kW", "u_actual_future_mps",
                         "wl_actual_future_m", "oracle", "ORACLE", "Y")
FORBIDDEN_ATTRS = ("y_trajectory", "e300_of")

SELFCHECK_SCHEMA = "s4_c3_kf_table_selfcheck_v1"


class KFError(RuntimeError):
    pass


# ---------------------------------------------------------------- repo anchors
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
REPO = find_repo_root(HERE)

SRC_MAT_REL = os.path.join("model", "simulink_modelE_(Phase4_Physics)",
                           "datasets_v3", "pilot", "S20_composite.mat")
OUT_DIR_REL = os.path.join("experiments", "softhil_ems03_2026-09-25",
                           "results", "s4_c3")
TABLE_NAME = "S20_composite.kf_table.bin"
SELFCHECK_NAME = "s4_c3_kf_table_selfcheck.json"


def rel(path: str) -> str:
    return os.path.relpath(path, REPO).replace("\\", "/")


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


# ------------------------------------------------------- static whitelist audit
def _static_whitelist_audit() -> dict:
    """AST 静态自查：本文件（排除禁止清单自身定义处）不得引用禁词/禁属性。"""
    with open(os.path.abspath(__file__), encoding="utf-8") as fh:
        src = fh.read()
    tree = ast.parse(src)
    # 豁免：禁止清单自身的字面定义（否则自查会命中自己的定义表）
    exempt = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(n in ("FORBIDDEN_STRING_KEYS", "FORBIDDEN_ATTRS")
                   for n in names):
                for sub in ast.walk(node.value):
                    if isinstance(sub, ast.Constant):
                        exempt.add(id(sub))
    bad_str, bad_attr = [], []
    for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            if id(node) not in exempt and node.value in FORBIDDEN_STRING_KEYS:
                bad_str.append(node.value)
        if isinstance(node, ast.Attribute) and node.attr in FORBIDDEN_ATTRS:
            bad_attr.append(node.attr)
    if bad_str or bad_attr:
        raise KFError("静态白名单自查失败：字符串=%s 属性=%s"
                      % (bad_str, bad_attr))
    return {"forbidden_string_keys": list(FORBIDDEN_STRING_KEYS),
            "forbidden_attrs": list(FORBIDDEN_ATTRS),
            "exempt_constants": len(exempt), "violations": [], "pass": True}


# ------------------------------------------------------- whitelist data access
def read_kf_whitelist(mat_path: str) -> dict:
    """只取材白名单字段（其余字段即便被 loadmat 载入也不得被引用）。"""
    m = loadmat(mat_path, simplify_cells=True)
    kf = m["kf"]
    meta = m["meta"]
    prov = meta["forecast_provenance"]
    issue = np.asarray(kf["issue_time_s"], dtype=float).ravel()
    leads = np.asarray(kf["lead_s"], dtype=float)
    cols = {c: np.asarray(kf[c], dtype=float) for c in KF_COLS}
    return {"issue_time_s": issue, "lead_s": leads, "cols": cols,
            "voyage_id": str(meta["voyage_id"]),
            "fixture_kind": str(meta["fixture_kind"]),
            "provenance": {"forecast_generator_id":
                               str(prov["forecast_generator_id"]),
                           "forecast_generator_version":
                               str(prov["forecast_generator_version"]),
                           "forecast_generator_hash":
                               str(prov["forecast_generator_hash"]),
                           "forecast_issue_time_s":
                               float(prov["issue_time_s"]),
                           "forecast_source_cutoff_time_s":
                               float(prov["source_cutoff_time_s"]),
                           "forecast_provenance_class":
                               str(prov["forecast_provenance_class"])}}


def preflight_checks(d: dict) -> dict:
    """打包前校验；任一不符即 raise KFError（退出码 1）。

    issue 网格泛化：等步长（diff 唯一）+ issue_first=%g + n_rows>=%d；
    终点与行数不再写死（S20 5700/541 为其一个特例）。
    """ % (EXPECTED_ISSUE_FIRST, MIN_ISSUE_ROWS)
    issue = d["issue_time_s"]
    leads = d["lead_s"]
    errs = []
    steps = np.diff(issue)
    step_uniq = np.unique(steps)
    grid_ok = (issue.size >= MIN_ISSUE_ROWS
               and float(issue[0]) == EXPECTED_ISSUE_FIRST)
    step_ok = step_uniq.size == 1
    if not (grid_ok and step_ok):
        errs.append("issue 网格不符: size=%d (需>=%d) first=%s "
                    "uniq_steps=%s"
                    % (issue.size, MIN_ISSUE_ROWS, issue[:1], step_uniq[:5]))
    order = np.argsort(leads, axis=1, kind="stable")
    identity = bool(np.all(order == np.arange(leads.shape[1])[None, :]))
    if not identity:
        errs.append("lead_s stable argsort 非恒等映射")
    leads_ok = bool(np.allclose(np.sort(leads, axis=1),
                                EXPECTED_LEADS[None, :],
                                rtol=0.0, atol=1e-9))
    if not leads_ok:
        errs.append("lead_s 取值集合与冻结网格 10:10:300 不符")
    shapes_ok = all(d["cols"][c].shape == (issue.size, 30)
                    for c in KF_COLS)
    if not shapes_ok:
        bad = {c: d["cols"][c].shape for c in KF_COLS
               if d["cols"][c].shape != (issue.size, 30)}
        errs.append("内容列形状不符: %s" % bad)
    if errs:
        raise KFError("; ".join(errs))
    return {"issue_grid_ok": bool(grid_ok),
            "issue_step_unique": bool(step_ok),
            "lead_argsort_identity": identity,
            "lead_sorted_equals_frozen_grid": leads_ok,
            "column_shapes_ok": bool(shapes_ok)}


def header_field_lengths_ok(d: dict) -> bool:
    prov = d["provenance"]
    return all(len(s.encode("ascii")) <= n for s, n in
               ((d["voyage_id"], 32),
                (prov["forecast_generator_id"], 32),
                (prov["forecast_generator_version"], 16),
                (prov["forecast_generator_hash"], 64)))


# ------------------------------------------------------------------ pack core
def row_bytes(cols: dict, row: int) -> bytes:
    """第 row 行 → 720 B（列序 = KF_COLS 冻结序，<f4 C-order k-major）。"""
    m = np.stack([np.asarray(cols[c][row], dtype=float).astype("<f4")
                  for c in KF_COLS], axis=-1)
    b = np.ascontiguousarray(m, dtype="<f4").tobytes()
    if len(b) != 30 * 6 * 4:
        raise KFError("kf 行字节数 %d != 720" % len(b))
    return b


def recompute_row_bytes_isolated(mat_path: str, row_idx: int) -> bytes:
    """独立第二路径：重新 loadmat、仅取白名单 6 内容列该行、重新 cast 打包。"""
    m = loadmat(mat_path, simplify_cells=True)
    kf = m["kf"]
    cols = [np.asarray(kf[c], dtype=float)[row_idx].astype("<f4")
            for c in KF_COLS]
    return np.ascontiguousarray(np.stack(cols, axis=-1), dtype="<f4").tobytes()


def _fixed_ascii(text: str, size: int, field: str) -> bytes:
    b = text.encode("ascii")
    if len(b) > size:
        raise KFError("字段 %s 超长（%d > %d）: %r" % (field, len(b), size, text))
    return b + b"\x00" * (size - len(b))


def build_table_bytes(rows: list, header: dict) -> tuple:
    payload = b"".join(struct.pack("<d", r["issue_time_s"]) + r["bytes"]
                       for r in rows)
    payload_sha = hashlib.sha256(payload).digest()
    head = struct.pack("<8sIIIIdd", MAGIC, FORMAT_VERSION, CONTRACT_VERSION,
                       header["forecast_profile_code"], len(rows),
                       header["issue_first"], header["issue_step"])
    head += _fixed_ascii(header["voyage_id"], 32, "voyage_id")
    head += _fixed_ascii(header["forecast_generator_id"], 32,
                         "forecast_generator_id")
    head += _fixed_ascii(header["forecast_generator_version"], 16,
                         "forecast_generator_version")
    head += _fixed_ascii(header["forecast_generator_hash"], 64,
                         "forecast_generator_hash")
    head += payload_sha
    if len(head) != HEADER_BYTES:
        raise KFError("header 长度 %d != %d" % (len(head), HEADER_BYTES))
    return head + payload, payload_sha.hex()


def parse_table_bytes(blob: bytes) -> dict:
    if len(blob) < HEADER_BYTES:
        raise KFError("表文件过短: %d" % len(blob))
    magic, fv, cv, code, n = struct.unpack_from("<8sIIII", blob, 0)
    issue_first, issue_step = struct.unpack_from("<dd", blob, 24)
    voyage_id = blob[40:72].split(b"\x00")[0].decode("ascii")
    gen_id = blob[72:104].split(b"\x00")[0].decode("ascii")
    gen_ver = blob[104:120].split(b"\x00")[0].decode("ascii")
    gen_hash = blob[120:184].split(b"\x00")[0].decode("ascii")
    payload_sha = blob[184:216]
    payload = blob[HEADER_BYTES:]
    if len(payload) != n * ROW_BYTES:
        raise KFError("payload 长度 %d != n_rows(%d)×%d"
                      % (len(payload), n, ROW_BYTES))
    rows = []
    for i in range(n):
        off = i * ROW_BYTES
        (t,) = struct.unpack_from("<d", payload, off)
        rows.append({"issue_time_s": t,
                     "bytes": payload[off + 8:off + ROW_BYTES]})
    return {"magic": magic, "format_version": fv, "contract_version": cv,
            "forecast_profile_code": code, "n_rows": n,
            "issue_first": issue_first, "issue_step": issue_step,
            "voyage_id": voyage_id, "forecast_generator_id": gen_id,
            "forecast_generator_version": gen_ver,
            "forecast_generator_hash": gen_hash,
            "payload_sha256": payload_sha.hex(),
            "payload_sha256_actual": hashlib.sha256(payload).hexdigest(),
            "payload_bytes": len(payload), "rows": rows,
            "total_bytes": len(blob)}


# ----------------------------------------------------------------------- work
READBACK_BOOL_KEYS = ("magic_ok", "format_version_ok", "contract_version_ok",
                      "profile_code_ok", "n_rows_ok", "issue_first_bitwise",
                      "issue_step_bitwise", "voyage_id_ok", "generator_id_ok",
                      "generator_version_ok", "generator_hash_ok",
                      "payload_sha256_ok", "row_grid_bitwise")


def run(mode: str, mat_rel: str = SRC_MAT_REL, table_name: str = TABLE_NAME,
        expect_voyage_id: str | None = None,
        profile_code: int = 3) -> dict:
    audit = _static_whitelist_audit()  # 违者 FAIL，不出表
    mat_path = os.path.join(REPO, mat_rel)
    if not os.path.isfile(mat_path):
        raise KFError("源 .mat 不存在: %s" % rel(mat_path))
    d = read_kf_whitelist(mat_path)
    if (expect_voyage_id is not None
            and d["voyage_id"] != expect_voyage_id):
        raise KFError("meta.voyage_id=%r 与 --voyage-id=%r 不一致"
                      % (d["voyage_id"], expect_voyage_id))
    pf = preflight_checks(d)
    issue = d["issue_time_s"]
    prov = d["provenance"]
    issue_step = float(np.diff(issue)[0])
    hdr_len_ok = header_field_lengths_ok(d)
    if not hdr_len_ok:
        raise KFError("header 字段 ASCII 编码后超长")

    header = {"forecast_profile_code": profile_code,
              "issue_first": float(issue[0]), "issue_step": issue_step,
              "voyage_id": d["voyage_id"],
              "forecast_generator_id": prov["forecast_generator_id"],
              "forecast_generator_version": prov["forecast_generator_version"],
              "forecast_generator_hash": prov["forecast_generator_hash"]}
    rows = [{"issue_time_s": float(issue[i]),
             "bytes": row_bytes(d["cols"], i)} for i in range(issue.size)]
    blob, payload_sha = build_table_bytes(rows, header)

    table_path = os.path.join(REPO, OUT_DIR_REL, table_name)
    if mode == "pack":
        os.makedirs(os.path.dirname(table_path), exist_ok=True)
        with open(table_path, "wb") as fh:
            fh.write(blob)
    elif not os.path.isfile(table_path):
        raise KFError("selfcheck 缺盘上表: %s" % rel(table_path))
    with open(table_path, "rb") as fh:
        disk_blob = fh.read()
    disk_identical = disk_blob == blob
    if not disk_identical:
        raise KFError("%s：盘上表与重算逐字节不一致（disk=%d B vs recompute=%d B）"
                      % (mode, len(disk_blob), len(blob)))
    parsed = parse_table_bytes(disk_blob)

    readback = {
        "magic_ok": bool(parsed["magic"] == MAGIC),
        "format_version_ok": bool(parsed["format_version"] == FORMAT_VERSION),
        "contract_version_ok":
            bool(parsed["contract_version"] == CONTRACT_VERSION),
        "profile_code_ok":
            bool(parsed["forecast_profile_code"] == profile_code),
        "n_rows_ok": bool(parsed["n_rows"] == issue.size),
        "issue_first_bitwise":
            bool(struct.pack("<d", parsed["issue_first"])
                 == struct.pack("<d", float(issue[0]))),
        "issue_step_bitwise":
            bool(struct.pack("<d", parsed["issue_step"])
                 == struct.pack("<d", issue_step)),
        "voyage_id_ok": bool(parsed["voyage_id"] == d["voyage_id"]),
        "generator_id_ok":
            bool(parsed["forecast_generator_id"]
                 == prov["forecast_generator_id"]),
        "generator_version_ok":
            bool(parsed["forecast_generator_version"]
                 == prov["forecast_generator_version"]),
        "generator_hash_ok":
            bool(parsed["forecast_generator_hash"]
                 == prov["forecast_generator_hash"]),
        "payload_sha256_declared": parsed["payload_sha256"],
        "payload_sha256_actual": parsed["payload_sha256_actual"],
        "payload_sha256_ok":
            bool(parsed["payload_sha256"] == parsed["payload_sha256_actual"]),
        "row_grid_bitwise": bool(all(
            struct.pack("<d", parsed["rows"][i]["issue_time_s"])
            == struct.pack("<d", float(issue[i]))
            for i in range(issue.size))),
    }

    sample_checks = {}
    # 抽首尾两行独立重算（S20 时即 300/5700，与历史版本抽样点一致）
    for t0 in (float(issue[0]), float(issue[-1])):
        idx = int(round((t0 - float(issue[0])) / issue_step))
        rec = recompute_row_bytes_isolated(mat_path, idx)
        sample_checks["t0_%05d" % int(round(t0))] = {
            "row_index": idx,
            "issue_time_s": parsed["rows"][idx]["issue_time_s"],
            "n_bytes": len(rec),
            "match": bool(rec == parsed["rows"][idx]["bytes"]),
        }

    overall = (all(bool(v) for v in pf.values()) and hdr_len_ok
               and disk_identical and bool(audit["pass"])
               and all(bool(readback[k]) for k in READBACK_BOOL_KEYS)
               and all(v["match"] for v in sample_checks.values()))

    return {
        "schema": SELFCHECK_SCHEMA,
        "created_utc": utc_now(),
        "mode": mode,
        "driver": rel(os.path.abspath(__file__)),
        "source_mat": rel(mat_path),
        "voyage_id": d["voyage_id"],
        "fixture_kind": d["fixture_kind"],
        "provenance": prov,
        "format": {
            "magic": MAGIC.decode("ascii"),
            "format_version": FORMAT_VERSION,
            "contract_version": "0x%08x" % CONTRACT_VERSION,
            "forecast_profile_code": profile_code,
            "forecast_profile_code_meaning":
                {str(k): v for k, v in FORECAST_PROFILE_CODES.items()},
            "n_rows": int(issue.size),
            "issue_first": float(issue[0]),
            "issue_step": issue_step,
            "issue_last": float(issue[-1]),
            "header_bytes": HEADER_BYTES,
            "row_bytes": ROW_BYTES,
            "total_bytes": HEADER_BYTES + int(issue.size) * ROW_BYTES,
            "endian": "little",
            "columns": list(KF_COLS),
            "row_layout": "f64 issue_time_s + (30,6) <f4 C-order k-major",
        },
        "table": {
            "path": rel(table_path),
            "total_bytes": len(disk_blob),
            "sha256": sha256_bytes(disk_blob),
            "payload_bytes": len(disk_blob) - HEADER_BYTES,
            "payload_sha256_declared": payload_sha,
            "payload_sha256_recomputed": parsed["payload_sha256_actual"],
        },
        "checks": {
            "preflight": pf,
            "header_field_lengths_ok": bool(hdr_len_ok),
            "on_disk_byte_identical_to_recomputed": bool(disk_identical),
            "readback": readback,
            "sample_rows_isolated_recompute": sample_checks,
            "static_audit": audit,
        },
        "overall": bool(overall),
        "errors": [],
    }


# ----------------------------------------------------------------------- main
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="EMS04 S4-C3 pilot 航次 kf 表打包器（EMS04KFT v1, "
                    "profile_code 3=PILOT_FIXTURE；缺省 = S20 历史行为）")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--pack", action="store_true",
                   help="生成 results/s4_c3/<表名>")
    g.add_argument("--selfcheck", action="store_true",
                   help="重算并与盘上表逐字节比对（不写表）")
    ap.add_argument("--mat", default=None,
                    help="源 .mat 相对仓库根路径（泛化模式；须与 --voyage-id "
                         "同时给出；缺省 = S20_composite 历史路径）")
    ap.add_argument("--voyage-id", default=None,
                    help="航次 id（须与 mat 内 meta.voyage_id 一致）；产物命名 "
                         "<id>.kf_table.bin / s4_c3_kf_table_selfcheck_<id>"
                         ".json；缺省 = S20 既有文件名")
    ap.add_argument("--profile-code", type=int, choices=(1, 2, 3), default=3,
                    help="forecast_profile_code（S4-P3 参数化；默认 3=PILOT_"
                         "FIXTURE 与历史行为一致；formal 臂：1=GOOD_V1 / "
                         "2=DEGRADED_V1，须与 --mat/--voyage-id 泛化模式同用）")
    args = ap.parse_args(argv)
    if (args.mat is None) != (args.voyage_id is None):
        ap.error("--mat 与 --voyage-id 必须同时给出或同时缺省")
    if args.profile_code != 3 and args.mat is None:
        ap.error("--profile-code 非默认值 3 时须与 --mat/--voyage-id 泛化模式同用"
                 "（缺省 S20 路径冻结为 3=PILOT_FIXTURE）")
    if args.voyage_id is None:
        mat_rel, table_name = SRC_MAT_REL, TABLE_NAME
        json_name, expect_vid = SELFCHECK_NAME, None
    else:
        vid = args.voyage_id
        if (not vid or vid in (".", "..") or "/" in vid or "\\" in vid
                or os.sep in vid):
            ap.error("--voyage-id 含路径分隔符或为空，拒绝用于产物命名")
        if os.path.isabs(args.mat):
            ap.error("--mat 必须是相对仓库根路径")
        mat_rel = rel(os.path.normpath(os.path.join(REPO, args.mat)))
        table_name = "%s.kf_table.bin" % vid
        json_name = "s4_c3_kf_table_selfcheck_%s.json" % vid
        expect_vid = vid
    mode = "pack" if args.pack else ("selfcheck" if args.selfcheck else "pack")
    json_path = os.path.join(REPO, OUT_DIR_REL, json_name)
    try:
        payload = run(mode, mat_rel=mat_rel, table_name=table_name,
                      expect_voyage_id=expect_vid,
                      profile_code=args.profile_code)
    except (KFError, SystemExit) as e:
        fail = {"schema": SELFCHECK_SCHEMA, "created_utc": utc_now(),
                "mode": mode, "driver": rel(os.path.abspath(__file__)),
                "overall": False,
                "errors": [{"kind": type(e).__name__, "detail": str(e)}]}
        try:
            os.makedirs(os.path.dirname(json_path), exist_ok=True)
            with open(json_path, "w", encoding="utf-8") as fh:
                json.dump(fail, fh, indent=1, ensure_ascii=False)
        except OSError:
            pass
        print("FAIL: %s" % e, file=sys.stderr)
        return 1
    os.makedirs(os.path.dirname(json_path), exist_ok=True)
    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=1, ensure_ascii=False)
    print(json.dumps({"overall": payload["overall"], "mode": mode,
                      "n_rows": payload["format"]["n_rows"],
                      "total_bytes": payload["table"]["total_bytes"],
                      "table_sha256": payload["table"]["sha256"],
                      "payload_sha256": payload["table"]["payload_sha256_declared"]},
                     indent=1))
    return 0 if payload["overall"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

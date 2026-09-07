#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cd_interface_gate.py — C/D Interface Gate 十项独立机器键 v2（DGN-16，FR-496，T-016/D-15）.

十项检查各自独立成键（result schema "cd_interface_gate_result_v2"，overall = 十键全 pass）：
  c01_schema_compliance          ① producer schema 符合（域相关；FORMAL 另核 embedded self_hash）
  c02_s_plan_causal_present      ② s_plan 族列存在
  c03_kf_snapshot_form           ③ K/F snapshot 形式（(issue,lead) 双索引 + lead 网格 + 短航次一致性）
  c04_forecast_provenance_valid  ④ forecast provenance（stage 随域：pilot | formal）
  c05_oracle_physical_separation ⑤ K/F 与 ORACLE 物理分离（闭世界白名单 + 受限角色命名空间
                                   + "actual_future" 子串防御 + FORMAL 真值对应物双存断言）
  c06_metadata_complete          ⑥ metadata（域相关键集 + baseline 法律角色 NFR-420）
  c07_voyage_qualification       ⑦ 航次合格判据（三态：activation 预注册值 >
                                   FORMAL embedded eligibility > PENDING / NO_QUALIFICATION_EVIDENCE）
  c08_ood_config_surface         ⑧ OOD 配置面
  c09_contract_validators        ⑨ 契约 validator（registry + producer schema）
  c10_modelE_baseline_integrity  ⑩ 基线完整性快照消费（fail-closed：缺快照/坏 JSON 即 FAIL；
                                   hash 侧由 MATLAB 72 号完成，本模块只消费其结果文件）

域开关 --formality {FORMAL,NON-FORMAL}（缺省 NON-FORMAL）：FORMAL 为正式 EXT-CD-GATE
（对象 = C Mission-v3 语料，⑤(iv) 载体双存 + ⑥ fixture/producer_script_sha + ⑦ eligibility
判定生效）；NON-FORMAL 沿用 D0 骨架语义（pilot fixtures，不构成 T-432 解锁，FR-498）。

用法（MODELE_ROOT 下）：
  python -m dataset_v3.cd_interface_gate --voyage-dir datasets_v3/pilot
  python -m dataset_v3.cd_interface_gate --voyage-dir datasets_v3/mission_v3/prequal \
      --formality FORMAL --result results/dataset_v3/cd_interface_formal_prequal.json
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import sys

import numpy as np

from .contracts import load_producer_schema, load_registry
from .run_pipeline import load_voyage
from .validate_contracts import (MODELE_ROOT, validate_forecast_provenance,
                                 validate_producer_schema, validate_registry)

RESULT_SCHEMA = "cd_interface_gate_result_v2"
FORMAL = "FORMAL"
NON_FORMAL = "NON-FORMAL"
FORMALITY_CHOICES = (FORMAL, NON_FORMAL)
FORMALITY_NOTE = {
    FORMAL: "正式 C/D Interface Gate（EXT-CD-GATE）",
    NON_FORMAL: "NON-FORMAL（D0 骨架自检：对象为 PILOT_ONLY fixtures，不构成 T-432 解锁）",
}
# 逐航次检查键（c07/c09/c10 为全局项；键序即 result 展示序）
PER_VOYAGE_KEYS = ("c01", "c02", "c03", "c04", "c05", "c06", "c08")
PER_VOYAGE_CHECK_NAMES = (
    "c01_schema_compliance", "c02_s_plan_causal_present", "c03_kf_snapshot_form",
    "c04_forecast_provenance_valid", "c05_oracle_physical_separation",
    "c06_metadata_complete", "c08_ood_config_surface")
# ⑩ 基线完整性快照（MATLAB 72 号产出）锚 MODELE_ROOT，禁 voyage-dir 推导（fail-closed，D-15）
BASELINE_SNAPSHOT_RELPATH = os.path.join("results", "dataset_v3", "cd_interface",
                                         "baseline_integrity.json")
# ⑤(i) C Mission-v3 导出件实测运营列（c_producer_interface_contract 侧定义，暂未收编进
# producer schema 能力族）——闭世界白名单显式枚举豁免；白名单外列仍 FAIL
MISSION_V3_KF_OPERATIONAL_COLS = frozenset({
    "in_speed_limit_zone", "in_bridge_zone", "missing_mask",
    "contract_status", "provenance"})


def _num(x) -> float:
    return float(np.asarray(x).ravel()[0])


def check_voyage(vid: str, v: dict, schema: dict, reg: dict, formality: str) -> dict:
    """对单航次执行 c01~c06、c08（c07/c09/c10 为全局项）。

    返回 {key: [违规…]}，key ∈ PER_VOYAGE_KEYS；空列表 = 该项 PASS。
    vid 为航次标识（与 per_voyage 键一致，供日志/消息定位）。
    """
    out: dict[str, list] = {k: [] for k in PER_VOYAGE_KEYS}
    st, kf, meta = v["stream"], v["kf"], v["meta"]
    fam = schema["capability_families"]
    formal = formality == FORMAL

    # ---- c01 producer schema 符合 ----
    req_cols = [f["name"] for f in fam["truth_stream"]["fields"] if f["required"]]
    if formal:
        missing = [c for c in req_cols if c not in st]
        if missing:
            out["c01"].append(f"c01 缺 truth_stream 必填列: {missing}")
        # embedded self_hash 核对（FORMAL：C 侧证据与 D 侧契约事实源逐字节一致；
        # C 导出件将 hash 记于 eligibility.windows.provenance 下，windows 顶层为兼容位）
        sch_h = (schema.get("self_hash") or {}).get("value")
        reg_h = (reg.get("self_hash") or {}).get("value")
        el = meta.get("eligibility")
        win = el.get("windows") if isinstance(el, dict) else None
        win_prov = win.get("provenance") if isinstance(win, dict) else None

        def _emb(key: str):
            for d in (win, win_prov):
                if isinstance(d, dict) and d.get(key):
                    return str(d[key])
            return None

        if sch_h and _emb("schema_self_hash") != sch_h:
            out["c01"].append(
                f"c01 embedded schema_self_hash 与 D 侧不一致: "
                f"embedded={_emb('schema_self_hash')!r} D={str(sch_h)[:16]}…")
        if reg_h and _emb("registry_self_hash") != reg_h:
            out["c01"].append(
                f"c01 embedded registry_self_hash 与 D 侧不一致: "
                f"embedded={_emb('registry_self_hash')!r} D={str(reg_h)[:16]}…")
    else:
        # NON-FORMAL（现状）：time_s 除外（其已在 stream 索引侧被消费）
        missing = [c for c in req_cols if c not in st and c != "time_s"]
        if missing:
            out["c01"].append(f"c01 缺 truth_stream 列: {missing}")
        h12 = [s["name"] for s in reg["signals"] if s["role"] in ("INPUT_H1", "INPUT_H2")]
        miss2 = [c for c in h12 if c not in st]
        if miss2:
            out["c01"].append(f"c01 缺 registry H1/H2 列: {miss2}")

    # ---- c02 s_plan 族列存在（两域同）----
    for c in ("s_plan_m", "v_plan_mps"):
        if c not in kf:
            out["c02"].append(f"c02 缺 s_plan 族列 {c}")

    # ---- c03 K/F snapshot 形式（(issue, lead) 双索引，lead ∈ {10..300}）；
    #      <600 s 航次合法产生零窗口/零快照（FR-472）——空表须与短航次一致否则违规 ----
    if "issue_time_s" not in kf or "lead_s" not in kf:
        out["c03"].append("c03 kf 缺 issue_time_s/lead_s")
    else:
        issue = np.asarray(kf["issue_time_s"], dtype=float).ravel()
        lead = np.asarray(kf["lead_s"], dtype=float)
        dur = float(np.asarray(st["time_s"], dtype=float).ravel()[-1]) if "time_s" in st else float("inf")
        if issue.size == 0:
            if dur >= 600.0:
                out["c03"].append(f"c03 issue_time_s 为空且航次时长 {dur:.0f}s ≥ 600（缺 K/F 快照）")
        else:
            if lead.ndim != 2 or lead.shape[-1] != 30:
                out["c03"].append(f"c03 lead_s 形状非法: {lead.shape}（须 (M,30) issue-major）")
            elif not np.allclose(np.unique(lead), np.arange(10, 301, 10)):
                out["c03"].append("c03 lead 网格须为 {10,…,300}")

    # ---- c04 forecast provenance（六字段 rec；stage 随域：pilot | formal）----
    prov = meta.get("forecast_provenance", {})
    try:
        rec = {"forecast_generator_id": str(prov.get("forecast_generator_id", "")),
               "forecast_generator_version": str(prov.get("forecast_generator_version", "")),
               "forecast_generator_hash": str(prov.get("forecast_generator_hash", "")),
               "issue_time_s": _num(prov.get("issue_time_s", np.nan)) if prov else float("nan"),
               "source_cutoff_time_s": _num(prov.get("source_cutoff_time_s", np.nan)) if prov else float("nan"),
               "forecast_provenance_class": str(prov.get("forecast_provenance_class", ""))}
        perrs = validate_forecast_provenance(rec, stage="formal" if formal else "pilot")
        if perrs:
            out["c04"].append(f"c04 provenance 违规: {perrs}")
    except (TypeError, ValueError, AttributeError) as e:
        out["c04"].append(f"c04 provenance 不可解析: {e}")

    # ---- c05 K/F 与 ORACLE 物理分离（D-15 收紧）----
    # (i) 闭世界白名单：kf 列只允许 schema K/F 快照字段 + 运营列豁免集
    kf_cols = set(kf.keys())
    wl = ({"issue_time_s", "lead_s"}
          | {f["name"] for f in fam["input_k_snapshots"]["fields"]}
          | {f["name"] for f in fam["input_f_snapshots"]["fields"]}
          | set(MISSION_V3_KF_OPERATIONAL_COLS))
    for c in sorted(kf_cols - wl):
        out["c05"].append(f"c05 kf 列越闭世界白名单: {c}")
    # (ii) 受限角色命名空间隔离：ORACLE/TARGET 或 predictor_access forbidden/oracle_only
    #      的信号名禁入 kf 表
    restricted = {s["name"]: f"role={s['role']},access={s.get('predictor_access')}"
                  for s in reg["signals"]
                  if s["role"] in ("ORACLE", "TARGET")
                  or s.get("predictor_access") in ("forbidden", "oracle_only")}
    for c in sorted(kf_cols & restricted.keys()):
        out["c05"].append(f"c05 kf 混入受限角色列（{restricted[c]}）: {c}")
    # (iii) 原子子串防御（独立于 registry 漂移）
    for c in sorted(kf_cols):
        if "actual_future" in c:
            out["c05"].append(f"c05 kf 列名含 oracle 子串 'actual_future': {c}")
    # (iv) FORMAL 附加：物理分离双存断言——INPUT_F 快照在 kf，其 ORACLE 真值对应物
    #      必须在 stream 显式存在（v_current_mps / water_depth_m）
    if formal:
        carriers = [f["name"] for f in fam["truth_stream"]["fields"]
                    if "ORACLE 真值对应物" in f.get("causality_rule", "")]
        miss_carrier = [c for c in carriers if c not in st]
        if miss_carrier:
            out["c05"].append(f"c05 FORMAL oracle 真值对应物未在 stream 双存: {miss_carrier}")

    # ---- c06 metadata 完整（域相关键集）----
    if formal:
        need = ([f["name"] for f in fam["voyage_metadata"]["fields"]]
                + ["generation_seed", "model_baseline_ref",
                   "producer_script_sha", "fixture_kind"])
        miss3 = [k for k in need if k not in meta or str(meta[k]) == ""]
        if miss3:
            out["c06"].append(f"c06 缺 metadata: {miss3}")
        if str(meta.get("fixture_kind", "")) == "PILOT_ONLY":
            out["c06"].append("c06 FORMAL 禁 PILOT_ONLY fixture（FR-500）")
    else:
        need_meta = ("voyage_id", "voyage_family_id", "route_template_id",
                     "mission_template_id", "split_group_id", "scenario_stratum",
                     "generation_seed", "model_baseline_ref", "producer_script_sha256")
        miss3 = [k for k in need_meta if k not in meta or str(meta[k]) == ""]
        if miss3:
            out["c06"].append(f"c06 缺 metadata: {miss3}")
    # baseline 法律角色（NFR-420）：formal 生成基线 = 物理 v1.1 + 控制 v1.0（两域同）
    ref = str(meta.get("model_baseline_ref", ""))
    if "v1.1" not in ref or "v1.0" not in ref or "A2 historical" in ref:
        out["c06"].append(f"c06 baseline 引用非法（须 物理 v1.1 + 控制 v1.0，A2 historical 不得混同）: {ref!r}")

    # ---- c08 OOD 配置面 ----
    need_ood = ("ood_axis", "ood_def_version", "physics_domain_ref")
    miss4 = [k for k in need_ood if k not in meta or str(meta[k]) == ""]
    if miss4:
        out["c08"].append(f"c08 缺 OOD 配置面: {miss4}")

    return out


def _check_c07(durations: dict, issue_rows: dict, metas: dict,
               formality: str, activation: dict | None) -> dict:
    """⑦ 航次合格判据（三态，DGN-16/D-15）."""
    if activation is not None and "formal_voyage_qualification" in activation:
        fq = activation["formal_voyage_qualification"] or {}
        md, mw = fq.get("min_duration_s"), fq.get("min_canonical_windows")
        if md is None or mw is None:
            return {"pass": False, "verdict": "INVALID_ACTIVATION_RECORD",
                    "note": "formal_voyage_qualification 缺 min_duration_s/min_canonical_windows"}
        md, mw = float(md), int(mw)
        below = {vid: d for vid, d in durations.items() if d < md}
        few = {vid: n for vid, n in issue_rows.items() if n < mw}
        return {"pass": not below and not few, "min_duration_s": md,
                "min_canonical_windows": mw, "below_min": below, "few_issue_rows": few}
    if formality == FORMAL:
        no_evidence: list = []
        viol: dict = {}
        for vid, meta in metas.items():
            el = meta.get("eligibility")
            if not isinstance(el, dict):
                no_evidence.append(vid)
                continue
            cv = el.get("criteria_values")
            win = el.get("windows") if isinstance(el.get("windows"), dict) else {}
            fails = []
            if el.get("disposition") != "ACCEPT":
                fails.append(f"disposition={el.get('disposition')!r}≠ACCEPT")
            if not isinstance(cv, dict):
                fails.append("criteria_values 缺失")
            elif "min_duration_s" not in cv or "min_canonical_windows" not in cv:
                fails.append("criteria_values 缺 min_duration_s/min_canonical_windows")
            else:
                md, mw = float(cv["min_duration_s"]), int(cv["min_canonical_windows"])
                d = durations.get(vid)
                if d is not None and d < md:
                    fails.append(f"duration {d:.0f}s < min_duration_s {md:.0f}s")
                if int(win.get("valid_canonical_windows", -1)) < mw:
                    fails.append(f"valid_canonical_windows "
                                 f"{win.get('valid_canonical_windows')} < min_canonical_windows {mw}")
            if fails:
                viol[vid] = fails[:3]
        verdict = ("NO_QUALIFICATION_EVIDENCE" if no_evidence
                   else "VERIFIED_AGAINST_EMBEDDED_ELIGIBILITY")
        return {"pass": not no_evidence and not viol, "verdict": verdict,
                "no_eligibility": no_evidence, "violations": viol}
    return {"pass": True, "verdict": "PENDING_ACTIVATION_RECORD",
            "note": "预注册值未提供（600s 仅为几何最小跨度，FR-495）；NON-FORMAL 不判定"}


def _check_baseline_integrity(path: str) -> dict:
    """⑩ 基线完整性快照消费（fail-closed，两域统一）。

    - 缺文件 → FAIL MISSING_BASELINE_SNAPSHOT（先跑 scripts/72_run_cd_interface_gate.m）；
    - 坏 JSON / 结构非法 → FAIL；
    - 含 pass 键 → 消费其值（verdict 沿用快照）；
    - 否则按 72 号实际结构：files ≥ 5 且逐文件 sha256 非空 → PASS SNAPSHOT_CONSUMED。
    """
    snap_ref = os.path.relpath(path, MODELE_ROOT)
    if not os.path.isfile(path):
        return {"pass": False, "verdict": "MISSING_BASELINE_SNAPSHOT",
                "snapshot": snap_ref,
                "note": "缺基线 hash 快照；先运行 scripts/72_run_cd_interface_gate.m 生成 "
                        "results/dataset_v3/cd_interface/baseline_integrity.json（⑩ fail-closed，D-15）"}
    try:
        with open(path, encoding="utf-8") as f:
            snap = json.load(f)
    except (json.JSONDecodeError, UnicodeDecodeError, OSError) as e:
        return {"pass": False, "verdict": "BASELINE_SNAPSHOT_UNREADABLE",
                "snapshot": snap_ref, "error": str(e)}
    if not isinstance(snap, dict):
        return {"pass": False, "verdict": "INVALID_BASELINE_SNAPSHOT",
                "snapshot": snap_ref, "error": f"顶层须为对象，实际 {type(snap).__name__}"}
    if "pass" in snap:
        return {"pass": bool(snap.get("pass")), "verdict": str(snap.get("verdict", "SNAPSHOT_CONSUMED")),
                "snapshot": snap_ref, "snapshot_detail": snap}
    files = snap.get("files")
    ok = (isinstance(files, list) and len(files) >= 5
          and all(isinstance(f, dict) and str(f.get("sha256", "")) != "" for f in files))
    if ok:
        return {"pass": True, "verdict": "SNAPSHOT_CONSUMED",
                "snapshot": snap_ref, "snapshot_detail": snap}
    return {"pass": False, "verdict": "INVALID_BASELINE_SNAPSHOT",
            "snapshot": snap_ref,
            "note": "快照无 pass 键且 files<5 或存在空 sha256（须由 72 号重写）"}


def run_gate(voyage_dir: str, formality: str = NON_FORMAL,
             activation_path: str | None = None) -> dict:
    """执行十项检查，返回 cd_interface_gate_result_v2 结构（不落盘）。"""
    # ⑨ 机器契约 validator（契约加载失败此处抛出——fail-fast，现状语义）
    reg = load_registry(validate=True)
    schema = load_producer_schema(validate=True)
    schema_errs = validate_producer_schema(schema)
    reg_errs = validate_registry(reg)

    files = sorted(f for f in os.listdir(voyage_dir) if f.endswith(".mat"))
    if not files:
        raise SystemExit(f"voyage 目录无 .mat: {voyage_dir}")

    activation = None
    if activation_path and os.path.isfile(activation_path):
        with open(activation_path, encoding="utf-8") as f:
            activation = json.load(f)

    per: dict[str, dict] = {k: {} for k in PER_VOYAGE_KEYS}
    durations, issue_rows, metas = {}, {}, {}
    for fn in files:
        vid = fn[:-4]
        v = load_voyage(os.path.join(voyage_dir, fn))
        res = check_voyage(vid, v, schema, reg, formality)
        for k in PER_VOYAGE_KEYS:
            if res.get(k):
                per[k][vid] = res[k]
        st = v["stream"]
        if "time_s" in st:
            durations[vid] = float(np.asarray(st["time_s"]).ravel()[-1])
        issue_rows[vid] = (int(np.asarray(v["kf"]["issue_time_s"], dtype=float).ravel().size)
                           if "issue_time_s" in v["kf"] else 0)
        metas[vid] = v["meta"]

    checks: dict = {"c09_contract_validators": {
        "pass": not schema_errs and not reg_errs,
        "registry_errors": reg_errs[:3], "producer_schema_errors": schema_errs[:3]}}
    for name in PER_VOYAGE_CHECK_NAMES:
        errs_by_vid = per[name[:3]]
        checks[name] = {"pass": not errs_by_vid,
                        "per_voyage": {vid: errs[:3] for vid, errs in errs_by_vid.items()}}
    checks["c07_voyage_qualification"] = _check_c07(durations, issue_rows, metas,
                                                    formality, activation)
    checks["c10_modelE_baseline_integrity"] = _check_baseline_integrity(
        os.path.join(MODELE_ROOT, BASELINE_SNAPSHOT_RELPATH))

    overall = all(v.get("pass", False) for v in checks.values())
    return {"schema": RESULT_SCHEMA, "formality": formality,
            "formality_note": FORMALITY_NOTE[formality],
            "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
            "n_voyages": len(files), "activation_provided": activation is not None,
            "overall": overall, "checks": checks}


def main(argv=None) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser(description="C/D Interface Gate（DGN-16；FORMAL=EXT-CD-GATE）")
    ap.add_argument("--voyage-dir", default=os.path.join("datasets_v3", "pilot"))
    ap.add_argument("--formality", choices=FORMALITY_CHOICES, default=NON_FORMAL,
                    help="FORMAL=正式 EXT-CD-GATE；NON-FORMAL=D0 骨架自检（缺省）")
    ap.add_argument("--activation", default=None,
                    help="t432 activation record JSON（提供后 ⑦ 才核对预注册值）")
    ap.add_argument("--result", default=os.path.join("results", "dataset_v3",
                                                     "cd_interface_gate_result.json"))
    args = ap.parse_args(argv)
    vd = args.voyage_dir if os.path.isabs(args.voyage_dir) else os.path.join(root, args.voyage_dir)
    rs = args.result if os.path.isabs(args.result) else os.path.join(root, args.result)
    res = run_gate(vd, formality=args.formality, activation_path=args.activation)
    os.makedirs(os.path.dirname(rs), exist_ok=True)
    with open(rs, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=2)
    print(f"[cd-gate] formality={args.formality}")
    if args.formality == FORMAL:
        print(f"[cd-gate] overall={res['overall']}（正式 C/D Gate EXT-CD-GATE，n_voyages={res['n_voyages']}）")
    else:
        print(f"[cd-gate] overall={res['overall']}（NON-FORMAL 骨架自检，不构成 T-432 解锁）")
    return 0 if res["overall"] else 1


if __name__ == "__main__":
    sys.exit(main())

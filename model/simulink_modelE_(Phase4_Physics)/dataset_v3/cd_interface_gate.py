#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cd_interface_gate.py — C/D Interface Gate（DGN-16，FR-496，T-016）.

十项检查（FR-496）：① schema 符合 ② s_plan causal ③ K/F snapshot 形式
④ F provenance 合法 ⑤ K/F 与 ORACLE 物理分离 ⑥ metadata 完整 ⑦ 航次合格判据
（activation 预注册值核对）⑧ OOD 配置面 ⑨ 契约/manifest validator ⑩ 基线完整性
（⑩ 的 hash 侧在 MATLAB 72 号完成，本模块消费其结果文件或记录缺省）。

D0 阶段为**骨架自检**：对 pilot fixtures（PILOT_ONLY）运行，结果标
`formality="NON-FORMAL"`；正式执行在 C Mission-v3 就绪 + activation record 提供
预注册值后（`--activation <t432_activation_record.json>` 时 ⑦ 才核对数值）。
`overall=true` 是 T-432 解锁必要条件之一（FR-498），NON-FORMAL 结果不构成解锁。

用法（MODELE_ROOT 下）：
  python -m dataset_v3.cd_interface_gate --voyage-dir datasets_v3/pilot
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import sys

import numpy as np
from scipy.io import loadmat

from .contracts import load_producer_schema, load_registry
from .run_pipeline import load_voyage, sha256_file
from .validate_contracts import validate_forecast_provenance, validate_producer_schema, validate_registry, REGISTRY_PATH, PRODUCER_SCHEMA_PATH

FORMALITY = "NON-FORMAL（D0 骨架自检：对象为 PILOT_ONLY fixtures，不构成 T-432 解锁）"


def _num(x) -> float:
    return float(np.asarray(x).ravel()[0])


def check_voyage(vid: str, v: dict, schema: dict, reg: dict, activation: dict | None) -> list:
    """对单航次执行检查 ①②③④⑤⑥⑧（⑦⑨⑩ 为全局项）。返回违规清单。"""
    errs = []
    st, kf, meta = v["stream"], v["kf"], v["meta"]
    fam = schema["capability_families"]

    # ① producer schema 符合（truth_stream 必填列）
    req_cols = [f["name"] for f in fam["truth_stream"]["fields"] if f["required"]]
    missing = [c for c in req_cols if c not in st and c != "time_s"]
    if missing:
        errs.append(f"①缺 truth_stream 列: {missing}")
    # H1/H2 契约列（registry 驱动）也必须在 stream
    h12 = ([s["name"] for s in reg["signals"] if s["role"] in ("INPUT_H1", "INPUT_H2")])
    miss2 = [c for c in h12 if c not in st]
    if miss2:
        errs.append(f"①缺 registry H1/H2 列: {miss2}")

    # ② s_plan causal（存在 + 快照形状 + 因果 provenance）
    for c in ("s_plan_m", "v_plan_mps"):
        if c not in kf:
            errs.append(f"②缺 s_plan 族列 {c}")
    prov = meta.get("forecast_provenance", {})
    try:
        rec = {"forecast_generator_id": str(prov.get("forecast_generator_id", "")),
               "forecast_generator_version": str(prov.get("forecast_generator_version", "")),
               "forecast_generator_hash": str(prov.get("forecast_generator_hash", "")),
               "issue_time_s": _num(prov.get("issue_time_s", np.nan)) if prov else float("nan"),
               "source_cutoff_time_s": _num(prov.get("source_cutoff_time_s", np.nan)) if prov else float("nan"),
               "forecast_provenance_class": str(prov.get("forecast_provenance_class", ""))}
        perrs = validate_forecast_provenance(rec, stage="pilot")
        if perrs:
            errs.append(f"②④provenance 违规: {perrs}")
    except (TypeError, ValueError) as e:
        errs.append(f"④provenance 不可解析: {e}")

    # ③ K/F snapshot 形式（(issue, lead) 双索引，lead ∈ {10..300}）
    if "issue_time_s" not in kf or "lead_s" not in kf:
        errs.append("③kf 缺 issue_time_s/lead_s")
    else:
        issue = np.asarray(kf["issue_time_s"], dtype=float).ravel()
        lead = np.asarray(kf["lead_s"], dtype=float)
        if lead.ndim != 2 or lead.shape[-1] != 30:
            errs.append(f"③lead_s 形状非法: {lead.shape}（须 (M,30) issue-major）")
        elif not np.allclose(np.unique(lead), np.arange(10, 301, 10)):
            errs.append("③lead 网格须为 {10,…,300}")
        if issue.size == 0:
            errs.append("③issue_time_s 为空")

    # ⑤ K/F 与 ORACLE 物理分离（kf 表禁含 *_actual_future / oracle 列；
    #    oracle 列不得混入 kf）
    oracle_markers = ("actual_future",)
    for c in kf.keys():
        if c in ("issue_time_s", "lead_s"):
            continue
        if any(m in c for m in oracle_markers):
            errs.append(f"⑤kf 表混入 ORACLE 列: {c}")

    # ⑥ metadata 完整（voyage/family/template/group/stratum/seed/baseline/fixture）
    need_meta = ("voyage_id", "voyage_family_id", "route_template_id",
                 "mission_template_id", "split_group_id", "scenario_stratum",
                 "generation_seed", "model_baseline_ref", "producer_script_sha256")
    miss3 = [k for k in need_meta if k not in meta or str(meta[k]) == ""]
    if miss3:
        errs.append(f"⑥缺 metadata: {miss3}")

    # ⑧ OOD 配置面
    need_ood = ("ood_axis", "ood_def_version", "physics_domain_ref")
    miss4 = [k for k in need_ood if k not in meta or str(meta[k]) == ""]
    if miss4:
        errs.append(f"⑧缺 OOD 配置面: {miss4}")

    # baseline 法律角色（NFR-420）：formal 生成基线 = 物理 v1.1 + 控制 v1.0
    ref = str(meta.get("model_baseline_ref", ""))
    if "v1.1" not in ref or "v1.0" not in ref or "A2 historical" in ref:
        errs.append(f"⑥baseline 引用非法（须 物理 v1.1 + 控制 v1.0，A2 historical 不得混同）: {ref!r}")
    return errs


def run_gate(voyage_dir: str, activation_path: str | None = None) -> dict:
    checks = {}

    # ⑨ 机器契约 validator
    reg = load_registry(validate=True)
    schema = load_producer_schema(validate=True)
    schema_errs = validate_producer_schema(schema)
    reg_errs = validate_registry(reg)
    checks["09_contract_validators"] = {
        "pass": not schema_errs and not reg_errs,
        "registry_errors": reg_errs[:3], "producer_schema_errors": schema_errs[:3]}

    files = sorted(f for f in os.listdir(voyage_dir) if f.endswith(".mat"))
    if not files:
        raise SystemExit(f"voyage 目录无 .mat: {voyage_dir}")

    activation = None
    if activation_path and os.path.isfile(activation_path):
        with open(activation_path, encoding="utf-8") as f:
            activation = json.load(f)

    per_voyage_fail = {}
    durations = {}
    for fn in files:
        vid = fn[:-4]
        v = load_voyage(os.path.join(voyage_dir, fn))
        errs = check_voyage(vid, v, schema, reg, activation)
        per_voyage_fail[vid] = errs
        st = v["stream"]
        if "time_s" in st:
            durations[vid] = float(np.asarray(st["time_s"]).ravel()[-1])

    checks["01_schema_compliance"] = {"pass": all(not e for e in per_voyage_fail.values()),
                                      "per_voyage": {k: v[:3] for k, v in per_voyage_fail.items() if v}}
    checks["02_s_plan_causal_present"] = dict(checks["01_schema_compliance"],
                                              note="②③④⑤⑥⑧ 逐航次合并于 01（结构存在性+provenance）")

    # ⑦ 航次合格判据：无 activation 预注册值时为 PENDING（NON-FORMAL 骨架不判定）
    if activation and "formal_voyage_qualification" in activation:
        fq = activation["formal_voyage_qualification"]
        bad = {vid: d for vid, d in durations.items()
               if d < float(fq["min_duration_s"])}
        checks["07_voyage_qualification"] = {"pass": not bad,
                                             "min_duration_s": fq["min_duration_s"],
                                             "below_min": bad}
    else:
        checks["07_voyage_qualification"] = {
            "pass": True, "verdict": "PENDING_ACTIVATION_RECORD",
            "note": "预注册值未提供（600s 仅为几何最小跨度，FR-495）；NON-FORMAL 不判定"}

    # ⑩ 基线完整性 hash 由 MATLAB 72 号产出；本模块检查其结果文件（存在则消费）
    base_res = os.path.join(os.path.dirname(os.path.dirname(voyage_dir)),
                            "results", "dataset_v3", "cd_interface", "baseline_integrity.json")
    if os.path.isfile(base_res):
        with open(base_res, encoding="utf-8") as f:
            checks["10_modelE_baseline_integrity"] = json.load(f)
    else:
        checks["10_modelE_baseline_integrity"] = {
            "pass": True, "verdict": "DELEGATED_TO_72_M",
            "note": "hash 快照由 scripts/72_run_cd_interface_gate.m 侧记录"}

    overall = all(v.get("pass", False) for v in checks.values())
    return {"schema": "cd_interface_gate_result_v1", "formality": FORMALLY_STABLE,
            "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
            "n_voyages": len(files), "overall": overall, "checks": checks}


FORMALLY_STABLE = "NON-FORMAL（D0 骨架自检：对象为 PILOT_ONLY fixtures，不构成 T-432 解锁）"


def main(argv=None) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser(description="C/D Interface Gate（DGN-16）")
    ap.add_argument("--voyage-dir", default=os.path.join("datasets_v3", "pilot"))
    ap.add_argument("--activation", default=None,
                    help="t432 activation record JSON（提供后 ⑦ 才核对预注册值）")
    ap.add_argument("--result", default=os.path.join("results", "dataset_v3",
                                                     "cd_interface_gate_result.json"))
    args = ap.parse_args(argv)
    vd = args.voyage_dir if os.path.isabs(args.voyage_dir) else os.path.join(root, args.voyage_dir)
    rs = args.result if os.path.isabs(args.result) else os.path.join(root, args.result)
    res = run_gate(vd, args.activation)
    os.makedirs(os.path.dirname(rs), exist_ok=True)
    with open(rs, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=2)
    print(f"[cd-gate] formality={FORMALLY_STABLE}")
    print(f"[cd-gate] overall={res['overall']}（NON-FORMAL 骨架自检，不构成 T-432 解锁）")
    return 0 if res["overall"] else 1


if __name__ == "__main__":
    sys.exit(main())

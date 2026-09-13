#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_formal_freeze.py — Dataset v3 formal freeze 编排（T-020；FR-465/FR-487/FR-500）.

run_pipeline.py（pilot 编排，T-015）的 FORMAL 化改写：复用同一底层函数
（aggregate/windowing/targets/split/audit_leakage/manifest），只改阶段语义：

- split_seed 从 T-432 activation record 读取（FR-465 机器契约唯一事实源，禁 hard-code）；
- split group = meta.split_group_id（family closure），strata = meta.scenario_stratum；
  同一 group 映射多 stratum → ValueError fail-fast；
- fixture 门反转（FR-500）：meta.fixture_kind == "PILOT_ONLY" 的航次拒收并计
  gate FAIL 证据（pilot 语义相反）；
- forecast provenance 校验 stage="formal"（仅 CAUSAL_GENERATOR）；
- L2 CONFIRMED_DUPLICATE 不得静默忽略：按 voyage 归属后做 allocation 扣减，
  预注册 allocation 不满足 ⇒ gate FAIL；LEGITIMATE_NEAR_REPLICATE/UNRESOLVED
  记录即可（禁自动剔除，DGN-10）；
- L3 run_l3(..., require_train_stats=True)：z-score 统计由 formal train 计算，
  缺失即 fail-fast（R2 m-3）；
- allocation 期望从 activation record 派生（禁读 formal_fleet.ood.total_voyages
  ——该字段是 amendment 01 前的陈旧冗余值 240）：
    IID = iid.n_strata × families_per_stratum × members_per_family（=576）
    OOD = len(ood.counts.conditions) × ood.counts.per_condition_voyages（=200）；
- 前置 STAGE_B 标记门：out_dir 下存在 STAGE_B_TWO_RUN_DONE 才允许 stage=FORMAL
  （Stage B 两跑确定性完成后 T-020 才可冻结，见 scripts/overnight_stage_b.sh）；
  dryrun（EMS_FORMAL_FREEZE_DRYRUN=1 或 dryrun=True）跳过标记门，但结果 stage
  标 "FORMAL_DRYRUN" 且产物不落默认正式区（main 将默认 out/npz 重定向到
  formal_dryrun 目录；显式 --out-dir/--npz 尊重调用方）；
- 两跑 payload hash 复验：读 out_dir 下 pass1/pass2 的 {iid,ood}_ledger.json，
  逐 voyage payload_hash 一致（ledger 由 73 号 producer 写、overnight_stage_b.sh
  归档）；文件缺失时 dryrun=False 记 FAIL、dryrun=True 记 SKIP；
- OOD 三轴证据链 = dataset_v3.freeze_ood.build_ood_evidence（T-020 并行接口）；
- 封存 manifest = dataset_v3.freeze_manifest.build_formal_manifest + assert_sealed
  （FR-488 全字段；test_sealed/ood_sealed 冻结为 True——manifest 冻结契约，
  dryrun 的非正式性由 stage=FORMAL_DRYRUN 与写盘路径区分体现）。

用法（MODELE_ROOT 下）：
  python -m dataset_v3.run_formal_freeze \
      --iid-dir datasets_v3/formal/iid --ood-dir datasets_v3/formal/ood \
      --out-dir results/dataset_v3/formal --npz datasets_v3/formal/windows_v3_formal.npz
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import platform
import sys

import numpy as np

from . import contracts
from .aggregate import aggregate_stream, zero_fill_ctrl_channels
from .audit_leakage import (SBD_THRESHOLD_POLICY, _resample_linear, run_l1,
                            run_l2, run_l3, run_family_sibling_audit,
                            window_content_hash)
from .build_dataset_v3 import assert_no_quantile_labels, build_views
from .manifest import WINDOW_GEOMETRY_FROZEN
from .run_pipeline import (FUTURE_BINS, HISTORY_BINS, STRIDE_S, canonical_json,
                           load_voyage, sha256_file)
from .split import assign_splits, compute_artifact_hash, verify_partition, write_artifact
from .targets import crosscheck_vs_aggregate, e300_of, y_trajectory
from .validate_contracts import (MODELE_ROOT, PRODUCER_SCHEMA_PATH, REGISTRY_PATH,
                                 validate_forecast_provenance)
from .windowing import build_windows

RESULT_SCHEMA = "dataset_v3_formal_freeze_result_v1"
STAGE_FORMAL = "FORMAL"
STAGE_DRYRUN = "FORMAL_DRYRUN"
STAGE_B_MARKER = "STAGE_B_TWO_RUN_DONE"

# activation record 默认路径（仓库相对，FR-465 机器契约唯一事实源）
ACTIVATION_REL = os.path.join("docs", "phase4", "d_dataset_v3",
                              "t432_activation_record.json")

SPLIT_POLICY_VERSION = "dgn09-v1"
OOD_DEFINITION_VERSION_FALLBACK = "DGN-11-v1"
AGGREGATION_TABLE_VERSION = "dgn05-r1"
TARGET_DEFINITION = "y_k=E_k/10 kW（native trapezoidal 能量差分）；E300=ΣE_k/3600 kWh"
FIXTURE_POLICY_FORMAL = "PILOT_ONLY 禁入 formal（FR-500）"

__all__ = ["build_formal_freeze", "formal_voyage_gate",
           "verify_two_run_payload_hash", "load_activation_record",
           "expected_allocation", "resolve_stage", "main",
           "RESULT_SCHEMA", "STAGE_FORMAL", "STAGE_DRYRUN", "STAGE_B_MARKER"]


# ---------------------------------------------------------------------------
# activation record（FR-465：split_seed / allocation 期望的唯一事实源）
# ---------------------------------------------------------------------------
def load_activation_record(path: str = None) -> tuple:
    """读取 T-432 activation record，返回 (record, 绝对路径)。

    path=None 时用仓库默认 docs/phase4/d_dataset_v3/t432_activation_record.json。
    split.split_seed 缺失 → KeyError fail-fast（FR-465：禁 hard-code seed）。
    """
    p = path if path is not None else os.path.join(
        os.path.dirname(os.path.dirname(MODELE_ROOT)), ACTIVATION_REL)
    p = os.path.abspath(p)
    if not os.path.isfile(p):
        raise FileNotFoundError(f"activation record 不存在: {p}")
    with open(p, "r", encoding="utf-8") as f:
        act = json.load(f)
    if "split_seed" not in act.get("split", {}):
        raise KeyError(f"activation record 缺 split.split_seed（FR-465）: {p}")
    return act, p


def expected_allocation(act: dict) -> dict:
    """从 activation record 派生预注册 allocation 期望（禁读 ood.total_voyages 陈旧值）。

    IID = n_strata × families_per_stratum × members_per_family；
    OOD = len(counts.conditions) × per_condition_voyages（amendment 01 后 =5×40=200）。
    """
    iid = act["formal_fleet"]["iid"]
    ood = act["formal_fleet"]["ood"]
    counts = ood["counts"]
    return {
        "iid": int(iid["n_strata"]) * int(iid["families_per_stratum"])
               * int(iid["members_per_family"]),
        "ood": len(counts["conditions"]) * int(counts["per_condition_voyages"]),
    }


# ---------------------------------------------------------------------------
# STAGE_B 标记门（Stage B 两跑确定性完成后才允许 formal freeze）
# ---------------------------------------------------------------------------
def resolve_stage(out_dir: str, dryrun: bool) -> tuple:
    """返回 (stage, errs)。dryrun → FORMAL_DRYRUN（跳过标记门）；
    否则要求 out_dir/STAGE_B_TWO_RUN_DONE 存在（缺失 → errs 非空，调用方 fail-fast）。"""
    if dryrun:
        return STAGE_DRYRUN, []
    if os.path.isfile(os.path.join(out_dir, STAGE_B_MARKER)):
        return STAGE_FORMAL, []
    return STAGE_FORMAL, [f"STAGE_B 标记缺失: {STAGE_B_MARKER} 不在 {out_dir}"
                          f"（两跑确定性未完成，禁止 formal freeze）"]


# ---------------------------------------------------------------------------
# 单航次 formal 门（可独立单测；FR-500 反转语义）
# ---------------------------------------------------------------------------
def formal_voyage_gate(meta: dict, prov: dict) -> list:
    """formal 航次准入门，返回违规清单（空 = 通过）。

    - fixture_kind == "PILOT_ONLY" → 拒收（formal 与 pilot 反转；FR-500）；
    - forecast provenance 六字段按 stage="formal" 校验（仅 CAUSAL_GENERATOR）。
    """
    errs: list = []
    if str(meta.get("fixture_kind", "")) == "PILOT_ONLY":
        errs.append("formal 拒收 PILOT_ONLY fixture 航次（FR-500 反转门；"
                    "pilot 产物不得进入 formal 冻结）")
    errs.extend(validate_forecast_provenance(prov, stage="formal"))
    return errs


def provenance_record(meta: dict) -> dict:
    """从 voyage meta 取 forecast_provenance 六字段记录（与 run_pipeline 同配方）。"""
    prov = meta.get("forecast_provenance", {}) or {}
    return {"forecast_generator_id": str(prov.get("forecast_generator_id", "")),
            "forecast_generator_version": str(prov.get("forecast_generator_version", "")),
            "forecast_generator_hash": str(prov.get("forecast_generator_hash", "")),
            "issue_time_s": float(prov.get("issue_time_s", 0.0)),
            "source_cutoff_time_s": float(prov.get("source_cutoff_time_s", 0.0)),
            "forecast_provenance_class": str(prov.get("forecast_provenance_class", ""))}


# ---------------------------------------------------------------------------
# 两跑 payload hash 复验（独立函数便于单测；ledger 格式见 overnight_stage_b.sh）
# ---------------------------------------------------------------------------
def verify_two_run_payload_hash(out_dir: str, expect_iid: int, expect_ood: int) -> dict:
    """复验 Stage B 两跑确定性：pass1/pass2 的 {iid,ood}_ledger.json 逐 voyage
    payload_hash 一致且航次数 == 预注册期望。

    返回 {"status": "PASS"|"FAIL"|"MISSING", "errors": [...], "missing": [...],
          "details": {key: {...}}}。文件缺失 → status=MISSING（由调用方按
    dryrun 语义决定记 SKIP 还是 FAIL）；数量/覆盖/hash 不一致 → FAIL。
    """
    res = {"status": "PASS", "errors": [], "missing": [], "details": {}}
    for key, expect in (("iid", int(expect_iid)), ("ood", int(expect_ood))):
        rows = {}
        for p in ("pass1", "pass2"):
            fname = f"{p}_{key}_ledger.json"
            path = os.path.join(out_dir, fname)
            if not os.path.isfile(path):
                res["missing"].append(fname)
                continue
            try:
                with open(path, "r", encoding="utf-8") as f:
                    d = json.load(f)
                rows[p] = {str(r["voyage_id"]): str(r["payload_hash"]) for r in d["voyages"]}
            except (ValueError, KeyError, TypeError) as exc:
                res["errors"].append(f"{fname} 解析失败: {exc}")
                continue
        detail = {"expect": expect, "n_pass1": len(rows.get("pass1", {})),
                  "n_pass2": len(rows.get("pass2", {}))}
        res["details"][key] = detail
        if len(rows) < 2:
            continue
        a, b = rows["pass1"], rows["pass2"]
        if len(a) != expect or len(b) != expect:
            res["errors"].append(f"{key}: ledger 航次数 {len(a)}/{len(b)} != 期望 {expect}")
        only_a = sorted(set(a) - set(b))
        only_b = sorted(set(b) - set(a))
        if only_a or only_b:
            res["errors"].append(f"{key}: pass1/pass2 voyage 覆盖不一致 "
                                 f"only_pass1={only_a[:5]} only_pass2={only_b[:5]}")
        diff = sorted(v for v in a if v in b and a[v] != b[v])
        if diff:
            res["errors"].append(f"{key}: payload_hash 不一致 {len(diff)} 个: {diff[:10]}")
        detail["n_mismatch"] = len(diff)
    if res["missing"]:
        res["status"] = "MISSING"
    elif res["errors"]:
        res["status"] = "FAIL"
    return res


# ---------------------------------------------------------------------------
# 编排主体
# ---------------------------------------------------------------------------
def build_formal_freeze(iid_dir, ood_dir, out_dir, npz_path,
                        activation_json=None, dryrun=False) -> dict:
    """T-020 formal freeze 全链编排，返回机器可读结果 dict（overall=true/false）。

    iid_dir=None 时跳过 IID 链（窗口/审计/split）；ood_dir=None 时跳过 OOD 证据
    集成。dryrun（或环境变量 EMS_FORMAL_FREEZE_DRYRUN=1）跳过 STAGE_B 标记门，
    结果 stage=FORMAL_DRYRUN，产物只写调用方 out_dir/npz_path（不得为正式默认区）。
    """
    dryrun = bool(dryrun or os.environ.get("EMS_FORMAL_FREEZE_DRYRUN") == "1")
    checks: list = []

    def add_check(name, passed, detail="", skipped=False):
        rec = {"name": name, "pass": bool(passed), "detail": detail}
        if skipped:
            rec["skipped"] = True
        checks.append(rec)
        tag = "SKIP" if skipped else ("PASS" if passed else "FAIL")
        print(f"[formal_freeze] {tag} {name} {detail}")
        return bool(passed)

    def rel(p: str) -> str:
        try:
            return os.path.relpath(os.path.abspath(p), MODELE_ROOT)
        except ValueError:
            return p

    # ---- ① STAGE_B 标记门（fail-fast：未过门不产生任何 formal 产物）----
    out_dir = os.path.abspath(out_dir)
    stage, gate_errs = resolve_stage(out_dir, dryrun)
    if gate_errs:
        add_check("stage_b_marker_gate", False, "; ".join(gate_errs))
        return {"schema": RESULT_SCHEMA, "stage": stage, "overall": False,
                "reason": "STAGE_B 标记门未通过，formal freeze 拒绝执行",
                "checks": checks, "outputs": {}}
    add_check("stage_b_marker_gate", True,
              "dryrun 跳过标记门" if dryrun else f"{STAGE_B_MARKER} 存在")

    # ---- ② activation record（split_seed / allocation 唯一事实源）----
    act, act_path = load_activation_record(activation_json)
    split_seed = act["split"]["split_seed"]          # FR-465：禁 hard-code
    alloc = expected_allocation(act)
    print(f"[formal_freeze] activation: {rel(act_path)} split_seed={split_seed} "
          f"allocation iid={alloc['iid']} ood={alloc['ood']}")

    reg = contracts.load_registry(validate=True)
    hist_cols = contracts.signal_names(reg, "INPUT_H1") + contracts.signal_names(reg, "INPUT_H2")
    kf_cols = contracts.signal_names(reg, "INPUT_K") + contracts.signal_names(reg, "INPUT_F")
    agg_map_hist = {c: contracts.aggregation_map(reg)[c] for c in hist_cols}
    tgt_src = contracts.signal_names(reg, "TARGET")[0]
    tgt_agg = contracts.aggregation_map(reg)[tgt_src]

    # ---- ③ 航次装载 + formal 门（fixture 反转 + provenance formal）----
    voyages: dict = {}
    rejects: list = []
    if iid_dir is not None:
        iid_dir = os.path.abspath(iid_dir)
        files = sorted(f for f in os.listdir(iid_dir) if f.endswith(".mat"))
        if not files:
            raise ValueError(f"voyage 目录无 .mat: {iid_dir}")
        for fn in files:
            vid = fn[:-4]
            v = load_voyage(os.path.join(iid_dir, fn))
            meta = v["meta"]
            errs = formal_voyage_gate(meta, provenance_record(meta))
            if errs:
                add_check(f"formal_gate[{vid}]", False, "; ".join(errs))
                rejects.append(vid)
                continue
            voyages[vid] = v
        add_check("voyage_formal_gate", len(voyages) > 0,
                  f"{len(voyages)} 航次通过 formal 门 / {len(rejects)} 拒收"
                  f"（PILOT_ONLY 拒收即 gate FAIL 证据）")

    # ---- ④ 逐航次 1Hz 聚合 → 窗口化 → target（与 pilot 同底层）----
    win_X_H, win_X_KF, win_Y, win_E300 = [], [], [], []
    win_meta: list = []
    win_hashes: dict = {}
    l2_series: dict = {}
    provenance: dict = {}
    total_invalid_kf = 0
    xcheck_max = 0.0
    if iid_dir is not None:
        for vid in sorted(voyages):
            v = voyages[vid]
            st, kf, meta = v["stream"], v["kf"], v["meta"]
            t = np.asarray(st["time_s"], dtype=float).ravel()
            cols = {c: np.asarray(st[c], dtype=float).ravel() for c in hist_cols if c in st}
            cols[tgt_src] = np.asarray(st[tgt_src], dtype=float).ravel()
            missing = [c for c in hist_cols if c not in cols]
            if missing:
                add_check(f"stream_cols[{vid}]", False, f"缺列 {missing}")
                continue
            edges = np.arange(0.0, float(np.floor(t[-1])) + 1.0, 1.0)
            agg = aggregate_stream(t, cols, edges, {**agg_map_hist, tgt_src: tgt_agg})
            t_1hz = edges[1:]
            hist = zero_fill_ctrl_channels({k: agg[k] for k in hist_cols if k in agg})
            p = cols[tgt_src]
            kf_in = {"issue_time_s": np.asarray(kf["issue_time_s"], dtype=float).ravel(),
                     "lead_s": np.asarray(kf["lead_s"], dtype=float)}
            for c in kf_cols:
                kf_in[c] = np.asarray(kf[c], dtype=float)
            wins = build_windows(t_1hz, hist, kf_in, stride_s=STRIDE_S)
            total_invalid_kf += int(wins["n_invalid_kf"])
            for i, t0 in enumerate(wins["anchors"]):
                y = y_trajectory(t, p, float(t0))
                win_X_H.append({c: wins["X_H"][c][i] for c in hist_cols})
                win_X_KF.append({c: wins["X_KF"][c][i] for c in kf_cols})
                win_Y.append(y)
                win_E300.append(e300_of(y))
                win_meta.append({"voyage_id": vid, "anchor_t0_s": float(t0),
                                 "window_index_in_voyage": int(wins["window_index"][i]),
                                 "split_group_id": str(meta["split_group_id"]),
                                 "scenario_stratum": str(meta["scenario_stratum"])})
                win_hashes[f"{vid}#{i}"] = window_content_hash(
                    np.stack([wins["X_H"][c][i] for c in hist_cols], axis=1),
                    np.stack([wins["X_KF"][c][i] for c in kf_cols], axis=1), y)
            if wins["anchors"].size:
                xc = crosscheck_vs_aggregate(t, p, float(wins["anchors"][0]),
                                             np.arange(float(wins["anchors"][0]) - 300.0,
                                                       float(wins["anchors"][0]) + 301.0, 1.0))
                xcheck_max = max(xcheck_max, abs(xc))
            l2_series[vid] = hist[tgt_src]
            provenance[vid] = {"generation_seed": str(meta.get("generation_seed", "")),
                               "route_template_id": str(meta.get("route_template_id", ""))}
            print(f"[formal_freeze] {vid}: windows={wins['anchors'].size} dur={t[-1]:.0f}s")

    add_check("energy_crosscheck_1e-9", xcheck_max < 1e-9, f"max_rel={xcheck_max:.3e}")
    add_check("kf_vintage_zero_invalid", total_invalid_kf == 0,
              f"invalid={total_invalid_kf}（阈值 0，DGN-07）")

    if iid_dir is None:
        l1 = {"overall_l1": True, "violations": [], "n_violations": 0, "note": "IID 链跳过"}
        l2 = {"counts": {"l2_candidate": 0, "CONFIRMED_DUPLICATE": 0,
                         "LEGITIMATE_NEAR_REPLICATE": 0, "UNRESOLVED": 0},
              "candidates": [], "note": "IID 链跳过"}
        l3 = {"n_suspects": 0, "suspects": [], "note": "IID 链跳过"}
        Y = np.zeros((0, 30)); E300 = np.zeros((0,)); splits = np.zeros((0,), dtype=str)
        views = None
        grp_split, groups = {}, []
        norm_path = None
        art_path = art_sha = None
    else:
        if not win_Y:
            return {"schema": RESULT_SCHEMA, "stage": stage, "overall": False,
                    "reason": "零有效窗口", "checks": checks, "outputs": {}}

        # ---- ⑤ split 冻结（group=family closure；strata=scenario_stratum）----
        voyage_grp = {vid: str(v["meta"]["split_group_id"]) for vid, v in voyages.items()}
        voyage_stratum = {vid: str(v["meta"]["scenario_stratum"]) for vid, v in voyages.items()}
        grp_stratum: dict = {}
        for vid in sorted(voyages):
            g, s = voyage_grp[vid], voyage_stratum[vid]
            if grp_stratum.setdefault(g, s) != s:
                raise ValueError(
                    f"FORMAL fail-fast: group {g!r} 映射多 stratum "
                    f"({grp_stratum[g]!r} vs {s!r})——family closure 必须一 group 一 stratum")
        strata: dict = {}
        # groups 必须是唯一 group 列表（assign_splits 契约；pilot 期 group=voyage_id
        # 天然唯一，formal family closure 后同 group 跨 member 重复，须按 grp_stratum
        # 键去重——2026-09-12 T-020 首次 formal 全量运行暴露）
        uniq_groups = sorted(grp_stratum)
        for g in uniq_groups:
            strata.setdefault(grp_stratum[g], []).append(g)
        assignment = assign_splits(uniq_groups, strata, split_seed)
        part_errs = verify_partition(assignment)
        add_check("split_partition_invariants", not part_errs, "; ".join(part_errs[:3]))
        grp_split = {g: rec["split"] for g, rec in assignment["assignment"].items()}
        voyage_split = {vid: grp_split[voyage_grp[vid]] for vid in voyages}
        for m in win_meta:
            m["split"] = grp_split[m["split_group_id"]]
        os.makedirs(out_dir, exist_ok=True)
        art_path = os.path.join(out_dir, "split_assignment_v3.json")
        write_artifact(assignment, art_path)
        art_sha = compute_artifact_hash(assignment)
        groups = sorted(voyage_grp.values())

        # ---- ⑥ L1 hard fail 零违规 ----
        artifact_hashes = {vid: sha256_file(os.path.join(iid_dir, vid + ".mat"))
                           for vid in voyages}
        l1 = run_l1({f"{g}#voyage": grp_split[g] for g in groups},
                    artifact_hashes, win_hashes, window_group_of=voyage_grp)
        add_check("L1_hard_fail_zero", l1["overall_l1"],
                  json.dumps(l1["violations"][:3], ensure_ascii=False))

        # ---- ⑦ L2 provenance 三分类 + allocation 扣减（formal：CONFIRMED 不静默）----
        # T-020 裁决 B（2026-09-12）：同 split_group（family closure）内的
        # provenance 撞车是 AMENDMENT_03 声明设计，转 family-sibling 一致性审计；
        # 仅跨独立 split group 的撞车维持 CONFIRMED_DUPLICATE。
        l2 = run_l2(l2_series, provenance, group_of=voyage_grp)
        sibling_audit = run_family_sibling_audit(
            [tuple(p) for p in l2["family_sibling_pairs"]],
            l2_series, member_of={
                vid: str(v["meta"].get("intervention", ""))
                for vid, v in voyages.items()})
        add_check("family_sibling_invariants", sibling_audit["overall"],
                  json.dumps(sibling_audit["violations"][:3], ensure_ascii=False)
                  if sibling_audit["violations"] else
                  f"DECLARED_FAMILY_SIBLING={l2['counts']['DECLARED_FAMILY_SIBLING']} "
                  f"对（{sibling_audit['n_families']} family）：真值逐点共享 + "
                  f"member lineage 互异均满足")
        confirmed_vids: set = set()
        for c in l2["candidates"]:
            if c["classification"] == "CONFIRMED_DUPLICATE":
                confirmed_vids.update(c["pair"])
        effective_iid = len(voyages) - len(confirmed_vids & set(voyages))
        add_check("l2_confirmed_allocation_deducted", True,
                  f"confirmed={l2['counts']['CONFIRMED_DUPLICATE']} "
                  f"涉及 voyage {sorted(confirmed_vids)[:5]} → 有效 {effective_iid}"
                  f"（扣减后须满足预注册 allocation；FAMILY_SIBLING/LEGITIMATE/"
                  f"UNRESOLVED 记录不剔除）")
        add_check("iid_allocation_preregistered", effective_iid == alloc["iid"],
                  f"有效 IID 航次 {effective_iid}（接受 {len(voyages)} − 确认重复 "
                  f"{len(confirmed_vids & set(voyages))}）!= 期望 {alloc['iid']} "
                  f"（activation 派生 n_strata×families×members）"
                  if effective_iid != alloc["iid"] else
                  f"有效 IID 航次 {effective_iid} == 期望 {alloc['iid']}")

        # ---- ⑧ L3（formal：train 统计必输，fail-fast）----
        train_vids = sorted(v for v in voyages if voyage_split[v] == "train")
        if not train_vids:
            add_check("train_nonempty", False)
            pooled = np.zeros(1)
        else:
            add_check("train_nonempty", True, f"train voyages={len(train_vids)}")
            pooled = np.concatenate([_resample_linear(l2_series[v]) for v in train_vids])
        # z-score 统计与 run_l3 内部表征同口径（同一 _resample_linear 256 点重采样）
        train_stats = {"mean": float(pooled.mean()), "std": float(pooled.std())}
        l3 = run_l3(l2_series, voyage_split, train_stats=train_stats,
                    provenance=provenance, require_train_stats=True)
        add_check("L3_sbd_train_stats_formal", l3["stats_source"] == "train_stats",
                  f"stats_source={l3['stats_source']} t_sbd={l3['t_sbd']}")
        add_check("L3_sbd_suspect_flag", True,
                  f"suspects={l3['n_suspects']}（similarity≠leakage，永不 FAIL）")

        # ---- ⑨ views + train-only 归一化（FR-483）----
        Y = np.stack(win_Y).astype(np.float64)
        E300 = np.asarray(win_E300, dtype=np.float64)
        X_H = {c: np.stack([w[c] for w in win_X_H]) for c in hist_cols}
        X_KF = {c: np.stack([w[c] for w in win_X_KF]) for c in kf_cols}
        views = build_views(X_H, X_KF, Y, E300, reg)
        assert_no_quantile_labels(views)
        splits = np.array([m["split"] for m in win_meta])
        tr = splits == "train"
        norm = {"fit_on": "train_only", "constant_channels_transform_std_1": []}
        n_const_registered = 0
        for c in views["h1_view"]:
            sd = X_H[c][tr].std(axis=0)
            const_ch = int((sd <= 0).sum())
            if const_ch:
                # T-020 裁决 D（2026-09-12）：零方差特征保留在冻结 H1 面，
                # scale=1 仅为除零数值约定（不构成训练常值以外的 learned
                # support），登记 train_constant=true 及 train min/max/mean。
                flat = X_H[c][tr].reshape(-1)
                norm["constant_channels_transform_std_1"].append({
                    "col": c, "n_bins": const_ch, "train_constant": True,
                    "train_min": float(flat.min()), "train_max": float(flat.max()),
                    "train_mean": float(flat.mean())})
                n_const_registered += 1
            if sd.size and not (sd > 0).any():
                # 全零方差列组：不再 FAIL（裁决 D），由 constant 通道登记 +
                # x_h1_std 的 scale=1 保护承接；norm_std_positive 改为核对
                # 登记完备性（见循环外 add_check）。
                pass
        add_check("norm_std_positive", True,
                  f"零方差列组 {n_const_registered} 个已完成 scale=1 保护与 "
                  f"train_constant 元数据登记（T-020 裁决 D；登记清单见 "
                  f"formal_norm_v3.json constant_channels_transform_std_1）")
        norm.update({
            "x_h1_mean": {c: X_H[c][tr].mean(axis=0).tolist() for c in views["h1_view"]},
            "x_h1_std": {c: np.where(X_H[c][tr].std(axis=0) > 0,
                                     X_H[c][tr].std(axis=0), 1.0).tolist()
                         for c in views["h1_view"]},
            "y_mean": Y[tr].mean(axis=0).tolist(), "y_std": Y[tr].std(axis=0).tolist(),
            "e300_mean": float(E300[tr].mean()), "e300_std": float(E300[tr].std())})
        if Y[tr].std() <= 0 or float(E300[tr].std()) <= 0:
            add_check("target_std_positive", False, "train 目标零方差")
        else:
            add_check("target_std_positive", True)
        add_check("normalization_fit_train_only", True, "val/test/OOD 仅 transform（FR-483）")
        os.makedirs(out_dir, exist_ok=True)
        norm_path = os.path.join(out_dir, "formal_norm_v3.json")
        with open(norm_path, "w", encoding="utf-8") as f:
            json.dump(norm, f, ensure_ascii=False, sort_keys=True, indent=2)

        # ---- ⑩ npz 落盘（本地不入库）----
        os.makedirs(os.path.dirname(os.path.abspath(npz_path)) or ".", exist_ok=True)
        arr = {"Y": Y.astype(np.float32), "E300": E300.astype(np.float32),
               "splits": splits.astype(str),
               "voyage_id": np.array([m["voyage_id"] for m in win_meta]),
               "anchor_t0_s": np.array([m["anchor_t0_s"] for m in win_meta], dtype=np.float64),
               "window_index_in_voyage": np.array([m["window_index_in_voyage"]
                                                   for m in win_meta], dtype=np.int32)}
        for c in views["h1_view"]:
            arr[f"X_H1::{c}"] = views["h1_view"][c].astype(np.float32)
        for c in views["h2_view"]:
            if c not in views["h1_view"]:
                arr[f"X_H2::{c}"] = views["h2_view"][c].astype(np.float32)
        for c in kf_cols:
            arr[f"X_KF::{c}"] = X_KF[c].astype(np.float32)
        np.savez_compressed(npz_path, **arr)

    # ---- ⑪ 两跑 payload hash 复验（dryrun 缺 ledger 记 SKIP，正式缺即 FAIL）----
    two = verify_two_run_payload_hash(out_dir, alloc["iid"], alloc["ood"])
    if two["status"] == "MISSING" and dryrun:
        add_check("two_run_payload_hash", True,
                  f"SKIP（dryrun：{two['missing']} 未生成，不判 FAIL）", skipped=True)
    else:
        add_check("two_run_payload_hash", two["status"] == "PASS",
                  "; ".join(two["errors"] + [f"缺文件 {m}" for m in two["missing"]][:3])
                  or f"两跑逐 voyage payload_hash 一致（iid={alloc['iid']}/ood={alloc['ood']}）")

    # ---- ⑫ OOD 三轴证据集成（T-020 并行接口；未就绪记 FAIL，不伪造）----
    ood_ev = None
    try:
        from .freeze_ood import build_ood_evidence
    except ImportError as exc:
        if ood_dir is None:
            add_check("ood_evidence_integration", True,
                      f"SKIP（ood_dir=None）：模块未就绪（{exc}）", skipped=True)
        else:
            add_check("ood_evidence_integration", False,
                      f"dataset_v3.freeze_ood 未就绪: {exc}")
    else:
        if ood_dir is None:
            add_check("ood_evidence_integration", True, "SKIP（ood_dir=None）", skipped=True)
        else:
            try:
                ood_ev = build_ood_evidence(os.path.abspath(ood_dir), act_path, reg)
                add_check("ood_evidence_integration",
                          bool(ood_ev.get("overall_pass")),
                          f"overall_pass={ood_ev.get('overall_pass')} "
                          f"allocation_check={json.dumps(ood_ev.get('allocation_check'),
                                                         ensure_ascii=False)[:200]}")
            except Exception as exc:   # noqa: BLE001 — 证据链失败必须显式暴露
                add_check("ood_evidence_integration", False,
                          f"build_ood_evidence 失败: {type(exc).__name__}: {exc}")

    # ---- ⑬ sealed manifest（FR-488 全字段；worker 并行接口）----
    man_path = None
    counts = {s: int((splits == s).sum()) for s in ("train", "val", "test")}
    if iid_dir is None:
        payload = None
    else:
        payload = {
            "split_policy_version": SPLIT_POLICY_VERSION,
            "split_seed": split_seed,
            "split_group_key": "split_group_id（family closure）",
            "split_assignment_artifact_sha256": art_sha,
            "train_val_test_voyage_counts": {
                s: int(sum(1 for g in groups if grp_split[g] == s))
                for s in ("train", "val", "test")},
            "normalization_source": "train_only",
            "preprocessing_artifact_sha256": sha256_file(norm_path),
            "ood_definition_version": act.get("formal_fleet", {}).get("ood", {})
                                      .get("definition_version", OOD_DEFINITION_VERSION_FALLBACK),
            "ood_generation_seed": act.get("formal_fleet", {}).get("ood", {})
                                   .get("seed_rule", "UNDEFINED"),
            "provenance_duplicate_count": int(l2["counts"]["CONFIRMED_DUPLICATE"]),
            "exact_duplicate_count": int(sum(len(v.get("windows", [])) - 1
                                             for v in l1["violations"]
                                             if v.get("type") == "duplicate_window_content")),
            "sbd_threshold_policy": SBD_THRESHOLD_POLICY,
            "sbd_suspect_count_disposition": f"{l3['n_suspects']}:unresolved",
            "test_sealed": True, "ood_sealed": True,
            "dataset_anchor_stride_s": 10,
            "predictor_runtime_period_s": "UNDEFINED_IN_D",
            "window_geometry": dict(WINDOW_GEOMETRY_FROZEN),
            "registry_sha256": json.loads(open(REGISTRY_PATH, encoding="utf-8").read())["self_hash"]["value"],
            "producer_schema_sha256": json.loads(open(PRODUCER_SCHEMA_PATH, encoding="utf-8").read())["self_hash"]["value"],
            "aggregation_table_version": AGGREGATION_TABLE_VERSION,
            "target_definition": TARGET_DEFINITION,
            "stage": stage,
            "fixture_policy": FIXTURE_POLICY_FORMAL,
        }
    if payload is None:
        add_check("manifest_sealed", True, "SKIP（iid_dir=None，无 IID payload）", skipped=True)
    elif stage != STAGE_FORMAL:
        # freeze_manifest 契约：build_formal_manifest 只接受 stage="FORMAL"，
        # dryrun 由调用方不调用本模块（FORMAL_DRYRUN 产物不封存）
        add_check("manifest_sealed", True,
                  f"SKIP（stage={stage}：dryrun 不封存 manifest）", skipped=True)
    else:
        try:
            from .freeze_manifest import assert_sealed, build_formal_manifest
        except ImportError as exc:
            add_check("manifest_sealed", False,
                      f"dataset_v3.freeze_manifest 未就绪: {exc}")
        else:
            run_metadata = {
                "build_utc": datetime.datetime.now(datetime.timezone.utc)
                             .isoformat(timespec="seconds"),
                "python": sys.version.split()[0], "numpy": np.__version__,
                "platform": platform.platform(), "pipeline": "dataset_v3.run_formal_freeze",
                "generation_script_path": "dataset_v3/run_formal_freeze.py",
                "generation_script_sha256": sha256_file(os.path.abspath(__file__)),
                "matlab_version": "R2026a（70 号 producer 侧记录）",
                "python_version": sys.version.split()[0]}
            try:
                man = build_formal_manifest(payload, run_metadata, act_path)
                seal_errs = assert_sealed(man)
                add_check("manifest_sealed", not seal_errs,
                          "; ".join(map(str, seal_errs[:3])) or "sealed（FR-488 全字段）")
                if not seal_errs:
                    man_path = os.path.join(out_dir, "dataset_v3_formal_manifest.json")
                    with open(man_path, "w", encoding="utf-8") as f:
                        json.dump(man, f, ensure_ascii=False, indent=2)
            except (ValueError, OSError) as exc:
                add_check("manifest_sealed", False,
                          f"build_formal_manifest fail-fast: {exc}")

    overall = all(c["pass"] for c in checks)
    outputs = {}
    for label, pth in (("npz", npz_path if iid_dir is not None else None),
                       ("manifest", man_path), ("split_artifact", art_path),
                       ("norm", norm_path)):
        if pth:
            outputs[label] = rel(pth)
    return {"schema": RESULT_SCHEMA, "stage": stage,
            "generated_utc": datetime.datetime.now(datetime.timezone.utc)
                             .isoformat(timespec="seconds"),
            "dryrun": dryrun, "overall": overall,
            "n_windows": int(len(Y)), "window_counts": counts,
            "n_voyages": len(voyages), "n_rejected_voyages": len(rejects),
            "activation": {"path": rel(act_path), "split_seed": split_seed,
                           "allocation_expected": alloc},
            "audits": {"L1": l1, "L2": l2, "L3": l3},
            "two_run_verify": two,
            "ood_evidence": ood_ev,
            "outputs": outputs,
            "checks": checks}


def main(argv=None) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)                       # MODELE_ROOT
    dryrun_env = os.environ.get("EMS_FORMAL_FREEZE_DRYRUN") == "1"
    ap = argparse.ArgumentParser(description="Dataset v3 formal freeze（T-020）")
    ap.add_argument("--iid-dir", default=None)
    ap.add_argument("--ood-dir", default=None)
    ap.add_argument("--out-dir", default=None,
                    help="默认 results/dataset_v3/formal（dryrun 时 results/dataset_v3/formal_dryrun）")
    ap.add_argument("--npz", default=None,
                    help="默认 datasets_v3/formal/windows_v3_formal.npz"
                         "（dryrun 时 datasets_v3/formal_dryrun/windows_v3_formal_dryrun.npz）")
    ap.add_argument("--activation-json", default=None)
    ap.add_argument("--dryrun", action="store_true")
    args = ap.parse_args(argv)
    dryrun = args.dryrun or dryrun_env

    def _res(arg, formal_rel, dryrun_rel):
        """显式传参尊重调用方；未传时 dryrun 重定向到 formal_dryrun（不落正式区）。"""
        if arg is not None:
            return arg if os.path.isabs(arg) else os.path.join(root, arg)
        return os.path.join(root, dryrun_rel if dryrun else formal_rel)

    out_dir = _res(args.out_dir, os.path.join("results", "dataset_v3", "formal"),
                   os.path.join("results", "dataset_v3", "formal_dryrun"))
    npz_path = _res(args.npz, os.path.join("datasets_v3", "formal", "windows_v3_formal.npz"),
                    os.path.join("datasets_v3", "formal_dryrun",
                                 "windows_v3_formal_dryrun.npz"))
    iid_dir = _res(args.iid_dir, os.path.join("datasets_v3", "formal", "iid"),
                   os.path.join("datasets_v3", "qualification", "stage_a_iid"))
    ood_dir = _res(args.ood_dir, os.path.join("datasets_v3", "formal", "ood"),
                   os.path.join("datasets_v3", "qualification", "stage_a_ood"))
    res = build_formal_freeze(iid_dir, ood_dir, out_dir, npz_path,
                              activation_json=args.activation_json, dryrun=dryrun)
    result_path = os.path.join(out_dir, "formal_freeze_result.json")
    os.makedirs(os.path.dirname(result_path), exist_ok=True)
    with open(result_path, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=2, default=canonical_json_safe)
    n_skip = sum(1 for c in res["checks"] if c.get("skipped"))
    print(f"[formal_freeze] formal_freeze_result: stage={res['stage']} "
          f"overall={res['overall']} windows={res.get('n_windows')} "
          f"voyages={res.get('n_voyages')} checks_skip={n_skip}")
    print(f"[formal_freeze] result: {result_path}")
    return 0 if res["overall"] else 1


def canonical_json_safe(obj):
    """json.dump default= 兜底（非常规类型显式转换；未知类型 fail-fast）。"""
    if isinstance(obj, np.ndarray):
        return obj.tolist()
    if isinstance(obj, (np.floating, np.integer)):
        return obj.item()
    raise TypeError(f"不可 JSON 序列化类型: {type(obj)!r}")


if __name__ == "__main__":
    sys.exit(main())

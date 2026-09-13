#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""freeze_ood.py — T-020 formal freeze：OOD 三轴（A/B/C）正式证据模块.

Fixed Decisions（delegation spec 冻结，不得更改）：
  - 期望 allocation 仅由 activation record formal_fleet.ood.counts 派生：
    len(counts.conditions) × counts.per_condition_voyages
    （amendment 01 后 conditions = A1/B1/C1/C2/C3 → 5 × 40 = 200）。
    total_voyages 为 amendment 01 前的陈旧冗余字段，禁止作为期望总数
    （本模块从不读取该字段）。
  - 期望条件清单唯一来源 = counts.conditions；出现清单外的 ood_condition_id
    （如 amendment 01 已退役的轴 A (2.4, 2.8] stress band 条件）→ FAIL，
    且该记录不计入正式 OOD counts。
  - scientific membership 一律用 realized 值（Stage A 样本 mat 实证字段）：
      轴 A ← meta.propulsion_check.inputs.u_sup_abs  → audit_ood 记录
              u_abs_mps（= stream 全程 sup|v_current_mps|，绝对流速定义）；
      轴 B ← meta.propulsion_check.inputs.ht_effective → ht_ratio
              （DRY 水位实现后的 realized ht_effective）；
      轴 C ← meta.ood_token_id 经 heldout token registry 派生
              composition = [factor_a, factor_b]（canonical token 禁手抄）。
    域判定逻辑 100% 复用 audit_ood.validate_ood_records / classify_ood，
    本模块不重复实现任何域判定语义。
  - FR-486：classify_ood 判为 PHYSICS_EXTRAPOLATION/STRESS_SET（以及轴 B
    UNCLASSIFIED margin 带）的 voyage 分流出证，绝不混入正式 OOD counts；
    正式冻结语料中出现任一条即 overall FAIL（formal 语料必须物理域纯净）。
  - FR-485：OOD 为独立集合，绝不进入 IID splitter —— 常量声明字段
    ood_records_enter_iid_splitter = False。
  - gate 语义：空目录/缺文件/activation 缺失/registry 失效 →
    overall_pass=False 带原因清单，不抛异常（调用方以机器可读证据为准）。

接口（与 worker-1 调用约定一致）：
  build_ood_evidence(ood_dir: str, activation_json: str, registry=None) -> dict
  load_ood_mat(path: str) -> dict

返回 dict 固定 keys：allocation_check / axes / stress_set / per_condition /
overall_pass；extra keys 自由（checks / reasons / totals / splitter 声明等）。
"""

from __future__ import annotations

import json
import math
import os

import numpy as np
from scipy.io import loadmat

from dataset_v3.audit_ood import (
    AXIS_VALUE_FIELD,
    OOD_AXES,
    VERDICT_NONE,
    VERDICT_OOD,
    VERDICT_PHYSICS_EXTRAPOLATION,
    VERDICT_UNCLASSIFIED,
    classify_ood,
    load_heldout_token_registry,
    validate_ood_records,
)

__all__ = ["build_ood_evidence", "load_ood_mat", "OOD_RECORDS_ENTER_IID_SPLITTER"]

# FR-485：OOD 独立集合，绝不进入 IID splitter（常量证据，冻结）。
OOD_RECORDS_ENTER_IID_SPLITTER = False

# realized 域指标字段（Stage A OOD 样本 mat 实证；propulsion_check 为
# generation pipeline D5 段物理域 guard 的落盘产物）。
_REALIZED_METRIC_FIELD = {
    "A": "propulsion_check.inputs.u_sup_abs",
    "B": "propulsion_check.inputs.ht_effective",
}


# --------------------------------------------------------------------------
# 基础装载
# --------------------------------------------------------------------------
def _norm_str(v):
    """mat 结构字段 → 规整 str 或 None（空数组/空白 → None）。"""
    if v is None:
        return None
    if isinstance(v, np.ndarray):
        if v.size == 0:
            return None
        v = v.reshape(-1)[0]
    if isinstance(v, np.generic):
        v = v.item()
    if isinstance(v, bytes):
        v = v.decode("utf-8", "replace")
    if isinstance(v, (list, tuple, dict)):
        return None
    s = str(v).strip()
    return s or None


def load_ood_mat(path: str) -> dict:
    """装载单个 OOD voyage .mat（stream/kf/meta 三表，simplify_cells=True）.

    返回 {"voyage_id", "meta", "stream", "kf", "path"}；voyage_id 优先取
    meta.voyage_id，缺失时回退文件名 stem。三表任一缺失/非 struct →
    ValueError（调用方 gate 循环逐文件捕获，不降级）。
    """
    data = loadmat(path, simplify_cells=True)
    out = {}
    for tbl in ("stream", "kf", "meta"):
        v = data.get(tbl)
        if not isinstance(v, dict):
            raise ValueError(f"{os.path.basename(path)}: 缺表 {tbl} 或非 struct")
        out[tbl] = v
    vid = _norm_str(out["meta"].get("voyage_id")) \
        or os.path.splitext(os.path.basename(path))[0]
    out["voyage_id"] = vid
    out["path"] = path
    return out


def _resolve_registry(registry):
    """registry 参数 → (REQUIRE token 条目清单, registry_source 出证串).

    None → audit_ood 默认冻结 registry 路径；str → 该路径；
    dict 含 tokens → 按条目过滤；list → 视为已装载条目；
    dict 无 tokens（如 T-020 编排层直传 contracts.load_registry() 的信号
    registry）→ 并非 token registry，回退冻结默认 registry（D4 唯一事实
    源），不降级为空、不改变判定语义。
    """
    if registry is None:
        return load_heldout_token_registry(), "default_frozen"
    if isinstance(registry, str):
        return load_heldout_token_registry(registry), f"path:{registry}"
    if isinstance(registry, dict):
        toks = registry.get("tokens")
        if not isinstance(toks, list):
            return load_heldout_token_registry(), \
                "caller_dict_not_token_registry_fallback_default_frozen"
        return ([t for t in toks if isinstance(t, dict)
                 and t.get("ood_action") == "REQUIRE"],
                "caller_dict_tokens")
    toks = list(registry)
    if not toks:
        raise ValueError("registry 条目清单为空")
    return toks, "caller_token_entries"


def _registry_token_map(tokens) -> dict:
    """token_id → (factor_a, factor_b)；缺关键字段的条目 fail-closed。"""
    out = {}
    for tk in tokens:
        for fld in ("token_id", "factor_a", "factor_b"):
            if fld not in tk:
                raise ValueError(f"token 缺字段 {fld}: {tk!r}")
        out[str(tk["token_id"])] = (str(tk["factor_a"]), str(tk["factor_b"]))
    return out


# --------------------------------------------------------------------------
# record 构造（只做字段映射，域判定全部委托 audit_ood）
# --------------------------------------------------------------------------
def _realized_metric(meta: dict, axis: str):
    """轴 A/B：从 meta.propulsion_check.inputs 取 realized 域指标。"""
    pc = meta.get("propulsion_check")
    inputs = pc.get("inputs") if isinstance(pc, dict) else None
    if not isinstance(inputs, dict):
        return None, "meta.propulsion_check.inputs 缺失或非 struct"
    fld = "u_sup_abs" if axis == "A" else "ht_effective"
    v = inputs.get(fld)
    if isinstance(v, bool) or not isinstance(v, (int, float, np.integer, np.floating)):
        return None, f"realized 域指标 {fld} 缺失或非数值: {v!r}"
    v = float(v)
    if not math.isfinite(v):
        return None, f"realized 域指标 {fld} 非有限数值: {v!r}"
    return v, None


def _mat_to_record(meta: dict, voyage_id: str, token_map: dict):
    """producer meta → audit_ood 冻结记录结构 + 证据附加字段。

    返回 (rec, ev) 或 (None, error_reason_str)。本函数不判定域归属，
    仅映射字段；判定由 classify_ood 承担。
    """
    axis = _norm_str(meta.get("ood_axis"))
    if axis not in OOD_AXES:
        return None, f"meta.ood_axis 非法或缺失: {axis!r}（须 ∈ {OOD_AXES}）"
    rec = {
        "voyage_id": voyage_id,
        "ood_axis": axis,
        "ood_def_version": _norm_str(meta.get("ood_def_version")),
        "physics_domain_ref": _norm_str(meta.get("physics_domain_ref")),
    }
    ev = {
        "voyage_id": voyage_id,
        "ood_axis": axis,
        "ood_def_version": _norm_str(meta.get("ood_def_version")),
        "ood_condition_id": _norm_str(meta.get("ood_condition_id")),
        "ood_token_id": _norm_str(meta.get("ood_token_id")),
        "dataset_domain": _norm_str(meta.get("dataset_domain")),
        "heldout_token_registry_sha":
            _norm_str(meta.get("heldout_token_registry_sha")),
        "metric_field": _REALIZED_METRIC_FIELD.get(axis),
        "metric_value": None,
    }
    if axis in ("A", "B"):
        val, err = _realized_metric(meta, axis)
        if err:
            return None, err
        ev["metric_value"] = val
        rec[AXIS_VALUE_FIELD[axis]] = val
    else:  # 轴 C：canonical token → registry 派生 composition（禁手抄）
        tok = ev["ood_token_id"]
        if not tok:
            return None, "轴 C 缺 meta.ood_token_id，无法 registry 派生 composition"
        factors = token_map.get(tok)
        if factors is None:
            return None, (f"轴 C ood_token_id 不在 registry REQUIRE token 清单: "
                          f"{tok!r}")
        ev["composition"] = [factors[0], factors[1]]
        rec["composition"] = [factors[0], factors[1]]
    return (rec, ev), None


def _stats(values):
    if not values:
        return None
    fs = [float(v) for v in values]
    return {"n": len(fs), "min": min(fs),
            "mean": sum(fs) / len(fs), "max": max(fs)}


def _failure_evidence(reasons: list, ood_dir=None, activation_json=None) -> dict:
    """gate 语义失败证据：固定 keys 齐备、overall_pass=False、不抛异常。"""
    return {
        "allocation_check": {"pass": False, "per_condition": {},
                             "total": {"expected": None, "actual": None,
                                       "pass": False}},
        "axes": {},
        "stress_set": {"diverted": True, "count": 0, "voyage_ids": [],
                       "by_axis": {}, "unclassified_count": 0,
                       "unclassified_voyage_ids": [], "pass": False},
        "per_condition": {},
        "overall_pass": False,
        "ood_records_enter_iid_splitter": OOD_RECORDS_ENTER_IID_SPLITTER,
        "reasons": list(reasons),
        "ood_dir": ood_dir,
        "activation_json": activation_json,
    }


# --------------------------------------------------------------------------
# 主入口
# --------------------------------------------------------------------------
def build_ood_evidence(ood_dir: str, activation_json: str,
                       registry=None) -> dict:
    """OOD 三轴正式冻结证据（T-020 formal freeze）.

    步骤：activation counts 派生期望 → 装载 ood_dir 全部 .mat → 映射为
    audit_ood 记录（realized 域指标）→ validate → classify → 分组出证 →
    allocation 逐 condition 精确对照。空目录/缺文件 → overall_pass=False。
    """
    reasons: list = []

    # ---- activation record：期望 allocation 唯一事实源 ----
    try:
        with open(activation_json, "r", encoding="utf-8") as f:
            act = json.load(f)
        ood_act = act["formal_fleet"]["ood"]
        counts = ood_act["counts"]
        exp_conditions = [str(c) for c in counts["conditions"]]
        per_cond = int(counts["per_condition_voyages"])
    except (OSError, ValueError, KeyError, TypeError) as e:
        reasons.append(f"activation record 不可用（formal_fleet.ood.counts）: {e!r}")
        return _failure_evidence(reasons, ood_dir, activation_json)
    # 注意：total_voyages 为 amendment 01 前陈旧冗余字段，刻意不读取；
    # 期望总数 = len(conditions) × per_condition_voyages。
    exp_total = len(exp_conditions) * per_cond
    exp_cond_set = set(exp_conditions)
    act_def_version = ood_act.get("definition_version")

    # ---- heldout token registry（轴 C composition 派生；D4 禁手抄）----
    try:
        tokens, registry_source = _resolve_registry(registry)
        token_map = _registry_token_map(tokens)
    except (OSError, ValueError) as e:
        reasons.append(f"heldout token registry 不可用: {e!r}")
        return _failure_evidence(reasons, ood_dir, activation_json)

    # ---- 装载 voyage .mat ----
    if not os.path.isdir(ood_dir):
        return _failure_evidence(
            [f"ood_dir 不存在或非目录: {ood_dir!r}"], ood_dir, activation_json)
    files = sorted(f for f in os.listdir(ood_dir) if f.lower().endswith(".mat"))
    if not files:
        return _failure_evidence(
            [f"ood_dir 无 .mat voyage 文件（formal 语料为空）: {ood_dir!r}"],
            ood_dir, activation_json)

    # ---- 冻结分配计划：planned generation identity 权威（T-020 裁决 C，2026-09-12）----
    # planned_condition_id = plan block（生成块，经文件名前缀 linkage）；
    # realized_ood_condition_id = producer 按 DGN-11 + Axis-C 优先写出的 realized
    # 独占分类（meta/ledger）。二者是两个独立身份：planned gate 保持 10 families
    # / 40 voyages 精确；realized 计数如实报告，不要求 planned==realized 相等。
    plan_path = os.path.join(ood_dir, "formal_draw_allocation_plan_ood.json")
    try:
        with open(plan_path, "r", encoding="utf-8") as f:
            plan = json.load(f)
        planned_quota = {str(b["condition_id"]): int(b["quota"])
                         for b in plan["blocks"]}
    except (OSError, ValueError, KeyError, TypeError) as e:
        reasons.append(f"冻结分配计划不可读（planned identity 权威）: {e!r}")
        return _failure_evidence(reasons, ood_dir, activation_json)

    load_errors: list = []
    validation_errors: list = []
    metadata_errors: list = []
    classification_errors: list = []
    unexpected_condition_ids: set = set()

    voyages: list = []   # {"rec","ev"}（已通过结构校验与 metadata 校验）
    all_recs: list = []
    rec_index: dict = {}  # list idx → voyage_id（validate 错误回投）

    for fname in files:
        fpath = os.path.join(ood_dir, fname)
        try:
            payload = load_ood_mat(fpath)
        except Exception as e:  # noqa: BLE001 — gate 语义逐文件捕获
            load_errors.append({"file": fname, "error": str(e)})
            continue
        vid = payload["voyage_id"]
        meta = payload["meta"]
        # planned identity（plan block 文件名 linkage）：FLTB_OOD_<cond>_<draw>_<member>
        _fp = fname.split("_")
        _planned_cond = _fp[2] if len(_fp) > 3 else ""
        _planned_fam = "_".join(_fp[2:4]) if len(_fp) > 4 else _planned_cond

        built, err = _mat_to_record(meta, vid, token_map)
        if err:
            metadata_errors.append({"voyage_id": vid, "file": fname,
                                    "reason": err})
            continue
        rec, ev = built
        ev = dict(ev, source_file=fname,
                  planned_condition_id=_planned_cond,
                  planned_family_id=_planned_fam)

        # metadata 层校验（audit_ood 结构校验之外的 producer 语义层）
        cond = ev["ood_condition_id"]
        if not cond:
            metadata_errors.append({"voyage_id": vid, "file": fname,
                                    "reason": "meta 缺 ood_condition_id"})
            continue
        if cond not in exp_cond_set:
            # 例：amendment 01 已退役条件出现在语料中 → 拒收并单独出证
            unexpected_condition_ids.add(cond)
            metadata_errors.append({"voyage_id": vid, "file": fname,
                                    "reason": (f"ood_condition_id {cond!r} 不在 "
                                               f"activation counts.conditions "
                                               f"{sorted(exp_cond_set)}（退役/未知条件）")})
            continue
        if ev["dataset_domain"] != "OOD":
            metadata_errors.append({"voyage_id": vid, "file": fname,
                                    "reason": (f"meta.dataset_domain 须为 'OOD'，"
                                               f"实际 {ev['dataset_domain']!r}")})
            continue
        if act_def_version and ev["ood_def_version"] \
                and ev["ood_def_version"] != act_def_version:
            metadata_errors.append({"voyage_id": vid, "file": fname,
                                    "reason": (f"ood_def_version {ev['ood_def_version']!r} "
                                               f"≠ activation definition_version "
                                               f"{act_def_version!r}")})
            continue

        idx = len(all_recs)
        all_recs.append(rec)
        rec_index[idx] = vid
        voyages.append({"rec": rec, "ev": ev})

    # 结构校验（audit_ood 冻结校验器，逐条归因）
    for i, rec in enumerate(all_recs):
        for e in validate_ood_records([rec]):
            validation_errors.append({"voyage_id": rec_index[i], "reason": e})

    invalid_vids = {e["voyage_id"] for e in validation_errors}
    valid_voyages = [v for v in voyages
                     if v["ev"]["voyage_id"] not in invalid_vids]

    # ---- 物理域判定（复用 audit_ood.classify_ood；realized 值）----
    formal: list = []          # verdict == OOD
    stress: list = []          # PHYSICS_EXTRAPOLATION / STRESS_SET
    unclassified: list = []    # 轴 B margin 带
    support_none: list = []    # realized 值落在训练支持域（不满足科学成员资格）
    for v in valid_voyages:
        try:
            cls = classify_ood(v["rec"])
        except ValueError as e:
            classification_errors.append({"voyage_id": v["ev"]["voyage_id"],
                                          "reason": str(e)})
            continue
        v["ev"]["verdict"] = cls["verdict"]
        v["ev"]["verdict_detail"] = cls["detail"]
        v["ev"]["stress_set"] = bool(cls.get("stress_set"))
        if cls["verdict"] == VERDICT_OOD:
            formal.append(v)
        elif cls["verdict"] == VERDICT_PHYSICS_EXTRAPOLATION:
            stress.append(v)
        elif cls["verdict"] == VERDICT_UNCLASSIFIED:
            unclassified.append(v)
        elif cls["verdict"] == VERDICT_NONE:
            support_none.append(v)
        else:  # pragma: no cover — audit_ood 契约外新增 verdict 时 fail-closed
            classification_errors.append(
                {"voyage_id": v["ev"]["voyage_id"],
                 "reason": f"未知 verdict: {cls['verdict']!r}"})

    # ---- per_condition 分组出证（仅 verdict==OOD 的正式记录计数）----
    per_condition: dict = {}
    for cond in exp_conditions:
        group = [v for v in formal if v["ev"]["ood_condition_id"] == cond]
        axes_seen = sorted({v["ev"]["ood_axis"] for v in group})
        per_condition[cond] = {
            "expected": per_cond,
            "actual": len(group),
            "axes_seen": axes_seen,
            # realized 计数不要求等于 planned 预注册值（T-020 裁决 C）；
            # 保留布尔仅作"realized==planned 计数"事实标记，不参与 gate。
            "realized_equals_planned_count": len(group) == per_cond,
            "metric_field": group[0]["ev"]["metric_field"] if group else None,
            "metric_stats": _stats([v["ev"]["metric_value"] for v in group
                                    if v["ev"]["metric_value"] is not None]),
            "compositions": sorted({tuple(v["ev"]["composition"])
                                    for v in group
                                    if "composition" in v["ev"]}),
            "voyage_ids": sorted(v["ev"]["voyage_id"] for v in group),
        }

    # ---- 轴归属出证（A/B/C）----
    axes: dict = {}
    for ax in OOD_AXES:
        group = [v for v in formal if v["ev"]["ood_axis"] == ax]
        axes[ax] = {
            "formal_count": len(group),
            "conditions": sorted({v["ev"]["ood_condition_id"]
                                  for v in group}),
            "metric_field": AXIS_VALUE_FIELD[ax],
            "metric_stats": _stats([v["ev"]["metric_value"] for v in group
                                    if v["ev"]["metric_value"] is not None]),
        }

    # ---- FR-486 分流出证（不混入正式 OOD counts）----
    stress_by_axis: dict = {}
    for v in stress:
        stress_by_axis.setdefault(v["ev"]["ood_axis"], []).append(
            v["ev"]["voyage_id"])
    stress_block = {
        "diverted": True,
        "count": len(stress),
        "voyage_ids": sorted(v["ev"]["voyage_id"] for v in stress),
        "by_axis": {k: sorted(vs) for k, vs in sorted(stress_by_axis.items())},
        "unclassified_count": len(unclassified),
        "unclassified_voyage_ids": sorted(v["ev"]["voyage_id"]
                                          for v in unclassified),
        "support_domain_none_count": len(support_none),
        "support_domain_none_voyage_ids": sorted(v["ev"]["voyage_id"]
                                                 for v in support_none),
        "excluded_from_formal_counts": True,
        "fr486": ("PHYSICS_EXTRAPOLATION/STRESS_SET（含轴 B UNCLASSIFIED "
                  "margin 带）分流出证，不混入正式 OOD counts；formal 语料"
                  "出现任一条即 FAIL"),
        # formal 冻结语料必须物理域纯净
        "pass": not stress and not unclassified,
    }

    # ---- planned allocation gate（hard；T-020 裁决 C：plan block = planned
    #      generation identity，逐 condition 精确 10 families / 40 voyages）----
    planned_counts: dict = {}
    planned_family_ids: dict = {}
    for v in voyages:
        pc_ = v["ev"].get("planned_condition_id", "_unknown")
        pf_ = v["ev"].get("planned_family_id", "_unknown")
        planned_counts[pc_] = planned_counts.get(pc_, 0) + 1
        planned_family_ids.setdefault(pc_, set()).add(pf_)
    planned_gate_rows: dict = {}
    for c in exp_conditions:
        n_fam = len(planned_family_ids.get(c, ()))
        planned_gate_rows[c] = {
            "expected_families": planned_quota.get(c),
            "expected_voyages": per_cond,
            "actual_families": n_fam,
            "actual_voyages": planned_counts.get(c, 0),
            "pass": (planned_counts.get(c, 0) == per_cond
                     and n_fam == planned_quota.get(c, -1)),
        }
    planned_extra = sorted(set(planned_counts) - exp_cond_set)
    planned_gate = {
        "basis": ("frozen formal_draw_allocation_plan_ood.json blocks quota × "
                  "counts.per_condition_voyages（文件名 block linkage = planned "
                  "generation identity；T-020 裁决 C）"),
        "per_condition": planned_gate_rows,
        "unexpected_planned_conditions": planned_extra,
        "pass": (all(r["pass"] for r in planned_gate_rows.values())
                 and not planned_extra),
    }

    # ---- planned→realized transition/overlap matrix（realized exclusive）----
    transition: dict = {}
    for v in valid_voyages:
        pc_ = v["ev"].get("planned_condition_id", "_unknown")
        rc_ = v["ev"].get("ood_condition_id", "_unknown")
        row = transition.setdefault(pc_, {})
        row[rc_] = row.get(rc_, 0) + 1

    # ---- ledger/meta agreement（classifier 侧由 classify_ood realized 重算
    #      + verdict==OOD 承载；此处验证 producer ledger 与 meta 标签一致）----
    ledger_agree: dict = {"checked": 0, "mismatches": []}
    ledger_path = os.path.join(ood_dir, "_ood_ledger.json")
    try:
        with open(ledger_path, "r", encoding="utf-8") as f:
            lg = json.load(f)
        lg_cond = {str(r.get("voyage_id")): str(r.get("ood_condition_id"))
                   for r in lg.get("voyages", [])}
        for v in valid_voyages:
            vid = v["ev"]["voyage_id"]
            if vid in lg_cond:
                ledger_agree["checked"] += 1
                if lg_cond[vid] != v["ev"]["ood_condition_id"]:
                    ledger_agree["mismatches"].append(
                        {"voyage_id": vid, "ledger": lg_cond[vid],
                         "meta": v["ev"]["ood_condition_id"]})
    except (OSError, ValueError) as e:
        # ledger 文件缺失（如合成测试语料）→ agreement SKIP 不 FAIL；
        # 正式 formal 语料必含 _ood_ledger.json，届时 checked>0 真检验。
        ledger_agree["error"] = f"_ood_ledger.json 不可读: {e!r}"
        ledger_agree["skipped"] = True
    realized_agreement_pass = (ledger_agree.get("skipped", False)
                               or (not ledger_agree.get("mismatches")
                                   and not ledger_agree.get("error")))

    # ---- realized 计数报告（不要求 planned==realized 相等；裁决 C）----
    actual_total = sum(e["actual"] for e in per_condition.values())
    allocation_check = {
        "basis": ("T-020 裁决 C：planned（plan block）为 hard gate；realized "
                  "（DGN-11 + Axis-C 优先独占分类）只报告不强制 40/40 相等"),
        "per_condition": {
            c: {"planned_voyages": planned_counts.get(c, 0),
                "planned_families": len(planned_family_ids.get(c, ())),
                "realized_voyages": per_condition[c]["actual"],
                "planned_pass": planned_gate_rows[c]["pass"]}
            for c in exp_conditions},
        "total": {"planned": sum(planned_counts.values()),
                  "expected_planned": exp_total,
                  "realized": actual_total},
        "transition_planned_to_realized": transition,
        "realized_ledger_meta_agreement": ledger_agree,
        "pass": planned_gate["pass"] and realized_agreement_pass,
    }

    # ---- 汇总 ----
    # allocation 检查按 T-020 裁决 C 拆分：planned allocation 为 hard gate，
    # realized ledger/meta 一致性为 gate；realized 计数本身只报告不 gate。
    checks = {
        "load": not load_errors,
        "structure_validation": not validation_errors,
        "metadata": not metadata_errors,
        "classification": not classification_errors,
        "conditions_universe": not unexpected_condition_ids,
        "planned_allocation_exact": planned_gate["pass"],
        "realized_ledger_meta_agreement": realized_agreement_pass,
        "domain_purity_fr486": stress_block["pass"],
    }
    overall = all(checks.values())

    evidence = {
        "allocation_check": allocation_check,
        "axes": axes,
        "stress_set": stress_block,
        "per_condition": per_condition,
        "overall_pass": overall,
        # FR-485 常量声明（extra keys）
        "ood_records_enter_iid_splitter": OOD_RECORDS_ENTER_IID_SPLITTER,
        "splitter_statement": ood_act.get("splitter"),
        "checks": checks,
        "reasons": reasons,
        "totals": {
            "voyage_files": len(files),
            "records_built": len(all_recs),
            "formal_ood_total": len(formal),
            "expected_total": exp_total,
        },
        "conditions_universe": {
            "source": "formal_fleet.ood.counts.conditions",
            "expected": list(exp_conditions),
            "unexpected_condition_ids": sorted(unexpected_condition_ids),
        },
        "load_errors": load_errors,
        "validation_errors": validation_errors,
        "metadata_errors": metadata_errors,
        "classification_errors": classification_errors,
        "ood_def_version_expected": act_def_version,
        "registry_source": registry_source,
        "voyage_evidence": [v["ev"] for v in valid_voyages],
        "ood_dir": ood_dir,
        "activation_json": activation_json,
    }
    if not overall:
        evidence["reasons"] = reasons + [
            k for k, ok in checks.items() if not ok]
    return evidence

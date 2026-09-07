#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""audit_ood.py — OOD 装载/物理域检查（DGN-11 + design 修订 R4；FR-485/486；T-012）.

唯一事实源：.specs/phase4-d-dataset-v3/design.md（DGN-11）+ requirements.md（FR-485/486）。
T-018 Phase 1 重构（activation record 生效）：
  1) 轴 B 训练支持下界 2.0 → 1.5（T-432 activation record axis_b.
     train_support_min_ht；LOW_EFFECTIVE_DEPTH/LOW_EFFECTIVE_HT 外推域
     1.3 ≤ H/T < 1.5，[1.5, 2.0) 回归数值 IID 支撑，仅由谓词驱动判定）；
  2) 轴 C held-out 组合改由冻结注册表 config/mission_v3/
     heldout_token_registry.json 加载（D4 裁决：canonical token 禁手抄）——
     composition 词表 = registry factor_a/factor_b 名；原手写常量
     AXIS_C_HELD_OUT_COMBINATIONS 与本模块内独立阈值一并移除。

本模块为机制级实现：pilot 期用合成标记数据验证装载器与域检查正确性（FR-489），
不产出任何正式 OOD 科学结论（formal 证据属 T-435-formal）。

OOD 记录结构（冻结）：
  {"voyage_id": str, "ood_axis": "A"|"B"|"C", "ood_def_version": str,
   "physics_domain_ref": str,
   "u_abs_mps": float,      # 轴 A 必备（|U|，m/s）
   "ht_ratio": float,       # 轴 B 必备（H/T）
   "composition": [str, …]} # 轴 C 必备（registry factor 名清单）

物理域判定（机制级，冻结）：
  轴 A：2.0 < |U| ≤ 2.8 → OOD；|U| > 2.8 → PHYSICS_EXTRAPOLATION/STRESS_SET；
        |U| ≤ 2.0 → NONE（训练支持域内）。
  轴 B：1.3 ≤ H/T < 1.5 → OOD；H/T < 1.2 → PHYSICS_EXTRAPOLATION/STRESS_SET；
        1.2 ≤ H/T < 1.3 → UNCLASSIFIED（margin 带报告）；H/T ≥ 1.5 → NONE。
  轴 C：composition 与 registry REQUIRE token 的 {factor_a, factor_b} 组合
        交集非空（子集语义）→ OOD；否则 NONE（各单因子在训练域内）。

预注册语义说明：轴 A 数值上界 2.8（及训练支持上界 2.0）为 design DGN-11 预注册
框架值；T-432 activation record 已确认 counts/seeds 并冻结轴 A 数值与轴 B
1.5 下界（FR-485/R4 + D1/D2 Final Ruling 2026-09-07）——常量命名带
PREREGISTERED 语义以提示其法律地位。
"""

from __future__ import annotations

import json
import os

OOD_AXES = ("A", "B", "C")
VERDICT_OOD = "OOD"
VERDICT_PHYSICS_EXTRAPOLATION = "PHYSICS_EXTRAPOLATION"
VERDICT_UNCLASSIFIED = "UNCLASSIFIED"
VERDICT_NONE = "NONE"

# ---- 轴 A（stronger current；单位 m/s）----
AXIS_A_PREREGISTERED_TRAIN_SUPPORT_ABS_MPS = 2.0   # 训练支持上界（T-432 确认）
AXIS_A_PREREGISTERED_OOD_UPPER_ABS_MPS = 2.8       # OOD 数值上界（DGN-11 预注册框架值，
                                                   # activation record 冻结；变更须 spec amendment）
# ---- 轴 B（lower water level；H/T 无量纲）----
# T-432 activation record axis_b.train_support_min_ht = 1.5（原框架值 2.0；
# D2 Final Ruling：轴 B membership = 1.3 ≤ ht_effective < 1.5，按 DRY 水位
# 实现后的 realized ht_effective 判定；[1.5, 2.0) 数值上属 IID 支撑，
# 仅由 canonical 谓词驱动判定，不自动归入轴 C）
AXIS_B_PREREGISTERED_TRAIN_SUPPORT_MIN_HT = 1.5    # 训练支持下界（activation record 生效值）
AXIS_B_PREREGISTERED_OOD_MIN_HT = 1.3              # OOD 下界（H/T ≥ 1.2 为 Model E 已验证
                                                   # 浅水下界 clamp 域，留 0.1 margin）
AXIS_B_STRESS_BELOW_HT = 1.2                       # H/T < 1.2 → 越物理域（STRESS_SET）

# ---- 轴 C（compositional；canonical token，registry 驱动，D4）----
# 唯一事实源 = config/mission_v3/heldout_token_registry.json（self_hash
# 校验由消费方 validate_contracts/MATLAB 侧另行持有；本模块读文件即校验
# 结构 fail-closed）。composition 词表与 held-out 组合全部从 registry 的
# factor_a / factor_b 派生，禁止手抄（原手写三组合常量已移除）。
HELDOUT_TOKEN_REGISTRY_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "config", "mission_v3", "heldout_token_registry.json",
)


def load_heldout_token_registry(path: str | None = None,
                                ood_action: str = "REQUIRE") -> list[dict]:
    """读取冻结 heldout token registry，返回 REQUIRE token 条目清单。

    结构 fail-closed：缺 token_id / factor_a / factor_b / ood_action /
    predicate / support_band 任一字段即抛 ValueError（禁降级）。
    """
    p = path or HELDOUT_TOKEN_REGISTRY_PATH
    with open(p, "r", encoding="utf-8") as f:
        reg = json.load(f)
    toks = reg.get("tokens")
    if not isinstance(toks, list) or not toks:
        raise ValueError(f"registry tokens 为空或缺失: {p}")
    out: list[dict] = []
    for tk in toks:
        for fld in ("token_id", "factor_a", "factor_b", "ood_action",
                    "predicate", "support_band"):
            if fld not in tk:
                raise ValueError(f"token 缺字段 {fld}: {tk!r}")
        if tk["ood_action"] == ood_action:
            out.append(tk)
    if not out:
        raise ValueError(f"registry 无 {ood_action} token: {p}")
    return out


def held_out_combinations(path: str | None = None) -> tuple[frozenset, ...]:
    """从 registry REQUIRE token 派生 held-out 组合（{factor_a, factor_b}）。"""
    combos = []
    for tk in load_heldout_token_registry(path):
        combos.append(frozenset((tk["factor_a"], tk["factor_b"])))
    return tuple(combos)


def held_out_composition_vocabulary(path: str | None = None) -> frozenset:
    """composition 词表 = registry factor_a/factor_b 名的并集（73 号 OOD
    记录按此词表生成 composition）。"""
    vocab: set = set()
    for tk in load_heldout_token_registry(path):
        vocab.add(tk["factor_a"])
        vocab.add(tk["factor_b"])
    return frozenset(vocab)


def _registry_combinations_cached() -> tuple[frozenset, ...]:
    """模块级缓存（文件冻结；同进程只读一次）。"""
    global _REGISTRY_COMBOS
    try:
        return _REGISTRY_COMBOS
    except NameError:
        _REGISTRY_COMBOS = held_out_combinations()
        return _REGISTRY_COMBOS


COMMON_FIELDS = ("voyage_id", "ood_axis", "ood_def_version", "physics_domain_ref")
AXIS_VALUE_FIELD = {"A": "u_abs_mps", "B": "ht_ratio", "C": "composition"}

__all__ = ["validate_ood_records", "classify_ood", "OOD_AXES",
           "VERDICT_OOD", "VERDICT_PHYSICS_EXTRAPOLATION",
           "VERDICT_UNCLASSIFIED", "VERDICT_NONE",
           "AXIS_A_PREREGISTERED_TRAIN_SUPPORT_ABS_MPS",
           "AXIS_A_PREREGISTERED_OOD_UPPER_ABS_MPS",
           "AXIS_B_PREREGISTERED_TRAIN_SUPPORT_MIN_HT",
           "AXIS_B_PREREGISTERED_OOD_MIN_HT", "AXIS_B_STRESS_BELOW_HT",
           "load_heldout_token_registry", "held_out_combinations",
           "held_out_composition_vocabulary", "HELDOUT_TOKEN_REGISTRY_PATH",
           "AXIS_VALUE_FIELD"]


def _is_num(x) -> bool:
    return isinstance(x, (int, float)) and not isinstance(x, bool)


def validate_ood_records(records: list) -> list:
    """OOD 记录结构校验，返回违规清单（空 = PASS）。

    检查：公共四字段齐备且类型正确；ood_axis ∈ {A,B,C}；轴值字段按轴齐备
    （A: u_abs_mps 数值 / B: ht_ratio 数值 / C: composition 非空 str 列表）；
    轴值字段不得出现在非对应轴记录上；voyage_id 全局唯一。
    """
    errs: list = []
    if not isinstance(records, list):
        return ["records 必须为 list[dict]"]
    seen_vids: set = set()
    for i, rec in enumerate(records):
        tag = f"records[{i}]"
        if not isinstance(rec, dict):
            errs.append(f"{tag} 必须为 dict")
            continue
        vid = rec.get("voyage_id")
        if not isinstance(vid, str) or not vid:
            errs.append(f"{tag} voyage_id 必须为非空字符串")
        elif vid in seen_vids:
            errs.append(f"{tag} voyage_id 重复: {vid!r}")
        else:
            seen_vids.add(vid)
        axis = rec.get("ood_axis")
        if axis not in OOD_AXES:
            errs.append(f"{tag} ood_axis 非法: {axis!r}（须 ∈ {OOD_AXES}）")
            continue
        for fld in COMMON_FIELDS:
            if fld not in rec:
                errs.append(f"{tag} 缺公共字段 {fld}")
            elif fld in ("ood_def_version", "physics_domain_ref") \
                    and (not isinstance(rec[fld], str) or not rec[fld]):
                errs.append(f"{tag} {fld} 必须为非空字符串")
        val_fld = AXIS_VALUE_FIELD[axis]
        val = rec.get(val_fld)
        if axis in ("A", "B"):
            if not _is_num(val):
                errs.append(f"{tag} 轴 {axis} 缺数值字段 {val_fld} 或类型非法: {val!r}")
        else:
            if not isinstance(val, list) or not val \
                    or not all(isinstance(t, str) and t for t in val):
                errs.append(f"{tag} 轴 C composition 必须为非空 list[str]: {val!r}")
        # 轴值字段不得出现在非对应轴记录上（结构防混）
        for ax, fld in AXIS_VALUE_FIELD.items():
            if ax != axis and fld in rec:
                errs.append(f"{tag} 轴值字段 {fld} 属于轴 {ax}，不得出现在轴 {axis} 记录上")
    return errs


def classify_ood(rec: dict) -> dict:
    """单条 OOD 记录的物理域判定（签名向后兼容；轴 C 内部消费 registry）。

    返回 {"voyage_id", "ood_axis", "verdict", "stress_set", "detail"}；
    verdict ∈ {OOD, PHYSICS_EXTRAPOLATION, UNCLASSIFIED, NONE}。
    结构违规由 validate_ood_records 负责；本函数对轴值字段缺失 fail-fast（ValueError）。
    """
    axis = rec.get("ood_axis")
    if axis not in OOD_AXES:
        raise ValueError(f"ood_axis 非法: {axis!r}")
    base = {"voyage_id": rec.get("voyage_id"), "ood_axis": axis,
            "ood_def_version": rec.get("ood_def_version"),
            "stress_set": False}

    if axis == "A":
        u = rec.get("u_abs_mps")
        if not _is_num(u):
            raise ValueError(f"轴 A 记录缺数值字段 u_abs_mps: {rec!r}")
        u = abs(float(u))   # 轴定义在 |U| 上：防御性取模（字段名 u_abs_mps 语义）
        if AXIS_A_PREREGISTERED_TRAIN_SUPPORT_ABS_MPS < u <= AXIS_A_PREREGISTERED_OOD_UPPER_ABS_MPS:
            verdict = VERDICT_OOD
            detail = (f"|U|={u} m/s ∈ ({AXIS_A_PREREGISTERED_TRAIN_SUPPORT_ABS_MPS}, "
                      f"{AXIS_A_PREREGISTERED_OOD_UPPER_ABS_MPS}] → 预注册 OOD 域")
        elif u > AXIS_A_PREREGISTERED_OOD_UPPER_ABS_MPS:
            verdict = VERDICT_PHYSICS_EXTRAPOLATION
            base["stress_set"] = True
            detail = (f"|U|={u} m/s > {AXIS_A_PREREGISTERED_OOD_UPPER_ABS_MPS} m/s → "
                      "越 Model E 物理域，PHYSICS_EXTRAPOLATION/STRESS_SET（不混入 OOD 指标，FR-486）")
        else:
            verdict = VERDICT_NONE
            detail = (f"|U|={u} m/s ≤ {AXIS_A_PREREGISTERED_TRAIN_SUPPORT_ABS_MPS} m/s → 训练支持域内")
        base["u_abs_mps"] = u

    elif axis == "B":
        ht = rec.get("ht_ratio")
        if not _is_num(ht):
            raise ValueError(f"轴 B 记录缺数值字段 ht_ratio: {rec!r}")
        if AXIS_B_PREREGISTERED_OOD_MIN_HT <= ht < AXIS_B_PREREGISTERED_TRAIN_SUPPORT_MIN_HT:
            verdict = VERDICT_OOD
            detail = (f"H/T={ht} ∈ [{AXIS_B_PREREGISTERED_OOD_MIN_HT}, "
                      f"{AXIS_B_PREREGISTERED_TRAIN_SUPPORT_MIN_HT}) → 预注册 OOD 域")
        elif ht < AXIS_B_STRESS_BELOW_HT:
            verdict = VERDICT_PHYSICS_EXTRAPOLATION
            base["stress_set"] = True
            detail = (f"H/T={ht} < {AXIS_B_STRESS_BELOW_HT} → 越物理域，"
                      "PHYSICS_EXTRAPOLATION/STRESS_SET（不混入 OOD 指标，FR-486）")
        elif ht < AXIS_B_PREREGISTERED_OOD_MIN_HT:
            verdict = VERDICT_UNCLASSIFIED
            detail = (f"H/T={ht} ∈ [{AXIS_B_STRESS_BELOW_HT}, {AXIS_B_PREREGISTERED_OOD_MIN_HT}) "
                      "→ margin 带，UNCLASSIFIED（仅报告，不归 OOD 亦不归支持域）")
        else:
            verdict = VERDICT_NONE
            detail = (f"H/T={ht} ≥ {AXIS_B_PREREGISTERED_TRAIN_SUPPORT_MIN_HT} → 训练支持域内")
        base["ht_ratio"] = ht

    else:  # axis == "C"
        comp = rec.get("composition")
        if not isinstance(comp, list):
            raise ValueError(f"轴 C 记录缺 list 字段 composition: {rec!r}")
        tokens = {str(t) for t in comp}
        hit = None
        for combo in _registry_combinations_cached():
            if combo <= tokens:   # 子集语义：组合整体出现在 composition 中
                hit = sorted(combo)
                break
        if hit is not None:
            verdict = VERDICT_OOD
            detail = (f"composition {sorted(tokens)} 含 registry held-out 组合 {hit} "
                      "→ 预注册 OOD 域（canonical token，registry 驱动）")
        else:
            verdict = VERDICT_NONE
            detail = (f"composition {sorted(tokens)} 不含任何 registry held-out 组合 "
                      "→ 训练支持域内（单因子域内）")
        base["composition"] = sorted(tokens)

    base["verdict"] = verdict
    base["detail"] = detail
    return base

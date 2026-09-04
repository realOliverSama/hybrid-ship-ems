#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""split.py — deterministic group-aware split（DGN-09 + design 修订 R2/R6；FR-481/482；T-011）.

唯一事实源：.specs/phase4-d-dataset-v3/design.md（DGN-09）+ requirements.md（FR-481/482）。
本文件数值/规则全部冻结，不得为了让结果"更好看"而调整（任何 split 变更须 spec amendment，
禁止按模型误差反向调整 split——FR-481）。

冻结算法（design R2/R6 落案）：
- assignment key：pilot 期 = voyage_id（字符串形式）；formal 期 = split_group_id。
  本模块接口统一命名 split_group_id。
- u = int(SHA256("{split_seed}|{split_group_id}".encode("utf-8")).hexdigest()[:16], 16) / 2**64
  （64-bit 归一化）。design 公式中的 "‖" 符号按"字面竖线分隔符"落案为 ASCII "|"
  （SEPARATOR 常量）；如 activation 解释为 U+2016，仅需改该常量并重建 artifact。
- group→stratum 必须唯一：同一 group 出现在 ≥2 个 stratum → ValueError（fail-fast）。
- 每 stratum 内：按 u 升序排序（u 相同——工程上不可能但防御——按 group_key 字典序）；
  目标计数 n_tr = half-up(0.7n)、n_va = half-up(0.15n)、n_te = half-up(0.15n)
  （half-up = x+0.5 取 floor，对 0.5 向上）。实现用整数运算
  n_tr=(7n+5)//10、n_va=n_te=(3n+10)//20，与 0.7/0.15 十进制精确值的
  floor(x+0.5) 严格等价，避免浮点噪声。
- 完备性 fix-up：sum<n 时每次给当前计数最小的 split +1（平手按 train→val→test
  优先）；sum>n 时每次给当前计数最大的 split −1（平手按 test→val→train 优先）。
- partition 完备性三不变量（构建路径代码内 assert）：①三个 split 两两不交
  ②并集 = 全部 group ③每 stratum 内分配数与计数规则一致。
  机器可读检查走 verify_partition（违规清单，空 = PASS）。
- artifact：{"schema":"split_assignment_v3", "split_seed":…, "assignment":
  {group:{"split":…,"stratum":…}}, "strata":{stratum:{"train":[…],"val":[…],
  "test":[…]}}, "counts":{…}, "artifact_sha256":…}；
  artifact_sha256 = SHA256(canonical_json(artifact 内容，排除 artifact_sha256 字段))，
  canonical = json.dumps(sort_keys=True, separators=(',',':'), ensure_ascii=False).encode("utf-8")
  （与 validate_contracts.canonical_json 同一配方）。
- 确定性：同输入集合任意顺序 → 同 artifact（先对 group 集合排序再算）；连跑两次 hash 一致。
"""

from __future__ import annotations

import hashlib
import json
import os
from itertools import combinations

from .validate_contracts import canonical_json

SPLIT_SCHEMA = "split_assignment_v3"
SPLITS = ("train", "val", "test")
SEPARATOR = "|"          # DGN-09/R2 字面竖线分隔符（见模块 docstring 说明）
ARTIFACT_HASH_FIELD = "artifact_sha256"

__all__ = ["assign_splits", "write_artifact", "verify_partition",
           "compute_artifact_hash", "target_counts", "fixup_counts",
           "SPLIT_SCHEMA", "SPLITS", "SEPARATOR", "ARTIFACT_HASH_FIELD"]


# ---------------------------------------------------------------------------
# 冻结数值规则
# ---------------------------------------------------------------------------
def group_u(split_seed: str, split_group_id: str) -> float:
    """DGN-09 冻结 hash：u ∈ [0,1)，64-bit 归一化（monkeypatch 此函数可测试 tie-break）。"""
    payload = f"{split_seed}{SEPARATOR}{split_group_id}".encode("utf-8")
    return int(hashlib.sha256(payload).hexdigest()[:16], 16) / 2**64


def target_counts(n: int) -> dict:
    """half-up(0.7n)/half-up(0.15n)/half-up(0.15n)（整数运算，与 floor(x+0.5) 精确等价）。"""
    if n < 0:
        raise ValueError(f"stratum 大小非法: n={n}")
    n_tr = (7 * n + 5) // 10      # floor(0.7n + 0.5)
    n_va = (3 * n + 10) // 20     # floor(0.15n + 0.5)
    return {"train": n_tr, "val": n_va, "test": n_va}


def fixup_counts(counts: dict, n: int) -> dict:
    """完备性 fix-up（冻结）：sum<n 时每次给当前计数最小的 split +1（平手按
    train→val→test 优先）；sum>n 时每次给当前计数最大的 split −1（平手按
    test→val→train 优先）。counts 键须为 SPLITS；n 为 stratum 规模（显式传入，
    不从 counts 之和反推——half-up 目标和可能 <n 或 >n）。"""
    out = {s: int(counts[s]) for s in SPLITS}
    while sum(out.values()) < n:
        best = min(SPLITS, key=lambda s: out[s])          # 平手按 SPLITS 顺序 = train→val→test
        out[best] += 1
    while sum(out.values()) > n:
        best = max(("test", "val", "train"), key=lambda s: out[s])  # 平手 test→val→train
        out[best] -= 1
    return out


# ---------------------------------------------------------------------------
# assign_splits
# ---------------------------------------------------------------------------
def assign_splits(groups: list, strata: dict, split_seed: str) -> dict:
    """确定性 group-aware split（DGN-09）。

    参数
    ----
    groups : list[str]          全部 split_group_id（pilot 期 = voyage_id 字符串）。
    strata : dict[str, list[str]]  预声明 strata（scenario family）→ 成员 group 列表。
                                每个 group 必须恰属一个 stratum（违反 → ValueError）。
    split_seed : str            split seed（registry/manifest 冻结字段）。

    返回
    ----
    dict：完整 assignment artifact（含 assignment / strata 视图 / counts /
    artifact_sha256），可直接交给 write_artifact 落盘。
    """
    if not isinstance(split_seed, str) or not split_seed:
        raise ValueError("split_seed 必须为非空字符串")
    if not isinstance(groups, (list, tuple)):
        raise ValueError("groups 必须为 list[str]")
    all_groups = [str(g) for g in groups]
    if len(set(all_groups)) != len(all_groups):
        raise ValueError("groups 列表含重复 split_group_id（fail-fast）")
    if not isinstance(strata, dict) or not strata:
        raise ValueError("strata 必须为非空 dict[str, list[str]]")

    # group→stratum 唯一性（fail-fast）：跨 stratum 重复 / 同 stratum 重复 / 缺失 / 多余
    owner: dict = {}
    for stratum, members in strata.items():
        for g in members:
            g = str(g)
            if g in owner:
                if owner[g] == stratum:
                    raise ValueError(f"group {g!r} 在 stratum {stratum!r} 内重复出现（fail-fast）")
                raise ValueError(
                    f"group {g!r} 同时出现在 stratum {owner[g]!r} 与 {stratum!r}（group→stratum 必须唯一，fail-fast）")
            owner[g] = stratum
    missing = sorted(set(all_groups) - set(owner))
    extra = sorted(set(owner) - set(all_groups))
    if missing:
        raise ValueError(f"以下 group 未分配到任何 stratum: {missing}")
    if extra:
        raise ValueError(f"strata 含 groups 之外的 group: {extra}")

    # 逐 stratum：u 升序（平手字典序）→ 冻结计数规则 + fix-up → 顺序切分
    assignment: dict = {}
    strata_view: dict = {}
    for stratum in sorted(strata):
        members = sorted(strata[stratum])
        ranked = sorted(members, key=lambda g: (group_u(split_seed, g), g))
        counts = fixup_counts(target_counts(len(ranked)), len(ranked))
        i_tr = counts["train"]
        i_va = i_tr + counts["val"]
        view = {
            "train": sorted(ranked[:i_tr]),
            "val": sorted(ranked[i_tr:i_va]),
            "test": sorted(ranked[i_va:]),
        }
        strata_view[stratum] = view
        for s in SPLITS:
            for g in view[s]:
                assignment[g] = {"split": s, "stratum": stratum}

    # partition 完备性三不变量（构建路径 assert；机器可读检查用 verify_partition）
    split_groups = {s: {g for g in assignment if assignment[g]["split"] == s} for s in SPLITS}
    # ① 两两不交
    for a, b in combinations(SPLITS, 2):
        assert not (split_groups[a] & split_groups[b]), f"invariant ① 违反: {a}/{b} 交集非空"
    # ② 并集 = 全部 group
    union = set()
    for s in SPLITS:
        union |= split_groups[s]
    assert union == set(all_groups), "invariant ② 违反: split 并集 != 全部 group"
    # ③ 每 stratum 计数与冻结规则一致
    for stratum, view in strata_view.items():
        n_st = len(strata[stratum])
        expect = fixup_counts(target_counts(n_st), n_st)
        got = {s: len(view[s]) for s in SPLITS}
        assert got == expect, f"invariant ③ 违反: stratum {stratum!r} 计数 {got} != 规则 {expect}"

    total = {s: len(split_groups[s]) for s in SPLITS}
    artifact = {
        "schema": SPLIT_SCHEMA,
        "split_seed": split_seed,
        "assignment": {g: assignment[g] for g in sorted(assignment)},
        "strata": {st: {s: strata_view[st][s] for s in SPLITS} for st in sorted(strata_view)},
        "counts": total,
    }
    artifact[ARTIFACT_HASH_FIELD] = compute_artifact_hash(artifact)
    return artifact


def compute_artifact_hash(artifact: dict) -> str:
    """SHA256(canonical_json(artifact 内容，排除 artifact_sha256 字段))。"""
    body = {k: v for k, v in artifact.items() if k != ARTIFACT_HASH_FIELD}
    return hashlib.sha256(canonical_json(body)).hexdigest()


def write_artifact(assignment: dict, path: str) -> None:
    """assignment artifact 落盘（JSON，sort_keys，utf-8，确定性字节序）。"""
    d = os.path.dirname(os.path.abspath(path))
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(assignment, f, sort_keys=True, ensure_ascii=False, indent=2)
        f.write("\n")


# ---------------------------------------------------------------------------
# verify_partition（机器可读检查）
# ---------------------------------------------------------------------------
def verify_partition(artifact: dict) -> list:
    """partition 完备性检查，返回违规清单（空 = PASS）。

    检查：schema / split 值域 / 三不变量（两两不交、并集=全部、strata 视图与
    assignment 双向一致）/ 每 stratum 计数与冻结规则一致 / artifact_sha256 自洽。
    """
    errs: list = []

    def err(msg):
        errs.append(msg)

    if not isinstance(artifact, dict):
        return ["artifact 必须为 dict"]
    if artifact.get("schema") != SPLIT_SCHEMA:
        err(f"schema 必须为 {SPLIT_SCHEMA}，实际 {artifact.get('schema')!r}")

    assignment = artifact.get("assignment")
    strata_view = artifact.get("strata")
    if not isinstance(assignment, dict) or not assignment:
        err("assignment 必须为非空 dict")
        return errs
    if not isinstance(strata_view, dict) or not strata_view:
        err("strata 必须为非空 dict")
        return errs

    # split 值域 + 记录结构
    owner: dict = {}
    for g, info in assignment.items():
        if not isinstance(info, dict) or info.get("split") not in SPLITS \
                or not isinstance(info.get("stratum"), str):
            err(f"assignment[{g!r}] 结构非法: {info!r}（split 须 ∈ {SPLITS}，stratum 须为 str）")
            continue
        owner[g] = info

    # strata 视图 ↔ assignment 双向一致（① 不交 + ② 完备在此一并强制）
    seen: dict = {}
    for stratum, view in strata_view.items():
        if not isinstance(view, dict):
            err(f"strata[{stratum!r}] 必须为 dict")
            continue
        for s in SPLITS:
            members = view.get(s)
            if not isinstance(members, list):
                err(f"strata[{stratum!r}][{s!r}] 必须为 list")
                continue
            for g in members:
                if g in seen:
                    err(f"group {g!r} 在 strata 视图中出现多次（{seen[g]} 与 {stratum}/{s}）——违反不变量①/②")
                else:
                    seen[g] = (stratum, s)
                info = owner.get(g)
                if info is None:
                    err(f"group {g!r} 在 strata 视图中存在但 assignment 缺失")
                elif info.get("split") != s or info.get("stratum") != stratum:
                    err(f"group {g!r} split/stratum 不一致: assignment={info.get('split')!r}/"
                        f"{info.get('stratum')!r} vs strata 视图={s!r}/{stratum!r}")
    for g, info in owner.items():
        if g not in seen:
            err(f"group {g!r} 在 assignment 中存在但 strata 视图缺失——违反不变量②")

    # ③ 每 stratum 计数与冻结规则一致（stratum 的 n 由视图三 split 计数之和反推）
    for stratum, view in strata_view.items():
        if not isinstance(view, dict):
            continue
        sizes = {s: len(view.get(s) or []) for s in SPLITS}
        n = sum(sizes.values())
        expect = fixup_counts(target_counts(n), n)
        if sizes != expect:
            err(f"stratum {stratum!r} 计数 {sizes} != 冻结规则 {expect}（不变量③）")

    # artifact_sha256 自洽（缺失即报错——FR-482 assignment artifact 必带 hash，
    # 删字段不得绕过校验；R2 审查 m-1 闭合）
    got_hash = artifact.get(ARTIFACT_HASH_FIELD)
    if got_hash is None:
        err(f"artifact 缺 {ARTIFACT_HASH_FIELD}（FR-482：assignment artifact 必带 hash）")
    else:
        want = compute_artifact_hash(artifact)
        if got_hash != want:
            err(f"artifact_sha256 不匹配: 文件={got_hash} 计算={want}")
    return errs

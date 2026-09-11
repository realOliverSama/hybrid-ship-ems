#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""audit_leakage.py — 三级泄漏审计 L1/L2/L3（DGN-10 + design 修订 R3；FR-484；T-012）.

唯一事实源：.specs/phase4-d-dataset-v3/design.md（DGN-10 + R3）+ requirements.md（FR-484）。

L1（Hard FAIL，任一触发 → overall_l1=false + 违规清单）：
  ① 任一 voyage_id 或 split_group_id 跨越 >1 个 split。
     记录键约定：split_of 的 key 为恒等记录键，形如 "<identity>" 或
     "<identity>#<子记录>"（如窗口/段级行）；恒等 identity = 首个 "#" 之前的主键
     （= voyage_id 或 split_group_id）。同一 identity 映射到多个不同 split → 违规。
  ② raw voyage artifact SHA256 重复（不同 voyage_id 共享同一 artifact hash）。
  ③ 窗口级 exact duplicate：窗口 content hash 重复。content hash 由
     window_content_hash() 计算——SHA256 over canonical 序列化（float64 tobytes，
     逐列固定列序）of (X_H 全部 H1/H2 列 [300×F] + X_KF 全部 K/F 列 [30×F] + Y [30])；
     metadata identity（voyage_id 等）不参与 content hash。

L2（provenance-gated，禁自动剔除——只报告，绝不在返回值外删除/修改数据）：
  对象 = 每 voyage 的 1Hz time_mean P_load 序列；同长度且 max|Δ| < 1e-6 kW
  → L2_candidate。ε=1e-6 kW 仅是 numerical near-identical candidate 阈值，
  不是物理相似阈值（冻结表述，禁止解释为物理相似）。分类仅当 provenance 支持：
  同 generation_seed 且同 route_template_id → CONFIRMED_DUPLICATE；
  provenance 不同 → LEGITIMATE_NEAR_REPLICATE；provenance 缺失 → UNRESOLVED。

L3（SBD Suspect Flag，similarity ≠ leakage：永不产生 FAIL，只产 flag）：
  表征 = 每 voyage 的 1Hz time_mean P_load 序列 → 线性重采样 256 点 → z-score
  （统计量用 train split；无 train 统计输入时用全体并在结果中声明 fallback）。
  距离 SBD = 1 − max NCC（Paparrizos & Gravano 2015 语义，与本工作区 Phase 2.5
  audit_kshape_v2.py 同方法语义独立实现，禁止跨工作区 import）。
  阈值 policy 冻结为 T_SBD = Q0.01(train-vs-train 最近邻 SBD 距离分布)
  （policy 冻结、数值由输入计算）。跨 split 对 SBD < T_SBD →
  suspected_near_neighbor 标记（pair/distance/disposition="unresolved"）。
"""

from __future__ import annotations

import hashlib

import numpy as np

from .split import SPLITS

# ---- 冻结常数（DGN-10/R3） ----
L2_EPSILON_KW = 1e-6          # numerical near-identical candidate 阈值（非物理相似阈值）
SBD_NPTS = 256                # L3 表征线性重采样点数
SBD_THRESHOLD_POLICY = "T_SBD = Q0.01(train-vs-train NN SBD distance distribution)"
L3_DISPOSITION_UNRESOLVED = "unresolved"
NUMERIC_FLOOR = 1e-12         # 零方差/零范数防御（与 Phase 2.5 同语义）

# L1 恒等记录键的子记录分隔符："<identity>#<sub>" → identity = "#" 前主键
IDENTITY_SEP = "#"

# L2 provenance 判定所需字段（同 generation_seed 且同 route_template_id → confirmed）
L2_PROVENANCE_KEYS = ("generation_seed", "route_template_id")

__all__ = ["run_l1", "run_l2", "run_l3", "window_content_hash",
           "run_family_sibling_audit", "L2_CLASS_FAMILY_SIBLING",
           "L2_EPSILON_KW", "SBD_NPTS", "SBD_THRESHOLD_POLICY",
           "L2_PROVENANCE_KEYS", "IDENTITY_SEP"]


# ---------------------------------------------------------------------------
# L1
# ---------------------------------------------------------------------------
def window_content_hash(x_h, x_kf, y) -> str:
    """窗口级 exact-duplicate content hash（DGN-10/R3 冻结配方）。

    SHA256 over canonical 序列化：按 (X_H, X_KF, Y) 固定次序，各数组转 float64
    C-contiguous 后 tobytes，并前置 "<名称>:<shape>" 头消除形状歧义。列序由调用方
    按 registry 冻结列序排列（本函数不重排——"逐列固定列序"是调用侧契约）。
    metadata identity（voyage_id 等）不参与——本函数只接收科学载荷数组。
    """
    h = hashlib.sha256()
    for name, arr in (("X_H", x_h), ("X_KF", x_kf), ("Y", y)):
        a = np.ascontiguousarray(np.asarray(arr), dtype=np.float64)
        h.update(f"{name}:{a.shape}:".encode("utf-8"))
        h.update(a.tobytes())
    return h.hexdigest()


def identity_of(record_key: str) -> str:
    """恒等主键："<identity>#<sub>" → identity（无分隔符则整键即 identity）。

    T-020 裁决 A（2026-09-12）：canonical split_group_id 自含 '#'（`SG#<hash>`），
    故恒等键形如 `SG#<hash>#<sub>` / `SG#<hash>`。首段切分（split('#',1)[0]）会把
    `SG#<hash>` 误降为 "SG" 使全部 group 折叠成同一身份——改为结构化解析：
    以 `SG#` 为 group 身份前缀时恒等主键 = 前两段；其余键（如 voyage 级
    `FLTA_..._M1#<win>`）恒等主键 = 首个 '#' 之前。canonical `split_group_id`
    值本身不做任何改写。
    """
    if record_key.startswith("SG" + IDENTITY_SEP):
        parts = record_key.split(IDENTITY_SEP)
        return IDENTITY_SEP.join(parts[:2]) if len(parts) > 2 else record_key
    return record_key.split(IDENTITY_SEP, 1)[0]


def run_l1(split_of: dict, artifact_hashes: dict, window_hashes: dict,
           window_group_of: dict | None = None) -> dict:
    """L1 provenance/identity 泄漏审计（Hard FAIL 级）。

    split_of: 恒等记录键 → split；artifact_hashes: voyage_id → raw artifact SHA256；
    window_hashes: 窗口键 → 窗口 content hash（window_content_hash 计算）。
    window_group_of（T-020 裁决 B/C 同源适配，2026-09-12）：{voyage_id:
    split_group_id}。提供时，同 split_group 内的窗口 exact-duplicate 对（同
    family 声明式 sibling 在稳态/停机窗口的物理重合）不判 L1-Hard，转入
    family_sibling_window_duplicates 报告（交 family-sibling 一致性审计语义
    承载）；跨 split_group 的窗口重复维持 L1-Hard。
    返回 {"overall_l1": bool, "violations": [...], ...}；overall_l1=false 即 Hard FAIL。
    """
    violations: list = []

    # ① identity 跨 split（含 split 值域检查）
    identity_splits: dict = {}
    invalid_values: list = []
    for key in sorted(split_of):
        val = split_of[key]
        if val not in SPLITS:
            invalid_values.append((key, val))
            continue
        identity_splits.setdefault(identity_of(key), set()).add(val)
    for key, val in invalid_values:
        violations.append({"type": "invalid_split_value", "record": key, "value": val,
                           "message": f"记录 {key!r} 的 split 值非法: {val!r}（须 ∈ {SPLITS}）"})
    for ident in sorted(identity_splits):
        splits = identity_splits[ident]
        if len(splits) > 1:
            violations.append({
                "type": "identity_cross_split", "identity": ident,
                "splits": sorted(splits),
                "message": f"identity {ident!r} 跨越 >1 个 split: {sorted(splits)}（L1-Hard）"})

    # ② raw voyage artifact SHA256 重复
    artifacts_by_hash: dict = {}
    for vid in sorted(artifact_hashes):
        artifacts_by_hash.setdefault(artifact_hashes[vid], []).append(vid)
    for h in sorted(artifacts_by_hash):
        vids = artifacts_by_hash[h]
        if len(vids) > 1:
            violations.append({
                "type": "duplicate_artifact_sha256", "sha256": h, "voyage_ids": vids,
                "message": f"raw voyage artifact SHA256 重复: {vids}（L1-Hard）"})

    # ③ 窗口级 exact duplicate（content hash 相同的不同窗口；同 split_group
    #    内的对按裁决 B/C 转独立报告，不判 Hard——family sibling 的 truth
    #    sharing 与稳态物理重合由 family-sibling 审计承载）
    windows_by_hash: dict = {}
    for wkey in sorted(window_hashes):
        windows_by_hash.setdefault(window_hashes[wkey], []).append(wkey)
    family_window_duplicates: list = []
    for h in sorted(windows_by_hash):
        wins = windows_by_hash[h]
        if len(wins) <= 1:
            continue
        same_group = window_group_of is not None and all(
            window_group_of.get(identity_of(w)) is not None
            and window_group_of.get(identity_of(w))
            == window_group_of.get(identity_of(wins[0]))
            for w in wins[1:])
        if same_group:
            family_window_duplicates.append({"sha256": h, "windows": wins})
            continue
        violations.append({
            "type": "duplicate_window_content", "sha256": h, "windows": wins,
            "message": f"窗口级 exact duplicate（content hash {h[:16]}…）: {wins}（L1-Hard）"})

    return {
        "level": "L1",
        "overall_l1": len(violations) == 0,
        "n_violations": len(violations),
        "violations": violations,
        "n_identities": len(identity_splits),
        "n_voyage_artifacts": len(artifact_hashes),
        "n_windows": len(window_hashes),
        "family_sibling_window_duplicates": family_window_duplicates,
        "group_aware": window_group_of is not None,
    }


# ---------------------------------------------------------------------------
# L2
# ---------------------------------------------------------------------------
L2_CLASS_FAMILY_SIBLING = "DECLARED_FAMILY_SIBLING"


def run_l2(series: dict, provenance: dict, group_of: dict | None = None) -> dict:
    """L2 numerical near-exact duplication 审计（provenance-gated，报告-only）。

    series: {voyage_id: 1Hz time_mean P_load 序列}；provenance: {voyage_id: dict}，
    判定字段 = generation_seed + route_template_id（缺失 → UNRESOLVED）。
    只报告，绝不删除/修改数据（返回值之外零副作用）。

    group_of（T-020 裁决 B，2026-09-12）：{voyage_id: split_group_id}。提供时，
    同 split_group 内的 provenance 撞车对不再是 CONFIRMED_DUPLICATE（family
    closure 内共享 seed/route/target 是 AMENDMENT_03 声明设计），改分类为
    DECLARED_FAMILY_SIBLING 并交给 run_family_sibling_audit 做族内不变量审计；
    仅跨独立 split group 的撞车维持 CONFIRMED_DUPLICATE（leakage FAIL 级）。
    """
    vids = sorted(series)
    candidates: list = []
    family_sibling_pairs: list = []
    n_pairs = 0
    for i in range(len(vids)):
        for j in range(i + 1, len(vids)):
            a, b = vids[i], vids[j]
            n_pairs += 1
            sa = np.asarray(series[a], dtype=float).ravel()
            sb = np.asarray(series[b], dtype=float).ravel()
            if sa.size != sb.size:
                continue
            max_abs_diff = float(np.max(np.abs(sa - sb))) if sa.size else 0.0
            if not (max_abs_diff < L2_EPSILON_KW):
                continue
            pa = provenance.get(a) or {}
            pb = provenance.get(b) or {}
            missing = [k for k in L2_PROVENANCE_KEYS if k not in pa or k not in pb]
            same_group = (group_of is not None
                          and group_of.get(a) is not None
                          and group_of.get(a) == group_of.get(b))
            if missing:
                classification = "UNRESOLVED"
                reason = f"provenance 缺失字段 {missing}（无法机械分类，人工处置）"
            elif same_group:
                classification = L2_CLASS_FAMILY_SIBLING
                reason = ("同 split_group（frozen family closure）内 sibling 对："
                          "seed/route/target 共享为 AMENDMENT_03 声明设计，不构成跨样本"
                          "重复证据；转 run_family_sibling_audit 族内不变量审计"
                          "（T-020 裁决 B）")
                family_sibling_pairs.append((a, b))
            elif (pa["generation_seed"] == pb["generation_seed"]
                  and pa["route_template_id"] == pb["route_template_id"]):
                classification = "CONFIRMED_DUPLICATE"
                reason = ("跨独立 split_group 的同 generation_seed 且同 route_template_id"
                          "（provenance 支持重复生成判断；须按预声明 retry/disposition "
                          "policy 处置，禁静默删除）")
            else:
                classification = "LEGITIMATE_NEAR_REPLICATE"
                reason = "provenance 不同（合法近似复现，numerical 近同不构成重复证据）"
            candidates.append({
                "pair": [a, b],
                "n_points": int(sa.size),
                "max_abs_diff_kw": max_abs_diff,
                "classification": classification,
                "reason": reason,
                "provenance": {a: {k: pa.get(k) for k in L2_PROVENANCE_KEYS},
                               b: {k: pb.get(k) for k in L2_PROVENANCE_KEYS}},
            })
    counts = {"l2_candidate": len(candidates), "CONFIRMED_DUPLICATE": 0,
              "DECLARED_FAMILY_SIBLING": 0,
              "LEGITIMATE_NEAR_REPLICATE": 0, "UNRESOLVED": 0}
    for c in candidates:
        counts[c["classification"]] += 1
    return {
        "level": "L2",
        "epsilon_kw": L2_EPSILON_KW,
        "epsilon_semantics": ("ε=1e-6 kW 仅为 numerical near-identical candidate 阈值，"
                              "不是物理相似阈值（DGN-10/R3 冻结表述）"),
        "report_only": True,
        "group_aware": group_of is not None,
        "note": ("L2 禁自动剔除：CONFIRMED_DUPLICATE 在 formal Stage B 使对应 preregistered "
                 "slot 失效并按预声明 retry/disposition policy 处置；本函数零数据副作用"),
        "n_pairs_compared": n_pairs,
        "counts": counts,
        "candidates": candidates,
        "family_sibling_pairs": [list(p) for p in family_sibling_pairs],
    }


def run_family_sibling_audit(sibling_pairs: list, series: dict,
                             member_of: dict | None = None) -> dict:
    """Family-sibling 一致性审计（T-020 裁决 B，2026-09-12）。

    对 run_l2 归类为 DECLARED_FAMILY_SIBLING 的 pair（同 split_group 内的
    member 对）验证 frozen intervention lineage 的族内不变量：

    ① information-intervention truth sharing：同 family 各 member 的 target
       序列须逐点一致（同 draw 的负载需求真值共享；max_abs_diff == 0）；
    ② intervention lineage 可分：family 内 member_id 互异（member_of 提供
       voyage_id → member_id；未提供时跳过并记 skipped）。

    返回 {"overall": bool, "families": {...}, "violations": [...]}——仅审计，
    零数据副作用；不做任何剔除。
    """
    violations: list = []
    fam_members: dict = {}
    for a, b in sibling_pairs:
        fam_members.setdefault(a, set()).add(b)
        fam_members.setdefault(b, set()).add(a)
    # 连通分量 = family
    families: dict = {}
    for seed in sorted(fam_members):
        if any(seed in f for f in families.values()):
            continue
        seen, stack = set(), [seed]
        while stack:
            cur = stack.pop()
            if cur in seen:
                continue
            seen.add(cur)
            stack.extend(fam_members.get(cur, ()))
        fam_id = f"FAM#{sorted(seen)[0]}"
        families[fam_id] = sorted(seen)

    fam_report: dict = {}
    for fam_id in sorted(families):
        members = families[fam_id]
        rep = members[0]
        s_rep = np.asarray(series[rep], dtype=float).ravel()
        entry: dict = {"members": members}
        diffs = {}
        for m in members[1:]:
            s_m = np.asarray(series[m], dtype=float).ravel()
            d = float(np.max(np.abs(s_rep - s_m))) if s_m.size == s_rep.size \
                and s_m.size else float("inf")
            diffs[m] = d
        entry["truth_share_max_abs_diff"] = max(diffs.values(), default=0.0)
        entry["truth_shared"] = all(d == 0.0 for d in diffs.values())
        if not entry["truth_shared"]:
            violations.append({
                "family": fam_id, "type": "truth_sharing_violation",
                "detail": {m: diffs[m] for m in diffs},
                "message": f"family {fam_id} 成员 target 未逐点共享真值（L2 sibling 不变量①）"})
        if member_of is not None:
            mids = sorted(str(member_of.get(m)) for m in members)
            entry["member_ids"] = mids
            entry["lineage_distinct"] = len(set(mids)) == len(mids)
            if not entry["lineage_distinct"]:
                violations.append({
                    "family": fam_id, "type": "lineage_not_distinct",
                    "member_ids": mids,
                    "message": f"family {fam_id} member_id 重复（intervention lineage 不可分）"})
        else:
            entry["lineage_distinct"] = None
        fam_report[fam_id] = entry

    checked = [f for f in fam_report.values() if f["lineage_distinct"] is not None]
    return {
        "audit": "family_sibling_consistency",
        "overall": not violations,
        "n_pairs": len(sibling_pairs),
        "n_families": len(families),
        "n_families_full_check": len(checked),
        "families": fam_report,
        "violations": violations,
        "note": ("声明式 family sibling（同 split_group 成员）按 frozen intervention "
                 "lineage 验证真值共享与 member 可分性；记录-only，零数据副作用"),
    }


# ---------------------------------------------------------------------------
# L3（SBD Suspect Flag）
# ---------------------------------------------------------------------------
def _resample_linear(x, npts: int = SBD_NPTS) -> np.ndarray:
    """线性重采样到 npts 点（时间轴归一化，形状对齐用；<2 点退化为全零）。"""
    x = np.asarray(x, dtype=float).ravel()
    if x.size < 2:
        return np.zeros(npts)
    return np.interp(np.linspace(0.0, x.size - 1.0, npts),
                     np.arange(x.size, dtype=float), x)


def _sbd(x: np.ndarray, y: np.ndarray) -> float:
    """Shape-based distance = 1 − max NCC（Paparrizos & Gravano 2015）。

    零填充 2n 的 FFT 互相关覆盖全部平移量 s∈[-(n-1), n-1]；x/y 须为等长 z 标准化
    表征。与 Phase 2.5 audit_kshape_v2.py 同方法语义的独立实现（零范数防御：
    denom < 1e-12 → 距离 1.0）。
    """
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    n = x.size
    denom = float(np.linalg.norm(x) * np.linalg.norm(y))
    if denom < NUMERIC_FLOOR:
        return 1.0
    ncc = np.fft.ifft(np.fft.fft(x, 2 * n) * np.conj(np.fft.fft(y, 2 * n))).real / denom
    return float(1.0 - ncc.max())


def run_l3(series: dict, split_of: dict, train_stats: dict = None,
           provenance: dict = None, require_train_stats: bool = False) -> dict:
    """L3 SBD 跨 split 近邻 Suspect Flag（永不 FAIL，只产 flag）。

    series: {voyage_id: 1Hz time_mean P_load 序列}；split_of: {voyage_id: split}
    （须覆盖 series 全部键，违反 → ValueError fail-fast）。
    train_stats: {"mean": float, "std": float}（train split 统计）；None 时用全体
    序列 pooled 统计并在结果中声明 population_fallback_declared；
    require_train_stats=True（formal 路径）时 None → ValueError fail-fast（R2 m-3）。
    provenance: {voyage_id: dict}（可选；提供时逐 suspect 记录附带 pair 双方
    provenance——DGN-10 冻结 pair/distance/provenance/disposition 入 manifest，
    R2 审查 m-2 闭合）。

    阈值 policy 冻结：T_SBD = Q0.01(train-vs-train 最近邻 SBD 距离分布)；
    数值由输入计算（train voyage < 2 时分布为空 → t_sbd=None，不产 flag）。
    """
    missing = sorted(set(series) - set(split_of))
    if missing:
        raise ValueError(f"split_of 未覆盖以下 voyage: {missing}（fail-fast）")
    if require_train_stats and train_stats is None:
        raise ValueError("formal 路径必须提供 train_stats（z-score 用 train 统计，DGN-10）")
    vids = sorted(series)
    resampled = {v: _resample_linear(series[v]) for v in vids}

    # z-score 统计来源：train_stats 输入优先；否则全体 pooled + 声明 fallback
    if train_stats is not None:
        try:
            mean, std = float(train_stats["mean"]), float(train_stats["std"])
        except (KeyError, TypeError) as exc:
            raise ValueError(f"train_stats 须含数值字段 mean/std: {exc}") from exc
        stats_source = "train_stats"
    else:
        pooled = np.concatenate([resampled[v] for v in vids]) if vids else np.zeros(1)
        mean, std = float(pooled.mean()), float(pooled.std())
        stats_source = "population_fallback_declared"

    def _z(v: str) -> np.ndarray:
        if std < NUMERIC_FLOOR:
            return np.zeros_like(resampled[v])
        return (resampled[v] - mean) / std

    z = {v: _z(v) for v in vids}
    train_vids = [v for v in vids if split_of[v] == "train"]

    # T_SBD = Q0.01(train-vs-train NN SBD 分布)（policy 冻结，数值由输入计算）
    nn_dists = []
    for a in train_vids:
        dists = [_sbd(z[a], z[b]) for b in train_vids if b != a]
        if dists:
            nn_dists.append(min(dists))
    t_sbd = float(np.percentile(nn_dists, 1.0)) if nn_dists else None

    # 跨 split 对扫描（sorted 确定性）；SBD < T_SBD → suspected_near_neighbor
    suspects: list = []
    n_cross = 0
    for i in range(len(vids)):
        for j in range(i + 1, len(vids)):
            a, b = vids[i], vids[j]
            if split_of[a] == split_of[b]:
                continue
            n_cross += 1
            if t_sbd is None:
                continue
            d = _sbd(z[a], z[b])
            if d < t_sbd:
                suspects.append({
                    "pair": [a, b],
                    "splits": [split_of[a], split_of[b]],
                    "distance": d,
                    "disposition": L3_DISPOSITION_UNRESOLVED,
                    "provenance": {a: provenance.get(a) if provenance else None,
                                   b: provenance.get(b) if provenance else None},
                })

    return {
        "level": "L3",
        "policy": SBD_THRESHOLD_POLICY,
        "stats_source": stats_source,
        "mean": mean,
        "std": std,
        "t_sbd": t_sbd,
        "n_voyages": len(vids),
        "n_train": len(train_vids),
        "n_cross_split_pairs": n_cross,
        "suspects": suspects,
        "n_suspects": len(suspects),
        "flag_only": True,
        "note": ("similarity ≠ leakage（NFR-424）：L3 只产 suspected_near_neighbor flag"
                 "（disposition=unresolved），不自动删数据、不单独判泄漏、不构成 Gate FAIL"),
    }

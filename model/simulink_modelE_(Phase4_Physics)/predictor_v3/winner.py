# -*- coding: utf-8 -*-
"""winner.py — val-only Winner 选择（T-017；DGN-E-13 逐行实现；FR-604/605）。

输入：五候选 × seeds{0,1,2} 的 val 逐 split_group 指标（pinball、MAE(q50)、CE80）
+ 参数量。执行顺序（DGN-E-13 冻结伪代码）：

 1. 每候选每指标按 DGN-E-12 聚合：M_c（split_group_macro，per-group seed-median）
 2. S0 = 全候选；best = argmin M_c(pinball)
 3. S1 = {c ∈ S0 : δ_rel(pinball; c vs best) CI ⊆ [−1%,+1%]}
    （δ_rel 分母为重采样参照指标；分母非正/未定义 ⇒ UNDEFINED 保守不判等价）
 4. |S1|==1 → winner=best
 5. S2 = {c ∈ S1 : δ_rel(MAE; c vs argmin_{S1} MAE) CI ⊆ 带}；|S2|==1 → argmin MAE
 6. bestCE = argmin_{S2} |CE80|；S3 = {c ∈ S2 : CI(Δ_CE80 有符号差) ⊆ [−0.01,+0.01]}
 7. |S3|==1 → S2 中 |CE80| 最小者
 8. winner = S3 中参数量最小者；并列 → model_id 字典序（行政平局）
 9. WINNER_SELECTED_PRETERMINAL（身份/候选集/val 证据 hash/算法版本/时间戳/commit）

不变量：只消费 val 证据；执行后 test/OOD 方可解封（T-018 六核验以本证据 hash
为前置）。多种子聚合（obligation 9）：per-split_group seed median → bootstrap；
种子非独立重复单元；禁 best-seed。
"""
from __future__ import annotations

import hashlib
import json
import os
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
CONFIRM_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "confirm")
WINNER_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "winner")
CANDIDATES_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3", "candidates")
ALGO_VERSION = "dgn-e-13-v1"

ARCH_ORDER = ("mlpq", "tcnq", "transformer", "tft", "reatt")

from predictor_v3 import registry as reg  # noqa: E402
from predictor_v3.inference_stats import (EQUIVALENCE_BAND,  # noqa: E402
                                          in_equivalence_band,
                                          median_over_seeds,
                                          paired_bootstrap_diff,
                                          paired_bootstrap_rel_diff)
from predictor_v3.quantile import pinball_numpy  # noqa: E402


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_candidate_groups(arch: str, metric: str = "pinball",
                          confirm_dir: str | None = None) -> tuple:
    """读取候选的 per-seed × per-split_group 指标 → [G]（seed median，obligation 9）。

    metric="pinball" 读 per_group pinball；"mae"/"ce80" 由调用方传入对应的
    per_group 文件（e300/CE80 由 metrics 栈在 confirm 证据生成期一并写盘）。
    返回 (groups 排序列表, vals [G])。
    """
    cdir = confirm_dir or CONFIRM_DIR
    per_seed = {}
    for s in (0, 1, 2):
        p = os.path.join(cdir, arch, f"s{s}_per_group_{metric}.json")
        if not os.path.isfile(p):
            p = os.path.join(cdir, arch, f"s{s}_per_group.json")  # pinball 默认文件
        with open(p, encoding="utf-8") as fh:
            per_seed[s] = json.load(fh)
    groups = sorted(per_seed[0].keys())
    for s in (1, 2):
        assert sorted(per_seed[s].keys()) == groups, f"{arch} seed{s} 组集不一致"
    mat = np.array([[per_seed[s][g] for g in groups] for s in (0, 1, 2)])
    return groups, median_over_seeds(mat)


def select_winner(candidates: dict, params: dict, *,
                  b: int = 10000, rng: np.random.Generator | None = None) -> dict:
    """DGN-E-13 逐行实现。candidates: {arch: {pinball: [G], mae: [G], ce80: [G]}}。"""
    rng = rng or np.random.default_rng()
    S0 = list(candidates.keys())

    # 1-2. 聚合已在载入侧完成（seed median）；best = argmin pinball
    best = min(S0, key=lambda c: float(np.mean(candidates[c]["pinball"])))

    # 3. S1 = pinball 相对差等价带
    S1 = [best]
    for c in S0:
        if c == best:
            continue
        r = paired_bootstrap_rel_diff(
            np.asarray(candidates[c]["pinball"]),
            np.asarray(candidates[best]["pinball"]), b=b, rng=rng)
        if r["status"] == "OK" and in_equivalence_band(r["ci_lower"], r["ci_upper"]):
            S1.append(c)

    if len(S1) == 1:
        return {"winner": best, "stage": "S1_pinball", "S1": S1, "S2": None,
                "S3": None}

    # 5. S2 = MAE 等价带（参照 = S1 内 MAE 最小者）
    best_mae = min(S1, key=lambda c: float(np.mean(candidates[c]["mae"])))
    S2 = [best_mae]
    for c in S1:
        if c == best_mae:
            continue
        r = paired_bootstrap_rel_diff(
            np.asarray(candidates[c]["mae"]),
            np.asarray(candidates[best_mae]["mae"]), b=b, rng=rng)
        if r["status"] == "OK" and in_equivalence_band(r["ci_lower"], r["ci_upper"]):
            S2.append(c)
    if len(S2) == 1:
        return {"winner": best_mae, "stage": "S2_mae", "S1": S1, "S2": S2,
                "S3": None}

    # 6. S3 = 有符号 ΔCE80 等价带（禁止先取 abs 再算 CI——R6）
    best_ce = min(S2, key=lambda c: abs(float(np.mean(candidates[c]["ce80"]))))
    S3 = [best_ce]
    for c in S2:
        if c == best_ce:
            continue
        r = paired_bootstrap_diff(
            np.asarray(candidates[c]["ce80"]),
            np.asarray(candidates[best_ce]["ce80"]), b=b, rng=rng)
        if r["status"] == "OK" and r["ci_lower"] >= -0.01 and r["ci_upper"] <= 0.01:
            S3.append(c)
    if len(S3) == 1:
        return {"winner": best_ce, "stage": "S3_ce80", "S1": S1, "S2": S2,
                "S3": S3}

    # 8. 参数量最小 → model_id 字典序（行政平局）
    winner = sorted(S3, key=lambda c: (params[c], c))[0]
    return {"winner": winner, "stage": "S4_params_lex", "S1": S1, "S2": S2,
            "S3": S3}


def write_preterminal(result: dict, candidates: dict, params: dict,
                      *, artifact_dir_map: dict, env_manifest_sha256: str) -> dict:
    """步骤 9：WINNER_SELECTED_PRETERMINAL.json（先于任何 test/OOD 访问）。"""
    val_ev_hashes = {}
    for c in ARCH_ORDER:
        p = os.path.join(CONFIRM_DIR, c, "summary.json")
        val_ev_hashes[c] = sha256_file(p) if os.path.isfile(p) else None
    sel_cfg = os.path.join(CANDIDATES_DIR, result["winner"],
                           "selected_config.json")
    with open(sel_cfg, encoding="utf-8") as fh:
        sel = json.load(fh)
    winner_identity = {"model_id": sel["model_id"],
                       "config_sha256": sel["config_sha256"]}
    identity_sha = hashlib.sha256(canonical_identity(winner_identity)).hexdigest()

    doc = {
        "schema": "predictor_v3_winner_selected_preterminal_v1",
        "winner": {"model_id": sel["model_id"], "arch": result["winner"],
                   "config_sha256": sel["config_sha256"],
                   "artifact_dir": artifact_dir_map[result["winner"]],
                   "canonical_delivery_seed_index": 0},
        "winner_identity": winner_identity,
        "winner_identity_sha256": identity_sha,
        "candidates": list(ARCH_ORDER),
        "params": params,
        "selection": {"stage": result["stage"], "S1": result["S1"],
                      "S2": result["S2"], "S3": result["S3"],
                      "algorithm": ALGO_VERSION,
                      "equivalence_band_rel": list(EQUIVALENCE_BAND)},
        "val_evidence_sha256": val_ev_hashes,
        "selected_utc": datetime.now(timezone.utc).isoformat(),
        "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
        "env_manifest_sha256": env_manifest_sha256,
        "invariant": "本文件先于任何 test/OOD 访问；终评结果不得改变 winner 身份（FR-605）",
    }
    os.makedirs(WINNER_DIR, exist_ok=True)
    path = os.path.join(WINNER_DIR, "WINNER_SELECTED_PRETERMINAL.json")
    reg.write_json_with_self_hash(doc, path)
    return doc


def canonical_identity(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")

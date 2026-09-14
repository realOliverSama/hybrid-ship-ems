# -*- coding: utf-8 -*-
"""terminal.py — T-018 一次性终评 campaign 编排（DGN-E-16(6,7)；FR-606~608）。

- campaign 登记生命周期：DRAFT → REGISTERED_LOCKED → UNSEAL_AUTHORIZED → RUNNING
  → COMPLETE（INTERRUPTED 经 FR-608）；
- UNSEAL 前置：六项核验（load_terminal 内部执行）+ 评估器 val 预演证据（rehearsed_on_val）；
- 评估范围（FR-607）：5 候选 + 消融臂 × {0,1,2} ×（IID test + OOD A1/B1/C1/C2/C3
  realized 五条件）× Q2 全指标栈；reatt(H1+K+F) 与 reatt 候选共享同一工件
 （按 artifact sha256 去重，禁第二份计算副本）；
- 指标 = metrics.full_report（复用 E1 同一冻结实现——NFR-444 禁终评特供版）；
- incident ledger：每次失败/重跑 append（FR-608；机械重跑条件 = 模型/config/归一化/
  契约/指标语义/winner 身份全不变）；
- 正交退化字段（FR-602/DGN-E-12）：two-sample cluster bootstrap + effect_band。
"""
from __future__ import annotations

import hashlib
import json
import os
import time
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
RESULTS = os.path.join(MODELE_ROOT, "results", "predictor_v3")
TERMINAL_DIR = os.path.join(RESULTS, "terminal")
CAMPAIGN_DIR = os.path.join(RESULTS, "terminal_campaigns")

from predictor_v3 import registry as reg  # noqa: E402
from predictor_v3 import data_access  # noqa: E402
from predictor_v3.inference_stats import (degradation_fields,  # noqa: E402
                                          two_sample_bootstrap_ratio)
from predictor_v3.quantile import crossing_rate  # noqa: E402

OOD_CONDITIONS = ("A1", "B1", "C1", "C2", "C3")


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ------------------------------------------------------------ campaign 生命周期

def register_campaign(campaign_id: str, *, scope: dict,
                      preterminal_path: str) -> dict:
    """DRAFT → REGISTERED_LOCKED：登记 campaign（引用 PRETERMINAL 工件，六核验数据齐备）。"""
    with open(preterminal_path, encoding="utf-8") as fh:
        pre = json.load(fh)
    campaign = {
        "schema": "predictor_v3_terminal_campaign_v1",
        "campaign_id": campaign_id,
        "intent": "TERMINAL_EVAL_T443",
        "scope": scope,
        "preterminal_winner_manifest_path": os.path.relpath(
            preterminal_path, MODELE_ROOT),
        "preterminal_winner_artifact_sha256": sha256_file(preterminal_path),
        "winner_identity_sha256": pre["winner_identity_sha256"],
        "eval_config_sha256": data_access.file_sha256(
            data_access.EVAL_CONFIG_PATH),
        "code_git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
        "val_rehearsal_evidence_sha256": None,   # 预演后回填
        "registered_utc": datetime.now(timezone.utc).isoformat(),
        "authorized_utc": None,                  # UNSEAL 前填
        "status": "REGISTERED_LOCKED",
    }
    campaign["artifact_sha256"] = hashlib.sha256(
        canonical_json({k: v for k, v in campaign.items()
                        if k != "artifact_sha256"})).hexdigest()
    os.makedirs(CAMPAIGN_DIR, exist_ok=True)
    path = os.path.join(CAMPAIGN_DIR, f"{campaign_id}.json")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(campaign, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    return campaign


def authorize_unseal(campaign_id: str, *, rehearsal_evidence_sha256: str,
                     campaign_dir: str | None = None) -> dict:
    """REGISTERED_LOCKED → UNSEAL_AUTHORIZED（预演证据 + 时间戳序落盘）。"""
    cdir = campaign_dir or CAMPAIGN_DIR
    path = os.path.join(cdir, f"{campaign_id}.json")
    with open(path, encoding="utf-8") as fh:
        c = json.load(fh)
    if c["status"] != "REGISTERED_LOCKED":
        raise ValueError(f"状态 {c['status']} 不可 UNSEAL（须 REGISTERED_LOCKED）")
    c["val_rehearsal_evidence_sha256"] = rehearsal_evidence_sha256
    c["authorized_utc"] = datetime.now(timezone.utc).isoformat()
    c["status"] = "UNSEAL_AUTHORIZED"
    c["artifact_sha256"] = hashlib.sha256(
        canonical_json({k: v for k, v in c.items()
                        if k != "artifact_sha256"})).hexdigest()
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(c, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    return c


def mark_running(campaign_id: str, campaign_dir: str | None = None) -> None:
    _set_status(campaign_id, "RUNNING", campaign_dir)


def mark_complete(campaign_id: str, campaign_dir: str | None = None) -> None:
    _set_status(campaign_id, "COMPLETE", campaign_dir)


def _set_status(campaign_id: str, status: str, campaign_dir: str | None) -> None:
    cdir = campaign_dir or CAMPAIGN_DIR
    path = os.path.join(cdir, f"{campaign_id}.json")
    with open(path, encoding="utf-8") as fh:
        c = json.load(fh)
    c["status"] = status
    c["artifact_sha256"] = hashlib.sha256(
        canonical_json({k: v for k, v in c.items()
                        if k != "artifact_sha256"})).hexdigest()
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(c, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")


# ------------------------------------------------------------ 评估核心

def evaluate_model_on_view(model, view, *, norm_ext: dict,
                           formal_norm: dict, batch_size: int = 1024) -> dict:
    """单模型 × 单 DatasetView 全指标（复用 metrics.full_report——NFR-444）。

    返回 metrics_result 主体（views/segments/crossing/e300/normalized）。
    归一化 transform 复用 features.assemble 的同源实现。
    """
    from predictor_v3.features import (assemble, registry_channel_order,
                                       load_formal_norm)
    from predictor_v3 import features as ft
    raw = view.raw_arrays()
    hist_by_ch = {ch: raw["hist_raw"][:, :, i]
                  for i, ch in enumerate(view.h1_channels)}
    kf_by_ch = {ch: raw["kf_raw"][:, :, i]
                for i, ch in enumerate(view.kf_channels)}
    feats = assemble(hist_by_ch, kf_by_ch, formal_norm, norm_ext)
    inputs = {"hist_input": feats["hist"]}
    if "kf" in feats:
        inputs["kf_input"] = feats["kf"]
    pred = model.predict(inputs, verbose=0, batch_size=batch_size)
    pred = np.asarray(pred, dtype=np.float64)

    gmap = json.load(open(os.path.join(
        MODELE_ROOT, "results", "predictor_v3", "iid_voyage_group_map.json")))
    group_map = gmap["mapping"]
    voyage_ids = view.voyage_ids
    if view.kind == "ood":
        groups = np.array([
            f"{c}|{g}" for c, g in zip(
                view.ood_conditions,
                [group_map.get(v, f"__{v}") for v in voyage_ids])])
    else:
        groups = np.array([group_map.get(v, f"__{v}") for v in voyage_ids])

    from predictor_v3.metrics import full_report
    report = full_report(
        raw["y_raw"], pred, voyage_ids,
        {},  # split_group_of 不再需要——full_report 需要 voyage→group，见下
        e300_true=raw["e300_raw"],
        y_std_per_lead=json.load(open(
            os.path.join(MODELE_ROOT, "results", "dataset_v3", "formal",
                         "formal_norm_v3.json")))["y_std"],
        y_std_global=json.load(open(data_access.EVAL_CONFIG_PATH))[
            "normalization_denominators"]["global"]["value"])
    # split_group 映射： voyage → group（OOD 视图 group 前缀条件名）
    sg_map = {}
    for v in np.unique(voyage_ids):
        sg_map[str(v)] = group_map.get(str(v), f"__{v}")
    if view.kind == "ood":
        cond_of = dict(zip(voyage_ids.tolist(),
                           view.ood_conditions.tolist()))
        sg_map = {v: f"{cond_of[v]}|{sg_map[v]}" for v in sg_map}
    report_full = full_report(
        raw["y_raw"], pred, voyage_ids, sg_map,
        e300_true=raw["e300_raw"],
        y_std_per_lead=json.load(open(
            os.path.join(MODELE_ROOT, "results", "dataset_v3", "formal",
                         "formal_norm_v3.json")))["y_std"],
        y_std_global=json.load(open(data_access.EVAL_CONFIG_PATH))[
            "normalization_denominators"]["global"]["value"])
    report_full["crossing_rate"] = crossing_rate(pred)
    report_full["n_windows"] = int(view.n_windows)
    return report_full


class IncidentLedger:
    """FR-608 terminal incident ledger（DGN-E-16(7)；空数组合法）。"""

    def __init__(self, campaign_id: str):
        self.path = os.path.join(TERMINAL_DIR, f"{campaign_id}_incidents.json")
        self.campaign_id = campaign_id
        self.incidents = []
        if os.path.isfile(self.path):
            with open(self.path, encoding="utf-8") as fh:
                self.incidents = json.load(fh).get("incidents", [])

    def add(self, *, phase: str, symptom: str, root_cause_class: str,
            action: str, rerun_allowed: bool, verdict: str) -> None:
        self.incidents.append({
            "utc": datetime.now(timezone.utc).isoformat(),
            "phase": phase, "symptom": symptom,
            "root_cause_class": root_cause_class, "action": action,
            "rerun_allowed_by": "FR-608" if rerun_allowed else None,
            "verdict": verdict})

    def save(self) -> str:
        doc = {"schema": "predictor_v3_terminal_incident_ledger_v1",
               "campaign_id": self.campaign_id,
               "incidents": self.incidents}
        doc["artifact_sha256"] = hashlib.sha256(
            canonical_json({k: v for k, v in doc.items()
                            if k != "artifact_sha256"})).hexdigest()
        os.makedirs(TERMINAL_DIR, exist_ok=True)
        with open(self.path, "w", encoding="utf-8") as fh:
            json.dump(doc, fh, ensure_ascii=False, indent=2, sort_keys=True)
            fh.write("\n")
        return self.path

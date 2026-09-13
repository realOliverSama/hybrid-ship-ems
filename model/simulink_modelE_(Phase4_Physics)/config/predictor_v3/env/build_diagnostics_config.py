#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build_diagnostics_config.py — T-005 diagnostics_config.json 冻结落盘。

train 参照统计（合法：train split 开放）：
- H1 per-(channel,bin) p01/p99 + per-channel 池化 p01/p99/std（W1 归一化分母）；
- KF per-(channel,lead) p01/p99 + per-channel 池化 std；
- target：Y per-lead p01/p99 + E300 p01/p99 + ȳ train 池化 std；
- 分位定义 np.percentile 线性插值（method 冻结记录）。

这些参照先于任何 test/OOD 访问冻结（SEALED 纪律下 T-018 轴①②使用）。
"""
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
REPO_ROOT = os.path.dirname(MODELE_ROOT)
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

IID_NPZ = os.path.join(MODELE_ROOT, "datasets_v3", "formal", "windows_v3_formal.npz")
NORM_EXT_PATH = os.path.join(MODELE_ROOT, "config", "predictor_v3",
                             "predictor_norm_ext_v1.json")
OUT_PATH = os.path.join(MODELE_ROOT, "config", "predictor_v3",
                        "diagnostics_config.json")


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    print("[diag_cfg] loading train split …", flush=True)
    from predictor_v3.data_access import load_split
    view = load_split("train", log=True)
    print(f"[diag_cfg] train windows={view.n_windows}", flush=True)

    def pcts(a, axis=None):
        return np.percentile(np.asarray(a, dtype=np.float64), [1.0, 99.0],
                             axis=axis).tolist()

    raw = view.raw_arrays()          # hist_raw [N,300,11] / kf_raw [N,30,6]
    h1_ch = view.h1_channels
    kf_ch = view.kf_channels

    print("[diag_cfg] H1 per-(channel,bin) percentiles …", flush=True)
    h1_ref = {}
    for i, ch in enumerate(h1_ch):
        col = raw["hist_raw"][:, :, i]
        h1_ref[ch] = {
            "per_bin_p01_p99": pcts(col, axis=0),
            "pooled_p01_p99": pcts(col),
            "pooled_std": float(col.std()),
        }
        print(f"  h1 {ch} done", flush=True)

    print("[diag_cfg] KF per-(channel,lead) percentiles …", flush=True)
    kf_ref = {}
    for i, ch in enumerate(kf_ch):
        col = raw["kf_raw"][:, :, i]
        kf_ref[ch] = {
            "per_lead_p01_p99": pcts(col, axis=0),
            "pooled_p01_p99": pcts(col),
            "pooled_std": float(col.std()),
        }
        print(f"  kf {ch} done", flush=True)

    print("[diag_cfg] target refs …", flush=True)
    y = raw["y_raw"]                 # [N,30] kW
    y_ref = {
        "per_lead_p01_p99": pcts(y, axis=0),
        "pooled_p01_p99": pcts(y),
        "pooled_std": float(y.std()),
        "unit": "kW",
    }
    e300 = raw["e300_raw"]
    e300_ref = {
        "p01_p99": pcts(e300),
        "pooled_std": float(e300.std()),
        "unit": "kWh",
        "identity_note": "E300 = ȳ/12（30×10s 几何下与 30-bin 均值功率线性等价，"
                         "不得计为两条独立证据，FR-601）",
    }

    doc = {
        "schema": "predictor_v3_diagnostics_config_v1",
        "task": "T-005",
        "utc": datetime.now(timezone.utc).isoformat(),
        "governance": "DGN-E-14 冻结定义（T-443 三轴）；train 参照先于任何 test/OOD "
                      "访问冻结；观测正式结果后禁改",
        "quantile_method": "np.percentile linear interpolation（p01/p05/p50/p95/p99 冻结集）",
        "w1": {
            "implementation": "scipy.stats.wasserstein_distance（精确经验分布，义务 8）",
            "normalization": "train 参照 std（分母有效为零 ⇒ UNDEFINED_CONSTANT_SUPPORT "
                             "转常值支持诊断路径）",
            "subsampling": {
                "method": "deterministic_order_independent_hash_bottom_k"
                          "（禁等距 stride——数组序可能按 voyage/时间结构化）",
                "cap": 200000,
                "key_format": "dataset/set|voyage_id|anchor_t0|channel|lag_or_lead",
                "note": "N ≤ cap 全取；N > cap 取确定性 hash 值最小的 cap 个观测"
            }
        },
        "axis1_input_drift": {
            "h1_channels": h1_ch,
            "kf_channels": kf_ch,
            "compare": {"primary": "ood_condition_vs_iid_test",
                        "reference": "ood_vs_train"},
            "out_of_support": "train p01/p99 支持外比例（本 config train_ref 冻结值）",
            "z_stats": "p95/p99/max |train-z|（z 用 formal_norm_v3 x_h1_mean/std 与 "
                       "predictor_norm_ext_v1 mean/scale）",
            "top_shifting_k": 5,
            "constant_fields": ["ctrl_mode", "manual_power_cmd_pct"],
            "constant_channel_rule": "常值通道禁算标准化距离比；单列非零计数/比例/偏差范围（FR-581）",
            "interpretation_filter": "模型级解释只能用该模型实际消费的特征（H1-only 模型"
                                     "不挂 F 通道归因）",
            "status": "描述/归因证据，非 Gate"
        },
        "axis2_target_drift": {
            "primary_scalar": "E300",
            "identity_note": e300_ref["identity_note"],
            "per_lead": True,
            "segments": True,
            "compare": {"primary": "ood_condition_vs_iid_test", "reference": "ood_vs_train"},
            "interpretation_invariant": "target shift ≠ predictor failure；边缘无强漂移"
                                        "不证明无条件分布预测困难",
            "status": "描述/归因证据，非 Gate"
        },
        "axis3_degradation": {
            "ratio_metrics": ["pinball", "mae_q50", "rmse_q50"],
            "ratio_note": "R_deg = metric_OOD/metric_IID_test 仅用于正误差/proper-loss 指标",
            "diff_metrics": ["ce80", "picp80", "r2_e300"],
            "context_only": ["mpiw", "crossing_rate"],
            "bootstrap": "two-sample split_group cluster bootstrap（B=10000；"
                         "禁 IID-test↔OOD paired bootstrap）",
            "effect_bands": {"SMALL_le": 1.2, "MODERATE_le": 1.5, "LARGE_gt": 1.5},
            "orthogonal_fields": ["effect_band", "ci_lower_exceeds_1",
                                  "ci_upper_within_20pct", "substantial_degradation"],
            "summary_label_precedence": ["MAINTAINED_WITHIN_20PCT",
                                         "SUPPORTED_DEGRADATION", "UNCERTAIN"]
        },
        "small_n": {"threshold": 20, "flag": "SMALL_N_CAUTION"},
        "train_ref": {
            "h1": h1_ref,
            "kf": kf_ref,
            "y": y_ref,
            "e300": e300_ref,
            "source": {
                "iid_npz_sha256": sha256_file(IID_NPZ),
                "norm_ext_artifact_sha256": json.load(open(NORM_EXT_PATH))["artifact_sha256"],
                "split": "train（fail-closed load_split；136052 窗）",
                "note": "全部为 train 子集统计（开放数据）；test/OOD 零接触"
            }
        },
    }
    body = {k: v for k, v in doc.items() if k != "artifact_sha256"}
    doc["artifact_sha256"] = hashlib.sha256(canonical_json(body)).hexdigest()
    with open(OUT_PATH, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    print(f"[diag_cfg] wrote {os.path.relpath(OUT_PATH, REPO_ROOT)} "
          f"self_hash={doc['artifact_sha256'][:16]}…")
    return 0


if __name__ == "__main__":
    sys.exit(main())

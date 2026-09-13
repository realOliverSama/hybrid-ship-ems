# -*- coding: utf-8 -*-
"""norm_ext.py — predictor_norm_ext_v1 拟合/加载/transform（T-004，DGN-E-04）。

契约（FR-580/581/582）：
- K/F 连续通道按 feature × future-lead-bin 从 **train 窗口** 拟合 mean/scale
 （window-uniform，与训练曝光一致）；val/test/OOD 只 transform、禁 refit；
- std < 1e-12 → scale=1 保护；禁 clip（支持外取值归一化后保留原样）；
- 常值通道 OUT_OF_TRAIN_CONSTANT_SUPPORT 诊断钩子（count/fraction/deviation，
  不 clamp、不拒样本；H1 侧常值通道 ctrl_mode/manual_power_cmd_pct 由
  formal_norm_v3 冻结（scale=1 + train_constant=true），本钩子按 FR-581 服务于评估）；
- v1 不拟合、不暴露 H2（FR-582）；
- 工件自哈希（canonical JSON 排除 artifact_sha256，DGN-E-16 配方）。
"""
from __future__ import annotations

import json
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)

ARTIFACT_PATH = os.path.join(MODELE_ROOT, "config", "predictor_v3",
                             "predictor_norm_ext_v1.json")
EPS_STD = 1e-12
LEAD_BINS = 30

# H1 面常值通道（D formal_norm_v3 已冻结 scale=1 + train_constant=true；
# FR-581 诊断义务的检测对象清单在此声明，供评估器统一挂钩）
H1_CONSTANT_FIELDS = ("ctrl_mode", "manual_power_cmd_pct")


def self_hash(obj: dict, key: str = "artifact_sha256") -> str:
    import hashlib
    body = {k: v for k, v in obj.items() if k != key}
    return hashlib.sha256(json.dumps(
        body, sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode("utf-8")).hexdigest()


# ---------------------------------------------------------------- fit

def fit(kf_arrays: dict, *, feature_order: list, source: dict,
        n_train_windows: int, version: str = "v1") -> dict:
    """train-only window-uniform 拟合。

    Parameters
    ----------
    kf_arrays : {channel: (N,30) raw KF 数组}（train split 全部窗口）
    feature_order : 通道顺序（registry 冻结顺序）
    source : 冻结资产 hash 引用（dataset_manifest / split_assignment /
             formal_norm_v3 / train npz 文件 sha256）
    n_train_windows : train 窗口数（身份摘要）
    """
    import hashlib
    kf, constant_fields = {}, []
    for ch in feature_order:
        arr = np.asarray(kf_arrays[ch], dtype=np.float64)
        if arr.ndim != 2 or arr.shape[1] != LEAD_BINS:
            raise ValueError(f"{ch} 形状须 (N,{LEAD_BINS})，实际 {arr.shape}")
        if arr.shape[0] != n_train_windows:
            raise ValueError(f"{ch} 窗口数 {arr.shape[0]} != 声明 {n_train_windows}")
        mean = arr.mean(axis=0)                      # window-uniform
        std = arr.std(axis=0)
        scale = np.where(std < EPS_STD, 1.0, std)    # 常值保护
        kf[ch] = {"mean": mean.tolist(), "scale": scale.tolist()}
        if bool(np.all(std < EPS_STD)):              # 通道级常值登记
            constant_fields.append(ch)
    artifact = {
        "schema": "predictor_norm_ext_v1",
        "version": version,
        "fit_on": "train_only",
        "source": dict(source),
        "kf": kf,
        "constant_policy": {"scale_one": True,
                            "train_constant_fields": constant_fields},
        "window_uniform_fit": True,
        "feature_order": list(feature_order),
        "lead_bins": LEAD_BINS,
        "lead_order": "ascending 10..300 s（k=1..30，与 X_KF:: 列语义一致）",
        "fit_semantics": ("per-(channel, lead-bin) mean/std over train windows"
                          "（window-uniform exposure，FR-580）"),
        "clip_policy": "FORBIDDEN（支持外取值归一化后保留原样）",
        "refit_policy": "FORBIDDEN（val/test/OOD 禁 refit；工件冻结后不可变）",
        "h2_policy": "v1 不拟合不暴露 H2（FR-582）",
        "n_train_windows": int(n_train_windows),
    }
    artifact["artifact_sha256"] = hashlib.sha256(json.dumps(
        {k: v for k, v in artifact.items()},
        sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode("utf-8")).hexdigest()
    return artifact


# ---------------------------------------------------------------- load/transform

def load_artifact(path: str = ARTIFACT_PATH) -> dict:
    """加载并校验自哈希（篡改即 raise——禁 refit 的机器保障）。"""
    with open(path, encoding="utf-8") as fh:
        artifact = json.load(fh)
    expect = artifact.get("artifact_sha256")
    if not expect or self_hash(artifact) != expect:
        raise ValueError(f"predictor_norm_ext 工件自哈希不一致: {path}")
    if artifact.get("schema") != "predictor_norm_ext_v1":
        raise ValueError(f"schema 不符: {artifact.get('schema')}")
    return artifact


def transform_channel(artifact: dict, channel: str, x_raw: np.ndarray) -> np.ndarray:
    """单通道 transform：(x − mean[c,k]) / scale[c,k]；禁 clip；支持外保留原样。

    x_raw 形状 (..., 30)（末维 = lead bin k=1..30）；返回 float64/float32 同形。
    不修改 artifact（禁 refit 的语义保障）。
    """
    if channel not in artifact["kf"]:
        raise KeyError(f"工件无通道 {channel}（v1 仅 K/F；H2 禁入，FR-582）")
    mean = np.asarray(artifact["kf"][channel]["mean"], dtype=np.float64)
    scale = np.asarray(artifact["kf"][channel]["scale"], dtype=np.float64)
    x = np.asarray(x_raw, dtype=np.float64)
    if x.shape[-1] != LEAD_BINS:
        raise ValueError(f"末维须为 {LEAD_BINS} lead bins，实际 {x.shape}")
    return (x - mean) / scale          # 无 clip、无拒样：支持外原样线性外推


def transform(artifact: dict, kf_raw: dict) -> dict:
    """全 K/F 通道 transform：{channel: raw (N,30)} → {channel: normalized}。"""
    return {ch: transform_channel(artifact, ch, v) for ch, v in kf_raw.items()}


# ---------------------------------------------------------------- 常值诊断钩子

def constant_support_diagnostics(x_raw: np.ndarray, constant_value: float) -> dict:
    """OUT_OF_TRAIN_CONSTANT_SUPPORT 诊断（FR-581/DGN-E-04；纯函数，不 clamp 不拒样）。

    判据（冻结字面）：|x_raw − train_constant_value| > 0 即支持外。
    返回 count/fraction/deviation_min/deviation_max（带符号 x−c 极值）；
    无支持外取值时 deviation 为 0.0/0.0。
    """
    x = np.asarray(x_raw, dtype=np.float64)
    dev = x - float(constant_value)
    out = dev != 0.0
    count = int(out.sum())
    if count:
        return {"count": count, "fraction": count / x.size,
                "deviation_min": float(dev[out].min()),
                "deviation_max": float(dev[out].max())}
    return {"count": 0, "fraction": 0.0, "deviation_min": 0.0,
            "deviation_max": 0.0}


# ---------------------------------------------------------------- fit 驱动

def fit_from_iid_npz(iid_npz_path: str, out_path: str = ARTIFACT_PATH) -> dict:
    """CLI 拟合入口：fail-closed 经 data_access.load_split("train") 取 train KF。"""
    from predictor_v3.data_access import (file_sha256, load_split)

    view = load_split("train", iid_npz_path=iid_npz_path, log=True)
    raw = view.raw_arrays()               # KF raw（train 子集）
    kf_arrays = {ch: raw["kf_raw"][:, :, i]
                 for i, ch in enumerate(view.kf_channels)}
    source = {
        "train_npz_sha256": file_sha256(iid_npz_path),
        "dataset_manifest_scientific_payload_sha256":
            "ee285091027a5bda50652832dbf7ba780302feca8897b9195715570a1ea7d712",
        "split_assignment_canonical_sha256":
            "ac4c4a7629534daf1c4d88b05848fbfe1bb1dc148679d0b7abbbe873f66c809f",
        "formal_norm_v3_sha256":
            "b64210bc47a035afe9fd164c65ba63fca84aead53dc7e2dcaada79bd88f065ba",
        "train_split_identity": "IID windows splits=='train'（windows_v3_formal.npz）",
    }
    artifact = fit(kf_arrays, feature_order=view.kf_channels,
                   source=source, n_train_windows=view.n_windows)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as fh:
        json.dump(artifact, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    return artifact


def main(argv=None) -> int:
    import sys
    iid = os.path.join(MODELE_ROOT, "datasets_v3", "formal",
                       "windows_v3_formal.npz")
    artifact = fit_from_iid_npz(iid)
    print(f"[norm_ext] fit windows={artifact['n_train_windows']} "
          f"channels={artifact['feature_order']} "
          f"constant_kf_fields={artifact['constant_policy']['train_constant_fields']} "
          f"sha256={artifact['artifact_sha256'][:16]}…")
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())

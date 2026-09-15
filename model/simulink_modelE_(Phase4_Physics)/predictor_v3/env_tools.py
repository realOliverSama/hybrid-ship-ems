#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""env_tools.py — 环境工具：确定性探针（T-006，DGN-E-10；FR-594）。

CPU 探针协议（冻结）：固定小配置（合成数据 + 小 MLP，非正式候选）两独立 CPU 进程
同种子双跑（deterministic ops 启用）→ 比较预测/指标/权重 hash；bitwise 或数值容差
如实落盘。禁用正式候选；MPS/Metal 不参与本探针（GPU 设备显式隐藏）。

用法（两跑 + 比较一把出）：
  .venv_tf/bin/python -m predictor_v3.env_tools
CUDA 探针（T-007，canonical 环境）复用同一 --once 协议。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)

PROBE_SEED = 20260913
PROBE_EPOCHS = 2
PROBE_N = 128
PROBE_BATCH = 32


def _sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def run_probe_once(out_json: str, device: str = "cpu") -> dict:
    """单进程探针 run：固定小 MLP（raw 投影 + 共用 MonotonicQuantileTransform）
    合成数据训练 PROBE_EPOCHS → 记录预测 hash/指标/权重 hash。

    device="cpu"：显式隐藏全部 GPU（本地 macOS/Linux CPU）；device="cuda"：
    不隐藏 GPU（canonical 环境探针，记录可见 GPU 身份）。
    """
    os.environ.setdefault("PYTHONHASHSEED", str(PROBE_SEED))
    import tensorflow as tf

    gpu_list = tf.config.list_physical_devices("GPU")
    if device == "cpu":
        tf.config.set_visible_devices([], "GPU")
    try:
        tf.config.experimental.enable_op_determinism()
        det = True
    except Exception:  # noqa: BLE001
        det = False

    from predictor_v3 import seeds as sd
    from predictor_v3.quantile import (build_monotonic_quantile_transform,
                                       pinball_loss, pinball_numpy,
                                       quantile_bias_init)
    sd.set_global_seeds(PROBE_SEED)

    rng = np.random.default_rng(PROBE_SEED)
    hist = rng.normal(0, 1, size=(PROBE_N, 300, 11)).astype(np.float32)
    kf = rng.normal(0, 1, size=(PROBE_N, 30, 6)).astype(np.float32)
    w = rng.normal(0, 1, size=(11 * 300 + 6 * 30, 1)).astype(np.float32)
    y = np.abs(hist[:, :, 0].mean(axis=1)[:, None] * 300.0
               + 30 * rng.normal(0, 1, size=(PROBE_N, 30))).astype(np.float32)

    tf.keras.utils.set_random_seed(PROBE_SEED)
    inp = tf.keras.Input(shape=(300, 11), name="hist_input")
    inp2 = tf.keras.Input(shape=(30, 6), name="kf_input")
    x = tf.keras.layers.Concatenate()([tf.keras.layers.Flatten()(inp),
                                       tf.keras.layers.Flatten()(inp2)])
    h = tf.keras.layers.Dense(16, activation="relu")(x)
    raw = tf.keras.layers.Dense(
        90, bias_initializer=tf.keras.initializers.Constant(
            np.tile(quantile_bias_init(30.0), 30)))(h)
    out = build_monotonic_quantile_transform()(
        tf.keras.layers.Reshape((30, 3))(raw))
    model = tf.keras.Model([inp, inp2], out)
    model.compile(optimizer=tf.keras.optimizers.Adam(1e-3, global_clipnorm=1.0),
                  loss=pinball_loss)

    hist_hist = model.fit(
        {"hist_input": hist, "kf_input": kf}, y,
        epochs=PROBE_EPOCHS, batch_size=PROBE_BATCH, verbose=0, shuffle=False)
    pred = model.predict({"hist_input": hist, "kf_input": kf}, verbose=0)

    weights_blob = b"".join(
        np.asarray(w).tobytes() for w in model.get_weights())
    result = {
        "device_policy": ("CPU_ONLY (all GPU devices hidden)" if device == "cpu"
                          else f"CUDA ({len(gpu_list)} GPU visible)"),
        "devices": [{"name": d.name, "type": d.device_type} for d in gpu_list],
        "op_determinism_enabled": bool(det),
        "seed": PROBE_SEED,
        "epochs": PROBE_EPOCHS,
        "pred_sha256": _sha256_bytes(np.asarray(pred, dtype=np.float32).tobytes()),
        "pred_max_abs": float(np.abs(pred).max()),
        "final_loss": float(hist_hist.history["loss"][-1]),
        "pinball_first_window_mean": float(pinball_numpy(y[:1], pred[:1])[0]),
        "weights_sha256": _sha256_bytes(weights_blob),
        "pred_sample_sha256": _sha256_bytes(
            np.asarray(pred[:8], dtype=np.float32).tobytes()),
        "python": sys.version.split()[0],
        "numpy": np.__version__,
        "tensorflow": tf.__version__,
        "platform": platform.platform(),
        "git_commit": os.environ.get(
            "EMS_PINNED_GIT_COMMIT", "UNPINNED"),
    }
    os.makedirs(os.path.dirname(out_json) or ".", exist_ok=True)
    with open(out_json, "w", encoding="utf-8") as fh:
        json.dump(result, fh, ensure_ascii=False, indent=2, sort_keys=True)
    return result


def run_probe_pair(out_dir: str | None = None, device: str = "cpu") -> dict:
    """两独立子进程双跑 + 比较（bitwise/容差如实落盘）。"""
    import tempfile
    out_dir = out_dir or os.path.join(
        MODELE_ROOT, "artifacts", "predictor_v3", "probe_tmp")
    os.makedirs(out_dir, exist_ok=True)
    runs = []
    for i in (1, 2):
        p = os.path.join(out_dir, f"probe_run{i}.json")
        cmd = [sys.executable, "-m", "predictor_v3.env_tools",
               "--once", "--device", device, "--out", p]
        subprocess.run(cmd, cwd=MODELE_ROOT, check=True,
                       capture_output=True, text=True)
        with open(p, encoding="utf-8") as fh:
            runs.append(json.load(fh))

    r1, r2 = runs
    comparison = {
        "pred_sha256_equal": r1["pred_sha256"] == r2["pred_sha256"],
        "weights_sha256_equal": r1["weights_sha256"] == r2["weights_sha256"],
        "final_loss_equal": r1["final_loss"] == r2["final_loss"],
        "loss_abs_delta": abs(r1["final_loss"] - r2["final_loss"]),
        "bitwise": (r1["pred_sha256"] == r2["pred_sha256"]
                    and r1["weights_sha256"] == r2["weights_sha256"]),
    }
    doc = {
        "schema": "predictor_v3_determinism_probe_v1",
        "task": "T-006" if device == "cpu" else "T-007",
        "platform": "cpu_local" if device == "cpu" else "cuda_canonical",
        "utc": datetime.now(timezone.utc).isoformat(),
        "protocol": "DGN-E-10：固定小配置（合成数据+小 MLP，非正式候选）两独立进程"
                    "同种子双跑；deterministic ops 启用；GPU 显式隐藏",
        "seed": PROBE_SEED,
        "runs": {"run1": r1, "run2": r2},
        "comparison": comparison,
        "verdict": "PASS_BITWISE" if comparison["bitwise"] else
                   ("PASS_WITH_TOLERANCE" if comparison["loss_abs_delta"] < 1e-5
                    else "FAIL"),
        "note": "结论如实：bitwise 为门内理想结果；非 bitwise 时容差如实记录，"
                "不声称跨后端/跨硬件 bitwise（FR-594）",
    }
    doc["artifact_sha256"] = hashlib.sha256(json.dumps(
        {k: v for k, v in doc.items() if k != "artifact_sha256"},
        sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode()).hexdigest()
    out = os.path.join(MODELE_ROOT, "results", "predictor_v3",
                       f"determinism_probe_{device}.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    print(json.dumps({"verdict": doc["verdict"],
                      "bitwise": comparison["bitwise"],
                      "evidence": os.path.relpath(out, os.path.dirname(MODELE_ROOT))},
                     ensure_ascii=False))
    return doc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="T-006 CPU 确定性探针")
    ap.add_argument("--once", action="store_true", help="单进程 run（子进程调用）")
    ap.add_argument("--device", default="cpu", choices=["cpu", "cuda"])
    ap.add_argument("--out", default=None)
    args = ap.parse_args(argv)
    if args.once:
        run_probe_once(args.out or os.path.join(
            MODELE_ROOT, "artifacts", "predictor_v3", "probe_tmp", "run.json"),
            device=args.device)
        return 0
    doc = run_probe_pair(device=args.device)
    return 0 if doc["verdict"].startswith("PASS") else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Phase 4-E predictor-v3 本地 macOS 兼容 smoke（T-001，DGN-E-15 / FR-592）。

证据落盘 results/predictor_v3/env_smoke_macos.json（入库）。
三必测项（DGN-E-15 冻结定义）：
  1. 合成数据 1-epoch 小 MLP 前向+反向；
  2. tf.config.list_physical_devices() 设备清单；
  3. npz 读冒烟（schema 级：array name/dtype/shape，经 zipfile 头部解析，
     不触碰数据字节——sealed 纪律下不读 test split 数值）。
附加 informational 项（不计入 PASS 判据，如实记录）：Metal GPU matmul 探测。

用法（仓库根）：
  .venv_tf/bin/python "model/simulink_modelE_(Phase4_Physics)/config/predictor_v3/env/env_smoke_macos.py"
"""
import hashlib
import io
import json
import os
import platform
import subprocess
import sys
import zipfile
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))  # env -> predictor_v3 -> config -> ModelE
REPO_ROOT = os.path.dirname(MODELE_ROOT)

NPZ_PATH = os.path.join(
    MODELE_ROOT, "datasets_v3", "formal", "windows_v3_formal.npz")
EVIDENCE_PATH = os.path.join(
    MODELE_ROOT, "results", "predictor_v3", "env_smoke_macos.json")

S0_INIT = 42


def git_commit():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT,
            stderr=subprocess.DEVNULL).decode().strip()
    except Exception as exc:  # noqa: BLE001
        return f"UNAVAILABLE:{type(exc).__name__}"


def npz_schema(path):
    """schema 级 npz 检查：zip namelist + npy header（不读数据字节）。"""
    import numpy as np
    arrays = []
    with zipfile.ZipFile(path, "r") as zf:
        for name in zf.namelist():
            if not name.endswith(".npy"):
                continue
            with zf.open(name) as fh:
                version = np.lib.format.read_magic(fh)
                shape, fortran, dtype = np.lib.format._read_array_header(
                    fh, version)
            arrays.append({
                "name": name[: -len(".npy")],
                "dtype": str(dtype),
                "shape": list(shape),
                "fortran_order": bool(fortran),
            })
    size_bytes = os.path.getsize(path)
    with open(path, "rb") as fh:
        digest = hashlib.sha256(fh.read()).hexdigest()
    return {
        "path": os.path.relpath(path, REPO_ROOT),
        "size_bytes": size_bytes,
        "sha256": digest,
        "n_arrays": len(arrays),
        "arrays": arrays,
    }


def main():
    import numpy as np
    import scipy
    import tensorflow as tf
    import keras

    ev = {
        "schema": "predictor_v3_env_smoke_v1",
        "task": "T-001",
        "platform": "macos_local",
        "utc": datetime.now(timezone.utc).isoformat(),
        "python": sys.version.split()[0],
        "tensorflow": tf.__version__,
        "keras": keras.__version__,
        "numpy": np.__version__,
        "scipy": scipy.__version__,
        "machine": platform.machine(),
        "os": f"{platform.system()} {platform.release()}",
        "git_commit": git_commit(),
        "fallback_chain": [
            {"tf": "2.21.0", "tensorflow-metal": "1.2.0", "result": "FAIL",
             "symptom": "libmetal_plugin.dylib dlopen NotFoundError: "
                        "_pywrap_tensorflow_internal.so rpath miss"},
            {"tf": "2.20.0", "tensorflow-metal": "1.2.0", "result": "FAIL",
             "symptom": "同上 dlopen NotFoundError"},
            {"tf": "2.19.1", "tensorflow-metal": "1.2.0", "result": "PASS",
             "symptom": "import OK, CPU+GPU(Metal) 设备注册成功"},
        ],
        "fallback_note": "首选 TF 2.21.0 组合 smoke 失败（T-001 Gate 预案证据驱动降钉）；"
                         "2.20.0 仍失败后按 FR-591/592 证据驱动 fallback 落锁 2.19.1"
                         "（tensorflow-metal 1.2.0 官方支持矩阵内最高 TF 版本；numpy 2.x 保持）。",
    }

    ev["physical_devices"] = [
        {"name": d.name, "type": d.device_type}
        for d in tf.config.list_physical_devices()
    ]

    # --- 1) 合成数据 1-epoch 小 MLP 前向+反向（显式 CPU） ---
    rng = np.random.default_rng(S0_INIT)
    x = rng.normal(size=(256, 8)).astype("float32")
    y = (x @ rng.normal(size=(8, 1)).astype("float32")).astype("float32")
    tf.keras.utils.set_random_seed(S0_INIT)
    with tf.device("/CPU:0"):
        m = tf.keras.Sequential([
            tf.keras.layers.Input(shape=(8,)),
            tf.keras.layers.Dense(16, activation="relu"),
            tf.keras.layers.Dense(1),
        ])
        m.compile(optimizer="adam", loss="mse")
        hist = m.fit(x, y, epochs=1, batch_size=32, verbose=0)
    losses = [float(v) for v in hist.history["loss"]]
    finite = all(np.isfinite(losses))
    ev["mlp_smoke"] = {
        "device": "CPU:0",
        "steps": int(hist.history["loss"].__len__()),
        "loss_first": losses[0],
        "loss_final": losses[-1],
        "finite": bool(finite),
        "pass": bool(finite and len(losses) == 1),
    }

    # --- informational: Metal GPU matmul 探测（不计入 PASS 判据） ---
    try:
        with tf.device("/GPU:0"):
            a = tf.constant(np.ones((64, 64), dtype="float32"))
            c = tf.matmul(a, a).numpy()
        ev["gpu_probe"] = {"device": "GPU:0", "matmul_ok": bool(
            np.isfinite(c).all()), "informational": True}
    except Exception as exc:  # noqa: BLE001
        ev["gpu_probe"] = {"device": "GPU:0", "matmul_ok": False,
                           "error": f"{type(exc).__name__}: {exc}",
                           "informational": True}

    # --- 2) npz 读冒烟（schema 级，不解封任何数值） ---
    schema = npz_schema(NPZ_PATH)
    ev["npz_smoke"] = {
        "pass": bool(schema["n_arrays"] > 0 and schema["sha256"].startswith("203dc92d")),
        "sha256_match_freeze": bool(schema["sha256"].startswith("203dc92d")),
        **schema,
    }

    checks = [ev["mlp_smoke"]["pass"], ev["npz_smoke"]["pass"]]
    ev["verdict"] = "PASS" if all(checks) else "FAIL"

    os.makedirs(os.path.dirname(EVIDENCE_PATH), exist_ok=True)
    with open(EVIDENCE_PATH, "w", encoding="utf-8") as fh:
        json.dump(ev, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    print(json.dumps({"verdict": ev["verdict"],
                      "evidence": os.path.relpath(EVIDENCE_PATH, REPO_ROOT)},
                     ensure_ascii=False))
    return 0 if ev["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())

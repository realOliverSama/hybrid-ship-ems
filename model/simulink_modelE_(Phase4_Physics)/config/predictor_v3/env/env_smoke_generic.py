#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""env_smoke_generic.py — 平台参数化兼容 smoke（T-007，DGN-E-15；FR-591/592）。

与 env_smoke_macos.py（T-001 历史冻结件）同协议：三必测项（1-epoch MLP 前向反向 /
设备清单 / npz schema 冒烟）+ informational GPU 探测。--platform cuda 用于
canonical AutoDL 环境（产出 env_smoke_cuda.json）；--platform macos 可在本机复跑
（与 macos 版同判据）。

用法（canonical，仓库根 = 代码树根）：
  python config/predictor_v3/env/env_smoke_generic.py --platform cuda \
      --npz datasets_v3/formal/windows_v3_formal.npz \
      --out results/predictor_v3/env_smoke_cuda.json
"""
import argparse
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
MODELE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
REPO_ROOT = os.path.dirname(MODELE_ROOT)

S0_INIT = 42


def git_commit():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT,
            stderr=subprocess.DEVNULL).decode().strip()
    except Exception as exc:  # noqa: BLE001
        return f"UNAVAILABLE:{type(exc).__name__}"


def npz_schema(path):
    """schema 级 npz 检查（公开 API，跨 numpy 版本；不读数据字节）。"""
    import numpy as np
    arrays = []
    with zipfile.ZipFile(path, "r") as zf:
        for name in zf.namelist():
            if not name.endswith(".npy"):
                continue
            raw = zf.read(name)
            fh = io.BytesIO(raw)
            ver = np.lib.format.read_magic(fh)
            if ver == (1, 0):
                shape, fortran, dtype = np.lib.format.read_array_header_1_0(fh)
            elif ver == (2, 0):
                shape, fortran, dtype = np.lib.format.read_array_header_2_0(fh)
            else:
                raise ValueError(f"未知 npy 版本 {ver}: {name}")
            arrays.append({"name": name[:-len(".npy")], "dtype": str(dtype),
                           "shape": list(shape), "fortran_order": bool(fortran)})
    return {
        "path": os.path.relpath(path, REPO_ROOT),
        "size_bytes": os.path.getsize(path),
        "sha256": hashlib.sha256(open(path, "rb").read()).hexdigest(),
        "n_arrays": len(arrays), "arrays": arrays,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--platform", required=True, choices=["macos", "cuda"])
    ap.add_argument("--npz", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--expect-npz-sha256-prefix", default="203dc92d",
                    help="冻结 IID npz hash 前缀（传输 hash 校验后应一致）")
    args = ap.parse_args()

    import numpy as np
    import scipy
    import tensorflow as tf
    import keras

    ev = {
        "schema": "predictor_v3_env_smoke_v1",
        "task": "T-007",
        "platform": args.platform,
        "utc": datetime.now(timezone.utc).isoformat(),
        "python": sys.version.split()[0],
        "tensorflow": tf.__version__,
        "keras": keras.__version__,
        "numpy": np.__version__,
        "scipy": scipy.__version__,
        "machine": platform.machine(),
        "os": f"{platform.system()} {platform.release()}",
        "git_commit": git_commit(),
    }
    ev["physical_devices"] = [
        {"name": d.name, "type": d.device_type}
        for d in tf.config.list_physical_devices()]

    # --- 1) 合成 1-epoch MLP 前向反向（canonical CUDA：默认设备流走 GPU） ---
    rng = np.random.default_rng(S0_INIT)
    x = rng.normal(size=(256, 8)).astype("float32")
    y = (x @ rng.normal(size=(8, 1)).astype("float32")).astype("float32")
    tf.keras.utils.set_random_seed(S0_INIT)
    m = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(8,)),
        tf.keras.layers.Dense(16, activation="relu"),
        tf.keras.layers.Dense(1),
    ])
    m.compile(optimizer="adam", loss="mse")
    hist = m.fit(x, y, epochs=1, batch_size=32, verbose=0)
    losses = [float(v) for v in hist.history["loss"]]
    ev["mlp_smoke"] = {"device": "default(visible)", "loss_final": losses[-1],
                       "finite": bool(np.isfinite(losses).all()),
                       "pass": bool(np.isfinite(losses).all())}

    # --- informational: 指定 GPU matmul 探测 ---
    try:
        if args.platform == "cuda":
            with tf.device("/GPU:0"):
                a = tf.constant(np.ones((64, 64), dtype="float32"))
                c = tf.matmul(a, a).numpy()
            ev["gpu_probe"] = {"device": "GPU:0",
                               "matmul_ok": bool(np.isfinite(c).all()),
                               "informational": True}
        else:
            ev["gpu_probe"] = {"informational": True,
                               "note": "macOS 版主设备即可用"}
    except Exception as exc:  # noqa: BLE001
        ev["gpu_probe"] = {"matmul_ok": False,
                           "error": f"{type(exc).__name__}: {exc}",
                           "informational": True}

    # --- 2) npz schema 冒烟 + 传输 hash 校验 ---
    schema = npz_schema(args.npz)
    ev["npz_smoke"] = {
        "pass": bool(schema["n_arrays"] > 0
                     and schema["sha256"].startswith(args.expect_npz_sha256_prefix)),
        "sha256_match_freeze": bool(schema["sha256"].startswith(
            args.expect_npz_sha256_prefix)),
        **schema,
    }

    checks = [ev["mlp_smoke"]["pass"], ev["npz_smoke"]["pass"]]
    ev["verdict"] = "PASS" if all(checks) else "FAIL"
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(ev, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    print(json.dumps({"verdict": ev["verdict"], "evidence": args.out},
                     ensure_ascii=False))
    return 0 if ev["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())

# -*- coding: utf-8 -*-
"""export_weights_v2.py — T-310：导出 v2 入选 MLP（分位数头）权重 + 门控阈值

产物：results/ecms_phase25/deploy/selected_model_weights_v2.json
  weights: W1/b1/W2/b2/W3/b3（302→64→32→3，输出 [q10,q50,q90] 归一化域）
  norm:    fleet_norm_v2.json 同源（X_mean/X_std/y_mean/y_std）
  gate:    S1/S2（val split 上 s=q90-q10 的 P75/P95，T-309 §3）
用法：python/export/export_weights_v2.py
"""
import io, sys, os, json
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
import numpy as np
import tensorflow as tf

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
MD = os.path.join(BASE, "results", "ecms_phase25", "models")
OUT_DIR = os.path.join(BASE, "results", "ecms_phase25", "deploy")
RUN_ID = "mlp_h64_32_lr3e-4_q"

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    m = tf.keras.models.load_model(os.path.join(MD, RUN_ID, RUN_ID + ".keras"),
                                   safe_mode=False, compile=False)
    denses = [l for l in m.layers if isinstance(l, tf.keras.layers.Dense)]
    assert len(denses) == 3, "预期 3 层 Dense，实际 %d" % len(denses)
    W1, b1 = denses[0].get_weights()[0], denses[0].get_weights()[1]
    W2, b2 = denses[1].get_weights()[0], denses[1].get_weights()[1]
    W3, b3 = denses[2].get_weights()[0], denses[2].get_weights()[1]
    assert W1.shape == (302, 64) and W2.shape == (64, 32) and W3.shape == (32, 3), \
        "权重形状异常: %s %s %s" % (W1.shape, W2.shape, W3.shape)

    norm = json.load(open(os.path.join(BASE, "datasets", "fleet_norm_v2.json"),
                          encoding="utf-8"))

    # 门控阈值：val split 上 s = q90-q10（去归一化 kWh）的 P75/P95
    d = np.load(os.path.join(BASE, "datasets", "fleet_windows_v2.npz"))
    Xv = d["X_val"].astype(np.float32)
    assert Xv.shape[1] == 302
    q = m.predict(Xv, batch_size=4096, verbose=0)
    s_kwh = (q[:, 2] - q[:, 0]) * norm["y_std"]
    S1 = float(np.percentile(s_kwh, 75))
    S2 = float(np.percentile(s_kwh, 95))

    obj = {
        "task": "T-310",
        "run_id": RUN_ID,
        "model": "MLP-Q 302-64-32-3 (q10,q50,q90)",
        "git_commit_sha": subprocess_git_sha(),
        "weights": {"W1": W1.tolist(), "b1": b1.tolist(),
                    "W2": W2.tolist(), "b2": b2.tolist(),
                    "W3": W3.tolist(), "b3": b3.tolist()},
        "norm": {"X_mean": norm["X_mean"], "X_std": norm["X_std"],
                 "y_mean": norm["y_mean"], "y_std": norm["y_std"]},
        "gate": {"S1_kwh": round(S1, 4), "S2_kwh": round(S2, 4),
                 "rule": "K_q=1 (s<=S1); 线性 (S1<s<S2); 0 (s>=S2); S 取 val P75/P95",
                 "hysteresis_windows": 3,
                 "upstream_class_id": 6},
    }
    # G2 识别器（workclf_v1）权重导出（T-308 产物）
    ck = os.path.join(MD, "..", "classifier", "workclf_v1.keras")
    mc = tf.keras.models.load_model(ck, safe_mode=False, compile=False)
    cl = [l for l in mc.layers]
    convs = [l for l in cl if "conv1d" in type(l).__name__.lower()]
    denses_c = [l for l in cl if isinstance(l, tf.keras.layers.Dense)]
    assert len(convs) == 3 and len(denses_c) == 3
    clf = {
        "conv1_w": convs[0].kernel.numpy().tolist(), "conv1_b": convs[0].bias.numpy().tolist(),
        "conv2_w": convs[1].kernel.numpy().tolist(), "conv2_b": convs[1].bias.numpy().tolist(),
        "conv3_w": convs[2].kernel.numpy().tolist(), "conv3_b": convs[2].bias.numpy().tolist(),
        "fc1_w": denses_c[0].kernel.numpy().tolist(), "fc1_b": denses_c[0].bias.numpy().tolist(),
        "fc2_w": denses_c[1].kernel.numpy().tolist(), "fc2_b": denses_c[1].bias.numpy().tolist(),
        "fc3_w": denses_c[2].kernel.numpy().tolist(), "fc3_b": denses_c[2].bias.numpy().tolist(),
    }
    obj["clf"] = clf

    out = os.path.join(OUT_DIR, "selected_model_weights_v2.json")
    json.dump(obj, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print("SAVED ->", out)
    print("gate S1=%.3f S2=%.3f kWh" % (S1, S2))

def subprocess_git_sha():
    try:
        import subprocess
        r = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                           cwd=os.path.join(BASE, "..", ".."))
        return r.stdout.decode().strip() or "unknown"
    except Exception:
        return "unknown"

if __name__ == "__main__":
    main()

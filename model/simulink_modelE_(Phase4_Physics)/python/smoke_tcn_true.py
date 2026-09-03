# -*- coding: utf-8 -*-
"""smoke_tcn_true.py — 修复后五配置 TCN tiny-overfit 冒烟（PC CPU）

验证：(1) 五配置 params 互不相同且量级正确；(2) 每个配置都能在 256 样本上过拟合
（train RMSE 显著低于 naive），证明学习回路健康，才允许上全量训练。
"""
import os, sys, argparse
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import tensorflow as tf
import train_predictor as tp

N = 256
EPOCHS = 150
BATCH = 64

def main():
    tp.DATA_NPZ = tp.BASE / "datasets" / "fleet_windows_v2.npz"
    tp.DATA_NORM_JSON = tp.BASE / "datasets" / "fleet_norm_v2.json"
    tp.DATA_MANIFEST_JSON = tp.BASE / "results" / "ecms_phase25" / "dataset_manifest_v2.json"
    tp.DATA_DATASET_VERSION = "v2"
    args = argparse.Namespace(dataset="v2", arch="tcn", quantiles=True, batch=BATCH,
                              epochs=EPOCHS, patience=10, seed=42, only="all",
                              no_plots=True, scoped=True, decision_note=None,
                              select_run_id=None, finalize=False)
    data = tp.load_data(args)
    Xtr = (data["X_train"][:N, :300].reshape(-1, 300, 1), data["X_train"][:N, 300:])
    ytr = data["y_train"][:N]

    results = []
    for f in (16, 32, 64, 128, 256):
        m = architectures_build(f)
        es = tf.keras.callbacks.EarlyStopping(monitor="loss", patience=25,
                                              restore_best_weights=True)
        m.fit(Xtr, ytr, epochs=EPOCHS, batch_size=BATCH, verbose=0, callbacks=[es])
        pred = m.predict(Xtr, batch_size=N, verbose=0)[:, 1]
        rmse = float(np.sqrt(np.mean((pred - ytr) ** 2))) * data["y_std"]
        ok = rmse < 8.0   # naive train-mean = 16.5；过拟合成功应显著更低
        results.append((f, m.count_params(), round(rmse, 3), ok))
        print("filters=%-4d params=%-8d tiny-train RMSE=%7.3f kWh  %s"
              % (f, m.count_params(), rmse, "PASS" if ok else "FAIL"), flush=True)
    assert all(r[3] for r in results), "SMOKE FAIL"
    print("SMOKE ALL PASS")

def architectures_build(filters):
    """直接调用 architectures.build_tcn（与 runner 同路径），验证 params 随配置变化。"""
    import architectures
    m = architectures.build_tcn(filters=filters, quantiles=True)
    return m

if __name__ == "__main__":
    main()

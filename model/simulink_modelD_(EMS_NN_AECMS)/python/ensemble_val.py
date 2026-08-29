# -*- coding: utf-8 -*-
"""ensemble_val.py — T-307 候选：MLP(point-MSE) × TCN(q50) val 预测集成分析

只在 val 上评估（test 纪律不动）。加权平均 w*MLP + (1-w)*TCN_q50，w ∈ {0.0..1.0}。
"""
import os, sys, argparse
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import tensorflow as tf
import train_predictor as tp

BASE = os.path.dirname(os.path.abspath(__file__))
MD = os.path.join(BASE, "..", "results", "ecms_phase25", "models")

def main():
    args = argparse.Namespace(dataset="v2", arch="tcn", quantiles=True,
                              batch=512, epochs=100, patience=10, seed=42,
                              only="all", no_plots=True, scoped=True,
                              decision_note=None, select_run_id=None, finalize=False)
    data = tp.load_data(args)
    yv = data["y_val"]
    mlp = tf.keras.models.load_model(os.path.join(
        MD, "mlp_h64_32_lr3e-4", "mlp_h64_32_lr3e-4.keras"), safe_mode=False)
    p_mlp = mlp.predict(data["X_val"], batch_size=4096, verbose=0).ravel()

    rows = []
    for run in ["tcn_f64", "tcn_f128"]:
        m = tf.keras.models.load_model(os.path.join(MD, run, run + ".keras"), safe_mode=False)
        Xv = (data["X_val"][:, :300].reshape(-1, 300, 1), data["X_val"][:, 300:])
        pr = m.predict(list(Xv), batch_size=4096, verbose=0)
        q50 = pr[:, 1]
        for w in (0.0, 0.3, 0.5, 0.7, 1.0):
            e = w * p_mlp + (1 - w) * q50
            mm = tp.val_metrics(e, yv, data["y_mean"], data["y_std"])
            rows.append((run, w, mm))
        # TCN 单独 q50 参照
        mm = tp.val_metrics(q50, yv, data["y_mean"], data["y_std"])
        rows.append((run + "(q50 only)", 0.0, mm))
        # MLP 单独参照
    mm = tp.val_metrics(p_mlp, yv, data["y_mean"], data["y_std"])
    rows.append(("mlp_only", 1.0, mm))

    print("%-22s %5s | RMSE %8s | MAE %7s | under %6s" % ("combo", "w_mlp", "kWh", "kWh", "freq"))
    for run, w, m in rows:
        print("%-22s %5.1f | %10.4f | %9.4f | %8.4f" % (
            run, w, m["val_rmse_kwh"], m["val_mae_kwh"], m["under_freq"]))

if __name__ == "__main__":
    main()

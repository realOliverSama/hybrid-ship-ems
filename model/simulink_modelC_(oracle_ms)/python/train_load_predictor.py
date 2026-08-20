#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
train_load_predictor.py — 船舶负荷预测神经网络训练程序（STM32N6 部署导向）
================================================================================

任务：用过去 W 秒的负荷/运动状态，预测未来 60 s 总负荷峰值相对当前负荷的增量
      delta [kW]（预测值 = 当前负荷 + max(delta,0)，保证不劣于 persistence），
      供 05_EMS_Controller 的预测前馈使用。

窗口自动搜索（v2.1）：
  默认启用：粗网格 {15, 30, 60, 90, 120} s + 二分细化（在最佳网格点与相邻点之间
  取中点复评，RMSE(w) 非严格单峰，故不用纯二分而采用"粗网格+细化"稳健策略），
  以 validation RMSE 选优，再用最佳窗口全量训练并导出。
  跳过：--no-sweep（使用 --window 指定值，默认 60）。

模型选型理由（STM32N6 / Cortex-M55 + Neural-ART NPU 约束）：
  - MLP(5W->64->32->1)，W=60 时约 21k 参数，int8 量化后约 21 KB；
  - 纯 Dense + ReLU，无 BatchNorm/RNN 状态，量化友好（X-CUBE-AI / ST Edge AI），
    且可在 MATLAB Function 块内用矩阵乘法做环内推理，
    保证 "Simulink 闭环测试" 与 "N6 上板" 使用同一组权重。

特征（5 通道 × W s @ 1 Hz，时间主序展平）：
  P_load_total_kW, v_cmd_mps, v_ship_mps, acceleration_mps2, x_ship_m
  （x_ship_m：固定航线下阻力区/桥区按里程出现，属当前状态，非未来信息）

数据：
  datasets/train.csv      —— 训练（S01/S02/S04/S07/S08，10 个完整航次）
  datasets/validation.csv —— 早停与窗口选优（S03/S05）
  datasets/test.csv       —— 最终诚实评估（S06 阶跃、S09 应急出动，训练中不可见）
  按完整航次划分（由 05_export_dataset.m 保证），窗口只在单航次内构造，无泄漏。

用法：
  .venv_tf/bin/python python/train_load_predictor.py              # 自动搜窗 + 训练
  .venv_tf/bin/python python/train_load_predictor.py --no-sweep --window 60
输出：
  python/export/load_predictor.keras / _fp32.tflite / _int8.tflite
  python/export/mlp_weights.mat          —— 权重+归一化统计+最佳窗口（MATLAB 环内推理）
  python/export/training_report.json     —— 指标与搜窗记录
  python/export/window_sweep.json        —— 搜窗明细
  reports/ai/training_curves.png / test_scenario_preview.png / window_sweep.png
"""

import argparse
import json
import os
import sys

import numpy as np
import pandas as pd
import tensorflow as tf
from scipy.io import savemat
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ---------------------------------------------------------------- 配置
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXPORT_DIR = os.path.join(ROOT, "python", "export")
REPORT_DIR = os.path.join(ROOT, "reports", "ai")
os.makedirs(EXPORT_DIR, exist_ok=True)
os.makedirs(REPORT_DIR, exist_ok=True)

FEATURE_COLS = ["P_load_total_kW", "v_cmd_mps", "v_ship_mps",
                "acceleration_mps2", "x_ship_m"]
LABEL_COL = "P_load_future_max_60s"
N_CH = len(FEATURE_COLS)
HIDDEN = (64, 32)
SEED = 42
BATCH = 256

COARSE_WINDOWS = [15, 30, 60, 90, 120]
SWEEP_EPOCHS = 60
FINAL_EPOCHS = 200


# ---------------------------------------------------------------- 数据构造
def build_windows(csv_path, window):
    """按航次（scenario_id × ems_mode）分组构造滑动窗口，杜绝跨航次泄漏。
    标签 delta = max(未来 60 s 负荷) − 当前负荷 >= 0。"""
    df = pd.read_csv(csv_path)
    X_list, y_list, meta_list = [], [], []
    for (scen, mode), g in df.groupby(["scenario_id", "ems_mode"]):
        g = g.sort_values("t").reset_index(drop=True)
        F = g[FEATURE_COLS].to_numpy(dtype=np.float64)
        y = g[LABEL_COL].to_numpy(dtype=np.float64)
        n = len(g)
        for end in range(window, n + 1):
            lbl = y[end - 1]
            if not np.isfinite(lbl):
                continue
            cur = F[end - 1, 0]
            X_list.append(F[end - window:end])
            y_list.append(max(lbl - cur, 0.0))
            meta_list.append((scen, mode, g["t"].iloc[end - 1], cur, lbl))
    if not X_list:
        return (np.zeros((0, window, N_CH)), np.zeros((0,)),
                np.zeros((0, 5)))
    return (np.stack(X_list).astype(np.float32),
            np.asarray(y_list, dtype=np.float32),
            np.asarray(meta_list, dtype=np.float64))


def flatten(X):
    """(N, W, 5) -> (N, 5W)，时间主序（与 MATLAB 环内 reshape(A.',1,[]) 一致）。"""
    return X.reshape(X.shape[0], -1)


def make_norm(Xtr):
    fm = flatten(Xtr).reshape(-1, Xtr.shape[1], N_CH).mean(axis=(0, 1))
    fs = flatten(Xtr).reshape(-1, Xtr.shape[1], N_CH).std(axis=(0, 1)) + 1e-9
    return fm, fs


def train_one(window, Xtr, ytr, Xva, yva, epochs, patience, verbose=0):
    """训练一个窗口配置的模型；返回 (model, history, val_pred_delta)。"""
    tf.random.set_seed(SEED)
    fm, fs = make_norm(Xtr)
    ym, ys = float(ytr.mean()), float(ytr.std() + 1e-9)
    Xtr_n = flatten((Xtr - fm) / fs)
    Xva_n = flatten((Xva - fm) / fs)

    inputs = tf.keras.Input(shape=(window * N_CH,), name="window")
    h = tf.keras.layers.Dense(HIDDEN[0], activation="relu", name="fc1")(inputs)
    h = tf.keras.layers.Dense(HIDDEN[1], activation="relu", name="fc2")(h)
    outputs = tf.keras.layers.Dense(1, name="fc3")(h)
    model = tf.keras.Model(inputs, outputs)
    model.compile(optimizer=tf.keras.optimizers.Adam(1e-3), loss="mse")
    cb = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_loss", patience=patience, restore_best_weights=True),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss", factor=0.5, patience=max(4, patience // 2),
            min_lr=1e-5),
    ]
    hist = model.fit(Xtr_n, (ytr - ym) / ys,
                     validation_data=(Xva_n, (yva - ym) / ys),
                     epochs=epochs, batch_size=BATCH, verbose=verbose,
                     callbacks=cb)
    pred_delta = np.maximum(
        model.predict(Xva_n, batch_size=1024, verbose=0).ravel() * ys + ym, 0.0)
    return model, hist, pred_delta, (fm, fs, ym, ys)


def rmse(a, b):
    return float(np.sqrt(np.mean((np.asarray(a) - np.asarray(b)) ** 2)))


def mae(a, b):
    return float(np.mean(np.abs(np.asarray(a) - np.asarray(b))))


def val_rmse_of(pred_delta, Xva, yva):
    """delta 预测 -> 峰值预测与真值 RMSE（与 persistence 可比）。"""
    cur = Xva[:, -1, 0]
    label = cur + yva
    return rmse(cur + pred_delta, label)


# ---------------------------------------------------------------- 主流程
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-sweep", action="store_true", help="跳过窗口搜索")
    ap.add_argument("--window", type=int, default=60, help="no-sweep 时的窗口 [s]")
    args = ap.parse_args()

    print("=" * 70)
    print("负荷预测网络训练（STM32N6 导向 MLP，窗口自动搜索）")
    print("=" * 70)

    sweep_log = []
    if args.no_sweep:
        best_window = args.window
        print(f"[搜窗] 跳过，使用窗口 {best_window} s")
    else:
        # ---- 粗网格 ----
        print(f"[搜窗] 粗网格 {COARSE_WINDOWS} s（val RMSE 选优）...")
        coarse = {}
        for w in COARSE_WINDOWS:
            Xtr, ytr, _ = build_windows(
                os.path.join(ROOT, "datasets", "train.csv"), w)
            Xva, yva, _ = build_windows(
                os.path.join(ROOT, "datasets", "validation.csv"), w)
            _, _, pd_, _ = train_one(w, Xtr, ytr, Xva, yva,
                                     SWEEP_EPOCHS, patience=12)
            r = val_rmse_of(pd_, Xva, yva)
            coarse[w] = r
            sweep_log.append({"window": w, "val_rmse": r, "phase": "coarse"})
            print(f"  W={w:4d} s: val RMSE={r:.2f} kW (样本 {len(Xtr)})")
        best_window = min(coarse, key=coarse.get)
        # ---- 二分细化（粗网格最佳点与相邻点之间取中点） ----
        ws = sorted(coarse)
        i = ws.index(best_window)
        cands = set()
        if i > 0:
            cands.add((ws[i - 1] + best_window) // 2)
        if i < len(ws) - 1:
            cands.add((best_window + ws[i + 1]) // 2)
        cands -= set(ws)
        for w in sorted(cands):
            Xtr, ytr, _ = build_windows(
                os.path.join(ROOT, "datasets", "train.csv"), w)
            Xva, yva, _ = build_windows(
                os.path.join(ROOT, "datasets", "validation.csv"), w)
            _, _, pd_, _ = train_one(w, Xtr, ytr, Xva, yva,
                                     SWEEP_EPOCHS, patience=12)
            r = val_rmse_of(pd_, Xva, yva)
            sweep_log.append({"window": w, "val_rmse": r, "phase": "refine"})
            print(f"  W={w:4d} s: val RMSE={r:.2f} kW (细化)")
            if r < coarse[best_window]:
                coarse[w] = r
                best_window = w
        print(f"[搜窗] 最佳窗口 = {best_window} s "
              f"(val RMSE={coarse[best_window]:.2f} kW)")
        with open(os.path.join(EXPORT_DIR, "window_sweep.json"), "w",
                  encoding="utf-8") as f:
            json.dump({"best_window": best_window, "log": sweep_log},
                      f, ensure_ascii=False, indent=2)

    # ---- 最佳窗口全量训练 ----
    W = best_window
    print(f"[训练] 最终模型：窗口 {W} s ...")
    Xtr, ytr, _ = build_windows(os.path.join(ROOT, "datasets", "train.csv"), W)
    Xva, yva, mva = build_windows(
        os.path.join(ROOT, "datasets", "validation.csv"), W)
    Xte, yte, mte = build_windows(os.path.join(ROOT, "datasets", "test.csv"), W)
    print(f"[数据] train {Xtr.shape}, val {Xva.shape}, test {Xte.shape}")
    if len(Xtr) < 100:
        print("[错误] 训练样本过少，请先在 MATLAB 中运行 04/05 号脚本")
        sys.exit(1)

    model, hist, _, (fm, fs, ym, ys) = train_one(
        W, Xtr, ytr, Xva, yva, FINAL_EPOCHS, patience=20, verbose=2)

    def evaluate(X, y):
        fm_b = fm
        Xn = flatten((X - fm_b) / fs)
        cur = X[:, -1, 0]
        label = cur + y
        pred = cur + np.maximum(
            model.predict(Xn, batch_size=1024, verbose=0).ravel() * ys + ym, 0.0)
        return {"mae_kw": mae(pred, label), "rmse_kw": rmse(pred, label),
                "persistence_mae_kw": mae(cur, label),
                "persistence_rmse_kw": rmse(cur, label)}, pred

    met_va, pred_va = evaluate(Xva, yva)
    met_te, pred_te = evaluate(Xte, yte)
    print(f"[评估] validation: MAE={met_va['mae_kw']:.1f} RMSE={met_va['rmse_kw']:.1f} kW"
          f" | persistence RMSE={met_va['persistence_rmse_kw']:.1f} kW")
    print(f"[评估] test:       MAE={met_te['mae_kw']:.1f} RMSE={met_te['rmse_kw']:.1f} kW"
          f" | persistence RMSE={met_te['persistence_rmse_kw']:.1f} kW")

    # ---- 导出 Keras / TFLite ----
    keras_path = os.path.join(EXPORT_DIR, "load_predictor.keras")
    model.save(keras_path)
    fp32_path = os.path.join(EXPORT_DIR, "load_predictor_fp32.tflite")
    with open(fp32_path, "wb") as f:
        f.write(tf.lite.TFLiteConverter.from_keras_model(model).convert())

    Xtr_n = flatten((Xtr - fm) / fs)

    def rep_dataset():
        idx = np.random.choice(len(Xtr_n), size=min(512, len(Xtr_n)),
                               replace=False)
        for i in idx:
            yield [Xtr_n[i:i + 1].astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = rep_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    int8_path = os.path.join(EXPORT_DIR, "load_predictor_int8.tflite")
    with open(int8_path, "wb") as f:
        f.write(converter.convert())

    # int8 全量验证集复核
    interpreter = tf.lite.Interpreter(model_path=int8_path)
    interpreter.allocate_tensors()
    id_, od_ = interpreter.get_input_details()[0], \
        interpreter.get_output_details()[0]
    s, z = float(id_["quantization"][0]), float(id_["quantization"][1])
    os_, oz = float(od_["quantization"][0]), float(od_["quantization"][1])
    Xva_n = flatten((Xva - fm) / fs).astype(np.float32)
    preds = np.zeros(len(Xva_n), dtype=np.float32)
    for i in range(len(Xva_n)):
        xq = np.clip(np.round(Xva_n[i:i + 1] / s + z), -128, 127).astype(np.int8)
        interpreter.set_tensor(id_["index"], xq)
        interpreter.invoke()
        preds[i] = (float(interpreter.get_tensor(od_["index"]).ravel()[0])
                    - oz) * os_
    cur_v = Xva[:, -1, 0]
    pred_q = cur_v + np.maximum(preds * ys + ym, 0.0)
    mae_q = mae(pred_q, cur_v + yva)
    rmse_q = rmse(pred_q, cur_v + yva)
    print(f"[量化复核] int8 全量 validation: MAE={mae_q:.2f} RMSE={rmse_q:.2f} kW")

    # ---- 导出 MATLAB 环内推理权重 ----
    W1, b1 = model.get_layer("fc1").get_weights()
    W2, b2 = model.get_layer("fc2").get_weights()
    W3, b3 = model.get_layer("fc3").get_weights()
    mat_path = os.path.join(EXPORT_DIR, "mlp_weights.mat")
    savemat(mat_path, {
        "MLP_W1": W1.astype(np.float64), "MLP_b1": b1.reshape(1, -1),
        "MLP_W2": W2.astype(np.float64), "MLP_b2": b2.reshape(1, -1),
        "MLP_W3": W3.astype(np.float64), "MLP_b3": b3.reshape(1, -1),
        "MLP_FeatMean": fm.reshape(1, -1), "MLP_FeatStd": fs.reshape(1, -1),
        "MLP_YMean": np.array([[ym]]), "MLP_YStd": np.array([[ys]]),
        "MLP_Window": np.array([[W]]), "MLP_NCh": np.array([[N_CH]]),
        "MLP_FeatureCols": np.array(FEATURE_COLS, dtype=object),
    })
    print(f"[导出] {keras_path}")
    print(f"[导出] {fp32_path} ({os.path.getsize(fp32_path)/1024:.1f} KB)")
    print(f"[导出] {int8_path} ({os.path.getsize(int8_path)/1024:.1f} KB)")
    print(f"[导出] {mat_path}")

    # ---- 图表 ----
    plt.rcParams["font.sans-serif"] = ["Arial Unicode MS", "PingFang SC",
                                       "Heiti SC", "DejaVu Sans"]
    plt.rcParams["axes.unicode_minus"] = False

    fig, ax = plt.subplots(1, 2, figsize=(11, 4))
    ax[0].plot(hist.history["loss"], label="train")
    ax[0].plot(hist.history["val_loss"], label="validation")
    ax[0].set_xlabel("epoch"); ax[0].set_ylabel("MSE (归一化)")
    ax[0].set_title(f"训练曲线 (W={W} s)"); ax[0].legend(); ax[0].grid(alpha=.3)
    ax[1].scatter(mva[:, 4], pred_va, s=4, alpha=.4, label="validation")
    ax[1].scatter(mte[:, 4], pred_te, s=4, alpha=.4, label="test (S06/S09)")
    lim = [0, float(max(mva[:, 4].max(), mte[:, 4].max())) * 1.05]
    ax[1].plot(lim, lim, "k--", lw=1)
    ax[1].set_xlabel("真实未来 60 s 负荷峰值 [kW]")
    ax[1].set_ylabel("预测值 [kW]"); ax[1].set_title("预测精度")
    ax[1].legend(); ax[1].grid(alpha=.3)
    fig.tight_layout()
    p1 = os.path.join(REPORT_DIR, "training_curves.png")
    fig.savefig(p1, dpi=140); plt.close(fig)

    # 搜窗图
    if sweep_log:
        fig, ax = plt.subplots(figsize=(7, 4))
        ws_ = [e["window"] for e in sweep_log]
        rs_ = [e["val_rmse"] for e in sweep_log]
        order = np.argsort(ws_)
        ax.plot(np.asarray(ws_)[order], np.asarray(rs_)[order], "o-")
        ax.axvline(best_window, color="r", ls="--",
                   label=f"最佳窗口 {best_window} s")
        ax.set_xlabel("历史窗口 W [s]"); ax.set_ylabel("validation RMSE [kW]")
        ax.set_title("窗口自动搜索（粗网格+二分细化）")
        ax.legend(); ax.grid(alpha=.3)
        fig.tight_layout()
        p3 = os.path.join(REPORT_DIR, "window_sweep.png")
        fig.savefig(p3, dpi=140); plt.close(fig)
        print(f"[图表] {p3}")

    # 测试航次时间历程（S09 优先，其次第一个）
    sel = (mte[:, 0] == 9) & (mte[:, 1] == 0)
    if sel.sum() < 10:
        scen0, mode0 = mte[0, 0], mte[0, 1]
        sel = (mte[:, 0] == scen0) & (mte[:, 1] == mode0)
    fig, ax = plt.subplots(figsize=(11, 4))
    ax.plot(mte[sel, 2], mte[sel, 4], label="真实未来 60 s 峰值", lw=1.2)
    ax.plot(mte[sel, 2], pred_te[sel], label="MLP 预测", lw=1.2)
    ax.plot(mte[sel, 2], mte[sel, 3], label="当前负荷 (persistence)",
            lw=.9, alpha=.7)
    ax.set_xlabel("航次内时间 t [s]"); ax.set_ylabel("P_load [kW]")
    sc_id = int(mte[sel, 0][0])
    ax.set_title(f"测试航次 S{sc_id:02d} 预测预览 (W={W} s)")
    ax.legend(); ax.grid(alpha=.3)
    fig.tight_layout()
    p2 = os.path.join(REPORT_DIR, "test_scenario_preview.png")
    fig.savefig(p2, dpi=140); plt.close(fig)
    print(f"[图表] {p1}")
    print(f"[图表] {p2}")

    # ---- 报告 JSON ----
    report = {
        "model": f"MLP {W*N_CH}-64-32-1 (Dense+ReLU)",
        "best_window_s": int(W),
        "window_sweep": sweep_log,
        "params": int(model.count_params()),
        "channels": FEATURE_COLS,
        "label": LABEL_COL + "（训练目标为相对当前负荷的增量 delta）",
        "train_samples": int(len(Xtr)), "val_samples": int(len(Xva)),
        "test_samples": int(len(Xte)),
        "metrics": {"validation": met_va, "test": met_te,
                    "int8_validation": {"mae_kw": mae_q, "rmse_kw": rmse_q}},
        "exports": {"keras": keras_path, "tflite_fp32": fp32_path,
                    "tflite_int8": int8_path, "matlab_weights": mat_path},
        "stm32n6_notes": (
            "将 load_predictor_int8.tflite 导入 ST Edge AI Developer Cloud "
            "或本地 stedgeai (target=stm32n6) 生成 C 代码；输入为 5W 维 int8 "
            "时间主序窗口（归一化参数见 mlp_weights.mat 中 MLP_FeatMean/Std，"
            "需在固件预处理中实现 z-score；输出反归一化用 MLP_YMean/YStd）。"
        ),
    }
    rpt_path = os.path.join(EXPORT_DIR, "training_report.json")
    with open(rpt_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print(f"[报告] {rpt_path}")
    print(f"[完成] 最佳窗口 {W} s，训练与导出全部完成")


if __name__ == "__main__":
    main()

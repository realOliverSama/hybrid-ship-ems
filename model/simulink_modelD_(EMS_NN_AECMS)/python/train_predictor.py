#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
train_predictor.py — T-203: MLP/LSTM 训练 + val 选模（nn-predictive-ecms-phase2）
=================================================================================

任务: 用过去 300 s P_demand 窗口（+ v_ship, accel）预测未来 300 s 能量需求
      Ê_H(kWh)。训练在归一化空间（z-score，fit-on-train），指标反归一化回 kWh。

实验网格（§10 批准范围，共 6 组，AI_MODELING §14 公平性）:
  MLP (主):   hidden ∈ {(64,32), (128,64)} × lr ∈ {1e-3, 3e-4}
  LSTM(对照): units ∈ {32, 64} × lr = 1e-3；输入 reshape [300,1]（仅 P_demand，
              v_ship/accel 不进入 LSTM —— design §4 批准口径，Notes 已声明差异）

选模: val RMSE 最小者入选；test 集不参与任何决策（T-205 一次性使用）。
失败/发散实验保留在 training_log.csv，不得删除（AI_MODELING §18/19）。

两个阶段（脚本可从 .keras 恢复，无需重训）:
  Phase 1 完整跑:
    .venv_tf/bin/python python/train_predictor.py [--epochs 100 --patience 10 --batch 256]
      → training_log.csv / plots / models/<key>/<key>.keras / selection.json
      （若触发 Escalate 条件，打印 ESCALATE_REQUIRED 行，selection.json decision_status=pending）
  Phase 2 定稿（按主 Agent 裁决重新生成最终产物）:
    .venv_tf/bin/python python/train_predictor.py --finalize --select-run-id <key> \
        [--decision-note "main agent: ..."]

数据可能被重建（fleet 扩展至 200 航次，split 变 140/30/30，路径不变）——
本脚本不硬编码航次数/比例，直接读取 npz 中 X_train/y_train/X_val/y_val。

T-304（nn-predictive-ecms-phase25）扩展：
  --arch {mlp,tcn,cnn_mlp,nbeats}（逗号分隔可多选，默认 "mlp,lstm"=v1 矩阵）
  --dataset {v1,v2}（默认 v1）
  --quantiles（默认关：单输出 MSE；开：q10/q50/q90 pinball 三头）
输出隔离：
  默认（无新参数）= v1 逐位一致（写 results/ecms_phase2/*，training_log.csv）
  显式 --arch/--dataset/--quantiles（v1 回归或 v2）= 写隔离目录
    v1 回归（--arch mlp --dataset v1）-> results/ecms_phase2/models_repro_t304/ + training_log_v2.csv
    v2（--dataset v2）            -> results/ecms_phase25/models/ + training_log_v2.csv
  Phase 2 既有文件（training_log.csv / selection.json / results/ecms_phase2/models/*）绝不读写。
  编码：全部 JSON/CSV 读写显式 encoding='utf-8'（本机 GBK 控制台下 read_text() 默认
  locale 编码会 UnicodeDecodeError——T-304 回归阻断修复）。
"""

import argparse
import csv
import hashlib
import json
import os
import random
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

# 确定性（TF 导入前设置）
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")

import numpy as np
import tensorflow as tf
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

import architectures   # Phase 2.5 模型族（T-304，同目录）

BASE = Path(__file__).resolve().parent.parent  # model/simulink_modelD_(EMS_NN_AECMS)
NPZ = BASE / "datasets" / "fleet_windows.npz"
NORM_JSON = BASE / "datasets" / "fleet_norm.json"
MANIFEST_JSON = BASE / "results" / "ecms_phase2" / "dataset_manifest.json"
OUT_DIR = BASE / "results" / "ecms_phase2"
MODEL_DIR = OUT_DIR / "models"
PLOT_DIR = OUT_DIR / "plots"
LOG_CSV = OUT_DIR / "training_log.csv"

# ---- T-304 输出隔离：作用域相关全局（默认 = v1 路径，逐位一致） ----
SELECTION_NAME = "selection.json"      # v1 默认文件名
WEIGHTS_NAME = "selected_model_weights.json"
CSV_FIELDNAMES_ACTIVE = None           # None => 用 CSV_FIELDNAMES（v1）
DATA_NPZ = NPZ
DATA_NORM_JSON = NORM_JSON
DATA_MANIFEST_JSON = MANIFEST_JSON
DATA_DATASET_VERSION = "v1"

SEED = 42
SCRIPT_VERSION = "1.3.0"    # v1.3: T-304 --arch/--dataset/--quantiles（默认路径逐位不变）+ utf-8 读写
CSV_FIELDNAMES = [
    "run_id", "model", "hyperparameters", "dataset", "dataset_version", "split",
    "input", "history_window", "forecast_horizon", "target", "seed",
    "best_epoch", "val_loss_norm", "val_mae_kwh", "val_rmse_kwh",
    "under_freq", "under_mag_kwh", "over_mag_kwh",
    "n_params", "size_kb_fp32", "est_ram_kb", "infer_ms_tf", "infer_ms_numpy_inline",
    "runtime_s", "metrics", "model_size", "export", "status", "notes", "run_utc",
]

NUMERIC_FIELDS = {
    "best_epoch", "val_loss_norm", "val_mae_kwh", "val_rmse_kwh",
    "under_freq", "under_mag_kwh", "over_mag_kwh",
    "n_params", "size_kb_fp32", "est_ram_kb",
    "infer_ms_tf", "infer_ms_numpy_inline", "runtime_s",
}


def git_sha():
    try:
        r = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=str(BASE.parent),
            capture_output=True, text=True, timeout=10)
        return r.stdout.strip() or "Unknown"
    except Exception:
        return "Unknown"


def load_data(args):
    """读取 npz + norm + manifest（按 --dataset 作用域路径），返回数组与 §19 元数据。"""
    npz_path = DATA_NPZ
    norm_path = DATA_NORM_JSON
    man_path = DATA_MANIFEST_JSON
    if not npz_path.exists():
        raise FileNotFoundError("缺失数据集: %s" % npz_path)
    if not norm_path.exists():
        raise FileNotFoundError("缺失归一化参数: %s" % norm_path)

    d = np.load(npz_path)
    norm = json.loads(norm_path.read_text(encoding="utf-8"))
    md5 = hashlib.md5(npz_path.read_bytes()).hexdigest()

    manifest = {}
    if man_path.exists():
        m = json.loads(man_path.read_text(encoding="utf-8"))
        meta = m.get("meta", {})
        manifest = {
            "task": meta.get("task"),
            "git_commit_sha": meta.get("git_commit_sha"),
            "script_version": meta.get("script_version"),
            "generated_utc": meta.get("generated_utc"),
        }

    n_train_v = len(np.unique(d["voyage_ids_train"]))
    n_val_v = len(np.unique(d["voyage_ids_val"]))
    n_test_v = len(np.unique(d["voyage_ids_test"]))
    split_str = "train %d 窗/%d 航次, val %d 窗/%d 航次, test %d 窗/%d 航次（航次级, seed=%s）" % (
        len(d["y_train"]), n_train_v, len(d["y_val"]), n_val_v,
        len(d["y_test"]), n_test_v, norm.get("split_seed"))

    ds_name = npz_path.name
    return {
        "X_train": d["X_train"], "y_train": d["y_train"],
        "X_val": d["X_val"], "y_val": d["y_val"],
        "X_test": d["X_test"], "y_test": d["y_test"],
        "y_mean": float(norm["y_mean"]), "y_std": float(norm["y_std"]),
        "X_mean": np.asarray(norm["X_mean"], dtype=np.float64),
        "X_std": np.asarray(norm["X_std"], dtype=np.float64),
        "npz_md5": md5, "manifest": manifest, "norm": norm,
        "dataset_str": "%s（N=%d 航次, md5=%s, manifest commit=%s）" % (
            ds_name, n_train_v + n_val_v + n_test_v, md5[:12],
            manifest.get("git_commit_sha")),
        "dataset_version": "%s npz md5=%s | build_dataset %s @ commit %s | generated %s" % (
            DATA_DATASET_VERSION, md5, manifest.get("script_version"),
            manifest.get("git_commit_sha"), manifest.get("generated_utc")),
        "split": split_str,
        "input_desc": "302 维 = [P_demand(t-299..t) x300（1 s 升序）+ v_ship(t) + accel(t)]",
        "history_window": "300 s（1 s 采样）",
        "forecast_horizon": "300 s",
        "target": "Ê_H(t) = sum_{s=t+1..t+300} P_demand(s)/3600 [kWh]",
    }


# ---------------------------------------------------------------- 模型构建

def build_mlp(hidden, lr):
    inp = tf.keras.Input(shape=(302,), name="P_in_302")
    x = inp
    for h in hidden:
        x = tf.keras.layers.Dense(h, activation="relu")(x)
    out = tf.keras.layers.Dense(1, name="E_hat_norm")(x)
    model = tf.keras.Model(inp, out)
    model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=lr), loss="mse")
    return model


def build_lstm(units, lr):
    inp = tf.keras.Input(shape=(300, 1), name="P_window_300x1")
    x = tf.keras.layers.LSTM(units)(inp)
    out = tf.keras.layers.Dense(1, name="E_hat_norm")(x)
    model = tf.keras.Model(inp, out)
    model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=lr), loss="mse")
    return model


# ---------------------------------------------------------------- 指标与规模

def val_metrics(pred_norm, true_norm, y_mean, y_std):
    """val 指标，反归一化回 kWh。低估 = 预测 < 真实（未来能量需求被低估）。"""
    pred = pred_norm.astype(np.float64) * y_std + y_mean
    true = true_norm.astype(np.float64) * y_std + y_mean
    err = pred - true
    rmse = float(np.sqrt(np.mean(err ** 2)))
    mae = float(np.mean(np.abs(err)))
    under = err < 0.0
    under_freq = float(np.mean(under))
    under_mag = float(np.mean(true[under] - pred[under])) if under.any() else 0.0
    over_mag = float(np.mean(pred[~under] - true[~under])) if (~under).any() else 0.0
    return {
        "val_mae_kwh": round(mae, 4), "val_rmse_kwh": round(rmse, 4),
        "under_freq": round(under_freq, 4),
        "under_mag_kwh": round(under_mag, 4), "over_mag_kwh": round(over_mag, 4),
    }


def val_metrics_quantiles(pred_norm, true_norm, y_mean, y_std):
    """分位数模型 val 指标：q50 反归一化计算 RMSE/MAE（与单输出可比），
    另加 q10/q90 覆盖率与平均 pinball（归一化空间）。pred_norm shape (N,3)。"""
    q = pred_norm.astype(np.float64)          # (N,3): q10/q50/q90
    true = true_norm.astype(np.float64)
    q50 = q[:, 1]
    pred = q50 * y_std + y_mean
    t_kwh = true * y_std + y_mean
    err = pred - t_kwh
    rmse = float(np.sqrt(np.mean(err ** 2)))
    mae = float(np.mean(np.abs(err)))
    under = err < 0.0
    under_freq = float(np.mean(under))
    under_mag = float(np.mean(t_kwh[under] - pred[under])) if under.any() else 0.0
    over_mag = float(np.mean(pred[~under] - t_kwh[~under])) if (~under).any() else 0.0
    # 覆盖率：q10 <= y <= q90
    q10 = q[:, 0]
    q90 = q[:, 2]
    coverage = float(np.mean((q10 <= true) & (true <= q90)))
    # pinball（归一化空间，三头平均）
    e10 = true - q10
    e50 = true - q50
    e90 = true - q90
    pb = (np.mean(np.maximum(0.1 * e10, -0.9 * e10)) +
          np.mean(np.maximum(0.5 * e50, -0.5 * e50)) +
          np.mean(np.maximum(0.9 * e90, -0.1 * e90))) / 3.0
    return {
        "val_mae_kwh": round(mae, 4), "val_rmse_kwh": round(rmse, 4),
        "under_freq": round(under_freq, 4),
        "under_mag_kwh": round(under_mag, 4), "over_mag_kwh": round(over_mag, 4),
        "q_coverage": round(coverage, 4), "q_pinball_norm": round(float(pb), 6),
    }


def est_activation_bytes(model, input_shape):
    """粗略激活内存估算（batch=1, fp32），含 LSTM h/c 状态。"""
    total = int(np.prod(input_shape)) * 4
    x = tf.zeros((1,) + tuple(input_shape))
    for layer in model.layers:
        if isinstance(layer, tf.keras.layers.InputLayer):
            continue
        try:
            x = layer(x)
            total += int(np.prod(x.shape[1:])) * 4
        except Exception:
            pass
        if isinstance(layer, tf.keras.layers.LSTM):
            total += 4 * 2 * layer.units
    return total


def time_tf_forward(model, x1, warmup=20, n=300):
    for _ in range(warmup):
        model(x1, training=False)
    t0 = time.perf_counter()
    for _ in range(n):
        model(x1, training=False)
    return (time.perf_counter() - t0) / n * 1000.0


def _np_mlp_forward(x, W1, b1, W2, b2, W3, b3):
    h = np.maximum(x @ W1 + b1, 0.0)
    h = np.maximum(h @ W2 + b2, 0.0)
    return h @ W3 + b3


def time_np_forward_mlp(x, weights, warmup=20, n=300):
    W1f = weights[0][0].astype(np.float32)
    b1f = weights[0][1].astype(np.float32)
    W2f = weights[1][0].astype(np.float32)
    b2f = weights[1][1].astype(np.float32)
    W3f = weights[2][0].astype(np.float32)
    b3f = weights[2][1].astype(np.float32)
    xf = np.asarray(x, dtype=np.float32)
    for _ in range(warmup):
        _np_mlp_forward(xf, W1f, b1f, W2f, b2f, W3f, b3f)
    t0 = time.perf_counter()
    for _ in range(n):
        _np_mlp_forward(xf, W1f, b1f, W2f, b2f, W3f, b3f)
    return (time.perf_counter() - t0) / n * 1000.0


# ---------------------------------------------------------------- 单组实验

def run_experiment(key, cfg, data, args, run_utc, group_idx, n_groups):
    random.seed(args.seed)
    np.random.seed(args.seed)
    tf.keras.utils.set_random_seed(args.seed)

    rec = {
        "run_id": key,
        "model": cfg["model"],
        "hyperparameters": cfg["hp"],
        "dataset": data["dataset_str"],
        "dataset_version": data["dataset_version"],
        "split": data["split"],
        "input": data["input_desc"],
        "history_window": data["history_window"],
        "forecast_horizon": data["forecast_horizon"],
        "target": data["target"],
        "seed": args.seed,
        "best_epoch": "", "val_loss_norm": "",
        "val_mae_kwh": "", "val_rmse_kwh": "",
        "under_freq": "", "under_mag_kwh": "", "over_mag_kwh": "",
        "n_params": "", "size_kb_fp32": "", "est_ram_kb": "",
        "infer_ms_tf": "", "infer_ms_numpy_inline": "",
        "runtime_s": "", "metrics": "", "model_size": "", "export": "",
        "status": "RUN", "notes": cfg["notes"], "run_utc": run_utc,
        "hidden": cfg.get("hidden"), "lr": cfg.get("lr"),
        "arch": cfg["kind"], "quantiles": args.quantiles,
    }

    t0 = time.time()
    model, hist = None, None
    try:
        kind = cfg["kind"]
        Xtr, Xva = data["X_train"], data["X_val"]
        if kind == "mlp":
            model = build_mlp(cfg["hidden"], cfg["lr"])
        elif kind == "lstm":
            Xtr = data["X_train"][:, :300].reshape(-1, 300, 1)
            Xva = data["X_val"][:, :300].reshape(-1, 300, 1)
            model = build_lstm(cfg["units"], cfg["lr"])
        elif kind in ("tcn", "cnn_mlp", "nbeats"):
            # 分路输入：P (N,300,1) + 标量 v/accel (N,2)
            Xtr = (data["X_train"][:, :300].reshape(-1, 300, 1),
                   data["X_train"][:, 300:])
            Xva = (data["X_val"][:, :300].reshape(-1, 300, 1),
                   data["X_val"][:, 300:])
            builder = architectures.BUILDERS[kind]
            model = builder(lr=cfg["lr"], quantiles=args.quantiles)
        else:
            raise ValueError("未知 arch: %s" % kind)

        es = tf.keras.callbacks.EarlyStopping(
            monitor="val_loss", patience=args.patience,
            restore_best_weights=True, verbose=0)
        hist = model.fit(
            Xtr, data["y_train"], validation_data=(Xva, data["y_val"]),
            epochs=args.epochs, batch_size=args.batch,
            callbacks=[es], verbose=0)

        vl = np.asarray(hist.history["val_loss"], dtype=np.float64)
        if not np.all(np.isfinite(vl)):
            raise ValueError("val loss 发散（含 NaN/Inf）")

        best_epoch = int(np.argmin(vl)) + 1
        pred = model.predict(Xva, batch_size=4096, verbose=0)
        if args.quantiles and kind in ("tcn", "cnn_mlp", "nbeats"):
            m = val_metrics_quantiles(pred, data["y_val"], data["y_mean"], data["y_std"])
        else:
            m = val_metrics(pred.ravel(), data["y_val"], data["y_mean"], data["y_std"])

        n_params = int(model.count_params())
        w_bytes = n_params * 4
        if kind == "mlp":
            act_bytes = est_activation_bytes(model, Xtr.shape[1:])
        else:
            act_bytes = 0   # 新架构：激活内存估算留待 T-307 NPU 环节，此处仅权重
        ms_tf = ""
        ms_np = ""
        if kind in ("tcn", "cnn_mlp", "nbeats"):
            # 双输入：time_tf_forward 需完整输入元组（各取 1 样本）
            ms_tf = round(time_tf_forward(
                model, ([Xva[0][:1]], [Xva[1][:1]])), 4)
        elif kind == "mlp":
            ms_tf = round(time_tf_forward(model, Xva[:1]), 4)
            dense_layers = [l for l in model.layers if isinstance(l, tf.keras.layers.Dense)]
            weights = [l.get_weights() for l in dense_layers]
            ms_np = round(time_np_forward_mlp(Xva[:1], weights), 5)

        outdir = MODEL_DIR / key
        outdir.mkdir(parents=True, exist_ok=True)
        keras_path = outdir / (key + ".keras")
        model.save(str(keras_path))

        rec.update({
            "best_epoch": best_epoch,
            "val_loss_norm": round(float(vl[best_epoch - 1]), 6),
            "val_mae_kwh": m["val_mae_kwh"], "val_rmse_kwh": m["val_rmse_kwh"],
            "under_freq": m["under_freq"], "under_mag_kwh": m["under_mag_kwh"],
            "over_mag_kwh": m["over_mag_kwh"],
            "n_params": n_params,
            "size_kb_fp32": round(w_bytes / 1024.0, 1),
            "est_ram_kb": round((w_bytes + act_bytes) / 1024.0, 1),
            "infer_ms_tf": ms_tf, "infer_ms_numpy_inline": ms_np,
            "metrics": ("val MAE=%.3f kWh, RMSE=%.3f kWh, under_freq=%.3f, "
                        "under_mag=%.3f kWh, over_mag=%.3f kWh%s" % (
                            m["val_mae_kwh"], m["val_rmse_kwh"], m["under_freq"],
                            m["under_mag_kwh"], m["over_mag_kwh"],
                            (", q10-q90 coverage=%.3f, pinball=%.4f" % (
                                m["q_coverage"], m["q_pinball_norm"]))
                            if "q_coverage" in m else "")),
            "model_size": ("%d params | %.1f KB fp32 | est RAM ~%.1f KB | "
                           "TF %s ms/sample%s" % (
                               n_params, w_bytes / 1024.0,
                               (w_bytes + act_bytes) / 1024.0, ms_tf,
                               (" | numpy-inline %s ms/sample" % ms_np) if ms_np != "" else "")),
            "export": str(keras_path.relative_to(BASE)),
            "status": "OK",
        })
    except Exception as e:
        rec["status"] = "FAIL"
        rec["notes"] = (rec["notes"] + " | " if rec["notes"] else "") + "%s: %s" % (
            type(e).__name__, e)

    rec["runtime_s"] = round(time.time() - t0, 1)
    print("[%d/%d] %-22s status=%-4s rmse=%s mae=%s runtime=%ss" % (
        group_idx, n_groups, key, rec["status"], rec["val_rmse_kwh"],
        rec["val_mae_kwh"], rec["runtime_s"]), flush=True)
    return rec, model, hist


# ---------------------------------------------------------------- 输出

def write_csv(records):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    fieldnames = CSV_FIELDNAMES_ACTIVE if CSV_FIELDNAMES_ACTIVE is not None else CSV_FIELDNAMES
    is_new = (not LOG_CSV.exists()) or LOG_CSV.stat().st_size == 0
    with open(LOG_CSV, "w" if is_new else "a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        if is_new:
            w.writeheader()
        for r in records:
            w.writerow(r)
    print("%s -> %s（append=%s，历史记录保留不覆盖）"
          % (LOG_CSV.name, LOG_CSV, not is_new), flush=True)


def read_records_from_csv():
    records = []
    with open(LOG_CSV, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            for k in NUMERIC_FIELDS:
                if row.get(k) not in (None, ""):
                    try:
                        row[k] = float(row[k])
                    except ValueError:
                        pass
            records.append(row)
    return records


def make_plots(records, histories, data):
    PLOT_DIR.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    for key, hist in histories.items():
        if hist is None:
            continue
        ax.plot(hist.history["val_loss"], label=key, linewidth=1.2)
    ax.set_yscale("log")
    ax.set_xlabel("epoch")
    ax.set_ylabel("val loss (MSE, 归一化空间)")
    ax.set_title("T-203 6 组实验 val 训练曲线")
    ax.legend(fontsize=7, ncol=2)
    fig.tight_layout()
    fig.savefig(PLOT_DIR / "training_curves.png", dpi=110)
    plt.close(fig)

    ok = [r for r in records if r.get("status") == "OK"]
    if ok:
        best = min(ok, key=lambda r: float(r["val_rmse_kwh"]))
        run_id = best["run_id"]
        keras_path = MODEL_DIR / run_id / (run_id + ".keras")
        try:
            model = tf.keras.models.load_model(
                str(keras_path), custom_objects=architectures.PINBALL_OBJECTS)
        except (ValueError, TypeError):
            model = tf.keras.models.load_model(str(keras_path))
        kind = best.get("arch", "mlp")
        if kind in ("tcn", "cnn_mlp", "nbeats"):
            Xva = (data["X_val"][:, :300].reshape(-1, 300, 1),
                   data["X_val"][:, 300:])
        elif kind == "lstm":
            Xva = data["X_val"][:, :300].reshape(-1, 300, 1)
        else:
            Xva = data["X_val"]
        pred = model.predict(Xva, batch_size=4096, verbose=0)
        if pred.ndim > 1:
            pred = pred[:, 1]     # 分位数头：取 q50
        pred = np.asarray(pred).ravel()
        pred_kwh = pred * data["y_std"] + data["y_mean"]
        true_kwh = data["y_val"].astype(np.float64) * data["y_std"] + data["y_mean"]
        fig, ax = plt.subplots(figsize=(6, 6))
        ax.scatter(true_kwh, pred_kwh, s=3, alpha=0.15, color="#1f77b4")
        lims = (float(min(true_kwh.min(), pred_kwh.min())),
                float(max(true_kwh.max(), pred_kwh.max())))
        ax.plot(lims, lims, "r--", linewidth=1, label="y=x")
        ax.set_xlabel("真实 Ê_H [kWh]")
        ax.set_ylabel("预测 Ê_H [kWh]")
        ax.set_title("val 散点 — %s (RMSE=%.2f kWh)" % (run_id, best["val_rmse_kwh"]))
        ax.legend()
        fig.tight_layout()
        fig.savefig(PLOT_DIR / "val_pred_scatter_selected.png", dpi=110)
        plt.close(fig)
    print("plots -> %s" % PLOT_DIR, flush=True)


def mechanical_winner(records):
    ok = [r for r in records if r.get("status") == "OK"]
    if not ok:
        raise SystemExit("全部实验失败，无法选模（详见 training_log.csv）")
    return min(ok, key=lambda r: float(r["val_rmse_kwh"]))


def selection_issues(best, records, data):
    """返回需要主 Agent 裁决的问题列表（空 = 无需裁决）。"""
    issues = []
    best_mlp = min((r for r in records if r.get("status") == "OK" and r["model"].startswith("MLP")),
                   key=lambda r: float(r["val_rmse_kwh"]), default=None)
    if best["model"].startswith("LSTM") and best_mlp is not None:
        rel = (float(best_mlp["val_rmse_kwh"]) - float(best["val_rmse_kwh"])) / float(best_mlp["val_rmse_kwh"])
        issues.append({
            "type": "lstm_vs_mlp",
            "best_lstm": best["run_id"], "best_lstm_rmse": best["val_rmse_kwh"],
            "best_mlp": best_mlp["run_id"], "best_mlp_rmse": best_mlp["val_rmse_kwh"],
            "rel_improvement": round(rel, 4),
            "detail": "design §5 内联公式/权重 JSON 仅定义 MLP；选 LSTM 需主 Agent 裁决导出格式与内联复杂度。",
        })
    ratio = float(best["val_rmse_kwh"]) / data["y_std"] if data["y_std"] else 0.0
    if ratio > 0.9:
        issues.append({
            "type": "unpredictable",
            "detail": "val RMSE/无条件 std=%.2f 接近 1 —— Ê_H 预测接近无信息量。" % ratio,
        })
    return issues


def export_weights_json(best, model, data, run_utc, verify_n=100):
    """入选 MLP 权重导出（W1/b1/W2/b2/W3/b3 + norm），并做 numpy 前向一致性自检。"""
    dense_layers = [l for l in model.layers if isinstance(l, tf.keras.layers.Dense)]
    weights = [l.get_weights() for l in dense_layers]
    W1, b1 = weights[0][0].astype(np.float64), weights[0][1].astype(np.float64)
    W2, b2 = weights[1][0].astype(np.float64), weights[1][1].astype(np.float64)
    W3, b3 = weights[2][0].astype(np.float64), weights[2][1].astype(np.float64)

    x = np.asarray(data["X_val"][:verify_n], dtype=np.float32)
    yk = model.predict(x, batch_size=verify_n, verbose=0).ravel()
    yn = _np_mlp_forward(
        x,
        W1.astype(np.float32), b1.astype(np.float32),
        W2.astype(np.float32), b2.astype(np.float32),
        W3.astype(np.float32), b3.astype(np.float32)).ravel()
    max_delta = float(np.max(np.abs(yk - yn)))

    hidden = best.get("hidden") or "(hidden 从 keras 恢复)"
    is_v2 = (data.get("dataset_version") == "v2")
    obj = {
        "spec": "nn-predictive-ecms-phase25" if is_v2 else "nn-predictive-ecms-phase2",
        "task": "T-305" if is_v2 else "T-203",
        "script": "python/train_predictor.py",
        "script_version": SCRIPT_VERSION,
        "model": "MLP",
        "config": {"hidden": list(hidden) if isinstance(hidden, (list, tuple)) else hidden,
                   "lr": best.get("lr")},
        "seed": int(float(best["seed"])),
        "run_id": best["run_id"],
        "created_utc": run_utc,
        "git_commit_sha": git_sha(),
        "dataset": {
            "path": "datasets/fleet_windows.npz",
            "md5": data["npz_md5"],
            "manifest_commit": data["manifest"].get("git_commit_sha"),
        },
        "input": {
            "layout": data["input_desc"],
            "shape": [302], "dtype": "float32",
        },
        "output": {
            "meaning": "y_norm = (Ê_H_kWh - y_mean) / y_std；恢复 Ê_H_kWh = y_norm * y_std + y_mean",
            "shape": [1], "dtype": "float32",
        },
        "forward": "y_norm = W3 @ relu(W2 @ relu(W1 @ x_norm + b1) + b2) + b3",
        "norm": {
            "y_mean": data["y_mean"], "y_std": data["y_std"],
            "X_mean": data["X_mean"].tolist(), "X_std": data["X_std"].tolist(),
        },
        "weights": {
            "W1": W1.tolist(), "b1": b1.tolist(),
            "W2": W2.tolist(), "b2": b2.tolist(),
            "W3": W3.tolist(), "b3": b3.tolist(),
        },
        "val_metrics_kWh": {
            "mae": best["val_mae_kwh"], "rmse": best["val_rmse_kwh"],
            "under_freq": best["under_freq"],
            "under_mag": best["under_mag_kwh"], "over_mag": best["over_mag_kwh"],
        },
        "export_self_check": {
            "n_samples": verify_n,
            "max_abs_delta_numpy_vs_keras": max_delta,
            "tolerance": 1e-5,
        },
    }
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    out = MODEL_DIR / WEIGHTS_NAME
    out.write_text(json.dumps(obj, indent=2), encoding="utf-8")
    print("%s -> %s (自检 max|Δ|=%.2e)" % (WEIGHTS_NAME, out, max_delta), flush=True)
    return out, max_delta


def export_selection_json(records, best, data, run_utc, decision_status,
                          main_agent_ruling=None, scope_note=None):
    ok = sorted([r for r in records if r.get("status") == "OK"],
                key=lambda r: float(r["val_rmse_kwh"]))
    all_rows = []
    for r in ok:
        all_rows.append({
            "run_id": r["run_id"], "model": r["model"], "config": r["hyperparameters"],
            "best_epoch": r["best_epoch"], "val_mae_kwh": r["val_mae_kwh"],
            "val_rmse_kwh": r["val_rmse_kwh"], "under_freq": r["under_freq"],
            "under_mag_kwh": r["under_mag_kwh"], "over_mag_kwh": r["over_mag_kwh"],
            "n_params": r["n_params"], "size_kb_fp32": r["size_kb_fp32"],
            "est_ram_kb": r["est_ram_kb"], "infer_ms_tf": r["infer_ms_tf"],
            "infer_ms_numpy_inline": r["infer_ms_numpy_inline"],
            "runtime_s": r["runtime_s"],
        })
    failed = [{"run_id": r["run_id"], "notes": r["notes"]}
              for r in records if r.get("status") != "OK"]

    # 朴素基准：恒预测 train 均值 -> val MAE
    naive_mae = float(np.mean(np.abs(
        data["y_val"].astype(np.float64) * data["y_std"] + data["y_mean"] - data["y_mean"])))

    selected_block = None
    if best is not None:
        selected_block = {
            "run_id": best["run_id"], "model": best["model"],
            "config": best["hyperparameters"], "seed": int(float(best["seed"])),
            "val_metrics_kWh": {
                "mae": best["val_mae_kwh"], "rmse": best["val_rmse_kwh"],
                "under_freq": best["under_freq"], "under_mag": best["under_mag_kwh"],
                "over_mag": best["over_mag_kwh"],
            },
            "model_size": {
                "n_params": best["n_params"], "size_kb_fp32": best["size_kb_fp32"],
                "est_ram_kb": best["est_ram_kb"],
                "infer_ms_tf": best["infer_ms_tf"],
                "infer_ms_numpy_inline": best["infer_ms_numpy_inline"],
            },
            "best_epoch": best["best_epoch"],
            "export": {
                "keras": best["export"],
                "weights_json": str(MODEL_DIR / WEIGHTS_NAME)
                                 if best["model"].startswith("MLP") else None,
            },
        }

    is_v2 = (data.get("dataset_version") == "v2")
    obj = {
        "spec": "nn-predictive-ecms-phase25" if is_v2 else "nn-predictive-ecms-phase2",
        "task": "T-305" if is_v2 else "T-203",
        "script": "python/train_predictor.py",
        "script_version": SCRIPT_VERSION,
        "created_utc": run_utc,
        "git_commit_sha": git_sha(),
        "dataset": {
            "path": "datasets/fleet_windows_v2.npz" if is_v2 else "datasets/fleet_windows.npz",
            "md5": data["npz_md5"],
            "split": data["split"],
            "manifest_commit": data["manifest"].get("git_commit_sha"),
        },
        "selection_rule": "成功实验中 val RMSE 最小者入选；test 集不参与任何决策（T-205 一次性使用）",
        "test_usage": "test 集未用于指标/选模/调参；仅记录窗口数（%d）。" % len(data["y_test"]),
        "fairness_notes": "MLP/LSTM 同 Dataset/同 Split/同归一化（fit-on-train）/同指标；"
                          "LSTM 输入仅 [300,1] P_demand 窗口（design §4 批准口径），"
                          "未使用 v_ship/accel，差异已在 training_log.csv Notes 声明。",
        "decision_status": decision_status,
        "scope_note": scope_note,
        "main_agent_ruling": main_agent_ruling,
        "selected": selected_block,
        "predictability_reference": {
            "val_y_std_kWh": round(data["y_std"], 4),
            "val_naive_mean_mae_kWh": round(naive_mae, 4),
        },
        "all_experiments_sorted_by_val_rmse": all_rows,
        "failed_experiments": failed,
    }
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    out = MODEL_DIR / SELECTION_NAME
    out.write_text(json.dumps(obj, indent=2), encoding="utf-8")
    print("%s -> %s (decision_status=%s)" % (SELECTION_NAME, out, decision_status), flush=True)
    return out


# ---------------------------------------------------------------- 主流程

def build_groups(args):
    """构造实验组。默认（无新参数）= v1 六组逐位一致；
    显式 --arch/--dataset/--quantiles 时按 --arch 列表构造（mlp/lstm 同 v1 配置）。"""
    MLP_GROUPS = [
        ("mlp_h64_32_lr1e-3",  {"kind": "mlp", "model": "MLP 302-64-32-1",
                                "hidden": (64, 32), "lr": 1e-3,
                                "hp": "hidden=(64,32), lr=1e-3, batch=%d, epochs_cap=%d, patience=%d" % (args.batch, args.epochs, args.patience),
                                "notes": ""}),
        ("mlp_h64_32_lr3e-4",  {"kind": "mlp", "model": "MLP 302-64-32-1",
                                "hidden": (64, 32), "lr": 3e-4,
                                "hp": "hidden=(64,32), lr=3e-4, batch=%d, epochs_cap=%d, patience=%d" % (args.batch, args.epochs, args.patience),
                                "notes": ""}),
        ("mlp_h128_64_lr1e-3", {"kind": "mlp", "model": "MLP 302-128-64-1",
                                "hidden": (128, 64), "lr": 1e-3,
                                "hp": "hidden=(128,64), lr=1e-3, batch=%d, epochs_cap=%d, patience=%d" % (args.batch, args.epochs, args.patience),
                                "notes": ""}),
        ("mlp_h128_64_lr3e-4", {"kind": "mlp", "model": "MLP 302-128-64-1",
                                "hidden": (128, 64), "lr": 3e-4,
                                "hp": "hidden=(128,64), lr=3e-4, batch=%d, epochs_cap=%d, patience=%d" % (args.batch, args.epochs, args.patience),
                                "notes": ""}),
    ]
    LSTM_GROUPS = [
        ("lstm_u32_lr1e-3",    {"kind": "lstm", "model": "LSTM 32 [300,1]->1",
                                "units": 32, "lr": 1e-3,
                                "hp": "units=32, lr=1e-3, batch=%d, epochs_cap=%d, patience=%d" % (args.batch, args.epochs, args.patience),
                                "notes": "LSTM 输入仅 [300,1] P_demand 窗口（design §4 对照口径），v_ship/accel 未使用；其余数据条件与 MLP 一致。"}),
        ("lstm_u64_lr1e-3",    {"kind": "lstm", "model": "LSTM 64 [300,1]->1",
                                "units": 64, "lr": 1e-3,
                                "hp": "units=64, lr=1e-3, batch=%d, epochs_cap=%d, patience=%d" % (args.batch, args.epochs, args.patience),
                                "notes": "LSTM 输入仅 [300,1] P_demand 窗口（design §4 对照口径），v_ship/accel 未使用；其余数据条件与 MLP 一致。"}),
    ]
    TCN_GROUPS = [
        ("tcn_f16",           {"kind": "tcn", "model": "TCN f16 d1-2-4-8-16-32-32",
                               "filters": 16, "lr": 1e-3,
                               "hp": "filters=16, kernel=3, dilations=(1,2,4,8,16,32,32), R=381, lr=1e-3, quantiles=%s, batch=%d, epochs_cap=%d, patience=%d" % (args.quantiles, args.batch, args.epochs, args.patience),
                               "notes": "T-305 规模扫描最小点（NPU 轻量偏好验证）。"}),
        ("tcn_f32",           {"kind": "tcn", "model": "TCN f32 d1-2-4-8-16-32-32",
                               "filters": 32, "lr": 1e-3,
                               "hp": "filters=32, kernel=3, dilations=(1,2,4,8,16,32,32), R=381, lr=1e-3, quantiles=%s, batch=%d, epochs_cap=%d, patience=%d" % (args.quantiles, args.batch, args.epochs, args.patience),
                               "notes": "T-305 规模扫描点 2/4。"}),
        ("tcn_f64",           {"kind": "tcn", "model": "TCN f64 d1-2-4-8-16-32-32",
                               "filters": 64, "lr": 1e-3,
                               "hp": "filters=64, kernel=3, dilations=(1,2,4,8,16,32,32), R=381, lr=1e-3, quantiles=%s, batch=%d, epochs_cap=%d, patience=%d" % (args.quantiles, args.batch, args.epochs, args.patience),
                               "notes": "T-305 规模扫描中点；沿用 T-304 基线 filters=64。"}),
        ("tcn_f128",          {"kind": "tcn", "model": "TCN f128 d1-2-4-8-16-32-32",
                               "filters": 128, "lr": 1e-3,
                               "hp": "filters=128, kernel=3, dilations=(1,2,4,8,16,32,32), R=381, lr=1e-3, quantiles=%s, batch=%d, epochs=%d, patience=%d" %
                              (args.quantiles, args.batch, args.epochs, args.patience),
                               "notes": "T-305 规模扫描最大点（精度上限探测，M1 恢复；int8 约 500 KB）。"}),
        ("tcn_f256",          {"kind": "tcn", "model": "TCN f256 d1-2-4-8-16-32-32",
                               "filters": 256, "lr": 1e-3,
                               "hp": "filters=256, kernel=3, dilations=(1,2,4,8,16,32,32), R=381, lr=1e-3, quantiles=%s, batch=%d, epochs=%d, patience=%d" %
                              (args.quantiles, args.batch, args.epochs, args.patience),
                               "notes": "T-305 规模扫描扩展点（用户裁决补测；f512/f1024 经主 Agent 裁决不跑：边际收益进入跨机噪声区间且 NPU Pareto 预期被杀）。"}),
    ]
    CNN_GROUPS = [
        ("cnn_f32_lr1e-3",    {"kind": "cnn_mlp", "model": "CNN-MLP 32-64-128",
                               "filters1": 32, "kernel1": 7, "filters2": 64, "kernel2": 5, "lr": 1e-3,
                               "hp": "conv(32,k7)-pool-conv(64,k5)-pool-dense128, lr=1e-3, quantiles=%s, batch=%d, epochs_cap=%d, patience=%d" % (args.quantiles, args.batch, args.epochs, args.patience),
                               "notes": "T-304 基线配置。"}),
    ]
    NBEATS_GROUPS = [
        ("nbeats_f256_lr1e-3", {"kind": "nbeats", "model": "N-BEATS generic 4x2x256",
                                "n_stacks": 4, "blocks_per_stack": 2, "fcs": 256, "lr": 1e-3,
                                "hp": "4 stack x2 block FC256, lr=1e-3, quantiles=%s, batch=%d, epochs_cap=%d, patience=%d" % (args.quantiles, args.batch, args.epochs, args.patience),
                                "notes": "T-304 基线配置。"}),
    ]
    FAMILIES = {"mlp": MLP_GROUPS, "lstm": LSTM_GROUPS,
                "tcn": TCN_GROUPS, "cnn_mlp": CNN_GROUPS, "nbeats": NBEATS_GROUPS}

    # 默认（无新参数）：v1 六组逐位一致 + --only 过滤（既有语义）
    if not args.scoped:
        groups = MLP_GROUPS + LSTM_GROUPS
        scope_note = None
        if args.only == "mlp":
            groups = MLP_GROUPS
            scope_note = ("T-204B: 本批仅复训 4 组 MLP（N=200 数据集，140/30/30 split）；"
                          "LSTM 对照组结论已在 N=80 建立，本次不复训（见 dataset_manifest "
                          "split.ratio_history）。")
        elif args.only == "lstm":
            groups = LSTM_GROUPS
            scope_note = "T-204B: 本批仅训练 LSTM 组（N=200 数据集）；MLP 组不训练。"
        if scope_note is not None:
            print("\nSCOPE: %s\n" % scope_note, flush=True)
            groups = [(k, dict(cfg, notes=((scope_note + "；" + cfg["notes"])
                                           if cfg["notes"] else scope_note)))
                      for k, cfg in groups]
        return groups, scope_note

    # 作用域模式：按 --arch 列表（逗号分隔）
    archs = [a.strip() for a in args.arch.split(",") if a.strip()]
    groups = []
    for a in archs:
        if a not in FAMILIES:
            raise SystemExit("--arch 无效: %s" % a)
        groups.extend(FAMILIES[a])
    if not groups:
        raise SystemExit("--arch 为空")
    # T-305：作用域模式下支持 --only 单组过滤（例：--arch tcn --only tcn_f16），
    # 用于本机逐组推进 / 跨机接力时避免自动启动后续配置
    if args.only and args.only != "all":
        only_keys = {k.strip() for k in args.only.split(",")}
        groups = [(k, c) for k, c in groups if k in only_keys]
        if not groups:
            raise SystemExit("--only 与 --arch 无交集: %s" % args.only)
    scope_note = "T-304 scoped: dataset=%s arch=%s quantiles=%s" % (
        args.dataset, args.arch, args.quantiles)
    if args.dataset == "v1":
        scope_note += "；v1 回归隔离目录 results/ecms_phase2/models_repro_t304/"
    print("\nSCOPE: %s\n" % scope_note, flush=True)
    groups = [(k, dict(cfg, notes=((scope_note + "；" + cfg["notes"])
                                   if cfg["notes"] else scope_note)))
              for k, cfg in groups]
    return groups, scope_note


def run_full(args, data):
    run_utc = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    groups, scope_note = build_groups(args)

    records, models, histories = [], {}, {}
    for i, (key, cfg) in enumerate(groups, 1):
        # 非 42 seed 的重复实验（T-306 噪声底）追加 _sNN 后缀，避免覆盖官方 s42 产物
        eff_key = key if int(args.seed) == SEED else "%s_s%d" % (key, args.seed)
        rec, model, hist = run_experiment(eff_key, cfg, data, args, run_utc, i, len(groups))
        records.append(rec)
        models[key] = model
        histories[key] = hist

    write_csv(records)
    if not args.no_plots:
        make_plots(records, histories, data)

    best = mechanical_winner(records)
    issues = selection_issues(best, records, data)
    print("\n=== 机械选模（val RMSE 最小）===", flush=True)
    print("入选: %s (val RMSE=%.3f kWh, MAE=%.3f kWh, under_freq=%.3f)" % (
        best["run_id"], best["val_rmse_kwh"], best["val_mae_kwh"], best["under_freq"]), flush=True)

    if issues:
        export_selection_json(records, None, data, run_utc, decision_status="pending",
                              scope_note=scope_note)
        for iss in issues:
            print("ESCALATE_REQUIRED [%s]: %s" % (iss["type"], iss.get("detail", "")), flush=True)
        print("最终产物待主 Agent 裁决后以 --finalize 生成", flush=True)
        return 1

    # 无需裁决 -> 直接定稿
    return finalize(args, data, records=records, selected_run_id=best["run_id"],
                    scope_note=scope_note)


def finalize(args, data, records=None, selected_run_id=None, scope_note=None):
    if records is None:
        records = read_records_from_csv()
    if selected_run_id is None:
        selected_run_id = args.select_run_id
    if not selected_run_id:
        best = mechanical_winner(records)
        selected_run_id = best["run_id"]

    match = [r for r in records if r.get("run_id") == selected_run_id and r.get("status") == "OK"]
    if not match:
        raise SystemExit("run_id %s 不存在或非 OK（见 training_log.csv）" % selected_run_id)
    best = match[0]

    keras_path = MODEL_DIR / selected_run_id / (selected_run_id + ".keras")
    if not keras_path.exists():
        raise SystemExit("缺失 %s —— 需先执行完整训练" % keras_path)
    try:
        model = tf.keras.models.load_model(
            str(keras_path),
            custom_objects=architectures.PINBALL_OBJECTS)
    except (ValueError, TypeError):
        model = tf.keras.models.load_model(str(keras_path))

    # 用保存的模型重算 val 指标（与 training_log.csv 交叉验证）
    kind = best.get("arch", "mlp")
    if kind in ("tcn", "cnn_mlp", "nbeats"):
        Xva = (data["X_val"][:, :300].reshape(-1, 300, 1),
               data["X_val"][:, 300:])
    elif kind == "lstm":
        Xva = data["X_val"][:, :300].reshape(-1, 300, 1)
    else:
        Xva = data["X_val"]
    pred = model.predict(Xva, batch_size=4096, verbose=0)
    if best.get("quantiles") and kind in ("tcn", "cnn_mlp", "nbeats"):
        m = val_metrics_quantiles(pred, data["y_val"], data["y_mean"], data["y_std"])
    else:
        m = val_metrics(pred.ravel(), data["y_val"], data["y_mean"], data["y_std"])
    assert abs(float(m["val_rmse_kwh"]) - float(best["val_rmse_kwh"])) < 1e-3, (
        "重算 val RMSE 与 CSV 不一致: %s vs %s" % (m["val_rmse_kwh"], best["val_rmse_kwh"]))
    assert abs(float(m["val_mae_kwh"]) - float(best["val_mae_kwh"])) < 1e-3

    run_utc = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    ruling = args.decision_note if args.decision_note else "n/a（机械规则选模，未触发裁决）"

    if best["model"].startswith("MLP"):
        export_weights_json(best, model, data, run_utc)
    else:
        print("注意: 入选模型为 LSTM，selected_model_weights.json 不导出（导出格式由 T-204 定义）", flush=True)
    export_selection_json(records, best, data, run_utc, decision_status="finalized",
                          main_agent_ruling=ruling, scope_note=scope_note)

    print("\n=== 定稿 ===", flush=True)
    print("入选: %s | val RMSE=%.3f kWh | MAE=%.3f kWh | under_freq=%.3f | ruling=%s" % (
        best["run_id"], best["val_rmse_kwh"], best["val_mae_kwh"],
        best["under_freq"], ruling), flush=True)
    return 0


def resolve_scope(args):
    """T-304 输出隔离。默认（无新参数）= v1 路径逐位不变；
    v1 回归（显式 --arch/--dataset v1）= repro 目录；v2 = ecms_phase25 目录。
    修改模块级全局路径（NPZ/NORM/MANIFEST/OUT_DIR/MODEL_DIR/PLOT_DIR/LOG_CSV/文件名）。"""
    global NPZ, NORM_JSON, MANIFEST_JSON, OUT_DIR, MODEL_DIR, PLOT_DIR, LOG_CSV
    global SELECTION_NAME, WEIGHTS_NAME, CSV_FIELDNAMES_ACTIVE
    global DATA_NPZ, DATA_NORM_JSON, DATA_MANIFEST_JSON, DATA_DATASET_VERSION

    explicit = (args.arch_explicit or args.dataset_explicit or args.quantiles)
    args.scoped = bool(explicit)
    if not explicit:
        # v1 默认：路径全局已是 v1，仅确保文件名默认
        SELECTION_NAME, WEIGHTS_NAME = "selection.json", "selected_model_weights.json"
        CSV_FIELDNAMES_ACTIVE = None
        DATA_DATASET_VERSION = "v1"
        return

    CSV_FIELDNAMES_ACTIVE = CSV_FIELDNAMES + ["arch", "quantiles"]
    if args.dataset == "v1":
        # v1 回归隔离：不触碰 Phase 2 任何文件
        OUT_DIR = BASE / "results" / "ecms_phase2" / "models_repro_t304"
        MODEL_DIR = OUT_DIR
        PLOT_DIR = BASE / "results" / "ecms_phase2" / "plots_repro_t304"
        LOG_CSV = BASE / "results" / "ecms_phase2" / "training_log_v2.csv"
        SELECTION_NAME, WEIGHTS_NAME = "selection.json", "selected_model_weights.json"
        DATA_DATASET_VERSION = "v1"
        print("\n[T-304 scoped v1 回归] 输出隔离 -> %s（Phase 2 文件零触碰）\n" % OUT_DIR, flush=True)
    else:
        OUT_DIR = BASE / "results" / "ecms_phase25"
        MODEL_DIR = OUT_DIR / "models"
        PLOT_DIR = OUT_DIR / "plots"
        LOG_CSV = OUT_DIR / "training_log_v2.csv"
        SELECTION_NAME, WEIGHTS_NAME = "selection_v2.json", "selected_model_weights_v2.json"
        NPZ = BASE / "datasets" / "fleet_windows_v2.npz"
        NORM_JSON = BASE / "datasets" / "fleet_norm_v2.json"
        MANIFEST_JSON = BASE / "results" / "ecms_phase25" / "dataset_manifest_v2.json"
        DATA_DATASET_VERSION = "v2"
        print("\n[T-304 scoped v2] 输出 -> %s（Phase 2 文件零触碰）\n" % OUT_DIR, flush=True)
    DATA_NPZ, DATA_NORM_JSON, DATA_MANIFEST_JSON = NPZ, NORM_JSON, MANIFEST_JSON


def main():
    ap = argparse.ArgumentParser(description="T-203 MLP/LSTM 训练 + val 选模（T-304 扩展模型族）")
    ap.add_argument("--epochs", type=int, default=100)
    ap.add_argument("--patience", type=int, default=10)
    ap.add_argument("--batch", type=int, default=256)
    ap.add_argument("--seed", type=int, default=SEED)
    ap.add_argument("--no-plots", action="store_true")
    ap.add_argument("--finalize", action="store_true",
                    help="定稿模式：从已保存 .keras 重新生成最终产物（不重训）")
    ap.add_argument("--select-run-id", type=str, default=None,
                    help="定稿时指定的入选 run_id")
    ap.add_argument("--only", type=str, default="all",
                    help="仅训练指定模型族/配置（T-204B: --only mlp；T-305 scoped: "
                         "--only tcn_f16 单组过滤，逗号分隔多选）")
    ap.add_argument("--decision-note", type=str, default=None,
                    help="主 Agent 裁决记录（写入 selection.json main_agent_ruling）")
    # ---- T-304 新参数（默认 None => 检测“是否显式给定”） ----
    ap.add_argument("--arch", type=str, default=None,
                    help="模型族（逗号分隔，可多选）：mlp,tcn,cnn_mlp,nbeats,lstm；"
                         "默认(不给)=v1 矩阵 mlp,lstm")
    ap.add_argument("--dataset", type=str, default=None, choices=("v1", "v2"),
                    help="数据集版本（默认 v1）")
    ap.add_argument("--quantiles", action="store_true",
                    help="分位数头（q10/q50/q90, pinball loss）；默认单输出 MSE")
    args = ap.parse_args()
    args.seed = SEED  # 任务规定种子固定 42
    args.arch_explicit = args.arch is not None
    args.dataset_explicit = args.dataset is not None
    if args.arch is None:
        args.arch = "mlp,lstm"
    if args.dataset is None:
        args.dataset = "v1"

    resolve_scope(args)

    print("=== T-203 train_predictor v%s ===" % SCRIPT_VERSION, flush=True)
    print("git sha:", git_sha(), "| epochs_cap=%d patience=%d batch=%d seed=%d | scoped=%s" % (
        args.epochs, args.patience, args.batch, args.seed, args.scoped), flush=True)
    print("arch=%s dataset=%s quantiles=%s" % (args.arch, args.dataset, args.quantiles), flush=True)

    data = load_data(args)
    print("dataset:", data["dataset_str"], flush=True)
    print("split:", data["split"], flush=True)

    if args.finalize:
        if args.select_run_id is None:
            raise SystemExit("--finalize 需要 --select-run-id <key>")
        sys.exit(finalize(args, data))
    sys.exit(run_full(args, data))


if __name__ == "__main__":
    main()

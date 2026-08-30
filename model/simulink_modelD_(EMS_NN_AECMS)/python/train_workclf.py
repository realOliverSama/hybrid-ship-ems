# -*- coding: utf-8 -*-
"""train_workclf.py — T-308：小型 Conv1D 工况识别器（7 类段型）

设计（design.md 工况识别器节）：历史窗口降采样 300→30 点（10 s 重采样），
Conv1D+pooling+FC；类别不平衡用 class_weight；产出 test 混淆矩阵、
macro-F1、per-class recall、段切换窗口识别率、transition latency。
输出：results/ecms_phase25/classifier/
用法：--reuse 复用已训模型（只重算指标）。
"""
import io, sys, os, json, time
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
import numpy as np
import tensorflow as tf
import scipy.io as sio

BASE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(BASE, "..")
OUT = os.path.join(ROOT, "results", "ecms_phase25", "classifier")
EPOCHS = 40
BATCH = 256

def confmat(y_true, y_pred, k=7):
    cm = np.zeros((k, k), dtype=np.int64)
    for t, p in zip(y_true, y_pred):
        cm[t - 1, p - 1] += 1
    return cm

def metrics_from_cm(cm):
    tp = np.diag(cm).astype(np.float64)
    fp = cm.sum(0) - tp
    fn = cm.sum(1) - tp
    prec = tp / np.maximum(tp + fp, 1)
    rec = tp / np.maximum(tp + fn, 1)
    f1 = 2 * prec * rec / np.maximum(prec + rec, 1e-9)
    return prec, rec, f1

def build_model():
    inp = tf.keras.Input(shape=(30, 1), name="P_dec30")
    x = tf.keras.layers.Conv1D(16, 5, padding="causal", activation="relu")(inp)
    x = tf.keras.layers.MaxPooling1D(2)(x)
    x = tf.keras.layers.Conv1D(32, 3, padding="causal", activation="relu")(x)
    x = tf.keras.layers.MaxPooling1D(2)(x)
    x = tf.keras.layers.Conv1D(32, 3, padding="causal", activation="relu")(x)
    x = tf.keras.layers.GlobalAveragePooling1D()(x)
    inp_s = tf.keras.Input(shape=(2,), name="V_accel")
    s = tf.keras.layers.Dense(8, activation="relu")(inp_s)
    z = tf.keras.layers.Concatenate()([x, s])
    z = tf.keras.layers.Dense(64, activation="relu")(z)
    out = tf.keras.layers.Dense(7, activation="softmax", name="seg")(z)
    model = tf.keras.Model([inp, inp_s], out)
    model.compile(tf.keras.optimizers.Adam(1e-3), "sparse_categorical_crossentropy")
    return model

def main():
    t0 = time.time()
    d = np.load(os.path.join(ROOT, "datasets", "fleet_windows_v2.npz"))
    lab = np.load(os.path.join(ROOT, "results", "ecms_phase25", "segment_labels",
                               "seg_labels_v2.npz"))
    seg_names = [str(s) for s in lab["seg_names"]]

    def prep(split):
        X = d[f"X_{split}"]
        P = np.ascontiguousarray(X[:, :300:10][..., None].astype(np.float32))
        S = np.ascontiguousarray(X[:, 300:].astype(np.float32))
        return P, S, lab[f"seg_labels_{split}"]

    Ptr, Str, ytr = prep("train")
    Pva, Sva, yva = prep("val")
    Pte, Ste, yte = prep("test")
    print(f"data ready {time.time()-t0:.0f}s | train {Ptr.shape}", flush=True)

    reuse = "--reuse" in sys.argv
    kpath = os.path.join(OUT, "workclf_v1.keras")
    if reuse and os.path.isfile(kpath):
        model = tf.keras.models.load_model(kpath)
        print("reused existing model", flush=True)
    else:
        model = build_model()
        cnt = np.bincount(ytr, minlength=8)[1:]
        w = cnt.sum() / (7 * np.maximum(cnt, 1))
        cw = {i + 1: float(w[i]) for i in range(7)}
        es = tf.keras.callbacks.EarlyStopping(monitor="val_loss", patience=6,
                                              restore_best_weights=True)
        model.fit([Ptr, Str], ytr - 1, validation_data=([Pva, Sva], yva - 1),
                  epochs=EPOCHS, batch_size=BATCH, class_weight=cw,
                  callbacks=[es], verbose=2)
        os.makedirs(OUT, exist_ok=True)
        model.save(kpath)
        tfl = tf.lite.TFLiteConverter.from_keras_model(model).convert()
        open(os.path.join(OUT, "workclf_v1_int8.tflite"), "wb").write(tfl)

    n_params = int(model.count_params())
    print("classifier params:", n_params, flush=True)

    report = {"task": "T-308", "params": n_params, "seg_names": seg_names}
    preds = {}
    for split, (P, S, y) in (("val", (Pva, Sva, yva)), ("test", (Pte, Ste, yte))):
        pr = model.predict([P, S], batch_size=2048, verbose=0).argmax(1) + 1
        preds[split] = pr
        cm = confmat(y, pr)
        prec, rec, f1 = metrics_from_cm(cm)
        report[split] = {
            "accuracy": round(float((y == pr).mean()), 4),
            "macro_f1": round(float(f1.mean()), 4),
            "per_class_recall": {seg_names[i]: round(float(rec[i]), 4) for i in range(7)},
            "per_class_f1": {seg_names[i]: round(float(f1[i]), 4) for i in range(7)},
            "confusion_matrix": cm.tolist(),
        }
        print(split, "acc=%.4f macroF1=%.4f" % (report[split]["accuracy"],
                                                report[split]["macro_f1"]), flush=True)

    # 段切换窗口识别率 + transition latency（test，逐航次逐边界）
    vids = d["voyage_ids_test"]
    vjson = json.load(open(os.path.join(ROOT, "results", "ecms_phase25", "fleet",
                                        "voyages_seed20260829.json"), encoding="utf-8"))
    vmap = {int(v["id"]): v for v in vjson["voyages"]}
    pr_te = preds["test"]
    switch_hits, latencies, n_bounds = [], [], 0
    for vid in np.unique(vids):
        vid = int(vid)
        idx = np.where(vids == vid)[0]
        mm = sio.loadmat(os.path.join(ROOT, "results", "ecms_phase25", "fleet_profiles",
                                      "voyage_%03d.mat" % vid))
        N = int(len(mm["t_s"]))
        a_arr = np.arange(299, N - 300, 10)
        v = vmap[vid]
        bounds, acc = [], 0
        for s_ in v["seg"]:
            acc += int(s_["dur_s"]); bounds.append(acc)
        for b in bounds[:-1]:
            n_bounds += 1
            after_local = np.where(a_arr >= b)[0]
            if len(after_local) == 0:
                continue
            pos0 = after_local[0]
            tc = int(yte[idx[pos0]])
            switch_hits.append(int(pr_te[idx[pos0]] == tc))
            run, lat = 0, None
            for pos in after_local:
                p = pr_te[idx[pos]]
                run = run + 1 if p == tc else 0
                if run >= 3:
                    lat = (a_arr[pos] - b) * 10
                    break
            if lat is not None:
                latencies.append(lat)
    report["test"]["switch_window_recall"] = round(float(np.mean(switch_hits)), 4) if switch_hits else None
    report["test"]["switch_windows"] = len(switch_hits)
    report["test"]["transition_boundaries"] = n_bounds
    report["test"]["transition_latency_s_median"] = float(np.median(latencies)) if latencies else None
    report["test"]["transition_latency_s_p90"] = float(np.percentile(latencies, 90)) if latencies else None

    json.dump(report, open(os.path.join(OUT, "t308_report.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=2)
    print("REPORT ->", os.path.join(OUT, "t308_report.json"), flush=True)
    print("TOTAL %.0f s" % (time.time() - t0), flush=True)

if __name__ == "__main__":
    main()

# -*- coding: utf-8 -*-
"""t307_int8.py — T-307 选模：候选 int8 量化 + 量化后 val 精度 + 模型体积表

候选（Amendment v1.1：val 仅资格筛选，Winner 由 T-310 闭环裁决）：
  MLP-MSE   mlp_h64_32_lr3e-4        （RMSE 冠军）
  MLP-Q     mlp_h64_32_lr3e-4_q      （pinball MLP 对照）
  TCN-f16   tcn_f16                  （最轻量 pinball）
  TCN-f64   tcn_f64                  （T-304 基线延续）
输出：results/ecms_phase25/t307_int8_report.json
"""
import os, sys, json, time, argparse
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import tensorflow as tf
import train_predictor as tp

BASE = os.path.dirname(os.path.abspath(__file__))
MD = os.path.join(BASE, "..", "results", "ecms_phase25", "models")
OUT_DIR = os.path.join(BASE, "..", "results", "ecms_phase25")
N_REP = 1000          # 代表性样本数（int8 校准）
BATCH = 512

CANDIDATES = [
    # (label, run_id, is_quantile, is_dual_input)
    ("MLP-MSE", "mlp_h64_32_lr3e-4",    False, False),
    ("MLP-Q",   "mlp_h64_32_lr3e-4_q",  True,  False),
    ("TCN-f16", "tcn_f16",              True,  True),
    ("TCN-f64", "tcn_f64",              True,  True),
]

def tflite_eval(interp, X, kind):
    """逐批解释器推理（支持单/双输入），返回 (N, out_dim) 预测。"""
    inputs = interp.get_input_details()
    out = interp.get_output_details()[0]
    n = X[0].shape[0] if isinstance(X, (list, tuple)) else X.shape[0]
    outs = []
    for i in range(0, n, BATCH):
        if isinstance(X, (list, tuple)):
            feeds = [x[i:i+BATCH].astype(np.float32) for x in X]
        else:
            feeds = [X[i:i+BATCH].astype(np.float32)]
        nb = feeds[0].shape[0]
        if nb < BATCH:  # 末批补齐到固定形状，输出后裁剪
            pad = BATCH - nb
            feeds = [np.concatenate([f, f[-1:].repeat(pad, axis=0)], axis=0) for f in feeds]
        if i == 0:
            for d, f in zip(inputs, feeds):
                interp.resize_tensor_input(d["index"], f.shape)
            interp.allocate_tensors()
        for d, f in zip(inputs, feeds):
            interp.set_tensor(d["index"], f)
        interp.invoke()
        o = interp.get_tensor(out["index"])
        outs.append(o[:nb])
    return np.concatenate(outs, axis=0)

def main():
    # v2 作用域全局
    tp.DATA_NPZ = tp.BASE / "datasets" / "fleet_windows_v2.npz"
    tp.DATA_NORM_JSON = tp.BASE / "datasets" / "fleet_norm_v2.json"
    tp.DATA_MANIFEST_JSON = tp.BASE / "results" / "ecms_phase25" / "dataset_manifest_v2.json"
    tp.DATA_DATASET_VERSION = "v2"
    args = argparse.Namespace(dataset="v2", arch="tcn", quantiles=True, batch=BATCH,
                              epochs=100, patience=10, seed=42, only="all",
                              no_plots=True, scoped=True, decision_note=None,
                              select_run_id=None, finalize=False)
    data = tp.load_data(args)
    yv = data["y_val"]

    # 代表性数据集（val 前 N_REP 样本，仅校准用，不参与训练/调参决策）
    Xv = data["X_val"]
    rep_dual = (Xv[:N_REP, :300].reshape(-1, 300, 1).astype(np.float32),
                Xv[:N_REP, 300:].astype(np.float32))
    rep_mlp = Xv[:N_REP].astype(np.float32)
    Xv_dual = (Xv[:, :300].reshape(-1, 300, 1), Xv[:, 300:])

    report = {"task": "T-307", "generated": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
              "git_commit_sha": tp.git_sha(), "quant_calib": "val 前 %d 样本（仅校准）" % N_REP,
              "candidates": []}

    for label, run_id, is_q, is_dual in CANDIDATES:
        rec = {"name": label, "run_id": run_id, "quantiles": is_q}
        mp = os.path.join(MD, run_id, run_id + ".keras")
        model = tf.keras.models.load_model(mp, safe_mode=False)
        rec["params"] = int(model.count_params())
        rec["size_fp32_kb"] = round(os.path.getsize(mp) / 1024, 1)

        # float 基线（keras 推理）：输入由架构决定，指标由输出头决定
        if is_dual:
            pf = model.predict(list(Xv_dual), batch_size=4096, verbose=0)
        else:
            pf = model.predict(Xv, batch_size=4096, verbose=0)
        if is_q:
            mq = tp.val_metrics_quantiles(pf, yv, data["y_mean"], data["y_std"])
            rec["float_val"] = {k: round(float(v), 4) for k, v in mq.items()}
            base = pf[:, 1]
        else:
            mv = tp.val_metrics(pf.ravel(), yv, data["y_mean"], data["y_std"])
            rec["float_val"] = {k: round(float(v), 4) for k, v in mv.items()}
            base = pf.ravel()

        # TFLite int8 转换（全整型，代表集校准）
        def rep_gen():
            for i in range(N_REP):
                if is_dual:
                    yield [rep_dual[0][i:i+1], rep_dual[1][i:i+1]]
                else:
                    yield [rep_mlp[i:i+1]]
        conv = tf.lite.TFLiteConverter.from_keras_model(model)
        conv.optimizations = [tf.lite.Optimize.DEFAULT]
        conv.representative_dataset = rep_gen
        try:
            tfl = conv.convert()
        except Exception as e:
            conv2 = tf.lite.TFLiteConverter.from_keras_model(model)
            conv2.optimizations = [tf.lite.Optimize.DEFAULT]
            conv2.representative_dataset = rep_gen
            conv2.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS,
                                               tf.lite.OpsSet.SELECT_TF_OPS]
            conv2._experimental_lower_tensor_list_ops = False
            tfl = conv2.convert()
            rec["fallback_select_tf_ops"] = True
        out_path = os.path.join(MD, run_id, run_id + "_int8.tflite")
        open(out_path, "wb").write(tfl)
        rec["size_int8_kb"] = round(len(tfl) / 1024, 1)
        rec["tflite_path"] = os.path.relpath(out_path, OUT_DIR)

        # int8 精度
        interp = tf.lite.Interpreter(model_path=out_path, num_threads=4)
        pi = tflite_eval(interp, list(Xv_dual) if is_dual else Xv, kind=None)
        if is_q:
            mq = tp.val_metrics_quantiles(pi, yv, data["y_mean"], data["y_std"])
            rec["int8_val"] = {k: round(float(v), 4) for k, v in mq.items()}
        else:
            mv = tp.val_metrics(pi.ravel(), yv, data["y_mean"], data["y_std"])
            rec["int8_val"] = {k: round(float(v), 4) for k, v in mv.items()}
        rec["delta_rmse_pct"] = round(100 * (rec["int8_val"]["val_rmse_kwh"]
                                             / rec["float_val"]["val_rmse_kwh"] - 1), 2)
        report["candidates"].append(rec)
        print(json.dumps(rec, ensure_ascii=False))

    out = os.path.join(OUT_DIR, "t307_int8_report.json")
    json.dump(report, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print("REPORT ->", out)

if __name__ == "__main__":
    main()

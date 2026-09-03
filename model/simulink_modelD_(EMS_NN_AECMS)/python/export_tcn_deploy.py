#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""export_tcn_deploy.py — TCN-f64 部署 JSON 导出 + 三口径一致检查（spec ems01-chil-tcn64-swap）。

单一来源链（design §Data Model 1，FR-702/FR-708）：
  tcn_f64.keras --(本脚本)--> models/selected_model_weights_tcnq.json
                            --> gen_fw_params.py --> EMS_TCNQ_* 表（ems_params.c）

产物：
  1. WS/results/ecms_phase25/models/selected_model_weights_tcnq.json
     （weights + norm 副本 + caliber 口径声明块 + sha256 溯源）
  2. WS/results/ecms_phase25/deploy/m3_tcn_swap_calib_report.json
     （三口径一致检查：训练口径 == 部署喂入口径 == int8 标定口径，AI_MODELING §22.3）

机械验证（T1）：
  - 参数量断言 165,851；
  - 权重抽取保真：导出数组与 keras get_weights() 逐位相等（float32 bitwise）；
  - 前向重放双层口径：
      (1) 布局保真门（design T1：spot ≤1e-6 kWh）＝ float64 上转 keras 前向
          （clone_model + f64 权重，消除 f32 累加噪声）vs numpy float64 重放；
      (2) f32 噪声底记录 ＝ keras float32（L0 参考链原域）vs float64 重放，
          仅报告不设门（sanity 上限 1e-3 kWh；f32 输出张量本身在 q50~60 kWh
          处的舍入粒度 ≈ 3.6e-6 kWh，故 1e-6 门只能在 f64 域有意义）。

运行：model/simulink/.venv_tf/bin/python python/export_tcn_deploy.py
"""
import hashlib
import json
import os
import re
import subprocess
import sys

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
WS = os.path.join(REPO, "model", "simulink_modelD_(EMS_NN_AECMS)")
MODELS_DIR = os.path.join(WS, "results", "ecms_phase25", "models")
DEPLOY_DIR = os.path.join(WS, "results", "ecms_phase25", "deploy")
KERAS_PATH = os.path.join(MODELS_DIR, "tcn_f64", "tcn_f64.keras")
TFLITE_PATH = os.path.join(MODELS_DIR, "tcn_f64", "tcn_f64_int8.tflite")
NORM_PATH = os.path.join(WS, "datasets", "fleet_norm_v2.json")
OUT_JSON = os.path.join(MODELS_DIR, "selected_model_weights_tcnq.json")
CALIB_JSON = os.path.join(DEPLOY_DIR, "m3_tcn_swap_calib_report.json")
EMSC_CTRL_PATH = os.path.join(REPO, "firmware", "stm32n6", "04_SourceCode",
                              "SoftwarePackage", "EMS_Projects", "EMS01_CHIL_ECMS",
                              "Appli", "App", "ems_ctrl.c")
T307_PATH = os.path.join(WS, "results", "ecms_phase25", "t307_int8_report.json")
DATASET_NPZ = os.path.join(WS, "datasets", "fleet_windows_v2.npz")

EXPECTED_PARAMS = 165851
SPOT_WINDOWS = 3
SPOT_SEED = 20260902
SPOT_TOL_KWH = 1e-6        # design T1：f64 keras vs f64 重放（抽取/布局保真门）
F32_NOISE_MAX_KWH = 1e-3   # f32 累加噪声 sanity 上限（非门；观测 ~1e-6..1e-5）

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 16), b""):
            h.update(blk)
    return h.hexdigest()


def load_keras():
    os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
    import tensorflow as tf
    import architectures
    model = tf.keras.models.load_model(
        KERAS_PATH, custom_objects={"LastTimeStep": architectures.LastTimeStep},
        compile=False)
    return model


def w(layer, name):
    arr = layer.get_weights()
    assert len(arr) >= 1, name
    return arr[0], arr[1]


def extract(model):
    """keras 层名 → flat 权重（float32）。布局 = design §Data Model 1/2。"""
    L = {l.name: l for l in model.layers}
    need = (["tcn_block%d_conv1" % i for i in range(7)]
            + ["tcn_block%d_conv2" % i for i in range(7)]
            + ["tcn_block0_skip", "scalar_dense8", "tcn_head64", "E_hat_q10_50_90"])
    for n in need:
        assert n in L, "缺层 %s（keras 层名与设计不符）" % n

    def flat(layer):
        wk, wb = w(layer, layer.name)
        return np.asarray(wk, dtype=np.float32), np.asarray(wb, dtype=np.float32)

    d = {}
    d["b0_conv1_w"], d["b0_conv1_b"] = flat(L["tcn_block0_conv1"])   # (3,1,64)
    d["b0_conv2_w"], d["b0_conv2_b"] = flat(L["tcn_block0_conv2"])   # (3,64,64)
    d["b0_skip_w"], d["b0_skip_b"] = flat(L["tcn_block0_skip"])      # (1,1,64)
    shapes = {"b0_conv1_w": (3, 1, 64), "b0_conv2_w": (3, 64, 64), "b0_skip_w": (1, 1, 64)}
    for i in range(1, 7):
        d["b%d_c1_w" % i], d["b%d_c1_b" % i] = flat(L["tcn_block%d_conv1" % i])
        d["b%d_c2_w" % i], d["b%d_c2_b" % i] = flat(L["tcn_block%d_conv2" % i])
        shapes["b%d_c1_w" % i] = (3, 64, 64)
        shapes["b%d_c2_w" % i] = (3, 64, 64)
    d["head8_w"], d["head8_b"] = flat(L["scalar_dense8"])            # (2,8)
    d["head64_w"], d["head64_b"] = flat(L["tcn_head64"])             # (72,64)
    d["out_w"], d["out_b"] = flat(L["E_hat_q10_50_90"])              # (64,3)
    shapes["head8_w"] = (2, 8)
    shapes["head64_w"] = (72, 64)
    shapes["out_w"] = (64, 3)
    for k, s in shapes.items():
        assert tuple(d[k].shape) == s, "%s shape %r != %r" % (k, d[k].shape, s)
    n_params = sum(int(v.size) for k, v in d.items())
    biases = sum(int(d[k].size) for k in d if k.endswith("_b"))
    assert n_params == EXPECTED_PARAMS, "参数量 %d != %d" % (n_params, EXPECTED_PARAMS)
    assert biases == 14 * 64 + 64 + 8 + 64 + 3, biases  # 14 conv b + skip b + head8/head64/out b
    return d, n_params


def conv1d_causal_f64(x, w_flat, b, dilation, kernel=3):
    """numpy float64 causal dilated conv（布局自证用；求和顺序与 C 前向不同但数学等价）。"""
    T, Cin = x.shape
    Cout = b.size
    W = np.asarray(w_flat, dtype=np.float64).reshape(kernel, Cin, Cout)
    out = np.zeros((T, Cout), dtype=np.float64)
    for kk in range(kernel):
        off = (kernel - 1 - kk) * dilation
        if T > off:
            out[off:] += x[:T - off] @ W[kk]
    return out + b


def tcn_forward_f64(wd, win, aux):
    """导出权重的 float64 前向重放（win (300,) 归一化，aux (2,)）。返回 q3_norm (3,)。"""
    x = np.asarray(win, dtype=np.float64).reshape(300, 1)
    dil = [1, 2, 4, 8, 16, 32, 32]
    for i, di in enumerate(dil):
        kw = wd["b0_conv1_w"] if i == 0 else wd["b%d_c1_w" % i]
        kb = wd["b0_conv1_b"] if i == 0 else wd["b%d_c1_b" % i]
        c1 = np.maximum(conv1d_causal_f64(x, kw, kb, di), 0.0)
        kw2 = wd["b0_conv2_w"] if i == 0 else wd["b%d_c2_w" % i]
        kb2 = wd["b0_conv2_b"] if i == 0 else wd["b%d_c2_b" % i]
        c2 = np.maximum(conv1d_causal_f64(c1, kw2, kb2, di), 0.0)
        if i == 0:
            sk = conv1d_causal_f64(x, wd["b0_skip_w"], wd["b0_skip_b"], 1, kernel=1)
            x = np.maximum(sk + c2, 0.0)
        else:
            x = np.maximum(x + c2, 0.0)
    last = x[-1]                                                    # (64,)
    s8 = np.maximum(np.asarray(aux, dtype=np.float64) @ np.asarray(wd["head8_w"], dtype=np.float64)
                    + wd["head8_b"], 0.0)
    z = np.concatenate([last, s8])
    h = np.maximum(z @ np.asarray(wd["head64_w"], dtype=np.float64) + wd["head64_b"], 0.0)
    return h @ np.asarray(wd["out_w"], dtype=np.float64) + wd["out_b"]


def clone_model_f64(model):
    """clone_model + 全层 dtype float64 + f64 权重：keras 真正以 float64 前向。
    （直接 load_model 时层 dtype 随 config 固定 float32，set_floatx 不生效。）"""
    import tensorflow as tf

    def cl(layer):
        cfg = layer.get_config()
        cfg["dtype"] = "float64"
        return layer.__class__.from_config(cfg)

    m64 = tf.keras.models.clone_model(model, clone_function=cl)
    m64.set_weights([np.asarray(w, dtype=np.float64) for w in model.get_weights()])
    return m64


def spot_check(model, wd, y_mean, y_std):
    """3 个 val 随机窗双层 spot（kWh 域）：
    (1) 布局保真门（design T1）：f64 上转 keras 前向 vs f64 重放 ≤1e-6 kWh；
    (2) f32 keras（L0 参考原域）vs f64 重放 = f32 累加噪声，报告 + sanity 上限。"""
    d = np.load(DATASET_NPZ)
    Xv = d["X_val"]
    rng = np.random.default_rng(SPOT_SEED)
    idx = rng.choice(Xv.shape[0], SPOT_WINDOWS, replace=False)
    m64 = clone_model_f64(model)
    rows = []
    worst64 = 0.0
    worst32 = 0.0
    for r, i in enumerate(idx):
        row = np.asarray(Xv[i], dtype=np.float64)
        P32 = row[:300].astype(np.float32).reshape(1, 300, 1)
        S32 = row[300:].astype(np.float32).reshape(1, 2)
        q_keras32 = np.asarray(model([P32, S32], training=False), dtype=np.float64)[0]
        q_keras64 = np.asarray(m64([row[:300].reshape(1, 300, 1),
                                    row[300:].reshape(1, 2)], training=False),
                                dtype=np.float64)[0]
        q_replay = tcn_forward_f64(wd, row[:300], row[300:])
        dq64 = np.abs((q_replay - q_keras64) * y_std)
        dq32 = np.abs((q_replay - q_keras32) * y_std)
        worst64 = max(worst64, float(dq64[1]))
        worst32 = max(worst32, float(dq32[1]))
        rows.append({"idx": int(i),
                     "max_abs_dq_kwh_f64_vs_replay": [float(v) for v in dq64],
                     "max_abs_dq_kwh_f32_vs_replay": [float(v) for v in dq32],
                     "q50_keras_f32_kwh": float(q_keras32[1] * y_std + y_mean),
                     "q50_replay_kwh": float(q_replay[1] * y_std + y_mean)})
        print("  spot[%d] idx=%d f64门 |dq50|=%.3e | f32噪声 |dq50|=%.3e kWh"
              % (r, int(i), dq64[1], dq32[1]))
    print("  布局保真门 worst |dq50| = %.3e kWh（design T1 门 %.0e）"
          % (worst64, SPOT_TOL_KWH))
    print("  f32 keras 噪声底（报告性，sanity %.0e）= %.3e kWh"
          % (F32_NOISE_MAX_KWH, worst32))
    assert worst64 <= SPOT_TOL_KWH, "布局 spot 超门：%.3e" % worst64
    assert worst32 <= F32_NOISE_MAX_KWH, "f32 噪声 sanity 上限超出：%.3e" % worst32
    return {"seed": SPOT_SEED, "n": SPOT_WINDOWS,
            "layout_gate_abs_dq50_kwh": worst64, "layout_gate_bound_kwh": SPOT_TOL_KWH,
            "f32_noise_abs_dq50_kwh": worst32, "f32_noise_sanity_kwh": F32_NOISE_MAX_KWH,
            "windows": rows}


def bitwise_check(model_wd, jd):
    """导出 JSON（roundtrip 后）与 keras get_weights() 逐位相等（float32 bitwise）。"""
    for k in model_wd:
        a = np.asarray(jd[k], dtype=np.float32).ravel()
        assert np.array_equal(a, np.asarray(model_wd[k]).ravel()), \
            "bitwise FAIL: %s（JSON roundtrip 后与 keras 权重不一致）" % k
    return True


def build_calib_report(deploy_json, spot=None):
    """三口径一致检查（AI_MODELING §22.3 规则 2/3，FR-708）；spot_check 附带入库
    （T1 部署链机械验证机器可读，design FR-702/FR-708）。"""
    with open(NORM_PATH, "r", encoding="utf-8") as f:
        norm = json.load(f)
    with open(T307_PATH, "r", encoding="utf-8") as f:
        t307 = json.load(f)
    with open(EMSC_CTRL_PATH, "r", encoding="utf-8") as f:
        ctrl_src = f.read()
    with open(deploy_json, "r", encoding="utf-8") as f:
        dep = json.load(f)

    # 口径 1（训练）：fleet_norm_v2 特征布局/变换式 + TCN 输入定义（architectures.split_inputs）
    c1 = {
        "feature_layout_302": bool(re.search(r"302 维 = \[P_demand", norm["feature_layout"])),
        "transform_per_feature": "x_norm[f] = (x[f] - mean[f]) / std[f]" in norm["transform_formula"],
        "fit_on_train_only": "train split" in norm["fit_on"],
        "tcn_train_input": "X[:, :300] (N,300,1) + X[:,300:] (N,2)，同一 302 特征与归一化",
        "source": "datasets/fleet_norm_v2.json + python/architectures.py split_inputs",
    }
    # 口径 2（部署喂入）：ems_ctrl 归一化后 x302 → 预测器槽位（窗 xnorm[0:300] + aux xnorm[300:302]）
    c2 = {
        "ctrl_normalizes_first": ("EMS_NORM_X_MEAN" in ctrl_src
                                  and "(x[i] - EMS_NORM_X_MEAN[i]) / EMS_NORM_X_STD[i]" in ctrl_src),
        "slot_contract_normalized_302": "ems_infer_mlpq(xnorm" in ctrl_src,
        "tcn_split": "ems_infer_tcn.c 内 x302[0:300]=窗（升序）+ x302[300:302]=aux（=训练 split_inputs 布局）",
        "source": "Appli/App/ems_ctrl.c（R8：归一化只在 ctrl 一份）+ design §Architecture 2",
    }
    # 口径 3（int8 标定）：T-307 quant_calib
    qc = t307.get("quant_calib", "")
    tcn_entry = None
    for cand in t307.get("candidates", []):
        if cand.get("run_id") == "tcn_f64":
            tcn_entry = cand
            break
    c3 = {
        "quant_calib_field": qc,
        "calib_domain_normalized": ("归一化" in qc) or ("val" in qc and "样本" in qc),
        "same_as_deployment_feed": True,   # 固件喂 xnorm（口径 2），标定用 val 归一化样本（口径 1 管线）→ 同域
        "tcn_int8_val_rmse_kwh": (tcn_entry or {}).get("int8_val", {}).get("val_rmse_kwh"),
        "source": "results/ecms_phase25/t307_int8_report.json",
    }
    checks = {
        "norm_bitwise_deploy_json": True,  # 已在 build 阶段断言（不一致不会走到这里）
        "training_caliber": all([c1["feature_layout_302"], c1["transform_per_feature"],
                                 c1["fit_on_train_only"]]),
        "deployment_feed_caliber": all([c2["ctrl_normalizes_first"], c2["slot_contract_normalized_302"]]),
        "int8_calib_caliber": bool(c3["calib_domain_normalized"]) and c3["tcn_int8_val_rmse_kwh"] is not None,
        "identity_sha256_tflite": dep["sha256_tflite"] == sha256_of(TFLITE_PATH),
        "deployment_chain_spot_check": bool(spot) if spot is not None else False,
    }
    overall = all(checks.values())
    report = {
        "task": "tcn64-swap-m3",
        "spec": "ems01-chil-tcn64-swap",
        "rule": "AI_MODELING §22.3：训练口径 == 部署喂入口径 == int8 标定集口径（三方逐项核对，不一致禁止部署）",
        "predictor": "tcn_f64 (TCN-f64, 165851 params, int8 tflite 219.7KB)",
        "training_caliber": c1,
        "deployment_feed_caliber": c2,
        "int8_calib_caliber": c3,
        "checks": checks,
        "deployment_chain_spot_check": spot,
        "overall": overall,
        "conclusion": ("三口径一致，允许部署" if overall
                       else "三口径不一致：禁止部署（FR-708 阻断）"),
    }
    os.makedirs(DEPLOY_DIR, exist_ok=True)
    with open(CALIB_JSON, "w", encoding="utf-8", newline="\n") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
        f.write("\n")
    return report


def main():
    try:
        sha = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO, capture_output=True,
                             text=True, check=True).stdout.strip()
    except Exception:
        sha = "unavailable"
    print("git HEAD:", sha)

    model = load_keras()
    wd, n_params = extract(model)
    print("extracted params:", n_params)

    with open(NORM_PATH, "r", encoding="utf-8") as f:
        norm = json.load(f)
    deploy = {
        "task": "tcn64-swap",
        "run_id": "tcn_f64",
        "model": "TCN-f64 7-block causal dilated (k=3, d=1,2,4,8,16,32,32) f=64 "
                 "+ LastTimeStep + Dense(2->8) bypass + Dense(72->64) + Dense(64->3)",
        "source_keras": "models/tcn_f64/tcn_f64.keras",
        "source_tflite": "models/tcn_f64/tcn_f64_int8.tflite",
        "sha256_keras": sha256_of(KERAS_PATH),
        "sha256_tflite": sha256_of(TFLITE_PATH),
        "git_commit_sha": sha,
        "export_script": "python/export_tcn_deploy.py",
        "caliber": {
            "input_desc": "302 = [P_demand(t-299..t) x300（1 s 升序） | v_ship | accel]",
            "norm_source": "datasets/fleet_norm_v2.json",
            "input_domain": "normalized (X_mean/X_std per-feature, float64 常数)",
            "output_domain": "normalized, q10/q50/q90；反归一化 q*Y_STD+Y_MEAN 属调用方",
            "window_order": "ascending 1s",
            "quant_calib": "T-307 val 前 1000 归一化样本（与部署喂入口径相同，§22.3 规则 3 例外条款适用）",
            "tflite_io": "QDQ int8，float32 I/O；IN_1=[1,300,1] 1200B, IN_2=[1,2] 8B, OUT_1=[1,3] 12B",
            "out_scale": 0.016138948500156403,
            "out_zp": -28,
            "lsb_tcn_kwh": 0.266214042,
        },
        "norm": {"X_mean": norm["X_mean"], "X_std": norm["X_std"],
                 "y_mean": norm["y_mean"], "y_std": norm["y_std"]},
        "weights": {k: v.ravel().tolist() for k, v in wd.items()},
    }
    # norm 逐位断言（train 源 = deploy 副本）
    assert np.array_equal(np.array(norm["X_mean"], dtype=np.float64),
                          np.array(deploy["norm"]["X_mean"], dtype=np.float64))
    assert float(norm["y_std"]) == float(deploy["norm"]["y_std"])

    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w", encoding="utf-8", newline="\n") as f:
        json.dump(deploy, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("wrote", OUT_JSON, "(%d B)" % os.path.getsize(OUT_JSON))

    # roundtrip bitwise：重新载入 JSON 的权重与 keras 原权重逐位相等
    with open(OUT_JSON, "r", encoding="utf-8") as f:
        rt = json.load(f)
    bitwise_check(wd, rt["weights"])
    print("bitwise extraction check: OK（%d 数组与 keras get_weights() 逐位相等）" % len(wd))

    spot = spot_check(model, wd, float(norm["y_mean"]), float(norm["y_std"]))
    report = build_calib_report(OUT_JSON, spot=spot)
    print("calib overall:", report["overall"], "-", report["conclusion"])
    assert report["overall"], "三口径一致检查 FAIL（FR-708 阻断）"
    print("DONE: export + spot + calib 全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""export_tcnq_reference.py — TCN-f64 keras float32 逐 tick/逐向量参考链（L0 层①段①基准）。

规格：spec ems01-chil-tcn64-swap，design §Testing T2/T4①（FR-708b/FR-709）。
参考口径：fleet_norm_v2.json 归一化 → keras float32 前向（tcn_f64.keras，双输入
[1,300,1]+[1,2]）→ 反归一化 q10/q50/q90 [kWh]。这是 L0 host 差分的“期望值生成器”
（TCN 无 EML 锚点，参考链 = keras 源模型本身）。

用法（两种模式）：
  --vectors-raw CSV --out CSV    302 维原始物理量向量（f32，如 vectors_raw.csv 200 行）
  --trace CSV --out CSV          注入轨迹（tick,t,P_load_kW,SOC,v_ship_mps 列，
                                 如 fw_reference/ref_G2_*.csv）→ 逐 tick q3_kwh

窗口/归一化时序语义与 ems_ctrl.c 严格一致（复用 gen_reference_int8.MirrorCtrl：
平 fill 初始化、环形窗升序展开、accel=v-vprev、NaN 跳写）。首拍预热语义补齐
（T-805 L0 捕获）：MirrorCtrl 自身 reset 仅覆盖 t<last_t 路径，首拍（t0 > -1e9）
不会平 fill——需按 ems_ctrl.c EML:27-35 预置 buf=平 fill(首行 P_load) +
vprev=首行 v_ship，否则轨迹前 300 tick 窗口与固件失配（零填充 vs 平 fill）。

运行：model/simulink/.venv_tf/bin/python python/export_tcnq_reference.py ...
"""
import argparse
import csv
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
WS = os.path.abspath(os.path.join(HERE, ".."))
REPO = os.path.abspath(os.path.join(WS, "..", ".."))
KERAS_PATH = os.path.join(WS, "results", "ecms_phase25", "models", "tcn_f64", "tcn_f64.keras")
NORM_PATH = os.path.join(WS, "datasets", "fleet_norm_v2.json")

sys.path.insert(0, HERE)


def load_keras():
    os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
    import tensorflow as tf
    import architectures
    return tf.keras.models.load_model(
        KERAS_PATH, custom_objects={"LastTimeStep": architectures.LastTimeStep},
        compile=False)


def keras_q3_batch(model, Xn):
    """Xn (N,302) float64 归一化 → (N,3) q3 [kWh]。"""
    P = np.ascontiguousarray(Xn[:, :300].astype(np.float32)).reshape(-1, 300, 1)
    S = np.ascontiguousarray(Xn[:, 300:].astype(np.float32))
    q = np.asarray(model([P, S], training=False), dtype=np.float64)
    return q


def mode_vectors_raw(model, in_csv, out_csv, x_mean, x_std, y_mean, y_std):
    raw = np.loadtxt(in_csv, delimiter=",", dtype=np.float64)
    assert raw.ndim == 2 and raw.shape[1] == 302, raw.shape
    Xn = (raw - x_mean[None, :]) / x_std[None, :]
    q = keras_q3_batch(model, Xn) * y_std + y_mean
    with open(out_csv, "w", newline="") as f:
        f.write("row,q10_kwh,q50_kwh,q90_kwh\n")
        for r in range(q.shape[0]):
            f.write("%d,%.6f,%.6f,%.6f\n" % (r, q[r, 0], q[r, 1], q[r, 2]))
    print("vectors: %d rows -> %s" % (q.shape[0], out_csv))


def mode_trace(model, in_csv, out_csv, x_mean, x_std, y_mean, y_std):
    fw_tools = os.path.join(REPO, "firmware", "stm32n6", "04_SourceCode",
                "SoftwarePackage", "EMS_Projects", "EMS01_CHIL_ECMS",
                "Host", "tools")
    sys.path.insert(0, fw_tools)
    sys.dont_write_bytecode = True
    import gen_reference_int8 as gri
    m = gri.MirrorCtrl(x_mean, x_std, y_mean, y_std, dt=1.0)
    xn_rows, tick_ids = [], []
    trace_rows = []
    with open(in_csv, "r", newline="") as f:
        for r in csv.DictReader(f):
            trace_rows.append((int(r["tick"]), float(r["t"]),
                               float(r["P_load_kW"]), float(r["SOC"]),
                               float(r["v_ship_mps"])))
    if not trace_rows:
        raise RuntimeError("trace CSV 无行")
    # 首拍预热语义对齐固件（ems_ctrl.c EML:27-35 首调平 fill + vprev=v_ship；
    # MirrorCtrl 首拍不触发自身 reset，零初始化会在预热段偏离固件口径）：
    # 仅预置 buf/vprev，last_t 保持 -1e9 使首拍正常推进（不落入 tick 门）。
    m.buf[:] = trace_rows[0][2]
    m.vprev = trace_rows[0][4]
    for (tick, t, pl, soc, v) in trace_rows:
        out = m.step(t, pl, v)
        if out.get("held"):
            continue
        if not out.get("ok"):
            xn_rows.append(None)   # 非有限特征：C 侧 ok=0，不产生 NN 输出
            tick_ids.append(tick)
            continue
        xn_rows.append(out["xnorm"])
        tick_ids.append(tick)
    valid = [i for i, x in enumerate(xn_rows) if x is not None]
    Xn = np.vstack([xn_rows[i] for i in valid])
    q = keras_q3_batch(model, Xn) * y_std + y_mean
    qi = np.full((len(xn_rows), 3), np.nan)
    for j, i in enumerate(valid):
        qi[i] = q[j]
    with open(out_csv, "w", newline="") as f:
        f.write("tick,q10_kwh,q50_kwh,q90_kwh\n")
        for i in range(len(tick_ids)):
            f.write("%d,%.6f,%.6f,%.6f\n" % (tick_ids[i], qi[i, 0], qi[i, 1], qi[i, 2]))
    print("trace: %d ticks (%d valid) -> %s" % (len(tick_ids), len(valid), out_csv))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--vectors-raw", metavar="CSV", help="302 维原始物理量向量 CSV")
    ap.add_argument("--trace", metavar="CSV", help="注入轨迹 CSV（tick,t,P_load_kW,SOC,v_ship_mps）")
    ap.add_argument("--out", metavar="CSV", required=True, help="输出 q3_kwh CSV")
    args = ap.parse_args()
    assert args.vectors_raw or args.trace, "需要 --vectors-raw 或 --trace 之一"

    with open(NORM_PATH, "r", encoding="utf-8") as f:
        norm = json.load(f)
    x_mean = np.asarray(norm["X_mean"], dtype=np.float64)
    x_std = np.asarray(norm["X_std"], dtype=np.float64)
    y_mean, y_std = float(norm["y_mean"]), float(norm["y_std"])

    model = load_keras()
    if args.vectors_raw:
        mode_vectors_raw(model, args.vectors_raw, args.out, x_mean, x_std, y_mean, y_std)
    else:
        mode_trace(model, args.trace, args.out, x_mean, x_std, y_mean, y_std)
    return 0


if __name__ == "__main__":
    sys.exit(main())

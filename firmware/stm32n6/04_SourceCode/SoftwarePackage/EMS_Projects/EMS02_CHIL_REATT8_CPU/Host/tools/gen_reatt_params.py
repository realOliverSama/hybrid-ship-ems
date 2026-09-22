#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_reatt_params.py — 生成 Appli/App/ems_reatt_params.c/.h（phase4-g，T-005）。

REAT-T 板上归一化常数单一来源（spec .specs/phase4-g-reatt-board-deployment D5），
从 3 个只读冻结源生成（+1 个部署物校验源）：

  1. ModelE/results/dataset_v3/formal/formal_norm_v3.json
       hist 逐 bin mean/std（x_h1_mean/x_h1_std，每通道 300 长）
  2. ModelE/config/predictor_v3/predictor_norm_ext_v1.json
       KF 逐 (channel, lead) mean/scale（kf.<ch>.mean/.scale，每通道 30 长）
  3. ModelE/config/dataset_v3_signal_registry.json
       INPUT_H1 / INPUT_K / INPUT_F 通道序（机器事实源；通道索引 = 模型输入
       堆叠序 = 线上传输序）
  4. Model/reatt8_q/reatt_raw_int8.tflite（sha256 溯源登记，不读内容）

确定性 / 幂等：输出是源文件字节的纯函数（gen_fw_params.py 同约定）；不写时间戳。

输出布局：
  EMS_REATT_H1_MEAN[11*300] / EMS_REATT_H1_STD[11*300]   —— [ch][bin] ch 主序
  EMS_REATT_KF_MEAN[6*30]  / EMS_REATT_KF_SCALE[6*30]    —— [ch][lead]
  归一化式（与 predictor_v3.features.transform_h1 / norm_ext.transform_channel
  数值同构，固件以 float32 域执行）：
    xnorm[t][ch] = (xraw[t][ch] - H1_MEAN[ch][t]) / H1_STD[ch][t]
    knorm[k][ch] = (kraw[k][ch] - KF_MEAN[ch][k]) / KF_SCALE[ch][k]

用法：
    python3 Host/tools/gen_reatt_params.py            # 生成（仓库任意位置）
    python3 Host/tools/gen_reatt_params.py --check    # 只校验一致性
"""
import argparse
import hashlib
import json
import os
import sys

import numpy as np


def find_repo_root(start_dir):
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("ERROR: 无法定位仓库根")
        d = parent


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = find_repo_root(SCRIPT_DIR)
MODELE = os.path.join(REPO_ROOT, "model", "simulink_modelE_(Phase4_Physics)")
OUT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Appli", "App"))
TFLITE = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Model",
                                       "reatt8_q", "reatt_raw_int8.tflite"))

SRC_NORM = os.path.join(MODELE, "results", "dataset_v3", "formal",
                        "formal_norm_v3.json")
SRC_NORM_EXT = os.path.join(MODELE, "config", "predictor_v3",
                            "predictor_norm_ext_v1.json")
SRC_REGISTRY = os.path.join(MODELE, "config", "dataset_v3_signal_registry.json")

H1_N, H1_T = 11, 300
KF_N, KF_T = 6, 30


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def registry_orders(reg):
    def names(role):
        sig = [s for s in reg["signals"] if s.get("role") == role]
        # registry 无显式 order 字段时按 signals 数组序（contracts.signal_names 同源）
        return [s["name"] for s in sig]
    return names("INPUT_H1"), names("INPUT_K") + names("INPUT_F")


def fmt_f32(v):
    """float32 round-trip 十进制（9 位有效数字保证往返精确；必带小数点）。"""
    f = float(np.float32(v))
    if f == 0.0:
        return "0.0f"
    s = f"{f:.9g}"
    if "." not in s and "e" not in s and "E" not in s:
        s += ".0"
    return s + "f"


def emit_array(name, arr, per_line=6):
    lines = [f"const float {name}[{arr.size}] = {{"]
    flat = arr.ravel(order="C")
    for i in range(0, flat.size, per_line):
        chunk = ", ".join(fmt_f32(v) for v in flat[i:i + per_line])
        comma = "," if i + per_line < flat.size else ""
        lines.append(f"    {chunk}{comma}")
    lines.append("};")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    norm = json.load(open(SRC_NORM, encoding="utf-8"))
    ext = json.load(open(SRC_NORM_EXT, encoding="utf-8"))
    reg = json.load(open(SRC_REGISTRY, encoding="utf-8"))
    h1_order, kf_order = registry_orders(reg)

    if len(h1_order) != H1_N or len(kf_order) != KF_N:
        raise SystemExit(f"registry 通道数异常: H1={len(h1_order)} KF={len(kf_order)}")

    # [ch][bin] ch 主序（与 predictor_v3.features.assemble 的堆叠通道序一致）
    h1_mean = np.zeros((H1_N, H1_T), dtype=np.float64)
    h1_std = np.zeros((H1_N, H1_T), dtype=np.float64)
    for j, ch in enumerate(h1_order):
        h1_mean[j] = np.asarray(norm["x_h1_mean"][ch], dtype=np.float64)
        h1_std[j] = np.asarray(norm["x_h1_std"][ch], dtype=np.float64)
        if h1_mean[j].shape != (H1_T,) or h1_std[j].shape != (H1_T,):
            raise SystemExit(f"H1 通道 {ch} 常数长度异常")
        if not np.all(h1_std[j] > 0):
            raise SystemExit(f"H1 通道 {ch} 存在非正 std")
    kf_mean = np.zeros((KF_N, KF_T), dtype=np.float64)
    kf_scale = np.zeros((KF_N, KF_T), dtype=np.float64)
    for j, ch in enumerate(kf_order):
        kf_mean[j] = np.asarray(ext["kf"][ch]["mean"], dtype=np.float64)
        kf_scale[j] = np.asarray(ext["kf"][ch]["scale"], dtype=np.float64)
        if kf_mean[j].shape != (KF_T,) or kf_scale[j].shape != (KF_T,):
            raise SystemExit(f"KF 通道 {ch} 常数长度异常")
        if not np.all(kf_scale[j] > 0):
            raise SystemExit(f"KF 通道 {ch} 存在非正 scale")

    srcs = {
        "formal_norm_v3.json": sha256_file(SRC_NORM),
        "predictor_norm_ext_v1.json": sha256_file(SRC_NORM_EXT),
        "dataset_v3_signal_registry.json": sha256_file(SRC_REGISTRY),
        "reatt_raw_int8.tflite": sha256_file(TFLITE),
    }
    provenance = "\n".join(f" *   {k}: {v}" for k, v in srcs.items())

    header = f"""/* ems_reatt_params.h — REAT-T 板上归一化常数（生成物，禁手改）。
 *
 * 生成器：Host/tools/gen_reatt_params.py（phase4-g T-005；确定性幂等，重跑逐字节一致）。
 * 源（全部只读冻结件，sha256）：
{provenance}
 *
 * 通道序（= 模型输入堆叠序 = CMD_REATT 线上传输序）：
 *   H1: {", ".join(h1_order)}
 *   KF: {", ".join(kf_order)}
 *
 * 布局：H1_MEAN/STD [ch][bin]（ch 主序，ch<11, bin<300）；
 *       KF_MEAN/SCALE [ch][lead]（ch<6, lead<30）。
 */
#ifndef EMS_REATT_PARAMS_H
#define EMS_REATT_PARAMS_H

#include <stdint.h>

#define EMS_REATT_H1_CH_N 11u   /* H1 通道数（元素计数宏在 ems_reatt.h：EMS_REATT_HIST_N） */
#define EMS_REATT_H1_T    300u  /* H1 bin 数（时间步） */
#define EMS_REATT_KF_CH_N 6u    /* KF 通道数（元素计数宏在 ems_reatt.h：EMS_REATT_KF_N） */
#define EMS_REATT_KF_T    30u   /* KF lead 数 */

extern const float EMS_REATT_H1_MEAN[EMS_REATT_H1_CH_N * EMS_REATT_H1_T];
extern const float EMS_REATT_H1_STD[EMS_REATT_H1_CH_N * EMS_REATT_H1_T];
extern const float EMS_REATT_KF_MEAN[EMS_REATT_KF_CH_N * EMS_REATT_KF_T];
extern const float EMS_REATT_KF_SCALE[EMS_REATT_KF_CH_N * EMS_REATT_KF_T];

#endif /* EMS_REATT_PARAMS_H */
"""

    body = f"""/* ems_reatt_params.c — REAT-T 板上归一化常数（生成物，禁手改）。
 *
 * 生成器：Host/tools/gen_reatt_params.py；源 sha256 见 ems_reatt_params.h。
 * 数值域：float32（模型输入张量域；f64 源常数经 f32 舍入，
 * 舍入差 ≪ int8 量化步长，L0/L1 对拍门覆盖）。
 */
#include "ems_reatt_params.h"

{emit_array("EMS_REATT_H1_MEAN", h1_mean)}

{emit_array("EMS_REATT_H1_STD", h1_std)}

{emit_array("EMS_REATT_KF_MEAN", kf_mean)}

{emit_array("EMS_REATT_KF_SCALE", kf_scale)}
"""

    h_path = os.path.join(OUT_DIR, "ems_reatt_params.h")
    c_path = os.path.join(OUT_DIR, "ems_reatt_params.c")
    if args.check:
        ok = (open(h_path, encoding="utf-8").read() == header
              and open(c_path, encoding="utf-8").read() == body)
        print("gen_reatt_params --check:", "OK" if ok else "STALE")
        return 0 if ok else 1
    open(h_path, "w", encoding="utf-8").write(header)
    open(c_path, "w", encoding="utf-8").write(body)
    print(f"written: {os.path.relpath(h_path, REPO_ROOT)}")
    print(f"written: {os.path.relpath(c_path, REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
